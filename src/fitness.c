#include "fitness.h"

/*
    F1 function fitness == sum(x^2)
*/
FIT_DATA_TYPE F1(FIT_DATA_TYPE *x, int dim)
{
    FIT_DATA_TYPE fitness = 0;
    for (int i = 0; i < dim; i++)
    {
        fitness += x[i] * x[i];
    }
    return fitness;
}

FIT_DATA_TYPE TSP(FIT_DATA_TYPE *x, int dim)
{
    if(dim <= 1)
        return 0;
    xData *order = (xData *)malloc(dim * sizeof(xData));
    for (int i = 0; i < dim; i++)
    {
        order[i].xIndex = i;
        order[i].data = x[i];
    }
    order = sortX(order, dim);
    FIT_DATA_TYPE totalDistance = 0;
    for (int i = 0; i < dim - 1; i++)
    {
        int from = order[i].xIndex;
        int to = order[i + 1].xIndex;
        FIT_DATA_TYPE distance = distanceMatrix[from][to];
        totalDistance += distance;
    }
    totalDistance += distanceMatrix[order[dim - 1].xIndex][order[0].xIndex]; //Need to add the distance of last stop to first stop
    free(order);
    return totalDistance;
}
int compareFunctionX(const void *a, const void *b)
{
    return ((*(xData *)a).data > (*(xData *)b).data ? 1 : -1);
}
xData *sortX(xData *order, int dim)
{
    qsort(order, dim, sizeof(order[0]), compareFunctionX);
    return order;
}

/*int **adjustPostion(int **x, int pop, int dim)
{
    for (int i = 0; i < pop; i++)
    {
        xData *order = (xData *)malloc(dim * sizeof(xData));
        for (int j = 0; j < dim; j++)
        {
            order[j].xIndex = j;
            order[j].data = x[j];
        }
        order = sortX(order, dim);
    }
}*/