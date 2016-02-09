#!/usr/bin/env python3

from scipy.sparse import coo_matrix, vstack, hstack
from scipy.io import savemat
import numpy as np
import subprocess
import pickle
import os.path
import sys
import random
import bz2
from time import time

from features import record_feature, extract_features_from_word_sequence, extract_features_from_word

def count_samples(file_name):
    count = 0
    open_function = bz2.open if file_name.endswith('.bz2') else open
    separator = 'samplesSeparator' if open_function == open else b'openSeparator'
    start = time()
    with open_function(file_name, 'r') as f:
        for line in f:
            if line.startswith(separator):
                count += 1
    print('Counted', file_name, 'in', time() - start, 'seconds')

    return count

featured_samples_file = 'data/input/featured-samples.stemmed.txt.bz2'
wiki_ads_samples_file = 'data/input/wiki-ads-samples.stemmed.txt.bz2'
ads_samples_file = 'data/input/ads-samples.stemmed.txt.bz2'

featured_samples_count = count_samples(featured_samples_file)
wiki_ads_samples_count = count_samples(wiki_ads_samples_file)
ads_samples_count = count_samples(ads_samples_file)
exit()

assert featured_samples_count > ads_samples_count

featured_test_samples_count = int(0.4 * featured_samples_count)

featured_test_samples = set() # FIXME тоже надо подгружать с features_indexes
while len(featured_test_samples) < featured_test_samples_count:
    featured_test_samples.add(random.randrange(0, featured_samples_count))



features_indexes = None
if os.path.exists('data/output/features_indexes.pickle') and not '-u' in sys.argv:
    with open('data/output/features_indexes.pickle', 'rb') as f:
        features_indexes = pickle.load(f)
else:
    features_counts = {}

    def extract_features_from_file(filename, predicate=lambda _: True):
        sample_number = 0
        sample_features = {}
        word_sequence = []
        p = subprocess.Popen(['./mystem', '-gind', filename], stdout=subprocess.PIPE, universal_newlines=True)
        #bytes(re.subn(l, s)[0].replace(r'\_', '_'), 'utf-8').decode("unicode_escape")
        for l in p.stdout:
            l = l.strip()
            assert l.count('{') == 1, '{}: {}'.format(i, l)
            if l.startswith('samplesSeparator'):
                try:
                    extract_features_from_word_sequence(sample_features, word_sequence)
                except:
                    print(word_sequence, sample_number, i, file=sys.stderr)
                    raise

                if predicate(sample_number):
                    for feature in sample_features:
                        record_feature(features_counts, feature)

                sample_number += 1
                if sample_number % 100 == 0:
                    print('preprocess', filename, sample_number)
                sample_features = {}
                word_sequence = []
            else:
                extract_features_from_word(sample_features, l, word_sequence)

    extract_features_from_file(ads_samples_file)
    extract_features_from_file(featured_samples_file, 
            predicate=lambda sample_number: sample_number not in featured_test_samples)

    features_indexes = {}
    for feature, count in features_counts.items():
        #if count/sample_number > 0.1:
        #if count > 1000:
        features_indexes[feature] = len(features_indexes)


    with open('data/output/features_indexes.pickle', 'wb') as f:
        pickle.dump(features_indexes, f)


print(len(features_indexes), 'features')


def get_matrix_from_file(filename):
    features = []
    row_ind = []
    col_ind = []

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

    p = subprocess.Popen(['./mystem', '-gind', filename], stdout=subprocess.PIPE, universal_newlines=True)
    for l in p.stdout:
        l = l.strip()
        if l.startswith('samplesSeparator'):
            try:
                extract_features_from_word_sequence(sample_features, word_sequence)
            except:
                print(sample_number, i, file=sys.stderr)
                raise

            record_sample_features()

            sample_number += 1
            if sample_number % 1000 == 0:
                print(filename, sample_number)
            sample_features = {}
            word_sequence = []
        else:
            extract_features_from_word(sample_features, l, word_sequence)

    return coo_matrix((features, (row_ind, col_ind)), (sample_number, len(features_indexes)))

wiki_ads_matrix = get_matrix_from_file(wiki_ads_samples_file)
featured_matrix = get_matrix_from_file(featured_samples_file)
ads_matrix = get_matrix_from_file(ads_samples_file)

train_features, train_row, train_col = [], [], []
train_row_id = -1
test_features, test_row, test_col = [], [], []
test_row_id = -1
last_row_id = -1
for i in range(len(featured_matrix.row)):
    if featured_matrix.row[i] in featured_test_samples:
        if featured_matrix.row[i] != last_row_id:
            test_row_id += 1
            last_row_id = featured_matrix.row[i]
        test_features.append(featured_matrix.data[i])
        test_row.append(test_row_id)
        test_col.append(featured_matrix.col[i])
    else:
        if featured_matrix.row[i] != last_row_id:
            train_row_id += 1
            last_row_id = featured_matrix.row[i]
        train_features.append(featured_matrix.data[i])
        train_row.append(train_row_id)
        train_col.append(featured_matrix.col[i])

del featured_matrix

featured_train_samples_count = featured_samples_count - featured_test_samples_count
featured_test_matrix = coo_matrix((test_features, (test_row, test_col)), (featured_test_samples_count, len(features_indexes)))
featured_train_matrix = coo_matrix((train_features, (train_row, train_col)), (featured_train_samples_count, len(features_indexes)))

del train_features, train_row, train_col 
del test_features, test_row, test_col

train_matrix = vstack([ads_matrix, featured_train_matrix])
del ads_matrix, featured_train_matrix
train_labels = hstack([coo_matrix(np.ones(ads_samples_count)), coo_matrix(([], ([], [])), (1, featured_train_samples_count))])
assert train_labels.shape[1] == train_matrix.shape[0]

test_matrix = vstack([wiki_ads_matrix, featured_test_matrix])
del wiki_ads_matrix, featured_test_matrix
test_labels = hstack([coo_matrix(np.ones(wiki_ads_samples_count)), coo_matrix(([], ([], [])), (1, featured_test_samples_count))])
assert test_labels.shape[1] == test_matrix.shape[0]

savemat('data/output/train-set.mat', {'m': train_matrix, 'labels': train_labels}, format='4')
savemat('data/output/test-set.mat', {'m': test_matrix, 'labels': test_labels}, format='4')

