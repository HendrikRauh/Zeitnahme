#include <anzeige.h>

MD_MAX72XX mx = MD_MAX72XX(MD_MAX72XX::DR1CR0RR1_HW, DIN_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

void matrixSetBrigtness(uint8_t brightness)
{
    if (brightness > 15)
        brightness = 15;
    if (brightness < 1)
        brightness = 1;
    mx.control(MD_MAX72XX::INTENSITY, brightness);
}

void initMatrix()
{
    mx.begin();
    matrixSetBrigtness(15);
    matrixWipeAnimation();
    matrixSetBrigtness(BRIGHTNESS);
    mx.clear();
    matrixShowString("MSC BK");
}

void matrixWipeAnimation()
{
    for (uint8_t col = 0; col < mx.getColumnCount(); col++)
    {
        mx.setColumn(col, 0xFF);
        delay(30);
    }
    for (uint8_t col = 0; col < mx.getColumnCount(); col++)
    {
        mx.setColumn(col, 0x00);
        delay(30);
    }
    delay(200);
}

void matrixShowString(const char *text)
{
    mx.clear();
    uint8_t col = 0;
    for (uint8_t i = 0; text[i] != '\0'; i++)
    {
        uint8_t buffer[8];
        uint8_t charWidth = mx.getChar(text[i], sizeof(buffer), buffer);
        for (uint8_t k = 0; k < charWidth; k++)
        {
            if (col < mx.getColumnCount())
                mx.setColumn(col++, buffer[k]);
        }
        // 1 Spalte Abstand zwischen Zeichen
        if (col < mx.getColumnCount())
            mx.setColumn(col++, 0x00);
    }
}

void matrixShowTime(unsigned long time)
{ // Wenn unter 1 minute einfach als string formatieren und anzeigen (ss,mms)
    if (time < 60000)
    {
        char buffer[10];
        sprintf(buffer, "%02lu,%03lu", time / 1000, time % 1000);
        matrixShowString(buffer);
    }
    else
    {
        // TODO: Wenn über 1 minute -> zu wenig platz
    }
}