#!/bin/bash
rsync -avP /flycast-dojo/ /flycast-local/

cd /flycast-local
git submodule init
git submodule sync
git submodule update
cd /flycast-local/core/deps/libchdr
git reset --hard
git pull

cd /flycast-local/shell/linux
make

cp /flycast-local/shell/linux/flycast.elf /flycast-dojo
