#!/usr/bin/env python3

import bz2
import regex
import random
import sys
import WikiExtractor3
import threading
import queue
import subprocess


ads_regex = regex.compile('\{\{реклама\}\}', flags=regex.IGNORECASE)

def get_wikitext(page_element):
    start = page_element.index('>', page_element.index('<text')) + 1
    end = page_element.rindex('</text>')
    return page_element[start:end]

# --------- ads ---------
def extract_wiki_section(before, after):
    i = before.rfind('==')
    if i != -1: 
        before = before[i + 2:]

    i = after.find('==')
    if i != -1: 
        after = after[:i]

    return '\n'.join([before, after])

def extract_ads(wikitext):
    m = ads_regex.search(wikitext)
    if not m:
        print(wikitext)
        input()
    before = wikitext[:m.start()]
    after = wikitext[m.end():]

    cleared = WikiExtractor3.clear_wikitext(before).strip()
    if len(cleared) == 0:
        return WikiExtractor3.clear_wikitext(after)

    return WikiExtractor3.clear_wikitext(extract_wiki_section(before, after))


def output_featured_sample(section, pageTitle, featuredf):
    featuredf.write(pageTitle.encode())
    featuredf.write(b'\n\n')
    featuredf.write(section.encode())
    featuredf.write(b'\n')
    featuredf.write(b'\nsamplesSeparator\n')

# -----------------------------------------------------------------------------------------------------------------------

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

def communicate_with_mystem(mystem, text, output_file):
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
                print(l, end='', file=output_file)

def writer_thread(task_queue):
    p = subprocess.Popen(["../mystem",  "-ind"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True)
    while True:
        task = task_queue.get()
        if task is None: 
            p.stdin.close()
            p.stdout.close()
            try:
                p.wait(30)
            except:
                print("Can't wait till mystem termination", file=sys.stderr)
            return
        title, sample, files = task

        communicate_with_mystem(p, sample, files[0].stdin)
        print('samplesSeparator', file=files[0].stdin)

        print(title, file=files[1])
        print('samplesSeparator', file=files[1])

writing_queue = queue.Queue(100000)
writer = threading.Thread(target=writer_thread, args=(writing_queue,))
writer.start()


featured_files = []
for i in range(8):
    p = subprocess.Popen(["bzip2 -9 > data/output/featured-samples.{}.stemmed.txt.bz2".format(i)],
            shell=True, stdin=subprocess.PIPE, universal_newlines=True)
    f = open("data/output/featured-samples.{}.index.txt".format(i), 'w')
    featured_files.append((p, f))
next_file = 0

ads_files = (subprocess.Popen(["bzip2 -9 > data/output/ads-samples.stemmed.txt.bz2"], 
                shell=True, stdin=subprocess.PIPE, universal_newlines=True),
             open("data/output/ads-samples.index.txt", 'w'))


isPage = False
pageParts = []
pageTitle = None
with bz2.open('data/input/ruwiki-20151226-pages-articles-multistream.xml.bz2', 'r') as f:
    i = 0
    for l in f:
        l = l.decode('utf-8')
        if isPage:
            #if ( (l.find('<ns>') != -1 and l != '    <ns>0</ns>\n') or featured_regex.search(l) ):
            if (l.find('<ns>') != -1 and l != '    <ns>0</ns>\n'):
                isPage = False
                pageParts = []
                pageTitle = None
                continue
            if l.find('<title>') != -1:
                pageTitle = l[l.find('>') + 1:l.rfind('<')]

            pageParts.append(l)
            if l == "  </page>\n":
                assert pageTitle is not None

                i += 1
                if i % 10000 == 0:
                    print(i)

                pageElement = ''.join(pageParts)
                if ads_regex.search(pageElement):
                    writing_queue.put((pageTitle, extract_ads(get_wikitext(pageElement)), ads_files))

                else:

                    wikitext = get_wikitext(pageElement)
                    if len(wikitext) > 2500:
                        for section in WikiExtractor3.split_article(wikitext):
                            writing_queue.put((pageTitle, section, featured_files[next_file]))
                            next_file = (next_file + 1) % len(featured_files)
                    else:
                        text = WikiExtractor3.clear_wikitext(wikitext)
                        if len(text) > 200:
                            writing_queue.put((pageTitle, text, featured_files[next_file]))
                            next_file = (next_file + 1) % len(featured_files)
                    
                
                isPage = False
                pageParts = []
                pageTitle = None
        elif l == "  <page>\n":
            pageParts.append(l)
            isPage = True

print('Done parsing')
writing_queue.put(None)
print('Waiting writer')
writer.join()
print('Writer finished')

ads_files[0].stdin.close()
try:
    print('Waiting ads subprocess')
    ads_files[0].wait(30)
except:
    print("Can't wait ads subprocess")
try:
    ads_files[1].close()
except:
    print("Can't close ads files")



for i in range(len(featured_files)):
    featured_files[i][0].stdin.close()
    try:
        print('Waiting featured subprocess #', i, sep='')
        featured_files[i][0].wait(30)
    except:
        print("Can't wait featured subprocess")
    try:
        featured_files[i][1].close()
    except:
        print("Can't close featured files")

