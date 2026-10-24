#include <MXParser/parser.hpp>
#include <MXParser/semantic.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
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

    const std::map<std::string, std::vector<std::string>> exclusion_words = {{"static-assert", {"static assertion"}}, {"label", {"labels"}}, {"union", {"union"}}, {"bit-field", {"bit field"}}, {"function-pointer", {"function pointer"}}, {"variadic", {"variadic"}}, {"compound-literal", {"compound literal"}}, {"designated-initializer", {"designated initializer"}}, {"goto", {"goto"}}, {"preprocessor", {"preprocessor"}}, {"atomic", {"atomic"}}, {"complex", {"complex"}}, {"generic", {"generic"}}, {"static-local", {"block-scope", "static"}}, {"inline", {"inline"}}, {"old-style", {"old style"}}, {"flexible-array", {"flexible array"}}, {"wide-literal", {"wide"}}, {"brace-elision", {"brace elision"}}, {"auto", {"auto"}}, {"register", {"register"}}, {"extern-local", {"block-scope", "extern"}}, {"enum-forward", {"enum forward"}}, {"address-initializer", {"address constants"}}, {"aggregate-copy-initializer", {"aggregate copy initializer"}}};
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
        if (fields[2] != fields[3]) {
            ++syntax_gaps;
            std::cerr << fields[0] << ": current parsing differs from final contract\n";
            ++failures;
        }
        auto check_exclusion = [&](const std::string &message) {
            auto name = std::filesystem::path(fields[0]).stem().string();
            if (!name.starts_with("excluded-"))
                return;
            const auto found = exclusion_words.find(name.substr(9));
            if (found == exclusion_words.end()) {
                std::cerr << fields[0] << ": exclusion has no diagnostic assertion\n";
                ++failures;
                return;
            }
            for (const auto &word : found->second) {
                if (message.find(word) == std::string::npos) {
                    std::cerr << fields[0] << ": expected exclusion diagnostic containing '" << word << "', got " << message << '\n';
                    ++failures;
                }
            }
        };
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
            if (accepted) {
                ++semantic_cases;
                bool valid = true;
                try {
                    mx::SemanticAnalyzer analyzer;
                    analyzer.validate(*ast);
                } catch (const mx::SemanticException &error) {
                    valid = false;
                    check_exclusion(error.what());
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
            check_exclusion(error.what());
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
    std::cout << cases << " subset fixtures checked against current syntax behavior; " << syntax_gaps << " documented target syntax gaps; " << semantic_cases << " final semantic expectations checked. No execution performed.\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
