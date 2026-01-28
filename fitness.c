#include "fitness.h"

#include <string.h>

/*
    模块：适应度计算（TSP/TSPTW）
    - 本文件实现了示例函数 F1，以及带硬时间窗的 TSPTW 适应度。
    - 位置编码采用“优先级编码”：对每个维度的实数键进行排序来确定访问顺序。
    - 在 TSPTW 中：
        * 将 0 号节点视为仓库（固定起点与终点）；
        * 仅对客户节点（1..dim-1）按键值排序决定访问顺序；
        * 时间窗为硬约束：
            - 到达早于 earliest 可等待；
            - 到达晚于 latest 直接判不可行（返回 FIT_DATA_TYPE_MAX）；
        * 目标函数为“总行驶距离”（不包含等待时间），返仓距离计入目标但通常不检查返仓窗口；
        * speed 用于把距离转换为行驶时间以做时间窗可行性判断。
*/

/*
    F1 function: 示例适应度，fitness == sum(x^2)
    仅用于测试与对照。
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

// 保留的 TSP/TSPTW 旧版本示例（注释掉），用于参考不同目标的实现方式
// ...existing code...

/*
    TSPTW（软时间窗惩罚）
    输入：
      - x: 个体的优先级键数组（长度 dim），其中 x[0] 对应仓库键，但不参与排序；
      - dim: 维度/节点总数（包含仓库）；
      - speed: 行驶速度，用于将距离转化为时间；
      - routeData: 路网数据（距离矩阵 dist 与时间窗 tw）。
    输出：
      - 适应度 = 总行驶距离 + 100 * (所有超时节点的超时时间之和)
    细节：
      - 仅客户(1..dim-1)参与排序，仓库固定为起终点；
      - 早到等待，迟到累计超时惩罚；
      - 返仓也检查时间窗，如有超时也计入惩罚。
*/
FIT_DATA_TYPE TSPTW(FIT_DATA_TYPE *x, int dim, FIT_DATA_TYPE speed, dataMatrix *routeData)
{
    if (dim <= 1)
        return 0;

    // 构造客户排序数组，仅包含 1..dim-1 节点
    xData *order = (xData *)malloc((dim - 1) * sizeof(xData));
    int k = 0;
    for (int i = 1; i < dim; i++)
    {
        order[k].xIndex = i;  // 保存客户原始索引
        order[k].data = x[i]; // 使用个体在第 i 维的键值作为排序键
        k++;
    }
    order = sortX(order, dim - 1); // 升序排序，得到访问顺序

    // 时间推进与距离累计变量
    FIT_DATA_TYPE currentTime = 0.0;     // 当前到达时间
    FIT_DATA_TYPE startTime;             // 目标点的最早时间窗
    FIT_DATA_TYPE endTime;               // 目标点的最晚时间窗
    FIT_DATA_TYPE travelTime;            // 路段行驶时间 = 距离 / 速度
    FIT_DATA_TYPE distance;              // 路段距离
    FIT_DATA_TYPE totalDistance = 0.0;   // 总行驶距离
    FIT_DATA_TYPE totalOvertime = 0.0;   // 所有超时节点的超时时间之和

    // 仓库（0）最早时间出发：若当前时间更早则先等待到 earliest
    startTime = routeData->tw[0].earliest;
    endTime = routeData->tw[0].latest;
    if (currentTime < startTime)
        currentTime = startTime;
    // 仓库出发也可能超时，计入惩罚
    if (currentTime > endTime)
    {
        totalOvertime += (currentTime - endTime);
    }

    // 1) 仓库 -> 第一位客户（若存在客户）
    if (dim > 1)
    {
        int first = order[0].xIndex;
        distance = routeData->dist[0][first];
        travelTime = distance / speed;
        currentTime += travelTime;
        totalDistance += distance;
        startTime = routeData->tw[first].earliest;
        endTime = routeData->tw[first].latest;
        // 超过最晚时间窗，累计超时
        if (currentTime > endTime)
        {
            totalOvertime += (currentTime - endTime);
        }
        // 早于最早窗，等待
        if (currentTime < startTime)
            currentTime = startTime;
    }

    // 2) 客户之间行驶并检查时间窗
    for (int i = 0; i < dim - 2; i++)
    {
        int from = order[i].xIndex;
        int to = order[i + 1].xIndex;
        distance = routeData->dist[from][to];
        travelTime = distance / speed;
        startTime = routeData->tw[to].earliest;
        endTime = routeData->tw[to].latest;
        currentTime += travelTime;
        totalDistance += distance;
        // 超时则累计惩罚
        if (currentTime > endTime)
        {
            totalOvertime += (currentTime - endTime);
        }
        // 等待到最早窗
        if (currentTime < startTime)
            currentTime = startTime;
    }

    // 3) 最后一个客户 -> 仓库：返航距离计入目标，也检查仓库返回时间窗
    if (dim > 1)
    {
        int last = order[dim - 2].xIndex;
        distance = routeData->dist[last][0];
        travelTime = distance / speed;
        currentTime += travelTime;
        totalDistance += distance;
        // 检查返回仓库的时间窗
        endTime = routeData->tw[0].latest;
        if (currentTime > endTime)
        {
            totalOvertime += (currentTime - endTime);
        }
    }

    free(order);

    // 修改：返回makespan（完工时间）而不是距离
    // makespan = currentTime（返回仓库的时间）
    // 如果有超时，加上较大的惩罚使其不可行
    FIT_DATA_TYPE fitness;

    if (totalOvertime < 1e-6) {
        // 完全可行解：返回makespan（完工时间）
        fitness = currentTime;
    } else {
        // 有超时：返回一个很大的惩罚值表示不可行
        // 使用currentTime + 大惩罚，这样仍然能区分不同程度的超时
        fitness = currentTime + 10.0 * totalOvertime;
    }

    return fitness;
}

/*
    贪心修复函数：按时间窗最早时间重新排序客户
    用于修复严重违反时间窗约束的解
*/
void repairSolutionGreedy(FIT_DATA_TYPE *x, int dim, dataMatrix *routeData, FIT_DATA_TYPE speed)
{
    if (!routeData || dim <= 1) return;

    // 创建客户-时间窗映射
    typedef struct {
        int customerIdx;
        FIT_DATA_TYPE earliest;
    } CustomerTW;

    CustomerTW *customers = (CustomerTW *)malloc((dim - 1) * sizeof(CustomerTW));

    // 收集所有客户及其最早时间窗
    for (int i = 1; i < dim; i++)
    {
        customers[i - 1].customerIdx = i;
        customers[i - 1].earliest = routeData->tw[i].earliest;
    }

    // 按最早时间窗排序（贪心策略）
    for (int i = 0; i < dim - 2; i++)
    {
        for (int j = i + 1; j < dim - 1; j++)
        {
            if (customers[j].earliest < customers[i].earliest)
            {
                CustomerTW temp = customers[i];
                customers[i] = customers[j];
                customers[j] = temp;
            }
        }
    }

    // 重新编码为优先级键
    x[0] = 0; // 仓库
    for (int i = 0; i < dim - 1; i++)
    {
        int custIdx = customers[i].customerIdx;
        x[custIdx] = (FIT_DATA_TYPE)i + 1e-6 * i;
    }

    free(customers);
}

/*
    TSPTW 带修复版本：如果解严重不可行，先修复再评估
*/
FIT_DATA_TYPE TSPTW_WithRepair(FIT_DATA_TYPE *x, int dim, FIT_DATA_TYPE speed, dataMatrix *routeData)
{
    // 先评估一次
    FIT_DATA_TYPE fitness = TSPTW(x, dim, speed, routeData);

    // 如果严重不可行（fitness > 10000 说明有很大惩罚），尝试修复
    if (fitness > 10000.0 && routeData != NULL)
    {
        repairSolutionGreedy(x, dim, routeData, speed);
        fitness = TSPTW(x, dim, speed, routeData);
    }

    return fitness;
}

/*
    排序比较函数：按 data 升序，支持相等键返回 0，保证 qsort 稳定性需求
*/
int compareFunctionX(const void *a, const void *b)
{
    FIT_DATA_TYPE da = ((const xData *)a)->data;
    FIT_DATA_TYPE db = ((const xData *)b)->data;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

/*
    sortX：对 xData 数组按 data 升序排序（原地），返回原指针
*/
xData *sortX(xData *order, int dim)
{
    qsort(order, dim, sizeof(order[0]), compareFunctionX);
    return order;
}

/*
    adjustPostion：示例函数，演示对一个体的各维按键值排序（不修改原个体 x）
    注意：当前仅分配临时 order 并释放，不改变 x 的实际内容。
*/
FIT_DATA_TYPE **adjustPostion(FIT_DATA_TYPE **x, int pop, int dim)
{
    for (int i = 0; i < pop; i++)
    {
        xData *order = (xData *)malloc(dim * sizeof(xData));
        for (int j = 0; j < dim; j++)
        {
            order[j].xIndex = j;
            order[j].data = x[i][j];
        }
        order = sortX(order, dim);
        // 这里只做示例，不回写 x[i][j]；真实应用里可在此回写排序后的索引或顺序
        free(order);
    }
    return x;
}
