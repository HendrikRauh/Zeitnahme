
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

uint8_t matrixGetChar(char c, uint8_t bufSize, uint8_t *buffer);
uint8_t matrixGetCharWithWidth(char c, uint8_t preferredWidth, uint8_t bufSize, uint8_t *buffer);

void matrixSetBrightness(uint8_t brightness);
void initMatrix();
void matrixWipeAnimation();

void matrixShowString(const char *text);
void matrixShowTime(unsigned long time);

void writeCharToMatrix(char c, uint8_t preferredWidth, uint8_t &col, bool addSpace = true);
void writeStringToMatrix(const char *str, uint8_t preferredWidth, uint8_t &col, bool addSpaceBetweenChars = true);
void clearRemainingColumns(uint8_t startCol);
void displaySecondsAndMilliseconds(unsigned long time, uint8_t &col);
void displayMinutesSecondsHundredths(unsigned long time, uint8_t &col);
void displayMinutesSecondsTenths(unsigned long time, uint8_t &col);

#endif
