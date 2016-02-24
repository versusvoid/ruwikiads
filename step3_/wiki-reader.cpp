#include <stdio.h>
#include <codecvt>
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

inline std::wstring::size_type word_end(const std::wstring& str, std::wstring::size_type p1) {
    auto p2 = p1 + 1;
    while ((str[p2] >= L'a' and str[p2] <= L'z') or (str[p2] >= L'а' and str[p2] <= L'я')) p2 += 1;

    return p2;
}

void process_file(const std::string& filename) {

    std::shared_ptr<FILE> bunzip2(popen(("bunzip2 -k -c " + filename).c_str(), "r"), pclose);
    size_t len = 4096;
    char* buf = (char*)malloc(len * sizeof(char));
    std::wstring line;
    while (getline(&buf, &len, bunzip2.get(), line)) {
        auto p1 = line.find(L'{');
        if (p1 == line.npos) {
            std::wcout << std::endl;
            continue;
        }
        auto p2 = word_end(line, p1 + 1);
        std::wcout << line.substr(p1 + 1, p2 - p1 - 1) << " ";
    }
    std::wcout << std::endl;
    free(buf);
}


int main() {

    std::ios_base::sync_with_stdio(false);
    std::locale::global(std::locale(std::locale(""), "C", std::locale::numeric));
    std::wcout.imbue(std::locale());


    process_file("../step1/data/output/ads-samples.stemmed.txt.bz2");
    std::cerr << "Done ads-samples" << std::endl;
    process_file("../step2/data/output/yaca-ads.stemmed.txt.bz2");
    std::cerr << "Done yaca-ads" << std::endl;
    for (auto i = 0; i < 8; ++i) {
        process_file("../step1/data/output/non-ads-samples." + std::to_string(i) + ".stemmed.txt.bz2");
        std::cerr << "Done non ads samples #" << i << std::endl;
    }

}
