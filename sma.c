#include "sma.h"

// 说明：
// 本文件实现黏菌算法（SMA）的主流程与若干工具函数。
// 核心思想：
//  - 使用“优先级编码”表示 TSPTW 解：对每个维度的实数键排序得到客户访问顺序（仓库固定0）。
//  - 适应度函数 TSPTW 使用硬时间窗（早到等待，迟到不可行），目标为总行驶距离。
//  - SMA 通过权重 W、vb/vc 两类更新与概率 p 来引导个体向更优位置移动。
//  - 若某代全体不可行，则对人群进行重采样并注入时间窗启发式，提高获得可行解的概率。

// 将区间 [minV, maxV] 线性映射到 [0, dim-1] 并做边界夹取（用于构造启发式优先级键）
static inline FIT_DATA_TYPE normalize_range(double v, double minV, double maxV, int dim)
{
    double denom = (maxV - minV);
    if (denom <= 1e-9) return 0.0;
    double t = (v - minV) / denom;
    if (t < 0) t = 0; if (t > 1) t = 1;
    return (FIT_DATA_TYPE)(t * (dim - 1));
}

/*
    initialization：
    - 生成 pop 个个体，每个个体为长度 DIM 的实数数组（优先级键）。
    - 采用连续随机键，并加入很小的 j 级偏移，减少相等键导致的排序退化。
    - 注意：x[0] 对应仓库键，当前实现中仍会被赋值，但 TSPTW 排序时只使用 1..DIM-1。
*/
FIT_DATA_TYPE **initialization(int pop, int DIM)
{
    FIT_DATA_TYPE **x;
    x = (FIT_DATA_TYPE **)malloc(pop * sizeof(FIT_DATA_TYPE *));
    srand((unsigned)time(NULL)); // 简单随机种子，若需复现实验可改为固定种子
    for (int i = 0; i < pop; i++)
    {
        x[i] = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
        for (int j = 0; j < DIM; j++)
        {
            // 连续随机键，减少大量相等键导致的退化
            x[i][j] = (FIT_DATA_TYPE)rand01() * (DIM - 1) + 1e-6 * j;
        }
    }
    return x;
}
/*
    compare function of fitness, order by ascending
    - 适应度越小越好（总距离越小越优）。
*/
int compareFunction(const void *a, const void *b)
{
    return ((*(fitnessData *)a).fitness > (*(fitnessData *)b).fitness ? 1 : -1);
}

/* sortFitness：按适应度升序排序（原地） */
fitnessData *sortFitness(fitnessData *fit, int size)
{
    qsort(fit, size, sizeof(fit[0]), compareFunction);
    return fit;
}

/*
    sortIndex：
    - 按排序后的适应度顺序重排种群数组指针。
    - 注意：仅重排第一维指针，不复制个体内容（节省内存与时间）。
*/
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

/*
    SMA：算法主循环
    输入：
      - pop：种群规模
      - DIM：维度/节点数
      - lb/ub：变量上下界（对“优先级编码”仅用于产生扰动与重采样，不做硬裁剪）
      - dataPath：数据文件路径，用于读取距离矩阵与时间窗
      - speed：行驶速度（影响可行性判定，非目标值）
    输出：
      - SMAResult 包含最优适应度、对应位置（排序前键）与收敛曲线

    主要步骤：
      1) 初始化种群；
      2) 注入时间窗启发式种子（earliest / latest / mid），提高可行解概率；
      3) 评估适应度，记录当前全局最优；
      4) 迭代 T 代：
         - 对适应度排序并重排种群；
         - 若本代最优为不可行（DBL_MAX），对人群重采样并再注入启发式；
         - 否则计算权重 W，使用公式2/3 更新（vb/vc）；
         - 重新评估适应度并更新全局最优；
      5) 输出结果并释放内存。
*/
SMAResult* SMA(int pop, int DIM, FIT_DATA_TYPE *lb, FIT_DATA_TYPE *ub, char *dataPath, int speed)
{
    FIT_DATA_TYPE **x = initialization(pop, DIM);              // pop initialization
    fitnessData *fit;                                     // 种群个体适应度数组
    FIT_DATA_TYPE *bestPositions;                         //(当前)最优解下X取值的数组（优先级键）
    FIT_DATA_TYPE destinationFitness = FIT_DATA_TYPE_MAX; // 当前最佳适应度
    FIT_DATA_TYPE *convergenceCurve;                      // 收敛曲线
    FIT_DATA_TYPE **W;                                    // 黏菌权重矩阵
    FIT_DATA_TYPE bestFitness;                            // 当代最佳适应度
    time_t startTime, endTime, diffTime;                  // 计时
    dataMatrix *data = readMatrix(dataPath);              // 读取数据

    fit = (fitnessData *)malloc(pop * sizeof(fitnessData));
    convergenceCurve = (FIT_DATA_TYPE *)malloc((T) * sizeof(FIT_DATA_TYPE));
    W = (FIT_DATA_TYPE **)malloc(pop * sizeof(FIT_DATA_TYPE *));
    bestPositions = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    for (int i = 0; i < pop; i++)
    {
        W[i] = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    }

    // 以时间窗启发式对前几个个体进行种子初始化，提升可行解概率
    if (data && data->size >= DIM)
    {
        // 统计早窗/晚窗范围（跳过仓库0）
        double minEar = 1e18, maxEar = -1e18;
        double minLat = 1e18, maxLat = -1e18;
        for (int j = 1; j < DIM; j++)
        {
            if (data->tw[j].earliest < minEar) minEar = data->tw[j].earliest;
            if (data->tw[j].earliest > maxEar) maxEar = data->tw[j].earliest;
            if (data->tw[j].latest < minLat) minLat = data->tw[j].latest;
            if (data->tw[j].latest > maxLat) maxLat = data->tw[j].latest;
        }
        // 个体0：按 earliest 升序映射为优先级键
        for (int j = 0; j < DIM; j++) x[0][j] = 0;
        for (int j = 1; j < DIM; j++)
            x[0][j] = normalize_range(data->tw[j].earliest, minEar, maxEar, DIM) + 1e-6 * j;
        // 个体1：按 latest 升序
        if (pop > 1)
        {
            for (int j = 0; j < DIM; j++) x[1][j] = 0;
            for (int j = 1; j < DIM; j++)
                x[1][j] = normalize_range(data->tw[j].latest, minLat, maxLat, DIM) + 1e-6 * j;
        }
        // 个体2：按 (earliest+latest)/2 升序
        if (pop > 2)
        {
            for (int j = 0; j < DIM; j++) x[2][j] = 0;
            for (int j = 1; j < DIM; j++)
            {
                double mid = 0.5 * (data->tw[j].earliest + data->tw[j].latest);
                x[2][j] = normalize_range(mid, minEar, maxEar, DIM) + 1e-6 * j;
            }
        }
    }

    // 初始评估
    for (int i = 0; i < pop; i++)
    {
        fit[i].popIndex = i;
        fit[i].fitness = TSPTW(x[i], DIM, speed, data);
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
        // 对适应度值排序并同步重排个体
        fit = sortFitness(fit, pop);
        x = sortIndex(x, fit, pop);
        bestFitness = fit[0].fitness;
        FIT_DATA_TYPE worstFitness = fit[pop - 1].fitness;

        int skipUpdate = 0;
        if (bestFitness == FIT_DATA_TYPE_MAX)
        {
            // 本代无可行解：一半随机重采样 + 一半时间窗启发式
            for (int i = 0; i < pop; i++)
            {
                for (int j = 0; j < DIM; j++)
                {
                    x[i][j] = (FIT_DATA_TYPE)rand01() * (DIM - 1) + 1e-6 * j;
                }
            }
            if (data && data->size >= DIM)
            {
                double minEar = 1e18, maxEar = -1e18;
                for (int j = 1; j < DIM; j++)
                {
                    if (data->tw[j].earliest < minEar) minEar = data->tw[j].earliest;
                    if (data->tw[j].earliest > maxEar) maxEar = data->tw[j].earliest;
                }
                for (int i = 0; i < pop/2; i++)
                {
                    x[i][0] = 0;
                    for (int j = 1; j < DIM; j++)
                        x[i][j] = normalize_range(data->tw[j].earliest, minEar, maxEar, DIM) + 1e-6 * j;
                }
            }
            skipUpdate = 1; // 本轮不做基于 W 的位置更新，直接进入下一次评估
        }

        if (!skipUpdate)
        {
            // S = 当前最优与最差的差值 + 极小量，避免分母为 0
            FIT_DATA_TYPE S = bestFitness - worstFitness + 10e-8;
            // 更新黏菌权重（不可行个体权重置 0，避免 NaN/Inf）
            for (int i = 0; i < pop; i++)
            {
                for (int j = 0; j < DIM; j++)
                {
                    if (fit[i].fitness == FIT_DATA_TYPE_MAX)
                    {
                        W[fit[i].popIndex][j] = 0.0;
                        continue;
                    }
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
            // 求 vb、vc 中 acrtanhx 参数 a, b（注意使用浮点除法）
            FIT_DATA_TYPE tt = -((FIT_DATA_TYPE)t / (FIT_DATA_TYPE)T) + 1;
            FIT_DATA_TYPE a, b;
            if (tt != -1 && tt != 1)
            {
                a = atanh(tt);
            }
            else
            {
                a = 1; // 极端情况下给一个合理常数
            }
            b = 1 - (FIT_DATA_TYPE)t / (FIT_DATA_TYPE)T;

            // 位置更新：公式2/3
            for (int i = 0; i < pop; i++)
            {
                // 公式2：以概率 Z 进行全局随机探索
                if (rand01() < Z)
                {
                    for (int j = 0; j < DIM; j++)
                    {
                        FIT_DATA_TYPE newv = (ub[j] - lb[j]) * rand01() + lb[j];
                        x[i][j] = newv;
                    }
                }
                // 公式3：利用 vb/vc、W 进行开发式搜索
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
                        vb[j] = 2 * a * rand01() - a;   // in [-a, a]
                        vc[j] = 2 * b * rand01() - b;   // in [-b, b]
                        if (r < p)
                        {
                            FIT_DATA_TYPE newv = bestPositions[j] + vb[j] * (W[i][j] * x[A][j] - x[B][j]);
                            x[i][j] = newv;
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
        }

        // 重新评估本代所有个体的适应度并更新全局最优
        for (int i = 0; i < pop; i++)
        {
            fit[i].popIndex = i;
            fit[i].fitness = TSPTW(x[i], DIM, speed, data);
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
        }
        t += 1;
    }

    endTime = time(NULL);
    diffTime = difftime(endTime, startTime);
    // 将最优键转为最优的“访问索引序列”以便输出查看（仍保留 bestPositions 数组供外部使用）
    bestPositions = sortPostionIndex(bestPositions, DIM);
    printf("Done, the best fitness is %lf, time %lld seconds.\n", destinationFitness, diffTime);
    printf("Best x set: [");
    for (int i = 0; i < DIM - 1; i++)
    {
        printf("%lf, ", bestPositions[i]);
    }
    printf("%lf]", bestPositions[DIM - 1]);

    // 资源释放与结果封装
    for (int i = 0; i < pop; i++)
    {
        free(x[i]);
        free(W[i]);
    }
    SMAResult *result = (SMAResult *)malloc(sizeof(SMAResult));
    result->pop = pop;
    result->dimension = DIM;
    result->iterationATime = T;
    result->destinationFitness = destinationFitness;
    result->bestPositions = bestPositions;
    result->convergenceCurve = convergenceCurve;
    free(x);
    free(fit);
    // bestPositions / convergenceCurve 交由 result 管理
    freeDataMatrix(data);
    free(W);
    return result;
}

/* rand01：返回 [0,1) 的随机小数 */
FIT_DATA_TYPE rand01()
{
    return rand() / (FIT_DATA_TYPE)(RAND_MAX);
}

/*
    sortPostionIndex：
    - 将“优先级键数组”转换为“访问顺序索引数组”，便于打印与验证。
    - 注意：此处是原地回写，返回的 xi 内容被替换为索引序（0..dim-1 的排列）。
*/
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