#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

static char source_path[PATH_MAX];   
#define VIRTUAL_FILE "/tujuan.txt"

static void build_source_path(char *buf, const char *fuse_path)
{
    snprintf(buf, PATH_MAX, "%s%s", source_path, fuse_path);
}

static char *build_tujuan_content(void)
{
    char combined[4096] = "";
    int  first = 1;

    for (int i = 1; i <= 7; i++) {
        char filepath[PATH_MAX];     // ← kembalikan ke PATH_MAX biasa
        snprintf(filepath, PATH_MAX, "%s/%d.txt", source_path, i);

        FILE *f = fopen(filepath, "r");
        if (!f) continue;

        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            char *koord = strstr(line, "KOORD:");
            if (koord) {
                char *fragment = koord + strlen("KOORD:");

                size_t len = strlen(fragment);
                while (len > 0 &&
                       (fragment[len-1] == '\n' ||
                        fragment[len-1] == '\r' ||
                        fragment[len-1] == ' ')) {
                    fragment[--len] = '\0';
                }
                while (*fragment == ' ' || *fragment == '\t')
                    fragment++;

                if (!first) strncat(combined, "", sizeof(combined) - strlen(combined) - 1);
                strncat(combined, fragment, sizeof(combined) - strlen(combined) - 1);
                first = 0;
            }
        }
        fclose(f);
    }
    char *result = (char *)malloc(strlen("Tujuan Mas Amba: ") + strlen(combined) + 2);
    if (!result) return NULL;
    sprintf(result, "Tujuan Mas Amba: %s\n", combined);
    return result;
}



static int kr_getattr(const char *path, struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, VIRTUAL_FILE) == 0) {
        char *content = build_tujuan_content();
        size_t sz = content ? strlen(content) : 0;
        free(content);

        stbuf->st_mode  = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size  = (off_t)sz;

        return 0;
    }

    char real[PATH_MAX];
    build_source_path(real, path);

    int res = lstat(real, stbuf);
    if (res == -1) return -errno;
    return 0;
}

static int kr_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    char real[PATH_MAX];
    build_source_path(real, path);

    DIR *dp = opendir(real);
    if (!dp) return -errno;

    filler(buf, ".",  NULL, 0);
    filler(buf, "..", NULL, 0);

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        filler(buf, de->d_name, NULL, 0);
    }
    closedir(dp);

    if (strcmp(path, "/") == 0) {
        filler(buf, "tujuan.txt", NULL, 0);
    }

    return 0;
}

static int kr_open(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path, VIRTUAL_FILE) == 0) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY)
            return -EACCES;
        return 0;
    }

    char real[PATH_MAX];
    build_source_path(real, path);

    int fd = open(real, fi->flags);
    if (fd == -1) return -errno;

    fi->fh = (uint64_t)fd;
    return 0;
}

static int kr_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi)
{
    if (strcmp(path, VIRTUAL_FILE) == 0) {
        char *content = build_tujuan_content();
        if (!content) return -ENOMEM;

        size_t len = strlen(content);
        int res = 0;

        if ((size_t)offset < len) {
            size_t available = len - (size_t)offset;
            size_t to_copy   = size < available ? size : available;
            memcpy(buf, content + offset, to_copy);
            res = (int)to_copy;
        }
        free(content);
        return res;
    }

    int fd = (int)fi->fh;
    int res = (int)pread(fd, buf, size, offset);
    if (res == -1) return -errno;
    return res;
}

static int kr_release(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path, VIRTUAL_FILE) == 0)
        return 0;

    close((int)fi->fh);
    return 0;
}

static struct fuse_operations kr_ops = {
    .getattr = kr_getattr,
    .readdir = kr_readdir,
    .open    = kr_open,
    .read    = kr_read,
    .release = kr_release,
};

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr,
            "Usage: %s <source_directory> <mount_directory> [fuse_options]\n",
            argv[0]);
        return 1;
    }

    if (realpath(argv[1], source_path) == NULL) {
        perror("realpath source_directory");
        return 1;
    }

    int new_argc = argc - 1;
    char **new_argv = malloc((size_t)(new_argc + 1) * sizeof(char *));
    if (!new_argv) { perror("malloc"); return 1; }

    new_argv[0] = argv[0];
    for (int i = 1; i < new_argc; i++)
        new_argv[i] = argv[i + 1];
    new_argv[new_argc] = NULL;

    return fuse_main(new_argc, new_argv, &kr_ops, NULL);
}

