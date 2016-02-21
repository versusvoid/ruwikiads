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

def load_mat(which):
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

THRESHOLD = 0.1
def PRF1(predicted, dmatrix):
    labels = dmatrix.get_label()
    predicted = (predicted > THRESHOLD).astype(int)
    tp = sum(np.logical_and(predicted == 1, labels == 1).astype(int))
    fp = sum(np.logical_and(predicted == 1, labels == 0).astype(int))
    fn = sum(np.logical_and(predicted == 0, labels == 1).astype(int))
    precision = tp / (tp + fp)
    recall = tp / (tp + fn)

    return [('1-recall', recall), ('2-precision', precision)]


#params = {'max_depth':4, 'eta':0.1, 'subsample':0.5, 'lambda':10, 'silent':1, 'objective':'binary:logistic' }
if '-cv' in sys.argv:
    with open('data/output/cv-results.{}.txt'.format(datetime.datetime.now().strftime('%Y-%m-%d-%H-%M')), 'w') as f:
        print('Dataset:', dataset_dir, end='\n\n', file=f)
        gridsearches = []
        for num_rounds, max_depth, eta, subsample, xgb_lambda in itertools.product([150], [6], [0.1], [0.5], [5]):
            tunable_params = {'max_depth':max_depth, 'eta':eta, 'subsample':subsample, 'lambda':xgb_lambda}
            params = {'objective':'binary:logistic', 'silent': True, 'nthread':4}
            params.update(tunable_params)
            print(num_rounds, tunable_params)
            print('num_rounds =', num_rounds, file=f)
            print(tunable_params, file=f)
            res = xgb.cv(params, dtrain, num_boost_round=num_rounds, nfold=3, 
                            show_progress=True, show_stdv=True, 
                            early_stopping_rounds=None, maximize=True, feval=PRF1)
            plt.plot(range(num_rounds), res['test-1-recall-mean'], 'r-')
            plt.plot(range(num_rounds), res['test-3-precision-mean'], 'g-')
            plt.savefig('test-means-{}-{}-{}.png'.format(num_rounds, eta, xgb_lambda))
            plt.clf()

            res = tuple(zip(res.values[-1,[0,4,2,6,10,8]], res.columns[[0,4,2,6,10,8]]))
            print(*res, sep='\n', end='\n\n')
            print(*res, sep='\n', end='\n\n', file=f)
            gridsearches.append((res, tunable_params))

        gridsearches.sort(key=lambda p: p[0])
        print(*gridsearches, sep='\n')
    exit()


params = {'max_depth':6, 'eta':0.01, 'subsample':0.5, 'lambda':5, 'silent':1, 'objective':'binary:logistic'}
num_rounds=800
other_choice = 'test' if choice == 'train' else 'train'
results = {}
bst = xgb.train(params, dtrain, num_boost_round=num_rounds, 
        evals=[(dtrain, choice), (load_mat(other_choice), other_choice)], feval=PRF1, evals_result=results)
del dtrain
output_dir = datetime.datetime.now().strftime('data/output/%Y-%m-%d-%H-%M')
os.makedirs(output_dir)
bst.save_model('{}/xgb.model'.format(output_dir))

plt.figure(figsize=(19.2, 10.8))
plt.plot(range(num_rounds), results['train']['recall'], 'r-', label='train recall')
plt.plot(range(num_rounds), results['train']['precision'], 'b-', label='train precision')
plt.plot(range(num_rounds), results['test']['recall'], 'm-', label='test recall')
plt.plot(range(num_rounds), results['test']['precision'], 'c-', label='test precision')
plt.legend(loc='center left')
plt.savefig('{}/learning-curves.png'.format(output_dir))
plt.clf()

def test_on_set(which, f):
    #m, labels = load_set(which)
    #dtest = xgb.DMatrix(m, label=labels)

    dtest = load_mat(which)
    if dtest.num_row() == 0: 
        print('Empty', which, 'set', file=sys.stderr)
        return
    labels = dtest.get_label()

    predicted = bst.predict(dtest)
    predicted = (predicted > THRESHOLD).astype(int)

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
    print('Params:', params, end='\n\n', sep='\n', file=f)
    print('num_boost_round =', num_rounds, file=f)
    print('THRESHOLD =', THRESHOLD, file=f)
    print('\n', file=f)

    test_on_set('train', f)
    test_on_set('test', f)
