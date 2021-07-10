#ifndef DIRLIST_H
#define DIRLIST_H
#include <stddef.h>

typedef struct {
	char **paths;
	size_t len;
} dirlist;

void dirlist_add(dirlist *dl, char *str);
void dirlist_free(dirlist *dl);
size_t dirlist_search(const dirlist *dl, const char *str);

#endif
