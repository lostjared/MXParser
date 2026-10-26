#pragma once

#ifdef _WIN32
#ifdef LS_GRAPHICS_BUILD
#define GRAPHICS_API __declspec(dllexport)
#else
#define GRAPHICS_API __declspec(dllimport)
#endif
#else
#define GRAPHICS_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif
GRAPHICS_API int gfx_open(const char *title, int width, int height);
GRAPHICS_API void gfx_close(void);
GRAPHICS_API int gfx_color(int red, int green, int blue, int alpha);
GRAPHICS_API int gfx_clear(void);
GRAPHICS_API int gfx_point(float x, float y);
GRAPHICS_API int gfx_line(float x1, float y1, float x2, float y2);
GRAPHICS_API int gfx_rect(float x, float y, float width, float height);
GRAPHICS_API int gfx_fill_rect(float x, float y, float width, float height);
GRAPHICS_API int gfx_text(float x, float y, const char *text);
GRAPHICS_API int gfx_present(void);
GRAPHICS_API int gfx_image_load(const char *filename);
GRAPHICS_API int gfx_image_draw(int image, float x, float y, float width, float height);
GRAPHICS_API int gfx_image_width(int image);
GRAPHICS_API int gfx_image_height(int image);
GRAPHICS_API int gfx_image_free(int image);
GRAPHICS_API int gfx_poll(void);
GRAPHICS_API int gfx_running(void);
GRAPHICS_API int gfx_key(void);
GRAPHICS_API int gfx_key_down(int key);
GRAPHICS_API int gfx_mouse_x(void);
GRAPHICS_API int gfx_mouse_y(void);
GRAPHICS_API int gfx_mouse_button(void);
GRAPHICS_API int gfx_delay(int milliseconds);
GRAPHICS_API int gfx_message(const char *title, const char *message);
#ifdef __cplusplus
}
#endif
