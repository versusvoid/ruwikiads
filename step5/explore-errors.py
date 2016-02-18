#!/usr/bin/env python3

import xgboost as xgb
import numpy as np
from matplotlib import rc
from matplotlib.pyplot import plot, draw, show
import bz2
import subprocess
from pathlib import Path
import re

rc('font', family='Droid Sans')

def get_model_dataset_dir(model_dir):
    with (model_dir / 'info.txt').open() as f:
        l = f.readline().strip()
        return Path(l[l.find('../step3'):])

models = [p for p in Path('..').glob('step4/data/output/*') 
                  if p.is_dir() and re.fullmatch('\d{4}-\d{2}-\d{2}-\d{2}-\d{2}', p.name)]
def set_files_present(p):
    if (not (p / 'info.txt').exists() or not (p / 'xgb.model').exists()):
        print('Incomplete model: ', p, file=sys.stderr)
        return False

    dataset_dir = get_model_dataset_dir(p)

    if (not dataset_dir.exists() or
         not (dataset_dir / 'train-set.index.txt.bz2').exists() or
         not (dataset_dir / 'train-set.dmatrix.bin').exists() or
         not (dataset_dir / 'test-set.index.txt.bz2').exists() or
         not (dataset_dir / 'test-set.dmatrix.bin').exists() or
         not (dataset_dir / 'features-indexes.txt').exists()):

        print('Incomplete dataset', dataset_dir, 'for model', p, file=sys.stderr)
        return False

    return True

models = list(filter(set_files_present, models))
models.sort(key=lambda p: p.name, reverse=True)

model_dir = None
if len(models) == 0:
    print('No suitable model', file=sys.stderr)
    exit(1)
elif len(models) == 1:
    model_dir = models[0]
    print('Using', model_dir)
else:
    print('Choose model:')
    for i, p in enumerate(models):
        print(i + 1, ') ', p, sep='')
    print('Your choice [1]: ', end='')
    choice = input().strip()
    if choice == '':
        model_dir = models[0]
    else:
        model_dir = dtatsets[int(choice) - 1]

dataset_dir = get_model_dataset_dir(model_dir)

print('Explore test (1) or train (2) set?')
print('Your choice [1]: ', end='')
which = input().strip()
if which == '' or which == '1':
    which = 'test'
elif which == '2':
    which = 'train'

dataset = xgb.DMatrix(str( dataset_dir / '{}-set.dmatrix.bin'.format(which) ))
bst = xgb.Booster(model_file=str(model_dir / 'xgb.model'))

def load_titles(label):
    p = subprocess.Popen(['bunzip2', '-k', '-c', str(dataset_dir / '{}-set.index.txt.bz2'.format(which))], 
                            stdout=subprocess.PIPE, universal_newlines=True)

    title_parts = []
    i = 0
    titles = []
    indices = []
    for l in p.stdout:
        if l.startswith('samplesSeparator'):
            if title_parts[0][0] == label:
                titles.append(''.join(title_parts[1:]))
                indices.append(i)

            i += 1
            title_parts = []
        else:
            title_parts.append(l)

    return indices, titles

def sanity_check(row_indices, label):
    labels = dataset.get_label()
    indices = np.nonzero(labels) if label == '1' else np.nonzero(1 - labels)
    indices = indices[0]
    assert np.all(row_indices == indices)

IGNORE_WEB_PAGES = True
def explore_samples(label):
    row_indices, titles = load_titles(label)
    sanity_check(row_indices, label)
    dataset_slice = dataset.slice(row_indices)
    predicted = list(zip(bst.predict(dataset_slice), titles))
    predicted.sort(key=lambda p: p[0], reverse=(label == '0'))


    assert len(titles) == len(predicted)
    for p, title in predicted:
        if title.startswith('http'):
            print('Page at', title, 'is an ad with P =', p)
            if IGNORE_WEB_PAGES: continue
            subprocess.run(['chromium', title])
        else:
            print('Article "', title, '" is an ad with P = ', p, sep='')
            subprocess.run(['chromium', 'https://ru.wikipedia.org/wiki/{}'.format(title)])
        try:
            input()
        except KeyboardInterrupt: break

print('Explore ads samples? [y/N]: ', end='')
if input().strip().lower().startswith('y'):
    explore_samples('1')

print('Explore non ads samples? [y/N]: ', end='')
if input().strip().lower().startswith('y'):
    explore_samples('0')

print('Draw features importance? [y/N]: ', end='')
if input().strip().lower().startswith('y'):
    feature_names = [0]*dtest.num_col()
    feature_map = {}
    with (dataset_dir / "features-indexes.txt").open() as f:
        for l in f:
            name, index = l.strip().split(':')
            name = name.replace(' ', '-')
            feature_names[int(index)] = name
            feature_map[name] = int(index)
    assert len(feature_map) == dtest.num_col()
    bst.feature_names = feature_names
    p = xgb.plot_importance(bst)
    p.figure.show()

    show()
