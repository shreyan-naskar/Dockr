/*
    demo/hungry.cpp

    Purpose:
    --------
    Memory pressure demo for cgroup limits.

    Where this runs:
    ----------------
    This program runs INSIDE the container.

    Build target:
        shared_folder/hungry

    Example:

        sudo ./build/mini-container ./rootfs host-1 192.168.1.2 /bin/sh

    Inside container:

        cd /var/shared_folder
        ./hungry

    Expected behavior:
    ------------------
    If the container cgroup memory limit is 10 MB, this program should be
    killed by the kernel once it exceeds the limit.

    You may see:

        Killed

    That proves memory.max is being enforced.
*/

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

/*
    main()
    ------

    This program repeatedly allocates memory and stores the pointers.

    Important:
    ----------
    We intentionally do not free the memory.

    This is normally bad practice, but here it is the point of the demo:
    we want memory usage to keep increasing until the cgroup limit stops it.
*/
int main()
{
    /*
        Allocate 1 MiB per loop iteration.

        1 MiB = 1024 * 1024 bytes.
    */
    const std::size_t block_size = 1024 * 1024;

    /*
        Store pointers so the allocated memory remains reachable.

        If we did not store them, a smarter version of code could be changed
        later and accidentally free/reuse memory. Keeping pointers makes the
        memory growth obvious.
    */
    std::vector<char*> blocks;

    std::size_t total_allocated = 0;

    std::cout << "Starting memory allocation demo...\n";

    while (true)
    {
        /*
            new char[block_size]
            --------------------
            Requests another 1 MiB block from the heap.

            Under cgroup memory pressure, the kernel may eventually kill this
            process instead of allowing allocation to continue.
        */
        char* block = new char[block_size];

        /*
            Touch the memory.

            Why?
            ----
            Some systems overcommit memory. That means allocation may appear
            to succeed before physical memory is actually committed.

            memset forces the process to really use the pages.
        */
        std::memset(block, 0, block_size);

        blocks.push_back(block);
        total_allocated += block_size;

        std::cout
            << "Allocated "
            << (total_allocated / (1024 * 1024))
            << " MiB\n";

        /*
            Slow down slightly so the output is readable.
        */
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    return 0;
}