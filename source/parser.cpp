#include "MXParser/parser.hpp"
#include "MXLex/token.hpp"
#include "MXParser/ast.hpp"
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace mx {

    std::string html_escape(const std::string &s) {
        std::string out;
        out.reserve(s.size() * 12 / 10 + 8);
        for (char c : s) {
            switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&#39;";
                break;
            default:
                out.push_back(c);
                break;
            }
        }
        return out;
    }
    std::string js_escape(const std::string &s) {
        std::string out;
        out.reserve(s.size() * 12 / 10 + 8);
        for (char c : s) {
            switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '\'':
                out += "\\'";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\0':
                out += "\\0";
                break;
            case '`':
                out += "\\`";
                break;
            case '$':
                out += "\\$";
                break;
            case '/':
                out += "\\/";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\x%02x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out.push_back(c);
                }
                break;
            }
        }
        return html_escape(out);
    }

    bool Parser::scan() {
        tokens.clear();
        index = 0;
        scan_failure.reset();
        name_scopes.assign(1, {});
        loop_depth = 0;
        switch_depth = 0;
        try {
            Token token;
            TOKEN_TYPE token_type;
            while ((token_type = scanner.lex(token)) != mx::TOKEN_TYPE::TOKEN_NULL) {
                if (token_type == mx::TOKEN_TYPE::TOKEN_ERROR) {
                    scan_failure.emplace("Expected a valid token", SourceLocation::from_token(token));
                    std::cerr << scan_failure->what() << "\n";
                    return false;
                }
                if (token.get_token() == "#" || token.get_token() == "%:") {
                    scan_failure.emplace("Unsupported feature: preprocessor directives", SourceLocation::from_token(token));
                    return false;
                }
                if (token.get_token() == "<:")
                    token.set_token("[", TOKEN_TYPE::OPERATOR, CHAR_TYPE::CHAR_LBRACKET);
                else if (token.get_token() == ":>")
                    token.set_token("]", TOKEN_TYPE::OPERATOR, CHAR_TYPE::CHAR_RBRACKET);
                tokens.push_back(token);
            }
            if (token.get_line() == 0)
                token.set_line(scanner.get_line());
            tokens.push_back(token);
        } catch (const mx::ScannerError &e) {
            scan_failure.emplace(std::string("Expected valid lexical input: ") + e.what(), SourceLocation{scanner.get_line(), "invalid lexical input"});
            std::cerr << scan_failure->what() << "\n";
            return false;
        }
        return true;
    }

    bool Parser::parse() {
        if (!scan()) {
            std::cerr << "Scanning failed.\n";
            return false;
        }
        Evaluate eval;
        ICode icode;
        HTMLCode html;
        std::fstream fout;
        fout.open("out.html", std::ios::out);

        if (!fout.is_open()) {
            throw ParserException("Error could not open output file for writing.\n");
        }

        std::vector<std::string> line_state;
        std::string gen_icode, gen_html_code;
        try {
            if (starts_declaration(index)) {
                auto unit = program();
                line_state.push_back(unit->to_string());
                unit->accept(eval);
                unit->accept(icode);
                unit->accept(html);
                if (eval.has_function("main")) {
                    eval.call("main");
                }
                gen_icode = icode.code();
                gen_html_code = html.code();
            } else {
                while (top().get_type() != TOKEN_TYPE::TOKEN_NULL) {
                    if (match(";"))
                        continue;
                    eval.clear();
                    icode.clear();
                    html.clear();
                    auto e = statement();
                    if (top().get_type() != TOKEN_TYPE::TOKEN_NULL)
                        expect(";");
                    auto representation = e->to_string();
                    std::cout << representation << "\n";
                    line_state.push_back(representation);
                    e->accept(eval);
                    e->accept(icode);
                    e->accept(html);
                    gen_icode += icode.code();
                    gen_html_code += html.code();
                }
            }
            std::cout << "ICode:\n" << gen_icode << "\n";

            output_html(fout, gen_html_code, line_state);

        } catch (std::runtime_error &e) {
            std::cerr << "Exception: " << e.what() << "\n";
            return false;
        }
        fout.close();
        if (!fout) {
            throw ParserException("Error writing to output file.");
        }
        return true;
    }

    void Parser::output_html(std::ostream &out, const std::string &html, std::vector<std::string> &state) {
        out << "<html><head><title>HTML Test</title></head><body>\n";
        for (const auto &s : state) {
            out << "<span style=\"color: red;\">" << html_escape(s) << "</span><br>\n";
        }
        out << html << "<br>";
        out << "</body></html>";
    }

    bool Parser::advance() {
        if (index + 1 >= tokens.size())
            return false;
        ++index;
        return true;
    }

    Token &Parser::peek(size_t offset) {
        if (index + offset >= tokens.size()) {
            throw ParserException(std::format("peek index {} out of bounds.", index + offset));
        }

        return tokens[index + offset];
    }

    Token &Parser::next() {
        if (advance())
            return tokens[index];

        throw ParserEOF();
    }

    Token &Parser::at(size_t index) {
        if (index < tokens.size()) {
            return tokens[index];
        }
        throw ParserException(std::format("Index: {}  out of bounds for token loop up", index));
    }

    Token &Parser::top() {
        if (index < tokens.size())
            return tokens[index];
        throw ParserException(std::format("index out of range for top at index:{}.", index));
    }

    void Parser::print_tokens(std::ostream &out) {
        out << "Tokens: \n";
        for (const auto &i : tokens) {
            out << i << std::endl;
        }
    }

    namespace {
        int precedence(const std::string &op) {
            if (op == "||")
                return 1;
            if (op == "&&")
                return 2;
            if (op == "|")
                return 3;
            if (op == "^")
                return 4;
            if (op == "&")
                return 5;
            if (op == "==" || op == "!=")
                return 6;
            if (op == "<" || op == ">" || op == "<=" || op == ">=")
                return 7;
            if (op == "<<" || op == ">>")
                return 8;
            if (op == "+" || op == "-")
                return 9;
            if (op == "*" || op == "/" || op == "%")
                return 10;
            return 0;
        }

        bool assignment_operator(const std::string &op) { return op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" || op == "<<=" || op == ">>=" || op == "&=" || op == "^=" || op == "|="; }

        bool qualifier(const std::string &text) { return text == "const" || text == "volatile" || text == "restrict" || text == "_Atomic"; }

        bool builtin_type(const std::string &text) { return text == "void" || text == "char" || text == "short" || text == "int" || text == "long" || text == "float" || text == "double" || text == "signed" || text == "unsigned" || text == "_Bool"; }

        bool keyword(const std::string &text) {
            static const std::set<std::string> words = {"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local"};
            return words.contains(text);
        }
    } // namespace

    bool Parser::match(const std::string &text) {
        if (top().get_token() != text)
            return false;
        advance();
        return true;
    }

    [[noreturn]] void Parser::fail_at(const Token &token, const std::string &message) { throw ParserException(message, SourceLocation::from_token(token)); }
    [[noreturn]] void Parser::fail(const std::string &message) { fail_at(top(), message); }

    void Parser::expect(const std::string &text) {
        if (!match(text))
            fail("Expected '" + text + "'");
    }

    void Parser::add_type_name(const std::string &name) { type_names.insert(name); }

    std::unique_ptr<ExprNode> Parser::parse_expression() {
        if (!scan())
            throw *scan_failure;
        auto result = expr();
        match(";");
        if (top().get_type() != TOKEN_TYPE::TOKEN_NULL) {
            fail("Expected end of input after expression");
        }
        return result;
    }

    std::vector<std::unique_ptr<ExprNode>> Parser::parse_expressions() {
        if (!scan())
            throw *scan_failure;
        std::vector<std::unique_ptr<ExprNode>> result;
        while (top().get_type() != TOKEN_TYPE::TOKEN_NULL) {
            if (match(";"))
                continue;
            result.push_back(expr());
            if (top().get_type() != TOKEN_TYPE::TOKEN_NULL)
                expect(";");
        }
        return result;
    }

    std::unique_ptr<ASTNode> Parser::statement() { return expr(); }

    std::unique_ptr<AssignmentNode> Parser::assignment() {
        Token var = top();
        advance();
        expect("=");
        return std::make_unique<AssignmentNode>(var, expr());
    }

    std::unique_ptr<ExprNode> Parser::expr() {
        auto left = assignment_expr();
        while (top().get_token() == ",") {
            Token op = top();
            advance();
            left = std::make_unique<BinaryNode>(std::move(left), op, assignment_expr());
        }
        return left;
    }

    std::unique_ptr<ExprNode> Parser::assignment_expr() {
        size_t start = index;
        auto left = conditional();
        if (assignment_operator(top().get_token())) {
            size_t end = index;
            index = start;
            unary();
            if (index != end)
                fail("Expected unary expression before assignment");
            Token op = top();
            advance();
            return std::make_unique<BinaryNode>(std::move(left), op, assignment_expr());
        }
        return left;
    }

    std::unique_ptr<ExprNode> Parser::conditional() {
        auto condition = binary(1);
        if (!match("?"))
            return condition;
        auto then_expr = expr();
        expect(":");
        auto else_expr = conditional();
        return std::make_unique<ConditionalNode>(std::move(condition), std::move(then_expr), std::move(else_expr));
    }

    std::unique_ptr<ExprNode> Parser::binary(int min_precedence) {
        auto left = cast();
        while (precedence(top().get_token()) >= min_precedence) {
            Token op = top();
            int level = precedence(op.get_token());
            advance();
            auto right = binary(level + 1);
            left = std::make_unique<BinaryNode>(std::move(left), op, std::move(right));
        }
        return left;
    }

    bool Parser::starts_type_name(size_t position) const {
        if (position >= tokens.size())
            return false;
        const auto &text = tokens[position].get_token();
        return builtin_type(text) || qualifier(text) || text == "struct" || text == "union" || text == "enum" || is_type_name(text);
    }

    bool Parser::is_type_name(const std::string &name) const {
        for (auto scope = name_scopes.rbegin(); scope != name_scopes.rend(); ++scope) {
            auto found = scope->find(name);
            if (found != scope->end())
                return bool(found->second);
        }
        return type_names.contains(name);
    }
    TypePtr Parser::lookup_type(const std::string &name) const {
        for (auto scope = name_scopes.rbegin(); scope != name_scopes.rend(); ++scope) {
            auto found = scope->find(name);
            if (found != scope->end())
                return found->second;
        }
        return {};
    }
    void Parser::bind_name(const Token &name, TypePtr type) {
        auto &scope = name_scopes.back();
        auto found = scope.find(name.get_token());
        if (found != scope.end() && (bool(found->second) != bool(type) || (type && !found->second->equals(*type))))
            fail_at(name, "Conflicting typedef and ordinary declarations");
        scope[name.get_token()] = std::move(type);
    }
    bool Parser::starts_declaration(size_t position) const {
        if (starts_type_name(position))
            return true;
        if (position >= tokens.size())
            return false;
        const auto &word = tokens[position].get_token();
        return word == "typedef" || word == "static" || word == "extern" || word == "inline" || word == "auto" || word == "register";
    }
    Parser::DeclarationSpecifiers Parser::declaration_specifiers(bool storage_allowed) {
        DeclarationSpecifiers result;
        result.location = SourceLocation::from_token(top());
        std::string spelling;
        Token last_specifier = top();
        TypePtr named_type;
        TypeQualifiers named_qualifiers;
        bool have_type = false;
        auto append = [&] {
            if (!spelling.empty())
                spelling += " ";
            spelling += top().get_token();
            advance();
        };
        for (;;) {
            const auto word = top().get_token();
            if (word == "_Static_assert")
                fail("Unsupported feature: static assertion");
            if (word == "_Complex" || word == "_Imaginary")
                fail("Unsupported feature: complex types");
            if (word == "typedef" || word == "static" || word == "extern" || word == "inline" || word == "auto" || word == "register") {
                if (!storage_allowed)
                    fail("Expected type name without a storage class");
                if (word == "auto" || word == "register")
                    fail("Unsupported storage class: " + word);
                result.storage.push_back(word);
                result.is_typedef |= word == "typedef";
                advance();
            } else if (qualifier(word)) {
                named_qualifiers.is_const |= word == "const";
                named_qualifiers.is_volatile |= word == "volatile";
                named_qualifiers.is_restrict |= word == "restrict";
                named_qualifiers.is_atomic |= word == "_Atomic";
                append();
            } else if (!named_type && builtin_type(word)) {
                have_type = true;
                last_specifier = top();
                append();
            } else if (!have_type && (word == "struct" || word == "union" || word == "enum")) {
                auto location = SourceLocation::from_token(top());
                advance();
                std::string name;
                if (top().get_type() == TOKEN_TYPE::IDENTIFIER && !keyword(top().get_token())) {
                    name = top().get_token();
                    advance();
                }
                auto kind = word == "struct" ? TagKind::Struct : word == "union" ? TagKind::Union : TagKind::Enum;
                std::shared_ptr<AggregateDefinition> definition;
                if (match("{")) {
                    if (kind == TagKind::Union)
                        fail("Unsupported feature: union");
                    definition = std::make_shared<AggregateDefinition>();
                    definition->location = location;
                    if (top().get_token() == "}")
                        fail("Empty aggregate definitions are unsupported");
                    if (kind == TagKind::Enum) {
                        for (;;) {
                            Enumerator enumerator;
                            enumerator.name = identifier();
                            if (match("="))
                                enumerator.value = conditional();
                            bind_name(enumerator.name);
                            definition->enumerators.push_back(std::move(enumerator));
                            if (!match(",") || top().get_token() == "}")
                                break;
                        }
                        expect("}");
                    } else {
                        DepthGuard members(member_depth);
                        while (!match("}")) {
                            if (top().get_type() == TOKEN_TYPE::TOKEN_NULL)
                                fail("Expected '}' in struct definition");
                            auto member = declaration(false);
                            auto field = dynamic_cast<DeclarationNode *>(member.get());
                            if (!field)
                                fail("Expected member declaration");
                            definition->members.emplace_back(static_cast<DeclarationNode *>(member.release()));
                        }
                    }
                } else if (name.empty())
                    fail("Expected type tag or aggregate definition");
                if (kind == TagKind::Enum && !definition && top().get_token() == ";")
                    fail("Unsupported feature: enum forward declaration");
                have_type = true;
                named_type = Type::tagged(kind, name, 0, definition);
            } else if (!have_type && is_type_name(word)) {
                named_type = Type::named(word, lookup_type(word));
                have_type = true;
                append();
            } else
                break;
        }
        if (!have_type)
            fail("Expected type name");
        try {
            result.type = named_type ? Type::qualified(named_type, named_qualifiers) : Type::from_name(spelling);
        } catch (const std::invalid_argument &error) {
            fail_at(last_specifier, std::string("Expected valid type specifiers: ") + error.what());
        }
        return result;
    }

    Parser::ParsedDeclarator Parser::parse_declarator(bool abstract_allowed, bool names_allowed) {
        ParsedDeclarator result;
        std::vector<DerivedDeclarator> pointers;
        while (top().get_token() == "*") {
            DerivedDeclarator pointer{TypeKind::Pointer, {}, {}, {}, top()};
            advance();
            while (qualifier(top().get_token())) {
                auto word = top().get_token();
                pointer.qualifiers.is_const |= word == "const";
                pointer.qualifiers.is_volatile |= word == "volatile";
                pointer.qualifiers.is_restrict |= word == "restrict";
                pointer.qualifiers.is_atomic |= word == "_Atomic";
                advance();
            }
            pointers.push_back(std::move(pointer));
        }
        if (names_allowed && top().get_type() == TOKEN_TYPE::IDENTIFIER && !keyword(top().get_token())) {
            result.name = identifier();
        } else if (top().get_token() == "(" && (peek(1).get_token() == "*" || peek(1).get_token() == "(" || peek(1).get_token() == "[" || (names_allowed && peek(1).get_type() == TOKEN_TYPE::IDENTIFIER && !keyword(peek(1).get_token()) && (!abstract_allowed || !is_type_name(peek(1).get_token()))))) {
            advance();
            result = parse_declarator(abstract_allowed, names_allowed);
            expect(")");
        } else if (!abstract_allowed)
            fail("Expected declarator name");
        while (top().get_token() == "[" || top().get_token() == "(") {
            DerivedDeclarator suffix{top().get_token() == "[" ? TypeKind::Array : TypeKind::Function, {}, {}, {}, top()};
            advance();
            if (suffix.kind == TypeKind::Array) {
                if (top().get_token() != "]")
                    suffix.bound = std::shared_ptr<const ExprNode>(conditional());
                expect("]");
            } else {
                suffix.has_prototype = top().get_token() != ")";
                suffix.parameters = parameter_list();
            }
            result.modifiers.push_back(std::move(suffix));
        }
        for (auto pointer = pointers.rbegin(); pointer != pointers.rend(); ++pointer)
            result.modifiers.push_back(std::move(*pointer));
        return result;
    }

    std::vector<Parameter> Parser::parameter_list() {
        ScopeGuard scope(*this);
        std::vector<Parameter> parameters;
        std::set<std::string> names;
        if (match(")"))
            return parameters;
        for (;;) {
            if (top().get_token() == "...")
                fail("Unsupported feature: variadic functions");
            if (!starts_declaration(index) && top().get_type() == TOKEN_TYPE::IDENTIFIER && !keyword(top().get_token()))
                fail("Unsupported feature: old style function parameters");
            Parameter parameter;
            parameter.location = SourceLocation::from_token(top());
            auto specifiers = declaration_specifiers(false);
            auto declarator = parse_declarator(true);
            parameter.name = declarator.name;
            parameter.declaration_type = derive_type(specifiers.type, declarator);
            parameter.type = parameter.declaration_type;
            const auto &base = parameter.type->unqualified();
            if (base.kind == TypeKind::Scalar && base.scalar == ScalarKind::Void) {
                if (parameters.empty() && parameter.name.get_type() == TOKEN_TYPE::TOKEN_NULL && top().get_token() == ")" && parameter.type->top_qualifiers() == TypeQualifiers{}) {
                    advance();
                    return {};
                }
                throw ParserException("Expected a non-void parameter type", parameter.location);
            }
            if (base.kind == TypeKind::Array)
                parameter.type = Type::pointer(Type::qualified(base.element, parameter.type->top_qualifiers()));
            if (base.kind == TypeKind::Function)
                throw ParserException("Unsupported feature: function pointer parameters", parameter.location);
            parameter.type_name = parameter.type->to_string();
            if (parameter.name.get_type() != TOKEN_TYPE::TOKEN_NULL) {
                if (!names.insert(parameter.name.get_token()).second)
                    fail_at(parameter.name, "Expected unique parameter name");
                bind_name(parameter.name);
            }
            parameters.push_back(std::move(parameter));
            if (!match(","))
                break;
        }
        expect(")");
        return parameters;
    }

    TypePtr Parser::derive_type(TypePtr base, const ParsedDeclarator &declarator) {
        for (auto modifier = declarator.modifiers.rbegin(); modifier != declarator.modifiers.rend(); ++modifier) {
            const auto kind = base->unqualified().kind;
            if (modifier->kind == TypeKind::Pointer) {
                if (kind == TypeKind::Function)
                    fail_at(modifier->token, "Unsupported feature: function pointers");
                base = Type::qualified(Type::pointer(base), modifier->qualifiers);
            } else if (modifier->kind == TypeKind::Array) {
                if (kind == TypeKind::Function)
                    fail_at(modifier->token, "An array cannot contain functions");
                std::optional<size_t> bound;
                if (auto literal = dynamic_cast<const NumberNode *>(modifier->bound.get())) {
                    auto text = literal->value.get_token();
                    while (!text.empty() && (text.back() == 'u' || text.back() == 'U' || text.back() == 'l' || text.back() == 'L'))
                        text.pop_back();
                    try {
                        size_t consumed = 0;
                        auto value = text.starts_with("0b") || text.starts_with("0B") ? std::stoull(text.substr(2), &consumed, 2) : std::stoull(text, &consumed, 0);
                        if (consumed == text.size() - (text.starts_with("0b") || text.starts_with("0B") ? 2 : 0) && value <= SIZE_MAX)
                            bound = static_cast<size_t>(value);
                    } catch (const std::exception &) {
                    }
                }
                base = Type::array(base, bound, modifier->bound);
            } else {
                if (kind == TypeKind::Array || kind == TypeKind::Function)
                    fail_at(modifier->token, "A function cannot return an array or function");
                std::vector<TypePtr> types;
                for (const auto &parameter : modifier->parameters)
                    types.push_back(parameter.type);
                base = Type::function(base, std::move(types), modifier->has_prototype);
            }
        }
        return base;
    }

    TypePtr Parser::type_name() {
        auto specifiers = declaration_specifiers(false);
        return derive_type(specifiers.type, parse_declarator(true, false));
    }

    std::unique_ptr<ExprNode> Parser::cast() {
        if (top().get_token() == "(" && starts_declaration(index + 1)) {
            Token start = top();
            advance();
            auto type = type_name();
            expect(")");
            if (top().get_token() == "{")
                fail("Unsupported feature: compound literal");
            auto result = std::make_unique<CastNode>(std::move(type), cast());
            result->location = SourceLocation::from_token(start);
            return result;
        }
        return unary();
    }

    std::unique_ptr<ExprNode> Parser::unary() {
        Token op = top();
        const auto text = op.get_token();
        if (text == "sizeof") {
            advance();
            if (top().get_token() == "(" && starts_declaration(index + 1)) {
                advance();
                auto type = type_name();
                expect(")");
                auto result = std::make_unique<SizeofNode>(std::move(type));
                result->location = SourceLocation::from_token(op);
                return result;
            }
            auto result = std::make_unique<SizeofNode>(unary());
            result->location = SourceLocation::from_token(op);
            return result;
        }
        if (text == "++" || text == "--") {
            advance();
            return std::make_unique<UnaryNode>(op, unary());
        }
        if (text == "+" || text == "-" || text == "!" || text == "~" || text == "*" || text == "&") {
            advance();
            return std::make_unique<UnaryNode>(op, cast());
        }
        return postfix();
    }

    std::unique_ptr<ExprNode> Parser::postfix() {
        auto result = primary();
        for (;;) {
            if (match("(")) {
                std::vector<std::unique_ptr<ExprNode>> args;
                if (!match(")")) {
                    do {
                        args.push_back(assignment_expr());
                    } while (match(","));
                    expect(")");
                }
                result = std::make_unique<CallNode>(std::move(result), std::move(args));
            } else if (match("[")) {
                auto subscript = expr();
                expect("]");
                result = std::make_unique<SubscriptNode>(std::move(result), std::move(subscript));
            } else if (top().get_token() == "." || top().get_token() == "->") {
                Token op = top();
                advance();
                Token member = top();
                if (member.get_type() != TOKEN_TYPE::IDENTIFIER || keyword(member.get_token()))
                    fail("Expected member name");
                advance();
                result = std::make_unique<MemberNode>(std::move(result), op, member);
            } else if (top().get_token() == "++" || top().get_token() == "--") {
                Token op = top();
                advance();
                result = std::make_unique<UnaryNode>(op, std::move(result), true);
            } else
                return result;
        }
    }

    std::unique_ptr<ExprNode> Parser::primary() {
        Token current = top();
        if (current.get_token() == "_Generic")
            fail("Unsupported feature: generic selection (_Generic)");
        if (match("(")) {
            auto result = expr();
            expect(")");
            result->location = SourceLocation::from_token(current);
            return result;
        }
        if (current.get_type() == TOKEN_TYPE::IDENTIFIER && !keyword(current.get_token()) && !is_type_name(current.get_token())) {
            advance();
            return std::make_unique<VariableNode>(current);
        }
        if (current.get_type() == TOKEN_TYPE::INTEGER_VALUE) {
            advance();
            return std::make_unique<NumberNode>(current);
        }
        if (current.get_type() == TOKEN_TYPE::FLOAT_VALUE || current.get_type() == TOKEN_TYPE::CHARACTER_VALUE || current.get_type() == TOKEN_TYPE::STRING_VALUE) {
            auto location = SourceLocation::from_token(current);
            advance();
            if (current.get_type() == TOKEN_TYPE::STRING_VALUE) {
                std::string text = current.get_token();
                while (top().get_type() == TOKEN_TYPE::STRING_VALUE) {
                    text += " " + top().get_token();
                    advance();
                }
                current.set_token(text, TOKEN_TYPE::STRING_VALUE);
            }
            auto result = std::make_unique<LiteralNode>(current);
            result->location = std::move(location);
            return result;
        }
        fail("Expected expression");
    }

    std::unique_ptr<ExprNode> Parser::factor() { return unary(); }
    std::unique_ptr<ExprNode> Parser::term() { return binary(10); }

    std::unique_ptr<ProgramNode> Parser::parse_program() {
        if (!scan())
            throw *scan_failure;
        return program();
    }

    Token Parser::identifier() {
        Token name = top();
        if (name.get_type() != TOKEN_TYPE::IDENTIFIER || keyword(name.get_token())) {
            fail("Expected identifier");
        }
        advance();
        return name;
    }

    std::unique_ptr<FunctionNode> Parser::function_node(const Declarator &declarator, const std::vector<std::string> &storage, SourceLocation location) {
        auto result = std::make_unique<FunctionNode>();
        result->location = std::move(location);
        result->storage = storage;
        result->name = declarator.name;
        result->parameters = declarator.parameters;
        const auto &signature = declarator.type->unqualified();
        if (result->parameters.empty()) {
            for (const auto &type : signature.parameters) {
                Parameter parameter;
                parameter.type = type;
                parameter.declaration_type = type;
                parameter.type_name = type->to_string();
                parameter.location = declarator.location;
                result->parameters.push_back(std::move(parameter));
            }
        }
        result->type = Type::function(signature.element, signature.parameters, signature.has_prototype);
        result->return_type = signature.element->to_string();
        return result;
    }

    std::unique_ptr<ProgramNode> Parser::program() {
        auto result = std::make_unique<ProgramNode>();
        result->location = SourceLocation::from_token(top());
        while (top().get_type() != TOKEN_TYPE::TOKEN_NULL) {
            if (match(";"))
                continue;
            auto node = declaration(true);
            auto group = dynamic_cast<DeclarationNode *>(node.get());
            bool functions = false;
            if (group && !group->is_typedef && !group->declared_type()->unqualified().definition)
                for (const auto &declarator : group->declarators)
                    functions |= declarator.type->unqualified().kind == TypeKind::Function;
            if (!functions)
                result->declarations.push_back(std::move(node));
            else {
                for (auto &declarator : group->declarators) {
                    if (declarator.type->unqualified().kind == TypeKind::Function)
                        result->declarations.push_back(function_node(declarator, group->storage, group->location));
                    else {
                        auto object = std::make_unique<DeclarationNode>();
                        object->location = group->location;
                        object->type = group->type;
                        object->type_name = group->type_name;
                        object->storage = group->storage;
                        object->declarators.push_back(std::move(declarator));
                        result->declarations.push_back(std::move(object));
                    }
                }
            }
        }
        return result;
    }

    std::unique_ptr<ASTNode> Parser::declaration(bool file_scope) {
        auto specifiers = declaration_specifiers(true);
        if (!file_scope)
            for (const auto &storage : specifiers.storage)
                if (storage != "typedef")
                    throw ParserException("Unsupported block-scope storage class: " + storage, specifiers.location);
        auto result = std::make_unique<DeclarationNode>();
        result->location = specifiers.location;
        result->type = specifiers.type;
        result->type_name = specifiers.type->to_string();
        result->is_typedef = specifiers.is_typedef;
        result->storage = specifiers.storage;
        if (top().get_token() == ";" && !specifiers.is_typedef && specifiers.type->unqualified().kind == TypeKind::Tagged && (specifiers.type->unqualified().tag == TagKind::Struct || specifiers.type->unqualified().definition)) {
            advance();
            return result;
        }
        for (;;) {
            auto parsed = parse_declarator(false);
            Declarator declarator;
            declarator.name = parsed.name;
            declarator.location = SourceLocation::from_token(parsed.name);
            declarator.type = derive_type(specifiers.type, parsed);
            if (!parsed.modifiers.empty() && parsed.modifiers.front().kind == TypeKind::Function)
                declarator.parameters = parsed.modifiers.front().parameters;
            if (!member_depth)
                bind_name(declarator.name, specifiers.is_typedef ? declarator.type : TypePtr{});
            if (top().get_token() == ":")
                fail("Unsupported feature: bit field");
            if (top().get_token() == "{") {
                if (!file_scope || specifiers.is_typedef || !result->declarators.empty() || declarator.type->unqualified().kind != TypeKind::Function || parsed.modifiers.empty() || parsed.modifiers.front().kind != TypeKind::Function)
                    fail("Expected a standalone function definition");
                auto function = function_node(declarator, specifiers.storage, specifiers.location);
                ScopeGuard scope(*this);
                for (const auto &parameter : function->parameters) {
                    if (parameter.name.get_type() == TOKEN_TYPE::TOKEN_NULL)
                        fail("Expected named parameters in function definition");
                    bind_name(parameter.name);
                }
                function->body = block(false);
                return function;
            }
            if (match("=")) {
                if (specifiers.is_typedef)
                    fail("A typedef cannot have an initializer");
                if (declarator.type->unqualified().kind == TypeKind::Function)
                    fail("A function cannot have an initializer");
                declarator.initializer = initializer();
            }
            result->declarators.push_back(std::move(declarator));
            if (!match(","))
                break;
        }
        expect(";");
        return result;
    }

    std::unique_ptr<ExprNode> Parser::initializer() {
        if (top().get_token() != "{")
            return assignment_expr();
        auto result = std::make_unique<InitializerListNode>();
        result->location = SourceLocation::from_token(top());
        advance();
        if (top().get_token() == "}")
            fail("Empty initializers are unsupported in C99");
        for (;;) {
            if (top().get_token() == "." || top().get_token() == "[")
                fail("Unsupported feature: designated initializer");
            result->elements.push_back(initializer());
            if (!match(",") || top().get_token() == "}")
                break;
        }
        expect("}");
        return result;
    }

    std::unique_ptr<BlockNode> Parser::block(bool new_scope) {
        ScopeGuard scope(*this, new_scope);
        auto location = SourceLocation::from_token(top());
        expect("{");
        auto result = std::make_unique<BlockNode>();
        result->location = location;
        while (!match("}")) {
            if (top().get_type() == TOKEN_TYPE::TOKEN_NULL)
                fail("Expected '}' before end of input");
            result->statements.push_back(function_statement(true));
        }
        return result;
    }

    std::unique_ptr<ASTNode> Parser::function_statement(bool allow_declaration) {
        if (top().get_token() == "_Static_assert")
            fail("Unsupported feature: static assertion");
        if (top().get_token() == "goto" || (top().get_type() == TOKEN_TYPE::IDENTIFIER && !keyword(top().get_token()) && peek(1).get_token() == ":"))
            fail("Unsupported feature: goto and ordinary labels");
        auto location = SourceLocation::from_token(top());
        if (top().get_token() == "{")
            return block();
        if (match(";")) {
            auto result = std::make_unique<BlockNode>();
            result->location = location;
            return result;
        }
        if (match("return")) {
            auto result = std::make_unique<ReturnNode>();
            result->location = location;
            if (top().get_token() != ";")
                result->expression = expr();
            expect(";");
            return result;
        }
        if (match("if")) {
            auto result = std::make_unique<IfNode>();
            result->location = location;
            expect("(");
            result->condition = expr();
            expect(")");
            result->then_statement = function_statement();
            if (match("else"))
                result->else_statement = function_statement();
            return result;
        }
        if (match("while")) {
            auto result = std::make_unique<WhileNode>();
            result->location = location;
            expect("(");
            result->condition = expr();
            expect(")");
            DepthGuard loop(loop_depth);
            result->body = function_statement();
            return result;
        }
        if (match("do")) {
            ScopeGuard scope(*this);
            auto result = std::make_unique<DoWhileNode>();
            result->location = location;
            {
                DepthGuard loop(loop_depth);
                result->body = function_statement();
            }
            expect("while");
            expect("(");
            result->condition = expr();
            expect(")");
            expect(";");
            return result;
        }
        if (match("switch")) {
            ScopeGuard scope(*this);
            auto result = std::make_unique<SwitchNode>();
            result->location = location;
            expect("(");
            result->condition = expr();
            expect(")");
            DepthGuard selection(switch_depth);
            result->body = function_statement();
            return result;
        }
        if (match("case")) {
            if (!switch_depth)
                throw ParserException("case requires an enclosing switch", location);
            auto result = std::make_unique<CaseNode>();
            result->location = location;
            result->value = conditional();
            expect(":");
            result->statement = function_statement();
            return result;
        }
        if (match("default")) {
            if (!switch_depth)
                throw ParserException("default requires an enclosing switch", location);
            auto result = std::make_unique<DefaultNode>();
            result->location = location;
            expect(":");
            result->statement = function_statement();
            return result;
        }
        if (match("for")) {
            ScopeGuard scope(*this);
            auto result = std::make_unique<ForNode>();
            result->location = location;
            expect("(");
            if (starts_declaration(index)) {
                result->initializer = declaration(false);
                if (auto declaration = dynamic_cast<DeclarationNode *>(result->initializer.get()); declaration && declaration->is_typedef)
                    throw ParserException("A for initializer must declare objects, not typedef names", declaration->location);
            } else {
                if (top().get_token() != ";")
                    result->initializer = expr();
                expect(";");
            }
            if (top().get_token() != ";")
                result->condition = expr();
            expect(";");
            if (top().get_token() != ")")
                result->update = expr();
            expect(")");
            DepthGuard loop(loop_depth);
            result->body = function_statement();
            return result;
        }
        if (top().get_token() == "break" || top().get_token() == "continue") {
            if (!loop_depth && (top().get_token() == "continue" || !switch_depth))
                fail(top().get_token() == "break" ? "Expected an enclosing loop or switch for break" : "Expected an enclosing loop for continue");
            auto result = std::make_unique<JumpNode>();
            result->location = location;
            result->is_break = top().get_token() == "break";
            advance();
            expect(";");
            return result;
        }
        if (starts_declaration(index)) {
            if (!allow_declaration)
                fail("Expected statement; a declaration requires a block");
            return declaration(false);
        }
        auto result = expr();
        expect(";");
        return result;
    }

    void Parser::error_message(const std::string &message, int line) { std::println("MXParser: Error: {} on Line: {}", message, line); }
} // namespace mx
