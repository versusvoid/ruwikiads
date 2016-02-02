#!/usr/bin/env python3

import WikiExtractor3
import xml.etree.ElementTree as ET
import re


# --------- ads ---------
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
    before = WikiExtractor3.clear_wikitext(before)
    if len(before.strip()) == 0:
        return WikiExtractor3.clear_wikitext(after)
    
    return WikiExtractor3.clear_wikitext(extract_wiki_section(after))

with open('data/output/ads-samples.txt', 'wb') as f:
    context = ET.iterparse('../step1/data/output/ads.xml', ['end'])
    context = iter(context)
    event, root = next(context)
    for event, elem in context:
        if elem.tag != 'page': continue

        print(elem.find('title').text, '\n', sep='', file=f)
        print(extract_ads(elem.find('revision').find('text').text), file=f)
        print('---------===============---------===============---------', file=f)

        elem.clear()
        root.clear()

    root.clear()
    del context



# ------- featured -------

with open('data/output/featured-samples.txt', 'wb') as f:
    context = ET.iterparse('../step1/data/output/featured.xml', ['end'])
    context = iter(context)
    event, root = next(context)
    for event, elem in context:
        if elem.tag != 'text': continue

        for section in WikiExtractor3.split_article(elem.text):
            print(section, '\n', sep='', file=f)
            print('---------===============---------===============---------', file=f)

        elem.clear()
        root.clear()

    root.clear()
    del context


