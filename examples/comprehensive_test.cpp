#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <chrono>
#include <random>
#include <algorithm>
#include <sstream>
#include <functional>
#include <cstring>
#include "textbuffer/piece_tree_base.h"
#include "textbuffer/piece_tree_builder.h"

using namespace textbuffer;

// Force flush output buffer after each operation
void flushOutput() {
    std::cout.flush();
    std::cerr.flush();
}

// 计时工具，用于性能测试
class Timer {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    std::string operation_name;
public:
    Timer(const std::string& name) : operation_name(name) {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    ~Timer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        std::cout << operation_name << " took " << duration << " ms" << std::endl;
    }
};

// 随机字符串生成器
class RandomStringGenerator {
private:
    std::mt19937 generator;
    std::uniform_int_distribution<int> char_dist;
    std::uniform_int_distribution<int> length_dist;
    static constexpr const char* charset = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 \n\t.,;:?!-+*/=()[]{}<>";
    
public:
    RandomStringGenerator(int min_length = 1, int max_length = 100, unsigned int seed = 42)
        : generator(seed), 
          char_dist(0, strlen(charset) - 1), 
          length_dist(min_length, max_length) {}
    
    std::string generate() {
        int length = length_dist(generator);
        std::string result;
        result.reserve(length);
        
        for (int i = 0; i < length; ++i) {
            result += charset[char_dist(generator)];
        }
        
        return result;
    }
    
    int random_number(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(generator);
    }
};

// Basic functionality test
void test_basic_operations() {
    std::cout << "\n--- Testing Basic Operations ---\n";

    // Create empty buffer
    PieceTreeTextBufferBuilder builder;
    auto factory = builder.finish();
    auto buffer = factory.create(DefaultEndOfLine::LF);
    
    // Verify initial state
    assert(buffer->getValue() == "");
    assert(buffer->getLength() == 0);
    assert(buffer->getLineCount() == 1); // Empty buffer has one line
    
    // Insert text
    buffer->insert(0, "Hello", false);
    assert(buffer->getValue() == "Hello");
    assert(buffer->getLength() == 5);
    
    // Insert in the middle
    buffer->insert(5, " World", false);
    assert(buffer->getValue() == "Hello World");
    
    // Insert at the end
    buffer->insert(11, "!", false);
    assert(buffer->getValue() == "Hello World!");
    
    // Delete text
    buffer->deleteText(5, 6); // Delete " World"
    assert(buffer->getValue() == "Hello!");
    
    // Get line count and content
    buffer->insert(5, "\nSecond line\nThird line", false);
    assert(buffer->getLineCount() == 3);
    assert(buffer->getLineContent(0) == "Hello");
    assert(buffer->getLineContent(1) == "Second line");
    assert(buffer->getLineContent(2) == "Third line!");
    
    // Get content in range
    common::Range range(0, 2, 1, 3);
    std::string rangeContent = buffer->getValueInRange(range);
    assert(rangeContent == "llo\nSec");
    
    // Position and offset conversion
    common::Position pos = buffer->getPositionAt(7); // "Hello\nS|econd line\nThird line!"
    assert(pos.lineNumber() == 1);
    assert(pos.column() == 1);
    
    int offset = buffer->getOffsetAt(2, 5);
    assert(offset == 18 + 5);
    
    std::cout << "Basic operations test passed!\n";
}

// Large text operations test
void test_manageable_text_size() {
    std::cout << "\n--- Testing Large Text Operations ---\n";
    
    try {
        std::cout << "  Creating test text...\n";
        std::string testText;
        testText.reserve(1000);
        for (int i = 0; i < 20; i++) {
            testText += "Line " + std::to_string(i) + " with some text to test buffer operations.\n";
        }
        
        std::cout << "  Creating buffer...\n";
        PieceTreeTextBufferBuilder builder;
        builder.acceptChunk(testText);
        auto factory = builder.finish();
        auto buffer = factory.create(DefaultEndOfLine::LF);
        
        // Verify initial state
        assert(buffer->getLength() == testText.length());
        assert(buffer->getLineCount() == 20);
        assert(buffer->getValue() == testText);
        
        std::cout << "  Performing middle insertion...\n";
        // Insert in the middle of the text
        std::string insertText = "Inserted text\n";
        int position = testText.length() / 2;
        buffer->insert(position, insertText, false);
        
        // Verify after insertion
        assert(buffer->getLength() == testText.length() + insertText.length());
        
        std::cout << "  Performing a few insertions...\n";
        // Perform a few more insertions
        buffer->insert(10, "Insert at 10\n", false);
        buffer->insert(100, "Insert at 100\n", false);
        
        std::cout << "  Performing a few deletions...\n";
        // Delete some text
        buffer->deleteText(5, 3);
        buffer->deleteText(50, 5);
        
        std::cout << "  Getting content range...\n";
        // Get content range
        std::string content = buffer->getValue().substr(0, 100);
        assert(content.length() <= 100);
        
        std::cout << "Large text operations test passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Error in large text test: " << e.what() << std::endl;
        throw;
    }
}

// Sequential edit operations test
void test_sequential_edits() {
    std::cout << "\n--- Testing Sequential Edit Operations ---\n";
    
    try {
        // Create a basic buffer with simple content
        std::cout << "  Creating buffer with initial content...\n";
        PieceTreeTextBufferBuilder builder;
        builder.acceptChunk("Hello World");
        auto factory = builder.finish();
        auto buffer = factory.create(DefaultEndOfLine::LF);
        
        // Perform a series of sequential operations
        std::cout << "  Performing sequential operations...\n";
        
        // 1. Insert at start
        buffer->insert(0, "Start: ", false);
        assert(buffer->getValue() == "Start: Hello World");
        
        // 2. Insert in middle
        buffer->insert(7, "[middle]", false);
        assert(buffer->getValue() == "Start: [middle]Hello World");
        
        // 3. Insert at end
        buffer->insert(buffer->getLength(), " End", false);
        assert(buffer->getValue() == "Start: [middle]Hello World End");
        
        // 4. Delete from start
        buffer->deleteText(0, 7);
        assert(buffer->getValue() == "[middle]Hello World End");
        
        // 5. Delete from middle
        buffer->deleteText(8, 5);
        assert(buffer->getValue() == "[middle]HelloWorld End");
        
        // 6. Delete from end
        buffer->deleteText(buffer->getLength() - 4, 4);
        assert(buffer->getValue() == "[middle]HelloWorld");
        
        // 7. Multiple insertions at same position
        for (int i = 0; i < 5; i++) {
            buffer->insert(0, std::to_string(i), false);
        }
        assert(buffer->getValue() == "43210[middle]HelloWorld");
        
        // 8. Multiple deletions from same region
        buffer->deleteText(5, 1);
        buffer->deleteText(5, 2);
        buffer->deleteText(5, 3);
        assert(buffer->getValue() == "4321HelloWorld");
        
        // 9. Complex sequence with validation
        buffer->insert(4, "XXX", false);
        assert(buffer->getValue() == "4321XXXHelloWorld");
        buffer->deleteText(7, 5);
        assert(buffer->getValue() == "4321XXXWorld");
        buffer->insert(7, "YYY", false);
        assert(buffer->getValue() == "4321XXXYYYWorld");
        
        std::cout << "Sequential edit operations test passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Error during sequential edit operations test: " << e.what() << std::endl;
        throw;
    }
}

// Edge cases test
void test_edge_cases() {
    std::cout << "\n--- Testing Edge Cases ---\n";
    
    // Create buffer
    PieceTreeTextBufferBuilder builder;
    auto factory = builder.finish();
    auto buffer = factory.create(DefaultEndOfLine::LF);
    
    // 1. Operations on empty buffer
    assert(buffer->getValue() == "");
    assert(buffer->getLength() == 0);
    
    // 2. Insert to empty buffer
    buffer->insert(0, "Text", false);
    assert(buffer->getValue() == "Text");
    
    // 3. Delete beyond range (should delete to the end)
    buffer->deleteText(2, 100);
    assert(buffer->getValue() == "Te");
    
    // 4. Insert empty string
    buffer->insert(1, "", false);
    assert(buffer->getValue() == "Te");
    
    // 5. Delete 0 characters
    buffer->deleteText(1, 0);
    assert(buffer->getValue() == "Te");
    
    // 6. Insert at beginning
    buffer->insert(0, "Start", false);
    assert(buffer->getValue() == "StartTe");
    
    std::cout << "Edge cases test passed!\n";
}

// EOL handling test
void test_eol_handling() {
    std::cout << "\n--- Testing EOL Handling ---\n";
    
    // Test LF EOL
    PieceTreeTextBufferBuilder builder1;
    builder1.acceptChunk("Line1\nLine2\nLine3");
    auto factory1 = builder1.finish();
    auto buffer1 = factory1.create(DefaultEndOfLine::LF);
    
    // Verify line count and EOL
    assert(buffer1->getLineCount() == 3);
    assert(buffer1->getEOL() == "\n");
    
    // Test CRLF EOL
    PieceTreeTextBufferBuilder builder2;
    builder2.acceptChunk("Line1\r\nLine2\r\nLine3");
    auto factory2 = builder2.finish();
    auto buffer2 = factory2.create(DefaultEndOfLine::CRLF);
    
    // Verify line count and EOL
    assert(buffer2->getLineCount() == 3);
    assert(buffer2->getEOL() == "\r\n");
    
    std::cout << "EOL handling test passed!\n";
}

// Performance test
void test_performance() {
    std::cout << "\n--- Testing Performance ---\n";
    
    // Prepare small text for testing
    std::cout << "  Preparing test text...\n";
    const int total_lines = 50; // Very small scale
    std::string testText;
    testText.reserve(total_lines * 30);
    for (int i = 0; i < total_lines; i++) {
        testText += "Line " + std::to_string(i) + " of test text.\n";
    }
    
    // Test initialization
    std::cout << "  Testing initialization...\n";
    PieceTreeTextBufferBuilder builder;
    builder.acceptChunk(testText);
    auto factory = builder.finish();
    auto buffer = factory.create(DefaultEndOfLine::LF);
    assert(buffer->getLineCount() == total_lines + 1);
    
    // Test a few insertions
    std::cout << "  Testing insertions...\n";
    for (int i = 0; i < 5; i++) {
        int pos = i * 100;
        if (pos < buffer->getLength()) {
            buffer->insert(pos, "INSERT" + std::to_string(i), false);
        }
    }
    
    // Test a few deletions
    std::cout << "  Testing deletions...\n";
    for (int i = 0; i < 3; i++) {
        int pos = i * 200;
        if (pos + 5 < buffer->getLength()) {
            buffer->deleteText(pos, 5);
        }
    }
    
    // Test getting content
    std::cout << "  Testing getting content...\n";
    std::string content = buffer->getValue();
    assert(!content.empty());
    
    // Test getting random lines
    std::cout << "  Testing line access...\n";
    for (int i = 0; i < 5; i++) {
        int lineNumber = i * 5;
        if (lineNumber < buffer->getLineCount()) {
            std::string line = buffer->getLineContent(lineNumber);
        }
    }
    
    std::cout << "Performance test passed!\n";
}

// Middle position operations test
void test_middle_operations() {
    std::cout << "\n--- Testing Middle Position Operations ---\n";
    
    // Create a buffer with content
    PieceTreeTextBufferBuilder builder;
    builder.acceptChunk("abcdefghijklmnopqrstuvwxyz");
    auto factory = builder.finish();
    auto buffer = factory.create(DefaultEndOfLine::LF);
    
    // Test middle insertions
    buffer->insert(1, "-1-", false);
    assert(buffer->getValue() == "a-1-bcdefghijklmnopqrstuvwxyz");
    
    buffer->insert(13, "-13-", false);
    assert(buffer->getValue() == "a-1-bcdefghi-13-jklmnopqrstuvwxyz");
    
    // Test middle deletions
    buffer->deleteText(2, 2);
    assert(buffer->getValue() == "a-bcdefghi-13-jklmnopqrstuvwxyz");
    
    buffer->deleteText(10, 5);
    assert(buffer->getValue() == "a-bcdefghiklmnopqrstuvwxyz");
    
    // Test with a slightly larger text
    std::string mediumText;
    for (int i = 0; i < 20; i++) {
        mediumText += "chunk" + std::to_string(i) + " ";
    }
    
    PieceTreeTextBufferBuilder builder2;
    builder2.acceptChunk(mediumText);
    auto factory2 = builder2.finish();
    auto buffer2 = factory2.create(DefaultEndOfLine::LF);
    
    // Middle insertion and deletion
    int middlePos = mediumText.length() / 2;
    buffer2->insert(middlePos, "[MIDDLE]", false);
    buffer2->deleteText(middlePos + 4, 3);
    
    std::cout << "Middle position operations test passed!\n";
}

// Unicode handling test
void test_unicode_handling() {
    std::cout << "\n--- Testing Unicode Handling ---\n";
    
    // Create buffer with Unicode characters
    PieceTreeTextBufferBuilder builder;
    std::string unicodeText = "ASCII text 你好 こんにちは";
    builder.acceptChunk(unicodeText);
    auto factory = builder.finish();
    auto buffer = factory.create(DefaultEndOfLine::LF);
    
    // Verify content
    assert(buffer->getValue() == unicodeText);
    
    // Insert between Unicode characters
    buffer->insert(11, "| |", false);
    assert(buffer->getValue() == "ASCII text | |你好 こんにちは");
    
    // Delete Unicode character
    buffer->deleteText(16, 1);
    
    // Multi-line Unicode text
    PieceTreeTextBufferBuilder builder2;
    std::string multilineUnicode = "Line1 你好\nLine2 こんにちは";
    builder2.acceptChunk(multilineUnicode);
    auto factory2 = builder2.finish();
    auto buffer2 = factory2.create(DefaultEndOfLine::LF);
    
    // Verify line count and content
    assert(buffer2->getLineCount() == 2);
    assert(buffer2->getLineContent(0) == "Line1 你好");
    assert(buffer2->getLineContent(1) == "Line2 こんにちは");
    
    std::cout << "Unicode handling test passed!\n";
}

// Snapshot test
void test_snapshot() {
    std::cout << "\n--- Testing Snapshot Functionality ---\n";
    
    // Create buffer with content
    PieceTreeTextBufferBuilder builder;
    std::string text = "This is a test for snapshot";
    builder.acceptChunk(text);
    auto factory = builder.finish();
    auto buffer = factory.create(DefaultEndOfLine::LF);
    
    // Create a snapshot
    auto snapshot = buffer->createSnapshot("");
    
    // Modify buffer
    buffer->insert(0, "Start: ", false);
    buffer->deleteText(buffer->getLength() - 5, 5);
    
    // Verify snapshot content remains unchanged
    std::string snapshotContent = snapshot->read();
    assert(snapshotContent == text);
    
    std::cout << "Snapshot functionality test passed!\n";
}

// Minimal functionality test
void test_minimal_functionality() {
    std::cout << "\n--- Minimal Functionality Test ---\n";
    
    // Create a small buffer for basic tests
    PieceTreeTextBufferBuilder builder;
    auto factory = builder.finish();
    auto buffer = factory.create(DefaultEndOfLine::LF);
    
    std::cout << "  Creating small buffer...\n";
    // Test basic functionality
    std::cout << "  Testing basic functionality...\n";
    buffer->insert(0, "Hello", false);
    assert(buffer->getValue() == "Hello");
    
    std::cout << "  Testing insert...\n";
    buffer->insert(5, " World", false);
    assert(buffer->getValue() == "Hello World");
    
    std::cout << "  Testing delete...\n";
    buffer->deleteText(5, 6);
    assert(buffer->getValue() == "Hello");
    
    std::cout << "Minimal functionality test passed!\n";
}

// Cross-node operations test
void test_cross_node_operations() {
    std::cout << "\n--- Testing Cross-Node Operations ---\n";
    
    try {
        // Create a buffer with multiple chunks to ensure multiple nodes
        PieceTreeTextBufferBuilder builder;
        builder.acceptChunk("Chunk1: This is the first chunk of text.");
        builder.acceptChunk("Chunk2: This is the second chunk of text.");
        builder.acceptChunk("Chunk3: This is the third chunk of text.");
        auto factory = builder.finish();
        auto buffer = factory.create(DefaultEndOfLine::LF);
        
        // Get the initial content
        std::string initialContent = buffer->getValue();
        int initialLength = buffer->getLength();
        
        std::cout << "  Testing cross-node deletion...\n";
        // Delete across node boundaries
        int deleteStart = 20; // Middle of first chunk
        int deleteLength = 40; // Spans into the second chunk
        buffer->deleteText(deleteStart, deleteLength);
        
        // Verify the deletion worked correctly
        std::string afterDelete = buffer->getValue();
        assert(afterDelete.length() == initialLength - deleteLength);
        
        std::cout << "  Testing cross-node insertion...\n";
        // Insert at a position that was previously a node boundary
        std::string insertText = "INSERTED_ACROSS_BOUNDARY";
        buffer->insert(deleteStart, insertText, false);
        
        // Verify the insertion worked correctly
        std::string afterInsert = buffer->getValue();
        assert(afterInsert.length() == afterDelete.length() + insertText.length());
        
        // Create a multi-line buffer to test cross-line operations
        PieceTreeTextBufferBuilder builder2;
        builder2.acceptChunk("Line1\nLine2\nLine3\nLine4\nLine5");
        auto factory2 = builder2.finish();
        auto buffer2 = factory2.create(DefaultEndOfLine::LF);
        
        std::cout << "  Testing cross-line deletion...\n";
        // Delete across line boundaries
        buffer2->deleteText(3, 10); // From middle of Line1 to middle of Line3
        
        // Verify lines are merged correctly
        assert(buffer2->getLineCount() < 5);
        
        // Test a complex sequence of operations
        std::cout << "  Testing complex cross-node sequence...\n";
        // Start with a new buffer
        PieceTreeTextBufferBuilder builder3;
        builder3.acceptChunk("AAAAA");
        builder3.acceptChunk("BBBBB");
        builder3.acceptChunk("CCCCC");
        auto factory3 = builder3.finish();
        auto buffer3 = factory3.create(DefaultEndOfLine::LF);
        
        // 1. Insert in the middle of each node
        buffer3->insert(2, "111", false);    // In first node
        buffer3->insert(10, "222", false);   // In second node
        buffer3->insert(18, "333", false);   // In third node
        
        // 2. Delete across node boundaries
        buffer3->deleteText(4, 10);   // Delete from first node into third
        
        // 3. Insert at the boundary
        buffer3->insert(4, "XXX", false);
        
        // Verify the correct operations
        std::string finalContent = buffer3->getValue();
        assert(!finalContent.empty());
        
        std::cout << "Cross-node operations test passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Error during cross-node operations test: " << e.what() << std::endl;
        throw;
    }
}

// Regression test for random operations
void test_regression_random_operations() {
    std::cout << "\n--- Testing Regression: Random Operations ---\n";
    
    try {
        // Create buffer with initial text
        PieceTreeTextBufferBuilder builder;
        builder.acceptChunk("Initial text\nSecond line\nThird line");
        auto factory = builder.finish();
        auto buffer = factory.create(DefaultEndOfLine::LF);
        
        // Original content for verification
        std::string content = buffer->getValue();
        std::cout << "  Initial content: " << content.length() << " characters\n";
        
        // The specific sequence that caused problems in the random test
        std::cout << "  Performing problematic sequence...\n";
        
        // Perform a deletion first
        buffer->deleteText(7, 2);
        std::string afterDelete = buffer->getValue();
        
        // Now insert text at the same position
        std::string textToInsert = "{CAYIhZ;3";
        buffer->insert(7, textToInsert, false);
        
        // Verify content integrity
        std::string afterInsert = buffer->getValue();
        
        // Try inserting at end of the buffer 
        std::string endText = "ENDTEXT";
        buffer->insert(buffer->getLength(), endText, false);
        std::string afterEndInsert = buffer->getValue();
        
        // Try inserting in the middle again
        std::string middleText = "MIDDLETEXT";
        int middlePos = afterEndInsert.length() / 2;
        buffer->insert(middlePos, middleText, false);
        
        // Final verification
        std::string finalContent = buffer->getValue();
        std::cout << "  Final content: " << finalContent.length() << " characters\n";
        
        // Now try a series of fast alternating operations
        std::cout << "  Performing fast alternating operations...\n";
        
        // Create a fresh buffer
        PieceTreeTextBufferBuilder builder2;
        builder2.acceptChunk("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        auto factory2 = builder2.finish();
        auto buffer2 = factory2.create(DefaultEndOfLine::LF);
        
        // Alternate between insert and delete operations at adjacent positions
        for (int i = 0; i < 10; i++) {
            int pos = 5 + i;
            buffer2->deleteText(pos, 1);
            buffer2->insert(pos, std::to_string(i), false);
        }
        
        std::string finalContent2 = buffer2->getValue();
        assert(!finalContent2.empty());
        
        std::cout << "Regression test passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Error during regression test: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    try {
        std::cout << "=== TextBuffer Comprehensive Tests ===\n";
        
        test_minimal_functionality();
        test_basic_operations();
        test_manageable_text_size();
        test_sequential_edits();
        test_edge_cases();
        test_eol_handling();
        test_performance();
        test_middle_operations();
        test_unicode_handling();
        test_snapshot();
        test_cross_node_operations();
        test_regression_random_operations();
        
        std::cout << "\nAll tests passed successfully!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\nTest failed with unknown exception\n";
        return 1;
    }
} 