#include <stdio.h>
#include <unordered_map>
#include <codecvt>
#include <locale>
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <cassert>

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

const uint32_t WINDOW_LENGTH = 25;

void process_file(const std::string& filename, uint8_t label,  
                  std::unordered_map<std::wstring, uint16_t>& embeddings, 
                  uint16_t unknown_embedding, uint16_t padding_embedding, std::shared_ptr<FILE> bzip2) {

    std::shared_ptr<FILE> bunzip2(popen(("bunzip2 -k -c " + filename).c_str(), "r"), pclose);
    size_t len = 4096;
    char* buf = (char*)malloc(len * sizeof(char));
    std::wstring line;
    std::vector<uint16_t> sample_embeddings;
    std::vector<uint16_t> padding_embeddings(WINDOW_LENGTH, padding_embedding);
    while (getline(&buf, &len, bunzip2.get(), line)) {
        auto p1 = line.find(L'{');
        if (p1 == line.npos) {
            fwrite(&label, sizeof(label), 1, bzip2.get());
            uint32_t len = sample_embeddings.size();
            fwrite(&len, sizeof(len), 1, bzip2.get());
            fwrite(sample_embeddings.data(), sizeof(uint16_t), len, bzip2.get());
            if (len < WINDOW_LENGTH) {
                fwrite(padding_embeddings.data(), sizeof(uint16_t), WINDOW_LENGTH - len, bzip2.get());
            }
            sample_embeddings.clear();
        } else {
            auto p2 = word_end(line, p1 + 1);
            auto word = line.substr(p1 + 1, p2 - p1 - 1);
            auto itr = embeddings.find(word);
            if (itr == embeddings.end()) {
                sample_embeddings.push_back(unknown_embedding);
            } else {
                sample_embeddings.push_back(itr->second);
            }
        }
    }
    std::wcout << std::endl;
    free(buf);
}

std::unordered_map<std::wstring, uint16_t> load_embeddings() {

    std::wifstream vectors("data/output/vectors-50d-25i-15w.txt");
    std::wstring line;
    std::getline(vectors, line);
    uint16_t i = 0;
    std::unordered_map<std::wstring, uint16_t> result;
    while (line.length() > 0) {
        result[line.substr(0, line.find(L' '))] = i;
        i += 1;
        std::getline(vectors, line);
    }
    return result;
}

int main() {

    std::ios_base::sync_with_stdio(false);
    std::locale::global(std::locale(std::locale(""), "C", std::locale::numeric));
    std::wcout.imbue(std::locale());

    auto embeddings = load_embeddings();
    assert(embeddings.count(L"<unk>") > 0);
    uint16_t unknown_embedding = embeddings[L"<unk>"];
    uint16_t padding_embedding = embeddings.size();

    std::shared_ptr<FILE> bzip2(popen("bzip2 -9 > data/output/corpus.bin.bz2", "w"), pclose);

    process_file("../step1/data/output/ads-samples.stemmed.txt.bz2", 1, embeddings, unknown_embedding, padding_embedding, bzip2);
    std::cerr << "Done ads-samples" << std::endl;
    process_file("../step2/data/output/yaca-ads.stemmed.txt.bz2", 1, embeddings, unknown_embedding, padding_embedding, bzip2);
    std::cerr << "Done yaca-ads" << std::endl;
    for (auto i = 0; i < 8; ++i) {
        process_file("../step1/data/output/non-ads-samples." + std::to_string(i) + ".stemmed.txt.bz2", 0, embeddings, unknown_embedding, padding_embedding, bzip2);
        std::cerr << "Done non ads samples #" << i << std::endl;
    }

}
