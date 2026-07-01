#include "sma.h"

#include <string.h>

// 前向声明（函数定义在文件后面）
static void generateOppositionSolution(const FIT_DATA_TYPE *keys, FIT_DATA_TYPE *oppKeys, int dim);
static void adaptiveLargeMutation(FIT_DATA_TYPE *keys, int dim, double intensity, dataMatrix *data);
static void layeredRestart(FIT_DATA_TYPE **x, int pop, int dim, dataMatrix *data);

// 生成 [0,1) 范围内的随机浮点数
FIT_DATA_TYPE rand01()
{
    return (FIT_DATA_TYPE)rand() / (FIT_DATA_TYPE)RAND_MAX;
}

/*
=================================================================================
    黏菌算法（Slime Mould Algorithm, SMA）求解TSPTW问题
=================================================================================

【核心编码方案】
  - 优先级编码（Priority-based Encoding）：
    * 每个个体 x[i] 是一个长度为 DIM 的实数数组（优先级键）
    * x[i][0] 对应仓库（depot），x[i][1..DIM-1] 对应客户节点
    * 解码：将客户键 x[i][1..DIM-1] 按值升序排序，得到访问顺序
    * 例如：keys = [_, 3.5, 1.2, 7.8] → 访问顺序为 0 -> 客户2 -> 客户1 -> 客户3 -> 0

【TSPTW问题特性】
  - 目标：最小化总行驶距离（闭合路线：0 -> 客户们 -> 0）
  - 约束：硬时间窗（到达早于 earliest 可等待，晚于 latest 直接不可行）
  - 难点：优先级键的微小变化会导致排序剧变，极易违反时间窗

【原始SMA的问题】
  - 盲目朝 bestPositions 靠拢 → 轻微扰动就违反时间窗 → 99% 更新产生不可行解
  - 算法困在"不可行解空间"打转，全局最优一直不更新
  - 只有末期随机重启才偶然找到新可行解（非正常优化行为）

【本实现的改进机制】
  1. 时间窗启发式初始化：前3个个体按 earliest/latest/mid 排序，提高初始可行解概率
  2. 可行解保持策略：每次更新后立即评估，若新解不可行但旧解可行则回退
  3. 保守步长：vb 分支步长缩小到 10%，vc 分支到 5%，避免过度扰动
  4. 提高探索下限：全局探索概率最低 10%（原 3%），增强跳出局部最优能力
  5. 停滞重启机制：连续无改进 patience 代后，重启底部 30% 个体
  6. 不可行解主动注入：新旧都不可行时，50% 概率注入 earliest 启发式
  7. 键交换微扰：对底部 40% 个体以 20% 概率做小幅键值交换，改变排序但保持值域

【结果】
  - 修复后：从第2个里程碑（~10% 迭代）就开始持续改进，整个过程平滑收敛
  - 修复前：前 80% 迭代完全卡死，只有末期才突然跳变
=================================================================================
*/

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
    if (denom <= 1e-9) return
    0.0;
    double t = (v - minV) / denom;
    if (t < 0) t = 0; if (t > 1) t = 1;
    return (FIT_DATA_TYPE)(t * (dim - 1));
}

// 针对优先级键的微扰：在客户键 1..dim-1 范围内随机交换若干对键值，改变访问顺序（文件作用域定义）
static inline void smallKeySwaps(FIT_DATA_TYPE *keys, int dim, int swaps)
{
    if (!keys || dim <= 2 || swaps <= 0) return;
    for (int s = 0; s < swaps; ++s)
    {
        int u = 1 + rand() % (dim - 1);
        int v = 1 + rand() % (dim - 1);
        if (u == v) continue;
        FIT_DATA_TYPE tmp = keys[u];
        keys[u] = keys[v];
        keys[v] = tmp;
    }
}

// 新增：计算种群多样性（基于解码后的访问顺序汉明距离）
static double calculateDiversity(FIT_DATA_TYPE **x, int pop, int dim)
{
    if (pop <= 1 || dim <= 1) return 0.0;

    // 为每个个体解码出访问顺序
    int **routes = (int **)malloc(pop * sizeof(int *));
    for (int i = 0; i < pop; i++)
    {
        routes[i] = (int *)malloc((dim - 1) * sizeof(int));
        xData *order = (xData *)malloc((dim - 1) * sizeof(xData));
        for (int j = 1, k = 0; j < dim; j++, k++)
        {
            order[k].xIndex = j;
            order[k].data = x[i][j];
        }
        sortX(order, dim - 1);
        for (int k = 0; k < dim - 1; k++)
        {
            routes[i][k] = order[k].xIndex;
        }
        free(order);
    }

    // 计算所有个体对之间的平均汉明距离
    double totalDistance = 0.0;
    int pairCount = 0;
    for (int i = 0; i < pop - 1; i++)
    {
        for (int j = i + 1; j < pop; j++)
        {
            int diffCount = 0;
            for (int k = 0; k < dim - 1; k++)
            {
                if (routes[i][k] != routes[j][k]) diffCount++;
            }
            totalDistance += (double)diffCount / (double)(dim - 1);
            pairCount++;
        }
    }

    for (int i = 0; i < pop; i++)
    {
        free(routes[i]);
    }
    free(routes);

    return (pairCount > 0) ? (totalDistance / pairCount) : 0.0;
}

// 新增：2-opt局部搜索（针对优先级编码的TSPTW）
void localSearch2Opt(FIT_DATA_TYPE *keys, int dim, int speed, dataMatrix *data, int maxTries)
{
    if (!keys || !data || dim <= 3) return;

    // 解码当前键为访问顺序
    xData *order = (xData *)malloc((dim - 1) * sizeof(xData));
    for (int i = 1, k = 0; i < dim; i++, k++)
    {
        order[k].xIndex = i;
        order[k].data = keys[i];
    }
    sortX(order, dim - 1);

    // 当前适应度
    FIT_DATA_TYPE currentFitness = TSPTW(keys, dim, speed, data);
    if (currentFitness == FIT_DATA_TYPE_MAX)
    {
        free(order);
        return; // 不可行解不做局部搜索
    }

    int improved = 1;
    int tries = 0;

    while (improved && tries < maxTries)
    {
        improved = 0;
        tries++;

        // 尝试所有可能的2-opt交换（在客户序列中）
        for (int i = 0; i < dim - 2 && !improved; i++)
        {
            for (int j = i + 1; j < dim - 1 && !improved; j++)
            {
                // 【新增】O(1) 剪枝：检查交换后引入的两条新边是否在预处理时被标记为不可达（极大值）
                int nodeBeforeI = (i == 0) ? 0 : order[i - 1].xIndex;
                int nodeAfterJ = (j == dim - 2) ? 0 : order[j + 1].xIndex;

                if (data->dist[nodeBeforeI][order[j].xIndex] >= 9999998.0 ||
                    data->dist[order[i].xIndex][nodeAfterJ] >= 9999998.0)
                {
                    continue; // 必然不可行，直接跳过内存分配和耗时的 TSPTW 评估
                }

                // 交换order[i]和order[j]
                int tmpIdx = order[i].xIndex;
                order[i].xIndex = order[j].xIndex;
                order[j].xIndex = tmpIdx;

                // 重新构造优先级键（保持排序关系）
                FIT_DATA_TYPE *newKeys = (FIT_DATA_TYPE *)malloc(dim * sizeof(FIT_DATA_TYPE));
                newKeys[0] = keys[0];
                for (int k = 0; k < dim - 1; k++)
                {
                    int nodeIdx = order[k].xIndex;
                    newKeys[nodeIdx] = (FIT_DATA_TYPE)k + 1e-6 * k;
                }

                // 评估新解
                FIT_DATA_TYPE newFitness = TSPTW(newKeys, dim, speed, data);

                if (newFitness < currentFitness)
                {
                    // 接受改进
                    for (int k = 0; k < dim; k++)
                    {
                        keys[k] = newKeys[k];
                    }
                    currentFitness = newFitness;
                    improved = 1;
                    free(newKeys);
                    break;
                }
                else
                {
                    // 恢复交换
                    tmpIdx = order[i].xIndex;
                    order[i].xIndex = order[j].xIndex;
                    order[j].xIndex = tmpIdx;
                }

                free(newKeys);
            }
        }
    }

    free(order);
}

// 新增：Or-opt局部搜索（移动一个或两个连续客户到其他位置）
void localSearchOrOpt(FIT_DATA_TYPE *keys, int dim, int speed, dataMatrix *data, int maxTries)
{
    if (!keys || !data || dim <= 3) return;

    xData *order = (xData *)malloc((dim - 1) * sizeof(xData));
    for (int i = 1, k = 0; i < dim; i++, k++)
    {
        order[k].xIndex = i;
        order[k].data = keys[i];
    }
    sortX(order, dim - 1);

    FIT_DATA_TYPE currentFitness = TSPTW(keys, dim, speed, data);
    if (currentFitness == FIT_DATA_TYPE_MAX)
    {
        free(order);
        return;
    }

    int improved = 1;
    int tries = 0;

    while (improved && tries < maxTries)
    {
        improved = 0;
        tries++;

        // 尝试移动单个客户
        for (int i = 0; i < dim - 1 && !improved; i++)
        {
            for (int j = 0; j < dim - 1 && !improved; j++)
            {
                if (i == j) continue;

                // 【新增】O(1) 剪枝预判：如果是将 i 插入到 j 的位置，会引入新的边。
                // 这取决于我们是向前移动还是向后移动。
                // 简单起见，我们可以在构造 tempOrder 后，检查新形成的关键连接是否被标记为极大值。

                // 保存原始order
                int *tempOrder = (int *)malloc((dim - 1) * sizeof(int));
                for (int k = 0; k < dim - 1; k++)
                {
                    tempOrder[k] = order[k].xIndex;
                }

                // 移动i到j位置
                int moved = tempOrder[i];
                if (i < j)
                {
                    for (int k = i; k < j; k++)
                    {
                        tempOrder[k] = tempOrder[k + 1];
                    }
                }
                else
                {
                    for (int k = i; k > j; k--)
                    {
                        tempOrder[k] = tempOrder[k - 1];
                    }
                }
                tempOrder[j] = moved;

                // 【新增】剪枝：快速检查转移点附近的新边是否有效
                // Or-opt 移动一个节点到 j 处，会形成两条全新的连接：
                // tempOrder[j-1] -> tempOrder[j] (若 j>0)   或  0 -> tempOrder[0]
                // tempOrder[j] -> tempOrder[j+1] (若 j<dim-2) 或 tempOrder[dim-2] -> 0
                // 另外原本 i 处的节点移走后，i 前后的节点相连也是新边。为求速度，这里至少检查最易违规的 j 处连接。
                int prevJ = (j == 0) ? 0 : tempOrder[j - 1];
                int nextJ = (j == dim - 2) ? 0 : tempOrder[j + 1];
                int currJ = tempOrder[j];

                if (data->dist[prevJ][currJ] >= 9999998.0 ||
                    data->dist[currJ][nextJ] >= 9999998.0)
                {
                    free(tempOrder);
                    continue; // 必然不可行，直接跳过 TSPTW() 评估
                }

                // 重新构造键
                FIT_DATA_TYPE *newKeys = (FIT_DATA_TYPE *)malloc(dim * sizeof(FIT_DATA_TYPE));
                newKeys[0] = keys[0];
                for (int k = 0; k < dim - 1; k++)
                {
                    int nodeIdx = tempOrder[k];
                    newKeys[nodeIdx] = (FIT_DATA_TYPE)k + 1e-6 * k;
                }

                FIT_DATA_TYPE newFitness = TSPTW(newKeys, dim, speed, data);

                if (newFitness < currentFitness)
                {
                    for (int k = 0; k < dim; k++)
                    {
                        keys[k] = newKeys[k];
                    }
                    currentFitness = newFitness;
                    improved = 1;
                    free(newKeys);
                    break;
                }

                free(newKeys);
                free(tempOrder);
            }
        }
    }

    free(order);
}

// 新增：路径重链接（Path Relinking）- 在两个解之间进行路径搜索
static void pathRelinking(FIT_DATA_TYPE *solution1, FIT_DATA_TYPE *solution2,
                          FIT_DATA_TYPE *best, int dim, int speed, dataMatrix *data)
{
    if (!solution1 || !solution2 || !best || !data || dim <= 1) return;

    FIT_DATA_TYPE *current = (FIT_DATA_TYPE *)malloc(dim * sizeof(FIT_DATA_TYPE));
    memcpy(current, solution1, dim * sizeof(FIT_DATA_TYPE));

    FIT_DATA_TYPE bestFit = TSPTW(current, dim, speed, data);
    if (bestFit == FIT_DATA_TYPE_MAX) {
        free(current);
        return; // 起点不可行则放弃
    }

    // 逐步向solution2靠近，每次移动一个维度
    int maxSteps = dim * 2; // 最多尝试2*dim步
    for (int step = 0; step < maxSteps; step++)
    {
        // 找差异最大的维度（客户键，跳过仓库）
        int maxDiffIdx = -1;
        FIT_DATA_TYPE maxDiff = 0.0;
        for (int j = 1; j < dim; j++) {
            FIT_DATA_TYPE diff = fabs(current[j] - solution2[j]);
            if (diff > maxDiff) {
                maxDiff = diff;
                maxDiffIdx = j;
            }
        }

        if (maxDiffIdx < 0 || maxDiff < 1e-6) break; // 已收敛

        // 向solution2的方向移动该维度（70%靠近）
        FIT_DATA_TYPE oldVal = current[maxDiffIdx];
        current[maxDiffIdx] = 0.7 * current[maxDiffIdx] + 0.3 * solution2[maxDiffIdx];

        FIT_DATA_TYPE newFit = TSPTW(current, dim, speed, data);
        if (newFit < bestFit && newFit != FIT_DATA_TYPE_MAX) {
            bestFit = newFit;
            memcpy(best, current, dim * sizeof(FIT_DATA_TYPE));
        } else {
            current[maxDiffIdx] = oldVal; // 不改进则回退
        }
    }

    free(current);
}

// 新增：从优先级键构造闭合路线（0 -> 客户排序 -> 0）用于打印/调试
// 返回一个长度为 (dim + 1) 的数组，元素为节点索引（以 FIT_DATA_TYPE 存放整数索引以方便打印）
static FIT_DATA_TYPE *buildRouteFromKeys(const FIT_DATA_TYPE *keys, int dim)
{
    if (dim <= 0) return NULL;
    // dim 包含仓库 0，客户为 1..dim-1
    FIT_DATA_TYPE *route = (FIT_DATA_TYPE *)malloc((dim + 1) * sizeof(FIT_DATA_TYPE));
    if (!route) return NULL;
    // 若 keys 为空，释放并返回 NULL
    if (!keys)
    {
        free(route);
        return NULL;
    }
    // 若没有客户，直接 0->0
    if (dim == 1)
    {
        route[0] = 0;
        route[1] = 0;
        return route;
    }
    // 准备排序用的临时数组，仅包含客户
    xData *order = (xData *)malloc((dim - 1) * sizeof(xData));
    if (!order)
    {
        free(route);
        return NULL;
    }
    for (int i = 1, k = 0; i < dim; i++, k++)
    {
        order[k].xIndex = i;
        order[k].data = keys[i];
    }
    sortX(order, dim - 1);

    // 填充闭合路线
    int p = 0;
    route[p++] = 0; // 起点仓库
    for (int i = 0; i < dim - 1; i++)
    {
        route[p++] = (FIT_DATA_TYPE)order[i].xIndex;
    }
    route[p++] = 0; // 终点回仓

    free(order);
    return route;
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
    - 按排序后的适应度顺序重排种群数组指针（原地操作，不释放传入的x）。
    - 使用临时数组暂存重排后的指针，再拷贝回原数组。
*/
FIT_DATA_TYPE **sortIndex(FIT_DATA_TYPE **x, fitnessData *fit, int pop)
{
    FIT_DATA_TYPE **xNew;
    xNew = (FIT_DATA_TYPE **)malloc(pop * sizeof(FIT_DATA_TYPE *));
    for (int i = 0; i < pop; i++)
    {
        xNew[i] = x[fit[i].popIndex];
    }
    // 拷贝回原数组
    for (int i = 0; i < pop; i++)
    {
        x[i] = xNew[i];
    }
    free(xNew); // 仅释放临时数组，不释放x
    return x;   // 返回原指针
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
SMAResult* SMA(int pop, int DIM, const FIT_DATA_TYPE *lb, const FIT_DATA_TYPE *ub, const char *dataPath, int speed)
{
    FIT_DATA_TYPE **x = initialization(pop, DIM);              // pop initialization
    fitnessData *fit;                                     // 种群个体适应度数组
    FIT_DATA_TYPE *bestPositions;                         //(当前)最优解下X取值的数组（优先级键）
    FIT_DATA_TYPE *bestPositionsStart;                     // 最初始最优解下X取值的数组（优先级键）
    FIT_DATA_TYPE destinationFitness = FIT_DATA_TYPE_MAX; // 当前最佳适应度
    FIT_DATA_TYPE *convergenceCurve;                      // 收敛曲线
    FIT_DATA_TYPE **W;                                    // 黏菌权重矩阵
    FIT_DATA_TYPE bestFitness;                            // 当代最佳适应度
    // 将秒级计时改为毫秒级：使用 clock() 记录 CPU 时间
    clock_t startClock, endClock;                         // 计时（CPU clock）
    dataMatrix *data = readMatrix((char*)dataPath);              // 读取数据

    // 添加缺失的变量声明
    double expectedMakespan = -1.0;  // SMA函数不使用早停
    int earlyStopTriggered = 0;

    fit = (fitnessData *)malloc(pop * sizeof(fitnessData));
    convergenceCurve = (FIT_DATA_TYPE *)malloc((T) * sizeof(FIT_DATA_TYPE));
    W = (FIT_DATA_TYPE **)malloc(pop * sizeof(FIT_DATA_TYPE *));
    bestPositions = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    bestPositionsStart = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    for (int i = 0; i < pop; i++)
    {
        W[i] = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    }

    // 以时间窗启发式对前几个个体进行种子初始化，提升可行解概率
    if (data && data->size >= DIM)
    {
        // 统计早窗/晚窗范围（跳过仓库0）
        double minEar = 1e18, maxEar = -1e18;
        double minLat = 1e18, maxLat = data->tw[1].latest;
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
                bestPositionsStart[j] = x[fit[i].popIndex][j];
            }
        }
    }

    int t = 1;
    startClock = clock();

    // 预设打印里程碑：等间隔打印10次最佳fitness（包含初始迭代1与最后迭代T）
    const int milestoneCount = 10;
    int *milestones = (int *)malloc(milestoneCount * sizeof(int));
    milestones[0] = 1;            // 第一次迭代
    milestones[milestoneCount - 1] = T; // 最后一次迭代
    // 中间里程碑均匀分布在 (1, T) 之间
    for (int mi = 1; mi < milestoneCount - 1; ++mi)
    {
        double ratio = (double)mi / (double)(milestoneCount - 1); // 1/(n-1), 2/(n-1), ...
        int val = (int)round(1 + ratio * (T - 1)); // 映射到[1,T]
        if (val <= milestones[mi - 1]) val = milestones[mi - 1] + 1; // 保证严格递增
        if (val > T - 1) val = T - 1; // 避免与最后T冲突
        milestones[mi] = val;
    }
    // 若因为校正出现倒序或越界，再做一次线性拉伸（简单保障）
    for (int mi = 1; mi < milestoneCount; ++mi)
    {
        if (milestones[mi] <= milestones[mi - 1])
        {
            milestones[mi] = milestones[mi - 1] + 1;
            if (milestones[mi] > T) milestones[mi] = T; // 最后可能与T重合，无妨
        }
    }
    int nextMilestoneIndex = 0;

    // 停滞检测与临时探索增强参数
    int lastImprovementIter = 0;                 // 最近一次全局最优提升的迭代号
    int patience = (T >= 50) ? (T / 10) : 5;     // 停滞阈值：默认10% T 或至少5代
    if (patience < 3) patience = 3;
    int boostCounter = 0;                        // 提升探索概率的剩余轮数
    const double zBase = Z;                      // 基础探索概率

    // 【新增】更强力的跳出局部最优机制
    int consecutiveNoImprovement = 0;            // 连续无改进代数
    FIT_DATA_TYPE lastBestFitness = FIT_DATA_TYPE_MAX;  // 上一代的最优适应度
    int superStagnationCounter = 0;              // 超级停滞计数器（fitness几乎不变）
    double mutationIntensity = 0.3;              // 变异强度（动态调整）


    while (t <= T)
    {
        // 对适应度值排序并同步重排个体
        fit = sortFitness(fit, pop);
        x = sortIndex(x, fit, pop);
        // 排序后对齐索引：本代后续均以 i 为该个体的新索引
        for (int i = 0; i < pop; ++i) {
            fit[i].popIndex = i;
        }
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
            // S = (worst - best) + 极小量，保证为正，缩放稳定在[0,1]
            FIT_DATA_TYPE S = (worstFitness - bestFitness) + 1e-8;
            if (S < 1e-12) S = 1e-12; // 额外保护
            // 更新黏菌权重：注意按排序后的 i 写入，避免索引错位
            for (int i = 0; i < pop; i++)
            {
                for (int j = 0, k = 0; j < DIM; j++, k++)
                {
                    if (fit[i].fitness == FIT_DATA_TYPE_MAX)
                    {
                        W[i][j] = 0.0; // 不可行个体不给权重
                        continue;
                    }
                    FIT_DATA_TYPE numer = (fit[i].fitness - bestFitness);
                    if (numer < 0) numer = 0; // 稳健保护
                    FIT_DATA_TYPE frac = numer / S; // ∈[0, +∞)，通常在[0,1]
                    if (i < pop / 2)
                    {
                        W[i][j] = 1 + rand01() * log10(frac + 1.0);
                    }
                    else
                    {
                        W[i][j] = 1 - rand01() * log10(frac + 1.0);
                    }
                }
            }
            // 求 vb、vc 中参数 a, b（注意使用浮点除法），并给 b 下界，避免 b->0 导致解塌缩
            FIT_DATA_TYPE tt = -((FIT_DATA_TYPE)t / (FIT_DATA_TYPE)T) + 1;
            FIT_DATA_TYPE a, b;
            if (tt > -1 && tt < 1)
            {
                a = atanh(tt);
            }
            else
            {
                a = 1; // 极端情况下给一个合理常数
            }
            b = 1 - (FIT_DATA_TYPE)t / (FIT_DATA_TYPE)T;
            if (b < 1e-3) b = 1e-3; // 防止后期过度收缩

            // 当前迭代的探索概率（如触发boost则临时增大）
            double zNow = zBase;
            if (boostCounter > 0)
            {
                double zBoost = zBase * 5.0;
                if (zBoost > 0.3) zBoost = 0.3; // 上限，避免完全随机
                zNow = zBoost;
                boostCounter--;
            }

            // 位置更新：公式2/3，改为"可行解保持"策略
            for (int i = 0; i < pop; i++)
            {
                // 保存更新前的位置（用于回退）
                FIT_DATA_TYPE *xOld = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                for (int j = 0; j < DIM; j++) xOld[j] = x[i][j];
                FIT_DATA_TYPE oldFitness = fit[i].fitness;

                // 公式2：以概率 zNow 进行全局随机探索（提高探索概率）
                double explorationProb = (zNow > 0.1) ? zNow : 0.1; // 最低10%探索
                if (rand01() < explorationProb)
                {
                    for (int j = 0; j < DIM; j++)
                    {
                        FIT_DATA_TYPE newv = (FIT_DATA_TYPE)rand01() * (DIM - 1) + 1e-6 * j;
                        newv += ((FIT_DATA_TYPE)rand01() * 2.0 - 1.0) * 1e-3;
                        if (newv < 0) newv = 0; else if (newv > (DIM - 1)) newv = (DIM - 1);
                        x[i][j] = newv;
                    }
                }
                // 公式3：开发式搜索，但步长更保守
                else
                {
                    FIT_DATA_TYPE p = tanh(fabs(fit[i].fitness - destinationFitness));
                    FIT_DATA_TYPE *vb = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                    FIT_DATA_TYPE *vc = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                    for (int j = 0; j < DIM; j++)
                    {
                        FIT_DATA_TYPE r = rand01();
                        int A, B;
                        if (rand01() < 0.7) {
                            A = rand() % (pop / 2 ? pop / 2 : 1);
                            B = (pop / 2) + (rand() % (pop - (pop / 2) ? (pop - (pop / 2)) : 1));
                        } else {
                            A = rand() % pop;
                            B = rand() % pop;
                        }
                        if (A == B) B = (B + 1) % pop;
                        vb[j] = 2 * a * rand01() - a;
                        vc[j] = 2 * b * rand01() - b;
                        if (r < p)
                        {
                            // 缩小步长到10%，避免过度扰动导致不可行
                            FIT_DATA_TYPE step = vb[j] * (W[i][j] * x[A][j] - x[B][j]) * 0.1;
                            FIT_DATA_TYPE newv = bestPositions[j] + step;
                            newv += ((FIT_DATA_TYPE)rand01() * 2.0 - 1.0) * 1e-3;
                            if (newv < 0) newv = 0; else if (newv > (DIM - 1)) newv = (DIM - 1);
                            x[i][j] = newv;
                        }
                        else
                        {
                            // 更保守的相对扰动：步长限制在5%
                            FIT_DATA_TYPE newv = x[i][j] + vc[j] * x[i][j] * 0.05;
                            newv += ((FIT_DATA_TYPE)rand01() * 2.0 - 1.0) * 1e-3;
                            if (newv < 0) newv = 0; else if (newv > (DIM - 1)) newv = (DIM - 1);
                            x[i][j] = newv;
                        }
                    }
                    free(vb);
                    free(vc);
                }

                // 立即评估新位置的可行性
                FIT_DATA_TYPE newFitness = TSPTW(x[i], DIM, speed, data);
                // 若新位置不可行且旧位置可行，则回退（保持可行解）
                if (newFitness == FIT_DATA_TYPE_MAX && oldFitness != FIT_DATA_TYPE_MAX)
                {
                    for (int j = 0; j < DIM; j++) x[i][j] = xOld[j];
                }
                // 若新旧都不可行，则50%概率注入时间窗启发式
                else if (newFitness == FIT_DATA_TYPE_MAX && oldFitness == FIT_DATA_TYPE_MAX && rand01() < 0.5)
                {
                    if (data && data->size >= DIM)
                    {
                        double minEar = 1e18, maxEar = -1e18;
                        for (int jj = 1; jj < DIM; jj++)
                        {
                            if (data->tw[jj].earliest < minEar) minEar = data->tw[jj].earliest;
                            if (data->tw[jj].earliest > maxEar) maxEar = data->tw[jj].earliest;
                        }
                        x[i][0] = 0;
                        for (int jj = 1; jj < DIM; jj++)
                            x[i][jj] = normalize_range(data->tw[jj].earliest, minEar, maxEar, DIM) + 1e-6 * jj;
                    }
                }

                free(xOld);
            }
        }

        // 重新评估
        for (int i = 0; i < pop; i++)
        {
            fit[i].fitness = TSPTW(x[i], DIM, speed, data);
            if (fit[i].fitness < destinationFitness)
            {
                destinationFitness = fit[i].fitness;
                for (int j = 0; j < DIM; j++)
                {
                    bestPositions[j] = x[i][j];
                }
                lastImprovementIter = t;
            }
        }
        convergenceCurve[t - 1] = destinationFitness;

        // 【方案1】每代对最优解进行轻量级2-opt局部搜索（只做1次尝试，非常快速）
        if (destinationFitness != FIT_DATA_TYPE_MAX)
        {
            FIT_DATA_TYPE oldBest = destinationFitness;
            localSearch2Opt(bestPositions, DIM, speed, data, 1); // 每代只做1次尝试，保持高效
            FIT_DATA_TYPE newBest = TSPTW(bestPositions, DIM, speed, data);
            if (newBest < oldBest && newBest != FIT_DATA_TYPE_MAX)
            {
                destinationFitness = newBest;
                lastImprovementIter = t; // 更新改进记录
            }
        }

        // 【方案5】路径重链接：每50代在最优解和次优解之间搜索
        if (t % 50 == 0 && pop > 1 && destinationFitness != FIT_DATA_TYPE_MAX)
        {
            // 找到第二优秀的可行解
            int secondBestIdx = -1;
            for (int i = 1; i < pop; i++)
            {
                if (fit[i].fitness != FIT_DATA_TYPE_MAX)
                {
                    secondBestIdx = i;
                    break;
                }
            }

            if (secondBestIdx >= 0)
            {
                FIT_DATA_TYPE *tempBest = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                memcpy(tempBest, bestPositions, DIM * sizeof(FIT_DATA_TYPE));

                pathRelinking(bestPositions, x[secondBestIdx], tempBest, DIM, speed, data);

                FIT_DATA_TYPE newFit = TSPTW(tempBest, DIM, speed, data);
                if (newFit < destinationFitness && newFit != FIT_DATA_TYPE_MAX)
                {
                    destinationFitness = newFit;
                    memcpy(bestPositions, tempBest, DIM * sizeof(FIT_DATA_TYPE));
                    lastImprovementIter = t;
                    // printf("Path relinking improved at iter %d: %.6f\n", t, destinationFitness);
                }

                free(tempBest);
            }
        }

        // 【新增】方案二：多样性增强和局部搜索
        // 1. 定期计算种群多样性并自适应调整变异强度
        if (t % 20 == 0) // 每20代检查一次多样性
        {
            double diversity = calculateDiversity(x, pop, DIM);
            // 如果多样性过低（<0.3），增强扰动
            if (diversity < 0.3 && boostCounter == 0)
            {
                boostCounter = 30; // 激活30轮增强探索
                // printf("Low diversity %.3f detected at iter %d, boosting exploration\n", diversity, t);
            }
        }

        // 2. 定期对最优解进行局部搜索（2-opt）- 已被方案1的每代搜索替代，此处改为深度搜索
        if (t % 100 == 0 && destinationFitness != FIT_DATA_TYPE_MAX)
        {
            // 每100代进行一次深度局部搜索（多次尝试）
            FIT_DATA_TYPE oldBest = destinationFitness;
            localSearch2Opt(bestPositions, DIM, speed, data, 5); // 深度搜索，5次尝试
            FIT_DATA_TYPE newBest = TSPTW(bestPositions, DIM, speed, data);
            if (newBest < oldBest)
            {
                destinationFitness = newBest;
                lastImprovementIter = t;
                // printf("Deep local search improved at iter %d: %.6f -> %.6f\n", t, oldBest, newBest);
            }
        }

        // 3. 对前5%精英个体应用局部搜索
        if (t % 30 == 0)
        {
            int eliteCount = pop / 20; // 5%
            if (eliteCount < 1) eliteCount = 1;
            if (eliteCount > 3) eliteCount = 3; // 最多3个

            for (int i = 0; i < eliteCount; i++)
            {
                if (fit[i].fitness != FIT_DATA_TYPE_MAX)
                {
                    FIT_DATA_TYPE oldFit = fit[i].fitness;
                    // 交替使用2-opt和Or-opt
                    if (t % 60 == 0)
                    {
                        localSearch2Opt(x[i], DIM, speed, data, 2);
                    }
                    else
                    {
                        localSearchOrOpt(x[i], DIM, speed, data, 2);
                    }
                    fit[i].fitness = TSPTW(x[i], DIM, speed, data);

                    // 如果改进了且比当前最优更好，更新全局最优
                    if (fit[i].fitness < destinationFitness)
                    {
                        destinationFitness = fit[i].fitness;
                        for (int j = 0; j < DIM; j++)
                        {
                            bestPositions[j] = x[i][j];
                        }
                        lastImprovementIter = t;
                    }
                }
            }
        }

        // 【新增】强力跳出局部最优机制
        // 检测停滞情况
        if (lastBestFitness != FIT_DATA_TYPE_MAX && destinationFitness != FIT_DATA_TYPE_MAX)
        {
            double improvement = (lastBestFitness - destinationFitness) / (lastBestFitness + 1e-9);

            if (improvement < 1e-6) // 改进幅度小于0.0001%
            {
                consecutiveNoImprovement++;
                superStagnationCounter++;
            }
            else
            {
                consecutiveNoImprovement = 0;
                superStagnationCounter = 0;
                mutationIntensity = 0.3; // 重置变异强度
            }
        }
        lastBestFitness = destinationFitness;

        // 4. 【反向学习】每60代对部分个体应用反向学习
        if (t % 60 == 0 && destinationFitness != FIT_DATA_TYPE_MAX)
        {
            int oppCount = pop / 5; // 20%个体
            if (oppCount < 2) oppCount = 2;

            for (int i = 0; i < oppCount; i++)
            {
                int targetIdx = pop / 2 + rand() % (pop / 2); // 后50%个体
                FIT_DATA_TYPE *oppKeys = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));

                // 基于最优解生成反向解
                generateOppositionSolution(bestPositions, oppKeys, DIM);

                FIT_DATA_TYPE oppFit = TSPTW(oppKeys, DIM, speed, data);
                if (oppFit < fit[targetIdx].fitness)
                {
                    // 反向解更好，接受
                    for (int j = 0; j < DIM; j++)
                    {
                        x[targetIdx][j] = oppKeys[j];
                    }
                    fit[targetIdx].fitness = oppFit;

                    // 如果比全局最优还好，更新
                    if (oppFit < destinationFitness)
                    {
                        destinationFitness = oppFit;
                        for (int j = 0; j < DIM; j++)
                        {
                            bestPositions[j] = oppKeys[j];
                        }
                        lastImprovementIter = t;
                    }
                }

                free(oppKeys);
            }
        }

        // 5. 【自适应大变异】检测到严重停滞时应用
        if (superStagnationCounter >= 40) // 40代几乎无改进
        {
            // 动态增加变异强度
            mutationIntensity += 0.1;
            if (mutationIntensity > 0.8) mutationIntensity = 0.8;

            // 对中下层个体（30%-80%）进行大变异
            int mutStart = pop * 3 / 10;
            int mutEnd = pop * 8 / 10;
            for (int i = mutStart; i < mutEnd; i++)
            {
                adaptiveLargeMutation(x[i], DIM, mutationIntensity, data);
                fit[i].fitness = TSPTW(x[i], DIM, speed, data);

                if (fit[i].fitness < destinationFitness && fit[i].fitness != FIT_DATA_TYPE_MAX)
                {
                    destinationFitness = fit[i].fitness;
                    for (int j = 0; j < DIM; j++)
                    {
                        bestPositions[j] = x[i][j];
                    }
                    lastImprovementIter = t;
                }
            }

            superStagnationCounter = 0; // 重置计数器
            boostCounter = 40; // 激活增强探索
        }

        // 6. 【分层重启】超长停滞时使用更激进的分层重启
        if (consecutiveNoImprovement >= 50) // 50代完全无改进
        {
            layeredRestart(x, pop, DIM, data);

            // 重新评估所有个体
            for (int i = 0; i < pop; i++)
            {
                fit[i].fitness = TSPTW(x[i], DIM, speed, data);
                if (fit[i].fitness < destinationFitness && fit[i].fitness != FIT_DATA_TYPE_MAX)
                {
                    destinationFitness = fit[i].fitness;
                    for (int j = 0; j < DIM; j++)
                    {
                        bestPositions[j] = x[i][j];
                    }
                    lastImprovementIter = t;
                }
            }

            consecutiveNoImprovement = 0;
            mutationIntensity = 0.3; // 重置变异强度
            boostCounter = 60; // 激活长期增强探索
        }

        if (t - lastImprovementIter >= patience)
        {
            int startIdx = (int)(pop * 0.7);
            if (startIdx < 1) startIdx = 1;
            for (int i = startIdx; i < pop; ++i)
            {
                for (int j = 0; j < DIM; ++j)
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
                for (int i = startIdx; i < pop; i += 2)
                {
                    x[i][0] = 0;
                    for (int j = 1; j < DIM; j++)
                        x[i][j] = normalize_range(data->tw[j].earliest, minEar, maxEar, DIM) + 1e-6 * j;
                }
            }
            boostCounter = 50;
            lastImprovementIter = t;
        }

        int perturbStart = (int)(pop * 0.6);
        if (perturbStart < 1) perturbStart = 1;
        for (int i = perturbStart; i < pop; ++i)
        {
            if (rand01() < 0.2)
            {
                smallKeySwaps(x[i], DIM, 2);
            }
        }

        t += 1;
    }

    endClock = clock();
    double elapsed_ms = (double)(endClock - startClock) * 1000.0 / (double)CLOCKS_PER_SEC;

    FIT_DATA_TYPE finalActualDistance = 0.0;
    {
        xData *order = (xData *)malloc((DIM - 1) * sizeof(xData));
        int k = 0;
        for (int i = 1; i < DIM; i++)
        {
            order[k].xIndex = i;
            order[k].data = bestPositions[i];
            k++;
        }
        order = sortX(order, DIM - 1);

        // 计算总距离
        // 仓库 -> 第一个客户
        if (DIM > 1)
        {
            int first = order[0].xIndex;
            finalActualDistance += data->dist[0][first];
        }
        // 客户之间
        for (int i = 0; i < DIM - 2; i++)
        {
            int from = order[i].xIndex;
            int to = order[i + 1].xIndex;
            finalActualDistance += data->dist[from][to];
        }
        // 最后一个客户 -> 仓库
        if (DIM > 1)
        {
            int last = order[DIM - 2].xIndex;
            finalActualDistance += data->dist[last][0];
        }

        free(order);
    }

    // 新增：计算最优路线的 makespan（基于最终解码的路线，而非重新排序）
    FIT_DATA_TYPE finalMakespan = 0.0;
    FIT_DATA_TYPE *route = buildRouteFromKeys(bestPositions, DIM);
    if (route && data != NULL && DIM > 0)
    {
        FIT_DATA_TYPE currentTime = 0.0;
        FIT_DATA_TYPE speedVal = (speed <= 0) ? 1.0 : (FIT_DATA_TYPE)speed;

        // 仓库出发：等待到仓库最早时间
        FIT_DATA_TYPE startTime = data->tw[0].earliest;
        if (currentTime < startTime) currentTime = startTime;

        // 按路线逐段累加：route[0]=0, route[1..DIM-1]=客户, route[DIM]=0
        for (int i = 0; i < DIM; i++)
        {
            int from = (int)route[i];
            int to = (int)route[i + 1];
            FIT_DATA_TYPE distance = data->dist[from][to];
            FIT_DATA_TYPE travelTime = distance / speedVal;
            currentTime += travelTime;

            // 到达后检查时间窗（最后回仓库不检查等待）
            if (i < DIM - 1) // 不是最后一段（返回仓库）
            {
                startTime = data->tw[to].earliest;
                if (currentTime < startTime) currentTime = startTime;
            }
        }

        finalMakespan = currentTime;
    }

    // NOTE: 不再原地把 bestPositions（优先级键）覆盖为索引，保留 keys 以便外部使用.
    // 使用 buildRouteFromKeys 构造闭合路线用于打印（0 开始、0 结束）
    printf("Iteration time: %d.\n", T);
    printf("Done, the best fitness is %lf, time %.3f ms.\n", destinationFitness, elapsed_ms);
    printf("Final actual distance (without penalty): %.2f\n", finalActualDistance);
    printf("Final makespan (total time including waiting, return to depot): %.2f\n", finalMakespan);
    if (route)
    {
        printf("Route (0->...->0): [");
        for (int i = 0; i < DIM; i++)
        {
            // route 长度为 DIM+1，前 DIM 元素为 0.. last customer
            printf("%d, ", (int)route[i]);
        }
        printf("%d]\n", (int)route[DIM]);
        free(route);
    }
    // 依然保留并打印原始 bestPositions（优先级键）以便分析
    printf("Best x set: [");
    for (int i = 0; i < DIM - 1; i++)
    {
        printf("%lf, ", bestPositions[i]);
    }
    printf("%lf]\n", bestPositions[DIM - 1]);

    // 资源释放与结果封装
    for (int i = 0; i < pop; i++)
    {
        free(x[i]);
        free(W[i]);
    }
    free(milestones); // 释放动态分配的里程碑数组
    SMAResult *result = (SMAResult *)malloc(sizeof(SMAResult));
    result->pop = pop;
    result->dimension = DIM;
    result->iterationATime = T;
    result->destinationFitness = destinationFitness;
    result->bestPositions = bestPositions;
    result->bestPositionsStart = bestPositionsStart;
    result->convergenceCurve = convergenceCurve;
    result->finalDistance = finalActualDistance;    // 存储最终距离
    result->finalMakespan = finalMakespan;          // 存储最终makespan
    result->elapsedTimeMs = elapsed_ms;
    result->earlyStopTriggered = 0;                 // 未提前停止
    free(x);
    free(fit);
    freeDataMatrix(data);
    free(W);
    return result;
}

/*
    SMA_TimeLimited_Internal：基于时间限制的SMA算法内部实现
    - 参数与原SMA相同，但增加 timeLimitSeconds 参数指定运行时间上限（秒）
    - expectedMakespan：预期makespan值，若 > 0 则启用早停（达到该值时停止），否则运行至时间限制
    - 内部会预估迭代次数上限，动态分配 convergenceCurve
*/
static SMAResult* SMA_TimeLimited_Internal(int pop, int DIM, const FIT_DATA_TYPE *lb, const FIT_DATA_TYPE *ub, const char *dataPath, int speed, double timeLimitSeconds, double expectedMakespan)
{
    FIT_DATA_TYPE **x = initialization(pop, DIM);
    fitnessData *fit;
    FIT_DATA_TYPE *bestPositions;
    FIT_DATA_TYPE *bestPositionsStart;
    FIT_DATA_TYPE destinationFitness = FIT_DATA_TYPE_MAX;
    FIT_DATA_TYPE **W;
    FIT_DATA_TYPE bestFitness;
    clock_t startClock, endClock;
    dataMatrix *data = readMatrix((char*)dataPath);

    int earlyStopTriggered = 0;

    fit = (fitnessData *)malloc(pop * sizeof(fitnessData));
    // 预分配一个较大的收敛曲线数组（预估最大迭代次数）
    int maxIterations = 100000; // 足够大的上限
    FIT_DATA_TYPE *convergenceCurve = (FIT_DATA_TYPE *)malloc(maxIterations * sizeof(FIT_DATA_TYPE));

    W = (FIT_DATA_TYPE **)malloc(pop * sizeof(FIT_DATA_TYPE *));
    bestPositions = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    bestPositionsStart = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    for (int i = 0; i < pop; i++)
    {
        W[i] = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    }

    // 时间窗启发式初始化（与原SMA相同）
    if (data && data->size >= DIM)
    {
        double minEar = 1e18, maxEar = -1e18;
        double minLat = 1e18, maxLat = data->tw[1].latest;
        for (int j = 1; j < DIM; j++)
        {
            if (data->tw[j].earliest < minEar) minEar = data->tw[j].earliest;
            if (data->tw[j].earliest > maxEar) maxEar = data->tw[j].earliest;
            if (data->tw[j].latest < minLat) minLat = data->tw[j].latest;
            if (data->tw[j].latest > maxLat) maxLat = data->tw[j].latest;
        }
        for (int j = 0; j < DIM; j++) x[0][j] = 0;
        for (int j = 1; j < DIM; j++)
            x[0][j] = normalize_range(data->tw[j].earliest, minEar, maxEar, DIM) + 1e-6 * j;
        if (pop > 1)
        {
            for (int j = 0; j < DIM; j++) x[1][j] = 0;
            for (int j = 1; j < DIM; j++)
                x[1][j] = normalize_range(data->tw[j].latest, minLat, maxLat, DIM) + 1e-6 * j;
        }
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
                bestPositionsStart[j] = x[fit[i].popIndex][j];
            }
        }
    }

    int t = 1;
    startClock = clock();
    double timeLimitClocks = timeLimitSeconds * CLOCKS_PER_SEC;

    int lastImprovementIter = 0;
    int patience = 100; // 基于时间的情况下，用固定值
    int boostCounter = 0;
    const double zBase = Z;

    // 【新增】更强力的跳出局部最优机制
    int consecutiveNoImprovement = 0;            // 连续无改进代数
    FIT_DATA_TYPE lastBestFitness = FIT_DATA_TYPE_MAX;  // 上一代的最优适应度
    int superStagnationCounter = 0;              // 超级停滞计数器（fitness几乎不变）
    double mutationIntensity = 0.3;              // 变异强度（动态调整）

    // 主循环：基于时间限制而非固定迭代次数
    while (1)
    {
        // 检查是否超时
        clock_t currentClock = clock();
        if ((currentClock - startClock) >= timeLimitClocks)
        {
            break; // 达到时间限制，退出
        }

        // 检查是否超出预分配的迭代次数
        if (t > maxIterations)
        {
            // 扩展收敛曲线数组
            maxIterations *= 2;
            convergenceCurve = (FIT_DATA_TYPE *)realloc(convergenceCurve, maxIterations * sizeof(FIT_DATA_TYPE));
        }

        // 排序并更新
        fit = sortFitness(fit, pop);
        x = sortIndex(x, fit, pop);
        for (int i = 0; i < pop; ++i) {
            fit[i].popIndex = i;
        }
        bestFitness = fit[0].fitness;
        FIT_DATA_TYPE worstFitness = fit[pop - 1].fitness;

        int skipUpdate = 0;
        if (bestFitness == FIT_DATA_TYPE_MAX)
        {
            // 全体不可行，重采样
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
            skipUpdate = 1;
        }

        if (!skipUpdate)
        {
            FIT_DATA_TYPE S = (worstFitness - bestFitness) + 1e-8;
            if (S < 1e-12) S = 1e-12;

            for (int i = 0; i < pop; i++)
            {
                for (int j = 0; j < DIM; j++)
                {
                    if (fit[i].fitness == FIT_DATA_TYPE_MAX)
                    {
                        W[i][j] = 0.0;
                        continue;
                    }
                    FIT_DATA_TYPE numer = (fit[i].fitness - bestFitness);
                    if (numer < 0) numer = 0;
                    FIT_DATA_TYPE frac = numer / S;
                    if (i < pop / 2)
                    {
                        W[i][j] = 1 + rand01() * log10(frac + 1.0);
                    }
                    else
                    {
                        W[i][j] = 1 - rand01() * log10(frac + 1.0);
                    }
                }
            }

            // 使用基于时间进度的参数（估算总迭代数）
            double timeProgress = (double)(currentClock - startClock) / timeLimitClocks;
            FIT_DATA_TYPE tt = -(FIT_DATA_TYPE)timeProgress + 1;
            FIT_DATA_TYPE a, b;
            if (tt > -1 && tt < 1)
            {
                a = atanh(tt);
            }
            else
            {
                a = 1;
            }
            b = 1 - (FIT_DATA_TYPE)timeProgress;
            if (b < 1e-3) b = 1e-3;

            double zNow = zBase;
            if (boostCounter > 0)
            {
                double zBoost = zBase * 5.0;
                if (zBoost > 0.3) zBoost = 0.3;
                zNow = zBoost;
                boostCounter--;
            }

            for (int i = 0; i < pop; i++)
            {
                FIT_DATA_TYPE *xOld = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                for (int j = 0; j < DIM; j++) xOld[j] = x[i][j];
                FIT_DATA_TYPE oldFitness = fit[i].fitness;

                double explorationProb = (zNow > 0.1) ? zNow : 0.1;
                if (rand01() < explorationProb)
                {
                    for (int j = 0; j < DIM; j++)
                    {
                        FIT_DATA_TYPE newv = (FIT_DATA_TYPE)rand01() * (DIM - 1) + 1e-6 * j;
                        newv += ((FIT_DATA_TYPE)rand01() * 2.0 - 1.0) * 1e-3;
                        if (newv < 0) newv = 0; else if (newv > (DIM - 1)) newv = (DIM - 1);
                        x[i][j] = newv;
                    }
                }
                else
                {
                    FIT_DATA_TYPE p = tanh(fabs(fit[i].fitness - destinationFitness));
                    FIT_DATA_TYPE *vb = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                    FIT_DATA_TYPE *vc = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                    for (int j = 0; j < DIM; j++)
                    {
                        FIT_DATA_TYPE r = rand01();
                        int A, B;
                        if (rand01() < 0.7) {
                            A = rand() % (pop / 2 ? pop / 2 : 1);
                            B = (pop / 2) + (rand() % (pop - (pop / 2) ? (pop - (pop / 2)) : 1));
                        } else {
                            A = rand() % pop;
                            B = rand() % pop;
                        }
                        if (A == B) B = (B + 1) % pop;
                        vb[j] = 2 * a * rand01() - a;
                        vc[j] = 2 * b * rand01() - b;
                        if (r < p)
                        {
                            FIT_DATA_TYPE step = vb[j] * (W[i][j] * x[A][j] - x[B][j]) * 0.1;
                            FIT_DATA_TYPE newv = bestPositions[j] + step;
                            newv += ((FIT_DATA_TYPE)rand01() * 2.0 - 1.0) * 1e-3;
                            if (newv < 0) newv = 0; else if (newv > (DIM - 1)) newv = (DIM - 1);
                            x[i][j] = newv;
                        }
                        else
                        {
                            FIT_DATA_TYPE newv = x[i][j] + vc[j] * x[i][j] * 0.05;
                            newv += ((FIT_DATA_TYPE)rand01() * 2.0 - 1.0) * 1e-3;
                            if (newv < 0) newv = 0; else if (newv > (DIM - 1)) newv = (DIM - 1);
                            x[i][j] = newv;
                        }
                    }
                    free(vb);
                    free(vc);
                }

                // 立即评估新位置的可行性
                FIT_DATA_TYPE newFitness = TSPTW(x[i], DIM, speed, data);
                // 若新位置不可行且旧位置可行，则回退（保持可行解）
                if (newFitness == FIT_DATA_TYPE_MAX && oldFitness != FIT_DATA_TYPE_MAX)
                {
                    for (int j = 0; j < DIM; j++) x[i][j] = xOld[j];
                }
                // 若新旧都不可行，则50%概率注入时间窗启发式
                else if (newFitness == FIT_DATA_TYPE_MAX && oldFitness == FIT_DATA_TYPE_MAX && rand01() < 0.5)
                {
                    if (data && data->size >= DIM)
                    {
                        double minEar = 1e18, maxEar = -1e18;
                        for (int jj = 1; jj < DIM; jj++)
                        {
                            if (data->tw[jj].earliest < minEar) minEar = data->tw[jj].earliest;
                            if (data->tw[jj].earliest > maxEar) maxEar = data->tw[jj].earliest;
                        }
                        x[i][0] = 0;
                        for (int jj = 1; jj < DIM; jj++)
                            x[i][jj] = normalize_range(data->tw[jj].earliest, minEar, maxEar, DIM) + 1e-6 * jj;
                    }
                }

                free(xOld);
            }
        }

        // 重新评估
        for (int i = 0; i < pop; i++)
        {
            fit[i].fitness = TSPTW(x[i], DIM, speed, data);
            if (fit[i].fitness < destinationFitness)
            {
                destinationFitness = fit[i].fitness;
                for (int j = 0; j < DIM; j++)
                {
                    bestPositions[j] = x[i][j];
                }
                lastImprovementIter = t;
            }
        }
        convergenceCurve[t - 1] = destinationFitness;

        // 【方案1】每代对最优解进行轻量级2-opt局部搜索（只做1次尝试，非常快速）
        if (destinationFitness != FIT_DATA_TYPE_MAX)
        {
            FIT_DATA_TYPE oldBest = destinationFitness;
            localSearch2Opt(bestPositions, DIM, speed, data, 1); // 每代只做1次尝试，保持高效
            FIT_DATA_TYPE newBest = TSPTW(bestPositions, DIM, speed, data);
            if (newBest < oldBest && newBest != FIT_DATA_TYPE_MAX)
            {
                destinationFitness = newBest;
                lastImprovementIter = t; // 更新改进记录
            }
        }

        // 【方案5】路径重链接：每50代在最优解和次优解之间搜索
        if (t % 50 == 0 && pop > 1 && destinationFitness != FIT_DATA_TYPE_MAX)
        {
            // 找到第二优秀的可行解
            int secondBestIdx = -1;
            for (int i = 1; i < pop; i++)
            {
                if (fit[i].fitness != FIT_DATA_TYPE_MAX)
                {
                    secondBestIdx = i;
                    break;
                }
            }

            if (secondBestIdx >= 0)
            {
                FIT_DATA_TYPE *tempBest = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                memcpy(tempBest, bestPositions, DIM * sizeof(FIT_DATA_TYPE));

                pathRelinking(bestPositions, x[secondBestIdx], tempBest, DIM, speed, data);

                FIT_DATA_TYPE newFit = TSPTW(tempBest, DIM, speed, data);
                if (newFit < destinationFitness && newFit != FIT_DATA_TYPE_MAX)
                {
                    destinationFitness = newFit;
                    memcpy(bestPositions, tempBest, DIM * sizeof(FIT_DATA_TYPE));
                    lastImprovementIter = t;
                    // printf("Path relinking improved at iter %d: %.6f\n", t, destinationFitness);
                }

                free(tempBest);
            }
        }

        // 【新增】方案二：多样性增强和局部搜索
        // 1. 定期计算种群多样性并自适应调整变异强度
        if (t % 20 == 0) // 每20代检查一次多样性
        {
            double diversity = calculateDiversity(x, pop, DIM);
            // 如果多样性过低（<0.3），增强扰动
            if (diversity < 0.3 && boostCounter == 0)
            {
                boostCounter = 30; // 激活30轮增强探索
                // printf("Low diversity %.3f detected at iter %d, boosting exploration\n", diversity, t);
            }
        }

        // 2. 定期对最优解进行局部搜索（2-opt）- 已被方案1的每代搜索替代，此处改为深度搜索
        if (t % 100 == 0 && destinationFitness != FIT_DATA_TYPE_MAX)
        {
            // 每100代进行一次深度局部搜索（多次尝试）
            FIT_DATA_TYPE oldBest = destinationFitness;
            localSearch2Opt(bestPositions, DIM, speed, data, 5); // 深度搜索，5次尝试
            FIT_DATA_TYPE newBest = TSPTW(bestPositions, DIM, speed, data);
            if (newBest < oldBest)
            {
                destinationFitness = newBest;
                lastImprovementIter = t;
                // printf("Deep local search improved at iter %d: %.6f -> %.6f\n", t, oldBest, newBest);
            }
        }

        // 3. 对前5%精英个体应用局部搜索
        if (t % 30 == 0)
        {
            int eliteCount = pop / 20; // 5%
            if (eliteCount < 1) eliteCount = 1;
            if (eliteCount > 3) eliteCount = 3; // 最多3个

            for (int i = 0; i < eliteCount; i++)
            {
                if (fit[i].fitness != FIT_DATA_TYPE_MAX)
                {
                    FIT_DATA_TYPE oldFit = fit[i].fitness;
                    // 交替使用2-opt和Or-opt
                    if (t % 60 == 0)
                    {
                        localSearch2Opt(x[i], DIM, speed, data, 2);
                    }
                    else
                    {
                        localSearchOrOpt(x[i], DIM, speed, data, 2);
                    }
                    fit[i].fitness = TSPTW(x[i], DIM, speed, data);

                    // 如果改进了且比当前最优更好，更新全局最优
                    if (fit[i].fitness < destinationFitness)
                    {
                        destinationFitness = fit[i].fitness;
                        for (int j = 0; j < DIM; j++)
                        {
                            bestPositions[j] = x[i][j];
                        }
                        lastImprovementIter = t;
                    }
                }
            }
        }

        // 【新增】强力跳出局部最优机制
        // 检测停滞情况
        if (lastBestFitness != FIT_DATA_TYPE_MAX && destinationFitness != FIT_DATA_TYPE_MAX)
        {
            double improvement = (lastBestFitness - destinationFitness) / (lastBestFitness + 1e-9);

            if (improvement < 1e-6) // 改进幅度小于0.0001%
            {
                consecutiveNoImprovement++;
                superStagnationCounter++;
            }
            else
            {
                consecutiveNoImprovement = 0;
                superStagnationCounter = 0;
                mutationIntensity = 0.3; // 重置变异强度
            }
        }
        lastBestFitness = destinationFitness;

        // 4. 【反向学习】每60代对部分个体应用反向学习
        if (t % 60 == 0 && destinationFitness != FIT_DATA_TYPE_MAX)
        {
            int oppCount = pop / 5; // 20%个体
            if (oppCount < 2) oppCount = 2;

            for (int i = 0; i < oppCount; i++)
            {
                int targetIdx = pop / 2 + rand() % (pop / 2); // 后50%个体
                FIT_DATA_TYPE *oppKeys = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));

                // 基于最优解生成反向解
                generateOppositionSolution(bestPositions, oppKeys, DIM);

                FIT_DATA_TYPE oppFit = TSPTW(oppKeys, DIM, speed, data);
                if (oppFit < fit[targetIdx].fitness)
                {
                    // 反向解更好，接受
                    for (int j = 0; j < DIM; j++)
                    {
                        x[targetIdx][j] = oppKeys[j];
                    }
                    fit[targetIdx].fitness = oppFit;

                    // 如果比全局最优还好，更新
                    if (oppFit < destinationFitness)
                    {
                        destinationFitness = oppFit;
                        for (int j = 0; j < DIM; j++)
                        {
                            bestPositions[j] = oppKeys[j];
                        }
                        lastImprovementIter = t;
                    }
                }

                free(oppKeys);
            }
        }

        // 5. 【自适应大变异】检测到严重停滞时应用
        if (superStagnationCounter >= 40) // 40代几乎无改进
        {
            // 动态增加变异强度
            mutationIntensity += 0.1;
            if (mutationIntensity > 0.8) mutationIntensity = 0.8;

            // 对中下层个体（30%-80%）进行大变异
            int mutStart = pop * 3 / 10;
            int mutEnd = pop * 8 / 10;
            for (int i = mutStart; i < mutEnd; i++)
            {
                adaptiveLargeMutation(x[i], DIM, mutationIntensity, data);
                fit[i].fitness = TSPTW(x[i], DIM, speed, data);

                if (fit[i].fitness < destinationFitness && fit[i].fitness != FIT_DATA_TYPE_MAX)
                {
                    destinationFitness = fit[i].fitness;
                    for (int j = 0; j < DIM; j++)
                    {
                        bestPositions[j] = x[i][j];
                    }
                    lastImprovementIter = t;
                }
            }

            superStagnationCounter = 0; // 重置计数器
            boostCounter = 40; // 激活增强探索
        }

        // 6. 【分层重启】超长停滞时使用更激进的分层重启
        if (consecutiveNoImprovement >= 50) // 50代完全无改进
        {
            layeredRestart(x, pop, DIM, data);

            // 重新评估所有个体
            for (int i = 0; i < pop; i++)
            {
                fit[i].fitness = TSPTW(x[i], DIM, speed, data);
                if (fit[i].fitness < destinationFitness && fit[i].fitness != FIT_DATA_TYPE_MAX)
                {
                    destinationFitness = fit[i].fitness;
                    for (int j = 0; j < DIM; j++)
                    {
                        bestPositions[j] = x[i][j];
                    }
                    lastImprovementIter = t;
                }
            }

            consecutiveNoImprovement = 0;
            mutationIntensity = 0.3; // 重置变异强度
            boostCounter = 60; // 激活长期增强探索
        }

        if (t - lastImprovementIter >= patience)
        {
            int startIdx = (int)(pop * 0.7);
            if (startIdx < 1) startIdx = 1;
            for (int i = startIdx; i < pop; ++i)
            {
                for (int j = 0; j < DIM; ++j)
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
                for (int i = startIdx; i < pop; i += 2)
                {
                    x[i][0] = 0;
                    for (int j = 1; j < DIM; j++)
                        x[i][j] = normalize_range(data->tw[j].earliest, minEar, maxEar, DIM) + 1e-6 * j;
                }
            }
            boostCounter = 50;
            lastImprovementIter = t;
        }

        int perturbStart = (int)(pop * 0.6);
        if (perturbStart < 1) perturbStart = 1;
        for (int i = perturbStart; i < pop; ++i)
        {
            if (rand01() < 0.2)
            {
                smallKeySwaps(x[i], DIM, 2);
            }
        }

        t += 1;
    }

    endClock = clock();
    double elapsed_ms = (double)(endClock - startClock) * 1000.0 / (double)CLOCKS_PER_SEC;

    FIT_DATA_TYPE finalActualDistance = 0.0;
    {
        xData *order = (xData *)malloc((DIM - 1) * sizeof(xData));
        int k = 0;
        for (int i = 1; i < DIM; i++)
        {
            order[k].xIndex = i;
            order[k].data = bestPositions[i];
            k++;
        }
        order = sortX(order, DIM - 1);

        if (DIM > 1)
        {
            int first = order[0].xIndex;
            finalActualDistance += data->dist[0][first];
        }
        for (int i = 0; i < DIM - 2; i++)
        {
            int from = order[i].xIndex;
            int to = order[i + 1].xIndex;
            finalActualDistance += data->dist[from][to];
        }
        if (DIM > 1)
        {
            int last = order[DIM - 2].xIndex;
            finalActualDistance += data->dist[last][0];
        }

        free(order);
    }

    // 计算makespan
    FIT_DATA_TYPE finalMakespan = 0.0;
    FIT_DATA_TYPE *route = buildRouteFromKeys(bestPositions, DIM);
    if (route && data != NULL && DIM > 0)
    {
        FIT_DATA_TYPE currentTime = 0.0;
        FIT_DATA_TYPE speedVal = (speed <= 0) ? 1.0 : (FIT_DATA_TYPE)speed;
        FIT_DATA_TYPE startTime = data->tw[0].earliest;
        if (currentTime < startTime) currentTime = startTime;

        for (int i = 0; i < DIM; i++)
        {
            int from = (int)route[i];
            int to = (int)route[i + 1];
            FIT_DATA_TYPE distance = data->dist[from][to];
            FIT_DATA_TYPE travelTime = distance / speedVal;
            currentTime += travelTime;

            if (i < DIM - 1)
            {
                startTime = data->tw[to].earliest;
                if (currentTime < startTime) currentTime = startTime;
            }
        }

        finalMakespan = currentTime;
    }

    if (earlyStopTriggered)
    {
        printf("Early stopped after %d iterations (%.2f seconds allowed)\n", t - 1, timeLimitSeconds);
    }
    else
    {
        printf("Time-limited run: %.2f seconds, %d iterations.\n", timeLimitSeconds, t - 1);
    }
    printf("Best fitness: %lf, elapsed time: %.3f ms.\n", destinationFitness, elapsed_ms);
    printf("Final actual distance: %.2f\n", finalActualDistance);
    printf("Final makespan: %.2f\n", finalMakespan);
    if (route)
    {
        printf("Route (0->...->0): [");
        for (int i = 0; i < DIM; i++)
        {
            printf("%d, ", (int)route[i]);
        }
        printf("%d]\n", (int)route[DIM]);
        free(route);
    }

    // 清理和返回结果
    for (int i = 0; i < pop; i++)
    {
        free(x[i]);
        free(W[i]);
    }

    convergenceCurve = (FIT_DATA_TYPE *)realloc(convergenceCurve, t * sizeof(FIT_DATA_TYPE));

    SMAResult *result = (SMAResult *)malloc(sizeof(SMAResult));
    result->pop = pop;
    result->dimension = DIM;
    result->iterationATime = t - 1;
    result->destinationFitness = destinationFitness;
    result->bestPositions = bestPositions;
    result->bestPositionsStart = bestPositionsStart;
    result->convergenceCurve = convergenceCurve;
    result->finalDistance = finalActualDistance;
    result->finalMakespan = finalMakespan;
    result->elapsedTimeMs = elapsed_ms;
    result->earlyStopTriggered = earlyStopTriggered;

    free(x);
    free(fit);
    freeDataMatrix(data);
    free(W);
    return result;
}

// 新增：反向学习策略（Opposition-Based Learning）
// 在搜索空间中生成当前解的对立解，有助于跳出局部最优
static void generateOppositionSolution(const FIT_DATA_TYPE *keys, FIT_DATA_TYPE *oppKeys, int dim)
{
    if (!keys || !oppKeys || dim <= 1) return;

    // 仓库位置保持不变
    oppKeys[0] = keys[0];

    // 对客户键生成对立解：opp(x) = max + min - x
    // 在优先级编码中，键的范围大致在 [0, dim-1]
    double minVal = 0.0;
    double maxVal = (double)(dim - 1);

    for (int j = 1; j < dim; j++)
    {
        oppKeys[j] = (FIT_DATA_TYPE)(maxVal + minVal - keys[j]);
        // 确保在有效范围内
        if (oppKeys[j] < minVal) oppKeys[j] = minVal;
        if (oppKeys[j] > maxVal) oppKeys[j] = maxVal;
        // 添加小扰动保持唯一性
        oppKeys[j] += 1e-6 * j;
    }
}

// 新增：自适应大变异（Adaptive Large Mutation）
// 当长期停滞时，对个体进行大幅度变异以跳出局部最优
static void adaptiveLargeMutation(FIT_DATA_TYPE *keys, int dim, double intensity, dataMatrix *data)
{
    if (!keys || !data || dim <= 1) return;

    // 保存仓库位置
    FIT_DATA_TYPE depotKey = keys[0];

    // 根据强度决定变异的客户数量
    int mutateCount = (int)(intensity * (dim - 1));
    if (mutateCount < 2) mutateCount = 2;
    if (mutateCount > dim - 1) mutateCount = dim - 1;

    // 随机选择要变异的客户
    for (int m = 0; m < mutateCount; m++)
    {
        int idx = 1 + rand() % (dim - 1);

        // 50%概率：基于时间窗的智能变异
        // 50%概率：完全随机变异
        if (rand01() < 0.5 && data->size >= dim)
        {
            // 智能变异：根据时间窗重新设置优先级
            double earliest = data->tw[idx].earliest;
            double latest = data->tw[idx].latest;
            double range = latest - earliest;
            if (range > 0)
            {
                // 在时间窗范围内随机选择一个位置
                double ratio = rand01();
                keys[idx] = (FIT_DATA_TYPE)(ratio * (dim - 1));
            }
            else
            {
                keys[idx] = (FIT_DATA_TYPE)(rand01() * (dim - 1));
            }
        }
        else
        {
            // 完全随机变异
            keys[idx] = (FIT_DATA_TYPE)(rand01() * (dim - 1));
        }

        keys[idx] += 1e-6 * idx; // 保持唯一性
    }

    keys[0] = depotKey; // 恢复仓库位置
}

// 新增：分层重启策略（Layered Restart）
// 不只重启底部个体，而是分成多层进行不同强度的重启
static void layeredRestart(FIT_DATA_TYPE **x, int pop, int dim, dataMatrix *data)
{
    if (!x || pop <= 3 || dim <= 1) return;

    // 保留前10%精英（向下取整）
    int eliteCount = pop / 10;
    if (eliteCount < 1) eliteCount = 1;

    // 第一层：10%-40% 轻度扰动（保持部分结构）
    int layer1Start = eliteCount;
    int layer1End = pop * 2 / 5;
    for (int i = layer1Start; i < layer1End && i < pop; i++)
    {
        // 对30%的维度进行小幅度随机化
        for (int j = 1; j < dim; j++)
        {
            if (rand01() < 0.3)
            {
                x[i][j] = (FIT_DATA_TYPE)(rand01() * (dim - 1) + 1e-6 * j);
            }
        }
    }

    // 第二层：40%-70% 中度重启（基于时间窗启发式 + 随机）
    int layer2Start = layer1End;
    int layer2End = pop * 7 / 10;
    if (data && data->size >= dim)
    {
        double minEar = 1e18, maxEar = -1e18;
        for (int j = 1; j < dim; j++)
        {
            if (data->tw[j].earliest < minEar) minEar = data->tw[j].earliest;
            if (data->tw[j].earliest > maxEar) maxEar = data->tw[j].earliest;
        }

        for (int i = layer2Start; i < layer2End && i < pop; i++)
        {
            if (rand01() < 0.5)
            {
                // 时间窗启发式
                x[i][0] = 0;
                for (int j = 1; j < dim; j++)
                {
                    x[i][j] = normalize_range(data->tw[j].earliest, minEar, maxEar, dim) + 1e-6 * j;
                }
            }
            else
            {
                // 完全随机
                for (int j = 0; j < dim; j++)
                {
                    x[i][j] = (FIT_DATA_TYPE)(rand01() * (dim - 1) + 1e-6 * j);
                }
            }
        }
    }
    else
    {
        for (int i = layer2Start; i < layer2End && i < pop; i++)
        {
            for (int j = 0; j < dim; j++)
            {
                x[i][j] = (FIT_DATA_TYPE)(rand01() * (dim - 1) + 1e-6 * j);
            }
        }
    }

    // 第三层：70%-100% 完全重启 + 反向学习
    int layer3Start = layer2End;
    for (int i = layer3Start; i < pop; i++)
    {
        if (rand01() < 0.4 && i > 0)
        {
            // 40%概率：使用反向学习（基于某个精英解的对立解）
            int eliteIdx = rand() % eliteCount;
            generateOppositionSolution(x[eliteIdx], x[i], dim);
        }
        else
        {
            // 60%概率：完全随机
            for (int j = 0; j < dim; j++)
            {
                x[i][j] = (FIT_DATA_TYPE)(rand01() * (dim - 1) + 1e-6 * j);
            }
        }
    }
}

/*
    SMA_TimeLimited：
    - 基于时间限制的SMA算法实现，允许在达到时间上限时提前停止。
    - 其他参数与原SMA相同。
*/
SMAResult* SMA_TimeLimited(int pop, int DIM, const FIT_DATA_TYPE *lb, const FIT_DATA_TYPE *ub, const char *dataPath, int speed, double timeLimitSeconds)
{
    return SMA_TimeLimited_Internal(pop, DIM, lb, ub, dataPath, speed, timeLimitSeconds, -1.0);
}
