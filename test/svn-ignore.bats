load 'common'

@test 'svn:ignore entries should only ignore matching direct children' {
    svn mkdir dir-a
    svn commit -m 'add dir-a'
    svn update
    svn propset svn:ignore $'ignore-a\nignore-b' dir-a
    svn commit -m 'ignore ignore-a and ignore-b on dir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" "$(cat <<-EOF
			/ignore-a
			/ignore-b
		EOF
    )"
}

@test 'svn:global-ignores entries should ignore all matching descendents' {
    svn mkdir dir-a
    svn commit -m 'add dir-a'
    svn update
    svn propset svn:global-ignores $'ignore-a\nignore-b' dir-a
    svn commit -m 'ignore ignore-a and ignore-b on dir-a and descendents'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" "$(cat <<-EOF
			ignore-a
			ignore-b
		EOF
    )"
}

@test 'svn-ignore translation should be done if svn-branches parameter is used' {
    svn mkdir trunk
    svn commit -m 'create trunk'
    svn propset svn:ignore $'ignore-a\nignore-b' trunk
    svn commit -m 'ignore ignore-a and ignore-b on root'
    svn propset svn:global-ignores 'ignore-c' trunk
    svn commit -m 'ignore ignore-c everywhere'
    svn mkdir branches
    svn copy trunk branches/branch-a
    svn commit -m 'create branch branch-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --svn-branches --rules <(echo "
        create repository git-repo
        end repository

        match /trunk/
            repository git-repo
            branch master
        end match

        match /branches/$
            action recurse
        end match

        match /branches/([^/]+)/
            repository git-repo
            branch \1
        end match
    ")

    assert_equal "$(git -C git-repo show master:.gitignore)" "$(cat <<-EOF
			/ignore-a
			/ignore-b
			ignore-c
		EOF
    )"
    assert_equal "$(git -C git-repo show branch-a:.gitignore)" "$(cat <<-EOF
			/ignore-a
			/ignore-b
			ignore-c
		EOF
    )"
}

@test 'svn-ignore translation should be done transitively when copying a directory' {
    svn mkdir --parents dir-a/subdir-a
    svn commit -m 'add dir-a/subdir-a'
    svn propset svn:ignore $'ignore-a\nignore-b' dir-a/subdir-a
    svn commit -m 'ignore ignore-a and ignore-b on dir-a/subdir-a'
    svn propset svn:global-ignores 'ignore-c' dir-a/subdir-a
    svn commit -m 'ignore ignore-c on dir-a/subdir-a and descendents'
    svn copy dir-a dir-b
    svn commit -m 'copy dir-a to dir-b'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    assert_equal "$(git -C git-repo show master:dir-a/subdir-a/.gitignore)" "$(cat <<-EOF
			/ignore-a
			/ignore-b
			ignore-c
		EOF
    )"
    assert_equal "$(git -C git-repo show master:dir-b/subdir-a/.gitignore)" "$(cat <<-EOF
			/ignore-a
			/ignore-b
			ignore-c
		EOF
    )"
}

@test 'svn-ignore parameter should not cause added directories to be dumped multiple times' {
    svn mkdir dir-a
    echo content-a >dir-a/file-a
    svn add dir-a/file-a
    svn commit -m 'add dir-a/file-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --create-dump --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    assert [ "$(grep -c '^M .* dir-a/file-a$' git-repo.fi)" -eq 1 ]
}

@test 'svn-ignore parameter should not cause added directories to be dumped multiple times (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    echo content-a >dir-a/file-a
    svn add dir-a/file-a
    svn commit -m 'add dir-a/file-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --create-dump --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    assert [ "$(grep -c '^M .* dir-a/file-a$' git-repo.fi)" -eq 1 ]
}

@test 'svn-ignore translation should not delete unrelated files' {
    svn mkdir dir-a
    echo content-a >dir-a/file-a
    svn add dir-a/file-a
    svn commit -m 'add dir-a/file-a'
    svn update
    svn propset svn:ignore 'ignore-a' dir-a
    svn commit -m 'ignore ignore-a on dir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" '/ignore-a'
    assert git -C git-repo show master:dir-a/file-a
}

@test 'svn-ignore translation should not delete unrelated files (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    echo content-a >dir-a/file-a
    svn add dir-a/file-a
    svn commit -m 'add dir-a/file-a'
    svn update
    svn propset svn:ignore 'ignore-a' dir-a
    svn commit -m 'ignore ignore-a on dir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" '/ignore-a'
    assert git -C git-repo show master:dir-a/file-a
}

@test 'gitignore file should be removed if all svn-ignores are removed' {
    svn mkdir dir-a
    svn propset svn:ignore 'ignore-a' dir-a
    svn commit -m 'add dir-a'
    svn propset svn:ignore '' dir-a
    svn commit -m 'unignore ignore-a on dir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:dir-a/.gitignore
}

@test 'gitignore file should be removed if all svn-ignores are removed (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    svn propset svn:ignore 'ignore-a' dir-a
    svn commit -m 'add dir-a'
    svn propset svn:ignore '' dir-a
    svn commit -m 'unignore ignore-a on dir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:dir-a/.gitignore
}

@test 'gitignore file should be removed if svn-ignore property is deleted' {
    svn mkdir dir-a
    svn propset svn:ignore 'ignore-a' dir-a
    svn commit -m 'add dir-a'
    svn propdel svn:ignore dir-a
    svn commit -m 'unignore ignore-a on dir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:dir-a/.gitignore
}

@test 'gitignore file should be removed if svn-ignore property is deleted (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    svn propset svn:ignore 'ignore-a' dir-a
    svn commit -m 'add dir-a'
    svn propdel svn:ignore dir-a
    svn commit -m 'unignore ignore-a on dir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:dir-a/.gitignore
}

@test 'gitignore file should be removed if all svn-global-ignores are removed' {
    svn mkdir dir-a
    svn propset svn:global-ignores 'ignore-a' dir-a
    svn commit -m 'add dir-a'
    svn propset svn:global-ignores '' dir-a
    svn commit -m 'unignore ignore-a on dir-a and descendents'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:dir-a/.gitignore
}

@test 'gitignore file should be removed if all svn-global-ignores are removed (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    svn propset svn:global-ignores 'ignore-a' dir-a
    svn commit -m 'add dir-a'
    svn propset svn:global-ignores '' dir-a
    svn commit -m 'unignore ignore-a on dir-a and descendents'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:dir-a/.gitignore
}

@test 'gitignore file should be removed if global-ignores property is deleted' {
    svn mkdir dir-a
    svn propset svn:global-ignores 'ignore-a' dir-a
    svn commit -m 'add dir-a'
    svn propdel svn:global-ignores dir-a
    svn commit -m 'unignore ignore-a on dir-a and descendents'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:dir-a/.gitignore
}

@test 'gitignore file should be removed if global-ignores property is deleted (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    svn propset svn:global-ignores 'ignore-a' dir-a
    svn commit -m 'add dir-a'
    svn propdel svn:global-ignores dir-a
    svn commit -m 'unignore ignore-a on dir-a and descendents'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:dir-a/.gitignore
}

@test 'gitignore file should not be removed if global-ignores property is deleted but svn-ignore property is still present' {
    svn mkdir dir-a
    svn propset svn:ignore 'ignore-a' dir-a
    svn propset svn:global-ignores 'ignore-b' dir-a
    svn commit -m 'add dir-a'
    svn propdel svn:global-ignores dir-a
    svn commit -m 'unignore ignore-b on dir-a and descendents'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" '/ignore-a'
}

@test 'gitignore file should not be removed if global-ignores property is deleted but svn-ignore property is still present (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    svn propset svn:ignore 'ignore-a' dir-a
    svn propset svn:global-ignores 'ignore-b' dir-a
    svn commit -m 'add dir-a'
    svn propdel svn:global-ignores dir-a
    svn commit -m 'unignore ignore-b on dir-a and descendents'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" '/ignore-a'
}

@test 'gitignore file should not be removed if svn-ignore property is deleted but global-ignores property is still present' {
    svn mkdir dir-a
    svn propset svn:ignore 'ignore-a' dir-a
    svn propset svn:global-ignores 'ignore-b' dir-a
    svn commit -m 'add dir-a'
    svn propdel svn:ignore dir-a
    svn commit -m 'unignore ignore-a on dir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" 'ignore-b'
}

@test 'gitignore file should not be removed if svn-ignore property is deleted but global-ignores property is still present (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    svn propset svn:ignore 'ignore-a' dir-a
    svn propset svn:global-ignores 'ignore-b' dir-a
    svn commit -m 'add dir-a'
    svn propdel svn:ignore dir-a
    svn commit -m 'unignore ignore-a on dir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" 'ignore-b'
}

@test 'gitignore file should remain empty if svn-ignore property is deleted but empty-dirs is used' {
    svn mkdir dir-a
    svn propset svn:ignore 'ignore-a' dir-a
    svn commit -m 'add dir-a'
    svn propdel svn:ignore dir-a
    svn commit -m 'unignore ignore-a on dir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --empty-dirs --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    assert git -C git-repo show master:dir-a/.gitignore
    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" ''
}

@test 'gitignore file should remain empty if svn-ignore property is deleted but empty-dirs is used (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    svn propset svn:ignore 'ignore-a' dir-a
    svn commit -m 'add dir-a'
    svn propdel svn:ignore dir-a
    svn commit -m 'unignore ignore-a on dir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --empty-dirs --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    assert git -C git-repo show master:dir-a/.gitignore
    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" ''
}

@test 'gitignore file should remain empty if global-ignores property is deleted but empty-dirs is used' {
    svn mkdir dir-a
    svn propset svn:global-ignores 'ignore-a' dir-a
    svn commit -m 'add dir-a'
    svn propdel svn:global-ignores dir-a
    svn commit -m 'unignore ignore-a on dir-a and descendents'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --empty-dirs --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    assert git -C git-repo show master:dir-a/.gitignore
    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" ''
}

@test 'gitignore file should remain empty if global-ignores property is deleted but empty-dirs is used (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    svn propset svn:global-ignores 'ignore-a' dir-a
    svn commit -m 'add dir-a'
    svn propdel svn:global-ignores dir-a
    svn commit -m 'unignore ignore-a on dir-a and descendents'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --empty-dirs --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    assert git -C git-repo show master:dir-a/.gitignore
    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" ''
}
