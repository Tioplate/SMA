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
} SMAResult;

FIT_DATA_TYPE **initialization(int pop, int DIM);
FIT_DATA_TYPE rand01();
int compareFunction(const void *a, const void *b);
fitnessData *sortFitness(fitnessData *fit, int size);
FIT_DATA_TYPE **sortIndex(FIT_DATA_TYPE **x, fitnessData *fit, int pop);
SMAResult* SMA(int pop, int DIM, const FIT_DATA_TYPE *lb, const FIT_DATA_TYPE *ub, const char *dataPath, int speed);
FIT_DATA_TYPE *sortPostionIndex(FIT_DATA_TYPE *xi, int dim);