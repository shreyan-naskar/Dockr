#include "include/namespace_process.hpp"

/*
    src/namespace_process.cpp

    Purpose:
    --------
    Implements the NamespaceProcess class.

    This file is where we actually call:

        clone()
        waitpid()

    This is one of the most important files in the runtime because clone()
    creates the container's first process.
*/

#ifdef __linux__

#include "include/error.hpp"

#include <cerrno>      // errno
#include <sched.h>     // clone()
#include <sys/wait.h>  // waitpid()
#include <unistd.h>    // pid_t

/*
    Constructor
    -----------

    stack_ is automatically constructed first.

    pid_ starts as -1, meaning:
        no child process has been created yet.
*/
NamespaceProcess::NamespaceProcess()
    : stack_(),
      pid_(-1)
{
}

/*
    Destructor
    ----------

    We intentionally do not call waitpid() here.

    Why?
    ----
    Process waiting is an explicit action.

    The higher-level Container class should decide when to wait because it
    also needs to coordinate cleanup:

        - unmount /proc
        - remove cgroup
        - delete veth
        - unmount shared folder

    If the destructor waited automatically, cleanup ordering would become
    less obvious.
*/
NamespaceProcess::~NamespaceProcess() = default;

/*
    start()
    -------

    Calls Linux clone().

    clone() parameters:
    -------------------

    1. child_fn
       Function that runs in the child process.

       It must have type:
           int (*)(void*)

    2. stack_.top()
       Pointer to the top of manually allocated child stack memory.

    3. flags
       Namespace and process flags.

       Example:
           CLONE_NEWNS
           CLONE_NEWNET
           CLONE_NEWPID
           CLONE_NEWUTS
           SIGCHLD

    4. arg
       Raw pointer passed into child_fn.

    Return value:
    -------------

    In the parent:
        clone() returns child PID.

    On failure:
        clone() returns -1 and sets errno.

    The child does not return here in the normal parent-style way.
    It begins execution inside child_fn.
*/
void NamespaceProcess::start(ChildFunction child_fn, int flags, void* arg)
{
    if (pid_ != -1)
    {
        Error::die_message("NamespaceProcess already started");
    }

    pid_ = clone(
        child_fn,
        stack_.top(),
        flags,
        arg
    );

    if (pid_ == -1)
    {
        Error::die("clone");
    }
}

/*
    wait()
    ------

    Waits for the cloned child process.

    Why waitpid()?
    --------------
    When a child process exits, the kernel keeps a small record containing
    its exit status. Until the parent reads that status, the child remains
    as a zombie process.

    waitpid(pid_, &status, 0) does two things:
        - blocks until this specific child exits
        - reaps the child so it does not become a zombie

    Return:
        status is the raw wait status.

    Later the caller can inspect it with:
        WIFEXITED(status)
        WEXITSTATUS(status)
        WIFSIGNALED(status)
        WTERMSIG(status)
*/
int NamespaceProcess::wait()
{
    if (pid_ == -1)
    {
        Error::die_message("NamespaceProcess::wait called before start");
    }

    int status = 0;

    if (waitpid(pid_, &status, 0) == -1)
    {
        Error::die("waitpid");
    }

    return status;
}

/*
    pid()
    -----

    Returns the child PID as seen by the parent namespace.

    This is used by the networking setup.

    Example:

        ip link set vethc-host1 netns 12345

    where 12345 is this PID.
*/
pid_t NamespaceProcess::pid() const
{
    return pid_;
}

#endif