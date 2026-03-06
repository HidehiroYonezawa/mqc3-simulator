#!/bin/bash -eu

docker run --rm -v $(pwd):/data -it hrektts/doxygen doxygen
sudo chown -R ${USER}:${USER} html/
cp -r docs/ html/
