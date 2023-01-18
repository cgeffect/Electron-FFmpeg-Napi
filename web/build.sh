workdir=$(cd $(dirname $0); pwd -P)
cd ${workdir}

git submodule update --init

cd ./psd-tools
python3 ./setup.py build

cd ${workdir}