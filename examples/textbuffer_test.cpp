#include <iostream>
#include <cassert>
#include <string>
#include "textbuffer/textbuffer.h"

using namespace textbuffer;

int main() {
    std::cout << "=== TextBuffer Wrapper Test ===" << std::endl;

    // Test 1: Empty buffer creation
    std::cout << "Testing empty buffer creation..." << std::endl;
    TextBuffer emptyBuffer(DefaultEndOfLine::LF);
    assert(emptyBuffer.getLength() == 0);
    assert(emptyBuffer.getLineCount() == 1);
    assert(emptyBuffer.getValue() == "");
    assert(emptyBuffer.getEOL() == "\n");
    
    // Test 2: Buffer with initial content
    std::cout << "Testing buffer with initial content..." << std::endl;
    TextBuffer contentBuffer("Hello\nWorld", DefaultEndOfLine::LF);
    assert(contentBuffer.getLength() == 11);
    assert(contentBuffer.getLineCount() == 2);
    assert(contentBuffer.getValue() == "Hello\nWorld");
    assert(contentBuffer.getLineContent(0) == "Hello");
    assert(contentBuffer.getLineContent(1) == "World");
    assert(contentBuffer.getLineLength(0) == 5);
    
    // Test 3: Insert operations
    std::cout << "Testing insert operations..." << std::endl;
    TextBuffer insertBuffer(DefaultEndOfLine::LF);
    insertBuffer.insert(0, "Hello", false);
    assert(insertBuffer.getValue() == "Hello");
    
    insertBuffer.insert(5, " World", false);
    assert(insertBuffer.getValue() == "Hello World");
    
    insertBuffer.insert(5, "\nNew Line\n", false);
    assert(insertBuffer.getValue() == "Hello\nNew Line\n World");
    assert(insertBuffer.getLineCount() == 3);
    
    // Test 4: Delete operations
    std::cout << "Testing delete operations..." << std::endl;
    TextBuffer deleteBuffer("ABCDEFGHIJKLMNOPQRSTUVWXYZ", DefaultEndOfLine::LF);
    deleteBuffer.deleteText(10, 5);
    assert(deleteBuffer.getValue() == "ABCDEFGHIJPQRSTUVWXYZ");
    
    deleteBuffer.deleteText(0, 5);
    assert(deleteBuffer.getValue() == "FGHIJPQRSTUVWXYZ");
    
    deleteBuffer.deleteText(deleteBuffer.getLength() - 3, 3);
    assert(deleteBuffer.getValue() == "FGHIJPQRSTUVW");
    
    // Test 5: Position and offset conversion
    std::cout << "Testing position and offset conversion..." << std::endl;
    TextBuffer posBuffer("Line1\nLine2\nLine3", DefaultEndOfLine::LF);
    
    // Check positions
    common::Position pos1 = posBuffer.getPositionAt(0);
    assert(pos1.lineNumber() == 1 && pos1.column() == 1);
    
    common::Position pos2 = posBuffer.getPositionAt(6);
    assert(pos2.lineNumber() == 2 && pos2.column() == 1);
    
    // Check offsets
    int32_t offset1 = posBuffer.getOffsetAt(0, 0);
    assert(offset1 == 0);
    
    int32_t offset2 = posBuffer.getOffsetAt(1, 2);
    assert(offset2 == 8);
    
    // Test 6: Range operations
    std::cout << "Testing range operations..." << std::endl;
    TextBuffer rangeBuffer("Line1\nLine2\nLine3\nLine4", DefaultEndOfLine::LF);
    
    common::Range range(1, 3, 3, 2);
    std::string rangeContent = rangeBuffer.getValueInRange(range);
    assert(rangeContent == "ine2\nL");
    
    // Test 7: EOL handling
    std::cout << "Testing EOL handling..." << std::endl;
    TextBuffer eolBuffer("Line1\r\nLine2\r\nLine3", DefaultEndOfLine::CRLF);
    assert(eolBuffer.getEOL() == "\r\n");
    assert(eolBuffer.getLineCount() == 3);
    
    eolBuffer.setEOL("\n");
    assert(eolBuffer.getEOL() == "\n");
    
    // Test 8: Snapshot creation
    std::cout << "Testing snapshot creation..." << std::endl;
    TextBuffer snapshotBuffer("Snapshot test", DefaultEndOfLine::LF);
    auto snapshot = snapshotBuffer.createSnapshot();
    assert(snapshot);
    
    // Modify buffer after snapshot
    snapshotBuffer.insert(0, "Modified: ", false);
    assert(snapshotBuffer.getValue() == "Modified: Snapshot test");
    
    // Snapshot should remain unchanged
    std::string snapshotContent = snapshot->read();
    assert(snapshotContent == "Snapshot test");
    
    std::cout << "All TextBuffer wrapper tests passed successfully!" << std::endl;
    
    return 0;
} 