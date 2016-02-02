import re

def record_feature(feature_dict, *args):
    feature = '-'.join(args)
    feature_dict[feature] = feature_dict.get(feature, 0) + 1

def extract_features_from_word(sample_features, word_line, word_sequence):
    parts = word_line.split('{')
    assert len(parts) == 2, word_line
    form, info = parts
    form = form.casefold()

    if re.search('[а-яё]', form, flags=re.IGNORECASE) is None:
        record_feature(sample_features, 'form', form)
        record_feature(sample_features, 'non russian')
        word_sequence.append('*')
        return

    english_letters = re.search('[a-z]', form, flags=re.IGNORECASE) is not None

    pos = re.search('=([A-Z]+)=', info)
    if pos is None:
        if not english_letters:
            record_feature(sample_features, 'form', form)
            word_sequence.append(form)
        else:
            word_sequence.append('*')
        return

    if pos.group(1) in ['CONJ', 'INTJ', 'PART', 'PR']:
        word_sequence.append(form)
        return

    lexeme = re.match('[а-яё]+', info, flags=re.IGNORECASE)
    assert lexeme is not None, '{}: {}'.format(i, l)
    word_sequence.append(lexeme.group(0))
    if lexeme.group(0) in ['мы', 'наш']: 
        return

    if not english_letters:
        record_feature(sample_features, 'form', form)
    record_feature(sample_features, 'lexeme', lexeme.group(0))
    if re.findall('\bпрев\b', info) != None:
        record_feature(sample_features, 'превосходное прилагательное')

def extract_features_from_word_sequence(sample_features, word_sequence):
    for i in range(2, len(word_sequence)):
        record_feature(sample_features, 'pair', word_sequence[i-2], word_sequence[i-1])

