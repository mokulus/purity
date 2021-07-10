#include "dirlist.h"
#include "dirlist_stack.h"
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
int ftsent_compare(const FTSENT **ap, const FTSENT **bp);

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
	dirlist_stack *dls = NULL;

	whitelist = dirlist_file(whitelist_path);
	if (!whitelist)
		goto fail;
	blacklist = dirlist_file(blacklist_path);
	if (!blacklist)
		goto fail;
	home = expand_path("~");
	if (!home)
		goto fail;
	fts = fts_open((char *const[]){home, NULL}, FTS_PHYSICAL, ftsent_compare);
	if (!fts)
		goto fail;
	dls = calloc(1, sizeof(*dls));
	if (!dls)
		goto fail;

	for (;;) {
		FTSENT *ent = fts_read(fts);
		if (!ent) {
			if (errno == 0) {
				break;
			} else {
				perror("fts_read");
				return 1;
			}
		}
		if (ent->fts_info == FTS_DP) {
			size_t nchildren = 0;
			// can't use fts_children here because last fts_read
			// was the last child of this directory
			DIR *dir = opendir(ent->fts_path);
			while (readdir(dir))
				nchildren++;
			closedir(dir);
			nchildren -= 2; // remove . and ..
			dirlist *last_dl = dls->dls[dls->len - 1];
			if (last_dl->len == nchildren) {
				// all were bad, mark this one as bad in the
				// previous frame
				dirlist_add(dls->dls[dls->len - 2],
					    strdup(ent->fts_path));
			} else {
				// some were good, print the bad ones, if any
				for (size_t i = 0; i < last_dl->len; ++i)
					puts(last_dl->paths[i]);
			}
			dirlist_stack_remove(dls);
			continue;
		}

		if (ent->fts_info == FTS_D) {
			// make new frame for new directory
			dirlist_stack_add(dls);
		}

		char *wpath =
		    whitelist->paths[dirlist_search(whitelist, ent->fts_path)];
		if (str_starts_with(ent->fts_path, wpath)) {
			/* printf("%s matched %s\n", ent->fts_path, wpath); */
			/* printf("Whitelisted: %s\n", ent->fts_path); */
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		int blacklisted = 0;
		for (size_t i = 0; i < blacklist->len; ++i) {
			if (str_starts_with(ent->fts_path,
					     blacklist->paths[i])) {
				blacklisted = 1;
				break;
			}
		}
		if (blacklisted) {
			/* printf("Blacklisted: %s\n", ent->fts_path); */
			puts(ent->fts_path);
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		int is_git_repo = 0;
		for (FTSENT *link = fts_children(fts, FTS_NAMEONLY); link;
		     link = link->fts_link) {
			if (!strcmp(link->fts_name, ".git")) {
				is_git_repo = 1;
				break;
			}
		}
		if (is_git_repo) {
			/* printf("Git repo: %s\n", ent->fts_path); */
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
			/* printf("Marking as bad: %s\n", ent->fts_path); */
			if (dls->len >= 1) {
				dirlist_add(dls->dls[dls->len - 1],
					    strdup(ent->fts_path));
			} else {
				/* printf("Non dir in top level: %s\n",
				 * ent->fts_path); */
				// non dir in top level
				// print it
				puts(ent->fts_path);
			}
		}
		/* printf("Should print: %s\n", ent->fts_path); */
	}

fail:
	dirlist_stack_free(dls);
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

int ftsent_compare(const FTSENT **ap, const FTSENT **bp)
{
	const FTSENT *a = *ap;
	const FTSENT *b = *bp;
	return strcmp(a->fts_name, b->fts_name);
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
	size_t lh = strlen(haystack);
	size_t ln = strlen(needle);
	if (ln > lh)
		return 0;
	return strncmp(haystack, needle, ln) == 0;
}
