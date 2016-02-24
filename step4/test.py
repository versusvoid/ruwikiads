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
        print(i + 1, ') ', p, sep='')
    print('Your choice [1]: ', end='')
    choice = input().strip()
    if choice == '':
        model_dir = models[0]
    else:
        model_dir = dtatsets[int(choice) - 1]

bst = xgb.Booster(model_file=str(model_dir / 'xgb.model'))

# ------------------------- Dataset --------------------------------------------------------


datasets = [p for p in Path('..').glob('step3*/data/output/*') 
                    if p.is_dir() and re.fullmatch('\d{4}-\d{2}-\d{2}-\d{2}-\d{2}', p.name)]
def set_files_present(p):
    if (not (p / 'train-set.dmatrix.bin').exists() or
         not (p / 'train-set.index.txt.bz2').exists() or
         not (p / 'test-set.dmatrix.bin').exists() or
         not (p / 'test-set.index.txt.bz2').exists() or
         not (p / 'features-indexes.txt').exists()):

        print('Incomplete dataset:', p, file=sys.stderr)
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
        print(i + 1, ') ', p, sep='')
    print('Your choice [1]: ', end='')
    choice = input().strip()
    if choice == '':
        dataset_dir = str(datasets[0])
    else:
        dataset_dir = str(datasets[int(choice) - 1])

# --------------- Part -----------------------------------------------------------------------

print('Evaluate on train (1) or test (2) set?')
print('Your choice [1]:', end='')
choice = input().strip()
if choice == '' or choice == '1':
    choice = 'train'
elif choice == '2':
    choice = 'test'
else:
    print('WAT', file=sys.stderr)
    exit(2)

dtest = xgb.DMatrix('{}/{}-set.dmatrix.bin'.format(dataset_dir, choice))
if dtest.num_row() == 0: 
    print('Empty', choice, 'set', file=sys.stderr)
    exit(3)

labels = dtest.get_label()

predicted = bst.predict(dtest)
predicted = (predicted > 0.1).astype(int)

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

