#ifndef DOCKR_NAMESPACE_PROCESS_HPP
#define DOCKR_NAMESPACE_PROCESS_HPP

/*
    include/namespace_process.hpp

    Purpose:
    --------
    Defines the NamespaceProcess class.

    This class will be responsible for one low-level job:

        create a child process using Linux clone()

    Why separate this into a class?
    -------------------------------
    clone() is one of the most dangerous and important parts of this project.

    It needs:
        - a function pointer for child execution
        - a manually allocated stack
        - namespace flags
        - an argument pointer passed into the child

    By wrapping clone() in a class, we keep process creation logic isolated
    from filesystem, cgroup, and network logic.

    Design pattern:
    ---------------
    Resource-owning wrapper around a low-level Linux primitive.

    NamespaceProcess owns:
        - the child PID
        - the CloneStack used by the child

    This fixes the common bug where stack memory is destroyed too early.
*/

#ifdef __linux__

#include "include/clone_stack.hpp"

#include <sched.h>      // clone(), namespace flags
#include <sys/types.h>  // pid_t

/*
    NamespaceProcess
    ----------------

    Creates and tracks one cloned process.

    It does not decide what the child does.
    It only starts the child function with the requested namespace flags.
*/
class NamespaceProcess
{
public:
    /*
        ChildFunction
        -------------

        clone() requires the child function to have this signature:

            int child_fn(void* arg);

        The child function receives one raw void* argument.

        Later, we will pass a pointer to a context object containing the
        container config and runtime state.
    */
    using ChildFunction = int (*)(void*);

    /*
        Constructor
        -----------

        Creates an empty NamespaceProcess object.

        No child process is started yet.

        This is useful because construction and process creation are separate:
            NamespaceProcess process;
            process.start(...);
    */
    NamespaceProcess();

    /*
        Destructor
        ----------

        This class owns stack memory, not the child process itself.

        The caller should explicitly wait for the child using wait().

        We do not kill the child in the destructor because automatic killing
        can be surprising in low-level process code.
    */
    ~NamespaceProcess();

    /*
        Copy disabled
        -------------

        A process wrapper should not be copied.

        If copied, two objects would claim to represent the same child PID
        and the same clone stack.
    */
    NamespaceProcess(const NamespaceProcess&) = delete;
    NamespaceProcess& operator=(const NamespaceProcess&) = delete;

    /*
        Move disabled
        -------------

        Moving could be implemented later.

        For learning clarity, we keep ownership stable.
    */
    NamespaceProcess(NamespaceProcess&&) = delete;
    NamespaceProcess& operator=(NamespaceProcess&&) = delete;

    /*
        start()
        -------

        Starts the child process with clone().

        Parameters:
            child_fn:
                Function executed inside the child.

            flags:
                clone flags.

                Example:
                    CLONE_NEWNS  - new mount namespace
                    CLONE_NEWNET - new network namespace
                    CLONE_NEWPID - new PID namespace
                    CLONE_NEWUTS - new hostname namespace
                    SIGCHLD      - child sends SIGCHLD when it exits

            arg:
                Raw pointer passed into child_fn.

        After this function succeeds:
            pid_ stores the child PID as seen by the parent.
    */
    void start(ChildFunction child_fn, int flags, void* arg);

    /*
        wait()
        ------

        Waits until the child process exits.

        This prevents zombie processes.

        Returns:
            child exit status from waitpid().
    */
    int wait();

    /*
        pid()
        -----

        Returns the child process ID.

        This PID is important because the parent needs it for networking:

            ip link set veth-child netns <pid>

        That moves one side of the veth pair into the child's network namespace.
    */
    pid_t pid() const;

private:
    /*
        stack_
        ------

        Stack memory used by clone().

        Because stack_ is a member of NamespaceProcess, it stays alive as long
        as the NamespaceProcess object stays alive.

        This fixes the stack lifetime bug from the original implementation.
    */
    CloneStack stack_;

    /*
        pid_
        ----

        PID returned by clone() in the parent.

        Before start(), pid_ is -1.
    */
    pid_t pid_;
};

#else

#error "NamespaceProcess requires Linux clone() and namespaces."

#endif

#endif