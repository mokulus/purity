#ifndef DIRLIST_H
#define DIRLIST_H
#include <fts.h>
#include <stddef.h>

typedef struct {
	char **paths;
	size_t len;
	short prune_level;
} dirlist;

dirlist *dirlist_file(const char *path);
void dirlist_free(dirlist *dl);
unsigned dirlist_match(dirlist *dl, FTSENT *ent);

#endif
