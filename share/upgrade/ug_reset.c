#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <malloc.h>
#include <string.h>
#include "ite/ug.h"
#include "ug_cfg.h"

static unsigned long GetFileSize(const char *input)
{
    struct stat st;

    st.st_size = 0;
    stat(input, &st);

    return (unsigned long)st.st_size;
}

int ugCopyFile(const char* destPath, const char* srcPath)
{
    int ret = 0;
    FILE *rf = NULL;
    FILE *wf = NULL;
    uint8_t* buf = NULL;
    unsigned long filesize = GetFileSize(srcPath);

    LOG_INFO "Copy %s (%ld bytes)\n", srcPath, filesize LOG_END

    if ((rf = fopen(srcPath, "rb")) == NULL)
    {
        ret = errno;
        LOG_ERR "cannot open file %s: %d\n", srcPath, errno LOG_END
        goto end;
    }

    buf = malloc(filesize);
    if (buf == NULL)
    {
        ret = __LINE__;
        LOG_ERR "out of memory (%d)\n", filesize LOG_END
        goto end;
    }

    if (fread(buf, 1, filesize, rf) != filesize)
    {
        ret = errno;
        LOG_ERR "cannot read file %s: %d\n", srcPath, errno LOG_END
        goto end;
    }

    if ((wf = fopen(destPath, "wb")) == NULL)
    {
        ret = errno;
        LOG_ERR "cannot open file %s: %d\n", destPath, errno LOG_END
        goto end;
    }

    if (fwrite(buf, 1, filesize, wf) != filesize)
    {
        ret = errno;
        LOG_ERR "cannot write file %s: %d\n", destPath, errno LOG_END
        goto end;
    }

end:
    if (rf)
        fclose(rf);

    if (wf)
        fclose(wf);

    if (buf)
        free(buf);

    return ret;
}

int ugRestoreDir(const char* dest, const char* src)
{
    DIR           *dir;
    struct dirent *ent;
    int ret = 0;

    dir = opendir(src);
    if (dir == NULL)
    {
        LOG_ERR "cannot open directory %s\n", src LOG_END
        ret = __LINE__;
        goto end;
    }

    while ((ent = readdir(dir)) != NULL)
    {
        char destPath[PATH_MAX];
        int ret1;

        if (strcmp(ent->d_name, ".") == 0)
            continue;

        if (strcmp(ent->d_name, "..") == 0)
            continue;

        strcpy(destPath, dest);
        strcat(destPath, "/");

        if (ent->d_type == DT_DIR)
        {
            if (ent->d_name[0] == '~')
            {
                strcat(destPath, &ent->d_name[1]);
                LOG_INFO "Remove dir %s\n", destPath LOG_END
                ugDeleteDirectory(destPath);
            }
            else
            {
                char srcPath[PATH_MAX];

                strcat(destPath, ent->d_name);
                
                ret1 = mkdir(destPath, S_IRWXU);
                if (ret1)
                {
                    LOG_WARN "cannot create dir %s: %d\n", destPath, errno LOG_END

                    if (ret == 0)
                        ret = ret1;
                }
                else
                {
                    strcpy(srcPath, src);
                    strcat(srcPath, "/");
                    strcat(srcPath, ent->d_name);

                    ret1 = ugRestoreDir(destPath, srcPath);
                    if (ret1)
                    {
                        if (ret == 0)
                            ret = ret1;
                    }
                }
            }
        }
        else
        {
            if (ent->d_name[0] == '~')
            {
                strcat(destPath, &ent->d_name[1]);

                LOG_INFO "Remove file %s\n", destPath LOG_END
                ret1 = remove(destPath);
                if (ret1)
                {
                    LOG_WARN "cannot remove file %s: %d\n", destPath, errno LOG_END
                }
            }
            else
            {
                char srcPath[PATH_MAX];

                strcat(destPath, ent->d_name);
                strcpy(srcPath, src);
                strcat(srcPath, "/");
                strcat(srcPath, ent->d_name);

                ret1 = ugCopyFile(destPath, srcPath);
                if (ret1)
                {
                    if (ret == 0)
                        ret = ret1;
                }
            }
        }
    }

end:
    if (dir)
    {
        if (closedir(dir))
            LOG_WARN "cannot closedir (%s)\n", src LOG_END
    }
    return ret;
}

int ugResetFactory(void)
{
    DIR           *dir;
    struct dirent *ent;
    int ret = 0;

    dir = opendir(CFG_PRIVATE_DRIVE ":/backup");
    if (dir == NULL)
    {
        LOG_ERR "cannot open directory %s\n", CFG_PRIVATE_DRIVE ":/backup" LOG_END
        ret = __LINE__;
        goto end;
    }

    while ((ent = readdir(dir)) != NULL)
    {
        if (strcmp(ent->d_name, ".") == 0)
            continue;

        if (strcmp(ent->d_name, "..") == 0)
            continue;

        if (ent->d_type == DT_DIR)
        {
            char destPath[PATH_MAX];
            char srcPath[PATH_MAX];
            int ret1;

            strcpy(destPath, ent->d_name);
            strcat(destPath, ":");
            strcpy(srcPath, CFG_PRIVATE_DRIVE ":/backup/");
            strcat(srcPath, ent->d_name);

            ret1 = ugRestoreDir(destPath, srcPath);
            if (ret1)
            {
                if (ret == 0)
                    ret = ret1;
            }
        }
    }

end:
    if (dir)
    {
        if (closedir(dir))
            LOG_WARN "cannot closedir (%s)\n", CFG_PRIVATE_DRIVE ":/backup" LOG_END
    }
    return ret;
}
