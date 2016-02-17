#include <fstream>
#include <iostream>


int main() {
    std::ifstream vectors;
    vectors.open("data/output/vectors-20d-30i-15w.txt");
    std::cerr << vectors.is_open() << std::endl;
    std::string line;
    std::cerr << "getting line" << std::endl;
    for (int i = 0; i < 25000; ++i) {
        std::getline(vectors, line);
        //std::cout << "line: " << line << std::endl;
    }
    std::cout << vectors.eof() << std::endl;
 

    return 0;
}
