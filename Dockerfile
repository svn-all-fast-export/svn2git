FROM ubuntu:22.04

# Change locale to let svn handle international characters
ENV LC_ALL C.UTF-8

# Install dependencies
RUN apt-get update && apt-get install --yes --no-install-recommends \
    build-essential \
    libapr1-dev \
    libsvn-dev \
    qt5-qmake \
    qtbase5-dev \
    git \
    subversion \
    && rm -rf /var/lib/apt/lists/*

# Build the binary
RUN mkdir /usr/local/svn2git
ADD . /usr/local/svn2git
RUN cd /usr/local/svn2git && qmake && make

# Docker interface
WORKDIR /workdir
CMD /usr/local/svn2git/svn-all-fast-export
