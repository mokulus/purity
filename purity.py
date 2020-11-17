#!/bin/python3
import os
import sys
from pathlib import Path


class FilesystemNode:
    def __init__(self, parent, path):
        self.parent = parent
        self.path = path.expanduser().absolute()
        self.children = set()
        self.ignored = False
        self.folded = False

        if self.path.is_dir():
            for child_path in self.path.iterdir():
                self.children.add(FilesystemNode(self, child_path))
        self.empty = len(self.children) == 0


class Purity:
    def __init__(self, path=Path.home()):
        self.root = FilesystemNode(None, path)

    def ignore(self, *paths):
        for path in paths:
            self.ignore_path(Path(str(path)).expanduser().absolute())

    def ignore_path(self, path):
        node = self.root
        for _ in path.parts[3:]:  # root, home, user are already common
            for child in node.children:
                length = len(child.path.parts)
                if child.path.parts == path.parts[:length]:
                    node = child
                    break
        node.ignored = True

    def ignore_public_in_home(self):
        for child in self.root.children:
            if not child.path.name.startswith("."):
                child.ignored = True

    def ignore_git_repos(self, node=None):
        node = node or self.root
        if node.path.is_dir():
            if node.path.parts[-1] == ".git":
                node.parent.ignored = True
            else:
                for child in node.children:
                    self.ignore_git_repos(child)

    def ignore_symlinks_in_home(self):
        for child in self.root.children:
            if child.path.is_symlink():
                child.ignored = True

    def ignore_dotfiles_symlinks(self, node=None):
        node = node or self.root
        if node.path.is_symlink() and "dotfiles" in node.path.resolve().parts:
            node.ignored = True
        else:
            for child in node.children:
                self.ignore_dotfiles_symlinks(child)

    def print(self, node=None):
        node = node or self.root
        if node.ignored:
            return
        if node.folded:
            print(node.path, end="")
            print("/" if node.path.is_dir() else "")
        else:
            for child in sorted(node.children, key=lambda node: node.path):
                self.print(child)

    def propagate_folded(self, node=None):
        node = node or self.root
        if node.empty:
            node.folded = not node.ignored
        else:
            for child in node.children:
                self.propagate_folded(child)
            node.folded = all(child.folded for child in node.children)

    def propagate_ignored(self, node=None):
        node = node or self.root
        for child in node.children:
            if node.ignored:
                child.ignored = True
            self.propagate_ignored(child)


if __name__ == "__main__":
    os.chdir(os.path.dirname(sys.argv[0]))

    p = Purity()
    p.ignore_git_repos()
    p.ignore_public_in_home()
    p.ignore_symlinks_in_home()
    p.ignore_dotfiles_symlinks()

    with open("ignore.txt", "r") as file:
        for line in file:
            line = line.partition("#")[0].strip()
            if line != "":
                p.ignore(line)

    p.propagate_ignored()
    p.propagate_folded()
    p.print()