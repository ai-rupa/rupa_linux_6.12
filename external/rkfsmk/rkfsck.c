/* fsck.fat.c - User interface

   Copyright (C) 1993 Werner Almesberger <werner.almesberger@lrc.di.epfl.ch>
   Copyright (C) 1998 Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de>
   Copyright (C) 2008-2014 Daniel Baumann <mail@daniel-baumann.ch>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.

   The complete text of the GNU General Public License
   can be found in /usr/share/common-licenses/GPL-3 file.
*/

/* FAT32, VFAT, Atari format support, and various fixes additions May 1998
 * by Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include "common.h"
#include "rkfsck.h"
#include "io.h"
#include "boot.h"
#include "fat.h"
#include "file.h"
#include "check.h"
#include "charconv.h"
#include "rkfsmk.h"

int interactive = 0, verbose = 0, write_immed = 0;
int rw = 1;

static void read_userpara(DOS_FS *fs)
{
    struct user_para_info userpara;

    /* Clean up from previous pass */
    if (fs->userpara)
        free(fs->userpara);
    fs->userpara = NULL;

    if (FsRead(fs, fs->fat_start + fs->fat_size * fs->nfats, sizeof(struct user_para_info), &userpara))
        return;
    //printf("%s, %x, %x, %d, %d, %d\n", __func__, userpara.Mark1, userpara.Mark2, userpara.AlignCluste, userpara.StartCluste, userpara.EndCluste);
    if ((userpara.Mark1 == 0x5a5a5a5a) && (userpara.Mark2 == 0xa5a5a5a5)) {
        fs->userpara = malloc(sizeof(struct user_para_info));
        memcpy(fs->userpara, &userpara, sizeof(struct user_para_info));
    }
}

static void fs_free(DOS_FS *fs)
{
    if (fs->userpara)
        free(fs->userpara);
    if (fs->cluster_owner)
        free(fs->cluster_owner);
    if (fs->label) {
        free(fs->label);
    }
    if (fs->fat) {
        free(fs->fat);
    }
}

int rkfsmk_fat_check(char *dev, struct reg_para *para)
{
    int ret = RKFSCK_FAIL;
    DOS_FS fs;
    int salvage_files, verify, c;
    uint32_t free_clusters = 0;
    int boot_only = 0;
    int quit = 0;

    memset(&fs, 0, sizeof(fs));
    if (para && para->quit)
        fs.quit = para->quit;
    else
        fs.quit = &quit;
    fs.lfn_slot = -1;
    fs.read_test = 0;
    salvage_files = 1;
    verify = 0;
    rw = 1;
    interactive = 0;

    if (FsOpen(&fs, dev, rw) != 0)
        return RKFSCK_FAIL;

    if (BootRead(&fs)) {
      goto exit;
    }

    if (boot_only)
      goto exit;

    if (para && para->check_format_id) {
        int i = 0;
        for (i = 0; i < 8; i++) {
            if (fs.system_id[i] != para->format_id[i]) {
                ret = RKFSCK_ID_ERR;
                goto exit;
            }
        }
    }
    read_userpara(&fs);

    if (verify)
        printf("Starting check/repair pass.\n");
    if (ReadFat(&fs)) {
        ret = RKFSCK_FAIL;
        goto exit;
    }
    if (ScanRoot(&fs, para)) {
        ret = RKFSCK_FAIL;
        goto exit;
    }
    if (fs.read_test)
        FixBad(&fs);
    if (salvage_files)
        ReclaimFile(&fs);
    else
        ReclaimFree(&fs);
    free_clusters = UpdateFree(&fs);
    FileUnused(fs.fp_root);
    Free(&fs.mem_queue);
    /*
    if (verify) {
        fs.n_files = 0;
        printf("Starting verification pass.\n");
        ReadFat(&fs);
        ScanRoot(&fs, para);
        ReclaimFree(&fs);
        Free(&fs.mem_queue);
    }
    */
    ret = RKFSCK_SUS;
exit:
    if (!boot_only)
        printf("%s: %u files, %lu/%lu clusters\n", dev,
               fs.n_files, (unsigned long)fs.data_clusters - free_clusters,
               (unsigned long)fs.data_clusters);
    Free(&fs.mem_queue);
    fs_free(&fs);
    FsClose(&fs, rw);

    return ret;
}

#if 0
void cb(void *userdata, char *filename, int dir, struct stat *statbuf)
{
    char *path = (char *)userdata;
    printf("%s, %s, %s, %lld, %ld\n", __func__, path, filename, statbuf->st_size, statbuf->st_mtime);
}

int main(int argc , char **argv)
{
    struct reg_para para;
    para.folder_num = 4;
    para.folder = malloc(sizeof(struct folder_para) * para.folder_num);
    para.folder[0].path = "/video_front/";
    para.folder[0].userdata = para.folder[0].path;
    para.folder[0].cb = &cb;
    para.folder[1].path = "/video_back/";
    para.folder[1].userdata = para.folder[1].path;
    para.folder[1].cb = &cb;
    para.folder[2].path = "/photo/";
    para.folder[2].userdata = para.folder[2].path;
    para.folder[2].cb = &cb;
    para.folder[3].path = "/video_urgent/";
    para.folder[3].userdata = para.folder[3].path;
    para.folder[3].cb = &cb;

    if (argc >= 2) {
        rkfsmk_fat_check(argv[1], &para);
    }

    return 0;
}
#endif
