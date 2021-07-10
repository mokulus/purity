#include "dirlist.h"
#include <stdlib.h>
#include <string.h>

void dirlist_add(dirlist *dl, char *str)
{
	dl->len++;
	dl->paths = realloc(dl->paths, sizeof(*dl->paths) * dl->len);
	dl->paths[dl->len - 1] = str;
}

void dirlist_free(dirlist *dl)
{
	for (size_t i = 0; i < dl->len; ++i)
		free(dl->paths[i]);
	free(dl->paths);
	free(dl);
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
