#include "path_util.h"
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>

char *expand_path(const char *path)
{
	wordexp_t exp;
	wordexp(path, &exp, 0);
	char *real_path = strdup(exp.we_wordv[0]);
	wordfree(&exp);
	return real_path;
}
