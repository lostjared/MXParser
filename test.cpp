#include<MXParser/parser.hpp>
#include<MXParser/sym_tab.hpp>
#include<fstream>
#include<iostream>
#include<print>
#include<string>
#include<cstdlib>

int main(int argc, char **argv) {
    if(argc != 2) {
        std::println("Requires one argument:\n{} fliename", argv[1]);
        return EXIT_FAILURE;
    }

    std::fstream file;
    file.open(argv[1], std::ios::in);
    if(!file.is_open()) {
        std::println("Enter  a valid filename: {}\n", argv[1]);
        return EXIT_FAILURE;
    }
    try {
    mx::SymbolTable table;
    mx::Scanner scanner(file);
    mx::Token token;
    mx::TOKEN_TYPE token_type;
        while ((token_type = scanner.lex(token)) != mx::TOKEN_TYPE::TOKEN_NULL) {
            std::string id_token = token.get_token();
            std::cout << "[" << token.get_line() << "] -> " << token << " ";
            if(token_type == mx::TOKEN_TYPE::IDENTIFIER) {
                if(table.is_keyword(id_token)) {
                    mx::Symbol sym{};
                    sym.sym_type = mx::SYMBOL_TYPE::SYMBOL_KEYWORD;
                    sym.sym_variable_name = id_token;
                    sym.sym_variable_value = id_token;
                    std::cout <<  "Keyword\n";

                } else if(token_type == mx::TOKEN_TYPE::IDENTIFIER) {
                    mx::Symbol sym{};
                    sym.sym_type = mx::SYMBOL_TYPE::SYMBOL_IDENTIFER;
                    sym.sym_variable_name = id_token;
                    sym.sym_variable_value = "";
                    std::cout << "\n";
                    table.enter(id_token, sym);
                }
            } else if(token_type == mx::TOKEN_TYPE::OPERATOR &&  !id_token.empty() && id_token.at(0) == '{') {
                table.push_scope();
                std::cout << "\n";
            } else if(token_type == mx::TOKEN_TYPE::OPERATOR &&  !id_token.empty() && id_token.at(0) == '}') {
                table.pop_scope();
                std::cout << "\n";
            } else {
                std::cout << "\n";
            }

            if (token_type == mx::TOKEN_TYPE::TOKEN_ERROR) {
                std::cerr << token << "\n";
                break;
            }

        }
    } catch(mx::ScannerError &e) {
        std::println("Exception: {}", e.what());
    }
}
