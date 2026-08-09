#include "include/clone_stack.hpp"

/*
    src/clone_stack.cpp

    Purpose:
    --------
    Implements the CloneStack class declared in:

        include/clone_stack.hpp

    This class owns the memory used as the child stack for Linux clone().

    Important concept:
    ------------------
    clone() needs a raw stack pointer.

    C++ gives us safer ownership through std::unique_ptr, but clone() itself
    still wants a plain void*.

    So this class acts as a bridge:

        C++ ownership  ->  Linux raw pointer API
*/

#include <utility>

/*
    Constructor
    -----------

    Allocates a heap buffer of "size" bytes.

    memory_(new char[size])
    -----------------------
    This creates raw byte storage.

    We do not use std::string because this is not text.
    We do not store characters with meaning.
    We only need memory that the CPU can use as a stack.

    size_(size)
    -----------
    We store the size because top() needs to calculate:

        memory_.get() + size_

    That gives clone() the top of the stack.
*/
CloneStack::CloneStack(std::size_t size)
    : size_(size),
      memory_(new char[size])
{
}

/*
    Destructor
    ----------

    Nothing manual is needed here.

    Why?
    ----
    memory_ is std::unique_ptr<char[]>.

    When CloneStack is destroyed, unique_ptr automatically calls:

        delete[] memory

    This is the RAII pattern:
        the object owns the resource,
        and cleanup happens automatically.
*/
CloneStack::~CloneStack() = default;

/*
    top()
    -----

    Returns the address that should be passed to clone().

    Linux/x86 stacks grow downward.

    Example:

        Suppose memory_ points to address 1000
        and size_ is 100 bytes.

        Allocated memory range:

            1000 ---------------------- 1099

        The top pointer is:

            1000 + 100 = 1100

        clone() starts using stack memory below that address.

    Why return void*?
    -----------------
    clone() expects a void* stack pointer.

    memory_.get() returns char*.
    char* can safely convert to void*.
*/
void* CloneStack::top()
{
    return memory_.get() + size_;
}