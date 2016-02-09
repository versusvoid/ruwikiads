#!/usr/bin/env python3

import sys
sys.path.insert(0, '../step1')
import WikiExtractor3 
sys.path.insert(0, '../step3')
from features import record_feature, extract_features_from_word_sequence, extract_features_from_word

from scipy.sparse import csr_matrix
import pickle
import os.path
import xgboost as xgb
import subprocess
import bz2
import regex
from time import time

import cProfile, pstats
class Profiler(object):

    def __init__(self, name):
        self.totalTime = 0
        self.start = 0
        self.name = name

    def enable(self):
        self.start = time()

    def disable(self):
        self.totalTime += time() - self.start

"""
mystem_prof = cProfile.Profile()
process_prof = cProfile.Profile()
feature_prof = cProfile.Profile()
xgb_prof = cProfile.Profile()
process_iteration_prof = cProfile.Profile()
split_prof = cProfile.Profile()
"""
process_prof = Profiler('process_page')
split_prof = Profiler('split article')
process_iteration_prof = Profiler('process_iteration')
mystem_prof = Profiler('mystem')
feature_prof = Profiler('features')
xgb_prof = Profiler('xgboost')

features_indexes = None
assert os.path.exists('../step3/data/output/features_indexes.pickle')
with open('../step3/data/output/features_indexes.pickle', 'rb') as f:
    features_indexes = pickle.load(f)

rows_indexes = []
features = []
row_ind = []
col_ind = []

bst = xgb.Booster(model_file='../step4/data/output/xgb.model')
def predict_section(of):
    global rows_indexes, features, row_ind, col_ind

    xgb_prof.enable()
    m = csr_matrix((features, (row_ind, col_ind)), shape=(len(rows_indexes), len(features_indexes)))
    m = xgb.DMatrix(m)
    predicted = bst.predict(m)
    assert len(predicted) == len(rows_indexes)
    for j, advertisment in enumerate(predicted > 0.5):
        if advertisment: 
            print(predicted[j], file=of)
            print(rows_indexes[j][0], '\n', sep='', file=of)
            print(rows_indexes[j][1], file=of)
            print('---------===============---------===============---------', file=of)

            print('Ads with P = ', predicted[j], '\n', rows_indexes[j][0], '\n', rows_indexes[j][1], sep='', end='\n---------------------------------------------\n')
            #input()

    rows_indexes = []
    features = []
    row_ind = []
    col_ind = []

    xgb_prof.disable()




def find_space(text, start, end):
    if end >= len(text):
        return len(text)
    i1 = text.rfind(' ', start, end)
    i2 = text.rfind('\xa0', start, end)
    i3 = text.rfind('\n', start, end)
    i4 = text.rfind('\t', start, end)
    last = max(i1, i2, i3, i4)
    assert last > -1, text
    return last   

p = subprocess.Popen(['../step3/mystem', '-gin'], stdout=subprocess.PIPE, stdin=subprocess.PIPE, universal_newlines=True)
def communicate_with_mystem(text):
    words = []
    start = 0
    end = find_space(text, 0, 40000)
    while start < len(text):
        print(text[start:end], file=p.stdin)
        print('\nCirrusSocratesParticleDecibelHurricaneDolphinTulip', file=p.stdin)
        p.stdin.flush()
        for l in p.stdout:
            if l.startswith('CirrusSocratesParticleDecibelHurricaneDolphinTulip'):
                start = end
                end = find_space(text, start, end + 40000)
                break
            else:
                words.append(l.strip())

    return words
            

def is_ads(article_title, section):
    
    mystem_prof.enable()
    words = communicate_with_mystem(section)
    mystem_prof.disable()

    sample_features = {}
    word_sequence = []
    for w in words:

        assert len(w) > 0
        feature_prof.enable()
        extract_features_from_word(sample_features, w, word_sequence)
        feature_prof.disable()


    feature_prof.enable()
    extract_features_from_word_sequence(sample_features, word_sequence)

    for k, v in sample_features.items():
        id = features_indexes.get(k)
        if id is not None:
            features.append(v)
            row_ind.append(len(rows_indexes))
            col_ind.append(id)
    rows_indexes.append((article_title, section))

    feature_prof.disable()


def process_page(of, title, wikitext):
    split_prof.enable()
    sections = WikiExtractor3.split_article(wikitext)
    split_prof.disable()

    for section_index, section in enumerate(sections):
        process_iteration_prof.enable()
        is_ads(title, section)
        process_iteration_prof.disable()
        

templates = list(map(lambda s: s.casefold(), ['{{реклама}}', '{{Избранная статья', '{{(Хорошая статья', '{{Добротная статья']))

s = time()
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
                            predict_section(adsf)

                            print(i)
                            #if i == 2000: break

                        pageText = ''.join(pageParts)
                        caselessText = pageText.casefold()
                        for template in templates:
                            if template in caselessText:
                                continue
                        
                        assert title is not None
                        process_prof.enable()
                        process_page(adsf, title, pageText)
                        process_prof.disable()
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
                    if j >= 0 and '</text>' not in l:
                        isText = True
                        pageParts.append(l[l.find('>')+1:])
            elif '<page>' in l:
                isPage = True



end = time()
print('Processed everything in ', end - s, 's')

for prof in [process_prof, split_prof, process_iteration_prof, mystem_prof, feature_prof, xgb_prof]:
    """
    sortby = 'cumulative'
    ps = pstats.Stats(prof).sort_stats(sortby)
    ps.print_stats()
    """
    print(prof.name, ': ', prof.totalTime, sep='')
    print('----------------------------------------------------------------------')
