
import sys
import re
import html
import html.parser
import enum

all_upper_first_word_ru_regexp = re.compile('["(«]?[А-ЯЁ]{2,5}[")»,;]?')
all_upper_first_word_en_regexp = re.compile('["(«]?[A-Z]{2,5}[")»,;]?')
sentence_first_word_ru_regexp = re.compile('["(«]?[А-ЯЁ][а-яё]*([֊־‐‑‒–—―﹣－-][А-ЯЁ]?[а-яё]+)?[")»,;]?')
sentence_first_word_en_regexp = re.compile('["(«]?[A-Z][a-z]*([֊־‐‑‒–—―﹣－-][A-Z]?[a-z]+)?')
all_upper_middle_word_ru_regexp = re.compile('["(«]?[А-ЯЁ]{2,5}[")»,;:.!?]?')
all_upper_middle_word_en_regexp = re.compile('["(«]?[A-Z]{2,5}[")»,;:.!?]?')
middle_word_ru_regexp = re.compile('["(«]?([А-ЯЁ][а-яё]*|[а-яё]+)([֊־‐‑‒–—―﹣－-][А-ЯЁ]?[а-яё]+)?[")»,;:.!?]?')
middle_word_en_regexp = re.compile('["(«]?([A-Z][a-z]*|[a-z]+)([֊־‐‑‒–—―﹣－-][A-Z]?[a-z]+)?[")»,;:.!?]?')
standalone_punctuation_regexp = re.compile('[֊־‐‑‒–—―﹣－-]')
standalone_number_regexp = re.compile('[0-9]+([,.][0-9]+)?')

class SentenceDetectionState(enum.Enum):
    BeforeFirstWord = 1
    Unknown = 2
    MiddleSentence = 3

def count_sentences(text):
    tokens = re.split('[  \t]+', text)
    #print(tokens)
    detection_state = SentenceDetectionState.BeforeFirstWord
    sentence_length = 0
    num_sentences = 0
    for i, token in enumerate(tokens):
        if detection_state == SentenceDetectionState.BeforeFirstWord:
            if (sentence_first_word_ru_regexp.fullmatch(token) != None or
                    sentence_first_word_ru_regexp.fullmatch(token) != None or
                    all_upper_first_word_ru_regexp.fullmatch(token) != None or
                    all_upper_first_word_en_regexp.fullmatch(token) != None):

                detection_state = SentenceDetectionState.MiddleSentence
                sentence_length = 1
            else:
                detection_state = SentenceDetectionState.Unknown

        elif detection_state == SentenceDetectionState.MiddleSentence:
            if (middle_word_ru_regexp.fullmatch(token) or
                    middle_word_en_regexp.fullmatch(token) or
                    all_upper_middle_word_ru_regexp.fullmatch(token) != None or
                    all_upper_middle_word_en_regexp.fullmatch(token) != None):
                if token[-1] in '.!?':
                    #print('Sentence:', ' '.join(tokens[i - sentence_length:i + 1]))
                    if sentence_length + 1 > 4:
                        num_sentences += 1
                    detection_state = SentenceDetectionState.BeforeFirstWord

                sentence_length += 1
            elif (standalone_punctuation_regexp.fullmatch(token) or
                    standalone_number_regexp.fullmatch(token)):
                sentence_length += 1
            else:
                detection_state = SentenceDetectionState.Unknown

        if detection_state == SentenceDetectionState.Unknown:
            if (token in ['<p>', '\n'] or (
                    (middle_word_ru_regexp.fullmatch(token) or middle_word_en_regexp.fullmatch(token)) 
                        and token[-1] in '.!?')):
                detection_state = SentenceDetectionState.BeforeFirstWord


    return num_sentences


sentence_regexp = '([.!?][  ]|^[  \t]*|<p>[  \t]*)[А-ЯЁ][А-ЯЁа-яё,"«»()  ֊־‐‑‒–—―﹣－-]+[:.!?]'

# TODO join divs
# но только братьев, когда оба содержат предложения
# и детектирование преложений улучшить
class MyHTMLParser(html.parser.HTMLParser):

    def __init__(self):
        html.parser.HTMLParser.__init__(self)
        self._texts_stack = []
        self._paragraph_sentence_text_num_sentences = 1
        self._paragraph_sentence_text = ''
        self._sentence_text_num_sentences = 1
        self._sentence_text = ''
        self._paragraph_text = ''

    def handle_starttag(self, tag, attrs):
        self._texts_stack.append([])

    def handle_endtag(self, tag):
        if len(self._texts_stack) == 0: return
        content = re.sub('\n+', ' \n ', ''.join(self._texts_stack[-1]), flags=re.MULTILINE)
        self._texts_stack.pop()

        if len(content.strip()) == 0 or re.search('\\b404\\b', content) != None: 
            return
        if len(re.findall('[a-z]', content, flags=re.IGNORECASE)) > len(re.findall('[а-яё]', content, flags=re.IGNORECASE)):
            return
        #print('trying (', len(content), '):\n', content, '\n----\n')

        paragraph = '<p>' in content
        m = re.search(sentence_regexp, content, re.MULTILINE)
        num_sentences = count_sentences(content)
        #sentence = m != None and re.search('\s', m.group(0)) != None
        #sentence = max(map(lambda s: len(s.strip()), content.split('\n'))) >= 70 and re.search(sentence_regexp, content, re.MULTILINE) != None

        if paragraph and num_sentences > self._paragraph_sentence_text_num_sentences: 
            assert len(self._paragraph_sentence_text) / len(content) < 2.0
            change = self.copyleft(self._paragraph_sentence_text, content)
            if change:
                self._paragraph_sentence_text = content
                self._paragraph_sentence_text_num_sentences = num_sentences

        if num_sentences > self._sentence_text_num_sentences: 
            assert len(self._sentence_text) / len(content) < 2.0
            #print('Sentence present')
            change = self.copyleft(self._sentence_text, content)
            if change:
                self._sentence_text = content
                self._sentence_text_num_sentences = num_sentences

        if paragraph and len(content) > len(self._paragraph_text):
            self._paragraph_text = content if self.copyleft(self._paragraph_text, content) else self._paragraph_text


        

    def copyleft(self, old_text, new_text):
        if len(old_text) == 0: return True

        if '©' in new_text:
            return False

        '''
        if ('компани'.casefold() in old_text.casefold() and
                'компани'.casefold() not in new_text.casefold() and
                len(new_text)/len(old_text) < 1.5):
            return old_text
        '''

        return True
            
        
    def handle_data(self, data):
        if len(self._texts_stack) == 0: 
            return
        assert type(data) == str
        self._texts_stack[-1].append(data)

    def get_longest_text(self):
        if len(self._paragraph_sentence_text) > 0:
            #print('Paragraph sentence text:', self._paragraph_sentence_text)
            return self._paragraph_sentence_text
        elif len(self._sentence_text) > 0:
            #print('Returning sentence text')
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

    #print(content)

    content = re.sub('<(br|dt|dd)[^>]*>', '\n', content, flags=re.IGNORECASE)
    content = re.sub('</?(b|u|a|ul|ol|li|i|font|dt|dl|dd|big|header|h[1-6]|em|strong|blockquote|span|wbr)[^>]*>', '', content, flags=re.IGNORECASE)
    content = re.sub('</p>', '\n', content, flags=re.IGNORECASE)
    content = re.sub('<p( [^>]+)?>', ' &lt;p&gt; ', content, flags=re.IGNORECASE)
    #content = re.sub('<p[^>]*>', '', content)

    #print(content)

    parser = MyHTMLParser()
    parser.feed(content)

    longest_text = parser.get_longest_text().replace('<p>', '\n')
    longest_text = re.sub('[ \t]+', ' ', longest_text)
    longest_text = re.sub('\s{2,}', '\n', longest_text)

    return longest_text

