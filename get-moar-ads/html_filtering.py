
import sys
import re
import html
import html.parser

sentence_regexp = '(\. |^[  \t]*|<p>[  \t]*)[А-ЯЁ][А-ЯЁа-яё,"«»  ֊־‐‑‒–—―﹣－-]+[.!?]'

# TODO join divs
class MyHTMLParser(html.parser.HTMLParser):

    def __init__(self):
        html.parser.HTMLParser.__init__(self)
        self._texts_stack = []
        self._paragraph_sentence_text = ''
        self._sentence_text = ''
        self._paragraph_text = ''

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
            self._sentence_text = self.copyleft(self._sentence_text, content)

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

