#include "textbuffer/piece_tree_base.h"
#include <vector>
#include <string>

namespace textbuffer {

std::vector<int32_t> createLineStartsFast(const std::string& str) {
    std::vector<int32_t> result;
    result.push_back(0);

    for (size_t i = 0; i < str.length(); i++) {
        char ch = str[i];
        if (ch == '\r') {
            if (i + 1 < str.length() && str[i + 1] == '\n') {
                result.push_back(i + 2);
                i++;
            } else {
                result.push_back(i + 1);
            }
        } else if (ch == '\n') {
            result.push_back(i + 1);
        }
    }

    return result;
}

LineStarts createLineStarts(const std::string& str) {
    std::vector<int32_t> lineStarts;
    lineStarts.push_back(0);
    int32_t cr = 0;
    int32_t lf = 0;
    int32_t crlf = 0;
    bool isBasicASCII = true;

    for (size_t i = 0; i < str.length(); i++) {
        unsigned char ch = static_cast<unsigned char>(str[i]);
        
        if (ch == 0x0D /* \r */) {
            // Carriage return
            if (i + 1 < str.length() && static_cast<unsigned char>(str[i + 1]) == 0x0A /* \n */) {
                // \r\n
                crlf++;
                lineStarts.push_back(i + 2);
                i++;
            } else {
                // \r
                cr++;
                lineStarts.push_back(i + 1);
            }
        } else if (ch == 0x0A /* \n */) {
            // Line feed
            lf++;
            lineStarts.push_back(i + 1);
        } else if (ch >= 128) {
            // Not basic ASCII
            isBasicASCII = false;
        }
    }

    return LineStarts(lineStarts, cr, lf, crlf, isBasicASCII);
}

} // namespace textbuffer 