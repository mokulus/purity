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

void usage(const char *arg0);
void change_dir(const char *path);
dirlist *process_file(const char *filename);

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


	dirlist *whitelist;
	if (whitelist_path) {
		whitelist = process_file(whitelist_path);
	} else {
		whitelist = calloc(1, sizeof(*whitelist));
	}
	dirlist *blacklist;
	if (blacklist_path) {
		blacklist = process_file(blacklist_path);
	} else {
		blacklist = calloc(1, sizeof(*blacklist));
	}
	char *home = expand_path("~");

	FTS *fts = fts_open( (char *const[]){home, NULL}, FTS_PHYSICAL, NULL);
	if (!fts) {
		perror("fts_open");
		return 1;
	}
	dirlist **pls = NULL;
	size_t pls_len = 0;

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
		if (ent->fts_level == 0)
			continue;

		if (ent->fts_info == FTS_DP)  {
			/* printf("DP for %s\n", ent->fts_path); */
			size_t nchildren = 0;
			// can't use fts_children here because last fts_read
			// was the last child of this directory
			DIR *dir = opendir(ent->fts_path);
			while (readdir(dir))
				nchildren++;
			closedir(dir);
			nchildren -= 2; // remove . and ..
			if (pls[pls_len-1]->len == nchildren) {
				// all were bad, mark this one as bad
				if (pls_len >= 2 ) {
					/* printf("All bad: %s\n", ent->fts_path); */
					dirlist *last_pl = pls[pls_len-2];
					last_pl->len++;
					last_pl->paths = realloc(last_pl->paths, sizeof(*last_pl->paths) * last_pl->len);
					last_pl->paths[last_pl->len - 1] = strdup(ent->fts_path);
				} else {
					/* printf("Bad top level: %s\n", ent->fts_path); */
					// top level with all children to print
					// so just print it
					puts(ent->fts_path);
				}
			} else if (pls[pls_len-1]->len == 0) {
				// was whitelisted, do nothing
				/* printf("Empty frame: %s\n", ent->fts_path); */
			} else {
				// some were good, print the bad ones
				/* printf("Mixed frame: %s\n", ent->fts_path); */
				/* printf("Children %zu - listed %zu\n", nchildren, pls[pls_len-1]->len); */
				for (size_t i = 0; i < pls[pls_len-1]->len; ++i)
					puts(pls[pls_len-1]->paths[i]);
			}
			pls_len--;
			for (size_t i = 0; i < pls[pls_len]->len; ++i)
				free(pls[pls_len]->paths[i]);
			pls = realloc(pls, pls_len * sizeof(*pls));
			continue;
		}

		if (ent->fts_info == FTS_D) {
			// make new frame for new directory
			/* printf("New frame for %s\n", ent->fts_path); */
			pls_len++;
			pls = realloc(pls, pls_len * sizeof(*pls));
			pls[pls_len-1] = malloc(sizeof(**pls));
			pls[pls_len-1]->len = 0;
			pls[pls_len-1]->paths = NULL;
		}

		int whitelisted = 0;
		for (size_t i = 0; i < whitelist->len; ++i) {
			if(strstr(ent->fts_path, whitelist->paths[i]) == ent->fts_path) {
				whitelisted = 1;
				break;
			}
		}
		if (whitelisted) {
			/* printf("Whitelisted: %s\n", ent->fts_path); */
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		int blacklisted = 0;
		for (size_t i = 0; i < blacklist->len; ++i) {
			if(strstr(ent->fts_path, blacklist->paths[i]) == ent->fts_path) {
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
			if (pls_len >= 1) {
				dirlist *last_pl = pls[pls_len-1];
				last_pl->len++;
				last_pl->paths = realloc(last_pl->paths, sizeof(*last_pl->paths) * last_pl->len);
				last_pl->paths[last_pl->len - 1] = strdup(ent->fts_path);
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
			dl->len++;
			dl->paths = realloc(dl->paths, sizeof(*dl->paths) * dl->len);
			dl->paths[dl->len - 1] = expand_path(start);
		}
	}
cleanup:
	free(line);
	fclose(file);
	return dl;
}
