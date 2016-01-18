#!/usr/bin/env python3

import codecs
import html
import html.parser
import http.client
import re
import requests
import sys
import time
import urllib.parse

sentence_regexp = '(\. |^[  \t]*|<p>[  \t]*)[А-ЯЁ][А-ЯЁа-яё,"«»  ֊־‐‑‒–—―﹣－-]+\.'

class MyHTMLParser(html.parser.HTMLParser):

    _texts_stack = []
    _paragraph_sentence_text = ''
    _sentence_text = ''
    _paragraph_text = ''

    def handle_starttag(self, tag, attrs):
        self._texts_stack.append([])

    def handle_endtag(self, tag):
        if len(self._texts_stack) == 0: return
        content = ''.join(self._texts_stack[-1])
        #print('trying:\n', content)

        paragraph = '<p>' in content
        m = re.search(sentence_regexp, content, re.MULTILINE)
        sentence = m != None and re.search('\s', m.group(0)) != None
        #sentence = max(map(lambda s: len(s.strip()), content.split('\n'))) >= 70 and re.search(sentence_regexp, content, re.MULTILINE) != None

        if paragraph and sentence and len(content) > len(self._paragraph_sentence_text):
            self._paragraph_sentence_text = self.copyleft(self._paragraph_sentence_text, content)

        if sentence and len(content) > len(self._sentence_text):
            self.sentence_text = self.copyleft(self._sentence_text, content)

        if paragraph and len(content) > len(self._paragraph_text):
            self._paragraph_text = self.copyleft(self._paragraph_text, content)
        
        self._texts_stack.pop()

    def copyleft(self, old_text, new_text):
        if '©' in new_text and len(old_text) != 0:
            return old_text
        else:
            return new_text
            
        
    def handle_data(self, data):
        if len(self._texts_stack) == 0: return
        assert type(data) == str
        self._texts_stack[-1].append(data)

    def get_longest_text(self):
        if len(self._paragraph_sentence_text) > 0:
            return self._paragraph_sentence_text
        elif len(self._sentence_text) > 0:
            return self._sentence_text
        else: 
            return self._paragraph_text

def remove_all(content, start_token, end_token):

    try:
        i1 = content.find(start_token)
        while i1 >= 0:
            i2 = content.find(end_token, i1)
            assert i2 >= 0
            content = content[:i1] + content[i2 + len(end_token):]

            i1 = content.find(start_token)

    except:
        print('no end token: "{}"'.format(end_token), file=sys.stderr)

    return content

def extract_main_content(content):
    content = remove_all(content, '<nav', '</nav>')
    content = remove_all(content, '<script', '</script>')
    content = remove_all(content, '<!--', '-->')


    content = re.sub('<nav.*</nav>', '', content)
    content = re.sub('<br[^>]*>', '\n', content)
    content = re.sub('</?(b|a|ul|ol|li|i|font|dt|dl|dd|big|header|h[1-6]|em|strong|blockquote|span)[^>]*>', '', content)
    content = content.replace('</p>', '\n')
    content = re.sub('<p( [^>]+)?>', '&lt;p&gt;', content)
    #content = re.sub('<p[^>]*>', '', content)

    parser = MyHTMLParser()
    parser.feed(content)

    longest_text = re.sub('[ \t]+', ' ', parser.get_longest_text())
    longest_text = re.sub('\s{2,}', '\n', longest_text)
    longest_text = longest_text.replace('<p>', '')

    return longest_text

def wrong_encoding(encoding):
    try:
        codecs.lookup(encoding)
        return False
    except:
        return True

def extract_content(response):
    if response != None and response.status_code == 200:
        text = None
        content_type_key = None
        for k in response.headers.keys():
            if k.casefold() == 'Content-Type'.casefold():
                content_type_key = k
                break
        if content_type_key == None or 'charset=' not in response.headers[content_type_key] or wrong_encoding(r.encoding):
            
            #m = re.search('charset=([a-zA-Z0-9-]+)', response.headers[content_type_key])
            #assert m != None
            #charset = m.group(1)
            #text = response.content.decode(charset)


#        if text == None:
            m1 = re.search('<meta[^>]*charset="([a-zA-Z0-9-]+)"', response.text, re.IGNORECASE)
            m2 = re.search('<meta[^>]*charset=([a-zA-Z0-9-]+)', response.text, re.IGNORECASE)
            charset = None
            if m1 != None and m2 != None:
                assert m1.group(1).casefold() == m2.group(1).casefold(), 'Different charsets "{}" and "{}" at {}"'.format(m1.group(1), m2.group(1), response.url)

            if m1 != None:
                charset = m1.group(1)
            elif m2 != None:
                charset = m2.group(1)

            if charset != None:
                try:
                    text = response.content.decode(charset)
                except:
                    print("Can't decode with charset", charset, "at", response.url, file=sys.stderr)

        if text == None:
            text = response.text

        content = extract_main_content(text)

        if len(content) >= 200:
            return content

    return None


def get_company_page(url):
    for i in range(3):
        try:
            return requests.get(url, verify=False)
        except:
            pass
    return None


conn = http.client.HTTPSConnection("www.google.ru")
NUM = 100

#logging = True
logging = False
def log(*args, **kwargs):
    if logging:
        print(*args, **kwargs)

def wait_log(*args, **kwargs):
    if logging:
        print(*args, **kwargs)
        input()

with open('ads.txt', 'w') as f:
    for start in range(0, NUM*100, NUM):
        print(start, file=sys.stderr)

        r = None
        for i in range(3):
            try:
                conn.request("GET", '/search?{}'.format(urllib.parse.urlencode({'q': '"о компании"', 'start': start, 'num': NUM, 'fg': 1})), 
                #conn.request("GET", '/search?{}'.format(urllib.parse.urlencode({'q': 'http://basel.aero/krasnodar/about/', 'start': start, 'num': NUM})), 
                                headers={'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/47.0.2526.106 Safari/537.36'})
                r = conn.getresponse()
                if r.status == 200: break
                time.sleep(5)
            except:
                pass

        if r == None:
            print('no response from google')
            exit()
        if r.status != 200:
            print('response from google:', r.status, r.reason)
            exit()

        data = r.read().decode('utf-8')
        s0 = '<h3 class="r"><a href="'
        s1 = '<span class="st">'
        i = data.find(s0)
        while i >= 0:
            j = data.find('"', i + len(s0))
            assert j >= 0
            str_url = data[i + len(s0):j]
            j = data.find(s1, j)
            assert j >= 0
            j2 = data.find('</span>', j)
            assert j2 >= 0
            annotation = data[j + len(s1):j2]
            i = data.find(s0, j2)

            annotation = re.sub('<[^>]+>', ' ', annotation)
            annotation = re.sub('&[a-z]+;', ' ', annotation)
            annotation = re.sub('\s+', ' ', annotation)

            log(str_url, annotation, sep='\n', end='\n\n')

            r = get_company_page(str_url)
            if r == None:
                print("Can't get page:", str_url, file=sys.stderr)
                continue

            url = urllib.parse.urlparse(r.url)
            extracted_content = extract_content(r)

            if r != None and r.status_code == 200 and extracted_content == None:
                for regexp in map('href="({}[^".?#]+)"'.format, [url.path, url.geturl().replace('.', '\.')]):
                    log('searching for:', regexp)
                    m = re.search(regexp, r.text)
                    if m:
                        next_url = urllib.parse.urlparse(m.group(1))
                        log('"{}"'.format(next_url.geturl()))
                        url = urllib.parse.ParseResult(url.scheme, url.netloc, next_url.path, url.params, url.query, url.fragment)
                        log('No text at base url, trying: "{}"'.format(url.geturl()))
                        extracted_content = extract_content(get_company_page(url.geturl()))
                        break
                    

            if extracted_content != None:
                wait_log('extracted:', extracted_content, sep='\n'); 
                if not re.search(sentence_regexp, extracted_content, re.MULTILINE):
                    print(url.geturl(), '\n', extracted_content, '\n===========================\n')
                    #input()
                print(url.geturl(), '\n', file=f)
                print(extracted_content, file=f)
                print('---------===============---------===============---------', file=f)
            else:
                wait_log('extracted none')

            log('\n----------------------------\n')

