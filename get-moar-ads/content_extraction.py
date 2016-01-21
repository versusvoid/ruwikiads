
from logs import *
from html_filtering import *
import re
import sys
import codecs
import requests
import requests.exceptions
import urllib.parse

def wrong_encoding(encoding):
    try:
        codecs.lookup(encoding)
        return False
    except:
        return True

def decode_content(response):
    content_type_key = None
    for k in response.headers.keys():
        if k.casefold() == 'Content-Type'.casefold():
            content_type_key = k
            break
    if content_type_key == None or 'charset=' not in response.headers[content_type_key] or wrong_encoding(response.encoding):
        
        #m = re.search('charset=([a-zA-Z0-9-]+)', response.headers[content_type_key])
        #assert m != None
        #charset = m.group(1)
        #text = response.content.decode(charset)


#   if text == None:
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
                return response.content.decode(charset)
            except:
                print("Can't decode with charset", charset, "at", response.url, file=sys.stderr)
                
    return response.text


def extract_content(response):
    if response != None and response.status_code == 200:
        text = decode_content(response)

        content = extract_main_content(text)

        if len(content) >= 200:
            return content

    return None


def get_company_page(url):
    encoding_problem = False
    for i in range(3):
        try:
            if encoding_problem:
                return requests.get(url, verify=False, headers={'Accept-Encoding': 'None'}, timeout=3)
            else:
                return requests.get(url, verify=False, timeout=3)
        except requests.exceptions.ContentDecodingError:
            encoding_problem = True
        except:
            pass
    return None

def same_top_domain(domain1, domain2):
    if '.' not in domain1 or '.' not in domain2:
        return domain1 == domain2
    domain1 = domain1[:domain1.rfind('.')]
    domain2 = domain2[:domain2.rfind('.')]

    return domain1[domain1.rfind('.') + 1:].casefold() == domain2[domain2.rfind('.') + 1:].casefold()

def compute_child_url(url, child_str_url):
    child_url = urllib.parse.urlparse(child_str_url)
    if child_url.scheme != '' and child_url.scheme.casefold() != url.scheme.casefold():
        return None

    new_domain = url.netloc
    if child_url.netloc != '':
        if not same_top_domain(child_url.netloc, url.netloc):
            return None
        new_domain = child_url.netloc
    
    new_path = ''
    if child_url.path.startswith('/'):
        new_path = child_url.path
    else:
        old_path = url.path
        if len(old_path) == 0:
            old_path = '/'
        assert '/' in old_path
            
        new_path = url.path[url.path.rfind('/') + 1:] + child_url.path

    if len(new_path) == 0:
        new_path = '/'


    return urllib.parse.ParseResult(url.scheme, new_domain, new_path, child_url.params, child_url.query, '')


def extract_from_child_page(url, content):
    for regexp in map('href="({}[^".?#]+)"'.format, [url.path, url.geturl().replace('.', '\.')]):
        log('searching for:', regexp)
        m = re.search(regexp, content)
        if m:
            url = compute_child_url(url, m.group(1))
            log('No text at base url, trying: "{}"'.format(url.geturl()))
            return extract_content(get_company_page(url.geturl()))

    return None

def extract_from_about_page_response(response):
    extracted_content = extract_content(response)

    if response.status_code == 200 and extracted_content == None:
        url = urllib.parse.urlparse(response.url)
        extracted_content = extract_from_child_page(url, response.text)

    return extracted_content


def extract_from_about_page(str_url):
    r = get_company_page(str_url)
    if r == None:
        print("Can't get page:", str_url, file=sys.stderr)
        return None


    return extract_from_about_page_response(r)


def output_extracted_content(f, extracted_content, url):
    wait_log('extracted:', extracted_content, sep='\n'); 
    if not re.search(sentence_regexp, extracted_content, re.MULTILINE):
        #wait_log(url.geturl(), '\n', extracted_content, '\n===========================\n')
        wait_log('no sentences')
        #input()
    print(url.geturl(), '\n', file=f)
    print(extracted_content, file=f)
    print('---------===============---------===============---------', file=f)


