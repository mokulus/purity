#include "path_util.h"
#include <bsd/string.h>
#include <stdlib.h>
#include <wordexp.h>

char* join_path(const char* a, const char* b)
{
	const size_t len_a = strlen(a);
	const size_t len_b = strlen(b);
	const size_t total_len = len_a + 1 + len_b + 1;
	char* path = malloc(total_len);
	strlcpy(path, a, total_len);
	path[len_a] = '/';
	strlcpy(path + len_a + 1, b, total_len);
	path[len_a + 1 + len_b] = '\0';
	return path;
}

char* expand_path(const char* path)
{
	wordexp_t exp;
	wordexp(path, &exp, 0);
	char* real_path = strdup(exp.we_wordv[0]);
	wordfree(&exp);
	return real_path;
}
