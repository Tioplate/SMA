#include "sma.h"

FIT_DATA_TYPE **initialization(int pop)
{
    FIT_DATA_TYPE **x;
    x = (FIT_DATA_TYPE **)malloc(pop * sizeof(FIT_DATA_TYPE *));
    srand((unsigned)time(NULL));
    for (int i = 0; i < pop; i++)
    {
        x[i] = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
        for (int j = 0; j < DIM; j++)
        {
            x[i][j] = (int)(rand() % DIM);
            for (int r = 0; r < j; r++)
            {
                while(x[i][r] == x[i][j])
                {
                    j--;
                }
            }
        }
    }
        return x;
}
/*
    compare function of fitness, order by ascending
*/
int compareFunction(const void *a, const void *b)
{
    return ((*(fitnessData *)a).fitness > (*(fitnessData *)b).fitness ? 1 : -1);
}

fitnessData *sortFitness(fitnessData *fit, int size)
{
    qsort(fit, size, sizeof(fit[0]), compareFunction);
    return fit;
}

FIT_DATA_TYPE **sortIndex(FIT_DATA_TYPE **x, fitnessData *fit, int pop)
{
    FIT_DATA_TYPE **xNew;
    xNew = (FIT_DATA_TYPE **)malloc(pop * sizeof(FIT_DATA_TYPE *));
    for (int i = 0; i < pop; i++)
    {
        xNew[i] = x[fit[i].popIndex];
    }  
    free(x);
    return xNew;
}

void SMA(int pop, FIT_DATA_TYPE *lb, FIT_DATA_TYPE *ub)
{
    FIT_DATA_TYPE **x = initialization(pop);              // pop initialization
    fitnessData *fit;                                     // 种群个体适应度数组
    FIT_DATA_TYPE *bestPositions;                         //(当前)最优解下X取值的数组
    FIT_DATA_TYPE destinationFitness = FIT_DATA_TYPE_MAX; // 即当前最佳适应度
    FIT_DATA_TYPE *convergenceCurve;
    FIT_DATA_TYPE **W;
    FIT_DATA_TYPE bestFitness;
    time_t startTime, endTime, diffTime;

    fit = (fitnessData *)malloc(pop * sizeof(fitnessData));
    convergenceCurve = (FIT_DATA_TYPE *)malloc((T - 1) * sizeof(FIT_DATA_TYPE));
    W = (FIT_DATA_TYPE **)malloc((pop * sizeof(FIT_DATA_TYPE *)));
    bestPositions = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    for (int i = 0; i < DIM; i++)
    {
        W[i] = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    }
    for (int i = 0; i < pop; i++) // bound check
    {
        for (int j = 0; j < DIM; j++)
        {
            if (x[i][j] > ub[j] && i != j)
                x[i][j] = ub[j];
            if (x[i][j] < lb[j] && i != j)
                x[i][j] = lb[j];
        }
    }
    for (int i = 0; i < pop; i++)
    {
        // 适应度计算
        fit[i].popIndex = i;
        fit[i].fitness = TSP(x[i], DIM);
        // 更新食物位置
        if (fit[i].fitness < destinationFitness)
        {
            destinationFitness = fit[i].fitness;
            for (int j = 0; j < DIM; j++)
            {
                bestPositions[j] = x[fit[i].popIndex][j];
            }
        }
    }
    int t = 1;
    startTime = time(NULL);
    while (t <= T)
    {
        // 对适应度值排序
        fit = sortFitness(fit, pop);
        x = sortIndex(x, fit, pop);
        bestFitness = fit[0].fitness;
        FIT_DATA_TYPE worstFitness = fit[pop - 1].fitness;
        FIT_DATA_TYPE S = bestFitness - worstFitness + 10e-8; // 当前最优适应度于最差适应度的差值，10E-8为极小值，避免分母为0
                                                              // 更新黏菌权重
        for (int i = 0; i < pop; i++)
        {
            for (int j = 0; j < DIM; j++)
            {
                if (i < pop / 2)
                {
                    W[fit[i].popIndex][j] = 1 + rand01() * log10((bestFitness - fit[i].fitness) / (S) + 1);
                }
                else
                {
                    W[fit[i].popIndex][j] = 1 - rand01() * log10((bestFitness - fit[i].fitness) / (S) + 1);
                }
            }
        }
        // 求vb，vc中acrtanhx参数a, b
        FIT_DATA_TYPE tt = -(t / T) + 1;
        FIT_DATA_TYPE a, b;
        if (tt != -1 && tt != 1)
        {
            a = atanh(tt);
        }
        else
        {
            a = 1;
        }
        b = 1 - t / T;

        for (int i = 0; i < pop; i++)
        {
            // formula 2
            if (rand01() < Z)
            {
                for (int j = 0; j < DIM; j++)
                {
                    int test = (ub[j] - lb[j]) * rand01() + lb[j];
                    x[i][j] = test;
                }
            }
            // formula 3
            else
            {
                FIT_DATA_TYPE p = tanh(fabs(fit[i].fitness - destinationFitness));
                FIT_DATA_TYPE *vb = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                FIT_DATA_TYPE *vc = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                for (int j = 0; j < DIM; j++)
                {
                    FIT_DATA_TYPE r = rand01();
                    int A = rand() % pop;
                    int B = rand() % pop;
                    vb[j] = 2 * a * rand01() - a;   //[-a, a]
                    vc[j] = 2 * b * rand01() - b;
                    if (r < p)
                    {
                        int test = bestPositions[j] + vb[j] * (W[i][j] * x[A][j] - x[B][j]);
                        x[i][j] = test;
                    }
                    else
                    {
                        x[i][j] = vc[j] * x[i][j];
                    }
                }
                free(vb);
                free(vc);
            }
        }
        for (int i = 0; i < pop; i++) // bound check
        {
            for (int j = 0; j < DIM; j++)
            {
                if (x[i][j] > ub[j] && i != j)
                    x[i][j] = ub[j];
                if (x[i][j] < lb[j] && i != j)
                    x[i][j] = lb[j];
            }
        }
        for (int i = 0; i < pop; i++)
        {
            // 适应度计算
            fit[i].popIndex = i;
            fit[i].fitness = TSP(x[i], DIM);
            // 更新食物位置
            if (fit[i].fitness < destinationFitness)
            {
                destinationFitness = fit[i].fitness;
                for (int j = 0; j < DIM; j++)
                {
                    bestPositions[j] = x[fit[i].popIndex][j];
                }
            }
        }
        convergenceCurve[t - 1] = destinationFitness;
        if (t % 1 == 0)
        {
            printf("At iteration %d, the best fitness is %lf.\n", t, destinationFitness);
            /*printf(" The best route is [");
            for (int i = 0; i < DIM - 1; i++)
            {
                //printf("%lf, ", bestPositions[i]);
                printf("%lf, ", x[0][i]);
            }
            //printf("%lf]\n", bestPositions[DIM - 1]);
            printf("%lf\n, ", x[0][DIM - 1]);*/
        }
        t += 1;
    }
    endTime = time(NULL);
    diffTime = difftime(endTime, startTime);
    bestPositions = sortPostionIndex(bestPositions, DIM);
    printf("Done, the best fitness is %lf, time %lld seconds.\n", destinationFitness, diffTime);
    printf("Best x set: [");
    for (int i = 0; i < DIM - 1; i++)
    {
        printf("%lf, ", bestPositions[i]);
    }
    printf("%lf]", bestPositions[DIM - 1]);
    for (int i = 0; i < pop; i++)
    {
        free(x[i]);
        free(W[i]);
    }
    free(x);
    free(fit);
    free(bestPositions);
    //free(convergenceCurve);
    free(W);
}
FIT_DATA_TYPE rand01()
{
    return rand() / (FIT_DATA_TYPE)(RAND_MAX);
}

FIT_DATA_TYPE *sortPostionIndex(FIT_DATA_TYPE *xi, int dim)
{
    if (dim <= 0)
    {
        return NULL;
    }
    xData *order = (xData *)malloc(dim * sizeof(xData));
    for (int i = 0; i < dim; i++)
    {
        order[i].xIndex = i;
        order[i].data = xi[i];
    }
    order = sortX(order, dim);
    for (int i = 0; i < dim; i++)
    {
        xi[i] = order[i].xIndex;
    }
    free(order);
    return xi;
}