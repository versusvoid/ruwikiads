import regex

russian_letter_regex = regex.compile('[а-яё]', flags=regex.IGNORECASE)
english_letter_regex = regex.compile('[a-z]', flags=regex.IGNORECASE)
pos_regex = regex.compile('=([A-Z]+)[,=]')
russian_word_regex = regex.compile('[а-яё]+', flags=regex.IGNORECASE)
superlative_regex = regex.compile(r'\bпрев\b')
person_regex = regex.compile(r'\b[12]-л\b')

ngrams_features = set([

    "агенство",
    "компания",
    "мы",
    "нас",
    "предприятие",
    "фабрика",
    "фирма",
    
    "автоматизация",
    "активно",
    "ассортимент",
    "более",
    "важный",
    "ведущий",
    "возможность",
    "впервые",
    "все",
    "выдающийся",
    "гибкий",
    "гибко",
    "город",
    "динамично",
    "известность",
    "известный",
    "ключевой",
    "комплексно",
    "край",
    "крупный",
    "лидер",
    "лидерский",
    "максимальный",
    "многолетний",
    "многофункциональный",
    "награда",
    "надёжный",
    "нестандартный",
    "область",
    "однако",
    "ответственность",
    "ответственный",
    "первый",
    "повсеместно",
    "полезный",
    "потенциал",
    "премия",
    "производство",
    "профессионал",
    "профессиональный",
    "развиваться",
    "развиваться",
    "регион",
    "рейтинг",
    "самый",
    "современный",
    "специалист",
    "успешно",
    "успешный",
    "эффективность",
    "эффективный",


    
    ("высокий", "качество"),
    ("более", "чем"),
    ("страна", "мир"),
    ('низкий', 'стоймость'),
    ('первый', 'место'),
    ('один', "из", "самый")
    ])

def record_feature(feature_dict, *args):
    feature = '-'.join(args)
    feature_dict[feature] = feature_dict.get(feature, 0) + 1

def extract_features_from_word(sample_features, word_line, word_sequence):
    parts = word_line.split('{')
    assert len(parts) == 2, word_line
    form, info = parts

    form = form.casefold()

    if russian_letter_regex.search(form) is None:
        record_feature(sample_features, 'non russian%')
        word_sequence.append('*')
        return

    english_letters = english_letter_regex.search(form) is not None

    pos = pos_regex.search(info)
    if pos is None:
        if not english_letters:
            word_sequence.append(form)
        else:
            word_sequence.append('*')
        return
    pos = pos.group(1)

    record_feature(sample_features, 'POS%', pos)
    if pos in ['CONJ', 'INTJ', 'PART', 'PR']:
        word_sequence.append(form)
        return

    lexeme = russian_word_regex.match(info)
    assert lexeme is not None, '{}: {}'.format(i, l)
    lexeme = lexeme.group(0)
    # TODO не просто лексему, но и грамматическую информацию на неё?
    word_sequence.append(lexeme)

    if lexeme.endswith('ние'):
        record_feature(sample_features, 'отглагольное существительное%') # maybe
    if len(lexeme) >= 15:
        record_feature(sample_features, 'длинное слово%')

    if superlative_regex.search(info) is not None:
        record_feature(sample_features, 'превосходное прилагательное%')
    if person_regex.search(info) is not None:
        record_feature(sample_features, '1,2-е лицо%')
        

def extract_features_from_word_sequence(sample_features, word_sequence):
    sample_features['length'] = len(word_sequence)
    for k in sample_features.keys():
        if '%' in k:
            sample_features[k] /= len(word_sequence)

    lexeme_sequence = list(map(lambda p:p[0], word_sequence))

    for i in range(len(lexeme_sequence)):
        if lexeme_sequence[i] in ngrams_features:
            record_feature(sample_features, '1gram', lexeme_sequence[i])
        if i > 0 and tuple(lexeme_sequence[i-1:i+1]) in ngrams_features:
            record_feature(sample_features, '2gram', *lexeme_sequence[i-1:i+1])
        if i > 1 and tuple(lexeme_sequence[i-2:i+1]) in ngrams_features:
            record_feature(sample_features, '3gram', *lexeme_sequence[i-2:i+1])
        if i > 2 and tuple(lexeme_sequence[i-3:i+1]) in ngrams_features:
            record_feature(sample_features, '4gram', *lexeme_sequence[i-3:i+1])






def extract_features_from_word_old(sample_features, word_line, word_sequence):
    parts = word_line.split('{')
    assert len(parts) == 2, word_line
    form, info = parts

    form = form.casefold()

    if russian_letter_regex.search(form) is None:
        record_feature(sample_features, 'form', form)
        record_feature(sample_features, 'non russian')
        word_sequence.append('*')
        return

    english_letters = english_letter_regex.search(form) is not None

    pos = pos_regex.search(info)
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

    lexeme = russian_word_regex.match(info)
    assert lexeme is not None, '{}: {}'.format(i, l)
    word_sequence.append(lexeme.group(0))

    if not english_letters:
        record_feature(sample_features, 'form', form)
    record_feature(sample_features, 'lexeme', lexeme.group(0))
    if superlative_regex.search(info) is not None:
        record_feature(sample_features, 'превосходное прилагательное')
    

def extract_features_from_word_sequence_old(sample_features, word_sequence):
    if len(word_sequence) < 2: return
    record_feature(sample_features, 'pair', word_sequence[0], word_sequence[1])
    for i in range(2, len(word_sequence)):
        record_feature(sample_features, 'pair', word_sequence[i-1], word_sequence[i])
        record_feature(sample_features, 'triple', word_sequence[i-2], word_sequence[i-1], word_sequence[i])

