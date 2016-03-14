#!/bin/sh

cat ../step2.appstore/data/output/appstore-ads.stemmed.txt.bz2 >> ../step2.yaca/data/output/yaca-ads.stemmed.txt.bz2
rm ../step2.appstore/data/output/appstore-ads.stemmed.txt.bz2
mv ../step2.yaca/data/output/yaca-ads.stemmed.txt.bz2 data/output/ads.stemmed.txt.bz2

cat ../step2.appstore/data/output/appstore-ads.index.txt >> ../step2.yaca/data/output/yaca-ads.index.txt
rm ../step2.appstore/data/output/appstore-ads.index.txt
mv ../step2.yaca/data/output/yaca-ads.index.txt data/output/ads.index.txt
