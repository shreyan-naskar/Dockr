#include "include/filesystem.hpp"

/*
    src/filesystem.cpp

    Purpose:
    --------
    Implements filesystem operations for the container runtime.

    This file directly uses Linux filesystem syscalls:

        chroot()
        chdir()
        mount()
        umount2()

    These operations normally require root privileges or CAP_SYS_ADMIN.
*/

#ifdef __linux__

#include "include/error.hpp"

#include <cstdlib>      // clearenv(), setenv()
#include <string>
#include <sys/mount.h>  // mount(), umount2()
#include <sys/stat.h>   // mkdir()
#include <unistd.h>     // chroot(), chdir()

/*
    setup_environment()
    -------------------

    Clears the host environment and creates a tiny container environment.

    clearenv()
        Removes all inherited environment variables.

    setenv()
        Adds variables back.

    TERM:
        Allows terminal programs inside the container shell to know the
        terminal type.

    PATH:
        Lets the shell find commands like ls, cat, sh, ps.

    Note:
        PATH must include /usr/bin and /usr/sbin with leading slashes.
*/
void Filesystem::setup_environment()
{
    if (clearenv() != 0)
    {
        Error::die("clearenv");
    }

    if (setenv("TERM", "xterm-256color", 1) != 0)
    {
        Error::die("setenv TERM");
    }

    if (setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin", 1) != 0)
    {
        Error::die("setenv PATH");
    }
}

/*
    change_root()
    -------------

    Changes the process root directory.

    chroot(rootfs_path.c_str())
        Makes rootfs_path become "/" for this process and its children.

    chdir("/")
        Moves the current working directory into the new root.

    Why chdir("/") after chroot?
    ----------------------------
    A process can technically keep its old working directory after chroot.
    That can be confusing and sometimes unsafe.

    So after changing root, we explicitly move to the new "/".
*/
void Filesystem::change_root(const std::string& rootfs_path)
{
    if (chroot(rootfs_path.c_str()) == -1)
    {
        Error::die("chroot");
    }

    if (chdir("/") == -1)
    {
        Error::die("chdir /");
    }
}

/*
    mount_proc()
    ------------

    Mounts procfs at /proc.

    Parameters to mount():
    ----------------------

    source:
        "proc"

    target:
        "/proc"

    filesystem type:
        "proc"

    flags:
        0 for normal proc mount

    data:
        nullptr because proc does not need extra data here
*/
void Filesystem::mount_proc()
{
    if (mount("proc", "/proc", "proc", 0, nullptr) == -1)
    {
        Error::die("mount /proc");
    }
}

/*
    unmount_proc()
    --------------

    Unmounts /proc.

    We use umount2() instead of umount() because umount2 allows flags.

    MNT_DETACH:
        Lazy unmount.

        It detaches the mount immediately from the filesystem tree and cleans
        it up once it is no longer busy.

    This is useful for container cleanup because /proc can sometimes be busy.
*/
void Filesystem::unmount_proc()
{
    if (umount2("/proc", MNT_DETACH) == -1)
    {
        Error::warn("umount /proc");
    }
}

/*
    bind_mount_shared_folder()
    --------------------------

    Bind-mounts a host folder into the container rootfs.

    Example:
        host_shared_path = "./shared_folder"
        rootfs_path      = "./rootfs"
        container_path   = "/var/shared_folder"

    Full target path:
        ./rootfs + /var/shared_folder
        ./rootfs/var/shared_folder

    mount() with MS_BIND:
        Makes an existing directory appear at another location.
*/
void Filesystem::bind_mount_shared_folder(
    const std::string& host_shared_path,
    const std::string& rootfs_path,
    const std::string& container_path
)
{
    std::string target = rootfs_path + container_path;

    /*
        Ensure the mount target directory exists.

        mkdir() may fail with EEXIST if the directory already exists.
        For simplicity in this step, we let mount() report the real issue
        if the path is bad.

        Later we can add recursive directory creation.
    */
    mkdir(target.c_str(), 0755);

    if (mount(
            host_shared_path.c_str(),
            target.c_str(),
            nullptr,
            MS_BIND | MS_RDONLY,
            nullptr
        ) == -1)
    {
        Error::die("bind mount shared folder");
    }
}

/*
    unmount_shared_folder()
    -----------------------

    Unmounts the bind-mounted shared folder from the host side.

    This runs after the container child exits.
*/
void Filesystem::unmount_shared_folder(
    const std::string& rootfs_path,
    const std::string& container_path
)
{
    std::string target = rootfs_path + container_path;

    if (umount2(target.c_str(), MNT_DETACH) == -1)
    {
        Error::warn("umount shared folder");
    }
}

#endif