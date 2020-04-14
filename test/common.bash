load 'libs/bats-support/load'
load 'libs/bats-assert/load'
load 'libs/bats-file/load'

setup() {
    commonSetup
}

commonSetup() {
    TEST_TEMP_DIR="$(temp_make --prefix 'svn2git-')"
    BATSLIB_FILE_PATH_REM="#${TEST_TEMP_DIR}"
    BATSLIB_FILE_PATH_ADD='<temp>'

    SVN_REPO="$TEST_TEMP_DIR/svn-repo"
    SVN_WORKTREE="$TEST_TEMP_DIR/svn-worktree"

    tar xf "$BATS_TEST_DIRNAME/base-fixture.tar" --one-top-level="$SVN_REPO"
    svn checkout "file:///$SVN_REPO" "$SVN_WORKTREE"
    cd "$SVN_WORKTREE"
}

teardown() {
    commonTeardown
}

commonTeardown() {
    if [ -n "${TEST_TEMP_DIR-}" ]; then
        temp_del "$TEST_TEMP_DIR"
    fi
}

svn2git() {
    "$BATS_TEST_DIRNAME/../svn-all-fast-export" "$@"
}
