#!/bin/bash
rsync -avP /flycast-dojo/ /tmp/flycast-local/

cd /tmp/flycast-local
git config --global --add safe.directory /tmp/flycast-local
for dir in `ls -1 core/deps`; do git config --global --add safe.directory /tmp/flycast-local/core/deps/$dir; done
git submodule init
git submodule sync
git submodule update
cd /tmp/flycast-local/core/deps/libchdr
git reset --hard
git pull origin master

cd /tmp/flycast-local
cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build --config Release

cp -R /tmp/flycast-local/build/flycast /flycast-dojo/build/
