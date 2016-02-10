#include <fstream>
#include <codecvt>
#include <future>
#include <vector>
#include <algorithm>
#include <iostream>
#include <bzlib.h>
#include <stdio.h>
#include <chrono>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <xgboost/c_api.h>
#include "features.hpp"
#include "blocking_queue.hpp"

#include <stdio.h>  // for popen, pclose
#if defined(_MSC_VER)
# define POPEN _popen
# define PCLOSE _pclose
#else
# define POPEN popen
# define PCLOSE pclose
#endif

void handle_bz_error(int bzerror, const char* function, const std::string& info = "") {
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
        std::wcerr << "bzip error: " << error_name.c_str() << " in function " << function << std::endl;
        if (info.length() > 0)
            std::wcerr << info.c_str() << std::endl;
        std::exit(bzerror);
    }
}


profiler bz_readline_profiler("bz2 readline");
std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> ucs2conv;
struct bzfile_wrapper {

    bzfile_wrapper(const std::string& filename) {
        assert(boost::filesystem::exists(filename.c_str()));

        file = fopen(filename.c_str(), "r");
        int bzerror;
        bzfile = BZ2_bzReadOpen(&bzerror, file, 0, 0, nullptr, 0);
        handle_bz_error(bzerror, "bzReadOpen", filename);

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
        profiler_guard guard(bz_readline_profiler);

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

void bzfile_reader(const std::string& filename, bound_queue<std::wstring, 10000>* lines_queue) {
    bzfile_wrapper file(filename);

    std::wstring line;
    while(std::getline(file, line)) {
        lines_queue->push(line);
    }
    lines_queue->push(L"");
}

features_dict extract_features_from_file(const std::string& filename, std::function<bool(int)> samplePredicate) {

    features_dict features_counts;

    uint32_t sample_number = 0;
    features_dict sample_features;
    std::vector<std::wstring> word_sequence;
    /*
    bound_queue<std::wstring, 10000> lines_queue;
    std::thread reader_thread(bzfile_reader,
               filename,
               &lines_queue);

    std::wstring line = lines_queue.pop();
    while(line.length() > 0) {
    */
    bzfile_wrapper file(filename);

    profiler end_profiler(filename + " sequence");
    profiler word_profiler(filename + " word");


    std::wstring line;
    while(std::getline(file, line)) {
        assert(line.length() > 0);
        if (boost::starts_with(line, L"samplesSeparator")) {
            profiler_guard guard(end_profiler);

            extract_features_from_sequence(sample_features, word_sequence);

            if (samplePredicate(sample_number)) {
                for (auto& kv : sample_features) {
                    record_feature(features_counts, kv.first);
                }
            }

            sample_number += 1;
            if (sample_number % 10000 == 0) {
                std::wcout << "preprocess " << filename.c_str() << " " << sample_number << std::endl;
                //if (sample_number == 50000)  return features_counts; // FIXME
            }
            word_sequence.clear();
            sample_features.clear();
        } else {
            profiler_guard guard(word_profiler);

            extract_features_from_word(sample_features, line, word_sequence);
        }

        //line = lines_queue.pop();
    }
    //reader_thread.join();

    return features_counts;
}

inline void join_counts(features_dict& dest, const features_dict& source) {
    for (auto& kv : source) {
        record_feature(dest, kv.first, kv.second);
    }
}

typedef std::unordered_map<std::wstring, uint32_t> features_indexes_t;
features_indexes_t compute_features_indexes(const std::string& featured_samples_file,
                                            std::vector<uint32_t> featured_samples_counts,
                                            std::unordered_set<uint32_t> featured_test_samples,
                                            const std::string& ads_samples_file) {
    std::vector<std::future<features_dict>> per_file_features_counts;

    /*
    uint32_t shift = 0;
    for (auto i = 0U; i < featured_samples_counts.size(); ++i) {
        per_file_features_counts.push_back(
                    std::async(std::launch::async, extract_features_from_file,
                               (boost::format(featured_samples_file) % i).str(),
                               [&featured_test_samples, shift] (int i) { return not contains(featured_test_samples, shift + i); })
                    );
        shift += featured_samples_counts[i];
    }
    */

    features_dict features_counts = extract_features_from_file(ads_samples_file, [] (int) { return true; });

    for (auto& counts_future : per_file_features_counts) {
        join_counts(features_counts, counts_future.get());
    }

    features_indexes_t features_indexes;
    for (auto& kv : features_counts) {
        //if (kv.second > 1000) {
            features_indexes[kv.first] = features_indexes.size();
        //}
    }

    return features_indexes;
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
    ~csr_matrix_t() {
        std::wcout << "~csr_matrix_t()" << std::endl;
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
            if (sample_number % 10000 == 0) {
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
                    std::unordered_set<uint32_t>& featured_test_samples, uint32_t shift) {

    featured_matrix.indptr.push_back(featured_matrix.indices.size());

    for (std::size_t i = 0; i < featured_matrix.indptr.size() - 1; ++i) {
        csr_matrix_t& dst = contains(featured_test_samples, shift + i)? wiki_ads_matrix : ads_matrix;
        dst.indptr.push_back(dst.indices.size());

        auto start = featured_matrix.indptr[i];
        auto end = featured_matrix.indptr[i + 1];
        for(bst_ulong j = start; j < end; ++j) {
            dst.indices.push_back(featured_matrix.indices[j]);
            dst.data.push_back(featured_matrix.data[j]);
        }
    }
}

namespace po = boost::program_options;
int main(int argc, char** argv)
{
    std::locale::global(std::locale(std::locale(""), "C", std::locale::numeric));
    std::wcout.imbue(std::locale());
    //std::cout.imbue(std::locale());

    std::string featured_samples_file = "../step1/data/output/featured-samples.%d.stemmed.txt.bz2";
    std::string featured_index_file = "../step1/data/output/featured-samples.%d.index.txt";
    std::string wiki_ads_samples_file = "../step1/data/output/ads-samples.stemmed.txt.bz2";
    std::string ads_samples_file = "../step2/data/output/yaca-ads.stemmed.txt.bz2";

    uint32_t num_featured_samples_files = 0;
    for (auto&& x : boost::filesystem::directory_iterator("../step1/data/output/")) {
        auto filename = x.path().filename().string();
        if (boost::starts_with(filename, "featured-samples.") and
                boost::ends_with(filename, ".stemmed.txt.bz2")) {
            num_featured_samples_files += 1;
        }
    }
    num_featured_samples_files = 1; // FIXME


    auto wiki_ads_samples_count = count_samples("../step1/data/output/ads-samples.index.txt");
    auto ads_samples_count = count_samples("../step2/data/output/yaca-ads.index.txt");


    std::vector<uint32_t> featured_samples_counts;
    uint32_t total_featured_samples_count = 0, featured_test_samples_count = 0;
    std::unordered_set<uint32_t> featured_test_samples;
    features_indexes_t features_indexes;


    po::variables_map vm;
    {
        po::options_description desc("Options");
        desc.add_options()
                ("update,u", "update features indexes");

        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }
    if (not boost::filesystem::exists("data/output/features-indexes.txt") or vm.count("update") > 0) {
        std::wcout << "Updating features indexes" << std::endl;
        {
            std::vector<std::future<uint32_t>> counts;
            for (auto i = 0U; i < num_featured_samples_files; ++i) {
                counts.push_back(std::async(std::launch::async, count_samples, (boost::format(featured_index_file) % i).str()));
                //counts.push_back(std::async(std::launch::async, count_samples, (boost::format(featured_samples_file) % i).str()));
            }
            for (auto&& count : counts) {
                featured_samples_counts.push_back(count.get());
                total_featured_samples_count += featured_samples_counts.back();
            }
        }
        std::wcout << "Featured samples counted: " << total_featured_samples_count << std::endl;
        //std::exit(1);
        assert(total_featured_samples_count > ads_samples_count);

        featured_test_samples_count = uint32_t(0.4 * total_featured_samples_count);
        {
            std::random_device r;
            std::mt19937 e(r());
            std::uniform_int_distribution<uint32_t> uniform_dist(0, total_featured_samples_count - 1);

            while (featured_test_samples.size() < featured_test_samples_count) {
                featured_test_samples.insert(uniform_dist(e));
            }
        }

        {
            std::ofstream featured_test_samples_file("data/output/featured-test-samples.txt");
            for(auto i = 0U; i < featured_samples_counts.size(); ++i) {
                featured_test_samples_file << std::to_string(featured_samples_counts[i]);
                if (i < featured_samples_counts.size() - 1)
                    featured_test_samples_file << ',';
            }
            featured_test_samples_file << std::endl;
            featured_test_samples_file << std::to_string(total_featured_samples_count) << ',' << std::to_string(featured_test_samples_count) << std::endl;

            for (auto& k : featured_test_samples) {
                featured_test_samples_file << std::to_string(k) << std::endl;
            }
            featured_test_samples_file.close();
        }

        features_indexes = compute_features_indexes(featured_samples_file, featured_samples_counts,
                                                    featured_test_samples,
                                                    ads_samples_file);

        std::exit(1);

        std::wofstream f("data/output/features-indexes.txt");
        for (auto& kv : features_indexes) {
            assert(kv.first.find(L':') == kv.first.npos);
            f << kv.first << L":" << std::to_wstring(kv.second) << std::endl;
        }
        f.close();

    } else {

        std::wcout << "Loading features indexes" << std::endl;
        {
            std::ifstream featured_test_samples_file("data/output/featured-test-samples.txt");

            std::string line;
            std::getline(featured_test_samples_file, line);

            std::vector<std::string> count_strings;            
            boost::split(count_strings, line, boost::is_punct());
            for (auto& count : count_strings) {
                featured_samples_counts.push_back(std::stol(count));
            }
            count_strings.clear();

            std::getline(featured_test_samples_file, line);
            boost::split(count_strings, line, boost::is_punct());

            total_featured_samples_count = std::stol(count_strings[0]);
            featured_test_samples_count = std::stol(count_strings[1]);

            while (not featured_test_samples_file.eof()) {
                std::getline(featured_test_samples_file, line);
                featured_test_samples.insert(std::stol(line));
            }
        }

        load_features_indexes(features_indexes);
    }

    std::wcout << features_indexes.size() << " features" << std::endl;
    std::exit(1);

    std::vector<std::future<csr_matrix_t>> featured_matrices;
    for (auto i = 0U; i < featured_samples_counts.size(); ++i) {
        featured_matrices.push_back(
                    std::async(std::launch::async,
                               get_matrix_from_file,
                               (boost::format(featured_samples_file) % i).str(),
                               featured_samples_file[i],
                               std::ref(features_indexes))
                    );
    }


    auto wiki_ads_matrix = get_matrix_from_file(wiki_ads_samples_file, wiki_ads_samples_count + featured_test_samples_count,
                                                features_indexes);
    auto ads_matrix = get_matrix_from_file(ads_samples_file, ads_samples_count + (total_featured_samples_count - featured_test_samples_count),
                                           features_indexes);

    ads_matrix.labels.resize(ads_matrix.indptr.size() - 1, 1.0);
    ads_matrix.labels.resize(ads_matrix.labels.size() + total_featured_samples_count - featured_test_samples_count, 0.0);
    wiki_ads_matrix.labels.resize(wiki_ads_matrix.indptr.size() - 1, 1.0);
    wiki_ads_matrix.labels.resize(wiki_ads_matrix.labels.size() + featured_test_samples_count, 0.0);

    uint32_t shift = 0;
    for (auto i = 0U; i < featured_matrices.size(); ++i) {
        auto featured_matrix = featured_matrices[i].get();
        split_featured(ads_matrix, wiki_ads_matrix, featured_matrix, featured_test_samples, shift);
        shift += featured_samples_counts[i];
    }

    ads_matrix.indptr.push_back(ads_matrix.indices.size());
    wiki_ads_matrix.indptr.push_back(ads_matrix.indices.size());


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
