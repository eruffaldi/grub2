/* loopbackx.c - command to add multiple loopback devices.  */
/*
 *  Emanuele Ruffaldi
 *
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2005,2006,2007  Free Software Foundation, Inc.
 * 
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/disk.h>
#include <grub/mm.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>

GRUB_MOD_LICENSE ("GPLv3+");

#define MAXCHAINFILES 4

struct grub_loopbackx
{
  char *devname;
  int nfiles;
  grub_file_t files[MAXCHAINFILES]; // 
  struct grub_loopbackx *next;
  unsigned long id;
};

static struct grub_loopbackx *loopback_list;
static unsigned long last_id = 0;

static const struct grub_arg_option options[] =
  {
    /* TRANSLATORS: The disk is simply removed from the list of available ones,
       not wiped, avoid to scare user.  */
    {"delete", 'd', 0, N_("Delete the specified loopback drive."), 0, 0},
    {0, 0, 0, 0, 0, 0}
  };

/* Delete the loopback device NAME.  */
static grub_err_t
delete_loopbackx (const char *name)
{
  struct grub_loopbackx *dev;
  struct grub_loopbackx **prev;

  /* Search for the device.  */
  for (dev = loopback_list, prev = &loopback_list;
       dev;
       prev = &dev->next, dev = dev->next)
    if (grub_strcmp (dev->devname, name) == 0)
      break;

  if (! dev)
    return grub_error (GRUB_ERR_BAD_DEVICE, "device not found");

  /* Remove the device from the list.  */
  *prev = dev->next;

  grub_free (dev->devname);
  while(dev->nfiles > 0)
  {
      grub_file_close (dev->files[--dev->nfiles]);
  }
  grub_free (dev);

  return 0;
}

/* The command to add and remove loopback devices.  */
static grub_err_t
grub_cmd_loopbackx (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  struct grub_loopbackx *newdev;
  grub_err_t ret;
  grub_file_t files[MAXCHAINFILES];
  int nfiles = 0;
  int i;

  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "device name required");

  /* Check if `-d' was used.  */
  if (state[0].set)
      return delete_loopbackx (args[0]);

  if (argc < 2)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
  if (argc > MAXCHAINFILES+1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("too many filenames expected"));
    
  /* Check that a device with requested name does not already exist. */
  for (newdev = loopback_list; newdev; newdev = newdev->next)
    if (grub_strcmp (newdev->devname, args[0]) == 0)
      return grub_error (GRUB_ERR_BAD_ARGUMENT, "device name already exists");

  for(i = 1; i < argc; i++)
  {
    grub_file_t file = grub_file_open (args[i], GRUB_FILE_TYPE_LOOPBACK
                | GRUB_FILE_TYPE_NO_DECOMPRESS);
    if (! file)
    {
        goto fail;
    }
    else
    {
        files[nfiles++] = file;
    }
  }

    /* Unable to replace it, make a new entry.  */
    newdev = grub_malloc (sizeof (struct grub_loopbackx));
    if (! newdev)
        goto fail;

    newdev->devname = grub_strdup (args[0]);
    if (! newdev->devname)
        {
        grub_free (newdev);
        goto fail;
        }
    newdev->id = last_id++;
    for(i = 0; i < nfiles; i++)
        newdev->files[i] = files[i];
    newdev->nfiles = nfiles;

    /* Add the new entry to the list.  */
    newdev->next = loopback_list;
    loopback_list = newdev;

  return 0;

fail:
  ret = grub_errno;
    // close all other open
    for(i = 0; i < nfiles; i++)
    {
        grub_file_close(files[i]);
    }
  return ret;
}


static int
grub_loopbackx_iterate (grub_disk_dev_iterate_hook_t hook, void *hook_data,
		       grub_disk_pull_t pull)
{
  struct grub_loopbackx *d;
  if (pull != GRUB_DISK_PULL_NONE)
    return 0;
  for (d = loopback_list; d; d = d->next)
    {
      if (hook (d->devname, hook_data))
	return 1;
    }
  return 0;
}

static grub_err_t
grub_loopbackx_open (const char *name, grub_disk_t disk)
{
  struct grub_loopbackx *dev;

  for (dev = loopback_list; dev; dev = dev->next)
    if (grub_strcmp (dev->devname, name) == 0)
      break;

  if (! dev)
    return grub_error (GRUB_ERR_UNKNOWN_DEVICE, "can't open device");

  int nfiles = dev->nfiles;
  grub_file_t *files = dev->files;
  grub_size_t alldata = 0;
  // one size unknown makes all size unknown, otherwise sum
  disk->total_sectors = 0;
  for(int i = 0; i < nfiles; i++)
  {
    if(files[i]->size == GRUB_FILE_SIZE_UNKNOWN)
    {
        disk->total_sectors = GRUB_DISK_SIZE_UNKNOWN;
        break;
    }
    else
    {
        alldata += files[i]->size;
    }
  }
  if(disk->total_sectors != GRUB_DISK_SIZE_UNKNOWN)
  {
        disk->total_sectors = ((alldata + GRUB_DISK_SECTOR_SIZE - 1)     / GRUB_DISK_SECTOR_SIZE);
  }

  /* Avoid reading more than 512M.  */
  disk->max_agglomerate = 1 << (29 - GRUB_DISK_SECTOR_BITS
				- GRUB_DISK_CACHE_BITS);

  disk->id = dev->id;

  disk->data = dev;

  return 0;
}


static grub_err_t
grub_loopbackx_read (grub_disk_t disk, grub_disk_addr_t sector,
		    grub_size_t size, char *buf)
{
  int nfiles =((struct grub_loopbackx *) disk->data)->nfiles;
  grub_file_t *files =((struct grub_loopbackx *) disk->data)->files;
  grub_size_t spos = sector << GRUB_DISK_SECTOR_BITS;
  grub_size_t total = size << GRUB_DISK_SECTOR_BITS;

  // if the starting point is less then border
  for(int i = 0; i < nfiles && total != 0; i++)
  {
    if(spos < files[i]->size)
    {
        grub_off_t epos = spos + files[i]->size;
        grub_size_t n = total;
        if(epos > files[i]->size)
        {
            n = files[i]->size-spos;
        }
        grub_file_seek (files[i], spos);
        grub_file_read (files[i], buf, n);
        buf += n;
        total -= n;
        spos = 0; // next file if any will start at 0
    }
    else
    {
        // relative to next
        spos -= files[i]->size;
    }
  }
  if (grub_errno)
    return grub_errno;

  // leftover are set to zero
  if(total > 0)
  {
      grub_memset (buf, 0, total);
  }
  return 0;
}

static grub_err_t
grub_loopbackx_write (grub_disk_t disk __attribute ((unused)),
		     grub_disk_addr_t sector __attribute ((unused)),
		     grub_size_t size __attribute ((unused)),
		     const char *buf __attribute ((unused)))
{
  return grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET,
		     "loopback write is not supported");
}

static struct grub_disk_dev grub_loopbackx_dev =
  {
    .name = "loopbackx",
    .id = GRUB_DISK_DEVICE_LOOPBACK_ID,
    .disk_iterate = grub_loopbackx_iterate,
    .disk_open = grub_loopbackx_open,
    .disk_read = grub_loopbackx_read,
    .disk_write = grub_loopbackx_write,
    .next = 0
  };

static grub_extcmd_t cmd;

GRUB_MOD_INIT(loopback)
{
  cmd = grub_register_extcmd ("loopbackx", grub_cmd_loopbackx, 0,
			      N_("[-d] DEVICENAME FILE1 FILE2 ..."),
			      /* TRANSLATORS: The file itself is not destroyed
				 or transformed into drive.  */
			      N_("Make a virtual drive from multiple files"), options);
  grub_disk_dev_register (&grub_loopbackx_dev);
}

GRUB_MOD_FINI(loopbackx)
{
  grub_unregister_extcmd (cmd);
  grub_disk_dev_unregister (&grub_loopbackx_dev);
}
