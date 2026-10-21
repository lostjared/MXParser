#include <MXParser/ast.hpp>
#include <MXParser/parser.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char **argv) {

    bool syntax_only = argc == 3 && std::string(argv[1]) == "--syntax";
    if (argc != 2 && !syntax_only) {
        std::cerr << "Program requires one argument:\n" << argv[0] << " [--syntax] filename\n";
        return EXIT_FAILURE;
    }

    const char *filename = argv[syntax_only ? 2 : 1];
    std::fstream file;
    file.open(filename, std::ios::in);
    if (!file.is_open()) {
        std::cerr << "Error could not open file: " << filename << std::endl;
        return EXIT_FAILURE;
    }

    try {
        mx::Parser parser(file);
        if (syntax_only) {
            for (const auto &expression : parser.parse_expressions()) {
                std::cout << expression->to_string() << ";\n";
            }
        } else if (!parser.parse()) {
            std::cerr << "Error: Parsing failed.\n";
            return EXIT_FAILURE;
        }
    } catch (mx::ParserException &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (mx::ParserEOF &e) {
        std::cerr << "Exception: Unexpected EOF.\n";
        return EXIT_FAILURE;
    }
    file.close();
    return EXIT_SUCCESS;
}
