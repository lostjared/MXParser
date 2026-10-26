#include "graphics.h"
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <climits>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>

namespace {
    int error(const char *message) {
        std::fprintf(stderr, "ls-graphics: %s\n", message);
        return 0;
    }
    int checked(bool success) { return success ? 1 : error(SDL_GetError()); }
    struct Image {
        SDL_Texture *texture;
        int width, height;
    };
    struct Graphics {
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        SDL_Texture *canvas = nullptr;
        std::map<int, Image> images;
        int next_image = 1;
        int key = 0, key_repeat = 0, mouse_x = 0, mouse_y = 0, mouse_button = 0;
        bool initialized = false, running = false;
        ~Graphics() { close(); }
        void close() {
            for (const auto &[id, image] : images)
                SDL_DestroyTexture(image.texture);
            images.clear();
            SDL_DestroyTexture(canvas);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            canvas = nullptr;
            renderer = nullptr;
            window = nullptr;
            running = false;
            key = key_repeat = mouse_x = mouse_y = mouse_button = 0;
            if (initialized)
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
            initialized = false;
        }
        bool ready() const {
            if (renderer)
                return true;
            error("call gfx_open first");
            return false;
        }
        Image *image(int id) {
            auto found = images.find(id);
            if (found == images.end()) {
                error("invalid or freed image ID");
                return nullptr;
            }
            return &found->second;
        }
    } graphics;
    bool coordinates(float x, float y) {
        if (std::isfinite(x) && std::isfinite(y))
            return true;
        error("coordinates must be finite");
        return false;
    }
    bool rectangle(float x, float y, float width, float height) {
        if (!coordinates(x, y) || !coordinates(width, height))
            return false;
        if (width >= 0 && height >= 0)
            return true;
        error("rectangle sizes must be nonnegative");
        return false;
    }
} // namespace

int gfx_open(const char *title, int width, int height) {
    if (!title || width <= 0 || height <= 0)
        return error("window title and positive dimensions are required");
    graphics.close();
    SDL_SetMainReady();
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        return error(SDL_GetError());
    graphics.initialized = true;
    if (!SDL_CreateWindowAndRenderer(title, width, height, 0, &graphics.window, &graphics.renderer)) {
        error(SDL_GetError());
        graphics.close();
        return 0;
    }
    graphics.canvas = SDL_CreateTexture(graphics.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
    if (!graphics.canvas || !SDL_SetTextureBlendMode(graphics.canvas, SDL_BLENDMODE_NONE) || !SDL_SetRenderTarget(graphics.renderer, graphics.canvas) || !SDL_SetRenderDrawColor(graphics.renderer, 0, 0, 0, 255) || !SDL_RenderClear(graphics.renderer) || !SDL_SetRenderDrawBlendMode(graphics.renderer, SDL_BLENDMODE_BLEND) || !SDL_SetRenderDrawColor(graphics.renderer, 255, 255, 255, 255)) {
        error(SDL_GetError());
        graphics.close();
        return 0;
    }
    graphics.running = true;
    return 1;
}
void gfx_close(void) { graphics.close(); }
int gfx_color(int red, int green, int blue, int alpha) {
    if (!graphics.ready())
        return 0;
    if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255 || alpha < 0 || alpha > 255)
        return error("color components must be between 0 and 255");
    return checked(SDL_SetRenderDrawColor(graphics.renderer, static_cast<Uint8>(red), static_cast<Uint8>(green), static_cast<Uint8>(blue), static_cast<Uint8>(alpha)));
}
int gfx_clear(void) { return graphics.ready() && checked(SDL_RenderClear(graphics.renderer)); }
int gfx_point(float x, float y) { return graphics.ready() && coordinates(x, y) && checked(SDL_RenderPoint(graphics.renderer, x, y)); }
int gfx_line(float x1, float y1, float x2, float y2) { return graphics.ready() && coordinates(x1, y1) && coordinates(x2, y2) && checked(SDL_RenderLine(graphics.renderer, x1, y1, x2, y2)); }
int gfx_rect(float x, float y, float width, float height) {
    SDL_FRect rect{x, y, width, height};
    return graphics.ready() && rectangle(x, y, width, height) && checked(SDL_RenderRect(graphics.renderer, &rect));
}
int gfx_fill_rect(float x, float y, float width, float height) {
    SDL_FRect rect{x, y, width, height};
    return graphics.ready() && rectangle(x, y, width, height) && checked(SDL_RenderFillRect(graphics.renderer, &rect));
}
int gfx_text(float x, float y, const char *text) {
    if (!text)
        return error("text must not be null");
    return graphics.ready() && coordinates(x, y) && checked(SDL_RenderDebugText(graphics.renderer, x, y, text));
}
int gfx_present(void) {
    if (!graphics.ready())
        return 0;
    bool success = SDL_SetRenderTarget(graphics.renderer, nullptr);
    if (success)
        success = SDL_RenderTexture(graphics.renderer, graphics.canvas, nullptr, nullptr) && SDL_RenderPresent(graphics.renderer);
    int result = checked(success);
    return checked(SDL_SetRenderTarget(graphics.renderer, graphics.canvas)) && result;
}
int gfx_image_load(const char *filename) {
    if (!graphics.ready())
        return 0;
    if (!filename || !*filename)
        return error("an image filename is required");
    if (graphics.next_image == INT_MAX)
        return error("image IDs exhausted");
    SDL_Surface *loaded = nullptr;
    const char *extension = SDL_strrchr(filename, '.');
    if (extension && SDL_strcasecmp(extension, ".png") == 0) {
#if SDL_VERSION_ATLEAST(3, 4, 0)
        loaded = SDL_LoadPNG(filename);
#else
        return error("PNG images require SDL 3.4 or newer; BMP is available with SDL 3.2");
#endif
    } else
        loaded = SDL_LoadBMP(filename);
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(loaded, SDL_DestroySurface);
    if (!surface)
        return error(SDL_GetError());
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture(SDL_CreateTextureFromSurface(graphics.renderer, surface.get()), SDL_DestroyTexture);
    if (!texture || !SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND))
        return error(SDL_GetError());
    int id = graphics.next_image;
    try {
        graphics.images.emplace(id, Image{texture.get(), surface->w, surface->h});
    } catch (...) {
        return error("cannot allocate image handle");
    }
    texture.release();
    ++graphics.next_image;
    return id;
}
int gfx_image_draw(int image, float x, float y, float width, float height) {
    if (!graphics.ready() || !rectangle(x, y, width, height))
        return 0;
    auto found = graphics.image(image);
    if (!found)
        return 0;
    SDL_FRect rect{x, y, width == 0 ? static_cast<float>(found->width) : width, height == 0 ? static_cast<float>(found->height) : height};
    return checked(SDL_RenderTexture(graphics.renderer, found->texture, nullptr, &rect));
}
int gfx_image_width(int image) {
    auto found = graphics.image(image);
    return found ? found->width : 0;
}
int gfx_image_height(int image) {
    auto found = graphics.image(image);
    return found ? found->height : 0;
}
int gfx_image_free(int image) {
    auto found = graphics.image(image);
    if (!found)
        return 0;
    SDL_DestroyTexture(found->texture);
    graphics.images.erase(image);
    return 1;
}
int gfx_poll(void) {
    if (!graphics.ready())
        return -1;
    SDL_Event event;
    auto window = SDL_GetWindowID(graphics.window);
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == window)) {
            graphics.running = false;
            return 1;
        }
        if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && event.key.windowID == window) {
            graphics.key = static_cast<int>(event.key.key);
            graphics.key_repeat = event.key.repeat ? 1 : 0;
            return event.type == SDL_EVENT_KEY_DOWN ? 2 : 3;
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION && event.motion.windowID == window) {
            graphics.mouse_x = static_cast<int>(event.motion.x);
            graphics.mouse_y = static_cast<int>(event.motion.y);
            return 4;
        }
        if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) && event.button.windowID == window) {
            graphics.mouse_x = static_cast<int>(event.button.x);
            graphics.mouse_y = static_cast<int>(event.button.y);
            graphics.mouse_button = event.button.button;
            return event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? 5 : 6;
        }
        if (event.type == SDL_EVENT_WINDOW_EXPOSED && event.window.windowID == window)
            gfx_present();
    }
    return 0;
}
int gfx_running(void) { return graphics.running ? 1 : 0; }
int gfx_key(void) { return graphics.key; }
int gfx_key_repeat(void) { return graphics.key_repeat; }
int gfx_key_down(int key) {
    if (!graphics.ready() || key < 0)
        return 0;
    int count = 0;
    auto state = SDL_GetKeyboardState(&count);
    auto scan = SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(key), nullptr);
    return scan > SDL_SCANCODE_UNKNOWN && scan < count && state[scan];
}
int gfx_mouse_x(void) { return graphics.mouse_x; }
int gfx_mouse_y(void) { return graphics.mouse_y; }
int gfx_mouse_button(void) { return graphics.mouse_button; }
int gfx_delay(int milliseconds) {
    if (milliseconds < 0)
        return error("delay must be nonnegative");
    SDL_Delay(static_cast<Uint32>(milliseconds));
    return 1;
}
unsigned long long gfx_ticks(void) { return SDL_GetTicks(); }
int gfx_message(const char *title, const char *message) {
    if (!graphics.ready())
        return 0;
    if (!title || !message)
        return error("message title and text must not be null");
    return checked(SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title, message, graphics.window));
}
