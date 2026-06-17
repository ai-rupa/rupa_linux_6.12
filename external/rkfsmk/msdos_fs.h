/* msdos_fs.h - MS-DOS filesystem constants/structures

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

#ifndef _MSDOS_FS_H
#define _MSDOS_FS_H

#include <stdint.h>

#define SECTOR_SIZE 512     /* sector size (bytes) */
#define MSDOS_DPS (SECTOR_SIZE / sizeof(struct msdos_dir_entry))
#define MSDOS_DPS_BITS 4    /* log2(MSDOS_DPS) */
#define MSDOS_DIR_BITS 5    /* log2(sizeof(struct msdos_dir_entry)) */

#define ATTR_NONE 0 /* no attribute bits */
#define ATTR_RO 1   /* read-only */
#define ATTR_HIDDEN 2   /* hidden */
#define ATTR_SYS 4  /* system */
#define ATTR_VOLUME 8   /* volume label */
#define ATTR_DIR 16 /* directory */
#define ATTR_ARCH 32    /* archived */

/* attribute bits that are copied "as is" */
#define ATTR_UNUSED (ATTR_VOLUME | ATTR_ARCH | ATTR_SYS | ATTR_HIDDEN)
#define ATTR_EXT    (ATTR_RO | ATTR_HIDDEN | ATTR_SYS | ATTR_VOLUME)

#define DELETED_FLAG 0xe5   /* marks file as deleted when in name[0] */
#define IS_FREE(n) (!*(n) || *(n) == DELETED_FLAG)

#define MSDOS_NAME 11           /* maximum name length */
#define MSDOS_SLOTS 21
#define MSDOS_DOT ".          "     /* ".", padded to MSDOS_NAME chars */
#define MSDOS_DOTDOT "..         "  /* "..", padded to MSDOS_NAME chars */

#define FAT_LFN_LEN 255
#define NLS_MAX_CHARSET_SIZE 6 /* for UTF-8 */

#define __ismask(x) (_ctype[(int)(unsigned char)(x)])
//#define isalpha(c)  ((__ismask(c)&(_U|_L)) != 0)
#define jiffies raid6_jiffies()

struct msdos_dir_entry {
    uint8_t name[MSDOS_NAME];   /* name including extension */
    uint8_t attr;       /* attribute bits */
    uint8_t lcase;      /* Case for base and extension */
    uint8_t ctime_cs;       /* Creation time, centiseconds (0-199) */
    uint16_t ctime;     /* Creation time */
    uint16_t cdate;     /* Creation date */
    uint16_t adate;     /* Last access date */
    uint16_t starthi;       /* High 16 bits of cluster in FAT32 */
    uint16_t time, date, start; /* time, date and first cluster */
    uint32_t size;      /* file size (in bytes) */
} __attribute__ ((packed));

/* Up to 13 characters of the name */
struct msdos_dir_slot {
    uint8_t    id;      /* sequence number for slot */
    uint8_t    name0_4[10]; /* first 5 characters in name */
    uint8_t    attr;        /* attribute byte */
    uint8_t    reserved;    /* always 0 */
    uint8_t    alias_checksum;  /* checksum for 8.3 alias */
    uint8_t    name5_10[12];    /* 6 more characters in name */
    uint16_t   start;       /* starting cluster number, 0 in long slots */
    uint8_t    name11_12[4];    /* last 2 characters in name */
} __attribute__ ((packed));

#endif /* _MSDOS_FS_H */
