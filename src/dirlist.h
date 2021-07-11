#ifndef DIRLIST_H
#define DIRLIST_H
#include <stddef.h>

typedef struct {
	char **paths;
	size_t len;
} dirlist;

dirlist *dirlist_file(const char *path);
size_t dirlist_search(const dirlist *dl, const char *str);
void dirlist_free(dirlist *dl);

#endif
