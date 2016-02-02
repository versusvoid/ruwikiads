#!/usr/bin/env python3

from scipy.sparse import csr_matrix
from scipy.io import savemat, mmwrite
import numpy as np
import re
import subprocess
import pickle
import os.path
import sys
from features import record_feature, extract_features_from_word_sequence, extract_features_from_word


features_indexes = None
if os.path.exists('data/output/features_indexes.pickle') and not '-u' in sys.argv:
    with open('data/output/features_indexes.pickle', 'rb') as f:
        features_indexes = pickle.load(f)
else:
    features_counts = {}
    i = 0

    sample_number = 0
    sample_features = {}
    word_sequence = []
    p = subprocess.Popen(['./mystem', '-gind', '../step4/data/output/train-set.txt'], stdout=subprocess.PIPE, universal_newlines=True)
#p = subprocess.Popen(['./mystem', '-gin', '../step4/data/output/train-set.txt'], stdout=subprocess.PIPE, universal_newlines=True)
    for l in p.stdout:
        i += 1
        l = l.strip()
        assert l.count('{') == 1, '{}: {}'.format(i, l)
        if l in ['plain{plain??}', 'ads{ads??}']:
            extract_features_from_word_sequence(sample_features, word_sequence)
            for feature in sample_features:
                record_feature(features_counts, feature)

            sample_number += 1
            if sample_number % 100 == 0:
                print('preprocess', sample_number)
            sample_features = {}
            word_sequence = []
        else:
            extract_features_from_word(sample_features, l, word_sequence)

    features_indexes = {}
    for feature, count in features_counts.items():
        if count > 25:
            features_indexes[feature] = len(features_indexes)


    with open('data/output/features_indexes.pickle', 'wb') as f:
        pickle.dump(features_indexes, f)

print(len(features_indexes), 'features')

for f in ['test-set.txt', 'train-set.txt']:
    features = []
    row_ind = []
    col_ind = []
    labels = []

    sample_number = 0
    sample_features = {}
    word_sequence = []

    def record_sample_features():
        for k, v in sample_features.items():
            id = features_indexes.get(k)
            if id is not None:
                features.append(v)
                row_ind.append(sample_number)
                col_ind.append(id)



    p = subprocess.Popen(['./mystem', '-gind', '../step4/data/output/{}'.format(f)], stdout=subprocess.PIPE, universal_newlines=True)
    #p = subprocess.Popen(['./mystem', '-gin', '../step4/data/output/{}'.format(f)], stdout=subprocess.PIPE, universal_newlines=True)
    for l in p.stdout:
        l = l.strip()
        if l in ['plain{plain??}', 'ads{ads??}']:
            extract_features_from_word_sequence(sample_features, word_sequence)
            record_sample_features()
            labels.append(1 if l.startswith('ads') else 0)

            sample_number += 1
            if sample_number % 1000 == 0:
                print(f, sample_number)
            sample_features = {}
            word_sequence = []
        else:
            extract_features_from_word(sample_features, l, word_sequence)

    m = csr_matrix((features, (row_ind, col_ind)))
    savemat('data/output/{}'.format(f.replace('.txt', '.mat')), {'m': m, 'labels': np.array(labels)})

