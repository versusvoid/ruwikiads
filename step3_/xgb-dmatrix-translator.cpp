#include <stdio.h>
#include <unordered_map>
#include <codecvt>
#include <locale>
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <cassert>
#include <boost/algorithm/string.hpp>
#include <boost/multiprecision/gmp.hpp>
#include <xgboost/c_api.h>

#define VECTOR_LENGTH 20

std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> ucs2conv;
inline bool getline(char** lineptr, size_t* n, FILE* file, std::wstring& out) {
    auto len = getline(lineptr, n, file);
    //if (len == ssize_t(-1) and not feof(file)) return false;
    if (len == ssize_t(-1)) return false;
    out = ucs2conv.from_bytes(*lineptr, *lineptr + len);
    return true;
}

inline std::wstring::size_type word_end(const std::wstring& str, std::wstring::size_type p1) {
    auto p2 = p1 + 1;
    while ((str[p2] >= L'a' and str[p2] <= L'z') or (str[p2] >= L'а' and str[p2] <= L'я')) p2 += 1;

    return p2;
}

struct csr_matrix_t {

    std::vector<float> data;
    std::vector<unsigned> indices;
    std::vector<bst_ulong> indptr;
    std::vector<float> labels;

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
    
    void finalize(csr_matrix_t& matrix) {
        assert(false);
        matrix.indptr.push_back(matrix.indices.size());
        uint32_t column_index = 0;
        for (auto i = 0; i < VECTOR_LENGTH; ++i) {
            float mean = float(sum[i] / length);
            float mean_2 = float(sum_of_squares[i] / length);
            float mean_3 = float(sum_of_cubes[i] / length);
            float mean_4 = float(sum_of_bisquares[i] / length);
            matrix.data.push_back(mean);
            matrix.indices.push_back(column_index++);
            float mean_sqr = mean * mean;
            float std = std::sqrt(float((sum_of_squares[i] - length*mean_sqr) / (length - 1)));
            matrix.data.push_back(std);
            matrix.indices.push_back(column_index++);
            float std_sqr = std*std;
            float root_mean_square = std::sqrt(float(sum_of_squares[i] / length));
            matrix.data.push_back(root_mean_square);
            matrix.indices.push_back(column_index++);
            float mean_cube = mean_sqr * mean;
            float mu3 = float((mean_3 - 3*mean_2*mean + 2*mean_cube) / length);
            float std_cube = std_sqr * std;
            matrix.data.push_back(mu3 / std_cube);
            matrix.indices.push_back(column_index++);
            float mean_bisquare = mean_cube * mean;
            float mu4 = float((mean_4 - 4*mean_3*mean + 6*mean_2*mean_sqr - 3 * mean_bisquare) / length);
            float std_bisquare = std_cube * std;
            matrix.data.push_back(mu4 / std_bisquare - 3.0);
            matrix.indices.push_back(column_index++);

            float harmonic_mean = float(length / sum_of_reciprocal[i]);
            matrix.data.push_back(harmonic_mean);
            matrix.indices.push_back(column_index++);

            /*
            float geometric_mean = float(boost::multiprecision::pow(product[i], 1.0 / double(length)));
            matrix.data.push_back(geometric_mean);
            matrix.indices.push_back(column_index++);
            */
        }
        clear();
    }

};

void process_file(const std::string& filename, uint8_t label,  
                  std::unordered_map<std::wstring, std::array<float, VECTOR_LENGTH> >& embeddings, 
                  std::array<float, VECTOR_LENGTH>& unknown_embedding,
                  csr_matrix_t& matrix,
                  std::ofstream& index) {

    std::shared_ptr<FILE> bunzip2(popen(("bunzip2 -k -c " + filename).c_str(), "r"), pclose);
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
                index << filename << i << std::endl;
                moments.clear();

                //moments.finalize(matrix);
                //matrix.labels.push_back(label);
            } else if (moments.length > 0) {
                moments.clear();
            }
            i += 1;
        } else {
            auto p2 = word_end(line, p1 + 1);
            auto word = line.substr(p1 + 1, p2 - p1 - 1);
            auto itr = embeddings.find(word);
            if (itr == embeddings.end()) {
                moments.update(unknown_embedding);
            } else {
                moments.update(itr->second);
            }
        }
    }
    std::wcout << std::endl;
    free(buf);
}

std::unordered_map<std::wstring, std::array<float, VECTOR_LENGTH> > load_embeddings() {

    std::wifstream vectors("data/output/vectors-20d-30i-15w.txt");
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

int main() {

    std::cerr << "start" << std::endl;
    std::locale::global(std::locale(std::locale(""), "C", std::locale::numeric));
    std::wcout.imbue(std::locale());

    auto embeddings = load_embeddings();
    std::wcout << "Embeddings loaded" << std::endl;
    assert(embeddings.count(L"<unk>") > 0);
    auto& unknown_embedding = embeddings[L"<unk>"];

    std::ofstream index("data/output/test-set.index.txt");

    csr_matrix_t matrix;

    process_file("../step1/data/output/ads-samples.stemmed.txt.bz2", 1, embeddings, unknown_embedding, matrix, index);
    std::cerr << "Done ads-samples" << std::endl;
    process_file("../step2/data/output/yaca-ads.stemmed.txt.bz2", 1, embeddings, unknown_embedding, matrix, index);
    std::cerr << "Done yaca-ads" << std::endl;
    for (auto i = 0; i < 2; ++i) {
        process_file("../step1/data/output/featured-samples." + std::to_string(i) + ".stemmed.txt.bz2", 0, embeddings, unknown_embedding, matrix, index);
        std::cerr << "Done featured samples #" << i << std::endl;
    }

    index.close();
    return 0;

    matrix.indptr.push_back(matrix.indices.size());
    assert(matrix.indptr.size() == matrix.labels.size() + 1);

    /*

    DMatrixHandle test_set;
    XGDMatrixCreateFromCSR(matrix.indptr.data(), matrix.indices.data(), matrix.data.data(),
                              matrix.indptr.size(), matrix.data.size(), &test_set);
    XGDMatrixSetFloatInfo(test_set, "label", matrix.labels.data(), matrix.labels.size());
    XGDMatrixSaveBinary(test_set, "data/output/test-set.dmatrix.bin", 0);
    XGDMatrixFree(test_set);
    */

    return 0;
}
