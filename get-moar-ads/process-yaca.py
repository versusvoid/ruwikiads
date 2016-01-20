#!/usr/bin/env python3

from content_extraction import *
from logs import *
import sys
import re
import html.parser
import urllib.parse


class HTMLLinksExtractor(html.parser.HTMLParser):

    def __init__(self):
        html.parser.HTMLParser.__init__(self)
        self._href = None
        self._link_text = None
        self.links = []

    def handle_starttag(self, tag, attrs):
        if tag == 'a':
            assert self._link_text == None
            for k, v in attrs:
                if k.casefold() == 'href'.casefold():
                    self._href = v
                    break
            if self._href == None: return

            self._link_text = []

    def handle_endtag(self, tag):
        if tag == 'a' and self._link_text != None:
            text = ''.join(self._link_text)

            scheme = urllib.parse.urlparse(self._href).scheme
            if scheme == '' or scheme.casefold() == 'http'.casefold():
                self.links.append((self._href, text))

            self._href = None
            self._link_text = None

        
    def handle_data(self, data):
        if self._link_text != None:
            data = data.strip()
            if len(data) > 0:
                self._link_text.append(data)


def extract_about_url(response):
    content = decode_content(response)

    m = re.search('<a[^>]*href="([^"]*/about[^"]*)"', content)
    if m:
        return m.group(1)


def is_about_link(p):
    return (re.search('\\babout\\b', p[0]) != None or
            re.search('\\bcompany\\b', p[0]) != None or
            'о компании'.casefold() in p[1].casefold() or
            'о нас'.casefold() in p[1].casefold() or
            re.search('\\bОб?[  ]', p[1]) != None)

urls = []
#with open('YaCa_02.2014_business.csv', 'r') as f:
with open('test-urls.csv', 'r') as f:
    for line in f:
        url = urllib.parse.urlparse(line.strip().split(',', 1)[0])
        assert url.scheme.startswith('http') 
        #urls.add('{}://{}/'.format(url.scheme, url.netloc))
        urls.append('{}://{}/'.format(url.scheme, url.netloc))

rus = ['ru'.casefold(), 'rus'.casefold(), 'рус'.casefold(), 'russian'.casefold()]

with open('yaca-ads.txt', 'w') as f:
    i = 0
    for str_url in urls:
        log('\n-------------------------------\n')
        log(str_url, '\n')

        url = urllib.parse.urlparse(str_url)
        r = get_company_page(str_url)
        if r is None: 
            log("Can't get site index page")
            continue

        content = decode_content(r)
        parser = HTMLLinksExtractor()
        parser.feed(content)

        for str_new_url, text in parser.links:
            if text.strip().casefold() in rus:
                new_url = compute_child_url(url, str_new_url)
                if new_url == url: break

                wait_log('Switching from', url.geturl(), 'to russian url:', new_url.geturl())
                r = get_company_page(url.geturl())
                if r == None: break
                content = decode_content(r)
                parser = HTMLLinksExtractor()
                parser.feed(content)
                break

        if r is None: continue

        links = list(filter(is_about_link, parser.links))
        about_links_num = len(links)
        for i, p in enumerate(links):
            score = 0
            if re.search('\\babout\\b', p[0]) != None:
                score += about_links_num - i
            elif re.search('\\bcompany\\b', p[0]) != None:
                score += (about_links_num - i)/2

            caseless_text = p[1].casefold()

            if 'о компании'.casefold() == caseless_text or 'о фирме'.casefold() == caseless_text:
                score += 1500
            if 'о компании'.casefold() in caseless_text or 'о фирме'.casefold() in caseless_text:
                score += 1000
            elif 'о нас'.casefold() == caseless_text:
                score += 750
            elif 'о нас'.casefold() in caseless_text:
                score += 500
            elif re.search('\\bОб?[  ]', p[1]) != None:
                score += 250

            links[i] = (score, p[0], p[1])

        for score, href, text in sorted(links, reverse=True):
            log('Trying link "{}" -> {} with score {}'.format(text, href, score))

            new_url = compute_child_url(url, href)
            extracted_content = None
            if new_url == url: 
                log('The same page')
                extracted_content = extract_from_about_page_response(r)
            else:
                extracted_content = extract_from_about_page(new_url.geturl())
           
            if extracted_content != None: 
                output_extracted_content(f, extracted_content, new_url)
                break

            wait_log()

        
        i += 1
        if i % 100 == 0:
            print(i, file=sys.stderr)

