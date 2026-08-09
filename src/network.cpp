#include "network.hpp"

/*
    src/network.cpp

    Purpose:
    --------
    Implements container networking.

    This file uses the Linux iproute2 command line tools:

        ip
        bridge

    Why use system commands instead of netlink?
    -------------------------------------------
    Real container runtimes usually use netlink sockets directly.

    But netlink code is much more verbose.

    For a learning-focused low-level design, using iproute2 keeps the network
    lifecycle understandable while still exposing the real Linux primitives:

        bridge
        veth
        network namespace
        interface IP assignment

    Later, you can replace run_command() with a Netlink class.
*/

#ifdef __linux__

#include "error.hpp"

#include <cstdlib>     // std::system()
#include <iostream>    // std::cout
#include <string>
#include <sys/wait.h>  // WIFEXITED(), WEXITSTATUS()

/*
    Constructor
    -----------

    Builds names used for network interfaces.

    Example:
        hostname = "host-1"

    Result:
        bridge_name_  = "br0"
        bridge_ip_    = "192.168.1.1/24"
        container_ip_ = "192.168.1.2"
        veth_host_    = "vethh-host-1"
        veth_child_   = "vethc-host-1"

    Note:
        Linux interface names have a length limit, usually IFNAMSIZ = 16.

        That means very long hostnames can break veth creation.

        Later we can sanitize/truncate names.
*/
Network::Network(
    const std::string& hostname,
    const std::string& container_ip
)
    : bridge_name_("br0"),
      bridge_ip_("192.168.1.1/24"),
      container_ip_(container_ip),
      veth_host_("vethh-" + hostname),
      veth_child_("vethc-" + hostname)
{
}

/*
    Destructor
    ----------

    No automatic cleanup.

    Network cleanup should be controlled by the higher-level Container class.
*/
Network::~Network() = default;

/*
    run_command()
    -------------

    Executes a shell command.

    std::system() returns encoded wait status, not just the command's exit code.

    We check:
        - system() itself failed
        - command did not exit normally
        - command exited with non-zero status

    Important security note:
    ------------------------
    system() is vulnerable to shell injection if input values are untrusted.

    This learning runtime assumes trusted input.

    Production runtime:
        use fork/exec or netlink, not system().
*/
void Network::run_command(const std::string& command)
{
    int status = std::system(command.c_str());

    if (status == -1)
    {
        Error::die("system: " + command);
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        Error::die_message("command failed: " + command);
    }
}

/*
    run_command_allow_failure()
    ---------------------------

    Cleanup helper.

    During cleanup, failure is often acceptable:
        - veth may already be deleted
        - bridge may still be used by another container
        - interface may already be down

    So this function logs the command but does not exit.
*/
void Network::run_command_allow_failure(const std::string& command)
{
    int status = std::system(command.c_str());

    if (status != 0)
    {
        std::cerr << "[warn] cleanup command failed: " << command << "\n";
    }
}

/*
    setup_host_side()
    -----------------

    Runs in the parent namespace.

    Step-by-step:

    1. Create bridge:
        ip link add br0 type bridge

    2. Assign bridge IP:
        ip addr add 192.168.1.1/24 dev br0

    3. Bring bridge up:
        ip link set br0 up

    4. Create veth pair:
        ip link add vethh-host-1 type veth peer name vethc-host-1

    5. Move child veth into child network namespace:
        ip link set vethc-host-1 netns <child_pid>

    6. Attach host veth to bridge:
        ip link set vethh-host-1 master br0

    7. Bring host veth up:
        ip link set vethh-host-1 up
*/
void Network::setup_host_side(pid_t child_pid)
{
    run_command_allow_failure(
        "ip link add " + bridge_name_ + " type bridge"
    );

    run_command_allow_failure(
        "ip addr add " + bridge_ip_ + " dev " + bridge_name_
    );

    run_command(
        "ip link set " + bridge_name_ + " up"
    );

    run_command(
        "ip link add " + veth_host_ + " type veth peer name " + veth_child_
    );

    run_command(
        "ip link set " + veth_child_ + " netns " + std::to_string(child_pid)
    );

    run_command(
        "ip link set " + veth_host_ + " master " + bridge_name_
    );

    run_command(
        "ip link set " + veth_host_ + " up"
    );
}

/*
    setup_container_side()
    ----------------------

    Runs inside the child process after it enters the new network namespace.

    The parent has already moved veth_child_ into this namespace.

    Commands:

        ip addr add <container_ip>/24 dev <veth_child>
        ip link set <veth_child> up
        ip link set lo up

    Bringing lo up is useful because many programs expect loopback to exist.
*/
void Network::setup_container_side()
{
    run_command(
        "ip addr add " + container_ip_ + "/24 dev " + veth_child_
    );

    run_command(
        "ip link set " + veth_child_ + " up"
    );

    run_command_allow_failure(
        "ip link set lo up"
    );
}

/*
    cleanup()
    ---------

    Runs in the parent namespace after the child exits.

    Deleting the host-side veth also removes the peer link.

    Bridge cleanup:
        We try to bring br0 down and delete it.

    In a multi-container runtime, you would only delete the bridge when no
    other containers are attached.

    This learning version keeps cleanup simple.
*/
void Network::cleanup()
{
    run_command_allow_failure(
        "ip link set " + veth_host_ + " down"
    );

    run_command_allow_failure(
        "ip link delete " + veth_host_
    );

    run_command_allow_failure(
        "ip link set " + bridge_name_ + " down"
    );

    run_command_allow_failure(
        "ip link delete " + bridge_name_ + " type bridge"
    );

    std::cout << "[network] cleanup complete\n";
}

#endif