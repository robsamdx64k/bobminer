#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
echo "Checking BobMiner files..."
head -1 install-userland.sh | grep -q '#!/usr/bin/env bash'
head -1 start.sh | grep -q '#!/usr/bin/env bash'
test -f src/bobminer.c
grep -q '#include <stdio.h>' src/bobminer.c
grep -q '^bobminer: src/bobminer.c' Makefile
echo "Files look clean."
