#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <limits.h>

#define MAX_PID_ITEMS (4096)
#define MAX_BUFFER    (4096)

int is_all_digit(const char *str) {
    while (*str) {
        if (!isdigit((unsigned char) *str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

char *pidcmd(int pid) {
    FILE *file;
    char path[256];
    char *cmdline = NULL;
    size_t size = 0;
    static char buff[MAX_BUFFER];

    int c;
    int idx = 0;
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    file = fopen(path, "r");
    if (file) {
        while ((c = fgetc(file)) != EOF && idx < (MAX_BUFFER - 1)) {
            if (c == 0x00)
                c = ' ';
            buff[idx++] = c;
        }
        fclose(file);
        buff[idx] = 0;
        return buff;
    } else {
        return NULL;
    }
}

int is_kernel(int pid) {
    char *cmdline = pidcmd(pid);
    if (cmdline) {
        if (strlen(cmdline) > 0) {
            return 0;
        } else {
            return 1;
        }
    } else {
        // treat "/proc/PID/cmdline" file-not-found case as kernel mode
        return 1;
    }
}

// search for /proc/PID/ directory where PID part is all digits
int pids(int *pidlist) {
    struct dirent *entry;
    DIR *dp = opendir("/proc");
    if (dp == NULL) {
        return 0;
    }

    int count = 0;
    while ((entry = readdir(dp)) != NULL && count < MAX_PID_ITEMS) {
        if (entry->d_type == DT_DIR && is_all_digit(entry->d_name)) {
            int pid = atoi(entry->d_name);
            if (!is_kernel(pid)) {
                pidlist[count++] = pid;
            }
        }
    }

    closedir(dp);
    return count;
}

int main(int argc, char **argv) {
    int *pidlist = (int *) malloc(sizeof(int) * MAX_PID_ITEMS);
    if (pidlist == NULL) {
        return 1;
    }
    int count = pids(pidlist);
    for (int i = 0; i < count; i++) {
        printf("[%d] %d\n", i, pidlist[i]);
    }
    free(pidlist);
    return 0;
}
