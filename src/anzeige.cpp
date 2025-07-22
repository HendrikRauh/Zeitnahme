#include <anzeige.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Forward declaration for getBrightness from data.h
int getBrightness();

MD_MAX72XX mx = MD_MAX72XX(MD_MAX72XX::DR1CR0RR1_HW, DIN_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

struct CustomChar
{
    char ch;
    uint8_t width;
    uint8_t bitmap[8];
};

const uint8_t CHAR_BUFFER_SIZE = 8;
const uint8_t TIME_STRING_BUFFER_SIZE = 6;
const uint8_t MINUTES_STRING_SIZE = 3;
const uint8_t SECONDS_STRING_SIZE = 3;
const uint8_t HUNDREDTHS_STRING_SIZE = 3;

const uint8_t WIPE_DELAY = 30;
const uint8_t END_DELAY = 200;

const uint8_t MAX_BRIGHTNESS = 15;
const uint8_t MIN_BRIGHTNESS = 1;

namespace TimeFormatWidths
{
    namespace SecondsMillis
    {
        const uint8_t DIGITS = 5;
        const uint8_t DECIMAL_POINT = 2;
    }

    namespace MinutesSecondsHundredths
    {
        const uint8_t MINUTE_DIGIT = 4;
        const uint8_t COLON = 1;
        const uint8_t SECOND_DIGITS = 5;
        const uint8_t DECIMAL_POINT = 1;
        const uint8_t HUNDREDTH_DIGITS = 5;
    }

    namespace MinutesSecondsTenths
    {
        const uint8_t MINUTE_DIGITS = 4;
        const uint8_t COLON = 2;
        const uint8_t SECOND_DIGITS = 5;
        const uint8_t DECIMAL_POINT = 2;
        const uint8_t TENTH_DIGIT = 4;
    }
}

const CustomChar customChars[] = {
    {'0', 4, {0b01111110, 0b10000001, 0b10000001, 0b01111110}},
    {'0', 5, {0b01111110, 0b10000001, 0b10000001, 0b10000001, 0b01111110}},

    {'1', 4, {0b10000100, 0b10000010, 0b11111111, 0b10000000}},
    {'1', 5, {0b10000100, 0b10000010, 0b11111111, 0b10000000, 0b10000000}},

    {'2', 4, {0b11000110, 0b10100001, 0b10010001, 0b10001110}},
    {'2', 5, {0b11000110, 0b10100001, 0b10010001, 0b10001001, 0b10000110}},

    {'3', 4, {0b01000010, 0b10001001, 0b10001001, 0b01110110}},
    {'3', 5, {0b01000010, 0b10001001, 0b10001001, 0b10001001, 0b01110110}},

    {'4', 4, {0b00111000, 0b00100100, 0b00100010, 0b11111111}},
    {'4', 5, {0b00111000, 0b00100100, 0b00100010, 0b00100001, 0b11111111}},

    {'5', 4, {0b01011111, 0b10001001, 0b10001001, 0b01110001}},
    {'5', 5, {0b01011111, 0b10001001, 0b10001001, 0b10001001, 0b01110001}},

    {'6', 4, {0b01111110, 0b10001001, 0b10001001, 0b01110010}},
    {'6', 5, {0b01111110, 0b10001001, 0b10001001, 0b10001001, 0b01110010}},

    {'7', 4, {0b00000001, 0b11110001, 0b00001001, 0b00000111}},
    {'7', 5, {0b00000001, 0b11110001, 0b00001001, 0b00000101, 0b00000011}},

    {'8', 4, {0b01110110, 0b10001001, 0b10001001, 0b01110110}},
    {'8', 5, {0b01110110, 0b10001001, 0b10001001, 0b10001001, 0b01110110}},

    {'9', 4, {0b01001110, 0b10010001, 0b10010001, 0b01111110}},
    {'9', 5, {0b01001110, 0b10010001, 0b10010001, 0b10010001, 0b01111110}},

    {'.', 2, {0b11000000, 0b11000000}},
    {'.', 1, {0b10000000}},

    {',', 2, {0b01000000, 0b11000000}},
    {',', 1, {0b11000000}},

    {':', 2, {0b01100110, 0b01100110}},
    {':', 1, {0b00100100}},
};

uint8_t matrixGetChar(char c, uint8_t bufSize, uint8_t *buffer)
{
    for (unsigned int i = 0; i < sizeof(customChars) / sizeof(CustomChar); i++)
    {
        if (customChars[i].ch == c)
        {
            uint8_t w = customChars[i].width;
            if (w > bufSize)
                w = bufSize;
            for (uint8_t k = 0; k < w; k++)
                buffer[k] = customChars[i].bitmap[k];
            return w;
        }
    }
    return mx.getChar(c, bufSize, buffer);
}

void writeCharToMatrix(char c, uint8_t preferredWidth, uint8_t &col, bool addSpace)
{
    uint8_t buffer[CHAR_BUFFER_SIZE];
    uint8_t charWidth = matrixGetCharWithWidth(c, preferredWidth, sizeof(buffer), buffer);

    for (uint8_t k = 0; k < charWidth; k++)
    {
        if (col < mx.getColumnCount())
            mx.setColumn(col++, buffer[k]);
    }

    if (addSpace && col < mx.getColumnCount())
        mx.setColumn(col++, 0x00);
}

void writeStringToMatrix(const char *str, uint8_t preferredWidth, uint8_t &col, bool addSpaceBetweenChars)
{
    uint8_t len = strlen(str);
    for (uint8_t i = 0; i < len; i++)
    {
        bool shouldAddSpace = addSpaceBetweenChars && (i < len - 1);
        writeCharToMatrix(str[i], preferredWidth, col, shouldAddSpace);
    }
}

void clearRemainingColumns(uint8_t startCol)
{
    while (startCol < mx.getColumnCount())
    {
        mx.setColumn(startCol++, 0x00);
    }
}

void displaySecondsAndMilliseconds(unsigned long time, uint8_t &col)
{
    unsigned long seconds = time / 1000;
    unsigned long milliseconds = time % 1000;

    char timeStr[TIME_STRING_BUFFER_SIZE];
    sprintf(timeStr, "%02lu.%03lu", seconds, milliseconds);

    for (uint8_t i = 0; timeStr[i] != '\0'; i++)
    {
        uint8_t width = (timeStr[i] == '.') ? TimeFormatWidths::SecondsMillis::DECIMAL_POINT
                                            : TimeFormatWidths::SecondsMillis::DIGITS;
        bool addSpace = (timeStr[i + 1] != '\0');
        writeCharToMatrix(timeStr[i], width, col, addSpace);
    }
}

void displayMinutesSecondsHundredths(unsigned long time, uint8_t &col)
{
    unsigned long minutes = time / 60000;
    unsigned long seconds = (time % 60000) / 1000;
    unsigned long hundredths = (time % 1000) / 10;

    char timeStr[10];
    sprintf(timeStr, "%lu:%02lu.%02lu", minutes, seconds, hundredths);

    for (uint8_t i = 0; timeStr[i] != '\0'; i++)
    {
        uint8_t width;
        if (timeStr[i] == ':')
            width = TimeFormatWidths::MinutesSecondsHundredths::COLON;
        else if (timeStr[i] == '.')
            width = TimeFormatWidths::MinutesSecondsHundredths::DECIMAL_POINT;
        else if (i == 0)
            width = TimeFormatWidths::MinutesSecondsHundredths::MINUTE_DIGIT;
        else if (i >= 2 && i <= 3)
            width = TimeFormatWidths::MinutesSecondsHundredths::SECOND_DIGITS;
        else
            width = TimeFormatWidths::MinutesSecondsHundredths::HUNDREDTH_DIGITS;

        bool addSpace = (timeStr[i + 1] != '\0');
        writeCharToMatrix(timeStr[i], width, col, addSpace);
    }
}

void displayMinutesSecondsTenths(unsigned long time, uint8_t &col)
{
    unsigned long minutes = time / 60000;
    unsigned long seconds = (time % 60000) / 1000;
    unsigned long tenths = (time % 1000) / 100;

    char timeStr[10];
    sprintf(timeStr, "%02lu:%02lu.%lu", minutes, seconds, tenths);

    for (uint8_t i = 0; timeStr[i] != '\0'; i++)
    {
        uint8_t width;
        if (timeStr[i] == ':')
            width = TimeFormatWidths::MinutesSecondsTenths::COLON;
        else if (timeStr[i] == '.')
            width = TimeFormatWidths::MinutesSecondsTenths::DECIMAL_POINT;
        else if (i >= 0 && i <= 1)
            width = TimeFormatWidths::MinutesSecondsTenths::MINUTE_DIGITS;
        else if (i >= 3 && i <= 4)
            width = TimeFormatWidths::MinutesSecondsTenths::SECOND_DIGITS;
        else
            width = TimeFormatWidths::MinutesSecondsTenths::TENTH_DIGIT;

        bool addSpace = (timeStr[i + 1] != '\0');
        writeCharToMatrix(timeStr[i], width, col, addSpace);
    }
}

uint8_t matrixGetCharWithWidth(char c, uint8_t preferredWidth, uint8_t bufSize, uint8_t *buffer)
{
    for (unsigned int i = 0; i < sizeof(customChars) / sizeof(CustomChar); i++)
    {
        if (customChars[i].ch == c && customChars[i].width == preferredWidth)
        {
            uint8_t w = customChars[i].width;
            if (w > bufSize)
                w = bufSize;
            for (uint8_t k = 0; k < w; k++)
                buffer[k] = customChars[i].bitmap[k];
            return w;
        }
    }

    for (unsigned int i = 0; i < sizeof(customChars) / sizeof(CustomChar); i++)
    {
        if (customChars[i].ch == c)
        {
            uint8_t w = customChars[i].width;
            if (w > bufSize)
                w = bufSize;
            for (uint8_t k = 0; k < w; k++)
                buffer[k] = customChars[i].bitmap[k];
            return w;
        }
    }

    return mx.getChar(c, bufSize, buffer);
}

void matrixSetBrightness(uint8_t brightness)
{
    if (brightness > MAX_BRIGHTNESS)
        brightness = MAX_BRIGHTNESS;
    if (brightness < MIN_BRIGHTNESS)
        brightness = MIN_BRIGHTNESS;
    mx.control(MD_MAX72XX::INTENSITY, brightness);
}

void initMatrix()
{
    mx.begin();
    matrixSetBrightness(MAX_BRIGHTNESS);
    matrixWipeAnimation();
    // Use saved brightness setting instead of hardcoded BRIGHTNESS
    int savedBrightness = getBrightness();
    matrixSetBrightness(savedBrightness);
    mx.clear();
    matrixShowString("MSC BK");
}

void matrixWipeAnimation()
{
    for (uint8_t col = 0; col < mx.getColumnCount(); col++)
    {
        mx.setColumn(col, 0xFF);
        vTaskDelay(pdMS_TO_TICKS(WIPE_DELAY));
    }
    for (uint8_t col = 0; col < mx.getColumnCount(); col++)
    {
        mx.setColumn(col, 0x00);
        vTaskDelay(pdMS_TO_TICKS(WIPE_DELAY));
    }
    vTaskDelay(pdMS_TO_TICKS(END_DELAY));
}

void matrixShowString(const char *text)
{
    uint8_t col = 0;
    uint8_t textLen = strlen(text);

    for (uint8_t i = 0; i < textLen; i++)
    {
        bool addSpace = (i < textLen - 1);
        writeCharToMatrix(text[i], 5, col, addSpace);
    }

    clearRemainingColumns(col);
}

void matrixShowTime(unsigned long time)
{
    uint8_t col = 0;

    if (time < 60000)
    {
        displaySecondsAndMilliseconds(time, col);
    }
    else if (time < 600000)
    {
        displayMinutesSecondsHundredths(time, col);
    }
    else if (time < 3600000)
    {
        displayMinutesSecondsTenths(time, col);
    }
    else
    {
        matrixShowString("> 60 min");
        return;
    }

    clearRemainingColumns(col);
}
