#include "include/clone_stack.hpp"

#include <utility>

CloneStack::CloneStack(std::size_t size) :
    size_(size),
    memory_(new char[size]) {}

CloneStack::~CloneStack() = default;

void* CloneStack::top() {
    return memory_.get() + size_;
}
