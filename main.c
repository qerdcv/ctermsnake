#include <ncurses.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <pthread/qos.h>
#include <pthread.h>
#include <math.h>
#include <string.h>

#define BODY_CH '#'
#define HEAD_CH '@'
#define FOOD_CH '0'

#define COLOR_HEAD 1
#define COLOR_BODY 2
#define COLOR_FOOD 3

#define UP -1
#define DOWN 1
#define LEFT -1
#define RIGHT 1

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

void grow_snake(struct SnakeNode *s, int max_height, int max_width) {
    struct SnakeNode *tmp = s->next;
    s->next = new_node(s->pos.x, s->pos.y, false);
    s->next->next = tmp;
    move_head(s, max_height, max_width);
}

bool is_collided(struct SnakeNode *s) {
    struct SnakeNode *head = s;

    s = s->next;

    while (true) {
        if (s == NULL) return false;

        if (head->pos.x == s->pos.x && head->pos.y == s->pos.y) return true;

        s = s->next;
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
    nodelay(stdscr, false);
    char *msg = "Game Over";
    char scr[9];

    sprintf(scr, "score: %d", SCORE);

    clear();
    mvaddstr(h / 2, (w / 2) - strlen(msg), msg);
    mvaddstr((h / 2) + 1, (w / 2) - strlen(scr), scr);
    refresh();
    getch();
}

void draw_snake(struct SnakeNode *s) {
    char ch;
    while (true) {
        if (s == NULL) {
            attroff(COLOR_PAIR(COLOR_BODY));
            return;
        }

        attron(COLOR_PAIR(COLOR_BODY));
        ch = BODY_CH;
        if (s->is_head) {
            attron(COLOR_PAIR(COLOR_HEAD));
            ch = HEAD_CH;
        }

        mvaddch(s->pos.y, s->pos.x, ch);

        s = s->next;
    }
}

void draw_food(struct Food *f) {
    attron(COLOR_PAIR(COLOR_FOOD));
    mvaddch(f->pos.y, f->pos.x, FOOD_CH);
    attroff(COLOR_PAIR(COLOR_FOOD));
}

void draw_info() {
    char scr[9];

    sprintf(scr, "score: %d", SCORE);
    mvaddstr(2, 2, scr);
}


int main() {
    int x, y;
    struct Food *food = (struct Food *) malloc(sizeof(struct Food));
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); // Init window resolution
    initscr();

    if (has_colors() == FALSE) {
        panic("colors is not enabled in terminal");
    }

    start_color();
    use_default_colors();

    init_pair(COLOR_HEAD, COLOR_YELLOW, A_COLOR);
    init_pair(COLOR_BODY, COLOR_GREEN, A_COLOR);
    init_pair(COLOR_FOOD, COLOR_RED, A_COLOR);

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
        // Handle keys
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

        // ---------------

        // handle logic

        // move snake
        move_snake(snake, food, w.ws_row, w.ws_col);
        if (is_collided(snake)) { // if it's collided itself - game over
            game_over(w.ws_col, w.ws_row);
            goto end;
        }

        if (snake->pos.x == food->pos.x && snake->pos.y == food->pos.y) { // if head is on food position
            SCORE++; // Increase score

            generate_food(snake, food, w.ws_col, w.ws_row); // Regenerate food
            grow_snake(snake, w.ws_row, w.ws_col);
        }

        // ---------------------

        // Draw
        clear();

        draw_food(food);
        draw_snake(snake);
        draw_info();

        refresh();

        // ------------------------

        // Sleep
        usleep((int) (5 * (pow(10, 5))));
        // -------------------------
    }
    end:
    endwin();
    clear_snake(snake);
    return EXIT_SUCCESS;
}
