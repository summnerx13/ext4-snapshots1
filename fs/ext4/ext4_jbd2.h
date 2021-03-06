/*
 * ext4_jbd2.h
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>, 1999
 *
 * Copyright 1998--1999 Red Hat corp --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Ext4-specific journaling extensions.
 *
 * Snapshot extra COW credits, Amir Goldstein <amir73il@users.sf.net>, 2011
 */

#ifndef _EXT4_JBD2_H
#define _EXT4_JBD2_H

#include <linux/fs.h>
#include <linux/jbd2.h>
#include "ext4.h"
#include "snapshot.h"

#define EXT4_JOURNAL(inode)	(EXT4_SB((inode)->i_sb)->s_journal)

/* Define the number of blocks we need to account to a transaction to
 * modify one block of data.
 *
 * We may have to touch one inode, one bitmap buffer, up to three
 * indirection blocks, the group and superblock summaries, and the data
 * block to complete the transaction.
 *
 * For extents-enabled fs we may have to allocate and modify up to
 * 5 levels of tree + root which are stored in the inode. */

#define EXT4_SINGLEDATA_TRANS_BLOCKS(sb)				\
	(EXT4_HAS_INCOMPAT_FEATURE(sb, EXT4_FEATURE_INCOMPAT_EXTENTS)   \
	 ? 27U : 8U)

/* Extended attribute operations touch at most two data buffers,
 * two bitmap buffers, and two group summaries, in addition to the inode
 * and the superblock, which are already accounted for. */

#define EXT4_XATTR_TRANS_BLOCKS		6U

/* Define the minimum size for a transaction which modifies data.  This
 * needs to take into account the fact that we may end up modifying two
 * quota files too (one for the group, one for the user quota).  The
 * superblock only gets updated once, of course, so don't bother
 * counting that again for the quota updates. */

#define EXT4_DATA_TRANS_BLOCKS(sb)	(EXT4_SINGLEDATA_TRANS_BLOCKS(sb) + \
					 EXT4_XATTR_TRANS_BLOCKS - 2 + \
					 EXT4_MAXQUOTAS_TRANS_BLOCKS(sb))

/*
 * Define the number of metadata blocks we need to account to modify data.
 *
 * This include super block, inode block, quota blocks and xattr blocks
 */
#define EXT4_META_TRANS_BLOCKS(sb)	(EXT4_XATTR_TRANS_BLOCKS + \
					EXT4_MAXQUOTAS_TRANS_BLOCKS(sb))

/* Delete operations potentially hit one directory's namespace plus an
 * entire inode, plus arbitrary amounts of bitmap/indirection data.  Be
 * generous.  We can grow the delete transaction later if necessary. */

#define EXT4_DELETE_TRANS_BLOCKS(sb)	(2 * EXT4_DATA_TRANS_BLOCKS(sb) + 64)

/* Define an arbitrary limit for the amount of data we will anticipate
 * writing to any given transaction.  For unbounded transactions such as
 * write(2) and truncate(2) we can write more than this, but we always
 * start off at the maximum transaction size and grow the transaction
 * optimistically as we go. */

#define EXT4_MAX_TRANS_DATA		64U

/* We break up a large truncate or write transaction once the handle's
 * buffer credits gets this low, we need either to extend the
 * transaction or to start a new one.  Reserve enough space here for
 * inode, bitmap, superblock, group and indirection updates for at least
 * one block, plus two quota updates.  Quota allocations are not
 * needed. */

#ifdef CONFIG_EXT4_FS_SNAPSHOT_JOURNAL_CREDITS
/* on block write we have to journal the block itself */
#define EXT4_WRITE_CREDITS 1
/* on snapshot block alloc we have to journal block group bitmap, exclude
   bitmap and gdb */
#define EXT4_ALLOC_CREDITS 3
/* number of credits for COW bitmap operation (allocated blocks are not
   journalled): alloc(dind+ind+cow) = 9 */
#define EXT4_COW_BITMAP_CREDITS	(3*EXT4_ALLOC_CREDITS)
/* number of credits for other block COW operations:
   alloc(dind+ind+cow)+write(dind+ind) = 11 */
#define EXT4_COW_BLOCK_CREDITS	(3*EXT4_ALLOC_CREDITS+2*EXT4_WRITE_CREDITS)
/* number of credits for the first COW operation in the block group, which
 * is not the first group in a flex group (alloc 2 dind blocks):
   9+11 = 20 */
#define EXT4_COW_CREDITS	(EXT4_COW_BLOCK_CREDITS +	\
				 EXT4_COW_BITMAP_CREDITS)
/* number of credits for snapshot operations counted once per transaction:
   write(sb+inode+tind) = 3 */
#define EXT4_SNAPSHOT_CREDITS	(3*EXT4_WRITE_CREDITS)
/*
 * in total, for N COW operations, we may have to journal 20N+3 blocks,
 * and we also want to reserve 20+3 credits for the last COW operation,
 * so we add 20(N-1)+3+(20+3) to the requested N buffer credits
 * and request 21N+6 buffer credits.
 * that's a lot of extra credits and much more then needed for the common
 * case, but what can we do?
 *
 * we are going to need a bigger journal to accommodate the
 * extra snapshot credits.
 * mke2fs -j uses the following default formula for fs-size above 1G:
 * journal-size = MIN(128M, fs-size/32)
 * mke2fs -j -J big uses the following formula:
 * journal-size = MIN(3G, fs-size/32)
 */
#define EXT4_SNAPSHOT_TRANS_BLOCKS(n) \
	((n)*(1+EXT4_COW_CREDITS)+EXT4_SNAPSHOT_CREDITS)
#define EXT4_SNAPSHOT_START_TRANS_BLOCKS(n) \
	((n)*(1+EXT4_COW_CREDITS)+2*EXT4_SNAPSHOT_CREDITS)

/*
 * check for sufficient buffer and COW credits
 */
#define EXT4_SNAPSHOT_HAS_TRANS_BLOCKS(handle, n)			\
	((handle)->h_buffer_credits >= EXT4_SNAPSHOT_TRANS_BLOCKS(n) && \
	 (handle)->h_user_credits >= (n))

#define EXT4_RESERVE_COW_CREDITS	(EXT4_COW_CREDITS +		\
					 EXT4_SNAPSHOT_CREDITS)

/*
 * Ext4 is not designed for filesystems under 4G with journal size < 128M
 * Recommended journal size is 3G (created with 'mke2fs -j -J big')
 */
#define EXT4_MIN_JOURNAL_BLOCKS	32768U
#define EXT4_BIG_JOURNAL_BLOCKS	(24*EXT4_MIN_JOURNAL_BLOCKS)

#endif
#define EXT4_RESERVE_TRANS_BLOCKS	12U

#define EXT4_INDEX_EXTRA_TRANS_BLOCKS	8

#ifdef CONFIG_QUOTA
/* Amount of blocks needed for quota update - we know that the structure was
 * allocated so we need to update only data block */
#define EXT4_QUOTA_TRANS_BLOCKS(sb) (test_opt(sb, QUOTA) ? 1 : 0)
/* Amount of blocks needed for quota insert/delete - we do some block writes
 * but inode, sb and group updates are done only once */
#define EXT4_QUOTA_INIT_BLOCKS(sb) (test_opt(sb, QUOTA) ? (DQUOT_INIT_ALLOC*\
		(EXT4_SINGLEDATA_TRANS_BLOCKS(sb)-3)+3+DQUOT_INIT_REWRITE) : 0)

#define EXT4_QUOTA_DEL_BLOCKS(sb) (test_opt(sb, QUOTA) ? (DQUOT_DEL_ALLOC*\
		(EXT4_SINGLEDATA_TRANS_BLOCKS(sb)-3)+3+DQUOT_DEL_REWRITE) : 0)
#else
#define EXT4_QUOTA_TRANS_BLOCKS(sb) 0
#define EXT4_QUOTA_INIT_BLOCKS(sb) 0
#define EXT4_QUOTA_DEL_BLOCKS(sb) 0
#endif
#define EXT4_MAXQUOTAS_TRANS_BLOCKS(sb) (MAXQUOTAS*EXT4_QUOTA_TRANS_BLOCKS(sb))
#define EXT4_MAXQUOTAS_INIT_BLOCKS(sb) (MAXQUOTAS*EXT4_QUOTA_INIT_BLOCKS(sb))
#define EXT4_MAXQUOTAS_DEL_BLOCKS(sb) (MAXQUOTAS*EXT4_QUOTA_DEL_BLOCKS(sb))

int
ext4_mark_iloc_dirty(handle_t *handle,
		     struct inode *inode,
		     struct ext4_iloc *iloc);

/*
 * On success, We end up with an outstanding reference count against
 * iloc->bh.  This _must_ be cleaned up later.
 */

int ext4_reserve_inode_write(handle_t *handle, struct inode *inode,
			struct ext4_iloc *iloc);

int ext4_mark_inode_dirty(handle_t *handle, struct inode *inode);

/*
 * Wrapper functions with which ext4 calls into JBD.
 */
void ext4_journal_abort_handle(const char *caller, unsigned int line,
			       const char *err_fn,
		struct buffer_head *bh, handle_t *handle, int err);

#ifdef CONFIG_EXT4_FS_SNAPSHOT_HOOKS_BITMAP
int __ext4_handle_get_bitmap_access(const char *where, unsigned int line,
				    handle_t *handle, struct super_block *sb,
				    ext4_group_t group, struct buffer_head *bh);
#else
int __ext4_journal_get_undo_access(const char *where, unsigned int line,
				   handle_t *handle, struct buffer_head *bh);
#endif

#ifdef CONFIG_EXT4_FS_SNAPSHOT_HOOKS_JBD
int __ext4_journal_get_write_access_inode(const char *where, unsigned int line,
					 handle_t *handle, struct inode *inode,
					 struct buffer_head *bh, int exclude);
#else
int __ext4_journal_get_write_access(const char *where, unsigned int line,
				    handle_t *handle, struct buffer_head *bh);

#endif
int __ext4_forget(const char *where, unsigned int line, handle_t *handle,
		  int is_metadata, struct inode *inode,
		  struct buffer_head *bh, ext4_fsblk_t blocknr);

int __ext4_journal_get_create_access(const char *where, unsigned int line,
				handle_t *handle, struct buffer_head *bh);

int __ext4_handle_dirty_metadata(const char *where, unsigned int line,
				 handle_t *handle, struct inode *inode,
				 struct buffer_head *bh);

int __ext4_handle_dirty_super(const char *where, unsigned int line,
			      handle_t *handle, struct super_block *sb);

#ifdef CONFIG_EXT4_FS_SNAPSHOT_HOOKS_BITMAP
#define ext4_handle_get_bitmap_access(handle, sb, group, bh) \
	__ext4_handle_get_bitmap_access(__func__, __LINE__, \
					(handle), (sb), (group), (bh))
#else
#define ext4_journal_get_undo_access(handle, bh) \
	__ext4_journal_get_undo_access(__func__, __LINE__, (handle), (bh))
#endif
#ifdef CONFIG_EXT4_FS_SNAPSHOT_HOOKS_JBD
#define ext4_journal_get_write_access_exclude(handle, bh) \
	__ext4_journal_get_write_access_inode(__func__, __LINE__, \
						 (handle), NULL, (bh), 1)
#define ext4_journal_get_write_access(handle, bh) \
	__ext4_journal_get_write_access_inode(__func__, __LINE__, \
						 (handle), NULL, (bh), 0)
#define ext4_journal_get_write_access_inode(handle, inode, bh) \
	__ext4_journal_get_write_access_inode(__func__, __LINE__, \
						(handle), (inode), (bh), 0)
#else
#define ext4_journal_get_write_access(handle, bh) \
	__ext4_journal_get_write_access(__func__, __LINE__, (handle), (bh))
#endif
#define ext4_forget(handle, is_metadata, inode, bh, block_nr) \
	__ext4_forget(__func__, __LINE__, (handle), (is_metadata), (inode), \
		      (bh), (block_nr))
#define ext4_journal_get_create_access(handle, bh) \
	__ext4_journal_get_create_access(__func__, __LINE__, (handle), (bh))
#define ext4_handle_dirty_metadata(handle, inode, bh) \
	__ext4_handle_dirty_metadata(__func__, __LINE__, (handle), (inode), \
				     (bh))
#define ext4_handle_dirty_super(handle, sb) \
	__ext4_handle_dirty_super(__func__, __LINE__, (handle), (sb))

#ifdef CONFIG_EXT4_FS_SNAPSHOT_BLOCK
#ifdef CONFIG_EXT4_FS_SNAPSHOT_JOURNAL_TRACE
/*
 * macros for ext4 to update transaction COW statistics.
 * if the kernel was compiled without CONFIG_JBD2_DEBUG
 * then the h_cow_* fields are not allocated in handle objects.
 */
#ifdef CONFIG_JBD2_DEBUG
#define trace_cow_add(handle, name, num)	\
	(handle)->h_cow_##name += (num)
#define trace_cow_inc(handle, name)		\
	(handle)->h_cow_##name++;

#else
#define trace_cow_add(handle, name, num)
#define trace_cow_inc(handle, name)

#endif
#else
#define trace_cow_add(handle, name, num)
#define trace_cow_inc(handle, name)

#endif
#endif
#ifdef CONFIG_EXT4_FS_SNAPSHOT_JOURNAL_CREDITS
#ifdef CONFIG_EXT4_FS_SNAPSHOT_JOURNAL_TRACE
#ifdef CONFIG_EXT4_DEBUG
void __ext4_journal_trace(int debug, const char *fn, const char *caller,
		handle_t *handle, int nblocks);

#define ext4_journal_trace(n, caller, handle, nblocks)			\
	do {								\
		if ((n) <= snapshot_enable_debug)			\
			__ext4_journal_trace((n), __func__, (caller),	\
						(handle), (nblocks));	\
	} while (0)

#else
#define ext4_journal_trace(n, caller, handle, nblocks)
#endif
#else
#define ext4_journal_trace(n, caller, handle, nblocks)
#endif

handle_t *__ext4_journal_start(const char *where,
		struct super_block *sb, int nblocks);

#define ext4_journal_start_sb(sb, nblocks) \
	__ext4_journal_start(__func__, \
			(sb), (nblocks))

#define ext4_journal_start(inode, nblocks) \
	__ext4_journal_start(__func__, \
			(inode)->i_sb, (nblocks))

#else
handle_t *ext4_journal_start_sb(struct super_block *sb, int nblocks);
#endif
int __ext4_journal_stop(const char *where, unsigned int line, handle_t *handle);

#define EXT4_NOJOURNAL_MAX_REF_COUNT ((unsigned long) 4096)

/* Note:  Do not use this for NULL handles.  This is only to determine if
 * a properly allocated handle is using a journal or not. */
static inline int ext4_handle_valid(handle_t *handle)
{
	if ((unsigned long)handle < EXT4_NOJOURNAL_MAX_REF_COUNT)
		return 0;
	return 1;
}

static inline void ext4_handle_sync(handle_t *handle)
{
	if (ext4_handle_valid(handle))
		handle->h_sync = 1;
}

#ifdef CONFIG_EXT4_FS_SNAPSHOT_JOURNAL_RELEASE
int __ext4_handle_release_buffer(const char *where, handle_t *handle,
				struct buffer_head *bh);

#define ext4_handle_release_buffer(handle, bh) \
	__ext4_handle_release_buffer(__func__, (handle), (bh))

#else
static inline void ext4_handle_release_buffer(handle_t *handle,
						struct buffer_head *bh)
{
	if (ext4_handle_valid(handle))
		jbd2_journal_release_buffer(handle, bh);
}

#endif
static inline int ext4_handle_is_aborted(handle_t *handle)
{
	if (ext4_handle_valid(handle))
		return is_handle_aborted(handle);
	return 0;
}

static inline int ext4_handle_has_enough_credits(handle_t *handle, int needed)
{
#ifdef CONFIG_EXT4_FS_SNAPSHOT_JOURNAL_CREDITS
	struct super_block *sb;

	if (!ext4_handle_valid(handle))
		return 1;

	sb = handle->h_transaction->t_journal->j_private;
	if (EXT4_SNAPSHOTS(sb))
		return EXT4_SNAPSHOT_HAS_TRANS_BLOCKS(handle, needed);
	/* sb has no snapshot feature */
	if (handle->h_buffer_credits < needed)
#else
	if (ext4_handle_valid(handle) && handle->h_buffer_credits < needed)
#endif
		return 0;
	return 1;
}
#ifndef CONFIG_EXT4_FS_SNAPSHOT_JOURNAL_CREDITS

static inline handle_t *ext4_journal_start(struct inode *inode, int nblocks)
{
	return ext4_journal_start_sb(inode->i_sb, nblocks);
}
#endif

#define ext4_journal_stop(handle) \
	__ext4_journal_stop(__func__, __LINE__, (handle))

static inline handle_t *ext4_journal_current_handle(void)
{
	return journal_current_handle();
}

#ifdef CONFIG_EXT4_FS_SNAPSHOT_JOURNAL_CREDITS
/*
 * Ext4 wrapper for journal_extend()
 * When transaction runs out of buffer credits it is possible to try and
 * extend the buffer credits without restarting the transaction.
 * Ext4 wrapper for journal_start() has increased the user requested buffer
 * credits to include the extra credits for COW operations.
 * This wrapper checks the remaining user credits and how many COW credits
 * are missing and then tries to extend the transaction.
 */
static inline int __ext4_journal_extend(const char *where,
					handle_t *handle, int nblocks)
{
	int credits = 0;
	int err = 0;
	struct super_block *sb;

	if (!ext4_handle_valid((handle_t *)handle))
		return 0;

	credits = nblocks;
	sb = handle->h_transaction->t_journal->j_private;
	if (EXT4_SNAPSHOTS(sb)) {
		/* extend transaction to valid buffer/user credits ratio */
		credits = EXT4_SNAPSHOT_TRANS_BLOCKS(handle->h_user_credits +
			nblocks) - handle->h_buffer_credits;
	}
	if (credits > 0)
		err = jbd2_journal_extend((handle_t *)handle, credits);
	if (EXT4_SNAPSHOTS(sb) && !err) {
		/* update base/user credits for future extends */
		handle->h_base_credits += nblocks;
		handle->h_user_credits += nblocks;
		ext4_journal_trace(SNAP_WARN, where, handle, nblocks);
	}
	return err;
}

/*
 * Ext4 wrapper for journal_restart()
 * When transaction runs out of buffer credits and cannot be extended,
 * the alternative is to restart it (start a new transaction).
 * This wrapper increases the user requested buffer credits to include the
 * extra credits for COW operations.
 */
static inline int __ext4_journal_restart(const char *where,
					 handle_t *handle, int nblocks)
{
	int err = 0;
	int credits = 0;
	struct super_block *sb;

	if (!ext4_handle_valid((handle_t *)handle))
		return 0;

	sb = handle->h_transaction->t_journal->j_private;
	credits = EXT4_SNAPSHOTS(sb) ?
		  EXT4_SNAPSHOT_START_TRANS_BLOCKS(nblocks) : nblocks;
	err = jbd2_journal_restart((handle_t *)handle, credits);
	if (EXT4_SNAPSHOTS(sb) && !err) {
		handle->h_base_credits = nblocks;
		handle->h_user_credits = nblocks;
		ext4_journal_trace(SNAP_WARN, where, handle, nblocks);
	}
	return err;
}

#define ext4_journal_extend(handle, nblocks) \
	__ext4_journal_extend(__func__, (handle), (nblocks))

#define ext4_journal_restart(handle, nblocks) \
	__ext4_journal_restart(__func__, (handle), (nblocks))
#else
static inline int ext4_journal_extend(handle_t *handle, int nblocks)
{
	if (ext4_handle_valid(handle))
		return jbd2_journal_extend(handle, nblocks);
	return 0;
}

static inline int ext4_journal_restart(handle_t *handle, int nblocks)
{
	if (ext4_handle_valid(handle))
		return jbd2_journal_restart(handle, nblocks);
	return 0;
}

#endif
static inline int ext4_journal_blocks_per_page(struct inode *inode)
{
	if (EXT4_JOURNAL(inode) != NULL)
		return jbd2_journal_blocks_per_page(inode);
	return 0;
}

static inline int ext4_journal_force_commit(journal_t *journal)
{
	if (journal)
		return jbd2_journal_force_commit(journal);
	return 0;
}

static inline int ext4_jbd2_file_inode(handle_t *handle, struct inode *inode)
{
	if (ext4_handle_valid(handle))
		return jbd2_journal_file_inode(handle, EXT4_I(inode)->jinode);
	return 0;
}

static inline void ext4_update_inode_fsync_trans(handle_t *handle,
						 struct inode *inode,
						 int datasync)
{
	struct ext4_inode_info *ei = EXT4_I(inode);

	if (ext4_handle_valid(handle)) {
		ei->i_sync_tid = handle->h_transaction->t_tid;
		if (datasync)
			ei->i_datasync_tid = handle->h_transaction->t_tid;
	}
}

/* super.c */
int ext4_force_commit(struct super_block *sb);

static inline int ext4_should_journal_data(struct inode *inode)
{
	if (EXT4_JOURNAL(inode) == NULL)
		return 0;
	if (!S_ISREG(inode->i_mode))
		return 1;
#ifdef CONFIG_EXT4_FS_SNAPSHOT
	if (EXT4_SNAPSHOTS(inode->i_sb))
		/* snapshots enforce ordered data */
		return 0;
#endif
	if (test_opt(inode->i_sb, DATA_FLAGS) == EXT4_MOUNT_JOURNAL_DATA)
		return 1;
	if (ext4_test_inode_flag(inode, EXT4_INODE_JOURNAL_DATA))
		return 1;
	return 0;
}

static inline int ext4_should_order_data(struct inode *inode)
{
	if (EXT4_JOURNAL(inode) == NULL)
		return 0;
	if (!S_ISREG(inode->i_mode))
		return 0;
#ifdef CONFIG_EXT4_FS_SNAPSHOT
	if (EXT4_SNAPSHOTS(inode->i_sb))
		/* snapshots enforce ordered data */
		return 1;
#endif
	if (ext4_test_inode_flag(inode, EXT4_INODE_JOURNAL_DATA))
		return 0;
	if (test_opt(inode->i_sb, DATA_FLAGS) == EXT4_MOUNT_ORDERED_DATA)
		return 1;
	return 0;
}

static inline int ext4_should_writeback_data(struct inode *inode)
{
	if (EXT4_JOURNAL(inode) == NULL)
		return 1;
#ifdef CONFIG_EXT4_FS_SNAPSHOT
	if (EXT4_SNAPSHOTS(inode->i_sb))
		/* snapshots enforce ordered data */
		return 0;
#endif
	if (!S_ISREG(inode->i_mode))
		return 0;
	if (ext4_test_inode_flag(inode, EXT4_INODE_JOURNAL_DATA))
		return 0;
	if (test_opt(inode->i_sb, DATA_FLAGS) == EXT4_MOUNT_WRITEBACK_DATA)
		return 1;
	return 0;
}

/*
 * This function controls whether or not we should try to go down the
 * dioread_nolock code paths, which makes it safe to avoid taking
 * i_mutex for direct I/O reads.  This only works for extent-based
 * files, and it doesn't work if data journaling is enabled, since the
 * dioread_nolock code uses b_private to pass information back to the
 * I/O completion handler, and this conflicts with the jbd's use of
 * b_private.
 */
static inline int ext4_should_dioread_nolock(struct inode *inode)
{
	if (!test_opt(inode->i_sb, DIOREAD_NOLOCK))
		return 0;
	if (!S_ISREG(inode->i_mode))
		return 0;
#ifdef CONFIG_EXT4_FS_SNAPSHOT
	if (EXT4_SNAPSHOTS(inode->i_sb))
		/* XXX: should snapshots support dioread_nolock? */
		return 0;
#endif
	if (!(ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS)))
		return 0;
	if (ext4_should_journal_data(inode))
		return 0;
	return 1;
}

#ifdef CONFIG_EXT4_FS_SNAPSHOT
#ifdef CONFIG_EXT4_FS_SNAPSHOT_HOOKS_DATA
/*
 * check if @inode data blocks should be moved-on-write
 */
static inline int ext4_snapshot_should_move_data(struct inode *inode)
{
	if (!EXT4_SNAPSHOTS(inode->i_sb))
		return 0;
	if (EXT4_JOURNAL(inode) == NULL)
		return 0;
#ifdef CONFIG_EXT4_FS_SNAPSHOT_FILE
	if (ext4_snapshot_excluded(inode))
		return 0;
#endif
#ifndef CONFIG_EXT4_FS_SNAPSHOT_HOOKS_EXTENT
	if (ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))
		return 0;
#endif
	/* when a data block is journaled, it is already COWed as metadata */
	if (ext4_should_journal_data(inode))
		return 0;
	return 1;
}

#endif
#endif
#endif	/* _EXT4_JBD2_H */
