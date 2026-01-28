#pragma once
#include <float.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "init.h"
#include "readMatrix.h"
// 说明：
// 本模块定义了适应度计算相关的类型与接口。
// 其中 FIT_DATA_TYPE 为 double，FIT_DATA_TYPE_MAX 使用 DBL_MAX 表示“不可行/无穷大”。
// TSPTW 使用硬时间窗：早到可等待，迟到直接判不可行（返回 FIT_DATA_TYPE_MAX）。

// 使用与 FIT_DATA_TYPE 匹配的最大值（double），用于表示不可行/无穷大
#define FIT_DATA_TYPE_MAX DBL_MAX

// 适应度与位置编码都统一用 double
typedef double FIT_DATA_TYPE;

// fitnessData：记录某个个体的适应度值与其在种群中的下标
typedef struct
{
    FIT_DATA_TYPE fitness; // 个体适应度（越小越好）
    int popIndex;          // 个体在种群数组中的索引
} fitnessData;

// xData：用于“优先级编码”的排序辅助结构，data 为键值、xIndex 为原始维度索引
typedef struct
{
    FIT_DATA_TYPE data; // 排序键（优先级）
    int xIndex;         // 对应的原始维度索引
} xData;

// xDataTime：如需同时按距离/时间对位置数据进行排序或记录，可用此结构（当前实现未使用）
typedef struct
{
    FIT_DATA_TYPE distance;
    FIT_DATA_TYPE time;
    int xIndex;
} xDataTime;

// 适应度函数声明
// F1：示例函数，sum(x^2)
FIT_DATA_TYPE F1(FIT_DATA_TYPE *x, int dim);
// TSP：若仅考虑距离的 TSP（当前未启用，保留示例）
FIT_DATA_TYPE TSP(FIT_DATA_TYPE *x, int dim);
// TSPTW：带时间窗的 TSP（硬约束），speed 为行驶速度，routeData 提供距离矩阵与时间窗
FIT_DATA_TYPE TSPTW(FIT_DATA_TYPE *x, int dim, FIT_DATA_TYPE speed, dataMatrix *routeData);
// TSPTW：带修复机制的 TSPTW，如果解严重不可行会尝试贪心修复
FIT_DATA_TYPE TSPTW_WithRepair(FIT_DATA_TYPE *x, int dim, FIT_DATA_TYPE speed, dataMatrix *routeData);
// repairSolutionGreedy：贪心修复函数，按时间窗最早时间重新排序客户
void repairSolutionGreedy(FIT_DATA_TYPE *x, int dim, dataMatrix *routeData, FIT_DATA_TYPE speed);
// sortX：对 xData 数组按 data 升序排序，返回传入指针（原地排序）
xData *sortX(xData *order, int dim);
// adjustPostion：示例函数，对每个个体的维度做排序演示（当前不改变原 x，返回原指针）
FIT_DATA_TYPE **adjustPostion(FIT_DATA_TYPE **x,int pop, int dim);