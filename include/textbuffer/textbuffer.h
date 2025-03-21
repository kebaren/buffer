#pragma once

#include "piece_tree_base.h"
#include "piece_tree_builder.h"
#include "common/position.h"
#include "common/range.h"

namespace textbuffer {

/**
 * TextBuffer class - A wrapper for PieceTreeBase that provides text editing functionality
 * This class encapsulates the piece tree implementation and provides a simpler interface
 * for text manipulation operations.
 */
class TextBuffer {
public:
    /**
     * Create an empty TextBuffer with the specified end-of-line sequence
     * 
     * @param eol The end-of-line sequence to use (LF or CRLF)
     */
    explicit TextBuffer(DefaultEndOfLine eol = DefaultEndOfLine::LF);

    /**
     * Create a TextBuffer with initial content
     * 
     * @param initialContent The initial content to populate the buffer with
     * @param eol The end-of-line sequence to use (LF or CRLF)
     */
    TextBuffer(const std::string& initialContent, DefaultEndOfLine eol = DefaultEndOfLine::LF);

    /**
     * Destructor
     */
    ~TextBuffer();

    /**
     * Get the entire content of the buffer as a string
     * 
     * @return The content of the buffer
     */
    std::string getValue() const;

    /**
     * Get the content in the specified range
     * 
     * @param range The range to get content from
     * @return The content in the specified range
     */
    std::string getValueInRange(const common::Range& range) const;

    /**
     * Get the length of the buffer in characters
     * 
     * @return The length of the buffer
     */
    int32_t getLength() const;

    /**
     * Get the number of lines in the buffer
     * 
     * @return The number of lines
     */
    int32_t getLineCount() const;

    /**
     * Get the content of a specific line
     * 
     * @param lineNumber The zero-based line number
     * @return The content of the specified line
     */
    std::string getLineContent(int32_t lineNumber) const;

    /**
     * Get the length of a specific line
     * 
     * @param lineNumber The zero-based line number
     * @return The length of the specified line
     */
    int32_t getLineLength(int32_t lineNumber) const;

    /**
     * Get the end-of-line sequence used by this buffer
     * 
     * @return The EOL sequence ("\n" or "\r\n")
     */
    std::string getEOL() const;

    /**
     * Set the end-of-line sequence used by this buffer
     * 
     * @param eol The new EOL sequence
     */
    void setEOL(const std::string& eol);

    /**
     * Get the position at a specific offset
     * 
     * @param offset The character offset in the buffer
     * @return The position (line and column)
     */
    common::Position getPositionAt(int32_t offset) const;

    /**
     * Get the offset for a specific position
     * 
     * @param lineNumber The line number (zero-based)
     * @param column The column (zero-based)
     * @return The character offset in the buffer
     */
    int32_t getOffsetAt(int32_t lineNumber, int32_t column) const;

    /**
     * Insert text at the specified offset
     * 
     * @param offset The character offset to insert at
     * @param text The text to insert
     * @param eolNormalized Whether the text's EOL is already normalized
     */
    void insert(int32_t offset, const std::string& text, bool eolNormalized = false);

    /**
     * Delete text at the specified offset
     * 
     * @param offset The character offset to delete from
     * @param count The number of characters to delete
     */
    void deleteText(int32_t offset, int32_t count);

    /**
     * Create a snapshot of the buffer
     * 
     * @param BOM The byte order mark to include (if any)
     * @return A unique_ptr to an ITextSnapshot
     */
    std::unique_ptr<ITextSnapshot> createSnapshot(const std::string& BOM = "") const;

private:
    std::unique_ptr<PieceTreeBase> _buffer;
};

} // namespace textbuffer 