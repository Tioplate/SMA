#include <float.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "init.h"
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



FIT_DATA_TYPE F1(FIT_DATA_TYPE *x, int dim);
FIT_DATA_TYPE TSP(FIT_DATA_TYPE *x, int dim);
xData *sortX(xData *order, int dim);
FIT_DATA_TYPE **adjustPostion(FIT_DATA_TYPE **x, int dim);