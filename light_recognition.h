/**
******************************************************************************
* @file : light_recognition.h
* @author : Albert Wang, July Fun
* @brief : None
* @date : 2023/12/18
******************************************************************************
* Copyright (c) 2023 Team JiaoLong-SJTU
* All rights reserved.
******************************************************************************
*/

#ifndef GUIDEDMISSILE_LIGHT_RECOGNITION_H
#define GUIDEDMISSILE_LIGHT_RECOGNITION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h>
//#include <memory.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "missile.h"

typedef struct {
    int x, y;
} Point;

typedef struct {
    float cx, cy, radius;
} Circle;

void cvHandle(void);

extern Circle circle;
extern uint8_t cv_fps;

#ifdef __cplusplus
}
#endif

#endif //GUIDEDMISSILE_LIGHT_RECOGNITION_H
