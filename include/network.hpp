#ifndef DOCKR_NETWORK_HPP
#define DOCKR_NETWORK_HPP

/*
    include/network.hpp

    Purpose:
    --------
    Defines the Network class.

    This class handles host/container networking.

    What we are building:
    ---------------------

        host namespace                         container namespace

        br0
        192.168.1.1/24
          |
          |
        vethh-host1  ----------------------  vethc-host1
                                             192.168.1.2/24

    Explanation:
    ------------

    A veth pair is like a virtual Ethernet cable.

    One end stays on the host:
        vethh-host1

    The other end is moved into the container network namespace:
        vethc-host1

    A Linux bridge acts like a virtual switch:
        br0

    The host-side veth is attached to br0.

    Then the container gets an IP address on its side of the veth pair.

    Result:
    -------
    The host can communicate with the container IP.
*/

#ifdef __linux__

#include <string>
#include <sys/types.h>  // pid_t

/*
    Network
    -------

    Represents the networking setup for one container.

    Design pattern:
    ---------------
    Subsystem object with explicit setup/cleanup lifecycle.

    This class owns names like:
        bridge name
        host veth name
        child veth name

    It does not own the network namespace itself.
    The namespace belongs to the container process created by clone().
*/
class Network
{
public:
    /*
        Constructor
        -----------

        Parameters:
            hostname:
                Used to build interface names.

            container_ip:
                IP assigned inside container.

        Example:
            hostname = "host-1"

        Interface names:
            vethh-host-1
            vethc-host-1

        Bridge:
            br0
    */
    Network(
        const std::string& hostname,
        const std::string& container_ip
    );

    /*
        Destructor
        ----------

        Does not automatically cleanup.

        Cleanup is explicit because network teardown should happen after the
        container exits and after the parent has finished waiting.
    */
    ~Network();

    /*
        Copy disabled.
    */
    Network(const Network&) = delete;
    Network& operator=(const Network&) = delete;

    /*
        Move disabled for clarity.
    */
    Network(Network&&) = delete;
    Network& operator=(Network&&) = delete;

    /*
        setup_host_side()
        -----------------

        Runs in the parent namespace.

        It creates:
            - bridge br0
            - veth pair
            - moves child end into container netns
            - attaches host end to bridge

        Needs child_pid because this command uses:

            ip link set <child-veth> netns <pid>
    */
    void setup_host_side(pid_t child_pid);

    /*
        setup_container_side()
        ----------------------

        Runs inside the container network namespace.

        It assigns IP address to the child-side veth and brings it up.

        Example:
            ip addr add 192.168.1.2/24 dev vethc-host-1
            ip link set vethc-host-1 up
    */
    void setup_container_side();

    /*
        cleanup()
        ---------

        Runs in the parent namespace after the container exits.

        Deletes host-side veth and optionally removes the bridge.
    */
    void cleanup();

private:
    /*
        run_command()
        -------------

        Helper for shelling out to Linux ip/bridge commands.

        This is simple and easy to understand.

        Later, for a more advanced version, you could replace this with
        netlink sockets to avoid system().
    */
    static void run_command(const std::string& command);

    /*
        run_command_allow_failure()
        ---------------------------

        Used during cleanup where failure is not always fatal.
    */
    static void run_command_allow_failure(const std::string& command);

    /*
        bridge_name_
        ------------

        Bridge name used by this runtime.

        For now:
            br0
    */
    std::string bridge_name_;

    /*
        bridge_ip_
        ----------

        IP address assigned to the bridge.

        For now:
            192.168.1.1/24
    */
    std::string bridge_ip_;

    /*
        container_ip_
        -------------

        IP address assigned to container veth.

        Example:
            192.168.1.2
    */
    std::string container_ip_;

    /*
        veth_host_
        ----------

        Host-side veth interface name.
    */
    std::string veth_host_;

    /*
        veth_child_
        -----------

        Container-side veth interface name.
    */
    std::string veth_child_;
};

#else

#error "Network requires Linux network namespaces and iproute2."

#endif

#endif