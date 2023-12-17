#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum {
    WIDTH = 188,
    HEIGHT = 120,
    BINARY_THRESHOLD = 180,
    EDGE_THRESHOLD = 255,
};

static uint8_t buf1[WIDTH][HEIGHT];
static uint8_t buf2[WIDTH][HEIGHT];
static int int_buf[WIDTH * HEIGHT];
static uint8_t queue[WIDTH * HEIGHT][2];

typedef struct {
    int x, y;
} Point;

typedef struct {
    int cx, cy, radius;
} Circle;

typedef struct {
    unsigned char red, green, blue;
} Pixel;

void binarize(uint8_t img[WIDTH][HEIGHT]) {
    for (int i = 0; i < WIDTH; ++i) {
        for (int j = 0; j < HEIGHT; j++) {
            img[i][j] = img[i][j] > BINARY_THRESHOLD ? 255 : 0;
        }
    }
}

void gauss_filter(uint8_t img[WIDTH][HEIGHT]) {
    for (int i = 1; i < WIDTH - 1; ++i) {
        for (int j = 1; j < HEIGHT - 1; j++) {
            int sum = 0;
            for (int k = -1; k <= 1; ++k) {
                for (int l = -1; l <= 1; ++l) {
                    sum += img[i + k][j + l];
                }
            }
            buf1[i][j] = sum / 9;
        }
    }
    for (int i = 1; i < WIDTH - 1; ++i) {
        for (int j = 1; j < HEIGHT - 1; j++) {
            img[i][j] = buf1[i][j];
        }
    }
}

void edge_detection(uint8_t img[WIDTH][HEIGHT]) {
    for (int i = 1; i < WIDTH - 1; ++i) {
        for (int j = 1; j < HEIGHT - 1; j++) {
            int dx = img[i + 1][j] - img[i - 1][j];
            int dy = img[i][j + 1] - img[i][j - 1];
            if (abs(dx) + abs(dy) >= EDGE_THRESHOLD) {
                buf1[i][j] = 255;
            } else {
                buf1[i][j] = 0;
            }
        }
    }
    for (int i = 1; i < WIDTH - 1; ++i) {
        for (int j = 1; j < HEIGHT - 1; j++) {
            img[i][j] = buf1[i][j];
        }
    }
}

// 找到最大面积的圆形
// 在最大面积轮廓内部进行 bfs 找到半径

void render(uint8_t img[WIDTH][HEIGHT]) {
    printf("P3\n%d %d\n255\n", WIDTH, HEIGHT);
    for (int j = 0; j < HEIGHT; ++j) {
        for (int i = 0; i < WIDTH; ++i) {
            // int out = 0 <= img[i][j] && img[i][j] <= 255 ? 0 : 1;
            int out = img[i][j] == 128;
            if (out) {
                // printf("%d %d %d ", 255, 0, 0);
                printf("%d %d %d ", img[i][j], img[i][j], img[i][j]);
                fprintf(stderr, "hello %d %d\n", i, j);
            } else {
                printf("%d %d %d ", img[i][j], img[i][j], img[i][j]);
            }
        }
        printf("\n");
    }
}

void rand_img(uint8_t img[WIDTH][HEIGHT]) {
    Circle cs[20];
    for (int i = 0; i < 10; i++) {
        cs[i].cx = rand() % WIDTH;
        cs[i].cy = rand() % HEIGHT;
        cs[i].radius = rand() % 10 + 5;
    };
    for (int j = 0; j < HEIGHT; j++) {
        for (int i = 0; i < WIDTH; ++i) {
            img[i][j] = 0;
            for (int k = 0; k < 10; k++) {
                int dx = i - cs[k].cx;
                int dy = j - cs[k].cy;
                if (dx * dx + dy * dy < cs[k].radius * cs[k].radius) {
                    img[i][j] = 255;
                }
            }
        }
    }
}

// 对于二值化图像
void bfs_depth(uint8_t img[WIDTH][HEIGHT], int color[WIDTH][HEIGHT]) {
    static int color_cnt = 0;
    color_cnt = 0;
    int dx[] = { 0, 0, 1, -1 };
    int dy[] = { 1, -1, 0, 0 };
    int head = 0, tail = 0;
    queue[tail][0] = 0;
    queue[tail][1] = 0;
    tail++;
    while (head < tail) {
        int x = queue[head][0];
        int y = queue[head][1];
        head++;
        for (int i = 0; i < 4; ++i) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT && img[nx][ny] == 255
                && color[nx][ny] == 0)
            {
                color[nx][ny] = color_cnt;
                queue[tail][0] = nx;
                queue[tail][1] = ny;
                tail++;
            }
        }
    }
    color_cnt++;
}

Circle color_img(uint8_t img[WIDTH][HEIGHT]) {
    static int color_cnt = 0;
    memset(buf1, 0, sizeof(buf1));
    memset(int_buf, 0, sizeof(int_buf));

    const int dx[] = { 0, 0, 1, -1 };
    const int dy[] = { 1, -1, 0, 0 };

    uint8_t(*color)[HEIGHT] = buf1;
    int* color_area = int_buf;
    color_cnt = 0;

    int head = 0, tail = -1;
    for (int j = 0; j < HEIGHT; j++) {
        for (int i = 0; i < WIDTH; i++) {
            if (color[i][j] == 0 && img[i][j] == 255) {
                color_cnt++;
                queue[++tail][0] = i;
                queue[tail][1] = j;
                color[i][j] = color_cnt;
                while (head <= tail) {
                    int x = queue[head][0];
                    int y = queue[head][1];
                    head++;
                    color_area[color_cnt]++;
                    for (int k = 0; k < 4; ++k) {
                        int nx = x + dx[k];
                        int ny = y + dy[k];
                        if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT && img[nx][ny] == 255
                            && color[nx][ny] == 0)
                        {
                            color[nx][ny] = color_cnt;
                            queue[++tail][0] = nx;
                            queue[tail][1] = ny;
                        }
                    }
                }
            }
        }
    }

    // 求出所有面积后
    int max_area = -1;
    int max_area_color = -1;
    for (int i = 0; i < color_cnt; ++i) {
        if (color_area[i] > max_area) {
            max_area = color_area[i];
            max_area_color = i;
        }
    }
    int begin = -1, end = -1;
    for (int i = 0; i <= tail; i++) {
        if (color[queue[i][0]][queue[i][1]] == max_area_color) {
            if (begin == -1) {
                begin = i;
            }
            end = i;
        }
    }
    if (max_area_color == -1) {
        return (Circle) { 0, 0, 0 };
    }

    head = 0, tail = -1;
    // 对边缘位置开启下一个 bfs
    memset(buf2, 127, sizeof(buf2));
    uint8_t(*dis)[HEIGHT] = buf2;
    for (int i = begin; i <= end; i++) {
        int x = queue[i][0];
        int y = queue[i][1];
        int on_edge = 0;
        for (int k = 0; k < 4; k++) {
            int tx = x + dx[k];
            int ty = y + dy[k];
            if (tx < 0 || tx >= WIDTH || ty < 0 || ty >= HEIGHT || img[tx][ty] == 0) {
                on_edge = 1;
                break;
            }
        }
        if (on_edge) {
            queue[++tail][0] = x;
            queue[tail][1] = y;
            color[x][y] = -1;
            dis[x][y] = 1;
        }
    }
    Point max_dis_point = { -1, -1 };
    int max_dis = -1;

    while (head <= tail) {
        int x = queue[head][0];
        int y = queue[head][1];
        head++;
        if (dis[x][y] > max_dis) {
            max_dis = dis[x][y];
            max_dis_point = (Point) { x, y };
        }
        for (int k = 0; k < 4; k++) {
            int tx = x + dx[k];
            int ty = y + dy[k];
            // calculate dis
            if (tx >= 0 && tx < WIDTH && ty >= 0 && ty < HEIGHT && color[tx][ty] == max_area_color
                && dis[x][y] + 1 < dis[tx][ty])
            {
                dis[tx][ty] = dis[x][y] + 1;
                queue[++tail][0] = tx;
                queue[tail][1] = ty;
            }
        }
    }
    return (Circle) {
        max_dis_point.x,
        max_dis_point.y,
        max_dis,
    };
}

void draw_circle(uint8_t img[WIDTH][HEIGHT], Circle c) {
    for (int i = 0; i < WIDTH; ++i) {
        for (int j = 0; j < HEIGHT; j++) {
            int dx = i - c.cx;
            int dy = j - c.cy;
            int d = dx * dx + dy * dy;
            int r_sq = c.radius * c.radius;
            if (r_sq - 20 <= d && d <= r_sq + 20) {
                img[i][j] = 128;
                // fprintf(stderr, "no %d %d\n", i, j);
            }
        }
    }
}

// 从文件读取PPM图片
int ppm_load(char* filename, uint8_t img[WIDTH][HEIGHT]) {
    FILE* file;
    // 尝试打开PPM文件
    file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Can't open file %s\n", filename);
        return 1;
    }

    // 读取并验证文件格式
    char magic[3];
    fscanf(file, "%s", magic);
    if (magic[0] != 'P' || magic[1] != '3') {
        fprintf(stderr, "Invalid ppm file\n");
        fclose(file);
        return 1;
    }

    // 跳过注释
    char comment_char = getc(file);
    while (comment_char == '#') {
        while (getc(file) != '\n')
            ; // 读取并忽略注释行
        comment_char = getc(file); // 读取下一行的第一个字符
    }
    ungetc(comment_char, file); // 将非注释字符放回输入流

    // 读取图像宽度、高度和最大颜色值
    int width, height, max_color;
    fscanf(file, "%d %d %d", &width, &height, &max_color);

    // 分配内存来存储图像数据
    // Pixel* buffer = (Pixel*)malloc(sizeof(Pixel) * width * height);
    // if (buffer == NULL) {
    //     fprintf(stderr, "Can't allocate buffer.\n");
    //     fclose(file);
    //     return 1;
    // }

    // // 读取像素数据
    // fread(buffer, sizeof(Pixel), width * height, file);
    // for (int i = 0; i < width * height; i++) {
    //     img[i / height][i % height] = (buffer[i].red + buffer[i].green + buffer[i].blue) / 3;
    // }
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; ++i) {
            int r, g, b;
            fscanf(file, "%d %d %d", &r, &g, &b);
            img[i][j] = (r + g + b) / 3;
        }
    }

    // 关闭文件
    fclose(file);

    // 从这里开始，你可以使用读取的图像数据进行任何操作

    // 释放分配的内存
    // free(buffer);
    return 0;
}

int main() {
    // uint8_t* image = malloc(WIDTH * HEIGHT * sizeof(uint8_t));
    srand(time(0));
    static uint8_t ori[WIDTH][HEIGHT];
    static uint8_t img[WIDTH][HEIGHT];
    fprintf(stderr, "Rendering image...\n");
    // rand_img(ori);
    ppm_load("ori-p3.ppm", ori);
    memcpy(img, ori, sizeof(img));
    // https://blog.csdn.net/yy197696/article/details/110103000
    // gauss_filter(img);
    binarize(img);
    // error when  c.radius == 0
    Circle c = color_img(img);
    draw_circle(ori, c);
    render(ori);
    return 0;
}
