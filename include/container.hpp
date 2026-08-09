#ifndef DOCK_CONTAINER_HPP
#define DOCK_CONTAINER_HPP

/*
    include/container.hpp

    Purpose:
    --------
    Defines the Container class.

    This is the orchestration layer.

    Lower-level classes:
        ContainerConfig     -> stores configuration
        NamespaceProcess    -> wraps clone()
        Filesystem          -> chroot, mount, env
        Cgroup              -> resource limits
        Network             -> bridge and veth

    Container connects them in the correct order.

    Design pattern:
    ---------------
    Facade / Orchestrator.

    The user of this class should not need to manually call:

        cgroup.create()
        clone()
        ip link add ...
        chroot()
        mount /proc
        execvp()

    Instead they should call:

        Container container(config);
        container.run();

    The Container class hides the complicated launch sequence.
*/

#ifdef __linux__

#include "cgroup.hpp"
#include "config.hpp"
#include "namespace_process.hpp"
#include "network.hpp"

#include <vector>
#include <string>

/*
    Container
    ---------

    Represents one container instance.
*/
class Container
{
public:
    /*
        Constructor
        -----------

        Takes a ContainerConfig by value.

        Why by value?
        -------------
        The caller can pass either:
            - an existing ContainerConfig
            - a temporary ContainerConfig

        Then we move it into config_.

        This keeps ownership simple.
    */
    explicit Container(ContainerConfig config);

    /*
        Destructor
        ----------

        Does not automatically run cleanup.

        Cleanup depends on lifecycle state and should happen inside run().
    */
    ~Container();

    /*
        Copy disabled.

        A Container owns process/network/cgroup state.
        Copying it would be confusing and dangerous.
    */
    Container(const Container&) = delete;
    Container& operator=(const Container&) = delete;

    /*
        Move disabled for clarity.
    */
    Container(Container&&) = delete;
    Container& operator=(Container&&) = delete;

    /*
        run()
        -----

        Starts the container and waits for it to exit.

        High-level order:

        Parent:
            1. Bind mount shared folder.
            2. Create cgroup.
            3. clone() child with namespaces.
            4. Setup host-side network.
            5. Add child process to cgroup.
            6. Wait for child.
            7. Cleanup network, cgroup, bind mount.

        Child:
            1. Set hostname.
            2. Setup container-side network.
            3. Setup environment.
            4. chroot into rootfs.
            5. Mount /proc.
            6. execvp(command).
    */
    int run();

private:
    /*
        ChildContext
        ------------

        Data passed into the cloned child process.

        clone() only accepts one void* argument.

        So we group whatever the child needs into one object and pass a
        pointer to it.

        The parent must keep this object alive until clone() has started
        the child and the child has copied/used what it needs.
    */
    class ChildContext
    {
    public:
        ChildContext(
            const ContainerConfig& config,
            Network& network
        );

        const ContainerConfig& config();
        Network& network();

    private:
        const ContainerConfig& config_;
        Network& network_;
    };

    /*
        child_entry()
        -------------

        Static function passed to clone().

        Why static?
        -----------
        clone() expects a plain C-style function pointer:

            int (*)(void*)

        A normal member function has a hidden "this" parameter, so it does
        not match that signature.

        A static member function has no hidden this pointer, so it works.
    */
    static int child_entry(void* arg);

    /*
        execute_command()
        -----------------

        Converts std::vector<std::string> command into char* argv[] and calls
        execvp().

        execvp() replaces the current child process image with the target
        command, such as /bin/sh.
    */
    static void execute_command(const std::vector<std::string>& command);

    /*
        config_
        -------

        Immutable launch configuration for this container.
    */
    ContainerConfig config_;

    /*
        process_
        --------

        Owns clone stack and tracks child PID.
    */
    NamespaceProcess process_;

    /*
        network_
        --------

        Owns names and commands for veth/bridge setup.
    */
    Network network_;

    /*
        cgroup_
        -------

        Owns cgroup path and resource limit operations.
    */
    Cgroup cgroup_;
};

#else

#error "Container requires Linux."

#endif

#endif