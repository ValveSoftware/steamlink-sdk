#!/bin/bash -e
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This tool is used to update libvpx source code with the latest git
# repository.
#
# Make sure you run this in a svn checkout of deps/third_party/libvpx!

# Usage:
#
# $ ./update_libvpx.sh [branch | revision | file or url containing a revision]
# When specifying a branch it may be necessary to prefix with origin/

# Tools required for running this tool:
#
# 1. Linux / Mac
# 2. svn
# 3. git

export LC_ALL=C

# Location for the remote git repository.
GIT_REPO="http://git.chromium.org/webm/libvpx.git"

GIT_BRANCH="origin/master"
LIBVPX_SRC_DIR="source/libvpx"
BASE_DIR=`pwd`

if [ -n "$1" ]; then
  GIT_BRANCH="$1"
  if [ -f "$1"  ]; then
    GIT_BRANCH=$(<"$1")
  elif [[ $1 = http* ]]; then
    GIT_BRANCH=`curl $1`
  fi
fi

prev_hash="$(egrep "^Commit: [[:alnum:]]" README.chromium | awk '{ print $2 }')"
echo "prev_hash:$prev_hash"

rm -rf $(svn ls $LIBVPX_SRC_DIR)
svn update $LIBVPX_SRC_DIR

cd $LIBVPX_SRC_DIR

# Make sure git doesn't mess up with svn.
echo ".svn" >> .gitignore

# Start a local git repo.
git init
git add .
git commit -a -m "Current libvpx"

# Add the remote repo.
git remote add origin $GIT_REPO
git fetch

add="$(git diff-index --diff-filter=D $GIT_BRANCH | \
tr -s '\t' ' ' | cut -f6 -d\ )"
delete="$(git diff-index --diff-filter=A $GIT_BRANCH | \
tr -s '\t' ' ' | cut -f6 -d\ )"

# Switch the content to the latest git repo.
git checkout -b tot $GIT_BRANCH

# Output the current commit hash.
hash=$(git log -1 --format="%H")
echo "Current HEAD: $hash"

# Output log for upstream from current hash.
if [ -n "$prev_hash" ]; then
  echo "git log from upstream:"
  pretty_git_log="$(git log --no-merges --pretty="%h %s" $prev_hash..$hash)"
  if [ -z "$pretty_git_log" ]; then
    echo "No log found. Checking for reverts."
    pretty_git_log="$(git log --no-merges --pretty="%h %s" $hash..$prev_hash)"
  fi
  echo "$pretty_git_log"
fi

# Git is useless now, remove the local git repo.
rm -rf .git

# Update SVN with the added and deleted files.
echo "$add" | xargs -I {} svn add --parents {}
echo "$delete" | xargs -I {} svn rm {}

# Find empty directories and remove them from SVN.
find . -type d -empty -not -iwholename '*.svn*' -exec svn rm {} \;

chmod 755 build/make/*.sh build/make/*.pl configure

cd $BASE_DIR
