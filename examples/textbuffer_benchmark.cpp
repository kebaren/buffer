#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <chrono>
#include <random>
#include <algorithm>
#include <sstream>
#include <functional>
#include <memory>
#include <fstream>
#include <iomanip>
#include "textbuffer/piece_tree_base.h"
#include "textbuffer/piece_tree_builder.h"

using namespace textbuffer;
using namespace common;

// 计时工具，用于性能测试
class Timer {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    std::string operation_name;
public:
    Timer(const std::string& name) : operation_name(name) {
        start_time = std::chrono::high_resolution_clock::now();
        std::cout << "Starting " << operation_name << "..." << std::endl;
    }
    
    ~Timer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        std::cout << operation_name << " completed in " << duration << " ms" << std::endl;
    }
};

// 随机数据生成器
class RandomDataGenerator {
private:
    std::mt19937 generator;
    
public:
    RandomDataGenerator(unsigned int seed = 42) : generator(seed) {}
    
    int64_t randomNumber(int64_t min, int64_t max) {
        std::uniform_int_distribution<int64_t> dist(min, max);
        return dist(generator);
    }
    
    std::string randomString(size_t length) {
        static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        
        std::string result;
        result.reserve(length);
        
        for (size_t i = 0; i < length; ++i) {
            result += alphanum[randomNumber(0, sizeof(alphanum) - 2)];
        }
        
        return result;
    }
    
    std::string randomMultilineString(size_t lines, size_t maxLineLength = 100) {
        std::string result;
        for (size_t i = 0; i < lines; ++i) {
            size_t lineLength = randomNumber(1, maxLineLength);
            result += randomString(lineLength) + "\n";
        }
        return result;
    }
    
    std::vector<std::pair<int64_t, int64_t>> generateRandomPositions(
        int64_t maxOffset, int count, int64_t maxLength = 100) {
        std::vector<std::pair<int64_t, int64_t>> positions;
        positions.reserve(count);
        
        for (int i = 0; i < count; ++i) {
            int64_t offset = randomNumber(0, maxOffset > 0 ? maxOffset - 1 : 0);
            int64_t length = randomNumber(1, std::min(maxLength, maxOffset - offset));
            positions.push_back({offset, length});
        }
        
        return positions;
    }
};

// 创建测试缓冲区
std::unique_ptr<PieceTreeBase> createTestBuffer(const std::string& initialContent = "") {
    PieceTreeTextBufferBuilder builder;
    
    if (!initialContent.empty()) {
        builder.acceptChunk(initialContent);
    }
    
    auto factory = builder.finish();
    return factory.create(DefaultEndOfLine::LF);
}

// 测试空缓冲区操作
void testEmptyBuffer() {
    std::cout << "\n=== Testing Empty Buffer Operations ===\n";
    Timer timer("Empty buffer operations");
    
    auto buffer = createTestBuffer();
    
    // 测试空缓冲区的基本属性
    assert(buffer->getLength() == 0);
    assert(buffer->getLineCount() == 1); // 空缓冲区也有一行
    
    // 测试空缓冲区的插入操作
    buffer->insert(0, "Hello, World!", false);
    assert(buffer->getLength() == 13);
    assert(buffer->getLineCount() == 1);
    assert(buffer->getValue() == "Hello, World!");
    
    // 测试在边界插入
    buffer->insert(13, "\nNew Line", false);
    assert(buffer->getLength() == 22);
    assert(buffer->getLineCount() == 2);
    assert(buffer->getLineContent(1) == "New Line");
    
    // 测试删除所有内容
    buffer->deleteText(0, 22);
    assert(buffer->getLength() == 0);
    assert(buffer->getLineCount() == 1);
    
    std::cout << "Empty buffer tests passed.\n";
}

// 测试基本的CRUD操作
void testBasicCRUD() {
    std::cout << "\n=== Testing Basic CRUD Operations ===\n";
    Timer timer("Basic CRUD operations");
    
    // 初始内容
    std::string initialContent = "First line\nSecond line\nThird line";
    auto buffer = createTestBuffer(initialContent);
    
    // 读取测试 (Read)
    assert(buffer->getLength() == 31);
    assert(buffer->getLineCount() == 3);
    assert(buffer->getLineContent(0) == "First line");
    assert(buffer->getLineContent(1) == "Second line");
    assert(buffer->getLineContent(2) == "Third line");
    
    Position start(0, 6); // 第一行第6个字符
    Position end(1, 7);   // 第二行第7个字符
    Range range(start.lineNumber(), start.column(), end.lineNumber(), end.column());
    std::string rangeText = buffer->getValueInRange(range);
    assert(rangeText == "line\nSecond l");
    
    // 创建测试 (Create/Insert)
    buffer->insert(31, "\nFourth line", false);
    assert(buffer->getLineCount() == 4);
    assert(buffer->getLineContent(3) == "Fourth line");
    
    // 在中间插入
    buffer->insert(11, " INSERTED ", false);
    assert(buffer->getLineContent(0) == "First line");
    assert(buffer->getLineContent(1) == "Second INSERTED line");
    
    // 更新测试 (Update)
    // 通过删除后插入实现更新
    buffer->deleteText(0, 5); // 删除"First"
    buffer->insert(0, "Initial", false);
    assert(buffer->getLineContent(0) == "Initial line");
    
    // 删除测试 (Delete)
    buffer->deleteText(buffer->getOffsetAt(2, 0), buffer->getLineContent(2).length());
    assert(buffer->getLineCount() == 4); // 行数不变，因为只删除了内容而非换行符
    assert(buffer->getLineContent(2) == "");
    
    // 删除整行（包括换行符）
    int64_t line2Start = buffer->getOffsetAt(2, 0);
    int64_t line3Start = buffer->getOffsetAt(3, 0);
    buffer->deleteText(line2Start, line3Start - line2Start);
    assert(buffer->getLineCount() == 3);
    
    std::cout << "Basic CRUD tests passed.\n";
}

// 测试边界情况
void testEdgeCases() {
    std::cout << "\n=== Testing Edge Cases ===\n";
    Timer timer("Edge cases");
    
    // 创建缓冲区
    auto buffer = createTestBuffer();
    
    // 测试在空缓冲区插入位置越界的处理
    buffer->insert(100, "Text after bounds", false); // 应该插入在末尾
    assert(buffer->getLength() == 17);
    assert(buffer->getValue() == "Text after bounds");
    
    // 测试删除范围越界的处理
    buffer->deleteText(5, 1000); // 删除超出实际文本长度
    assert(buffer->getLength() == 5);
    assert(buffer->getValue() == "Text ");
    
    // 测试删除位置越界
    buffer->deleteText(1000, 10); // 位置超出文本长度
    assert(buffer->getLength() == 5); // 不应有变化
    
    // 测试删除负长度
    buffer->deleteText(0, -10);
    assert(buffer->getLength() == 5); // 不应有变化
    
    // 测试在0位置删除0长度
    buffer->deleteText(0, 0);
    assert(buffer->getLength() == 5); // 不应有变化
    
    // 测试特殊字符插入
    std::string specialChars = "áéíóúñÁÉÍÓÚÑ你好,世界\n\t\r\0";
    buffer->insert(buffer->getLength(), specialChars, false);
    assert(buffer->getValue().find("áéíóú") != std::string::npos);
    
    std::cout << "Edge case tests passed.\n";
}

// 测试多行文本操作
void testMultilineOperations() {
    std::cout << "\n=== Testing Multiline Operations ===\n";
    Timer timer("Multiline operations");
    
    // 创建多行文本
    std::string multilineText = 
        "Line 1: This is the first line.\n"
        "Line 2: This is the second line.\n"
        "Line 3: This is the third line.\n"
        "Line 4: This is the fourth line.\n"
        "Line 5: This is the fifth line.";
    
    auto buffer = createTestBuffer(multilineText);
    
    // 验证行数和内容
    assert(buffer->getLineCount() == 5);
    
    // 测试跨行删除
    int64_t line1Start = buffer->getOffsetAt(0, 0);
    int64_t line3Start = buffer->getOffsetAt(2, 0);
    buffer->deleteText(line1Start, line3Start - line1Start);
    
    assert(buffer->getLineCount() == 3);
    assert(buffer->getLineContent(0) == "Line 3: This is the third line.");
    
    // 测试跨行插入（包含多个换行符）
    std::string insertText = 
        "New Line A\n"
        "New Line B\n"
        "New Line C\n";
    
    buffer->insert(0, insertText, false);
    assert(buffer->getLineCount() == 6);
    assert(buffer->getLineContent(0) == "New Line A");
    assert(buffer->getLineContent(1) == "New Line B");
    assert(buffer->getLineContent(2) == "New Line C");
    
    // 测试位置和偏移量转换
    Position pos = buffer->getPositionAt(buffer->getOffsetAt(4, 5));
    assert(pos.lineNumber() == 4);
    assert(pos.column() == 5);
    
    std::cout << "Multiline operations tests passed.\n";
}

// 测试大批量操作
void testBulkOperations() {
    std::cout << "\n=== Testing Bulk Operations ===\n";
    Timer timer("Bulk operations");
    
    RandomDataGenerator random;
    auto buffer = createTestBuffer();
    
    // 测试批量插入
    const int insertCount = 1000;
    std::cout << "Performing " << insertCount << " random insertions..." << std::endl;
    
    int64_t totalLength = 0;
    for (int i = 0; i < insertCount; ++i) {
        std::string text = random.randomString(10) + (random.randomNumber(0, 5) == 0 ? "\n" : "");
        int64_t pos = random.randomNumber(0, buffer->getLength());
        buffer->insert(pos, text, false);
        totalLength += text.length();
    }
    
    assert(buffer->getLength() == totalLength);
    
    // 测试批量删除
    const int deleteCount = 500;
    std::cout << "Performing " << deleteCount << " random deletions..." << std::endl;
    
    int64_t deletedLength = 0;
    for (int i = 0; i < deleteCount && buffer->getLength() > 0; ++i) {
        int64_t pos = random.randomNumber(0, buffer->getLength() - 1);
        int64_t len = random.randomNumber(1, std::min<int64_t>(buffer->getLength() - pos, 20));
        buffer->deleteText(pos, len);
        deletedLength += len;
    }
    
    assert(buffer->getLength() == totalLength - deletedLength);
    
    std::cout << "Bulk operations tests passed.\n";
}

// EOL处理测试
void testEOLHandling() {
    std::cout << "\n=== Testing EOL Handling ===\n";
    Timer timer("EOL handling");
    
    // 测试不同的EOL类型
    {
        PieceTreeTextBufferBuilder builder;
        builder.acceptChunk("Line1\r\nLine2\rLine3\nLine4");
        auto factory = builder.finish();
        
        // LF 处理
        auto bufferLF = factory.create(DefaultEndOfLine::LF);
        assert(bufferLF->getLineCount() == 4);
        assert(bufferLF->getLineContent(0) == "Line1");
        assert(bufferLF->getLineContent(1) == "Line2");
        assert(bufferLF->getLineContent(2) == "Line3");
        assert(bufferLF->getLineContent(3) == "Line4");
        
        // CRLF 处理
        auto bufferCRLF = factory.create(DefaultEndOfLine::CRLF);
        assert(bufferCRLF->getLineCount() == 4);
        assert(bufferCRLF->getLineContent(0) == "Line1");
        assert(bufferCRLF->getLineContent(1) == "Line2");
        assert(bufferCRLF->getLineContent(2) == "Line3");
        assert(bufferCRLF->getLineContent(3) == "Line4");
        
        // CR 处理
        auto bufferCR = factory.create(DefaultEndOfLine::CR);
        assert(bufferCR->getLineCount() == 4);
        assert(bufferCR->getLineContent(0) == "Line1");
        assert(bufferCR->getLineContent(1) == "Line2");
        assert(bufferCR->getLineContent(2) == "Line3");
        assert(bufferCR->getLineContent(3) == "Line4");
    }
    
    // 测试EOL跨节点删除和插入
    {
        auto buffer = createTestBuffer("Line1\nLine2\nLine3\n");
        assert(buffer->getLineCount() == 4); // 最后有一个空行
        
        // 删除换行符
        buffer->deleteText(5, 1); // 删除第一个\n
        assert(buffer->getLineCount() == 3);
        assert(buffer->getLineContent(0) == "Line1Line2");
        
        // 插入换行符
        buffer->insert(2, "\n", false);
        assert(buffer->getLineCount() == 4);
        assert(buffer->getLineContent(0) == "Li");
        assert(buffer->getLineContent(1) == "ne1Line2");
    }
    
    std::cout << "EOL handling tests passed.\n";
}

// Unicode字符处理测试
void testUnicodeHandling() {
    std::cout << "\n=== Testing Unicode Handling ===\n";
    Timer timer("Unicode handling");
    
    // 创建包含Unicode字符的文本
    std::string unicodeText = 
        "English: Hello World\n"
        "Chinese: 你好世界\n"
        "Japanese: こんにちは世界\n"
        "Korean: 안녕하세요 세계\n"
        "Russian: Привет, мир\n"
        "Emoji: 😀🌎🚀💻📱";
    
    auto buffer = createTestBuffer(unicodeText);
    
    // 验证行数和长度
    assert(buffer->getLineCount() == 6);
    
    // 测试Unicode字符位置计算
    {
        std::string line = buffer->getLineContent(1); // Chinese line
        assert(line == "Chinese: 你好世界");
        
        int64_t lineStart = buffer->getOffsetAt(1, 0);
        int64_t charPos = buffer->getOffsetAt(1, 9); // 位置在"你"字符
        
        // 删除中文字符
        buffer->deleteText(charPos, 3); // "你"是一个3字节的UTF-8字符
        line = buffer->getLineContent(1);
        assert(line == "Chinese: 好世界");
        
        // 插入中文字符
        buffer->insert(charPos, "新", false);
        line = buffer->getLineContent(1);
        assert(line == "Chinese: 新好世界");
    }
    
    // 测试Emoji字符
    {
        std::string line = buffer->getLineContent(5); // Emoji line
        assert(line == "Emoji: 😀🌎🚀💻📱");
        
        int64_t lineStart = buffer->getOffsetAt(5, 0);
        int64_t charPos = buffer->getOffsetAt(5, 7); // 位置在第一个Emoji
        
        // 删除Emoji
        buffer->deleteText(charPos, 4); // "😀"是一个4字节的UTF-8字符
        line = buffer->getLineContent(5);
        assert(line == "Emoji: 🌎🚀💻📱");
        
        // 插入Emoji
        buffer->insert(charPos, "👍", false);
        line = buffer->getLineContent(5);
        assert(line == "Emoji: 👍🌎🚀💻📱");
    }
    
    std::cout << "Unicode handling tests passed.\n";
}

// 快照功能测试
void testSnapshotFunctionality() {
    std::cout << "\n=== Testing Snapshot Functionality ===\n";
    Timer timer("Snapshot functionality");
    
    std::string initialText = "Line 1\nLine 2\nLine 3\nLine 4\nLine 5";
    auto buffer = createTestBuffer(initialText);
    
    // 创建快照
    auto snapshot1 = buffer->createSnapshot("");
    
    // 修改文本
    buffer->insert(0, "Modified ", false);
    buffer->deleteText(15, 7); // 删除第二行
    
    // 验证快照内容不变
    std::string snapshotContent = snapshot1->read();
    assert(snapshotContent == initialText);
    
    // 创建第二个快照
    auto snapshot2 = buffer->createSnapshot("");
    
    // 再次修改文本
    buffer->insert(buffer->getLength(), "\nNew line at end", false);
    
    // 验证各个快照
    assert(snapshot1->read() == initialText);
    assert(snapshot2->read() != initialText);
    assert(snapshot2->read() != buffer->getValue());
    
    // 创建第三个快照后销毁之前的快照
    auto snapshot3 = buffer->createSnapshot("");
    snapshot1 = nullptr; // 释放第一个快照
    snapshot2 = nullptr; // 释放第二个快照
    
    // 确保第三个快照仍然有效
    assert(snapshot3->read() == buffer->getValue());
    
    std::cout << "Snapshot functionality tests passed.\n";
}

// 性能稳定性测试
void testPerformanceStability() {
    std::cout << "\n=== Testing Performance Stability ===\n";
    Timer timer("Performance stability");
    
    RandomDataGenerator random;
    
    // 创建一个中等大小的文本（约1MB）
    std::string mediumText = random.randomMultilineString(10000, 100);
    auto buffer = createTestBuffer(mediumText);
    
    std::cout << "Created text with " << buffer->getLineCount() << " lines and "
              << buffer->getLength() << " characters." << std::endl;
    
    // 测试随机位置的读取性能
    {
        Timer readTimer("1000 random position reads");
        for (int i = 0; i < 1000; ++i) {
            int64_t offset = random.randomNumber(0, buffer->getLength() - 1);
            Position pos = buffer->getPositionAt(offset);
            int64_t backToOffset = buffer->getOffsetAt(pos.lineNumber(), pos.column());
            assert(offset == backToOffset);
        }
    }
    
    // 测试随机范围读取性能
    {
        Timer rangeTimer("100 random range reads");
        for (int i = 0; i < 100; ++i) {
            int64_t startOffset = random.randomNumber(0, buffer->getLength() - 2);
            int64_t endOffset = random.randomNumber(startOffset + 1, std::min<int64_t>(buffer->getLength() - 1, startOffset + 1000));
            
            Position startPos = buffer->getPositionAt(startOffset);
            Position endPos = buffer->getPositionAt(endOffset);
            
            Range range(startPos.lineNumber(), startPos.column(), endPos.lineNumber(), endPos.column());
            std::string content = buffer->getValueInRange(range);
            
            assert(content.length() <= (endOffset - startOffset) + 10); // +10 for potential line breaks
        }
    }
    
    // 测试多次编辑后的性能
    {
        Timer editTimer("500 random edits (insert+delete)");
        for (int i = 0; i < 250; ++i) {
            // 插入
            int64_t insertPos = random.randomNumber(0, buffer->getLength());
            std::string text = random.randomString(20);
            buffer->insert(insertPos, text, false);
            
            // 删除
            if (buffer->getLength() > 1) {
                int64_t deletePos = random.randomNumber(0, buffer->getLength() - 1);
                int64_t deleteLen = random.randomNumber(1, std::min<int64_t>(buffer->getLength() - deletePos, 20));
                buffer->deleteText(deletePos, deleteLen);
            }
        }
    }
    
    // 最终检查缓冲区状态
    int64_t finalLines = 0;
    int64_t finalChars = 0;
    
    try {
        finalLines = buffer->getLineCount();
        finalChars = buffer->getLength();
    } catch (const std::exception& e) {
        std::cerr << "Error getting buffer state: " << e.what() << std::endl;
    }
    
    std::cout << "Final buffer state: " << finalLines << " lines, "
              << finalChars << " characters." << std::endl;
    
    std::cout << "Performance stability tests passed.\n";
}

// 极端边界情况测试
void testExtremeEdgeCases() {
    std::cout << "\n=== Testing Extreme Edge Cases ===\n";
    Timer timer("Extreme edge cases");
    
    auto buffer = createTestBuffer();
    
    // 测试非常长的单行文本
    {
        std::string longLine(100000, 'a');
        buffer->insert(0, longLine, false);
        assert(buffer->getLength() == 100000);
        assert(buffer->getLineCount() == 1);
        
        // 在中间插入换行符
        buffer->insert(50000, "\n", false);
        assert(buffer->getLineCount() == 2);
        
        // 检查行长度
        assert(buffer->getLineContent(0).length() == 50000);
        
        // 清空缓冲区
        buffer->deleteText(0, buffer->getLength());
        assert(buffer->getLength() == 0);
    }
    
    // 测试许多短行
    {
        // 创建10000个只有一个字符的行
        for (int i = 0; i < 10000; ++i) {
            buffer->insert(buffer->getLength(), "a\n", false);
        }
        
        assert(buffer->getLineCount() == 10001); // 10000行加1个空行
        
        // 随机读取行
        RandomDataGenerator random;
        for (int i = 0; i < 100; ++i) {
            int64_t line = random.randomNumber(0, 10000);
            std::string content = buffer->getLineContent(line);
            if (line < 10000) {
                assert(content == "a");
            } else {
                assert(content == "");
            }
        }
        
        // 清空缓冲区
        buffer->deleteText(0, buffer->getLength());
    }
    
    // 测试插入位置小于0
    {
        try {
            buffer->insert(-1, "test", false);
            assert(false); // 应该不会执行到这里
        } catch (...) {
            // 应该捕获到异常
        }
    }
    
    // 测试一次性创建和删除大量文本
    {
        std::string largeText(1000000, 'x'); // 1MB的文本
        buffer->insert(0, largeText, false);
        assert(buffer->getLength() == 1000000);
        
        buffer->deleteText(0, 1000000);
        assert(buffer->getLength() == 0);
    }
    
    std::cout << "Extreme edge case tests passed.\n";
}

// 测试缓冲区在可分块的情况下的行为
void testChunkBehavior() {
    std::cout << "\n=== Testing Chunk Behavior ===\n";
    Timer timer("Chunk behavior");
    
    // 准备要分块加载的文本
    std::string text1 = "Chunk 1: This is the first chunk of text.\n";
    std::string text2 = "Chunk 2: This is the second chunk of text.\n";
    std::string text3 = "Chunk 3: This is the third chunk of text.";
    
    // 创建构建器并分块加载
    PieceTreeTextBufferBuilder builder;
    builder.acceptChunk(text1);
    builder.acceptChunk(text2);
    builder.acceptChunk(text3);
    
    auto factory = builder.finish();
    auto buffer = factory.create(DefaultEndOfLine::LF);
    
    // 验证内容
    assert(buffer->getLineCount() == 3);
    assert(buffer->getLineContent(0) == "Chunk 1: This is the first chunk of text.");
    assert(buffer->getLineContent(1) == "Chunk 2: This is the second chunk of text.");
    assert(buffer->getLineContent(2) == "Chunk 3: This is the third chunk of text.");
    
    // 测试跨越不同块的操作
    int64_t pos1 = text1.length() - 5; // 第一块末尾
    int64_t pos2 = text1.length() + 5; // 第二块开头
    
    // 删除跨越两个块的文本
    buffer->deleteText(pos1, 10);
    
    // 验证删除操作已正确处理
    std::string line1 = buffer->getLineContent(0);
    std::string line2 = buffer->getLineContent(1);
    
    assert(line1.substr(line1.length() - 5) + line2.substr(0, 5) != 
           text1.substr(text1.length() - 5, 5) + text2.substr(0, 5));
    
    std::cout << "Chunk behavior tests passed.\n";
}

// 运行所有测试
int main() {
    try {
        std::cout << "=== Starting TextBuffer Comprehensive Tests ===\n";
        
        // 基本测试
        testEmptyBuffer();
        testBasicCRUD();
        testEdgeCases();
        testMultilineOperations();
        
        // 高级测试
        testBulkOperations();
        testEOLHandling();
        testUnicodeHandling();
        testSnapshotFunctionality();
        
        // 性能和稳定性测试
        testPerformanceStability();
        testExtremeEdgeCases();
        testChunkBehavior();
        
        std::cout << "\n=== All TextBuffer Tests Completed Successfully ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
} 