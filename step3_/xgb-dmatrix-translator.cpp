#include <stdio.h>
#include <unordered_map>
#include <future>
#include <codecvt>
#include <locale>
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <cassert>
#include <boost/algorithm/string.hpp>
#include <boost/multiprecision/gmp.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/filesystem.hpp>
#include <xgboost/c_api.h>
#include <sys/types.h>
#include <sys/stat.h>

#define VECTOR_LENGTH 20
#define NUM_FEATURES VECTOR_LENGTH*6
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

struct matrix_t {

    matrix_t()
    {}

    matrix_t(uint32_t size)
    {
        reserve(size);
    }

    void reserve(uint32_t size) {
        data.reserve(size * NUM_FEATURES);
        sources.reserve(size);
        labels.reserve(size);
    }

    std::vector<float> data;
    std::vector<std::wstring> sources;
    std::vector<float> labels;
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
        uint32_t num_set = 0;

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
    matrix_t train_part;
    matrix_t test_part;
};


struct moments_data_t {


    uint32_t length;
    std::array<double, VECTOR_LENGTH> sum;
    std::array<double, VECTOR_LENGTH> sum_of_squares;
    std::array<double, VECTOR_LENGTH> sum_of_cubes;
    std::array<double, VECTOR_LENGTH> sum_of_bisquares;
    std::array<double, VECTOR_LENGTH> sum_of_reciprocal;
    //std::array<boost::multiprecision::number<boost::multiprecision::gmp_float<85000> >, VECTOR_LENGTH> product;

    void clear() {
        length = 0;
        for (auto i = 0; i < VECTOR_LENGTH; ++i) {
            sum[i] = 0;
            sum_of_squares[i] = 0;
            sum_of_cubes[i] = 0;
            sum_of_bisquares[i] = 0;
            sum_of_reciprocal[i] = 0;
            //product[i] = 1.0;
        }
    }

    void update(std::array<float, VECTOR_LENGTH>& word_vector) {
        length += 1;
        return; // FIXME
        for (auto i = 0; i < VECTOR_LENGTH; ++i) {
            double v = word_vector[i];
            sum[i] += v;
            sum_of_squares[i] += v*v;
            sum_of_cubes[i] += v*v*v;
            sum_of_bisquares[i] += v*v*v*v;
            sum_of_reciprocal[i] += 1/v;
            //product[i] *= v;
        }
    }
    
    void finalize(matrix_t& matrix) {
        assert(false);
        for (auto i = 0; i < VECTOR_LENGTH; ++i) {
            float mean = float(sum[i] / length);
            float mean_2 = float(sum_of_squares[i] / length);
            float mean_3 = float(sum_of_cubes[i] / length);
            float mean_4 = float(sum_of_bisquares[i] / length);
            matrix.data.push_back(mean);
            float mean_sqr = mean * mean;
            float std = std::sqrt(float((sum_of_squares[i] - length*mean_sqr) / (length - 1)));
            matrix.data.push_back(std);
            float std_sqr = std*std;
            float root_mean_square = std::sqrt(float(sum_of_squares[i] / length));
            matrix.data.push_back(root_mean_square);
            float mean_cube = mean_sqr * mean;
            float mu3 = float((mean_3 - 3*mean_2*mean + 2*mean_cube) / length);
            float std_cube = std_sqr * std;
            matrix.data.push_back(mu3 / std_cube);
            float mean_bisquare = mean_cube * mean;
            float mu4 = float((mean_4 - 4*mean_3*mean + 6*mean_2*mean_sqr - 3 * mean_bisquare) / length);
            float std_bisquare = std_cube * std;
            matrix.data.push_back(mu4 / std_bisquare - 3.0);

            float harmonic_mean = float(length / sum_of_reciprocal[i]);
            matrix.data.push_back(harmonic_mean);

            /*
            float geometric_mean = float(boost::multiprecision::pow(product[i], 1.0 / double(length)));
            matrix.data.push_back(geometric_mean);
            matrix.indices.push_back(column_index++);
            */
        }
        clear();
    }

};


inline std::wstring::size_type word_end(const std::wstring& str, std::wstring::size_type p1) {
    auto p2 = p1 + 1;
    while ((str[p2] >= L'a' and str[p2] <= L'z') or (str[p2] >= L'а' and str[p2] <= L'я')) p2 += 1;

    return p2;
}

void process_file(file_split_t* file_split,
                  std::unordered_map<std::wstring, std::array<float, VECTOR_LENGTH> >* embeddings,
                  std::array<float, VECTOR_LENGTH>* unknown_embedding) {

    std::shared_ptr<FILE> bunzip2(popen(("bunzip2 -k -c " + file_split->filename).c_str(), "r"), pclose);
    size_t len = 4096;
    char* buf = (char*)malloc(len * sizeof(char));
    moments_data_t moments;
    moments.clear();
    std::wstring line;
    uint32_t i = 0;
    while (getline(&buf, &len, bunzip2.get(), line)) {
        auto p1 = line.find(L'{');
        if (p1 == line.npos) {
            if (moments.length > 1) {

                matrix_t* matrix = file_split->split.test(i)? &file_split->test_part : &file_split->train_part;
                moments.finalize(*matrix);
                matrix->labels.push_back(file_split->label);
                matrix->sources.push_back(file_split->sources.at(i));

                moments.clear();
            } else if (moments.length > 0) {
                moments.clear();
            }
            i += 1;
        } else {
            auto p2 = word_end(line, p1 + 1);
            auto word = line.substr(p1 + 1, p2 - p1 - 1);
            auto itr = embeddings->find(word);
            if (itr == embeddings->end()) {
                moments.update(*unknown_embedding);
            } else {
                moments.update(itr->second);
            }
        }
    }
    std::wcout << std::endl;
    free(buf);
}

std::unordered_map<std::wstring, std::array<float, VECTOR_LENGTH> > load_embeddings() {

    std::wifstream vectors("data/temp/vectors-20d-30i-15w.txt");
    std::wstring line;
    std::getline(vectors, line);
    std::unordered_map<std::wstring, std::array<float, VECTOR_LENGTH> > result;
    while (line.length() > 0) {

        auto p1 = line.find(L' ');
        assert(p1 != line.npos and p1 < line.length() - 1);
        std::wstring word = line.substr(0, p1);

        auto p2 = line.find(L' ', p1 + 1);
        assert(p2 != line.npos and p2 < line.length() - 1);

        int j = 0;
        std::array<float, VECTOR_LENGTH> vector;
        while (p1 != line.npos) {
            assert(j < VECTOR_LENGTH);
            vector[j] = std::stof(line.substr(p1 + 1, std::min(p2, line.length()) - p1 - 1));
            j += 1;
            p1 = p2;
            p2 = line.find(L' ', p1 + 1);
        }
        assert( j == VECTOR_LENGTH );

        result[word] = vector;
        std::getline(vectors, line);
    }
    return result;
}

void compute_matrices(std::vector<file_split_t>& non_ads_splits,
                      file_split_t& ads_split, file_split_t& wiki_ads_split) {

    auto embeddings = load_embeddings();
    assert(embeddings.count(L"<unk>") > 0);
    auto unknown_embedding = embeddings.at(L"<unk>");


    std::wcout << "Running first non ads half" << std::endl;

    std::vector<std::future<void> > processed_file_futures;
    for (auto i = 0U; i < std::min(4UL, non_ads_splits.size()); ++i) {

        processed_file_futures.push_back(
                    std::async(std::launch::async, process_file,
                               &non_ads_splits.at(i),
                               &embeddings,
                               &unknown_embedding
                               )
                    );
    }


    process_file(&ads_split, &embeddings, &unknown_embedding);
    process_file(&wiki_ads_split, &embeddings, &unknown_embedding);


    for (auto& processed_file_future : processed_file_futures) {
        processed_file_future.get();
    }


    std::wcout << "Running second non ads half" << std::endl;

    for (auto j = 4U; j < non_ads_splits.size(); j += 4) {

        for (auto i = j; i < std::min(non_ads_splits.size(), j + 4UL); ++i) {
            processed_file_futures.push_back(
                        std::async(std::launch::async, process_file,
                                   &non_ads_splits.at(i),
                                   &embeddings,
                                   &unknown_embedding
                                   )
                        );
        }

        for (auto i = j; i < std::min(non_ads_splits.size(), j + 4UL); ++i) {
            processed_file_futures.at(i).get();
        }
    }

}


void join_matrices(matrix_t& to, const matrix_t& from) {

    to.data.insert(to.data.end(), from.data.begin(), from.data.end());
    to.labels.insert(to.labels.end(), from.labels.begin(), from.labels.end());
    to.sources.insert(to.sources.end(), from.sources.begin(), from.sources.end());
}

void shuffle(matrix_t& matrix) {
    matrix_t result(matrix.labels.size());

    std::vector<uint32_t> permutation(matrix.labels.size(), 0);
    std::iota(permutation.begin(), permutation.end(), 0);
    std::shuffle(permutation.begin(), permutation.end(), std::mt19937(RANDOM_DEVICE()));

    for(auto index : permutation) {

        result.labels.push_back(matrix.labels.at(index));
        result.sources.push_back(matrix.sources.at(index));

        result.data.insert(result.data.end(),
                           matrix.data.begin() + index * NUM_FEATURES,
                           matrix.data.begin() + (index + 1) * NUM_FEATURES);
    }

    matrix = std::move(result);
}

void init_set(matrix_t& set, matrix_t file_split_t::*matrix,
              file_split_t& ads_file_split, file_split_t& wiki_ads_file_split,
              std::vector<file_split_t>& non_ads_file_splits) {
    uint32_t set_size = 0;

    set_size += (ads_file_split.*matrix).labels.size();
    set_size += (wiki_ads_file_split.*matrix).labels.size();

    for(auto& non_ads_file_split : non_ads_file_splits) {
        set_size += (non_ads_file_split.*matrix).labels.size();
    }

    set.reserve(set_size);


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

void output_index(matrix_t& set, const std::string& filename) {

    std::shared_ptr<FILE> bzip2(popen(("bzip2 -9 > " + filename).c_str(), "w"), pclose);
    std::array<std::string, 2> labels = { "0", "1" };

    const char* separator = "\nsamplesSeparator\n";
    std::size_t separator_len = strlen(separator);

    for(auto i = 0U; i < set.labels.size(); ++i) {
        auto& label = labels.at(int(set.labels.at(i)));
        fwrite(label.c_str(), sizeof(char), label.length(), bzip2.get());
        fwrite("\n", sizeof(char), 1, bzip2.get());
        auto buf = ucs2conv.to_bytes(set.sources.at(i));
        fwrite(buf.data(), sizeof(char), buf.length(), bzip2.get());
        fwrite(separator, sizeof(char), separator_len, bzip2.get());
    }
}

void output_features_index(const std::string& filename)
{
    std::wofstream f(filename);
    auto j = 0U;
    for (auto i = 0U; i < VECTOR_LENGTH; ++i) {
        f << "mean-" << std::to_wstring(i) << ':' << j << std::endl;
        j += 1;
        f << "std-" << std::to_wstring(i) << ':' << j << std::endl;
        j += 1;
        f << "root-mean-square-" << std::to_wstring(i) << ':' << j << std::endl;
        j += 1;
        f << "skewness-" << std::to_wstring(i) << ':' << j << std::endl;
        j += 1;
        f << "kurtosis-" << std::to_wstring(i) << ':' << j << std::endl;
        j += 1;
        f << "harmonic_mean-" << std::to_wstring(i) << ':' << j << std::endl;
        j += 1;
    }
    assert(j == NUM_FEATURES);
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
    std::wcout << "Non ads samples counted" << std::endl;
    std::wcout << "Dumping feature indices to file." << std::endl;

    output_features_index(output_directory + "/features-indexes.txt");

    std::wcout << "Computing file matrices" << std::endl;

    compute_matrices(non_ads_file_splits, ads_file_split, wiki_ads_file_split);

    std::wcout << "Splitting non-ads samples to test and train set" << std::endl;


    matrix_t test_set, train_set;
    init_set(test_set, &file_split_t::test_part, ads_file_split,
             wiki_ads_file_split, non_ads_file_splits);
    init_set(train_set, &file_split_t::train_part, ads_file_split,
             wiki_ads_file_split, non_ads_file_splits);

    non_ads_file_splits.clear();

    assert(test_set.labels.size() == test_set.sources.size() and
           test_set.data.size() % NUM_FEATURES == 0 and
           test_set.data.size() / NUM_FEATURES == test_set.labels.size());
    assert(train_set.labels.size() == train_set.sources.size() and
           train_set.data.size() % NUM_FEATURES == 0 and
           train_set.data.size() / NUM_FEATURES == train_set.labels.size() );


    std::wcout << "Dumping test and train sets to disk" << std::endl;


    DMatrixHandle xgb_test_set;
    XGDMatrixCreateFromMat(test_set.data.data(),
                           test_set.data.size() / NUM_FEATURES, NUM_FEATURES,
                           NAN, &xgb_test_set);
    XGDMatrixSetFloatInfo(xgb_test_set, "label", test_set.labels.data(), test_set.labels.size());
    XGDMatrixSaveBinary(xgb_test_set, (output_directory + "/test-set.dmatrix.bin").c_str(), 0);
    XGDMatrixFree(xgb_test_set);
    output_index(test_set, output_directory + "/test-set.index.txt.bz2");


    DMatrixHandle xgb_train_set;
    XGDMatrixCreateFromMat(train_set.data.data(),
                           train_set.data.size() / NUM_FEATURES, NUM_FEATURES,
                           NAN, &xgb_train_set);
    XGDMatrixSetFloatInfo(xgb_train_set, "label", train_set.labels.data(), train_set.labels.size());
    XGDMatrixSaveBinary(xgb_train_set, (output_directory + "/train-set.dmatrix.bin").c_str(), 0);
    XGDMatrixFree(xgb_train_set);
    output_index(train_set, output_directory + "/train-set.index.txt.bz2");

    return 0;
}
