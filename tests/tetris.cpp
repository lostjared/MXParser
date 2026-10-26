#include <MXParser/interpreter.hpp>
#include <MXParser/parser.hpp>
#include <SDL3/SDL.h>
#include <array>
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
    void reset(mx::Interpreter &interpreter) { interpreter.call("tetris_reset", {uint64_t{1}}); }
    SDL_Window *window() {
        int count = 0;
        SDL_Window **windows = SDL_GetWindows(&count);
        auto result = windows && count == 1 ? windows[0] : nullptr;
        SDL_free(windows);
        check(result != nullptr, "Tetris window missing");
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
        call(interpreter, "tetris_frame");
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
        std::ifstream file(root + "/examples/graphics/tetris.c");
        mx::Parser parser(file);
        auto program = parser.parse_program();
        interpreter.load(*program);
        value(interpreter, "gfx_open(\"ls-C Tetris tests\",568,656)", "1");
        reset(interpreter);
        for (int bag = 0; bag < 2; ++bag) {
            interpreter.submit("tetris_bag_index=7;");
            std::array<int, 7> counts{};
            for (int i = 0; i < 7; ++i) {
                int piece = call(interpreter, "tetris_take_piece");
                check(piece >= 0 && piece < 7, "invalid piece kind");
                ++counts[piece];
            }
            for (int count : counts)
                check(count == 1, "seven-piece bag repeats or omits a piece");
        }
        reset(interpreter);
        interpreter.submit("tetris_kind=1; tetris_rotation=0; tetris_x=-1; tetris_y=0;");
        check(call(interpreter, "tetris_move", {int64_t{-1}}) == 0, "piece crossed left wall");
        check(call(interpreter, "tetris_move", {int64_t{1}}) == 1, "piece could not move right");
        value(interpreter, "tetris_fits(1,0,3,18)", "1");
        value(interpreter, "tetris_fits(1,0,3,19)", "0");
        interpreter.submit("tetris_board[1][4]=7;");
        value(interpreter, "tetris_fits(1,0,3,0)", "0");
        reset(interpreter);
        interpreter.submit("tetris_kind=0; tetris_rotation=1; tetris_x=-2; tetris_y=3;");
        check(call(interpreter, "tetris_rotate", {int64_t{1}}) == 1, "wall kick failed");
        value(interpreter, "tetris_x==0 && tetris_rotation==2", "1");
        interpreter.submit("tetris_rotation=0; tetris_x=3; tetris_y=18;");
        check(call(interpreter, "tetris_rotate", {int64_t{1}}) == 1, "floor kick failed");
        value(interpreter, "tetris_y", "16");
        interpreter.submit("for(int y=0;y<20;++y) for(int x=0;x<10;++x) tetris_board[y][x]=1; tetris_kind=2; tetris_rotation=0; tetris_x=3; tetris_y=8;");
        check(call(interpreter, "tetris_rotate", {int64_t{1}}) == 0, "rotation passed through occupied cells");

        reset(interpreter);
        interpreter.submit("for(int x=0;x<10;++x) { tetris_board[19][x]=1; tetris_board[18][x]=2; } tetris_board[17][2]=4; tetris_board[16][4]=5;");
        check(call(interpreter, "tetris_clear_lines") == 2, "double line clear failed");
        value(interpreter, "tetris_lines==2 && tetris_score==300 && tetris_board[19][2]==4 && tetris_board[18][4]==5 && tetris_board[0][0]==0", "1");
        reset(interpreter);
        interpreter.submit("for(int y=16;y<20;++y) for(int x=0;x<10;++x) tetris_board[y][x]=1;");
        check(call(interpreter, "tetris_clear_lines") == 4, "four-line clear failed");
        value(interpreter, "tetris_score==800 && tetris_lines==4", "1");
        reset(interpreter);
        interpreter.submit("tetris_lines=9; for(int x=0;x<10;++x) tetris_board[19][x]=1;");
        call(interpreter, "tetris_clear_lines");
        value(interpreter, "tetris_lines==10 && tetris_level==2 && tetris_interval()==605", "1");

        reset(interpreter);
        interpreter.submit("tetris_kind=1; tetris_rotation=0; tetris_x=3; tetris_y=18; for(int x=0;x<10;++x) if(x!=4 && x!=5) tetris_board[19][x]=3;");
        interpreter.call("tetris_key", {int64_t{'s'}, int64_t{0}});
        value(interpreter, "tetris_lines==1 && tetris_score==100 && !tetris_over", "1");
        reset(interpreter);
        interpreter.submit("tetris_kind=1; tetris_rotation=0; tetris_x=3; tetris_y=0;");
        interpreter.call("tetris_hard_drop");
        value(interpreter, "tetris_score==36 && tetris_board[18][4]==2 && tetris_board[19][5]==2", "1");
        reset(interpreter);
        auto original = interpreter.submit("tetris_kind");
        interpreter.call("tetris_hold");
        check(interpreter.submit("tetris_held") == original, "hold did not save the active piece");
        auto active = interpreter.submit("tetris_kind");
        interpreter.call("tetris_hold");
        check(interpreter.submit("tetris_kind") == active, "hold allowed twice before locking");
        interpreter.call("tetris_hard_drop");
        value(interpreter, "tetris_can_hold", "1");
        interpreter.call("tetris_hold");
        check(interpreter.submit("tetris_kind") == original, "held piece did not swap back");
        reset(interpreter);
        interpreter.submit("tetris_kind=1; tetris_rotation=0; tetris_x=3; tetris_y=-1;");
        interpreter.call("tetris_lock");
        value(interpreter, "tetris_over==1 && tetris_board[0][4]==0", "1");
        check(call(interpreter, "tetris_frame") == 1, "game over exited instead of waiting for restart");
        interpreter.call("tetris_key", {int64_t{'r'}, int64_t{0}});
        value(interpreter, "!tetris_over && tetris_score==0 && tetris_running", "1");

        reset(interpreter);
        interpreter.submit("tetris_kind=1; tetris_rotation=0; tetris_x=3; tetris_y=3;");
        key(interpreter, SDLK_LEFT);
        value(interpreter, "tetris_x", "2");
        key(interpreter, SDLK_SPACE, true);
        value(interpreter, "tetris_score", "0");
        key(interpreter, SDLK_P);
        value(interpreter, "tetris_paused", "1");
        interpreter.submit("tetris_update(tetris_fall_at+10000ULL);");
        value(interpreter, "tetris_y", "3");
        key(interpreter, SDLK_P);
        value(interpreter, "tetris_paused", "0");
        interpreter.submit("tetris_update(tetris_fall_at+1ULL);");
        value(interpreter, "tetris_y", "4");
        key(interpreter, SDLK_SPACE);
        value(interpreter, "tetris_score>0", "1");
        key(interpreter, SDLK_R);
        value(interpreter, "tetris_score==0 && !tetris_over", "1");

        interpreter.submit("int heights[10]={4,7,3,5,2,6,4,3,5,2};");
        interpreter.submit("for(int x=0;x<10;++x) for(int y=20-heights[x];y<20;++y) tetris_board[y][x]=(x+y)%7+1; tetris_kind=2; tetris_rotation=0; tetris_x=3; tetris_y=5; tetris_next=0; tetris_held=4; tetris_score=12840; tetris_lines=24; tetris_level=3;");
        check(call(interpreter, "tetris_render") == 1, "Tetris rendering failed");
        auto renderer = SDL_GetRenderer(window());
        std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(SDL_RenderReadPixels(renderer, nullptr), SDL_DestroySurface);
        check(bool(surface), SDL_GetError());
        Uint8 red, green, blue, alpha;
        check(SDL_ReadSurfacePixel(surface.get(), 35, 610, &red, &green, &blue, &alpha), SDL_GetError());
        check(red == 80 && green == 210 && blue == 130 && alpha == 255, "colored board block not rendered");
#if SDL_VERSION_ATLEAST(3, 4, 0)
        check(SDL_SavePNG(surface.get(), (std::string(argv[3]) + "/tetris.png").c_str()), SDL_GetError());
#else
        check(SDL_SaveBMP(surface.get(), (std::string(argv[3]) + "/tetris.bmp").c_str()), SDL_GetError());
#endif
        key(interpreter, SDLK_ESCAPE);
        value(interpreter, "tetris_running", "0");
        interpreter.call("gfx_close");
        std::cout << "ls-C Tetris logic, rendering, keyboard controls, pause, restart, and quit verified\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
