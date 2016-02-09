#!/usr/bin/env python

import numpy as np
import xgboost as xgb
from scipy.io import loadmat
from scipy.sparse import csr_matrix
import os.path
import sys

def load_set(which='train'):
    d = loadmat('../step3/data/output/{}-set.mat'.format(which))
    m = d['m'].tocsr()
    print(m.shape)
    labels = np.array(d['labels'].tocsr().todense())[0,:]
    del d
    return m, labels

bst = None
if os.path.exists('data/output/xgb.model') and not '-u' in sys.argv:
    bst = xgb.Booster(model_file='data/output/xgb.model')
else:
    #m, labels = load_set()
    m, labels = load_set('test')
    permutation = np.random.permutation(m.shape[0])
    dtrain = xgb.DMatrix(m[permutation, :], label=labels[permutation])
    num_rounds=75
    #params = {'max_depth':4, 'eta':0.1, 'subsample':0.5, 'lambda':10, 'silent':1, 'objective':'binary:logistic' }
    params = {'max_depth':6, 'eta':0.3, 'subsample':1.0, 'lambda':1, 'silent':1, 'objective':'binary:logistic' }
    if '-cv' in sys.argv:
        xgb.cv(params, dtrain, num_boost_round=num_rounds, nfold=10, show_progress=True)
        exit()
    else:
        bst = xgb.train(params, dtrain, num_boost_round=num_rounds, evals=[(dtrain,'train')])
        bst.save_model('data/output/xgb.model')

def test_on_set(which='train'):
    m, labels = load_set(which)
    dtest = xgb.DMatrix(m, label=labels)
    predicted = bst.predict(dtest)
    predicted = (predicted > 0.5).astype(int)

    #print('labels:', labels.shape, len(labels))
    #print('predicted:', predicted.shape, len(predicted))
    tp = sum(np.logical_and(predicted == 1, labels == 1).astype(int))
    #print('tp:', tp)
    #print('sum(predicted[0:tp]):', sum(predicted[0:tp]))
    #print('sum(predicted[tp:]):', sum(predicted[tp:]))
    fp = sum(np.logical_and(predicted == 1, labels == 0).astype(int))
    #print('fp:', fp)
    fn = sum(np.logical_and(predicted == 0, labels == 1).astype(int))
    #print('fn:', fn)
    precision = tp / (tp + fp)
    recall = tp / (tp + fn)
    f1 = 2 * precision * recall / (precision + recall)
    print('On', which)
    print('precision:', precision)
    print('recall:', recall)
    print('f1:', f1)
    print()


test_on_set('train')
test_on_set('test')
