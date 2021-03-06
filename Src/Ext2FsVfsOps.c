/*	$NetBSD: ext2fs_vfsops.c,v 1.160 2011/06/12 03:36:00 rmind Exp $	*/

/*
 * Copyright (c) 1989, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ffs_vfsops.c	8.14 (Berkeley) 11/28/94
 * Modified for ext2fs by Manuel Bouyer.
 */

/*
 * Copyright (c) 1997 Manuel Bouyer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	@(#)ffs_vfsops.c	8.14 (Berkeley) 11/28/94
 * Modified for ext2fs by Manuel Bouyer.
 */

#ifndef _EXT2_TIANOCORE_SOURCE
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ext2fs_vfsops.c,v 1.160 2011/06/12 03:36:00 rmind Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/file.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/lock.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_dir.h>
#include <ufs/ext2fs/ext2fs_extern.h>

MODULE(MODULE_CLASS_VFS, ext2fs, "ffs");

int ext2fs_sbupdate(struct ufsmount *, int);
static int ext2fs_checksb(struct ext2fs *, int);

static struct sysctllog *ext2fs_sysctl_log;

extern const struct vnodeopv_desc ext2fs_vnodeop_opv_desc;
extern const struct vnodeopv_desc ext2fs_specop_opv_desc;
extern const struct vnodeopv_desc ext2fs_fifoop_opv_desc;

const struct vnodeopv_desc * const ext2fs_vnodeopv_descs[] = {
        &ext2fs_vnodeop_opv_desc,
        &ext2fs_specop_opv_desc,
        &ext2fs_fifoop_opv_desc,
        NULL,
};

struct vfsops ext2fs_vfsops = {
        MOUNT_EXT2FS,
        sizeof (struct ufs_args),
        ext2fs_mount,
        ufs_start,
        ext2fs_unmount,
        ufs_root,
        ufs_quotactl,
        ext2fs_statvfs,
        ext2fs_sync,
        ext2fs_vget,
        ext2fs_fhtovp,
        ext2fs_vptofh,
        ext2fs_init,
        ext2fs_reinit,
        ext2fs_done,
        ext2fs_mountroot,
        (int (*)(struct mount *, struct vnode *, struct timespec *)) eopnotsupp,
        vfs_stdextattrctl,
        (void *)eopnotsupp,     /* vfs_suspendctl */
        genfs_renamelock_enter,
        genfs_renamelock_exit,
        (void *)eopnotsupp,
        ext2fs_vnodeopv_descs,
        0,
        { NULL, NULL },
};

static const struct genfs_ops ext2fs_genfsops = {
        .gop_size = genfs_size,
        .gop_alloc = ext2fs_gop_alloc,
        .gop_write = genfs_gop_write,
        .gop_markupdate = ufs_gop_markupdate,
};

static const struct ufs_ops ext2fs_ufsops = {
        .uo_itimes = ext2fs_itimes,
        .uo_update = ext2fs_update,
        .uo_vfree = ext2fs_vfree,
        .uo_unmark_vnode = (void (*)(vnode_t *))nullop,
};

/* Fill in the inode uid/gid from ext2 halves.  */
void
ext2fs_set_inode_guid(struct inode *ip)
{

        ip->i_gid = ip->i_e2fs_gid;
        ip->i_uid = ip->i_e2fs_uid;
        if (ip->i_e2fs->e2fs.e2fs_rev > E2FS_REV0) {
                ip->i_gid |= ip->i_e2fs_gid_high << 16;
                ip->i_uid |= ip->i_e2fs_uid_high << 16;
        }
}

static int
ext2fs_modcmd(modcmd_t cmd, void *arg)
{
        int error;

        switch (cmd) {
        case MODULE_CMD_INIT:
                error = vfs_attach(&ext2fs_vfsops);
                if (error != 0)
                        break;
                sysctl_createv(&ext2fs_sysctl_log, 0, NULL, NULL,
                               CTLFLAG_PERMANENT,
                               CTLTYPE_NODE, "vfs", NULL,
                               NULL, 0, NULL, 0,
                               CTL_VFS, CTL_EOL);
                sysctl_createv(&ext2fs_sysctl_log, 0, NULL, NULL,
                               CTLFLAG_PERMANENT,
                               CTLTYPE_NODE, "ext2fs",
                               SYSCTL_DESCR("Linux EXT2FS file system"),
                               NULL, 0, NULL, 0,
                               CTL_VFS, 17, CTL_EOL);
                /*
                 * XXX the "17" above could be dynamic, thereby eliminating
                 * one more instance of the "number to vfs" mapping problem,
                 * but "17" is the order as taken from sys/mount.h
                 */
                break;
        case MODULE_CMD_FINI:
                error = vfs_detach(&ext2fs_vfsops);
                if (error != 0)
                        break;
                sysctl_teardown(&ext2fs_sysctl_log);
                break;
        default:
                error = ENOTTY;
                break;
        }

        return (error);
}

/*
 * XXX Same structure as FFS inodes?  Should we share a common pool?
 */
struct pool ext2fs_inode_pool;
struct pool ext2fs_dinode_pool;

extern u_long ext2gennumber;

void
ext2fs_init(void)
{

        pool_init(&ext2fs_inode_pool, sizeof(struct inode), 0, 0, 0,
            "ext2fsinopl", &pool_allocator_nointr, IPL_NONE);
        pool_init(&ext2fs_dinode_pool, sizeof(struct ext2fs_dinode), 0, 0, 0,
            "ext2dinopl", &pool_allocator_nointr, IPL_NONE);
        ufs_init();
}

void
ext2fs_reinit(void)
{
        ufs_reinit();
}

void
ext2fs_done(void)
{

        ufs_done();
        pool_destroy(&ext2fs_inode_pool);
        pool_destroy(&ext2fs_dinode_pool);
}
#else

/** @file

Modified for edk2

Copyright (c) 2011, Alin-Florin Rus-Rebreanu <alin@softwareliber.ro>

This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution. The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "Ext2.h"
#include "ext2fs_dir.h"
#include "inode.h"
#include "CompatibilityLayer.h"
#endif
//extern u_long ext2gennumber;

#define struct
int
ext2fs_mountfs(struct vnode *devvp, struct mount *mp);
#undef struct

static int
ext2fs_checksb(struct ext2fs *fs, int ronly);

#define struct
int vfs_rootmountalloc (int mext2fs, char *dev, struct mount **mp) {
#undef struct

    (*mp)->f_mntonname[0] = '/';
    (*mp)->f_mntonname[1] = '\0';

    return 0;

}

int
#define struct
ext2fs_mountroot(struct mount *mp)
{
	struct vnode *rootvp;
#undef struct
        struct m_ext2fs *fs;
//        struct mount *mp;
        int error;
        
        DEBUG ((EFI_D_INFO, "mountroot 1\n"));
        if (device_class(root_device) != DV_DISK)
    		return (ENONDEV);
        
        DEBUG ((EFI_D_INFO, "mountroot 2\n"));
        if ((error = vfs_rootmountalloc(MOUNT_EXT2FS, "root_device", &mp))) {
    		vrele(rootvp);
                return (error);
        }
        
        DEBUG ((EFI_D_INFO, "mountroot 3\n"));
        if ((error = ext2fs_mountfs(rootvp, mp)) != 0) {
    		vfs_unbusy(mp,false,NULL);
    		vfs_destroy(mp);
                return (error);
        }
      DEBUG ((EFI_D_INFO, "mountroot 4\n"));
        fs = mp->fs;
        memset(fs->e2fs_fsmnt, 0, sizeof(fs->e2fs_fsmnt));
        (void) copystr(mp->f_mntonname, fs->e2fs_fsmnt,
            sizeof(fs->e2fs_fsmnt) - 1, 0);
        if (fs->e2fs.e2fs_rev > E2FS_REV0) {
                memset(fs->e2fs.e2fs_fsmnt, 0, sizeof(fs->e2fs.e2fs_fsmnt));
                (void) copystr(mp->f_mntonname, fs->e2fs.e2fs_fsmnt,
                    sizeof(fs->e2fs.e2fs_fsmnt) - 1, 0);
        }
        vfs_unbusy(mp, false, NULL);
        return (0);
}

/*
 * Common code for mount and mountroot
 */
int
#define struct
ext2fs_mountfs(struct vnode *devvp, struct mount *mp)
#undef struct
{
	struct buf *bp;
	struct ext2fs *fs;
	struct m_ext2fs *m_fs;
	int error, i, ronly;

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;

      DEBUG ((EFI_D_INFO, "mountrootf 3\n"));
	bp = NULL;
	error = bread(devvp, SBLOCK, SBSIZE, cred, 0, &bp);
	if (error)
		goto out;
	fs = (struct ext2fs *)bp->b_data;
      DEBUG ((EFI_D_INFO, "mountrootf 4\n"));
	error = ext2fs_checksb(fs, ronly);
	if (error)
		goto out;
	mp->fs = malloc(sizeof(struct m_ext2fs), M_UFSMNT, M_WAITOK);
	memset(mp->fs, 0, sizeof(struct m_ext2fs));
	
      DEBUG ((EFI_D_INFO, "mountrootf 5\n"));
	e2fs_sbload((struct ext2fs *)bp->b_data, &mp->fs->e2fs);
	brelse(bp, 0);
	bp = NULL;
	m_fs = mp->fs;
	m_fs->e2fs_ronly = ronly;

#ifdef DEBUG_EXT2
	printf("ext2 ino size %zu\n", EXT2_DINODE_SIZE(m_fs));
#endif

      DEBUG ((EFI_D_INFO, "mountrootf 6\n"));
	if (ronly == 0) {
		if (m_fs->e2fs.e2fs_state == E2FS_ISCLEAN)
			m_fs->e2fs.e2fs_state = 0;
		else
			m_fs->e2fs.e2fs_state = E2FS_ERRORS;
		m_fs->e2fs_fmod = 1;
	}

      DEBUG ((EFI_D_INFO, "mountrootf 7\n"));
	/* compute dynamic sb infos */
	m_fs->e2fs_ncg =
	    howmany(m_fs->e2fs.e2fs_bcount - m_fs->e2fs.e2fs_first_dblock,
	    m_fs->e2fs.e2fs_bpg);
	m_fs->e2fs_fsbtodb = m_fs->e2fs.e2fs_log_bsize + LOG_MINBSIZE - DEV_BSHIFT;
	m_fs->e2fs_bsize = MINBSIZE << m_fs->e2fs.e2fs_log_bsize;
	m_fs->e2fs_bshift = LOG_MINBSIZE + m_fs->e2fs.e2fs_log_bsize;
	m_fs->e2fs_qbmask = m_fs->e2fs_bsize - 1;
	m_fs->e2fs_bmask = ~m_fs->e2fs_qbmask;
	m_fs->e2fs_ngdb =
	    howmany(m_fs->e2fs_ncg, m_fs->e2fs_bsize / sizeof(struct ext2_gd));
	m_fs->e2fs_ipb = m_fs->e2fs_bsize / EXT2_DINODE_SIZE(m_fs);
	m_fs->e2fs_itpg = m_fs->e2fs.e2fs_ipg / m_fs->e2fs_ipb;
	m_fs->e2fs_gd = malloc(m_fs->e2fs_ngdb * m_fs->e2fs_bsize,
	    M_UFSMNT, M_WAITOK);
	for (i = 0; i < m_fs->e2fs_ngdb; i++) {
	
      DEBUG ((EFI_D_INFO, "mountrootf 8\n"));
		error = bread(devvp ,
		    fsbtodb(m_fs, m_fs->e2fs.e2fs_first_dblock +
		    1 /* superblock */ + i),
		    m_fs->e2fs_bsize, NOCRED, 0, &bp);
		if (error) {
			free(m_fs->e2fs_gd, M_UFSMNT);
			goto out;
		}
		e2fs_cgload((struct ext2_gd *)bp->b_data,
		    &m_fs->e2fs_gd[
			i * m_fs->e2fs_bsize / sizeof(struct ext2_gd)],
		    m_fs->e2fs_bsize);
		brelse(bp, 0);
		bp = NULL;
	}
      DEBUG ((EFI_D_INFO, "mountrootf 9\n"));
	return (0);

out:
	brelse(bp, 0);
	return (error);
}


#define struct
int
ext2fs_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
#undef struct
{
        struct m_ext2fs *fs;
        struct inode *ip;
        struct buf *bp;
#define struct
        struct vnode *vp;
#undef struct
        int error;
        void *cp;

        error = getnewvnode (VT_EXT2FS, mp, ext2fs_vnodeop_p, NULL, &vp);
        if (error) {
	    *vpp = NULL;
	    return (error);
        }

	ip = pool_get(&ext2fs_inode_pool, PR_WAITOK);

        memset(ip, 0, sizeof(struct inode));
        vp->File = ip;
        ip->i_e2fs = fs = mp->fs;
        ip->i_number = ino;
        ip->i_e2fs_last_lblk = 0;
        ip->i_e2fs_last_blk = 0;
        ip->vp = &vp->EfiFile;

        /* Read in the disk contents for the inode, copy into the inode. */
        error = bread(vp, fsbtodb(fs, ino_to_fsba(fs, ino)),
            (int)fs->e2fs_bsize, NOCRED, 0, &bp);

        if (error) {

                /*
                 * The inode does not contain anything useful, so it would
                 * be misleading to leave it on its hash chain. With mode
                 * still zero, it will be unlinked and returned to the free
                 * list by vput().
                 */
                vput(vp);
                free (ip, 0);
                brelse(bp, 0);
                return (error);
        }

        cp = (char *)bp->b_data + (ino_to_fsbo(fs, ino) * EXT2_DINODE_SIZE(fs));
        ip->i_din.e2fs_din = malloc (sizeof(struct ext2fs_dinode), 0 ,0);
        e2fs_iload((struct ext2fs_dinode *)cp, ip->i_din.e2fs_din);
        brelse(bp, 0);
        
        ip->i_mode = ip->i_din.e2fs_din->e2di_mode;
        
        if ((ip->i_mode & IFMT) == IFDIR) {
    	    DEBUG((EFI_D_INFO, "VDIR ino %d\n",ip->i_number));
    	    vp->v_type = VDIR;
    	} else {
    	    DEBUG((EFI_D_INFO, "VREG ino %d\n",ip->i_number));
    	    vp->v_type = VREG;
    	}

        /* If the inode was deleted, reset all fields */
        if (ip->i_e2fs_dtime != 0) {
    	    ip->i_e2fs_mode = ip->i_e2fs_nblock = 0;
	    (void)ext2fs_setsize(ip, 0);
    	    memset(ip->i_e2fs_blocks, 0, sizeof(ip->i_e2fs_blocks));
        }

	*vpp = vp;
        return (0);
}

static int
ext2fs_checksb(struct ext2fs *fs, int ronly)
{

        if (fs2h16(fs->e2fs_magic) != E2FS_MAGIC) {
                return (EINVAL);                /* XXX needs translation */
        }
        if (fs2h32(fs->e2fs_rev) > E2FS_REV1) {
#ifdef DIAGNOSTIC
                printf("Ext2 fs: unsupported revision number: %x\n",
                    fs2h32(fs->e2fs_rev));
#endif
                return (EINVAL);                /* XXX needs translation */
        }
        if (fs2h32(fs->e2fs_log_bsize) > 2) { /* block size = 1024|2048|4096 */
#ifdef DIAGNOSTIC
                printf("Ext2 fs: bad block size: %d "
                    "(expected <= 2 for ext2 fs)\n",
                    fs2h32(fs->e2fs_log_bsize));
#endif
                return (EINVAL);           /* XXX needs translation */
        }
        if (fs2h32(fs->e2fs_rev) > E2FS_REV0) {
                if (fs2h32(fs->e2fs_first_ino) != EXT2_FIRSTINO) {
                        printf("Ext2 fs: unsupported first inode position\n");
                        return (EINVAL);      /* XXX needs translation */
                }
                if (fs2h32(fs->e2fs_features_incompat) &
                    ~EXT2F_INCOMPAT_SUPP) {
                        printf("Ext2 fs: unsupported optional feature\n");
                        return (EINVAL);      /* XXX needs translation */
                }
                if (!ronly && fs2h32(fs->e2fs_features_rocompat) &
                    ~EXT2F_ROCOMPAT_SUPP) {
                        return (EROFS);      /* XXX needs translation */
                }
        }
        return (0);
}

