#!/usr/bin/env python

import numpy as np
import xgboost as xgb
import matplotlib.pyplot as plt
from scipy.io import loadmat
from scipy.sparse import csr_matrix
import os.path
from pathlib import Path
import sys
import re
import datetime
import itertools

# ------------------------- Model ----------------------------------------------------------

models = [p for p in Path('.').glob('data/output/*') 
                  if p.is_dir() and re.fullmatch('\d{4}-\d{2}-\d{2}-\d{2}-\d{2}', p.name)]
def model_files_present(p):
    if not (p / 'xgb.model').exists():
        print('Incomplete model: ', p, file=sys.stderr)
        return False
    return True

models = list(filter(model_files_present, models))
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
        precision = None
        recall = None
        dataset = None
        with (p / 'info.txt').open('r') as f:
            lines = f.readlines()
            dataset = lines[0].strip().split()[-1]
            recall = lines[-3].strip().split()[1]
            precision = lines[-4].strip().split()[1]
        print(i + 1, ') ', p, ' (precision: ', precision, ', recall: ', recall, ', trained on: ', dataset, ')', sep='')
    print('Your choice [1]: ', end='')
    choice = input().strip()
    if choice == '':
        model_dir = models[0]
    else:
        model_dir = models[int(choice) - 1]

bst = xgb.Booster(model_file=str(model_dir / 'xgb.model'))
model_dataset_dir = None
dataset_basedir = None
with (model_dir / 'info.txt').open('r') as f:
    model_dataset_dir = Path(f.readline().strip().split()[-1])
    dataset_basedir = model_dataset_dir.parts[1]
model_dataset_features = None
with (model_dataset_dir / 'features-indexes.txt').open('r') as f:
    model_dataset_features = f.readlines()
    model_dataset_features.sort()


# ------------------------- Dataset --------------------------------------------------------


datasets = [p for p in Path('..').glob('{}/data/output/*'.format(dataset_basedir)) 
                    if p.is_dir() and re.fullmatch('\d{4}-\d{2}-\d{2}-\d{2}-\d{2}', p.name)]
def set_files_present(p):
    if (not (p / 'train-set.dmatrix.bin').exists() or
         not (p / 'train-set.index.txt.bz2').exists() or
         not (p / 'test-set.dmatrix.bin').exists() or
         not (p / 'test-set.index.txt.bz2').exists() or
         not (p / 'features-indexes.txt').exists()):

        print('Incomplete dataset:', p, file=sys.stderr)
        return False

    with (p / 'features-indexes.txt').open('r') as f:
        features = f.readlines()
        features.sort()
        if model_dataset_features != features:
            return False

    return True
datasets = list(filter(set_files_present, datasets))
datasets.sort(key=lambda p: p.name, reverse=True)

dataset_dir = None
if len(datasets) == 0:
    print('No suitable dataset', file=sys.stderr)
    exit(1)
elif len(datasets) == 1:
    dataset_dir = str(datasets[0])
    print('Using', dataset_dir)
else:
    print('Choose dataset:')
    for i, p in enumerate(datasets):
        ads_split = None
        wiki_ads_split = None
        non_ads_split = None
        with (p / 'info.txt').open('r') as f:
            lines = f.readlines()
            wiki_ads_split = lines[0].strip().split()[1]
            ads_split = lines[1].strip().split()[1]
            non_ads_split = lines[2].strip().split()[1]
        print(i + 1, ') ', p, ' (ads: ', ads_split, ', wiki ads: ', wiki_ads_split, ', non-ads split: ', non_ads_split, ')', sep='')
    print('Your choice [1]: ', end='')
    choice = input().strip()
    if choice == '':
        dataset_dir = str(datasets[0])
    else:
        dataset_dir = str(datasets[int(choice) - 1])

# --------------- Part -----------------------------------------------------------------------

print('Evaluate on train (1) or test (2) set?')
print('Your choice [1]: ', end='')
choice = input().strip()
if choice == '' or choice == '1':
    choice = 'train'
elif choice == '2':
    choice = 'test'
else:
    print('WAT', file=sys.stderr)
    exit(2)

# ------------ Threshold ---------------------------------------------------------------------
print("Input threshold [0.1]: ", end='')
threshold = input().strip()
if threshold == '':
    threshold = 0.1
else:
    threshold = float(threshold)

dtest = xgb.DMatrix('{}/{}-set.dmatrix.bin'.format(dataset_dir, choice))
if dtest.num_row() == 0: 
    print('Empty', choice, 'set', file=sys.stderr)
    exit(3)

labels = dtest.get_label()

predicted = bst.predict(dtest)
predicted = (predicted > threshold).astype(int)

tp = sum(np.logical_and(predicted == 1, labels == 1).astype(int))
fp = sum(np.logical_and(predicted == 1, labels == 0).astype(int))
fn = sum(np.logical_and(predicted == 0, labels == 1).astype(int))
precision = tp / (tp + fp)
recall = tp / (tp + fn)
f1 = 2 * precision * recall / (precision + recall)

print('On', choice)
print('precision:', precision)
print('recall:', recall)
print('f1:', f1)

