#ifndef DOCKR_CGROUP_HPP
#define DOCKR_CGROUP_HPP

/*
    include/cgroup.hpp

    Purpose:
    --------
    Defines the Cgroup class.

    A cgroup controls resource usage for one container.

    Linux namespaces isolate what a process can see.
    Linux cgroups limit what a process can consume.

    Examples:
        - memory limit
        - CPU limit
        - process count limit

    This project uses cgroups v2.

    cgroups v2 filesystem:
    ----------------------

        /sys/fs/cgroup/

    We will create one child directory:

        /sys/fs/cgroup/mini-docker-<hostname>

    Example:

        /sys/fs/cgroup/mini-docker-host-1

    Inside that directory, Linux exposes control files:

        memory.max
        memory.swap.max
        cpu.max
        pids.max
        cgroup.procs
        cgroup.events
*/

#ifdef __linux__

#include <string>
#include <sys/types.h>  // pid_t

/*
    Cgroup
    ------

    Represents one cgroup v2 directory for one container.

    Design pattern:
    ---------------
    RAII-style ownership of a kernel filesystem resource.

    The object knows:
        - where the cgroup directory is
        - how to write resource limits
        - how to move a process into the cgroup
        - how to clean up after the process exits
*/
class Cgroup
{
public:
    /*
        Constructor
        -----------

        Takes a name and builds a cgroup path.

        Example:
            name = "host-1"

        Path becomes:
            /sys/fs/cgroup/mini-docker-host-1

        Why not always "/sys/fs/cgroup/container"?
        ------------------------------------------
        A fixed path causes collisions.

        If you run two containers, both would fight over the same cgroup.
        A name-based path is cleaner.
    */
    explicit Cgroup(const std::string& name);

    /*
        Destructor
        ----------

        Does not automatically remove the cgroup.

        Why?
        ----
        Removing a cgroup only succeeds when it is empty.

        Cleanup timing matters, so the higher-level Container class should
        explicitly call cleanup() after the child exits.
    */
    ~Cgroup();

    /*
        Copy disabled
        -------------

        Two Cgroup objects should not represent the same kernel cgroup path.
    */
    Cgroup(const Cgroup&) = delete;
    Cgroup& operator=(const Cgroup&) = delete;

    /*
        Move disabled for clarity.
    */
    Cgroup(Cgroup&&) = delete;
    Cgroup& operator=(Cgroup&&) = delete;

    /*
        create()
        --------

        Creates the cgroup directory.

        Also enables required controllers in the parent cgroup:

            +pids
            +cpu
            +memory
    */
    void create();

    /*
        apply_limits()
        --------------

        Writes resource limits into cgroup files.

        For this project:
            memory.max       = 10M
            memory.swap.max  = 0
            cpu.max          = 25000 100000
            pids.max         = 5

        Meaning:
            memory limited to 10 MB
            swap disabled
            CPU limited to 25% of one CPU
            max 5 processes
    */
    void apply_limits();

    /*
        add_process()
        -------------

        Moves a process into this cgroup by writing its PID into:

            cgroup.procs

        Example:
            echo 12345 > /sys/fs/cgroup/mini-docker-host-1/cgroup.procs
    */
    void add_process(pid_t pid);

    /*
        cleanup()
        ---------

        Removes the cgroup directory.

        This will only work when no processes remain inside the cgroup.
    */
    void cleanup();

    /*
        path()
        ------

        Returns the full cgroup path.

        Useful for logging/debugging.
    */
    const std::string& path() const;

private:
    /*
        write_file()
        ------------

        Helper for writing to cgroup control files.

        Example:
            write_file(path_ + "/memory.max", "10M");
    */
    static void write_file(const std::string& path, const std::string& value);

    /*
        cgroup_root_
        ------------

        Root of cgroup v2 filesystem.

        Usually:
            /sys/fs/cgroup
    */
    std::string cgroup_root_;

    /*
        path_
        -----

        Full path to this container's cgroup directory.
    */
    std::string path_;
};

#else

#error "Cgroup requires Linux cgroups v2."

#endif

#endif