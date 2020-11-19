#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <libgen.h>
#include "fs_node.h"


void usage(const char** argv);


int
main(int argc, const char** argv) {
	if (argc != 2) {
		usage(argv);
		return 1;
	}
	fs_node *root = fs_node_init(NULL, "~");
	fs_node_ignore_git_repos(root);
	fs_node_ignore_public_in_home(root);
	fs_node_ignore_symlinks_in_home(root);
	fs_node_ignore_dotfiles_symlinks(root);

	char *path = realpath(argv[0], NULL);
	char *dir_name = dirname(path);
	chdir(dir_name);
	free(path);
	FILE *ignore_file = fopen(argv[1], "r");
	if (!ignore_file) {
		perror("fopen");
		goto cleanup;

	}
	size_t size = 0;
	char *line = NULL;
	while(getline(&line, &size, ignore_file) != -1) {
		char *ptr = line;
		while(*ptr && isspace(*ptr)) ptr++; // skip whitespace
		char *start = ptr;
		while(*ptr && !(*ptr == '#' || isspace(*ptr))) ptr++; //skip till comment or whitespace
		*ptr = '\0';
		if (*start) // ensure path is not empty (it's not just a comment line
			fs_node_ignore_path(root, start);
	}
	free(line);
	fclose(ignore_file);

	fs_node_propagate_ignored(root);
	fs_node_propagate_folded(root);
	fs_node_print(root);
cleanup:
	fs_node_free(root);
}


void
usage(const char** argv) {
	printf("%s [ignore.txt]\n", argv[0]);
}
