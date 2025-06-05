#include "fitness.h"

FIT_DATA_TYPE **initialization(int pop);
FIT_DATA_TYPE rand01();
int compareFunction(const void *a, const void *b);
fitnessData *sortFitness(fitnessData *fit, int size);
FIT_DATA_TYPE **sortIndex(FIT_DATA_TYPE **x, fitnessData *fit, int pop);
void SMA(int pop, FIT_DATA_TYPE *lb, FIT_DATA_TYPE *ub);
FIT_DATA_TYPE *sortPostionIndex(FIT_DATA_TYPE *xi, int dim);