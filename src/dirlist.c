#include "dirlist.h"
#include "util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t dirlist_search(const dirlist *dl, const char *str);
static void dirlist_add(dirlist *dl, char *str);
static int strcmpp(const void *ap, const void *bp);
static dirlist *dirlist_read_file(const char *filename);
static int str_starts_with(const char *haystack, const char *needle);

dirlist *dirlist_file(const char *path)
{
	dirlist *list = NULL;
	if (path) {
		list = dirlist_read_file(path);
	} else {
		list = calloc(1, sizeof(*list));
	}
	if (list && list->paths)
		qsort(list->paths, list->len, sizeof(*list->paths), strcmpp);
	return list;
}

void dirlist_free(dirlist *dl)
{
	for (size_t i = 0; i < dl->len; ++i)
		free(dl->paths[i]);
	free(dl->paths);
	free(dl);
}

static size_t dirlist_search(const dirlist *dl, const char *str)
{
	size_t base = 0;
	size_t mid = -1;
	for (size_t range = dl->len; range; range /= 2) {
		mid = base + range / 2;
		int cmp = strcmp(dl->paths[mid], str);
		if (cmp < 0) {
			base = mid + 1;
			range--;
		} else if (cmp == 0)
			return mid;
	}
	return mid;
}

static void dirlist_add(dirlist *dl, char *str)
{
	dl->len++;
	dl->paths = realloc(dl->paths, sizeof(*dl->paths) * dl->len);
	dl->paths[dl->len - 1] = str;
}

static int strcmpp(const void *ap, const void *bp)
{
	const char *a = *(const char **)ap;
	const char *b = *(const char **)bp;
	return strcmp(a, b);
}

static dirlist *dirlist_read_file(const char *filename)
{
	dirlist *dl = NULL;
	char *line = NULL;
	FILE *file = NULL;

	dl = calloc(1, sizeof(*dl));
	if (!dl)
		goto fail;
	dl->prune_level = -1;
	size_t size = 0;
	file = fopen(filename, "r");
	if (!file)
		goto fail;
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

static int str_starts_with(const char *haystack, const char *needle)
{
	while (*haystack && *haystack == *needle) {
		haystack++;
		needle++;
	}
	return !*needle;
}

unsigned dirlist_match(dirlist *dl, FTSENT *ent)
{
	if (dl->prune_level >= ent->fts_level)
		dl->prune_level = -1;
	if (dl->prune_level == -1) {
		int windex = dirlist_search(dl, ent->fts_path);
		if (windex != -1 &&
		    str_starts_with(ent->fts_path, dl->paths[windex])) {
			return 1;
		} else {
			int could_get_match = 0;
			for (size_t i = 0; i < dl->len; ++i) {
				if (str_starts_with(dl->paths[i],
						    ent->fts_path)) {
					could_get_match = 1;
					break;
				}
			}
			if (!could_get_match) {
				dl->prune_level = ent->fts_level;
			}
		}
	}
	return 0;
}
