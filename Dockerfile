FROM debian:11

RUN apt update && apt install -y \
    make g++ libapr1-dev libsvn-dev qtbase5-dev \
    git subversion \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir /usr/local/svn2git

ADD . /usr/local/svn2git

RUN cd /usr/local/svn2git && qmake && make \
    && ln -st /usr/local/bin /usr/local/svn2git/svn-all-fast-export \
    && cp /usr/local/bin/svn-all-fast-export /usr/local/bin/svn2git

WORKDIR /workdir
CMD /usr/local/svn2git/svn-all-fast-export
