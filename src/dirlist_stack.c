#include "dirlist_stack.h"
#include <stdlib.h>

dirlist *dirlist_stack_add(dirlist_stack *dls)
{
	dls->len++;
	dls->dls = realloc(dls->dls, dls->len * sizeof(*dls->dls));
	dls->dls[dls->len - 1] = malloc(sizeof(**dls->dls));
	dls->dls[dls->len - 1]->len = 0;
	dls->dls[dls->len - 1]->paths = NULL;
	return dls->dls[dls->len - 1];
}

void dirlist_stack_remove(dirlist_stack *dls)
{
	dirlist_free(dls->dls[dls->len - 1]);
	dls->len--;
	dls->dls = realloc(dls->dls, dls->len * sizeof(*dls->dls));
}

void dirlist_stack_free(dirlist_stack *dls)
{
	for (size_t i = 0; i < dls->len; ++i) {
		dirlist_free(dls->dls[i]);
	}
	free(dls->dls);
	free(dls);
}
