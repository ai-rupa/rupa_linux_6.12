#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <asm/types.h>

#include "rkfsmk.h"

static char *
seekstr(const char *str, int slen,const char *token)
{
    int len = strlen(token);
    const char *p = str + slen;

    while (str <= --p)
        if (p[0] == token[0] && strncmp(p, token, len) == 0)
            return (char *)p;
    return NULL;
}

static int mp4_get_moov_offset(char *buff, int buff_len, int *find)
{
    char *moov = seekstr(buff, buff_len, "moov");
    int ret = -5;
    *find = 0;

    if (moov) {
        int offset = moov - buff;
        int len = buff_len - offset;
        ret = offset - 4;
        *find = 1;
    }

    return ret;
}

static int mp4_get_moov_offset_repa(char *buff, int buff_len, __u64 repa_id, int *find)
{
    char *moov = seekstr(buff, buff_len, "moov");
    int ret = -5;
    *find = 0;

    if (moov) {
        int offset = moov - buff;
        int len = buff_len - offset;
        char *repa = seekstr(moov, len, "repa");
        ret = offset - 4;
        if (len >= 20 && repa) {
            int i;
            __u64 tmp_id = 0;
            for (i = 4; i < 12; i++) {
                tmp_id = tmp_id << 8;
                tmp_id = tmp_id | repa[i];
            }
            //printf("%s %016llx, %016llx\n", __func__, tmp_id, repa_id);
            if (tmp_id == repa_id)
                *find = 1;
            else 
                *find = -1;
        }
    }

    return ret;
}

static int mp4_get_mdat_offset(char *buff, int buff_len)
{
    char *mdat = seekstr(buff, buff_len, "mdat");
    if (mdat) {
        int offset = mdat - buff;

        return offset - 4;
    }
    printf("no mdat\n");

    return -1;
}

static int mp4_get_len(char *buff)
{
    return buff[0] << 24 | buff[1] << 16 | buff[2] << 8 | buff[3];
}

static int mp4_set_mdat_len(int fd, off_t offset, unsigned int len)
{
    char buff[4];

    buff[3] = len & 0xff;
    buff[2] = (len >> 8) & 0xff;
    buff[1] = (len >> 16) & 0xff;
    buff[0] = (len >> 24) & 0xff;
    lseek(fd, offset, SEEK_SET);
    if (write(fd, buff, 4) == 4) {
        return 0;
    }

    return -1;
}

static int mp4_get_repa(char *buff, int buff_len, __u64 *id, __u64 *pre)
{
    int i;
    int j;
    for (i = 0; i < buff_len - 20; i++) {
        if ((buff[i + 4] == 'r') && (buff[i + 5] == 'e') && (buff[i + 6] == 'p') && (buff[i + 7] == 'a')) {
            *id = 0;
            *pre = 0;
            for (j = i + 8; j < i + 16; j++) {
                *id = *id << 8;
                *id = *id | buff[j];
            }
            for (j = i + 16; j < i + 24; j++) {
                *pre = *pre << 8;
                *pre = *pre | buff[j];
            }
            return 0;
        }
    }

    return -1;
}

static int mp4_set_end(int fd, off_t offset)
{
    char buff[8];
    int moov_len;
    off_t end_offset;
    long long seek_offset;

    lseek(fd, offset, SEEK_SET);
    read(fd, buff, 4);

    moov_len = buff[0] << 24 | buff[1] << 16 | buff[2] << 8 | buff[3];
    end_offset = offset + moov_len;
    memset(buff, 0, 8);
    seek_offset = lseek(fd, end_offset, SEEK_SET);
    if ((seek_offset == end_offset) && (write(fd, buff, 8) == 8)) {
        return 0;
    }

    return -1;
}

static int mp4_moov_check(int fd, off_t offset)
{
    char buff[8];
    int moov_len;

    memset(buff, 0, 8);
    lseek(fd, offset, SEEK_SET);
    read(fd, buff, 8);
    moov_len = buff[0] << 24 | buff[1] << 16 | buff[2] << 8 | buff[3];

    if (moov_len && (buff[4] == 'm') && (buff[5] == 'o') && (buff[6] == 'o') && (buff[7] == 'v')) {
        unsigned long file_size = lseek(fd, 0, SEEK_END);
        if ((offset + moov_len) <= file_size)
            return 0;
    }

    printf("%s mdat len err\n", __func__);
    return -1;
}

int repair_mp4(char *file)
{
    int ret = REPA_FAIL;
    int buff_size = 256 * 1024;
    int fd = 0;
    char *buff = NULL;
    int need_remove = 0;
    off_t cnt = 0; 
    //printf("%s file = %s\n", __func__, file);
    printf("%s 20220616 %s\n", __func__, file);
    if (file) {
        fd = open(file, O_RDWR);
        if (fd) {
            off_t mdat_offset = 0;
            off_t moov_offset = 0;
            off_t file_offset = 0;
            off_t moov_offset_last = 0;
            __u64 repa_id = 0;
            __u64 repa_pre = 0;
            unsigned int mdat_len = 0;
            off_t file_size = lseek(fd, 0, SEEK_END);
            if (file_size <= 0) {
                printf("Does not support repair\n");
                ret = REPA_NOSP;
                need_remove = 1;
                goto out;
            }
            moov_offset_last = file_size;
            if (buff_size > file_size)
                buff_size = file_size;

            buff = malloc(buff_size);
            memset(buff, 0, buff_size);
            lseek(fd, 0, SEEK_SET);
            read(fd, buff, buff_size);
            if (mp4_get_repa(buff, buff_size, &repa_id, &repa_pre) != 0) {
                printf("Does not support repair\n");
                ret = REPA_NOSP;
                goto out;
            }
            //printf("repa info %016llx, %016llx\n", repa_id, repa_pre);
            mdat_offset = mp4_get_mdat_offset(buff, buff_size);
            if (mdat_offset < 0)
                printf("no mdat info\n");
            else
                mdat_len = mp4_get_len(&buff[mdat_offset]);
            if (mdat_len) {
                if (mp4_moov_check(fd, mdat_offset + mdat_len) == -1)
                    mdat_len = 0;
            }
            if (mdat_len == 0) {
                int again = 0;
                int find = 0;
                file_offset = file_size - buff_size;
                while (1) {
                    if (cnt > file_size) {
                        ret = REPA_FAIL;
                        printf("Repaired fail\n");
                        break;
                    }
                    cnt += 64;
                    if (file_offset < 0)
                        file_offset = 0;
                    //printf("%lld, %d\n", moov_offset_last - file_offset, buff_size);
                    if (buff_size > (moov_offset_last - file_offset))
                        buff_size = moov_offset_last - file_offset;
                    if (buff_size <= 16) {
                        ret = REPA_FAIL;
                        break;
                    }
                    lseek(fd, file_offset, SEEK_SET);
                    memset(buff, 0, buff_size);
                    read(fd, buff, buff_size);
                    //printf("file_offset = 0x%llx, buff_size = %d, %lld, %lld\n", file_offset, buff_size, cnt, file_size);
                    moov_offset = mp4_get_moov_offset(buff, buff_size, &find);
                    if (find == 1) {
                        int moov_len;
                        char *moov_buff;
                        if (moov_offset < 0)
                            file_offset = file_offset - 4;
                        else
                            file_offset = file_offset + moov_offset;
                        moov_offset_last = file_offset;
                        lseek(fd, file_offset, SEEK_SET);
                        memset(buff, 0, buff_size);
                        read(fd, buff, buff_size);
                        moov_len = mp4_get_len(buff);
                        if ((moov_len > 0) && (moov_len <= (file_size - file_offset))) {
                            lseek(fd, file_offset, SEEK_SET);
                            moov_buff = malloc(moov_len);
                            memset(moov_buff, 0, moov_len);
                            read(fd, moov_buff, moov_len);
                            moov_offset = mp4_get_moov_offset_repa(moov_buff, moov_len, repa_id, &find);
                            free(moov_buff);
                        } else {
                            find = -1;
                        }
                    }
                    if (find == 0) {
                        if (file_offset == 0) {
                            printf("Repaired fail\n");
                            ret = REPA_FAIL;
                            break;
                        } else {
                            if (moov_offset == -5) {
                                file_offset = file_offset - buff_size + 64;
                            } else if (moov_offset < 0) {
                                file_offset = file_offset - 4;
                            } else {
                                if (again) {
                                   again = 0;
                                   file_offset = file_offset - buff_size + moov_offset;
                                } else {
                                   again = 1;
                                   file_offset = file_offset + moov_offset;
                                }
                            }
                        }
                    } else if (find == -1) {
                        file_offset = file_offset - buff_size;
                    } else if (find == 1) {
                        mdat_len = file_offset + moov_offset - mdat_offset;
                        //printf("file_offset = 0x%llx, moov_offset = 0x%llx, mdat_offset = 0x%llx, mdat_len= 0x%x\n", file_offset, moov_offset, mdat_offset, mdat_len);
                        //printf("0x%llx\n", file_offset + moov_offset);
                        if (mp4_moov_check(fd, mdat_offset + mdat_len) == 0) {
                            if (mp4_set_mdat_len(fd, mdat_offset, mdat_len) == 0) {
                                mp4_set_end(fd, file_offset + moov_offset);
                                printf("Repaired successfully\n");
                                ret = REPA_SUCC;
                            }
                            break;
                        } else {
                            if (file_offset == 0) {
                                printf("Repaired fail\n");
                                ret = REPA_FAIL;
                                break;
                            } else {
                                if (moov_offset == -5) {
                                    file_offset = file_offset - buff_size + 64;
                                } else if (moov_offset < 0) {
                                    file_offset = file_offset - 4;
                                } else {
                                    if (again) {
                                       again = 0;
                                       file_offset = file_offset - buff_size + moov_offset;
                                    } else {
                                       again = 1;
                                       file_offset = file_offset + moov_offset;
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                printf("No need to fix\n");
                ret = REPA_NONE;
            }
        }
    }
out:
    if (buff)
        free(buff);
    if (fd) {
        fsync(fd);
        close(fd);
    }

    if (need_remove) {
        remove(file);
    }

    return ret;
}