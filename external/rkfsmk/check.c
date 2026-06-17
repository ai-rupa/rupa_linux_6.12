/* check.c - Check and repair a PC/MS-DOS filesystem

   Copyright (C) 1993 Werner Almesberger <werner.almesberger@lrc.di.epfl.ch>
   Copyright (C) 1998 Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de>
   Copyright (C) 2008-2014 Daniel Baumann <mail@daniel-baumann.ch>
   Copyright (C) 2015 Andreas Bombe <aeb@debian.org>

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
#include <time.h>
#include <stdint.h>
#include <limits.h>

#include "common.h"
#include "rkfsck.h"
#include "io.h"
#include "fat.h"
#include "file.h"
#include "check.h"
#include "lfn.h"

/* the longest path on the filesystem that can be handled by path_name() */
#define PATH_NAME_MAX 1023

static struct reg_para *regpara = NULL;
//static DOS_FILE *root;

/* get start field of a dir entry */
#define FSTART(p,fs) \
  ((uint32_t)le16toh(p->dir_ent.start) | \
   (fs->fat_bits == 32 ? le16toh(p->dir_ent.starthi) << 16 : 0))

#define MODIFY(p,i,v,fs)                   \
  do {                          \
    if (p->offset) {                    \
    p->dir_ent.i = v;               \
    FsWrite(fs,p->offset+offsetof(DIR_ENT,i),     \
         sizeof(p->dir_ent.i),&p->dir_ent.i);   \
    }                           \
  } while(0)

#define MODIFY_START(p,v,fs)                        \
  do {                                  \
    uint32_t __v = (v);                         \
    if (!p->offset) {                           \
    /* writing to fake entry for FAT32 root dir */          \
    if (!__v) Die("Oops, deleting FAT32 root dir!");        \
    fs->root_cluster = __v;                     \
    p->dir_ent.start = htole16(__v&0xffff);             \
    p->dir_ent.starthi = htole16(__v>>16);              \
    __v = htole32(__v);                     \
    FsWrite(fs,offsetof(struct boot_sector,root_cluster),     \
             sizeof(((struct boot_sector *)0)->root_cluster),   \
         &__v);                         \
    }                                   \
    else {                              \
    MODIFY(p,start,htole16((__v)&0xffff),fs);              \
    if (fs->fat_bits == 32)                     \
        MODIFY(p,starthi,htole16((__v)>>16),fs);           \
    }                                   \
  } while(0)

#if 0
off_t AllocRootEntry(DOS_FS * fs, DIR_ENT * de, const char *pattern, int gen_name)
{
    static int curr_num = 0;
    off_t offset;

    if (fs->root_cluster) {
        DIR_ENT d2;
        int i = 0, got = 0;
        uint32_t clu_num, prev = 0;
        off_t offset2;

        clu_num = fs->root_cluster;
        offset = ClusterStart(fs, clu_num);
        while (clu_num > 0 && clu_num != -1) {
            FsRead(fs, offset, sizeof(DIR_ENT), &d2);
            if (IS_FREE(d2.name) && d2.attr != VFAT_LN_ATTR) {
                got = 1;
                break;
            }
            i += sizeof(DIR_ENT);
            offset += sizeof(DIR_ENT);
            if ((i % fs->cluster_size) == 0) {
                prev = clu_num;
                if ((clu_num = NextCluster(fs, clu_num)) == 0 || clu_num == -1)
                    break;
                offset = ClusterStart(fs, clu_num);
            }
        }
        if (!got) {
            /* no free slot, need to extend root dir: alloc next free cluster
             * after previous one */
            if (!prev)
                Die("Root directory has no cluster allocated!");
            for (clu_num = prev + 1; clu_num != prev; clu_num++) {
                FAT_ENTRY entry;

                if (clu_num >= fs->data_clusters + 2)
                    clu_num = 2;
                GetFat(&entry, fs->fat, clu_num, fs);
                if (!entry.value)
                    break;
            }
            if (clu_num == prev)
                Die("Root directory full and no free cluster");
            SetFat(fs, prev, clu_num);
            SetFat(fs, clu_num, -1);
            SetOwner(fs, clu_num, GetOwner(fs, fs->root_cluster));
            /* clear new cluster */
            memset(&d2, 0, sizeof(d2));
            offset = ClusterStart(fs, clu_num);
            for (i = 0; i < fs->cluster_size; i += sizeof(DIR_ENT))
                FsWrite(fs, offset + i, sizeof(d2), &d2);
        }
        memset(de, 0, sizeof(DIR_ENT));
        if (gen_name) {
            while (1) {
                char expanded[12];
                sprintf(expanded, pattern, curr_num);
                memcpy(de->name, expanded, MSDOS_NAME);
                clu_num = fs->root_cluster;
                i = 0;
                offset2 = ClusterStart(fs, clu_num);
                while (clu_num > 0 && clu_num != -1) {
                    FsRead(fs, offset2, sizeof(DIR_ENT), &d2);
                    if (offset2 != offset &&
                        !strncmp((const char *)d2.name, (const char *)de->name,
                                 MSDOS_NAME))
                        break;
                    i += sizeof(DIR_ENT);
                    offset2 += sizeof(DIR_ENT);
                    if ((i % fs->cluster_size) == 0) {
                        if ((clu_num = NextCluster(fs, clu_num)) == 0 ||
                            clu_num == -1)
                            break;
                        offset2 = ClusterStart(fs, clu_num);
                    }
                }
                if (clu_num == 0 || clu_num == -1)
                    break;
                if (++curr_num >= 10000)
                    Die("Unable to create unique name");
            }
        } else {
            memcpy(de->name, pattern, MSDOS_NAME);
        }
    } else {
        DIR_ENT *root;
        int next_free = 0, scan;

        root = Malloc(fs->root_entries * sizeof(DIR_ENT));
        FsRead(fs, fs->root_start, fs->root_entries * sizeof(DIR_ENT), root);

        while (next_free < fs->root_entries)
            if (IS_FREE(root[next_free].name) &&
                root[next_free].attr != VFAT_LN_ATTR)
                break;
            else
                next_free++;
        if (next_free == fs->root_entries)
            Die("Root directory is full.");
        offset = fs->root_start + next_free * sizeof(DIR_ENT);
        memset(de, 0, sizeof(DIR_ENT));
        if (gen_name) {
            while (1) {
                char expanded[12];
                sprintf(expanded, pattern, curr_num);
                memcpy(de->name, expanded, MSDOS_NAME);
                for (scan = 0; scan < fs->root_entries; scan++)
                    if (scan != next_free &&
                        !strncmp((const char *)root[scan].name,
                                 (const char *)de->name, MSDOS_NAME))
                        break;
                if (scan == fs->root_entries)
                    break;
                if (++curr_num >= 10000)
                    Die("Unable to create unique name");
            }
        } else {
            memcpy(de->name, pattern, MSDOS_NAME);
        }
        free(root);
    }
    ++fs->n_files;
    return offset;
}
#endif

/**
 * Construct a full path (starting with '/') for the specified dentry,
 * relative to the partition. All components are "long" names where possible.
 *
 * @param[in]   file    Information about dentry (file or directory) of interest
 *
 * return       Pointer to static string containing file's full path
 */
static char *path_name(DOS_FILE * file)
{
    static char path[PATH_NAME_MAX * 2];

    if (!file)
        *path = 0;      /* Reached the root directory */
    else {
        if (strlen(path_name(file->parent)) > PATH_NAME_MAX)
            Die("Path name too long.");
        if (strcmp(path, "/") != 0)
            strcat(path, "/");

        /* Append the long name to the path,
         * or the short name if there isn't a long one
         */
        strcpy(strrchr(path, 0),
               file->lfn ? file->lfn : FileName(file->dir_ent.name));
    }
    return path;
}

static char *file_name(DOS_FILE * file)
{
    static char path[PATH_NAME_MAX * 2];

    if (!file)
        *path = 0;      /* Reached the root directory */
    else {
        strcpy(path,
               file->lfn ? file->lfn : FileName(file->dir_ent.name));
    }
    return path;
}

static off_t file_size(DOS_FILE * file)
{
    return le32toh(file->dir_ent.size);
}

static const int day_n[] =
{   0,  31,  59,  90, 120, 151, 181, 212, 243, 273, 304, 334, 0, 0, 0, 0 };
/*    Jan  Feb  Mar  Apr  May  Jun  Jul  Aug  Sep  Oct  Nov  Dec              */

/* Convert a MS-DOS time/date pair to a UNIX date (seconds since 1 1 70). */

static time_t date_dos2unix(unsigned short time, unsigned short date)
{
    int month, year;
    time_t secs;

    month = ((date >> 5) & 15) - 1;
    if (month < 0) {
        /* make sure that nothing bad happens if the month bits were zero */
        month = 0;
    }
    year = date >> 9;
    secs =
        (time & 31) * 2 + 60 * ((time >> 5) & 63) + (time >> 11) * 3600 +
        86400 * ((date & 31) - 1 + day_n[month] + (year / 4) + year * 365 -
                 ((year & 3) == 0 && month < 2 ? 1 : 0) + 3653);
    /* days since 1.1.70 plus 80's leap day */
    return secs;
}

static time_t file_time(DOS_FILE * file)
{
    time_t date;

    date =
        date_dos2unix(le16toh(file->dir_ent.time), le16toh(file->dir_ent.date));
    return date;
}

static time_t file_ctime(DOS_FILE * file)
{
    time_t date;

    date =
        date_dos2unix(le16toh(file->dir_ent.ctime), le16toh(file->dir_ent.cdate));
    date += file->dir_ent.ctime_ms / 100;
    return date;
}

static blkcnt_t file_blocks(DOS_FS * fs, DOS_FILE * file)
{
    return file->clusters;
}

static char *file_stat(DOS_FILE * file)
{
    static char temp[100];
    struct tm *tm;
    char tmp[100];
    time_t date;

    date =
        date_dos2unix(le16toh(file->dir_ent.time), le16toh(file->dir_ent.date));
    tm = localtime(&date);
    strftime(tmp, 99, "%H:%M:%S %b %d %Y", tm);
    sprintf(temp, "  Size %u bytes, date %s", le32toh(file->dir_ent.size), tmp);
    return temp;
}

static int bad_name(DOS_FS * fs, DOS_FILE * file)
{
    int i, spc, suspicious = 0;
    const char *bad_chars = fs->atari_format ? "*?\\/:" : "*?<>|\"\\/:";
    const unsigned char *name = file->dir_ent.name;
    const unsigned char *ext = name + 8;

    /* Do not complain about (and auto-correct) the extended attribute files
     * of OS/2. */
    if (strncmp((const char *)name, "EA DATA  SF", 11) == 0 ||
        strncmp((const char *)name, "WP ROOT  SF", 11) == 0)
        return 0;

    /* check if we have neither a long filename nor a short name */
    if ((file->lfn == NULL) && (file->dir_ent.lcase & FAT_NO_83NAME)) {
        return 1;
    }

    /* don't complain about the dummy 11 bytes used by patched Linux
       kernels */
    if (file->dir_ent.lcase & FAT_NO_83NAME)
        return 0;

    for (i = 0; i < MSDOS_NAME; i++) {
        if (name[i] < ' ' || name[i] == 0x7f)
            return 1;
        if (name[i] > 0x7f)
            ++suspicious;
        if (strchr(bad_chars, name[i]))
            return 1;
    }

    spc = 0;
    for (i = 0; i < 8; i++) {
        if (name[i] == ' ')
            spc = 1;
        else if (spc)
            /* non-space after a space not allowed, space terminates the name
             * part */
            return 1;
    }

    spc = 0;
    for (i = 0; i < 3; i++) {
        if (ext[i] == ' ')
            spc = 1;
        else if (spc)
            /* non-space after a space not allowed, space terminates the ext
             * part */
            return 1;
    }

    /* Under GEMDOS, chars >= 128 are never allowed. */
    if (fs->atari_format && suspicious)
        return 1;

    /* Under MS-DOS and Windows, chars >= 128 in short names are valid
     * (but these characters can be visualised differently depending on
     * local codepage: CP437, CP866, etc). The chars are all basically ok,
     * so we shouldn't auto-correct such names. */
    return 0;
}

static void lfn_remove(DOS_FS * fs, off_t from, off_t to)
{
    DIR_ENT empty;

    /* New dir entry is zeroed except first byte, which is set to 0xe5.
     * This is to avoid that some FAT-reading OSes (not Linux! ;) stop reading
     * a directory at the first zero entry...
     */
    memset(&empty, 0, sizeof(empty));
    empty.name[0] = DELETED_FLAG;

    for (; from < to; from += sizeof(empty)) {
        FsWrite(fs, from, sizeof(DIR_ENT), &empty);
    }
}

static void drop_file(DOS_FS * fs, DOS_FILE * file)
{
    uint32_t cluster;

    MODIFY(file, name[0], DELETED_FLAG, fs);
    if (file->lfn)
        lfn_remove(fs, file->lfn_offset, file->offset);
    for (cluster = FSTART(file, fs); cluster > 0 && cluster <
         fs->data_clusters + 2; cluster = NextCluster(fs, cluster))
        SetOwner(fs, cluster, NULL);
    --fs->n_files;
}

static void truncate_file(DOS_FS * fs, DOS_FILE * file, uint32_t clusters)
{
    int deleting;
    uint32_t walk, next;

    walk = FSTART(file, fs);
    if ((deleting = !clusters))
        MODIFY_START(file, 0, fs);
    while (walk > 0 && walk != -1) {
        next = NextCluster(fs, walk);
        if (deleting)
            SetFat(fs, walk, 0);
        else if ((deleting = !--clusters))
            SetFat(fs, walk, -1);
        walk = next;
    }
}

static void align_file(DOS_FS * fs, DOS_FILE * file, uint32_t clusters)
{
    int deleting;
    uint32_t walk, next;

    walk = FSTART(file, fs);
    if ((deleting = !clusters))
        MODIFY_START(file, 0, fs);
    while (walk > 0 && walk != -1) {
        next = NextCluster(fs, walk);
        if (deleting)
            SetFat(fs, walk, 0);
        else if ((deleting = !--clusters))
            SetFat(fs, walk, -1);
        walk = next;
    }
}

static void auto_rename(DOS_FS * fs, DOS_FILE * file)
{
    DOS_FILE *first, *walk;
    uint32_t number;

    if (!file->offset)
        return;         /* cannot rename FAT32 root dir */
    first = file->parent ? file->parent->first : fs->root;
    number = 0;
    while (1) {
        char num[8];
        sprintf(num, "%07lu", (unsigned long)number);
        memcpy(file->dir_ent.name, "FSCK", 4);
        memcpy(file->dir_ent.name + 4, num, 7);
        for (walk = first; walk; walk = walk->next)
            if (walk != file
                && !strncmp((const char *)walk->dir_ent.name,
                            (const char *)file->dir_ent.name, MSDOS_NAME))
                break;
        if (!walk) {
            if (file->dir_ent.lcase & FAT_NO_83NAME) {
                /* as we only assign a new 8.3 filename, reset flag that 8.3 name is not
                   present */
                file->dir_ent.lcase &= ~FAT_NO_83NAME;
                /* reset the attributes, only keep DIR and VOLUME */
                file->dir_ent.attr &= ~(ATTR_DIR | ATTR_VOLUME);
                FsWrite(fs, file->offset, MSDOS_NAME + 2, &file->dir_ent);
            } else {
                FsWrite(fs, file->offset, MSDOS_NAME, file->dir_ent.name);
            }
            if (file->lfn)
                LfnFixChecksum(fs, file->lfn_offset, file->offset,
                                 (const char *)file->dir_ent.name);
            return;
        }
        number++;
        if (number > 9999999) {
            Die("Too many files need repair.");
        }
    }
    Die("Can't generate a unique name.");
}

static void rename_file(DOS_FS * fs, DOS_FILE * file)
{
    unsigned char name[46];
    unsigned char *walk, *here;

    if (!file->offset) {
        printf("Cannot rename FAT32 root dir\n");
        return;         /* cannot rename FAT32 root dir */
    }
    while (1) {
        printf("New name: ");
        fflush(stdout);
        if (fgets((char *)name, 45, stdin)) {
            if ((here = (unsigned char *)strchr((const char *)name, '\n')))
                * here = 0;
            for (walk = (unsigned char *)strrchr((const char *)name, 0);
                 walk >= name && (*walk == ' ' || *walk == '\t'); walk--) ;
            walk[1] = 0;
            for (walk = name; *walk == ' ' || *walk == '\t'; walk++) ;
            if (FileCvt(walk, file->dir_ent.name)) {
                if (file->dir_ent.lcase & FAT_NO_83NAME) {
                    /* as we only assign a new 8.3 filename, reset flag that 8.3 name is not
                       present */
                    file->dir_ent.lcase &= ~FAT_NO_83NAME;
                    /* reset the attributes, only keep DIR and VOLUME */
                    file->dir_ent.attr &= ~(ATTR_DIR | ATTR_VOLUME);
                    FsWrite(fs, file->offset, MSDOS_NAME + 2, &file->dir_ent);
                } else {
                    FsWrite(fs, file->offset, MSDOS_NAME, file->dir_ent.name);
                }
                if (file->lfn)
                    LfnFixChecksum(fs, file->lfn_offset, file->offset,
                                     (const char *)file->dir_ent.name);
                return;
            }
        }
    }
}

static int handle_dot(DOS_FS * fs, DOS_FILE * file, int dots)
{
    const char *name;

    name =
        strncmp((const char *)file->dir_ent.name, MSDOS_DOT,
                MSDOS_NAME) ? ".." : ".";
    if (!(file->dir_ent.attr & ATTR_DIR)) {
        printf("%s\n  Is a non-directory.\n", path_name(file));
        if (interactive)
            printf("1) Drop it\n2) Auto-rename\n3) Rename\n"
                   "4) Convert to directory\n");
        else
            printf("  Auto-renaming it.\n");
        switch (interactive ? GetKey("1234", "?") : '2') {
        case '1':
            drop_file(fs, file);
            return 1;
        case '2':
            auto_rename(fs, file);
            printf("  Renamed to %s\n", FileName(file->dir_ent.name));
            return 0;
        case '3':
            rename_file(fs, file);
            return 0;
        case '4':
            MODIFY(file, size, htole32(0), fs);
            MODIFY(file, attr, file->dir_ent.attr | ATTR_DIR, fs);
            break;
        }
    }
    if (!dots) {
        printf("Root contains directory \"%s\". Dropping it.\n", name);
        drop_file(fs, file);
        return 1;
    }
    return 0;
}

static int check_file(DOS_FS * fs, DOS_FILE * file, struct folder_para *folder)
{
    DOS_FILE *owner;
    int restart;
    uint32_t expect, curr, this, clusters, prev, walk, clusters2;
    int dir = 0;

    if (file->dir_ent.attr & ATTR_DIR) {
        dir = 1;
        if (le32toh(file->dir_ent.size)) {
            printf("%s\n  Directory has non-zero size. Fixing it.\n",
                   path_name(file));
            MODIFY(file, size, htole32(0), fs);
        }
        if (file->parent
            && !strncmp((const char *)file->dir_ent.name, MSDOS_DOT,
                        MSDOS_NAME)) {
            expect = FSTART(file->parent, fs);
            if (FSTART(file, fs) != expect) {
                printf("%s\n  Start (%lu) does not point to parent (%lu)\n",
                       path_name(file), (unsigned long)FSTART(file, fs), (long)expect);
                MODIFY_START(file, expect, fs);
            }
            return 0;
        }
        if (file->parent
            && !strncmp((const char *)file->dir_ent.name, MSDOS_DOTDOT,
                        MSDOS_NAME)) {
            expect =
                file->parent->parent ? FSTART(file->parent->parent, fs) : 0;
            if (fs->root_cluster && expect == fs->root_cluster)
                expect = 0;
            if (FSTART(file, fs) != expect) {
                printf("%s\n  Start (%lu) does not point to .. (%lu)\n",
                       path_name(file), (unsigned long)FSTART(file, fs), (unsigned long)expect);
                MODIFY_START(file, expect, fs);
            }
            return 0;
        }
        if (FSTART(file, fs) == 0) {
            printf("%s\n Start does point to root directory. Deleting dir. \n",
                   path_name(file));
            MODIFY(file, name[0], DELETED_FLAG, fs);
            return 0;
        }
    }

    if (FSTART(file, fs) == 1) {
        printf("%s\n  Bad start cluster 1. Truncating file.\n",
               path_name(file));
        if (!file->offset)
            Die("Bad FAT32 root directory! (bad start cluster 1)\n");
        MODIFY_START(file, 0, fs);
    }
    if (FSTART(file, fs) >= fs->data_clusters + 2) {
        printf
        ("%s\n  Start cluster beyond limit (%lu > %lu). Truncating file.\n",
         path_name(file), (unsigned long)FSTART(file, fs),
         (unsigned long)(fs->data_clusters + 1));
        if (!file->offset)
            Die("Bad FAT32 root directory! (start cluster beyond limit: %lu > %lu)\n",
                  (unsigned long)FSTART(file, fs),
                  (unsigned long)(fs->data_clusters + 1));
        MODIFY_START(file, 0, fs);
    }
    clusters = prev = 0;
    for (curr = FSTART(file, fs) ? FSTART(file, fs) :
                -1; curr != -1; curr = NextCluster(fs, curr)) {
        FAT_ENTRY curEntry;
        GetFat(&curEntry, fs->fat, curr, fs);

        if (!curEntry.value || BadCluster(fs, curr)) {
            printf("%s\n  Contains a %s cluster (%lu). Assuming EOF.\n",
                   path_name(file), curEntry.value ? "bad" : "free", (unsigned long)curr);
            if (prev)
                SetFat(fs, prev, -1);
            else if (!file->offset)
                Die("FAT32 root dir starts with a bad cluster!");
            else
                MODIFY_START(file, 0, fs);
            break;
        }
#if 0
        if (fs->userpara && FSTART(file, fs) >= fs->userpara->StartCluste) {

        } else {
            if (!(file->dir_ent.attr & ATTR_DIR) && le32toh(file->dir_ent.size) <=
                (uint64_t)clusters * fs->cluster_size) {
                printf
                ("%s\n  File size is %u bytes, cluster chain length is > %llu "
                 "bytes.\n  Truncating file to %u bytes.\n", path_name(file),
                 le32toh(file->dir_ent.size),
                 (unsigned long long)clusters * fs->cluster_size,
                 le32toh(file->dir_ent.size));
                truncate_file(fs, file, clusters);
                break;
            }
        }
#endif
        if ((owner = GetOwner(fs, curr))) {
            int do_trunc = 0;
            printf("%s  and\n", path_name(owner));
            printf("%s\n  share clusters.\n", path_name(file));
            clusters2 = 0;
            for (walk = FSTART(owner, fs); walk > 0 && walk != -1; walk =
                     NextCluster(fs, walk))
                if (walk == curr)
                    break;
                else
                    clusters2++;
            restart = file->dir_ent.attr & ATTR_DIR;
            if (!owner->offset) {
                printf("  Truncating second to %llu bytes because first "
                       "is FAT32 root dir.\n",
                       (unsigned long long)clusters * fs->cluster_size);
                do_trunc = 2;
            } else if (!file->offset) {
                printf("  Truncating first to %llu bytes because second "
                       "is FAT32 root dir.\n",
                       (unsigned long long)clusters2 * fs->cluster_size);
                do_trunc = 1;
            } else if (interactive)
                printf("1) Truncate first to %llu bytes%s\n"
                       "2) Truncate second to %llu bytes\n",
                       (unsigned long long)clusters2 * fs->cluster_size,
                       restart ? " and restart" : "",
                       (unsigned long long)clusters * fs->cluster_size);
            else
                printf("  Truncating second to %llu bytes.\n",
                       (unsigned long long)clusters * fs->cluster_size);
            if (do_trunc != 2
                && (do_trunc == 1
                    || (interactive && GetKey("12", "?") == '1'))) {
                prev = 0;
                clusters = 0;
                for (this = FSTART(owner, fs); this > 0 && this != -1; this =
                         NextCluster(fs, this)) {
                    if (this == curr) {
                        if (prev)
                            SetFat(fs, prev, -1);
                        else
                            MODIFY_START(owner, 0, fs);
                        MODIFY(owner, size,
                               htole32((uint64_t)clusters *
                                       fs->cluster_size), fs);
                        if (restart)
                            return 1;
                        while (this > 0 && this != -1) {
                            SetOwner(fs, this, NULL);
                            this = NextCluster(fs, this);
                        }
                        this = curr;
                        break;
                    }
                    clusters++;
                    prev = this;
                }
                if (this != curr)
                    Die("Internal error: didn't find cluster %d in chain"
                          " starting at %d", curr, FSTART(owner, fs));
            } else {
                if (prev)
                    SetFat(fs, prev, -1);
                else
                    MODIFY_START(file, 0, fs);
                break;
            }
        }
        SetOwner(fs, curr, file);
        if (fs->userpara && curr >= fs->userpara->StartCluste)
            file->clusters++;
        clusters++;
        prev = curr;
    }
    if (fs->userpara && file->clusters) {
        uint32_t tmp = file->clusters % fs->userpara->AlignCluste;
        if (tmp) {
            int i;
            tmp = fs->userpara->AlignCluste - tmp;
            curr = prev;
            for (i = 0; i < (fs->data_clusters - fs->userpara->StartCluste) && tmp; i++) {
                FAT_ENTRY curEntry;
                if (curr > fs->data_clusters + 1 || curr < fs->userpara->StartCluste)
                    curr = fs->userpara->StartCluste;
                GetFat(&curEntry, fs->fat, curr, fs);
                if (!curEntry.value) {
                    //printf("%d\n", curr);
                    SetFat(fs, prev, curr);
                    SetOwner(fs, prev, file);
                    prev = curr;
                    tmp--;
                    if (tmp == 0) {
                        SetFat(fs, prev, -1);
                        SetOwner(fs, prev, file);
                    }
                }
                curr++;
            }
        }
    }
    //printf("%s, %s, %d\n", path_name(file), fs->name_filter, strstr(path_name(file), fs->name_filter));
    //if (fs->userpara && FSTART(file, fs) >= fs->userpara->StartCluste) {
    //    printf("    %d, %d, %d, %d\n", FSTART(file, fs), le32toh(file->dir_ent.size), clusters, clusters % fs->userpara->AlignCluste);
    //}
#if 0
    if (!(file->dir_ent.attr & ATTR_DIR) && le32toh(file->dir_ent.size) >
        (uint64_t)clusters * fs->cluster_size) {
        printf
        ("%s\n  File size is %u bytes, cluster chain length is %llu bytes."
         "\n  Truncating file to %llu bytes.\n", path_name(file),
         le32toh(file->dir_ent.size),
         (unsigned long long)clusters * fs->cluster_size,
         (unsigned long long)clusters * fs->cluster_size);
        MODIFY(file, size,
               htole32((uint64_t)clusters * fs->cluster_size), fs);
    }
#endif
    if (folder) {
        struct stat st;
        unsigned long long blocks;
        memset(&st, 0, sizeof(struct stat));
        blocks = fs->cluster_size / 512 * clusters;
        st.st_atime = file_ctime(file);
        st.st_mtime = file_time(file);
        st.st_size = file_size(file);
        st.st_blocks = blocks;
        st.st_blksize = 512;
        
        if (folder->cb)
            folder->cb(folder->userdata, file_name(file), dir, &st);
        //printf("%s, %s, %s, %lld, %ld, %d, %d, %lld\n", __func__, path_name(file), file_name(file), st.st_size, st.st_mtime, clusters, fs->cluster_size, st.st_blocks);
    }

    //printf("%s, %s, %s, %lld, %ld, %ld, %d\n", __func__, path_name(file), file_name(file), file_size(file), file_time(file), file_ctime(file), clusters);
    return 0;
}

static int check_files(DOS_FS * fs, DOS_FILE * start)
{
    struct folder_para *folder = NULL;
	  //printf("%s, %s, %s, %d\n", __func__, path_name(start), file_name(start), FSTART(start, fs));
	  if (regpara) {
	      int i;
	      char path[256];
	      snprintf(path, 256, "%s", path_name(start));
	      for (i = 0; i < 255; i ++) {
	          if (path[i] == 0)
	              break;
	          if((path[i] == '.') && (path[i + 1] == 0)) {
	              path[i] = 0;
	              break;
	          }
	      }
	      for (i = 0; i < regpara->folder_num; i ++) {
	          if (strcmp(regpara->folder[i].path, path) == 0) {
	              //printf("%s, %s, %s\n", __func__, regpara->folder[i].path, path_name(start));
	              folder = &regpara->folder[i];
	              break;
	          }
	      }
	  }
    while (start) {
        if (check_file(fs, start, folder))
            return 1;
        start = start->next;
    }
    return 0;
}

static int check_dir(DOS_FS * fs, DOS_FILE ** root, int dots)
{
    DOS_FILE *parent, **walk, **scan;
    int dot, dotdot, skip, redo;
    int good, bad;

    if (!*root)
        return 0;
    parent = (*root)->parent;
    good = bad = 0;
    for (walk = root; *walk; walk = &(*walk)->next)
        if (bad_name(fs, *walk))
            bad++;
        else
            good++;
    if (*root && parent && good + bad > 4 && bad > good / 2) {
        printf("%s\n  Has a large number of bad entries. (%d/%d)\n",
               path_name(parent), bad, good + bad);
        if (!dots)
            printf("  Not dropping root directory.\n");
        else if (!interactive)
            printf("  Not dropping it in auto-mode.\n");
        else if (GetKey("yn", "Drop directory ? (y/n)") == 'y') {
            truncate_file(fs, parent, 0);
            MODIFY(parent, name[0], DELETED_FLAG, fs);
            /* buglet: deleted directory stays in the list. */
            return 1;
        }
    }
    dot = dotdot = redo = 0;
    walk = root;

    while (*walk) {
        if (*(fs->quit)) {
            printf("%s quit\n", __func__);
            return 1;
        }
        if (!strncmp
            ((const char *)((*walk)->dir_ent.name), MSDOS_DOT, MSDOS_NAME)
            || !strncmp((const char *)((*walk)->dir_ent.name), MSDOS_DOTDOT,
                        MSDOS_NAME)) {
            if (handle_dot(fs, *walk, dots)) {
                *walk = (*walk)->next;
                continue;
            }
            if (!strncmp
                ((const char *)((*walk)->dir_ent.name), MSDOS_DOT, MSDOS_NAME))
                dot++;
            else
                dotdot++;
        }
        if (!((*walk)->dir_ent.attr & ATTR_VOLUME) && bad_name(fs, *walk)) {
            puts(path_name(*walk));
            printf("  Bad short file name (%s).\n",
                   FileName((*walk)->dir_ent.name));
            if (interactive)
                printf("1) Drop file\n2) Rename file\n3) Auto-rename\n"
                       "4) Keep it\n");
            else
                printf("  Auto-renaming it.\n");
            switch (interactive ? GetKey("1234", "?") : '3') {
            case '1':
                drop_file(fs, *walk);
                walk = &(*walk)->next;
                continue;
            case '2':
                rename_file(fs, *walk);
                redo = 1;
                break;
            case '3':
                auto_rename(fs, *walk);
                printf("  Renamed to %s\n", FileName((*walk)->dir_ent.name));
                break;
            case '4':
                break;
            }
        }

        /* don't check for duplicates of the volume label */
        if (!((*walk)->dir_ent.attr & ATTR_VOLUME)) {
            scan = &(*walk)->next;
            skip = 0;
            while (*scan && !skip) {
                if (!((*scan)->dir_ent.attr & ATTR_VOLUME) &&
                    !memcmp((*walk)->dir_ent.name, (*scan)->dir_ent.name,
                            MSDOS_NAME)) {
                    printf("%s\n  Duplicate directory entry.\n  First  %s\n",
                           path_name(*walk), file_stat(*walk));
                    printf("  Second %s\n", file_stat(*scan));
                    if (interactive)
                        printf
                        ("1) Drop first\n2) Drop second\n3) Rename first\n"
                         "4) Rename second\n5) Auto-rename first\n"
                         "6) Auto-rename second\n");
                    else
                        printf("  Auto-renaming second.\n");
                    switch (interactive ? GetKey("123456", "?") : '6') {
                    case '1':
                        drop_file(fs, *walk);
                        *walk = (*walk)->next;
                        skip = 1;
                        break;
                    case '2':
                        drop_file(fs, *scan);
                        *scan = (*scan)->next;
                        continue;
                    case '3':
                        rename_file(fs, *walk);
                        printf("  Renamed to %s\n", path_name(*walk));
                        redo = 1;
                        break;
                    case '4':
                        rename_file(fs, *scan);
                        printf("  Renamed to %s\n", path_name(*walk));
                        redo = 1;
                        break;
                    case '5':
                        auto_rename(fs, *walk);
                        printf("  Renamed to %s\n",
                               FileName((*walk)->dir_ent.name));
                        break;
                    case '6':
                        auto_rename(fs, *scan);
                        printf("  Renamed to %s\n",
                               FileName((*scan)->dir_ent.name));
                        break;
                    }
                }
                scan = &(*scan)->next;
            }
            if (skip)
                continue;
        }
        if (!redo)
            walk = &(*walk)->next;
        else {
            walk = root;
            dot = dotdot = redo = 0;
        }
    }
    if (dots && !dot)
        printf("%s\n  \".\" is missing. Can't fix this yet.\n",
               path_name(parent));
    if (dots && !dotdot)
        printf("%s\n  \"..\" is missing. Can't fix this yet.\n",
               path_name(parent));
    return 0;
}

/**
 * Check a dentry's cluster chain for bad clusters.
 * If requested, we verify readability and mark unreadable clusters as bad.
 *
 * @param[inout]    fs          Information about the filesystem
 * @param[in]       file        dentry to check
 * @param[in]       read_test   Nonzero == verify that dentry's clusters can
 *                              be read
 */
static void test_file(DOS_FS * fs, DOS_FILE * file, int read_test)
{
    DOS_FILE *owner;
    uint32_t walk, prev, clusters, next_clu;

    prev = clusters = 0;
    for (walk = FSTART(file, fs); walk > 1 && walk < fs->data_clusters + 2;
         walk = next_clu) {
        next_clu = NextCluster(fs, walk);

        /* In this stage we are checking only for a loop within our own
         * cluster chain.
         * Cross-linking of clusters is handled in check_file()
         */
        if ((owner = GetOwner(fs, walk))) {
            if (owner == file) {
                printf("%s\n  Circular cluster chain. Truncating to %lu "
                       "cluster%s.\n", path_name(file), (unsigned long)clusters,
                       clusters == 1 ? "" : "s");
                if (prev)
                    SetFat(fs, prev, -1);
                else if (!file->offset)
                    Die("Bad FAT32 root directory! (bad start cluster)\n");
                else
                    MODIFY_START(file, 0, fs);
            }
            break;
        }
        if (BadCluster(fs, walk))
            break;
        if (read_test) {
            if (FsCheck(fs, ClusterStart(fs, walk), fs->cluster_size)) {
                prev = walk;
                clusters++;
            } else {
                printf("%s\n  Cluster %lu (%lu) is unreadable. Skipping it.\n",
                       path_name(file), (unsigned long)clusters, (unsigned long)walk);
                if (prev)
                    SetFat(fs, prev, NextCluster(fs, walk));
                else
                    MODIFY_START(file, NextCluster(fs, walk), fs);
                SetFat(fs, walk, -2);
            }
        } else {
            prev = walk;
            clusters++;
        }
        SetOwner(fs, walk, file);
    }
    /* Revert ownership (for now) */
    for (walk = FSTART(file, fs); walk > 1 && walk < fs->data_clusters + 2;
         walk = NextCluster(fs, walk))
        if (BadCluster(fs, walk))
            break;
        else if (GetOwner(fs, walk) == file)
            SetOwner(fs, walk, NULL);
        else
            break;
}

static void undelete(DOS_FS * fs, DOS_FILE * file)
{
    uint32_t clusters, left, prev, walk;

    clusters = left = (le32toh(file->dir_ent.size) + fs->cluster_size - 1) /
                      fs->cluster_size;
    prev = 0;

    walk = FSTART(file, fs);

    while (left && (walk >= 2) && (walk < fs->data_clusters + 2)) {

        FAT_ENTRY curEntry;
        GetFat(&curEntry, fs->fat, walk, fs);

        if (!curEntry.value)
            break;

        left--;
        if (prev)
            SetFat(fs, prev, walk);
        prev = walk;
        walk++;
    }
    if (prev)
        SetFat(fs, prev, -1);
    else
        MODIFY_START(file, 0, fs);
    if (left)
        printf("Warning: Did only undelete %lu of %lu cluster%s.\n",
               (unsigned long)clusters - left, (unsigned long)clusters, clusters == 1 ? "" : "s");

}

static void new_dir(DOS_FS * fs)
{
    LfnReset(fs);
}

/**
 * Create a description for a referenced dentry and insert it in our dentry
 * tree. Then, go check the dentry's cluster chain for bad clusters and
 * cluster loops.
 *
 * @param[inout]    fs      Information about the filesystem
 * @param[out]      chain
 * @param[in]       parent  Information about parent directory of this file
 *                          NULL == no parent ('file' is root directory)
 * @param[in]       offset  Partition-relative byte offset of directory entry of interest
 *                          0 == Root directory
 * @param           cp
 */
static int add_file(DOS_FS * fs, DOS_FILE *** chain, DOS_FILE * parent,
                     off_t offset, FDSC ** cp)
{
    DOS_FILE *new;
    DIR_ENT de;
    FD_TYPE type;

    if (offset) {
        if (FsRead(fs, offset, sizeof(DIR_ENT), &de))
            return 1;
    } else {
        /* Construct a DIR_ENT for the root directory */
        memset(&de, 0, sizeof de);
        memcpy(de.name, "           ", MSDOS_NAME);
        de.attr = ATTR_DIR;
        de.start = htole16(fs->root_cluster & 0xffff);
        de.starthi = htole16((fs->root_cluster >> 16) & 0xffff);
    }
    if ((type = FileType(cp, (char *)de.name)) != fdt_none) {
        if (type == fdt_undelete && (de.attr & ATTR_DIR)) {
            printf("Can't undelete directories.\n");
            return 1;
        }
        FileModify(cp, (char *)de.name);
        FsWrite(fs, offset, 1, &de);
    }
    if (IS_FREE(de.name)) {
        LfnCheckOrphaned(fs);
        return 0;
    }
    if (de.attr == VFAT_LN_ATTR) {
        LfnAddSlot(fs, &de, offset);
        return 0;
    }
    new = Alloc(&fs->mem_queue, sizeof(DOS_FILE));
    memset(new, 0, sizeof(DOS_FILE));
    new->lfn = LfnGet(fs, &de, &new->lfn_offset);
    new->offset = offset;
    memcpy(&new->dir_ent, &de, sizeof(de));
    new->next = new->first = NULL;
    new->parent = parent;
    if (type == fdt_undelete)
        undelete(fs, new);
    **chain = new;
    *chain = &new->next;

    /* Don't include root directory, '.', or '..' in the total file count */
    if (offset &&
        strncmp((const char *)de.name, MSDOS_DOT, MSDOS_NAME) != 0 &&
        strncmp((const char *)de.name, MSDOS_DOTDOT, MSDOS_NAME) != 0)
        ++fs->n_files;
    test_file(fs, new, fs->read_test);   /* Bad cluster check */
    return 0;
}

static int subdirs(DOS_FS * fs, DOS_FILE * parent, FDSC ** cp);

static int scan_dir(DOS_FS * fs, DOS_FILE * this, FDSC ** cp)
{
    DOS_FILE **chain;
    int i;
    uint32_t clu_num;

    chain = &this->first;
    i = 0;
    clu_num = FSTART(this, fs);
    new_dir(fs);
    while (clu_num > 0 && clu_num != -1) {
        if (add_file(fs, &chain, this,
                 ClusterStart(fs, clu_num) + (i % fs->cluster_size), cp))
            return 1;
        i += sizeof(DIR_ENT);
        if (!(i % fs->cluster_size))
            if ((clu_num = NextCluster(fs, clu_num)) == 0 || clu_num == -1)
                break;
    }
    LfnCheckOrphaned(fs);
    if (check_dir(fs, &this->first, this->offset))
        return 1;
    if (check_files(fs, this->first))
        return 1;
    return subdirs(fs, this, cp);
}

/**
 * Recursively scan subdirectories of the specified parent directory.
 *
 * @param[inout]    fs      Information about the filesystem
 * @param[in]       parent  Identifies the directory to scan
 * @param[in]       cp
 *
 * @return  0   Success
 * @return  1   Error
 */
static int subdirs(DOS_FS * fs, DOS_FILE * parent, FDSC ** cp)
{
    DOS_FILE *walk;

    for (walk = parent ? parent->first : fs->root; walk; walk = walk->next)
        if (walk->dir_ent.attr & ATTR_DIR)
            if (strncmp((const char *)walk->dir_ent.name, MSDOS_DOT, MSDOS_NAME)
                && strncmp((const char *)walk->dir_ent.name, MSDOS_DOTDOT,
                           MSDOS_NAME))
                if (scan_dir(fs, walk, FileCd(cp, (char *)walk->dir_ent.name)))
                    return 1;
    return 0;
}

/**
 * Scan all directory and file information for errors.
 *
 * @param[inout]    fs      Information about the filesystem
 *
 * @return  0   Success
 * @return  1   Error
 */
int ScanRoot(DOS_FS * fs, struct reg_para *para)
{
    DOS_FILE **chain;
    int i;
    regpara = para;
    if (fs->root)
        free(fs->root);
    fs->root = NULL;
    chain = &fs->root;
    new_dir(fs);
    if (fs->root_cluster) {
        if (add_file(fs, &chain, NULL, 0, &fs->fp_root))
            return 1;
    } else {
        for (i = 0; i < fs->root_entries; i++)
            if (add_file(fs, &chain, NULL, fs->root_start + i * sizeof(DIR_ENT),
                     &fs->fp_root))
                return 1;
    }
    LfnCheckOrphaned(fs);
    if (check_dir(fs, &fs->root, 0))
        return 1;
    if (check_files(fs, fs->root))
        return 1;
    return subdirs(fs, NULL, &fs->fp_root);
}
