#!/usr/bin/env python3

from content_extraction import *
from html_filtering import *
from logs import *
import http.client
import re
import sys
import time
import urllib.parse



conn = http.client.HTTPSConnection("www.google.ru")
NUM = 100

def ask_google(start):
    for i in range(3):
        try:
            conn.request("GET", '/search?{}'.format(urllib.parse.urlencode({'q': '"о компании"', 'start': start, 'num': NUM, 'fg': 1})), 
            #conn.request("GET", '/search?{}'.format(urllib.parse.urlencode({'q': 'http://basel.aero/krasnodar/about/', 'start': start, 'num': NUM})), 
                            headers={'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/47.0.2526.106 Safari/537.36'})
            r = conn.getresponse()
            if r.status == 200: return r
            time.sleep(5)
        except:
            pass

    return None

def get_url_and_annotation(data):
    s0 = '<h3 class="r"><a href="'
    s1 = '<span class="st">'

    i = data.find(s0)
    if i < 0: 
        return None, None, data

    j = data.find('"', i + len(s0))
    assert j >= 0
    str_url = data[i + len(s0):j]
    j = data.find(s1, j)
    assert j >= 0
    j2 = data.find('</span>', j)
    assert j2 >= 0
    annotation = data[j + len(s1):j2]
    return str_url, annotation, data[j2 + 7:]


def output_extracted_content(f, extracted_content, url):
    wait_log('extracted:', extracted_content, sep='\n'); 
    if not re.search(sentence_regexp, extracted_content, re.MULTILINE):
        print(url.geturl(), '\n', extracted_content, '\n===========================\n')
        #input()
    print(url.geturl(), '\n', file=f)
    print(extracted_content, file=f)
    print('---------===============---------===============---------', file=f)

with open('ads.txt', 'w') as f:
    for start in range(0, NUM*100, NUM):
        print(start, file=sys.stderr)

        r = ask_google(start) 
        if r == None:
            print('no response from google')
            exit()
        if r.status != 200:
            print('response from google:', r.status, r.reason)
            exit()

        data = r.read().decode('utf-8')
        str_url, annotation, data = get_url_and_annotation(data)
        while str_url != None:

            annotation = re.sub('<[^>]+>', ' ', annotation)
            annotation = re.sub('&[a-z]+;', ' ', annotation)
            annotation = re.sub('\s+', ' ', annotation)
            log(str_url, annotation, sep='\n', end='\n\n')

            extracted_content = extract_from_about_page(str_ulr)

            if extracted_content != None:
                output_extracted_content(f, extracted_content, str_url)
            else:
                wait_log('extracted none')

            log('\n----------------------------\n')

            str_url, annotation, data = get_url_and_annotation(data)
