#include <Argz/argz.hpp>
#include <MXParser/ast.hpp>
#include <MXParser/parser.hpp>
#include <cstdlib>
#include <fstream>
#include <string>

int main(int argc, char **argv) {
    Argz<std::string> arg_parser(argc, argv);
    arg_parser.addOptionDoubleValue(256, "input", "Specify a file").addOptionDoubleValue(257, "output", "Specify output file");
    Argument<std::string> arg;
    std::string in_file, out_file;
    int value = 0;
    try {
        while ((value = arg_parser.proc(arg)) != -1) {
            switch (value) {
            case 'h':
            case 'v':
                arg_parser.help(std::cout);
                exit(EXIT_SUCCESS);
                break;
            case 256:
                in_file = arg.arg_value;
                break;
            case 257:
                out_file = arg.arg_value;
                break;
            }
        }
    } catch (const ArgException<std::string> &e) {
        std::cerr << e.text() << std::endl;
    }
    if (in_file.length() == 0) {
        std::cerr << "MXParser: Error required input...\n";
        return EXIT_FAILURE;
    }

    std::fstream file;
    file.open(in_file, std::ios::in);
    if (!file.is_open()) {
        std::cerr << "MXParser: Error opening file: " << in_file << std::endl;
        return EXIT_FAILURE;
    }
    try {
        mx::Parser parser(file);
        if (!parser.parse()) {
            std::cerr << "MXParser: Parse error." << std::endl;
            file.close();
            return EXIT_FAILURE;
        }
        parser.print_tokens(std::cout);
    } catch (mx::ParserException &p) {
        std::cerr << "MXParser: Parser Exception: " << p.what() << std::endl;
        file.close();
        return EXIT_FAILURE;
    }
    file.close();

    return EXIT_SUCCESS;
}
