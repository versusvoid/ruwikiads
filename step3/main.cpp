#include <fstream>
#include <codecvt>
#include <future>
#include <vector>
#include <algorithm>
#include <iostream>
#include <stdio.h>
#include <chrono>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/dynamic_bitset.hpp>
#include <xgboost/c_api.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "features.hpp"

#if defined(_MSC_VER)
# define POPEN _popen
# define PCLOSE _pclose
#else
# define POPEN popen
# define PCLOSE pclose
#endif

#define SEPARATOR L"samplesSeparator"
#define ADS_LABEL 1.0
#define NON_ADS_LABEL 0.0

std::random_device RANDOM_DEVICE;
std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> ucs2conv;

inline bool getline(char** lineptr, size_t* n, FILE* file, std::wstring& out) {
    assert(*lineptr != nullptr);
    auto len = getline(lineptr, n, file);
    //if (len == ssize_t(-1) and not feof(file)) return false;
    if (len == ssize_t(-1)) return false;
    out = ucs2conv.from_bytes(*lineptr, *lineptr + len);
    return true;
}

struct csr_matrix_t {

    csr_matrix_t() {
        std::wcout << "csr_matrix_t()" << std::endl;
    }
    csr_matrix_t(uint32_t samples_count) {
        std::wcout << "csr_matrix_t(uint32_t)" << std::endl;
        reserve(samples_count);
    }

    csr_matrix_t(const csr_matrix_t& other)
        : data(other.data)
        , indices(other.indices)
        , indptr(other.indptr)
        , labels(other.labels)
        , sources(other.sources)
    {
        std::wcout << "csr_matrix_t(const csr_matrix_t&)" << std::endl;
    }

    csr_matrix_t(csr_matrix_t&& other)
        : data(std::move(other.data))
        , indices(std::move(other.indices))
        , indptr(std::move(other.indptr))
        , labels(std::move(other.labels))
        , sources(std::move(other.sources))
    {
        std::wcout << "csr_matrix_t(csr_matrix_t&&)" << std::endl;
    }

    csr_matrix_t& operator =(const csr_matrix_t& other) {
        std::wcout << "csr_matrix_t& operator =(const csr_matrix_t&)" << std::endl;
        data = other.data;
        indices = other.indices;
        indptr = other.indptr;
        labels = other.labels;

        sources = other.sources;

        return *this;
    }

    csr_matrix_t& operator =(csr_matrix_t&& other) {
        std::wcout << "csr_matrix_t& operator =(csr_matrix_t&&)" << std::endl;
        data = std::move(other.data);
        indices = std::move(other.indices);
        indptr = std::move(other.indptr);
        labels = std::move(other.labels);

        sources = std::move(other.sources);

        return *this;
    }

    ~csr_matrix_t() {
        std::wcout << "~csr_matrix_t()" << std::endl;
    }

    void reserve(uint32_t samples_count) {

        indptr.reserve(samples_count + 1);
        labels.reserve(samples_count);
        sources.reserve(samples_count);
    }

    std::vector<float> data;
    std::vector<unsigned> indices;
    std::vector<bst_ulong> indptr;
    std::vector<float> labels;

    std::vector<std::wstring> sources;
};

struct file_split_t {

    file_split_t(const std::string& filename, float label, float split_ratio = 0.4)
        : filename(filename)
        , label(label)
    {
        assert(boost::filesystem::exists(filename));
        assert(split_ratio < 0.5 or split_ratio == 1.0);
        load_sources();
        num_samples = sources.size();
        generate_split(split_ratio);

    }

    std::vector<std::wstring> load_sources() {
        assert(filename.find("stemmed.txt.bz2") != filename.npos);
        auto index_filename = filename.substr(0, filename.find("stemmed.txt.bz2"))
                              + "index.txt";

        std::wifstream index(index_filename);
        assert(index.is_open());
        std::vector<std::wstring> source_parts;
        uint32_t len = 0;
        std::wstring line;
        std::getline(index, line);
        while (line.length() > 0) {

            if (boost::starts_with(line, SEPARATOR)) {
                assert(source_parts.size() > 0);
                std::wstring source;
                source.reserve(len + source_parts.size() - 1);
                for (auto i = 0U; i < source_parts.size() - 1; ++i) {
                    source += source_parts.at(i);
                    source += L'\n';
                }
                source += source_parts.back();
                sources.push_back(source);

                len = 0;
                source_parts.clear();
            } else {
                source_parts.push_back(line);
                len += line.length();
            }

            std::getline(index, line);
        }
        assert(index.eof());
        index.close();

        std::wcout << "Counted in " << index_filename.c_str() << " " << sources.size() << std::endl;

        return sources;
    }

    void generate_split(float split_ratio) {
        if (split_ratio == 1.0) {
            split.resize(num_samples, true);
            return;
        } else {
            split.resize(num_samples);
        }
        auto num_test_samples = std::size_t(num_samples * split_ratio);
        std::uint32_t num_set = 0;

        std::mt19937 e(RANDOM_DEVICE());
        std::uniform_int_distribution<uint32_t> uniform_dist(0, num_samples - 1);
        while (num_set < num_test_samples) {
            uint32_t i = uniform_dist(e);
            if (not split.test(i)) {
                split.set(i);
                num_set += 1;
            }
        }

        train_part.reserve(num_samples - num_set);
        test_part.reserve(num_set);
    }

    std::string filename;
    float label;
    uint32_t num_samples;
    std::vector<std::wstring> sources;
    boost::dynamic_bitset<> split;
    csr_matrix_t train_part;
    csr_matrix_t test_part;
};

typedef std::unordered_map<std::wstring, uint32_t> features_indexes_t;
std::mutex temp_features_indexes_mutex;
int32_t get_temp_feature_index(const std::wstring& feature,
                                features_indexes_t& temp_features_indexes,
                                bool update) {

    std::lock_guard<std::mutex> lock(temp_features_indexes_mutex);

    auto pos = temp_features_indexes.find(feature);
    if (pos == temp_features_indexes.end()) {

        if (update) {
            temp_features_indexes.insert(std::make_pair(feature, temp_features_indexes.size()));
            return temp_features_indexes.size() - 1;
        } else {
            return -1;
        }

    } else {
        return pos->second;
    }
}

features_dict extract_features_from_file(file_split_t* file_split,
                                         features_indexes_t* temp_features_indexes,
                                         bool update_temp_features_indexes = true) {

    features_dict features_counts;

    uint32_t sample_number = 0;
    features_dict sample_features;
    std::vector<std::wstring> word_sequence;


#ifdef PROFILING
    profiler end_profiler(filename + " sequence");
    profiler word_profiler(filename + " word");
#endif

    std::shared_ptr<FILE> bunzip2(POPEN(("bunzip2 -c -k " + file_split->filename).c_str(), "r"), PCLOSE);

    size_t len = 4096;
    char* buf = (char*)malloc(len  * sizeof(char));
    std::wstring line;
    while(getline(&buf, &len, bunzip2.get(), line)) {
        assert(line.length() > 0);

        if (boost::starts_with(line, SEPARATOR)) {
#ifdef PROFILING
            profiler_guard guard(end_profiler);
#endif

            if (word_sequence.size() > 1) {

                extract_features_from_sequence(sample_features, word_sequence);

                bool test_sample = file_split->split.test(sample_number);
                if (not test_sample) {
                    for (auto& kv : sample_features) {
                        record_feature(features_counts, kv.first);
                    }
                }

                csr_matrix_t* matrix = test_sample? &file_split->test_part : &file_split->train_part;
                matrix->indptr.push_back(matrix->indices.size());
                matrix->sources.push_back(file_split->sources.at(sample_number));
                matrix->labels.push_back(file_split->label);
                for (auto& kv : sample_features) {

                    int32_t feature_index = get_temp_feature_index(kv.first,
                                                                   *temp_features_indexes,
                                                                   update_temp_features_indexes);
                    if (feature_index >= 0) {
                        matrix->indices.push_back(feature_index);
                        matrix->data.push_back(kv.second);
                    }
                }
            }

            sample_number += 1;
            if (sample_number % 30000 == 0) {
                std::wcout << "feature extraction " << file_split->filename.c_str()
                           << " " << sample_number << std::endl;
            }
            word_sequence.clear();
            sample_features.clear();

        } else {
#ifdef PROFILING
            profiler_guard guard(word_profiler);
#endif

            extract_features_from_word(sample_features, line, word_sequence);
        }

    }
    free(buf);
    file_split->sources.clear();

    return features_counts;
}

inline void join_counts(features_dict& dest, const features_dict& source) {
    for (auto& kv : source) {
        record_feature(dest, kv.first, kv.second);
    }
}

void convert_features(csr_matrix_t& matrix,
                      const std::vector<int32_t>& features_transform) {

    csr_matrix_t result(matrix.labels.capacity());
    result.indices.reserve(matrix.indices.size());
    result.data.reserve(matrix.data.size());

    for (auto i = 0U; i < matrix.indptr.size(); ++i) {

        result.labels.push_back(matrix.labels.at(i));
        result.sources.push_back(matrix.sources.at(i));

        result.indptr.push_back(result.indices.size());

        auto start = matrix.indptr.at(i);
        auto end = i == matrix.indptr.size() - 1? matrix.indices.size() : matrix.indptr.at(i + 1);
        for (auto j = start; j < end; ++j) {

            int32_t new_index = features_transform.at(matrix.indices.at(j));
            if (new_index != -1) {
                result.indices.push_back(new_index);
                result.data.push_back(matrix.data.at(j));
            }
        }
    }

    matrix = std::move(result);
}

inline void convert_features(file_split_t& file_split,
                      const std::vector<int32_t>& features_transform) {
    convert_features(file_split.test_part, features_transform);
    convert_features(file_split.train_part, features_transform);
}

features_indexes_t compute_features_indexes(std::vector<file_split_t>& non_ads_splits,
                                            file_split_t& ads_split) {

    features_indexes_t temp_features_indexes;
    std::vector<std::future<features_dict>> per_file_features_counts;

    std::wcout << "Running first non ads half" << std::endl;

    for (auto i = 0U; i < std::min(4UL, non_ads_splits.size()); ++i) {

        per_file_features_counts.push_back(
                    std::async(std::launch::async, extract_features_from_file,
                               &non_ads_splits.at(i),
                               &temp_features_indexes,
                               true
                               )
                    );
    }


    features_dict features_counts = extract_features_from_file(&ads_split,
                                                               &temp_features_indexes,
                                                               true);


    for (auto& counts_future : per_file_features_counts) {
        join_counts(features_counts, counts_future.get());
    }


    std::wcout << "Running second non ads half" << std::endl;

    for (auto j = 4U; j < non_ads_splits.size(); j += 4) {

        for (auto i = j; i < std::min(non_ads_splits.size(), j + 4UL); ++i) {
            per_file_features_counts.push_back(
                        std::async(std::launch::async, extract_features_from_file,
                                   &non_ads_splits[i],
                                   &temp_features_indexes,
                                   true
                                   )
                        );
        }

        for (auto i = j; i < std::min(non_ads_splits.size(), j + 4UL); ++i) {

            join_counts(features_counts, per_file_features_counts[i].get());
        }
    }

    features_indexes_t features_indexes;
    std::vector<int32_t> features_transform(temp_features_indexes.size(), -1);
    for (auto& kv : features_counts) {
        //if (kv.second > 1000) {
            features_transform[temp_features_indexes.at(kv.first)] = features_indexes.size();
            features_indexes.insert(std::make_pair(kv.first, features_indexes.size()));
        //}
    }

    std::wcout << "Converting termporal features' indices to final" << std::endl;

    for (auto& split : non_ads_splits) {
        convert_features(split, features_transform);
    }

    convert_features(ads_split, features_transform);

    return features_indexes;
}

void join_matrices(csr_matrix_t& to, const csr_matrix_t& from) {

    for (std::size_t i = 0; i < from.indptr.size(); ++i) {

        to.labels.push_back(from.labels.at(i));
        to.sources.push_back(from.sources.at(i));

        to.indptr.push_back(to.indices.size());

        auto start = from.indptr.at(i);
        auto end = i == from.indptr.size() - 1? from.indices.size() : from.indptr.at(i + 1);
        for(bst_ulong j = start; j < end; ++j) {
            to.indices.push_back(from.indices.at(j));
            to.data.push_back(from.data.at(j));
        }
    }
}

void shuffle(csr_matrix_t& matrix) {
    csr_matrix_t result(matrix.labels.size());
    result.data.reserve(matrix.data.size());
    result.indices.reserve(matrix.indices.size());

    std::vector<uint32_t> permutation(matrix.labels.size(), 0);
    std::iota(permutation.begin(), permutation.end(), 0);
    std::shuffle(permutation.begin(), permutation.end(), std::mt19937(RANDOM_DEVICE()));

    for(auto index : permutation) {

        result.labels.push_back(matrix.labels.at(index));
        result.sources.push_back(matrix.sources.at(index));

        result.indptr.push_back(result.indices.size());

        auto start = matrix.indptr.at(index);
        auto end = index == matrix.indptr.size() - 1? matrix.indices.size() : matrix.indptr.at(index + 1);
        for(bst_ulong j = start; j < end; ++j) {
            result.indices.push_back(matrix.indices.at(j));
            result.data.push_back(matrix.data.at(j));
        }
    }

    matrix = std::move(result);
}

void sum_sizes(uint32_t& set_size, uint32_t& set_data_size,
               uint32_t& set_indices_size, const csr_matrix_t& matrix) {

    set_size += matrix.labels.size();
    set_data_size += matrix.data.size();
    set_indices_size += matrix.indices.size();
}

void init_set(csr_matrix_t& set, csr_matrix_t file_split_t::*matrix,
              file_split_t& ads_file_split, file_split_t& wiki_ads_file_split,
              std::vector<file_split_t>& non_ads_file_splits) {
    uint32_t set_size = 0, set_data_size = 0, set_indices_size = 0;

    sum_sizes(set_size, set_data_size, set_indices_size, (ads_file_split.*matrix));
    sum_sizes(set_size, set_data_size, set_indices_size, (wiki_ads_file_split.*matrix));

    for(auto& non_ads_file_split : non_ads_file_splits) {
        sum_sizes(set_size, set_data_size, set_indices_size, (non_ads_file_split.*matrix));
    }

    set.reserve(set_size);
    set.data.reserve(set_data_size);
    set.indices.reserve(set_indices_size);



    join_matrices(set, ads_file_split.*matrix);
    join_matrices(set, wiki_ads_file_split.*matrix);
    for(auto& non_ads_file_split : non_ads_file_splits) {
        join_matrices(set, non_ads_file_split.*matrix);
    }

    shuffle(set);
}

std::string make_output_directory() {

    char buf[4096];
    std::time_t t = std::time(nullptr);
    auto len = std::strftime(buf, 4096, "data/output/%F-%H-%M", std::localtime(&t));
    assert(len > 0);
    std::string output_directory(buf, len);
    if (boost::filesystem::exists(output_directory)) return output_directory;

    assert(0 == mkdir(buf, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH));
    std::cerr << "Output dir set to " << output_directory << std::endl;

    return output_directory;
}

void output_index(std::vector<std::wstring>& index, const std::string& filename) {

    std::shared_ptr<FILE> bzip2(POPEN(("bzip2 -9 > " + filename).c_str(), "w"), PCLOSE);
    for (auto& source : index) {
        auto buf = ucs2conv.to_bytes(source);
        fwrite(buf.data(), sizeof(char), buf.length(), bzip2.get());
        fwrite("\nsamplesSeparator\n", sizeof(char), strlen("\nsamplesSeparator\n"), bzip2.get());
    }
}

void output_features_index(const std::string& output_directory,
                           features_indexes_t& features_indexes)
{
    std::wofstream f(output_directory + "/features-indexes.txt");
    for (auto& kv : features_indexes) {
        assert(kv.first.find(L':') == kv.first.npos);
        f << kv.first << L":" << std::to_wstring(kv.second) << std::endl;
    }
    f.close();
}

int main(int argc, char** argv)
{
    std::string output_directory = make_output_directory();

    std::locale::global(std::locale(std::locale(""), "C", std::locale::numeric));
    std::wcout.imbue(std::locale());

    std::string non_ads_samples_file = "../step1/data/output/featured-samples.%d.stemmed.txt.bz2";
    std::string wiki_ads_samples_file = "../step1/data/output/ads-samples.stemmed.txt.bz2";
    std::string ads_samples_file = "../step2/data/output/yaca-ads.stemmed.txt.bz2";

    uint32_t num_non_ads_samples_files = 0;
    for (auto&& x : boost::filesystem::directory_iterator("../step1/data/output/")) {
        auto filename = x.path().filename().string();
        if (boost::starts_with(filename, "featured-samples.") and
                boost::ends_with(filename, ".stemmed.txt.bz2")) {
            num_non_ads_samples_files += 1;
        }
    }
    num_non_ads_samples_files = 2;


    file_split_t wiki_ads_file_split(wiki_ads_samples_file, ADS_LABEL, 0.0);
    file_split_t ads_file_split(ads_samples_file, ADS_LABEL, 0.0);


    std::wcout << "Counting non ads samples" << std::endl;

    std::vector<file_split_t> non_ads_file_splits;
    non_ads_file_splits.reserve(num_non_ads_samples_files);
    for (auto i = 0U; i < num_non_ads_samples_files; ++i) {
        non_ads_file_splits.emplace_back((boost::format(non_ads_samples_file) % i).str(),
                                         NON_ADS_LABEL,
                                         0.0);
    }
    std::wcout << "Featured samples counted" << std::endl;

    std::wcout << "Computing feature indices" << std::endl;

    auto features_indexes = compute_features_indexes(non_ads_file_splits,
                                                     ads_file_split);

    std::wcout << features_indexes.size() << " features" << std::endl;
    std::wcout << "Feature indexes computed. Dumping to file." << std::endl;

    output_features_index(output_directory, features_indexes);

    auto _ignored = extract_features_from_file(&wiki_ads_file_split,
                                               &features_indexes,
                                               false);


    std::wcout << "Splitting non-ads samples to test and train set" << std::endl;


    csr_matrix_t test_set, train_set;
    init_set(test_set, &file_split_t::test_part, ads_file_split,
             wiki_ads_file_split, non_ads_file_splits);
    init_set(train_set, &file_split_t::train_part, ads_file_split,
             wiki_ads_file_split, non_ads_file_splits);

    non_ads_file_splits.clear();

    test_set.indptr.push_back(test_set.indices.size());
    train_set.indptr.push_back(train_set.indices.size());

    assert(test_set.indptr.size() == test_set.labels.size() + 1 and
           test_set.labels.size() == test_set.sources.size());
    assert(train_set.indptr.size() == train_set.labels.size() + 1 and
           train_set.labels.size() == train_set.sources.size());


    std::wcout << "Dumping test and train sets to disk" << std::endl;


    DMatrixHandle xgb_test_set;
    XGDMatrixCreateFromCSR(test_set.indptr.data(), test_set.indices.data(), test_set.data.data(),
                              test_set.indptr.size(), test_set.data.size(), &xgb_test_set);
    XGDMatrixSetFloatInfo(xgb_test_set, "label", test_set.labels.data(), test_set.labels.size());
    XGDMatrixSaveBinary(xgb_test_set, (output_directory + "/test-set.dmatrix.bin").c_str(), 0);
    XGDMatrixFree(xgb_test_set);
    output_index(test_set.sources, output_directory + "/test-set.index.txt.bz2");


    DMatrixHandle xgb_train_set;
    XGDMatrixCreateFromCSR(train_set.indptr.data(), train_set.indices.data(), train_set.data.data(),
                              train_set.indptr.size(), train_set.data.size(), &xgb_train_set);
    XGDMatrixSetFloatInfo(xgb_train_set, "label", train_set.labels.data(), train_set.labels.size());
    XGDMatrixSaveBinary(xgb_train_set, (output_directory + "/train-set.dmatrix.bin").c_str(), 0);
    XGDMatrixFree(xgb_train_set);
    output_index(train_set.sources, output_directory + "/train-set.index.txt.bz2");

    return 0;
}
