#include <MXParser/parser.hpp>
#include <MXParser/semantic.hpp>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
    int failures = 0;
    void check(bool condition, const std::string &message) {
        if (!condition) {
            ++failures;
            std::cerr << message << '\n';
        }
    }
    std::unique_ptr<mx::ProgramNode> parse(const std::string &source) {
        std::istringstream input(source);
        mx::Parser parser(input);
        return parser.parse_program();
    }
    std::unique_ptr<mx::ExprNode> expression(const std::string &source) {
        std::istringstream input(source);
        mx::Parser parser(input);
        return parser.parse_expression();
    }
    mx::FunctionNode &main_node(mx::ProgramNode &program) { return dynamic_cast<mx::FunctionNode &>(*program.declarations.back()); }
    void accepts(const std::string &source) {
        try {
            auto ast = parse(source);
            mx::SemanticAnalyzer analyzer;
            analyzer.validate(*ast);
            auto text = ast->to_string();
            auto second = parse(text);
            check(second->to_string() == text, "Statement round trip changed: " + text);
            analyzer.validate(*second);
        } catch (const std::exception &error) {
            check(false, "Rejected valid statements: " + source + "\n" + error.what());
        }
    }
    void rejects_syntax(const std::string &source, const std::string &message = {}) {
        try {
            parse(source);
            check(false, "Accepted invalid statement syntax: " + source);
        } catch (const mx::ParserException &error) {
            check(error.location.line > 0 && !error.location.token.empty(), "Missing parser diagnostic location");
            if (!message.empty())
                check(std::string(error.what()).find(message) != std::string::npos, "Wrong parser diagnostic: " + std::string(error.what()));
        }
    }
    void rejects_semantics(const std::string &source, const std::string &message) {
        auto ast = parse(source);
        try {
            mx::SemanticAnalyzer analyzer;
            analyzer.validate(*ast);
            check(false, "Accepted invalid statement semantics: " + source);
        } catch (const mx::SemanticException &error) {
            check(error.location.line > 0 && error.location.column > 0, "Missing semantic diagnostic location");
            check(std::string(error.what()).find(message) != std::string::npos, "Wrong semantic diagnostic: " + std::string(error.what()));
        }
    }
    void valid() {
        for (const auto &source : {"int main(void) { int n=0; do ++n; while(n<3); return n; }",
                                   "int main(void) { int n=0; do { ++n; if(n==1) continue; break; } while(n<3); return n; }",
                                   "int main(void) { switch(1) {} return 0; }",
                                   "int main(void) { switch(1) ; return 0; }",
                                   "int main(void) { switch(1) case 1: return 2; return 0; }",
                                   "int main(void) { switch(1) { case 1: case 2: return 2; default: return 0; } }",
                                   "int main(void) { int n=0; switch(n) { case 0: n+=1; case 1: n+=2; break; default: n=3; } return n; }",
                                   "int main(void) { int n=0; while(n<4) { ++n; switch(n) { case 1: continue; default: break; } n+=1; } return n; }",
                                   "int main(void) { int n=0; switch(n) { case 0: while(n<2) { ++n; continue; } break; default: return 0; } return n; }",
                                   "int main(void) { for(int i=0;i<2;++i) { switch(i) { default: do { continue; } while(0); break; } } return 0; }",
                                   "int main(void) { switch(1) { case 1: switch(2) { case 1: break; default: break; } break; default: break; } return 0; }",
                                   "int main(void) { int n=0; switch(n) { if(n) { case 0: n++; } else { default: n--; } } return n; }",
                                   "int main(void) { switch(1) do { case 1: break; } while(0); return 0; }",
                                   "int main(void) { if(1) do if(0) return 1; else break; while(0); else return 2; return 0; }",
                                   "int main(void) { int n=0; do do ++n; while(n<2); while(n<3); return n; }",
                                   "typedef int T; int main(void) { do { typedef double T; T n=1; } while(0); T n=2; return n; }",
                                   "typedef int T; int main(void) { switch(1) { case 1: { int T=2; } break; default: break; } T n=2; return n; }",
                                   "int main(void) { switch(1) { case 1:; typedef int T; T n=2; case 2: n=3; break; } return 0; }",
                                   "int main(void) { int n=0; switch(n) { case (1+2)*3: break; case 'a': break; case (int)2.5: break; case sizeof(int): break; default: break; } return 0; }",
                                   "int main(void) { switch(0) { case sizeof(int[1+2]): break; case sizeof(\"hi\"): break; case (1?4:5): break; } return 0; }",
                                   "int main(void) { unsigned char n=0; switch(n) { case -1: break; case 255: break; } return 0; }",
                                   "int main(void) { unsigned int n=0; switch(n) { case -1: break; case 1U: break; } return 0; }",
                                   "void done(void); int main(void) { do { done(); } while(0); return 0; }"})
            accepts(source);
    }
    void invalid() {
        for (const auto &source : {"int main(void) { do; }", "int main(void) { do; while(0) }", "int main(void) { do; while(); }", "int main(void) { do; while 0; }", "int main(void) { do while(0); }", "int main(void) { switch() {} }", "int main(void) { switch 1 {} }", "int main(void) { switch(1) }", "int main(void) { switch(1) { case 1 return 0; } }", "int main(void) { switch(1) { case : return 0; } }", "int main(void) { switch(1) { case 1: } }", "int main(void) { switch(1) { default: } }", "int main(void) { switch(1) { default 1: return 0; } }", "int main(void) { switch(1) { case 1: int n=0; } }", "int main(void) { switch(1) { case 1,2: return 0; } }"})
            rejects_syntax(source);
        rejects_syntax("int main(void) { case 1: return 0; }", "enclosing switch");
        rejects_syntax("int main(void) { default: return 0; }", "enclosing switch");
        rejects_syntax("int main(void) { switch(1) { case 1: continue; } }", "enclosing loop");
        rejects_syntax("int main(void) { break; }", "loop or switch");
        rejects_syntax("int main(void) { do; while(0); continue; }", "enclosing loop");
        rejects_syntax("int main(void) { switch(1) { default: break; } case 1: return 0; }", "enclosing switch");
        rejects_semantics("int main(void) { switch(1.0) {} return 0; }", "integer type");
        rejects_semantics("int main(void) { int *p=0; switch(p) {} return 0; }", "integer type");
        rejects_semantics("void f(void); int main(void) { do; while(f()); return 0; }", "scalar type");
        rejects_semantics("int main(void) { do { int n=0; } while(n); return 0; }", "Undeclared");
        rejects_semantics("int main(void) { switch(0) { case 1: break; case 1: break; } return 0; }", "Duplicate case");
        rejects_semantics("int main(void) { switch(0) { case 1+2: break; case 3: break; } return 0; }", "Duplicate case");
        rejects_semantics("int main(void) { unsigned n=0; switch(n) { case -1: break; case 4294967295U: break; } return 0; }", "Duplicate case");
        rejects_semantics("int main(void) { switch(0) { case 1: break; case 4294967297LL: break; } return 0; }", "Duplicate case");
        rejects_semantics("int main(void) { switch(0) { if(0) { case 1: break; } else { case 1: break; } } return 0; }", "Duplicate case");
        rejects_semantics("int main(void) { switch(0) { default: break; default: break; } return 0; }", "Multiple default");
        rejects_semantics("int main(void) { switch(0) { switch(0) { default: break; } default: break; default: break; } return 0; }", "Multiple default");
        rejects_semantics("int main(void) { switch(0) { case 1.0: break; } return 0; }", "integer constant");
        rejects_semantics("int main(void) { int n=1; switch(0) { case n: break; } return 0; }", "integer constant");
        rejects_semantics("int f(void); int main(void) { switch(0) { case f(): break; } return 0; }", "integer constant");
        rejects_semantics("int main(void) { switch(0) { case (1,2): break; } return 0; }", "integer constant");
        rejects_semantics("int main(void) { switch(0) { case 1/0: break; } return 0; }", "integer constant");
        rejects_semantics("int main(void) { switch(0) { case 2147483647+1: break; } return 0; }", "integer constant");
        rejects_semantics("int main(void) { if(0) switch(0) { case 1: return missing; } return 0; }", "Undeclared");
    }
    void structure_and_locations() {
        auto ast = parse("int main(void) {\n  do {\n    switch(1) {\n    case 1: case 2: break;\n    default: break;\n    }\n  } while(0);\n  return 0;\n}");
        auto &loop = dynamic_cast<mx::DoWhileNode &>(*main_node(*ast).body->statements[0]);
        auto &body = dynamic_cast<mx::BlockNode &>(*loop.body);
        auto &selection = dynamic_cast<mx::SwitchNode &>(*body.statements[0]);
        auto &cases = dynamic_cast<mx::BlockNode &>(*selection.body);
        auto &first = dynamic_cast<mx::CaseNode &>(*cases.statements[0]);
        auto &second = dynamic_cast<mx::CaseNode &>(*first.statement);
        auto &fallback = dynamic_cast<mx::DefaultNode &>(*cases.statements[1]);
        check(loop.location.line == 2 && loop.location.column == 3, "do location");
        check(selection.location.line == 3 && selection.location.column == 5, "switch location");
        check(first.location.line == 4 && first.location.column == 5 && second.location.column == 13, "Stacked case locations and shape");
        check(fallback.location.line == 5 && fallback.location.column == 5, "default location");
        mx::SemanticAnalyzer analyzer;
        analyzer.validate(*ast);
        check(analyzer.info(*first.value).integer_constant == 1 && analyzer.info(*second.value).integer_constant == 2, "Label constant facts");
        auto duplicate = parse("int main(void) {\n switch(0) {\n case 1: break;\n case 1: break;\n } return 0; }");
        try {
            analyzer.validate(*duplicate);
            check(false, "Missing duplicate-case diagnostic");
        } catch (const mx::SemanticException &error) {
            check(error.location.line == 4 && error.location.column == 2 && error.location.token == "case", "Duplicate-case diagnostic location");
        }
        analyzer.validate(*ast);

        auto no_context = parse("int main(void) { return 0; }");
        auto label = std::make_unique<mx::CaseNode>();
        label->location = {1, "case", 18};
        label->value = expression("1");
        label->statement = std::make_unique<mx::BlockNode>();
        main_node(*no_context).body->statements.insert(main_node(*no_context).body->statements.begin(), std::move(label));
        try {
            analyzer.validate(*no_context);
            check(false, "AST case context must be checked independently");
        } catch (const mx::SemanticException &) {
        }
        auto missing_default_context = parse("int main(void) { return 0; }");
        auto fallback_label = std::make_unique<mx::DefaultNode>();
        fallback_label->location = {1, "default", 18};
        fallback_label->statement = std::make_unique<mx::BlockNode>();
        main_node(*missing_default_context).body->statements.insert(main_node(*missing_default_context).body->statements.begin(), std::move(fallback_label));
        try {
            analyzer.validate(*missing_default_context);
            check(false, "AST default context must be checked independently");
        } catch (const mx::SemanticException &) {
        }
        auto switch_only = parse("int main(void) { switch(0) { default: break; } return 0; }");
        auto &manual_switch = dynamic_cast<mx::SwitchNode &>(*main_node(*switch_only).body->statements[0]);
        auto &manual_default = dynamic_cast<mx::DefaultNode &>(*dynamic_cast<mx::BlockNode &>(*manual_switch.body).statements[0]);
        dynamic_cast<mx::JumpNode &>(*manual_default.statement).is_break = false;
        try {
            analyzer.validate(*switch_only);
            check(false, "AST continue must not target a switch");
        } catch (const mx::SemanticException &) {
        }
        analyzer.validate(*ast);
    }
    class LegacyVisitor : public mx::ASTVisitor {
      public:
        void visit(mx::NumberNode &) override {}
        void visit(mx::BinaryNode &) override {}
        void visit(mx::UnaryNode &) override {}
        void visit(mx::AssignmentNode &) override {}
        void visit(mx::VariableNode &) override {}
    };
    void unsupported_visitors() {
        auto ast = parse("int main(void) { do; while(0); switch(0) { case 0: break; default: break; } return 0; } int good(void) { return 42; }");
        auto &main = dynamic_cast<mx::FunctionNode &>(*ast->declarations[0]);
        auto &selection = dynamic_cast<mx::SwitchNode &>(*main.body->statements[1]);
        auto &labels = dynamic_cast<mx::BlockNode &>(*selection.body);
        mx::Evaluate evaluator;
        mx::ICode code;
        mx::HTMLCode html;
        LegacyVisitor legacy;
        for (auto visitor : std::vector<mx::ASTVisitor *>{&evaluator, &code, &html, &legacy}) {
            for (auto node : std::vector<mx::ASTNode *>{main.body->statements[0].get(), &selection, labels.statements[0].get(), labels.statements[1].get()}) {
                try {
                    node->accept(*visitor);
                    check(false, "Unsupported visitor must report new statements");
                } catch (const std::runtime_error &error) {
                    check(std::string(error.what()).find("does not support") != std::string::npos, "Unclear visitor error");
                }
            }
        }
        ast->accept(evaluator);
        try {
            evaluator.call("main");
            check(false, "New statements must not execute before step 8");
        } catch (const std::runtime_error &) {
        }
        evaluator.call("good");
        check(evaluator.result() == 42, "Evaluator frame restored after unsupported statement");
    }
} // namespace

int main() {
    try {
        valid();
        invalid();
        structure_and_locations();
        unsupported_visitors();
    } catch (const std::exception &error) {
        check(false, std::string("Unexpected exception: ") + error.what());
    }
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
