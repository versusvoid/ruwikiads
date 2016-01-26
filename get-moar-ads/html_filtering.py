
import sys
import re
import html
import html.parser
import enum
from selenium.common.exceptions import NoSuchElementException

js_extractor = None
with open('js/text-extraction.js', 'r') as f:
    js_extractor = ''.join(f)

def extract_text_content(driver):
    return driver.execute_script(js_extractor)

def remove_all(driver, tag):
    for elem in driver.find_elements_by_tag_name(tag):
       driver.execute_script("""
           var element = arguments[0];
           element.parentNode.removeChild(element);
       """, elem) 

def replace_elements(driver, tag, prefix, suffix):
    script = "arguments[0].outerHTML = ['{}', arguments[0].innerHTML, '{}'].join('');".format(prefix.replace("'", r"\'"), suffix.replace("'", r"\'"))
    try:
        elem = driver.find_element_by_tag_name(tag)
        while True:
            if elem.is_displayed():
                driver.execute_script(script, elem);
            else:
                driver.execute_script("arguments[0].parentNode.removeChild(arguments[0]);", elem);
            elem = driver.find_element_by_tag_name(tag)
    except NoSuchElementException:
        pass

def debug(driver):
    print(driver.page_source)

    for entry in driver.get_log('browser'):
        print(entry['message'][:-4])

def extract_main_content(driver):
    '''
    remove_all(driver, 'nav')
    for tag in ['br', 'dt', 'dd']:
        replace_elements(driver, tag, r'\n', r'\n')

    for tag in ['b', 'u', 'a', 'ul', 'ol', 'li', 'i', 'font', 'dt', 'dl', 'big', 'header', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'em', 'strong', 'blockquote', 'span', 'wbr']:
        replace_elements(driver, tag, '', '')

    replace_elements(driver, 'p', '&lt;p&gt;', r'\n')
    '''

    text = extract_text_content(driver)

    #debug(driver)

    text = text.replace('<p>', '\n')
    text = re.sub('[ \t]+', ' ', text)
    text = re.sub('\s{2,}', '\n', text)

    return text

