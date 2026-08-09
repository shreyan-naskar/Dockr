#ifndef DOCKR_CONFIG_HPP
#define DOCKR_CONFIG_HPP

/*
    include/clone_stack.hpp

    Purpose:
    --------
    Defines the CloneStack class.

    Why this class exists:
    ----------------------
    Linux clone() is lower-level than fork().

    With fork():
        The kernel creates a child process and gives it a normal stack.

    With clone():
        The caller must provide stack memory manually.

    The clone() function looks roughly like this:

        int clone(
            int (*fn)(void *),
            void *child_stack,
            int flags,
            void *arg
        );

    The second argument, child_stack, must point to memory that the child
    process can use as its stack.

    Very important:
    ---------------
    The stack memory must stay alive until the cloned child process exits.

    Bad design:

        pid_t create_child()
        {
            char stack[65536];

            return clone(child_fn, stack + 65536, flags, arg);
        }

    Why bad?
        stack is a local variable.
        It is destroyed when create_child() returns.
        The child may still be using that memory.

    Good design:
        Put stack memory inside a C++ object.
        Keep that object alive while the child is alive.

    This is RAII:
        Resource Acquisition Is Initialization.

    The object owns the memory.
    When the object dies, memory is released.
*/

#include <cstddef>  // std::size_t
#include <memory>   // std::unique_ptr

/*
    CloneStack
    ----------

    Owns the stack memory passed to clone().

    This class has one job:
        allocate stack memory and return a pointer to the top of it.
*/
class CloneStack
{
public:
    /*
        Default stack size.

        We use 1 MiB.

        64 KiB can work in tiny examples, but 1 MiB is safer for real code
        before execvp() replaces the process image.
    */
    static constexpr std::size_t default_size = 1024 * 1024;

    /*
        Constructor
        -----------

        Allocates stack memory.

        explicit prevents accidental conversions like:

            CloneStack stack = 1234;

        We want construction to be intentional.
    */
    explicit CloneStack(std::size_t size = default_size);

    /*
        Destructor
        ----------

        We do not manually free memory here because std::unique_ptr handles it.

        Still, declaring the destructor makes the ownership model clear in
        the class interface.
    */
    ~CloneStack();

    /*
        Copy disabled
        -------------

        A stack represents one owned memory region.

        Copying would create two CloneStack objects that think they own
        the same resource, or it would require duplicating memory in a way
        that makes no sense for clone().

        So copying is forbidden.
    */
    CloneStack(const CloneStack&) = delete;
    CloneStack& operator=(const CloneStack&) = delete;

    /*
        Move disabled
        -------------

        Moving could be supported, but for this project we keep it disabled.

        Why?
        ----
        Once a CloneStack is associated with a running cloned process, moving
        ownership around makes lifetime reasoning harder.

        Low-level systems code benefits from boring ownership rules.
    */
    CloneStack(CloneStack&&) = delete;
    CloneStack& operator=(CloneStack&&) = delete;

    /*
        top()
        -----

        Returns a pointer to the top of the allocated stack.

        Linux stacks usually grow downward.

        If memory looks like this:

            memory_[0]                         memory_[size - 1]
            lower address -------------------- higher address

        The top of the stack is:

            memory_.get() + size_

        clone() expects that top pointer.
    */
    void* top();

private:
    /*
        Number of bytes allocated for the stack.
    */
    std::size_t size_;

    /*
        Owned heap allocation.

        std::unique_ptr<char[]> means:
            - this class owns a dynamic char array
            - memory is automatically freed
            - copying is naturally prevented
    */
    std::unique_ptr<char[]> memory_;
};

#endif