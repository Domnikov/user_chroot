// references:
// [1]: http://man7.org/linux/man-pages/man7/user_namespaces.7.html
// [2]: http://man7.org/linux/man-pages/man2/unshare.2.html

#include <sched.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("Usage: %s <rootfs> <command>\n", argv[0]);
    }

    int uid = getuid();
    int gid = getgid();
    printf("Before unshare, uid=%d, gid=%d\n", uid, gid);

    // First, unshare the user namespace and assume admin capability in the
    // new namespace
    int err = unshare(CLONE_NEWUSER);
    if(err) {
        printf("Failed to unshare user namespace\n");
        return 1;
    }

    // write a uid/gid map
    char file_path_buf[100];
    int pid = getpid();
    printf("My pid: %d\n", pid);

    sprintf(file_path_buf, "/proc/%d/uid_map", pid);
    int fd = open(file_path_buf, O_WRONLY);
    if(fd == -1) {
        printf("Failed to open %s for write [%d] %s\n", file_path_buf, errno, 
               strerror(errno));
    } else {
        printf("Writing : %s (fd=%d)\n", file_path_buf, fd);
        err = dprintf(fd, "%d %d 1\n", uid, uid);
        if(err == -1) {
            printf("Failed to write contents [%d]: %s\n", errno, 
                   strerror(errno));
        }
        close(fd);
    }

    sprintf(file_path_buf, "/proc/%d/setgroups", pid);
    fd = open(file_path_buf, O_WRONLY);
    if(fd == -1) {
        printf("Failed to open %s for write [%d] %s\n", file_path_buf, errno, 
               strerror(errno));
    } else {
        dprintf(fd, "deny\n");
        close(fd);
    }

    sprintf(file_path_buf, "/proc/%d/gid_map", pid);
    fd = open(file_path_buf, O_WRONLY);
    if(fd == -1) {
        printf("Failed to open %s for write [%d] %s\n", file_path_buf, errno, 
               strerror(errno));
    } else {
        printf("Writing : %s (fd=%d)\n", file_path_buf, fd);
        err = dprintf(fd, "%d %d 1\n", gid, gid);
        if(err == -1) {
            printf("Failed to write contents [%d]: %s\n", errno, 
                   strerror(errno));
        }
        close(fd);
    }

    // Now chroot into the desired directory
    err = chroot(argv[1]);
    if(err) {
        printf("Failed to chroot\n");
        return 1;
    }

    // Now drop admin in our namespace
    err = setresuid(uid, uid, uid);
    if(err) {
        printf("Failed to set uid\n");
    }

    err = setresgid(gid, gid, gid);
    if(err) {
        printf("Failed to set gid\n");
    }

    // and start a shell

    char* cmd = NULL;
	char** new_argv = NULL;
	int args_count = 0;

	if(argc >= 3)
	{
		cmd = argv[2];
		args_count = argc - 3 + 2;
		new_argv = new char*[args_count];
		new_argv[0] = basename(cmd);
		for(int i = 0; i < args_count-1; i++)
		{
			new_argv[i+1] = ( (3+i) < argc) ? argv[3+i] : NULL;
		}
	}
	else
	{
		args_count = 2;
		cmd = new char[10];
		snprintf(cmd, 10, "/bin/bash");
		new_argv = new char*[2];
		new_argv[0] = new char[5];
		snprintf(new_argv[0], 5, "bash");
	}
	
    err = execvp(cmd, new_argv);
    if(err) {
        perror("Failed to start shell");
        return -1;
    }

	
	if(argc < 3)
	{
		delete [] cmd;
		for(int i = 0; i < args_count;i++) delete [] new_argv[i];
	}
	delete [] new_argv;

}
