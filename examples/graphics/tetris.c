unsigned int tetris_shapes[7][4] = {{0x00f0, 0x4444, 0x0f00, 0x2222}, {0x0066, 0x0066, 0x0066, 0x0066}, {0x0072, 0x0262, 0x0270, 0x0232}, {0x0071, 0x0226, 0x0470, 0x0322}, {0x0074, 0x0622, 0x0170, 0x0223}, {0x0036, 0x0462, 0x0360, 0x0231}, {0x0063, 0x0264, 0x0630, 0x0132}};
int tetris_colors[7][3] = {{55, 210, 235}, {250, 205, 65}, {175, 105, 235}, {75, 120, 245}, {245, 155, 60}, {80, 210, 130}, {240, 85, 110}};
int tetris_board[20][10];
int tetris_bag[7];
int tetris_bag_index = 7;
unsigned int tetris_random_state = 1;
int tetris_kicks[5] = {0, -1, 1, -2, 2};
int tetris_line_points[5] = {0, 100, 300, 500, 800};
int tetris_kind, tetris_rotation, tetris_x, tetris_y;
int tetris_next, tetris_held = -1, tetris_can_hold = 1;
int tetris_score, tetris_lines, tetris_level = 1;
int tetris_over, tetris_paused, tetris_dirty = 1, tetris_running = 1;
int tetris_move_direction;
unsigned long long tetris_fall_at, tetris_move_at, tetris_soft_at;

unsigned int tetris_random(void) {
    tetris_random_state = tetris_random_state * 1664525U + 1013904223U;
    return tetris_random_state;
}

int tetris_take_piece(void) {
    if (tetris_bag_index == 7) {
        for (int i = 0; i < 7; ++i)
            tetris_bag[i] = i;
        for (int i = 6; i > 0; --i) {
            int other = tetris_random() % (unsigned int)(i + 1);
            int temporary = tetris_bag[i];
            tetris_bag[i] = tetris_bag[other];
            tetris_bag[other] = temporary;
        }
        tetris_bag_index = 0;
    }
    return tetris_bag[tetris_bag_index++];
}

int tetris_cell(int kind, int rotation, int x, int y) { return (tetris_shapes[kind][rotation] >> (y * 4 + x)) & 1U; }

int tetris_fits(int kind, int rotation, int x, int y) {
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            if (tetris_cell(kind, rotation, column, row)) {
                int bx = x + column;
                int by = y + row;
                if (bx < 0 || bx >= 10 || by >= 20)
                    return 0;
                if (by >= 0 && tetris_board[by][bx])
                    return 0;
            }
    return 1;
}

int tetris_interval(void) {
    int interval = 650 - (tetris_level - 1) * 45;
    return interval < 90 ? 90 : interval;
}

void tetris_spawn(void) {
    tetris_kind = tetris_next;
    tetris_next = tetris_take_piece();
    tetris_rotation = 0;
    tetris_x = 3;
    tetris_y = -1;
    tetris_can_hold = 1;
    tetris_over = !tetris_fits(tetris_kind, 0, tetris_x, tetris_y);
    tetris_fall_at = gfx_ticks() + (unsigned int)tetris_interval();
    tetris_dirty = 1;
}

void tetris_reset(unsigned int seed) {
    for (int row = 0; row < 20; ++row)
        for (int column = 0; column < 10; ++column)
            tetris_board[row][column] = 0;
    tetris_random_state = seed;
    tetris_bag_index = 7;
    tetris_next = tetris_take_piece();
    tetris_held = -1;
    tetris_score = tetris_lines = tetris_over = tetris_paused = 0;
    tetris_level = tetris_running = 1;
    tetris_move_direction = 0;
    tetris_move_at = tetris_soft_at = 0;
    tetris_spawn();
}

int tetris_clear_lines(void) {
    int cleared = 0;
    int row = 19;
    while (row >= 0) {
        int full = 1;
        for (int column = 0; column < 10; ++column)
            if (!tetris_board[row][column])
                full = 0;
        if (!full) {
            --row;
            continue;
        }
        for (int target = row; target > 0; --target)
            for (int column = 0; column < 10; ++column)
                tetris_board[target][column] = tetris_board[target - 1][column];
        for (int column = 0; column < 10; ++column)
            tetris_board[0][column] = 0;
        ++cleared;
    }
    if (cleared > 0) {
        tetris_score += tetris_line_points[cleared] * tetris_level;
        tetris_lines += cleared;
        tetris_level = 1 + tetris_lines / 10;
        tetris_dirty = 1;
    }
    return cleared;
}

void tetris_lock(void) {
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            if (tetris_cell(tetris_kind, tetris_rotation, column, row) && tetris_y + row < 0) {
                tetris_over = tetris_dirty = 1;
                return;
            }
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            if (tetris_cell(tetris_kind, tetris_rotation, column, row))
                tetris_board[tetris_y + row][tetris_x + column] = tetris_kind + 1;
    tetris_clear_lines();
    tetris_spawn();
}

int tetris_move(int direction) {
    if (!tetris_fits(tetris_kind, tetris_rotation, tetris_x + direction, tetris_y))
        return 0;
    tetris_x += direction;
    tetris_dirty = 1;
    return 1;
}

int tetris_rotate(int direction) {
    int rotation = (tetris_rotation + direction + 4) % 4;
    for (int rise = 0; rise <= 2; ++rise)
        for (int kick = 0; kick < 5; ++kick) {
            int x = tetris_x + tetris_kicks[kick];
            int y = tetris_y - rise;
            if (tetris_fits(tetris_kind, rotation, x, y)) {
                tetris_x = x;
                tetris_y = y;
                tetris_rotation = rotation;
                tetris_dirty = 1;
                return 1;
            }
        }
    return 0;
}

int tetris_step(void) {
    if (tetris_fits(tetris_kind, tetris_rotation, tetris_x, tetris_y + 1)) {
        ++tetris_y;
        tetris_dirty = 1;
        return 1;
    }
    tetris_lock();
    return 0;
}

void tetris_hard_drop(void) {
    while (tetris_fits(tetris_kind, tetris_rotation, tetris_x, tetris_y + 1)) {
        ++tetris_y;
        tetris_score += 2;
    }
    tetris_lock();
}

void tetris_hold(void) {
    if (!tetris_can_hold)
        return;
    int previous = tetris_held;
    tetris_held = tetris_kind;
    if (previous < 0)
        tetris_spawn();
    else {
        tetris_kind = previous;
        tetris_rotation = 0;
        tetris_x = 3;
        tetris_y = -1;
        tetris_over = !tetris_fits(tetris_kind, 0, tetris_x, tetris_y);
        tetris_fall_at = gfx_ticks() + (unsigned int)tetris_interval();
    }
    tetris_can_hold = 0;
    tetris_dirty = 1;
}

void tetris_key(int key, int repeat) {
    if (repeat)
        return;
    if (key == 27) {
        tetris_running = 0;
        return;
    }
    if (key == 'r') {
        tetris_reset((unsigned int)gfx_ticks());
        return;
    }
    if (key == 'p' && !tetris_over) {
        tetris_paused = !tetris_paused;
        tetris_fall_at = gfx_ticks() + (unsigned int)tetris_interval();
        tetris_move_direction = 0;
        tetris_dirty = 1;
        return;
    }
    if (tetris_over || tetris_paused)
        return;
    if (key == 'a' || key == 1073741904)
        tetris_move(-1);
    if (key == 'd' || key == 1073741903)
        tetris_move(1);
    if (key == 'w' || key == 'x' || key == 1073741906)
        tetris_rotate(1);
    if (key == 'z')
        tetris_rotate(-1);
    if (key == 's' || key == 1073741905) {
        int moved = tetris_step();
        tetris_score += moved;
        tetris_soft_at = gfx_ticks() + 35ULL;
    }
    if (key == ' ')
        tetris_hard_drop();
    if (key == 'c')
        tetris_hold();
}

void tetris_update(unsigned long long now) {
    if (tetris_over || tetris_paused)
        return;
    int direction = (gfx_key_down('d') || gfx_key_down(1073741903)) - (gfx_key_down('a') || gfx_key_down(1073741904));
    if (direction != tetris_move_direction) {
        tetris_move_direction = direction;
        tetris_move_at = now + 160ULL;
    } else if (direction && now >= tetris_move_at) {
        tetris_move(direction);
        tetris_move_at = now + 50ULL;
    }
    int soft = gfx_key_down('s') || gfx_key_down(1073741905);
    if (soft && now >= tetris_soft_at) {
        int moved = tetris_step();
        tetris_score += moved;
        tetris_soft_at = now + 35ULL;
        tetris_fall_at = now + (unsigned int)tetris_interval();
    } else if (now >= tetris_fall_at) {
        tetris_step();
        tetris_fall_at = now + (unsigned int)tetris_interval();
    }
}

void tetris_color(int color, int alpha) { gfx_color(tetris_colors[color - 1][0], tetris_colors[color - 1][1], tetris_colors[color - 1][2], alpha); }

void tetris_block(int x, int y, int size, int color) {
    tetris_color(color, 255);
    gfx_fill_rect(x + 1, y + 1, size - 2, size - 2);
    gfx_color(255, 255, 255, 75);
    gfx_line(x + 3, y + 3, x + size - 4, y + 3);
}

void tetris_number(int x, int y, int number) {
    char text[16] = {0};
    int count = 0;
    do {
        text[count++] = '0' + number % 10;
        number /= 10;
    } while (number > 0);
    for (int i = 0; i < count / 2; ++i) {
        char temporary = text[i];
        text[i] = text[count - 1 - i];
        text[count - 1 - i] = temporary;
    }
    gfx_text(x, y, text);
}

void tetris_preview(int kind, int x, int y) {
    if (kind < 0)
        return;
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            if (tetris_cell(kind, 0, column, row))
                tetris_block(x + column * 22, y + row * 22, 22, kind + 1);
}

int tetris_render(void) {
    gfx_color(12, 16, 26, 255);
    gfx_clear();
    gfx_color(210, 225, 245, 255);
    gfx_text(32, 24, "ls-C / TETRIS");
    gfx_color(55, 210, 235, 255);
    gfx_line(32, 44, 536, 44);
    gfx_color(18, 25, 39, 255);
    gfx_fill_rect(32, 64, 280, 560);
    gfx_color(30, 40, 57, 255);
    for (int column = 0; column <= 10; ++column)
        gfx_line(32 + column * 28, 64, 32 + column * 28, 624);
    for (int row = 0; row <= 20; ++row)
        gfx_line(32, 64 + row * 28, 312, 64 + row * 28);
    for (int row = 0; row < 20; ++row)
        for (int column = 0; column < 10; ++column)
            if (tetris_board[row][column])
                tetris_block(32 + column * 28, 64 + row * 28, 28, tetris_board[row][column]);
    if (!tetris_over) {
        int ghost = tetris_y;
        while (tetris_fits(tetris_kind, tetris_rotation, tetris_x, ghost + 1))
            ++ghost;
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column)
                if (tetris_cell(tetris_kind, tetris_rotation, column, row)) {
                    if (ghost + row >= 0) {
                        tetris_color(tetris_kind + 1, 130);
                        gfx_rect(34 + (tetris_x + column) * 28, 66 + (ghost + row) * 28, 24, 24);
                    }
                    if (tetris_y + row >= 0)
                        tetris_block(32 + (tetris_x + column) * 28, 64 + (tetris_y + row) * 28, 28, tetris_kind + 1);
                }
    }
    gfx_color(155, 175, 200, 255);
    gfx_text(352, 72, "NEXT");
    gfx_text(352, 200, "HOLD / C");
    tetris_preview(tetris_next, 352, 104);
    tetris_preview(tetris_held, 352, 232);
    gfx_color(155, 175, 200, 255);
    gfx_text(352, 344, "SCORE");
    gfx_text(352, 400, "LINES");
    gfx_text(352, 456, "LEVEL");
    gfx_color(235, 240, 250, 255);
    tetris_number(352, 364, tetris_score);
    tetris_number(352, 420, tetris_lines);
    tetris_number(352, 476, tetris_level);
    gfx_color(155, 175, 200, 255);
    gfx_text(352, 520, "ARROWS / A D S");
    gfx_text(352, 540, "UP / W / X: ROTATE");
    gfx_text(352, 560, "Z: REVERSE ROTATE");
    gfx_text(352, 580, "SPACE: DROP");
    gfx_text(352, 600, "P: PAUSE  R: RESTART");
    gfx_text(352, 620, "ESC: QUIT");
    if (tetris_over || tetris_paused) {
        gfx_color(5, 10, 20, 225);
        gfx_fill_rect(48, 288, 248, 92);
        gfx_color(240, 245, 255, 255);
        if (tetris_over) {
            gfx_text(104, 312, "GAME OVER");
            gfx_text(80, 344, "R TO PLAY AGAIN");
        } else {
            gfx_text(120, 312, "PAUSED");
            gfx_text(88, 344, "P TO CONTINUE");
        }
    }
    tetris_dirty = 0;
    return gfx_present();
}

int tetris_frame(void) {
    int event;
    while ((event = gfx_poll()) > 0) {
        if (event == 1)
            tetris_running = 0;
        if (event == 2)
            tetris_key(gfx_key(), gfx_key_repeat());
        if (!tetris_running)
            return 0;
    }
    if (event < 0)
        return 0;
    tetris_update(gfx_ticks());
    if (tetris_dirty && !tetris_render())
        return 0;
    return tetris_running;
}

int main(void) {
    if (!gfx_open("ls-C Tetris", 568, 656))
        return 1;
    tetris_reset((unsigned int)gfx_ticks());
    while (tetris_running) {
        if (!tetris_frame())
            break;
        gfx_delay(8);
    }
    gfx_close();
    return 0;
}
