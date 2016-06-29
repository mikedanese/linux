#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "internal.h"
#include "mount.h"

// guarded by mntns_list_lock
static char buff[PATH_MAX];

// visit each mntns, preorder traverse the mount tree printing juicy details
static int mntns_tree_show(struct seq_file *m, void *v)
{
	struct mnt_namespace *mntns;
	LIST_HEAD(mnt_stack);
	struct mount *cur, *child;
	int depth = 0, i = 0;

	spin_lock(&mntns_list_lock);
	list_for_each_entry(mntns, &mntns_list, mntns_list) {
		seq_printf(m, "mnt:[%u] ->\n", mntns->ns.inum);

		mntns->root->visited = false;
		list_add(&mntns->root->mnt_stack, &mnt_stack);
		while (!list_empty(&mnt_stack)) {
			cur = list_first_entry(&mnt_stack, struct mount, mnt_stack);
			if (cur->visited) {
				list_del(&cur->mnt_stack);
				depth--;
				continue;
			}

			seq_printf(m, "\t%d\t", cur->mnt_id);
			for (i = 0; i < depth; i++)
				if (i == depth - 1)
					seq_printf(m, "-> ");
				else
					seq_printf(m, "---");
			seq_printf(m, dentry_path(cur->mnt_mountpoint, buff, PATH_MAX));
			seq_printf(m, "\n");
			
			cur->visited = true;
			depth++;

			list_for_each_entry(child, &cur->mnt_mounts, mnt_child) {
				child->visited = false;
				list_add(&child->mnt_stack, &mnt_stack);
			}
		}
	}
	spin_unlock(&mntns_list_lock);

	return 0;
}

static int mntns_tree_open(struct inode *inode, struct file *file)
{
	return single_open(file, &mntns_tree_show, NULL);
}

static const struct file_operations mntns_tree_file_operations = {
	.open		= mntns_tree_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int __init mntns_tree_module_init(void)
{
	proc_create("mntns_tree", 0444, NULL, &mntns_tree_file_operations);
	return 0;
}
module_init(mntns_tree_module_init);

static void __exit mntns_tree_module_exit(void)
{
	remove_proc_entry("mntns_tree", NULL);
}
module_exit(mntns_tree_module_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("mntns_tree");
