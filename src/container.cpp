#include "include/container.hpp"

/*
    src/container.cpp

    Purpose:
    --------
    Implements the high-level container lifecycle.

    This file connects all lower-level pieces:

        NamespaceProcess
        Network
        Filesystem
        Cgroup
        Config

    Parent and child do different work.

    Parent process:
        - creates cgroup
        - starts child with clone()
        - configures host-side veth/bridge
        - waits
        - cleans up

    Child process:
        - enters namespaces automatically through clone flags
        - sets hostname
        - configures child network side
        - chroots
        - mounts /proc
        - execs shell/program
*/

#ifdef __linux__

#include "include/error.hpp"
#include "include/filesystem.hpp"

#include <csignal>      // SIGCHLD
#include <cstring>      // std::strlen
#include <iostream>     // std::cout
#include <sched.h>      // CLONE_NEWNS, CLONE_NEWNET, ...
#include <sys/utsname.h>
#include <unistd.h>     // sethostname(), execvp(), _exit()
#include <utility>      // std::move
#include <vector>

/*
    Container constructor
    ---------------------

    Member initialization order follows the order in container.hpp:

        config_
        process_
        network_
        cgroup_

    network_ needs:
        hostname
        IP address

    cgroup_ needs:
        hostname

    We use hostname to create unique names like:
        vethh-host-1
        mini-docker-host-1
*/
Container::Container(ContainerConfig config)
    : config_(std::move(config)),
      process_(),
      network_(config_.hostname(), config_.ip_address()),
      cgroup_(config_.hostname())
{
}

/*
    Destructor
    ----------

    Explicit cleanup happens in run().
*/
Container::~Container() = default;

/*
    ChildContext constructor
    ------------------------

    Stores references needed by the child process.

    Important:
    ----------
    These are references to objects owned by Container.

    The Container object lives in the parent memory before clone().
    With clone(), the child receives its own virtual memory view.

    Because we are not using CLONE_VM, the child has a separate memory space,
    like fork(). The references point to copied memory in the child.

    That is okay for read/use during startup.
*/
Container::ChildContext::ChildContext(
    const ContainerConfig& config,
    Network& network
)
    : config_(config),
      network_(network)
{
}

/*
    config()
    --------

    Returns child access to container config.
*/
const ContainerConfig& Container::ChildContext::config()
{
    return config_;
}

/*
    network()
    ---------

    Returns child access to Network object.

    The child uses this only for setup_container_side().
*/
Network& Container::ChildContext::network()
{
    return network_;
}

/*
    run()
    -----

    Starts the container.

    Detailed lifecycle:
    -------------------

    1. Bind mount shared folder into rootfs.

       Host:
           ./shared_folder

       Container path:
           /var/shared_folder

       Actual host mount target:
           ./rootfs/var/shared_folder

    2. Create cgroup directory.

    3. Apply resource limits.

    4. clone() child with namespaces.

    5. Parent configures bridge/veth.

    6. Parent moves child process into cgroup.

    7. Parent waits for child to exit.

    8. Parent cleans network/cgroup/shared mount.
*/
int Container::run()
{
    const std::string shared_host_path = "./shared_folder";
    const std::string shared_container_path = "/var/shared_folder";

    Filesystem::bind_mount_shared_folder(
        shared_host_path,
        config_.rootfs_path(),
        shared_container_path
    );

    cgroup_.create();
    cgroup_.apply_limits();

    ChildContext child_context(config_, network_);

    /*
        Namespace flags:
        ----------------

        CLONE_NEWNS:
            New mount namespace.
            Mount changes inside child do not affect host.

        CLONE_NEWNET:
            New network namespace.
            Child starts with separate network stack.

        CLONE_NEWPID:
            New PID namespace.
            The first process inside becomes PID 1 inside the namespace.

        CLONE_NEWUTS:
            New UTS namespace.
            Allows child to have its own hostname.

        SIGCHLD:
            Parent receives SIGCHLD when child exits.
            Required so waitpid() behaves like normal child waiting.
    */
    int flags =
        CLONE_NEWNS |
        CLONE_NEWNET |
        CLONE_NEWPID |
        CLONE_NEWUTS |
        SIGCHLD;

    process_.start(
        &Container::child_entry,
        flags,
        &child_context
    );

    std::cout << "[parent] child pid: " << process_.pid() << "\n";

    /*
        Network setup must happen after clone().

        Why?
        ----
        The parent needs the child PID so it can move one veth endpoint
        into the child's network namespace.
    */
    network_.setup_host_side(process_.pid());

    /*
        Add child to cgroup.

        Note:
        -----
        If CLONE_NEWPID is used, process_.pid() is the PID as seen by parent.
        That is the correct PID to write into cgroup.procs.
    */
    cgroup_.add_process(process_.pid());

    int status = process_.wait();

    network_.cleanup();
    cgroup_.cleanup();

    Filesystem::unmount_shared_folder(
        config_.rootfs_path(),
        shared_container_path
    );

    return status;
}

/*
    child_entry()
    -------------

    This function runs inside the cloned child process.

    Because clone() used namespace flags, this child is already inside:

        - new mount namespace
        - new network namespace
        - new PID namespace
        - new UTS namespace

    The parent still needs to finish moving the veth interface into this
    network namespace. For now, the child network setup may race with parent
    setup. A later improvement is to add pipe-based synchronization.

    For learning simplicity, this version expects the parent to finish quickly.
*/
int Container::child_entry(void* arg)
{
    ChildContext* context = static_cast<ChildContext*>(arg);

    const ContainerConfig& config = context->config();

    if (sethostname(
            config.hostname().c_str(),
            config.hostname().size()
        ) == -1)
    {
        Error::die("sethostname");
    }

    /*
        Small race warning:
        -------------------
        The parent creates the veth pair and moves veth_child into this
        namespace after clone() returns.

        If this child runs too early, setup_container_side() may fail because
        the interface is not present yet.

        Your original project used sleep(2).

        A better production design uses a pipe:
            child waits
            parent sets up veth
            parent signals child
            child continues

        We will add that in a later refinement step if you want.
    */
    context->network().setup_container_side();

    Filesystem::setup_environment();

    Filesystem::change_root(config.rootfs_path());

    Filesystem::mount_proc();

    execute_command(config.command());

    /*
        execute_command() should never return if execvp() succeeds.
        If it returns, it already reported a fatal error.
    */
    return 1;
}

/*
    execute_command()
    -----------------

    Converts:

        std::vector<std::string>

    into:

        char* argv[]

    because execvp() expects C-style argv.

    Example:

        command = {
            "/bin/sh",
            "-c",
            "echo hello"
        }

    becomes:

        argv[0] = "/bin/sh"
        argv[1] = "-c"
        argv[2] = "echo hello"
        argv[3] = nullptr

    nullptr is required because execvp() expects argv to be null-terminated.
*/
void Container::execute_command(const std::vector<std::string>& command)
{
    if (command.empty())
    {
        Error::die_message("empty command");
    }

    std::vector<char*> argv;

    argv.reserve(command.size() + 1);

    for (const std::string& part : command)
    {
        /*
            execvp() takes char* const argv[] instead of const char* const[].

            It does not normally modify the strings, but the old C API does
            not express const-correctness here.

            const_cast is acceptable in this specific case because the strings
            remain alive until execvp() replaces the process.
        */
        argv.push_back(const_cast<char*>(part.c_str()));
    }

    argv.push_back(nullptr);

    execvp(argv[0], argv.data());

    /*
        If execvp succeeds, the current process image is replaced and this
        code is never reached.

        If it fails, execvp returns -1 and sets errno.
    */
    Error::die("execvp " + command[0]);
}

#endif