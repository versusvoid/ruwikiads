#include <fstream>
#include <codecvt>
#include <vector>
#include <algorithm>
#include <iostream>
#include <bzlib.h>
#include <stdio.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
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

void handle_bz_error(int bzerror, const char* function) {
    if (bzerror < 0) {
        std::string error_name;
        switch (bzerror) {

            case BZ_SEQUENCE_ERROR:
                error_name = "BZ_SEQUENCE_ERROR";
                break;
            case BZ_PARAM_ERROR:
                error_name = "BZ_PARAM_ERROR";
                break;
            case BZ_MEM_ERROR:
                error_name = "BZ_MEM_ERROR";
                break;
            case BZ_DATA_ERROR:
                error_name = "BZ_DATA_ERROR";
                break;
            case BZ_DATA_ERROR_MAGIC:
                error_name = "BZ_DATA_ERROR_MAGIC";
                break;
            case BZ_IO_ERROR:
                error_name = "BZ_IO_ERROR";
                break;
            case BZ_UNEXPECTED_EOF:
                error_name = "BZ_UNEXPECTED_EOF";
                break;
            case BZ_CONFIG_ERROR:
                error_name = "BZ_CONFIG_ERROR";
                break;
            default:
                error_name = "*unknown (" + std::to_string(bzerror) + ")*";
                break;
        }
        std::cerr << "bzip error: " << error_name << " in function " << function << std::endl;
        std::exit(bzerror);
    }
}


std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> ucs2conv;
struct bzfile_wrapper {

    bzfile_wrapper(const std::string& filename) {
        file = fopen(filename.c_str(), "r");
        int bzerror;
        bzfile = BZ2_bzReadOpen(&bzerror, file, 0, 0, nullptr, 0);
        handle_bz_error(bzerror, "bzReadOpen");

        capacity = 1 << 18;
        size = 0;
        buf = new char[capacity];
        start = buf;
    }

    ~bzfile_wrapper() {
        delete[] buf;
        buf = nullptr;
        start = nullptr;
        size = 0;
        capacity = 0;
        if (bzfile != nullptr) {
            int bzerror;
            BZ2_bzReadClose(&bzerror, bzfile);
            handle_bz_error(bzerror, "bzReadClose");
            bzfile = nullptr;
        }
        if (file != nullptr) {
            fclose(file);
            file = nullptr;
        }
    }
    
    bool readline(std::wstring& result) {
        auto pos = std::find(start, start + size, '\n');
        while (pos == start + size and bzfile != nullptr) {
            read_moar();
            pos = std::find(start, start + size, '\n');
        }
        if (pos == start + size and size == 0) return  false;
        
        std::size_t len = pos - start;
        result = ucs2conv.from_bytes(start, start + len);

        start += len + 1;
        size -= len + 1;
        return true;
    }

    void read_moar() {
        if ((start - buf) + size == capacity) {
            if (start != buf) {
                std::copy(start, start + size, buf);
                start = buf;
            }
            else 
            {
                std::cerr << "Doubling buffer" << std::endl;
                capacity *= 2;
                char* old_buf = buf;
                buf = new char[capacity];
                std::copy(old_buf, old_buf + capacity, buf);
                delete[] old_buf;
                start = buf;
            }
        }

        read();
    }

    void read() {
        int max_len = capacity - size - (start - buf);
        int bzerror;
        int len = BZ2_bzRead(&bzerror, bzfile, start + size, max_len);
        handle_bz_error(bzerror, "bzRead");
        if (bzerror == BZ_STREAM_END) {
            if (feof(file)) {
                BZ2_bzReadClose(&bzerror, bzfile);
                handle_bz_error(bzerror, "bzReadClose");
                bzfile = nullptr;
                fclose(file);
                file = nullptr;    
            } else {
                void* unused = nullptr;
                int nUnused = 0;
                BZ2_bzReadGetUnused(&bzerror, bzfile, &unused, &nUnused);
                handle_bz_error(bzerror, "bzReadGetUnused");
                BZ2_bzReadClose(&bzerror, bzfile);
                handle_bz_error(bzerror, "bzReadClose");
                bzfile = BZ2_bzReadOpen(&bzerror, file, 0, 0, unused, nUnused);
                handle_bz_error(bzerror, "bzReadOpen");
            }
        } 
        size += len;
    }

    bool eof() const {
        return bzfile == nullptr;
    }

    FILE* file;
    BZFILE* bzfile;
    char* buf;
    char* start;
    std::size_t capacity;
    std::size_t size;
};

namespace std {

    inline bool getline(::bzfile_wrapper& bzfile, std::wstring& out) { return bzfile.readline(out); }
    inline bool getline(char** lineptr, size_t* n, FILE* file, std::wstring& out) {
        assert(*lineptr != nullptr);
        auto len = getline(lineptr, n, file);
        //if (len == ssize_t(-1) and not feof(file)) return false;
        if (len == ssize_t(-1)) return false;
        out = ucs2conv.from_bytes(*lineptr, *lineptr + len);
        return true;
    }

}

template<typename FileObject>
inline uint32_t _count_samples(FileObject& file) {
    uint32_t count = 0;
    while (not file.eof()) {
        std::wstring line;
        std::getline(file, line);
        if (boost::starts_with(line, L"samplesSeparator")) {
            count += 1;
            if (count % 10000 == 0) std::wcout << "counted " << count << std::endl;
        }
    }
    return count;
}

uint32_t count_samples(const std::string& filename) {
    if (boost::ends_with(filename, ".bz2")) {
        bzfile_wrapper file(filename);
        return _count_samples(file);
    } else {
        auto file = std::wifstream(filename);
        return _count_samples(file);
    }
}

namespace po = boost::program_options;

void extract_features_from_file(const std::string& filename, features_dict& features_counts, std::function<bool(int)> samplePredicate) {


    uint32_t sample_number = 0;
    features_dict sample_features;
    std::vector<std::wstring> word_sequence;
    bzfile_wrapper file(filename);

    std::wstring line;
    while(std::getline(file, line)) {
        assert(line.length() > 0);
        if (boost::starts_with(line, L"samplesSeparator")) {
            extract_features_from_sequence(sample_features, word_sequence);

            if (samplePredicate(sample_number)) {
                for (auto& kv : sample_features) {
                    record_feature(features_counts, kv.first);
                }
            }

            sample_number += 1;
            if (sample_number % 100 == 0) {
                std::wcout << "preprocess " << filename.c_str() << " " << sample_number << std::endl;
            }
            word_sequence.clear();
            sample_features.clear();
        } else {
            extract_features_from_word(sample_features, line, word_sequence);
        }
    }


}

typedef std::unordered_map<std::wstring, uint32_t> features_indexes_t;
void compute_features_indexes(features_indexes_t& features_indexes,
                              const std::string& featured_samples_file, const std::string& ads_samples_file,
                              std::unordered_set<uint32_t> featured_test_samples) {
    features_dict features_counts;

    extract_features_from_file(featured_samples_file, features_counts, [&] (int i) { return not contains(featured_test_samples, i); });
    extract_features_from_file(ads_samples_file, features_counts, [] (int) { return true; });

    for (auto& kv : features_counts) {
        //if (kv.second > 1000) {
            features_indexes[kv.first] = features_indexes.size();
        //}
    }

    std::wofstream f("data/output/features-indexes.txt");
    for (auto& kv : features_indexes) {
        assert(kv.first.find(L':') == kv.first.npos);
        f << kv.first << L":" << kv.second << std::endl;
    }
    f.close();
}

void load_features_indexes(features_indexes_t& features_indexes) {
    std::wifstream f("data/output/features-indexes.txt");
    while(not f.eof()) {
        std::wstring line;
        std::getline(f, line);
        auto pos = line.find(L':');
        features_indexes[line.substr(0, pos)] = std::stol(line.substr(pos + 1));
    }
    f.close();
}

struct csr_matrix_t {

    csr_matrix_t() {
        //std::wcout << "csr_matrix_t()" << std::endl;
    }

    std::vector<float> data;
    std::vector<unsigned> indices;
    std::vector<bst_ulong> indptr;
    std::vector<float> labels;
};
csr_matrix_t get_matrix_from_file(const std::string& samples_file, uint32_t samples_count, features_indexes_t& features_indexes) {
    csr_matrix_t result;
    result.indptr.reserve(samples_count + 1);
    result.labels.reserve(samples_count);

    uint32_t sample_number = 0;
    features_dict sample_features;
    std::vector<std::wstring> word_sequence;
    bzfile_wrapper file(samples_file);

    std::wstring line;
    while(std::getline(file, line)) {
        if (boost::starts_with(line, L"samplesSeparator")) {
            extract_features_from_sequence(sample_features, word_sequence);

            result.indptr.push_back(result.indices.size());
            for (auto& kv : sample_features){
                result.indices.push_back(features_indexes[kv.first]);
                result.data.push_back(kv.second);
            }

            sample_number += 1;
            if (sample_number % 100 == 0) {
                std::wcout << samples_file.c_str() << " " << sample_number << std::endl;
            }
            word_sequence.clear();
            sample_features.clear();
        } else {
            extract_features_from_word(sample_features, line, word_sequence);
        }
    }
    return result;
}

void split_featured(csr_matrix_t& ads_matrix, csr_matrix_t& wiki_ads_matrix, csr_matrix_t& featured_matrix,
                    std::unordered_set<uint32_t>& featured_test_samples) {
    ads_matrix.labels.resize(ads_matrix.indptr.size() - 1, 1.0);
    ads_matrix.labels.resize(ads_matrix.labels.size() + (featured_matrix.indptr.size() - 1) - featured_test_samples.size(), 0.0);
    wiki_ads_matrix.labels.resize(wiki_ads_matrix.indptr.size() - 1, 1.0);
    wiki_ads_matrix.labels.resize(wiki_ads_matrix.labels.size() + featured_test_samples.size(), 0.0);

    featured_matrix.indptr.push_back(featured_matrix.indices.size());

    for (std::size_t i = 0; i < featured_matrix.indptr.size() - 1; ++i) {
        csr_matrix_t& dst = contains(featured_test_samples, i)? wiki_ads_matrix : ads_matrix;
        dst.indptr.push_back(dst.indices.size());

        auto start = featured_matrix.indptr[i];
        auto end = featured_matrix.indptr[i + 1];
        for(bst_ulong j = start; j < end; ++j) {
            dst.indices.push_back(featured_matrix.indices[j]);
            dst.data.push_back(featured_matrix.data[j]);
        }
    }

    ads_matrix.indptr.push_back(ads_matrix.indices.size());
    wiki_ads_matrix.indptr.push_back(ads_matrix.indices.size());
}

int main(int argc, char** argv)
{
    std::locale::global(std::locale(""));
    std::wcout.imbue(std::locale());
    //std::wcout.imbue(std::locale());

    std::string featured_samples_file = "data/input/featured-samples.stemmed.txt.bz2";
    std::string wiki_ads_samples_file = "data/input/wiki-ads-samples.stemmed.txt.bz2";
    std::string ads_samples_file = "data/input/ads-samples.stemmed.txt.bz2";

    std::wcout << L"Counting" << std::endl;
    auto featured_samples_count = count_samples(featured_samples_file);
    std::wcout << L"Counted first: " << featured_samples_count << std::endl;
    auto wiki_ads_samples_count = count_samples(wiki_ads_samples_file);
    std::wcout << L"Counted second" << std::endl;
    auto ads_samples_count = count_samples(ads_samples_file);
    std::wcout << L"Counted third" << std::endl;

    assert(featured_samples_count > ads_samples_count);

    auto featured_test_samples_count = uint32_t(0.4 * featured_samples_count);


    std::unordered_set<uint32_t> featured_test_samples;// # FIXME тоже надо подгружать с features_indexes
    {
        std::random_device r;
        std::mt19937 e(r());
        std::uniform_int_distribution<uint32_t> uniform_dist(0, featured_samples_count - 1);

        while (featured_test_samples.size() < featured_test_samples_count) {
            featured_test_samples.insert(uniform_dist(e));
        }
    }

    po::variables_map vm;
    {
        po::options_description desc("Options");
        desc.add_options()
                ("update,u", "update features indexes");

        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }

    std::wcout << not boost::filesystem::exists("data/output/features-indexes.txt") << " " << vm.count("update") << std::endl;
    std::exit(1);

    features_indexes_t features_indexes;
    if (not boost::filesystem::exists("data/output/features-indexes.txt") or vm.count("update") > 0) {
        compute_features_indexes(features_indexes, featured_samples_file, ads_samples_file, featured_test_samples);
    } else {
        load_features_indexes(features_indexes);
    }

    std::wcout << features_indexes.size() << " features" << std::endl;

    auto wiki_ads_matrix = get_matrix_from_file(wiki_ads_samples_file, wiki_ads_samples_count + featured_test_samples_count,
                                                features_indexes);
    auto featured_matrix = get_matrix_from_file(featured_samples_file, featured_samples_count,
                                                features_indexes);
    auto ads_matrix = get_matrix_from_file(ads_samples_file, ads_samples_count + (featured_samples_count - featured_test_samples_count),
                                           features_indexes);

    split_featured(ads_matrix, wiki_ads_matrix, featured_matrix, featured_test_samples);

    DMatrixHandle train_set, test_set;
    XGDMatrixCreateFromCSR(ads_matrix.indptr.data(), ads_matrix.indices.data(), ads_matrix.data.data(),
                              ads_matrix.indptr.size(), ads_matrix.data.size(), &train_set);
    XGDMatrixSetFloatInfo(train_set, "label", ads_matrix.labels.data(), ads_matrix.labels.size());
    XGDMatrixSaveBinary(train_set, "data/output/train-set.dmatrix.bin", 0);


    XGDMatrixCreateFromCSR(wiki_ads_matrix.indptr.data(), wiki_ads_matrix.indices.data(), wiki_ads_matrix.data.data(),
                              wiki_ads_matrix.indptr.size(), wiki_ads_matrix.data.size(), &test_set);
    XGDMatrixSetFloatInfo(test_set, "label", wiki_ads_matrix.labels.data(), wiki_ads_matrix.labels.size());
    XGDMatrixSaveBinary(test_set, "data/output/test-set.dmatrix.bin", 0);

    return 0;
}
