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
} dataMatrix;


#define MAX_NODES 512 // 假设最大节点数
#define LINE_LEN 4096 // 假设最大行长度

dataMatrix *readMatrix(char *path);
void freeDataMatrix(dataMatrix *data);