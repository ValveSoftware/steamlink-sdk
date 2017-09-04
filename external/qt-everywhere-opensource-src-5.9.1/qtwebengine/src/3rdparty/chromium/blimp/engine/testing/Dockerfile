# This Dockerfile is used to build a filesystem environment containing
# binaries and required files for Blimp Engine test and tests for dependencies
# of Blimp Engine. It is built on the same base image that is used to run the
# Engine itself.
FROM base:latest

RUN mkdir /tmp/blimp/

RUN useradd -ms /bin/bash blimp_user

# Put all the Blimp related files in /blimp so they are kept separate from
# the OS files. Using '.' instead of '*' ensures directory structure is
# maintained since ADD only copies the contents of directories. This is done by
# first adding the files to directory under /tmp/blimp and then copying it to
# target /blimp location to workaround Docker permission bug
# (https://github.com/docker/docker/issues/7511 and
# https://github.com/docker/docker/issues/1295).
ADD . /tmp/blimp/

RUN cp -r /tmp/blimp /blimp && \
    chown -R blimp_user /blimp && \
    rm -rf /tmp/blimp

USER blimp_user
