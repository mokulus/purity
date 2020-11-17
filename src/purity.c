#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <libgen.h>
#include "fs_node.h"

int main(int argc, const char** argv) {
	(void)argc;
	fs_node *root = fs_node_init(NULL, "~");
	fs_node_ignore_git_repos(root);
	fs_node_ignore_public_in_home(root);
	fs_node_ignore_symlinks_in_home(root);
	fs_node_ignore_dotfiles_symlinks(root);

	char *path = realpath(argv[0], NULL);
	char *dir_name = dirname(path);
	chdir(dir_name);
	free(path);
	FILE *ignore_file = fopen("ignore.txt", "r");
	size_t size = 0;
	char *line = 0;
	while(getline(&line, &size, ignore_file) != -1) {
		char *ptr = line;
		while(isspace(*ptr)) ptr++; // skip whitespace
		char *start = ptr;
		while(!(*ptr == '#' || isspace(*ptr))) ptr++; //skip till comment or whitespace
		*ptr = '\0';
		if (*start) // ensure path is not empty (it's not just a comment line
			fs_node_ignore_path(root, start);
	}
	fclose(ignore_file);

	fs_node_propagate_ignored(root);
	fs_node_propagate_folded(root);
	fs_node_print(root);

	fs_node_free(root);
}
