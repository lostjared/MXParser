#include "repl.hpp"
#include "MXParser/interpreter.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <atomic>
#include <windows.h>
#else
#include <csignal>
#include <poll.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <unistd.h>
#endif

namespace mx {
    namespace {
        enum class InputStatus { Line, End, Interrupted };
        struct Input {
            InputStatus status;
            std::string text;
        };
#ifdef _WIN32
        std::atomic_bool interrupted = false;
        BOOL WINAPI interrupt_handler(DWORD event) {
            if (event != CTRL_C_EVENT)
                return FALSE;
            interrupted.store(true);
            return TRUE;
        }
        class LineReader {
          public:
            LineReader() : handle(GetStdHandle(STD_INPUT_HANDLE)), terminal(GetConsoleMode(handle, &mode) != 0) {
                if (terminal) {
                    if (!SetConsoleMode(handle, mode | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT))
                        throw std::runtime_error("Cannot enable Windows console line editing");
                    output_page = GetConsoleOutputCP();
                    SetConsoleOutputCP(CP_UTF8);
                    handler = SetConsoleCtrlHandler(interrupt_handler, TRUE) != 0;
                }
            }
            ~LineReader() {
                if (handler)
                    SetConsoleCtrlHandler(interrupt_handler, FALSE);
                if (terminal) {
                    SetConsoleMode(handle, mode);
                    SetConsoleOutputCP(output_page);
                }
            }
            bool interactive() const { return terminal; }
            Input read(const char *prompt) {
                if (!terminal) {
                    std::string line;
                    return std::getline(std::cin, line) ? Input{InputStatus::Line, line} : Input{InputStatus::End, {}};
                }
                if (interrupted.exchange(false))
                    return {InputStatus::Interrupted, {}};
                std::cout << prompt << std::flush;
                std::wstring line;
                for (;;) {
                    wchar_t buffer[256];
                    DWORD count = 0;
                    BOOL success = ReadConsoleW(handle, buffer, 256, &count, nullptr);
                    if (interrupted.exchange(false))
                        return {InputStatus::Interrupted, {}};
                    if (!success && GetLastError() == ERROR_OPERATION_ABORTED)
                        return {InputStatus::Interrupted, {}};
                    if (!success)
                        throw std::runtime_error("Cannot read Windows console input");
                    if (count == 0)
                        return {InputStatus::End, {}};
                    line.append(buffer, count);
                    if (line.find(L'\n') != std::wstring::npos)
                        break;
                }
                if (!line.empty() && line.front() == L'\x1a')
                    return {InputStatus::End, {}};
                while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n'))
                    line.pop_back();
                if (line.empty())
                    return {InputStatus::Line, {}};
                int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, line.data(), static_cast<int>(line.size()), nullptr, 0, nullptr, nullptr);
                if (bytes == 0)
                    throw std::runtime_error("Invalid Unicode console input");
                std::string text(static_cast<size_t>(bytes), '\0');
                WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, line.data(), static_cast<int>(line.size()), text.data(), bytes, nullptr, nullptr);
                return {InputStatus::Line, text};
            }

          private:
            HANDLE handle;
            DWORD mode = 0;
            UINT output_page = 0;
            bool terminal;
            bool handler = false;
        };
#else
        volatile std::sig_atomic_t interrupted = 0;
        void interrupt_handler(int) { interrupted = 1; }
        struct ReadlineState {
            bool ready = false;
            bool active = false;
            Input input{InputStatus::End, {}};
        };
        ReadlineState *active_read = nullptr;
        void line_ready(char *line) {
            active_read->input = line ? Input{InputStatus::Line, line} : Input{InputStatus::End, {}};
            std::free(line);
            active_read->ready = true;
            rl_callback_handler_remove();
            active_read->active = false;
        }
        class LineReader {
          public:
            LineReader() : terminal(isatty(fileno(stdin)) != 0) {
                if (terminal) {
                    using_history();
                    stifle_history(500);
                    signal_mode = rl_catch_signals;
                    rl_catch_signals = 0;
                    previous_handler = std::signal(SIGINT, interrupt_handler);
                }
            }
            ~LineReader() {
                if (terminal) {
                    std::signal(SIGINT, previous_handler);
                    rl_catch_signals = signal_mode;
                    clear_history();
                }
            }
            bool interactive() const { return terminal; }
            Input read(const char *prompt) {
                if (!terminal) {
                    std::string line;
                    return std::getline(std::cin, line) ? Input{InputStatus::Line, line} : Input{InputStatus::End, {}};
                }
                ReadlineState state;
                active_read = &state;
                if (!interrupted) {
                    state.active = true;
                    rl_callback_handler_install(prompt, line_ready);
                }
                while (!state.ready && !interrupted) {
                    pollfd descriptor{fileno(stdin), POLLIN, 0};
                    int status = poll(&descriptor, 1, -1);
                    if (status < 0 && errno != EINTR) {
                        if (state.active)
                            rl_callback_handler_remove();
                        active_read = nullptr;
                        throw std::runtime_error("Cannot read terminal input");
                    }
                    if (status > 0)
                        rl_callback_read_char();
                }
                if (state.active)
                    rl_callback_handler_remove();
                active_read = nullptr;
                if (interrupted) {
                    interrupted = 0;
                    std::cout << '\n';
                    return {InputStatus::Interrupted, {}};
                }
                if (state.input.status == InputStatus::Line && !state.input.text.empty())
                    add_history(state.input.text.c_str());
                return state.input;
            }

          private:
            bool terminal;
            int signal_mode = 0;
            using SignalHandler = void (*)(int);
            SignalHandler previous_handler = SIG_DFL;
        };
#endif
        bool needs_more(const std::string &source) {
            std::vector<char> brackets;
            char quote = 0;
            bool escaped = false, block_comment = false, line_comment = false;
            for (size_t i = 0; i < source.size(); ++i) {
                char current = source[i];
                char next = i + 1 < source.size() ? source[i + 1] : '\0';
                if (line_comment) {
                    if (current == '\n')
                        line_comment = false;
                    continue;
                }
                if (block_comment) {
                    if (current == '*' && next == '/') {
                        block_comment = false;
                        ++i;
                    }
                    continue;
                }
                if (quote) {
                    if (current == '\n')
                        return false;
                    if (escaped)
                        escaped = false;
                    else if (current == '\\')
                        escaped = true;
                    else if (current == quote)
                        quote = 0;
                    continue;
                }
                if (current == '/' && next == '/') {
                    line_comment = true;
                    ++i;
                } else if (current == '/' && next == '*') {
                    block_comment = true;
                    ++i;
                } else if (current == '\'' || current == '"')
                    quote = current;
                else if (current == '(' || current == '[' || current == '{')
                    brackets.push_back(current);
                else if (current == ')' || current == ']' || current == '}') {
                    char expected = current == ')' ? '(' : current == ']' ? '[' : '{';
                    if (brackets.empty() || brackets.back() != expected)
                        return false;
                    brackets.pop_back();
                }
            }
            return !brackets.empty() || block_comment;
        }
        std::string trim(const std::string &text) {
            auto first = text.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
                return {};
            return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
        }
    } // namespace
    int run_repl(const char *filename, const char *module_path, const char *imports_path, ExecutionLimits limits) {
        try {
            LineReader reader;
            auto make_interpreter = [limits] {
                auto interpreter = std::make_unique<Interpreter>(limits);
                interpreter->set_interrupt_handler([] { return interrupted != 0; });
                return interpreter;
            };
            auto interpreter = make_interpreter();
            if (module_path && imports_path)
                interpreter->import_module(module_path, imports_path);
            if (filename) {
                std::ifstream input(filename);
                if (!input)
                    throw std::runtime_error(std::string("Cannot open file: ") + filename);
                interpreter->submit(std::string(std::istreambuf_iterator<char>(input), {}));
            }
            if (reader.interactive())
                std::cout << "ls-C Interactive use C expressions, statements, declarations, or functions. :help for commands.\n";
            std::string pending;
            for (;;) {
                auto input = reader.read(pending.empty() ? "$> " : "... ");
                if (input.status == InputStatus::End) {
                    if (!pending.empty()) {
                        std::cerr << "Error: incomplete input at end of session\n";
                        return EXIT_FAILURE;
                    }
                    if (reader.interactive())
                        std::cout << '\n';
                    return EXIT_SUCCESS;
                }
                if (input.status == InputStatus::Interrupted) {
                    pending.clear();
                    std::cerr << "KeyboardInterrupt\n";
                    continue;
                }
                auto command = trim(input.text);
                if (command == ":quit" || command == ":exit" || command == "exit()" || command == "quit()")
                    return EXIT_SUCCESS;
                if (command == ":reset") {
                    pending.clear();
                    interpreter = make_interpreter();
                    if (reader.interactive())
                        std::cout << "Session reset.\n";
                    continue;
                }
                if (command == ":cancel") {
                    pending.clear();
                    continue;
                }
                if (command == ":import" || command.starts_with(":import ") || command.starts_with(":import\t")) {
                    std::istringstream arguments(command.substr(7));
                    std::string module_path, declarations_path, extra;
                    if (!(arguments >> std::quoted(module_path, '"', '\0') >> std::quoted(declarations_path, '"', '\0')) || (arguments >> extra)) {
                        std::cerr << "Error: usage: :import \"library-path\" \"declarations-file\"\n";
                        continue;
                    }
                    try {
                        interpreter->import_module(module_path, declarations_path);
                    } catch (const std::exception &error) {
                        std::cerr << "Error: " << error.what() << '\n';
                    }
                    continue;
                }
                if (command == ":help") {
                    std::cout << ":quit / exit()  leave the REPL\n:reset          clear session state\n:cancel         discard incomplete input\n:import \"library-path\" \"declarations-file\"  load __import__ function prototypes\nExpressions display their value; a trailing ; suppresses it.\nStatements (if, while, for, switch, and blocks) execute without displaying a value.\nAt the ... prompt, enter a blank line to submit complete code.\nDeclare variables with C types and use (void) for parameterless functions.\n";
                    continue;
                }
                if (pending.empty() && command.empty())
                    continue;
                bool continuing = !pending.empty();
                pending += input.text + '\n';
                if (needs_more(pending) || (continuing && !command.empty()))
                    continue;
                try {
                    if (auto result = interpreter->submit(pending))
                        std::cout << *result << '\n';
                } catch (const std::exception &error) {
                    if (interrupted) {
                        interrupted = 0;
                        std::cerr << "KeyboardInterrupt\n";
                    } else
                        std::cerr << "Error: " << error.what() << '\n';
                }
                pending.clear();
            }
        } catch (const std::exception &error) {
            std::cerr << "REPL error: " << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
} // namespace mx
