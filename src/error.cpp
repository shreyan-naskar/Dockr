#include "include/error.hpp"

/*
    src/error.cpp

    Purpose:
    --------
    Implements the Error utility class declared in:

        include/error.hpp

    This file centralizes fatal and non-fatal error reporting.

    Keeping this logic in one place makes the rest of the container runtime
    easier to read because syscall-heavy code can simply say:

        Error::die("mount /proc");

    instead of repeating errno formatting everywhere.
*/

#include <cerrno>    // errno
#include <cstdlib>   // std::exit(), EXIT_FAILURE
#include <cstring>   // std::strerror()
#include <iostream>  // std::cerr

/*
    Error::die()
    ------------

    Used after a failed syscall or C library call that sets errno.

    Example:

        if (mount("proc", "/proc", "proc", 0, nullptr) == -1)
        {
            Error::die("mount /proc");
        }

    If errno contains EPERM, this prints:

        [fatal] mount /proc failed: Operation not permitted

    Then it exits the program.
*/
[[noreturn]] void Error::die(const std::string& operation)
{
    std::cerr
        << "[fatal] "
        << operation
        << " failed: "
        << std::strerror(errno)
        << "\n";

    std::exit(EXIT_FAILURE);
}

/*
    Error::die_message()
    --------------------

    Used for errors that are not tied to errno.

    Example:
        Invalid command-line usage.

    This prints the message and exits.
*/
[[noreturn]] void Error::die_message(const std::string& message)
{
    std::cerr << "[fatal] " << message << "\n";
    std::exit(EXIT_FAILURE);
}

/*
    Error::warn()
    -------------

    Used when something failed but the runtime can continue.

    Example:
        Cleanup code tries to delete a network interface.
        If it is already gone, we may only want a warning.

    This uses errno, just like die(), but does not exit.
*/
void Error::warn(const std::string& operation)
{
    std::cerr
        << "[warn] "
        << operation
        << " failed: "
        << std::strerror(errno)
        << "\n";
}