/****************************************************************************
 * fs/vfs/fs_stat64.c
 *
 *   Copyright (C) 2007-2009, 2012 Gregory Nutt. All rights reserved.
 *   Copyright (c) <2014-2015>, <Huawei Technologies Co., Ltd>
 *   All rights reserved.
 *
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/
/****************************************************************************
 * Notice of Export Control Law
 * ===============================================
 * Huawei LiteOS may be subject to applicable export control laws and regulations,
 * which might include those applicable to Huawei LiteOS of U.S. and the country in
 * which you are located.
 * Import, export and usage of Huawei LiteOS in any manner by you shall be in
 * compliance with such applicable export control laws and regulations.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "vfs_config.h"

#include "errno.h"
#include "sys/stat.h"
#include "string.h"
#include "stdlib.h"
#include "inode/inode.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stat64pseudo
 ****************************************************************************/

static inline int stat64pseudo(FAR struct inode *inode_ptr, FAR struct stat64 *buf)
{
  /* Most of the stat entries just do not apply */

  memset(buf, 0, sizeof(struct stat64) );

  if (INODE_IS_SPECIAL(inode_ptr))
    {
#if defined(CONFIG_FS_NAMED_SEMAPHORES)
      if (INODE_IS_NAMEDSEM(inode_ptr))
        {
          buf->st_mode = S_IFSEM;
        }
      else
#endif
#if !defined(CONFIG_DISABLE_MQUEUE)
      if (INODE_IS_MQUEUE(inode_ptr))
        {
          buf->st_mode = S_IFMQ;
        }
      else
#endif
#if defined(CONFIG_FS_SHM)
       if (INODE_IS_SHM(inode_ptr)) */
        {
          buf->st_mode | S_IFSHM;
        }
      else
#endif
       {
       }
    }
  else if (inode_ptr->u.i_ops)
    {
      if (inode_ptr->u.i_ops->read)
        {
          buf->st_mode = S_IROTH|S_IRGRP|S_IRUSR;
        }

      if (inode_ptr->u.i_ops->write)
        {
          buf->st_mode |= S_IWOTH|S_IWGRP|S_IWUSR;
        }

      if (INODE_IS_MOUNTPT(inode_ptr))
        {
          buf->st_mode |= S_IFDIR;
        }
      else if (INODE_IS_BLOCK(inode_ptr))
        {
          /* What is it also has child inodes? */

          buf->st_mode |= S_IFBLK;
        }
      else /* if (INODE_IS_DRIVER(inode)) */
        {
          /* What is it if it also has child inodes? */

          buf->st_mode |= S_IFCHR;
        }
    }
  else
    {
      /* If it has no operations, then it must just be a intermediate
       * node in the inode tree.  It is something like a directory.
       * We'll say that all pseudo-directories are read-able but not
       * write-able.
       */

      buf->st_mode |= S_IFDIR|S_IROTH|S_IRGRP|S_IRUSR;
    }

  return OK;
}

/****************************************************************************
 * Name: stat64root
 ****************************************************************************/

static inline int stat64root(FAR struct stat64 *buf)
{
  /* There is no inode associated with the fake root directory */

  memset(buf, 0, sizeof(struct stat64) );
  buf->st_mode = S_IFDIR|S_IROTH|S_IRGRP|S_IRUSR;
  return OK;
}

/****************************************************************************
 * Global Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stat64
 *
 * Returned Value:
 *   Zero on success; -1 on failure with errno set:
 *
 *   EACCES  Search permission is denied for one of the directories in the
 *           path prefix of path.
 *   EFAULT  Bad address.
 *   ENOENT  A component of the path path does not exist, or the path is an
 *           empty string.
 *   ENOMEM  Out of memory
 *   ENOTDIR A component of the path is not a directory.
 *
 ****************************************************************************/

int stat64(FAR const char *path, FAR struct stat64 *buf)
{
  FAR struct inode *inode_ptr;
  const char       *relpath = (const char *)NULL;
  int               ret     = OK;
  char *fullpath            = (char *)NULL;

  /* Sanity checks */

  if (!path || !buf)
    {
      ret = EFAULT;
      goto errout;
    }

  if (!path[0])
    {
      ret = ENOENT;
      goto errout;
    }

  ret = vfs_normalize_path((const char *)NULL, path, &fullpath);

  if(ret < 0)
  {
    ret = -ret;
    goto errout;
  }

  /* Check for the fake root directory (which has no inode) */

  if (strcmp(fullpath, "/") == 0)
    {
      free(fullpath);
      return stat64root(buf);
    }

  /* Get an inode for this file */

  inode_ptr = inode_find(fullpath, &relpath);
  if (!inode_ptr)
    {
      /* This name does not refer to a psudeo-inode and there is no
       * mountpoint that includes in this path.
       */

      ret = ENOENT;
      goto errout_with_path;
    }

  /* The way we handle the stat depends on the type of inode that we
   * are dealing with.
   */

#ifndef CONFIG_DISABLE_MOUNTPOINT
  if (INODE_IS_MOUNTPT(inode_ptr))
    {
      /* The node is a file system mointpoint. Verify that the mountpoint
       * supports the stat() method
       */

      if (inode_ptr->u.i_mops && inode_ptr->u.i_mops->stat64)
        {
          /* Perform the stat64() operation */

          ret = inode_ptr->u.i_mops->stat64(inode_ptr, relpath, buf);
        }
      else
        {
          ret = -ENOTSUPP;
        }
    }
  else
#endif
    {
      /* The node is part of the root pseudo file system */

      ret = stat64pseudo(inode_ptr, buf);
    }

  /* Check if the stat operation was successful */

  if (ret < 0)
    {
      ret = -ret;
      goto errout_with_inode;
    }

  /* Successfully stat'ed the file */

  inode_release(inode_ptr);
  free(fullpath);
  return OK;

/* Failure conditions always set the errno appropriately */

errout_with_inode:
  inode_release(inode_ptr);
errout_with_path:
  free(fullpath);
errout:
  set_errno(ret);
  return VFS_ERROR;
}

