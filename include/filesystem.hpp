#ifndef DOCKR_FILESYSTEM_HPP
#define DOCKR_FILESYSTEM_HPP

/*
    include/filesystem.hpp

    Purpose:
    --------
    Defines the Filesystem class.

    This class owns the filesystem-related operations needed by the container.

    Responsibilities:
    -----------------
    1. Prepare environment variables.
    2. Change root filesystem using chroot().
    3. Move working directory to "/".
    4. Mount /proc inside the container.
    5. Unmount /proc during cleanup.
    6. Bind-mount host shared folder into the rootfs.

    Design pattern:
    ---------------
    Low-level subsystem wrapper.

    Filesystem code is separated from:
        - process creation
        - networking
        - cgroups
        - command execution

    This keeps each class focused on one kernel area.
*/

#ifdef __linux__

#include <string>

/*
    Filesystem
    ----------

    Provides filesystem operations for the container runtime.

    Most functions are static because filesystem operations are actions,
    not long-lived objects.

    However, for mount lifecycle we use small objects later if needed.
*/
class Filesystem
{
public:
    /*
        setup_environment()
        -------------------

        Clears the inherited host environment and sets a minimal environment
        for the container shell.

        Why clear the host environment?
        -------------------------------
        The host may have many variables:

            AWS_SECRET_ACCESS_KEY
            SSH_AUTH_SOCK
            HOME
            USER
            PATH
            SHELL

        A container should not automatically inherit everything from the host.

        For a learning runtime, we keep only:
            TERM
            PATH
    */
    static void setup_environment();

    /*
        change_root()
        -------------

        Calls chroot(rootfs_path), then chdir("/").

        chroot():
            Changes what "/" means for the current process.

        chdir("/"):
            After chroot(), the process should move into the new root.

        Example:
            rootfs_path = "./rootfs"

        After change_root("./rootfs"):
            /bin/sh resolves to ./rootfs/bin/sh on the host filesystem.
    */
    static void change_root(const std::string& rootfs_path);

    /*
        mount_proc()
        ------------

        Mounts a proc filesystem at /proc inside the container rootfs.

        Why?
        ----
        Tools like ps, top, and many shell utilities expect /proc to exist.

        Because the process will be inside a new PID namespace, mounted /proc
        will show the container's process view instead of the host's full
        process tree.
    */
    static void mount_proc();

    /*
        unmount_proc()
        --------------

        Unmounts /proc from inside the container.

        This should be called before the container init process exits.
    */
    static void unmount_proc();

    /*
        bind_mount_shared_folder()
        --------------------------

        Bind-mounts a host directory into the rootfs before chroot.

        Example:
            host_shared_path = "./shared_folder"
            rootfs_path      = "./rootfs"
            container_path   = "/var/shared_folder"

        Host-side mount target becomes:
            ./rootfs/var/shared_folder

        Inside the container, it appears as:
            /var/shared_folder
    */
    static void bind_mount_shared_folder(
        const std::string& host_shared_path,
        const std::string& rootfs_path,
        const std::string& container_path
    );

    /*
        unmount_shared_folder()
        -----------------------

        Unmounts the shared folder bind mount after the container exits.
    */
    static void unmount_shared_folder(
        const std::string& rootfs_path,
        const std::string& container_path
    );

private:
    /*
        Private constructor
        -------------------

        Filesystem is a static utility class.
        No objects should be created.
    */
    Filesystem() = delete;
};

#else

#error "Filesystem container operations require Linux."

#endif

#endif