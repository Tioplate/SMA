#include "sma.h"

int main()
{
    FIT_DATA_TYPE lb[DIM];
    FIT_DATA_TYPE ub[DIM];
    for (int i = 0; i < DIM; i++)
    {
        lb[i] = 0;
        ub[i] = DIM;
    }
    SMA(30, lb, ub);
    return 0;
}
