#!/bin/sh

cp ../step2.yaca/data/output/yaca-ads.stemmed.txt.bz2 data/output/ads.stemmed.txt.bz2
cat ../step2.appstore/data/output/appstore-ads.stemmed.txt.bz2 >> data/output/ads.stemmed.txt.bz2

cp ../step2.yaca/data/output/yaca-ads.index.txt data/output/ads.index.txt
cat ../step2.appstore/data/output/appstore-ads.index.txt >> data/output/ads.index.txt
