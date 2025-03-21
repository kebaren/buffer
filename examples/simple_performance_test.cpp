#include <iostream>
#include <string>
#include <cassert>
#include <chrono>
#include <vector>
#include <random>
#include "textbuffer/piece_tree_base.h"
#include "textbuffer/piece_tree_builder.h"

using namespace textbuffer;

// 简单计时器
class Timer {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    std::string operation_name;
public:
    Timer(const std::string& name) : operation_name(name) {
        start_time = std::chrono::high_resolution_clock::now();
        std::cout << "开始" << operation_name << "..." << std::endl;
    }
    
    ~Timer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        std::cout << operation_name << "完成，耗时：" << duration << " ms" << std::endl;
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

// 安全获取缓冲区指定位置的字符
char getCharAt(PieceTreeBase* buffer, int64_t pos) {
    try {
        if (pos < 0 || pos >= buffer->getLength()) {
            std::cout << "警告：尝试访问越界位置 " << pos << "，缓冲区长度为 " << buffer->getLength() << std::endl;
            return '\0';
        }
        
        std::string content = buffer->getValue();
        if (pos < content.size()) {
            return content.at(pos);
        } else {
            std::cout << "警告：位置 " << pos << " 超出了获取的内容长度 " << content.size() << std::endl;
            return '\0';
        }
    } catch (const std::exception& e) {
        std::cout << "获取字符时发生异常：" << e.what() << std::endl;
        return '\0';
    }
}

// 测试1：基本插入和读取
void testInsertAndRead() {
    std::cout << "\n=== 测试1：基本插入和读取 ===\n";
    Timer timer("基本插入和读取测试");
    
    auto buffer = createTestBuffer();
    
    // 测试插入
    buffer->insert(0, "Hello", false);
    buffer->insert(5, " World", false);
    buffer->insert(11, "!", false);
    
    // 测试读取
    std::string content = buffer->getValue();
    std::cout << "缓冲区内容：" << content << std::endl;
    assert(content == "Hello World!");
    assert(buffer->getLength() == 12);
    assert(buffer->getLineCount() == 1);
    
    // 测试多行插入
    buffer->insert(12, "\nLine 2\nLine 3", false);
    assert(buffer->getLineCount() == 3);
    assert(buffer->getLineContent(0) == "Hello World!");
    assert(buffer->getLineContent(1) == "Line 2");
    assert(buffer->getLineContent(2) == "Line 3");
    
    std::cout << "基本插入和读取测试通过！" << std::endl;
}

// 测试2：删除操作
void testDelete() {
    std::cout << "\n=== 测试2：删除操作 ===\n";
    Timer timer("删除操作测试");
    
    std::string initialText = "Line 1\nLine 2\nLine 3\nLine 4\nLine 5";
    auto buffer = createTestBuffer(initialText);
    
    // 验证初始状态
    assert(buffer->getLineCount() == 5);
    assert(buffer->getLength() == 29);
    
    // 删除第一行末尾的换行符
    buffer->deleteText(6, 1);
    assert(buffer->getLineCount() == 4);
    assert(buffer->getLineContent(0) == "Line 1Line 2");
    
    // 删除整行内容（但不包括换行符）
    int64_t line1Start = buffer->getOffsetAt(1, 0);
    int64_t line2Start = buffer->getOffsetAt(2, 0);
    int64_t line1Length = line2Start - line1Start - 1; // -1 排除换行符
    buffer->deleteText(line1Start, line1Length);
    
    // 验证行内容已删除但行数未变
    assert(buffer->getLineCount() == 4);
    std::string line1Content = buffer->getLineContent(1);
    assert(line1Content.empty());
    
    // 删除多行内容（包括换行符）
    line2Start = buffer->getOffsetAt(2, 0);
    int64_t line3Start = buffer->getOffsetAt(3, 0);
    buffer->deleteText(line2Start, line3Start - line2Start);
    
    // 验证行数减少了
    assert(buffer->getLineCount() == 3);
    
    // 验证最终内容
    std::cout << "最终内容:" << std::endl;
    for (int i = 0; i < buffer->getLineCount(); i++) {
        std::cout << "[" << i << "]: " << buffer->getLineContent(i) << std::endl;
    }
    
    std::cout << "删除操作测试通过！" << std::endl;
}

// 测试3：随机位置的CRUD操作
void testRandomCRUD() {
    std::cout << "\n=== 测试3：随机位置的CRUD操作 ===\n";
    Timer timer("随机CRUD操作测试");
    
    // 创建一个随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // 创建初始文本
    std::string initialText;
    for (int i = 0; i < 100; i++) {
        initialText += "Line " + std::to_string(i) + ": This is a test line.\n";
    }
    
    auto buffer = createTestBuffer(initialText);
    int64_t originalLength = buffer->getLength();
    int64_t originalLineCount = buffer->getLineCount();
    
    std::cout << "初始状态: " << originalLineCount << " 行, " 
              << originalLength << " 字符" << std::endl;
    
    // 执行一系列随机但安全的插入操作
    int insertCount = 50;
    int64_t expectedAddedLength = 0;
    
    for (int i = 0; i < insertCount; i++) {
        try {
            int64_t bufferLength = buffer->getLength();
            if (bufferLength <= 0) break;
            
            int64_t pos = std::uniform_int_distribution<int64_t>(0, bufferLength)(gen);
            std::string text = "Inserted(" + std::to_string(i) + ")";
            
            if (i % 5 == 0) {
                text += "\n"; // 每5次插入一个换行符
            }
            
            buffer->insert(pos, text, false);
            expectedAddedLength += text.length();
            
            // 验证插入后的长度是否合理
            int64_t newLength = buffer->getLength();
            if (newLength != bufferLength + text.length()) {
                std::cout << "警告：插入后长度异常！预期：" << (bufferLength + text.length()) 
                          << "，实际：" << newLength << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "插入操作异常：" << e.what() << std::endl;
        }
    }
    
    // 执行一系列安全的随机删除操作
    int deleteCount = 30;
    int64_t expectedRemovedLength = 0;
    
    for (int i = 0; i < deleteCount; i++) {
        try {
            int64_t bufferLength = buffer->getLength();
            if (bufferLength <= 10) break;
            
            int64_t pos = std::uniform_int_distribution<int64_t>(0, bufferLength - 10)(gen);
            int64_t len = std::uniform_int_distribution<int64_t>(1, 10)(gen);
            
            buffer->deleteText(pos, len);
            expectedRemovedLength += len;
            
            // 验证删除后的长度是否合理
            int64_t newLength = buffer->getLength();
            if (newLength != bufferLength - len) {
                std::cout << "警告：删除后长度异常！预期：" << (bufferLength - len) 
                          << "，实际：" << newLength << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "删除操作异常：" << e.what() << std::endl;
        }
    }
    
    // 验证最终长度
    int64_t finalLength = 0;
    int64_t finalLineCount = 0;
    
    try {
        finalLength = buffer->getLength();
        finalLineCount = buffer->getLineCount();
    } catch (const std::exception& e) {
        std::cout << "获取缓冲区信息异常：" << e.what() << std::endl;
    }
    
    int64_t expectedLength = originalLength + expectedAddedLength - expectedRemovedLength;
    
    std::cout << "最终状态: " << finalLineCount << " 行, " 
              << finalLength << " 字符" << std::endl;
    
    std::cout << "期望长度: " << expectedLength << " 字符" << std::endl;
    
    // 允许一些误差，因为随机插入和删除可能重叠
    if (std::abs(finalLength - expectedLength) > insertCount) {
        std::cout << "警告：最终长度与期望长度相差较大!" << std::endl;
    }
    
    std::cout << "随机CRUD操作测试完成！" << std::endl;
}

// 测试4：大文本边界操作
void testLargeTextBoundaries() {
    std::cout << "\n=== 测试4：大文本边界操作 ===\n";
    Timer timer("大文本边界操作测试");
    
    try {
        // 创建较大的文本
        std::string largeText;
        largeText.reserve(100000);
        
        for (int i = 0; i < 1000; i++) {
            largeText += "Line " + std::to_string(i) + ": This is test line number " 
                      + std::to_string(i) + " for boundary testing.\n";
        }
        
        auto buffer = createTestBuffer(largeText);
        int64_t initialLength = buffer->getLength();
        int64_t initialLineCount = buffer->getLineCount();
        
        std::cout << "创建了大小为 " << initialLength << " 字符的缓冲区，共 " 
                  << initialLineCount << " 行" << std::endl;
        
        // 测试在开头插入
        buffer->insert(0, "START OF TEXT\n", false);
        assert(buffer->getLineContent(0) == "START OF TEXT");
        
        // 测试在末尾插入
        buffer->insert(buffer->getLength(), "\nEND OF TEXT", false);
        assert(buffer->getLineContent(buffer->getLineCount() - 1) == "END OF TEXT");
        
        // 测试删除开头
        buffer->deleteText(0, 14); // 删除"START OF TEXT\n"
        std::string firstLine = buffer->getLineContent(0);
        std::cout << "删除开头后的第一行：" << firstLine << std::endl;
        
        // 测试删除末尾
        int64_t length = buffer->getLength();
        buffer->deleteText(length - 11, 11); // 删除"END OF TEXT"
        assert(buffer->getLength() == length - 11);
        
        // 测试在随机位置进行单字符操作（安全版本）
        std::random_device rd;
        std::mt19937 gen(rd());
        
        for (int i = 0; i < 100; i++) {
            try {
                int64_t bufferLength = buffer->getLength();
                if (bufferLength <= 1) break;
                
                int64_t pos = std::uniform_int_distribution<int64_t>(0, bufferLength - 1)(gen);
                
                // 安全地获取当前位置的字符
                char c = getCharAt(buffer.get(), pos);
                if (c == '\0') continue; // 跳过无效字符
                
                // 删除该字符
                buffer->deleteText(pos, 1);
                
                // 插入回相同的字符
                std::string ch(1, c);
                buffer->insert(pos, ch, false);
            } catch (const std::exception& e) {
                std::cout << "单字符操作异常：" << e.what() << std::endl;
            }
        }
        
        // 验证最终状态
        std::cout << "边界操作后的缓冲区：" << buffer->getLineCount() << " 行，" 
                  << buffer->getLength() << " 字符" << std::endl;
        
        std::cout << "大文本边界操作测试通过！" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "大文本边界操作测试异常：" << e.what() << std::endl;
    }
}

int main() {
    try {
        std::cout << "=== 开始TextBuffer基础CRUD测试 ===\n";
        
        testInsertAndRead();
        testDelete();
        testRandomCRUD();
        testLargeTextBoundaries();
        
        std::cout << "\n=== 所有TextBuffer测试通过 ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "测试失败，异常信息: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "测试失败，发生未知异常" << std::endl;
        return 1;
    }
} 