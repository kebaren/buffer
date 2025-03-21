#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <utility>
#include "textbuffer/common/position.h"
#include "textbuffer/common/range.h"
#include "textbuffer/common/char_code.h"
#include "rb_tree_base.h"

namespace textbuffer {



/**
 * Text snapshot interface
 */
class ITextSnapshot {
public:
    virtual ~ITextSnapshot() = default;
    virtual std::string read() = 0;
};

// Average buffer size for chunking
constexpr int32_t AverageBufferSize = 65535;

/**
 * Utility function to create an appropriate sized uint array
 */
template <typename T>
std::unique_ptr<T[]> createUintArray(const std::vector<int32_t>& arr) {
    auto result = std::make_unique<T[]>(arr.size());
    for (size_t i = 0; i < arr.size(); i++) {
        result[i] = static_cast<T>(arr[i]);
    }
    return result;
}

/**
 * Line starts information for a buffer
 */
class LineStarts {
public:
    std::vector<int32_t> lineStarts;
    int32_t cr;
    int32_t lf;
    int32_t crlf;
    bool isBasicASCII;

    LineStarts(std::vector<int32_t> lineStarts, int32_t cr, int32_t lf, int32_t crlf, bool isBasicASCII)
        : lineStarts(std::move(lineStarts)), cr(cr), lf(lf), crlf(crlf), isBasicASCII(isBasicASCII) {}
};

/**
 * Create line starts array quickly (just detects line breaks)
 */
std::vector<int32_t> createLineStartsFast(const std::string& str);

/**
 * Create full line starts information including CR, LF and CRLF counts
 */
LineStarts createLineStarts(const std::string& str);

/**
 * Node position in the piece tree
 */
struct NodePosition {
    TreeNode* node;
    int32_t remainder;
    int32_t nodeStartOffset;

    NodePosition() : node(nullptr), remainder(0), nodeStartOffset(0) {}
    
    NodePosition(TreeNode* node, int32_t remainder, int32_t nodeStartOffset)
        : node(node), remainder(remainder), nodeStartOffset(nodeStartOffset) {}
};

/**
 * Buffer cursor position (line and column)
 */
struct BufferCursor {
    int32_t line;
    int32_t column;

    BufferCursor() : line(0), column(0) {}
    
    BufferCursor(int32_t line, int32_t column)
        : line(line), column(column) {}
    
    bool operator==(const BufferCursor& other) const {
        return line == other.line && column == other.column;
    }
    
    operator common::Position() const {
        return common::Position(line, column);
    }
};

/**
 * Piece of text in the buffer
 */
class Piece {
public:
    int32_t bufferIndex;
    BufferCursor start;
    BufferCursor end;
    int32_t length;
    int32_t lineFeedCnt;

    Piece(int32_t bufferIndex, const BufferCursor& start, const BufferCursor& end, int32_t lineFeedCnt, int32_t length)
        : bufferIndex(bufferIndex), start(start), end(end), length(length), lineFeedCnt(lineFeedCnt) {}
};

/**
 * String buffer with line starts information
 */
class StringBuffer {
public:
    std::string buffer;
    std::vector<int32_t> lineStarts;
    int32_t cr;
    int32_t lf;
    int32_t crlf;
    bool isBasicASCII;

    StringBuffer() : cr(0), lf(0), crlf(0), isBasicASCII(true) {}
    StringBuffer(std::string buffer, std::vector<int32_t> lineStarts)
        : buffer(std::move(buffer)), lineStarts(std::move(lineStarts)), cr(0), lf(0), crlf(0), isBasicASCII(true) {
        computeLineBreakCounts();
    }

private:
    void computeLineBreakCounts() {
        cr = 0;
        lf = 0;
        crlf = 0;
        isBasicASCII = true;

        for (size_t i = 0; i < buffer.length(); i++) {
            char c = buffer[i];
            if (c == '\r') {
                if (i + 1 < buffer.length() && buffer[i + 1] == '\n') {
                    crlf++;
                    i++; // Skip the next LF
                } else {
                    cr++;
                }
            } else if (c == '\n') {
                lf++;
            } else if (static_cast<unsigned char>(c) >= 128) {
                isBasicASCII = false;
            }
        }
    }
};

/**
 * Cache entry for the piece tree search
 */
struct CacheEntry {
    TreeNode* node;
    int32_t nodeStartOffset;
    int32_t nodeStartLineNumber;

    CacheEntry(TreeNode* node, int32_t nodeStartOffset, int32_t nodeStartLineNumber = 1)
        : node(node), nodeStartOffset(nodeStartOffset), nodeStartLineNumber(nodeStartLineNumber) {}
};

/**
 * Search cache for the piece tree
 */
class PieceTreeSearchCache {
private:
    int32_t _limit;
    std::vector<CacheEntry> _cache;

public:
    PieceTreeSearchCache(int32_t limit) : _limit(limit) {}
    
    /**
     * Get a cache entry for the given offset
     */
    CacheEntry* get(int32_t offset) {
        for (auto& entry : _cache) {
            if (entry.nodeStartOffset <= offset && 
                entry.nodeStartOffset + entry.node->piece->length >= offset) {
                return &entry;
            }
        }
        return nullptr;
    }
    
    /**
     * Get a cache entry for the given line number
     */
    CacheEntry* get2(int32_t lineNumber) {
        for (auto& entry : _cache) {
            if (entry.nodeStartLineNumber <= lineNumber && 
                entry.nodeStartLineNumber + entry.node->piece->lineFeedCnt >= lineNumber) {
                return &entry;
            }
        }
        return nullptr;
    }
    
    /**
     * Set a cache entry
     */
    void set(const CacheEntry& entry) {
        _cache.push_back(entry);
        if (_cache.size() > _limit) {
            _cache.erase(_cache.begin());
        }
    }
    
    /**
     * Validate a cache entry at the given offset
     */
    void validate(int32_t offset) {
        for (auto it = _cache.begin(); it != _cache.end();) {
            if (it->nodeStartOffset <= offset && 
                it->nodeStartOffset + it->node->piece->length >= offset) {
                ++it;
            } else {
                it = _cache.erase(it);
            }
        }
    }
};

/**
 * Piece Tree Base implementation
 */
class PieceTreeBase {
public:
    TreeNode* root;

protected:
    std::vector<StringBuffer> _buffers; // 0 is change buffer, others are readonly original buffer.
    int32_t _lineCnt;
    int32_t _length;
    std::string _EOL;
    int32_t _EOLLength;
    bool _EOLNormalized;
    BufferCursor _lastChangeBufferPos;
    std::unique_ptr<PieceTreeSearchCache> _searchCache;
    std::pair<int32_t, std::string> _lastVisitedLine;
    static const int AverageBufferSize = 65535;

public:
    PieceTreeBase() : root(SENTINEL), _lineCnt(1), _length(0), _EOLNormalized(false), _lastChangeBufferPos(1, 1) {
        _buffers.emplace_back();
        _searchCache = std::make_unique<PieceTreeSearchCache>(10);
        _lastVisitedLine = std::make_pair(0, "");
    }

    /**
     * Create a piece tree from chunks
     */
    void create(const std::vector<StringBuffer>& chunks, const std::string& eol, bool eolNormalized);

    /**
     * Normalize EOL in the buffer
     */
    void normalizeEOL(const std::string& eol);

    /**
     * Get the EOL sequence
     */
    std::string getEOL() const;

    /**
     * Set the EOL sequence
     */
    void setEOL(const std::string& newEOL);

    /**
     * Create a snapshot of the buffer
     */
    std::unique_ptr<ITextSnapshot> createSnapshot(const std::string& BOM) const;

    /**
     * Check if this buffer equals another buffer
     */
    bool equal(const PieceTreeBase& other) const;

    /**
     * Get the offset at the given line and column
     */
    int32_t getOffsetAt(int32_t lineNumber, int32_t column);

    /**
     * Get the position at the given offset
     */
    common::Position getPositionAt(int32_t offset);

    /**
     * Get the value in the given range
     */
    std::string getValueInRange(const common::Range& range, const std::string& eol = "");

    /**
     * Get the value between two node positions
     */
    std::string getValueInRange2(const NodePosition& startPosition, const NodePosition& endPosition);

    /**
     * Get the content of all lines
     */
    std::vector<std::string> getLinesContent();

    /**
     * Get the buffer length
     */
    int32_t getLength() const;

    /**
     * Get the line count
     */
    int32_t getLineCount() const;

    /**
     * Get the content of a line
     */
    std::string getLineContent(int32_t lineNumber);

    /**
     * Get the char code at the given line and index
     */
    uint32_t getLineCharCode(int32_t lineNumber, int32_t index);

    /**
     * Get the length of a line
     */
    int32_t getLineLength(int32_t lineNumber);

    /**
     * Get the entire text content
     */
    std::string getValue();

    /**
     * Insert text at the given offset
     */
    void insert(int offset, const std::string& value, bool eolNormalized = false);

    /**
     * Delete text at the given offset
     */
    void delete_(int offset, int count);
    
    /**
     * Insert content to the left of a node
     */
    void insertContentToNodeLeft(const std::string& value, TreeNode* node);

    /**
     * Insert content to the right of a node
     */
    void insertContentToNodeRight(const std::string& value, TreeNode* node);

    /**
     * Get the position in a buffer at the given remainder
     */
    BufferCursor positionInBuffer(TreeNode* node, int32_t remainder);

    /**
     * Get the line feed count in a buffer range
     */
    int32_t getLineFeedCnt(int32_t bufferIndex, const BufferCursor& start, const BufferCursor& end);

    /**
     * Get the offset in a buffer at the given cursor
     */
    int32_t offsetInBuffer(int32_t bufferIndex, const BufferCursor& cursor) const;

    /**
     * Delete the given nodes
     */
    void deleteNodes(const std::vector<TreeNode*>& nodes);

    /**
     * Create new pieces from the given text
     */
    std::vector<Piece*> createNewPieces(const std::string& text);

    /**
     * Insert a node with the given piece to the left of the given node
     */
    TreeNode* rbInsertLeft(TreeNode* node, Piece* piece);

    /**
     * Insert a node with the given piece to the right of the given node
     */
    TreeNode* rbInsertRight(TreeNode* node, Piece* piece);

    /**
     * Get the raw content of all lines
     */
    std::string getLinesRawContent();

    /**
     * Get the raw content of a line
     */
    std::string getLineRawContent(int32_t lineNumber, int32_t endOffset = 0);

    /**
     * Compute buffer metadata
     */
    void computeBufferMetadata();

    /**
     * Get the index of a node with the given accumulated value
     */
    std::pair<int32_t, int32_t> getIndexOf(TreeNode* node, int32_t accumulatedValue);

    /**
     * Get the accumulated value at the given index
     */
    int32_t getAccumulatedValue(TreeNode* node, int32_t index);

    /**
     * Delete the tail of a node
     */
    void deleteNodeTail(TreeNode* node, const BufferCursor& pos);

    /**
     * Delete the head of a node
     */
    void deleteNodeHead(TreeNode* node, const BufferCursor& pos);

    /**
     * Shrink a node to the given range
     */
    void shrinkNode(TreeNode* node, const BufferCursor& start, const BufferCursor& end);

    /**
     * Append to a node
     */
    void appendToNode(TreeNode* node, const std::string& value);

    /**
     * Get the node at the given offset
     */
    NodePosition nodeAt(int offset);

    /**
     * Get the node at the given line and column
     */
    NodePosition nodeAt2(int lineNumber, int column);

    /**
     * Get the char code at the given offset in a node
     */
    uint32_t nodeCharCodeAt(TreeNode* node, int32_t offset);

    /**
     * Get the offset of a node
     */
    int32_t offsetOfNode(TreeNode* node);

    /**
     * Check if CRLF should be checked
     */
    bool shouldCheckCRLF() const;

    /**
     * Check if the given value starts with a line feed
     */
    bool startWithLF(TreeNode* node) const;
    bool startWithLF(const std::string& val);

    /**
     * Check if the given value ends with a carriage return
     */
    bool endWithCR(TreeNode* node) const;
    bool endWithCR(const std::string& val);

    /**
     * Validate CRLF with the previous node
     */
    void validateCRLFWithPrevNode(TreeNode* node);

    /**
     * Validate CRLF with the next node
     */
    void validateCRLFWithNextNode(TreeNode* node);

    /**
     * Fix CRLF between two nodes
     */
    void fixCRLF(TreeNode* prev, TreeNode* next);

    /**
     * Adjust carriage return from the next node
     */
    bool adjustCarriageReturnFromNext(const std::string& value, TreeNode* node);

    /**
     * Iterate over the tree
     */
    bool iterate(TreeNode* node, const std::function<bool(TreeNode*)>& callback) const;

    /**
     * Get the content of a node
     */
    std::string getNodeContent(TreeNode* node) const;

    /**
     * Get the content of a piece
     */
    std::string getPieceContent(Piece* piece) const;

    /**
     * Count line feeds in a node between start and end offsets
     */
    int32_t countLineFeedsInNode(TreeNode* node, int32_t startOffset, int32_t endOffset);
    
    /**
     * Delete a range of text in a node
     */
    void deleteNodeRange(TreeNode* node, int32_t startOffset, int32_t endOffset);
    
    /**
     * Get the leftmost node
     */
    TreeNode* leftest(TreeNode* node);
    
    /**
     * Get the rightmost node
     */
    TreeNode* rightest(TreeNode* node);

    /**
     * Get the content of a subtree
     */
    std::string getContentOfSubTree(TreeNode* node);

    /**
     * Initialize sentinel node
     */
    void initializeSentinel();

    void deleteText(int32_t offset, int32_t cnt);

    void deleteTree(TreeNode* node);

    // 处理CRLF连接的辅助函数
    void handleCRLFJoin(TreeNode* prevNode, TreeNode* nextNode);

    // 创建新片段
    std::vector<Piece*> createNewPieces(int32_t bufferIndex, const std::string& value, bool eol_normalization = true);

private:
    // Helper methods for piece tree operations
    void fixInsert(PieceTreeBase* tree, TreeNode* node);
    void fixDelete(PieceTreeBase* tree, TreeNode* node);
    void rotateLeft(PieceTreeBase* tree, TreeNode* node);
    void rotateRight(PieceTreeBase* tree, TreeNode* node);
    void rbDelete(PieceTreeBase* tree, TreeNode* node);
    TreeNode* minimum(TreeNode* node);
    void transplant(PieceTreeBase* tree, TreeNode* u, TreeNode* v);
    void updateMetadata(TreeNode* node);
    void updateTreeMetadata(PieceTreeBase* tree, TreeNode* x, int32_t lengthDeltaLeft, int32_t lfDeltaLeft);

    // Helper methods
    int countLineFeeds(const std::string& content);
};

} // namespace textbuffer 