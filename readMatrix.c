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
        perror("File opening failed");
        return NULL;
    }

    int n;
    char line[LINE_LEN];

    // 1. Read the number of nodes
    if (fgets(line, LINE_LEN, fp) == NULL)
    {
        printf("Failed to read the number of nodes\n");
        fclose(fp);
        return NULL;
    }
    sscanf(line, "%d", &n);
    printf("Number of nodes: %d\n", n);

    // 2. Read the distance matrix
    double **dist = malloc(n * sizeof(double *));
    for (int i = 0; i < n; ++i)
        dist[i] = malloc(n * sizeof(double));

    // Read the distance matrix line by line
    for (int i = 0; i < n; ++i)
    {
        if (fgets(line, LINE_LEN, fp) == NULL)
        {
            printf("Failed to read line %d of the distance matrix\n", i);
            fclose(fp);
            return NULL;
        }
        char *ptr = line;
        for (int j = 0; j < n; ++j)
        {
            while (*ptr == ' ' || *ptr == '\t')
                ++ptr; // Skip spaces
            if (*ptr == '\0' || *ptr == '\n')
                break;
            sscanf(ptr, "%lf", &dist[i][j]);
            // Move to the next number
            while (*ptr != ' ' && *ptr != '\t' && *ptr != '\0' && *ptr != '\n')
                ++ptr;
        }
    }

    // 3. Read the time windows
    TimeWindow *tw = malloc(n * sizeof(TimeWindow));
    for (int i = 0; i < n; ++i)
    {
        if (fgets(line, LINE_LEN, fp) == NULL)
        {
            printf("Failed to read line %d of the time windows\n", i);
            fclose(fp);
            return NULL;
        }
        // Skip comment lines
        if (line[0] == '#')
        {
            --i; // Do not count as a time window
            continue;
        }
        sscanf(line, "%lf %lf", &tw[i].earliest, &tw[i].latest);
    }

    // Initialize precedence matrix R (all zeros, filled by filterNetwork)
    int **R = malloc(n * sizeof(int *));
    for (int i = 0; i < n; i++)
        R[i] = calloc(n, sizeof(int));

    // Initialize valid edge matrix E (all ones = all edges initially valid)
    int **E = malloc(n * sizeof(int *));
    for (int i = 0; i < n; i++)
    {
        E[i] = malloc(n * sizeof(int));
        for (int j = 0; j < n; j++)
            E[i][j] = (i != j) ? 1 : 0;
    }

    printf("Preprocessing: distance matrix and time windows loaded, awaiting filterNetwork pruning\n");

    // Output sample data
    printf("\nDistance matrix (first 5x5):\n");
    for (int i = 0; i < (n < 5 ? n : 5); ++i)
    {
        for (int j = 0; j < (n < 5 ? n : 5); ++j)
            printf("%8.2f ", dist[i][j]);
        printf("\n");
    }

    printf("\nTime windows (first 5 nodes):\n");
    for (int i = 0; i < (n < 5 ? n : 5); ++i)
        printf("Node %2d: [%6.2f, %6.2f]\n", i, tw[i].earliest, tw[i].latest);

    data->size = n;
    data->dist = dist;
    data->tw = tw;
    data->R = R;
    data->E = E;

    fclose(fp);
    return data;
}

void freeDataMatrix(dataMatrix *data)
{
    if (!data) return;
    int n = data->size;
    if (data->dist) { for (int i = 0; i < n; i++) free(data->dist[i]); free(data->dist); }
    if (data->tw) free(data->tw);
    if (data->R) { for (int i = 0; i < n; i++) free(data->R[i]); free(data->R); }
    if (data->E) { for (int i = 0; i < n; i++) free(data->E[i]); free(data->E); }
    free(data);
}