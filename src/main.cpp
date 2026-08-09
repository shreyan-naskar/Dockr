/*
    src/main.cpp

    Purpose:
    --------
    Program entry point.

    This file should stay small.

    Design idea:
    ------------
    main() should not contain low-level container logic.

    It should only:
        1. Parse command-line arguments.
        2. Create a Container object.
        3. Run it.

    This keeps the actual runtime behavior inside proper classes.
*/

#ifdef __linux__

#include "include/config.hpp"
#include "include/container.hpp"

#include <iostream>
#include <sys/wait.h>  // WIFEXITED, WEXITSTATUS, WIFSIGNALED, WTERMSIG

int main(int argc, char* argv[])
{
    /*
        parse_config()
        --------------

        Converts argc/argv into a ContainerConfig object.

        Example input:

            sudo ./mini-container ./rootfs host-1 192.168.1.2 /bin/sh

        Result:
            rootfs_path = "./rootfs"
            hostname    = "host-1"
            ip_address  = "192.168.1.2"
            command     = {"/bin/sh"}
    */
    ContainerConfig config = parse_config(argc, argv);

    /*
        Container
        ---------

        Owns the high-level runtime lifecycle.

        Internally it manages:
            - namespace process
            - network
            - cgroup
            - filesystem setup
    */
    Container container(std::move(config));

    /*
        run()
        -----

        Starts the container and waits for it to finish.

        Return value is the raw waitpid() status.
    */
    int status = container.run();

    /*
        Decode child exit status.

        waitpid() does not return plain exit code.
        It returns encoded status bits.

        WIFEXITED(status):
            true if child exited normally.

        WEXITSTATUS(status):
            actual exit code.

        WIFSIGNALED(status):
            true if child was killed by signal.

        WTERMSIG(status):
            signal number.
    */
    if (WIFEXITED(status))
    {
        std::cout
            << "[main] container exited with code "
            << WEXITSTATUS(status)
            << "\n";

        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status))
    {
        std::cout
            << "[main] container killed by signal "
            << WTERMSIG(status)
            << "\n";

        return 128 + WTERMSIG(status);
    }

    /*
        Fallback for unusual status cases.
    */
    std::cout << "[main] container exited with unknown status\n";
    return 1;
}

#else

#error "mini-container supports Linux only."

#endif