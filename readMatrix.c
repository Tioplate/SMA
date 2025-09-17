#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "readMatrix.h"

dataMatrix* readMatrix(char* path)
{
    FILE *fp = fopen(path, "r");
    dataMatrix *data = malloc(sizeof(dataMatrix));
    if (!fp)
    {
        perror("文件打开失败");
        return NULL;
    }

    int n;
    char line[LINE_LEN];

    // 1. 读取节点数量
    if (fgets(line, LINE_LEN, fp) == NULL)
    {
        printf("读取节点数量失败\n");
        fclose(fp);
        return NULL;
    }
    sscanf(line, "%d", &n);
    printf("节点数: %d\n", n);

    // 2. 读取距离矩阵
    double **dist = malloc(n * sizeof(double *));
    for (int i = 0; i < n; ++i)
        dist[i] = malloc(n * sizeof(double));

    // 按行读取距离矩阵
    for (int i = 0; i < n; ++i)
    {
        if (fgets(line, LINE_LEN, fp) == NULL)
        {
            printf("读取距离矩阵第%d行失败\n", i);
            fclose(fp);
            return NULL;
        }
        char *ptr = line;
        for (int j = 0; j < n; ++j)
        {
            while (*ptr == ' ' || *ptr == '\t')
                ++ptr; // 跳过空格
            if (*ptr == '\0' || *ptr == '\n')
                break;
            sscanf(ptr, "%lf", &dist[i][j]);
            // 跳到下一个数字
            while (*ptr != ' ' && *ptr != '\t' && *ptr != '\0' && *ptr != '\n')
                ++ptr;
        }
    }

    // 3. 读取时间窗
    TimeWindow *tw = malloc(n * sizeof(TimeWindow));
    for (int i = 0; i < n; ++i)
    {
        if (fgets(line, LINE_LEN, fp) == NULL)
        {
            printf("读取时间窗第%d行失败\n", i);
            fclose(fp);
            return NULL;
        }
        // 跳过注释行
        if (line[0] == '#')
        {
            --i; // 不算作一个时间窗
            continue;
        }
        sscanf(line, "%lf %lf", &tw[i].earliest, &tw[i].latest);
    }

    fclose(fp);

    // 4. 示例输出部分数据
    printf("\n距离矩阵部分（前5x5）：\n");
    for (int i = 0; i < (n < 5 ? n : 5); ++i)
    {
        for (int j = 0; j < (n < 5 ? n : 5); ++j)
            printf("%8.2f ", dist[i][j]);
        printf("\n");
    }

    printf("\n时间窗部分（前5个节点）：\n");
    for (int i = 0; i < (n < 5 ? n : 5); ++i)
        printf("节点%2d: [%6.2f, %6.2f]\n", i, tw[i].earliest, tw[i].latest);
    data->size = n;
    data->dist = dist;
    data->tw = tw;
    return data;
}

void freeDataMatrix(dataMatrix* data)
{
    int n = data->size;
    double **dist = data->dist;
    TimeWindow *tw = data->tw;
    // 释放内存
    for (int i = 0; i < n; ++i)
        free(dist[i]);
    free(dist);
    free(tw);
    free(data);
    return;
}