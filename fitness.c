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

/*
    排序比较函数：按 data 升序
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
    sortX：对 xData 数组按 data 升序排序
*/
xData *sortX(xData *order, int dim)
{
    qsort(order, dim, sizeof(xData), compareFunctionX);
    return order;
}

/*
    adjustPostion：示例函数
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
        free(order);
    }
    return x;
}

/*
    比较函数：按 earliest 升序
*/
typedef struct {
    int customerIdx;
    double earliest;
} CustomerTW;

static int cmpCustomerTW(const void *a, const void *b)
{
    double da = ((const CustomerTW *)a)->earliest;
    double db = ((const CustomerTW *)b)->earliest;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

/*
    贪心修复函数：按时间窗最早时间重新排序客户
*/
void repairSolutionGreedy(FIT_DATA_TYPE *x, int dim, dataMatrix *routeData, FIT_DATA_TYPE speed)
{
    if (!routeData || dim <= 1) return;

    CustomerTW *customers = (CustomerTW *)malloc((dim - 1) * sizeof(CustomerTW));

    for (int i = 1; i < dim; i++)
    {
        customers[i - 1].customerIdx = i;
        customers[i - 1].earliest = routeData->tw[i].earliest;
    }

    qsort(customers, dim - 1, sizeof(CustomerTW), cmpCustomerTW);

    x[0] = 0;
    for (int i = 0; i < dim - 1; i++)
    {
        int custIdx = customers[i].customerIdx;
        x[custIdx] = (FIT_DATA_TYPE)i + 1e-6 * i;
    }

    free(customers);
}

/*
    TSPTW 带修复版本
*/
FIT_DATA_TYPE TSPTW_WithRepair(FIT_DATA_TYPE *x, int dim, FIT_DATA_TYPE speed, dataMatrix *routeData)
{
    FIT_DATA_TYPE fitness = TSPTW(x, dim, speed, routeData);
    if (fitness > 10000.0 && routeData != NULL)
    {
        repairSolutionGreedy(x, dim, routeData, speed);
        fitness = TSPTW(x, dim, speed, routeData);
    }
    return fitness;
}

/*
    TSPTW（软时间窗惩罚）
*/
FIT_DATA_TYPE TSPTW(FIT_DATA_TYPE *x, int dim, FIT_DATA_TYPE speed, dataMatrix *routeData)
{
    if (dim <= 1)
        return 0;

    int customerCount = dim - 1;

    xData *order = (xData *)malloc(customerCount * sizeof(xData));

    // 填充排序数组
    for (int i = 1; i < dim; i++)
    {
        order[i - 1].xIndex = i;
        order[i - 1].data   = x[i];
    }
    sortX(order, customerCount);

    FIT_DATA_TYPE currentTime   = 0.0;
    FIT_DATA_TYPE totalDistance = 0.0;
    FIT_DATA_TYPE totalOvertime = 0.0;
    FIT_DATA_TYPE startTime, endTime, travelTime, distance;

    // 仓库出发等待逻辑
    startTime = routeData->tw[0].earliest;
    endTime   = routeData->tw[0].latest;
    if (currentTime < startTime) currentTime = startTime;
    if (currentTime > endTime)   totalOvertime += (currentTime - endTime);

    // 仓库 -> 第一位客户
    {
        int first = order[0].xIndex;
        distance    = routeData->dist[0][first];
        travelTime  = distance / speed;
        currentTime += travelTime;
        totalDistance += distance;
        startTime = routeData->tw[first].earliest;
        endTime   = routeData->tw[first].latest;
        if (currentTime > endTime)   totalOvertime += (currentTime - endTime);
        if (currentTime < startTime) currentTime = startTime;
    }

    // 客户之间
    for (int i = 0; i < customerCount - 1; i++)
    {
        int from = order[i].xIndex;
        int to   = order[i + 1].xIndex;
        distance    = routeData->dist[from][to];
        travelTime  = distance / speed;
        startTime = routeData->tw[to].earliest;
        endTime   = routeData->tw[to].latest;
        currentTime  += travelTime;
        totalDistance += distance;
        if (currentTime > endTime)   totalOvertime += (currentTime - endTime);
        if (currentTime < startTime) currentTime = startTime;
    }

    // 最后一个客户 -> 仓库
    {
        int last = order[customerCount - 1].xIndex;
        distance    = routeData->dist[last][0];
        travelTime  = distance / speed;
        currentTime  += travelTime;
        totalDistance += distance;
        endTime = routeData->tw[0].latest;
        if (currentTime > endTime) totalOvertime += (currentTime - endTime);
    }

    free(order);

    return currentTime + 100.0 * totalOvertime;
}
