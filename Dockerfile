FROM debian:8.11

RUN apt update && apt install -y \
    make g++ libapr1-dev libsvn-dev libqt4-dev \
    git subversion \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir /usr/local/svn2git

ADD . /usr/local/svn2git

RUN cd /usr/local/svn2git && qmake && make

WORKDIR /workdir
CMD /usr/local/svn2git/svn-all-fast-export
