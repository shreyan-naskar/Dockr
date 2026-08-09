#ifndef DOCKR_ERROR_HPP
#define DOCKR_ERROR_HPP

/*
    include/error.hpp

    Purpose:
    --------
    This file declares a small error-handling class.

    Why do we need this?
    --------------------
    A container runtime calls many low-level Linux APIs:

        clone()
        chroot()
        chdir()
        mount()
        umount2()
        sethostname()
        open()
        write()
        mkdir()
        rmdir()
        waitpid()

    Most of these functions report errors in the C style:

        return -1;
        set errno;

    Example:

        if (chroot(path) == -1)
        {
            // errno tells us why it failed
        }

    Instead of repeating this pattern everywhere:

        std::cerr << "chroot failed: " << strerror(errno) << "\n";
        std::exit(EXIT_FAILURE);

    we centralize it in one small class.
*/

#include <string>

/*
    Error
    -----

    This class does not need object state.

    It is just a namespace-like utility class with static functions.

    Design pattern:
    ---------------
    Utility class for cross-cutting low-level behavior.

    We use a class instead of free functions because you asked to use classes,
    and it gives all error operations one clear home.
*/
class Error
{
public:
    /*
        die()
        -----

        Use this when the failure comes from a Linux syscall or C library call
        that sets errno.

        Example:

            if (chroot(rootfs.c_str()) == -1)
            {
                Error::die("chroot");
            }

        If errno is EACCES, output might be:

            [fatal] chroot failed: Permission denied
    */
    [[noreturn]] static void die(const std::string& operation);

    /*
        die_message()
        -------------

        Use this when the failure is logical and does not come from errno.

        Examples:
            - invalid command-line arguments
            - invalid IP address
            - empty hostname
            - unsupported runtime state

        Example output:

            [fatal] Usage: ./mini-container <rootfs> <hostname> <ip> <command>
    */
    [[noreturn]] static void die_message(const std::string& message);

    /*
        warn()
        ------

        Use this when a syscall failed but the program can continue.

        Example:
            During cleanup, deleting a veth interface may fail because it was
            already deleted by the kernel.

        In that case, we should warn but not always exit.
    */
    static void warn(const std::string& operation);

private:
    /*
        Private constructor
        -------------------

        This prevents code from creating Error objects:

            Error e; // not allowed

        Since every function is static, objects are unnecessary.
    */
    Error() = delete;
};

#endif