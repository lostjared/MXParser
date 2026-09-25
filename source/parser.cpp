#include "MXParser/parser.hpp"

namespace mx {

    bool Parser::parse() { return true; }

    void Parser::error_message(const std::string &message, int line) { std::println("Error: {} on Line: {}", message, line); }
} // namespace mx
