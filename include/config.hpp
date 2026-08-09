#ifndef DOCKR_CONFIG_HPP
#define DOCKR_CONFIG_HPP

/*
    include/config.hpp

    Purpose:
    --------
    This file defines the ContainerConfig class.

    Low-level design idea:
    ----------------------
    A container runtime needs many values before it can start:

        - root filesystem path
        - hostname
        - IP address
        - command to execute

    Instead of passing many loose variables everywhere, we group them into
    one class.

    This follows an object-oriented design pattern:

        "Encapsulate related state behind a clear interface."

    The ContainerConfig class owns the configuration data.
    Other parts of the runtime can read the configuration through small
    getter functions.

    Important:
    ----------
    This class does not perform any Linux syscalls.

    It does not:
        - call clone()
        - call chroot()
        - call mount()
        - create cgroups
        - configure networking

    It only stores validated runtime configuration.
*/

#include <string>
#include <vector>

/*
    ContainerConfig
    ---------------

    A small class that stores container launch settings.

    We keep members private so other code cannot accidentally modify them
    after construction.

    This is useful because configuration should be stable once the container
    starts.
*/
class ContainerConfig
{
public:
    /*
        Constructor
        -----------

        Takes all values needed to describe one container.

        Parameters:
            rootfs_path:
                Path to the extracted root filesystem.

                Example:
                    "./rootfs"

            hostname:
                Hostname that will be visible inside the container.

                Example:
                    "host-1"

            ip_address:
                IP address assigned to the container.

                Example:
                    "192.168.1.2"

            command:
                Command and arguments to execute inside the container.

                Example:
                    {"/bin/sh"}

                Another example:
                    {"/bin/sh", "-c", "echo hello"}
    */
    ContainerConfig(
        std::string rootfs_path,
        std::string hostname,
        std::string ip_address,
        std::vector<std::string> command
    );

    /*
        rootfs_path()
        -------------

        Returns the root filesystem path.

        We return const std::string& instead of std::string to avoid copying.
        The caller can read the value but cannot modify it.
    */
    const std::string& rootfs_path() const;

    /*
        hostname()
        ----------

        Returns the hostname configured for the container.
    */
    const std::string& hostname() const;

    /*
        ip_address()
        ------------

        Returns the container IP address.
    */
    const std::string& ip_address() const;

    /*
        command()
        ---------

        Returns the command vector.

        This will later be converted into argv format for execvp().
    */
    const std::vector<std::string>& command() const;

private:
    /*
        Private member variables
        ------------------------

        The trailing underscore is a common C++ naming convention for
        private class members.

        Example:
            rootfs_path_ is the private storage.
            rootfs_path() is the public getter.
    */
    std::string rootfs_path_;
    std::string hostname_;
    std::string ip_address_;
    std::vector<std::string> command_;
};

/*
    parse_config()
    --------------

    This function will read argc/argv from main() and create a
    ContainerConfig object.

    Example command:

        sudo ./mini-container ./rootfs host-1 192.168.1.2 /bin/sh

    argv values:

        argv[0] = "./mini-container"
        argv[1] = "./rootfs"
        argv[2] = "host-1"
        argv[3] = "192.168.1.2"
        argv[4] = "/bin/sh"

    Result:

        ContainerConfig(
            "./rootfs",
            "host-1",
            "192.168.1.2",
            {"/bin/sh"}
        )

    Only declared here.
    The implementation will be in:

        src/config.cpp
*/
ContainerConfig parse_config(int argc, char* argv[]);

#endif