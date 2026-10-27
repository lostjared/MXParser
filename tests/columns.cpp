#include <MXParser/interpreter.hpp>
#include <MXParser/parser.hpp>
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

namespace {
    void check(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }
    void value(mx::Interpreter &interpreter, const std::string &source, const std::string &expected) {
        auto actual = interpreter.submit(source);
        check(actual == expected, source + ": expected " + expected + ", got " + actual.value_or("<none>"));
    }
    int call(mx::Interpreter &interpreter, const std::string &name, const std::vector<mx::ExecutionValue> &arguments = {}) { return static_cast<int>(std::get<int64_t>(interpreter.call(name, arguments))); }
    void reset(mx::Interpreter &interpreter) { interpreter.call("columns_reset", {uint64_t{1}}); }
    SDL_Window *window() {
        int count = 0;
        SDL_Window **windows = SDL_GetWindows(&count);
        auto result = windows && count == 1 ? windows[0] : nullptr;
        SDL_free(windows);
        check(result != nullptr, "Columns window missing");
        return result;
    }
    void key(mx::Interpreter &interpreter, SDL_Keycode code, bool repeat = false) {
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.windowID = SDL_GetWindowID(window());
        event.key.key = code;
        event.key.repeat = repeat;
        check(SDL_PushEvent(&event), SDL_GetError());
        call(interpreter, "columns_frame");
    }
} // namespace
int main(int argc, char **argv) {
    if (argc != 4)
        return EXIT_FAILURE;
    try {
        std::string root = argv[2];
        std::filesystem::create_directories(argv[3]);
        mx::Interpreter interpreter({0, 256, 1024 * 1024});
        interpreter.import_module(argv[1], root + "/examples/graphics/graphics.ls-h");
        std::ifstream file(root + "/examples/graphics/columns.c");
        mx::Parser parser(file);
        auto program = parser.parse_program();
        interpreter.load(*program);
        value(interpreter, "gfx_open(\"ls-C Columns tests\",544,616)", "1");
        reset(interpreter);
        value(interpreter, "columns_find_matches()", "0");
        for (int i = 0; i < 100; ++i) {
            int gem = call(interpreter, "columns_random_gem");
            check(gem >= 1 && gem <= 6, "invalid random gem");
        }
        interpreter.submit("columns_piece[0]=1; columns_piece[1]=2; columns_piece[2]=3;");
        interpreter.call("columns_cycle", {int64_t{1}});
        value(interpreter, "columns_piece[0]==3 && columns_piece[1]==1 && columns_piece[2]==2", "1");
        interpreter.call("columns_cycle", {int64_t{-1}});
        value(interpreter, "columns_piece[0]==1 && columns_piece[1]==2 && columns_piece[2]==3", "1");
        interpreter.submit("columns_x=0; columns_y=0;");
        check(call(interpreter, "columns_move", {int64_t{-1}}) == 0, "column crossed left wall");
        check(call(interpreter, "columns_move", {int64_t{1}}) == 1, "column could not move right");
        value(interpreter, "columns_fits(6,0)", "0");
        value(interpreter, "columns_fits(2,10)", "1");
        value(interpreter, "columns_fits(2,11)", "0");
        interpreter.submit("columns_board[4][2]=1;");
        value(interpreter, "columns_fits(2,2)", "0");

        reset(interpreter);
        interpreter.submit("columns_board[12][0]=1; columns_board[12][1]=1;");
        check(call(interpreter, "columns_find_matches") == 0, "two gems incorrectly matched");
        interpreter.submit("columns_board[12][2]=1; columns_board[12][3]=1; columns_board[12][4]=1;");
        check(call(interpreter, "columns_find_matches") == 5, "long horizontal match failed");
        reset(interpreter);
        interpreter.submit("columns_board[5][2]=4; columns_board[6][1]=4; columns_board[6][2]=4; columns_board[6][3]=4; columns_board[7][2]=4;");
        check(call(interpreter, "columns_find_matches") == 5, "crossing matches were missed or counted twice");
        reset(interpreter);
        interpreter.submit("columns_board[2][0]=2; columns_board[3][1]=2; columns_board[4][2]=2; columns_board[8][2]=3; columns_board[9][1]=3; columns_board[10][0]=3;");
        check(call(interpreter, "columns_find_matches") == 6, "diagonal matches failed");
        reset(interpreter);
        interpreter.submit("columns_board[10][0]=1; columns_board[11][0]=1; columns_board[12][0]=1; columns_board[12][3]=2; columns_board[12][4]=2; columns_board[12][5]=2;");
        check(call(interpreter, "columns_find_matches") == 6, "simultaneous separate matches failed");
        reset(interpreter);
        interpreter.submit("columns_board[2][2]=3; columns_board[6][2]=1; columns_board[11][2]=4;");
        interpreter.call("columns_compact");
        value(interpreter, "columns_board[12][2]==4 && columns_board[11][2]==1 && columns_board[10][2]==3 && columns_board[2][2]==0", "1");

        reset(interpreter);
        interpreter.submit("columns_board[12][0]=1; columns_board[12][1]=1; columns_board[12][2]=1; columns_board[10][0]=2; columns_board[11][1]=2; columns_board[10][2]=2; columns_x=5; columns_y=10; columns_piece[0]=3; columns_piece[1]=4; columns_piece[2]=5;");
        interpreter.call("columns_lock");
        value(interpreter, "columns_phase==1 && columns_chain==1 && columns_score==0", "1");
        interpreter.submit("columns_update(columns_clear_at-1ULL);");
        value(interpreter, "columns_gems", "0");
        interpreter.call("columns_key", {int64_t{'a'}, int64_t{0}});
        value(interpreter, "columns_x", "5");
        interpreter.submit("columns_resolve(columns_clear_at);");
        value(interpreter, "columns_phase==1 && columns_chain==2 && columns_gems==3 && columns_score==30 && columns_board[12][0]==2", "1");
        interpreter.call("columns_key", {int64_t{'p'}, int64_t{0}});
        interpreter.submit("columns_update(columns_clear_at+10000ULL);");
        value(interpreter, "columns_gems==3 && columns_paused", "1");
        interpreter.call("columns_key", {int64_t{'p'}, int64_t{0}});
        interpreter.submit("columns_resolve(columns_clear_at);");
        value(interpreter, "columns_phase==0 && columns_chain==2 && columns_best_chain==2 && columns_gems==6 && columns_score==90 && columns_board[12][0]==0", "1");
        reset(interpreter);
        interpreter.submit("columns_gems=29; columns_board[12][0]=1; columns_board[12][1]=1; columns_board[12][2]=1; columns_find_matches(); columns_phase=1; columns_chain=1; columns_clear_at=0;");
        interpreter.call("columns_resolve", {uint64_t{1}});
        value(interpreter, "columns_level==2 && columns_gems==32 && columns_score==30 && columns_interval()==655", "1");

        reset(interpreter);
        interpreter.submit("columns_piece[0]=1; columns_piece[1]=2; columns_piece[2]=3; columns_x=2; columns_y=0;");
        interpreter.call("columns_hard_drop");
        value(interpreter, "columns_score==20 && columns_board[10][2]==1 && columns_board[11][2]==2 && columns_board[12][2]==3 && !columns_over", "1");
        reset(interpreter);
        interpreter.submit("columns_y=-1;");
        interpreter.call("columns_lock");
        value(interpreter, "columns_over==1 && columns_board[0][2]==0", "1");
        check(call(interpreter, "columns_frame") == 1, "game over exited instead of waiting for restart");
        key(interpreter, SDLK_R);
        value(interpreter, "!columns_over && columns_score==0 && columns_running", "1");
        interpreter.submit("columns_board[0][2]=1;");
        interpreter.call("columns_spawn");
        value(interpreter, "columns_over", "1");

        reset(interpreter);
        interpreter.submit("columns_piece[0]=1; columns_piece[1]=2; columns_piece[2]=3; columns_y=3;");
        key(interpreter, SDLK_LEFT);
        value(interpreter, "columns_x", "1");
        key(interpreter, SDLK_UP);
        value(interpreter, "columns_piece[0]==3 && columns_piece[1]==1 && columns_piece[2]==2", "1");
        key(interpreter, SDLK_UP, true);
        value(interpreter, "columns_piece[0]", "3");
        key(interpreter, SDLK_P);
        interpreter.submit("columns_update(columns_fall_at+10000ULL);");
        value(interpreter, "columns_paused && columns_y==3", "1");
        key(interpreter, SDLK_P);
        interpreter.submit("columns_update(columns_fall_at+1ULL);");
        value(interpreter, "columns_y", "4");
        key(interpreter, SDLK_SPACE);
        value(interpreter, "columns_score>0", "1");
        key(interpreter, SDLK_R);
        value(interpreter, "columns_score==0 && !columns_over", "1");

        interpreter.submit("int heights[6]={4,7,3,6,5,2};");
        interpreter.submit("for(int x=0;x<6;++x) for(int y=13-heights[x];y<13;++y) columns_board[y][x]=(x+2*y)%6+1; columns_piece[0]=1; columns_piece[1]=3; columns_piece[2]=5; columns_next[0]=2; columns_next[1]=4; columns_next[2]=6; columns_x=2; columns_y=3; columns_score=3240; columns_gems=64; columns_level=3; columns_chain=2; columns_best_chain=4;");
        check(call(interpreter, "columns_render") == 1, "Columns rendering failed");
        std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(SDL_RenderReadPixels(SDL_GetRenderer(window()), nullptr), SDL_DestroySurface);
        check(bool(surface), SDL_GetError());
        Uint8 red, green, blue, alpha;
        check(SDL_ReadSurfacePixel(surface.get(), 35, 570, &red, &green, &blue, &alpha), SDL_GetError());
        check(red == 245 && green == 85 && blue == 115 && alpha == 255, "colored gem not rendered");
#if SDL_VERSION_ATLEAST(3, 4, 0)
        check(SDL_SavePNG(surface.get(), (std::string(argv[3]) + "/columns.png").c_str()), SDL_GetError());
#else
        check(SDL_SaveBMP(surface.get(), (std::string(argv[3]) + "/columns.bmp").c_str()), SDL_GetError());
#endif
        key(interpreter, SDLK_ESCAPE);
        value(interpreter, "columns_running", "0");
        interpreter.call("gfx_close");
        std::cout << "ls-C Columns matches, cascades, scoring, input, rendering, and restart verified\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
