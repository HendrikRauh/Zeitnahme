#include <timeLogic.h>

unsigned long lastTime = 0;

unsigned long getLastTime()
{
    return lastTime;
}

void calcLastTime(unsigned long startTime, unsigned long endTime)
{
    lastTime = endTime - startTime;
}
