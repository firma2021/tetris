#include <err.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// 本游戏中，
// 以终端窗口的左上角为坐标原点，
// 水平向右为x轴，水平向下为y轴。

#define MARGIN_LEFT (3 + 1)
#define MARGIN_TOP (2 + 1)

#define MAP_LEFT (MARGIN_LEFT + 2)
#define MAP_TOP (MARGIN_TOP + 1)

#define MAP_HEIGHT 15
#define MAP_WIDTH 10

int map[MAP_HEIGHT][MAP_WIDTH];

#define BLOCK_H_SIZE 1
#define BLOCK_V_SIZE 1

void pause_game()
{
}
void resume_game()
{
}
void save_game()
{
}

void load_game()
{
}

void exit_game()
{
    puts("game over.");
    exit(EXIT_SUCCESS);
}

void position_print(int x, int y, char *format, ...)
{
    printf("\033[%d;%dH", y + 1, x + 1);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

struct block
{
    int x;
    int y;
};

struct position
{
    int x;
    int y;
};

// 每个盒子由4个小块构成
#define BLOCK_COUNT 4

struct box
{
    struct block blocks[BLOCK_COUNT]; // 由四个小块构成的俄罗斯方块
    struct position spawn_pos;        // 生成方块的位置
};

// 俄罗斯方块有7类，每类由该类中的第一个方块围绕（1, 1）旋转而来

#define BOX_TYPE 7
const int box_style_count[BOX_TYPE] = {1, 2, 2, 2, 4, 4, 4};

const struct box boxes[19] =
    {
        // 第一类方块
        {0, 0, 0, 1, 1, 0, 1, 1, 0, 4},
        // 第二类方块
        {0, 1, 1, 1, 2, 1, 3, 1, 0, 3},
        {1, 0, 1, 1, 1, 2, 1, 3, -1, 3},
        // 第三类方块
        {0, 0, 1, 0, 1, 1, 2, 1, 0, 4},
        {0, 1, 0, 2, 1, 0, 1, 1, 0, 3},
        // 第四类方块
        {0, 1, 1, 0, 1, 1, 2, 0, 0, 4},
        {0, 0, 0, 1, 1, 1, 1, 2, 0, 4},
        // 第五类方块
        {0, 2, 1, 0, 1, 1, 1, 2, 0, 3},
        {0, 1, 1, 1, 2, 1, 2, 2, 0, 3},
        {1, 0, 1, 1, 1, 2, 2, 0, -1, 3},
        {0, 0, 0, 1, 1, 1, 2, 1, 0, 4},
        // 第六类方块
        {0, 0, 1, 0, 1, 1, 1, 2, 0, 3},
        {0, 1, 0, 2, 1, 1, 2, 1, 0, 3},
        {1, 0, 1, 1, 1, 2, 2, 2, -1, 3},
        {0, 1, 1, 1, 2, 0, 2, 1, 0, 4},
        // 第七类方块
        {0, 1, 1, 0, 1, 1, 1, 2, 0, 3},
        {0, 1, 1, 1, 1, 2, 2, 1, 0, 3},
        {1, 0, 1, 1, 1, 2, 2, 1, -1, 3},
        {0, 1, 1, 0, 1, 1, 2, 1, 0, 4}};

int get_box_index(int type, int style_index)
{
    switch (type)
    {
    case 1:
        return 0;
        break;
    case 2:
        return 1 + style_index;
        break;
    case 3:
        return 3 + style_index;
        break;
    case 4:
        return 5 + style_index;
        break;
    case 5:
        return 7 + style_index;
        break;
    case 6:
        return 11 + style_index;
        break;
    case 7:
        return 15 + style_index;
        break;
    default:
        fputs("invalid arguments: get_box_index", stderr);
        return -1;
        break;
    }
}

enum CharColor
{
    BLACK_CHAR = 30,
    RED_CHAR,
    GREEN_CHAR,
    YELLOW_CHAR,
    BLUE_CHAR,
    FUCHSIA_CHAR, // 紫红色
    Cyan_CHAR,    // 蓝绿色、青色
    WHITE_CHAR
};

enum BackgroundColor
{
    BLACK_BG = 40,
    RED_BG,
    GREEN_BG,
    YELLOW_BG,
    BLUE_BG,
    FUCHSIA_BG,
    Cyan_BG,
    WHITE_BG
};

/// @brief 设置高亮度，前景色，背景色
/// @param
/// @param
void set_color(enum CharColor char_color, enum BackgroundColor bg_color)
{
    printf("\033[1;%d;%dm", char_color, bg_color);
}

void clear_output()
{
    printf("\e[2J");
}
void revert_output()
{
    printf("\033[0m");
}

struct game_info
{
    int score;
    int speed;
};

struct game_info game_info = {0, 1};

void speed_up()
{
    ++game_info.speed;
}

void draw_game_info()
{
    int x = MARGIN_LEFT + MAP_WIDTH * 2 + 7;
    int y = MARGIN_TOP + 8;

    set_color(BLUE_CHAR, BLACK_BG);
    position_print(x, y, "score");
    position_print(x, y, "%d", game_info.score);

    set_color(FUCHSIA_CHAR, BLACK_BG);
    position_print(x, y, "speed");
    position_print(x, y, "%d", game_info.speed);

    revert_output();
}

// 方块为type种类的第style_index种分格
// type, style的下标从0开始
struct game_box
{
    int type; // 方块的种类
    int style_index;
    struct box box;
    enum CharColor ch_color;
    enum BackgroundColor bg_color;
    int x_position;
    int y_position;
    bool exists;
};

struct game_box current_box;
struct game_box prepared_box;

/// @brief 生成[m,n]之间的随机数
/// @param m
/// @param n
int rand_int(int m, int n)
{
    if (m >= n)
    {
        fputs("rand_int: m must less than n", stderr);
        return -1;
    }
    int diff = n - m + 1;
    int random = rand() % diff;
    return m + random;
}
void generate_game_box(struct game_box *game_box)
{
    game_box->type = rand_int(0, BOX_TYPE);
    game_box->style_index = rand_int(0, box_style_count[game_box->type]);

    game_box->ch_color = rand_int(BLACK_CHAR, WHITE_CHAR);
    game_box->bg_color = rand_int(BLACK_BG, WHITE_BG);

    game_box->box = boxes[get_box_index(game_box->type, game_box->style_index)];

    game_box->x_position = game_box->box.spawn_pos.x;
    game_box->y_position = game_box->box.spawn_pos.y;

    game_box->exists = true;
}

/// @brief 绘制或清除方块
/// @param game_box 被绘制（清除）的方块的信息
/// @param erase 为true时清除方块
void draw_game_box(struct game_box *game_box, bool erase, int x_offset, int y_offset)
{
    const char *filler = erase ? "  " : "[]";

    set_color(game_box->ch_color, game_box->bg_color);

    for (int i = 0; i < BLOCK_COUNT; ++i)
    {
        int x = MAP_LEFT + 2 * (game_box->x_position + game_box->box.blocks[i].x) + x_offset;
        int y = MAP_TOP + game_box->y_position + game_box->box.blocks[i].y + y_offset;

        position_print(x, y, "%s", filler);
    }

    revert_output();
}

#define PREPARED_BOX_OFFSET 7

void prepare_game_box()
{
    if (prepared_box.exists)
    {
        draw_game_box(&prepared_box, true, PREPARED_BOX_OFFSET, 0);
    }
    generate_game_box(&prepared_box);
    draw_game_box(&prepared_box, false, PREPARED_BOX_OFFSET, 0);
}

/// @brief 将方块移动到(x,y)位置后，检测方块是否与地图边界或地图底部发生碰撞
/// @param x
/// @param y
/// @return 无碰撞，返回真
bool detect_collision(struct game_box *game_box, int x, int y)
{
    for (int i = 0; i < BLOCK_COUNT; ++i)
    {
        int x_pos = x + game_box->x_position;
        int y_pos = y + game_box->y_position;

        if (x_pos < 0 || x_pos >= MAP_WIDTH || y_pos < 0 || y_pos >= MAP_HEIGHT)
        {
            return false;
        }
        if (map[x][y] == true)
        {
            return false;
        }
    }

    return true;
}

void create_current_box()
{
    if (current_box.exists)
    {
        current_box = prepared_box;
    }
    else
    {
        generate_game_box(&current_box);
    }

    draw_game_box(&current_box, false, 0, 0);

    if (!detect_collision(current_box.x_position, current_box.y_position))
    {
        exit_game();
    }
    else
    {
        prepare_game_box();
    }
}

void move_box()
{
}

void box_down_one_line()
{
}

void move_box_to_bottom()
{
}

/// @brief 绘制游戏上下左右四个边框
void draw_map_border(void)
{
    clear_output();
    set_color(GREEN_CHAR, GREEN_BG);

    const int left_x = MARGIN_LEFT;
    const int right_x = MAP_LEFT + MAP_WIDTH * 2;

    // 绘制垂直边框（左边框和右边框）
    for (int i = 0; i < MAP_HEIGHT; ++i)
    {
        int y = i + MAP_TOP;
        position_print(left_x, y, "%s", "||");  // 绘制左边框
        position_print(right_x, y, "%s", "||"); // 绘制右边框
    }

    // 绘制水平边框（上边框和下边框）
    for (int i = 0; i < MAP_WIDTH + 2; ++i)
    {
        int x = i * 2 + MARGIN_LEFT;

        position_print(x, MARGIN_TOP, "%s", "==");                  // 绘制上边框
        position_print(x, MARGIN_TOP + MAP_HEIGHT + 1, "%s", "=="); // 绘制下边框
    }

    revert_output();
}

static struct termios old_attr;
static struct termios new_attr;

void unbuffer_terminal()
{
    int ret = tcgetattr(STDIN_FILENO, &old_attr);
    if (ret == -1)
    {
        err(EXIT_FAILURE, "tcgetattr failed");
    }

    new_attr = old_attr;
    new_attr.c_lflag &= ~((unsigned)ICANON | (unsigned)ECHO);

    ret = tcsetattr(STDIN_FILENO, TCSANOW, &new_attr);
    if (ret == -1)
    {
        err(EXIT_FAILURE, "tcsetattr failed");
    }
}

void revert_terminal()
{
    if (tcsetattr(STDIN_FILENO, TCSANOW, &old_attr) == -1)
    {
        err(EXIT_FAILURE, "tcsetattr failed");
    }
}

char read_char()
{
    int ch = getchar();
    if (ch == EOF) // 取消终端的缓冲后，ctrl + d 不会发送EOF符
    {
        puts("end of input file, exit.");
        exit(EXIT_SUCCESS);
    }

    return (char)ch;
}

enum DIRECTION
{
    LEFT,
    RIGHT,
    UP,
    DOWN,
    DOWN_IMMEDIATE,
    UNKNOWN
};

void quit_game()
{
}

void handle_pause()
{
}
enum DIRECTION read_direction()
{
    char ch = read_char();

    if (ch == 'q' || ch == 'Q')
    {
        puts("quit game");
        exit(EXIT_SUCCESS);
    }

    if (ch == 'w' || ch == 'W')
    {
        return UP;
    }
    if (ch == 'a' || ch == 'A')
    {
        return LEFT;
    }
    if (ch == 's' || ch == 'S')
    {
        return DOWN;
    }
    if (ch == 'd' || ch == 'D')
    {
        return RIGHT;
    }
    if (ch == ' ')
    {
        return DOWN_IMMEDIATE;
    }

    char esc = 033;
    if (ch == esc && read_char() == '[')
    {
        ch = read_char();
        switch (ch)
        {
        case 'A':
            return UP;
        case 'B':
            return DOWN;
        case 'C':
            return RIGHT;
        case 'D':
            return LEFT;
        default:
            return UNKNOWN;
        }
    }

    return UNKNOWN;
}

enum DIRECTION current_direction;

void move_horizontally(int offset)
{
    if (detect_collision(&current_box, current_box.x_position + offset, current_box.y_position))
    {
        draw_game_box(&current_box, true, 0, 0);
        current_box.x_position += offset;
        draw_game_box(&current_box, false, 0, 0);
    }
}

void move_left(void)
{
    move_horizontally(-1);
}

void move_right(void)
{
    move_horizontally(1);
}

void embed_box_into_map(struct game_box *game_box)
{

}

void move_down(void)
{
    if (detect_collision(&current_box, current_box.x_position, current_box.y_position + 1))
    {
        draw_game_box(&current_box, true, 0, 0);
        ++current_box.y_position;
        draw_game_box(&current_box, false, 0, 0);
    }
    else
    {
        embed_box_into_map(&current_box);
        create_current_box();
    }
}

void rotate_box(void)
{
    if (box_style_count[current_box.type] == 1)
    {
        return;
    }

    current_box.style_index += 1;
    if (current_box.style_index == box_style_count[current_box.type])
    {
        current_box.style_index = 0;
    }

    struct game_box temp = current_box;

    current_box.box = boxes[get_box_index(current_box.type, current_box.style_index)];

    if (detect_collision(&current_box, current_box.x_position, current_box.y_position))
    {
        draw_game_box(&temp, true, 0, 0);
        draw_game_box(&current_box, false, 0, 0);
    }
    else
    {
        current_box.box = temp.box;
    }
}

void immediate_down()
{
}

void display()
{
    for (int i = 0; i < 21 - game_info.speed_level; ++i)
    {
        usleep(10000);
        switch (current_direction)
        {
        case LEFT:
            left_move();
            break;
        case RIGHT:
            right_move();
            break;
        case UP:
            rotate_box();
            break;
        case DOWN:
            speed_up();
            break;
        case DOWN_IMMEDIATE:
            immediate_down();
            break;
        default:
            break;
        }
    }
}

void print_terminal_pathname()
{
    printf("pathname of the stdin file: %s\n", ttyname(STDIN_FILENO));
    printf("pathname of the stdout file:%s\n", ttyname(STDOUT_FILENO));
    printf("pathname of the stderr file: %s\n", ttyname(STDERR_FILENO));
}

extern void test_input();

int main(int argc, char *argv[])
{
    if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0))
    {
        puts("tetris game, version 2023.3.18");
        return 0;
    }

    srand(time(NULL));
    // test_input();

    draw_map_border();
    draw_game_info();
    revert_output();
}

void test_input()
{
    while (true)
    {
        unbuffer_terminal();

        current_direction = read_direction();

        switch (current_direction)
        {
        case LEFT:
            puts("left");
            break;
        case RIGHT:
            puts("right");
            break;
        case UP:
            puts("up");
            break;
        case DOWN:
            puts("down");
            break;
        case DOWN_IMMEDIATE:
            puts("down immediate");
            break;
        default:
            puts("unknown");
            break;
        }

        revert_terminal();
    }
}
