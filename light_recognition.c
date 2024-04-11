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

#include <stdint.h>
#include <math.h>

#include "missile.h"

#if !__APPLE__
Circle circle;
uint8_t cv_fps;
#endif

#if __APPLE__
void draw_circle(uint8_t img[HEIGHT][WIDTH], Circle c) {
    for (int i = 0; i < WIDTH; ++i) {
        for (int j = 0; j < HEIGHT; j++) {
            int dx = i - c.cx;
            int dy = j - c.cy;
            int d = dx * dx + dy * dy;
            int r_sq = c.radius * c.radius;
            if (r_sq - 20 <= d && d <= r_sq + 20) {
                img[j][i] = 128;
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
            int out = img[j][i] == 128;
            int pix = img[j][i];
            if (out) {
                // printf("%d %d %d ", 255, 0, 0);
                printf("%d %d %d ", pix, pix, pix);
                // fprintf(stderr, "hello %d %d\n", i, j);
            } else {
                printf("%d %d %d ", pix, pix, pix);
            }
        }
        printf("\n");
    }
}
#endif

float normalized_ratio(float x) {
    if (x > 1.0f) {
        return 1.0f / x;
    }
    return x;
}

void binarize(uint8_t img[HEIGHT][WIDTH]) {
    for (uint8_t i = 0; i < HEIGHT; i++) {
        for (uint8_t j = 0; j < WIDTH; j++) {
            img[i][j] = img[i][j] > BINARY_THRESHOLD ? 255 : 0;
        }
    }
}

Circle color_img(uint8_t img[HEIGHT][WIDTH]) {
    const int dx[] = { 0, 0, 1, -1 };
    const int dy[] = { 1, -1, 0, 0 };

    // <buf1 borrowed>
    // <line_buf borrowed>
    memset(buf1, 0, sizeof(buf1));
    memset(line_buf, 0, sizeof(line_buf));
    uint8_t(*color)[WIDTH] = buf1;
    uint16_t* color_area = line_buf;
    int color_cnt = 0;
    // [找连通块，染色]
    int head = 0, tail = -1;
    for (int j = 0; j < WIDTH && color_cnt + 1 < LINE_BUF_SIZE; j++) {
        for (int i = 0; i < HEIGHT && color_cnt + 1 < LINE_BUF_SIZE; i++) { // 不能双重 break 无语
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

    // [bfs 求距离]
    // <buf2 borrowed>
    head = 0, tail = -1;
    memset(buf2, 127, sizeof(buf2));
    uint8_t(*outer_dis)[WIDTH] = buf2;
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            // 边缘和无 color 处（注意不是 img = 0）距离为 0
            if (color[i][j] == 0 || i == 0 || i == HEIGHT - 1 || j == 0 || j == WIDTH - 1) {
                outer_dis[i][j] = 0;
                queue[++tail][0] = i;
                queue[tail][1] = j;
            }
            else {
                outer_dis[i][j] = UINT8_MAX; // <warn>
            }
        }
    }  
    // render(dis);
    // [bfs 求距离边缘] 
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
    static float ave_r2[LINE_BUF_SIZE];
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
    for (int i = 1; i <= color_cnt; i++) {
        if (ave_r2_cnt[i] == 0) {
            continue;
        }
        ave_r2[i] /= ave_r2_cnt[i];
    }

    // [求圆]
    static Circle circles[LINE_BUF_SIZE]; // all 0 不共享
    memset(circles, 0, sizeof(circles));
    for (int i = 1; i <= color_cnt; i++) { // 0 也求
        circles[i] = (Circle) {
            .cx = mass_center[i][1],
            .cy = mass_center[i][0],
            .radius = sqrt(ave_r2[i]),
        };
    }
    // for (int i = 1; i <= color_cnt; i++) {
    //     fprintf(stderr, "circle[%d]: %d %d %d\n", i, circles[i].cx, circles[i].cy, circles[i].radius);
    // }
    
    // [求圆和整个连通块的 iou]
    static float area_iou[LINE_BUF_SIZE];
    static float area_union[LINE_BUF_SIZE];
    for (int i = 1; i <= color_cnt; i++) {
        area_union[i] = M_PI * ave_r2[i]; // 这里 ave_r2 精度高点
    }
    float best_iou = 0;
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
    // [处理 iou]
    for (int i = 1; i <= color_cnt; i++) {
        area_iou[i] /= area_union[i];
        if (area_iou[i] > best_iou) {
            best_iou = area_iou[i];
        }
    }
    // [至少要在最佳的 xx% 以上]
    float max_area = 0;
    Circle best_circle = (Circle) {
        0, 0, 0,
    };
    for (int i = 1; i <= color_cnt; i++) {
        if (area_iou[i] >= best_iou * 0.928888) {
            if (color_area[i] > max_area) {
                max_area = color_area[i];
                best_circle = circles[i];
            }
        }
    }
    
    return best_circle;
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
    static uint8_t img[HEIGHT][WIDTH];
    fprintf(stderr, "Rendering image...\n");
    // rand_img(ori);
    ppm_load(argv[1], ori);
    memcpy(img, ori, sizeof(img));
    // https://blog.csdn.net/yy197696/article/details/110103000
    // gauss_filter(img);
    binarize(img);
    // error when  c.radius == 0
    Circle c = color_img(img);
    if (c.radius == 0) {
        fprintf(stderr, "No Circle found.");
        return 0;
    }
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
    cv_fps = 1000 / (HAL_GetTick() - last_tick);
    last_tick = HAL_GetTick();
}
#endif
