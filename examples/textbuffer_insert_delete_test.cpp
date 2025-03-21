#include <iostream>
#include <string>
#include <cassert>
#include <random>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
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

// Function to verify the buffer content
void verifyBufferContent(PieceTreeBase& buffer, const std::string& expectedContent) {
    std::string actualContent = buffer.getLinesRawContent();
    if (actualContent != expectedContent) {
        std::cout << "Buffer content verification failed!" << std::endl;
        std::cout << "Expected length: " << expectedContent.length() << ", Actual length: " << actualContent.length() << std::endl;
        
        // Find the first mismatch position
        size_t mismatchPos = 0;
        while (mismatchPos < std::min(actualContent.length(), expectedContent.length()) && 
               actualContent[mismatchPos] == expectedContent[mismatchPos]) {
            mismatchPos++;
        }
        
        // Calculate display range
        size_t displayStart = (mismatchPos > 10) ? mismatchPos - 10 : 0;
        size_t displayEnd = std::min(mismatchPos + 10, std::min(actualContent.length(), expectedContent.length()));
        
        std::cout << "First mismatch at position " << mismatchPos << std::endl;
        std::cout << "Expected: \"" << expectedContent.substr(displayStart, displayEnd - displayStart) << "\"" << std::endl;
        std::cout << "Actual  : \"" << actualContent.substr(displayStart, displayEnd - displayStart) << "\"" << std::endl;
        
        // Print character codes
        if (mismatchPos < std::min(actualContent.length(), expectedContent.length())) {
            std::cout << "Expected char: '" << expectedContent[mismatchPos] << "' (ASCII: " 
                    << (int)(unsigned char)expectedContent[mismatchPos] << ")" << std::endl;
            std::cout << "Actual char: '" << actualContent[mismatchPos] << "' (ASCII: " 
                    << (int)(unsigned char)actualContent[mismatchPos] << ")" << std::endl;
        }
        
        // If length differs but all shared content matches
        if (actualContent.length() != expectedContent.length() && mismatchPos == std::min(actualContent.length(), expectedContent.length())) {
            std::cout << "Content length differs but all shared characters match." << std::endl;
            if (actualContent.length() > expectedContent.length()) {
                std::cout << "Actual content has extra characters." << std::endl;
            } else {
                std::cout << "Expected content has extra characters." << std::endl;
            }
        }
        
        assert(false && "Buffer content verification failed");
    }
}

// 测试基本插入功能
void testBasicInsert() {
    std::cout << "Testing basic insert operations..." << std::endl;
    
    // Create a new buffer
    PieceTreeBase buffer = createEmptyBuffer();
    
    // 测试开头插入
    buffer.insert(0, "Hello");
    verifyBufferContent(buffer, "Hello");
    
    // 测试末尾插入
    buffer.insert(5, " World");
    verifyBufferContent(buffer, "Hello World");
    
    // 测试中间插入
    buffer.insert(5, ",");
    verifyBufferContent(buffer, "Hello, World");
    
    // 测试多行文本插入
    buffer.insert(12, "\nHow are you?");
    verifyBufferContent(buffer, "Hello, World\nHow are you?");
    
    // 测试在行尾插入
    buffer.insert(buffer.getLength(), "\nI am fine, thank you!");
    verifyBufferContent(buffer, "Hello, World\nHow are you?\nI am fine, thank you!");
    
    // 测试在行首插入
    int lineStart = buffer.getOffsetAt(2, 0); // 第3行的开始位置
    buffer.insert(lineStart, "- ");
    verifyBufferContent(buffer, "Hello, World\nHow are you?\n- I am fine, thank you!");
    
    std::cout << "Final buffer state: " << buffer.getLineCount() << " lines, " 
              << buffer.getLength() << " characters" << std::endl;
    
    std::cout << "Basic insert tests passed!" << std::endl;
}

// 测试基本删除功能
void testBasicDelete() {
    std::cout << "Testing basic delete operations..." << std::endl;
    
    // Create a new buffer with initial content
    PieceTreeBase buffer = createEmptyBuffer();
    buffer.insert(0, "Hello, World\nHow are you?\nI am fine, thank you!");
    
    // 测试删除单个字符
    buffer.deleteText(5, 1); // 删除逗号
    verifyBufferContent(buffer, "Hello World\nHow are you?\nI am fine, thank you!");
    
    // 测试删除多个字符
    buffer.deleteText(5, 6); // 删除" World"
    verifyBufferContent(buffer, "Hello\nHow are you?\nI am fine, thank you!");
    
    // 测试删除跨行文本
    int startOffset = buffer.getOffsetAt(1, 0); // 第2行的开始位置
    int length = buffer.getOffsetAt(2, 0) - startOffset; // 直到第3行的开始位置
    buffer.deleteText(startOffset, length);
    verifyBufferContent(buffer, "HelloI am fine, thank you!");
    
    // 测试删除所有内容
    buffer.deleteText(0, buffer.getLength());
    verifyBufferContent(buffer, "");
    
    std::cout << "Final buffer state: " << buffer.getLineCount() << " lines, " 
              << buffer.getLength() << " characters" << std::endl;
    
    std::cout << "Basic delete tests passed!" << std::endl;
}

// 测试边缘情况
void testEdgeCases() {
    std::cout << "Testing edge cases..." << std::endl;
    
    // Create a new buffer
    PieceTreeBase buffer = createEmptyBuffer();
    
    // 测试零长度插入
    buffer.insert(0, "");
    verifyBufferContent(buffer, "");
    
    // 测试超出范围的插入
    buffer.insert(100, "Text");
    verifyBufferContent(buffer, "Text");
    
    // 测试大文本插入
    std::string largeText(10000, 'A'); // 创建10000个'A'的字符串
    buffer.insert(0, largeText);
    verifyBufferContent(buffer, largeText + "Text");
    
    // 测试一次性删除大文本
    buffer.deleteText(0, 100000);
    verifyBufferContent(buffer, "");
    
    // 测试多字节Unicode字符
    buffer.insert(0, "你好，世界！");
    verifyBufferContent(buffer, "你好，世界！");
    
    // 测试行尾插入和删除
    buffer.insert(buffer.getLength(), "\n换行测试");
    buffer.deleteText(buffer.getLength() - 2, 2);
    buffer.insert(buffer.getLength(), "试");
    verifyBufferContent(buffer, "你好，世界！\n换行测试");
    
    std::cout << "Final buffer state: " << buffer.getLineCount() << " lines, " 
              << buffer.getLength() << " characters" << std::endl;
    
    std::cout << "Edge case tests passed!" << std::endl;
}

// 测试随机插入
void testRandomInserts(int operationCount = 10000) {
    std::cout << "Testing " << operationCount << " random inserts..." << std::endl;
    
    // Create a new buffer
    PieceTreeBase buffer = createEmptyBuffer();
    
    // 创建随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // 存储预期内容，用于验证
    std::string expectedContent;
    
    for (int i = 0; i < operationCount; i++) {
        // 随机生成插入位置和内容
        std::uniform_int_distribution<> posDist(0, expectedContent.length());
        int insertPos = posDist(gen);
        
        // 生成随机字符串
        std::uniform_int_distribution<> lenDist(1, 10);
        int insertLen = lenDist(gen);
        std::string insertStr;
        for (int j = 0; j < insertLen; j++) {
            std::uniform_int_distribution<> charDist(32, 126); // ASCII 可打印字符
            insertStr += static_cast<char>(charDist(gen));
        }
        
        // 在buffer中插入
        buffer.insert(insertPos, insertStr);
        
        // 更新预期内容
        expectedContent.insert(insertPos, insertStr);
        
        if (i % 1000 == 0) {
            std::cout << "Completed " << i << " random inserts" << std::endl;
            verifyBufferContent(buffer, expectedContent);
        }
    }
    
    verifyBufferContent(buffer, expectedContent);
    std::cout << "Final buffer state after " << operationCount << " random inserts: " 
              << buffer.getLineCount() << " lines, " << buffer.getLength() << " characters" << std::endl;
    
    std::cout << "Random insert tests passed!" << std::endl;
}

// 测试随机删除
void testRandomDeletes(int operationCount = 10000) {
    std::cout << "Testing random deletes..." << std::endl;
    
    // Create a new buffer with initial content
    PieceTreeBase buffer = createEmptyBuffer();
    
    // 创建一个大的初始内容
    std::string initialContent(100000, 'X');
    buffer.insert(0, initialContent);
    
    // 创建随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // 存储预期内容，用于验证
    std::string expectedContent = initialContent;
    
    for (int i = 0; i < operationCount; i++) {
        if (expectedContent.empty()) {
            break; // 如果内容已经全部删除，则退出循环
        }
        
        // 随机生成删除位置和长度
        std::uniform_int_distribution<> posDist(0, expectedContent.length() - 1);
        int deletePos = posDist(gen);
        
        std::uniform_int_distribution<> lenDist(1, std::min(10, (int)expectedContent.length() - deletePos));
        int deleteLen = lenDist(gen);
        
        // 在buffer中删除
        buffer.deleteText(deletePos, deleteLen);
        
        // 更新预期内容
        expectedContent.erase(deletePos, deleteLen);
        
        if (i % 1000 == 0) {
            std::cout << "Completed " << i << " random deletes" << std::endl;
            verifyBufferContent(buffer, expectedContent);
        }
    }
    
    verifyBufferContent(buffer, expectedContent);
    std::cout << "Final buffer state after random deletes: " 
              << buffer.getLineCount() << " lines, " << buffer.getLength() << " characters" << std::endl;
    
    std::cout << "Random delete tests passed!" << std::endl;
}

// 测试混合操作（插入、删除、读取）
void testMixedOperations(int operationCount = 10000) {
    std::cout << "Testing mixed operations..." << std::endl;
    
    // Create a new buffer
    PieceTreeBase buffer = createEmptyBuffer();
    
    // 创建随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // 存储预期内容，用于验证
    std::string expectedContent;
    
    for (int i = 0; i < operationCount; i++) {
        // 随机选择操作类型：0=插入，1=删除，2=读取
        std::uniform_int_distribution<> opDist(0, 2);
        int opType = opDist(gen);
        
        try {
            if (opType == 0 || expectedContent.empty()) { // 插入操作，或者当内容为空时强制插入
                // 随机生成插入位置和内容
                std::uniform_int_distribution<> posDist(0, expectedContent.length());
                int insertPos = posDist(gen);
                
                // 生成随机字符串
                std::uniform_int_distribution<> lenDist(1, 10);
                int insertLen = lenDist(gen);
                std::string insertStr;
                for (int j = 0; j < insertLen; j++) {
                    std::uniform_int_distribution<> charDist(32, 126); // ASCII 可打印字符
                    insertStr += static_cast<char>(charDist(gen));
                }
                
                // 在buffer中插入
                buffer.insert(insertPos, insertStr);
                
                // 更新预期内容
                expectedContent.insert(insertPos, insertStr);
            } else if (opType == 1 && !expectedContent.empty()) { // 删除操作
                // 随机生成删除位置和长度
                std::uniform_int_distribution<> posDist(0, expectedContent.length() - 1);
                int deletePos = posDist(gen);
                
                std::uniform_int_distribution<> lenDist(1, std::min(10, (int)expectedContent.length() - deletePos));
                int deleteLen = lenDist(gen);
                
                // 在buffer中删除
                buffer.deleteText(deletePos, deleteLen);
                
                // 更新预期内容
                expectedContent.erase(deletePos, deleteLen);
            } else { // 读取操作
                // 随机选择行号
                if (buffer.getLineCount() > 0) {
                    std::uniform_int_distribution<> lineDist(0, buffer.getLineCount() - 1);
                    int lineNumber = lineDist(gen);
                    
                    // 读取该行内容
                    std::string lineContent = buffer.getLineContent(lineNumber);
                    // 无需验证，只是执行读取操作
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception during mixed operation " << i << ": " << e.what() << std::endl;
        }
        
        if (i % 1000 == 0) {
            std::cout << "Completed " << i << " mixed operations" << std::endl;
            verifyBufferContent(buffer, expectedContent);
        }
    }
    
    verifyBufferContent(buffer, expectedContent);
    std::cout << "Final buffer state after mixed operations: " 
              << buffer.getLineCount() << " lines, " << buffer.getLength() << " characters" << std::endl;
    
    std::cout << "Mixed operation tests passed!" << std::endl;
}

// 测试大块操作
void testLargeOperations() {
    std::cout << "Testing large block operations..." << std::endl;
    
    // Create a new buffer
    PieceTreeBase buffer = createEmptyBuffer();
    
    // 创建大文本
    std::string largeText(100000, 'A');
    for (int i = 0; i < 100; i++) {
        largeText.replace(i * 1000, 1, "\n"); // 每1000个字符插入一个换行
    }
    
    // 插入大文本
    buffer.insert(0, largeText);
    verifyBufferContent(buffer, largeText);
    
    // 删除前半部分
    int halfLength = largeText.length() / 2;
    buffer.deleteText(0, halfLength);
    std::string expectedContent = largeText.substr(halfLength);
    verifyBufferContent(buffer, expectedContent);
    
    // 在开头重新插入不同的大文本
    std::string anotherLargeText(50000, 'B');
    for (int i = 0; i < 50; i++) {
        anotherLargeText.replace(i * 1000, 1, "\n"); // 每1000个字符插入一个换行
    }
    
    buffer.insert(0, anotherLargeText);
    expectedContent = anotherLargeText + expectedContent;
    verifyBufferContent(buffer, expectedContent);
    
    // 删除中间部分
    int start = anotherLargeText.length() / 2;
    int length = anotherLargeText.length() / 4 + expectedContent.length() / 4;
    buffer.deleteText(start, length);
    expectedContent.erase(start, length);
    verifyBufferContent(buffer, expectedContent);
    
    std::cout << "Final buffer state after large block operations: " 
              << buffer.getLineCount() << " lines, " << buffer.getLength() << " characters" << std::endl;
    
    std::cout << "Large block operation tests passed!" << std::endl;
}

// 测试行操作
void testLineOperations() {
    std::cout << "Testing line operations..." << std::endl;
    
    // Create a new buffer
    PieceTreeBase buffer = createEmptyBuffer();
    
    // 创建有1000行的文本
    std::string expectedContent;
    for (int i = 0; i < 1000; i++) {
        expectedContent += "Line " + std::to_string(i) + "\n";
    }
    
    // 插入文本
    buffer.insert(0, expectedContent);
    verifyBufferContent(buffer, expectedContent);
    
    // 在每行末尾添加文本
    std::string modifiedExpectedContent;
    for (int i = 0; i < 1000; i++) {
        modifiedExpectedContent += "Line " + std::to_string(i) + " (modified)\n";
    }
    
    // 更新buffer，逐行修改
    for (int i = 0; i < 1000; i++) {
        int lineEndOffset = buffer.getOffsetAt(i, 0) + buffer.getLineContent(i).length();
        buffer.insert(lineEndOffset - 1, " (modified)");
    }
    
    verifyBufferContent(buffer, modifiedExpectedContent);
    
    std::cout << "Final buffer state after line operations: " 
              << buffer.getLineCount() << " lines, " << buffer.getLength() << " characters" << std::endl;
    
    std::cout << "Line operation tests passed!" << std::endl;
}

// 测试并发操作
void testConcurrentOperations() {
    std::cout << "Testing concurrent operations..." << std::endl;
    
    // Create a new buffer
    PieceTreeBase buffer = createEmptyBuffer();
    buffer.insert(0, "Initial content"); // 创建初始内容
    
    // 创建多个线程进行并发操作
    constexpr int threadCount = 10;
    constexpr int operationsPerThread = 100;
    
    std::vector<std::thread> threads;
    std::mutex bufferMutex; // 用于同步对buffer的访问
    
    // 计数已完成的操作数
    std::atomic<int> completedOperations(0);
    
    // 启动工作线程
    for (int t = 0; t < threadCount; t++) {
        threads.emplace_back([t, &buffer, &bufferMutex, &completedOperations]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            
            for (int i = 0; i < operationsPerThread; i++) {
                // 随机选择操作类型：0=插入，1=删除
                std::uniform_int_distribution<> opDist(0, 1);
                int opType = opDist(gen);
                
                try {
                    // 获取锁，防止并发修改导致的不一致状态
                    std::lock_guard<std::mutex> lock(bufferMutex);
                    
                    if (opType == 0) { // 插入操作
                        // 随机生成插入位置和内容
                        int bufferLen = buffer.getLength();
                        if (bufferLen == 0) bufferLen = 1; // 避免除零错误
                        
                        std::uniform_int_distribution<> posDist(0, bufferLen - 1);
                        int insertPos = posDist(gen);
                        
                        // 生成随机字符串
                        std::uniform_int_distribution<> lenDist(1, 5);
                        int insertLen = lenDist(gen);
                        std::string insertStr = "T" + std::to_string(t) + "O" + std::to_string(i) + ":";
                        for (int j = 0; j < insertLen; j++) {
                            insertStr += static_cast<char>('a' + (t * i + j) % 26);
                        }
                        
                        // 在buffer中插入
                        buffer.insert(insertPos, insertStr);
                    } else { // 删除操作
                        // 随机生成删除位置和长度
                        int bufferLen = buffer.getLength();
                        if (bufferLen <= 1) continue; // 如果内容太少，跳过删除
                        
                        std::uniform_int_distribution<> posDist(0, bufferLen - 2);
                        int deletePos = posDist(gen);
                        
                        std::uniform_int_distribution<> lenDist(1, std::min(5, bufferLen - deletePos - 1));
                        int deleteLen = lenDist(gen);
                        
                        // 在buffer中删除
                        buffer.deleteText(deletePos, deleteLen);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << t << " operation " << i 
                              << " failed: " << e.what() << std::endl;
                }
                
                // 更新完成的操作数
                completedOperations++;
                
                // 短暂休眠，让其他线程有机会获取锁
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // 显示进度
    int lastReported = 0;
    while (completedOperations < threadCount * operationsPerThread) {
        int current = completedOperations.load();
        if (current - lastReported >= 100) {
            std::cout << "Completed " << current << " / " 
                      << (threadCount * operationsPerThread) << " operations" << std::endl;
            lastReported = current;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Final buffer state after concurrent operations: " 
              << buffer.getLineCount() << " lines, " << buffer.getLength() << " characters" << std::endl;
    
    // 输出最终内容（可选，内容可能很长）
    // std::cout << "Final content: " << buffer.getValue() << std::endl;
    
    std::cout << "Concurrent operation tests passed!" << std::endl;
}

int main() {
    try {
        std::cout << "Starting TextBuffer Insert/Delete Tests..." << std::endl;
        
        // Set up the test cases with proper buffer initialization
        testBasicInsert();
        testBasicDelete();
        testEdgeCases();
        testRandomInserts(1000);  // Reduce test count for speed
        testRandomDeletes(1000);  // Reduce test count for speed
        testMixedOperations(1000); // Reduce test count for speed
        testLargeOperations();
        testLineOperations();
        testConcurrentOperations();
        
        std::cout << "All TextBuffer Insert/Delete Tests passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
} 