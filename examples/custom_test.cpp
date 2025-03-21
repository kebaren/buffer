#include <iostream>
#include <cassert>
#include <string>
#include "textbuffer/piece_tree_base.h"
#include "textbuffer/piece_tree_builder.h"

using namespace textbuffer;

int main() {
    // Test 1: Multiple inserts and deletes in the middle
    PieceTreeTextBufferBuilder builder;
    auto factory = builder.finish();
    auto buffer = factory.create(DefaultEndOfLine::LF);
    
    // Insert initial content
    buffer->insert(0, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", false);
    assert(buffer->getValue() == "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    
    // Insert in the middle
    buffer->insert(13, "---MIDDLE---", false);
    assert(buffer->getValue() == "ABCDEFGHIJKLM---MIDDLE---NOPQRSTUVWXYZ");
    
    // Delete from middle
    buffer->deleteText(10, 10);
    std::string currentValue = buffer->getValue();
    assert(currentValue == "ABCDEFGHIJ---LE---NOPQRSTUVWXYZ");
    
    // Insert at start
    buffer->insert(0, "START:", false);
    assert(buffer->getValue() == "START:ABCDEFGHIJ---LE---NOPQRSTUVWXYZ");
    
    // Delete from end
    buffer->deleteText(buffer->getLength() - 5, 5);
    assert(buffer->getValue() == "START:ABCDEFGHIJ---LE---NOPQRSTUV");
    
    // Test 2: CRLF handling
    PieceTreeTextBufferBuilder builder2;
    auto factory2 = builder2.finish();
    auto buffer2 = factory2.create(DefaultEndOfLine::CRLF);
    
    buffer2->insert(0, "Line1\r\nLine2\r\nLine3", false);
    assert(buffer2->getLineCount() == 3);
    
    // Insert between CR and LF
    buffer2->insert(5, "X", false);
    
    // Delete across nodes
    buffer2->deleteText(4, 3);
    
    // Test 3: Edge case with empty buffer
    PieceTreeTextBufferBuilder builder3;
    auto factory3 = builder3.finish();
    auto buffer3 = factory3.create(DefaultEndOfLine::LF);
    
    // Delete on empty buffer (should be no-op)
    buffer3->deleteText(0, 5);
    assert(buffer3->getValue() == "");
    
    // Insert and then delete more than buffer length
    buffer3->insert(0, "Short text", false);
    buffer3->deleteText(2, 100); // Should only delete to the end
    assert(buffer3->getValue() == "Sh");
    
    // Test 4: Multiple operations sequence
    PieceTreeTextBufferBuilder builder4;
    auto factory4 = builder4.finish();
    auto buffer4 = factory4.create(DefaultEndOfLine::LF);
    
    // Create a multi-line buffer
    buffer4->insert(0, "Line1\nLine2\nLine3\nLine4\nLine5", false);
    assert(buffer4->getLineCount() == 5);
    
    // Delete across line boundaries
    buffer4->deleteText(3, 10);
    
    // Insert at exact node boundary
    int position = buffer4->getLength() / 2;
    buffer4->insert(position, "BOUNDARY", false);
    
    // Sequence of inserts and deletes at the same position
    for (int i = 0; i < 10; i++) {
        buffer4->insert(5, "X", false);
        buffer4->deleteText(5, 1);
    }
    
    // Test 5: Very large operations
    PieceTreeTextBufferBuilder builder5;
    auto factory5 = builder5.finish();
    auto buffer5 = factory5.create(DefaultEndOfLine::LF);
    
    // Create a reasonably sized string for testing
    std::string longText;
    for (int i = 0; i < 1000; i++) {
        longText += "Line " + std::to_string(i) + "\n";
    }
    
    // Insert the long text
    buffer5->insert(0, longText, false);
    assert(buffer5->getLineCount() == 1001); // 1000 lines + empty line at end
    
    // Delete a large portion
    buffer5->deleteText(100, 5000);
    
    // Insert in the middle of the large text
    int midPos = buffer5->getLength() / 2;
    buffer5->insert(midPos, "INSERTED_IN_LARGE_TEXT", false);
    
    std::cout << "All custom insertion and deletion tests passed!" << std::endl;
    return 0;
} 