#pragma once

#include <string>
#include "piece_tree_base.h"
#include "unicode.h"

namespace textbuffer {

/**
 * End of line sequence
 */
enum class DefaultEndOfLine {
    /**
     * Use line feed (\n) as the end of line character.
     */
    LF = 1,
    /**
     * Use carriage return and line feed (\r\n) as the end of line character.
     */
    CRLF = 2,
    /**
     * Use carriage return (\r) as the end of line character.
     */
    CR = 3
};

/**
 * Factory for creating PieceTreeBase instances
 */
class PieceTreeTextBufferFactory {
private:
    std::vector<StringBuffer> _chunks;
    std::string _bom;
    int32_t _cr;
    int32_t _lf;
    int32_t _crlf;
    bool _normalizeEOL;

public:
    /**
     * Create a factory with the given chunks
     */
    PieceTreeTextBufferFactory(
        std::vector<StringBuffer> chunks,
        const std::string& bom,
        int32_t cr,
        int32_t lf,
        int32_t crlf,
        bool normalizeEOL
    );

    /**
     * Determine the end-of-line character to use
     */
    std::string getEOL(DefaultEndOfLine defaultEOL);

    /**
     * Create a PieceTreeBase instance
     */
    std::unique_ptr<PieceTreeBase> create(DefaultEndOfLine defaultEOL);

    /**
     * Get the first line text limited to a specified length
     */
    std::string getFirstLineText(int32_t lengthLimit);
};

/**
 * Builder for PieceTreeTextBuffer
 */
class PieceTreeTextBufferBuilder {
private:
    std::vector<StringBuffer> chunks;
    std::string BOM;

    bool _hasPreviousChar;
    uint32_t _previousChar;
    std::vector<int32_t> _tmpLineStarts;

    int32_t cr;
    int32_t lf;
    int32_t crlf;

    /**
     * Accept a chunk with a possible preceding character
     */
    void _acceptChunk1(const std::string& chunk, bool allowEmptyStrings);

    /**
     * Accept a chunk of text
     */
    void _acceptChunk2(const std::string& chunk);

    /**
     * Finish accumulating chunks
     */
    void _finish();

public:
    /**
     * Create a new builder
     */
    PieceTreeTextBufferBuilder();

    /**
     * Accept a chunk of text
     */
    void acceptChunk(const std::string& chunk);

    /**
     * Finish building and return a factory
     */
    PieceTreeTextBufferFactory finish(bool normalizeEOL = true);
};

} // namespace textbuffer 