#include "cgroup.hpp"

/*
    src/cgroup.cpp

    Purpose:
    --------
    Implements cgroups v2 support.

    cgroups are exposed as files.

    So limiting a container is mostly:
        - create a directory
        - write values into special files
        - write PID into cgroup.procs

    Example:

        mkdir /sys/fs/cgroup/mini-docker-host-1
        echo "+pids +cpu +memory" > /sys/fs/cgroup/cgroup.subtree_control
        echo "10M" > /sys/fs/cgroup/mini-docker-host-1/memory.max
        echo "0" > /sys/fs/cgroup/mini-docker-host-1/memory.swap.max
        echo "25000 100000" > /sys/fs/cgroup/mini-docker-host-1/cpu.max
        echo "5" > /sys/fs/cgroup/mini-docker-host-1/pids.max
        echo "<pid>" > /sys/fs/cgroup/mini-docker-host-1/cgroup.procs
*/

#ifdef __linux__

#include "error.hpp"

#include <cerrno>      // errno
#include <cstring>     // std::strerror
#include <fcntl.h>     // open()
#include <iostream>    // std::cout
#include <string>
#include <sys/stat.h>  // mkdir()
#include <unistd.h>    // write(), close(), rmdir()

/*
    Constructor
    -----------

    Builds the cgroup path.

    For hostname "host-1":

        /sys/fs/cgroup/mini-docker-host-1

    Note:
        This assumes the system uses cgroups v2 mounted at /sys/fs/cgroup.
*/
Cgroup::Cgroup(const std::string& name)
    : cgroup_root_("/sys/fs/cgroup"),
      path_(cgroup_root_ + "/mini-docker-" + name)
{
}

/*
    Destructor
    ----------

    No automatic cleanup.

    cleanup() must be called explicitly after the container exits.
*/
Cgroup::~Cgroup() = default;

/*
    write_file()
    ------------

    Opens a cgroup control file and writes a value.

    Why not std::ofstream?
    ----------------------
    For normal files, std::ofstream is fine.

    For kernel control files, low-level open/write gives clearer error
    behavior and matches the rest of the syscall-based runtime.

    open(path, O_WRONLY)
        Opens the file for writing.

    write(fd, value.data(), value.size())
        Writes bytes to the kernel file.

    close(fd)
        Releases the file descriptor.
*/
void Cgroup::write_file(const std::string& path, const std::string& value)
{
    int fd = open(path.c_str(), O_WRONLY);

    if (fd == -1)
    {
        Error::die("open " + path);
    }

    ssize_t written = write(fd, value.data(), value.size());

    if (written == -1)
    {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        Error::die("write " + path);
    }

    if (static_cast<std::size_t>(written) != value.size())
    {
        close(fd);
        Error::die_message("short write to " + path);
    }

    if (close(fd) == -1)
    {
        Error::die("close " + path);
    }
}

/*
    create()
    --------

    Creates the cgroup directory and enables controllers.

    mkdir(path_, 0755)
        Creates:
            /sys/fs/cgroup/mini-docker-host-1

    If it already exists, we accept EEXIST.
*/
void Cgroup::create()
{
    if (mkdir(path_.c_str(), 0755) == -1 && errno != EEXIST)
    {
        Error::die("mkdir " + path_);
    }

    /*
        Enable controllers in the parent cgroup.

        cgroups v2 rule:
        ----------------
        A child cgroup can only use controllers that are enabled in the
        parent's cgroup.subtree_control file.

        So before writing memory.max/cpu.max/pids.max in our child cgroup,
        we enable the controllers at the root.

        This can fail on some systems if the process is not in the right
        delegated cgroup. For a learning root-run runtime, this is acceptable.
    */
    write_file(
        cgroup_root_ + "/cgroup.subtree_control",
        "+pids +cpu +memory"
    );
}

/*
    apply_limits()
    --------------

    Writes resource limits.

    memory.max = 10M:
        Container can use at most 10 megabytes of memory.

    memory.swap.max = 0:
        Disables swap usage.

    cpu.max = "25000 100000":
        cgroups v2 CPU quota format is:

            quota period

        25000 / 100000 = 0.25

        So this gives roughly 25% of one CPU.

    pids.max = 5:
        Container can have at most 5 processes.
*/
void Cgroup::apply_limits()
{
    write_file(path_ + "/memory.max", "10M");
    write_file(path_ + "/memory.swap.max", "0");
    write_file(path_ + "/cpu.max", "25000 100000");
    write_file(path_ + "/pids.max", "5");
}

/*
    add_process()
    -------------

    Moves a process into the cgroup.

    Writing a PID into cgroup.procs asks the kernel to attach that process
    to this cgroup.

    std::to_string(pid):
        Converts PID number to text because cgroup files accept text input.
*/
void Cgroup::add_process(pid_t pid)
{
    write_file(path_ + "/cgroup.procs", std::to_string(pid));
}

/*
    cleanup()
    ---------

    Removes the cgroup directory.

    This only succeeds when the cgroup is empty.

    If cleanup fails, we warn instead of dying because the container process
    has already exited and we do not want cleanup failure to hide the main
    exit reason.
*/
void Cgroup::cleanup()
{
    if (rmdir(path_.c_str()) == -1)
    {
        Error::warn("rmdir " + path_);
        return;
    }

    std::cout << "[cgroup] removed " << path_ << "\n";
}

/*
    path()
    ------

    Returns full cgroup path.
*/
const std::string& Cgroup::path() const
{
    return path_;
}

#endif