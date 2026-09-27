#include <MXParser/ast.hpp>
#include <MXParser/parser.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char **argv) {

    if (argc != 2) {
        std::cerr << "Program requires one argument:\n" << argv[0] << " filename\n";
        return EXIT_FAILURE;
    }

    std::fstream file;
    file.open(argv[1], std::ios::in);
    if (!file.is_open()) {
        std::cerr << "Error could not open file: " << argv[1] << std::endl;
        return EXIT_FAILURE;
    }

    try {
        mx::Parser parser(file);
        parser.parse();

    } catch (mx::ParserException &e) {
        std::cerr << e.what() << std::endl;
    }

    return EXIT_SUCCESS;
}
