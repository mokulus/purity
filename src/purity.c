#include "dirlist.h"
#include "util.h"
#include "stack.h"
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
	stack *stack = NULL;

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
		/* disable pruning if we came back to the same level */
		if (whitelist_prune_level == ent->fts_level)
			whitelist_prune_level = -1;
		if (ent->fts_info == FTS_DP || ent->fts_info == FTS_DNR) {
			size_t nchildren = ent->fts_number - 1;
			size_t start_index = stack->indices_len - 1;
			while (*stack_at(stack, start_index))
				--start_index;
			size_t nmarked = stack->indices_len - 1 - start_index;
			unsigned is_whitelisted = whitelist_parent == ent->fts_level;
			unsigned is_fully_marked = nmarked == nchildren;
			if (!is_whitelisted && !is_fully_marked) {
				// some were good, print the bad ones, if any
				for (size_t i = start_index + 1; i < stack->indices_len;
				     ++i)
					puts(stack_at(stack, i));
			}
			stack->data_len = stack->indices[start_index];
			stack->indices_len = start_index;
			if (!is_whitelisted && is_fully_marked) {
				// all were bad, mark this one as bad in the
				// previous frame, so only after freeing
				stack_add(stack, ent->fts_path, ent->fts_pathlen);
			}
			if (is_whitelisted)
				whitelist_parent = -1;
			continue;
		}

		ent->fts_parent->fts_number++;

		if (ent->fts_info == FTS_D) {
			// mark new frame for new directory
			stack_add(stack, NULL, 0);
		}

		if (whitelist_prune_level == -1) {
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
		}

		int bindex = dirlist_search(blacklist, ent->fts_path);
		if (bindex != -1 &&
		    str_starts_with(ent->fts_path, blacklist->paths[bindex])) {
			puts(ent->fts_path);
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		if (!strcmp(ent->fts_name, ".git")) {
			whitelist_parent = ent->fts_level - 1;
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}
		/* public in home */
		if (ent->fts_level == 1 && ent->fts_name[0] != '.') {
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}
		/* symlinks in home */
		if (ent->fts_level == 1 && S_ISLNK(ent->fts_statp->st_mode)) {
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}
		/* dotfiles */
		if (S_ISLNK(ent->fts_statp->st_mode) &&
		    strstr(ent->fts_path, "dotfiles")) {
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		/* dirs are added in postorder */
		if (ent->fts_info != FTS_D)
			stack_add(stack, ent->fts_path, ent->fts_pathlen);

		ent->fts_number = 1;
	}

fail:
	stack_free(stack);
	fts_close(fts);
	free(home);
	dirlist_free(whitelist);
	dirlist_free(blacklist);
}

void usage(const char *arg0)
{
	printf("%s [-w whitelist.txt] [-b blacklist.txt]\n", arg0);
}

int str_starts_with(const char *haystack, const char *needle)
{
	while (*haystack && *haystack == *needle) {
		haystack++;
		needle++;
	}
	return !*needle;
}
