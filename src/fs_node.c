#include "fs_node.h"
#include "path_util.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct fs_node {
	fs_node* parent;
	char* path;
	char* name;
	fs_node** children;
	size_t n_children;
	unsigned int ignored;
	unsigned int folded;
};

fs_node* fs_node_init(fs_node* parent, const char* path)
{
	fs_node* fsn = malloc(sizeof(*fsn));
	fsn->parent = parent;
	fsn->name = strdup(path);
	if (fsn->parent) {
		fsn->path = join_path(parent->path, path);
	} else {
		fsn->path = expand_path(path);
	}
	fsn->children = NULL;
	fsn->n_children = 0;
	DIR* dir = opendir(fsn->path);
	if (dir) {
		size_t index = 0;
		struct dirent* d;
		while ((d = readdir(dir))) {
			if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
				continue;
			fsn->n_children++;
			fsn->children =
			    realloc(fsn->children,
				    fsn->n_children * sizeof(*fsn->children));
			fsn->children[index++] = fs_node_init(fsn, d->d_name);
		}
		closedir(dir);
	}
	fsn->ignored = 0;
	fsn->folded = 0;
	return fsn;
}

void fs_node_free(fs_node* node)
{
	for (size_t i = 0; i < node->n_children; ++i) {
		fs_node_free(node->children[i]);
	}
	free(node->children);
	free(node->path);
	free(node->name);
	free(node);
}

fs_node* fs_node_match(fs_node* root, const char* path)
{
	fs_node* node = root;
	char* path_dup = expand_path(path);
	char* token_loc = path_dup;
	char* context;
	char* token = strtok_r(token_loc, "/", &context); // skip home
	/* skip user, get to first interesting one */
	for (int i = 0; i < 2; ++i)
		token = strtok_r(NULL, "/", &context);
	while (token) {
		unsigned int found = 0;
		for (size_t i = 0; i < node->n_children; ++i) {
			if (!strcmp(token, node->children[i]->name)) {
				node = node->children[i];
				found = 1;
				break;
			}
		}
		if (!found) {
			node = NULL;
			goto cleanup;
		}
		token = strtok_r(NULL, "/", &context);
	}
cleanup:
	free(path_dup);
	return node;
}

void fs_node_ignore_path(fs_node* root, const char* path)
{
	fs_node* node = fs_node_match(root, path);
	if (node)
		node->ignored = 1;
}

static void fs_node_blacklist_recursive(fs_node* node)
{
	node->ignored = 0;
	for (size_t i = 0; i < node->n_children; ++i) {
		fs_node_blacklist_recursive(node->children[i]);
	}
}

void fs_node_blacklist_path(fs_node* root, const char* path)
{
	fs_node* node = fs_node_match(root, path);
	if (node)
		fs_node_blacklist_recursive(node);
}

void fs_node_ignore_public_in_home(fs_node* root)
{
	for (size_t i = 0; i < root->n_children; ++i) {
		fs_node* child = root->children[i];
		if (child->name[0] != '.') {
			child->ignored = 1;
		}
	}
}

void fs_node_ignore_git_repos(fs_node* node)
{
	struct stat sb;
	// TODO errors
	lstat(node->path, &sb);
	if (S_ISDIR(sb.st_mode)) {
		if (!strcmp(node->name, ".git")) {
			if (node->parent)
				node->parent->ignored = 1;
		} else {
			for (size_t i = 0; i < node->n_children; ++i) {
				fs_node* child = node->children[i];
				fs_node_ignore_git_repos(child);
			}
		}
	}
}

void fs_node_ignore_symlinks_in_home(fs_node* root)
{
	struct stat sb;
	for (size_t i = 0; i < root->n_children; ++i) {
		fs_node* child = root->children[i];
		// TODO fix errors
		lstat(child->path, &sb);
		if (S_ISLNK(sb.st_mode)) {
			child->ignored = 1;
		}
	}
}

void fs_node_ignore_dotfiles_symlinks(fs_node* node)
{
	struct stat sb;
	// TODO fix errors
	lstat(node->path, &sb);
	if (S_ISLNK(sb.st_mode)) {
		char buf[1024];
		ssize_t len = readlink(node->path, buf, sizeof(buf) - 1);
		buf[len] = '\0';
		unsigned int had_match = strstr(buf, "dotfiles") != NULL;
		if (had_match) {
			node->ignored = 1;
			return;
		}
	}

	for (size_t i = 0; i < node->n_children; ++i) {
		fs_node* child = node->children[i];
		fs_node_ignore_dotfiles_symlinks(child);
	}
}

void fs_node_propagate_folded(fs_node* node)
{
	if (!node->n_children) {
		node->folded = !node->ignored;
	} else {
		for (size_t i = 0; i < node->n_children; ++i) {
			fs_node* child = node->children[i];
			fs_node_propagate_folded(child);
		}
		unsigned int all_folded = 1;
		for (size_t i = 0; i < node->n_children; ++i) {
			fs_node* child = node->children[i];
			if (!child->folded) {
				all_folded = 0;
				break;
			}
		}
		node->folded = all_folded;
	}
}

void fs_node_propagate_ignored(fs_node* node)
{
	for (size_t i = 0; i < node->n_children; ++i) {
		fs_node* child = node->children[i];
		if (node->ignored)
			child->ignored = 1;
		fs_node_propagate_ignored(child);
	}
}

static int fs_node_cmp(const void* p1, const void* p2)
{
	const fs_node* const fsn1 = *(const fs_node* const*)p1;
	const fs_node* const fsn2 = *(const fs_node* const*)p2;
	return strcmp(fsn1->path, fsn2->path);
}

void fs_node_print(fs_node* node)
{
	if (node->ignored)
		return;

	if (node->folded) {
		printf("%s", node->path);
		struct stat sb;
		lstat(node->path, &sb);
		if (S_ISDIR(sb.st_mode)) {
			printf("/");
		}
		printf("\n");
	} else {
		qsort(node->children, node->n_children, sizeof(*node->children),
		      fs_node_cmp);
		for (size_t i = 0; i < node->n_children; ++i) {
			fs_node* child = node->children[i];
			fs_node_print(child);
		}
	}
}
