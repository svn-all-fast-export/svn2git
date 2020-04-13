#!/bin/bash

set -euo pipefail

# Ensure needed tools are present
svn --version >/dev/null
git --version >/dev/null

# Determine SCRIPT_DIR
# Resolve links: $0 may be a link
PRG="$0"
# Need this for relative symlinks.
while [ -h "$PRG" ]; do
    ls=$(ls -ld "$PRG")
    link=$(expr "$ls" : '.*-> \(.*\)$')
    if expr "$link" : '/.*' >/dev/null; then
        PRG="$link"
    else
        PRG=$(dirname "$PRG")"/$link"
    fi
done
cd "$(dirname "$PRG")/" >/dev/null
SCRIPT_DIR="$(pwd -P)"

if [ "${1-}" == "--no-make" ]; then
    shift
else
    qmake
    make
fi

if [ $# -eq 0 ]; then
    set -- test
else
    set -- "${@/#/test/}"
    set -- "${@/%/.bats}"
fi

mkdir -p "$SCRIPT_DIR/build/tmp"
TMPDIR="$SCRIPT_DIR/build/tmp" test/libs/bats-core/bin/bats "$@"
