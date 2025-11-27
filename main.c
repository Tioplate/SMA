#include "myjni.h"
#include <stdio.h>
#include <stdlib.h>

// 保持种群规模与速度常量
#define POP 50
#define SPEED 2

// 读取数据文件第一行的维度（节点数）
static int readDimFromDataset(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        perror("打开数据文件失败");
        return -1;
    }
    int n = -1;
    if (fscanf(fp, "%d", &n) != 1)
    {
        fprintf(stderr, "读取维度失败\n");
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return n;
}

int main()
{
    const char *dataPath = "../dataset/SolomonPotvinBengio/rc_202.1.txt"; // 可切换为其他数据集
    int DIM = readDimFromDataset(dataPath);
    if (DIM <= 0)
    {
        fprintf(stderr, "数据集中第一行维度非法: %d\n", DIM);
        return 1;
    }
    printf("从数据集读取到维度 DIM = %d\n", DIM);

    // 动态分配上下界
    FIT_DATA_TYPE *lb = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    FIT_DATA_TYPE *ub = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    if (!lb || !ub)
    {
        fprintf(stderr, "分配lb/ub内存失败\n");
        free(lb); free(ub);
        return 1;
    }
    for (int i = 0; i < DIM; i++)
    {
        lb[i] = 0;
        ub[i] = DIM; // 仍使用 DIM 作为范围上界（键值生成用）
    }

    SMAResult *result = SMA(POP, DIM, lb, ub, dataPath, SPEED);

    // 可在此根据需要输出 result->bestPositions 等（当前 SMA 已内部打印）

    // 释放动态上下界（SMAResult 由调用者决定何时释放）
    free(lb);
    free(ub);
    return 0;
}
