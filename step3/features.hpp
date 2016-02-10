#include <array>
#include <unordered_set>
#include <unordered_map>
#include <tuple>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>


typedef std::wstring one_gram;
std::unordered_set<one_gram> one_grams = {
    L"агенство",
    L"компания",
    L"мы",
    L"нас",
    L"предприятие",
    L"фабрика",
    L"фирма",
    
    L"автоматизация",
    L"активно",
    L"ассортимент",
    L"более",
    L"важный",
    L"ведущий",
    L"возможность",
    L"впервые",
    L"все",
    L"выдающийся",
    L"гибкий",
    L"гибко",
    L"город",
    L"динамично",
    L"известность",
    L"известный",
    L"ключевой",
    L"комплексно",
    L"край",
    L"крупный",
    L"лидер",
    L"лидерский",
    L"максимальный",
    L"многолетний",
    L"многофункциональный",
    L"награда",
    L"надёжный",
    L"нестандартный",
    L"область",
    L"однако",
    L"ответственность",
    L"ответственный",
    L"первый",
    L"повсеместно",
    L"полезный",
    L"потенциал",
    L"премия",
    L"производство",
    L"профессионал",
    L"профессиональный",
    L"развиваться",
    L"развиваться",
    L"регион",
    L"рейтинг",
    L"самый",
    L"современный",
    L"специалист",
    L"успешно",
    L"успешный",
    L"эффективность",
    L"эффективный"
};

typedef std::tuple<std::wstring, std::wstring> two_gram;
std::set<two_gram> two_grams = {

    std::make_tuple(L"высокий", L"качество"),
    std::make_tuple(L"более", L"чем"),
    std::make_tuple(L"страна", L"мир"),
    std::make_tuple(L"низкий", L"стоймость"),
    std::make_tuple(L"первый", L"место")
};

typedef std::tuple<std::wstring, std::wstring, std::wstring> three_gram;
std::set<three_gram> three_grams = {
    std::make_tuple(L"один", L"из", L"самый")
};

typedef std::unordered_map<std::wstring, float> features_dict;
inline void record_feature(features_dict& features, std::initializer_list<std::wstring> const& l, float value = 1) {
    auto feature = boost::algorithm::join(l, L"-");
    auto pos = features.find(feature);
    if (pos != features.end()) {
        pos->second += value;
    } else {
        features[feature] = value;
    }
}
inline void record_feature(features_dict& features, const std::wstring& s, float value = 1) { record_feature(features, {s}, value); }

inline bool contains_russian(const std::wstring& str) {
    for (std::size_t i = 0; i < str.length(); ++i) {
        if ((str[i] >= L'а' and str[i] <= L'я') or str[i] == L'ё') return true;
    }
    return false;
}

inline bool contains_english(const std::wstring& str) {
    for (std::size_t i = 0; i < str.length(); ++i) {
        if (str[i] >= L'a' and str[i] <= L'z') return true;
    }
    return false;
}

inline bool extract_pos(const std::wstring& info, std::wstring& pos, std::wstring& grammatical_info) {
    auto pos_pos = info.find(L'=');
    if (pos_pos == info.npos) return false;
    auto end = pos_pos + 1;
    while (end < info.length() and info[end] >= 'A' and info[end] <= 'Z') end += 1;
    assert(end != pos_pos + 1 and end < info.length());
    pos = info.substr(pos_pos + 1, end - pos_pos - 1);

    auto grammatical_info_pos = info.find(L'=', end);
    end = grammatical_info_pos + 1;
    while (end < info.length() and info[end] != L'|' and info[end] != L'}') end += 1;
    grammatical_info = info.substr(grammatical_info_pos + 1, end - grammatical_info_pos - 1);

    return true;
}

template<typename Container, typename Elem>
inline bool contains(const Container& container, const Elem& elem) {
    return container.find(elem) != container.end();
}

inline std::wstring extract_lexeme(const std::wstring& info) {
    std::size_t len = 0;
    while (len < info.length() and ((info[len] >= L'а' and info[len] <= L'я') or info[len] == L'ё')) len += 1;
    return info.substr(0, len);
}

inline bool gerund_like(const std::wstring& lexeme) {
    return lexeme.length() > 3 and lexeme.substr(lexeme.length() - 3) == L"ние";
}

inline bool is_superlative(const std::wstring& info) {
    return info.find(L",прев") != info.npos;
}

inline bool is_12person(const std::wstring& info) {
    return info.find(L"1-л") != info.npos or info.find(L"2-л") != info.npos;
}



std::unordered_set<std::wstring> ignore_pos = {L"CONJ", L"INTJ", L"PART", L"PR"};
void extract_features_from_word(features_dict& features, std::wstring& word, std::vector<std::wstring> sequence) {
    std::wstring form, info;
    {
        auto pos = word.find(L'{');
        //std::wcout << '"' << word << '"' << word.length() << std::endl;
        assert(pos != word.npos);

        form = word.substr(0, pos);
        boost::to_lower(form);

        info = word.substr(pos + 1);
    }

    if (not contains_russian(form)) {
        record_feature(features, {L"non russian%"});
        sequence.push_back(L"*");
        return;
    }

    bool english_letters = contains_english(form);

    std::wstring pos, grammatical_info;

    if (not extract_pos(info, pos, grammatical_info)) {
        if (not english_letters) {
            sequence.push_back(form);
        } else {
            sequence.push_back(L"*");
        }
        return;
    }

    record_feature(features, {L"POS%", pos});
    if (contains(ignore_pos, pos)) {
        sequence.push_back(form);
        return;
    }

    auto lexeme = extract_lexeme(info);
    assert(not lexeme.empty());
    sequence.push_back(lexeme);

    if (gerund_like(lexeme)) {
        record_feature(features, L"отглагольное существительное%"); // maybe
    }
    if (lexeme.length() >= 15) {
        record_feature(features, L"длинное слово%");
    }
    if (contains(one_grams, lexeme)) {
        record_feature(features, {lexeme, grammatical_info});
    }

    //if (boost::regex_search(info, superlative_regex)) {
    if (is_superlative(info)) {
        record_feature(features, L"превосходное прилагательное%");
    }
    //if (boost::regex_search(info, person_regex)) {
    if (is_12person(info)) {
        record_feature(features, L"1,2-е лицо%");
    }
}
        
void extract_features_from_sequence(features_dict& features, std::vector<std::wstring> sequence) {
    features[L"length"] = sequence.size();
    for (auto& kv : features) {
        if (kv.first.find(L'%') != std::wstring::npos) {
            kv.second /= sequence.size();
        }
    }

    for (std::size_t i = 1; i < sequence.size(); ++i) {
        if (contains(two_grams, std::make_tuple(sequence[i-1], sequence[i]))) {
            record_feature(features, {L"2gram", sequence[i-1], sequence[i]});
        }
        if (i > 1 and contains(three_grams, std::make_tuple(sequence[i-2], sequence[i-1], sequence[i]))) {
            record_feature(features, {L"3gram", sequence[i-2], sequence[i-1], sequence[i]});
        };
    }
}

