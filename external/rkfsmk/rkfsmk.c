/* mkfs.fat.c - utility to create FAT/MS-DOS filesystems

   Copyright (C) 1991 Linus Torvalds <torvalds@klaava.helsinki.fi>
   Copyright (C) 1992-1993 Remy Card <card@masi.ibp.fr>
   Copyright (C) 1993-1994 David Hudson <dave@humbug.demon.co.uk>
   Copyright (C) 1998 H. Peter Anvin <hpa@zytor.com>
   Copyright (C) 1998-2005 Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de>
   Copyright (C) 2008-2014 Daniel Baumann <mail@daniel-baumann.ch>
   Copyright (C) 2015-2016 Andreas Bombe <aeb@debian.org>

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

/* Description: Utility to allow an MS-DOS filesystem to be created
   under Linux.  A lot of the basic structure of this program has been
   borrowed from Remy Card's "mke2fs" code.

   As far as possible the aim here is to make the "mkfs.fat" command
   look almost identical to the other Linux filesystem make utilties,
   eg bad blocks are still specified as blocks, not sectors, but when
   it comes down to it, DOS is tied to the idea of a sector (512 bytes
   as a rule), and not the block.  For example the boot block does not
   occupy a full cluster.

   Fixes/additions May 1998 by Roman Hodek
   <Roman.Hodek@informatik.uni-erlangen.de>:
   - Atari format support
   - New options -A, -S, -C
   - Support for filesystems > 2GB
   - FAT32 support */

/* Include the header files */

#define _GNU_SOURCE
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <getopt.h>
#include <sys/ioctl.h> 
#include <linux/types.h>

#include "msdos_fs.h"
#include "device_info.h"
#include "rkfsmk.h"
#include "uni_chr.h"

#define FAT_IOCTL_SET_ALIGNSIZE		_IOR('r', 0x14, __u32)
#define FAT_IOCTL_GET_ALIGNSIZE		_IOR('r', 0x15, __u32)
#define FAT_IOCTL_CHK_FORMAT  		_IOR('r', 0x16, __u32)

#define SIZE_1MB    (1024 * 1024)

#define DIR_SIZE    SIZE_1MB
/* Constant definitions */

#define TRUE 1          /* Boolean constants */
#define FALSE 0

#define TEST_BUFFER_BLOCKS 16
#define BLOCK_SIZE         1024
#define HARD_SECTOR_SIZE   512
#define SECTORS_PER_BLOCK ( BLOCK_SIZE / HARD_SECTOR_SIZE )

#define NO_NAME "NO NAME    "

/* Macro definitions */

/* Report a failure message and return a failure error code */

#define die( str ) fatal_error( "%s: " str "\n" )

/* Mark a cluster in the FAT as bad */

#define mark_sector_bad(fmtinfo, sector ) mark_FAT_sector( fmtinfo, sector, FAT_BAD )

/* Compute ceil(a/b) */

static inline int cdiv(int a, int b)
{
    return (a + b - 1) / b;
}

/* FAT values */
//#define FAT_EOF      (fmtinfo->atari_format ? 0x0fffffff : 0x0ffffff8)
#define FAT_EOF      0x0fffffff
#define FAT_BAD      0x0ffffff7

#define MSDOS_EXT_SIGN 0x29 /* extended boot sector signature */
#define MSDOS_FAT12_SIGN "FAT12   " /* FAT12 filesystem signature */
#define MSDOS_FAT16_SIGN "FAT16   " /* FAT16 filesystem signature */
#define MSDOS_FAT32_SIGN "FAT32   " /* FAT32 filesystem signature */

#define BOOT_SIGN 0xAA55    /* Boot sector magic number */

#define MAX_CLUST_12    ((1 << 12) - 16)
#define MAX_CLUST_16    ((1 << 16) - 16)
#define MIN_CLUST_32    65529
/* M$ says the high 4 bits of a FAT32 FAT entry are reserved and don't belong
 * to the cluster number. So the max. cluster# is based on 2^28 */
#define MAX_CLUST_32    ((1 << 28) - 16)

#define FAT12_THRESHOLD 4085

#define OLDGEMDOS_MAX_SECTORS   32765
#define GEMDOS_MAX_SECTORS  65531
#define GEMDOS_MAX_SECTOR_SIZE  (16*1024)

#define BOOTCODE_SIZE       448
#define BOOTCODE_FAT32_SIZE 420

/* __attribute__ ((packed)) is used on all structures to make gcc ignore any
 * alignments */

struct msdos_volume_info {
    uint8_t drive_number;   /* BIOS drive number */
    uint8_t RESERVED;       /* Unused */
    uint8_t ext_boot_sign;  /* 0x29 if fields below exist (DOS 3.3+) */
    uint8_t volume_id[4];   /* Volume ID number */
    uint8_t volume_label[11];   /* Volume label */
    uint8_t fs_type[8];     /* Typically FAT12 or FAT16 */
} __attribute__ ((packed));

struct msdos_boot_sector {
    uint8_t boot_jump[3];   /* Boot strap short or near jump */
    uint8_t system_id[8];   /* Name - can be used to special case
                   partition manager volumes */
    uint8_t sector_size[2]; /* bytes per logical sector */
    uint8_t cluster_size;   /* sectors/cluster */
    uint16_t reserved;      /* reserved sectors */
    uint8_t fats;       /* number of FATs */
    uint8_t dir_entries[2]; /* root directory entries */
    uint8_t sectors[2];     /* number of sectors */
    uint8_t media;      /* media code (unused) */
    uint16_t fat_length;    /* sectors/FAT */
    uint16_t secs_track;    /* sectors per track */
    uint16_t heads;     /* number of heads */
    uint32_t hidden;        /* hidden sectors (unused) */
    uint32_t total_sect;    /* number of sectors (if sectors == 0) */
    union {
        struct {
            struct msdos_volume_info vi;
            uint8_t boot_code[BOOTCODE_SIZE];
        } __attribute__ ((packed)) _oldfat;
        struct {
            uint32_t fat32_length;  /* sectors/FAT */
            uint16_t flags;     /* bit 8: fat mirroring, low 4: active fat */
            uint8_t version[2];     /* major, minor filesystem version */
            uint32_t root_cluster;  /* first cluster in root directory */
            uint16_t info_sector;   /* filesystem info sector */
            uint16_t backup_boot;   /* backup boot sector */
            uint16_t reserved2[6];  /* Unused */
            struct msdos_volume_info vi;
            uint8_t boot_code[BOOTCODE_FAT32_SIZE];
        } __attribute__ ((packed)) _fat32;
    } __attribute__ ((packed)) fstype;
    uint16_t boot_sign;
} __attribute__ ((packed));
#define fat32   fstype._fat32
#define oldfat  fstype._oldfat

struct fat32_fsinfo {
    uint32_t reserved1;     /* Nothing as far as I can tell */
    uint32_t signature;     /* 0x61417272L */
    uint32_t free_clusters; /* Free cluster count.  -1 if unknown */
    uint32_t next_cluster;  /* Most recently allocated cluster.
                 * Unused under Linux. */
    uint32_t reserved2[4];
};

struct user_para_info {
    unsigned int Mark1;
    unsigned int Mark2;
    unsigned int AlignCluste;
    unsigned int StartCluste;
    unsigned int EndCluste;
};

struct cluste_info {
    struct msdos_dir_slot *slots;
    int nr_slots;
    int nr_cluste;
    struct cluste_info *next;
};

struct inode {
    unsigned char name[128];
    uint8_t msdos_name[MSDOS_NAME];
    int isdir;
    unsigned long long size;
    unsigned int num;
    int hidden;
    struct cluste_info *cluste_first;
    struct cluste_info *cluste_last;
    struct inode *child;
    struct inode *next;
};

struct formatinfo {
    unsigned char device_name[128];
    struct device_info devinfo;
    unsigned int align_size;
    int cluste_size;
    int dir_cluste;
    int file_cluste;
    int start_cluste;
    int cluster_count;
    int check;                      /* Default to no readablity checking */
    int atari_format;               /* Use Atari variation of MS-DOS FS format */
    long volume_id;                 /* Volume ID number */
    char volume_name[MSDOS_NAME];   /* Volume name */
    int dev;                        /* FS block device file handle */
    int badblocks;                  /* Number of bad blocks in the filesystem */
    off_t currently_testing;        /* Block currently being tested (if autodetect bad blocks) */
    struct msdos_boot_sector bs;    /* Boot sector data */
    int start_data_sector;          /* Sector number for the start of the data area */
    int start_data_block;           /* Block number for the start of the data area */
    time_t create_time;             /* Creation time */
    unsigned char *fat;             /* File allocation table */
    unsigned alloced_fat_length;    /* # of FAT sectors we can keep in memory */
    unsigned fat_entries;           /* total entries in FAT table (including reserved) */
    uint32_t num_sectors;           /* Total number of sectors in device */
    int sectors_per_cluster;        /* Number of sectors per disk cluster */
    int root_dir_entries;           /* Number of root directory entries */
    char *blank_sector;             /* Blank sector - all zeros */
    int hidden_sectors;             /* Number of hidden sectors */
    int hidden_sectors_by_user;     /* -h option invoked */
    int nr_fats;                    /* Default number of FATs to produce */
    int reserved_sectors;           /* Number of reserved sectors */
    int size_fat;                   /* Size in bits of FAT entries */
    int backup_boot;                /* Sector# of backup boot sector */
    uint64_t blocks;                /* Number of blocks in filesystem */
    int size_fat_by_user;           /* 1 if FAT size user selected */
    unsigned sector_size;           /* Size of a logical sector */
    int sector_size_set;            /* User selected sector size */
    int ignore_full_disk;           /* Ignore warning about 'full' disk devices */
    int drive_number_option;        /* drive number */
    int drive_number_by_user;       /* drive number option invoked */
    int fat_media_byte;             /* media byte in header and starting FAT */
    int malloc_entire_fat;          /* Whether we should malloc() the entire FAT or not */
    int align_structures;           /* Whether to enforce alignment */
    int orphaned_sectors;           /* Sectors that exist in the last block of filesystem */
    int invariant;                  /* Whether to set normally randomized or current time based values to constants */
    unsigned char *info_sector;     /* FAT32 info sector */
    struct inode *root_inode;
    char format_id[8];
    struct user_para_info *user_para;
};

struct shortname_info {
    unsigned char lower: 1,
             upper: 1,
             valid: 1;
};
#define INIT_SHORTNAME_INFO(x)  do {        \
    (x)->lower = 1;             \
    (x)->upper = 1;             \
    (x)->valid = 1;             \
} while (0)

static int creat_root_cluste(struct formatinfo *fmtinfo);
static char *get_slots(struct inode *inode, int cluste);

/* The "boot code" we put into the filesystem... it writes a message and
   tells the user to try again */

unsigned char dummy_boot_jump[3] = { 0xeb, 0x3c, 0x90 };

unsigned char dummy_boot_jump_m68k[2] = { 0x60, 0x1c };

#define MSG_OFFSET_OFFSET 3
char dummy_boot_code[BOOTCODE_SIZE] = "\x0e"    /* push cs */
                                      "\x1f"          /* pop ds */
                                      "\xbe\x5b\x7c"      /* mov si, offset message_txt */
                                      /* write_msg: */
                                      "\xac"          /* lodsb */
                                      "\x22\xc0"          /* and al, al */
                                      "\x74\x0b"          /* jz key_press */
                                      "\x56"          /* push si */
                                      "\xb4\x0e"          /* mov ah, 0eh */
                                      "\xbb\x07\x00"      /* mov bx, 0007h */
                                      "\xcd\x10"          /* int 10h */
                                      "\x5e"          /* pop si */
                                      "\xeb\xf0"          /* jmp write_msg */
                                      /* key_press: */
                                      "\x32\xe4"          /* xor ah, ah */
                                      "\xcd\x16"          /* int 16h */
                                      "\xcd\x19"          /* int 19h */
                                      "\xeb\xfe"          /* foo: jmp foo */
                                      /* message_txt: */
                                      "This is not a bootable disk.  Please insert a bootable floppy and\r\n"
                                      "press any key to try again ... \r\n";

#define MESSAGE_OFFSET 29   /* Offset of message in above code */

/* Global variables - the root of all evil :-) - see these and weep! */
static int verbose = 0;     /* Default to verbose mode off */

/* Function prototype definitions */

static void fatal_error(const char *fmt_string) __attribute__ ((noreturn));
static int mark_FAT_cluster(struct formatinfo *fmtinfo, int cluster, unsigned int value);
static int mark_FAT_sector(struct formatinfo *fmtinfo, int sector, unsigned int value);
static long do_check(struct formatinfo *fmtinfo, char *buffer, int try, off_t current_block);
static int check_blocks(struct formatinfo *fmtinfo);
static int check_mount(char *device_name);
static void establish_params(struct formatinfo *fmtinfo, struct device_info *info);
static int setup_tables(struct formatinfo *fmtinfo);
static int write_tables(struct formatinfo *fmtinfo);

/* The function implementations */

/* Handle the reporting of fatal errors.  Volatile to let gcc know that this doesn't return */

static void fatal_error(const char *fmt_string)
{
    exit(1);            /* The error exit code is 1! */
}

/* Mark the specified cluster as having a particular value */

static int mark_FAT_cluster(struct formatinfo *fmtinfo, int cluster, unsigned int value)
{

    if (cluster < 0 || cluster >= fmtinfo->fat_entries)
        return -1;

    switch (fmtinfo->size_fat) {
    case 12:
        value &= 0x0fff;
        if (((cluster * 3) & 0x1) == 0) {
            fmtinfo->fat[3 * cluster / 2] = (unsigned char)(value & 0x00ff);
            fmtinfo->fat[(3 * cluster / 2) + 1] =
                (unsigned char)((fmtinfo->fat[(3 * cluster / 2) + 1] & 0x00f0)
                                | ((value & 0x0f00) >> 8));
        } else {
            fmtinfo->fat[3 * cluster / 2] =
                (unsigned char)((fmtinfo->fat[3 * cluster / 2] & 0x000f) |
                                ((value & 0x000f) << 4));
            fmtinfo->fat[(3 * cluster / 2) + 1] = (unsigned char)((value & 0x0ff0) >> 4);
        }
        break;

    case 16:
        value &= 0xffff;
        fmtinfo->fat[2 * cluster] = (unsigned char)(value & 0x00ff);
        fmtinfo->fat[(2 * cluster) + 1] = (unsigned char)(value >> 8);
        break;

    case 32:
        value &= 0xfffffff;
        fmtinfo->fat[4 * cluster] = (unsigned char)(value & 0x000000ff);
        fmtinfo->fat[(4 * cluster) + 1] = (unsigned char)((value & 0x0000ff00) >> 8);
        fmtinfo->fat[(4 * cluster) + 2] = (unsigned char)((value & 0x00ff0000) >> 16);
        fmtinfo->fat[(4 * cluster) + 3] = (unsigned char)((value & 0xff000000) >> 24);
        break;

    default:
        return -1;
    }

    return 0;
}

/* Mark a specified sector as having a particular value in it's FAT entry */

static int mark_FAT_sector(struct formatinfo *fmtinfo, int sector, unsigned int value)
{
    int ret = 0;
    int cluster = (sector - fmtinfo->start_data_sector) / (int)(fmtinfo->bs.cluster_size) /
                  (fmtinfo->sector_size / HARD_SECTOR_SIZE) + 2;

    if (sector < fmtinfo->start_data_sector || sector >= fmtinfo->num_sectors)
        return -1;

    ret = mark_FAT_cluster(fmtinfo, cluster, value);

    return ret;
}

/* Perform a test on a block.  Return the number of blocks that could be read successfully */

static long do_check(struct formatinfo *fmtinfo, char *buffer, int try, off_t current_block)
{
    long got;

    if (lseek(fmtinfo->dev, current_block * BLOCK_SIZE, SEEK_SET)    /* Seek to the correct location */
        != current_block * BLOCK_SIZE)
        return -1;

    got = read(fmtinfo->dev, buffer, try * BLOCK_SIZE);  /* Try reading! */
    if (got < 0)
        got = 0;

    //if (got & (BLOCK_SIZE - 1))
    //    printf("Unexpected values in do_check: probably bugs\n");
    got /= BLOCK_SIZE;

    return got;
}

static int check_blocks(struct formatinfo *fmtinfo)
{
    int try, got;
    int i;
    static char blkbuf[BLOCK_SIZE * TEST_BUFFER_BLOCKS];

    if (verbose) {
        printf("search bad blocks ");
        fflush(stdout);
    }
    fmtinfo->currently_testing = 0;

    try = TEST_BUFFER_BLOCKS;
    while (fmtinfo->currently_testing < fmtinfo->blocks) {
        if (fmtinfo->currently_testing + try > fmtinfo->blocks)
                try = fmtinfo->blocks - fmtinfo->currently_testing;
        got = do_check(fmtinfo, blkbuf, try, fmtinfo->currently_testing);
        fmtinfo->currently_testing += got;
        if (got == try) {
            try = TEST_BUFFER_BLOCKS;
            continue;
        } else
            try = 1;
        if (fmtinfo->currently_testing < fmtinfo->start_data_block)
            return -1;

        for (i = 0; i < SECTORS_PER_BLOCK; i++) /* Mark all of the sectors in the block as bad */
            mark_sector_bad(fmtinfo, fmtinfo->currently_testing * SECTORS_PER_BLOCK + i);
        fmtinfo->badblocks++;
        fmtinfo->currently_testing++;
    }

    if (verbose)
        printf("\n");

    if (fmtinfo->badblocks)
        printf("have %d bad block\n", fmtinfo->badblocks);
    return fmtinfo->badblocks;
}

/* Check to see if the specified device is currently mounted - abort if it is */

static int check_mount(char *device_name)
{
    if (IsDevMounted(device_name))
        return -1;

    return 0;
}

/* Establish the geometry and media parameters for the device */

static void establish_params(struct formatinfo *fmtinfo, struct device_info *info)
{
    unsigned int sec_per_track = 63;
    unsigned int heads = 255;
    unsigned int media = 0xf8;
    unsigned int cluster_size = 4;  /* starting point for FAT12 and FAT16 */
    int def_root_dir_entries = 512;

    if (info->size < 512 * 1024 * 1024) {
        /*
         * These values are more or less meaningless, but we can at least
         * use less extreme values for smaller filesystems where the large
         * dummy values signifying LBA only access are not needed.
         */
        sec_per_track = 32;
        heads = 64;
    }

    if (info->type != TYPE_FIXED) {
        /* enter default parameters for floppy disks if the size matches */
        switch (info->size / 1024) {
        case 360:
            sec_per_track = 9;
            heads = 2;
            media = 0xfd;
            cluster_size = 2;
            def_root_dir_entries = 112;
            break;

        case 720:
            sec_per_track = 9;
            heads = 2;
            media = 0xf9;
            cluster_size = 2;
            def_root_dir_entries = 112;
            break;

        case 1200:
            sec_per_track = 15;
            heads = 2;
            media = 0xf9;
            cluster_size = (fmtinfo->atari_format ? 2 : 1);
            def_root_dir_entries = 224;
            break;

        case 1440:
            sec_per_track = 18;
            heads = 2;
            media = 0xf0;
            cluster_size = (fmtinfo->atari_format ? 2 : 1);
            def_root_dir_entries = 224;
            break;

        case 2880:
            sec_per_track = 36;
            heads = 2;
            media = 0xf0;
            cluster_size = 2;
            def_root_dir_entries = 224;
            break;
        }
    }

    if (!fmtinfo->size_fat && info->size >= 512 * 1024 * 1024) {
        //if (verbose)
        //    printf("Auto-selecting FAT32 for large filesystem\n");
        fmtinfo->size_fat = 32;
    }
    if (fmtinfo->size_fat == 32) {
        /*
         * For FAT32, try to do the same as M$'s format command
         * (see http://www.win.tue.nl/~aeb/linux/fs/fat/fatgen103.pdf p. 20):
         * fs size <= 260M: 0.5k clusters
         * fs size <=   8G:   4k clusters
         * fs size <=  16G:   8k clusters
         * fs size <=  32G:  16k clusters
         * fs size >   32G:  32k clusters
         *
         * This only works correctly for 512 byte sectors!
         */
        uint32_t sz_mb = info->size / (1024 * 1024);
        cluster_size =
            sz_mb > 32 * 1024 ? 64 : sz_mb > 16 * 1024 ? 32 : sz_mb >
            8 * 1024 ? 16 : sz_mb > 260 ? 8 : 1;
    }

    if (info->geom_heads > 0) {
        heads = info->geom_heads;
        sec_per_track = info->geom_sectors;
    }

    if (!fmtinfo->hidden_sectors_by_user && info->geom_start >= 0)
        fmtinfo->hidden_sectors = htole32(info->geom_start);

    if (!fmtinfo->root_dir_entries)
        fmtinfo->root_dir_entries = def_root_dir_entries;

    fmtinfo->bs.secs_track = htole16(sec_per_track);
    fmtinfo->bs.heads = htole16(heads);
    fmtinfo->bs.media = media;
    fmtinfo->bs.cluster_size = cluster_size;
}

/*
 * If alignment is enabled, round the first argument up to the second; the
 * latter must be a power of two.
 */
static unsigned int align_object(struct formatinfo *fmtinfo, unsigned int sectors, unsigned int clustsize)
{
    if (fmtinfo->align_structures)
        return (sectors + clustsize - 1) & ~(clustsize - 1);
    else
        return sectors;
}

/* Create the filesystem data tables */

static int setup_tables(struct formatinfo *fmtinfo)
{
    int ret = 0;
    unsigned cluster_count = 0, fat_length;
    struct tm *ctime;
    struct msdos_volume_info *vi =
        (fmtinfo->size_fat == 32 ? &fmtinfo->bs.fat32.vi : &fmtinfo->bs.oldfat.vi);

    if (fmtinfo->atari_format) {
        /* On Atari, the first few bytes of the boot sector are assigned
         * differently: The jump code is only 2 bytes (and m68k machine code
         * :-), then 6 bytes filler (ignored), then 3 byte serial number. */
        fmtinfo->bs.boot_jump[2] = 'm';
        memcpy((char *)fmtinfo->bs.system_id, fmtinfo->format_id, 8);
    } else
        memcpy((char *)fmtinfo->bs.system_id, fmtinfo->format_id, 8);
    if (fmtinfo->sectors_per_cluster)
        fmtinfo->bs.cluster_size = (char)fmtinfo->sectors_per_cluster;

    if (fmtinfo->fat_media_byte)
        fmtinfo->bs.media = (char) fmtinfo->fat_media_byte;

    if (fmtinfo->bs.media == 0xf8)
        vi->drive_number = 0x80;
    else
        vi->drive_number = 0x00;

    if (fmtinfo->drive_number_by_user)
        vi->drive_number = (char) fmtinfo->drive_number_option;

    if (fmtinfo->size_fat == 32) {
        /* Under FAT32, the root dir is in a cluster chain, and this is
         * signalled by bs.dir_entries being 0. */
        fmtinfo->root_dir_entries = 0;
    }

    if (fmtinfo->atari_format) {
        fmtinfo->bs.system_id[5] = (unsigned char)(fmtinfo->volume_id & 0x000000ff);
        fmtinfo->bs.system_id[6] = (unsigned char)((fmtinfo->volume_id & 0x0000ff00) >> 8);
        fmtinfo->bs.system_id[7] = (unsigned char)((fmtinfo->volume_id & 0x00ff0000) >> 16);
    } else {
        vi->volume_id[0] = (unsigned char)(fmtinfo->volume_id & 0x000000ff);
        vi->volume_id[1] = (unsigned char)((fmtinfo->volume_id & 0x0000ff00) >> 8);
        vi->volume_id[2] = (unsigned char)((fmtinfo->volume_id & 0x00ff0000) >> 16);
        vi->volume_id[3] = (unsigned char)(fmtinfo->volume_id >> 24);
    }

    if (!fmtinfo->atari_format) {
        memcpy(vi->volume_label, fmtinfo->volume_name, 11);

        memcpy(fmtinfo->bs.boot_jump, dummy_boot_jump, 3);
        /* Patch in the correct offset to the boot code */
        fmtinfo->bs.boot_jump[1] = ((fmtinfo->size_fat == 32 ?
                                     (char *)&fmtinfo->bs.fat32.boot_code :
                                     (char *)&fmtinfo->bs.oldfat.boot_code) - (char *)&fmtinfo->bs) - 2;

        if (fmtinfo->size_fat == 32) {
            int offset = (char *)&fmtinfo->bs.fat32.boot_code -
                         (char *)&fmtinfo->bs + MESSAGE_OFFSET + 0x7c00;
            if (dummy_boot_code[BOOTCODE_FAT32_SIZE - 1])
                printf("Warning: message too long; truncated\n");
            dummy_boot_code[BOOTCODE_FAT32_SIZE - 1] = 0;
            memcpy(fmtinfo->bs.fat32.boot_code, dummy_boot_code, BOOTCODE_FAT32_SIZE);
            fmtinfo->bs.fat32.boot_code[MSG_OFFSET_OFFSET] = offset & 0xff;
            fmtinfo->bs.fat32.boot_code[MSG_OFFSET_OFFSET + 1] = offset >> 8;
        } else {
            memcpy(fmtinfo->bs.oldfat.boot_code, dummy_boot_code, BOOTCODE_SIZE);
        }
        fmtinfo->bs.boot_sign = htole16(BOOT_SIGN);
    } else {
        memcpy(fmtinfo->bs.boot_jump, dummy_boot_jump_m68k, 2);
    }
    //if (verbose >= 2)
    //    printf("Boot jump code is %02x %02x\n",
    //           fmtinfo->bs.boot_jump[0], fmtinfo->bs.boot_jump[1]);

    if (!fmtinfo->reserved_sectors)
        fmtinfo->reserved_sectors = (fmtinfo->size_fat == 32) ? 32 : 1;
    else {
        if (fmtinfo->size_fat == 32 && fmtinfo->reserved_sectors < 2)
            return -1;
    }
    fmtinfo->bs.reserved = htole16(fmtinfo->reserved_sectors);
    //if (verbose >= 2)
    //    printf("Using %d reserved sectors\n", fmtinfo->reserved_sectors);
    fmtinfo->bs.fats = (char)fmtinfo->nr_fats;
    if (!fmtinfo->atari_format || fmtinfo->size_fat == 32)
        fmtinfo->bs.hidden = htole32(fmtinfo->hidden_sectors);
    else {
        /* In Atari format, hidden is a 16 bit field */
        uint16_t hidden = htole16(fmtinfo->hidden_sectors);
        if (fmtinfo->hidden_sectors & ~0xffff)
            return -1;
        memcpy(&fmtinfo->bs.hidden, &hidden, 2);
    }

    if ((long long)(fmtinfo->blocks * BLOCK_SIZE / fmtinfo->sector_size) + fmtinfo->orphaned_sectors >
        UINT32_MAX) {
        //printf("Warning: target too large, space at end will be left unused\n");
        fmtinfo->num_sectors = UINT32_MAX;
        fmtinfo->blocks = (uint64_t)UINT32_MAX * fmtinfo->sector_size / BLOCK_SIZE;
    } else {
        fmtinfo->num_sectors =
            (long long)(fmtinfo->blocks * BLOCK_SIZE / fmtinfo->sector_size) + fmtinfo->orphaned_sectors;
    }

    if (!fmtinfo->atari_format) {
        unsigned fatdata1216;   /* Sectors for FATs + data area (FAT12/16) */
        unsigned fatdata32; /* Sectors for FATs + data area (FAT32) */
        unsigned fatlength12, fatlength16, fatlength32;
        unsigned maxclust12, maxclust16, maxclust32;
        unsigned clust12, clust16, clust32;
        int maxclustsize;
        unsigned root_dir_sectors = cdiv(fmtinfo->root_dir_entries * 32, fmtinfo->sector_size);

        /*
         * If the filesystem is 8192 sectors or less (4 MB with 512-byte
         * sectors, i.e. floppy size), don't align the data structures.
         */
        if (fmtinfo->num_sectors <= 8192) {
            //if (fmtinfo->align_structures && verbose >= 2)
            //    printf("Disabling alignment due to tiny filesystem\n");

            fmtinfo->align_structures = FALSE;
        }

        if (fmtinfo->sectors_per_cluster)
            fmtinfo->bs.cluster_size = maxclustsize = fmtinfo->sectors_per_cluster;
        else
            /* An initial guess for bs.cluster_size should already be set */
            maxclustsize = 128;

        do {
            fatdata32 = fmtinfo->num_sectors
                        - align_object(fmtinfo, fmtinfo->reserved_sectors, fmtinfo->bs.cluster_size);
            fatdata1216 = fatdata32
                          - align_object(fmtinfo, root_dir_sectors, fmtinfo->bs.cluster_size);

            //if (verbose >= 2)
            //    printf("Trying with %d sectors/cluster:\n", fmtinfo->bs.cluster_size);

            /* The factor 2 below avoids cut-off errors for nr_fats == 1.
             * The "nr_fats*3" is for the reserved first two FAT entries */
            clust12 = 2 * ((long long)fatdata1216 * fmtinfo->sector_size + fmtinfo->nr_fats * 3) /
                      (2 * (int)fmtinfo->bs.cluster_size * fmtinfo->sector_size + fmtinfo->nr_fats * 3);
            fatlength12 = cdiv(((clust12 + 2) * 3 + 1) >> 1, fmtinfo->sector_size);
            fatlength12 = align_object(fmtinfo, fatlength12, fmtinfo->bs.cluster_size);
            /* Need to recalculate number of clusters, since the unused parts of the
             * FATS and data area together could make up space for an additional,
             * not really present cluster. */
            clust12 = (fatdata1216 - fmtinfo->nr_fats * fatlength12) / fmtinfo->bs.cluster_size;
            maxclust12 = (fatlength12 * 2 * fmtinfo->sector_size) / 3;
            if (maxclust12 > MAX_CLUST_12)
                maxclust12 = MAX_CLUST_12;
            //if (verbose >= 2)
            //    printf("FAT12: #clu=%u, fatlen=%u, maxclu=%u, limit=%u\n",
            //           clust12, fatlength12, maxclust12, MAX_CLUST_12);
            if (clust12 > maxclust12 - 2) {
                clust12 = 0;
                //if (verbose >= 2)
                //    printf("FAT12: too much clusters\n");
            }

            clust16 = ((long long)fatdata1216 * fmtinfo->sector_size + fmtinfo->nr_fats * 4) /
                      ((int)fmtinfo->bs.cluster_size * fmtinfo->sector_size + fmtinfo->nr_fats * 2);
            fatlength16 = cdiv((clust16 + 2) * 2, fmtinfo->sector_size);
            fatlength16 = align_object(fmtinfo, fatlength16, fmtinfo->bs.cluster_size);
            /* Need to recalculate number of clusters, since the unused parts of the
             * FATS and data area together could make up space for an additional,
             * not really present cluster. */
            clust16 = (fatdata1216 - fmtinfo->nr_fats * fatlength16) / fmtinfo->bs.cluster_size;
            maxclust16 = (fatlength16 * fmtinfo->sector_size) / 2;
            if (maxclust16 > MAX_CLUST_16)
                maxclust16 = MAX_CLUST_16;
            //if (verbose >= 2)
            //    printf("FAT16: #clu=%u, fatlen=%u, maxclu=%u, limit=%u\n",
            //           clust16, fatlength16, maxclust16, MAX_CLUST_16);
            if (clust16 > maxclust16 - 2) {
                //if (verbose >= 2)
                //    printf("FAT16: too much clusters\n");
                clust16 = 0;
            }
            /* The < 4078 avoids that the filesystem will be misdetected as having a
             * 12 bit FAT. */
            if (clust16 < FAT12_THRESHOLD
                && !(fmtinfo->size_fat_by_user && fmtinfo->size_fat == 16)) {
                //if (verbose >= 2)
                //    printf("FAT16: would be misdetected as FAT12\n");
                clust16 = 0;
            }

            clust32 = ((long long)fatdata32 * fmtinfo->sector_size + fmtinfo->nr_fats * 8) /
                      ((int)fmtinfo->bs.cluster_size * fmtinfo->sector_size + fmtinfo->nr_fats * 4);
            fatlength32 = cdiv((clust32 + 2) * 4, fmtinfo->sector_size);
            fatlength32 = align_object(fmtinfo, fatlength32, fmtinfo->bs.cluster_size);
            /* Need to recalculate number of clusters, since the unused parts of the
             * FATS and data area together could make up space for an additional,
             * not really present cluster. */
            clust32 = (fatdata32 - fmtinfo->nr_fats * fatlength32) / fmtinfo->bs.cluster_size;
            maxclust32 = (fatlength32 * fmtinfo->sector_size) / 4;
            if (maxclust32 > MAX_CLUST_32)
                maxclust32 = MAX_CLUST_32;
            if (clust32 && clust32 < MIN_CLUST_32
                && !(fmtinfo->size_fat_by_user && fmtinfo->size_fat == 32)) {
                clust32 = 0;
                //if (verbose >= 2)
                //    printf("FAT32: not enough clusters (%d)\n", MIN_CLUST_32);
            }
            //if (verbose >= 2)
            //    printf("FAT32: #clu=%u, fatlen=%u, maxclu=%u, limit=%u\n",
            //           clust32, fatlength32, maxclust32, MAX_CLUST_32);
            if (clust32 > maxclust32) {
                clust32 = 0;
                //if (verbose >= 2)
                //    printf("FAT32: too much clusters\n");
            }

            if ((clust12 && (fmtinfo->size_fat == 0 || fmtinfo->size_fat == 12)) ||
                (clust16 && (fmtinfo->size_fat == 0 || fmtinfo->size_fat == 16)) ||
                (clust32 && fmtinfo->size_fat == 32))
                break;

            fmtinfo->bs.cluster_size <<= 1;
        } while (fmtinfo->bs.cluster_size && fmtinfo->bs.cluster_size <= maxclustsize);

        /* Use the optimal FAT size if not specified;
         * FAT32 is (not yet) choosen automatically */
        if (!fmtinfo->size_fat) {
            fmtinfo->size_fat = (clust16 > clust12) ? 16 : 12;
            //if (verbose >= 2)
            //    printf("Choosing %d bits for FAT\n", fmtinfo->size_fat);
        }

        switch (fmtinfo->size_fat) {
        case 12:
            cluster_count = clust12;
            fat_length = fatlength12;
            fmtinfo->bs.fat_length = htole16(fatlength12);
            memcpy(vi->fs_type, MSDOS_FAT12_SIGN, 8);
            break;

        case 16:
            /*
            if (clust16 < FAT12_THRESHOLD) {
                if (fmtinfo->size_fat_by_user) {
                    fprintf(stderr, "WARNING: Not enough clusters for a "
                            "16 bit FAT! The filesystem will be\n"
                            "misinterpreted as having a 12 bit FAT without "
                            "mount option \"fat=16\".\n");
                } else {
                    fprintf(stderr, "This filesystem has an unfortunate size. "
                            "A 12 bit FAT cannot provide\n"
                            "enough clusters, but a 16 bit FAT takes up a little "
                            "bit more space so that\n"
                            "the total number of clusters becomes less than the "
                            "threshold value for\n"
                            "distinction between 12 and 16 bit FATs.\n");
                    die("Make the filesystem a bit smaller manually.");
                }
            }
            */
            cluster_count = clust16;
            fat_length = fatlength16;
            fmtinfo->bs.fat_length = htole16(fatlength16);
            memcpy(vi->fs_type, MSDOS_FAT16_SIGN, 8);
            break;

        case 32:
            if (clust32 < MIN_CLUST_32)
                fprintf(stderr,
                        "WARNING: Not enough clusters for a 32 bit FAT!\n");
            cluster_count = clust32;
            fat_length = fatlength32;
            fmtinfo->bs.fat_length = htole16(0);
            fmtinfo->bs.fat32.fat32_length = htole32(fatlength32);
            memcpy(vi->fs_type, MSDOS_FAT32_SIGN, 8);
            fmtinfo->root_dir_entries = 0;
            break;

        default:
            return -1;
        }

        /* Adjust the reserved number of sectors for alignment */
        fmtinfo->reserved_sectors = align_object(fmtinfo, fmtinfo->reserved_sectors, fmtinfo->bs.cluster_size);
        fmtinfo->bs.reserved = htole16(fmtinfo->reserved_sectors);

        /* Adjust the number of root directory entries to help enforce alignment */
        if (fmtinfo->align_structures) {
            fmtinfo->root_dir_entries = align_object(fmtinfo, root_dir_sectors, fmtinfo->bs.cluster_size)
                                        * (fmtinfo->sector_size >> 5);
        }
    } else {
        unsigned clusters, maxclust, fatdata;

        /* GEMDOS always uses a 12 bit FAT on floppies, and always a 16 bit FAT on
         * hard disks. So use 12 bit if the size of the filesystem suggests that
         * this fs is for a floppy disk, if the user hasn't explicitly requested a
         * size.
         */
        if (!fmtinfo->size_fat)
            fmtinfo->size_fat = (fmtinfo->num_sectors == 1440 || fmtinfo->num_sectors == 2400 ||
                                 fmtinfo->num_sectors == 2880 || fmtinfo->num_sectors == 5760) ? 12 : 16;
        //if (verbose >= 2)
        //    printf("Choosing %d bits for FAT\n", fmtinfo->size_fat);

        /* Atari format: cluster size should be 2, except explicitly requested by
         * the user, since GEMDOS doesn't like other cluster sizes very much.
         * Instead, tune the sector size for the FS to fit.
         */
        fmtinfo->bs.cluster_size = fmtinfo->sectors_per_cluster ? fmtinfo->sectors_per_cluster : 2;
        if (!fmtinfo->sector_size_set) {
            while (fmtinfo->num_sectors > GEMDOS_MAX_SECTORS) {
                fmtinfo->num_sectors >>= 1;
                fmtinfo->sector_size <<= 1;
            }
        }
        //if (verbose >= 2)
        //    printf("Sector size must be %d to have less than %d log. sectors\n",
        //           fmtinfo->sector_size, GEMDOS_MAX_SECTORS);

        /* Check if there are enough FAT indices for how much clusters we have */
        do {
            fatdata = fmtinfo->num_sectors - cdiv(fmtinfo->root_dir_entries * 32, fmtinfo->sector_size) -
                      fmtinfo->reserved_sectors;
            /* The factor 2 below avoids cut-off errors for nr_fats == 1 and
             * size_fat == 12
             * The "2*nr_fats*size_fat/8" is for the reserved first two FAT entries
             */
            clusters =
                (2 *
                 ((long long)fatdata * fmtinfo->sector_size -
                  2 * fmtinfo->nr_fats * fmtinfo->size_fat / 8)) / (2 * ((int)fmtinfo->bs.cluster_size *
                                                                         fmtinfo->sector_size +
                                                                         fmtinfo->nr_fats * fmtinfo->size_fat / 8));
            fat_length = cdiv((clusters + 2) * fmtinfo->size_fat / 8, fmtinfo->sector_size);
            /* Need to recalculate number of clusters, since the unused parts of the
             * FATS and data area together could make up space for an additional,
             * not really present cluster. */
            clusters = (fatdata - fmtinfo->nr_fats * fat_length) / fmtinfo->bs.cluster_size;
            maxclust = (fat_length * fmtinfo->sector_size * 8) / fmtinfo->size_fat;
            //if (verbose >= 2)
            //    printf("ss=%d: #clu=%d, fat_len=%d, maxclu=%d\n",
            //           fmtinfo->sector_size, clusters, fat_length, maxclust);

            /* last 10 cluster numbers are special (except FAT32: 4 high bits rsvd);
             * first two numbers are reserved */
            if (maxclust <=
                (fmtinfo->size_fat == 32 ? MAX_CLUST_32 : (1 << fmtinfo->size_fat) - 0x10)
                && clusters <= maxclust - 2)
                break;
            if (verbose >= 2)
                printf(clusters > maxclust - 2 ?
                       "Too many clusters\n" : "FAT too big\n");

            /* need to increment sector_size once more to  */
            if (fmtinfo->sector_size_set)
                return -1;
            fmtinfo->num_sectors >>= 1;
            fmtinfo->sector_size <<= 1;
        } while (fmtinfo->sector_size <= GEMDOS_MAX_SECTOR_SIZE);

        if (fmtinfo->sector_size > GEMDOS_MAX_SECTOR_SIZE)
            return -1;

        cluster_count = clusters;
        if (fmtinfo->size_fat != 32)
            fmtinfo->bs.fat_length = htole16(fat_length);
        else {
            fmtinfo->bs.fat_length = 0;
            fmtinfo->bs.fat32.fat32_length = htole32(fat_length);
        }
    }

    fmtinfo->bs.sector_size[0] = (char)(fmtinfo->sector_size & 0x00ff);
    fmtinfo->bs.sector_size[1] = (char)((fmtinfo->sector_size & 0xff00) >> 8);

    fmtinfo->bs.dir_entries[0] = (char)(fmtinfo->root_dir_entries & 0x00ff);
    fmtinfo->bs.dir_entries[1] = (char)((fmtinfo->root_dir_entries & 0xff00) >> 8);

    if (fmtinfo->size_fat == 32) {
        /* set up additional FAT32 fields */
        fmtinfo->bs.fat32.flags = htole16(0);
        fmtinfo->bs.fat32.version[0] = 0;
        fmtinfo->bs.fat32.version[1] = 0;
        fmtinfo->bs.fat32.root_cluster = htole32(3);
        fmtinfo->bs.fat32.info_sector = htole16(1);
        if (!fmtinfo->backup_boot)
            fmtinfo->backup_boot = (fmtinfo->reserved_sectors >= 7) ? 6 :
                                   (fmtinfo->reserved_sectors >= 2) ? fmtinfo->reserved_sectors - 1 : 0;
        else {
            if (fmtinfo->backup_boot == 1) {
                ret = -1;
                goto out;
            } else if (fmtinfo->backup_boot >= fmtinfo->reserved_sectors) {
                ret = -1;
                goto out;
            }
        }
        //if (verbose >= 2)
        //    printf("Using sector %d as backup boot sector (0 = none)\n",
        //           fmtinfo->backup_boot);
        fmtinfo->bs.fat32.backup_boot = htole16(fmtinfo->backup_boot);
        memset(&fmtinfo->bs.fat32.reserved2, 0, sizeof(fmtinfo->bs.fat32.reserved2));
    }

    if (fmtinfo->atari_format) {
        /* Just some consistency checks */
        if (fmtinfo->num_sectors >= GEMDOS_MAX_SECTORS)
            return -1;
        //else if (fmtinfo->num_sectors >= OLDGEMDOS_MAX_SECTORS)
        //    printf("Warning: More than 32765 sector need TOS 1.04 "
        //           "or higher.\n");
    }
    if (fmtinfo->num_sectors >= 65536) {
        fmtinfo->bs.sectors[0] = (char)0;
        fmtinfo->bs.sectors[1] = (char)0;
        fmtinfo->bs.total_sect = htole32(fmtinfo->num_sectors);
    } else {
        fmtinfo->bs.sectors[0] = (char)(fmtinfo->num_sectors & 0x00ff);
        fmtinfo->bs.sectors[1] = (char)((fmtinfo->num_sectors & 0xff00) >> 8);
        if (!fmtinfo->atari_format)
            fmtinfo->bs.total_sect = htole32(0);
    }

    if (!fmtinfo->atari_format)
        vi->ext_boot_sign = MSDOS_EXT_SIGN;

    if (!cluster_count) {
        return -1;
        //if (fmtinfo->sectors_per_cluster)    /* If yes, die if we'd spec'd sectors per cluster */
        //    die("Too many clusters for filesystem - try more sectors per cluster");
        //else
        //    die("Attempting to create a too large filesystem");
    }
    fmtinfo->fat_entries = cluster_count + 2;

    /* The two following vars are in hard sectors, i.e. 512 byte sectors! */
    fmtinfo->start_data_sector = (fmtinfo->reserved_sectors + fmtinfo->nr_fats * fat_length +
                                  cdiv(fmtinfo->root_dir_entries * 32, fmtinfo->sector_size)) *
                                 (fmtinfo->sector_size / HARD_SECTOR_SIZE);
    fmtinfo->start_data_block = (fmtinfo->start_data_sector + SECTORS_PER_BLOCK - 1) /
                                SECTORS_PER_BLOCK;

    if (fmtinfo->blocks < fmtinfo->start_data_block + 32) /* Arbitrary undersize filesystem! */
        return -1;

    if (verbose) {
        //printf("dev %s\n", fmtinfo->device_name);
        printf("vi 0x%08lx\n", fmtinfo->volume_id &
               (fmtinfo->atari_format ? 0x00ffffff : 0xffffffff));
        if (strcmp(fmtinfo->volume_name, NO_NAME))
            printf("vn: %s\n", fmtinfo->volume_name);
        printf("hs 0x%04x\n",  fmtinfo->hidden_sectors);
        printf("ss %d\n", fmtinfo->sector_size);
        printf("dn 0x%x\n", (int)(vi->drive_number));
        printf("md 0x%x\n", (int)(fmtinfo->bs.media));
        printf("ns %d\n", fmtinfo->num_sectors);
        printf("rs %u\n", fmtinfo->reserved_sectors);
        
        printf("fats %d\n", (int)(fmtinfo->bs.fats));
        printf("cs %d\n", (int)(fmtinfo->bs.cluster_size));

        printf("fl %d\n", fat_length);
        printf("cc %d\n", cluster_count);
        
        printf("hd %d\n", le16toh(fmtinfo->bs.heads));
        printf("st %d\n", le16toh(fmtinfo->bs.secs_track));

        if (fmtinfo->size_fat != 32) {
            unsigned root_dir_entries =
                fmtinfo->bs.dir_entries[0] + ((fmtinfo->bs.dir_entries[1]) * 256);
            unsigned root_dir_sectors =
                cdiv(root_dir_entries * 32, fmtinfo->sector_size);
            printf("rdn: %u\n\n",
                   fmtinfo->root_dir_entries);
            printf("rds: %u\n\n",
                   root_dir_sectors);
        }

    }

    /* Make the file allocation tables! */

    if (fmtinfo->malloc_entire_fat)
        fmtinfo->alloced_fat_length = fat_length;
    else
        fmtinfo->alloced_fat_length = 1;

    if (fmtinfo->nr_fats != 1) {
        printf("rkfsmk: nr_fats %d != 1, not supported\n", fmtinfo->nr_fats);
        ret = -1;
        goto out;
    }
    if ((fmtinfo->fat = mmap(NULL, fmtinfo->alloced_fat_length * fmtinfo->sector_size,
                             PROT_WRITE | PROT_READ, MAP_SHARED, fmtinfo->dev,
                             fmtinfo->reserved_sectors * fmtinfo->sector_size)) == MAP_FAILED) {
         printf("rkfsmk: mmap fat failed, %d\n", errno);
         ret = -1;
         goto out;
    }

    memset(fmtinfo->fat, 0, fmtinfo->alloced_fat_length * fmtinfo->sector_size);

    if (mark_FAT_cluster(fmtinfo, 0, 0xffffffff) != 0) {
        ret = -1;
        goto out;
    }
    mark_FAT_cluster(fmtinfo, 1, 0xffffffff);
    fmtinfo->fat[0] = (unsigned char)fmtinfo->bs.media;   /* Put media type in first byte! */
    if (fmtinfo->size_fat == 32) {
        /* Mark cluster 2 as EOF (used for root dir) */
        if (mark_FAT_cluster(fmtinfo, 3, FAT_EOF) != 0) {
            ret = -1;
            goto out;
        }
    }

    /* Make the root directory entries */

    fmtinfo->cluste_size = (fmtinfo->size_fat == 32) ?
                           fmtinfo->bs.cluster_size * fmtinfo->sector_size :
                           (((int)fmtinfo->bs.dir_entries[1] * 256 + (int)fmtinfo->bs.dir_entries[0]) *
                            sizeof(struct msdos_dir_entry));

    if (mark_FAT_cluster(fmtinfo, 2, FAT_BAD) != 0) {
        ret = -1;
        goto out;
    }
    if ((fmtinfo->user_para = (struct user_para_info *)malloc(fmtinfo->cluste_size)) == NULL) {
        ret = -1;
        goto out;
    }

    int file_cluste = fmtinfo->root_inode->size / fmtinfo->cluste_size;
    memset(fmtinfo->user_para, 0, fmtinfo->cluste_size);
    if (fmtinfo->align_size > 0) {
        fmtinfo->user_para->Mark1 = 0x5a5a5a5a;
        fmtinfo->user_para->Mark2 = 0xa5a5a5a5;
        fmtinfo->user_para->AlignCluste = fmtinfo->align_size / fmtinfo->cluste_size;
        fmtinfo->user_para->EndCluste = fmtinfo->fat_entries;
        fmtinfo->user_para->StartCluste = fmtinfo->fat_entries - (((fmtinfo->fat_entries - file_cluste) / fmtinfo->user_para->AlignCluste) - 1) * fmtinfo->user_para->AlignCluste;
        fmtinfo->file_cluste = fmtinfo->user_para->StartCluste - file_cluste;
    } else {
        fmtinfo->file_cluste = fmtinfo->fat_entries - file_cluste;
    }
    fmtinfo->dir_cluste = 4;
    if (creat_root_cluste(fmtinfo) != 0) {
        ret = -1;
        goto out;
    }

    if (fmtinfo->size_fat == 32) {
        /* For FAT32, create an info sector */
        struct fat32_fsinfo *info;

        if (!(fmtinfo->info_sector = malloc(fmtinfo->sector_size))) {
            ret = -1;
            goto out;
        }
        memset(fmtinfo->info_sector, 0, fmtinfo->sector_size);
        /* fsinfo structure is at offset 0x1e0 in info sector by observation */
        info = (struct fat32_fsinfo *)(fmtinfo->info_sector + 0x1e0);

        /* Info sector magic */
        fmtinfo->info_sector[0] = 'R';
        fmtinfo->info_sector[1] = 'R';
        fmtinfo->info_sector[2] = 'a';
        fmtinfo->info_sector[3] = 'A';

        /* Magic for fsinfo structure */
        info->signature = htole32(0x61417272);
        /* We've allocated cluster 2 for the root dir. */
        info->free_clusters = htole32(cluster_count - fmtinfo->cluster_count);
        info->next_cluster = htole32(fmtinfo->dir_cluste);

        /* Info sector also must have boot sign */
        *(uint16_t *) (fmtinfo->info_sector + 0x1fe) = htole16(BOOT_SIGN);
    }

    if (!(fmtinfo->blank_sector = malloc(fmtinfo->sector_size))) {
        ret = -1;
        goto out;
    }
    memset(fmtinfo->blank_sector, 0, fmtinfo->sector_size);

out:
    return ret;
}

/* Write the new filesystem's data tables to wherever they're going to end up! */

#define seekto(pos,errstr)                      \
  do {                                  \
    off_t __pos = (pos);                        \
    if (lseek (fmtinfo->dev, __pos, SEEK_SET) != __pos)              \
        return -1;  \
  } while(0)

#define writebuf(buf,size,errstr)           \
  do {                          \
    int __size = (size);                \
    if (write (fmtinfo->dev, buf, __size) != __size)     \
        return -1;    \
  } while(0)

static int write_tables(struct formatinfo *fmtinfo)
{
    int i;
    int x;
    int fat_length;

    fat_length = (fmtinfo->size_fat == 32) ?
                 le32toh(fmtinfo->bs.fat32.fat32_length) : le16toh(fmtinfo->bs.fat_length);

    seekto(0, "start of device");
    /* clear all reserved sectors */
    for (x = 0; x < fmtinfo->reserved_sectors; ++x)
        writebuf(fmtinfo->blank_sector, fmtinfo->sector_size, "reserved sector");
    /* seek back to sector 0 and write the boot sector */
    seekto(0, "boot sector");
    writebuf((char *)&fmtinfo->bs, sizeof(struct msdos_boot_sector), "boot sector");
    /* on FAT32, write the info sector and backup boot sector */
    if (fmtinfo->size_fat == 32) {
        seekto(le16toh(fmtinfo->bs.fat32.info_sector) * fmtinfo->sector_size, "info sector");
        writebuf(fmtinfo->info_sector, 512, "info sector");
        if (fmtinfo->backup_boot != 0) {
            seekto(fmtinfo->backup_boot * fmtinfo->sector_size, "backup boot sector");
            writebuf((char *)&fmtinfo->bs, sizeof(struct msdos_boot_sector),
                     "backup boot sector");
        }
    }
    /* seek to start of FATS and write them all */
    seekto(fmtinfo->reserved_sectors * fmtinfo->sector_size +
           fmtinfo->alloced_fat_length * fmtinfo->sector_size, "first FAT");
    //FIXME: use mmap so skip write fat buf. nr_fats shall be 1.
    for (x = 1; x <= 1 /*fmtinfo->nr_fats*/; x++) {
        int y;
        int blank_fat_length = fat_length - fmtinfo->alloced_fat_length;
        // writebuf(fmtinfo->fat, fmtinfo->alloced_fat_length * fmtinfo->sector_size, "FAT");
        for (y = 0; y < blank_fat_length; y++)
            writebuf(fmtinfo->blank_sector, fmtinfo->sector_size, "FAT");
    }

    writebuf((char *)fmtinfo->user_para, fmtinfo->cluste_size, "user_para");
    writebuf((char *)fmtinfo->root_inode->cluste_first->slots, fmtinfo->cluste_size, "root directory");
    for (i = 4; i < fmtinfo->dir_cluste; i++) {
        char *buff = get_slots(fmtinfo->root_inode, i);
        writebuf((char *)buff, fmtinfo->cluste_size, "directory");
    }

    return 0;
}

static inline uint32_t raid6_jiffies(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static inline unsigned short vfat_replace_char(unsigned short w)
{
    return (w == '[') || (w == ']') || (w == ';') || (w == ',')
           || (w == '+') || (w == '=');
}

static unsigned short vfat_skip_char(unsigned short w)
{
    return (w == '.') || (w == ' ');
}

static inline int to_shortname_char(unsigned char *buf, int buf_size,
                                    unsigned short *src, struct shortname_info *info)
{
    int len;

    if (vfat_skip_char(*src)) {
        info->valid = 0;
        return 0;
    }
    if (vfat_replace_char(*src)) {
        info->valid = 0;
        buf[0] = '_';
        return 1;
    }

    len = Uni2Char(*src, buf);
    if (len <= 0) {
        info->valid = 0;
        buf[0] = '_';
        len = 1;
    } else if (len == 1) {
        unsigned char prev = buf[0];

        if (buf[0] >= 0x7F) {
            info->lower = 0;
            info->upper = 0;
        }

        buf[0] = Char2Upper(buf[0]);
        if (isalpha(buf[0])) {
            if (buf[0] == prev)
                info->lower = 0;
            else
                info->upper = 0;
        }
    } else {
        info->lower = 0;
        info->upper = 0;
    }

    return len;
}

static int vfat_find_form(struct inode *inode, char *msdos_name)
{
    struct inode *tmp = inode->child;

    while (tmp) {
        if (memcmp(tmp->msdos_name, msdos_name, MSDOS_NAME) == 0)
            return 0;

        tmp = tmp->next;
    }

    return -1;
}

static int vfat_create_shortname(struct inode *inode, unsigned short *uname, int ulen,
                                 unsigned char *name_res, unsigned char *lcase)
{
    unsigned short *ip, *ext_start, *end, *name_start;
    unsigned char base[9], ext[4], buf[5], *p;
    unsigned char charbuf[NLS_MAX_CHARSET_SIZE];
    int chl, chi;
    int sz = 0, extlen, baselen, i, numtail_baselen, numtail2_baselen;
    int is_shortname;
    struct shortname_info base_info, ext_info;

    is_shortname = 1;
    INIT_SHORTNAME_INFO(&base_info);
    INIT_SHORTNAME_INFO(&ext_info);

    /* Now, we need to create a shortname from the long name */
    ext_start = end = &uname[ulen];
    while (--ext_start >= uname) {
        if (*ext_start == 0x002E) { /* is `.' */
            if (ext_start == end - 1) {
                sz = ulen;
                ext_start = NULL;
            }
            break;
        }
    }

    if (ext_start == uname - 1) {
        sz = ulen;
        ext_start = NULL;
    } else if (ext_start) {
        /*
         * Names which start with a dot could be just
         * an extension eg. "...test".  In this case Win95
         * uses the extension as the name and sets no extension.
         */
        name_start = &uname[0];
        while (name_start < ext_start) {
            if (!vfat_skip_char(*name_start))
                break;
            name_start++;
        }
        if (name_start != ext_start) {
            sz = ext_start - uname;
            ext_start++;
        } else {
            sz = ulen;
            ext_start = NULL;
        }
    }

    numtail_baselen = 6;
    numtail2_baselen = 2;
    for (baselen = i = 0, p = base, ip = uname; i < sz; i++, ip++) {
        chl = to_shortname_char(charbuf, sizeof(charbuf),
                                ip, &base_info);
        if (chl == 0)
            continue;

        if (baselen < 2 && (baselen + chl) > 2)
            numtail2_baselen = baselen;
        if (baselen < 6 && (baselen + chl) > 6)
            numtail_baselen = baselen;
        for (chi = 0; chi < chl; chi++) {
            *p++ = charbuf[chi];
            baselen++;
            if (baselen >= 8)
                break;
        }
        if (baselen >= 8) {
            if ((chi < chl - 1) || (ip + 1) - uname < sz)
                is_shortname = 0;
            break;
        }
    }
    if (baselen == 0) {
        return -1;
    }

    extlen = 0;
    if (ext_start) {
        for (p = ext, ip = ext_start; extlen < 3 && ip < end; ip++) {
            chl = to_shortname_char(charbuf, sizeof(charbuf),
                                    ip, &ext_info);
            if (chl == 0)
                continue;

            if ((extlen + chl) > 3) {
                is_shortname = 0;
                break;
            }
            for (chi = 0; chi < chl; chi++) {
                *p++ = charbuf[chi];
                extlen++;
            }
            if (extlen >= 3) {
                if (ip + 1 != end)
                    is_shortname = 0;
                break;
            }
        }
    }
    ext[extlen] = '\0';
    base[baselen] = '\0';

    /* Yes, it can happen. ".\xe5" would do it. */
    if (base[0] == DELETED_FLAG)
        base[0] = 0x05;

    /* OK, at this point we know that base is not longer than 8 symbols,
     * ext is not longer than 3, base is nonempty, both don't contain
     * any bad symbols (lowercase transformed to uppercase).
     */

    memset(name_res, ' ', MSDOS_NAME);
    memcpy(name_res, base, baselen);
    memcpy(name_res + 8, ext, extlen);
    *lcase = 0;
    if (is_shortname && base_info.valid && ext_info.valid) {
        //if (vfat_find_form(dir, name_res) == 0)
        //  return -EEXIST;
        return (base_info.upper && ext_info.upper);
    }

    /*
     * Try to find a unique extension.  This used to
     * iterate through all possibilities sequentially,
     * but that gave extremely bad performance.  Windows
     * only tries a few cases before using random
     * values for part of the base.
     */

    if (baselen > 6) {
        baselen = numtail_baselen;
        name_res[7] = ' ';
    }
    name_res[baselen] = '~';
    for (i = 1; i < 10; i++) {
        name_res[baselen + 1] = i + '0';
        if (vfat_find_form(inode, name_res) < 0)
            return 0;
    }

    i = jiffies;
    sz = (jiffies >> 16) & 0x7;
    if (baselen > 2) {
        baselen = numtail2_baselen;
        name_res[7] = ' ';
    }
    name_res[baselen + 4] = '~';
    name_res[baselen + 5] = '1' + sz;
    while (1) {
        snprintf(buf, sizeof(buf), "%04X", i & 0xffff);
        memcpy(&name_res[baselen], buf, 4);
        if (vfat_find_form(inode, name_res) < 0)
            break;
        i -= 11;
    }
    return 0;
}

static int
xlate_to_uni(const unsigned char *name, int len, unsigned char *outname,
             int *longlen, int *outlen)
{
    const unsigned char *ip;
    unsigned char *op;
    int i, fill;
    int charlen;

    for (i = 0, ip = name, op = outname, *outlen = 0;
         i < len && *outlen < FAT_LFN_LEN;
         *outlen += 1) {
        charlen = Char2Uni(ip, (unsigned short*)op);
        if (charlen < 0)
            return -1;
        ip += charlen;
        i += charlen;
        op += 2;
    }
    if (i < len)
        return -1;

    *longlen = *outlen;
    if (*outlen % 13) {
        *op++ = 0;
        *op++ = 0;
        *outlen += 1;
        if (*outlen % 13) {
            fill = 13 - (*outlen % 13);
            for (i = 0; i < fill; i++) {
                *op++ = 0xff;
                *op++ = 0xff;
            }
            *outlen += fill;
        }
    }

    return 0;
}

static inline void fatwchar_to16(uint8_t *dst, uint16_t *src, size_t len)
{
    memcpy(dst, src, len * 2);
}

static inline unsigned char fat_checksum(const uint8_t *name)
{
    unsigned char s = name[0];
    s = (s << 7) + (s >> 1) + name[1];  s = (s << 7) + (s >> 1) + name[2];
    s = (s << 7) + (s >> 1) + name[3];  s = (s << 7) + (s >> 1) + name[4];
    s = (s << 7) + (s >> 1) + name[5];  s = (s << 7) + (s >> 1) + name[6];
    s = (s << 7) + (s >> 1) + name[7];  s = (s << 7) + (s >> 1) + name[8];
    s = (s << 7) + (s >> 1) + name[9];  s = (s << 7) + (s >> 1) + name[10];
    return s;
}

static int build_dot_slots(struct formatinfo *fmtinfo, struct inode *inode)
{
    struct tm *ctime;
    struct msdos_dir_entry *de = (struct msdos_dir_entry *)inode->cluste_first->slots;
    if (!fmtinfo->invariant)
        ctime = localtime(&fmtinfo->create_time);
    else
        ctime = gmtime(&fmtinfo->create_time);

    memcpy(de->name, MSDOS_DOT, MSDOS_NAME);
    de->attr = ATTR_DIR;
    de->lcase = 0;

    de->time = htole16((unsigned short)((ctime->tm_sec >> 1) +
                                        (ctime->tm_min << 5) +
                                        (ctime->tm_hour << 11)));
    de->date =
        htole16((unsigned short)(ctime->tm_mday +
                                 ((ctime->tm_mon + 1) << 5) +
                                 ((ctime->tm_year - 80) << 9)));
    de->ctime_cs = 0;
    de->ctime = de->time;
    de->cdate = de->date;
    de->adate = de->date;
    de->starthi = htole16(inode->cluste_first->nr_cluste >> 16);
    de->start = htole16(inode->cluste_first->nr_cluste);
    de->size = htole32(0);

    de++;

    memcpy(de->name, MSDOS_DOTDOT, MSDOS_NAME);
    de->attr = ATTR_DIR;
    de->lcase = 0;

    de->time = htole16((unsigned short)((ctime->tm_sec >> 1) +
                                        (ctime->tm_min << 5) +
                                        (ctime->tm_hour << 11)));
    de->date =
        htole16((unsigned short)(ctime->tm_mday +
                                 ((ctime->tm_mon + 1) << 5) +
                                 ((ctime->tm_year - 80) << 9)));
    de->ctime_cs = 0;
    de->ctime = de->time;
    de->cdate = de->date;
    de->adate = de->date;
    de->starthi = htole16(0);
    de->start = htole16(0);
    de->size = htole32(0);

    inode->cluste_first->nr_slots += 2;

    return 0;
}

static int build_volume_slots(struct formatinfo *fmtinfo)
{
    if (memcmp(fmtinfo->volume_name, NO_NAME, MSDOS_NAME)) {

        struct tm *ctime;
        struct msdos_dir_entry *de = (struct msdos_dir_entry *)fmtinfo->root_inode->cluste_first->slots;
        if (!fmtinfo->invariant)
            ctime = localtime(&fmtinfo->create_time);
        else
            ctime = gmtime(&fmtinfo->create_time);

        memcpy(de->name, fmtinfo->volume_name, MSDOS_NAME);
        de->attr = ATTR_VOLUME;
        de->lcase = 0;

        de->time = htole16((unsigned short)((ctime->tm_sec >> 1) +
                                            (ctime->tm_min << 5) +
                                            (ctime->tm_hour << 11)));
        de->date =
            htole16((unsigned short)(ctime->tm_mday +
                                     ((ctime->tm_mon + 1) << 5) +
                                     ((ctime->tm_year - 80) << 9)));
        de->ctime_cs = 0;
        de->ctime = de->time;
        de->cdate = de->date;
        de->adate = de->date;
        de->starthi = htole16(0);
        de->start = htole16(0);
        de->size = htole32(0);

        fmtinfo->root_inode->cluste_first->nr_slots += 1;
    }

    return 0;
}

static int vfat_build_slots(struct formatinfo *fmtinfo, struct inode *father, struct inode *inode, struct msdos_dir_slot *slots, int *nr_slots)
{
    struct tm *ctime;
    struct msdos_dir_slot *ps;
    struct msdos_dir_entry *de;
    unsigned char cksum, lcase;
    unsigned char msdos_name[MSDOS_NAME];
    uint16_t *uname = NULL;

    int err, ulen, usize, i;
    loff_t offset;

    *nr_slots = 0;

    uname = malloc(256);
    if (!uname)
        return -ENOMEM;

    err = xlate_to_uni(inode->name, strlen(inode->name), (unsigned char *)uname, &ulen, &usize);
    if (err)
        goto out_free;

    //err = vfat_is_used_badchars(uname, ulen);
    //if (err)
    //    goto out_free;

    err = vfat_create_shortname(father, (unsigned short *)uname, ulen,
                                msdos_name, &lcase);
    memcpy(inode->msdos_name, msdos_name, MSDOS_NAME);
    //printf("%s %s\n", __func__, msdos_name);
    if (err < 0)
        goto out_free;
    else if (err == 1) {
        de = (struct msdos_dir_entry *)slots;
        err = 0;
        goto shortname;
    }

    /* build the entry of long file name */
    cksum = fat_checksum(msdos_name);

    *nr_slots = usize / 13;
    for (ps = slots, i = *nr_slots; i > 0; i--, ps++) {
        ps->id = i;
        ps->attr = ATTR_EXT;
        ps->reserved = 0;
        ps->alias_checksum = cksum;
        ps->start = 0;
        offset = (i - 1) * 13;
        fatwchar_to16(ps->name0_4, uname + offset, 5);
        fatwchar_to16(ps->name5_10, uname + offset + 5, 6);
        fatwchar_to16(ps->name11_12, uname + offset + 11, 2);
    }
    slots[0].id |= 0x40;
    de = (struct msdos_dir_entry *)ps;

shortname:
    /* build the entry of 8.3 alias name */
    (*nr_slots)++;
    memcpy(de->name, msdos_name, MSDOS_NAME);
    de->attr = inode->isdir ? ATTR_DIR : ATTR_ARCH;
    if (inode->hidden)
        de->attr |= ATTR_HIDDEN;
    de->lcase = lcase;

    if (!fmtinfo->invariant)
        ctime = localtime(&fmtinfo->create_time);
    else
        ctime = gmtime(&fmtinfo->create_time);
    de->time = htole16((unsigned short)((ctime->tm_sec >> 1) +
                                        (ctime->tm_min << 5) +
                                        (ctime->tm_hour << 11)));
    de->date =
        htole16((unsigned short)(ctime->tm_mday +
                                 ((ctime->tm_mon + 1) << 5) +
                                 ((ctime->tm_year - 80) << 9)));
    de->ctime_cs = 0;
    de->ctime = de->time;
    de->cdate = de->date;
    de->adate = de->date;
    de->starthi = htole16(inode->cluste_first->nr_cluste >> 16);
    de->start = htole16(inode->cluste_first->nr_cluste);
    de->size = htole32(inode->size);

out_free:
    if (uname)
        free(uname);
    return err;
}

static void free_inode_tree(struct inode *inode)
{
    if (inode) {
        struct cluste_info *cluste_info = inode->cluste_first;
        struct cluste_info *info2free = NULL;
        while (cluste_info) {
            if (cluste_info->slots)
                free(cluste_info->slots);
            info2free = cluste_info;
            cluste_info = cluste_info->next;
            free(info2free);
        }
        if (inode->child)
            free_inode_tree(inode->child);
        if (inode->next)
            free_inode_tree(inode->next);
        free(inode);
    }
}

static int mark_cluster(struct formatinfo *fmtinfo, struct inode *inode)
{
    int ret = 0;
    if (inode) {
        struct cluste_info *cluste_first = inode->cluste_first;
        while (cluste_first) {
            if (cluste_first->next) {
                if (mark_FAT_cluster(fmtinfo, cluste_first->nr_cluste, cluste_first->next->nr_cluste) != 0) {
                    ret = -1;
                    goto out;
                }
            } else {
                if (mark_FAT_cluster(fmtinfo, cluste_first->nr_cluste, FAT_EOF) != 0) {
                    ret = -1;
                    goto out;
                }
            }
            cluste_first = cluste_first->next;
        }
        if (inode->child) {
            ret = mark_cluster(fmtinfo, inode->child);
        }
        if (inode->next) {
            ret = mark_cluster(fmtinfo, inode->next);
        }
    }
out:
    return ret;
}

static char *get_slots(struct inode *inode, int cluste)
{
    char *ret = NULL;
    if (inode) {
        struct cluste_info *cluste_first = inode->cluste_first;
        while (cluste_first) {
            if (cluste_first->nr_cluste == cluste) {
                return (char *)cluste_first->slots;
            }
            cluste_first = cluste_first->next;
        }
        if (inode->child) {
            ret = get_slots(inode->child, cluste);
            if (ret)
                return ret;
        }
        if (inode->next) {
            ret = get_slots(inode->next, cluste);
            if (ret)
                return ret;
        }
    }

    return NULL;
}

static int add_cluste(struct inode *inode, int isdir, int size, int nr_cluste)
{
    struct cluste_info *info = (struct cluste_info *)malloc(sizeof(struct cluste_info));
    memset(info, 0, sizeof(struct cluste_info));
    if (isdir) {
        info->slots = (struct msdos_dir_slot *)malloc(size);
        memset(info->slots, 0, size);
    }
    info->nr_cluste = nr_cluste;
    if (inode->cluste_first) {
        inode->cluste_last->next = info;
        inode->cluste_last = info;
    } else {
        inode->cluste_first = info;
        inode->cluste_last = info;
    }

    return 0;
}

static int add_slots(struct formatinfo *fmtinfo, struct inode *father, struct inode *child)
{
    int i;
    struct msdos_dir_slot *slots;
    unsigned int len;
    int err, nr_slots;
    slots = (struct msdos_dir_slot *)malloc(sizeof(struct msdos_dir_slot) * MSDOS_SLOTS);
    if (!slots)
        printf("alloc slots fail\n");
    memset(slots, 0, sizeof(struct msdos_dir_slot) * MSDOS_SLOTS);
    //printf("father->name = %s, child->name %s, %d\n", father->name, child->name, child->cluste_first->nr_cluste);
    err = vfat_build_slots(fmtinfo, father, child, slots, &nr_slots);
    for (i = 0; i < nr_slots; i++) {
        if (father->cluste_last->nr_slots >= (fmtinfo->cluste_size / sizeof(struct msdos_dir_slot))) {
            fmtinfo->cluster_count++;
            add_cluste(father, father->isdir, fmtinfo->cluste_size, fmtinfo->dir_cluste++);
        }
        memcpy(&father->cluste_last->slots[father->cluste_last->nr_slots], &slots[i], sizeof(struct msdos_dir_slot));
        father->cluste_last->nr_slots++;
    }
    free(slots);

    return err;
}

static int creat_tree_clustes(struct formatinfo *fmtinfo, struct inode *inode)
{
    struct inode *child = inode->child;
    struct inode *next = inode->next;

    if (child) {
        //printf("child->name %s\n", child->name);
        if (child->isdir) {
            int i;
            int num = (child->size + fmtinfo->cluste_size - 1) / fmtinfo->cluste_size;
            if (num <= 0)
                num = 1;
            //printf("%s size = %lld, num = %d\n", child->name, child->size, num);
            for (i = 0; i < num; i++) {
                fmtinfo->cluster_count++;
                add_cluste(child, child->isdir, fmtinfo->cluste_size, fmtinfo->dir_cluste++);
            }
            build_dot_slots(fmtinfo, child);
        } else {
            int i;
            int num = (child->size + fmtinfo->cluste_size - 1) / fmtinfo->cluste_size;
            //printf("%s size = %lld, num = %d\n", child->name, child->size, num);
            for (i = 0; i < num; i++) {
                fmtinfo->cluster_count++;
                add_cluste(child, child->isdir, fmtinfo->cluste_size, fmtinfo->file_cluste++);
            }
        }
        creat_tree_clustes(fmtinfo, child);
    }
    if (next) {
        //printf("next->name %s\n", next->name);
        if (next->isdir) {
            int i;
            int num = (next->size + fmtinfo->cluste_size - 1) / fmtinfo->cluste_size;
            if (num <= 0)
                num = 1;
            //printf("%s size = %lld, num = %d\n", next->name, next->size, num);
            for (i = 0; i < num; i++) {
                fmtinfo->cluster_count++;
                add_cluste(next, next->isdir, fmtinfo->cluste_size, fmtinfo->dir_cluste++);
            }
            build_dot_slots(fmtinfo, next);
        } else {
            int i;
            int num = (next->size + fmtinfo->cluste_size - 1) / fmtinfo->cluste_size;
            if (num <= 0)
                num = 1;
            //printf("%s size = %lld, num = %d\n", next->name, next->size, num);
            for (i = 0; i < num; i++) {
                fmtinfo->cluster_count++;
                add_cluste(next, next->isdir, fmtinfo->cluste_size, fmtinfo->file_cluste++);
            }
        }
        creat_tree_clustes(fmtinfo, next);
    }
    if (child) {
        struct inode *child_next = inode->child->next;
        add_slots(fmtinfo, inode, child);
        while (child_next) {
            add_slots(fmtinfo, inode, child_next);
            child_next = child_next->next;
        }
    }

    return 0;
}

static int creat_root_cluste(struct formatinfo *fmtinfo)
{
    int ret = 0;
    struct inode *inode = fmtinfo->root_inode;
    if (inode) {
        add_cluste(inode, inode->isdir, fmtinfo->cluste_size, 3);
        build_volume_slots(fmtinfo);
        creat_tree_clustes(fmtinfo, inode);
        ret = mark_cluster(fmtinfo, inode);
    }

    return ret;
}

static int get_path_depth(char *path)
{
    int i;
    int num = 0;
    int len = strlen(path);
    for (i = 0; i < len; i++) {
        if (path[i] == '/') {
            num ++;
        }
    }
    if (path[len - 1] == '/')
        num -= 1;
    return num;
}

static int get_dir_depth(char *path, int depth, char *dir)
{
    int i = 0;
    int j = 0;
    int ret = -1;
    int num = 0;
    int len = strlen(path);
    for (i = 0; i < len; i++) {
        if (path[i] == '/') {
            num ++;
        } else {
            if (num == depth) {
                dir[j++] = path[i];
                ret = 0;
            }
        }
    }

    return ret;
}

static struct inode *new_inode(void *handle, struct inode *father_inode, char *name, int isdir, int hidden)
{
    int ulen;
    int usize;
    unsigned char lcase;
    struct formatinfo *fmtinfo = (struct formatinfo*)handle;
    struct inode *inode = (struct inode*)malloc(sizeof(struct inode));
    memset(inode, 0, sizeof(struct inode));
    //if (isdir)
    //    fmtinfo->dir_cluste++;
    inode->isdir = isdir;
    inode->hidden = hidden;
    sprintf(inode->name, "%s", name);

    return inode;
}

static struct inode *add_dir(void *handle, char *path, int hidden, int size)
{
    struct formatinfo *fmtinfo = (struct formatinfo*)handle;
    struct inode *inode = fmtinfo->root_inode;
    int depth = 1;
    char dir[128];
    memset(dir, 0, 128);

    //printf("%s path = %s\n", __func__, path);
    while (get_dir_depth(path, depth, dir) == 0) {
        struct inode *father_inode = inode;
        if (inode->child == NULL) {
            inode->child = new_inode(fmtinfo, father_inode, dir, 1, hidden);
            inode->child->size = size;
            inode = inode->child;
        } else {
            inode = inode->child;
            if (strcmp(inode->name, dir) == 0)
                goto cont;

            while (inode->next) {
                inode = inode->next;
                if (inode) {
                    if (strcmp(inode->name, dir) == 0)
                        goto cont;
                }
            }
            if (inode->next == NULL) {
                inode->next = new_inode(fmtinfo, father_inode, dir, 1, hidden);
                inode->next->size = size;
                inode = inode->next;
                goto cont;
            }
        }
    cont:
        memset(dir, 0, 128);
        depth++;
    }

    return inode;
}

void rkfsmk_add_dir(void *handle, char *path, int hidden, int size)
{
    add_dir(handle, path, hidden, size);
}

int rkfsmk_add_file(void *handle, char *path, char *filename, int hidden, int size)
{
    struct formatinfo *fmtinfo = (struct formatinfo*)handle;
    struct inode *inode = add_dir(fmtinfo, path, hidden, 0);
    struct inode *father_inode = inode;

    fmtinfo->root_inode->size += size;

    if (inode->child == NULL) {
        inode->child = new_inode(fmtinfo, father_inode, filename, 0, hidden);
        inode->child->size = size;
        return 0;
    } else {
        inode = inode->child;
        if (strcmp(inode->name, filename) == 0)
            return -1;

        while (inode->next) {
            inode = inode->next;
            if (inode) {
                if (strcmp(inode->name, filename) == 0)
                    return -1;
            }
        }
        if (inode->next == NULL) {
            inode->next = new_inode(fmtinfo, father_inode, filename, 0, hidden);
            inode->next->size = size;
            return 0;
        }
    }

    return -1;
}

int rkfsmk_create(void **info, char *device_name, char *volume_name, unsigned int align_size)
{
    int ret = 0;
    int dev = -1;
    struct formatinfo *fmtinfo = NULL;
    uint64_t cblocks = 0;
    int blocks_specified = 0;
    struct timeval create_timeval;

    printf("rkfsmk 20220920\n");
    printf("device_name = %s, volume_name = %s\n", device_name, volume_name);
    *info = (void *)fmtinfo;

    if (check_mount(device_name) == -1) {
        ret = -4;
        goto err;
    }

    dev = open(device_name, O_CLOEXEC | O_RDWR);   /* Is it a suitable device to build the FS on? */
    if (dev < 0) {
        ret = -2;
        goto err;
    }

    fmtinfo = (struct formatinfo*)malloc(sizeof(struct formatinfo));
    memset(fmtinfo, 0, sizeof(struct formatinfo));
    if (GetDevInfo(dev, &fmtinfo->devinfo) < 0) {
        ret = -3;
        goto err;
    }

    if (fmtinfo->devinfo.size <= 0) {
        ret = -3;
        goto err;
    }

    fmtinfo->dev = -1;
    sprintf(fmtinfo->device_name, "%s", device_name);
    fmtinfo->root_inode = (struct inode*)malloc(sizeof(struct inode));
    memset(fmtinfo->root_inode, 0, sizeof(struct inode));
    fmtinfo->root_inode->isdir = 1;
    fmtinfo->align_size = align_size;
    fmtinfo->sector_size = 512;
    fmtinfo->malloc_entire_fat = TRUE;
    fmtinfo->align_structures = TRUE;
    fmtinfo->check = FALSE;
    fmtinfo->nr_fats = 1;
    fmtinfo->dev = dev;
    fmtinfo->cluster_count = 2;
    fmtinfo->size_fat_by_user = 1;
    fmtinfo->ignore_full_disk = 1;

    gettimeofday(&create_timeval, NULL);
    fmtinfo->create_time = create_timeval.tv_sec;
    fmtinfo->volume_id = (uint32_t) ((create_timeval.tv_sec << 20) | create_timeval.tv_usec);    /* Default volume ID = creation time, fudged for more uniqueness */

    fmtinfo->sectors_per_cluster = 128;
    verbose = 0;

    if (fmtinfo->devinfo.sector_size > 0) {
        if (fmtinfo->sector_size_set) {
            if (fmtinfo->sector_size < fmtinfo->devinfo.sector_size) {
                fmtinfo->sector_size = fmtinfo->devinfo.sector_size;
                //fprintf(stderr,
                //        "Warning: sector size was set to %d (minimal for this device)\n",
                //        fmtinfo->sector_size);
            }
        } else {
            fmtinfo->sector_size = fmtinfo->devinfo.sector_size;
            fmtinfo->sector_size_set = 1;
        }
    }

    //if (fmtinfo->sector_size > 4096)
    //    fprintf(stderr,
    //            "Warning: sector size %d > 4096 is non-standard, filesystem may not be usable\n",
    //            fmtinfo->sector_size);

    cblocks = fmtinfo->devinfo.size / BLOCK_SIZE;
    fmtinfo->orphaned_sectors = (fmtinfo->devinfo.size % BLOCK_SIZE) / fmtinfo->sector_size;

    if (blocks_specified) {
        if (fmtinfo->blocks != cblocks) {
            //fprintf(stderr, "Warning: block count mismatch: ");
            //fprintf(stderr, "found %llu but assuming %llu.\n",
            //        (unsigned long long)cblocks, (unsigned long long)fmtinfo->blocks);
        }
    } else {
        fmtinfo->blocks = cblocks;
    }

    /*
     * Ignore any 'full' fixed disk devices, if -I is not given.
     */
    if (!fmtinfo->ignore_full_disk && fmtinfo->devinfo.type == TYPE_FIXED &&
        fmtinfo->devinfo.partition == 0) {
        ret = -3;
        goto err;
    }

    if (!fmtinfo->ignore_full_disk && fmtinfo->devinfo.has_children > 0) {
        ret = -3;
        goto err;
    }

    establish_params(fmtinfo, &fmtinfo->devinfo);
    /* Establish the media parameters */

    memset(fmtinfo->volume_name, 0, MSDOS_NAME);
    if (volume_name) {
        int volume_len = strlen(volume_name);
        volume_len = (volume_len > MSDOS_NAME) ? MSDOS_NAME : volume_len;
        if (volume_len)
            memcpy(fmtinfo->volume_name, volume_name, volume_len);
        else 
            memcpy(fmtinfo->volume_name, NO_NAME, MSDOS_NAME);
    } else {
        memcpy(fmtinfo->volume_name, NO_NAME, MSDOS_NAME);
    }

    *info = (void *)fmtinfo;
    printf("disk format sus\n");

    return 0;
err:
    printf("disk format err %d\n", ret);
    if (dev > 0)
        close(dev);
    if (fmtinfo) {
        if (fmtinfo->root_inode) {
            free(fmtinfo->root_inode);
        }
        free(fmtinfo);
        *info = NULL;
    }

    return ret;
}

void rkfsmk_destroy(void *handle)
{
    struct formatinfo *fmtinfo = (struct formatinfo*)handle;
    if (fmtinfo) {
        if (fmtinfo->root_inode) {
            free_inode_tree(fmtinfo->root_inode);
            fmtinfo->root_inode = NULL;
        }
        if (fmtinfo->blank_sector)
            free(fmtinfo->blank_sector);
        if (fmtinfo->info_sector)
            free(fmtinfo->info_sector);
        if (fmtinfo->fat && fmtinfo->fat != MAP_FAILED)
            munmap(fmtinfo->fat, fmtinfo->alloced_fat_length * fmtinfo->sector_size);
        if (fmtinfo->user_para) {
            free(fmtinfo->user_para);
        }
        free(fmtinfo);
    }
}

/* The "main" entry point into the utility - we pick up the options and attempt to process them in some sort of sensible
   way.  In the event that some/all of the options are invalid we need to tell the user so that something can be done! */

int rkfsmk_start(void *handle)
{
    int ret = 0;
    struct formatinfo *fmtinfo = (struct formatinfo*)handle;

    if (!fmtinfo)
        return -1;

    ret = setup_tables(fmtinfo);     /* Establish the filesystem tables */
    if (ret)
        goto out;

    if (fmtinfo->check)          /* Determine any bad block locations and mark them */
        check_blocks(fmtinfo);

    ret = write_tables(fmtinfo);     /* Write the filesystem tables away! */
out:
    if (fmtinfo->dev)
        close(fmtinfo->dev);

    return ret;
}

int rkfsmk_set_format_id(void *handle, char format_id[8])
{
    int ret = 0;
    struct formatinfo *fmtinfo = (struct formatinfo*)handle;

    if (!fmtinfo)
        return -1;

    memcpy(fmtinfo->format_id, format_id, 8);

    return ret;
}


unsigned long long rkfsmk_disk_size_get(void *handle)
{
    struct formatinfo *fmtinfo = (struct formatinfo *)handle;

    return fmtinfo->devinfo.size;
}

int rkfsmk_format(char *device_name, char *volume_name)
{
    int ret = 0;
    struct formatinfo *fmtinfo = NULL;
    ret = rkfsmk_create((void **)&fmtinfo, device_name, volume_name, 0);
    if (ret == 0) {
        char format_id[8] = {'R', 'K', 'F', 'S', 'M', 'K', '0', '1'};
        rkfsmk_set_format_id(fmtinfo, format_id);
        ret = rkfsmk_start(fmtinfo);
        rkfsmk_destroy(fmtinfo);
    }

    return ret;
}

int rkfsmk_format_ex(char *device_name, char *volume_name, char format_id[8])
{
    int ret = 0;
    struct formatinfo *fmtinfo = NULL;
    ret = rkfsmk_create((void **)&fmtinfo, device_name, volume_name, 0);
    if (ret == 0) {
        rkfsmk_set_format_id(fmtinfo, format_id);
        ret = rkfsmk_start(fmtinfo);
        rkfsmk_destroy(fmtinfo);
    }

    return ret;
}

int kernel_get_file_size(char *filename, off_t *size, off_t *space)
{
    int ret;
    struct stat statbuf;

    ret = lstat(filename, &statbuf);

    if (ret == 0) {
        *size = statbuf.st_size;
        *space = statbuf.st_blocks * 512;
    }

    return ret;
}

int kernel_pre_created_file(char *filename, off_t size)
{
    int fd = open(filename, O_CREAT | O_RDWR | O_DIRECT, 0644);
    if (fd) {
        write(fd, NULL, size);
        fsync(fd);
        close(fd);
        return 0;
    }
    return -1;
}

int kernel_set_alignsize(int file_fd, unsigned int alignsize)
{
    return ioctl(file_fd, FAT_IOCTL_SET_ALIGNSIZE, &alignsize);
}

int kernel_chk_format(char *path)
{
	  char *filename;
	  int flag = -1;
	  int fd;
	  int len = strlen(path) + 16;
	  filename = malloc(len);
	  sprintf(filename, "%s/chkfmt", path);
    fd = open(filename, O_CREAT | O_RDWR, 0644);
    if (fd) {
        ioctl(fd, FAT_IOCTL_CHK_FORMAT, &flag);
        close(fd);
        remove(filename);
    }
    free(filename);
    return flag;
}
