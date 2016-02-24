#include <stdio.h>
#include <codecvt>
#include <algorithm>
#include <locale>
#include <iostream>
#include <memory>

std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> ucs2conv;
inline bool getline(char** lineptr, size_t* n, FILE* file, std::wstring& out) {
    auto len = getline(lineptr, n, file);
    //if (len == ssize_t(-1) and not feof(file)) return false;
    if (len == ssize_t(-1)) return false;
    out = ucs2conv.from_bytes(*lineptr, *lineptr + len);
    return true;
}

void process_file(const std::string& filename, uint32_t& max, uint32_t& min) {

    std::shared_ptr<FILE> bunzip2(popen(("bunzip2 -k -c " + filename).c_str(), "r"), pclose);
    size_t len = 4096;
    char* buf = (char*)malloc(len * sizeof(char));
    uint32_t sample_len = 0;
    std::wstring line;
    while (getline(&buf, &len, bunzip2.get(), line)) {
        auto p1 = line.find(L'{');
        if (p1 == line.npos) {
            max = std::max(max, sample_len);
            min = std::min(min, sample_len);
            sample_len = 0;
        } else {
            sample_len += 1;
        }
    }
    free(buf);
}


int main() {

    std::locale::global(std::locale(std::locale(""), "C", std::locale::numeric));
    std::wcout.imbue(std::locale());

    uint32_t max = 0, min = 0xFFFFFFFF;


    process_file("../step1/data/output/ads-samples.stemmed.txt.bz2", max, min);
    std::cerr << "Done ads-samples" << std::endl;
    process_file("../step2/data/output/yaca-ads.stemmed.txt.bz2", max, min);
    std::cerr << "Done yaca-ads" << std::endl;
    for (auto i = 0; i < 8; ++i) {
        process_file("../step1/data/output/non-ads-samples." + std::to_string(i) + ".stemmed.txt.bz2", max, min);
        std::cerr << "Done non ads samples #" << i << std::endl;
    }

    std::wcout << "Max: " << max << ", min: " << min << std::endl;

}
