Based on a bug report by Ge Bae: https://github.com/svn-all-fast-export/svn2git/issues/43

When an svn transaction replaces an existing directory from another directory, it
deletes any pre-existing files before the copy.

svn-all-fast-export did not perform the remove and left the pre-existing files in-place.

	r1:
      A trunk/folder/file1.txt
    r2:
      A branches/b1 from trunk
    r3:
      D trunk/folder/file1.txt
      A trunk/folder/file2.txt
    r4:
      A branches/b2 from trunk
    r5:
      # how svn log shows a replace, but it is a discrete transaction of type
      # svn_fs_path_change_replace
      D branches/b1/folder
      A branches/b1/folder from trunk

In the SVN repos, b1/folder contains file2.txt and not file1.txt.

In a converted git repos, b1/folder would contain both file1.txt and file2.txt.

The code in this folder was written with a view to constructing a general purpose
test harness that could test more than just this one case.


-Oliver 'kfsone' Smith <oliver@kfs.org>

