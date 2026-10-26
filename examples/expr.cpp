#include "repl.hpp"
#include <Argz/argz.hpp>
#include <MXParser/ast.hpp>
#include <MXParser/interpreter.hpp>
#include <MXParser/parser.hpp>
#include <MXParser/semantic.hpp>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>

int main(int argc, char **argv) {
    Argz<std::string> arguments(argc, argv);
    auto add_flag = [&](char code, const std::string &name, const std::string &description) { arguments.addOptionSingle(code, description).addOptionDouble(code + 256, name, description); };
    add_flag('h', "help", "Show usage and options");
    add_flag('s', "syntax", "Print expression syntax");
    add_flag('S', "syntax-program", "Print translation-unit syntax");
    add_flag('c', "check-program", "Validate a translation unit");
    add_flag('r', "run-program", "Execute main and print program output");
    add_flag('p', "print-result", "Execute main and also print its result");
    add_flag('i', "repl", "Start the REPL, optionally preloading a file");
    arguments.addOptionSingleValue('f', "Input filename").addOptionDoubleValue('f' + 256, "file", "Input filename");
    arguments.addOptionSingleValue('m', "Native shared-library path").addOptionDoubleValue('m' + 256, "module", "Native shared-library path");
    arguments.addOptionSingleValue('H', "Native import header (.ls-h)").addOptionDoubleValue('H' + 256, "imports", "Native import header (.ls-h)");
    arguments.addOptionSingleValue('n', "Instruction budget; 0 disables it").addOptionDoubleValue('n' + 256, "instructions", "Instruction budget; 0 disables it");

    auto usage = [&](std::ostream &output) {
        output << "Usage: " << argv[0] << " [mode] [-f filename | filename] [--module library --imports header.ls-h]\n"
               << "Without arguments, starts the REPL. File modes require a filename.\n"
               << "Select one mode; --repl accepts an optional preload filename.\n";
        arguments.help(output);
    };
    char mode = 0;
    std::string input_filename;
    std::string module_path, imports_path;
    bool has_filename = false;
    bool has_instructions = false;
    mx::ExecutionLimits limits;
    bool help = false;
    try {
        Argument<std::string> argument;
        int code;
        while ((code = arguments.proc(argument)) != -1) {
            if (code >= 256)
                code -= 256;
            if (code == 'h') {
                help = true;
            } else if (code == '-' || code == 'f') {
                if (has_filename)
                    throw std::runtime_error("Specify only one input filename");
                input_filename = argument.arg_value;
                has_filename = true;
                if (input_filename.empty())
                    throw std::runtime_error("Input filename must not be empty");
            } else if (code == 'n') {
                if (has_instructions)
                    throw std::runtime_error("Specify only one instruction budget");
                const auto &text = argument.arg_value;
                auto result = std::from_chars(text.data(), text.data() + text.size(), limits.instructions);
                if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size())
                    throw std::runtime_error("Instruction budget must be a nonnegative integer");
                has_instructions = true;
            } else if (code == 'm' || code == 'H') {
                auto &path = code == 'm' ? module_path : imports_path;
                if (!path.empty())
                    throw std::runtime_error("Specify only one native module and import header");
                path = argument.arg_value;
                if (path.empty())
                    throw std::runtime_error("Native module and import header paths must not be empty");
            } else {
                if (mode != 0 && mode != code)
                    throw std::runtime_error("Select only one mode");
                mode = static_cast<char>(code);
            }
        }
        if (help) {
            usage(std::cout);
            return EXIT_SUCCESS;
        }
        if (mode != 0 && mode != 'i' && !has_filename)
            throw std::runtime_error("This mode requires an input filename");
        if (module_path.empty() != imports_path.empty())
            throw std::runtime_error("Use --module and --imports together");
        if ((!module_path.empty() || has_instructions) && mode != 'i' && mode != 'r' && mode != 'p' && !(mode == 0 && !has_filename))
            throw std::runtime_error("Native imports and instruction budgets require --repl, --run-program, or --print-result");
    } catch (const ArgException<std::string> &error) {
        std::cerr << error.text() << '\n';
        usage(std::cerr);
        return EXIT_FAILURE;
    } catch (const std::runtime_error &error) {
        std::cerr << "Argument error: " << error.what() << '\n';
        usage(std::cerr);
        return EXIT_FAILURE;
    }

    if (mode == 'i' || (mode == 0 && !has_filename))
        return mx::run_repl(has_filename ? input_filename.c_str() : nullptr, module_path.empty() ? nullptr : module_path.c_str(), imports_path.empty() ? nullptr : imports_path.c_str(), limits);

    bool syntax_only = mode == 's';
    bool syntax_program = mode == 'S';
    bool print_result = mode == 'p';
    bool run_program = mode == 'r' || print_result;
    bool check_program = mode == 'c';
    const char *filename = input_filename.c_str();
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
            mx::Interpreter interpreter(limits);
            if (!module_path.empty())
                interpreter.import_module(module_path, imports_path);
            interpreter.load(*program);
            auto result = interpreter.call();
            if (print_result) {
                std::visit(
                    [](auto value) {
                        using T = decltype(value);
                        if constexpr (!std::is_same_v<T, std::monostate>)
                            std::cout << "Result: " << value << '\n';
                    },
                    result);
            }
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
