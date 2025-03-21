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

// 随机位置和长度生成器
class RandomGenerator {
private:
    std::mt19937 generator;
public:
    RandomGenerator(unsigned int seed = 42) : generator(seed) {}
    
    int64_t random_number(int64_t min, int64_t max) {
        std::uniform_int_distribution<int64_t> dist(min, max);
        return dist(generator);
    }
    
    std::vector<std::pair<int64_t, int64_t>> generate_test_positions(int64_t max_pos, int count, int64_t max_len = 100) {
        std::vector<std::pair<int64_t, int64_t>> positions;
        for (int i = 0; i < count; i++) {
            int64_t pos = random_number(0, max_pos - 1);
            int64_t len = random_number(1, std::min(max_len, max_pos - pos));
            positions.push_back({pos, len});
        }
        return positions;
    }
};

// 创建大文件的工具函数
std::string create_large_text(int64_t target_size_mb) {
    const int64_t target_size = target_size_mb * 1024 * 1024; // MB 转换为字节
    std::cout << "Creating text of approximately " << target_size_mb << "MB..." << std::endl;
    
    // 创建一个基本的重复模式，包含各种字符类型
    std::string pattern;
    for (int i = 0; i < 100; i++) {
        pattern += "Line " + std::to_string(i) + ": The quick brown fox jumps over the lazy dog. 0123456789!@#$%^&*()_+-=[]{}|;':\",./<>?\n";
    }
    
    // 重复模式直到达到目标大小
    int64_t pattern_size = pattern.size();
    int64_t repetitions = target_size / pattern_size + 1;
    
    std::string result;
    result.reserve(target_size); // 预分配内存
    
    for (int64_t i = 0; i < repetitions && result.size() < target_size; i++) {
        result += pattern;
    }
    
    // 截断到目标大小
    if (result.size() > target_size) {
        result.resize(target_size);
    }
    
    std::cout << "Created text of " << (result.size() / (1024 * 1024)) << "MB (" << result.size() << " bytes)" << std::endl;
    return result;
}

// 测试大文件的加载性能
void test_large_file_loading(const std::string& large_text) {
    std::cout << "\n=== Testing Large File Loading ===\n";
    Timer timer("Large file loading test");
    
    // 分块加载文件，每块50MB
    const int64_t chunk_size = 50 * 1024 * 1024;
    int64_t remaining = large_text.size();
    int64_t offset = 0;
    
    PieceTreeTextBufferBuilder builder;
    
    while (remaining > 0) {
        int64_t size = std::min(chunk_size, remaining);
        std::string chunk = large_text.substr(offset, size);
        
        std::cout << "Loading chunk of " << (chunk.size() / (1024 * 1024)) << "MB at offset " 
                  << (offset / (1024 * 1024)) << "MB" << std::endl;
        
        builder.acceptChunk(chunk);
        
        offset += size;
        remaining -= size;
    }
    
    std::cout << "Creating TextBuffer..." << std::endl;
    auto factory = builder.finish();
    auto buffer = factory.create(DefaultEndOfLine::LF);
    
    // 验证加载后的长度
    assert(buffer->getLength() == large_text.size());
    std::cout << "TextBuffer length verified: " << buffer->getLength() << " characters" << std::endl;
    
    // 验证行数统计
    int64_t lineCount = buffer->getLineCount();
    std::cout << "TextBuffer has " << lineCount << " lines" << std::endl;
    
    // 验证一些行的内容
    if (lineCount > 1) {
        std::string firstLine = buffer->getLineContent(0);
        std::string middleLine = buffer->getLineContent(lineCount / 2);
        std::string lastLine = buffer->getLineContent(lineCount - 1);
        
        assert(!firstLine.empty());
        assert(!middleLine.empty());
        
        std::cout << "First line length: " << firstLine.length() << std::endl;
        std::cout << "Middle line length: " << middleLine.length() << std::endl;
        std::cout << "Last line length: " << lastLine.length() << std::endl;
    }
}

// 测试大文件的读取性能
void test_large_file_reading(std::unique_ptr<PieceTreeBase>& buffer) {
    std::cout << "\n=== Testing Large File Reading ===\n";
    
    int64_t length = buffer->getLength();
    int64_t line_count = buffer->getLineCount();
    RandomGenerator random;
    
    // 测试随机行读取
    {
        Timer timer("Random line reading (100 operations)");
        for (int i = 0; i < 100; i++) {
            int64_t line = random.random_number(0, line_count - 1);
            std::string content = buffer->getLineContent(line);
            assert(!content.empty());
        }
    }
    
    // 测试随机范围读取
    {
        Timer timer("Random range reading (50 operations)");
        auto positions = random.generate_test_positions(length, 50, 1024 * 1024); // 最大读取1MB
        
        for (const auto& pos : positions) {
            int64_t start_pos = pos.first;
            int64_t len = pos.second;
            
            common::Position start = buffer->getPositionAt(start_pos);
            common::Position end = buffer->getPositionAt(start_pos + len);
            common::Range range(start.lineNumber(), start.column(), end.lineNumber(), end.column());
            
            std::string content = buffer->getValueInRange(range);
            assert(content.length() <= len + 1); // +1 for potential line breaks
        }
    }
    
    // 测试偏移量和位置转换
    {
        Timer timer("Position and offset conversion (200 operations)");
        for (int i = 0; i < 200; i++) {
            int64_t offset = random.random_number(0, length - 1);
            common::Position pos = buffer->getPositionAt(offset);
            int64_t offset2 = buffer->getOffsetAt(pos.lineNumber(), pos.column());
            
            // 验证转换的正确性
            assert(offset == offset2);
        }
    }
}

// 测试大文件的插入性能
void test_large_file_insertion(std::unique_ptr<PieceTreeBase>& buffer) {
    std::cout << "\n=== Testing Large File Insertion ===\n";
    
    int64_t original_length = buffer->getLength();
    RandomGenerator random;
    
    // 测试小字符串的随机位置插入
    {
        Timer timer("Small insertions at random positions (100 operations)");
        auto positions = random.generate_test_positions(original_length, 100);
        int64_t total_inserted = 0;
        
        for (const auto& pos : positions) {
            std::string text = "INSERTED_TEXT_" + std::to_string(pos.first);
            buffer->insert(pos.first, text, false);
            total_inserted += text.length();
        }
        
        // 验证插入后的长度
        assert(buffer->getLength() == original_length + total_inserted);
    }
    
    // 测试大字符串的插入
    {
        Timer timer("Large string insertion (10 operations)");
        int64_t current_length = buffer->getLength();
        std::string large_insert_text = create_large_text(1); // 1MB 的文本
        
        for (int i = 0; i < 10; i++) {
            int64_t pos = random.random_number(0, current_length);
            buffer->insert(pos, large_insert_text, false);
            current_length += large_insert_text.length();
        }
        
        // 验证插入后的长度
        assert(buffer->getLength() == current_length);
    }
}

// 测试大文件的删除性能
void test_large_file_deletion(std::unique_ptr<PieceTreeBase>& buffer) {
    std::cout << "\n=== Testing Large File Deletion ===\n";
    
    int64_t length = buffer->getLength();
    RandomGenerator random;
    
    // 测试小范围删除
    {
        Timer timer("Small deletions at random positions (100 operations)");
        auto positions = random.generate_test_positions(length, 100, 100); // 最大删除100字符
        int64_t total_deleted = 0;
        
        for (const auto& pos : positions) {
            buffer->deleteText(pos.first, pos.second);
            total_deleted += pos.second;
            length -= pos.second;
        }
        
        // 验证删除后的长度
        assert(buffer->getLength() == length);
    }
    
    // 测试大范围删除
    {
        Timer timer("Large deletions (10 operations)");
        length = buffer->getLength(); // 更新长度
        auto positions = random.generate_test_positions(length, 10, 1024 * 1024); // 最大删除1MB
        
        for (const auto& pos : positions) {
            buffer->deleteText(pos.first, pos.second);
            length -= pos.second;
        }
        
        // 验证删除后的长度
        assert(buffer->getLength() == length);
    }
}

// 测试大文件的快照功能
void test_large_file_snapshot(std::unique_ptr<PieceTreeBase>& buffer) {
    std::cout << "\n=== Testing Large File Snapshot ===\n";
    
    int64_t length = buffer->getLength();
    std::cout << "Current buffer length: " << length << " characters" << std::endl;
    
    // 创建快照
    Timer timer("Creating and reading snapshot");
    auto snapshot = buffer->createSnapshot("");
    
    // 修改原始缓冲区
    buffer->insert(0, "SNAPSHOT_TEST_HEADER\n", false);
    buffer->deleteText(length / 2, 1000);
    
    // 验证快照内容保持不变
    std::string snapshotContent = snapshot->read();
    assert(snapshotContent.length() == length);
    
    std::cout << "Snapshot content length verified: " << snapshotContent.length() << " characters" << std::endl;
}

// 测试程序内存使用情况
void report_memory_usage() {
    std::cout << "\n=== Memory Usage Report ===\n";
    
    // 在此处输出当前内存使用的简单指示 - 使用内存分配器统计
    size_t currently_allocated = 0;
    
    // 记录当前已分配内存的近似值
    try {
        // 尝试分配10MB内存来测试可用内存（不精确但简单）
        std::vector<char> test_allocation(10 * 1024 * 1024);
        std::cout << "Memory allocation test: Successfully allocated 10MB" << std::endl;
    }
    catch (const std::bad_alloc&) {
        std::cout << "Memory allocation test: Failed to allocate 10MB" << std::endl;
    }
    
    std::cout << "Note: For precise memory usage monitoring, please use an external tool." << std::endl;
}

// 主测试函数
void test_very_large_file() {
    std::cout << "=== Starting TextBuffer Large File Tests ===\n";
    
    try {
        // 创建1GB的测试文件
        const int64_t TEST_SIZE_MB = 1024; // 1GB
        std::string large_text = create_large_text(TEST_SIZE_MB);
        
        // 测试加载性能
        test_large_file_loading(large_text);
        
        // 清除不需要的文本数据以释放内存
        large_text.clear();
        large_text.shrink_to_fit();
        report_memory_usage();
        
        // 重新构建缓冲区以进行后续测试
        std::cout << "\n=== Preparing TextBuffer for Subsequent Tests ===\n";
        PieceTreeTextBufferBuilder builder;
        std::string test_text = create_large_text(TEST_SIZE_MB);
        builder.acceptChunk(test_text);
        auto factory = builder.finish();
        auto buffer = factory.create(DefaultEndOfLine::LF);
        
        test_text.clear();
        test_text.shrink_to_fit();
        report_memory_usage();
        
        // 测试读取性能
        test_large_file_reading(buffer);
        
        // 测试插入性能
        test_large_file_insertion(buffer);
        report_memory_usage();
        
        // 测试删除性能
        test_large_file_deletion(buffer);
        report_memory_usage();
        
        // 测试快照功能
        test_large_file_snapshot(buffer);
        
        std::cout << "\n=== All Large File Tests Completed Successfully ===\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    try {
        test_very_large_file();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
} 