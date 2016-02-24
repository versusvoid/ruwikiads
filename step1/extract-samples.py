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
lower_bound_section_regexes = [regex.compile(r'(^|\s)={' + str(i) + r',}(\s|$)') if i > 0 else None for i in range(7)]
upper_bound_section_regexes = [regex.compile(r'(^|\s)={1,' + str(i) + r'}(\s|$)') if i > 0 else None for i in range(7)]

def extract_wiki_section(before, after):
    r_before = ''.join(reversed(before))
    m = lower_bound_section_regexes[2].search(r_before)
    sep_len = 2
    if m is not None: 
        sep_len = len(m.group(0).strip())
        before = before[len(before) - m.start():]

    m = upper_bound_section_regexes[sep_len].search(after)
    if m is not None:
        after = after[:m.start()]

    return '\n'.join([before, after])

def extract_ads(wikitext):
    m = ads_regex.search(wikitext)
    if not m:
        print(wikitext)
        input()
    before = wikitext[:m.start()]
    after = wikitext[m.end():]

    cleared = WikiExtractor3.clear_wikitext(before)
    if len(cleared) == 0:
        return WikiExtractor3.clear_wikitext(after)

    return WikiExtractor3.clear_wikitext(extract_wiki_section(before, after))


def output_non_ads_sample(section, pageTitle, non_adsf):
    non_adsf.write(pageTitle.encode())
    non_adsf.write(b'\n\n')
    non_adsf.write(section.encode())
    non_adsf.write(b'\n')
    non_adsf.write(b'\nsamplesSeparator\n')

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


non_ads_files = []
for i in range(8):
    p = subprocess.Popen(["bzip2 -9 > data/output/non-ads-samples.{}.stemmed.txt.bz2".format(i)],
            shell=True, stdin=subprocess.PIPE, universal_newlines=True)
    f = open("data/output/non-ads-samples.{}.index.txt".format(i), 'w')
    non_ads_files.append((p, f))
next_file = 0

ads_files = (subprocess.Popen(["bzip2 -9 > data/output/ads-samples.stemmed.txt.bz2"], 
                shell=True, stdin=subprocess.PIPE, universal_newlines=True),
             open("data/output/ads-samples.index.txt", 'w'))
ads_text_file = open("data/output/ads-samples.txt", 'w')


isPage = False
pageParts = []
pageTitle = None
with bz2.open('data/input/ruwiki-pages-articles-multistream.xml.bz2', 'r') as f:
    i = 0
    for l in f:
        l = l.decode('utf-8')
        if isPage:
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
                    text = extract_ads(get_wikitext(pageElement))
                    if len(text) < 100:
                        print('Too small ads text in ', pageTitle, ':', sep='', file=sys.stderr)
                        print(text, file=sys.stderr)
                        #input()
                    else:
                        writing_queue.put((pageTitle, text, ads_files))

                        assert pageTitle.find('\n') == -1

                        print(pageTitle, file=ads_text_file)
                        print(text, file=ads_text_file)
                        print('samplesSeparator', file=ads_text_file)

                else:

                    wikitext = get_wikitext(pageElement)
                    if len(wikitext) > 2500:
                        for section in WikiExtractor3.split_article(wikitext):
                            writing_queue.put((pageTitle, section, non_ads_files[next_file]))
                            next_file = (next_file + 1) % len(non_ads_files)
                    else:
                        text = WikiExtractor3.clear_wikitext(wikitext)
                        if len(text) > 200:
                            writing_queue.put((pageTitle, text, non_ads_files[next_file]))
                            next_file = (next_file + 1) % len(non_ads_files)
                    
                
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

ads_text_file.close()
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



for i in range(len(non_ads_files)):
    non_ads_files[i][0].stdin.close()
    try:
        print('Waiting non ads subprocess #', i, sep='')
        non_ads_files[i][0].wait(30)
    except:
        print("Can't wait non ads subprocess")
    try:
        non_ads_files[i][1].close()
    except:
        print("Can't close non ads files")

