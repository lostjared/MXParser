#include <MXParser/interpreter.hpp>
#include <MXParser/parser.hpp>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

namespace {
    void check(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }
    std::string c_string(const std::string &text) {
        std::ostringstream stream;
        stream << std::quoted(text);
        return stream.str();
    }
    void value(mx::Interpreter &interpreter, const std::string &source, const std::string &expected) {
        auto result = interpreter.submit(source);
        check(result == expected, "graphics result: " + source + ", expected " + expected + ", got " + result.value_or("<none>"));
    }
    void event_value(mx::Interpreter &interpreter, const std::string &expected) {
        for (int attempt = 0; attempt < 4; ++attempt) {
            auto result = interpreter.submit("gfx_poll()");
            if (result != "0") {
                check(result == expected, "wrong graphics event: expected " + expected + ", got " + result.value_or("<none>"));
                return;
            }
        }
        throw std::runtime_error("expected graphics event: " + expected);
    }
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
    Surface pixels(SDL_Renderer *renderer) {
        Surface surface(SDL_RenderReadPixels(renderer, nullptr), SDL_DestroySurface);
        check(bool(surface), SDL_GetError());
        return surface;
    }
    void pixel(SDL_Surface *surface, int x, int y, int red, int green, int blue) {
        Uint8 r, g, b, a;
        check(SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, &a), SDL_GetError());
        check(r == red && g == green && b == blue && a == 255, "wrong rendered pixel at " + std::to_string(x) + "," + std::to_string(y));
    }
    SDL_Window *window() {
        int count = 0;
        SDL_Window **windows = SDL_GetWindows(&count);
        SDL_Window *result = windows && count == 1 ? windows[0] : nullptr;
        SDL_free(windows);
        check(result != nullptr, "expected one graphics window");
        return result;
    }
} // namespace
int main(int argc, char **argv) {
    if (argc != 5)
        return EXIT_FAILURE;
    try {
        std::string directory = argv[4];
        std::filesystem::create_directories(directory);
        SDL_SetMainReady();
        check(SDL_InitSubSystem(SDL_INIT_VIDEO), SDL_GetError());
        {
            mx::Interpreter interpreter;
            interpreter.import_module(argv[1], std::string(argv[2]) + "/examples/graphics/graphics.ls-h");
            value(interpreter, "gfx_point(1,1)", "0");
            value(interpreter, "gfx_poll()", "-1");
            value(interpreter, "gfx_open(\"graphics test\",0,64)", "0");
            value(interpreter, "gfx_open(\"graphics test\",64,64)", "1");
            auto renderer = SDL_GetRenderer(window());
            check(renderer && SDL_GetRenderTarget(renderer), "persistent graphics canvas missing");
            value(interpreter, "gfx_color(10,20,30,255)", "1");
            value(interpreter, "gfx_clear()", "1");
            value(interpreter, "gfx_color(255,0,0,255)", "1");
            value(interpreter, "gfx_fill_rect(4,4,12,12)", "1");
            value(interpreter, "gfx_color(0,255,0,255)", "1");
            value(interpreter, "gfx_point(2,2)", "1");
            value(interpreter, "gfx_color(0,0,255,255)", "1");
            value(interpreter, "gfx_line(20,20,30,20)", "1");
            value(interpreter, "gfx_color(255,255,0,255)", "1");
            value(interpreter, "gfx_rect(32,32,10,10)", "1");
            auto surface = pixels(renderer);
            pixel(surface.get(), 5, 5, 255, 0, 0);
            pixel(surface.get(), 2, 2, 0, 255, 0);
            pixel(surface.get(), 25, 20, 0, 0, 255);
            pixel(surface.get(), 32, 32, 255, 255, 0);
            pixel(surface.get(), 63, 63, 10, 20, 30);
            value(interpreter, "gfx_present()", "1");
            value(interpreter, "gfx_color(255,0,255,255)", "1");
            value(interpreter, "gfx_point(3,3)", "1");
            value(interpreter, "gfx_present()", "1");
            surface = pixels(renderer);
            pixel(surface.get(), 5, 5, 255, 0, 0);
            pixel(surface.get(), 3, 3, 255, 0, 255);
            value(interpreter, "gfx_color(210,20,30,128)", "1");
            value(interpreter, "gfx_fill_rect(56,56,4,4)", "1");
            surface = pixels(renderer);
            Uint8 red, green, blue, alpha;
            check(SDL_ReadSurfacePixel(surface.get(), 57, 57, &red, &green, &blue, &alpha), SDL_GetError());
            check(red >= 109 && red <= 111 && green >= 19 && green <= 21 && blue >= 29 && blue <= 31 && alpha == 255, "alpha blending failed: " + std::to_string(red) + "," + std::to_string(green) + "," + std::to_string(blue) + "," + std::to_string(alpha));
            value(interpreter, "gfx_color(256,0,0,255)", "0");
            value(interpreter, "gfx_rect(1,1,-1,1)", "0");
            value(interpreter, "gfx_delay(-1)", "0");
            value(interpreter, "gfx_delay(0)", "1");
            interpreter.submit("int picture=gfx_image_load(" + c_string(argv[3]) + ");");
            value(interpreter, "picture>0", "1");
            value(interpreter, "gfx_image_width(picture)", "32");
            value(interpreter, "gfx_image_height(picture)", "32");
            value(interpreter, "gfx_image_draw(picture,16,16,0,0)", "1");
            surface = pixels(renderer);
            pixel(surface.get(), 17, 17, 245, 180, 50);
            pixel(surface.get(), 25, 17, 45, 100, 220);
            value(interpreter, "gfx_image_draw(picture,48,0,16,16)", "1");
            surface = pixels(renderer);
            pixel(surface.get(), 49, 1, 245, 180, 50);
            pixel(surface.get(), 53, 1, 45, 100, 220);
            check(SDL_SaveBMP(surface.get(), (directory + "/graphics.bmp").c_str()), SDL_GetError());
#if SDL_VERSION_ATLEAST(3, 4, 0)
            auto png = directory + "/graphics.png";
            check(SDL_SavePNG(surface.get(), png.c_str()), SDL_GetError());
            interpreter.submit("int png=gfx_image_load(" + c_string(png) + ");");
            value(interpreter, "gfx_image_width(png)", "64");
            value(interpreter, "gfx_image_draw(png,0,0,64,64)", "1");
            surface = pixels(renderer);
            pixel(surface.get(), 17, 17, 245, 180, 50);
            value(interpreter, "gfx_image_free(png)", "1");
#endif
            value(interpreter, "gfx_text(0,48,\"hello\")", "1");
            value(interpreter, "gfx_image_load(" + c_string(std::string(argv[3]) + ".missing") + ")", "0");
            value(interpreter, "gfx_image_free(picture)", "1");
            value(interpreter, "gfx_image_draw(picture,0,0,1,1)", "0");
            value(interpreter, "gfx_image_free(picture)", "0");

            auto id = SDL_GetWindowID(window());
            SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
            value(interpreter, "gfx_poll()", "0");
            SDL_Event event{};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.windowID = id;
            event.key.key = SDLK_ESCAPE;
            check(SDL_PushEvent(&event), SDL_GetError());
            event_value(interpreter, "2");
            value(interpreter, "gfx_key()", "27");
            event.type = SDL_EVENT_KEY_UP;
            check(SDL_PushEvent(&event), SDL_GetError());
            event_value(interpreter, "3");
            event = {};
            event.type = SDL_EVENT_MOUSE_MOTION;
            event.motion.windowID = id;
            event.motion.x = 42;
            event.motion.y = 24;
            check(SDL_PushEvent(&event), SDL_GetError());
            event_value(interpreter, "4");
            value(interpreter, "gfx_mouse_x()", "42");
            value(interpreter, "gfx_mouse_y()", "24");
            event = {};
            event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            event.button.windowID = id;
            event.button.button = SDL_BUTTON_LEFT;
            event.button.x = 10;
            event.button.y = 20;
            check(SDL_PushEvent(&event), SDL_GetError());
            event_value(interpreter, "5");
            value(interpreter, "gfx_mouse_button()", "1");
            value(interpreter, "gfx_mouse_x()", "10");
            event.type = SDL_EVENT_MOUSE_BUTTON_UP;
            check(SDL_PushEvent(&event), SDL_GetError());
            event_value(interpreter, "6");
            value(interpreter, "gfx_key_down(-1)", "0");
            event = {};
            event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            event.window.windowID = id;
            check(SDL_PushEvent(&event), SDL_GetError());
            event_value(interpreter, "1");
            value(interpreter, "gfx_running()", "0");
            interpreter.submit("int stale=gfx_image_load(" + c_string(argv[3]) + ");");
            interpreter.submit("gfx_close();");
            check(SDL_WasInit(SDL_INIT_VIDEO) != 0, "graphics module shut down the host's SDL video subsystem");
            value(interpreter, "gfx_open(\"reopened\",64,64)", "1");
            interpreter.submit("int fresh=gfx_image_load(" + c_string(argv[3]) + ");");
            value(interpreter, "fresh>stale", "1");
            value(interpreter, "gfx_image_draw(stale,0,0,1,1)", "0");
            event = {};
            event.type = SDL_EVENT_QUIT;
            check(SDL_PushEvent(&event), SDL_GetError());
            event_value(interpreter, "1");
            value(interpreter, "gfx_running()", "0");

            std::ifstream input(std::string(argv[2]) + "/examples/graphics/demo.c");
            mx::Parser parser(input);
            auto program = parser.parse_program();
            interpreter.load(*program);
            check(std::get<int64_t>(interpreter.call("graphics_demo", {int64_t{2}, int64_t{0}})) == 0, "normal interpreter graphics demo failed");
            value(interpreter, "gfx_open(\"cleanup test\",64,64)", "1");
            interpreter.submit("gfx_image_load(" + c_string(argv[3]) + ");");
        }
        int count = 0;
        SDL_free(SDL_GetWindows(&count));
        check(count == 0, "module unload did not close its window");
        check(SDL_WasInit(SDL_INIT_VIDEO) != 0, "module unload shut down the host's video subsystem");
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        std::cout << "SDL3 graphics, images, persistent pixels, events, imports, and cleanup verified\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
