#!/bin/sh
git diff main --name-only --relative | egrep \\.\(c\|cpp\|h\|hpp\)$ | xargs clang-format -style=file -i

