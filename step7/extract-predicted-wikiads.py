#!/usr/bin/env python3

import sys
sys.path.insert(0, '../step2')
import WikiExtractor3 
sys.path.insert(0, '../step5')
from features import record_feature, extract_features_from_word_sequence, extract_features_from_word

from scipy.sparse import csr_matrix
import pickle
import os.path
import xgboost as xgb
import subprocess
import bz2
import re


features_indexes = None
assert os.path.exists('../step5/data/output/features_indexes.pickle')
with open('../step5/data/output/features_indexes.pickle', 'rb') as f:
    features_indexes = pickle.load(f)

bst = xgb.Booster(model_file='../step6/data/output/xgb.model')

def is_ads(section):
    
    annotated = None
    try:
        p = subprocess.run(['../step5/mystem', '-gind'], input=section, timeout=15, stdout=subprocess.PIPE, universal_newlines=True)
        annotated = p.stdout
    except subprocess.TimeoutExpired:
        print('mystem TimeoutExpired on', section, sep='\n', file=sys.stderr)
        return False

    sample_features = {}
    word_sequence = []
    for l in annotated.split('\n'):
        if len(l) > 0:
            extract_features_from_word(sample_features, l, word_sequence)
    extract_features_from_word_sequence(sample_features, word_sequence)

    features = []
    row_ind = []
    col_ind = []
    for k, v in sample_features.items():
        id = features_indexes.get(k)
        if id is not None:
            features.append(v)
            row_ind.append(0)
            col_ind.append(id)

    vector = csr_matrix((features, (row_ind, col_ind)), shape=(1, len(features_indexes)))
    predicted = bst.predict(xgb.DMatrix(vector))
    return predicted[0] > 0.75


def process_page(of, title, wikitext):
    for section in WikiExtractor3.split_article(wikitext):
        if is_ads(section):
            print(title, '\n', sep='', file=of)
            print(section, file=of)
            print('---------===============---------===============---------', file=of)

            #print('Ads:', section, sep='\n'); input()

    of.flush()

with open('data/output/wikiads.txt', 'w') as adsf:

    filename = sys.argv[1] if len(sys.argv) > 1 else '../step1/data/input/ruwiki-20151226-pages-articles-multistream.xml.bz2'
    with bz2.open(filename, 'r') as f:
        isText = False
        isPage = False
        title = None
        pageParts = []
        i = 0
        for l in f:
            l = l.decode('utf-8')
            if isPage:
                if isText:
                    j = l.find('</text>')
                    if j >= 0:
                        pageParts.append(l[:j])
                        isText = False
                        isPage = False 

                        i += 1
                        if i % 1000 == 0:
                            print(i)

                        pageText = ''.join(pageParts)
                        if (re.search('\{\{реклама\}\}', pageText, flags=re.IGNORECASE) or 
                                re.search('\{\{(Избранная|Хорошая|Добротная) статья', pageText, flags=re.IGNORECASE)):
                            continue
                        
                        assert title is not None
                        process_page(adsf, title, pageText)
                        title = None
                        pageParts = []
                    else:
                        pageParts.append(l)

                elif '<title>' in l:
                    title = l[l.find('>') + 1:l.rfind('<')]
                elif '<ns>' in l and l != '    <ns>0</ns>\n':
                    isPage = False
                else:
                    j = l.find('<text')
                    if j >= 0:
                        isText = True
                        pageParts.append(l[j+6:])
            elif '<page>' in l:
                isPage = True


