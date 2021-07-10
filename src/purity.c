#include <ctype.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fts.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "path_util.h"
#include <dirent.h>

typedef struct {
	char **paths;
	size_t len;
} dirlist;

typedef struct {
	dirlist **dls;
	size_t len;
} dirlist_stack;

void usage(const char *arg0);
void change_dir(const char *path);
dirlist *process_file(const char *filename);
int ftsent_compare(const FTSENT** ap, const FTSENT** bp);

void dirlist_add(dirlist *dl, char *str);
void dirlist_free(dirlist *dl);
dirlist *dirlist_stack_add(dirlist_stack *dls);
void dirlist_stack_remove(dirlist_stack *dls);
void dirlist_stack_free(dirlist_stack *dls);
dirlist *dirlist_file(const char *path);
int str_common_start(const char *haystack, const char *needle);
size_t dirlist_search(const dirlist *dl, const char *str);

int
main(int argc, char *argv[])
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

	dirlist *whitelist = dirlist_file(whitelist_path);
	dirlist *blacklist = dirlist_file(blacklist_path);
	char *home = expand_path("~");

	FTS *fts = fts_open( (char *const[]){home, NULL}, FTS_PHYSICAL, ftsent_compare);
	if (!fts) {
		perror("fts_open");
		return 1;
	}

	dirlist_stack *dls = calloc(1, sizeof(*dls));
	if (!dls)
		return 1;
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
		if (ent->fts_info == FTS_DP)  {
			size_t nchildren = 0;
			// can't use fts_children here because last fts_read
			// was the last child of this directory
			DIR *dir = opendir(ent->fts_path);
			while (readdir(dir))
				nchildren++;
			closedir(dir);
			nchildren -= 2; // remove . and ..
			dirlist *last_dl = dls->dls[dls->len-1];
			if (last_dl->len == nchildren) {
				// all were bad, mark this one as bad in the previous frame
				dirlist_add(dls->dls[dls->len-2], strdup(ent->fts_path));
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

		char *wpath = whitelist->paths[dirlist_search(whitelist, ent->fts_path)];
		if (str_common_start(ent->fts_path, wpath)) {
			/* printf("%s matched %s\n", ent->fts_path, wpath); */
			/* printf("Whitelisted: %s\n", ent->fts_path); */
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		int blacklisted = 0;
		for (size_t i = 0; i < blacklist->len; ++i) {
			if (str_common_start(ent->fts_path, blacklist->paths[i])) {
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
		for (FTSENT *link = fts_children(fts, FTS_NAMEONLY); link; link = link->fts_link) {
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
		if (S_ISLNK(ent->fts_statp->st_mode) && strstr(ent->fts_path, "dotfiles")) {
			/* printf("Dotfiles symlink: %s\n", ent->fts_path); */
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		if (ent->fts_info != FTS_D) {
			/* printf("Marking as bad: %s\n", ent->fts_path); */
			if (dls->len >= 1) {
				dirlist_add(dls->dls[dls->len-1], strdup(ent->fts_path));
			} else {
				/* printf("Non dir in top level: %s\n", ent->fts_path); */
				// non dir in top level
				// print it
				puts(ent->fts_path);
			}
		}
		/* printf("Should print: %s\n", ent->fts_path); */
	}
	fts_close(fts);
	free(home);
	dirlist_stack_free(dls);
	dirlist_free(whitelist);
	dirlist_free(blacklist);
}

void
usage(const char *arg0)
{
	printf("%s [-w whitelist.txt] [-b blacklist.txt]\n", arg0);
}

void
change_dir(const char *path)
{
	char *rpath = realpath(path, NULL);
	char *dir_name = dirname(rpath);
	chdir(dir_name);
	free(rpath);
}

dirlist *process_file(const char *filename)
{
	dirlist *dl = malloc(sizeof(*dl));
	dl->paths = NULL;
	dl->len = 0;
	if (!dl)
		return dl;
	size_t size = 0;
	char *line = NULL;
	FILE *file = fopen(filename, "r");
	if (!file) {
		free(dl);
		dl = NULL;
		perror("fopen");
		goto cleanup;
	}
	while (getline(&line, &size, file) != -1) {
		char *ptr = line;
		while (*ptr && isspace(*ptr))
			ptr++; // skip whitespace
		char *start = ptr;
		while (*ptr && !(*ptr == '#' || isspace(*ptr)))
			ptr++; // skip till comment or whitespace
		*ptr = '\0';
		if (*start) { // ensure path is not empty (it's not just a comment line)
			dirlist_add(dl, expand_path(start));
		}
	}
cleanup:
	free(line);
	fclose(file);
	return dl;
}

int ftsent_compare(const FTSENT** ap, const FTSENT** bp)
{
	const FTSENT *a = *ap;
	const FTSENT *b = *bp;
	return strcmp(a->fts_name, b->fts_name);
}

void dirlist_add(dirlist *dl, char *str) {
	dl->len++;
	dl->paths = realloc(dl->paths, sizeof(*dl->paths) * dl->len);
	dl->paths[dl->len - 1] = str;
}

dirlist *dirlist_stack_add(dirlist_stack *dls) {
	dls->len++;
	dls->dls = realloc(dls->dls, dls->len * sizeof(*dls->dls));
	dls->dls[dls->len - 1] = malloc(sizeof(**dls->dls));
	dls->dls[dls->len - 1]->len = 0;
	dls->dls[dls->len - 1]->paths = NULL;
	return dls->dls[dls->len - 1];
}

void dirlist_stack_remove(dirlist_stack *dls) {
	dirlist_free(dls->dls[dls->len - 1]);
	dls->len--;
	dls->dls = realloc(dls->dls, dls->len * sizeof(*dls->dls));
}

void dirlist_free(dirlist *dl) {
	for (size_t i = 0; i < dl->len; ++i)
		free(dl->paths[i]);
	free(dl->paths);
	free(dl);
}

void dirlist_stack_free(dirlist_stack *dls) {
	for (size_t i = 0; i < dls->len; ++i) {
		dirlist_free(dls->dls[i]);
	}
	free(dls->dls);
	free(dls);
}

static int strcmpp(const void *ap, const void *bp) {
	const char *a = *(const char **)ap;
	const char *b = *(const char **)bp;
	return strcmp(a, b);
}

dirlist *dirlist_file(const char *path) {
	dirlist *list;
	if (path) {
		list = process_file(path);
	} else {
		list = calloc(1, sizeof(*list));
	}
	qsort(list->paths, list->len, sizeof(*list->paths), strcmpp);
	return list;
}

int str_common_start(const char *haystack, const char *needle) {
	size_t lh = strlen(haystack);
	size_t ln = strlen(needle);
	if (ln > lh)
		return 0;
	return strncmp(haystack, needle, ln) == 0;
}

size_t dirlist_search(const dirlist *dl, const char *str) {
	int low = 0;
	int high = dl->len - 1;
	int mid = -1;
	while (low <= high) {
		mid = (low + high) / 2;
		int cmp = strcmp(dl->paths[mid], str);
		if (cmp < 0) {
			low = mid + 1;
		} else if (cmp == 0) {
			return mid;
		} else {
			high = mid - 1;
		}
	}
	return mid;
}
