#include "textbuffer/piece_tree_builder.h"
#include <regex>
#include <algorithm>

namespace textbuffer {

// Factory implementation

PieceTreeTextBufferFactory::PieceTreeTextBufferFactory(
    std::vector<StringBuffer> chunks,
    const std::string& bom,
    int32_t cr,
    int32_t lf,
    int32_t crlf,
    bool normalizeEOL
) : _chunks(std::move(chunks)),
    _bom(bom),
    _cr(cr),
    _lf(lf),
    _crlf(crlf),
    _normalizeEOL(normalizeEOL) {
}

std::string PieceTreeTextBufferFactory::getEOL(DefaultEndOfLine defaultEOL) {
    const int32_t totalEOLCount = _cr + _lf + _crlf;
    const int32_t totalCRCount = _cr + _crlf;
    
    if (totalEOLCount == 0) {
        // This is an empty file or a file with precisely one line
        return (defaultEOL == DefaultEndOfLine::LF ? "\n" : "\r\n");
    }
    
    if (totalCRCount > totalEOLCount / 2) {
        // More than half of the file contains \r\n ending lines
        return "\r\n";
    }
    
    // At least one line more ends in \n
    return "\n";
}

std::unique_ptr<PieceTreeBase> PieceTreeTextBufferFactory::create(DefaultEndOfLine defaultEOL) {
    std::string eol = getEOL(defaultEOL);
    std::vector<StringBuffer> chunks = _chunks;

    if (_normalizeEOL &&
        ((eol == "\r\n" && (_cr > 0 || _lf > 0)) ||
         (eol == "\n" && (_cr > 0 || _crlf > 0)))
    ) {
        // Normalize pieces
        for (size_t i = 0; i < chunks.size(); i++) {
            std::string str = chunks[i].buffer;
            std::regex newlinePattern("\r\n|\r|\n");
            str = std::regex_replace(str, newlinePattern, eol);
            std::vector<int32_t> newLineStart = createLineStartsFast(str);
            chunks[i] = StringBuffer(str, newLineStart);
        }
    }

    auto result = std::make_unique<PieceTreeBase>();
    result->create(chunks, eol, _normalizeEOL);
    return result;
}

std::string PieceTreeTextBufferFactory::getFirstLineText(int32_t lengthLimit) {
    if (_chunks.empty() || _chunks[0].buffer.empty()) {
        return "";
    }
    
    std::string text = _chunks[0].buffer.substr(0, std::min(lengthLimit, static_cast<int32_t>(_chunks[0].buffer.length())));
    
    // Split by any newline and take the first part
    std::regex pattern("\r\n|\r|\n");
    std::sregex_token_iterator iter(text.begin(), text.end(), pattern, -1);
    std::sregex_token_iterator end;
    
    if (iter != end) {
        return *iter;
    }
    
    return text;
}

// Builder implementation

PieceTreeTextBufferBuilder::PieceTreeTextBufferBuilder()
    : _hasPreviousChar(false), _previousChar(0), cr(0), lf(0), crlf(0) {
    BOM = "";
}

void PieceTreeTextBufferBuilder::acceptChunk(const std::string& chunk) {
    if (chunk.empty()) {
        return;
    }

    if (chunks.empty()) {
        if (Unicode::startsWithUTF8BOM(chunk)) {
            BOM = Unicode::UTF8_BOM_CHARACTER;
            _acceptChunk1(chunk.substr(3), false); // Skip BOM
        } else {
            _acceptChunk1(chunk, false);
        }
    } else {
        const uint32_t lastChar = chunk[chunk.length() - 1];
        
        if (lastChar == static_cast<uint32_t>(common::CharCode::CarriageReturn) || 
            (lastChar >= 0xD800 && lastChar <= 0xDBFF)) {
            // Last character is \r or a high surrogate => keep it back
            _acceptChunk1(chunk.substr(0, chunk.length() - 1), false);
            _hasPreviousChar = true;
            _previousChar = lastChar;
        } else {
            _acceptChunk1(chunk, false);
            _hasPreviousChar = false;
            _previousChar = lastChar;
        }
    }
}

void PieceTreeTextBufferBuilder::_acceptChunk1(const std::string& chunk, bool allowEmptyStrings) {
    if (!allowEmptyStrings && chunk.empty()) {
        // Nothing to do
        return;
    }

    if (_hasPreviousChar) {
        std::string combinedChunk;
        combinedChunk.push_back(static_cast<char>(_previousChar));
        combinedChunk.append(chunk);
        _acceptChunk2(combinedChunk);
    } else {
        _acceptChunk2(chunk);
    }
}

void PieceTreeTextBufferBuilder::_acceptChunk2(const std::string& chunk) {
    LineStarts lineStarts = createLineStarts(chunk);
    
    chunks.emplace_back(chunk, lineStarts.lineStarts);
    cr += lineStarts.cr;
    lf += lineStarts.lf;
    crlf += lineStarts.crlf;
}

PieceTreeTextBufferFactory PieceTreeTextBufferBuilder::finish(bool normalizeEOL) {
    _finish();
    return PieceTreeTextBufferFactory(
        chunks,
        BOM,
        cr,
        lf,
        crlf,
        normalizeEOL
    );
}

void PieceTreeTextBufferBuilder::_finish() {
    if (chunks.empty()) {
        _acceptChunk1("", true);
    }

    if (_hasPreviousChar) {
        _hasPreviousChar = false;
        // Recreate last chunk
        StringBuffer& lastChunk = chunks[chunks.size() - 1];
        lastChunk.buffer.push_back(static_cast<char>(_previousChar));
        std::vector<int32_t> newLineStarts = createLineStartsFast(lastChunk.buffer);
        lastChunk.lineStarts = newLineStarts;
        
        if (_previousChar == static_cast<uint32_t>(common::CharCode::CarriageReturn)) {
            cr++;
        }
    }
}

// Helper functions implementation

std::vector<int32_t> createLineStartsFast(const std::string& str) {
    std::vector<int32_t> r = {0};

    for (size_t i = 0, len = str.length(); i < len; i++) {
        const auto chr = static_cast<uint8_t>(str[i]);

        if (chr == static_cast<uint8_t>(common::CharCode::CarriageReturn)) {
            if (i + 1 < len && static_cast<uint8_t>(str[i + 1]) == static_cast<uint8_t>(common::CharCode::LineFeed)) {
                // \r\n case
                r.push_back(static_cast<int32_t>(i + 2));
                i++; // skip \n
            } else {
                // \r case
                r.push_back(static_cast<int32_t>(i + 1));
            }
        } else if (chr == static_cast<uint8_t>(common::CharCode::LineFeed)) {
            // \n case
            r.push_back(static_cast<int32_t>(i + 1));
        }
    }

    return r;
}

LineStarts createLineStarts(const std::string& str) {
    std::vector<int32_t> r = {0};
    int32_t cr = 0, lf = 0, crlf = 0;
    bool isBasicASCII = true;

    for (size_t i = 0, len = str.length(); i < len; i++) {
        const auto chr = static_cast<uint8_t>(str[i]);

        if (chr == static_cast<uint8_t>(common::CharCode::CarriageReturn)) {
            if (i + 1 < len && static_cast<uint8_t>(str[i + 1]) == static_cast<uint8_t>(common::CharCode::LineFeed)) {
                // \r\n case
                crlf++;
                r.push_back(static_cast<int32_t>(i + 2));
                i++; // skip \n
            } else {
                // \r case
                cr++;
                r.push_back(static_cast<int32_t>(i + 1));
            }
        } else if (chr == static_cast<uint8_t>(common::CharCode::LineFeed)) {
            // \n case
            lf++;
            r.push_back(static_cast<int32_t>(i + 1));
        } else {
            if (isBasicASCII) {
                if (chr != static_cast<uint8_t>(common::CharCode::Tab) && (chr < 32 || chr > 126)) {
                    isBasicASCII = false;
                }
            }
        }
    }

    return LineStarts(r, cr, lf, crlf, isBasicASCII);
}

} // namespace textbuffer 