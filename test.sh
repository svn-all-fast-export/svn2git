#!/bin/bash

set -euo pipefail

# Ensure needed tools are present
svn --version >/dev/null
git --version >/dev/null
tar --version >/dev/null

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
{
    TMPDIR="$SCRIPT_DIR/build/tmp" \
        test/libs/bats-core/bin/bats "$@" \
        4>&1 1>&2 2>&4 |
        awk '
            BEGIN {
                duplicate_test_names = ""
            }

            {
                print
            }

            /duplicate test name/ {
                duplicate_test_names = duplicate_test_names "\n\t" $0
            }

            END {
                if (length(duplicate_test_names)) {
                    print "\nERROR: duplicate test name(s) found:" duplicate_test_names
                    exit 1
                }
            }
        '
} 4>&1 1>&2 2>&4
