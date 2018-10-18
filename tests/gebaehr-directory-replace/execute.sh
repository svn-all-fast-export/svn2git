#! /usr/bin/env bash

# Deterine if we were executed or sourced.
if [ "$_" = "${0:-}" ]; then MAIN=: ; else MAIN=/bin/false; fi

# Did you ask us to clean?
if [ "${1:-}" = "--clean" ]; then CLEAN=1; shift; fi


##############################################################################
# Configurables

# Path to the svn-all-fast-export binary you want to test.
SAFE_EXE="${SAFE_EXE:-../../svn-all-fast-export}"

# Name of the svn dump file
SVN_DUMP="${SVN_DUMP:-$(pwd)/svn.dmp}"

# Name for the svn repository
SVN_REPO="${SVN_REPO:-$(pwd)/svn-repo}"

# Name for the svn working copy, if any
SVN_WC="${SVN_WC:-$(pwd)/svn-wc}"

# Name for the migrated git repository
GIT_REPO="${GIT_REPO:-$(pwd)/git-repo}"

# Name for the migrated git working copy
GIT_WC="${GIT_WC:-$(pwd)/git-wc}"

# If there is a "test.cond" file present, source it for the "test_cond" fn
test_cond() { return :; }  # : evaluates to true
if [ -f "./test.cond" ]; then . ./test.cond; fi


##############################################################################
# Helpers

# Consistent reporting of status messages
MESSAGE_STATUS () {
  echo "-- $*"
}

# Cross between cmake MESSAGE(FATAL) and perl's die
MESSAGE_FATAL() {
  echo >&2 "ERROR: $*"
  exit 1
}

# Describe test conditions to the user
TEST_CONDITION() {
  echo "== TEST: $*"
}

# Conditions passed
TEST_PASSED() {
  echo "== Passed!"
}

# Describe a test fail and increment the failure count.
failures=0
TEST_FAIL() {
  echo "XX FAILED: $*"
  failures=$((failures + 1))
}

# Indents text piped to it
indent_output () {
  local identity="${1:-Output}"
  local indent="${2:-| }"
  echo "${identity}:"
  sed -e "s/^/${indent}/"
  echo
}

# reset the working environment
cleanup() {
  rm -rf "${SVN_REPO}" "${SVN_WC}" "${GIT_REPO}" "${GIT_WC}"
  rm -rf gitlog-* log*
}


##############################################################################
# Main code

run_test() {
  # Reset things to the initial state
  MESSAGE_STATUS "Cleaning up test environment"
  cleanup
  if [ ${CLEAN:-0} -gt 0 ]; then return 0; fi

  # Check that SAFE_EXE refers to a valid, executable file
  [ -x "${SAFE_EXE}" ] || die "Couldn't find SAFE_EXE (${SAFE_EXE})"

  # Initialize the svn repository
  MESSAGE_STATUS "Creating SVN repos"
  svnadmin create "${SVN_REPO}" || die "svnadmin init failed"

  # Load the dump file into the repository
  MESSAGE_STATUS "Populating SVN repos"
  svnadmin load "${SVN_REPO}" <"${SVN_DUMP}" || die "svnadmin load failed"

  # Perform the migration
  MESSAGE_STATUS "Performing migration"
  "${SAFE_EXE}" --identity-map authors.txt --rules rules.conf --add-metadata --empty-dirs --propcheck --svn-ignore --debug-rules "${SVN_REPO}" \
    || die "${SAFE_EXE} exited non-zero: $?"

  # Check out the git repository
  MESSAGE_STATUS "Cloning migrated repos"
  git clone "${GIT_REPO}" "${GIT_WC}" || die "Git clone failed: $?"

  echo
  MESSAGE_STATUS
  MESSAGE_STATUS Running test conditions...
  MESSAGE_STATUS
  echo

  pushd "${GIT_WC}"
  test_cond
  error=$?
  popd
  if [ $failures -gt 0 -o $error -ne 0 ]; then
    MESSAGE_FATAL  "Test failed"
  else
    MESSAGE_STATUS "SUCCESS"
  fi
}


##############################################################################
# entry: run main

# This is akin to python's "if __name__ == '__main__': main()"
if $MAIN ; then run_test; fi

