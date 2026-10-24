#include <MXParser/interpreter.hpp>
#include <MXParser/parser.hpp>
#include <cstdio>
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
    struct Capture {
        std::ostringstream output;
        std::streambuf *previous = std::cout.rdbuf(output.rdbuf());
        ~Capture() { std::cout.rdbuf(previous); }
    };
    std::unique_ptr<mx::ProgramNode> parse(const std::string &source) {
        std::istringstream input(source);
        mx::Parser parser(input);
        return parser.parse_program();
    }
    template <typename... Args> std::string native(const char *format, Args... args) {
        int size = std::snprintf(nullptr, 0, format, args...);
        if (size < 0)
            throw std::runtime_error("Native formatting failed");
        std::vector<char> buffer(size_t(size) + 1);
        std::snprintf(buffer.data(), buffer.size(), format, args...);
        return {buffer.data(), size_t(size)};
    }
    void accepts(const std::string &source, const std::string &expected, int64_t result = -2) {
        try {
            auto ast = parse(source);
            mx::Interpreter interpreter;
            interpreter.load(*ast);
            Capture capture;
            auto value = std::get<int64_t>(interpreter.call());
            check(capture.output.str() == expected, "Wrong lsprintf output for: " + source + "\nExpected: " + expected + "\nGot: " + capture.output.str());
            check(value == (result == -2 ? int64_t(expected.size()) : result), "Wrong lsprintf return/count result for: " + source);
        } catch (const std::exception &error) {
            check(false, source + "\n" + error.what());
        }
    }
    void rejects(const std::string &source, const std::string &message, mx::ExecutionLimits limits = {}) {
        try {
            auto ast = parse(source);
            mx::Interpreter interpreter(limits);
            interpreter.load(*ast);
            Capture capture;
            interpreter.call();
            check(false, "Accepted invalid lsprintf call: " + source);
        } catch (const std::exception &error) {
            check(std::string(error.what()).find(message) != std::string::npos, "Wrong error: " + std::string(error.what()));
        }
    }
} // namespace
int main() {
    accepts(R"(int main(void){return lsprintf("Hello, %s! %d\n", "world", 42);})", "Hello, world! 42\n");
    accepts(R"(int main(void){return lsprintf("%d %i %u %o %x %X", -12, 34, 4294967295u, 9u, 255u, 255u);})", native("%d %i %u %o %x %X", -12, 34, 4294967295u, 9u, 255u, 255u));
    accepts(R"(int main(void){return lsprintf("%+08d|%-6d|%#x|%#o|%.0d", -12, 3, 255u, 9u, 0);})", native("%+08d|%-6d|%#x|%#o|%.0d", -12, 3, 255u, 9u, 0));
    accepts(R"(int main(void){return lsprintf("%hhd %hd %ld %lld %ju %zu %td", 255, 65535, -12L, -13LL, 18446744073709551615UL, sizeof(int), -14L);})", native("%hhd %hd %ld %lld %ju %zu %td", 255, 65535, -12L, -13LL, UINT64_MAX, size_t(4), ptrdiff_t(-14)));
    accepts(R"(int main(void){return lsprintf("%hhu %hu %lu %llu %jd %zd %tu", -1, -1, 18446744073709551615UL, 18446744073709551615ULL, -12L, -13L, 14UL);})", native("%hhu %hu %lu %llu %jd %zd %tu", -1, -1, UINT64_MAX, static_cast<unsigned long long>(UINT64_MAX), intmax_t(-12), ptrdiff_t(-13), size_t(14)));
    accepts(R"(int main(void){return lsprintf("%f|%F|%.2e|%.2E|%.4g|%.4G|%a|%A", 1.25, 2.5, 12.5, 12.5, 1234.5, 1234.5, 1.25, 1.25);})", native("%f|%F|%.2e|%.2E|%.4g|%.4G|%a|%A", 1.25, 2.5, 12.5, 12.5, 1234.5, 1234.5, 1.25, 1.25));
    accepts(R"(int main(void){float f=1.25f;return lsprintf("%.2f %lf %.2Lf %La", f, 2.5, 3.25L, 1.25L);})", native("%.2f %lf %.2Lf %La", 1.25, 2.5, 3.25L, 1.25L));
    accepts(R"(int main(void){return lsprintf("[%*.*f][%*s][%.*s][%.*f]", 9, 3, 1.25, -5, "x", 2, "abcd", -1, 1.25);})", native("[%*.*f][%*s][%.*s][%.*f]", 9, 3, 1.25, -5, "x", 2, "abcd", -1, 1.25));
    accepts(R"(int main(void){return lsprintf("%c%c%%", 'A', 0);})", std::string("A\0%", 3));
    accepts(R"(int main(void){char s[2]={'a','b'};return lsprintf("[%5.2s][%.0s]",s,s);})", "[   ab][]");
    accepts(R"(int main(void){char format[]="%d %s";char text[]="okay";return lsprintf(format,7,text);})", "7 okay");
    accepts(R"(enum {A=2};int main(void){char c='A';short n=-2;return lsprintf("%d %d %d",A,c,n);})", "2 65 -2");
    accepts(R"(int main(void){int n=0;lsprintf("abc%nxyz",&n);return n;})", "abcxyz", 3);
    accepts(R"(int main(void){signed char a=0;short b=0;long c=0;long long d=0;long e=0;long f=0;long g=0;lsprintf("X%hhnY%hnZ%ln%lln%jn%zn%tn",&a,&b,&c,&d,&e,&f,&g);return a+b+c+d+e+f+g;})", "XYZ", 18);
    accepts(R"(int main(void){int n=0;return lsprintf("constant%%",++n);})", "constant%");
    accepts(R"(int main(void){return lsprintf("%p",(void*)0);})", native("%p", static_cast<void *>(nullptr)));
    accepts(R"(int n;int next(void){return ++n;}int main(void){lsprintf("%d %d",next(),next());return n;})", "1 2", 2);
    accepts(R"(int main(void){int lsprintf=7;return lsprintf;})", "", 7);
    accepts(R"(int main(void){return lsprintf("");})", "");
    accepts(R"(int main(void){int n;lsprintf("%n",&n);return n;})", "", 0);
    accepts(R"(int main(void){0&&lsprintf("no");1||lsprintf("no");return lsprintf("yes");})", "yes");
    const std::vector<std::pair<std::string, std::string>> bad = {{R"(int main(void){return lsprintf();})", "format string"},
                                                                  {R"(int main(void){return lsprintf(42);})", "Incompatible assignment"},
                                                                  {R"(struct S{int x;};int main(void){struct S s={1};return lsprintf("%d",s);})", "scalar values"},
                                                                  {R"(int lsprintf(const char *f){return 0;}int main(void){return 0;})", "reserved built-in"},
                                                                  {R"(int main(void){int lsprintf=1;return lsprintf("x");})", "visible typed function"},
                                                                  {R"(int main(void){return lsprintf("%d");})", "too few"},
                                                                  {R"(int main(void){return lsprintf("%d",1.0);})", "integer argument type"},
                                                                  {R"(int main(void){return lsprintf("%ld",1);})", "integer argument type"},
                                                                  {R"(int main(void){return lsprintf("%f",1);})", "floating argument type"},
                                                                  {R"(int main(void){return lsprintf("%Lf",1.0);})", "floating argument type"},
                                                                  {R"(int main(void){return lsprintf("%f",1.0L);})", "floating argument type"},
                                                                  {R"(int main(void){return lsprintf("%s",(char*)0);})", "null or expired"},
                                                                  {R"(int main(void){char s[2]={'a','b'};return lsprintf("%s",s);})", "one-past"},
                                                                  {R"(int main(void){char *p=0;{char s[]="x";p=s;}return lsprintf("%s",p);})", "expired"},
                                                                  {R"(int main(void){return lsprintf("%s",1);})", "character pointer"},
                                                                  {R"(int main(void){return lsprintf("%p",0);})", "object pointer"},
                                                                  {R"(int main(void){return lsprintf("%");})", "incomplete format"},
                                                                  {R"(int main(void){return lsprintf("%q",1);})", "unsupported format"},
                                                                  {R"(int main(void){return lsprintf("%2$d",1);})", "unsupported format"},
                                                                  {R"(int main(void){return lsprintf("%ls","x");})", "wide strings"},
                                                                  {R"(int main(void){return lsprintf("%lc",'x');})", "wide characters"},
                                                                  {R"(int main(void){return lsprintf("%Ld",1);})", "integer length"},
                                                                  {R"(int main(void){int n=0;return lsprintf("%1n",&n);})", "does not allow"},
                                                                  {R"(int main(void){return lsprintf("%n",1);})", "integer pointer"},
                                                                  {R"(int main(void){long n=0;return lsprintf("%n",&n);})", "pointer type"},
                                                                  {R"(int main(void){const int n=0;return lsprintf("%n",&n);})", "pointer type"},
                                                                  {R"(int main(void){return lsprintf("%n",(int*)0);})", "null or expired"},
                                                                  {R"(int main(void){return lsprintf("%*d",1.0,1);})", "integer argument type"},
                                                                  {R"(int main(void){return lsprintf("%99999999999999999d",1);})", "output limit"},
                                                                  {R"(int main(void){return lsprintf("%*d",-2147483647-1,1);})", "output limit"}};
    for (const auto &[source, message] : bad)
        rejects(source, message);
    rejects(R"(int main(void){return lsprintf("%100d",1);})", "output limit", {1000000, 256, 1024 * 1024, 20});
    try {
        auto ast = parse(R"(int main(void){int a=1;return lsprintf("%p %p",&a,&a);})");
        mx::Interpreter interpreter;
        interpreter.load(*ast);
        Capture capture;
        auto count = std::get<int64_t>(interpreter.call());
        auto output = capture.output.str();
        auto space = output.find(' ');
        check(space != std::string::npos && output.substr(0, space) == output.substr(space + 1) && count == int64_t(output.size()), "Virtual pointer formatting/identity");
    } catch (const std::exception &error) {
        check(false, error.what());
    }
    try {
        struct FailingBuffer : std::streambuf {
            std::streamsize xsputn(const char *, std::streamsize) override { return 0; }
        } buffer;
        auto ast = parse(R"(int main(void){return lsprintf("text");})");
        mx::Interpreter interpreter;
        interpreter.load(*ast);
        auto previous = std::cout.rdbuf(&buffer);
        int64_t value;
        try {
            value = std::get<int64_t>(interpreter.call());
        } catch (...) {
            std::cout.rdbuf(previous);
            std::cout.clear();
            throw;
        }
        std::cout.rdbuf(previous);
        std::cout.clear();
        check(value == -1, "lsprintf must return a negative value on output failure");
        Capture capture;
        check(std::get<int64_t>(interpreter.call()) == 4 && capture.output.str() == "text", "lsprintf recovery after output failure");
    } catch (const std::exception &error) {
        check(false, error.what());
    }
    std::cout << "lsprintf conversions, stdout, byte counts, pointer/string checks, and " << bad.size() + 1 << " rejection cases checked\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
