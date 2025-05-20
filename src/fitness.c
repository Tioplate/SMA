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

FIT_DATA_TYPE TSP(int *x, int dim, FIT_DATA_TYPE **distanceMatrix)
{
    if(dim <= 1)
        return 0;
    FIT_DATA_TYPE totalDistance = 0;
    for (int i = 0; i < dim - 1; i++)
    {
        int from = x[i];
        int to = x[i + 1];
        FIT_DATA_TYPE distance = distanceMatrix[from][to];
        totalDistance += distance;
    }
    totalDistance += distanceMatrix[dim - 1][0]; //Need to add the distance of last stop to first stop
    return 1 / totalDistance;
}