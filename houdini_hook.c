#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <cutils/log.h>

#define ABIARM_UIDLIST "/data/data/.appwithABI2"
#define ABIARM_CPUINFO "/system/lib/arm/cpuinfo"
#define ABIARMNEON_UIDLIST "/data/data/.appwithABI2neon"
#define ABIARMNEON_CPUINFO "/system/lib/arm/cpuinfo.neon"

static inline int check_uid(const int uid, const char *list_file, const char *cpuinfo_file) {
    int fdlist;
    fdlist = open(list_file, O_RDONLY);
    if (fdlist<0) {
        ALOGE("Unable to open file %s", list_file);
        return -1;
    }

    int id;
    while (read(fdlist, &id, sizeof(int))==sizeof(int)) {
        ALOGE("Read ID %d from file %s", id, list_file);
        if (id == uid) {
            ALOGE("Found UID %d in list file %s, will redirect to %s", uid, list_file, cpuinfo_file);
            close(fdlist);
            return open(cpuinfo_file, O_RDONLY);
        }
    }
    close(fdlist);
    return -1;
}

int houdini_hook_open(const char *path, int flags, int mode) {
    if (path && (strcmp(path, "/proc/cpuinfo")==0)) {
        int uid = getuid();
        ALOGE("UID %d asks for /proc/cpuinfo", uid);

        int try_arm = check_uid(uid, ABIARM_UIDLIST, ABIARM_CPUINFO);
        if (try_arm>=0)
            return try_arm;

        int try_armneon = check_uid(uid, ABIARMNEON_UIDLIST, ABIARMNEON_CPUINFO);
        if (try_armneon>=0)
            return try_armneon;
 
        ALOGE("UID %d is not in ARM UID list files", uid);
    }
    return open(path, flags, mode);
}
