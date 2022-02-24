#include <ncurses.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <pthread/qos.h>
#include <pthread.h>
#include <math.h>
#include <string.h>

#define FIELD_WIDTH 500
#define FIELD_HEIGHT 500

#define UP -1
#define DOWN 1
#define LEFT -1
#define RIGHT 1

char FIELD[FIELD_HEIGHT][FIELD_WIDTH];
char CURRENT_KEY = -1;

int VERTICAL_SPEED = DOWN;
int HORIZONTAL_SPEED = 0;
int SCORE = 2;

struct Position {
    int x;
    int y;
};

struct Food {
    struct Position pos;
};


struct SnakeNode {
    struct Position pos;
    bool is_head;
    struct SnakeNode *next;
};


void generate_food(struct SnakeNode *s, struct Food *f, int max_x, int max_y) {
    int x, y;
    struct SnakeNode *tmp;
    while (true) {
        start:
        x = (rand() %
             (max_x + 1));
        y = (rand() %
             (max_y + 1));
        tmp = s;

        while (true) {
            if (tmp == NULL) {
                f->pos.x = x;
                f->pos.y = y;
                return;
            }

            if (tmp->pos.x == x && tmp->pos.y == y) {
                goto start;
            }
            tmp = tmp->next;
        }
    }
}

void panic(char *e) {
    fprintf(stderr, "ERROR: %s;", e);
    exit(1);
}

struct SnakeNode *new_node(int x, int y, bool is_head) {
    struct SnakeNode *s = (struct SnakeNode *) malloc(sizeof(struct SnakeNode));
    if (s == NULL) {
        panic("Malloc snake");
    }

    s->pos = (struct Position) {
            x,
            y,
    };
    s->is_head = is_head;

    return s;
}

void clear_snake(struct SnakeNode *s) {
    while (true) {
        if (s == NULL) return;

        struct SnakeNode *tmp = s;

        free(s);
        s = tmp->next;
    }
}

void move_head(struct SnakeNode *s, int max_height, int max_width) {
    s->pos.x += HORIZONTAL_SPEED;
    s->pos.y += VERTICAL_SPEED;

    if (s->pos.y > max_height - 1) {
        s->pos.y = 0;
    } else if (s->pos.y < 0) {
        s->pos.y = max_height - 1;
    }

    if (s->pos.x > max_width - 1) {
        s->pos.x = 0;
    } else if (s->pos.x < 0) {
        s->pos.x = max_width - 1;
    }
}

void move_snake(struct SnakeNode *snake, struct Food *food, int max_height, int max_width) {
    int x, y, prev_x, prev_y;
    printf("%d", food->pos.x);

    x = snake->pos.x;
    y = snake->pos.y;
    move_head(snake, max_height, max_width);

    snake = snake->next;

    while (true) {
        if (snake == NULL) {
            return;
        }

        prev_x = snake->pos.x;
        prev_y = snake->pos.y;

        snake->pos.x = x;
        snake->pos.y = y;

        x = prev_x;
        y = prev_y;

        snake = snake->next;
    }
}

bool is_colised(struct SnakeNode *s) {
    struct SnakeNode *head = s;

    s = s->next;

    while (true) {
        if (s == NULL) return false;

        if (head->pos.x == s->pos.x && head->pos.y == s->pos.y) return true;

        s = s->next;
    }
}

void cleanup_field() {
    for (int i = 0; i < FIELD_HEIGHT; i++) {
        for (int j = 0; j < FIELD_WIDTH; j++) {
            FIELD[i][j] = ' ';
        }
    }
}

void put_snake(struct SnakeNode *snake) {
    while (true) {
        if (snake == NULL) return;

        if (snake->is_head) {
            FIELD[snake->pos.y][snake->pos.x] = '@';
        } else {
            FIELD[snake->pos.y][snake->pos.x] = '#';
        }

        snake = snake->next;
    }
}

void put_food(struct Food *food) {
    FIELD[food->pos.y][food->pos.x] = '0';
}

void draw_field(int w, int h) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            addch(FIELD[i][j]);
        }
    }
}

void *listen_keys() {
    char new_key;
    while (true) {
        new_key = getch();
        switch (new_key) {
            case 'q':
                CURRENT_KEY = new_key;
                return NULL;
            case -1:
                break;
            default:
                CURRENT_KEY = new_key;
        }
        usleep(50000);
    }
}

void game_over(int w, int h) {
    cleanup_field();
    nodelay(stdscr, false);
    char *msg = "Game Over";

    uint8_t score_cap = 9;
    char scr[score_cap];

    size_t msg_len = strlen(msg);
    for (size_t i = msg_len; i > 0; i--) {
        FIELD[h / 2][(w / 2) - i] = msg[msg_len - i];
    }

    if (SCORE < 10) {
        sprintf(scr, "score: 0%d", SCORE);
    } else {
        sprintf(scr, "score: %d", SCORE);
    }

    for (size_t i = score_cap; i > 0; i--) {
        FIELD[(h / 2) + 1][(w / 2) - i] = scr[score_cap - i];
    }


    clear();
    draw_field(w, h);
    refresh();
    getch();
}


int main(void) {
    struct Food *food = (struct Food *) malloc(sizeof(struct Food));
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); // Init window resolution

    int x, y;

    x = (int) (w.ws_col / 2);
    y = (int) (w.ws_row / 2);
    struct SnakeNode *snake = new_node(x, y, true);
    snake->next = new_node(x, y - 1, false);
    snake->next->next = new_node(x, y - 2, false);

    srand(time(0));

    // Listen keys in separated thread
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, listen_keys, NULL);
    pthread_detach(thread_id);
    // --------------------------------

    // Init screen
    initscr();
    noecho();
    curs_set(0);
    nodelay(stdscr, true);
    // -----------------

    // Init game
    generate_food(snake, food, w.ws_col, w.ws_row);
    // ---------

    while (true) {
        switch (CURRENT_KEY) {
            case 'q':
                goto end;
            case 'w':
                if (VERTICAL_SPEED != DOWN) {
                    VERTICAL_SPEED = UP;
                }
                HORIZONTAL_SPEED = 0;
                break;
            case 's':
                if (VERTICAL_SPEED != UP) {
                    VERTICAL_SPEED = DOWN;
                }
                HORIZONTAL_SPEED = 0;
                break;
            case 'a':
                if (HORIZONTAL_SPEED != RIGHT) {
                    HORIZONTAL_SPEED = LEFT;
                }
                VERTICAL_SPEED = 0;
                break;
            case 'd':
                if (HORIZONTAL_SPEED != LEFT) {
                    HORIZONTAL_SPEED = RIGHT;
                }
                VERTICAL_SPEED = 0;
                break;
            default:
                break;
        }

        move_snake(snake, food, w.ws_row, w.ws_col);
        if (is_colised(snake)) {
            game_over(w.ws_col, w.ws_row);
            goto end;
        }

        if (snake->pos.x == food->pos.x && snake->pos.y == food->pos.y) {
            SCORE++;

            generate_food(snake, food, w.ws_col, w.ws_row);

            struct SnakeNode *tmp = snake->next;
            snake->next = new_node(snake->pos.x, snake->pos.y, false);
            snake->next->next = tmp;
            move_head(snake, w.ws_row, w.ws_col);
        }

        clear();

        cleanup_field();
        put_food(food);
        put_snake(snake);
        draw_field(w.ws_col, w.ws_row);

        refresh();

        usleep((int) (5 * (pow(10, 5))));
    }
    end:
    endwin();
    clear_snake(snake);
    return EXIT_SUCCESS;
}
