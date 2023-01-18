#!/bin/sh
workdir=$(cd $(dirname $0); pwd)
echo $(pwd)
echo ${workdir}

build_type="Release"
if [[ x"$1" == x"Debug" ]]; then
    build_type="Debug"
fi

echo ${build_type}

rm -rf ./build
mkdir -p ./build
cd ./build
cmake ../ -DCMAKE_BUILD_TYPE=${build_type} && \
make