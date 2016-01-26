#!/usr/bin/env python3

from content_extraction import *
from logs import *
import threading
import traceback
import sys
import re
import html.parser
import urllib.parse
from selenium import webdriver
from time import time


def process_domain(f, driver, str_url):
    log('\n-------------------------------\n')
    log(str_url, '\n')

    #start = time()
    get_company_page(driver, str_url)
    #end = time()
    #print('Getting took:', end - start)

    url = urllib.parse.urlparse(driver.current_url)
    new_url = url

    #print('start')
    #start = process_time()
    #start1 = time()
    links = get_all_links(driver, stop_on_ru=True)
    #end = process_time()
    #end1 = time()
    #print('end')
    #wait_log('Took:', end - start, 'or', end1 - start1)
    if type(links) == dict:
        # TODO неплохо бы проверку на домен
        new_url = compute_child_url(url, links['href'])
        assert new_url is not None
        log('Switching from', url.geturl(), 'to russian url:', new_url.geturl())
        get_company_page(driver, new_url.geturl())
        url = urllib.parse.urlparse(driver.current_url)
        links = get_all_links(driver)

    #wait_log(*links)

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
            except PageLoadException:
                traceback.print_exception(*sys.exc_info())
                log("Can't load", entry['href'])

        if extracted_content != None: 
            break

        if entry['href'].startswith('javascript:'):
            get_company_page(driver, url.geturl())
        #raise Exception('tmp')
        wait_log('damn')

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



def run(id, urls):

    '''
    chrome_options = webdriver.ChromeOptions()
    prefs = {"profile.managed_default_content_settings.images": 2}
    chrome_options.add_experimental_option("prefs", prefs)
    #driver = webdriver.Chrome(chrome_options=chrome_options)
    '''
    driver = webdriver.PhantomJS(service_args=['--load-images=false', '--disk-cache=true'])
    #driver.set_page_load_timeout(3)

    try:
        with open('data/output/yaca-ads-{}.txt'.format(id), 'w') as f:
            i = 0
            for str_url in urls:
                assert len(driver.window_handles) < 2
                i += 1
                if i % 100 == 0:
                    print(i, file=sys.stderr)

                start = time()
                try:
                    process_domain(f, driver, str_url)
                except PageLoadException as e:
                    print("Can't get", e.args[0], file=sys.stderr)
                end = time()
                #print('One domain took', end - start)
                #input()
    except (Exception, KeyboardInterrupt) as e:
        exc_info = sys.exc_info()
        for entry in driver.get_log('browser'):
            print(entry['message'][:-4], file=sys.stderr)
        try:
            driver.quit()
        except: pass
        traceback.print_exception(*exc_info)
        #print(type(e), e, file=sys.stderr)


NUM_THREADS=8
#urls = []
urls = set()
#with open('data/input/YaCa_02.2014_business.csv', 'r') as f:
with open('data/input/test-urls.csv', 'r') as f:
    for line in f:
        url = urllib.parse.urlparse(line.strip().split(',', 1)[0])
        assert url.scheme.startswith('http') 
        urls.add('{}://{}/'.format(url.scheme, url.netloc))
        #urls.append('{}://{}/'.format(url.scheme, url.netloc))
if type(urls) != list:
    urls = list(urls)

urls_per_thread = len(urls) // NUM_THREADS
abundance = len(urls) % NUM_THREADS

threads = []
for i in range(NUM_THREADS):
    thread_urls = urls[urls_per_thread * i + min(i, abundance) : urls_per_thread * (i + 1) + min(i + 1, abundance)]
    threads.append(threading.Thread(target=run, args=(i, thread_urls)))
    threads[-1].start()

for thread in threads:
    thread.join()

with open('data/output/yaca-ads.txt', 'w') as of:
    for i in range(NUM_THREADS):
        with open('data/output/yaca-ads-{}.txt'.format(i), 'r') as f:
            for line in f:
                print(line, file=of, end='')
