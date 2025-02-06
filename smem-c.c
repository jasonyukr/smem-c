#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <limits.h>
#include <pwd.h>
#include <unistd.h>

#define MAX_PID_ITEMS (4096)
#define MAX_CMDLINE   (28)
#define MAX_BUFFER    (1024)

typedef struct {
    int size;
    int rss;
    int pss;
    int shared_clean;
    int shared_dirty;
    int private_clean;
    int count;
    int private_dirty;
    int referenced;
    int swap;
} Stat;

int is_digit(const char *str) {
    while (*str) {
        if (!isdigit((unsigned char) *str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

int piduid(int pid) {
    struct stat statbuf;
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d", pid);

    if (stat(path, &statbuf) != 0) {
        return -1;
    }
    return statbuf.st_uid;
}

const char* username(int uid) {
    struct passwd *pwd = getpwuid(uid);
    if (pwd) {
        return pwd->pw_name;
    } else {
        return "";
    }
}

char *pidcmd(int pid) {
    FILE *file;
    char path[PATH_MAX];
    char *cmdline = NULL;
    static char buff[MAX_CMDLINE];

    int c;
    int idx = 0;
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    file = fopen(path, "r");
    if (file) {
        while ((c = fgetc(file)) != EOF && idx < (MAX_CMDLINE - 1)) {
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
    FILE *file;
    char path[PATH_MAX];
    char *cmdline = NULL;

    int c;
    int idx = 0;
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    file = fopen(path, "r");
    if (file) {
        if (fgetc(file) != EOF) {
            fclose(file);
            return 0;
        } else {
            fclose(file);
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
        if (entry->d_type == DT_DIR && is_digit(entry->d_name)) {
            int pid = atoi(entry->d_name);
            if (!is_kernel(pid)) {
                pidlist[count++] = pid;
            }
        }
    }

    closedir(dp);
    return count;
}

int parse_line(char *line, char *key) {
    int key_len = strlen(key);
    if (strncmp(line, key, key_len) == 0) {
        // Find the position of " kB" and replace it with '\0'
        char *kb_pos = strstr(line, " kB");
        if (kb_pos != NULL) {
            *kb_pos = '\0';
        }
        // Parse the size value
        return atoi(line + key_len);
    } else {
        return -1;
    }
}

Stat *parse_smaps_file(int pid) {
    static Stat stat;
    stat.size = 0;
    stat.rss = 0;
    stat.pss = 0;
    stat.shared_clean = 0;
    stat.shared_dirty = 0;
    stat.private_clean = 0;
    stat.count = 0;
    stat.private_dirty = 0;
    stat.referenced = 0;
    stat.swap = 0;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/smaps", pid);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }

    char line[MAX_BUFFER];
    while (fgets(line, sizeof(line), file)) {
        int size = parse_line(line, "Size:");
        if (size != -1) {
            stat.size += size;
            continue;
        }
        size = parse_line(line, "Rss:");
        if (size != -1) {
            stat.rss += size;
            continue;
        }
        size = parse_line(line, "Pss:");
        if (size != -1) {
            stat.pss += size;
            continue;
        }
        size = parse_line(line, "Shared_Clean:");
        if (size != -1) {
            stat.shared_clean += size;
            continue;
        }
        size = parse_line(line, "Shared_Dirty:");
        if (size != -1) {
            stat.shared_dirty += size;
            continue;
        }
        size = parse_line(line, "Private_Clean:");
        if (size != -1) {
            stat.private_clean += size;
            continue;
        }
        size = parse_line(line, "Count:");
        if (size != -1) {
            stat.count += size;
            continue;
        }
        size = parse_line(line, "Private_Dirty:");
        if (size != -1) {
            stat.private_dirty += size;
            continue;
        }
        size = parse_line(line, "Referenced:");
        if (size != -1) {
            stat.referenced += size;
            continue;
        }
        size = parse_line(line, "Swap:");
        if (size != -1) {
            stat.swap += size;
            continue;
        }
    }
    fclose(file);
    return &stat;
}

int last_uid;
const char *last_username;

void show_stat(int pid) {
    Stat *stat = parse_smaps_file(pid);
    if (stat) {
        const char *uname = "";
        int uid = piduid(pid);
        if (uid != -1) {
            if (last_uid == uid) {
                uname = last_username;
            } else {
                uname = username(uid);
                last_uid = uid;
                last_username = uname;
            }
        }
        printf("%5d %-8s %-27s %8d %8d %8d %8d \n",
                pid, uname, pidcmd(pid), stat->swap, stat->private_dirty + stat->private_clean, stat->pss, stat->rss);
    }
}

int main(int argc, char **argv) {
    last_uid = -1;
    last_username = "";

    int *pidlist = (int *) malloc(sizeof(int) * MAX_PID_ITEMS);
    if (pidlist == NULL) {
        return 1;
    }
    int count = pids(pidlist);
    if (count > 0) {
        printf("  PID User     Command                         Swap      USS      PSS      RSS \n");
        for (int i = 0; i < count; i++) {
            show_stat(pidlist[i]);
        }
    }
    free(pidlist);
    return 0;
}
