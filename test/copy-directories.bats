load 'common'

@test 'copying a directory with source not in target repo should dump full directory' {
    svn mkdir --parents project-a/dir-a
    touch project-a/dir-a/file-a
    svn add project-a/dir-a/file-a
    svn commit -m 'add project-a/dir-a/file-a'
    svn mkdir --parents project-b
    svn commit -m 'add project-b'
    svn cp project-a/dir-a project-b
    svn commit -m 'copy project-a/dir-a to project-b'

    cd "$TEST_TEMP_DIR"
    svn2git "$SVN_REPO" --debug-rules --rules <(echo "
        create repository git-repo
        end repository

        match /project-b/
            repository git-repo
            branch master
        end match

        match /project-a/
        end match
    ")

    assert git -C git-repo show master:dir-a/file-a
}
