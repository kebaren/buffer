#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include "textbuffer/piece_tree_base.h"

using namespace textbuffer;

// Create a patched version of PieceTreeBase with a correctly implemented getLineContent method
class PatchedTextBuffer : public PieceTreeBase {
public:
    using PieceTreeBase::PieceTreeBase;  // Inherit constructors
    
    // Override getLineContent to fix the issue with line 0
    std::string getLineContent(int32_t lineNumber) {
        if (lineNumber < 0 || lineNumber >= getLineCount()) {
            throw std::out_of_range("Invalid line number");
        }

        // Cache for the last visited line is private, so we need to manage our own cache
        static int32_t lastLineNumber = -1;
        static std::string lastLineContent = "";
        
        // If cache is available, return cached result
        if (lineNumber == lastLineNumber) {
            return lastLineContent;
        }

        // Update cache line number
        lastLineNumber = lineNumber;

        if (root == SENTINEL) {
            lastLineContent = "";
            return "";
        }
        
        // Important: adjust lineNumber to work with getLineRawContent which uses 1-based indexing
        int32_t adjustedLineNumber = lineNumber + 1;
        
        // The rest of the implementation uses the adjusted lineNumber
        if (adjustedLineNumber == getLineCount()) {
            lastLineContent = getLineRawContent(adjustedLineNumber);
        } else if (_EOLNormalized) {
            lastLineContent = getLineRawContent(adjustedLineNumber, _EOLLength);
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
            lastLineContent = content;
        }
        
        return lastLineContent;
    }
};

// Helper function to dump buffer contents
void dumpBuffer(PatchedTextBuffer& buffer, const std::string& label) {
    std::cout << "\n--- " << label << " ---" << std::endl;
    std::cout << "Content: '" << buffer.getLinesRawContent() << "'" << std::endl;
    std::cout << "Length: " << buffer.getLength() << std::endl;
    std::cout << "Line count: " << buffer.getLineCount() << std::endl;
    
    // Function to properly get line content
    auto getCorrectLineContent = [&](int lineNumber) -> std::string {
        if (lineNumber < 0 || lineNumber >= buffer.getLineCount()) {
            throw std::out_of_range("Invalid line number");
        }
        
        // Get start and end offsets for the line
        int startOffset = buffer.getOffsetAt(lineNumber, 0);
        int endOffset;
        
        if (lineNumber + 1 < buffer.getLineCount()) {
            endOffset = buffer.getOffsetAt(lineNumber + 1, 0);
        } else {
            endOffset = buffer.getLength();
        }
        
        // Extract content
        std::string fullContent = buffer.getLinesRawContent();
        int length = endOffset - startOffset;
        
        // Remove trailing line breaks
        if (length > 0) {
            if (length >= 2 && 
                fullContent[startOffset + length - 2] == '\r' && 
                fullContent[startOffset + length - 1] == '\n') {
                length -= 2;
            } else if (length >= 1 && 
                      (fullContent[startOffset + length - 1] == '\n' || 
                       fullContent[startOffset + length - 1] == '\r')) {
                length -= 1;
            }
        }
        
        return fullContent.substr(startOffset, length);
    };

    std::cout << "Line contents:" << std::endl;
    for (int i = 0; i < buffer.getLineCount(); i++) {
        std::string lineContent = getCorrectLineContent(i);
        std::cout << "  Line " << i << ": '" << lineContent << "'" << std::endl;
    }
}

// Test 1: Create a buffer and examine its initial state
void testEmptyBuffer() {
    std::cout << "\n=== Testing Empty Buffer ===\n";
    
    // Create empty buffer
    std::vector<StringBuffer> chunks;
    chunks.push_back(StringBuffer("", {0}));
    
    PatchedTextBuffer buffer;
    buffer.create(chunks, "\n", true);
    
    dumpBuffer(buffer, "Empty Buffer");
}

// Test 2: Insert single characters and examine state after each
void testSingleCharInserts() {
    std::cout << "\n=== Testing Single Character Inserts ===\n";
    
    // Create empty buffer
    std::vector<StringBuffer> chunks;
    chunks.push_back(StringBuffer("", {0}));
    
    PatchedTextBuffer buffer;
    buffer.create(chunks, "\n", true);
    
    // Insert a single character at position 0
    buffer.insert(0, "A");
    dumpBuffer(buffer, "After inserting 'A' at position 0");
    
    // Insert a single character at position 1
    buffer.insert(1, "B");
    dumpBuffer(buffer, "After inserting 'B' at position 1");
    
    // Insert at start again
    buffer.insert(0, "C");
    dumpBuffer(buffer, "After inserting 'C' at position 0");
    
    // Insert in the middle
    buffer.insert(1, "D");
    dumpBuffer(buffer, "After inserting 'D' at position 1");
}

// Test 3: Insert and delete operations
void testInsertDelete() {
    std::cout << "\n=== Testing Insert and Delete Operations ===\n";
    
    // Create empty buffer
    std::vector<StringBuffer> chunks;
    chunks.push_back(StringBuffer("", {0}));
    
    PatchedTextBuffer buffer;
    buffer.create(chunks, "\n", true);
    
    // Insert some text
    buffer.insert(0, "ABCDEF");
    dumpBuffer(buffer, "After inserting 'ABCDEF'");
    
    // Delete first character
    buffer.deleteText(0, 1);
    dumpBuffer(buffer, "After deleting first character");
    
    // Delete 2 characters from the middle
    buffer.deleteText(1, 2);
    dumpBuffer(buffer, "After deleting 2 middle characters");
    
    // Try to delete beyond the end (should do nothing)
    buffer.deleteText(3, 1);
    dumpBuffer(buffer, "After deleting last character");
}

// Test 4: Line operations - insert and manipulate multiple lines
void testLineOperations() {
    std::cout << "\n=== Testing Line Operations ===\n";
    
    // Create empty buffer
    std::vector<StringBuffer> chunks;
    chunks.push_back(StringBuffer("", {0}));
    
    PatchedTextBuffer buffer;
    buffer.create(chunks, "\n", true);
    
    // Insert multiple lines
    buffer.insert(0, "Line 1\nLine 2\nLine 3");
    dumpBuffer(buffer, "After inserting 3 lines");
    
    // Get offset at specific line
    int offsetAtLine1 = buffer.getOffsetAt(1, 0);
    std::cout << "Offset at line 1, column 0: " << offsetAtLine1 << std::endl;
    
    // Insert text at beginning of line 2
    buffer.insert(offsetAtLine1, "INSERTED: ");
    dumpBuffer(buffer, "After inserting at beginning of line 2");
    
    // Locate line position by searching the content for debugging
    int line2StartOffset = -1;
    std::string content = buffer.getLinesRawContent();
    size_t pos = content.find("INSERTED: ");
    if (pos != std::string::npos) {
        line2StartOffset = static_cast<int>(pos);
        std::cout << "Line 2 starts at offset " << line2StartOffset << " (found by search)" << std::endl;
    }
    
    // Insert a prefix at that position
    if (line2StartOffset >= 0) {
        buffer.insert(line2StartOffset, "PREFIX: ");
        dumpBuffer(buffer, "After inserting prefix at calculated line 2 position");
    }
}

// Test 5: Fully examine getOffsetAt and line calculation 
void testLineCalculation() {
    std::cout << "\n=== Testing Line Position Calculation ===\n";
    
    // Create empty buffer
    std::vector<StringBuffer> chunks;
    chunks.push_back(StringBuffer("", {0}));
    
    PatchedTextBuffer buffer;
    buffer.create(chunks, "\n", true);
    
    // Insert a multi-line text
    buffer.insert(0, "Line 1\nLine 2\nLine 3\nLine 4\nLine 5");
    dumpBuffer(buffer, "Multi-line buffer");
    
    // Examine the line offsets and content at those positions
    std::cout << "Line offsets:" << std::endl;
    for (int i = 0; i < buffer.getLineCount(); i++) {
        int offset = buffer.getOffsetAt(i, 0);
        std::cout << "  Offset at line " << i << ", column 0: " << offset << std::endl;
        
        if (offset >= 0 && offset < buffer.getLength()) {
            // Get character at this position
            char c = buffer.getLinesRawContent()[offset];
            std::cout << "    Character at this offset: '" << c << "' (ASCII: " << (int)c << ")" << std::endl;
            
            // Show a few characters from this position if possible
            if (offset + 5 <= buffer.getLength()) {
                std::string nextChars = buffer.getLinesRawContent().substr(offset, 5);
                std::cout << "    5 chars from this position: '" << nextChars << "'" << std::endl;
            }
        }
    }
}

// Test 6: Specifically diagnose line offset issues
void testLineOffsetDiagnostics() {
    std::cout << "\n=== Diagnosing Line Offset Issues ===\n";
    
    // Create empty buffer
    std::vector<StringBuffer> chunks;
    chunks.push_back(StringBuffer("", {0}));
    
    PatchedTextBuffer buffer;
    buffer.create(chunks, "\n", true);
    
    // Insert a multi-line text with known content
    buffer.insert(0, "Line 1\nLine 2\nLine 3\nLine 4\nLine 5");
    
    std::cout << "Buffer raw content: '" << buffer.getLinesRawContent() << "'" << std::endl;
    std::cout << "Buffer length: " << buffer.getLength() << std::endl;
    std::cout << "Buffer line count: " << buffer.getLineCount() << std::endl;
    
    // 1. First, manually locate line positions by searching the content
    std::string content = buffer.getLinesRawContent();
    std::cout << "\nManual line location:" << std::endl;
    
    for (int i = 0; i < buffer.getLineCount(); i++) {
        std::string lineMarker = "Line " + std::to_string(i+1);
        size_t pos = content.find(lineMarker);
        if (pos != std::string::npos) {
            std::cout << "  Expected line " << i << " found at position " << pos << std::endl;
        }
    }
    
    // 2. Then, use getOffsetAt to get line positions and compare
    std::cout << "\ngetOffsetAt results:" << std::endl;
    for (int i = 0; i < buffer.getLineCount(); i++) {
        int offset = buffer.getOffsetAt(i, 0);
        std::cout << "  getOffsetAt(" << i << ", 0): " << offset;
        
        if (offset >= 0 && offset < buffer.getLength()) {
            char c = buffer.getLinesRawContent()[offset];
            std::cout << " -> Character: '" << c << "' (ASCII: " << (int)c << ")";
            
            if (offset + 10 <= buffer.getLength()) {
                std::string nextChars = buffer.getLinesRawContent().substr(offset, 10);
                std::cout << ", Next chars: '" << nextChars << "'";
            }
        }
        std::cout << std::endl;
    }
    
    // 3. Direct comparison between getLineContent and manual extraction
    std::cout << "\nComparing getLineContent with expected content:" << std::endl;
    std::vector<std::string> expectedLines = {"Line 1", "Line 2", "Line 3", "Line 4", "Line 5"};
    
    for (int i = 0; i < buffer.getLineCount(); i++) {
        std::string lineContent = buffer.getLineContent(i);
        std::string expected = (i < (int)expectedLines.size()) ? expectedLines[i] : "";
        
        std::cout << "  Line " << i << ":" << std::endl;
        std::cout << "    getLineContent(): '" << lineContent << "'" << std::endl;
        std::cout << "    Expected content: '" << expected << "'" << std::endl;
        std::cout << "    Match: " << (lineContent == expected ? "YES" : "NO") << std::endl;
    }
    
    // 4. Try manual line content extraction
    std::cout << "\nAttempting manual line content extraction:" << std::endl;
    for (int i = 0; i < buffer.getLineCount(); i++) {
        int startOffset = buffer.getOffsetAt(i, 0);
        int endOffset = (i + 1 < buffer.getLineCount()) ? buffer.getOffsetAt(i + 1, 0) : buffer.getLength();
        std::string extractedContent = content.substr(startOffset, endOffset - startOffset);
        
        // Remove trailing newline if present
        if (!extractedContent.empty() && 
            (extractedContent.back() == '\n' || extractedContent.back() == '\r')) {
            extractedContent.pop_back();
        }
        
        std::cout << "  Line " << i << " extracted from positions " << startOffset << "-" 
                  << endOffset << ": '" << extractedContent << "'" << std::endl;
        
        if (i < (int)expectedLines.size()) {
            std::cout << "  Match with expected: " 
                      << (extractedContent == expectedLines[i] ? "YES" : "NO") << std::endl;
        }
    }
}

// Test 7: Specifically examine getLineRawContent output
void testRawLineContent() {
    std::cout << "\n=== Examining getLineRawContent ===\n";
    
    // Create empty buffer
    std::vector<StringBuffer> chunks;
    chunks.push_back(StringBuffer("", {0}));
    
    PatchedTextBuffer buffer;
    buffer.create(chunks, "\n", true);
    
    // Insert a multi-line text with known content
    buffer.insert(0, "Line 1\nLine 2\nLine 3\nLine 4\nLine 5");
    
    std::cout << "Buffer raw content: '" << buffer.getLinesRawContent() << "'" << std::endl;
    std::cout << "Buffer length: " << buffer.getLength() << std::endl;
    std::cout << "Buffer line count: " << buffer.getLineCount() << std::endl;
    
    // Try lineNumber from 1 to lineCount+1 to match TypeScript behavior
    std::cout << "\nCalling getLineRawContent with lineNumber+1:" << std::endl;
    for (int i = 1; i <= buffer.getLineCount(); i++) {
        try {
            std::string content = buffer.getLineRawContent(i);
            std::cout << "  getLineRawContent(" << i << "): '" << content << "'" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "  Exception for getLineRawContent(" << i << "): " << e.what() << std::endl;
        }
    }
    
    // Also test with endOffset parameter
    std::cout << "\nCalling getLineRawContent with lineNumber+1 and endOffset=1:" << std::endl;
    for (int i = 1; i <= buffer.getLineCount(); i++) {
        try {
            std::string content = buffer.getLineRawContent(i, 1);
            std::cout << "  getLineRawContent(" << i << ", 1): '" << content << "'" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "  Exception for getLineRawContent(" << i << ", 1): " << e.what() << std::endl;
        }
    }
}

// Test 8: Fix and test getLineContent directly
void testCorrectLineContent() {
    std::cout << "\n=== Direct Line Content Fix ===\n";
    
    // Create empty buffer
    std::vector<StringBuffer> chunks;
    chunks.push_back(StringBuffer("", {0}));
    
    PatchedTextBuffer buffer;
    buffer.create(chunks, "\n", true);
    
    // Insert a multi-line text with known content
    buffer.insert(0, "Line 1\nLine 2\nLine 3\nLine 4\nLine 5");
    
    std::cout << "Buffer raw content: '" << buffer.getLinesRawContent() << "'" << std::endl;
    std::cout << "Buffer length: " << buffer.getLength() << std::endl;
    std::cout << "Buffer line count: " << buffer.getLineCount() << std::endl;
    
    // Implement our own getLineContent function
    auto getCorrectLineContent = [&](int lineNumber) -> std::string {
        if (lineNumber < 0 || lineNumber >= buffer.getLineCount()) {
            throw std::out_of_range("Invalid line number");
        }
        
        // Get start and end offsets for the line
        int startOffset = buffer.getOffsetAt(lineNumber, 0);
        int endOffset;
        
        if (lineNumber + 1 < buffer.getLineCount()) {
            endOffset = buffer.getOffsetAt(lineNumber + 1, 0);
        } else {
            endOffset = buffer.getLength();
        }
        
        // Extract content
        std::string fullContent = buffer.getLinesRawContent();
        int length = endOffset - startOffset;
        
        // Remove trailing line breaks
        if (length > 0) {
            if (length >= 2 && 
                fullContent[startOffset + length - 2] == '\r' && 
                fullContent[startOffset + length - 1] == '\n') {
                length -= 2;
            } else if (length >= 1 && 
                      (fullContent[startOffset + length - 1] == '\n' || 
                       fullContent[startOffset + length - 1] == '\r')) {
                length -= 1;
            }
        }
        
        return fullContent.substr(startOffset, length);
    };
    
    // Compare with original and expected
    std::vector<std::string> expectedLines = {"Line 1", "Line 2", "Line 3", "Line 4", "Line 5"};
    std::cout << "\nComparing custom getCorrectLineContent with expected:" << std::endl;
    
    for (int i = 0; i < buffer.getLineCount(); i++) {
        std::string originalContent = buffer.getLineContent(i);
        std::string correctContent = getCorrectLineContent(i);
        
        std::cout << "  Line " << i << ":" << std::endl;
        std::cout << "    Buffer's getLineContent(): '" << originalContent << "'" << std::endl;
        std::cout << "    Our getCorrectLineContent(): '" << correctContent << "'" << std::endl;
        
        if (i < (int)expectedLines.size()) {
            std::cout << "    Expected content: '" << expectedLines[i] << "'" << std::endl;
            std::cout << "    Original match: " << (originalContent == expectedLines[i] ? "YES" : "NO") << std::endl;
            std::cout << "    Correct match: " << (correctContent == expectedLines[i] ? "YES" : "NO") << std::endl;
        }
    }
}

int main() {
    try {
        std::cout << "Starting Simplest Buffer Tests..." << std::endl;
        
        testEmptyBuffer();
        testSingleCharInserts();
        testInsertDelete();
        testLineOperations();
        testLineCalculation();
        testLineOffsetDiagnostics();
        testRawLineContent();
        testCorrectLineContent();
        
        std::cout << "\nAll Simplest Buffer Tests completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\nTest failed with unknown exception" << std::endl;
        return 1;
    }
}