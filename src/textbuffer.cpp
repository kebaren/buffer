#include "textbuffer/textbuffer.h"

namespace textbuffer {

TextBuffer::TextBuffer(DefaultEndOfLine eol) {
    // Create an empty buffer with the specified EOL
    PieceTreeTextBufferBuilder builder;
    auto factory = builder.finish();
    _buffer = factory.create(eol);
}

TextBuffer::TextBuffer(const std::string& initialContent, DefaultEndOfLine eol) {
    // Create a builder and add the initial content
    PieceTreeTextBufferBuilder builder;
    builder.acceptChunk(initialContent);
    
    // Create the buffer with the specified EOL
    auto factory = builder.finish();
    _buffer = factory.create(eol);
}

TextBuffer::~TextBuffer() = default;

std::string TextBuffer::getValue() const {
    return _buffer->getValue();
}

std::string TextBuffer::getValueInRange(const common::Range& range) const {
    return _buffer->getValueInRange(range);
}

int32_t TextBuffer::getLength() const {
    return _buffer->getLength();
}

int32_t TextBuffer::getLineCount() const {
    return _buffer->getLineCount();
}

std::string TextBuffer::getLineContent(int32_t lineNumber) const {
    return _buffer->getLineContent(lineNumber);
}

int32_t TextBuffer::getLineLength(int32_t lineNumber) const {
    return _buffer->getLineLength(lineNumber);
}

std::string TextBuffer::getEOL() const {
    return _buffer->getEOL();
}

void TextBuffer::setEOL(const std::string& eol) {
    _buffer->setEOL(eol);
}

common::Position TextBuffer::getPositionAt(int32_t offset) const {
    return _buffer->getPositionAt(offset);
}

int32_t TextBuffer::getOffsetAt(int32_t lineNumber, int32_t column) const {
    return _buffer->getOffsetAt(lineNumber, column);
}

void TextBuffer::insert(int32_t offset, const std::string& text, bool eolNormalized) {
    _buffer->insert(offset, text, eolNormalized);
}

void TextBuffer::deleteText(int32_t offset, int32_t count) {
    _buffer->deleteText(offset, count);
}

std::unique_ptr<ITextSnapshot> TextBuffer::createSnapshot(const std::string& BOM) const {
    return _buffer->createSnapshot(BOM);
}

} // namespace textbuffer 