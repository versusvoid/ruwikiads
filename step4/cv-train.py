#!/usr/bin/env python

import numpy as np
import xgboost as xgb
from scipy.io import loadmat
from scipy.sparse import csr_matrix
import os.path
from pathlib import Path
import sys
import re
import datetime

datasets = [p for p in Path('..').glob('step3*/data/output/*') 
                    if p.is_dir() and re.fullmatch('\d{4}-\d{2}-\d{2}-\d{2}-\d{2}', p.name)]
def set_files_present(p):
    if (not (p / 'train-set.dmatrix.bin').exists() or
         not (p / 'train-set.index.txt.bz2').exists() or
         not (p / 'test-set.dmatrix.bin').exists() or
         not (p / 'test-set.index.txt.bz2').exists() or
         not (p / 'features-indexes.txt').exists()):

        print('Incomplete dataset: ', p, file=sys.stderr)
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
else:
    print('Choose dataset:')
    for i, p in enumerate(datasets):
        print(i + 1, ') ', p, sep='')
    print('Your choice [1]: ', end='')
    choice = input().strip()
    if choice == '':
        dataset_dir = str(datasets[0])
    else:
        dataset_dir = str(dtatsets[int(choice) - 1])


def load_set(which='train'):
    d = loadmat('../step3/data/output/{}-set.mat'.format(which))
    m = d['m'].tocsr()
    print(m.shape)
    labels = np.array(d['labels'].tocsr().todense())[0,:]
    del d
    return m, labels

def load_mat(which='train'):
    d = xgb.DMatrix('{}/{}-set.dmatrix.bin'.format(dataset_dir, which))
    return d

print('Train on train (1) or test (2) set?')
print('Your choice [1]:', end='')
choice = input().strip()
if choice == '' or choice == '1':
    choice = 'train'
elif choice == '2':
    choice = 'test'
else:
    print('WAT', file=sys.stderr)
    exit(2)

dtrain = load_mat(choice)
if dtrain.num_row() == 0: 
    print('Empty', choice, 'set', file=sys.stderr)
    exit(3)

num_rounds=150
#params = {'max_depth':4, 'eta':0.1, 'subsample':0.5, 'lambda':10, 'silent':1, 'objective':'binary:logistic' }
params = {'max_depth':6, 'eta':0.3, 'subsample':1.0, 'lambda':6, 'silent':1, 'objective':'binary:logistic'}
if '-cv' in sys.argv:
    xgb.cv(params, dtrain, num_boost_round=num_rounds, nfold=10, show_progress=True)
    exit()


bst = xgb.train(params, dtrain, num_boost_round=num_rounds, evals=[(dtrain,'train')])
del dtrain
#xgb.train(params, dtest, num_boost_round=num_rounds, evals=[(dtrain,'train'), (dtest, 'test')], xgb_model=bst)
output_dir = datetime.datetime.now().strftime('data/output/%Y-%m-%d-%H-%M')
os.makedirs(output_dir)
bst.save_model('{}/xgb.model'.format(output_dir))

def test_on_set(which, f):
    #m, labels = load_set(which)
    #dtest = xgb.DMatrix(m, label=labels)

    dtest = load_mat(which)
    if dtest.num_row() == 0: 
        print('Empty', which, 'set', file=sys.stderr)
        return
    labels = dtest.get_label()

    predicted = bst.predict(dtest)
    predicted = (predicted > 0.1).astype(int)

    tp = sum(np.logical_and(predicted == 1, labels == 1).astype(int))
    fp = sum(np.logical_and(predicted == 1, labels == 0).astype(int))
    fn = sum(np.logical_and(predicted == 0, labels == 1).astype(int))
    precision = tp / (tp + fp)
    recall = tp / (tp + fn)
    f1 = 2 * precision * recall / (precision + recall)

    for of in [f, sys.stdout]:
        print('On', which, file=of)
        print('precision:', precision, file=of)
        print('recall:', recall, file=of)
        print('f1:', f1, file=of)
        print(file=of)

with open('{}/info.txt'.format(output_dir), 'w') as f:
    print('Trained on', dataset_dir, end='\n\n', file=f)

    test_on_set('train', f)
    test_on_set('test', f)
