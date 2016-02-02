#!/usr/bin/env python

import numpy as np
import xgboost as xgb
from scipy.io import loadmat
import os.path
import sys

bst = None
if os.path.exists('data/output/xgb.model') and not '-u' in sys.argv:
    bst = xgb.Booster(model_file='data/output/xgb.model')
else:
    d = loadmat('../step5/data/output/test-set.mat')
    permutation = np.random.permutation(d['m'].shape[0])
    dtrain = xgb.DMatrix(d['m'][permutation, :], label=d['labels'][0,permutation])
    num_rounds=75
    params = {'max_depth':4, 'eta':1, 'silent':0, 'objective':'binary:logistic' }
    if '-cv' in sys.argv:
        params['silent'] = 1
        xgb.cv(params, dtrain, num_boost_round=num_rounds, nfold=10, show_progress=True)
        exit()
    else:
        bst = xgb.train(params, dtrain, num_boost_round=num_rounds, evals=[(dtrain,'train')])
        bst.save_model('data/output/xgb.model')

def test(set_file):
    d = loadmat(set_file)
    print(d['m'].shape)
    labels = d['labels'][0,:]
    dtest = xgb.DMatrix(d['m'], label=labels)
    predicted = bst.predict(dtest)
    predicted = (predicted > 0.5).astype(int)

    tp = sum(np.logical_and(predicted == 1, labels == 1).astype(int))
    fp = sum(np.logical_and(predicted == 1, labels == 0).astype(int))
    fn = sum(np.logical_and(predicted == 0, labels == 1).astype(int))
    precision = tp / (tp + fp)
    recall = tp / (tp + fn)
    f1 = 2 * precision * recall / (precision + recall)
    print('On', set_file)
    print('precision:', precision)
    print('recall:', recall)
    print('f1:', f1)
    print()


test('../step5/data/output/train-set.mat')
test('../step5/data/output/test-set.mat')
