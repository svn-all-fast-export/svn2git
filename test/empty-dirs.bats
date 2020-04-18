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

@test 'empty-dirs parameter should not cause added directories to be dumped multiple times' {
    svn mkdir dir-a
    echo content-a >dir-a/file-a
    svn add dir-a/file-a
    svn commit -m 'add dir-a/file-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --create-dump --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    assert [ "$(grep -c '^M .* dir-a/file-a$' git-repo.fi)" -eq 1 ]
}

@test 'empty-dirs parameter should not cause added directories to be dumped multiple times (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    echo content-a >dir-a/file-a
    svn add dir-a/file-a
    svn commit -m 'add dir-a/file-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --create-dump --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    assert [ "$(grep -c '^M .* dir-a/file-a$' git-repo.fi)" -eq 1 ]
}

@test 'deleting last file from a directory should add empty .gitignore with empty-dirs-parameter' {
    svn mkdir dir-a
    touch dir-a/file-a
    svn add dir-a/file-a
    svn commit -m 'add dir-a/file-a'
    svn rm dir-a/file-a
    svn commit -m 'delete dir-a/file-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:.gitignore
    assert git -C git-repo show master:dir-a/.gitignore
    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" ''
}

@test 'deleting last file from a directory should add empty .gitignore with empty-dirs-parameter (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    touch dir-a/file-a
    svn add dir-a/file-a
    svn commit -m 'add dir-a/file-a'
    svn rm dir-a/file-a
    svn commit -m 'delete dir-a/file-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:.gitignore
    assert git -C git-repo show master:dir-a/.gitignore
    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" ''
}

@test 'deleting last file from a directory should not add empty .gitignore with empty-dirs-parameter and svn-ignore-parameter if there is an svn:ignore property' {
    svn mkdir dir-a
    touch dir-a/file-a
    svn add dir-a/file-a
    svn commit -m 'add dir-a/file-a'
    svn propset svn:ignore 'ignore-a' dir-a
    svn commit -m 'ignore ignore-a on dir-a'
    svn rm dir-a/file-a
    svn commit -m 'delete dir-a/file-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:.gitignore
    assert git -C git-repo show master:dir-a/.gitignore
    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" '/ignore-a'
}

@test 'deleting last file from a directory should not add empty .gitignore with empty-dirs-parameter and svn-ignore-parameter if there is an svn:ignore property (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    touch dir-a/file-a
    svn add dir-a/file-a
    svn commit -m 'add dir-a/file-a'
    svn propset svn:ignore 'ignore-a' dir-a
    svn commit -m 'ignore ignore-a on dir-a'
    svn rm dir-a/file-a
    svn commit -m 'delete dir-a/file-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:.gitignore
    assert git -C git-repo show master:dir-a/.gitignore
    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" '/ignore-a'
}

@test 'deleting last directory from a directory should add empty .gitignore with empty-dirs-parameter' {
    svn mkdir --parents dir-a/subdir-a
    svn commit -m 'add dir-a/subdir-a'
    svn rm dir-a/subdir-a
    svn commit -m 'delete dir-a/subdir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:.gitignore
    assert git -C git-repo show master:dir-a/.gitignore
    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" ''
}

@test 'deleting last directory from a directory should add empty .gitignore with empty-dirs-parameter (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir --parents dir-a/subdir-a
    svn commit -m 'add dir-a/subdir-a'
    svn rm dir-a/subdir-a
    svn commit -m 'delete dir-a/subdir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:.gitignore
    assert git -C git-repo show master:dir-a/.gitignore
    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" ''
}

@test 'deleting last directory from a directory should not add empty .gitignore with empty-dirs-parameter and svn-ignore-parameter if there is an svn:ignore property' {
    svn mkdir --parents dir-a/subdir-a
    svn commit -m 'add dir-a/subdir-a'
    svn propset svn:ignore 'ignore-a' dir-a
    svn commit -m 'ignore ignore-a on dir-a'
    svn rm dir-a/subdir-a
    svn commit -m 'delete dir-a/subdir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:.gitignore
    assert git -C git-repo show master:dir-a/.gitignore
    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" '/ignore-a'
}

@test 'deleting last directory from a directory should not add empty .gitignore with empty-dirs-parameter and svn-ignore-parameter if there is an svn:ignore property (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir --parents dir-a/subdir-a
    svn commit -m 'add dir-a/subdir-a'
    svn propset svn:ignore 'ignore-a' dir-a
    svn commit -m 'ignore ignore-a on dir-a'
    svn rm dir-a/subdir-a
    svn commit -m 'delete dir-a/subdir-a'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --empty-dirs --svn-ignore --rules <(echo "
        create repository git-repo
        end repository

        match /project-a/
            repository git-repo
            branch master
        end match
    ")

    refute git -C git-repo show master:.gitignore
    assert git -C git-repo show master:dir-a/.gitignore
    assert_equal "$(git -C git-repo show master:dir-a/.gitignore)" '/ignore-a'
}

@test 'copying an empty directory should put empty .gitignore file to copy with empty-dirs parameter' {
    svn mkdir dir-a
    svn commit -m 'add dir-a'
    svn cp dir-a dir-b
    svn commit -m 'copy dir-a to dir-b'

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
    assert git -C git-repo show master:dir-b/.gitignore
    assert_equal "$(git -C git-repo show master:dir-b/.gitignore)" ''
}

@test 'copying an empty directory should put empty .gitignore file to copy with empty-dirs parameter (nested)' {
    svn mkdir project-a
    cd project-a
    svn mkdir dir-a
    svn commit -m 'add dir-a'
    svn cp dir-a dir-b
    svn commit -m 'copy dir-a to dir-b'

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
    assert git -C git-repo show master:dir-b/.gitignore
    assert_equal "$(git -C git-repo show master:dir-b/.gitignore)" ''
}
