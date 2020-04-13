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

        match /([^/]+(/|$))
            repository git-repo
            branch master
            prefix \1
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

        match /([^/]+(/|$))
            repository git-repo
            branch master
            prefix \1
        end match
    ")

    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" "$(cat <<-EOF
			ignore-a
			ignore-b
		EOF
    )"
}
