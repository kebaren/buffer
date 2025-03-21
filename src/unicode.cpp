#include "textbuffer/unicode.h"

namespace textbuffer {

std::string Unicode::UTF8_BOM_CHARACTER = "\xEF\xBB\xBF";

bool Unicode::startsWithUTF8BOM(const std::string& str) {
    return !str.empty() && str.length() >= 3 &&
           static_cast<uint8_t>(str[0]) == 0xEF &&
           static_cast<uint8_t>(str[1]) == 0xBB &&
           static_cast<uint8_t>(str[2]) == 0xBF;
}

uint32_t Unicode::getUTF8CodePoint(const std::string& str, size_t offset) {
    if (offset >= str.length()) {
        return 0;
    }

    uint8_t firstByte = static_cast<uint8_t>(str[offset]);
    
    if (firstByte < 0x80) {
        // 1-byte sequence (0xxxxxxx)
        return firstByte;
    } else if ((firstByte & 0xE0) == 0xC0) {
        // 2-byte sequence (110xxxxx 10xxxxxx)
        if (offset + 1 >= str.length()) {
            // Incomplete sequence
            return 0xFFFD; // Replacement character
        }
        
        uint8_t secondByte = static_cast<uint8_t>(str[offset + 1]);
        if ((secondByte & 0xC0) != 0x80) {
            // Invalid sequence
            return 0xFFFD;
        }
        
        return ((firstByte & 0x1F) << 6) | (secondByte & 0x3F);
    } else if ((firstByte & 0xF0) == 0xE0) {
        // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
        if (offset + 2 >= str.length()) {
            // Incomplete sequence
            return 0xFFFD;
        }
        
        uint8_t secondByte = static_cast<uint8_t>(str[offset + 1]);
        uint8_t thirdByte = static_cast<uint8_t>(str[offset + 2]);
        
        if ((secondByte & 0xC0) != 0x80 || (thirdByte & 0xC0) != 0x80) {
            // Invalid sequence
            return 0xFFFD;
        }
        
        return ((firstByte & 0x0F) << 12) | ((secondByte & 0x3F) << 6) | (thirdByte & 0x3F);
    } else if ((firstByte & 0xF8) == 0xF0) {
        // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
        if (offset + 3 >= str.length()) {
            // Incomplete sequence
            return 0xFFFD;
        }
        
        uint8_t secondByte = static_cast<uint8_t>(str[offset + 1]);
        uint8_t thirdByte = static_cast<uint8_t>(str[offset + 2]);
        uint8_t fourthByte = static_cast<uint8_t>(str[offset + 3]);
        
        if ((secondByte & 0xC0) != 0x80 || (thirdByte & 0xC0) != 0x80 || (fourthByte & 0xC0) != 0x80) {
            // Invalid sequence
            return 0xFFFD;
        }
        
        return ((firstByte & 0x07) << 18) | ((secondByte & 0x3F) << 12) | 
               ((thirdByte & 0x3F) << 6) | (fourthByte & 0x3F);
    } else {
        // Invalid UTF-8 sequence
        return 0xFFFD;
    }
}

size_t Unicode::getUTF8Length(const std::string& str) {
    size_t length = 0;
    for (size_t i = 0; i < str.length();) {
        int charLen = getUTF8CharLength(static_cast<uint8_t>(str[i]));
        if (charLen == 0) {
            // Invalid sequence, treat as single byte
            i++;
        } else {
            i += charLen;
        }
        length++;
    }
    return length;
}

bool Unicode::isHighSurrogate(uint32_t charCode) {
    return 0xD800 <= charCode && charCode <= 0xDBFF;
}

bool Unicode::isLowSurrogate(uint32_t charCode) {
    return 0xDC00 <= charCode && charCode <= 0xDFFF;
}

uint32_t Unicode::computeCodePoint(uint32_t highSurrogate, uint32_t lowSurrogate) {
    return ((highSurrogate - 0xD800) << 10) + (lowSurrogate - 0xDC00) + 0x10000;
}

int Unicode::getUTF8CharLength(uint8_t firstByte) {
    if (firstByte < 0x80) {
        return 1;
    } else if ((firstByte & 0xE0) == 0xC0) {
        return 2;
    } else if ((firstByte & 0xF0) == 0xE0) {
        return 3;
    } else if ((firstByte & 0xF8) == 0xF0) {
        return 4;
    }
    return 0; // Invalid UTF-8 sequence
}

std::string Unicode::getUTF8Substring(const std::string& str, size_t start, size_t end) {
    if (start >= str.length()) {
        return "";
    }
    
    // Find byte position for start
    size_t byteStart = 0;
    size_t codePointCount = 0;
    
    while (byteStart < str.length() && codePointCount < start) {
        int charLen = getUTF8CharLength(static_cast<uint8_t>(str[byteStart]));
        if (charLen == 0) {
            // Invalid sequence, treat as single byte
            byteStart++;
        } else {
            byteStart += charLen;
        }
        codePointCount++;
    }
    
    // Find byte position for end
    size_t byteEnd = byteStart;
    while (byteEnd < str.length() && codePointCount < end) {
        int charLen = getUTF8CharLength(static_cast<uint8_t>(str[byteEnd]));
        if (charLen == 0) {
            // Invalid sequence, treat as single byte
            byteEnd++;
        } else {
            byteEnd += charLen;
        }
        codePointCount++;
    }
    
    return str.substr(byteStart, byteEnd - byteStart);
}

} // namespace textbuffer 