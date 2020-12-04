#include "fs_node.h"
#include <ctype.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void usage(const char *arg0);
void change_dir(const char *path);
int process_file(const char *filename, fs_node *root,
		 void callback(fs_node *root, const char *str));

int main(int argc, char *argv[])
{
	char *whitelist_path = NULL;
	char *blacklist_path = NULL;

	int ch;
	while ((ch = getopt(argc, argv, "w:b:")) != -1) {
		switch (ch) {
		case 'w':
			whitelist_path = optarg;
			break;
		case 'b':
			blacklist_path = optarg;
			break;
		default:
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	if (argc == 1 || optind != argc) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	fs_node *root = fs_node_init(NULL, "~");
	fs_node_ignore_git_repos(root);
	fs_node_ignore_public_in_home(root);
	fs_node_ignore_symlinks_in_home(root);
	fs_node_ignore_dotfiles_symlinks(root);

	change_dir(argv[0]);
	if (blacklist_path &&
	    process_file(blacklist_path, root, fs_node_blacklist_path) == -1)
		goto cleanup;
	if (whitelist_path &&
	    process_file(whitelist_path, root, fs_node_ignore_path) == -1)
		goto cleanup;

	fs_node_propagate_ignored(root);
	fs_node_propagate_folded(root);
	fs_node_print(root);
cleanup:
	fs_node_free(root);
}

void usage(const char *arg0)
{
	printf("%s [-w whitelist.txt] [-b blacklist.txt]\n", arg0);
}

void change_dir(const char *path)
{
	char *rpath = realpath(path, NULL);
	char *dir_name = dirname(rpath);
	chdir(dir_name);
	free(rpath);
}

int process_file(const char *filename, fs_node *root,
		 void callback(fs_node *root, const char *str))
{
	unsigned int ret = 0;
	size_t size = 0;
	char *line = NULL;
	FILE *file = fopen(filename, "r");
	if (!file) {
		ret = -1;
		perror("fopen");
		goto cleanup;
	}
	while (getline(&line, &size, file) != -1) {
		char *ptr = line;
		while (*ptr && isspace(*ptr))
			ptr++; // skip whitespace
		char *start = ptr;
		while (*ptr && !(*ptr == '#' || isspace(*ptr)))
			ptr++; // skip till comment or whitespace
		*ptr = '\0';
		if (*start) { // ensure path is not empty (it's not just a
			      // comment line)
			callback(root, start);
		}
	}
cleanup:
	free(line);
	fclose(file);
	return ret;
}
