load 'common'

@test 'empty-dirs parameter should put empty .gitignore files to empty directories' {
    svn mkdir dir-a
    svn commit -m 'add dir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --rules <(echo "
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

@test 'empty-dirs parameter should put empty .gitignore files to empty directories (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    svn commit -m 'add dir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --rules <(echo "
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

@test 'empty-dirs parameter should not put empty .gitignore files to non-empty directories' {
    svn mkdir dir-a
    touch dir-a/file-a
    svn add dir-a/file-a
    svn commit -m 'add dir-a/file-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:dir-a/.gitignore
}

@test 'empty-dirs parameter should not put empty .gitignore files to non-empty directories (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    touch dir-a/file-a
    svn add dir-a/file-a
    svn commit -m 'add dir-a/file-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:dir-a/.gitignore
}

@test 'empty-dirs parameter should not put empty .gitignore files to directories with generated .gitignore' {
    svn mkdir dir-a
    svn propset svn:ignore $'ignore-a\nignore-b' dir-a
    svn commit -m 'add dir-a with ignores'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --empty-dirs --rules <(echo "
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

@test 'empty-dirs parameter should not put empty .gitignore files to directories with generated .gitignore (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    svn propset svn:ignore $'ignore-a\nignore-b' dir-a
    svn commit -m 'add dir-a with ignores'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --svn-ignore --empty-dirs --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
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

@test 'empty-dirs parameter should not put empty .gitignore files to directories with .gitignore' {
    svn mkdir dir-a
    echo ignore-a >dir-a/.gitignore
    svn add dir-a/.gitignore
    svn commit -m 'add dir-a/.gitignore'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" 'ignore-a'
}

@test 'empty-dirs parameter should not put empty .gitignore files to directories with .gitignore (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    echo ignore-a >dir-a/.gitignore
    svn add dir-a/.gitignore
    svn commit -m 'add dir-a/.gitignore'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" 'ignore-a'
}
