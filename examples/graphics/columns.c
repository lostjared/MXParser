int columns_board[13][6];
int columns_mark[13][6];
int columns_piece[3], columns_next[3];
int columns_colors[6][3] = {{245, 85, 115}, {250, 195, 65}, {75, 210, 150}, {65, 185, 240}, {165, 110, 240}, {235, 240, 250}};
int columns_dx[4] = {1, 0, 1, -1};
int columns_dy[4] = {0, 1, 1, 1};
unsigned int columns_random_state = 1;
int columns_x, columns_y;
int columns_score, columns_gems, columns_level = 1;
int columns_chain, columns_best_chain, columns_phase;
int columns_over, columns_paused, columns_running = 1, columns_dirty = 1;
int columns_move_direction;
unsigned long long columns_fall_at, columns_move_at, columns_soft_at, columns_clear_at, columns_pause_at;

int columns_random_gem(void) {
    columns_random_state = columns_random_state * 1664525U + 1013904223U;
    return (int)((columns_random_state >> 16) % 6U) + 1;
}

int columns_interval(void) {
    int interval = 700 - (columns_level - 1) * 45;
    return interval < 100 ? 100 : interval;
}

int columns_fits(int x, int y) {
    if (x < 0 || x >= 6 || y + 2 >= 13)
        return 0;
    for (int i = 0; i < 3; ++i)
        if (y + i >= 0 && columns_board[y + i][x])
            return 0;
    return 1;
}

void columns_spawn(void) {
    for (int i = 0; i < 3; ++i) {
        columns_piece[i] = columns_next[i];
        columns_next[i] = columns_random_gem();
    }
    columns_x = 2;
    columns_y = -2;
    columns_phase = 0;
    columns_over = !columns_fits(columns_x, columns_y);
    columns_fall_at = gfx_ticks() + (unsigned int)columns_interval();
    columns_dirty = 1;
}

void columns_reset(unsigned int seed) {
    for (int row = 0; row < 13; ++row)
        for (int column = 0; column < 6; ++column) {
            columns_board[row][column] = 0;
            columns_mark[row][column] = 0;
        }
    columns_random_state = seed;
    for (int i = 0; i < 3; ++i)
        columns_next[i] = columns_random_gem();
    columns_score = columns_gems = columns_chain = columns_best_chain = 0;
    columns_over = columns_paused = columns_move_direction = 0;
    columns_level = columns_running = 1;
    columns_move_at = columns_soft_at = columns_clear_at = columns_pause_at = 0;
    columns_spawn();
}

int columns_find_matches(void) {
    for (int row = 0; row < 13; ++row)
        for (int column = 0; column < 6; ++column)
            columns_mark[row][column] = 0;
    for (int row = 0; row < 13; ++row)
        for (int column = 0; column < 6; ++column) {
            int gem = columns_board[row][column];
            if (!gem)
                continue;
            for (int direction = 0; direction < 4; ++direction) {
                int dx = columns_dx[direction];
                int dy = columns_dy[direction];
                int length = 1;
                while (1) {
                    int x = column + dx * length;
                    int y = row + dy * length;
                    if (x < 0 || x >= 6 || y >= 13)
                        break;
                    if (columns_board[y][x] != gem)
                        break;
                    ++length;
                }
                if (length >= 3)
                    for (int i = 0; i < length; ++i)
                        columns_mark[row + dy * i][column + dx * i] = 1;
            }
        }
    int count = 0;
    for (int row = 0; row < 13; ++row)
        for (int column = 0; column < 6; ++column)
            count += columns_mark[row][column];
    return count;
}

void columns_compact(void) {
    for (int column = 0; column < 6; ++column) {
        int target = 12;
        for (int row = 12; row >= 0; --row)
            if (columns_board[row][column]) {
                columns_board[target][column] = columns_board[row][column];
                --target;
            }
        while (target >= 0)
            columns_board[target--][column] = 0;
    }
}

void columns_lock(void) {
    if (columns_y < 0) {
        columns_over = columns_dirty = 1;
        return;
    }
    for (int i = 0; i < 3; ++i)
        columns_board[columns_y + i][columns_x] = columns_piece[i];
    columns_chain = 0;
    if (columns_find_matches()) {
        columns_chain = columns_phase = 1;
        if (columns_best_chain < 1)
            columns_best_chain = 1;
        columns_clear_at = gfx_ticks() + 180ULL;
    } else
        columns_spawn();
    columns_dirty = 1;
}

void columns_resolve(unsigned long long now) {
    if (columns_phase != 1 || now < columns_clear_at)
        return;
    int removed = 0;
    for (int row = 0; row < 13; ++row)
        for (int column = 0; column < 6; ++column)
            if (columns_mark[row][column]) {
                columns_board[row][column] = 0;
                ++removed;
            }
    columns_score += removed * 10 * columns_level * columns_chain;
    columns_gems += removed;
    columns_level = 1 + columns_gems / 30;
    columns_compact();
    if (columns_find_matches()) {
        ++columns_chain;
        if (columns_chain > columns_best_chain)
            columns_best_chain = columns_chain;
        columns_clear_at = now + 180ULL;
    } else
        columns_spawn();
    columns_dirty = 1;
}

int columns_move(int direction) {
    if (!columns_fits(columns_x + direction, columns_y))
        return 0;
    columns_x += direction;
    columns_dirty = 1;
    return 1;
}

void columns_cycle(int direction) {
    int temporary;
    if (direction > 0) {
        temporary = columns_piece[2];
        columns_piece[2] = columns_piece[1];
        columns_piece[1] = columns_piece[0];
        columns_piece[0] = temporary;
    } else {
        temporary = columns_piece[0];
        columns_piece[0] = columns_piece[1];
        columns_piece[1] = columns_piece[2];
        columns_piece[2] = temporary;
    }
    columns_dirty = 1;
}

int columns_step(void) {
    if (columns_fits(columns_x, columns_y + 1)) {
        ++columns_y;
        columns_dirty = 1;
        return 1;
    }
    columns_lock();
    return 0;
}

void columns_hard_drop(void) {
    while (columns_fits(columns_x, columns_y + 1)) {
        ++columns_y;
        columns_score += 2;
    }
    columns_lock();
}

void columns_key(int key, int repeat) {
    if (repeat)
        return;
    if (key == 27) {
        columns_running = 0;
        return;
    }
    if (key == 'r') {
        columns_reset((unsigned int)gfx_ticks());
        return;
    }
    if (key == 'p' && !columns_over) {
        unsigned long long now = gfx_ticks();
        columns_paused = !columns_paused;
        if (columns_paused)
            columns_pause_at = now;
        else {
            columns_fall_at += now - columns_pause_at;
            columns_clear_at += now - columns_pause_at;
        }
        columns_move_direction = 0;
        columns_dirty = 1;
        return;
    }
    if (columns_over || columns_paused || columns_phase)
        return;
    if (key == 'a' || key == 1073741904)
        columns_move(-1);
    if (key == 'd' || key == 1073741903)
        columns_move(1);
    if (key == 'w' || key == 'x' || key == 1073741906)
        columns_cycle(1);
    if (key == 'z')
        columns_cycle(-1);
    if (key == 's' || key == 1073741905) {
        int moved = columns_step();
        columns_score += moved;
        columns_soft_at = gfx_ticks() + 35ULL;
    }
    if (key == ' ')
        columns_hard_drop();
}

void columns_update(unsigned long long now) {
    if (columns_over || columns_paused)
        return;
    if (columns_phase) {
        columns_resolve(now);
        return;
    }
    int direction = (gfx_key_down('d') || gfx_key_down(1073741903)) - (gfx_key_down('a') || gfx_key_down(1073741904));
    if (direction != columns_move_direction) {
        columns_move_direction = direction;
        columns_move_at = now + 160ULL;
    } else if (direction && now >= columns_move_at) {
        columns_move(direction);
        columns_move_at = now + 50ULL;
    }
    int soft = gfx_key_down('s') || gfx_key_down(1073741905);
    if (soft && now >= columns_soft_at) {
        int moved = columns_step();
        columns_score += moved;
        columns_soft_at = now + 35ULL;
        columns_fall_at = now + (unsigned int)columns_interval();
    } else if (now >= columns_fall_at) {
        columns_step();
        columns_fall_at = now + (unsigned int)columns_interval();
    }
}

void columns_color(int gem, int alpha) { gfx_color(columns_colors[gem - 1][0], columns_colors[gem - 1][1], columns_colors[gem - 1][2], alpha); }

void columns_gem(int x, int y, int size, int gem, int marked) {
    columns_color(gem, 255);
    gfx_fill_rect(x + 2, y + 2, size - 4, size - 4);
    gfx_color(255, 255, 255, marked ? 225 : 90);
    gfx_line(x + 5, y + 5, x + size - 6, y + 5);
    gfx_line(x + 5, y + 5, x + 5, y + size - 6);
    gfx_rect(x + 8, y + 8, size - 16, size - 16);
    if (marked) {
        gfx_color(255, 255, 255, 150);
        gfx_fill_rect(x + 2, y + 2, size - 4, size - 4);
    }
}

void columns_number(int x, int y, int number) {
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

int columns_render(void) {
    gfx_color(13, 16, 29, 255);
    gfx_clear();
    gfx_color(225, 230, 250, 255);
    gfx_text(32, 24, "ls-C / COLUMNS");
    gfx_color(165, 110, 240, 255);
    gfx_line(32, 44, 512, 44);
    gfx_color(21, 26, 44, 255);
    gfx_fill_rect(32, 64, 240, 520);
    gfx_color(36, 43, 65, 255);
    for (int x = 0; x <= 6; ++x)
        gfx_line(32 + x * 40, 64, 32 + x * 40, 584);
    for (int y = 0; y <= 13; ++y)
        gfx_line(32, 64 + y * 40, 272, 64 + y * 40);
    for (int y = 0; y < 13; ++y)
        for (int x = 0; x < 6; ++x)
            if (columns_board[y][x])
                columns_gem(32 + x * 40, 64 + y * 40, 40, columns_board[y][x], columns_mark[y][x]);
    if (!columns_over && !columns_phase) {
        int ghost = columns_y;
        while (columns_fits(columns_x, ghost + 1))
            ++ghost;
        for (int i = 0; i < 3; ++i) {
            if (ghost + i >= 0) {
                columns_color(columns_piece[i], 120);
                gfx_rect(36 + columns_x * 40, 68 + (ghost + i) * 40, 32, 32);
            }
            if (columns_y + i >= 0)
                columns_gem(32 + columns_x * 40, 64 + (columns_y + i) * 40, 40, columns_piece[i], 0);
        }
    }
    gfx_color(165, 180, 210, 255);
    gfx_text(320, 72, "NEXT COLUMN");
    for (int i = 0; i < 3; ++i)
        columns_gem(344, 104 + i * 32, 32, columns_next[i], 0);
    gfx_color(165, 180, 210, 255);
    gfx_text(320, 240, "SCORE");
    gfx_text(320, 296, "GEMS / LEVEL");
    gfx_text(320, 352, "CHAIN / BEST");
    gfx_color(240, 245, 255, 255);
    columns_number(320, 260, columns_score);
    columns_number(320, 316, columns_gems);
    columns_number(416, 316, columns_level);
    columns_number(320, 372, columns_chain);
    columns_number(416, 372, columns_best_chain);
    gfx_color(165, 180, 210, 255);
    gfx_text(320, 420, "MATCH 3 OR MORE");
    gfx_text(320, 440, "ACROSS / DOWN / DIAG");
    gfx_text(320, 480, "ARROWS / A D S: MOVE");
    gfx_text(320, 500, "UP / W / X: CYCLE");
    gfx_text(320, 520, "Z: REVERSE CYCLE");
    gfx_text(320, 540, "SPACE: DROP");
    gfx_text(320, 560, "P: PAUSE  R: RESTART");
    gfx_text(320, 580, "ESC: QUIT");
    if (columns_over || columns_paused) {
        gfx_color(5, 9, 20, 230);
        gfx_fill_rect(44, 276, 216, 88);
        gfx_color(245, 245, 255, 255);
        if (columns_over) {
            gfx_text(112, 300, "GAME OVER");
            gfx_text(88, 332, "R TO RESTART");
        } else {
            gfx_text(128, 300, "PAUSED");
            gfx_text(96, 332, "P TO CONTINUE");
        }
    }
    columns_dirty = 0;
    return gfx_present();
}

int columns_frame(void) {
    int event;
    while ((event = gfx_poll()) > 0) {
        if (event == 1)
            columns_running = 0;
        if (event == 2)
            columns_key(gfx_key(), gfx_key_repeat());
        if (!columns_running)
            return 0;
    }
    if (event < 0)
        return 0;
    columns_update(gfx_ticks());
    if (columns_dirty && !columns_render())
        return 0;
    return columns_running;
}

int main(void) {
    if (!gfx_open("ls-C Columns", 544, 616))
        return 1;
    columns_reset((unsigned int)gfx_ticks());
    while (columns_running) {
        if (!columns_frame())
            break;
        gfx_delay(8);
    }
    gfx_close();
    return 0;
}
