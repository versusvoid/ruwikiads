#include <array>
#include <unordered_set>
#include <unordered_map>
#include <tuple>
#include <chrono>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

#ifdef PROFILING

struct profiler {

    profiler(const std::string& name)
        : name(name)
        , calls(0)
    {}

    ~profiler() {
        std::wcout << name.c_str() << ": " << elapsed_seconds.count() << "s in " << calls << " calls" << std::endl;
    }

    inline void start_profiling() {
        start = std::chrono::high_resolution_clock::now();
    }

    inline void stop_profiling() {
        calls += 1;
        elapsed_seconds += std::chrono::high_resolution_clock::now() - start;
    }


    std::string name;
    uint64_t calls;
    std::chrono::duration<double> elapsed_seconds;

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
};

profiler first_half("first half");
profiler second_half("second half");

struct profiler_guard {

    profiler_guard(profiler& parent, bool run=true)
        : parent(parent)
        , _start(std::chrono::high_resolution_clock::now())
        , running(run)
    {}

    inline void start() {
        _start = std::chrono::high_resolution_clock::now();
        running = true;
    }

    inline void stop() {
        parent.elapsed_seconds += std::chrono::high_resolution_clock::now() - _start;
        parent.calls += 1;
        running = false;
    }

    ~profiler_guard() {
        if (running) {
            parent.elapsed_seconds += std::chrono::high_resolution_clock::now() - _start;
            parent.calls += 1;
        }
    }

    profiler& parent;
    std::chrono::time_point<std::chrono::high_resolution_clock> _start;
    bool running;
};

#endif


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

#ifdef PROFILING
profiler record_feature_profiler("record feature");
#endif
typedef std::unordered_map<std::wstring, float> features_dict;
/*
inline void record_feature(features_dict& features, std::initializer_list<const std::wstring&> l, float value = 1) {
    profiler_guard guard(record_feature_profiler);

    auto feature = boost::algorithm::join(l, L"-");
}
*/
/*inline */void record_feature(features_dict& features, const std::wstring& feature, float value = 1) {

    auto pos = features.find(feature);
    if (pos != features.end()) {
        pos->second += value;
    } else {
        features[feature] = value;
    }
}
/*inline */void record_feature(features_dict& features, const std::wstring& s1, const std::wstring& s2, float value = 1) {
#ifdef PROFILING
    profiler_guard guard(record_feature_profiler);
#endif

    std::wstring feature;
    feature.reserve(s1.length() + 1 + s2.length() + 1);
    feature += s1;
    feature += L'-';
    feature += s2;
    record_feature(features, feature, value);
}
/*inline */void record_feature(features_dict& features, const std::wstring& s1, const std::wstring& s2, const std::wstring& s3, float value = 1) {
#ifdef PROFILING
    profiler_guard guard(record_feature_profiler);
#endif

    std::wstring feature;
    feature.reserve(s1.length() + 1 + s2.length() + 1 + s3.length() + 1);
    feature += s1;
    feature += L'-';
    feature += s2;
    feature += L'-';
    feature += s3;
    record_feature(features, feature, value);
}
/*inline */void record_feature(features_dict& features, const std::wstring& s1, const std::wstring& s2, const std::wstring& s3, const std::wstring& s4, float value = 1) {
#ifdef PROFILING
    profiler_guard guard(record_feature_profiler);
#endif

    std::wstring feature;
    feature.reserve(s1.length() + 1 + s2.length() + 1 + s3.length() + 1 + s4.length() + 1);
    feature += s1;
    feature += L'-';
    feature += s2;
    feature += L'-';
    feature += s3;
    feature += L'-';
    feature += s4;
    record_feature(features, feature, value);
}

inline void test_leters(const std::wstring& str, bool& russian, bool& english) {
    russian = false;
    english = false;
    for (std::size_t i = 0; i < str.length(); ++i) {
        if (not russian and ((str[i] >= L'а' and str[i] <= L'я') or str[i] == L'ё')) {
            russian = true;
            if (english) return;
        }
        if (not english and (str[i] >= L'a' and str[i] <= L'z')) {
            english = true;
            if (russian) return;
        }
    }
    return;
}

#ifdef PROFILING
profiler letters_profiler("letters");
#endif
inline bool contains_russian(const std::wstring& str) {
#ifdef PROFILING
    profiler_guard guard(letters_profiler);
#endif

    for (std::size_t i = 0; i < str.length(); ++i) {
        //if ((str[i] >= L'а' and str[i] <= L'я') or str[i] == L'ё') return true;
        if ((str[i] >= L'а' and str[i] <= L'я') or str[i] == L'ё' or
                (str[i] >= L'А' and str[i] <= L'Я') or str[i] == L'Ё') return true;
    }
    return false;
}

inline bool contains_english(const std::wstring& str) {
#ifdef PROFILING
    profiler_guard guard(letters_profiler);
#endif

    for (std::size_t i = 0; i < str.length(); ++i) {
        //if (str[i] >= L'a' and str[i] <= L'z') return true;
        if ((str[i] >= L'a' and str[i] <= L'z') or (str[i] >= L'A' and str[i] <= L'Z')) return true;
    }
    return false;
}

#ifdef PROFILING
profiler pos_profiler("pos");
#endif
inline bool extract_pos(const std::wstring& info, std::wstring& pos, std::wstring& grammatical_info) {
#ifdef PROFILING
    profiler_guard guard(pos_profiler);
#endif

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

#ifdef PROFILING
profiler split_profiler("split to form and info");
profiler pos_checks_profiler("pos record");
#endif
std::unordered_set<std::wstring> ignore_pos = {L"CONJ", L"INTJ", L"PART", L"PR"};
void extract_features_from_word(features_dict& features, std::wstring& word, std::vector<std::wstring>& sequence) {
#ifdef PROFILING
    profiler_guard first_half_guard(first_half);
#endif

    std::wstring form, info;
    {
#ifdef PROFILING
        profiler_guard guard(split_profiler);
#endif

        auto pos = word.find(L'{');
        //std::wcout << '"' << word << '"' << word.length() << std::endl;
        assert(pos != word.npos);

        form = word.substr(0, pos);
        //boost::to_lower(form);

        info = word.substr(pos + 1);
    }

    if (not contains_russian(form)) {
        record_feature(features, L"non russian%");
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

#ifdef PROFILING
    first_half_guard.stop();
#endif

    {
#ifdef PROFILING
        profiler_guard guard(pos_checks_profiler);
#endif
        //record_feature(features, {L"POS%", pos});
        record_feature(features, L"POS%", pos);
    }

    if (pos == L"CONJ" or pos == L"INTJ" or pos == L"PART" or pos == L"PR") {
        sequence.push_back(form);
        return;
    }

#ifdef PROFILING
    profiler_guard second_half_guard(second_half, false);
    second_half_guard.start();
#endif

    auto lexeme = extract_lexeme(info);
    assert(not lexeme.empty());
    sequence.push_back(lexeme);

    if (gerund_like(lexeme)) {
        record_feature(features, L"отглагольное существительное%"); // maybe
    }
    if (lexeme.length() >= 15) {
        record_feature(features, L"длинное слово%");
    }

    if (one_grams.count(lexeme) > 0) {
        record_feature(features, lexeme, grammatical_info);
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
        
void extract_features_from_sequence(features_dict& features, std::vector<std::wstring>& sequence) {
    features[L"length"] = sequence.size();
    for (auto& kv : features) {
        if (kv.first.find(L'%') != std::wstring::npos) {
            kv.second /= sequence.size();
        }
    }

    for (std::size_t i = 1; i < sequence.size(); ++i) {
        if (two_grams.count(std::make_tuple(sequence[i-1], sequence[i])) > 0) {
            record_feature(features, L"2gram", sequence[i-1], sequence[i]);
        }
        if (i > 1 and three_grams.count(std::make_tuple(sequence[i-2], sequence[i-1], sequence[i])) > 0) {
            record_feature(features, L"3gram", sequence[i-2], sequence[i-1], sequence[i]);
        };
    }
}

