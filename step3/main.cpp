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
#include <xgboost/c_api.h>

#include "features.hpp"

#include <stdio.h>  // for popen, pclose
#if defined(_MSC_VER)
# define POPEN _popen
# define PCLOSE _pclose
#else
# define POPEN popen
# define PCLOSE pclose
#endif

std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> ucs2conv;

inline bool getline(char** lineptr, size_t* n, FILE* file, std::wstring& out) {
    assert(*lineptr != nullptr);
    auto len = getline(lineptr, n, file);
    //if (len == ssize_t(-1) and not feof(file)) return false;
    if (len == ssize_t(-1)) return false;
    out = ucs2conv.from_bytes(*lineptr, *lineptr + len);
    return true;
}

uint32_t count_samples(const std::string& filename) {

    assert(boost::ends_with(filename, ".index.txt"));

    std::wifstream file(filename);
    uint32_t count = 0;
    while (not file.eof()) {
        std::wstring line;
        std::getline(file, line);
        if (boost::starts_with(line, L"samplesSeparator")) {
            count += 1;
        }
    }

    std::wcout << "counted " << count << " in " << filename.c_str() << std::endl;
    return count;
}

struct csr_matrix_t {

    csr_matrix_t() {
        std::wcout << "csr_matrix_t()" << std::endl;
    }
    csr_matrix_t(uint32_t samples_count) {
        std::wcout << "csr_matrix_t(uint32_t)" << std::endl;
        indptr.reserve(samples_count + 1);
        labels.reserve(samples_count);
    }

    csr_matrix_t(const csr_matrix_t& other)
        : data(other.data)
        , indices(other.indices)
        , indptr(other.indptr)
        , labels(other.labels)
    {
        std::wcout << "csr_matrix_t(const csr_matrix_t&)" << std::endl;
    }

    csr_matrix_t(csr_matrix_t&& other)
        : data(std::move(other.data))
        , indices(std::move(other.indices))
        , indptr(std::move(other.indptr))
        , labels(std::move(other.labels))
    {
        std::wcout << "csr_matrix_t(csr_matrix_t&&)" << std::endl;
    }

    csr_matrix_t& operator =(const csr_matrix_t& other) {
        std::wcout << "csr_matrix_t& operator =(const csr_matrix_t&)" << std::endl;
        data = other.data;
        indices = other.indices;
        indptr = other.indptr;
        labels = other.labels;

        return *this;
    }

    csr_matrix_t& operator =(csr_matrix_t&& other) {
        std::wcout << "csr_matrix_t& operator =(csr_matrix_t&&)" << std::endl;
        data = std::move(other.data);
        indices = std::move(other.indices);
        indptr = std::move(other.indptr);
        labels = std::move(other.labels);

        return *this;
    }

    ~csr_matrix_t() {
        std::wcout << "~csr_matrix_t()" << std::endl;
    }

    std::vector<float> data;
    std::vector<unsigned> indices;
    std::vector<bst_ulong> indptr;
    std::vector<float> labels;
};

typedef std::unordered_map<std::wstring, uint32_t> features_indexes_t;
std::mutex temp_features_indexes_mutex;
uint32_t get_temp_feature_index(const std::wstring& feature, features_indexes_t& temp_features_indexes) {

    std::lock_guard<std::mutex> lock(temp_features_indexes_mutex);

    auto pos = temp_features_indexes.find(feature);
    if (pos == temp_features_indexes.end()) {
        temp_features_indexes.insert(std::make_pair(feature, temp_features_indexes.size()));
        return temp_features_indexes.size() - 1;
    } else {
        return pos->second;
    }
}

features_dict extract_features_from_file(const std::string& filename,
                                         std::function<bool(int)> samplePredicate,
                                         csr_matrix_t* matrix,
                                         features_indexes_t* temp_features_indexes) {

    features_dict features_counts;

    uint32_t sample_number = 0;
    features_dict sample_features;
    std::vector<std::wstring> word_sequence;


#ifdef PROFILING
    profiler end_profiler(filename + " sequence");
    profiler word_profiler(filename + " word");
#endif

    std::shared_ptr<FILE> bunzip2(POPEN(("bunzip2 -c -k " + filename).c_str(), "r"), PCLOSE);
    size_t len = 4096;
    char* buf = (char*)malloc(len  * sizeof(char));
    std::wstring line;
    while(getline(&buf, &len, bunzip2.get(), line)) {
        assert(line.length() > 0);

        if (boost::starts_with(line, L"samplesSeparator")) {
#ifdef PROFILING
            profiler_guard guard(end_profiler);
#endif

            extract_features_from_sequence(sample_features, word_sequence);

            if (samplePredicate(sample_number)) {
                for (auto& kv : sample_features) {
                    record_feature(features_counts, kv.first);
                }
            }

            matrix->indptr.push_back(matrix->indices.size());
            for (auto& kv : sample_features) {
                uint32_t feature_index = get_temp_feature_index(kv.first, *temp_features_indexes);
                matrix->indices.push_back(feature_index);
                matrix->data.push_back(kv.second);
            }

            sample_number += 1;
            if (sample_number % 30000 == 0) {
                std::wcout << "feature extraction " << filename.c_str() << " " << sample_number << std::endl;
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

    return features_counts;
}

inline void join_counts(features_dict& dest, const features_dict& source) {
    for (auto& kv : source) {
        record_feature(dest, kv.first, kv.second);
    }
}

csr_matrix_t convert_features(csr_matrix_t& matrix,
                              const std::vector<int32_t>& features_transform) {

    csr_matrix_t result(matrix.labels.capacity());
    result.indices.reserve(matrix.indices.size());
    result.data.reserve(matrix.data.size());

    matrix.indptr.push_back(matrix.indices.size());

    for (auto i = 1U; i < matrix.indptr.size(); ++i) {
        result.indptr.push_back(result.indices.size());

        for (auto j = matrix.indptr.at(i - 1); j < matrix.indptr.at(i); ++j) {

            int32_t new_index = features_transform.at(matrix.indices.at(j));
            if (new_index != -1) {
                result.indices.push_back(new_index);
                result.data.push_back(matrix.data.at(j));
            }
        }
    }

    return result;
}

features_indexes_t compute_features_indexes(const std::string& non_ads_samples_file,
                                            const std::vector<uint32_t>& non_ads_samples_counts,
                                            std::vector<csr_matrix_t>& non_ads_matrices,
                                            const std::unordered_set<uint32_t>& non_ads_test_samples,
                                            const std::string& ads_samples_file,
                                            csr_matrix_t& ads_matrix) {

    assert(non_ads_samples_counts.size() == non_ads_matrices.size());
    features_indexes_t temp_features_indexes;
    std::vector<std::future<features_dict>> per_file_features_counts;

    std::wcout << "Running first non ads half" << std::endl;

    uint32_t shift = 0;
    for (auto i = 0U; i < std::min(4UL, non_ads_samples_counts.size()); ++i) {

        per_file_features_counts.push_back(
                    std::async(std::launch::async, extract_features_from_file,
                               (boost::format(non_ads_samples_file) % i).str(),
                               [&non_ads_test_samples, shift] (int i) { return non_ads_test_samples.count(shift + i) == 0; },
                               &non_ads_matrices[i],
                               &temp_features_indexes
                               )
                    );
        shift += non_ads_samples_counts[i];
    }


    features_dict features_counts = extract_features_from_file(ads_samples_file,
                                                               [] (int) { return true; },
                                                               &ads_matrix,
                                                               &temp_features_indexes
                                                               );


    for (auto& counts_future : per_file_features_counts) {
        join_counts(features_counts, counts_future.get());
    }


    std::wcout << "Running second non ads half" << std::endl;

    for (auto j = 4U; j < non_ads_samples_counts.size(); j += 4) {

        for (auto i = j; i < std::min(non_ads_samples_counts.size(), j + 4UL); ++i) {
            per_file_features_counts.push_back(
                        std::async(std::launch::async, extract_features_from_file,
                                   (boost::format(non_ads_samples_file) % i).str(),
                                   [&non_ads_test_samples, shift] (int i) { return non_ads_test_samples.count(shift + i) == 0; },
                                   &non_ads_matrices[i],
                                   &temp_features_indexes
                                   )
                        );
            shift += non_ads_samples_counts[i];
        }

        for (auto i = j; i < std::min(non_ads_samples_counts.size(), j + 4UL); ++i) {

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

    for (auto& matrix : non_ads_matrices) {
        matrix = convert_features(matrix, features_transform);
    }

    ads_matrix = convert_features(ads_matrix, features_transform);


    return features_indexes;
}

void load_features_indexes(features_indexes_t& features_indexes) {
    std::wifstream f("data/output/features-indexes.txt");
    std::wstring line;
    std::getline(f, line);
    while(line.length() > 0) {
        auto pos = line.find(L':');
        features_indexes[line.substr(0, pos)] = std::stol(line.substr(pos + 1));
        std::getline(f, line);
    }
    f.close();
}


void get_matrix_from_file(const std::string& samples_file,
                          const features_indexes_t* features_indexes,
                          csr_matrix_t* result) {

    uint32_t sample_number = 0;
    features_dict sample_features;
    std::vector<std::wstring> word_sequence;

    std::shared_ptr<FILE> bunzip2(POPEN(("bunzip2 -c -k " + samples_file).c_str(), "r"), PCLOSE);
    size_t len = 4096;
    char* buf = (char*)malloc(len * sizeof(char));
    std::wstring line;
    while(getline(&buf, &len, bunzip2.get(), line)) {
        assert(line.length() > 0);

        if (boost::starts_with(line, L"samplesSeparator")) {
            extract_features_from_sequence(sample_features, word_sequence);

            result->indptr.push_back(result->indices.size());
            for (auto& kv : sample_features){
                auto itr = features_indexes->find(kv.first);
                if (itr != features_indexes->end()) {
                    result->indices.push_back(itr->second);
                    result->data.push_back(kv.second);
                }
            }

            sample_number += 1;
            if (sample_number % 30000 == 0) {
                std::wcout << samples_file.c_str() << " " << sample_number << std::endl;
            }
            word_sequence.clear();
            sample_features.clear();
        } else {
            extract_features_from_word(sample_features, line, word_sequence);
        }
    }
    free(buf);
}

void split_non_ads(csr_matrix_t& ads_matrix, csr_matrix_t& wiki_ads_matrix,
                    csr_matrix_t& non_ads_matrix,
                    const std::unordered_set<uint32_t>& non_ads_test_samples,
                    uint32_t shift) {

    non_ads_matrix.indptr.push_back(non_ads_matrix.indices.size());

    for (std::size_t i = 0; i < non_ads_matrix.indptr.size() - 1; ++i) {

        csr_matrix_t& dst = non_ads_test_samples.count(shift + i) > 0? wiki_ads_matrix : ads_matrix;
        dst.indptr.push_back(dst.indices.size());
        dst.labels.push_back(0.0);

        auto start = non_ads_matrix.indptr.at(i);
        auto end = non_ads_matrix.indptr.at(i + 1);
        for(bst_ulong j = start; j < end; ++j) {
            dst.indices.push_back(non_ads_matrix.indices.at(j));
            dst.data.push_back(non_ads_matrix.data.at(j));
        }
    }
}

int main(int argc, char** argv)
{
    std::locale::global(std::locale(std::locale(""), "C", std::locale::numeric));
    std::wcout.imbue(std::locale());

    std::string non_ads_samples_file = "../step1/data/output/featured-samples.%d.stemmed.txt.bz2";
    std::string non_ads_index_file = "../step1/data/output/featured-samples.%d.index.txt";
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


    auto wiki_ads_samples_count = count_samples("../step1/data/output/ads-samples.index.txt");
    auto ads_samples_count = count_samples("../step2/data/output/yaca-ads.index.txt");





    std::wcout << "Counting non ads samples" << std::endl;

    std::vector<uint32_t> non_ads_samples_counts;
    uint32_t total_non_ads_samples_count = 0;

    std::vector<csr_matrix_t> non_ads_matrices;
    non_ads_matrices.reserve(num_non_ads_samples_files);
    {
        std::vector<std::future<uint32_t>> counts;
        for (auto i = 0U; i < num_non_ads_samples_files; ++i) {
            counts.push_back(std::async(std::launch::async, count_samples, (boost::format(non_ads_index_file) % i).str()));
            //counts.push_back(std::async(std::launch::async, count_samples, (boost::format(non_ads_samples_file) % i).str()));
        }
        for (auto&& count : counts) {
            non_ads_samples_counts.push_back(count.get());
            total_non_ads_samples_count += non_ads_samples_counts.back();
            non_ads_matrices.emplace_back(non_ads_samples_counts.back());
        }
    }
    std::wcout << "Featured samples counted: " << total_non_ads_samples_count << std::endl;
    assert(total_non_ads_samples_count > ads_samples_count);


    std::wcout << "Generating test samples indices" << std::endl;
    uint32_t non_ads_test_samples_count = uint32_t(0.4 * total_non_ads_samples_count);
    std::unordered_set<uint32_t> non_ads_test_samples;
    {
        std::random_device r;
        std::mt19937 e(r());
        std::uniform_int_distribution<uint32_t> uniform_dist(0, total_non_ads_samples_count - 1);

        while (non_ads_test_samples.size() < non_ads_test_samples_count) {
            non_ads_test_samples.insert(uniform_dist(e));
        }
    }


    csr_matrix_t ads_matrix(ads_samples_count + total_non_ads_samples_count - non_ads_test_samples_count);
    std::wcout << "Dumping test samples indices to file" << std::endl;
    {
        std::ofstream non_ads_test_samples_file("data/output/featured-test-samples.txt");

        for (auto& k : non_ads_test_samples) {
            non_ads_test_samples_file << std::to_string(k) << std::endl;
        }
        non_ads_test_samples_file.close();
    }

    std::wcout << "Computing feature indices" << std::endl;

    auto features_indexes = compute_features_indexes(non_ads_samples_file,
                                                non_ads_samples_counts,
                                                non_ads_matrices,
                                                non_ads_test_samples,
                                                ads_samples_file,
                                                ads_matrix);

    std::wcout << "Feature indexes computed. Dumping to file." << std::endl;

    {
        std::wofstream f("data/output/features-indexes.txt");
        for (auto& kv : features_indexes) {
            assert(kv.first.find(L':') == kv.first.npos);
            f << kv.first << L":" << std::to_wstring(kv.second) << std::endl;
        }
        f.close();
    }


    csr_matrix_t wiki_ads_matrix(wiki_ads_samples_count + non_ads_test_samples_count);
    get_matrix_from_file(wiki_ads_samples_file, &features_indexes, &wiki_ads_matrix);

    ads_matrix.labels.resize(ads_samples_count, 1.0);
    wiki_ads_matrix.labels.resize(wiki_ads_samples_count, 1.0);

    std::wcout << "Splitting non-ads samples to test and train set" << std::endl;

    uint32_t shift = 0;
    for (auto i = 0U; i < non_ads_matrices.size(); ++i) {
        split_non_ads(ads_matrix, wiki_ads_matrix, non_ads_matrices[i], non_ads_test_samples, shift);
        shift += non_ads_samples_counts[i];
    }


    ads_matrix.indptr.push_back(ads_matrix.indices.size());
    wiki_ads_matrix.indptr.push_back(wiki_ads_matrix.indices.size());

    assert(ads_matrix.indptr.size() == ads_matrix.labels.size() + 1);
    assert(wiki_ads_matrix.indptr.size() == wiki_ads_matrix.labels.size() + 1);


    std::wcout << "Dumping test and train sets to disk" << std::endl;


    DMatrixHandle train_set;
    XGDMatrixCreateFromCSR(ads_matrix.indptr.data(), ads_matrix.indices.data(), ads_matrix.data.data(),
                              ads_matrix.indptr.size(), ads_matrix.data.size(), &train_set);
    XGDMatrixSetFloatInfo(train_set, "label", ads_matrix.labels.data(), ads_matrix.labels.size());
    XGDMatrixSaveBinary(train_set, "data/output/train-set.dmatrix.bin", 0);
    XGDMatrixFree(train_set);

    DMatrixHandle test_set;
    XGDMatrixCreateFromCSR(wiki_ads_matrix.indptr.data(), wiki_ads_matrix.indices.data(), wiki_ads_matrix.data.data(),
                              wiki_ads_matrix.indptr.size(), wiki_ads_matrix.data.size(), &test_set);
    XGDMatrixSetFloatInfo(test_set, "label", wiki_ads_matrix.labels.data(), wiki_ads_matrix.labels.size());
    XGDMatrixSaveBinary(test_set, "data/output/test-set.dmatrix.bin", 0);
    XGDMatrixFree(test_set);

    return 0;
}
