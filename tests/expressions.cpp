#include <MXParser/parser.hpp>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
    int failures = 0;

    void check(bool condition, const std::string &message) {
        if (!condition) {
            ++failures;
            std::cerr << message << '\n';
        }
    }

    std::unique_ptr<mx::ExprNode> parse(const std::string &source) {
        std::istringstream input(source);
        mx::Parser parser(input);
        return parser.parse_expression();
    }

    void rejects(const std::string &source) {
        try {
            parse(source);
            check(false, "Accepted invalid expression: " + source);
        } catch (const mx::ParserException &) {
        }
    }
} // namespace

int main() {
    const std::vector<std::pair<std::string, std::string>> trees = {{"a+b*c", "(a+(b*c))"},
                                                                    {"a-b-c", "((a-b)-c)"},
                                                                    {"a/b%c", "((a/b)%c)"},
                                                                    {"a<<b+1", "(a<<(b+1))"},
                                                                    {"a<b==c", "((a<b)==c)"},
                                                                    {"a&b^c|d&&e||f", "(((((a&b)^c)|d)&&e)||f)"},
                                                                    {"a=b=c+1", "(a=(b=(c+1)))"},
                                                                    {"a+=b*=c", "(a+=(b*=c))"},
                                                                    {"a=1,b=2,a+b", "(((a=1),(b=2)),(a+b))"},
                                                                    {"a?b:c?d:e", "(a?b:(c?d:e))"},
                                                                    {"a?b=1,c:d", "(a?((b=1),c):d)"},
                                                                    {"a||b?c:d", "((a||b)?c:d)"},
                                                                    {"f(a,b=2,(c,d))", "(f)(a,(b=2),(c,d))"},
                                                                    {"f()", "(f)()"},
                                                                    {"table[i+1].field->next++", "((((table)[(i+1)]).field)->next)++"},
                                                                    {"a<:i:>", "(a)[i]"},
                                                                    {"(*fp)(x)[i]", "((*(fp))(x))[i]"},
                                                                    {"- -a", "-(-(a))"},
                                                                    {"*(p+1)", "*((p+1))"},
                                                                    {"&a", "&(a)"},
                                                                    {"!~a", "!(~(a))"},
                                                                    {"++a + b--", "(++(a)+(b)--)"},
                                                                    {"(unsigned long *)p", "(unsigned long *)(p)"},
                                                                    {"(int)(double)x", "(int)((double)(x))"},
                                                                    {"sizeof(int) + sizeof *p", "(sizeof(int)+sizeof(*(p)))"},
                                                                    {"sizeof(struct item * const)", "sizeof(struct item * const)"},
                                                                    {"0xffUL", "0xffUL"},
                                                                    {"1.25e-2f", "1.25e-2f"},
                                                                    {"'\\n'", "'\\n'"},
                                                                    {"\"hello\" \"world\"", "\"hello\" \"world\""},
                                                                    {"a /* skip */ + // skip\n b;", "(a+b)"}};
    for (const auto &[source, expected] : trees) {
        try {
            auto tree = parse(source);
            check(tree->to_string() == expected, source + ": expected " + expected + ", got " + tree->to_string());
            check(parse(tree->to_string())->to_string() == tree->to_string(), "Round trip failed: " + source);
        } catch (const std::exception &error) {
            check(false, source + ": " + error.what());
        }
    }
    for (const auto &source : {"", "a+", "(a+b", "a b", "f(a,)", "f(,a)", "f(a b)", "a[]", "a[1", "a.", "a->int", "a?b", "a?:b", "a?b:", "a=", "a+b=2", "(int)x=2", "sizeof()", "(int)", "int", "return", "(unsigned float)x", "(long long double)x", "(int int)x", "a;;", "a;b", "1e+", "\"unterminated", "/* unterminated"}) {
        rejects(source);
    }
    for (const auto &op : {"=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|="}) {
        auto tree = parse(std::string("a") + op + "b");
        auto *binary = dynamic_cast<mx::BinaryNode *>(tree.get());
        check(binary && binary->op.get_token() == op, std::string("Assignment operator: ") + op);
    }
    {
        std::istringstream input("(size_t)n + sizeof(size_t)");
        mx::Parser parser(input);
        parser.add_type_name("size_t");
        check(parser.parse_expression()->to_string() == "((size_t)(n)+sizeof(size_t))", "Typedef disambiguation");
    }
    {
        std::istringstream input("; a=1; f(a); a?b:c;");
        mx::Parser parser(input);
        check(parser.parse_expressions().size() == 3, "Expression list");
        std::istringstream empty("");
        mx::Parser empty_parser(empty);
        check(empty_parser.parse_expressions().empty(), "Empty expression list");
    }
    mx::Evaluate evaluator;
    const std::vector<std::pair<std::string, std::int64_t>> evaluations = {{"a=3,b=4,a+b*2", 11}, {"a+=2", 5}, {"b%=3", 1}, {"a++", 5}, {"++a", 7}, {"a<<=1", 14}, {"a>=14 && b!=0", 1}, {"0 && (1/0)", 0}, {"1 || missing", 1}, {"1?42:1/0", 42}, {"0?missing:9", 9}, {"~0 & 255", 255}, {"0x10 + 010 + 0b10", 26}, {"!0 + +2", 3}, {"4294967296+1", 4294967297LL}};
    for (const auto &[source, expected] : evaluations) {
        try {
            evaluator.clear();
            auto tree = parse(source);
            tree->accept(evaluator);
            check(evaluator.result() == expected, "Evaluation failed: " + source);
            mx::ICode code;
            mx::HTMLCode html;
            tree->accept(code);
            tree->accept(html);
            check(!code.code().empty() && !html.code().empty(), "Empty code: " + source);
        } catch (const std::exception &error) {
            check(false, source + ": " + error.what());
        }
    }
    {
        auto tree = parse("0 && (1/0)");
        mx::ICode code;
        tree->accept(code);
        auto text = code.code();
        check(text.find("JZ ") < text.find("DIV ") && text.find("DIV ") < text.find("LABEL "), "Short circuit branch placement");
    }
    {
        auto tree = parse("1 ? 2 : 3");
        mx::ICode code;
        tree->accept(code);
        auto text = code.code();
        check(text.find("JZ ") < text.find("JMP ") && text.find("JMP ") < text.find("LABEL "), "Conditional branch placement");
    }
    {
        std::istringstream input("a=2; b=3; a+b*4;");
        mx::Parser parser(input);
        std::ostringstream output;
        auto *previous = std::cout.rdbuf(output.rdbuf());
        bool success = parser.parse();
        std::cout.rdbuf(previous);
        check(success && output.str().find("Result: 14") != std::string::npos, "Legacy evaluator entry point");
    }
    {
        auto tree = parse("flag ? left() : right()");
        try {
            tree->accept(evaluator);
            check(false, "Unsupported evaluation should throw");
        } catch (const std::runtime_error &) {
        }
    }
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
