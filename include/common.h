#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#define INITIAL_CAPACITY 1000
#define PAGE_SIZE 15

/* ── 参数合理范围（用于数据修改验证 & 异常值检测）── */
#define VALID_TEMP_MIN       -5.0
#define VALID_TEMP_MAX       40.0
#define VALID_SALINITY_MIN    0.0
#define VALID_SALINITY_MAX   45.0
#define VALID_PH_MIN          6.5
#define VALID_PH_MAX          9.0
#define VALID_DO_MIN          0.0
#define VALID_DO_MAX         15.0
#define VALID_PRECIP_MIN      0.0
#define VALID_PRECIP_MAX    500.0
#define VALID_AIR_TEMP_MIN  -10.0
#define VALID_AIR_TEMP_MAX   50.0

typedef struct {
    char timestamp[32];
    double temp;
    double salinity;
    double pH;
    double DO;
    double precipitation;
    double air_temp;
    bool valid;
} WaterRecord;

typedef struct {
    WaterRecord *records;
    size_t total_count;
    size_t valid_count;
    size_t capacity;
    bool preprocessed;   /* 是否已执行预处理（异常值+缺失值+滤波） */
} WaterDataset;

#endif // COMMON_H
