#!/usr/bin/env python3

import urllib.parse
import http.client
import re
import requests
import html
import html.parser


class MyHTMLParser(html.parser.HTMLParser):

    longest_text = ''
    texts_stack = [[]]

    def handle_starttag(self, tag, attrs):
        self.texts_stack.append([])

    def handle_endtag(self, tag):
        content = ''.join(self.texts_stack[-1])
        if '<p>' in content and len(content) > len(self.longest_text):
            print('found one')
            self.longest_text = content
        
        self.texts_stack.pop()
        
    def handle_data(self, data):
        assert type(data) == str
        self.texts_stack[-1].append(data)

content = None
#with open('test.html', 'r', encoding='cp1251') as f:
with open('test.html', 'r') as f:
    content = ''.join(f.readlines())

def remove_all(content, start_token, end_token):

    i1 = content.find(start_token)
    while i1 >= 0:
        if i1 >= 0:
            i2 = content.find(end_token, i1)
            assert i2 >= 0, 'no end token: "{}"'.format(end_token)
            content = content[:i1] + content[i2 + len(end_token):]

        i1 = content.find(start_token)

    return content

content = remove_all(content, '<nav', '</nav>')
content = remove_all(content, '<!--', '-->')


content = re.sub('<nav.*</nav>', ' ', content)
content = re.sub('</?(b|a|ul|ol|li|i|br|big|header|h[1-6]|em|strong|blockquote|span)[^>]*>', ' ', content)
content = content.replace('</p>', '')
content = re.sub('<p[^>]*>', '&lt;p&gt;', content)

parser = MyHTMLParser()
parser.feed(content)

longest_text = re.sub('[ \t]+', ' ', parser.longest_text)
longest_text = re.sub('\s{2,}', '\n', longest_text)
longest_text = longest_text.replace('<p>', '')

print('extracted:', longest_text, sep='\n', end='\n----------------------------\n')
