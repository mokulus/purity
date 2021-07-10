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
