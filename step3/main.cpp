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

#ifdef PROFILING
profiler bz_readline_profiler("bz2 readline");
#endif
std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> ucs2conv;
struct bzfile_wrapper {

    static std::map<std::string, std::pair<void*, std::size_t> > loaded_files;

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
#ifdef PROFILING
        profiler_guard guard(bz_readline_profiler);
#endif

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
std::map<std::string, std::pair<void*, std::size_t> > bzfile_wrapper::loaded_files;

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
/*inline*/ uint32_t _count_samples(FileObject& file) {
    uint32_t count = 0;
    while (not file.eof()) {
        std::wstring line;
        std::getline(file, line);
        if (boost::starts_with(line, L"samplesSeparator")) {
            count += 1;
            //if (count % 50000 == 0) std::wcout << "counted " << count << std::endl;
        }
    }
    return count;
}

uint32_t count_samples(const std::string& filename) {
    if (boost::ends_with(filename, ".bz2")) {
        assert(false);
//        bzfile_wrapper file(filename);
//        return _count_samples(file);
    } else {
        auto file = std::wifstream(filename);
        auto count = _count_samples(file);
        std::wcout << "counted " << count << " in " << filename.c_str() << std::endl;
        return count;
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

    /* * /
    bound_queue<std::wstring, 10000> lines_queue;
    std::thread reader_thread(bzfile_reader,
               filename,
               &lines_queue);

    std::wstring line = lines_queue.pop();
    while(line.length() > 0) {
    / * */


    /*
    bzfile_wrapper file(filename);
    std::wstring line;
    while(std::getline(file, line)) {
        assert(line.length() > 0);
    */

    std::shared_ptr<FILE> bunzip2(POPEN(("bunzip2 -c -k " + filename).c_str(), "r"), PCLOSE);
    size_t len = 2<<16;
    char* buf = (char*)malloc(len  * sizeof(char));
    std::wstring line;
    while(std::getline(&buf, &len, bunzip2.get(), line)) {
        assert(line.length() > 0 and len == 2<<16);

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
                std::wcout << "preprocess " << filename.c_str() << " " << sample_number << std::endl;

                //free(buf); return features_counts; // FIXME
            }
            word_sequence.clear();
            sample_features.clear();

        } else {
#ifdef PROFILING
            profiler_guard guard(word_profiler);
#endif

            extract_features_from_word(sample_features, line, word_sequence);
        }


        //line = lines_queue.pop();
    }
    free(buf);
    //reader_thread.join();

    return features_counts;
}

/*inline*/ void join_counts(features_dict& dest, const features_dict& source) {
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

features_indexes_t compute_features_indexes(const std::string& featured_samples_file,
                                            const std::vector<uint32_t>& featured_samples_counts,
                                            std::vector<csr_matrix_t>& featured_matrices,
                                            const std::unordered_set<uint32_t>& featured_test_samples,
                                            const std::string& ads_samples_file,
                                            csr_matrix_t& ads_matrix) {

    assert(featured_samples_counts.size() == featured_matrices.size());
    features_indexes_t temp_features_indexes;
    std::vector<std::future<features_dict>> per_file_features_counts;

    std::wcout << "Running first featured half" << std::endl;

    uint32_t shift = 0;
    for (auto i = 0U; i < std::min(4UL, featured_samples_counts.size()); ++i) {

        per_file_features_counts.push_back(
                    std::async(std::launch::async, extract_features_from_file,
                               (boost::format(featured_samples_file) % i).str(),
                               [&featured_test_samples, shift] (int i) { return featured_test_samples.count(shift + i) == 0; },
                               &featured_matrices[i],
                               &temp_features_indexes
                               )
                    );
        shift += featured_samples_counts[i];
    }


    features_dict features_counts = extract_features_from_file(ads_samples_file,
                                                               [] (int) { return true; },
                                                               &ads_matrix,
                                                               &temp_features_indexes
                                                               );


    for (auto& counts_future : per_file_features_counts) {
        assert(counts_future.valid());
        join_counts(features_counts, counts_future.get());
    }


    std::wcout << "Running second featured half" << std::endl;

    for (auto j = 4U; j < featured_samples_counts.size(); j += 4) {

        for (auto i = j; i < std::min(featured_samples_counts.size(), j + 4UL); ++i) {
            per_file_features_counts.push_back(
                        std::async(std::launch::async, extract_features_from_file,
                                   (boost::format(featured_samples_file) % i).str(),
                                   [&featured_test_samples, shift] (int i) { return featured_test_samples.count(shift + i) == 0; },
                                   &featured_matrices[i],
                                   &temp_features_indexes
                                   )
                        );
            shift += featured_samples_counts[i];
        }

        for (auto i = j; i < std::min(featured_samples_counts.size(), j + 4UL); ++i) {

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

    std::wcout << "Converting termporal features' indexes to final" << std::endl;

    for (auto& matrix : featured_matrices) {
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
    size_t len = 2048;
    char* buf = (char*)malloc(len * sizeof(char));
    std::wstring line;
    while(std::getline(&buf, &len, bunzip2.get(), line)) {
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
                //return; // FIXME
            }
            word_sequence.clear();
            sample_features.clear();
        } else {
            extract_features_from_word(sample_features, line, word_sequence);
        }
    }
    free(buf);
}

void split_featured(csr_matrix_t& ads_matrix, csr_matrix_t& wiki_ads_matrix,
                    csr_matrix_t& featured_matrix,
                    const std::unordered_set<uint32_t>& featured_test_samples,
                    uint32_t shift) {

    featured_matrix.indptr.push_back(featured_matrix.indices.size());

    for (std::size_t i = 0; i < featured_matrix.indptr.size() - 1; ++i) {

        csr_matrix_t& dst = featured_test_samples.count(shift + i) > 0? wiki_ads_matrix : ads_matrix;
        dst.indptr.push_back(dst.indices.size());
        dst.labels.push_back(0.0);

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
    //num_featured_samples_files = 2; // FIXME


    auto wiki_ads_samples_count = count_samples("../step1/data/output/ads-samples.index.txt");
    auto ads_samples_count = count_samples("../step2/data/output/yaca-ads.index.txt");


    std::vector<uint32_t> featured_samples_counts;
    uint32_t total_featured_samples_count = 0, featured_test_samples_count = 0;
    std::unordered_set<uint32_t> featured_test_samples;
    features_indexes_t features_indexes;
    csr_matrix_t ads_matrix;
    std::vector<csr_matrix_t> featured_matrices;
    featured_matrices.reserve(num_featured_samples_files);


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
                featured_matrices.emplace_back(featured_samples_counts.back());
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

        ads_matrix = csr_matrix_t(ads_samples_count + total_featured_samples_count - featured_test_samples_count);

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

        std::wcout << "recorded featured test samples" << std::endl;

        features_indexes = compute_features_indexes(featured_samples_file,
                                                    featured_samples_counts,
                                                    featured_matrices,
                                                    featured_test_samples,
                                                    ads_samples_file,
                                                    ads_matrix);

        std::wcout << "features indexes computed" << std::endl;

        {
            std::wofstream f("data/output/features-indexes.txt");
            for (auto& kv : features_indexes) {
                assert(kv.first.find(L':') == kv.first.npos);
                f << kv.first << L":" << std::to_wstring(kv.second) << std::endl;
            }
            f.close();
        }

        std::wcout << "features indexes recorded" << std::endl;

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
                featured_matrices.emplace_back(featured_samples_counts.back());
            }
            count_strings.clear();

            std::getline(featured_test_samples_file, line);
            boost::split(count_strings, line, boost::is_punct());

            total_featured_samples_count = std::stol(count_strings[0]);
            featured_test_samples_count = std::stol(count_strings[1]);

            std::getline(featured_test_samples_file, line);
            while (line.length() > 0) {
                featured_test_samples.insert(std::stol(line));
                std::getline(featured_test_samples_file, line);
            }
        }

        ads_matrix = csr_matrix_t(ads_samples_count + total_featured_samples_count - featured_test_samples_count);

        load_features_indexes(features_indexes);

        std::vector<std::future<void>> async_featured_matrices;
        for (auto i = 0U; i < featured_samples_counts.size(); ++i) {
            async_featured_matrices.push_back(
                        std::async(std::launch::async,
                                   get_matrix_from_file,
                                   (boost::format(featured_samples_file) % i).str(),
                                   &features_indexes,
                                   &featured_matrices[i]
                                   )
                        );
        }

        get_matrix_from_file(ads_samples_file, &features_indexes, &ads_matrix);

        for(auto& featured_matrix_future : async_featured_matrices) {
            featured_matrix_future.get();
        }

    }


    std::wcout << features_indexes.size() << " features" << std::endl;


    csr_matrix_t wiki_ads_matrix(wiki_ads_samples_count + featured_test_samples_count);
    get_matrix_from_file(wiki_ads_samples_file, &features_indexes, &wiki_ads_matrix);

    ads_matrix.labels.resize(ads_samples_count, 1.0);
    wiki_ads_matrix.labels.resize(wiki_ads_samples_count, 1.0);

    uint32_t shift = 0;
    for (auto i = 0U; i < featured_matrices.size(); ++i) {
        split_featured(ads_matrix, wiki_ads_matrix, featured_matrices[i], featured_test_samples, shift);
        shift += featured_samples_counts[i];
    }


    ads_matrix.indptr.push_back(ads_matrix.indices.size());
    wiki_ads_matrix.indptr.push_back(ads_matrix.indices.size());

    std::wcout << "ads_matrix: " << ads_matrix.indptr.size() << " " <<  ads_matrix.labels.size() << std::endl;
    std::wcout << "wiki_ads_matrix: " << wiki_ads_matrix.indptr.size() << " " <<  wiki_ads_matrix.labels.size() << std::endl;
    assert(ads_matrix.indptr.size() == ads_matrix.labels.size() + 1);
    assert(wiki_ads_matrix.indptr.size() == wiki_ads_matrix.labels.size() + 1);


    //std::cerr << "Exiting at " << __LINE__ << std::endl; std::exit(1);


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
