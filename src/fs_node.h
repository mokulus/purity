#pragma once

typedef struct fs_node fs_node;

fs_node* fs_node_init(fs_node* parent, const char* path);
void fs_node_free(fs_node* node);
fs_node* fs_node_match(fs_node* root, const char* path);
void fs_node_ignore_path(fs_node* root, const char* path);
void fs_node_blacklist_path(fs_node* root, const char* path);
void fs_node_ignore_public_in_home(fs_node* root);
void fs_node_ignore_git_repos(fs_node* node);
void fs_node_ignore_symlinks_in_home(fs_node* root);
void fs_node_ignore_dotfiles_symlinks(fs_node* node);
void fs_node_propagate_folded(fs_node* node);
void fs_node_propagate_ignored(fs_node* node);
void fs_node_print(fs_node* node);
