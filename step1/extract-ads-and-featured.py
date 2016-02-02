#!/usr/bin/env python3

import bz2
import numpy as np
from numpy.linalg import norm
import pickle
import itertools
import bisect
import matplotlib.pyplot as plt
import re

import sys

filename = sys.argv[1] if len(sys.argv) > 1 else 'data/input/ruwiki-20151226-pages-articles-multistream.xml.bz2'

isPage = False
pageParts = []
with open('data/ouput/ads.xml', 'w') as adsf:
    print('<pages>', file=adsf)
    with open('data/ouput/featured.xml', 'w') as featuredf:
        print('<pages>', file=featuredf)

        with bz2.open(filename, 'r') as f:
            i = 0
            for l in f:
                l = l.decode('utf-8')
                if isPage:
                    if l.find('<ns>') != -1 and l != '    <ns>0</ns>\n':
                        isPage = False
                        pageParts = []

                    pageParts.append(l)
                    if l == "  </page>\n":
                        pageElement = ''.join(pageParts)
                        if re.search('\{\{реклама\}\}', pageElement, flags=re.IGNORECASE):
                            print(pageElement, file=adsf)
                        if re.search('\{\{(Избранная|Хорошая|Добротная) статья', pageElement, flags=re.IGNORECASE):
                            print(pageElement, file=featuredf)
                        
                        isPage = False
                        pageParts = []
                elif l == "  <page>\n":
                    pageParts.append(l)
                    isPage = True
                    i += 1

                    if i % 10000 == 0:
                        print(i)


        print('</pages>', file=featuredf)

    print('</pages>', file=adsf)
