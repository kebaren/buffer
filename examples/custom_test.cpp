#include <iostream>
#include <cassert>
#include <string>
#include "textbuffer/textbuffer.h"

using namespace textbuffer;

int main() {
  
    auto buffer = textbuffer::TextBuffer();
    buffer.insert(0, "Hello");
    std::cout<<buffer.getValue()<<std::endl;
    buffer.insert(5, "World");
    std::cout<<buffer.getValue()<<std::endl;
    buffer.insert(5, ", ");
    buffer.insert(7,"Good");
    std::cout<<buffer.getValue()<<std::endl;
    buffer.deleteText(5, 1);
    std::cout<<buffer.getValue()<<std::endl;
    buffer.deleteText(7, 4);
    std::cout<<buffer.getValue()<<std::endl;
    buffer.deleteText(7, 1);
    std::cout<<buffer.getValue()<<std::endl;
    return 0;
} 