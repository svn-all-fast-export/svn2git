svn-all-fast-export aka svn2git
===============================

This is the tool used to convert KDE's Subversion into multiple Git
repositories.  You can find more description and usage examples at
https://techbase.kde.org/Projects/MoveToGit/UsingSvn2Git


Building
--------

Run `qmake && make`.  You get `./svn-all-fast-export`.


Usage
-----

You need to write a rules file that describes how to slice the Subversion
history into Git repositories and branches.  See
https://techbase.kde.org/Projects/MoveToGit/UsingSvn2Git.
