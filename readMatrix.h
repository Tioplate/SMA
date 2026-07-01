#pragma once
typedef struct
{
    double earliest, latest;
} TimeWindow;
typedef struct
{
    int size;
    double **dist;
    TimeWindow *tw;
    int **R;   // 先后顺序约束矩阵：R[i][j]=1 表示 i 必须在 j 之前访问
    int **E;   // Valid edge matrix: E[i][j]=1 means edge (i->j) is feasible


#define MAX_NODES 512 // 假设最大节点数
#define LINE_LEN 4096 // 假设最大行长度
} dataMatrix;

dataMatrix *readMatrix(char *path);
void freeDataMatrix(dataMatrix *data);