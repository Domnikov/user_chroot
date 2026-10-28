#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>


static bool write_file(const char* path, const char* value)
{
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "open(%s): %s\n",
                path, strerror(errno));
        return false;
    }

    size_t len = strlen(value);
    ssize_t n = write(fd, value, len);

    if (n != static_cast<ssize_t>(len)) {
        int saved_errno = (n == -1) ? errno : EIO;

        fprintf(stderr, "write(%s): %s\n",
                path, strerror(saved_errno));

        close(fd);
        return false;
    }

    if (close(fd) == -1) {
        fprintf(stderr, "close(%s): %s\n",
                path, strerror(errno));
        return false;
    }

    return true;
}

static bool write_id_map(
    const char* map_name,
    uid_t inside_id,
    unsigned int outside_id)
{
    char path[128];
    char mapping[128];

    snprintf(path, sizeof(path),
             "/proc/self/%s", map_name);

    snprintf(mapping, sizeof(mapping),
             "%u %u 1\n",
             static_cast<unsigned int>(inside_id),
             outside_id);

    return write_file(path, mapping);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s <rootfs> [command [args...]]\n",
                argv[0]);
        return 1;
    }

    uid_t original_uid = getuid();
    gid_t original_gid = getgid();

    printf("Before unshare: uid=%u gid=%u\n",
           static_cast<unsigned int>(original_uid),
           static_cast<unsigned int>(original_gid));

    // Create a user namespace and a separate mount namespace.
    if (unshare(CLONE_NEWUSER | CLONE_NEWNS) == -1) {
        perror("unshare");
        return 1;
    }

    printf("User and mount namespaces created.\n");

    // Map namespace UID 0 to the original UID.
    if (!write_id_map("uid_map", 0, original_uid)) {
        fprintf(stderr, "Failed to configure UID mapping.\n");
        return 1;
    }

    // An unprivileged process normally must disable setgroups
    // before configuring gid_map.
    if (!write_file("/proc/self/setgroups", "deny\n")) {
        fprintf(stderr, "Failed to configure setgroups.\n");
        return 1;
    }

    // Map namespace GID 0 to the original GID.
    if (!write_id_map("gid_map", 0, original_gid)) {
        fprintf(stderr, "Failed to configure GID mapping.\n");
        return 1;
    }

    printf("UID/GID mappings configured.\n");

    // Make mount propagation private so mount operations do not
    // propagate to the parent mount namespace.
    if (mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) == -1) {
        perror("mount(MS_PRIVATE)");
        return 1;
    }

    // Validate root filesystem.
    struct stat st;

    if (stat(argv[1], &st) == -1) {
        perror("stat(rootfs)");
        return 1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: rootfs is not a directory.\n");
        return 1;
    }

    // Resolve rootfs to an absolute path before changing root.
    char rootfs[PATH_MAX];

    if (realpath(argv[1], rootfs) == nullptr) {
        perror("realpath(rootfs)");
        return 1;
    }

    // Change the process root.
    if (chroot(rootfs) == -1) {
        fprintf(stderr, "chroot(%s): %s\n",
                rootfs, strerror(errno));
        return 1;
    }

    // Move the working directory into the new root.
    if (chdir("/") == -1) {
        perror("chdir(/)");
        return 1;
    }

    printf("Entered root filesystem.\n");

    // We are namespace root (UID/GID 0), mapped to the original
    // unprivileged UID/GID outside the namespace.
    //
    // Do not switch to original_uid/original_gid here: those
    // numbers are not necessarily mapped inside this namespace.

    // Construct the command argument vector.
    char* default_argv[] = {
        const_cast<char*>("bash"),
        nullptr
    };

    char** command_argv;

    if (argc >= 3) {
        command_argv = &argv[2];

        // Use the executable's basename as argv[0].
        char* name = basename(argv[2]);

        // Preserve the original argument vector while replacing
        // only the first argument.
        command_argv[0] = name;
    } else {
        command_argv = default_argv;
    }

    const char* command =
        (argc >= 3) ? argv[2] : "/bin/bash";

    execvp(command, command_argv);

    fprintf(stderr, "execvp(%s): %s\n",
            command, strerror(errno));

    return 1;
}
