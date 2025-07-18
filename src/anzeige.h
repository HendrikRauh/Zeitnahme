
#ifndef ANZEIGE_H
#define ANZEIGE_H

#include <MD_MAX72xx.h>
#include <deviceInfo.h>
#include <Utility.h>

#define DIN_PIN 14
#define CS_PIN 27
#define CLK_PIN 26

#define MAX_DEVICES 4
#define BRIGHTNESS 1

extern MD_MAX72XX mx;

void matrixSetBrigtness(uint8_t brightness);
void initMatrix();
void matrixWipeAnimation();
void matrixShowString(const char *text);
void matrixShowTime(unsigned long time);

#endif // ANZEIGE_H
