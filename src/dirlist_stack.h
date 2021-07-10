#ifndef DIRLIST_STACK_H
#define DIRLIST_STACK_H

#include "dirlist.h"

typedef struct {
	dirlist **dls;
	size_t len;
} dirlist_stack;

dirlist *dirlist_stack_add(dirlist_stack *dls);
void dirlist_stack_remove(dirlist_stack *dls);
void dirlist_stack_free(dirlist_stack *dls);

#endif
