#include <iostream>
#include <string>
#include <cassert>
#include "textbuffer/piece_tree_base.h"

using namespace textbuffer;

// Helper function to create a new PieceTreeBase instance
PieceTreeBase createEmptyBuffer() {
    // Initialize with empty buffer and default settings
    PieceTreeBase buffer;
    
    // Create empty StringBuffer for initialization
    std::vector<StringBuffer> chunks = {StringBuffer("", {0})};
    
    // Configure the buffer with default settings
    buffer.create(chunks, "\n", true);
    
    return buffer;
}

// Simple test to verify buffer insert and retrieval
void testSimpleInsert() {
    std::cout << "Testing simple insert operations..." << std::endl;
    
    // Create a new buffer
    PieceTreeBase buffer = createEmptyBuffer();
    
    // Test empty buffer
    std::string content = buffer.getLinesRawContent();
    std::cout << "Empty buffer content: '" << content << "'" << std::endl;
    std::cout << "Empty buffer length: " << buffer.getLength() << std::endl;
    std::cout << "Empty buffer line count: " << buffer.getLineCount() << std::endl;
    
    // Insert text at beginning
    buffer.insert(0, "Hello");
    content = buffer.getLinesRawContent();
    std::cout << "After insert 'Hello': '" << content << "'" << std::endl;
    std::cout << "Buffer length: " << buffer.getLength() << std::endl;
    
    // Insert text at end
    buffer.insert(5, " World");
    content = buffer.getLinesRawContent();
    std::cout << "After append ' World': '" << content << "'" << std::endl;
    std::cout << "Buffer length: " << buffer.getLength() << std::endl;
    
    // Insert text in middle
    buffer.insert(5, ",");
    content = buffer.getLinesRawContent();
    std::cout << "After insert ',' in middle: '" << content << "'" << std::endl;
    std::cout << "Buffer length: " << buffer.getLength() << std::endl;
    
    // Insert multiline text
    buffer.insert(12, "\nHow are you?");
    content = buffer.getLinesRawContent();
    std::cout << "After insert multiline text: '" << content << "'" << std::endl;
    std::cout << "Buffer length: " << buffer.getLength() << std::endl;
    std::cout << "Buffer line count: " << buffer.getLineCount() << std::endl;
    
    std::cout << "Simple insert tests completed." << std::endl;
}

// Simple test to verify buffer delete operations
void testSimpleDelete() {
    std::cout << "\nTesting simple delete operations..." << std::endl;
    
    // Create a new buffer with initial content
    PieceTreeBase buffer = createEmptyBuffer();
    buffer.insert(0, "Hello, World\nHow are you?\nI am fine, thank you!");
    
    std::string content = buffer.getLinesRawContent();
    std::cout << "Initial content: '" << content << "'" << std::endl;
    std::cout << "Initial length: " << buffer.getLength() << std::endl;
    std::cout << "Initial line count: " << buffer.getLineCount() << std::endl;
    
    // Delete single character
    buffer.deleteText(5, 1); // Delete comma
    content = buffer.getLinesRawContent();
    std::cout << "After delete comma: '" << content << "'" << std::endl;
    std::cout << "Buffer length: " << buffer.getLength() << std::endl;
    
    // Delete word
    buffer.deleteText(5, 6); // Delete " World"
    content = buffer.getLinesRawContent();
    std::cout << "After delete ' World': '" << content << "'" << std::endl;
    std::cout << "Buffer length: " << buffer.getLength() << std::endl;
    
    // Delete across lines
    int startOffset = buffer.getOffsetAt(1, 0);
    int length = buffer.getOffsetAt(2, 0) - startOffset;
    std::cout << "Deleting from offset " << startOffset << " for " << length << " characters" << std::endl;
    buffer.deleteText(startOffset, length);
    content = buffer.getLinesRawContent();
    std::cout << "After delete across lines: '" << content << "'" << std::endl;
    std::cout << "Buffer length: " << buffer.getLength() << std::endl;
    std::cout << "Buffer line count: " << buffer.getLineCount() << std::endl;
    
    std::cout << "Simple delete tests completed." << std::endl;
}

// Test buffer line operations
void testLineOperations() {
    std::cout << "\nTesting line operations..." << std::endl;
    
    // Create a new buffer with multiline content
    PieceTreeBase buffer = createEmptyBuffer();
    buffer.insert(0, "Line 1\nLine 2\nLine 3\nLine 4\nLine 5");
    
    std::string content = buffer.getLinesRawContent();
    std::cout << "Initial content: '" << content << "'" << std::endl;
    std::cout << "Initial length: " << buffer.getLength() << std::endl;
    std::cout << "Initial line count: " << buffer.getLineCount() << std::endl;
    
    // Get line content for each line
    for (int i = 0; i < buffer.getLineCount(); i++) {
        std::string lineContent = buffer.getLineContent(i);
        std::cout << "Line " << i << ": '" << lineContent << "'" << std::endl;
    }
    
    // Get offset at each line
    for (int i = 0; i < buffer.getLineCount(); i++) {
        int offset = buffer.getOffsetAt(i, 0);
        std::cout << "Offset at line " << i << ", column 0: " << offset << std::endl;
    }
    
    // Insert at beginning of line 2
    int line2Offset = buffer.getOffsetAt(2, 0);
    buffer.insert(line2Offset, "INSERTED: ");
    content = buffer.getLinesRawContent();
    std::cout << "After insert at beginning of line 3: '" << content << "'" << std::endl;
    
    // Try to find line information manually
    int middleOffset = buffer.getLength() / 2;
    std::cout << "Middle offset: " << middleOffset << std::endl;
    
    // Find which line contains this offset by checking
    int lineContainingOffset = -1;
    for (int i = 0; i < buffer.getLineCount(); i++) {
        int lineStart = buffer.getOffsetAt(i, 0);
        int lineEnd = (i < buffer.getLineCount() - 1) ? 
                      buffer.getOffsetAt(i + 1, 0) : 
                      buffer.getLength();
        
        if (middleOffset >= lineStart && middleOffset < lineEnd) {
            lineContainingOffset = i;
            break;
        }
    }
    
    std::cout << "Line containing middle offset: " << lineContainingOffset << std::endl;
    
    std::cout << "Line operations tests completed." << std::endl;
}

int main() {
    try {
        std::cout << "Starting Simple Buffer Tests..." << std::endl;
        
        testSimpleInsert();
        testSimpleDelete();
        testLineOperations();
        
        std::cout << "All Simple Buffer Tests completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
} 