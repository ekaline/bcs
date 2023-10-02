#!/usr/bin/env bash

# Updates the doxygen documentation, and copies it into the appropriate place
# in the gh-pages branch.

set -e

tempdir=`mktemp -d`
branch=`git rev-parse --abbrev-ref HEAD`

cp -r /home/vitaly/libeka_git/libeka/cereal/doc/html/ ${tempdir}

git stash
git checkout gh-pages

rm -rf /home/vitaly/libeka_git/libeka/cereal/assets/doxygen
mkdir /home/vitaly/libeka_git/libeka/cereal/assets/doxygen
cp -r ${tempdir}/html/* /home/vitaly/libeka_git/libeka/cereal/assets/doxygen/

rm -rf ${tempdir}

git commit /home/vitaly/libeka_git/libeka/cereal/assets/doxygen

git checkout ${branch}
git stash apply
