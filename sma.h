#pragma once
#include "fitness.h"
typedef struct
{
    int pop;
    int dimension;
    int iterationATime;
    FIT_DATA_TYPE destinationFitness;
    FIT_DATA_TYPE *convergenceCurve; // 数组，大小为iterationATime
    FIT_DATA_TYPE *bestPositions;    // 数组，大小为dimension
    FIT_DATA_TYPE *bestPositionsStart; // 最初始最优解下X取值的数组（优先级键）
    FIT_DATA_TYPE finalDistance;     // 新增：最终路线的实际距离（不含惩罚）
    FIT_DATA_TYPE finalMakespan;     // 新增：最终路线的总时间（makespan）
    FIT_DATA_TYPE elapsedTimeMs;     // 新增：算法运行时间，单位毫秒
    int earlyStopTriggered;          // 新增：是否因达到预期结果而提前停止
} SMAResult;

FIT_DATA_TYPE **initialization(int pop, int DIM);
FIT_DATA_TYPE rand01();
int compareFunction(const void *a, const void *b);
fitnessData *sortFitness(fitnessData *fit, int size);
FIT_DATA_TYPE **sortIndex(FIT_DATA_TYPE **x, fitnessData *fit, int pop);
// 原有函数：基于固定迭代次数
SMAResult* SMA(int pop, int DIM, const FIT_DATA_TYPE *lb, const FIT_DATA_TYPE *ub, const char *dataPath, int speed);
// 新增函数：基于时间限制（秒）运行
SMAResult* SMA_TimeLimited(int pop, int DIM, const FIT_DATA_TYPE *lb, const FIT_DATA_TYPE *ub, const char *dataPath, int speed, double timeLimitSeconds);
// 新增函数：基于时间限制且可提前停止的版本
SMAResult* SMA_TimeLimited_WithEarlyStop(int pop, int DIM, const FIT_DATA_TYPE *lb, const FIT_DATA_TYPE *ub, const char *dataPath, int speed, double timeLimitSeconds, double expectedMakespan);
FIT_DATA_TYPE *sortPostionIndex(FIT_DATA_TYPE *xi, int dim);
void localSearch2Opt(FIT_DATA_TYPE *keys, int dim, int speed, dataMatrix *data, int maxTries);
void localSearch2Opt(FIT_DATA_TYPE *keys, int dim, int speed, dataMatrix *data, int maxTries);