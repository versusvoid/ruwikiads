#!/usr/bin/env python3

from content_extraction import *
from logs import *

import html.parser
import os
import re
import resource
import subprocess
import sys
import threading
import traceback
import urllib.parse

from selenium import webdriver
from selenium.common.exceptions import WebDriverException
from time import time, gmtime, strftime
from urllib.error import URLError
from http.client import RemoteDisconnected

# phantomjs eats memory and hangs system sometimes
# won't let it
try:
    resource.setrlimit(resource.RLIMIT_VMEM, (2**31 * 3, 2**31 * 3))
except:
    resource.setrlimit(resource.RLIMIT_AS, (2**31 * 3, 2**31 * 3))

def process_domain(f, driver, str_url):
    log('\n-------------------------------\n')
    log(str_url, '\n')

    get_company_page(driver, str_url)

    url = urllib.parse.urlparse(driver.driver.current_url)
    new_url = url

    links = get_all_links(driver, stop_on_ru=True)
    if type(links) == dict:
        new_url = compute_child_url(url, links['href'])
        assert new_url is not None, str_url
        log('Switching from', url.geturl(), 'to russian url:', new_url.geturl())
        get_company_page(driver, new_url.geturl())
        url = urllib.parse.urlparse(driver.driver.current_url)
        links = get_all_links(driver)

    extracted_content = None
    links = list(sorted(links, reverse=True, key=lambda d: d['score']))
    base_page_content = None
    base_page_processed = False
    for entry in links:
        new_url = compute_child_url(url, entry['href'])
        entry['url'] = new_url
        if new_url == url:
            log('Processing base page')
            base_page_content = extract_from_about_page(driver)
            base_page_processed = True

    for entry in links:
        log('Trying link "{text}" -> {href} with score {score}'.format(**entry))

        new_url = entry['url']
        if new_url == url:
            extracted_content = base_page_content
        elif new_url is not None:
            try:
                get_company_page(driver, new_url.geturl())
                extracted_content = extract_from_about_page(driver)
            except PageLoadException as e:
                print("Can't load", e.args[0], file=sys.stderr)

        if extracted_content != None: 
            break

        if entry['href'].startswith('javascript:'):
            get_company_page(driver, url.geturl())
        wait_log('Nothing')

    if extracted_content == None and not base_page_processed:
        new_url = url
        get_company_page(driver, url.geturl())
        log('trying last resort - main page')
        extracted_content = extract_from_about_page(driver)
        wait_log('content from main page:\n', extracted_content)
        if extracted_content != None and (len(extracted_content) < 500 or 
                re.search('\\bкомпани([яию])\\b', extracted_content, flags=re.IGNORECASE) is None):
            extracted_content = None

    if extracted_content != None: 
        output_extracted_content(f, extracted_content, new_url)
        return True
    else:
        return False


directory = strftime('data/output/yaca-ads-%d.%m.%Y-%H:%M:%S', gmtime())
os.mkdir(directory)

def run(id, urls):
    driver = renew_driver()

    failed_urls = []
    try:
        with open('{}/yaca-ads-{}.txt'.format(directory, id), 'w') as f:
            for i, str_url in enumerate(urls):
                if (i + 1) % 100 == 0:
                    print('Thread #', id, ': ', i, file=sys.stderr, sep='')
                    renew_driver(driver)

                success = False
                error = None
                try:
                    for _ in range(2):
                        try:
                            success = process_domain(f, driver, str_url)
                            break
                        except (URLError, RemoteDisconnected) as e:
                            print('Looks like phantom is down at', str_url, file=sys.stderr)
                            error = e
                            renew_driver(driver)
                except PageLoadException as e:
                    print("Can't get", e.args[0], file=sys.stderr)
                    error = e
                except AssertionError as e:
                    error = e
                    print('Something went very bad at', str_url + ':', file=sys.stderr)
                    traceback.print_exception(*sys.exc_info())

                if not success:
                    if error is None:
                        failed_urls.append(str_url)
                    elif len(error.args) > 0 and type(error.args[0]) == str:
                        failed_urls.append(','.join([str_url, type(error).__name__, error.args[0]]))
                    else:
                        failed_urls.append(','.join([str_url, type(error).__name__]))


                while len(driver.driver.window_handles) > 1:
                    print('Closing additional window from', str_url, file=sys.stderr)
                    driver.driver.switch_to.window(driver.driver.window_handles[1])
                    driver.driver.close()
                    driver.driver.switch_to.window(driver.driver.window_handles[0])

    except:
        exc_info = sys.exc_info()
        if len(exc_info[1].args) > 0 and type(exc_info[1].args[0]) == str:
            failed_urls.append(','.join([str_url, exc_info[0].__name__, exc_info[1].args[0]]))
        else:
            failed_urls.append(','.join([str_url, exc_info[0].__name__]))

        print('Unrecoverable error at', str_url, 'in thread #{}:'.format(id), exc_info[0], file=sys.stderr)
        try:
            for entry in driver.driver.get_log('browser'):
                print(entry['message'][:-4], file=sys.stderr)
        except: pass
        try:
            driver.driver.quit()
        except: pass
        traceback.print_exception(*exc_info)

    if len(failed_urls) > 0:
        with open('{}/yaca-failed-{}.txt'.format(directory, id), 'w') as f:
            for url in failed_urls:
                print(url, file=f)


if len(sys.argv) < 2:
    print('Please specify file with urls', file=sys.stderr)
    exit(1)
unique_urls = set()
urls = []
with open(sys.argv[1], 'r') as f:
    for line in f:
        line = line.strip().split(',', 1)[0]
        if not line.startswith('https://'):
            line = 'http://' + line
        url = urllib.parse.urlparse(line.strip().split(',', 1)[0])
        assert url.scheme.startswith('http') 
        str_url = '{}://{}/'.format(url.scheme, url.netloc)
        if str_url not in unique_urls:
            unique_urls.add(str_url)
            urls.append(str_url)
#urls = urls[:10]

NUM_THREADS=8
urls_per_thread = len(urls) // NUM_THREADS
abundance = len(urls) % NUM_THREADS


threads = []
for i in range(NUM_THREADS):
    # splitting in roughly equal parts
    thread_urls = urls[urls_per_thread * i + min(i, abundance) : urls_per_thread * (i + 1) + min(i + 1, abundance)]
    threads.append(threading.Thread(target=run, args=(i, thread_urls)))
    threads[-1].start()

for thread in threads:
    thread.join()



mystem = subprocess.Popen(['../mystem', '-ind'], 
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True)

bzip2 = subprocess.Popen(['bzip2 -9 > data/output/yaca-ads.stemmed.txt.bz2'], 
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

def communicate_with_mystem(text, output_file):
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


with open('data/output/yaca-ads.index.txt', 'w') as of:
    for i in range(NUM_THREADS):
        with open('{}/yaca-ads-{}.txt'.format(directory, i), 'r') as f:
            skip = 1
            for l in f:
                if skip > 0:
                    print(l, end='', file=of)
                    skip -= 1
                    continue
                
                if l.startswith('samplesSeparator'):
                    print(l, end='', file=of)
                    print(l, end='', file=bzip2.stdin)
                    skip = 1
                else:
                    communicate_with_mystem(l, bzip2.stdin)

mystem.stdin.close()
bzip2.stdin.close()
try:
    print('Waiting mystem')
    mystem.wait()
    print('Waiting bzip2')
    bzip2.wait()
except:
    print("Can't wait")
