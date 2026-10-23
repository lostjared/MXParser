#include <MXParser/parser.hpp>
#include <MXParser/semantic.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Expected a subset fixture manifest path\n";
        return EXIT_FAILURE;
    }
    const std::filesystem::path manifest_path(argv[1]);
    std::ifstream manifest(manifest_path);
    if (!manifest) {
        std::cerr << "Cannot open " << manifest_path << '\n';
        return EXIT_FAILURE;
    }

    int failures = 0;
    size_t cases = 0;
    size_t syntax_gaps = 0;
    size_t semantic_cases = 0;
    std::set<std::string> paths;
    std::string line;
    while (std::getline(manifest, line)) {
        if (line.empty() || line.front() == '#')
            continue;
        std::istringstream row(line);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(row, field, '\t'))
            fields.push_back(field);
        auto outcome = [](const std::string &value) { return value == "accept" || value == "reject"; };
        if (fields.size() != 6 || !outcome(fields[2]) || !outcome(fields[3]) || !outcome(fields[4]) || fields[1].size() != 1 || fields[1][0] < '1' || fields[1][0] > '7' || (fields[3] == "reject" && fields[4] != "reject") || ((fields[4] == "reject") != (fields[5] != "-")) || !paths.insert(fields[0]).second) {
            std::cerr << "Invalid or duplicate manifest entry: " << line << '\n';
            ++failures;
            continue;
        }
        ++cases;
        if (fields[2] != fields[3])
            ++syntax_gaps;
        const auto source_path = manifest_path.parent_path() / fields[0];
        std::ifstream input(source_path);
        if (!input) {
            std::cerr << "Cannot open " << source_path << '\n';
            ++failures;
            continue;
        }
        bool accepted = false;
        try {
            mx::Parser parser(input);
            auto ast = parser.parse_program();
            accepted = ast != nullptr;
            if (accepted && fields[1][0] <= '4') {
                ++semantic_cases;
                bool valid = true;
                try {
                    mx::SemanticAnalyzer analyzer;
                    analyzer.validate(*ast);
                } catch (const mx::SemanticException &error) {
                    valid = false;
                    if (error.location.line == 0 || error.location.token.empty()) {
                        std::cerr << source_path << ": missing semantic diagnostic location\n";
                        ++failures;
                    }
                }
                if (valid != (fields[4] == "accept")) {
                    std::cerr << source_path << ": expected semantic validation to " << fields[4] << ", got " << (valid ? "accept" : "reject") << '\n';
                    ++failures;
                }
            }
        } catch (const mx::ParserException &error) {
            if (std::string(error.what()).empty()) {
                std::cerr << source_path << ": empty parser diagnostic\n";
                ++failures;
            }
            if (error.location.line == 0 || error.location.token.empty() || std::string(error.what()).find("at line ") == std::string::npos) {
                std::cerr << source_path << ": missing parser source location\n";
                ++failures;
            }
            if (error.location.token != "end of input" && error.location.token != "invalid lexical input" && error.location.column == 0) {
                std::cerr << source_path << ": missing token column\n";
                ++failures;
            }
        } catch (const std::exception &error) {
            std::cerr << source_path << ": unexpected exception: " << error.what() << '\n';
            ++failures;
        } catch (...) {
            std::cerr << source_path << ": unexpected nonstandard exception\n";
            ++failures;
        }
        if (accepted != (fields[2] == "accept")) {
            std::cerr << source_path << ": expected current parse to " << fields[2] << ", got " << (accepted ? "accept" : "reject") << '\n';
            ++failures;
        }
    }
    if (cases == 0 || manifest.bad()) {
        std::cerr << "Empty or unreadable fixture manifest\n";
        ++failures;
    }
    std::cout << cases << " subset fixtures checked against current syntax behavior; " << syntax_gaps << " documented target syntax gaps; " << semantic_cases << " step 1-4 semantic expectations checked. No execution performed.\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
