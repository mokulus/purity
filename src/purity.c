#include "dirlist.h"
#include "path_util.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fts.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void usage(const char *arg0);
void change_dir(const char *path);
dirlist *dirlist_read_file(const char *filename);

dirlist *dirlist_file(const char *path);
int str_starts_with(const char *haystack, const char *needle);

int main(int argc, char *argv[])
{
	char *whitelist_path = NULL;
	char *blacklist_path = NULL;

	int ch;
	while ((ch = getopt(argc, argv, "w:b:")) != -1) {
		switch (ch) {
		case 'w':
			whitelist_path = optarg;
			break;
		case 'b':
			blacklist_path = optarg;
			break;
		default:
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	if (optind != argc) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	dirlist *whitelist = NULL;
	dirlist *blacklist = NULL;
	char *home = NULL;
	FTS *fts = NULL;
	dirlist *stack = NULL;

	whitelist = dirlist_file(whitelist_path);
	if (!whitelist)
		goto fail;
	blacklist = dirlist_file(blacklist_path);
	if (!blacklist)
		goto fail;
	home = expand_path("~");
	if (!home)
		goto fail;
	fts = fts_open((char *const[]){home, NULL}, FTS_PHYSICAL, NULL);
	if (!fts)
		goto fail;
	stack = calloc(1, sizeof(*stack));
	if (!stack)
		goto fail;
	short whitelist_prune_level = -1;
	short whitelist_parent = -1;
	for (;;) {
		FTSENT *ent = fts_read(fts);
		if (!ent) {
			if (errno == 0) {
				break;
			} else {
				perror("fts_read");
				goto fail;
			}
		}
		if (whitelist_prune_level == ent->fts_level)
			whitelist_prune_level = -1;
		if (ent->fts_info == FTS_DP || ent->fts_info == FTS_DNR) {
			size_t nchildren = 0;
			// can't use fts_children here because last fts_read
			// was the last child of this directory
			DIR *dir = opendir(ent->fts_path);
			if (dir) {
				while (readdir(dir))
					nchildren++;
				closedir(dir);
				nchildren -= 2; // remove . and ..
			} else {
				fprintf(stderr, "opendir %s: %s\n",
					ent->fts_path, strerror(errno));
				nchildren = -1;
			}
			size_t start_index;
			for (start_index = stack->len - 1;
			     stack->paths[start_index]; --start_index)
				;
			size_t nmarked = stack->len - 1 - start_index;
			if (whitelist_parent != ent->fts_level &&
			    nmarked != nchildren) {
				// some were good, print the bad ones, if any
				for (size_t i = start_index + 1; i < stack->len;
				     ++i)
					puts(stack->paths[i]);
			}
			for (size_t i = start_index; i < stack->len; ++i)
				free(stack->paths[i]);
			stack->len = start_index;
			if (whitelist_parent != ent->fts_level &&
			    nmarked == nchildren) {
				// all were bad, mark this one as bad in the
				// previous frame, so only after freeing
				dirlist_add(stack, strdup(ent->fts_path));
			}
			if (whitelist_parent == ent->fts_level)
				whitelist_parent = -1;
			continue;
		}

		if (ent->fts_info == FTS_D) {
			// mark new frame for new directory
			dirlist_add(stack, NULL);
		}

		if (whitelist_prune_level > ent->fts_level ||
		    whitelist_prune_level == -1) {
			int windex = dirlist_search(whitelist, ent->fts_path);
			if (windex != -1 &&
			    str_starts_with(ent->fts_path,
					    whitelist->paths[windex])) {
				fts_set(fts, ent, FTS_SKIP);
				continue;
			} else {
				int could_get_match = 0;
				for (size_t i = 0; i < whitelist->len; ++i) {
					if (str_starts_with(whitelist->paths[i],
							    ent->fts_path)) {
						could_get_match = 1;
						break;
					}
				}
				if (!could_get_match) {
					whitelist_prune_level = ent->fts_level;
				}
			}
		} else {
			/* fprintf(stderr, "pruned %s\n", ent->fts_path); */
		}

		int bindex = dirlist_search(blacklist, ent->fts_path);
		if (bindex != -1 &&
		    str_starts_with(ent->fts_path, blacklist->paths[bindex])) {
			puts(ent->fts_path);
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		/* int is_git_repo = 0; */
		/* for (FTSENT *link = fts_children(fts, FTS_NAMEONLY); link; */
		/*      link = link->fts_link) { */
		/* 	if (!strcmp(link->fts_name, ".git")) { */
		/* 		is_git_repo = 1; */
		/* 		break; */
		/* 	} */
		/* } */
		/* if (is_git_repo) { */
		/* 	/1* printf("Git repo: %s\n", ent->fts_path); *1/ */
		/* 	fts_set(fts, ent, FTS_SKIP); */
		/* 	continue; */
		/* } */
		if (!strcmp(ent->fts_name, ".git")) {
			whitelist_parent = ent->fts_level - 1;
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		/* public in home */
		if (ent->fts_level == 1 && ent->fts_name[0] != '.') {
			/* printf("Public in home: %s\n", ent->fts_path); */
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		/* symlinks in home */
		if (ent->fts_level == 1 && S_ISLNK(ent->fts_statp->st_mode)) {
			/* printf("Symlink in home: %s\n", ent->fts_path); */
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}
		/* dotfiles */
		if (S_ISLNK(ent->fts_statp->st_mode) &&
		    strstr(ent->fts_path, "dotfiles")) {
			/* printf("Dotfiles symlink: %s\n", ent->fts_path); */
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		if (ent->fts_info != FTS_D) {
			dirlist_add(stack, strdup(ent->fts_path));
		}
	}

fail:
	dirlist_free(stack);
	fts_close(fts);
	free(home);
	dirlist_free(whitelist);
	dirlist_free(blacklist);
}

void usage(const char *arg0)
{
	printf("%s [-w whitelist.txt] [-b blacklist.txt]\n", arg0);
}

void change_dir(const char *path)
{
	char *rpath = realpath(path, NULL);
	char *dir_name = dirname(rpath);
	chdir(dir_name);
	free(rpath);
}

dirlist *dirlist_read_file(const char *filename)
{
	dirlist *dl = NULL;
	char *line = NULL;
	FILE *file = NULL;

	dl = calloc(1, sizeof(*dl));
	if (!dl) {
		perror("calloc");
		goto fail;
	}
	size_t size = 0;
	file = fopen(filename, "r");
	if (!file) {
		perror("fopen");
		goto fail;
	}
	while (getline(&line, &size, file) != -1) {
		char *ptr = line;
		// skip whitespace
		while (*ptr && isspace(*ptr))
			ptr++;
		char *start = ptr;
		// skip till comment or whitespace
		while (*ptr && !(*ptr == '#' || isspace(*ptr)))
			ptr++;
		*ptr = '\0';
		// ensure path is not empty (it's not just a comment line)
		if (*start) {
			dirlist_add(dl, expand_path(start));
		}
	}

fail:
	free(line);
	if (file)
		fclose(file);
	return dl;
}

static int strcmpp(const void *ap, const void *bp)
{
	const char *a = *(const char **)ap;
	const char *b = *(const char **)bp;
	return strcmp(a, b);
}

dirlist *dirlist_file(const char *path)
{
	dirlist *list = NULL;
	if (path) {
		list = dirlist_read_file(path);
	} else {
		list = calloc(1, sizeof(*list));
		if (!list)
			perror("calloc");
	}
	if (list)
		qsort(list->paths, list->len, sizeof(*list->paths), strcmpp);
	return list;
}

int str_starts_with(const char *haystack, const char *needle)
{
	while (*haystack && *haystack == *needle) {
		haystack++;
		needle++;
	}
	return !*needle;
}
