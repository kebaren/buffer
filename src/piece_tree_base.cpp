#include "textbuffer/piece_tree_base.h"
#include "textbuffer/rb_tree_base.h"
#include "textbuffer/piece_tree_snapshot.h"
#include "textbuffer/common/position.h"
#include "textbuffer/common/range.h"
#include <algorithm>
#include <stdexcept>

namespace textbuffer {

void PieceTreeBase::deleteTree(TreeNode* node) {
    if (node != SENTINEL) {
        deleteTree(node->left);
        deleteTree(node->right);
        delete node->piece;
        delete node;
    }
}

void PieceTreeBase::create(const std::vector<StringBuffer>& chunks, const std::string& eol, bool eolNormalized) {
    _buffers = {StringBuffer("", {0})};
    _lastChangeBufferPos = {0, 0};
    root = SENTINEL;
    _lineCnt = 1;
    _length = 0;
    _EOL = eol;
    _EOLLength = eol.length();
    _EOLNormalized = eolNormalized;

    TreeNode* lastNode = nullptr;
    for (size_t i = 0; i < chunks.size(); i++) {
        if (!chunks[i].buffer.empty()) {
            StringBuffer chunk = chunks[i];
            if (chunk.lineStarts.empty()) {
                chunk.lineStarts = createLineStartsFast(chunk.buffer);
            }

            auto piece = new Piece(
                i + 1,
                {0, 0},
                {static_cast<int32_t>(chunk.lineStarts.size() - 1), 
                 static_cast<int32_t>(chunk.buffer.length() - chunk.lineStarts[chunk.lineStarts.size() - 1])},
                chunk.lineStarts.size() - 1,
                chunk.buffer.length()
            );
            _buffers.push_back(chunk);
            lastNode = rbInsertRight(lastNode, piece);
        }
    }

    _searchCache = std::make_unique<PieceTreeSearchCache>(1);
    _lastVisitedLine = {0, ""};
    computeBufferMetadata();
}

void PieceTreeBase::normalizeEOL(const std::string& eol) {
    int32_t averageBufferSize = AverageBufferSize;
    int32_t min = averageBufferSize - averageBufferSize / 3;
    int32_t max = min * 2;

    std::string tempChunk;
    int32_t tempChunkLen = 0;
    std::vector<StringBuffer> chunks;

    iterate(root, [&](TreeNode* node) {
        std::string str = getNodeContent(node);
        int32_t len = str.length();
        if (tempChunkLen <= min || tempChunkLen + len < max) {
            tempChunk += str;
            tempChunkLen += len;
            return true;
        }

        // flush anyways
        std::string text = tempChunk;
        std::replace(text.begin(), text.end(), '\r', '\n');
        chunks.push_back(StringBuffer(text, createLineStartsFast(text)));
        tempChunk = str;
        tempChunkLen = len;
        return true;
    });

    if (tempChunkLen > 0) {
        std::string text = tempChunk;
        std::replace(text.begin(), text.end(), '\r', '\n');
        chunks.push_back(StringBuffer(text, createLineStartsFast(text)));
    }

    create(chunks, eol, true);
}

std::string PieceTreeBase::getEOL() const {
    return _EOL;
}

void PieceTreeBase::setEOL(const std::string& newEOL) {
    _EOL = newEOL;
    _EOLLength = _EOL.length();
    normalizeEOL(newEOL);
}

std::unique_ptr<ITextSnapshot> PieceTreeBase::createSnapshot(const std::string& BOM) const {
    return std::make_unique<PieceTreeSnapshot>(const_cast<PieceTreeBase*>(this), BOM);
}

bool PieceTreeBase::equal(const PieceTreeBase& other) const {
    if (getLength() != other.getLength()) {
        return false;
    }
    if (getLineCount() != other.getLineCount()) {
        return false;
    }

    int32_t offset = 0;
    bool ret = iterate(root, [&](TreeNode* node) {
        if (node == SENTINEL) {
            return true;
        }
        std::string str = getNodeContent(node);
        int32_t len = str.length();
        NodePosition startPosition = const_cast<PieceTreeBase&>(other).nodeAt(offset);
        NodePosition endPosition = const_cast<PieceTreeBase&>(other).nodeAt(offset + len);
        std::string val = const_cast<PieceTreeBase&>(other).getValueInRange2(startPosition, endPosition);

        return str == val;
    });

    return ret;
}

int32_t PieceTreeBase::getOffsetAt(int32_t lineNumber, int32_t column) {
    // Line numbers in the buffer are 0-based
    if (lineNumber <= 0) {
        // First line starts at offset 0
        return column;
    }

    int32_t leftLen = 0; // inorder
    TreeNode* x = root;

    while (x != SENTINEL) {
        if (x->left != SENTINEL && x->lf_left >= lineNumber) {
            // If left subtree contains enough line feeds, go left
            x = x->left;
        } else if (x->lf_left + x->piece->lineFeedCnt >= lineNumber) {
            // Line is in this node
            leftLen += x->size_left;
            
            // Calculate the accumulated value up to the line we're looking for
            // Note: we want the start of the line, not the newline character
            int32_t prevLineIndex = lineNumber - 1;
            int32_t prevAccumulatedValue = getAccumulatedValue(x, prevLineIndex - x->lf_left);
            
            // Return the offset at the start of the line
            return leftLen + prevAccumulatedValue + column;
        } else {
            // Line is in right subtree
            lineNumber -= x->lf_left + x->piece->lineFeedCnt;
            leftLen += x->size_left + x->piece->length;
            x = x->right;
        }
    }

    return leftLen + column;
}

common::Position PieceTreeBase::getPositionAt(int32_t offset) {
    offset = std::max(0, offset);

    TreeNode* x = root;
    int32_t lfCnt = 0;
    int32_t originalOffset = offset;

    while (x != SENTINEL) {
        if (x->size_left != 0 && x->size_left >= offset) {
            x = x->left;
        } else if (x->size_left + x->piece->length >= offset) {
            auto out = getIndexOf(x, offset - x->size_left);

            lfCnt += x->lf_left + out.first;

            if (out.first == 0) {
                int32_t lineStartOffset = getOffsetAt(lfCnt + 1, 1);
                int32_t column = originalOffset - lineStartOffset;
                return common::Position(lfCnt + 1, column + 1);
            }

            return common::Position(lfCnt + 1, out.second + 1);
        } else {
            offset -= x->size_left + x->piece->length;
            lfCnt += x->lf_left + x->piece->lineFeedCnt;

            if (x->right == SENTINEL) {
                // last node
                int32_t lineStartOffset = getOffsetAt(lfCnt + 1, 1);
                int32_t column = originalOffset - offset - lineStartOffset;
                return common::Position(lfCnt + 1, column + 1);
            } else {
                x = x->right;
            }
        }
    }

    return common::Position(1, 1);
}

std::string PieceTreeBase::getValueInRange(const common::Range& range, const std::string& eol) {
    if (range.startLineNumber() == range.endLineNumber() && range.startColumn() == range.endColumn()) {
        return "";
    }

    NodePosition startPosition = nodeAt2(range.startLineNumber(), range.startColumn());
    NodePosition endPosition = nodeAt2(range.endLineNumber(), range.endColumn());

    std::string value = getValueInRange2(startPosition, endPosition);
    if (!eol.empty()) {
        if (eol != _EOL || !_EOLNormalized) {
            std::string result = value;
            size_t pos = 0;
            while ((pos = result.find("\r\n", pos)) != std::string::npos) {
                result.replace(pos, 2, eol);
                pos += eol.length();
            }
            while ((pos = result.find("\r", pos)) != std::string::npos) {
                result.replace(pos, 1, eol);
                pos += eol.length();
            }
            while ((pos = result.find("\n", pos)) != std::string::npos) {
                result.replace(pos, 1, eol);
                pos += eol.length();
            }
            return result;
        }

        if (eol == getEOL() && _EOLNormalized) {
            return value;
        }
        std::string result = value;
        size_t pos = 0;
        while ((pos = result.find("\r\n", pos)) != std::string::npos) {
            result.replace(pos, 2, eol);
            pos += eol.length();
        }
        while ((pos = result.find("\r", pos)) != std::string::npos) {
            result.replace(pos, 1, eol);
            pos += eol.length();
        }
        while ((pos = result.find("\n", pos)) != std::string::npos) {
            result.replace(pos, 1, eol);
            pos += eol.length();
        }
        return result;
    }
    return value;
}

std::string PieceTreeBase::getValueInRange2(const NodePosition& startPosition, const NodePosition& endPosition) {
    if (startPosition.node == endPosition.node) {
        TreeNode* node = startPosition.node;
        std::string buffer = _buffers[node->piece->bufferIndex].buffer;
        int32_t startOffset = offsetInBuffer(node->piece->bufferIndex, node->piece->start);
        return buffer.substr(startOffset + startPosition.remainder, 
                           endPosition.remainder - startPosition.remainder);
    }

    TreeNode* x = startPosition.node;
    std::string buffer = _buffers[x->piece->bufferIndex].buffer;
    int32_t startOffset = offsetInBuffer(x->piece->bufferIndex, x->piece->start);
    std::string ret = buffer.substr(startOffset + startPosition.remainder, 
                                  startOffset + x->piece->length - (startOffset + startPosition.remainder));

    x = x->next();
    while (x != SENTINEL) {
        std::string buffer = _buffers[x->piece->bufferIndex].buffer;
        int32_t startOffset = offsetInBuffer(x->piece->bufferIndex, x->piece->start);

        if (x == endPosition.node) {
            ret += buffer.substr(startOffset, startOffset + endPosition.remainder - startOffset);
            break;
        } else {
            ret += buffer.substr(startOffset, x->piece->length);
        }

        x = x->next();
    }

    return ret;
}

std::vector<std::string> PieceTreeBase::getLinesContent() {
    std::string content = getContentOfSubTree(root);
    std::vector<std::string> lines;
    size_t start = 0;
    size_t end = content.find_first_of("\r\n");
    
    while (end != std::string::npos) {
        lines.push_back(content.substr(start, end - start));
        start = end + 1;
        if (content[end] == '\r' && content[start] == '\n') {
            start++;
        }
        end = content.find_first_of("\r\n", start);
    }
    
    if (start < content.length()) {
        lines.push_back(content.substr(start));
    }
    
    return lines;
}

int32_t PieceTreeBase::getLength() const {
    return _length;
}

int32_t PieceTreeBase::getLineCount() const {
    return _lineCnt;
}

std::string PieceTreeBase::getLineContent(int32_t lineNumber) {
    if (lineNumber < 0 || lineNumber >= getLineCount()) {
        throw std::out_of_range("Invalid line number");
    }

    // If cache is available, return cached result
    if (lineNumber == _lastVisitedLine.first) {
        return _lastVisitedLine.second;
    }

    // Update cache line number
    _lastVisitedLine.first = lineNumber;

    if (root == SENTINEL) {
        _lastVisitedLine.second = "";
        return "";
    }
    
    // Important: adjust lineNumber to work with getLineRawContent which uses 1-based indexing
    int32_t adjustedLineNumber = lineNumber + 1;
    
    // The rest of the implementation uses the adjusted lineNumber
    if (adjustedLineNumber == getLineCount()) {
        _lastVisitedLine.second = getLineRawContent(adjustedLineNumber);
    } else if (_EOLNormalized) {
        _lastVisitedLine.second = getLineRawContent(adjustedLineNumber, _EOLLength);
    } else {
        std::string content = getLineRawContent(adjustedLineNumber);
        // Remove trailing line endings
        if (!content.empty()) {
            if (content.size() >= 2 && 
                content[content.size() - 2] == '\r' && 
                content[content.size() - 1] == '\n') {
                content = content.substr(0, content.size() - 2);
            } else if (content.back() == '\n' || content.back() == '\r') {
                content = content.substr(0, content.size() - 1);
            }
        }
        _lastVisitedLine.second = content;
    }
    
    return _lastVisitedLine.second;
}

uint32_t PieceTreeBase::getLineCharCode(int32_t lineNumber, int32_t index) {
    NodePosition nodePos = nodeAt2(lineNumber, index + 1);
    if (nodePos.remainder == nodePos.node->piece->length) {
        // the char we want to fetch is at the head of next node.
        TreeNode* matchingNode = nodePos.node->next();
        if (!matchingNode) {
            return 0;
        }

        std::string buffer = _buffers[matchingNode->piece->bufferIndex].buffer;
        int32_t startOffset = offsetInBuffer(matchingNode->piece->bufferIndex, matchingNode->piece->start);
        return static_cast<unsigned char>(buffer[startOffset]);
    } else {
        std::string buffer = _buffers[nodePos.node->piece->bufferIndex].buffer;
        int32_t startOffset = offsetInBuffer(nodePos.node->piece->bufferIndex, nodePos.node->piece->start);
        int32_t targetOffset = startOffset + nodePos.remainder;

        return static_cast<unsigned char>(buffer[targetOffset]);
    }
}

int32_t PieceTreeBase::getLineLength(int32_t lineNumber) {
    if (lineNumber == getLineCount()) {
        int32_t startOffset = getOffsetAt(lineNumber, 1);
        return getLength() - startOffset;
    }
    return getOffsetAt(lineNumber + 1, 1) - getOffsetAt(lineNumber, 1) - _EOLLength;
}

std::string PieceTreeBase::getValue() {
    return getContentOfSubTree(root);
}

void PieceTreeBase::insert(int32_t offset, const std::string& value, bool eolNormalized) {
    // Don't proceed if value is empty
    if (value.empty()) {
        return;
    }
    
    _EOLNormalized = _EOLNormalized && eolNormalized;
    _lastVisitedLine.first = 0;
    _lastVisitedLine.second = "";

    int32_t currentLength = getLength();
    if (offset > currentLength) {
        // If offset is beyond the end, just append to the end
        offset = currentLength;
    }

    if (root != SENTINEL) {
        auto nodePosition = nodeAt(offset);
        TreeNode* node = nodePosition.node;
        int32_t remainder = nodePosition.remainder;
        int32_t nodeStartOffset = nodePosition.nodeStartOffset;
        
        if (!node) {
            // If somehow nodeAt failed (shouldn't happen with the checks), just return
            return;
        }
        
        Piece* piece = node->piece;
        int32_t bufferIndex = piece->bufferIndex;
        BufferCursor insertPosInBuffer = positionInBuffer(node, remainder);
        
        if (node->piece->bufferIndex == 0 &&
            piece->end.line == _lastChangeBufferPos.line &&
            piece->end.column == _lastChangeBufferPos.column &&
            (nodeStartOffset + piece->length == offset) &&
            value.length() < AverageBufferSize) {
            // changed buffer
            appendToNode(node, value);
            computeBufferMetadata();
            return;
        }

        if (nodeStartOffset == offset) {
            insertContentToNodeLeft(value, node);
            _searchCache->validate(offset);
        } else if (nodeStartOffset + node->piece->length > offset) {
            // we are inserting into the middle of a node.
            std::vector<TreeNode*> nodesToDel;
            auto newRightPiece = new Piece(
                piece->bufferIndex,
                insertPosInBuffer,
                piece->end,
                getLineFeedCnt(piece->bufferIndex, insertPosInBuffer, piece->end),
                offsetInBuffer(bufferIndex, piece->end) - offsetInBuffer(bufferIndex, insertPosInBuffer)
            );

            if (shouldCheckCRLF() && endWithCR(value)) {
                int32_t headOfRight = nodeCharCodeAt(node, remainder);

                if (headOfRight == 10) { // \n
                    // Adjust the right piece to start after the \n
                    BufferCursor newStart{newRightPiece->start.line + 1, 0};
                    delete newRightPiece; // Delete the old piece to avoid memory leak
                    newRightPiece = new Piece(
                        piece->bufferIndex,
                        newStart,
                        piece->end,
                        getLineFeedCnt(piece->bufferIndex, newStart, piece->end),
                        offsetInBuffer(bufferIndex, piece->end) - offsetInBuffer(bufferIndex, newStart)
                    );

                    std::string newValue = value + '\n';
                    insertContentToNodeLeft(newValue, node);
                    if (newRightPiece->length > 0) {
                        rbInsertRight(node, newRightPiece);
                    } else {
                        delete newRightPiece; // Prevent memory leak
                    }
                    computeBufferMetadata();
                    return;
                }
            }

            // reuse node for content before insertion point.
            if (shouldCheckCRLF() && startWithLF(value)) {
                int32_t tailOfLeft = nodeCharCodeAt(node, remainder - 1);
                if (tailOfLeft == 13) { // \r
                    BufferCursor previousPos = positionInBuffer(node, remainder - 1);
                    deleteNodeTail(node, previousPos);
                    std::string newValue = '\r' + value;

                    if (node->piece->length == 0) {
                        nodesToDel.push_back(node);
                    }
                    
                    std::vector<Piece*> newPieces = createNewPieces(newValue);
                    TreeNode* tmpNode = node;
                    for (Piece* p : newPieces) {
                        tmpNode = rbInsertRight(tmpNode, p);
                    }
                    
                    // Insert right piece after new pieces
                    if (newRightPiece->length > 0) {
                        rbInsertRight(tmpNode, newRightPiece);
                    } else {
                        delete newRightPiece; // Prevent memory leak
                    }
                    
                    deleteNodes(nodesToDel);
                    computeBufferMetadata();
                    return;
                } else {
                    deleteNodeTail(node, insertPosInBuffer);
                }
            } else {
                deleteNodeTail(node, insertPosInBuffer);
            }

            std::vector<Piece*> newPieces = createNewPieces(value);
            
            // Insert pieces between left and right part
            TreeNode* tmpNode = node;
            for (Piece* p : newPieces) {
                tmpNode = rbInsertRight(tmpNode, p);
            }
            
            // Insert right part after new pieces
            if (newRightPiece->length > 0) {
                rbInsertRight(tmpNode, newRightPiece);
            } else {
                delete newRightPiece; // Prevent memory leak
            }
            
            deleteNodes(nodesToDel);
        } else {
            insertContentToNodeRight(value, node);
        }
    } else {
        // insert new node
        std::vector<Piece*> pieces = createNewPieces(value);
        TreeNode* node = rbInsertLeft(nullptr, pieces[0]);

        for (size_t k = 1; k < pieces.size(); k++) {
            node = rbInsertRight(node, pieces[k]);
        }
    }

    computeBufferMetadata();
}

void PieceTreeBase::delete_(int32_t offset, int32_t count) {
    _lastVisitedLine.first = 0;
    _lastVisitedLine.second = "";

    if (count <= 0 || root == SENTINEL) {
        return;
    }

    // 确保不超出缓冲区长度
    int32_t maxLength = getLength();
    if (offset >= maxLength) {
        return; // 起始点超出缓冲区，不执行操作
    }
    
    // 确保不会删除超出缓冲区的部分
    if (offset + count > maxLength) {
        count = maxLength - offset;
    }

    NodePosition startPosition = nodeAt(offset);
    NodePosition endPosition = nodeAt(offset + count);
    TreeNode* startNode = startPosition.node;
    TreeNode* endNode = endPosition.node;

    if (!startNode || !endNode) {
        // 防御性检查，确保节点存在
        return;
    }

    if (startNode == endNode) {
        BufferCursor startSplitPosInBuffer = positionInBuffer(startNode, startPosition.remainder);
        BufferCursor endSplitPosInBuffer = positionInBuffer(startNode, endPosition.remainder);

        if (startPosition.nodeStartOffset == offset) {
            if (count == startNode->piece->length) { // delete node
                TreeNode* next = startNode->next();
                rbDelete(this, startNode);
                validateCRLFWithPrevNode(next);
                computeBufferMetadata();
                return;
            }
            deleteNodeHead(startNode, endSplitPosInBuffer);
            _searchCache->validate(offset);
            validateCRLFWithPrevNode(startNode);
            computeBufferMetadata();
            return;
        }

        if (startPosition.nodeStartOffset + startNode->piece->length == offset + count) {
            deleteNodeTail(startNode, startSplitPosInBuffer);
            validateCRLFWithNextNode(startNode);
            computeBufferMetadata();
            return;
        }

        // delete content in the middle, this node will be splitted to nodes
        shrinkNode(startNode, startSplitPosInBuffer, endSplitPosInBuffer);
        computeBufferMetadata();
        return;
    }

    std::vector<TreeNode*> nodesToDel;

    // 处理起始节点
    BufferCursor startSplitPosInBuffer = positionInBuffer(startNode, startPosition.remainder);
    deleteNodeTail(startNode, startSplitPosInBuffer);
    _searchCache->validate(offset);
    bool startNodeEmpty = (startNode->piece->length == 0);
    if (startNodeEmpty) {
        nodesToDel.push_back(startNode);
    }

    // 处理结束节点
    BufferCursor endSplitPosInBuffer = positionInBuffer(endNode, endPosition.remainder);
    deleteNodeHead(endNode, endSplitPosInBuffer);
    bool endNodeEmpty = (endNode->piece->length == 0);
    if (endNodeEmpty) {
        nodesToDel.push_back(endNode);
    }

    // 删除中间的节点
    // 确保secondNode不是endNode，防止错误删除
    TreeNode* secondNode = startNode->next();
    for (TreeNode* node = secondNode; node != SENTINEL && node != endNode; node = node->next()) {
        nodesToDel.push_back(node);
    }

    // 查找需要验证CRLF连接的节点
    TreeNode* prevNode = nullptr;
    TreeNode* nextNode = nullptr;
    
    if (!startNodeEmpty) {
        prevNode = startNode;
        nextNode = endNodeEmpty ? endNode->next() : endNode;
    } else {
        prevNode = startNode->prev();
        nextNode = endNodeEmpty ? endNode->next() : endNode;
    }
    
    // 先删除需要删除的节点
    deleteNodes(nodesToDel);
    
    // 然后再验证CRLF连接
    // 确保前节点有效
    if (prevNode && prevNode != SENTINEL) {
        if (nextNode && nextNode != SENTINEL) {
            // 验证前后节点的CRLF连接
            if (shouldCheckCRLF() && endWithCR(prevNode) && startWithLF(nextNode)) {
                // 处理CRLF连接
                handleCRLFJoin(prevNode, nextNode);
            }
        }
    } else if (nextNode && nextNode != SENTINEL) {
        // 只有后节点有效
        validateCRLFWithPrevNode(nextNode);
    }
    
    computeBufferMetadata();
}

// 新增处理CRLF连接的辅助函数
void PieceTreeBase::handleCRLFJoin(TreeNode* prevNode, TreeNode* nextNode) {
    if (!prevNode || !nextNode || prevNode == SENTINEL || nextNode == SENTINEL) {
        return;
    }
    
    if (!endWithCR(prevNode) || !startWithLF(nextNode)) {
        return;  // 没有CRLF需要处理
    }
    
    // 调整前节点删除最后的\r
    Piece* prevPiece = prevNode->piece;
    BufferCursor originalEnd = prevPiece->end;
    int32_t originalLength = prevPiece->length;
    int32_t originalLFCnt = prevPiece->lineFeedCnt;
    
    // 获取前节点最后位置的前一个位置（去掉\r）
    BufferCursor newEnd;
    if (prevPiece->end.column > 0) {
        newEnd = {prevPiece->end.line, prevPiece->end.column - 1};
    } else if (prevPiece->end.line > 0) {
        // 需要回到前一行的末尾
        int32_t prevLineIndex = prevPiece->end.line - 1;
        int32_t prevLineLength = _buffers[prevPiece->bufferIndex].lineStarts[prevPiece->end.line] - 
                              _buffers[prevPiece->bufferIndex].lineStarts[prevLineIndex];
        newEnd = {prevLineIndex, prevLineLength};
    } else {
        // 无法调整，避免无效操作
        return;
    }
    
    int32_t newLength = originalLength - 1;
    int32_t newLFCnt = getLineFeedCnt(prevPiece->bufferIndex, prevPiece->start, newEnd);
    int32_t lf_delta = newLFCnt - originalLFCnt;
    
    Piece* newPiece = new Piece(
        prevPiece->bufferIndex,
        prevPiece->start,
        newEnd,
        newLFCnt,
        newLength
    );
    
    delete prevNode->piece;
    prevNode->piece = newPiece;
    updateTreeMetadata(this, prevNode, -1, lf_delta);
    
    // 调整后节点，将第一个\n变为\r\n
    // 这里我们不实际修改后节点内容，而是在逻辑上使其与前节点连接正确
}

void PieceTreeBase::insertContentToNodeLeft(const std::string& value, TreeNode* node) {
    std::vector<TreeNode*> nodesToDel;
    if (shouldCheckCRLF() && endWithCR(value) && startWithLF(node)) {
        Piece* piece = node->piece;
        BufferCursor newStart{piece->start.line + 1, 0};
        Piece* nPiece = new Piece(
            piece->bufferIndex,
            newStart,
            piece->end,
            getLineFeedCnt(piece->bufferIndex, newStart, piece->end),
            piece->length - 1
        );

        node->piece = nPiece;
        delete piece;

        std::string newValue = value + '\n';
        updateTreeMetadata(this, node, -1, -1);

        if (node->piece->length == 0) {
            nodesToDel.push_back(node);
        }

        std::vector<Piece*> newPieces = createNewPieces(newValue);
        TreeNode* newNode = rbInsertLeft(node, newPieces[newPieces.size() - 1]);
        for (int32_t k = newPieces.size() - 2; k >= 0; k--) {
            newNode = rbInsertLeft(newNode, newPieces[k]);
        }
        validateCRLFWithPrevNode(newNode);
        deleteNodes(nodesToDel);
        return;
    }

    std::vector<Piece*> newPieces = createNewPieces(value);
    TreeNode* newNode = rbInsertLeft(node, newPieces[newPieces.size() - 1]);
    for (int32_t k = newPieces.size() - 2; k >= 0; k--) {
        newNode = rbInsertLeft(newNode, newPieces[k]);
    }
    validateCRLFWithPrevNode(newNode);
    deleteNodes(nodesToDel);
}

void PieceTreeBase::insertContentToNodeRight(const std::string& value, TreeNode* node) {
    std::string newValue = value;
    if (adjustCarriageReturnFromNext(newValue, node)) {
        newValue += '\n';
    }

    std::vector<Piece*> newPieces = createNewPieces(newValue);
    TreeNode* newNode = rbInsertRight(node, newPieces[0]);
    TreeNode* tmpNode = newNode;

    for (size_t k = 1; k < newPieces.size(); k++) {
        tmpNode = rbInsertRight(tmpNode, newPieces[k]);
    }

    validateCRLFWithPrevNode(newNode);
}

BufferCursor PieceTreeBase::positionInBuffer(TreeNode* node, int32_t remainder) {
    Piece* piece = node->piece;
    int32_t bufferIndex = node->piece->bufferIndex;
    const std::vector<int32_t>& lineStarts = _buffers[bufferIndex].lineStarts;

    int32_t startOffset = lineStarts[piece->start.line] + piece->start.column;
    int32_t offset = startOffset + remainder;

    int32_t low = piece->start.line;
    int32_t high = piece->end.line;

    int32_t mid = 0;
    int32_t midStop = 0;
    int32_t midStart = 0;

    while (low <= high) {
        mid = low + ((high - low) / 2);
        midStart = lineStarts[mid];

        if (mid == high) {
            break;
        }

        midStop = lineStarts[mid + 1];

        if (offset < midStart) {
            high = mid - 1;
        } else if (offset >= midStop) {
            low = mid + 1;
        } else {
            break;
        }
    }

    return {mid, offset - midStart};
}

int32_t PieceTreeBase::getLineFeedCnt(int32_t bufferIndex, const BufferCursor& start, const BufferCursor& end) {
    if (end.column == 0) {
        return end.line - start.line;
    }

    const std::vector<int32_t>& lineStarts = _buffers[bufferIndex].lineStarts;
    if (end.line == lineStarts.size() - 1) {
        return end.line - start.line;
    }

    int32_t nextLineStartOffset = lineStarts[end.line + 1];
    int32_t endOffset = lineStarts[end.line] + end.column;
    if (nextLineStartOffset > endOffset + 1) {
        return end.line - start.line;
    }

    int32_t previousCharOffset = endOffset - 1;
    const std::string& buffer = _buffers[bufferIndex].buffer;

    if (static_cast<unsigned char>(buffer[previousCharOffset]) == 13) {
        return end.line - start.line + 1;
    } else {
        return end.line - start.line;
    }
}

int32_t PieceTreeBase::offsetInBuffer(int32_t bufferIndex, const BufferCursor& cursor) const {
    const std::vector<int32_t>& lineStarts = _buffers[bufferIndex].lineStarts;
    return lineStarts[cursor.line] + cursor.column;
}

void PieceTreeBase::deleteNodes(const std::vector<TreeNode*>& nodes) {
    for (TreeNode* node : nodes) {
        rbDelete(this, node);
    }
}

std::vector<Piece*> PieceTreeBase::createNewPieces(const std::string& text) {
    if (text.length() > AverageBufferSize) {
        std::vector<Piece*> newPieces;
        std::string remainingText = text;
        
        while (remainingText.length() > AverageBufferSize) {
            // 找到合适的分割点，避免切分UTF-8字符和CRLF
            int32_t splitPos = AverageBufferSize - 1; // 从平均大小前一个字符开始检查
            bool foundSplitPos = false;
            
            // 检查最后一个字符
            unsigned char lastChar = static_cast<unsigned char>(remainingText[splitPos]);
            
            // 如果最后一个字符是CR或者是UTF-8多字节字符的首字节，回退一个位置
            if (lastChar == 13 || (lastChar >= 0xD8 && lastChar <= 0xDB)) {
                splitPos--;
            }
            
            // 确保分割点在合理范围内
            splitPos = std::max(splitPos, 0);
            
            std::string splitText = remainingText.substr(0, splitPos + 1);
            remainingText = remainingText.substr(splitPos + 1);

            std::vector<int32_t> lineStarts = createLineStartsFast(splitText);
            if (lineStarts.empty()) {
                lineStarts.push_back(0);  // 确保至少有一个行起始位置
            }
            
            newPieces.push_back(new Piece(
                _buffers.size(),
                {0, 0},
                {static_cast<int32_t>(lineStarts.size() - 1), 
                 static_cast<int32_t>(splitText.length() - lineStarts[lineStarts.size() - 1])},
                lineStarts.size() - 1,
                splitText.length()
            ));
            _buffers.push_back(StringBuffer(splitText, lineStarts));
        }

        // 处理剩余部分
        if (!remainingText.empty()) {
            std::vector<int32_t> lineStarts = createLineStartsFast(remainingText);
            if (lineStarts.empty()) {
                lineStarts.push_back(0);  // 确保至少有一个行起始位置
            }
            
            newPieces.push_back(new Piece(
                _buffers.size(),
                {0, 0},
                {static_cast<int32_t>(lineStarts.size() - 1), 
                 static_cast<int32_t>(remainingText.length() - lineStarts[lineStarts.size() - 1])},
                lineStarts.size() - 1,
                remainingText.length()
            ));
            _buffers.push_back(StringBuffer(remainingText, lineStarts));
        }

        return newPieces;
    }

    int32_t startOffset = _buffers[0].buffer.length();
    std::vector<int32_t> lineStarts = createLineStartsFast(text);
    if (lineStarts.empty()) {
        lineStarts.push_back(0);  // 确保至少有一个行起始位置
    }

    BufferCursor start = _lastChangeBufferPos;
    if (_buffers[0].lineStarts.size() > 0 && 
        _buffers[0].lineStarts[_buffers[0].lineStarts.size() - 1] == startOffset
        && startOffset != 0
        && startWithLF(text)
        && endWithCR(_buffers[0].buffer)) {
        _lastChangeBufferPos = {_lastChangeBufferPos.line, _lastChangeBufferPos.column + 1};
        start = _lastChangeBufferPos;
        
        // 注意: VSCode实现在这里添加了一个特殊字符'_'，以确保缓冲区增加正确的偏移量
        // 我们也添加一个填充字符确保对齐
        _buffers[0].buffer += '_' + text;
        
        for (size_t i = 1; i < lineStarts.size(); i++) {
            _buffers[0].lineStarts.push_back(lineStarts[i] + startOffset + 1); // +1 for the added '_'
        }
        
        startOffset += 1; // 考虑添加的填充字符
    } else {
        _buffers[0].buffer += text;
        for (size_t i = 1; i < lineStarts.size(); i++) {
            _buffers[0].lineStarts.push_back(lineStarts[i] + startOffset);
        }
    }
    
    BufferCursor end{static_cast<int32_t>(_buffers[0].lineStarts.size() - 1), 
                   static_cast<int32_t>(_buffers[0].buffer.length() - _buffers[0].lineStarts.back())};
    _lastChangeBufferPos = end;
    
    Piece* piece = new Piece(0, start, end, end.line - start.line, _buffers[0].buffer.length() - startOffset);
    return {piece};
}

TreeNode* PieceTreeBase::rbInsertLeft(TreeNode* node, Piece* piece) {
    TreeNode* z = new TreeNode(piece, NodeColor::Red);
    z->left = SENTINEL;
    z->right = SENTINEL;
    z->parent = SENTINEL;
    z->size_left = 0;
    z->lf_left = 0;

    if (root == SENTINEL) {
        root = z;
        z->color = NodeColor::Black;
    } else if (node->left == SENTINEL) {
        node->left = z;
        z->parent = node;
    } else {
        TreeNode* prevNode = rightest(node->left);
        prevNode->right = z;
        z->parent = prevNode;
    }

    fixInsert(this, z);
    return z;
}

TreeNode* PieceTreeBase::rbInsertRight(TreeNode* node, Piece* piece) {
    TreeNode* z = new TreeNode(piece, NodeColor::Red);
    z->left = SENTINEL;
    z->right = SENTINEL;
    z->parent = SENTINEL;
    z->size_left = 0;
    z->lf_left = 0;

    if (root == SENTINEL) {
        root = z;
        z->color = NodeColor::Black;
    } else if (node->right == SENTINEL) {
        node->right = z;
        z->parent = node;
    } else {
        TreeNode* nextNode = leftest(node->right);
        nextNode->left = z;
        z->parent = nextNode;
    }

    fixInsert(this, z);
    return z;
}

std::string PieceTreeBase::getLinesRawContent() {
    return getContentOfSubTree(root);
}

std::string PieceTreeBase::getLineRawContent(int32_t lineNumber, int32_t endOffset) {
    TreeNode* x = root;
    std::string ret;
    CacheEntry* cache = _searchCache->get2(lineNumber);
    
    if (cache) {
        x = cache->node;
        int32_t prevAccumualtedValue = getAccumulatedValue(x, lineNumber - cache->nodeStartLineNumber - 1);
        std::string buffer = _buffers[x->piece->bufferIndex].buffer;
        int32_t startOffset = offsetInBuffer(x->piece->bufferIndex, x->piece->start);
        
        if (cache->nodeStartLineNumber + x->piece->lineFeedCnt == lineNumber) {
            ret = buffer.substr(startOffset + prevAccumualtedValue, 
                              startOffset + x->piece->length - (startOffset + prevAccumualtedValue));
        } else {
            int32_t accumualtedValue = getAccumulatedValue(x, lineNumber - cache->nodeStartLineNumber);
            return buffer.substr(startOffset + prevAccumualtedValue, 
                               startOffset + accumualtedValue - endOffset - (startOffset + prevAccumualtedValue));
        }
    } else {
        int32_t nodeStartOffset = 0;
        const int32_t originalLineNumber = lineNumber;
        
        while (x != SENTINEL) {
            if (x->left != SENTINEL && x->lf_left >= lineNumber - 1) {
                x = x->left;
            } else if (x->lf_left + x->piece->lineFeedCnt > lineNumber - 1) {
                int32_t prevAccumualtedValue = getAccumulatedValue(x, lineNumber - x->lf_left - 2);
                int32_t accumualtedValue = getAccumulatedValue(x, lineNumber - x->lf_left - 1);
                std::string buffer = _buffers[x->piece->bufferIndex].buffer;
                int32_t startOffset = offsetInBuffer(x->piece->bufferIndex, x->piece->start);
                nodeStartOffset += x->size_left;
                
                _searchCache->set(CacheEntry(x, nodeStartOffset, 
                    originalLineNumber - (lineNumber - 1 - x->lf_left)));
                
                return buffer.substr(startOffset + prevAccumualtedValue, 
                                   startOffset + accumualtedValue - endOffset - (startOffset + prevAccumualtedValue));
            } else if (x->lf_left + x->piece->lineFeedCnt == lineNumber - 1) {
                int32_t prevAccumualtedValue = getAccumulatedValue(x, lineNumber - x->lf_left - 2);
                std::string buffer = _buffers[x->piece->bufferIndex].buffer;
                int32_t startOffset = offsetInBuffer(x->piece->bufferIndex, x->piece->start);
                
                ret = buffer.substr(startOffset + prevAccumualtedValue, 
                                  startOffset + x->piece->length - (startOffset + prevAccumualtedValue));
                break;
            } else {
                lineNumber -= x->lf_left + x->piece->lineFeedCnt;
                nodeStartOffset += x->size_left + x->piece->length;
                x = x->right;
            }
        }
    }

    x = x->next();
    while (x != SENTINEL) {
        std::string buffer = _buffers[x->piece->bufferIndex].buffer;

        if (x->piece->lineFeedCnt > 0) {
            int32_t accumualtedValue = getAccumulatedValue(x, 0);
            int32_t startOffset = offsetInBuffer(x->piece->bufferIndex, x->piece->start);
            
            ret += buffer.substr(startOffset, startOffset + accumualtedValue - endOffset - startOffset);
            return ret;
        } else {
            int32_t startOffset = offsetInBuffer(x->piece->bufferIndex, x->piece->start);
            ret += buffer.substr(startOffset, x->piece->length);
        }

        x = x->next();
    }

    return ret;
}

void PieceTreeBase::computeBufferMetadata() {
    _lineCnt = 1;
    _length = 0;

    for (TreeNode* node = minimum(root); node != SENTINEL; node = node->next()) {
        Piece* piece = node->piece;
        if (piece) {
            _length += piece->length;
            _lineCnt += piece->lineFeedCnt;
        }
    }
    
    // 确保_lineCnt和_length始终为非负值
    if (_lineCnt < 1) _lineCnt = 1;
    if (_length < 0) _length = 0;
}

std::pair<int32_t, int32_t> PieceTreeBase::getIndexOf(TreeNode* node, int32_t accumulatedValue) {
    Piece* piece = node->piece;
    BufferCursor pos = positionInBuffer(node, accumulatedValue);
    int32_t lineCnt = pos.line - piece->start.line;

    if (offsetInBuffer(piece->bufferIndex, piece->end) - 
        offsetInBuffer(piece->bufferIndex, piece->start) == accumulatedValue) {
        int32_t realLineCnt = getLineFeedCnt(node->piece->bufferIndex, piece->start, pos);
        if (realLineCnt != lineCnt) {
            return {realLineCnt, 0};
        }
    }

    return {lineCnt, pos.column};
}

int32_t PieceTreeBase::getAccumulatedValue(TreeNode* node, int32_t index) {
    if (index < 0) {
        return 0;
    }
    Piece* piece = node->piece;
    const std::vector<int32_t>& lineStarts = _buffers[piece->bufferIndex].lineStarts;
    int32_t expectedLineStartIndex = piece->start.line + index + 1;
    if (expectedLineStartIndex > piece->end.line) {
        return lineStarts[piece->end.line] + piece->end.column - 
               lineStarts[piece->start.line] - piece->start.column;
    } else {
        return lineStarts[expectedLineStartIndex] - 
               lineStarts[piece->start.line] - piece->start.column;
    }
}

void PieceTreeBase::deleteNodeTail(TreeNode* node, const BufferCursor& pos) {
    Piece* piece = node->piece;
    int32_t originalLFCnt = piece->lineFeedCnt;
    int32_t originalEndOffset = offsetInBuffer(piece->bufferIndex, piece->end);

    const BufferCursor& newEnd = pos;
    int32_t newEndOffset = offsetInBuffer(piece->bufferIndex, newEnd);
    int32_t newLineFeedCnt = getLineFeedCnt(piece->bufferIndex, piece->start, newEnd);

    int32_t lf_delta = newLineFeedCnt - originalLFCnt;
    int32_t size_delta = newEndOffset - originalEndOffset;
    int32_t newLength = piece->length + size_delta;

    Piece* newPiece = new Piece(
        piece->bufferIndex,
        piece->start,
        newEnd,
        newLineFeedCnt,
        newLength
    );

    delete node->piece;
    node->piece = newPiece;

    updateTreeMetadata(this, node, size_delta, lf_delta);
}

void PieceTreeBase::deleteNodeHead(TreeNode* node, const BufferCursor& pos) {
    Piece* piece = node->piece;
    int32_t originalLFCnt = piece->lineFeedCnt;
    int32_t originalStartOffset = offsetInBuffer(piece->bufferIndex, piece->start);

    const BufferCursor& newStart = pos;
    int32_t newLineFeedCnt = getLineFeedCnt(piece->bufferIndex, newStart, piece->end);
    int32_t newStartOffset = offsetInBuffer(piece->bufferIndex, newStart);
    int32_t lf_delta = newLineFeedCnt - originalLFCnt;
    int32_t size_delta = originalStartOffset - newStartOffset;
    int32_t newLength = piece->length + size_delta;

    Piece* newPiece = new Piece(
        piece->bufferIndex,
        newStart,
        piece->end,
        newLineFeedCnt,
        newLength
    );

    delete node->piece;
    node->piece = newPiece;

    updateTreeMetadata(this, node, size_delta, lf_delta);
}

// 优化shrinkNode方法，确保在处理大文本时不会发生内存泄漏
void PieceTreeBase::shrinkNode(TreeNode* node, const BufferCursor& start, const BufferCursor& end) {
    if (!node || node == SENTINEL) {
        return;
    }
    
    Piece* piece = node->piece;
    if (!piece) {
        return;
    }

    // 计算新的左侧片段
    int32_t startOffset = offsetInBuffer(piece->bufferIndex, piece->start);
    int32_t midStartOffset = offsetInBuffer(piece->bufferIndex, start);
    int32_t midEndOffset = offsetInBuffer(piece->bufferIndex, end);
    int32_t endOffset = offsetInBuffer(piece->bufferIndex, piece->end);
    
    int32_t leftLength = midStartOffset - startOffset;
    if (leftLength <= 0) {
        // 如果左侧长度为0或负数，只保留右侧部分
        deleteNodeHead(node, end);
        return;
    }

    // 计算新的右侧片段
    int32_t rightLength = endOffset - midEndOffset;
    if (rightLength <= 0) {
        // 如果右侧长度为0或负数，只保留左侧部分
        deleteNodeTail(node, start);
        return;
    }

    // 需要分割节点，左侧保留在原节点，右侧创建新节点
    int32_t totalLF = piece->lineFeedCnt;
    int32_t leftLFCnt = getLineFeedCnt(piece->bufferIndex, piece->start, start);
    int32_t middleLFCnt = getLineFeedCnt(piece->bufferIndex, start, end);
    int32_t rightLFCnt = totalLF - leftLFCnt - middleLFCnt;
    
    // 为确保计算正确，防御性检查
    if (rightLFCnt < 0) rightLFCnt = 0;
    
    // 创建右侧片段
    Piece* rightPiece = new Piece(
        piece->bufferIndex,
        end,
        piece->end,
        rightLFCnt,
        rightLength
    );
    
    // 更新左侧片段（当前节点）
    Piece* leftPiece = new Piece(
        piece->bufferIndex,
        piece->start,
        start,
        leftLFCnt,
        leftLength
    );
    
    // 清理原来的piece，防止内存泄漏
    delete piece;
    node->piece = leftPiece;
    
    // 更新树元数据
    int32_t sizeDelta = -(rightLength + (midEndOffset - midStartOffset));
    int32_t lfDelta = -(rightLFCnt + middleLFCnt);
    updateTreeMetadata(this, node, sizeDelta, lfDelta);
    
    // 插入右侧节点
    TreeNode* rightNode = rbInsertRight(node, rightPiece);
    
    // 验证CRLF处理
    validateCRLFWithNextNode(node);
}

void PieceTreeBase::appendToNode(TreeNode* node, const std::string& value) {
    std::string newValue = value;
    if (adjustCarriageReturnFromNext(newValue, node)) {
        newValue += '\n';
    }

    const bool hitCRLF = shouldCheckCRLF() && startWithLF(newValue) && endWithCR(node);
    const int32_t startOffset = _buffers[0].buffer.length();
    _buffers[0].buffer += newValue;
    std::vector<int32_t> lineStarts = createLineStartsFast(newValue);
    
    for (size_t i = 0; i < lineStarts.size(); i++) {
        lineStarts[i] += startOffset;
    }
    
    if (hitCRLF) {
        int32_t prevStartOffset = _buffers[0].lineStarts[_buffers[0].lineStarts.size() - 2];
        _buffers[0].lineStarts.pop_back();
        _lastChangeBufferPos = {_lastChangeBufferPos.line - 1, 
                              startOffset - prevStartOffset};
    }

    _buffers[0].lineStarts.insert(_buffers[0].lineStarts.end(), 
                                lineStarts.begin() + 1, lineStarts.end());
    
    const int32_t endIndex = _buffers[0].lineStarts.size() - 1;
    const int32_t endColumn = _buffers[0].buffer.length() - _buffers[0].lineStarts[endIndex];
    const BufferCursor newEnd{endIndex, endColumn};
    const int32_t newLength = node->piece->length + newValue.length();
    const int32_t oldLineFeedCnt = node->piece->lineFeedCnt;
    const int32_t newLineFeedCnt = getLineFeedCnt(0, node->piece->start, newEnd);
    const int32_t lf_delta = newLineFeedCnt - oldLineFeedCnt;

    Piece* newPiece = new Piece(
        node->piece->bufferIndex,
        node->piece->start,
        newEnd,
        newLineFeedCnt,
        newLength
    );

    delete node->piece;
    node->piece = newPiece;

    _lastChangeBufferPos = newEnd;
    updateTreeMetadata(this, node, newValue.length(), lf_delta);
}

NodePosition PieceTreeBase::nodeAt(int32_t offset) {
    // Safety check - if the offset is out of bounds, return last node
    int32_t currentLength = getLength();
    if (offset > currentLength) {
        offset = currentLength;
    }
    
    TreeNode* x = root;
    CacheEntry* cache = _searchCache->get(offset);
    
    if (cache) {
        return NodePosition(cache->node, offset - cache->nodeStartOffset, cache->nodeStartOffset);
    }

    int32_t nodeStartOffset = 0;

    while (x != SENTINEL) {
        if (x->size_left > offset) {
            x = x->left;
        } else if (x->size_left + x->piece->length >= offset) {
            nodeStartOffset += x->size_left;
            NodePosition ret(x, offset - x->size_left, nodeStartOffset);
            _searchCache->set(CacheEntry(x, nodeStartOffset));
            return ret;
        } else {
            offset -= x->size_left + x->piece->length;
            nodeStartOffset += x->size_left + x->piece->length;
            x = x->right;
        }
    }

    return NodePosition();
}

NodePosition PieceTreeBase::nodeAt2(int32_t lineNumber, int32_t column) {
    // Line numbers in the buffer are 0-based
    TreeNode* x = root;
    int32_t nodeStartOffset = 0;

    while (x != SENTINEL) {
        if (x->left != SENTINEL && x->lf_left >= lineNumber) {
            // Line is in left subtree
            x = x->left;
        } else if (x->lf_left + x->piece->lineFeedCnt > lineNumber) {
            // Line is in this node
            int32_t prevLineIndex = lineNumber - 1;
            int32_t prevAccumulatedValue = (prevLineIndex >= x->lf_left) ? 
                                          getAccumulatedValue(x, prevLineIndex - x->lf_left) : 0;
            int32_t accumualtedValue = getAccumulatedValue(x, lineNumber - x->lf_left);
            nodeStartOffset += x->size_left;
            
            return NodePosition(x, 
                std::min(prevAccumulatedValue + column, accumualtedValue),
                nodeStartOffset);
        } else if (x->lf_left + x->piece->lineFeedCnt == lineNumber) {
            // This node ends exactly at the line we want
            int32_t prevLineIndex = lineNumber - 1;
            int32_t prevAccumulatedValue = (prevLineIndex >= x->lf_left) ? 
                                          getAccumulatedValue(x, prevLineIndex - x->lf_left) : 0;
            nodeStartOffset += x->size_left;
            
            if (prevAccumulatedValue + column <= x->piece->length) {
                return NodePosition(x, prevAccumulatedValue + column, nodeStartOffset);
            } else {
                column -= x->piece->length - prevAccumulatedValue;
                break;
            }
        } else {
            // Line is in right subtree
            lineNumber -= x->lf_left + x->piece->lineFeedCnt;
            nodeStartOffset += x->size_left + x->piece->length;
            x = x->right;
        }
    }

    x = x->next();
    while (x != SENTINEL) {
        if (x->piece->lineFeedCnt > 0) {
            int32_t accumualtedValue = getAccumulatedValue(x, 0);
            int32_t nodeStartOffset = offsetOfNode(x);
            return NodePosition(x, std::min(column, accumualtedValue), nodeStartOffset);
        } else {
            if (x->piece->length >= column) {
                int32_t nodeStartOffset = offsetOfNode(x);
                return NodePosition(x, column, nodeStartOffset);
            } else {
                column -= x->piece->length;
            }
        }

        x = x->next();
    }

    return NodePosition();
}

uint32_t PieceTreeBase::nodeCharCodeAt(TreeNode* node, int32_t offset) {
    if (node->piece->lineFeedCnt < 1) {
        return 0;
    }
    const std::string& buffer = _buffers[node->piece->bufferIndex].buffer;
    int32_t newOffset = offsetInBuffer(node->piece->bufferIndex, node->piece->start) + offset;
    return static_cast<unsigned char>(buffer[newOffset]);
}

int32_t PieceTreeBase::offsetOfNode(TreeNode* node) {
    if (!node) {
        return 0;
    }
    int32_t pos = node->size_left;
    while (node != root) {
        if (node->parent->right == node) {
            pos += node->parent->size_left + node->parent->piece->length;
        }
        node = node->parent;
    }
    return pos;
}

bool PieceTreeBase::shouldCheckCRLF() const {
    return !_EOLNormalized;
}

bool PieceTreeBase::startWithLF(const std::string& val) {
    if (val.length() > 0) {
        return val[0] == '\n';
    }
    return false;
}

bool PieceTreeBase::startWithLF(TreeNode* val) const {
    if (!val || val == SENTINEL || !val->piece) {
        return false;
    }

    BufferCursor start = val->piece->start;
    if (start.line >= _buffers[val->piece->bufferIndex].lineStarts.size()) {
        return false;
    }

    int32_t pos = _buffers[val->piece->bufferIndex].lineStarts[start.line] + start.column;
    return pos < _buffers[val->piece->bufferIndex].buffer.length() && _buffers[val->piece->bufferIndex].buffer[pos] == '\n';
}

bool PieceTreeBase::endWithCR(const std::string& val) {
    if (val.length() > 0) {
        return val[val.length() - 1] == '\r';
    }
    return false;
}

bool PieceTreeBase::endWithCR(TreeNode* val) const {
    if (!val || val == SENTINEL || !val->piece) {
        return false;
    }

    BufferCursor end = val->piece->end;
    if (end.line >= _buffers[val->piece->bufferIndex].lineStarts.size()) {
        return false;
    }

    int32_t pos = _buffers[val->piece->bufferIndex].lineStarts[end.line] + end.column;
    return pos > 0 && pos <= _buffers[val->piece->bufferIndex].buffer.length() && _buffers[val->piece->bufferIndex].buffer[pos - 1] == '\r';
}

// 验证与前一个节点的CRLF连接
void PieceTreeBase::validateCRLFWithPrevNode(TreeNode* node) {
    if (!node || node == SENTINEL || !shouldCheckCRLF()) {
        return;
    }

    TreeNode* prev = node->prev();
    if (prev == SENTINEL) {
        return;
    }

    if (endWithCR(prev) && startWithLF(node)) {
        // 处理CRLF连接
        handleCRLFJoin(prev, node);
    }
}

// 验证与后一个节点的CRLF连接
void PieceTreeBase::validateCRLFWithNextNode(TreeNode* node) {
    if (!node || node == SENTINEL || !shouldCheckCRLF()) {
        return;
    }

    TreeNode* next = node->next();
    if (next == SENTINEL) {
        return;
    }

    if (endWithCR(node) && startWithLF(next)) {
        // 处理CRLF连接
        handleCRLFJoin(node, next);
    }
}

void PieceTreeBase::fixCRLF(TreeNode* prev, TreeNode* next) {
    std::vector<TreeNode*> nodesToDel;
    
    const std::vector<int32_t>& lineStarts = _buffers[prev->piece->bufferIndex].lineStarts;
    BufferCursor newEnd;
    
    if (prev->piece->end.column == 0) {
        newEnd = {prev->piece->end.line - 1, 
                 lineStarts[prev->piece->end.line] - lineStarts[prev->piece->end.line - 1] - 1};
    } else {
        newEnd = {prev->piece->end.line, prev->piece->end.column - 1};
    }

    const int32_t prevNewLength = prev->piece->length - 1;
    const int32_t prevNewLFCnt = prev->piece->lineFeedCnt - 1;
    
    Piece* newPrevPiece = new Piece(
        prev->piece->bufferIndex,
        prev->piece->start,
        newEnd,
        prevNewLFCnt,
        prevNewLength
    );

    delete prev->piece;
    prev->piece = newPrevPiece;

    updateTreeMetadata(this, prev, -1, -1);
    if (prev->piece->length == 0) {
        nodesToDel.push_back(prev);
    }

    BufferCursor newStart{next->piece->start.line + 1, 0};
    const int32_t newLength = next->piece->length - 1;
    const int32_t newLineFeedCnt = getLineFeedCnt(next->piece->bufferIndex, newStart, next->piece->end);
    
    Piece* newNextPiece = new Piece(
        next->piece->bufferIndex,
        newStart,
        next->piece->end,
        newLineFeedCnt,
        newLength
    );

    delete next->piece;
    next->piece = newNextPiece;

    updateTreeMetadata(this, next, -1, -1);
    if (next->piece->length == 0) {
        nodesToDel.push_back(next);
    }

    std::vector<Piece*> pieces = createNewPieces("\r\n");
    rbInsertRight(prev, pieces[0]);

    for (TreeNode* node : nodesToDel) {
        rbDelete(this, node);
    }
}

bool PieceTreeBase::adjustCarriageReturnFromNext(const std::string& value, TreeNode* node) {
    if (shouldCheckCRLF() && endWithCR(value)) {
        TreeNode* nextNode = node->next();
        if (startWithLF(nextNode)) {
            std::string newValue = value + '\n';

            if (nextNode->piece->length == 1) {
                rbDelete(this, nextNode);
            } else {
                Piece* piece = nextNode->piece;
                BufferCursor newStart{piece->start.line + 1, 0};
                const int32_t newLength = piece->length - 1;
                const int32_t newLineFeedCnt = getLineFeedCnt(piece->bufferIndex, newStart, piece->end);
                
                Piece* newPiece = new Piece(
                    piece->bufferIndex,
                    newStart,
                    piece->end,
                    newLineFeedCnt,
                    newLength
                );

                delete nextNode->piece;
                nextNode->piece = newPiece;

                updateTreeMetadata(this, nextNode, -1, -1);
            }
            return true;
        }
    }

    return false;
}

bool PieceTreeBase::iterate(TreeNode* node, const std::function<bool(TreeNode*)>& callback) const {
    if (node == SENTINEL) {
        return callback(SENTINEL);
    }

    bool leftRet = iterate(node->left, callback);
    if (!leftRet) {
        return leftRet;
    }

    return callback(node) && iterate(node->right, callback);
}

std::string PieceTreeBase::getNodeContent(TreeNode* node) const {
    if (node == SENTINEL) {
        return "";
    }
    const std::string& buffer = _buffers[node->piece->bufferIndex].buffer;
    int32_t startOffset = offsetInBuffer(node->piece->bufferIndex, node->piece->start);
    int32_t endOffset = offsetInBuffer(node->piece->bufferIndex, node->piece->end);
    return buffer.substr(startOffset, endOffset - startOffset);
}

std::string PieceTreeBase::getPieceContent(Piece* piece) const {
    const std::string& buffer = _buffers[piece->bufferIndex].buffer;
    int32_t startOffset = offsetInBuffer(piece->bufferIndex, piece->start);
    int32_t endOffset = offsetInBuffer(piece->bufferIndex, piece->end);
    return buffer.substr(startOffset, endOffset - startOffset);
}

int32_t PieceTreeBase::countLineFeedsInNode(TreeNode* node, int32_t startOffset, int32_t endOffset) {
    if (node->piece->lineFeedCnt < 1) {
        return 0;
    }
    const std::string& buffer = _buffers[node->piece->bufferIndex].buffer;
    int32_t start = offsetInBuffer(node->piece->bufferIndex, node->piece->start) + startOffset;
    int32_t end = offsetInBuffer(node->piece->bufferIndex, node->piece->start) + endOffset;
    int32_t count = 0;
    for (int32_t i = start; i < end; i++) {
        if (static_cast<unsigned char>(buffer[i]) == 10) {
            count++;
        }
    }
    return count;
}

void PieceTreeBase::deleteNodeRange(TreeNode* node, int32_t startOffset, int32_t endOffset) {
    if (startOffset == 0) {
        deleteNodeHead(node, positionInBuffer(node, endOffset));
    } else if (endOffset == node->piece->length) {
        deleteNodeTail(node, positionInBuffer(node, startOffset));
    } else {
        shrinkNode(node, positionInBuffer(node, startOffset), positionInBuffer(node, endOffset));
    }
}

TreeNode* PieceTreeBase::leftest(TreeNode* node) {
    while (node->left != SENTINEL) {
        node = node->left;
    }
    return node;
}

TreeNode* PieceTreeBase::rightest(TreeNode* node) {
    while (node->right != SENTINEL) {
        node = node->right;
    }
    return node;
}

std::string PieceTreeBase::getContentOfSubTree(TreeNode* node) {
    std::string str;
    iterate(node, [&](TreeNode* node) {
        str += getNodeContent(node);
        return true;
    });
    return str;
}

void PieceTreeBase::initializeSentinel() {
    SENTINEL->color = NodeColor::Black;
    SENTINEL->parent = nullptr;
    SENTINEL->left = nullptr;
    SENTINEL->right = nullptr;
    SENTINEL->piece = nullptr;
    SENTINEL->size_left = 0;
    SENTINEL->lf_left = 0;
}

void PieceTreeBase::deleteText(int32_t offset, int32_t count) {
    _lastVisitedLine.first = 0;
    _lastVisitedLine.second = "";

    if (count <= 0 || root == SENTINEL) {
        return;
    }

    // Ensure offset is within bounds
    int32_t maxLength = getLength();
    if (offset >= maxLength) {
        return; // Offset is beyond the buffer
    }
    
    // Ensure we don't delete beyond the end of the buffer
    if (offset + count > maxLength) {
        count = maxLength - offset;
    }

    // For very large deletions, use chunked approach
    const int32_t CHUNK_SIZE = AverageBufferSize / 2;
    if (count > CHUNK_SIZE && count > maxLength / 10) { // Only chunk if large enough
        int32_t remaining = count;
        int32_t currentOffset = offset;
        
        while (remaining > 0) {
            int32_t deleteSize = std::min(CHUNK_SIZE, remaining);
            // Handle each chunk with a separate delete operation
            delete_(currentOffset, deleteSize);
            
            remaining -= deleteSize;
            // Note: currentOffset doesn't change because content shifts left after deletion
        }
        
        return; // Already computed buffer metadata in each delete_ call
    }

    // Normal deletion flow
    NodePosition startPosition = nodeAt(offset);
    NodePosition endPosition = nodeAt(offset + count);
    TreeNode* startNode = startPosition.node;
    TreeNode* endNode = endPosition.node;

    if (!startNode || !endNode) {
        return; // Defensive check
    }

    if (startNode == endNode) {
        // Deletion within a single node
        BufferCursor startSplitPosInBuffer = positionInBuffer(startNode, startPosition.remainder);
        BufferCursor endSplitPosInBuffer = positionInBuffer(startNode, endPosition.remainder);

        if (startPosition.nodeStartOffset == offset) {
            if (count == startNode->piece->length) { // delete entire node
                TreeNode* next = startNode->next();
                rbDelete(this, startNode);
                validateCRLFWithPrevNode(next);
                computeBufferMetadata();
                return;
            }
            deleteNodeHead(startNode, endSplitPosInBuffer);
            _searchCache->validate(offset);
            validateCRLFWithPrevNode(startNode);
            computeBufferMetadata();
            return;
        }

        if (startPosition.nodeStartOffset + startNode->piece->length == offset + count) {
            deleteNodeTail(startNode, startSplitPosInBuffer);
            validateCRLFWithNextNode(startNode);
            computeBufferMetadata();
            return;
        }

        // Delete content in the middle, split the node
        shrinkNode(startNode, startSplitPosInBuffer, endSplitPosInBuffer);
        computeBufferMetadata();
        return;
    }

    std::vector<TreeNode*> nodesToDel;

    // Handle start node
    BufferCursor startSplitPosInBuffer = positionInBuffer(startNode, startPosition.remainder);
    deleteNodeTail(startNode, startSplitPosInBuffer);
    _searchCache->validate(offset);
    bool startNodeEmpty = (startNode->piece->length == 0);
    if (startNodeEmpty) {
        nodesToDel.push_back(startNode);
    }

    // Handle end node
    BufferCursor endSplitPosInBuffer = positionInBuffer(endNode, endPosition.remainder);
    deleteNodeHead(endNode, endSplitPosInBuffer);
    bool endNodeEmpty = (endNode->piece->length == 0);
    if (endNodeEmpty) {
        nodesToDel.push_back(endNode);
    }

    // Delete all nodes between start and end (exclusive)
    TreeNode* secondNode = startNode->next();
    for (TreeNode* node = secondNode; node != SENTINEL && node != endNode; node = node->next()) {
        nodesToDel.push_back(node);
    }

    // Determine nodes for CRLF validation
    TreeNode* prevNode = startNodeEmpty ? startNode->prev() : startNode;
    TreeNode* nextNode = endNodeEmpty ? endNode->next() : endNode;
    
    // First delete nodes
    deleteNodes(nodesToDel);
    
    // Then handle CRLF connections
    if (prevNode && prevNode != SENTINEL && nextNode && nextNode != SENTINEL) {
        // Check if we need to handle CRLF split across nodes
        if (shouldCheckCRLF() && endWithCR(prevNode) && startWithLF(nextNode)) {
            // Handle the CR+LF sequence
            fixCRLF(prevNode, nextNode);
        }
    } else if (nextNode && nextNode != SENTINEL) {
        validateCRLFWithPrevNode(nextNode);
    } else if (prevNode && prevNode != SENTINEL) {
        validateCRLFWithNextNode(prevNode);
    }
    
    computeBufferMetadata();
}

void PieceTreeBase::fixInsert(PieceTreeBase* tree, TreeNode* x) {
    while (x != tree->root && x->parent->color == NodeColor::Red) {
        if (x->parent == x->parent->parent->left) {
            TreeNode* y = x->parent->parent->right;
            if (y->color == NodeColor::Red) {
                x->parent->color = NodeColor::Black;
                y->color = NodeColor::Black;
                x->parent->parent->color = NodeColor::Red;
                x = x->parent->parent;
            } else {
                if (x == x->parent->right) {
                    x = x->parent;
                    rotateLeft(tree, x);
                }
                x->parent->color = NodeColor::Black;
                x->parent->parent->color = NodeColor::Red;
                rotateRight(tree, x->parent->parent);
            }
        } else {
            TreeNode* y = x->parent->parent->left;
            if (y->color == NodeColor::Red) {
                x->parent->color = NodeColor::Black;
                y->color = NodeColor::Black;
                x->parent->parent->color = NodeColor::Red;
                x = x->parent->parent;
            } else {
                if (x == x->parent->left) {
                    x = x->parent;
                    rotateRight(tree, x);
                }
                x->parent->color = NodeColor::Black;
                x->parent->parent->color = NodeColor::Red;
                rotateLeft(tree, x->parent->parent);
            }
        }
    }
    tree->root->color = NodeColor::Black;
}

void PieceTreeBase::fixDelete(PieceTreeBase* tree, TreeNode* x) {
    while (x != tree->root && x->color == NodeColor::Black) {
        if (x == x->parent->left) {
            TreeNode* w = x->parent->right;
            if (w->color == NodeColor::Red) {
                w->color = NodeColor::Black;
                x->parent->color = NodeColor::Red;
                rotateLeft(tree, x->parent);
                w = x->parent->right;
            }
            if (w->left->color == NodeColor::Black && w->right->color == NodeColor::Black) {
                w->color = NodeColor::Red;
                x = x->parent;
            } else {
                if (w->right->color == NodeColor::Black) {
                    w->left->color = NodeColor::Black;
                    w->color = NodeColor::Red;
                    rotateRight(tree, w);
                    w = x->parent->right;
                }
                w->color = x->parent->color;
                x->parent->color = NodeColor::Black;
                w->right->color = NodeColor::Black;
                rotateLeft(tree, x->parent);
                x = tree->root;
            }
        } else {
            TreeNode* w = x->parent->left;
            if (w->color == NodeColor::Red) {
                w->color = NodeColor::Black;
                x->parent->color = NodeColor::Red;
                rotateRight(tree, x->parent);
                w = x->parent->left;
            }
            if (w->right->color == NodeColor::Black && w->left->color == NodeColor::Black) {
                w->color = NodeColor::Red;
                x = x->parent;
            } else {
                if (w->left->color == NodeColor::Black) {
                    w->right->color = NodeColor::Black;
                    w->color = NodeColor::Red;
                    rotateLeft(tree, w);
                    w = x->parent->left;
                }
                w->color = x->parent->color;
                x->parent->color = NodeColor::Black;
                w->left->color = NodeColor::Black;
                rotateRight(tree, x->parent);
                x = tree->root;
            }
        }
    }
    x->color = NodeColor::Black;
}

void PieceTreeBase::rotateLeft(PieceTreeBase* tree, TreeNode* x) {
    TreeNode* y = x->right;
    x->right = y->left;
    if (y->left != SENTINEL) {
        y->left->parent = x;
    }
    y->parent = x->parent;
    if (x->parent == SENTINEL) {
        tree->root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    y->left = x;
    x->parent = y;

    // 更新metadata - 修复用于计算节点大小和行数的逻辑
    // 旧代码: y->size_left = x->size_left + x->left->size_left + x->left->piece->length;
    // 旧代码: y->lf_left = x->lf_left + x->left->lf_left + x->left->piece->lineFeedCnt;
    
    // x成为y的左子节点，所以y的左侧大小现在包括x的大小
    y->size_left = x->size_left + (x->left != SENTINEL ? x->left->size_left + x->left->piece->length : 0) + x->piece->length;
    y->lf_left = x->lf_left + (x->left != SENTINEL ? x->left->lf_left + x->left->piece->lineFeedCnt : 0) + x->piece->lineFeedCnt;
    
    // x的右子节点变了，需要重新计算x的metadata
    // x现在的右子节点是y的原左子节点
    x->size_left = (x->left != SENTINEL ? x->left->size_left + x->left->piece->length : 0);
    x->lf_left = (x->left != SENTINEL ? x->left->lf_left + x->left->piece->lineFeedCnt : 0);
}

void PieceTreeBase::rotateRight(PieceTreeBase* tree, TreeNode* y) {
    TreeNode* x = y->left;
    y->left = x->right;
    if (x->right != SENTINEL) {
        x->right->parent = y;
    }
    x->parent = y->parent;
    if (y->parent == SENTINEL) {
        tree->root = x;
    } else if (y == y->parent->right) {
        y->parent->right = x;
    } else {
        y->parent->left = x;
    }
    x->right = y;
    y->parent = x;

    // 更新metadata - 修复用于计算节点大小和行数的逻辑
    // 旧代码: y->size_left = y->left->size_left + y->left->piece->length;
    // 旧代码: y->lf_left = y->left->lf_left + y->left->piece->lineFeedCnt;
    
    // 更新y的左子树metadata - y的左子节点变成了x的右子节点
    y->size_left = (y->left != SENTINEL ? y->left->size_left + y->left->piece->length : 0);
    y->lf_left = (y->left != SENTINEL ? y->left->lf_left + y->left->piece->lineFeedCnt : 0);
    
    // 更新x的右子树metadata - x的右子节点变成了y
    // x原来的右子节点变成了y的左子节点
    x->size_left = x->left != SENTINEL ? x->left->size_left + x->left->piece->length : 0;
    x->lf_left = x->left != SENTINEL ? x->left->lf_left + x->left->piece->lineFeedCnt : 0;
}

void PieceTreeBase::rbDelete(PieceTreeBase* tree, TreeNode* z) {
    // Adapted from "Introduction to Algorithms" by Cormen et al.
    TreeNode* y = z;
    TreeNode* x;
    NodeColor y_original_color = y->color;

    if (z->left == SENTINEL) {
        x = z->right;
        transplant(tree, z, z->right);
        updateTreeMetadata(tree, x, -z->piece->length, -z->piece->lineFeedCnt);
    } else if (z->right == SENTINEL) {
        x = z->left;
        transplant(tree, z, z->left);
        updateTreeMetadata(tree, x, -z->piece->length, -z->piece->lineFeedCnt);
    } else {
        y = minimum(z->right);
        y_original_color = y->color;
        x = y->right;

        if (y->parent == z) {
            x->parent = y;
        } else {
            transplant(tree, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }

        transplant(tree, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
        y->size_left = z->size_left;
        y->lf_left = z->lf_left;
    }

    delete z->piece;
    delete z;

    if (y_original_color == NodeColor::Black) {
        fixDelete(tree, x);
    }
}

TreeNode* PieceTreeBase::minimum(TreeNode* node) {
    while (node->left != SENTINEL) {
        node = node->left;
    }
    return node;
}

void PieceTreeBase::transplant(PieceTreeBase* tree, TreeNode* u, TreeNode* v) {
    if (u->parent == SENTINEL) {
        tree->root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

void PieceTreeBase::updateMetadata(TreeNode* node) {
    node->size_left = node->left->size_left + node->left->piece->length;
    node->lf_left = node->left->lf_left + node->left->piece->lineFeedCnt;
}

void PieceTreeBase::updateTreeMetadata(PieceTreeBase* tree, TreeNode* x, int32_t lengthDeltaLeft, int32_t lfDeltaLeft) {
    if (x == SENTINEL) {
        return;
    }

    if (lengthDeltaLeft != 0 || lfDeltaLeft != 0) {
        // Update size_left and lf_left of all nodes in the path to the root
        TreeNode* node = x;
        while (node != SENTINEL) {
            node->size_left += lengthDeltaLeft;
            node->lf_left += lfDeltaLeft;
            node = node->parent;
        }
    }
}

int PieceTreeBase::countLineFeeds(const std::string& content) {
    int count = 0;
    for (char c : content) {
        if (c == '\n') {
            count++;
        }
    }
    return count;
}

} // namespace textbuffer 