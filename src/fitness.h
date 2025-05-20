#include <float.h>
#define FIT_DATA_TYPE_MAX FLT_MAX

typedef double FIT_DATA_TYPE;
typedef struct
{
    FIT_DATA_TYPE fitness; //
    int popIndex;
} fitnessData;


FIT_DATA_TYPE F1(FIT_DATA_TYPE *x, int dim);
FIT_DATA_TYPE TSP(FIT_DATA_TYPE *x, int dim);
