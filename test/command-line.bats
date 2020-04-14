load 'common'

setup() {
    : # suppress common setup, no repository needed
}

@test 'specifying version parameter should output version and exit with 0 no matter what' {
    run svn2git --version --non-existing repo-a repo-b
    assert_success
    assert_output --partial 'Git version:'

    run svn2git --non-existing repo-a --version repo-b
    assert_success
    assert_output --partial 'Git version:'

    run svn2git --non-existing repo-a repo-b --version
    assert_success
    assert_output --partial 'Git version:'

    run svn2git -v --non-existing repo-a repo-b
    assert_success
    assert_output --partial 'Git version:'

    run svn2git --non-existing repo-a -v repo-b
    assert_success
    assert_output --partial 'Git version:'

    run svn2git --non-existing repo-a repo-b -v
    assert_success
    assert_output --partial 'Git version:'
}

@test 'specifying help parameter should output usage and exit with 0 no matter what' {
    run svn2git --help --non-existing repo-a repo-b
    assert_success
    assert_output --partial 'Usage:'

    run svn2git --non-existing repo-a --help repo-b
    assert_success
    assert_output --partial 'Usage:'

    run svn2git --non-existing repo-a repo-b --help
    assert_success
    assert_output --partial 'Usage:'

    run svn2git -h --non-existing repo-a repo-b
    assert_success
    assert_output --partial 'Usage:'

    run svn2git --non-existing repo-a -h repo-b
    assert_success
    assert_output --partial 'Usage:'

    run svn2git --non-existing repo-a repo-b -h
    assert_success
    assert_output --partial 'Usage:'
}

@test 'not giving a repository should exist with non-zero exit code and print usage' {
    run svn2git
    assert_failure 12
    assert_output --partial 'Usage:'
}

@test 'giving mutliple repositories should exist with non-zero exit code and print usage' {
    run svn2git repo-a repo-b
    assert_failure 12
    assert_output --partial 'Usage:'
}
