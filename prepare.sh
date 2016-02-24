#!/bin/sh

dir=$(dirname $0)
pushd $dir

mkdir -p step*/data/{input,output}
mv ./YaCa_02.2014_business.domains-only.csv step2/data/input/

wget https://dumps.wikimedia.org/ruwiki/latest/ruwiki-latest-pages-articles-multistream.xml.bz2 -O step1/data/input/ruwiki-pages-articles-multistream.xml.bz2
if [ $(uname -m) == "x86_64" ]; then
    wget http://download.cdn.yandex.net/mystem/mystem-3.0-linux3.1-64bit.tar.gz -O mystem.tar.gz
else
    wget http://download.cdn.yandex.net/mystem/mystem-3.0-linux3.5-32bit.tar.gz -O mystem.tar.gz
fi

tar -xf mystem.tar.gz
rm mystem.tar.gz


if [ -z $(command -v python3) ]; then
    echo No python3 >&2
    exit 1
fi

if [ -z $(command -v phantomjs) ]; then
    echo No phantomjs >&2
    exit 2
fi

build_xgboost() {

    git clone --recursive https://github.com/dmlc/xgboost
    pushd xgboost
    make -j4
    pushd python-package
    python3 setup.py --user
    popd
    popd

}
if [ -z $(command -v xgboost) -a ! -d xgboost ]; then
    read -p "Build xgboost? [Y/n]" yn
    case $yn in
        [Yy]*|"" ) build_xgboost;;
        * ) echo You need xgboost >&2; exit 3;;
    esac
fi

pushd step3
make
popd


pushd step3_

mkdir data/temp

git clone https://github.com/stanfordnlp/GloVe.git
pushd GloVe
make
popd

make

popd # step3_


popd
