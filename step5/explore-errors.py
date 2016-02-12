#!/usr/bin/env python3

import xgboost as xgb
from matplotlib import rc
from matplotlib.pyplot import plot, draw, show
import bz2
import subprocess

rc('font', family='Droid Sans')


dtest = xgb.DMatrix('../step3/data/output/test-set.dmatrix.bin')

ads = dtest.slice(list(range(601)))
non_ads = dtest.slice(list(range(601, dtest.num_row())))
del dtest

bst = xgb.Booster(model_file='../step4/data/output/xgb.model')

def load_titles(filename, titles_list):
    with open(filename, 'r') as f:
        title = []
        for l in f:
            if l.startswith('samplesSeparator'):
                titles_list.append('\n'.join(title))
                title = []
            else:
                title.append(l.strip())


def explore_ads():
    predicted = list(enumerate(bst.predict(ads)))
    predicted.sort(key=lambda p: p[1])

    ads_titles = []
    load_titles('../step1/data/output/ads-samples.index.txt', ads_titles)

    assert len(ads_titles) == len(predicted)
    for i, p in predicted:
        print('Article "', ads_titles[i], '" is an ad with P = ', p, sep='')
        try:
            input()
        except KeyboardInterrupt: break

def explore_non_ads():
    predicted = list(enumerate(bst.predict(non_ads)))
    predicted.sort(key=lambda p: p[1], reverse=True)

    samples_indices = []
    with open('../step3/data/output/featured-test-samples.txt', 'r') as f:
        f.readline()
        f.readline()
        for l in f:
            samples_indices.append(int(l.strip()))
    samples_indices.sort()


    all_non_ads_titles = []
    for i in range(8):
        load_titles('../step1/data/output/featured-samples.{}.index.txt'.format(i), all_non_ads_titles)

    non_ads_titles = []
    for index in samples_indices:
        non_ads_titles.append(all_non_ads_titles[index])

    del samples_indices, all_non_ads_titles

    assert len(non_ads_titles) == len(predicted), '{} and {}'.format(len(non_ads_titles), len(predicted))
    for i, p in predicted:
        print('Article "', non_ads_titles[i], '" is an ad with P = ', p, sep='')
        subprocess.run(['chromium', 'https://ru.wikipedia.org/wiki/{}'.format(non_ads_titles[i])])
        try:
            input()
        except KeyboardInterrupt: break

explore_non_ads()


feature_names = [0]*(ads.num_col() + non_ads.num_col())
feature_map = {}
with open("../step3/data/output/features-indexes.txt", 'r') as f:
    for l in f:
        name, index = l.strip().split(':')
        name = name.replace(' ', '-')
        feature_names[int(index)] = name
        feature_map[name] = int(index)
bst.feature_names = feature_names
p = xgb.plot_importance(bst)
p.figure.show()

xgb.plot_tree(bst, num_trees=feature_map['вы'])

show()
