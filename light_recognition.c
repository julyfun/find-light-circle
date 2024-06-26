/**
******************************************************************************
* @file : light_recognition.c
* @author : Albert Wang, July Fun
* @brief : None
* @date : 2023/12/18
******************************************************************************
* Copyright (c) 2023 Team JiaoLong-SJTU
* All rights reserved.
******************************************************************************
*/

#include "light_recognition.h"

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#include "missile.h"

const uint8_t ZIG = 0xAA;
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

const int BINARY_THRESHOLD = 180;
const float IOU_BY_BEST_MIN_REQUIRED = 0.88886666;
const float IOU_MIN_REQUIRED = 0.80;
const float RADIUS_MIN_REQUIRED = 5;

#if !__APPLE__
Circle circle;
uint8_t cv_fps;
#endif

static uint8_t buf1[HEIGHT][WIDTH]; // size 188 * 120 * 1 = 22560B = 22.5KB
static uint8_t buf2[HEIGHT][WIDTH]; // 22.5KB
static uint16_t line_buf[LINE_BUF_SIZE]; // 256 * 2 = 1KB
static uint8_t queue[WIDTH * HEIGHT][2]; // 188 * 120 * 2 * 1 = 45120B = 44KB
#if __APPLE__
void draw_circle(uint8_t img[HEIGHT][WIDTH], Circle c) {
    for (int i = 0; i < HEIGHT; ++i) {
        for (int j = 0; j < WIDTH; j++) {
            int dx = i - c.cx;
            int dy = j - c.cy;
            int d = dx * dx + dy * dy;
            int r_sq = c.radius * c.radius;
            if (r_sq - 2 * c.radius <= d && d <= r_sq + 2 * c.radius) {
                img[i][j] = ZIG;
                // fprintf(stderr, "no %d %d\n", i, j);
            }
        }
    }
}

void render(uint8_t img[HEIGHT][WIDTH]) {
    printf("P3\n%d %d\n255\n", WIDTH, HEIGHT);
    for (int j = 0; j < HEIGHT; ++j) {
        for (int i = 0; i < WIDTH; ++i) {
            // int out = 0 <= img[i][j] && img[i][j] <= 255 ? 0 : 1;
            int out = img[j][i] == ZIG;
            int pix = img[j][i];
            if (out) {
                // printf("%d %d %d ", 255, 0, 0);
                printf("%d %d %d ", 255, pix, pix);
                // fprintf(stderr, "hello %d %d\n", i, j);
            } else {
                printf("%d %d %d ", pix, pix, pix);
            }
        }
        printf("\n");
    }
}

void rand_img(uint8_t img[HEIGHT][WIDTH], int n, int r) {
    for (int j = 0; j < HEIGHT; j++) {
        for (int i = 0; i < WIDTH; ++i) {
            img[j][i] = 0;
        }
    }
    for (int i = 0; i < n; i++) {
        int x = rand() % HEIGHT;
        int y = rand() % WIDTH;
        int radius = rand() % r + 1;
        for (int j = MAX(0, x - radius); j < MIN(HEIGHT, x + radius); j++) {
            for (int k = MAX(0, y - radius); k < MIN(WIDTH, y + radius); k++) {
                if ((j - x) * (j - x) + (k - y) * (k - y) <= radius * radius) {
                    img[j][k] = 255;
                }
            }
        }
    }
}
#endif

void binarize(uint8_t img[HEIGHT][WIDTH]) {
    for (uint8_t i = 0; i < HEIGHT; i++) {
        for (uint8_t j = 0; j < WIDTH; j++) {
            img[i][j] = img[i][j] > BINARY_THRESHOLD ? 255 : 0;
        }
    }
}

struct OptionCircleInner {
    uint8_t some;
    Circle circle;
};
typedef struct OptionCircleInner OptionCircle;

OptionCircle color_img(uint8_t img[HEIGHT][WIDTH]) {
    const int dx[] = { 0, 0, 1, -1 };
    const int dy[] = { 1, -1, 0, 0 };
    fprintf(stderr, "---\n");

    // <buf1 borrowed>
    // <line_buf borrowed>
    memset(buf1, 0, sizeof(buf1));
    memset(line_buf, 0, sizeof(line_buf));
    uint8_t(*color)[WIDTH] = buf1;
    uint16_t* color_area = line_buf;
    int color_cnt = 0;
    // [找连通块，染色]
    int head = 0, tail = -1;
    for (int i = 0; i < HEIGHT && color_cnt + 1 < LINE_BUF_SIZE; i++) { // 不能双重 break 无语
        for (int j = 0; j < WIDTH && color_cnt + 1 < LINE_BUF_SIZE; j++) {
            if (color[i][j] == 0 && img[i][j] == 255) {
                color_cnt++;
                queue[++tail][0] = i;
                queue[tail][1] = j;
                color[i][j] = color_cnt;
                // bfs1: 遍历所在连通块
                while (head <= tail) {
                    int x = queue[head][0];
                    int y = queue[head][1];
                    head++;
                    color_area[color_cnt]++;
                    // fprintf(stderr, "color_area[%d]: %d\n", color_cnt, color_area[color_cnt]);
                    for (int k = 0; k < 4; ++k) {
                        int nx = x + dx[k];
                        int ny = y + dy[k];
                        if (nx >= 0 && nx < HEIGHT && ny >= 0 && ny < WIDTH && img[nx][ny] == 255
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

    // [bfs 求外部距离]
    // <buf2 borrowed>
    head = 0, tail = -1;
    memset(buf2, 0, sizeof(buf2));
    uint8_t(*outer_dis)[WIDTH] = buf2;
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            // 边缘和无 color 处（注意不是 img = 0）距离为 0
            if (color[i][j] == 0) {
                outer_dis[i][j] = 0;
                queue[++tail][0] = i;
                queue[tail][1] = j;
            }
            else {
                if (i == 0 || i == HEIGHT - 1 || j == 0 || j == WIDTH - 1) {
                    outer_dis[i][j] = 1; // 边缘处白色距离为 1
                } else {
                    outer_dis[i][j] = UINT8_MAX; // <warn>
                }
            }
        }
    }  
    // render(dis);
    while (head <= tail) {
        int x = queue[head][0];
        int y = queue[head][1];
        head++;
        for (int k = 0; k < 4; k++) {
            int tx = x + dx[k];
            int ty = y + dy[k];
            // calculate dis
            // 可能会减到 255
            if (tx >= 0 && tx < HEIGHT && ty >= 0 && ty < WIDTH
                && outer_dis[x][y] + 1 < outer_dis[tx][ty])
            {
                // 为啥没判断重复入队，因为第一次更新的时候一定是最小距离
                outer_dis[tx][ty] = outer_dis[x][y] + 1;
                queue[++tail][0] = tx;
                queue[tail][1] = ty;
            }
        }
    }
    
    // [find mass center]
    static float mass_center[LINE_BUF_SIZE][2];
    memset(mass_center, 0, sizeof(mass_center));
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            mass_center[color[i][j]][0] += i;
            mass_center[color[i][j]][1] += j;
        }
    }
    for (int i = 1; i <= color_cnt; i++) {
        mass_center[i][0] /= color_area[i];
        mass_center[i][1] /= color_area[i];
    }
    // [求外接半径]
    static float ave_r2[LINE_BUF_SIZE]; // takes 1KB static memory
    static uint16_t ave_r2_cnt[LINE_BUF_SIZE];
    memset(ave_r2, 0, sizeof(ave_r2));
    memset(ave_r2_cnt, 0, sizeof(ave_r2_cnt));
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            if (outer_dis[i][j] == 1) {
                ave_r2[color[i][j]] +=
                    (i - mass_center[color[i][j]][0]) * (i - mass_center[color[i][j]][0])
                    + (j - mass_center[color[i][j]][1]) * (j - mass_center[color[i][j]][1]);
                ave_r2_cnt[color[i][j]]++;
            }
        }
    }
    // [检测是否有 0 边缘]
    for (int i = 1; i <= color_cnt; i++) {
        if (ave_r2_cnt[i] == 0) {
            render(img);
            fprintf(stderr, "ave_r2_cnt[%d] == 0\n", i);
            fprintf(stderr, "color_area[%d]: %d\n", i, color_area[i]);
            assert(0);
        }
        ave_r2[i] /= ave_r2_cnt[i];
    }

    // [求圆]
    static Circle circles[LINE_BUF_SIZE]; // all 0 不共享
    memset(circles, 0, sizeof(circles));
    for (int i = 1; i <= color_cnt; i++) { // 0 也求
        circles[i] = (Circle) {
            .cx = mass_center[i][0],
            .cy = mass_center[i][1],
            .radius = sqrt(ave_r2[i]),
        };
    }
    // for (int i = 1; i <= color_cnt; i++) {
    //     fprintf(stderr, "circle[%d]: %d %d %d\n", i, circles[i].cx, circles[i].cy, circles[i].radius);
    // }
    
    // [求圆和整个连通块的 iou]
    static float area_iou[LINE_BUF_SIZE];
    memset(area_iou, 0, sizeof(area_iou));
    static float area_union[LINE_BUF_SIZE];
    memset(area_union, 0, sizeof(area_union));
    for (int i = 1; i <= color_cnt; i++) {
        area_union[i] = M_PI * ave_r2[i]; // 这里 ave_r2 精度高点
    }
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            if (color[i][j] == 0) {
                continue;
            }
            // 与所在颜色块的圆相交
            float d2 = (i - mass_center[color[i][j]][0]) * (i - mass_center[color[i][j]][0])
                + (j - mass_center[color[i][j]][1]) * (j - mass_center[color[i][j]][1]);
            if (d2 <= ave_r2[color[i][j]]) {
                area_iou[color[i][j]] += 1.0f; // 这里是 area intersection
            } else {
                area_union[color[i][j]] += 1.0f; // 这里是 area difference
            }
        }
    }
    
    static uint8_t valid[LINE_BUF_SIZE];
    memset(valid, 1, sizeof(valid));
    
    // [过滤掉太小的圆，因为他们容易成为最优解]
    for (int i = 1; i <= color_cnt; i++) {
        if (circles[i].radius < RADIUS_MIN_REQUIRED) {
            valid[i] = 0; // 因为太小了
        }
    }

    // [iou 计算，求最大的]
    float best_iou = 0;
    for (int i = 1; i <= color_cnt; i++) {
        if (!valid[i]) {
            continue;
        }
        area_iou[i] /= area_union[i];
        if (area_iou[i] > best_iou) {
            best_iou = area_iou[i];
        }
    }
    
    // [输出 iou]
    for (int i = 1; i <= color_cnt; i++) {
        if (!valid[i]) {
            continue;
        }
        fprintf(stderr, "area_iou[%d]: %.3f\n", i, area_iou[i]);
    }
    fprintf(stderr, "best_iou: %.3f\n", best_iou);
    // [iou 均太差，退出]
    if (best_iou < IOU_MIN_REQUIRED) {
        return (OptionCircle) {
            .some = 0,
        };
    }
    // [至少要在最佳的 xx% 以上]
    float max_area = 0;
    int best_idx = -1;
    Circle best_circle = (Circle) {
        0, 0, 0,
    };
    for (int i = 1; i <= color_cnt; i++) {
        if (!valid[i]) {
            continue;
        }
        if (area_iou[i] < best_iou * IOU_BY_BEST_MIN_REQUIRED) {
            valid[i] = 0; // 因为不像圆
            continue;
        }
        if (color_area[i] > max_area) {
            max_area = color_area[i];
            best_circle = circles[i];
            best_idx = i;
        }
    }
    
    if (best_idx == -1) {
        return (OptionCircle) {
            .some = 0,
        };
    }
    return (OptionCircle) {
        .some = 1,
        .circle = best_circle,
    };
    // <line_buf returned>
    // <buf1 returned>
    // <buf2 returned>
}

// 从文件读取PPM图片
int ppm_load(char* filename, uint8_t img[HEIGHT][WIDTH]) {
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

    // 读取像素数据
    // fread(buffer, sizeof(Pixel), width * height, file);
    // for (int i = 0; i < width * height; i++) {
    //     img[i / height][i % height] = (buffer[i].red + buffer[i].green + buffer[i].blue) / 3;
    // }
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; ++j) {
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

#if __APPLE__
int main(int argc, char* argv[]) {
    // uint8_t* image = malloc(WIDTH * HEIGHT * sizeof(uint8_t));
    srand(time(0));
    static uint8_t ori[HEIGHT][WIDTH] = { 0 };
    static uint8_t chaos[HEIGHT][WIDTH];
    static uint8_t img[HEIGHT][WIDTH];

    OptionCircle res;
    fprintf(stderr, "Rendering image...\n");
    rand_img(ori, 5, 40);
    // ppm_load(argv[1], ori);
    memcpy(img, ori, sizeof(img));
    binarize(img);
    res = color_img(img);
    for (int i = 1; i <= 100; i++) {
        rand_img(chaos, rand() % 3, rand() % 50 + 5);
        memcpy(img, chaos, sizeof(img));
        binarize(img);
        res = color_img(img); // 搅乱静态数组
        if (res.some == 1 && res.circle.radius == 0) {
            assert(0);
        }

    }

    // https://blog.csdn.net/yy197696/article/details/110103000
    // gauss_filter(img);
    memcpy(img, ori, sizeof(img));
    binarize(img);
    res = color_img(img);
    if (res.some == 0) {
        render(ori);
        fprintf(stderr, "No Circle found.");
        return 0;
    }
    Circle c = res.circle;
    draw_circle(ori, c);
    render(ori);
    fprintf(stderr, "Done\n");
    return 0;
}
#else
void cvHandle(void) {
    uint8_t cv_image[HEIGHT][WIDTH];
    memcpy(cv_image, image, HEIGHT * WIDTH);
    static uint32_t last_tick;
    binarize(cv_image);
    circle = color_img(cv_image);
    // 大概这样写：
    // 先接收一个结果结构体，判断是否找到了圆
    // OptionCircle res = color_img(cv_image);
    // if (res.some == 0) { 没找到圆，错误处理 }
    // else { circle = res.circle; }
    cv_fps = 1000 / (HAL_GetTick() - last_tick);
    last_tick = HAL_GetTick();
}
#endif
