#include "dirlist.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static void dirlist_add(dirlist *dl, char *str);
static int strcmpp(const void *ap, const void *bp);
static dirlist *dirlist_read_file(const char *filename);

dirlist *dirlist_file(const char *path)
{
	dirlist *list = NULL;
	if (path) {
		list = dirlist_read_file(path);
	} else {
		list = calloc(1, sizeof(*list));
	}
	if (list)
		qsort(list->paths, list->len, sizeof(*list->paths), strcmpp);
	return list;
}

size_t dirlist_search(const dirlist *dl, const char *str)
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

void dirlist_free(dirlist *dl)
{
	for (size_t i = 0; i < dl->len; ++i)
		free(dl->paths[i]);
	free(dl->paths);
	free(dl);
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
