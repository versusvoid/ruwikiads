#!/usr/bin/env python3

import time
import requests
import itertools
import traceback
import random
import html
import subprocess
import os
from pathlib import Path

if not Path('RuLanAdCor/corpus.xml.bz2').exists():
    print("Generating corpus")
    os.system("cd RuLanAdCor; ./generate-corpus.sh")


mystem = subprocess.Popen(['../mystem', '-ind'], 
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True)

bzip2 = subprocess.Popen(['bzip2 -9 > data/output/ads.stemmed.txt.bz2'], 
        shell=True, stdin=subprocess.PIPE, universal_newlines=True)

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

def communicate_with_mystem(text):
    start = 0
    end = find_space(text, 0, 40000)
    while start < len(text):
        print(text[start:end], file=mystem.stdin)
        print('partSeparator', file=mystem.stdin)
        mystem.stdin.flush()
        for l in mystem.stdout:
            if l.startswith('partSeparator'):
                start = end
                end = find_space(text, start, end + 40000)
                break
            else:
                print(l, end='', file=bzip2.stdin)

of = open('data/output/ads.index.txt', 'w')

corpus = subprocess.Popen(['bunzip2 -k -c RuLanAdCor/corpus.xml.bz2'],
        shell=True, stdout=subprocess.PIPE, universal_newlines=True)
corpus.stdout.readline()
page = False
for l in corpus.stdout:
    if l.startswith('    </page>'): 
        print('samplesSeparator', file=of)
        print('samplesSeparator', file=bzip2.stdin)
        page = False
    elif l.startswith('    <page source="'):
        url = l[l.find('"') + 1:l.rfind('"')]
        print(url, file=of)
        page = True
    elif page:
        communicate_with_mystem(l)

corpus.stdout.close()

of.close()
mystem.stdin.close()
bzip2.stdin.close()
try:
    print('Waiting mystem')
    mystem.wait()
    print('Waiting bzip2')
    bzip2.wait()
except:
    print("Can't wait")
