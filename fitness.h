#pragma once
#include <float.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "init.h"
#include "readMatrix.h"
#define FIT_DATA_TYPE_MAX FLT_MAX

typedef double FIT_DATA_TYPE;
typedef struct
{
    FIT_DATA_TYPE fitness; //
    int popIndex;
} fitnessData;
typedef struct
{
    FIT_DATA_TYPE data;
    int xIndex;
} xData;
typedef struct 
{
    FIT_DATA_TYPE distance;
    FIT_DATA_TYPE time;
    int xIndex
} xDataTime;




FIT_DATA_TYPE F1(FIT_DATA_TYPE *x, int dim);
FIT_DATA_TYPE TSP(FIT_DATA_TYPE *x, int dim);
FIT_DATA_TYPE TSPTW(FIT_DATA_TYPE *x, int dim, FIT_DATA_TYPE speed, dataMatrix *routeData);
xData *sortX(xData *order, int dim);
FIT_DATA_TYPE **adjustPostion(FIT_DATA_TYPE **x,int pop, int dim);