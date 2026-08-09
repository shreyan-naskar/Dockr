#include "include/config.hpp"

/*
    src/config.cpp

    Purpose:
    --------
    This file implements the ContainerConfig class declared in:

        include/config.hpp

    Design idea:
    ------------
    The header file describes what the class can do.
    The .cpp file defines how it does it.

    This separation keeps the public interface clean and lets implementation
    details stay inside src/.
*/

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

/*
    die()
    -----

    Small helper used only inside this file.

    Because it is declared as static, it has internal linkage.
    That means this function is private to config.cpp and cannot conflict
    with another die() function in another .cpp file.

    We use it here for argument validation errors.
*/
static void die(const std::string& message)
{
    std::cerr << "[fatal] " << message << "\n";
    std::exit(EXIT_FAILURE);
}

/*
    ContainerConfig constructor
    ---------------------------

    This constructor receives configuration values and moves them into
    private member variables.

    Why std::move?
    --------------
    The constructor parameters are already local copies.

    Instead of copying their contents again into the class members, we move
    their internal buffers.

    For strings and vectors, moving is cheap.
*/
ContainerConfig::ContainerConfig(
    std::string rootfs_path,
    std::string hostname,
    std::string ip_address,
    std::vector<std::string> command
)
    : rootfs_path_(std::move(rootfs_path)),
      hostname_(std::move(hostname)),
      ip_address_(std::move(ip_address)),
      command_(std::move(command))
{
}

/*
    rootfs_path()
    -------------

    Getter for the root filesystem path.

    const std::string& means:
      - no copy is made
      - caller cannot modify the stored value
*/
const std::string& ContainerConfig::rootfs_path() const
{
    return rootfs_path_;
}

/*
    hostname()
    ----------

    Getter for the hostname.
*/
const std::string& ContainerConfig::hostname() const
{
    return hostname_;
}

/*
    ip_address()
    ------------

    Getter for the container IP address.
*/
const std::string& ContainerConfig::ip_address() const
{
    return ip_address_;
}

/*
    command()
    ---------

    Getter for the command vector.

    Later, the process-launching code will convert this vector into a
    char* argv[] array for execvp().
*/
const std::vector<std::string>& ContainerConfig::command() const
{
    return command_;
}

/*
    parse_config()
    --------------

    Converts argc/argv into a ContainerConfig object.

    Expected command format:

        sudo ./mini-container <rootfs> <hostname> <ip> <command> [args...]

    Example:

        sudo ./mini-container ./rootfs host-1 192.168.1.2 /bin/sh

    Another example:

        sudo ./mini-container ./rootfs host-1 192.168.1.2 /bin/sh -c "echo hello"

    Minimum argument count:
    -----------------------
        argv[0] program name
        argv[1] rootfs
        argv[2] hostname
        argv[3] ip
        argv[4] command

    So argc must be at least 5.
*/
ContainerConfig parse_config(int argc, char* argv[])
{
    if (argc < 5)
    {
        die(
            "Usage: ./mini-container <rootfs> <hostname> <ip> <command> [args...]"
        );
    }

    /*
        Copy simple positional arguments into std::string.

        argv values are raw C strings.
        std::string gives us safer ownership and easier passing between classes.
    */
    std::string rootfs_path = argv[1];
    std::string hostname = argv[2];
    std::string ip_address = argv[3];

    /*
        Collect the command and all remaining arguments.

        Starting from argv[4]:

            argv[4] = command executable
            argv[5] = optional first command argument
            argv[6] = optional second command argument
            ...

        Example input:

            ./mini-container ./rootfs host-1 192.168.1.2 /bin/sh -c "echo hi"

        Produces:

            command = {
                "/bin/sh",
                "-c",
                "echo hi"
            }
    */
    std::vector<std::string> command;

    for (int i = 4; i < argc; ++i)
    {
        command.emplace_back(argv[i]);
    }

    /*
        Basic validation.

        More advanced validation, such as checking whether the IP belongs
        to a subnet, can be added later in a Network-related class.
    */
    if (rootfs_path.empty())
    {
        die("rootfs path cannot be empty");
    }

    if (hostname.empty())
    {
        die("hostname cannot be empty");
    }

    if (ip_address.empty())
    {
        die("IP address cannot be empty");
    }

    if (command.empty())
    {
        die("command cannot be empty");
    }

    return ContainerConfig(
        std::move(rootfs_path),
        std::move(hostname),
        std::move(ip_address),
        std::move(command)
    );
}