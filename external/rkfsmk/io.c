/* io.c - Virtual disk input/output

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

/*
 * Thu Feb 26 01:15:36 CET 1998: Martin Schulze <joey@infodrom.north.de>
 *  Fixed nasty bug that caused every file with a name like
 *  xxxxxxxx.xxx to be treated as bad name that needed to be fixed.
 */

/* FAT32, VFAT, Atari format support, and various fixes additions May 1998
 * by Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de> */

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#include "rkfsck.h"
#include "common.h"
#include "io.h"

int FsOpen(DOS_FS * fs, char *path, int rw)
{
    if ((fs->fd = open(path, rw ? O_RDWR : O_RDONLY)) < 0) {
        return -1;
    }
    fs->changes = fs->last = NULL;
    fs->did_change = 0;
    return 0;
}

/**
 * Read data from the partition, accounting for any pending updates that are
 * queued for writing.
 *
 * @param[in]   pos     Byte offset, relative to the beginning of the partition,
 *                      at which to read
 * @param[in]   size    Number of bytes to read
 * @param[out]  data    Where to put the data read
 */
int FsRead(DOS_FS * fs, off_t pos, int size, void *data)
{
    CHANGE *walk;
    int got;

    if (lseek(fs->fd, pos, 0) != pos) {
        printf("Seek to %lld\n", (long long)pos);
        return 1;
    }
    if ((got = read(fs->fd, data, size)) < 0) {
        printf("Read %d bytes at %lld\n", size, (long long)pos);
        return 1;
    }
    if (got != size) {
        printf("Got %d bytes instead of %d at %lld\n", got, size, (long long)pos);
        return 1;
    }
    for (walk = fs->changes; walk; walk = walk->next) {
        if (walk->pos < pos + size && walk->pos + walk->size > pos) {
            if (walk->pos < pos)
                memcpy(data, (char *)walk->data + pos - walk->pos,
                       min(size, walk->size - pos + walk->pos));
            else
                memcpy((char *)data + walk->pos - pos, walk->data,
                       min(walk->size, size + pos - walk->pos));
        }
    }
    return 0;
}

int FsCheck(DOS_FS * fs, off_t pos, int size)
{
    void *scratch;
    int okay;

    if (lseek(fs->fd, pos, 0) != pos)
        return -1;
    scratch = Malloc(size);
    okay = read(fs->fd, scratch, size) == size;
    free(scratch);
    return okay;
}

void FsWrite(DOS_FS * fs, off_t pos, int size, void *data)
{
    CHANGE *new;
    int did;

    if (write_immed) {
        fs->did_change = 1;
        if (lseek(fs->fd, pos, 0) != pos)
            Die("Seek to %lld", (long long)pos);
        if ((did = write(fs->fd, data, size)) == size)
            return;
        if (did < 0)
            Die("Write %d bytes at %lld", size, (long long)pos);
        Die("Wrote %d bytes instead of %d at %lld", did, size, (long long)pos);
    }
    new = Malloc(sizeof(CHANGE));
    new->pos = pos;
    memcpy(new->data = Malloc(new->size = size), data, size);
    new->next = NULL;
    if (fs->last)
        fs->last->next = new;
    else
        fs->changes = new;
    fs->last = new;
}

static void FsFlush(DOS_FS * fs)
{
    CHANGE *this;
    int size;

    while (fs->changes) {
        this = fs->changes;
        fs->changes = fs->changes->next;
        if (lseek(fs->fd, this->pos, 0) != this->pos) {
            if (fs->log)
                printf("seek to %lld failed, %s\n  Did not write %d bytes.\n",
                       (long long)this->pos, strerror(errno), this->size);
        } else if ((size = write(fs->fd, this->data, this->size)) < 0) {
            if (fs->log)
                printf("writing %d bytes at %lld failed, %s\n", this->size,
                       (long long)this->pos, strerror(errno));
        } else if (size != this->size) {
            if (fs->log)
                printf("wrote %d bytes instead of %d bytes at %lld\n",
                       size, this->size, (long long)this->pos);
        }
        free(this->data);
        free(this);
    }
}

int FsClose(DOS_FS * fs, int write)
{
    CHANGE *next;
    int changed;

    changed = ! !fs->changes;
    if (write)
        FsFlush(fs);
    else
        while (fs->changes) {
            next = fs->changes->next;
            free(fs->changes->data);
            free(fs->changes);
            fs->changes = next;
        }
    if (close(fs->fd) < 0)
        return -1;
    return changed || fs->did_change;
}
