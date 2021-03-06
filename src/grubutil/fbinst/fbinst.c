/*
 *  GRUB Utilities --  Utilities for GRUB Legacy, GRUB2 and GRUB for DOS
 *  Copyright (C) 2009  Bean (bean123ch@gmail.com)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "xdio.h"
#include "keytab.h"

#include "version.h"
#include "fbinst.h"
#include "fb_mbr_rel.h"
#include "fb_mbr_dbg.h"

#ifdef WIN32
#include <windows.h>
#endif

#define BUILD_NUMBER		1

char *progname = "fbinst";
int verbosity = 0;
unsigned char *fb_mbr_data = fb_mbr_rel;
int fb_mbr_size = sizeof (fb_mbr_rel);

#define DEF_FAT32_SIZE		512 * 2048
#define MIN_FAT16_SIZE		8400 + 1

#define DEF_BASE_SIZE		63
#define MAX_LIST_SIZE		((0x80000 - 0x10000) >> 9)
#define DEF_LIST_SIZE		(MAX_LIST_SIZE * 510)
#define MIN_PRI_SIZE		63 * 256
#define MAX_PRI_SIZE		65535
#define MIN_NAND_ALIGN		255

#define GLOB_BUF_SIZE		64

#define uchar	unsigned char
#define uchar2	unsigned short
#define uchar4	unsigned long

#ifdef __GNUC__
#define PACK	__attribute__((packed))
#else
#define PACK
#pragma pack(1)
#endif

struct fb_mbr
{
  uchar jmp_code;
  uchar jmp_ofs;
  uchar boot_code[0x1ab];
  uchar max_sec;		/* 0x1ad  */
  uchar2 lba;			/* 0x1ae  */
  uchar spt;			/* 0x1b0  */
  uchar heads;			/* 0x1b1  */
  uchar2 boot_base;		/* 0x1b2  */
  uchar4 fb_magic;		/* 0x1b4  */
  uchar mbr_table[0x46];	/* 0x1b8  */
  uchar2 end_magic;		/* 0x1fe  */
} PACK;

struct fb_data
{
  uchar2 boot_size;		/* 0x200  */
  uchar2 flags;			/* 0x202  */
  uchar ver_major;		/* 0x204  */
  uchar ver_minor;		/* 0x205  */
  uchar2 list_used;		/* 0x206  */
  uchar2 list_size;		/* 0x208  */
  uchar2 pri_size;		/* 0x20a  */
  uchar4 ext_size;		/* 0x20c  */
} PACK;

struct fb_ar_data
{
  uchar4 ar_magic;		/* 0x200  */
  uchar ver_major;		/* 0x204  */
  uchar ver_minor;		/* 0x205  */
  uchar2 list_used;		/* 0x206  */
  uchar2 list_size;		/* 0x208  */
  uchar2 pri_size;		/* 0x20a  */
  uchar4 ext_size;		/* 0x20c  */
} PACK;

struct fat_bs16
{
  uchar jb[3];		/* Jump boot  */
  char on[8];		/* Oem name  */
  uchar2 bps;		/* Bytes per sector  */
  uchar spc;		/* Sectors per cluster  */
  uchar2 nrs;		/* Number of reserved sector  */
  uchar nf;		/* Number of FATs  */
  uchar2 nrd;		/* Number of root directory entries  */
  uchar2 ts16;		/* 16-bit total sector number  */
  uchar md;		/* Media  */
  uchar2 fz16;		/* 16-Bit FAT size  */
  uchar2 spt;		/* Sectors per track  */
  uchar2 nh;		/* Number of heads  */
  uchar4 nhs;		/* Number of hidden sectors  */
  uchar4 ts32;		/* 32-Bit total sectors  */
  uchar dn;		/* Drive number  */
  uchar r1;		/* Reserved field 1  */
  uchar ebs;		/* Extended boot sigature (0x29)  */
  uchar4 vn;		/* Volume serial number  */
  char vl[11];		/* Volume label  */
  char fs[8];		/* File system  */
  char bc[448];		/* Boot code  */
  uchar2 bss;		/* Boot sector sigature (0xAA55)  */
} PACK;

struct fat_bs32
{
  uchar jb[3];		/* Jump boot  */
  char on[8];		/* Oem name  */
  uchar2 bps;		/* Bytes per sector */
  uchar spc;		/* Sectors per cluster  */
  uchar2 nrs;		/* Number of reserved sector  */
  uchar nf;		/* Number of FATs  */
  uchar2 nrd;		/* Number of root directory entries  */
  uchar2 ts16;		/* 16-bit total sector number  */
  uchar md;		/* Media  */
  uchar2 fz16;		/* 16-Bit FAT size  */
  uchar2 spt;		/* Sectors per track  */
  uchar2 nh;		/* Number of heads  */
  uchar4 nhs;		/* Number of hidden sectors  */
  uchar4 ts32;		/* 32-Bit total sectors  */
  uchar4 fz32;		/* 32-Bit FAT size  */
  uchar2 ef;		/* Extended flags  */
  uchar2 fsv;		/* File system version  */
  uchar4 rc;		/* Root cluster  */
  uchar2 fsi;		/* Sector number of FSINFO structure (1)  */
  uchar2 bbs;		/* Backup boot sector (6)  */
  uchar r1[12];		/* Reserved field 1  */
  uchar dn;		/* Drive number  */
  uchar r2;		/* Reserved field 2  */
  uchar ebs;		/* Extended boot sigature (0x29)  */
  uchar4 vn;		/* Volume serial number  */
  char vl[11];		/* Volume label  */
  char fs[8];		/* File system  */
  uchar bc[420];	/* Boot code  */
  uchar2 bss;		/* Boot sector sigature (0xAA55)  */
} PACK;

struct fbm_file
{
  uchar size;
  uchar flag;
  uchar4 data_start;
  uchar4 data_size;
  time_t data_time;
  char name[0];
} PACK;

struct fbm_text
{
  uchar size;
  uchar type;
  char title[0];
} PACK;

struct fbm_menu
{
  uchar size;
  uchar type;
  uchar2 key;
  uchar sys_type;
  char name[0];
} PACK;

struct fbm_timeout
{
  uchar size;
  uchar type;
  uchar timeout;
} PACK;

#ifdef __GNUC__
#undef PACK
#else
#pragma pack()
#endif

void
info (char *fmt, ...)
{
  if (verbosity > 0)
    {
      va_list ap;

      fprintf (stderr, "%s: info: ", progname);
      va_start (ap, fmt);
      vfprintf (stderr, fmt, ap);
      va_end (ap);
      fputc ('\n', stderr);
      fflush (stderr);
    }
}

void
quit (char *fmt, ...)
{
  va_list ap;

  fprintf (stderr, "%s: error: ", progname);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

void *
xmalloc (int size)
{
  void *p;

  p = malloc (size);
  if (! p)
    quit ("not enough memory");

  return p;
}

void *
xrealloc (void *p, int size)
{
  p = realloc (p, size);
  if (! p)
    quit ("not enough memory");

  return p;
}

#define DOT_MIN_SIZE	(1 << 20)
#define DOT_COUNT	20

static uchar4 dot_size, dot_remain;

void
dot_init (uchar4 total_size)
{
  dot_remain = 0;
  total_size /= DOT_COUNT;
  dot_size = (total_size >= DOT_MIN_SIZE) ? total_size : 0;
}

void
dot_print (uchar4 size)
{
  if (dot_size == 0)
    return;

  dot_remain += size;
  while (dot_remain >= dot_size)
    {
      printf (".");
      dot_remain -= dot_size;
    }
}

void
dot_fini (void)
{
  if (dot_size)
    printf ("\n");
}

void
help (void)
{
  printf ("Usage:\n"
	  "\tfbinst [OPTIONS] DEVICE_OR_FILE COMMANDS [PARAMETERS]\n\n"
	  "Global Options:\n"
	  "  --help,-h\t\tDisplay this message and exit\n"
	  "  --version,-V\t\tPrint version information and exit\n"
	  "  --list,-l\t\tList all disks in system and exit\n"
	  "  --verbose,-v\t\tPrint verbose messages\n"
	  "  --debug,-d\t\tUse the debug version of mbr\n\n"
	  "Commands:\n"
	  "  format\t\tFormat disk\n"
	  "    --raw,-r\t\tFormat with normal layout (not bootable)\n"
	  "    --force,-f\t\tForce the creation of data partition\n"
	  "    --zip,-z\t\tFormat as USB-ZIP\n"
	  "    --fat16\t\tFormat data partition as FAT16\n"
	  "    --fat32\t\tFormat data partition as FAT32\n"
	  "    --align,-a\t\tAlign to cluster boundary\n"
	  "    --nalign,-n NUM\t\tNAND alignment\n"
	  "    --unit-size,-u NUM\tUnit size for FAT16/FAT32 in sectors\n"
	  "    --base,-b NUM\tSet base boot sector\n"
	  "    --size,-s NUM\tSet size of data partition\n"
	  "    --primary,-p NUM\tSet primary data size\n"
	  "    --extended,-e NUM\tSet extended data size\n"
	  "    --list-size,-l NUM\tSet size of file list\n"
	  "    --max-sectors NUM\tSet maximum number of sectors per read\n"
	  "    --chs\t\tForce chs mode\n"
	  "    --archive FILE\tInitialize fb using archive file\n"
	  "  restore\t\tTry to restore fb mbr\n"
	  "  update\t\tUpdate boot code\n"
	  "  sync\t\t\tSynchronize disk information\n"
	  "    --copy-bpb\t\tCopy bpb from the first partition\n"
	  "    --reset-bpb\t\tReset bpb to inital state\n"
	  "    --clear-bpb\t\tClear bpb\n"
	  "    --max-sectors NUM\tSet maximum number of sectors per read\n"
	  "    --chs\t\tForce chs mode\n"
	  "    --zip,-z\t\tFormat as USB-ZIP\n"
	  "  info\t\t\tShow disk information\n"
	  "  clear\t\t\tClear files\n"
	  "  add NAME [FILE]\tAdd/update file item\n"
	  "    --extended,-e\tStore the file in extended data area\n"
	  "    --syslinux,-s\tPatch syslinux boot file\n"
	  "  add-menu NAME FILE\tAdd/update menu file\n"
	  "    --append,-a\t\tAppend to existing menu file\n"
	  "    --string,-s\t\tThe menu items are passed as command argument\n"
	  "  resize NAME SIZE\tResize/create file item\n"
	  "    --extended,-e\tStore the file in extended data area\n"
	  "    --fill,-f NUM\tSet fill character for expansion\n"
	  "  copy OLD NEW\t\tCopy file item\n"
	  "  move OLD NEW\t\tMove file item\n"
	  "  export NAME [FILE]\tExport file item\n"
	  "  remove NAME\t\tRemove file item\n"
	  "  cat NAME\t\tShow the content of text file\n"
	  "  cat-menu NAME\t\tShow the content of menu file\n"
	  "  pack\t\t\tPack free space\n"
	  "  check\t\t\tCheck primary data area for inconsistency\n"
	  "  save FILE\t\tSave to archive file\n"
	  "    --list-size,-l NUM\tSet size of file list\n"
	  "  load FILE\t\tLoad from archive file\n"
	  "  create\t\tCreate archive file\n"
	  "    --primary,-p NUM\tSet primary data size\n"
	  "    --extended,-e NUM\tSet extended data size\n"
	  "    --list-size,-l NUM\tSet size of file list\n");
}

void
list_devs (void)
{
  int i;
  char name[16];

  for (i = 0; i < MAX_DISKS; i++)
    {
      xd_t *xd;

      sprintf (name, "(hd%d)", i);
      xd = xd_open (name, 1);
      if (xd)
	{
	  unsigned long size;
	  int s;
	  char c;
	  struct fb_mbr m;
	  char *mark;

	  size = xd_size (xd);
	  if (size == XD_INVALID_SIZE)
	    goto quit;

	  if (xd_read (xd, (char *) &m, 1))
	    goto quit;

	  mark = ((m.end_magic == 0xaa55) &&
		  (m.fb_magic == FB_MAGIC_LONG) &&
		  (m.lba == 0)) ? " *" : "";

	  if (size >= (3 << 20))
	    {
	      s = (size + (1 << 20)) >> 21;
	      c = 'g';

	    }
	  else
	    {
	      s = (size + (1 << 10)) >> 11;
	      c = 'm';
	    }

	  printf ("%s: %lu (%d%c)%s\n", name, size, s, c, mark);

	quit:
	  xd_close (xd);
	}
    }
}

uchar global_buffer[512 * GLOB_BUF_SIZE];
uchar *fb_list;
uchar4 fb_pri_size;
uchar4 fb_part_ofs;
int fb_total_size;
int fb_boot_base;
int fb_list_start;
int fb_list_sectors;
int fb_list_size;
int fb_list_tail;
int fb_ar_mode;
int fb_ar_size;

void
read_disk (xd_t *xd, void *buf, int size)
{
  if (xd_read (xd, buf, size))
    quit ("xd_read fails at offset %d, size %d", xd->ofs, size);
}

void
write_disk (xd_t *xd, void *buf, int size)
{
  if (xd_write (xd, buf, size))
    quit ("xd_write fails at offset %d, size %d", xd->ofs, size);
}

void
seek_disk (xd_t *xd, unsigned long ofs)
{
  if (xd_seek (xd, ofs))
    quit ("xd_seek fails at offset %l", ofs);
}

void
zero_disk (xd_t *xd, int size)
{
  while (size)
    {
      int n;

      n = (size <= GLOB_BUF_SIZE) ? size : GLOB_BUF_SIZE;
      write_disk (xd, global_buffer, n);
      size -= n;
    }
}

void
copy_disk (xd_t *xd, int new, int old, int size)
{
  while (size)
    {
      int n;

      n = (size <= GLOB_BUF_SIZE) ? size : GLOB_BUF_SIZE;
      seek_disk (xd, old);
      read_disk (xd, global_buffer, n);

      if (new < fb_pri_size)
	{
	  int i;

	  for (i = 0; i < n; i++)
	    *((uchar2 *) &global_buffer[i * 512 + 510]) = new + i;
	}
      seek_disk (xd, new);
      write_disk (xd, global_buffer, n);

      old += n;
      new += n;
      size -= n;
    }
}

void
format_fat16 (xd_t *xd, uchar4 ds, int unit_size, int is_align)
{
  char buf[512];
  uchar4 table[][2]={{8400, 0}, {32680, 2}, {262144, 4},
		     {524288, 8}, {1048576, 16}, {2097152, 32},
		     {4194304, 64}, {0xFFFFFFFF,0}};
  struct fat_bs16 *pbs = (struct fat_bs16 *) &buf[0];
  uchar4 i, j, nf, fz, rd;

  memset(buf, 0, sizeof (buf));
  pbs->jb[0] = 0xEB;
  pbs->jb[1] = 0x3C;
  pbs->jb[2] = 0x90;
  memcpy(pbs->on, "MSWIN4.1", 8);
  pbs->bps = 512;
  pbs->nrs = 1;
  pbs->nf = 2;
  pbs->nrd = (ds < MIN_FAT16_SIZE) ? 0xF0 : ((xd->ofs & 1) ? 0x200 : 0x1F0);
  pbs->md = (ds < MIN_FAT16_SIZE) ? 0xF0 : 0xF8;
  pbs->spt = 63;
  pbs->nh = 255;
  pbs->nhs = xd->ofs;
  pbs->dn = 0x80;
  pbs->ebs = 0x29;
  pbs->vn = 0;
  memcpy(pbs->vl, "NO NAME    ", 11);
  memcpy(pbs->fs, "FAT16   ", 8);
  if (ds < MIN_FAT16_SIZE)
    pbs->fs[4] = '2';

  pbs->bss = 0xAA55;
  if (ds < 65536)
    pbs->ts16 = (uchar2) ds;
  else
    pbs->ts32 = ds;

  if (ds == 2880)
    {
      pbs->spc = 1;
      pbs->fz16 = 9;
      pbs->spt = 18;
      pbs->nh = 2;
    }
  else if (ds == 5760)
    {
      pbs->spc = 2;
      pbs->fz16 = 9;
      pbs->spt = 36;
      pbs->nh = 2;
    }
  else
    {
      if (unit_size)
	pbs->spc = unit_size;
      else
	{
	  i = 0;
	  while (ds > table[i][0])
	    i++;

	  if (! table[i][1])
	    quit ("invalid size of fat16");

	  pbs->spc = (uchar) table[i][1];
	}
      i = ds - (pbs->nrs + ((pbs->nrd * 32) + pbs->bps - 1) / pbs->bps);
      j = (256 * pbs->spc) + pbs->nf;
      pbs->fz16 = (uchar2)((i + (j - 1)) / j);

      if (is_align)
	{
	  uchar4 b, n;

	  b = xd->ofs + pbs->nrs + pbs->fz16 * 2 + (pbs->nrd * 32) / 512;
	  n = (b + pbs->spc - 1) / pbs->spc;
	  pbs->fz16 += (n * pbs->spc - b) / 2;
	}

      i = ds - (pbs->nrs + ((pbs->nrd * 32) + pbs->bps - 1) / pbs->bps);
      i /= pbs->spc;
      if (i >= 65525)
	quit ("unit size %d invalid for fat16", pbs->spc);
    }

  write_disk (xd, buf, 1);
  zero_disk (xd, pbs->nrs - 1);

  nf = pbs->nf;
  fz = pbs->fz16;
  rd = (pbs->nrd * 32 + 511) >> 9;
  j = pbs->fz16 - 1;

  memset(buf, 0, sizeof (buf));
  if (ds < MIN_FAT16_SIZE)
    *((uchar4 *) &buf[0]) = 0xFFFFF8;
  else
    *((uchar4 *) &buf[0]) = 0xFFFFFFF8;

  for (i = 0; i < nf; i++)
    {
      write_disk (xd, buf, 1);
      zero_disk (xd, j);
    }

  zero_disk (xd, rd);
}

void
format_fat32 (xd_t *xd, uchar4 ds, int unit_size, int is_align)
{
  char buf[512 * 3];
  uchar4 table[][2]={{66600, 0}, {532480, 1}, {16777216, 8}, {33554432, 16},
		     {67108864, 32}, {0xFFFFFFFF, 64}};
  struct fat_bs32* pbs = (struct fat_bs32 *) &buf[0];
  uchar4 i, j, nf, spc;

  memset(buf, 0, sizeof (buf));
  pbs->jb[0] = 0xEB;
  pbs->jb[1] = 0x58;
  pbs->jb[2] = 0x90;
  memcpy(pbs->on, "MSWIN4.1", 8);
  pbs->bps = 512;
  pbs->nrs = 32 + (xd->ofs & 1);
  pbs->nf = 2;

  pbs->nrd = 0;
  pbs->md = 0xF8;
  pbs->spt = 63;
  pbs->nh = 255;
  pbs->nhs = xd->ofs;
  pbs->rc = 2;
  pbs->fsi = 1;
  pbs->bbs = 6;
  pbs->dn = 0x80;
  pbs->ebs = 0x29;
  pbs->vn = 0;
  memcpy (pbs->vl, "NO NAME    ", 11);
  memcpy (pbs->fs, "FAT32   ", 8);
  pbs->bss = 0xAA55;

  *((uchar4 *) &buf[0x200]) = 0x41615252;
  *((uchar4 *) &buf[0x3e4]) = 0x61417272;
  *((uchar4 *) &buf[0x3e8]) = 0xFFFFFFFF;
  *((uchar4 *) &buf[0x3ec]) = 0xFFFFFFFF;
  *((uchar2 *) &buf[0x3fe]) = 0xAA55;
  *((uchar2 *) &buf[0x5fe]) = 0xAA55;

  if (unit_size)
    {
      pbs->spc = unit_size;
    }
  else
    {
      i = 0;
      while (ds > table[i][0])
	i++;

      if (table[i][1] == 0)
	quit ("invalid size for fat32\n");

      pbs->spc = table[i][1];
    }

  pbs->ts16 = 0;
  pbs->ts32 = ds;

  i = ds - (uchar4) (pbs->nrs + ((pbs->nrd * 32) + pbs->bps - 1) / pbs->bps);
  j =((256 * pbs->spc) + pbs->nf) >> 1;
  pbs->fz32 = (i + (j - 1)) / j;

  if (is_align)
    {
      uchar4 b, n;

      b = xd->ofs + pbs->nrs + pbs->fz32 * 2;
      n = (b + pbs->spc - 1) / pbs->spc;
      pbs->fz32 += (n * pbs->spc - b) / 2;
    }

  i = ds - (uchar4) (pbs->nrs + ((pbs->nrd * 32) + pbs->bps - 1) / pbs->bps);
  i /= pbs->spc;
  if (i < 65525)
    quit ("unit size %d invalid for fat32", pbs->spc);

  write_disk (xd, buf, 3);
  zero_disk (xd, pbs->bbs - 3);
  write_disk (xd, buf, 3);
  zero_disk (xd, pbs->nrs - pbs->bbs - 3);

  nf = pbs->nf;
  spc = pbs->spc;
  j = pbs->fz32 - 1;
  memset(buf, 0, sizeof (buf));

  *((uchar4 *) &buf[0]) = 0xFFFFFF8;
  *((uchar4 *) &buf[4]) = 0xFFFFFFF;
  *((uchar4 *) &buf[8]) = 0xFFFFFFF;

  for (i = 0; i < nf; i++)
    {
      write_disk (xd, buf, 1);
      zero_disk (xd, j);
    }

  zero_disk (xd, spc);
}

void
lba2chs (uchar4 lba, uchar *data)
{
  uchar4 tmp;

  tmp = (lba / (63 * 255)) & 0x3FF;
  lba = lba % (63 * 255);
  data[0] = lba / 63;
  data[1] = (lba % 63) + 1;
  if (tmp > 255)
    data[1] += (tmp >> 8) * 64;
  data[2] = tmp;
}

uchar4
get_sector_size (char *s)
{
  char *p;
  uchar4 size;

  size = strtoul (s, &p, 0);
  if ((*p == 'k') || (*p == 'K'))
    size <<= 1;
  else if ((*p == 'm') || (*p == 'M'))
    size <<= 11;
  else if ((*p == 'g') || (*p == 'G'))
    size <<= 21;
  else if (*p)
    quit ("invalid subfix %s", p);

  return size;
}

void
sync_mbr (xd_t *xd, uchar *buf, int base, int copy_bpb, int force)
{
  int i;

  if ((force) && (xd_lock (xd)))
    quit ("can\'t lock disk");

  seek_disk (xd, 0);
  for (i = 0; i <= base; i++)
    {
      ((struct fb_mbr *) buf)->lba = i;
      if (i)
	{
	  int j;

	  for (j = 0x1be; j < 0x1fe; j += 16)
	    {
	      uchar4 start, size;

	      if (! buf[j + 4])
		continue;

	      (*((uchar4 *) &buf[j + 8]))--;
	      start = *((uchar4 *) &buf[j + 8]);
	      size = *((uchar4 *) &buf[j + 12]);

	      lba2chs (start, (uchar *) &buf[j + 1]);
	      lba2chs (start + size - 1, (uchar *) &buf[j + 5]);
	    }

	  if (copy_bpb)
	    ((struct fat_bs16 *) buf)->nrs--;
	}

      write_disk (xd, buf, 1);
    }
}

uchar4
get_part_ofs (uchar *buf)
{
  int i;
  uchar4 min;

  min = 0xffffffff;
  for (i = 0x1be; i < 0x1fe; i += 16)
    {
      uchar4 lba;

      if (! buf[i + 4])
	continue;

      lba = *((uchar4 *) &buf[i + 8]);
      if ((lba) && (lba < min))
	min = lba;
    }

  return min;
}

void load_archive (xd_t *xd, int argc, char **argv);
void read_header (xd_t *xd, int allow_ar);
void write_header (xd_t *xd);

void
mem_copy (uchar *d, uchar *s, int size)
{
  int dir = 1;

  if (s == d)
    return;

  if (s < d)
    {
      s += size - 1;
      d += size - 1;
      dir = -1;
    }

  while (size)
    {
      *d = *s;
      s += dir;
      d += dir;
      size--;
    }
}

void
add_mark (uchar *buf, int sectors, int base, int size)
{
  int i;
  uchar *p;

  memset (buf + size, 0, (sectors << 9) - size);
  p = buf + (sectors - 1) * 510;
  for (i = sectors - 1; i >= 0; i--)
    {
      mem_copy (buf + i * 512, p, 510);
      p -= 510;
      *((uchar2 *) (buf + i * 512 + 510)) = base + i;
    }
}

void
remove_mark (uchar *buf, int sectors)
{
  int i;
  uchar *p;

  p = buf;
  for (i = 0; i < sectors - 1; i++)
    {
      p += 510;
      buf += 512;
      mem_copy (p, buf, 510);
    }
}

uchar *
get_ar_header (int hd)
{
  uchar *buf;
  struct fb_ar_data *d1;
  struct fb_data *d2;
  int size;

  if (read (hd, global_buffer, 512) != 512)
    quit ("file read fails");

  if (*((uchar4 *) global_buffer) != FB_AR_MAGIC_LONG)
    quit ("invalid archive file");

  d1 = (struct fb_ar_data *) global_buffer;
  d2 = (struct fb_data *) (fb_mbr_data + 512);
  if ((d1->ver_major != d2->ver_major) ||
      (d1->ver_minor != d2->ver_minor))
    quit ("version number not match");

  size = d1->list_size << 9;
  buf = xmalloc (size);
  if (read (hd, buf, size) != size)
    quit ("file read fails");

  remove_mark (buf, d1->list_size);

  return buf;
}

void
config_mbr (int max_sec, int chs_mode, int is_zip)
{
  struct fb_mbr *m1;

  m1 = (struct fb_mbr *) global_buffer;
  if (! max_sec)
    max_sec = (m1->max_sec & 0x7f);
  if (chs_mode)
    max_sec |= 0x80;
  m1->max_sec = max_sec;

  if (is_zip)
    {
      global_buffer[0x26] = 0x29;
      strcpy ((char *) &global_buffer[3], "MSWIN4.1");
    }
}

void
format_disk (xd_t *xd, int argc, char **argv)
{
  struct fb_mbr *m1, *m2;
  struct fb_data *data;
  int i;
  uchar4 max_size;
  uchar4 part_size = 0;
  int is_force = 0;
  int is_zip = 0;
  int is_raw = 0;
  int is_fat32 = -1;
  int is_align = 0;
  int unit_size = 0;
  int list_size = 0;
  int base = DEF_BASE_SIZE;
  int pri_size = 0;
  int ext_size = 0;
  int total_size = 0;
  int max_sec = 0;
  int chs_mode = 0;
  int nand_align = MIN_NAND_ALIGN;
  char *ar_file = 0;

  for (i = 0; i < argc; i++)
    {
      if (argv[i][0] != '-')
	break;

      if (! strcmp (argv[i], "--size") ||
	  ! strcmp (argv[i], "-s"))
	{

	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  part_size = get_sector_size (argv[i]);
	}
      else if (! strcmp (argv[i], "--primary") ||
	       ! strcmp (argv[i], "-p"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  pri_size = get_sector_size (argv[i]);
	  if (pri_size < MIN_PRI_SIZE)
	    quit ("primary data size too small");
	  if (pri_size > MAX_PRI_SIZE)
	    quit ("primary data size too large");
	}
      else if (! strcmp (argv[i], "--extended") ||
	       ! strcmp (argv[i], "-e"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  ext_size = get_sector_size (argv[i]);
	}
      else if (! strcmp (argv[i], "--base") ||
	       ! strcmp (argv[i], "-b"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  base = strtoul (argv[i], 0, 0);
	}
      else if (! strcmp (argv[i], "--list-size") ||
	       ! strcmp (argv[i], "-l"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  list_size = strtoul (argv[i], 0, 0);
	}
      else if (! strcmp (argv[i], "--force") ||
	       ! strcmp (argv[i], "-f"))
	{
	  is_force = 1;
	}
      else if (! strcmp (argv[i], "--zip") ||
	       ! strcmp (argv[i], "-z"))
	{
	  is_zip = 1;
	}
      else if (! strcmp (argv[i], "--raw") ||
	       ! strcmp (argv[i], "-r"))
	{
	  is_raw = 1;
	}
      else if (! strcmp (argv[i], "--align") ||
	       ! strcmp (argv[i], "-a"))
	{
	  is_align = 1;
	}
      else if (! strcmp (argv[i], "--fat16"))
	{
	  is_fat32 = 0;
	}
      else if (! strcmp (argv[i], "--fat32"))
	{
	  is_fat32 = 1;
	}
      else if (! strcmp (argv[i], "--archive"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  ar_file = argv[i];
	}
      else if (! strcmp (argv[i], "--nalign") ||
	       ! strcmp (argv[i], "-n"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  nand_align = strtoul (argv[i], 0, 0) - 1;
	  if ((nand_align < MIN_NAND_ALIGN) ||
	      (((nand_align + 1) & nand_align) != 0))
	    quit ("invalid alignment value");
	}
      else if (! strcmp (argv[i], "--unit-size") ||
	       ! strcmp (argv[i], "-u"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  unit_size = strtoul (argv[i], 0, 0);
	}
      else if (! strcmp (argv[i], "--max-sectors"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  max_sec = strtoul (argv[i], 0, 0);
	  if ((max_sec < 0) || (max_sec > 127))
	    quit ("invalid max sectors value");
	}
      else if (! strcmp (argv[i], "--chs"))
	{
	  chs_mode = 1;
	}
      else
	quit ("invalid option %s for format", argv[i]);
    }

  data = (struct fb_data *) &fb_mbr_data[512];

  if (ar_file)
    {
      int hd;
      struct fb_ar_data *ar_data;

      hd = open (ar_file, O_RDONLY | O_BINARY);
      if (hd < 0)
	quit ("can\'t open file %s", ar_file);

      free (get_ar_header (hd));
      ar_data = (struct fb_ar_data *) global_buffer;
      if (! pri_size)
	pri_size = ar_data->pri_size;

      if (! ext_size)
	ext_size = ar_data->ext_size;

      if (! list_size)
	list_size = ar_data->list_size * 510;

      close (hd);
    }

  if (! pri_size)
    pri_size = MIN_PRI_SIZE;

  if (! list_size)
    list_size = DEF_LIST_SIZE;

  list_size = (list_size + 509) / 510;
  if (list_size > MAX_LIST_SIZE)
    quit ("list too large");

  max_size = xd_size (xd);
  if (max_size == XD_INVALID_SIZE)
    quit ("can\'t get size");

  total_size = (is_raw) ? base :
    ((pri_size + ext_size + nand_align) & ~nand_align);
  if (total_size >= max_size)
    quit ("the disk is too small");

  max_size -= total_size;
  if ((! part_size) || (part_size > max_size))
    part_size = max_size;

  if (is_fat32 == -1)
    is_fat32 = (part_size >= DEF_FAT32_SIZE);

  if (is_raw)
    {
      if (! is_force)
	quit ("--raw would destroy existing partiiton layout,\n"
	      "use --force if you want to continue.");

      if (xd_lock (xd))
	quit ("can\'t lock disk");

      if (base)
	{
	  uchar4 part_start;

	  global_buffer[0x1be] = 0x80;
	  *((uchar2 *) &global_buffer[0x1fe]) = 0xaa55;

	  part_start = base;
	  lba2chs (part_start, &global_buffer[0x1bf]);
	  global_buffer[0x1c2] = (is_fat32) ? 0xc : 0xe;
	  lba2chs (part_start + part_size - 1, &global_buffer[0x1c3]);
	  *((uchar4 *) &global_buffer[0x1c6]) = part_start;
	  *((uchar4 *) &global_buffer[0x1ca]) = part_size;

	  write_disk (xd, global_buffer, 1);

	  memset (global_buffer, 0, 512);
	  if (base > 1)
	    zero_disk (xd, base - 1);
	}

      if (is_fat32)
	format_fat32 (xd, part_size, unit_size, is_align);
      else
	format_fat16 (xd, part_size, unit_size, is_align);

      return;
    }

  if (! is_force)
    {
      uchar4 min;

      read_disk (xd, global_buffer, 1);
      seek_disk (xd, 0);

      min = get_part_ofs (global_buffer);
      if (min == 0xffffffff)
	is_force = 1;
      else
	{
	  if (min < pri_size + ext_size)
	    quit ("offset of data partition too small,\n"
		  "use --force to recreate disk layout");

	  if (! ext_size)
	    ext_size = min - pri_size;
	}
    }

  if (is_force)
    {
      memset (&global_buffer, 0, 512);

      global_buffer[0x1be] = 0x80;
      lba2chs (total_size, &global_buffer[0x1bf]);
      global_buffer[0x1c2] = (is_fat32) ? 0xc : 0xe;
      lba2chs (total_size + part_size - 1, &global_buffer[0x1c3]);
      *((uchar4 *) &global_buffer[0x1c6]) = total_size;
      *((uchar4 *) &global_buffer[0x1ca]) = part_size;
    }

  m1 = (struct fb_mbr *) global_buffer;
  m2 = (struct fb_mbr *) fb_mbr_data;

  memcpy (global_buffer, fb_mbr_data, OFS_mbr_table);

  m1->end_magic = 0xaa55;
  m1->boot_base = base;

  config_mbr (max_sec, chs_mode, is_zip);
  sync_mbr (xd, global_buffer, base, 0, is_force);

  i = (fb_mbr_size - 512 + 509) / 510;
  memcpy (&global_buffer[512], &fb_mbr_data[512], fb_mbr_size - 512);
  add_mark (&global_buffer[512], i, base + 1, fb_mbr_size - 512);

  data = (struct fb_data *) &global_buffer[512];
  data->pri_size = pri_size;
  data->ext_size = ext_size;
  data->list_used = 1;
  data->list_size = list_size;
  data->boot_size = i;
  write_disk (xd, &global_buffer[512], i);
  memset (global_buffer, 0, sizeof (global_buffer));

  i += base + 1;
  while (i < pri_size)
    {
      int j, n;

      n = pri_size - i;
      if (n > GLOB_BUF_SIZE)
	n = GLOB_BUF_SIZE;

      for (j = 0; j < n; j++)
	*((uchar2 *) &global_buffer[j * 512 + 510]) = i + j;

      write_disk (xd, global_buffer, n);
      i += n;
    }

  if (is_force)
    {
      seek_disk (xd, total_size);
      memset (global_buffer, 0, sizeof (global_buffer));
      if (is_fat32)
	format_fat32 (xd, part_size, unit_size, is_align);
      else
	format_fat16 (xd, part_size, unit_size, is_align);
    }

  if (ar_file)
    {
      char * v[1];

      seek_disk (xd, 0);
      read_header (xd, 0);
      v[0] = ar_file;
      load_archive (xd, 1, v);
      write_header (xd);
    }
}

void
restore_disk (xd_t *xd)
{
  struct fb_mbr *mbr;
  int i;

  mbr = (struct fb_mbr *) global_buffer;
  for (i = 0; i <= DEF_BASE_SIZE; i++)
    {
      read_disk (xd, (char *) mbr, 1);
      if ((mbr->end_magic == 0xaa55) && (mbr->fb_magic == FB_MAGIC_LONG) &&
	  (mbr->lba == (uchar) i))
	break;
    }

  if (i > DEF_BASE_SIZE)
    quit ("can\'t find fb mbr");

  if (i)
    {
      seek_disk (xd, 0);
      read_disk (xd, &global_buffer[512], 1);
      memcpy (&mbr->mbr_table, &global_buffer[512 + OFS_mbr_table],
	      0x200 - OFS_mbr_table);
      sync_mbr (xd, (uchar *) mbr, i - 1, 0, 0);
    }
}

uchar4
get_ar_size ()
{
  int last_ofs, ofs;

  ofs = 0;
  last_ofs = -1;
  while (fb_list[ofs])
    {
      last_ofs = ofs;
      ofs += fb_list[ofs] + 2;
    }

  if (last_ofs >= 0)
    {
      struct fbm_file *m;
      int n;

      m = (struct fbm_file *) (fb_list + last_ofs);
      n = (m->data_start >= fb_pri_size) ? 512 : 510;
      return m->data_start + (m->data_size + n - 1) / n;
    }
  else
    return fb_list_start + fb_list_sectors;
}

void
read_header (xd_t *xd, int allow_ar)
{
  struct fb_mbr *m1, *m2;
  struct fb_data *d1, *d2;

  read_disk (xd, global_buffer, 1);

  m1 = (struct fb_mbr *) global_buffer;
  m2 = (struct fb_mbr *) fb_mbr_data;
  d1 = (struct fb_data *) global_buffer;
  memcpy (&global_buffer[512], global_buffer, 512);
  if (m1->fb_magic != m2->fb_magic)
    {
      if (((struct fb_ar_data *) global_buffer)->ar_magic == FB_AR_MAGIC_LONG)
	{
	  if (! allow_ar)
	    quit ("this command can\'t work with archive");

	  fb_boot_base = -1;
	  fb_list_start = 1;
	  fb_ar_mode = 1;
	}
      else
	{
	  seek_disk (xd, DEF_BASE_SIZE);
	  read_disk (xd, global_buffer, 1);
	  quit ((m1->fb_magic != m2->fb_magic) ? "fb mbr not initialized" :
		"fb mbr corrupted");
	}
    }
  else
    {
      fb_boot_base = m1->boot_base;
      fb_part_ofs = *((uchar4 *) &global_buffer[0x1c6]);
      seek_disk (xd, fb_boot_base + 1);
      read_disk (xd, global_buffer, 1);
      fb_list_start = fb_boot_base + 1 + d1->boot_size;
    }

  fb_list_sectors = d1->list_size;
  fb_list = xmalloc (fb_list_sectors << 9);

  seek_disk (xd, fb_list_start);
  read_disk (xd, fb_list, fb_list_sectors);

  d2 = (struct fb_data *) (fb_mbr_data + 512);

  if ((d1->ver_major != d2->ver_major) ||
      (d1->ver_minor != d2->ver_minor))
    quit ("version number not match");

  if (fb_ar_mode)
    {
      fb_pri_size = 1 + fb_list_sectors;
      fb_total_size = FB_AR_MAX_SIZE;
    }
  else
    {
      fb_pri_size = d1->pri_size;
      fb_total_size = d1->pri_size + d1->ext_size;
    }

  remove_mark (fb_list, fb_list_sectors);
  fb_list_size = fb_list_sectors * 510;
  fb_list_tail = 0;

  while (fb_list[fb_list_tail])
    {
      fb_list_tail += fb_list[fb_list_tail] + 2;

      if (fb_list_tail >= fb_list_size)
	quit ("invalid file list");
    }

  if (fb_ar_mode)
    fb_ar_size = get_ar_size ();
}

void
write_header (xd_t *xd)
{
  struct fb_data *data;
  int ar_size;

  ar_size = (fb_ar_mode) ? get_ar_size () : 0;
  add_mark (fb_list, fb_list_sectors, fb_list_start, fb_list_tail);

  seek_disk (xd, fb_boot_base + 1);
  read_disk (xd, global_buffer, 1);
  data = (struct fb_data *) global_buffer;
  data->list_used = (fb_list_tail / 510) + 1;
  seek_disk (xd, fb_boot_base + 1);
  write_disk (xd, global_buffer, 1);

  seek_disk (xd, fb_list_start);
  write_disk (xd, fb_list, fb_list_sectors);

#ifdef WIN32
  if (fb_ar_mode)
    {
      if ((ar_size < fb_ar_size) && ((xd->flg & XDF_FILE) != 0) &&
	  ((xd->flg & XDF_DISK) == 0))
	{
	  seek_disk (xd, ar_size);
	  SetEndOfFile ((HANDLE) xd->num);
	}
    }
#endif
}

void
update_header (xd_t *xd)
{
  uchar4 i;
  struct fb_mbr *m2;
  struct fb_data *d2;
  int boot_size;

  m2 = (struct fb_mbr *) fb_mbr_data;
  d2 = (struct fb_data *) &fb_mbr_data[512];

  boot_size = (fb_mbr_size - 512 + 509) / 510;
  if (fb_boot_base + 1 + boot_size > fb_list_start)
    quit ("not enough space, you need to use format command");

  seek_disk (xd, 0);
  for (i = 0; i <= fb_boot_base; i++)
    {
      int ofs;
      struct fb_mbr *m1;

      read_disk (xd, global_buffer, 1);
      ofs = m2->jmp_ofs + 2;
      memcpy (&global_buffer[ofs], &fb_mbr_data[ofs],
	      sizeof (m2->boot_code) + 2 - ofs);

      m1 = (struct fb_mbr *) global_buffer;
      m1->jmp_ofs = m2->jmp_ofs;

      seek_disk (xd, i);
      write_disk (xd, global_buffer, 1);
    }

  memset (global_buffer, 0, sizeof (global_buffer));
  read_disk (xd, global_buffer, 1);
  memcpy (&global_buffer[sizeof (struct fb_data)],
	  &fb_mbr_data[512 + sizeof (struct fb_data)],
	  fb_mbr_size - 512 - sizeof (struct fb_data));

  add_mark (global_buffer, boot_size, fb_boot_base + 1, fb_mbr_size - 512);
  seek_disk (xd, fb_boot_base + 1);
  write_disk (xd, global_buffer, boot_size);
}

void
sync_disk (xd_t *xd, int argc, char **argv)
{
  uchar4 start;
  char buf[512];
  int copy_bpb = -1;
  int bpb_size = 0;
  int i, jmp_ofs;
  int is_zip = 0;
  int max_sec = 0;
  int chs_mode = 0;

  for (i = 0; i < argc; i++)
    {
      if (argv[i][0] != '-')
	break;

      if (! strcmp (argv[i], "--copy-bpb"))
	{
	  copy_bpb = 2;
	}
      else if (! strcmp (argv[i], "--reset-bpb"))
	{
	  copy_bpb = 1;
	}
      else if (! strcmp (argv[i], "--clear-bpb"))
	{
	  copy_bpb = 0;
	}
      else if (! strcmp (argv[i], "--bpb-size"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  bpb_size = strtoul (argv[i], 0, 0);
	}
      else if (! strcmp (argv[i], "--zip") ||
	       ! strcmp (argv[i], "-z"))
	{
	  is_zip = 1;
	}
      else if (! strcmp (argv[i], "--max-sectors"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  max_sec = strtoul (argv[i], 0, 0);
	  if ((max_sec < 0) || (max_sec > 127))
	    quit ("invalid max sectors value");
	}
      else if (! strcmp (argv[i], "--chs"))
	{
	  chs_mode = 1;
	}
      else
	quit ("invalid option %s for sync", argv[i]);
    }

  seek_disk (xd, 0);
  read_disk (xd, global_buffer, 1);
  start = get_part_ofs (global_buffer);
  seek_disk (xd, start);
  read_disk (xd, buf, 1);

  jmp_ofs = global_buffer[1];
  if (copy_bpb == 2)
    {
      struct fat_bs16 *bs;
      uchar4 ts;

      bs = (struct fat_bs16 *) global_buffer;
      memcpy (&global_buffer[2], &buf[2], jmp_ofs);
      bs->nrs += start;
      bs->nhs = 0;
      ts = ((struct fat_bs16 *) buf)->ts16;
      if (! ts)
	ts = ((struct fat_bs16 *) buf)->ts32;
      ts += start;
      bs->ts16 = 0;
      bs->ts32 = 0;
      if (ts < 65536)
	bs->ts16 = (uchar2) ts;
      else
	bs->ts32 = ts;
      is_zip = 0;
    }
  else if (copy_bpb >= 0)
    {
      memset (&global_buffer[2], 0, jmp_ofs);
      if (copy_bpb)
	{
	  global_buffer[0x10] = 2;
	  global_buffer[0x18] = 0x3f;
	  global_buffer[0x1a] = 0xff;
	  global_buffer[0x24] = 0x80;
	}
    }

  if ((bpb_size) && (bpb_size < jmp_ofs + 2))
    memset (&global_buffer[bpb_size], 0, jmp_ofs + 2 - bpb_size);

  config_mbr (max_sec, chs_mode, is_zip);
  sync_mbr (xd, global_buffer, fb_boot_base, copy_bpb == 1, 0);
}

void
print_info (xd_t *xd)
{
  int o;
  struct fb_data *data;
  uchar4 s1, s2, b;
  char buf[256], *p;

  seek_disk (xd, fb_boot_base + 1);
  read_disk (xd, global_buffer, 1);
  data = (struct fb_data *) global_buffer;

  printf ("version: %d.%d\n", data->ver_major, data->ver_minor);
  p = buf;
  *p = 0;
  if (! fb_ar_mode)
    {
      int chs;
      struct fb_mbr *mbr = (struct fb_mbr *) &global_buffer[512];
      char *s;

      printf ("base boot sector: %d\n", fb_boot_base);
      printf ("boot code size: %d\n", data->boot_size);
      printf ("primary data size: %u\n", data->pri_size);
      printf ("extended data size: %lu\n", data->ext_size);

      chs = mbr->max_sec;
      if (chs & 0x80)
	p += sprintf (p, " --chs");
      chs &= 0x7f;
      if (chs != 63)
	p += sprintf (p, " --max-sectors %d", chs);
      printf ("debug version: %s\n",
	      (global_buffer[512 + 0x1a8]) ? "yes" : "no");

      if (global_buffer[512 + 0xd])
	s = "copy";
      else
	{
	  s = (global_buffer[512 + 0x18]) ? "init" : "zero";
	  if (global_buffer[512 + 0x26] == 0x29)
	    p += sprintf (p, " --zip");
	}
      printf ("bpb status: %s\n", s);
    }
  else
    {
      printf ("file list size: %d\n", fb_list_sectors);
      printf ("original primary data size: %u\n", data->pri_size);
      printf ("original extended data size: %lu\n", data->ext_size);
      printf ("total sectors: %d\n", fb_ar_size);
    }

  if (data->pri_size != MIN_PRI_SIZE)
    p += sprintf (p, " --primary %d", data->pri_size);
  if (data->ext_size)
    p += sprintf (p, " --extended %ld", data->ext_size);
  if (data->list_size * 510 != DEF_LIST_SIZE)
    p += sprintf (p, " --list-size %d", data->list_size * 510);
  printf ("format options:%s\n", buf);
  printf ("file list size: %d\n", data->list_size);
  printf ("file list used: %d\n", data->list_used);

  printf ("files:\n");
  o = 0;
  b = fb_list_start + fb_list_sectors;
  s1 = 0;
  s2 = 0;
  while (fb_list[o])
    {
      struct fbm_file *m;
      struct tm *tm;
      int n;

      m = (struct fbm_file *) (fb_list + o);
      if (m->data_start < fb_pri_size)
	{
	  n = (m->data_size + 509) / 510;
	  s1 += n;
	}
      else
	{
	  n = (m->data_size + 511) >> 9;
	  s2 += n;
	}

      if (m->data_start != b)
	{
	  if ((b >= fb_pri_size) || (m->data_start <= fb_pri_size))
	    printf ("  %d*   0x%lx 0x%lx\n",
		    b >= fb_pri_size,
		    b, m->data_start - b);
	  else
	    {
	      printf ("  0*   0x%lx 0x%lx\n", b, fb_pri_size - b);
	      printf ("  1*   0x%lx 0x%lx\n", fb_pri_size,
		      m->data_start - fb_pri_size);
	    }
	}

      b = m->data_start + n;
      tm = localtime (&m->data_time);
      printf ("  %d%c%c  \"%s\" 0x%lx %ld "
	      "(%d-%02d-%02d %02d:%02d:%02d)\n",
	      (m->data_start >= fb_pri_size),
	      (m->flag & FBF_FLAG_EXTENDED) ? 'e' : ' ',
	      (m->flag & FBF_FLAG_SYSLINUX) ? 's' : ' ',
	      m->name, m->data_start, m->data_size,
	      tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
	      tm->tm_hour, tm->tm_min, tm->tm_sec);

      o += fb_list[o] + 2;
    }

  if (! fb_ar_mode)
    {
      unsigned long long sz;

      if (b != fb_total_size)
	printf ("  1*   0x%lx 0x%lx\n", b, fb_total_size - b);

      printf ("primary area free space: %ld\n",
	      (fb_pri_size - fb_list_start - fb_list_sectors - s1) * 510);
      sz = data->ext_size - s2;
      sz <<= 9;
#ifdef WIN32
      printf ("extended area free space: %I64u\n", sz);
#else
      printf ("extended area free space: %llu\n", sz);
#endif
    }
}

void
clear_menu ()
{
  fb_list_tail = 0;
  fb_list[0] = 0;
}

int
check_space (uchar4* start, uchar4 size, uchar4 *count,
	     uchar4 begin, uchar4 end, int is_ext)
{
  if ((begin >= fb_pri_size) || (end <= fb_pri_size))
    {
      if (((begin >= fb_pri_size) || (! is_ext)) && (end - begin >= *count))
	{
	  *start = begin;
	  return 1;
	}
    }
  else
    {
      if ((! is_ext) && (fb_pri_size - begin >= *count))
	{
	  *start = begin;
	  return 1;
	}

      *count = (size + 511) >> 9;
      if (end - fb_pri_size >= *count)
	{
	  *start = fb_pri_size;
	  return 1;
	}
    }

  return 0;
}

int
find_space (uchar4* start, uchar4 size, int is_ext)
{
  int ofs;
  uchar4 begin, count;

  begin = fb_list_start + fb_list_sectors;
  count = (size + 509) / 510;

  ofs = 0;
  while (fb_list[ofs])
    {
      struct fbm_file *m;
      int n;

      m = (struct fbm_file *) (fb_list + ofs);
      if (check_space (start, size, &count, begin, m->data_start, is_ext))
	return ofs;

      n = (m->data_start >= fb_pri_size) ? 512 : 510;
      begin = m->data_start + (m->data_size + n - 1) / n;

      ofs += fb_list[ofs] + 2;
    }

  if (! check_space (start, size, &count, begin, fb_total_size, is_ext))
    quit ("not enough space");

  return ofs;
}

char *
get_name (char *name)
{
  while (*name == '/')
    name++;

  if (! *name)
    quit ("empty file name");

  return name;
}

struct fbm_file *
find_file (char *name)
{
  int ofs;

  name = get_name (name);
  ofs = 0;
  while (fb_list[ofs])
    {
      struct fbm_file *m;

      m = (struct fbm_file *) (fb_list + ofs);
      if (! strcasecmp (m->name, name))
	return m;

      ofs += fb_list[ofs] + 2;
    }

  return 0;
}

int
del_file (char *name)
{
  struct fbm_file *m;
  char *p;
  int len;

  m = find_file (get_name (name));
  if (! m)
    return 0;

  len = m->size + 2;
  p = (char *) m;
  memcpy (p, p + len, (char *) (fb_list + fb_list_tail) - (char *) (p + len));
  fb_list_tail -= len;
  *(fb_list + fb_list_tail) = 0;

  return 1;
}

struct fbm_file *
alloc_file (char *in_name, uchar4 *start, uchar4 size, int is_ext, time_t tm)
{
  int len, ofs;
  struct fbm_file *m;

  in_name = get_name (in_name);
  del_file (in_name);

  len = sizeof (struct fbm_file) + strlen (in_name) + 1;
  if ((len > 255) || (fb_list_tail + len >= fb_list_size))
    quit ("file item too long");

  ofs = find_space (start, size, is_ext);
  if (ofs < fb_list_tail)
    memcpy (fb_list + ofs + len, fb_list + ofs, fb_list_tail - ofs);

  m = (struct fbm_file *) (fb_list + ofs);
  m->size = len - 2;
  m->data_start = *start;
  m->data_size = size;
  m->data_time = tm;
  m->flag = (is_ext) ? FBF_FLAG_EXTENDED : 0;
  strcpy (m->name, in_name);
  fb_list_tail += len;

  return m;
}

void
cpy_file (xd_t *xd, char *name, uchar4 size,
	  uchar4 old_start, uchar4 old_size,
	  int is_ext, int fill, time_t tm)
{
  int block_size;
  uchar4 start;
  struct fbm_file *m;

  m = alloc_file (name, &start, size, is_ext, tm);

  if (! old_size)
    is_ext = (start >= fb_pri_size);
  else if (is_ext != (start >= fb_pri_size))
    quit ("not enough space");

  block_size = (is_ext) ? 512 : 510;
  size = (size + block_size - 1) / block_size;
  if (old_size)
    {
      int num;

      num = old_size / block_size;

      if (start != old_start)
	copy_disk (xd, start, old_start, num);

      start += num;
      old_start += num;
      size -= num;
      old_size %= block_size;

      if (old_size)
	{
	  seek_disk (xd, old_start);
	  read_disk (xd, global_buffer, 1);
	  memset (&global_buffer[old_size], fill, block_size - old_size);
	  if (! is_ext)
	    *((uchar2 *) &global_buffer[510]) = start;
	  seek_disk (xd, start);
	  write_disk (xd, global_buffer, 1);
	  start++;
	  size--;
	}
    }

  xd_seek (xd, start);
  memset (global_buffer, fill, sizeof (global_buffer));
  while (size)
    {
      int n;

      n = (size > GLOB_BUF_SIZE) ? GLOB_BUF_SIZE : size;

      if (! is_ext)
	{
	  int i;

	  for (i = 0; i < n; i++)
	    *((uchar2 *) &global_buffer[i * 512 + 510]) = start + i;
	}

      write_disk (xd, global_buffer, n);

      start += n;
      size -= n;
    }
}

void
save_file_data (xd_t *xd, struct fbm_file *m, int hd, int print_dot)
{
  uchar4 start, size;
  int n;

  start = m->data_start;
  seek_disk (xd, start);
  size = m->data_size;
  if (print_dot > 0)
    dot_init (size);
  n = (start >= fb_pri_size) ? 512 : 510;
  while (size)
    {
      uchar4 nb, ns, i;
      uchar *p;

      nb = (size > GLOB_BUF_SIZE * n) ? GLOB_BUF_SIZE * n : size;
      ns = (nb + n - 1) / n;

      p = global_buffer;
      if (n == 512)
	{
	  if (read (hd, global_buffer, nb) != nb)
	    quit ("file read fails");
	}
      else
	{
	  for (i = 0; i < ns; i++, start++)
	    {
	      int nr;

	      nr = (i == (ns - 1)) ? nb - i * 510 : 510;
	      if (read (hd, &global_buffer[i * 512], nr) != nr)
		quit ("file read fails");

	      *((uchar2 *) &global_buffer[i * 512 + 510]) = start;
	    }
	}

      write_disk (xd, global_buffer, ns);
      if (print_dot)
	dot_print (nb);
      size -= nb;
    }
  if (print_dot > 0)
    dot_fini ();
}

static uchar *
read_input (uchar4 *size)
{
  uchar *result = 0;
  int hd, pos = 0;

  hd = fileno (stdin);
#ifdef WIN32
  setmode (hd, O_BINARY);
#endif

  while (1)
    {
      int n;

      n = read (hd, global_buffer, sizeof (global_buffer));
      if (n < 0)
	quit ("read stdin fails");

      if (n == 0)
	break;

      result = xrealloc (result, pos + n + 512);
      memcpy (result + pos, global_buffer, n);
      pos += n;
    }

  close (hd);
  *size = pos;
  memset (result + pos, 0, 512);
  return result;
}

struct fbm_file *
save_buff (xd_t *xd, char *in_name, uchar *buf, uchar4 size, int is_ext);

struct fbm_file *
save_file (xd_t *xd, char *in_name, char *name, int is_ext)
{
  struct stat st;
  int hd;
  uchar4 start, size;
  struct fbm_file *m;

  if (! name)
    {
      uchar *p;

      p = read_input (&size);
      if (! p)
	quit ("no input");

      return save_buff (xd, in_name, p, size, is_ext);
    }

  hd = open (name, O_RDONLY | O_BINARY);
  if (hd < 0)
    quit ("can\'t open file %s", name);

  if (fstat (hd, &st) < 0)
    quit ("can\'t get file stat");

  size = st.st_size;
  if (! size)
    quit ("empty file %s", name);

  m = alloc_file (in_name, &start, size, is_ext, st.st_mtime);

  save_file_data (xd, m, hd, 1);

  close (hd);

  return m;
}

void
load_file_data (xd_t *xd, struct fbm_file *m, int hd, int print_dot)
{
  int n;
  uchar4 size;

  seek_disk (xd, m->data_start);
  size = m->data_size;
  if (print_dot > 0)
    dot_init (size);
  n = (m->data_start >= fb_pri_size) ? 512 : 510;
  while (size)
    {
      uchar4 nb, ns, i;

      nb = (size > GLOB_BUF_SIZE * n) ? GLOB_BUF_SIZE * n : size;
      ns = (nb + n - 1) / n;

      read_disk (xd, global_buffer, ns);

      if (n == 512)
	{
	  if (write (hd, global_buffer, nb) != nb)
	    quit ("file write fails");
	}
      else
	{
	  for (i = 0; i < ns; i++)
	    {
	      int nw;

	      nw = (i == (ns - 1)) ? nb - i * 510 : 510;
	      if (write (hd, &global_buffer[i * 512], nw) != nw)
		quit ("file write fails");
	    }
	}

      if (print_dot)
	dot_print (nb);
      size -= nb;
    }
  if (print_dot > 0)
    dot_fini ();
}

void
create_dir (char *path)
{
  char *p;

  p = path;
  if (p[0] == '/')
    p++;

  for (; *p; p++)
    {
      if ((*p == '/') || (*p == '\\'))
	{
	  char c;

	  c = *p;
	  *p = 0;
#ifdef WIN32
	  mkdir (path);
#else
	  mkdir (path, 0777);
#endif
	  *p = c;
	}
    }
}

void
load_file (xd_t *xd, char *in_name, char *name)
{
  struct fbm_file *m;
  int hd;

  m = find_file (in_name);
  if (! m)
    quit ("file %s not found", in_name);

  if (! name)
    {
      hd = fileno (stdout);
#ifdef WIN32
      setmode (hd, O_BINARY);
#endif
    }
  else
    {
      create_dir (name);
      hd = open (name, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666);
    }
  if (hd < 0)
    quit ("can\'t write to file %s", name);

  load_file_data (xd, m, hd, name != 0);

  close (hd);
}

struct fbm_file *
save_buff (xd_t *xd, char *in_name, uchar *buf, uchar4 size, int is_ext)
{
  uchar4 start;
  struct fbm_file *m;

  m = alloc_file (in_name, &start, size, 0, time (0));

  seek_disk (xd, start);
  if (start >= fb_pri_size)
    {
      write_disk (xd, buf, (size + 511) >> 9);
    }
  else
    {
      int ns;

      ns = (size + 509) / 510;
      while (ns)
	{
	  uchar2 saved;

	  saved = *((uchar2 *) (buf + 510));
	  *((uchar2 *) (buf + 510)) = start;
	  write_disk (xd, buf, 1);
	  *((uchar2 *) (buf + 510)) = saved;

	  buf += 510;
	  start++;
	  ns--;
	}
    }

  return m;
}

int
load_buff (xd_t *xd, char *in_name)
{
  struct fbm_file *m;

  m = find_file (in_name);
  if (! m)
    quit ("file %s not found", in_name);

  if (m->data_size > sizeof (global_buffer))
    quit ("file %s too large", in_name);

  seek_disk (xd, m->data_start);
  if (m->data_start >= fb_pri_size)
    {
      read_disk (xd, global_buffer, (m->data_size + 511) >> 9);
    }
  else
    {
      int ns;
      uchar *p;

      p = global_buffer;
      ns = (m->data_size + 509) / 510;
      while (ns)
	{
	  read_disk (xd, p, 1);
	  p += 510;
	  ns--;
	}
    }

  return m->data_size;
}

#define LDLINUX_MAGIC		0x3eb202fe

void
syslinux_patch (xd_t *xd, struct fbm_file *m)
{
  int dw, i;
  uchar4 start, cs, *p;
  uchar *pa;
  uchar2 *ps, *pw;
  uchar4 *pc;
  int ns, ver;

  m->flag |= FBF_FLAG_SYSLINUX;

  ns = (m->data_size + 511) >> 9;
  if ((ns <= 2) || (ns > GLOB_BUF_SIZE))
    quit ("invalid size for ldlinux.bin");

  seek_disk (xd, m->data_start);
  read_disk (xd, global_buffer, ns);

  if (memcmp ((char *) &global_buffer[0x202], "SYSLINUX", 8))
    quit ("not a valid ldlinux.bin");

  pa = &global_buffer[0x200];
  while ((*((uchar4 *) pa) != LDLINUX_MAGIC) &&
	 (pa < (uchar *) &global_buffer[0x400]))
    pa += 4;

  if (pa >= (uchar *) &global_buffer[0x400])
    quit ("syslinux signature not found");

  start = m->data_start + 1 - fb_part_ofs;
  ver = global_buffer[0x20b] - '0';
  if (ver == 3)
    {
      *((uchar4 *) &global_buffer[0x1f8]) = start;
      *((uchar2 *) &global_buffer[0x1fe]) = 0xaa55;

      pw = (uchar2 *) (pa + 8);
      ps = (uchar2 *) (pa + 10);
      pc = (uchar4 *) (pa + 12);
      p = (uchar4 *) (pa + 16);
    }
  else if (ver == 4)
    {
      int ofs, epa_ofs;

      epa_ofs = *((uchar2 *) (pa + 22));
      ofs = *((uchar2 *) &global_buffer[0x200 + epa_ofs + 14]);
      *((uchar4 *) &global_buffer[ofs]) = start;
      ofs = *((uchar2 *) &global_buffer[0x200 + epa_ofs + 16]);
      *((uchar4 *) &global_buffer[ofs]) = 0xffffffff;

      pw = (uchar2 *) (pa + 12);
      ps = (uchar2 *) (pa + 8);
      pc = (uchar4 *) (pa + 16);
      ofs = *((uchar2 *) &global_buffer[0x200 + epa_ofs + 10]);
      p = (uchar4 *) &global_buffer[0x200 + ofs];
    }
  else
    quit ("invalid ldlinux.bin version %d", ver);

  dw = (m->data_size - 512) >> 2;
  *pw = dw;

#if SYSLINUX_FULL_LOAD
  start++;
  ns -= 2;
  *ps = ns;
  while (ns)
    {
      *p = start - fb_part_ofs;
      start++;
      ns--;
      p++;
    }
#else
  *ps = (ver == 3) ? 0 : 1;
#endif

  *pc = 0;
  cs = LDLINUX_MAGIC;
  for (i = 0, p = (uchar4 *) &global_buffer[0x200]; i < dw; i++, p++)
    cs -= *p;

  *pc = cs;
  seek_disk (xd, m->data_start);
  write_disk (xd, global_buffer, 2);
}

void
add_file (xd_t *xd, int argc, char **argv)
{
  int i, is_ext, is_syslinux;
  struct fbm_file *m;

  is_ext = 0;
  is_syslinux = 0;
  for (i = 0; i < argc; i++)
    {
      if (argv[i][0] != '-')
	break;

      if (! strcmp (argv[i], "--extended") ||
	  ! strcmp (argv[i], "-e"))
	{
	  is_ext = 1;
	}
      else if (! strcmp (argv[i], "--syslinux") ||
	       ! strcmp (argv[i], "-s"))
	{
	  is_syslinux = 1;
	  is_ext = 1;
	}
      else
	quit ("invalid option %s for add", argv[i]);
    }

  argc -= i;
  argv += i;

  if (argc < 1)
    quit ("not enough parameters");

  m = save_file (xd, argv[0], (argc < 2) ? 0 : argv[1], is_ext);
  if (is_syslinux)
    syslinux_patch (xd, m);
}

int
add_item_menu (int ofs, int argc, char **argv)
{
  int size, type;
  struct fbm_menu *m;

  if (argc < 2)
    quit ("not enough parameters");

  type = 0;
  size = sizeof (struct fbm_menu);
  if ((! strcmp (argv[1], "grldr")) ||
      (! strcmp (argv[1], "syslinux")) ||
      (! strcmp (argv[1], "msdos")) ||
      (! strcmp (argv[1], "freedos")) ||
      (! strcmp (argv[1], "chain")))
    {
      if (argc < 3)
	quit ("not enough parameters");

      switch (argv[1][0])
	{
	case 'g':
	  type = FBS_TYPE_GRLDR;
	  break;

	case 's':
	  type = FBS_TYPE_SYSLINUX;
	  break;

	case 'm':
	  type = FBS_TYPE_MSDOS;
	  break;

	case 'f':
	  type = FBS_TYPE_FREEDOS;
	  break;

	case 'c':
	  type = FBS_TYPE_CHAIN;
	  break;
	}
      size += strlen (get_name (argv[2])) + 1;
    }
  else if (! strcmp (argv[1], "linux"))
    {
      if (argc < 3)
	quit ("not enough parameters");

      type = FBS_TYPE_LINUX;
      size += strlen (get_name (argv[2])) + 1;

      if (argc >= 4)
	size += strlen (argv[3]) + 1;
      else
	size++;

      if (argc >= 5)
	size += strlen (argv[4]) + 1;
      else
	size++;
    }
  else
    quit ("invalid system type %s", argv[1]);

  if ((size > 255) || (ofs + size >= sizeof (global_buffer) - 512))
    quit ("menu item too long");

  m = (struct fbm_menu *) &global_buffer[ofs];
  m->size = size - 2;
  m->type = FBM_TYPE_MENU;
  m->sys_type = type;
  m->key = get_keycode (argv[0]);
  if (! m->key)
    quit ("invalid hotkey %s", argv[0]);

  switch (type)
    {
    case FBS_TYPE_GRLDR:
    case FBS_TYPE_SYSLINUX:
    case FBS_TYPE_MSDOS:
    case FBS_TYPE_FREEDOS:
    case FBS_TYPE_CHAIN:
      strcpy (m->name, get_name (argv[2]));
      break;

    case FBS_TYPE_LINUX:
      {
	char *p;

	p = m->name;
	strcpy (p, get_name (argv[2]));
	p += strlen (p) + 1;
	if (argc >= 4)
	  strcpy (p, argv[3]);
	else
	  *p = 0;
	p += strlen (p) + 1;
	if (argc >= 5)
	  strcpy (p, argv[4]);
	else
	  *p = 0;

	break;
      }
    }

  return ofs + size;
}

int
add_item_text (int ofs, int argc, char **argv)
{
  int size, i, has_newline;
  struct fbm_text *m;
  char *p;

  has_newline = 1;
  if ((argc > 0) && (! strcmp (argv[0], "-n")))
    {
      has_newline = 0;
      argc--;
      argv++;
    }

  size = sizeof (struct fbm_text);
  for (i = 0; i < argc; i++)
    size += strlen (argv[i]) + 1;

  if (has_newline)
    size += 2;

  if ((size > 255) || (ofs + size >= sizeof (global_buffer) - 512))
    quit ("text item too long");

  m = (struct fbm_text *) &global_buffer[ofs];
  m->size = size - 2;
  m->type = FBM_TYPE_TEXT;

  p = m->title;
  for (i = 0; i < argc; i++)
    {
      strcpy (p, argv[i]);
      p += strlen (argv[i]);
      *(p++) = ' ';
    }

  p--;
  if (has_newline)
    {
      *(p++) = '\r';
      *(p++) = '\n';
    }
  *p = 0;

  return ofs + size;
}

static char *color_list[16] =
{
  "black",
  "blue",
  "green",
  "cyan",
  "red",
  "magenta",
  "brown",
  "light-gray",
  "dark-gray",
  "light-blue",
  "light-green",
  "light-cyan",
  "light-red",
  "light-magenta",
  "yellow",
  "white"
};

int
name2color (char *name)
{
  int i;

  for (i = 0; i < sizeof (color_list) / sizeof (*color_list); i++)
    if (! strcmp (name, color_list[i]))
      return i;
  return -1;
}

int
get_color_value (char *name)
{
  int fg, bg;
  char *p;

  if (! strcmp (name, "normal"))
    return COLOR_NORMAL;

  p = strchr (name, '/');
  if (p)
    {
      *(p++) = 0;

      bg = name2color (p);
      if (bg < 0)
	quit ("invalid background color %s", p);
    }
  else
    bg = 0;

  fg = name2color (name);
  if (fg < 0)
    quit ("invalid foreground color %s", name);

  return (bg << 4) + fg;
}

static char color_name[32];

char *
get_color_name (uchar4 color)
{
  int fg, bg;

  if (color == COLOR_NORMAL)
    return "normal";

  fg = color & 0xf;
  bg = color >> 4;
  if (! bg)
    return color_list[fg];

  sprintf (color_name, "%s/%s", color_list [fg], color_list[bg]);
  return color_name;
}

int
add_item_timeout (int ofs, int argc, char **argv, uchar type)
{
  int size;
  struct fbm_timeout *m;

  if (argc < 1)
    quit ("not enough parameters");

  size = sizeof (struct fbm_timeout);
  if (ofs + size >= sizeof (global_buffer) - 512)
    quit ("menu item too long");

  m = (struct fbm_timeout *) &global_buffer[ofs];
  m->size = size - 2;
  m->type = type;
  m->timeout = ((type == FBM_TYPE_COLOR) ? get_color_value (argv[0]) :
		strtoul (argv[0], 0, 0));

  return ofs + size;
}

#define PARSE_LINE_ARGS_STEP	10

int
parse_line (char *line, char ***args)
{
  char **v;
  int n, max;

  n = 0;
  max = 0;
  v = 0;

  while (*line)
    {
      char *p;

      while ((*line == ' ') || (*line == '\t') ||
	     (*line == '\r') || (*line == '\n'))
	line++;

      if (! *line)
	break;

      p = line;
      if (*line == '\"')
	{
	  p++;
	  line++;
	  while ((*line) && (*line != '\"'))
	    line++;
	}
      else
	{
	  while ((*line) && (*line != ' ') && (*line != '\t') &&
		 (*line != '\r') && (*line != '\n'))
	    line++;
	}

      if (*line)
	{
	  *line = 0;
	  line++;
	}

      if (n == max)
	{
	  v = realloc (v, (max + PARSE_LINE_ARGS_STEP) * sizeof (v[0]));
	  if (! v)
	    quit ("not enough memory");
	  max += PARSE_LINE_ARGS_STEP;
	}
      v[n] = p;
      n++;
    }

  *args = v;
  return n;
}

int
add_menu_line (int ofs, char *line)
{
  int n;
  char **v;

  if (line[0] == '#')
    return ofs;

  n = parse_line (line, &v);
  if (! n)
    return ofs;

  if (! strcmp (v[0], "menu"))
    ofs = add_item_menu (ofs, n - 1, v + 1);
  else if (! strcmp (v[0], "text"))
    ofs = add_item_text (ofs, n - 1, v + 1);
  else if (! strcmp (v[0], "timeout"))
    ofs = add_item_timeout (ofs, n - 1, v + 1, FBM_TYPE_TIMEOUT);
  else if (! strcmp (v[0], "default"))
    ofs = add_item_timeout (ofs, n - 1, v + 1, FBM_TYPE_DEFAULT);
  else if (! strcmp (v[0], "color"))
    ofs = add_item_timeout (ofs, n - 1, v + 1, FBM_TYPE_COLOR);
  else
    quit ("unknown menu command");

  free (v);

  return ofs;
}

void
add_menu (xd_t *xd, int argc, char **argv)
{
  FILE *f;
  char line[512];
  int i, ofs;
  int is_append = 0;
  int is_direct = 0;

  for (i = 0; i < argc; i++)
    {
      if (argv[i][0] != '-')
	break;

      if (! strcmp (argv[i], "--append") ||
	  ! strcmp (argv[i], "-a"))
	{
	  is_append = 1;
	}
      else if (! strcmp (argv[i], "--string") ||
	       ! strcmp (argv[i], "-s"))
	{
	  is_direct = 1;
	}
      else
	quit ("invalid option %s for add-menu", argv[i]);
    }

  argc -= i;
  argv += i;

  if (argc < 2)
    quit ("not enough parameters");

  if ((is_append) && (find_file (argv[0])))
    ofs = load_buff (xd, argv[0]) - 1;
  else
    ofs = 0;

  if (is_direct)
    {
      for (i = 1; i < argc; i++)
	ofs = add_menu_line (ofs, argv[i]);
    }
  else
    {
      f = fopen (argv[1], "r");
      if (! f)
	quit ("can\'t open file %s", argv[1]);

      while (fgets (line, sizeof (line), f))
	ofs = add_menu_line (ofs, line);

      fclose (f);
    }

  global_buffer[ofs] = 0;
  save_buff (xd, argv[0], global_buffer, ofs + 1, 0);
}

void
resize_file (xd_t *xd, int argc, char **argv)
{
  int fill = 0;
  int is_ext = 0;
  struct fbm_file *m;
  uchar4 size, old_start, old_size;
  int i;
  char *name;

  for (i = 0; i < argc; i++)
    {
      if (argv[i][0] != '-')
	break;

      if (! strcmp (argv[i], "--extended") ||
	  ! strcmp (argv[i], "-e"))
	{
	  is_ext = 1;
	}
      else if (! strcmp (argv[i], "--fill") ||
	       ! strcmp (argv[i], "-f"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  if (strlen (argv[i]) == 1)
	    fill = argv[i][0];
	  else
	    fill = strtoul (argv[i], 0, 0);
	}
      else
	quit ("invalid option %s for resize", argv[i]);
    }

  argc -= i;
  argv += i;

  if (argc < 2)
    quit ("not enough parameters");

  size = strtoul (argv[1], 0, 0);
  if (! size)
    quit ("invalid size");

  name = get_name (argv[0]);
  m = find_file (name);
  if (m)
    {
      if (m->data_size >= size)
	{
	  m->data_size = size;
	  return;
	}

      is_ext = (m->data_start >= fb_pri_size);
      old_start = m->data_start;
      old_size = m->data_size;
      del_file (name);
    }
  else
    {
      old_start = 0;
      old_size = 0;
    }

  cpy_file (xd, name, size, old_start, old_size, is_ext, fill, time (0));
}

void
copy_file (xd_t *xd, int argc, char **argv)
{
  struct fbm_file *m;

  del_file (argv[1]);

  m = find_file (argv[0]);
  if (! m)
    quit ("file %s not found", argv[0]);

  cpy_file (xd, get_name (argv[1]), m->data_size,
	    m->data_start, m->data_size,
	    m->data_start >= fb_pri_size, 0, m->data_time);
}

void
move_file (xd_t *xd, int argc, char **argv)
{
  struct fbm_file *m;
  int old_len, len;
  char *name;

  if (argc < 2)
    quit ("not enough parameters");

  del_file (argv[1]);

  m = find_file (argv[0]);
  if (! m)
    quit ("file %s not found", argv[0]);

  name = get_name (argv[1]);
  len = sizeof (struct fbm_file) + strlen (name) + 1;
  old_len = m->size + 2;
  if ((len > 255) || (fb_list_tail + len - old_len >= fb_list_size))
    quit ("file item too long");

  if (len != old_len)
    {
      char *p;

      p = (char *) m;
      memcpy (p + len, p + old_len,
	      (char *) (fb_list + fb_list_tail) - (char *) (p + old_len));
      fb_list_tail += len - old_len;
      *(fb_list + fb_list_tail) = 0;
      m->size = len - 2;
    }

  strcpy (m->name, name);
}

void
export_file (xd_t *xd, int argc, char **argv)
{
  if (argc < 1)
    quit ("not enough parameters");

  load_file (xd, argv[0], (argc < 2) ? 0 : argv[1]);
}

void
remove_file (xd_t *xd, int argc, char **argv)
{
  if (argc < 1)
    quit ("not enough parameters");

  if (! del_file (argv[0]))
    quit ("file %s not found", argv[0]);
}

void
cat_file (xd_t *xd, int argc, char **argv)
{
  int len;

  if (argc < 1)
    quit ("not enough parameters");

  len = load_buff (xd, argv[0]);
  global_buffer[len] = 0;
  puts ((char *) global_buffer);
}

void
cat_menu (xd_t *xd, int argc, char **argv)
{
  int o, len;

  if (argc < 1)
    quit ("not enough parameters");

  len = load_buff (xd, argv[0]);
  o = 0;
  while (global_buffer[o])
    {
      switch (global_buffer[o + 1])
	{
	case FBM_TYPE_MENU:
	  {
	    struct fbm_menu *m;

	    m = (struct fbm_menu *) &global_buffer[o];
	    printf ("menu %s ", get_keyname (m->key));
	    switch (m->sys_type)
	      {
	      case FBS_TYPE_GRLDR:
		printf ("grldr \"%s\"\n", m->name);
		break;
	      case FBS_TYPE_SYSLINUX:
		printf ("syslinux \"%s\"\n", m->name);
		break;
	      case FBS_TYPE_LINUX:
		{
		  char *p1, *p2;

		  p1 = m->name + strlen (m->name) + 1;
		  p2 = p1 + strlen (p1) + 1;
		printf ("linux \"%s\" \"%s\" \"%s\"\n", m->name, p1, p2);
		break;
		}
	      case FBS_TYPE_MSDOS:
		printf ("msdos \"%s\"\n", m->name);
		break;
	      case FBS_TYPE_FREEDOS:
		printf ("freedos \"%s\"\n", m->name);
		break;
	      case FBS_TYPE_CHAIN:
		printf ("chain \"%s\"\n", m->name);
		break;
	      default:
		quit ("invalid system type %d", m->sys_type);
	      }
	    break;
	  }

	case FBM_TYPE_TEXT:
	  {
	    struct fbm_text *m;
	    int text_len, has_newline;

	    m = (struct fbm_text *) &global_buffer[o];
	    text_len = strlen (m->title);
	    has_newline = 0;
	    if ((text_len >= 2) &&
		(m->title[text_len - 1] == '\n') &&
		(m->title[text_len - 2] == '\r'))
	      {
		has_newline = 1;
		m->title[text_len - 2] = 0;
	      }

	    printf ("text %s\"%s\"\n", (has_newline) ? "" : "-n ", m->title);
	    break;
	  }

	case FBM_TYPE_TIMEOUT:
	case FBM_TYPE_DEFAULT:
	  {
	    struct fbm_timeout *m;
	    int is_timeout;

	    is_timeout = (global_buffer[o + 1] == FBM_TYPE_TIMEOUT);
	    m = (struct fbm_timeout *) &global_buffer[o];
	    printf ("%s %d\n", (is_timeout) ? "timeout" : "default",
		    m->timeout);
	    break;
	  }

	case FBM_TYPE_COLOR:
	  {
	    struct fbm_timeout *m;

	    m = (struct fbm_timeout *) &global_buffer[o];
	    printf ("color %s\n", get_color_name (m->timeout));
	    break;
	  }

	default:
	  quit ("invalid menu type %d", global_buffer[o + 1]);
	}

      o += global_buffer[o] + 2;
      if (o >= len)
	quit ("invalid menu");
    }
}

void
pack_disk (xd_t *xd)
{
  uchar4 b;
  int ofs;

  b = fb_list_start + fb_list_sectors;
  ofs = 0;
  while (fb_list[ofs])
    {
      struct fbm_file *m;
      int n;

      m = (struct fbm_file *) (fb_list + ofs);
      if (m->data_start < fb_pri_size)
	n = (m->data_size + 509) / 510;
      else
	n = (m->data_size + 511) >> 9;

      if ((b < fb_pri_size) && (m->data_start >= fb_pri_size))
	b = fb_pri_size;

      if (m->data_start != b)
	{
	  copy_disk (xd, b, m->data_start, n);
	  m->data_start = b;
	}

      b = m->data_start + n;
      ofs += fb_list[ofs] + 2;
    }
}

void
check_disk (xd_t *xd)
{
  int start;

  seek_disk (xd, 0);
  start = 0;
  while (start < fb_pri_size)
    {
      int i, n;

      n = fb_pri_size - start;
      if (n > GLOB_BUF_SIZE)
	n = GLOB_BUF_SIZE;

      read_disk (xd, global_buffer, n);
      for (i = 0; i < n; i++, start++)
	{
	  struct fb_mbr *mbr;
	  int b;

	  mbr = (struct fb_mbr *) &global_buffer[i * 512];
	  if (start <= fb_boot_base)
	    b = ((mbr->end_magic != 0xaa55) || (mbr->lba != start));
	  else
	    b = (mbr->end_magic != start);

	  if (b)
	    quit ("check fail at sector %d", start);
	}
    }
}

void
save_archive (xd_t *xd, int argc, char **argv)
{
  uchar *buf;
  int i, hd, o1, o2, start;
  int list_size, list_sectors;
  struct fb_ar_data *data;
  uchar4 total_size;

  list_size = 0;
  for (i = 0; i < argc; i++)
    {
      if (argv[i][0] != '-')
	break;

      if (! strcmp (argv[i], "--list-size") ||
	  ! strcmp (argv[i], "-l"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  list_size = strtoul (argv[i], 0, 0);
	}
      else
	quit ("invalid option %s for save", argv[i]);
    }

  argc -= i;
  argv += i;

  if (argc < 1)
    quit ("not enough parameters");

  if (! list_size)
    list_size = fb_list_size;

  list_sectors = (list_size + 509) / 510;
  list_size = list_sectors * 510;
  buf = xmalloc (list_sectors << 9);

  memset (global_buffer, 0, 512);
  data = (struct fb_ar_data *) global_buffer;
  data->ar_magic = FB_AR_MAGIC_LONG;
  data->ver_major = VER_MAJOR;
  data->ver_minor = VER_MINOR;
  data->pri_size = fb_pri_size;
  data->ext_size = fb_total_size - fb_pri_size;
  data->list_size = list_sectors;
  start = 1 + list_sectors;

  o1 = o2 = 0;
  total_size = 0;
  while (fb_list[o1])
    {
      struct fbm_file *m1, *m2;

      m1 = (struct fbm_file *) (fb_list + o1);
      m2 = (struct fbm_file *) (buf + o2);

      if (o2 + m1->size + 2 >= list_size)
	quit ("not enough space for file list");

      memcpy ((char *) m2, (char *) m1, m1->size + 2);
      m2->data_start = start;

      start += (m1->data_size + 511) >> 9;
      total_size += m1->data_size;
      o1 += fb_list[o1] + 2;
      o2 += buf[o2] + 2;
    }

  data->list_used = (o2 / 510) + 1;
  add_mark (buf, list_sectors, 1, o2);

  hd = open (argv[0], O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666);
  if (hd < 0)
    quit ("can\'t write to file %s", argv[0]);

  if ((write (hd, global_buffer, 512) != 512) |
      (write (hd, buf, list_sectors * 512) != list_sectors * 512))
    quit ("file write fails");

  free (buf);

  dot_init (total_size);
  o1 = 0;
  while (fb_list[o1])
    {
      struct fbm_file *m;
      int n;

      m = (struct fbm_file *) (fb_list + o1);
      load_file_data (xd, m, hd, -1);
      n = m->data_size & 511;
      if (n)
	{
	  n = 512 - n;
	  memset (global_buffer, 0, n);
	  if (write (hd, global_buffer, n) != n)
	    quit ("file write fails");
	}

      o1 += fb_list[o1] + 2;
    }
  dot_fini ();

  close (hd);
}

void
load_archive (xd_t *xd, int argc, char **argv)
{
  uchar *buf;
  int hd, ofs;
  uchar4 total_size;

  if (argc < 1)
    quit ("not enough parameters");

  hd = open (argv[0], O_RDONLY | O_BINARY);
  if (hd < 0)
    quit ("can\'t open file %s", argv[0]);

  buf = get_ar_header (hd);
  total_size = 0;
  ofs = 0;
  while (buf[ofs])
    {
      struct fbm_file *m1;
      m1 = (struct fbm_file *) (buf + ofs);
      total_size += m1->data_size;
      ofs += buf[ofs] + 2;
    }

  dot_init (total_size);
  ofs = 0;
  while (buf[ofs])
    {
      struct fbm_file *m1, *m2;
      uchar4 start;

      m1 = (struct fbm_file *) (buf + ofs);
      m2 = alloc_file (m1->name, &start, m1->data_size,
		       m1->flag & FBF_FLAG_EXTENDED, m1->data_time);
      lseek (hd, m1->data_start << 9, SEEK_SET);
      save_file_data (xd, m2, hd, -1);
      m2->flag = m1->flag;
      if (m1->flag & FBF_FLAG_SYSLINUX)
	syslinux_patch (xd, m2);

      ofs += buf[ofs] + 2;
    }
  dot_fini ();

  free (buf);
  close (hd);
}

void
create_archive (xd_t *xd, int argc, char **argv)
{
  int i;
  int pri_size = MIN_PRI_SIZE;
  int ext_size = 0;
  int list_size = DEF_LIST_SIZE;
  struct fb_ar_data *data;

  for (i = 0; i < argc; i++)
    {
      if (argv[i][0] != '-')
	break;

      if (! strcmp (argv[i], "--primary") ||
	  ! strcmp (argv[i], "-p"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  pri_size = get_sector_size (argv[i]);
	  if (pri_size < MIN_PRI_SIZE)
	    quit ("primary data size too small");
	  if (pri_size > MAX_PRI_SIZE)
	    quit ("primary data size too large");
	}
      else if (! strcmp (argv[i], "--extended") ||
	       ! strcmp (argv[i], "-e"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  ext_size = get_sector_size (argv[i]);
	}
      else if (! strcmp (argv[i], "--list-size") ||
	       ! strcmp (argv[i], "-l"))
	{
	  if (i >= argc)
	    quit ("no parameter for %s", argv[i]);

	  i++;
	  list_size = strtoul (argv[i], 0, 0);
	}
      else
	quit ("invalid option %s for create", argv[i]);
    }

  list_size = (list_size + 509) / 510;
  if (list_size > MAX_LIST_SIZE)
    quit ("list too large");

  fb_ar_mode = 1;
  fb_boot_base = -1;
  fb_list_start = 1;
  fb_list_sectors = list_size;
  fb_list = xmalloc (list_size << 9);
  fb_list[0] = 0;
  fb_list_tail = 0;
  fb_ar_size = fb_list_start + fb_list_sectors + 1;

  data = (struct fb_ar_data *) global_buffer;
  data->ar_magic = FB_AR_MAGIC_LONG;
  data->ver_major = VER_MAJOR;
  data->ver_minor = VER_MINOR;
  data->pri_size = pri_size;
  data->ext_size = ext_size;
  data->list_used = 1;
  data->list_size = list_size;
  write_disk (xd, global_buffer, 1);
  write_header (xd);
}

int
main (int argc, char **argv)
{
  int i;
  xd_t* xd;

  assert (sizeof (time_t) == 4);
  assert (sizeof (struct fb_mbr) == 512);
  assert (sizeof (struct fat_bs16) == 512);
  assert (sizeof (struct fat_bs32) == 512);

  for (i = 1; i < argc; i++)
    {
      if (argv[i][0] != '-')
	break;

      if (! strcmp (argv[i], "--help") ||
	  ! strcmp (argv[i], "-h"))
	{
	  help ();
	  return 1;
	}
      else if (! strcmp (argv[i], "--version") ||
	       ! strcmp (argv[i], "-V"))

	{
	  fprintf (stderr, "%s version : %s build %d\n", progname, VERSION,
		   BUILD_NUMBER);
	  return 1;
	}
      else if (! strcmp (argv[i], "--verbose") ||
	       ! strcmp (argv[i], "-v"))
	{
	  verbosity++;
	}
      else if (! strcmp (argv[i], "--list") ||
	       ! strcmp (argv[i], "-l"))
	{
	  list_devs ();
	  return 0;
	}
      else if (! strcmp (argv[i], "--debug") ||
	       ! strcmp (argv[i], "-d"))
	{
	  fb_mbr_data = fb_mbr_dbg;
	  fb_mbr_size = sizeof (fb_mbr_dbg);
	}
      else
	quit ("invalid option %s", argv[i]);
    }

  if (i >= argc - 1)
    quit ("no device name or command");

  xd = xd_open (argv[i], 1);
  if (! xd)
    quit ("open %s fails", argv[i]);

  i++;
  if (! strcmp (argv[i], "format"))
    {
      format_disk (xd, argc - i - 1, argv + i + 1);
    }
  else if (! strcmp (argv[i], "restore"))
    {
      restore_disk (xd);
    }
  else if (! strcmp (argv[i], "update"))
    {
      read_header (xd, 0);
      update_header (xd);
      write_header (xd);
    }
  else if (! strcmp (argv[i], "sync"))
    {
      read_header (xd, 0);
      sync_disk (xd, argc - i - 1, argv + i + 1);
    }
  else if (! strcmp (argv[i], "info"))
    {
      read_header (xd, 1);
      print_info (xd);
    }
  else if (! strcmp (argv[i], "clear"))
    {
      read_header (xd, 1);
      clear_menu ();
      write_header (xd);
    }
  else if (! strcmp (argv[i], "add"))
    {
      read_header (xd, 1);
      add_file (xd, argc - i - 1, argv + i + 1);
      write_header (xd);
    }
  else if (! strcmp (argv[i], "add-menu"))
    {
      read_header (xd, 1);
      add_menu (xd, argc - i - 1, argv + i + 1);
      write_header (xd);
    }
  else if (! strcmp (argv[i], "resize"))
    {
      read_header (xd, 1);
      resize_file (xd, argc - i - 1, argv + i + 1);
      write_header (xd);
    }
  else if (! strcmp (argv[i], "copy"))
    {
      read_header (xd, 1);
      copy_file (xd, argc - i - 1, argv + i + 1);
      write_header (xd);
    }
  else if (! strcmp (argv[i], "move"))
    {
      read_header (xd, 1);
      move_file (xd, argc - i - 1, argv + i + 1);
      write_header (xd);
    }
  else if (! strcmp (argv[i], "export"))
    {
      read_header (xd, 1);
      export_file (xd, argc - i - 1, argv + i + 1);
    }
  else if (! strcmp (argv[i], "remove"))
    {
      read_header (xd, 1);
      remove_file (xd, argc - i - 1, argv + i + 1);
      write_header (xd);
    }
  else if (! strcmp (argv[i], "cat"))
    {
      read_header (xd, 1);
      cat_file (xd, argc - i - 1, argv + i + 1);
    }
  else if (! strcmp (argv[i], "cat-menu"))
    {
      read_header (xd, 1);
      cat_menu (xd, argc - i - 1, argv + i + 1);
    }
  else if (! strcmp (argv[i], "pack"))
    {
      read_header (xd, 1);
      pack_disk (xd);
      write_header (xd);
    }
  else if (! strcmp (argv[i], "check"))
    {
      read_header (xd, 0);
      check_disk (xd);
    }
  else if (! strcmp (argv[i], "save"))
    {
      read_header (xd, 1);
      save_archive (xd, argc - i - 1, argv + i + 1);
    }
  else if (! strcmp (argv[i], "load"))
    {
      read_header (xd, 1);
      load_archive (xd, argc - i - 1, argv + i + 1);
      write_header (xd);
    }
  else if (! strcmp (argv[i], "create"))
    {
      create_archive (xd, argc - i - 1, argv + i + 1);
    }
  else
    quit ("Invalid command %s", argv[i]);

  xd_close (xd);
  xd_unlock ();

  return 0;
}
