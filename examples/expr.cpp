#include <MXParser/ast.hpp>
#include <MXParser/interpreter.hpp>
#include <MXParser/parser.hpp>
#include <MXParser/semantic.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>

int main(int argc, char **argv) {

    bool syntax_only = argc == 3 && std::string(argv[1]) == "--syntax";
    bool syntax_program = argc == 3 && std::string(argv[1]) == "--syntax-program";
    bool run_program = argc == 3 && std::string(argv[1]) == "--run-program";
    bool check_program = argc == 3 && std::string(argv[1]) == "--check-program";
    if (argc != 2 && !syntax_only && !syntax_program && !check_program && !run_program) {
        std::cerr << "Program requires one argument:\n" << argv[0] << " [--syntax | --syntax-program | --check-program | --run-program] filename\n";
        return EXIT_FAILURE;
    }

    const char *filename = argv[(syntax_only || syntax_program || check_program || run_program) ? 2 : 1];
    std::fstream file;
    file.open(filename, std::ios::in);
    if (!file.is_open()) {
        std::cerr << "Error could not open file: " << filename << std::endl;
        return EXIT_FAILURE;
    }

    try {
        mx::Parser parser(file);
        if (run_program) {
            auto program = parser.parse_program();
            mx::Interpreter interpreter;
            interpreter.load(*program);
            auto result = interpreter.call();
            std::visit(
                [](auto value) {
                    using T = decltype(value);
                    if constexpr (!std::is_same_v<T, std::monostate>)
                        std::cout << "Result: " << value << '\n';
                },
                result);
        } else if (check_program) {
            auto program = parser.parse_program();
            mx::SemanticAnalyzer analyzer;
            analyzer.validate(*program);
            std::cout << "Semantic validation passed\n";
        } else if (syntax_program) {
            std::cout << parser.parse_program()->to_string();
        } else if (syntax_only) {
            for (const auto &expression : parser.parse_expressions()) {
                std::cout << expression->to_string() << ";\n";
            }
        } else if (!parser.parse()) {
            std::cerr << "Error: Parsing failed.\n";
            return EXIT_FAILURE;
        }
    } catch (const mx::SemanticException &e) {
        std::cerr << "Semantic error: " << e.what() << '\n';
        return EXIT_FAILURE;
    } catch (mx::ParserException &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (const std::runtime_error &e) {
        std::cerr << "Execution error: " << e.what() << '\n';
        return EXIT_FAILURE;
    } catch (mx::ParserEOF &e) {
        std::cerr << "Exception: Unexpected EOF.\n";
        return EXIT_FAILURE;
    }
    file.close();
    return EXIT_SUCCESS;
}
