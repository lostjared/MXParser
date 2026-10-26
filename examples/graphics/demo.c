int graphics_demo(int frames, int pause) {
    if (!gfx_open("ls-C SDL3 graphics demo", 640, 400))
        return 1;
    int image = gfx_image_load("examples/graphics/checker.bmp");
    if (!image) {
        gfx_close();
        return 2;
    }
    float x = 260;
    int running = 1;
    for (int frame = 0; frame < frames && running; ++frame) {
        int event;
        while ((event = gfx_poll()) > 0) {
            if (event == 1 || (event == 2 && gfx_key() == 27))
                running = 0;
            if (event == 5 && gfx_mouse_button() == 1)
                x = gfx_mouse_x();
        }
        if (!running)
            break;
        if (gfx_key_down('a'))
            x -= 3;
        if (gfx_key_down('d'))
            x += 3;
        if (x < 0)
            x = 0;
        if (x > 520)
            x = 520;

        gfx_color(18, 24, 38, 255);
        gfx_clear();
        gfx_color(230, 235, 245, 255);
        gfx_text(24, 24, "SDL3 from the ls-C interpreter");
        gfx_text(24, 48, "A/D or click: move. Escape or close: quit.");
        gfx_image_draw(image, 32, 104, 128, 128);
        gfx_color(65, 180, 245, 255);
        gfx_fill_rect(x, 150, 120, 80);
        gfx_color(250, 190, 70, 255);
        gfx_rect(220, 120, 220, 150);
        gfx_line(24, 310, 616, 310);
        for (int px = 24; px < 616; px += 12)
            gfx_point(px, 340);
        gfx_present();
        gfx_delay(pause);
    }
    gfx_image_free(image);
    gfx_close();
    return 0;
}

int main(void) { return graphics_demo(600, 16); }
