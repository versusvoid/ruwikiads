#!/usr/bin/env python2
# -*- coding: utf-8 -*-
from __future__ import print_function

import WikiExtractor
import xml.etree.ElementTree as ET
import re

extractor = WikiExtractor.Extractor(0, '', [])

def clear_wikitext(wikitext):
    text = WikiExtractor.clean(extractor, wikitext)
    text = WikiExtractor.compact(text)
    return '\n'.join(text) 

def extract_wiki_section(wikitext):
    i = wikitext.find('==')
    if i != -1: return wikitext[:i]

    return wikitext

def extract_ads(wikitext):
    m = re.search(u'\{\{реклама\}\}', wikitext)
    if not m:
        m = re.search(u'\{\{Реклама\}\}', wikitext)
    if not m:
        print(wikitext)
        input()
    before = wikitext[:m.start()]
    after = wikitext[m.end():]
    before = clear_wikitext(before)
    if len(before.strip()) == 0:
        return clear_wikitext(after)
    
    return clear_wikitext(extract_wiki_section(after))

with open('filtered-ads.txt', 'wb') as f:
    context = ET.iterparse('ads.xml', ['start', 'end'])
    context = iter(context)
    event, root = next(context)
    for event, elem in context:
        if event == 'start' or elem.tag != 'page': continue

        f.write(elem.find('title').text.encode('utf-8'))
        f.write(b'\n')
        f.write(extract_ads(elem.find('revision').find('text').text).encode('utf-8'))
        f.write(b'\n-----------------------==========================-----------------------==========================-----------------------\n')

        elem.clear()
        root.clear()

    root.clear()
    del context

