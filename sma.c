#include "sma.h"
#define ALPHA_EARLY 0.1
#define ALPHA_LATE  2.0

FIT_DATA_TYPE **initialization(int pop, int DIM)
{
    FIT_DATA_TYPE **x = (FIT_DATA_TYPE **)malloc(pop * sizeof(FIT_DATA_TYPE *));
    srand((unsigned)time(NULL));
    for (int i = 0; i < pop; i++) {
        x[i] = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
        for (int j = 0; j < DIM; j++) {
            // 随机键编码 0~1
            x[i][j] = rand01();
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

SMAResult* SMA(int pop, int DIM, FIT_DATA_TYPE *lb, FIT_DATA_TYPE *ub, char *dataPath, int speed)
{
    FIT_DATA_TYPE **x = initialization(pop, DIM);
    fitnessData *fit = (fitnessData *)malloc(pop * sizeof(fitnessData));
    FIT_DATA_TYPE *bestPositions = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    FIT_DATA_TYPE destinationFitness = FIT_DATA_TYPE_MAX;
    FIT_DATA_TYPE *convergenceCurve = (FIT_DATA_TYPE *)malloc(T * sizeof(FIT_DATA_TYPE));
    FIT_DATA_TYPE **W = (FIT_DATA_TYPE **)malloc(pop * sizeof(FIT_DATA_TYPE *));
    for (int i = 0; i < pop; i++) W[i] = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    dataMatrix *data = readMatrix(dataPath);

    // 初始边界裁剪（移除 i!=j 条件）
    for (int i = 0; i < pop; i++) {
        for (int j = 0; j < DIM; j++) {
            if (x[i][j] > ub[j]) x[i][j] = ub[j];
            if (x[i][j] < lb[j]) x[i][j] = lb[j];
        }
    }

    for (int i = 0; i < pop; i++) {
        fit[i].popIndex = i;
        fit[i].fitness = TSPTW_soft(x[i], DIM, speed, data, ALPHA_EARLY, ALPHA_LATE);
        if (fit[i].fitness < destinationFitness) {
            destinationFitness = fit[i].fitness;
            for (int j = 0; j < DIM; j++) bestPositions[j] = x[i][j];
        }
    }

    int t = 1;
    time_t startTime = time(NULL);
    while (t <= T) {
        fit = sortFitness(fit, pop);
        x = sortIndex(x, fit, pop);
        FIT_DATA_TYPE bestFitness = fit[0].fitness;
        FIT_DATA_TYPE worstFitness = fit[pop - 1].fitness;
        FIT_DATA_TYPE S = bestFitness - worstFitness + 1e-8;

        // 权重更新使用排序后索引 W[i][j]
        for (int i = 0; i < pop; i++) {
            for (int j = 0; j < DIM; j++) {
                if (i < pop / 2)
                    W[i][j] = 1 + rand01() * log10((bestFitness - fit[i].fitness) / S + 1);
                else
                    W[i][j] = 1 - rand01() * log10((bestFitness - fit[i].fitness) / S + 1);
            }
        }

        FIT_DATA_TYPE tt = 1 - (FIT_DATA_TYPE)t / (FIT_DATA_TYPE)T;
        FIT_DATA_TYPE a = (fabs(tt) < 1.0) ? atanh(tt) : (FIT_DATA_TYPE)1;
        FIT_DATA_TYPE b = 1 - (FIT_DATA_TYPE)t / (FIT_DATA_TYPE)T;

        for (int i = 0; i < pop; i++) {
            if (rand01() < Z) { // 重新随机键
                for (int j = 0; j < DIM; j++) {
                    x[i][j] = lb[j] + rand01() * (ub[j] - lb[j]);
                }
            } else { // 公式3
                FIT_DATA_TYPE p = tanh(fabs(fit[i].fitness - destinationFitness));
                FIT_DATA_TYPE *vb = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                FIT_DATA_TYPE *vc = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                for (int j = 0; j < DIM; j++) {
                    FIT_DATA_TYPE r = rand01();
                    int A = rand() % pop;
                    int B = rand() % pop;
                    vb[j] = 2 * a * rand01() - a;
                    vc[j] = 2 * b * rand01() - b;
                    if (r < p) {
                        FIT_DATA_TYPE val = bestPositions[j] + vb[j] * (W[i][j] * x[A][j] - x[B][j]);
                        // 裁剪到边界
                        if (val < lb[j]) val = lb[j];
                        if (val > ub[j]) val = ub[j];
                        x[i][j] = val;
                    } else {
                        FIT_DATA_TYPE val = vc[j] * x[i][j];
                        if (val < lb[j]) val = lb[j];
                        if (val > ub[j]) val = ub[j];
                        x[i][j] = val;
                    }
                }
                free(vb);
                free(vc);
            }
        }

        // 边界再次裁剪
        for (int i = 0; i < pop; i++) {
            for (int j = 0; j < DIM; j++) {
                if (x[i][j] > ub[j]) x[i][j] = ub[j];
                if (x[i][j] < lb[j]) x[i][j] = lb[j];
            }
        }

        for (int i = 0; i < pop; i++) {
            fit[i].popIndex = i;
            fit[i].fitness = TSPTW_soft(x[i], DIM, speed, data, ALPHA_EARLY, ALPHA_LATE);
            if (fit[i].fitness < destinationFitness) {
                destinationFitness = fit[i].fitness;
                for (int j = 0; j < DIM; j++) bestPositions[j] = x[i][j];
            }
        }
        convergenceCurve[t - 1] = destinationFitness;
        if (t % 1 == 0) {
            printf("At iteration %d, the best fitness is %lf.\n", t, destinationFitness);
        }
            /*printf(" The best route is [");
            for (int i = 0; i < DIM - 1; i++)
            {
                //printf("%lf, ", bestPositions[i]);
                printf("%lf, ", x[0][i]);
            }
            //printf("%lf]\n", bestPositions[DIM - 1]);
            printf("%lf\n, ", x[0][DIM - 1]);*/
        t++;
    }
    time_t endTime = time(NULL);
    double elapsedSeconds = difftime(endTime, startTime);
    bestPositions = sortPostionIndex(bestPositions, DIM);
    printf("Done, the best fitness is %lf, time %.0f seconds.\n", destinationFitness, elapsedSeconds);
    printf("Best x set: [");
    for (int i = 0; i < DIM - 1; i++) printf("%lf, ", bestPositions[i]);
    printf("%lf]", bestPositions[DIM - 1]);

    for (int i = 0; i < pop; i++) free(x[i]);
    for (int i = 0; i < pop; i++) free(W[i]);

    SMAResult *result = (SMAResult *)malloc(sizeof(SMAResult));
    result->pop = pop;
    result->dimension = DIM;
    result->iterationATime = T;
    result->destinationFitness = destinationFitness;
    result->bestPositions = bestPositions;
    result->convergenceCurve = convergenceCurve;

    free(x);
    free(fit);
    freeDataMatrix(data);
    free(W);
    return result;
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