#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "common/char_code.h"

namespace textbuffer {

/**
 * Unicode utilities for the text buffer
 */
class Unicode {
public:
    /**
     * Check if the string starts with UTF-8 BOM
     */
    static bool startsWithUTF8BOM(const std::string& str);

    /**
     * Get the UTF-8 BOM character
     */
    static std::string UTF8_BOM_CHARACTER;

    /**
     * Get the code point at the given offset in a UTF-8 string
     */
    static uint32_t getUTF8CodePoint(const std::string& str, size_t offset);

    /**
     * Get the number of code points in a UTF-8 string
     */
    static size_t getUTF8Length(const std::string& str);

    /**
     * Check if a character is a high surrogate
     */
    static bool isHighSurrogate(uint32_t charCode);

    /**
     * Check if a character is a low surrogate
     */
    static bool isLowSurrogate(uint32_t charCode);

    /**
     * Compute code point from surrogate pair
     */
    static uint32_t computeCodePoint(uint32_t highSurrogate, uint32_t lowSurrogate);

    /**
     * Get the byte length of a UTF-8 character
     */
    static int getUTF8CharLength(uint8_t firstByte);

    /**
     * Get the string at the given offsets in a UTF-8 string
     */
    static std::string getUTF8Substring(const std::string& str, size_t start, size_t end);

private:
    // Private constructor to prevent instantiation
    Unicode() = default;
};

} // namespace textbuffer 