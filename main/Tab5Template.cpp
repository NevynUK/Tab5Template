#include <M5GFX.h>
M5GFX display;

#include <stdio.h>

void Setup(void)
{
    display.init();
    display.setBrightness(128);  // AXP2101 backlight — 0 by default, must be set explicitly
    display.setFont(&fonts::Font4);
    // Tab5 native panel is portrait (720×1280); rotation 3 = landscape rotated 180 degrees (1280×720)
    display.setRotation(3);

    // Confirm display pipeline with a visible splash
    display.fillScreen(TFT_WHITE);
    display.setTextColor(TFT_BLACK, TFT_WHITE);
    display.setTextDatum(textdatum_t::middle_center);
    display.drawString("Tab5 Ready", display.width() / 2, display.height() / 2 - 24);
    if (!display.touch())
    {
        display.drawString("Touch not found.", display.width() / 2, display.height() / 2 + 24);
    }
    display.display();
    display.setTextColor(TFT_WHITE, TFT_BLACK);

    display.startWrite();
}

void Loop(void)
{
    static bool drawn = false;
    lgfx::touch_point_t tp[3];

    int nums = display.getTouchRaw(tp, 3);
    if (nums)
    {
        for (int i = 0; i < nums; ++i)
        {
            display.setCursor(16, 16 + i * 24);
            display.printf("Raw X:%04d  Y:%04d    ", tp[i].x, tp[i].y);
        }

        display.convertRawXY(tp, nums);

        for (int i = 0; i < nums; ++i)
        {
            display.setCursor(16, 128 + i * 24);
            display.printf("Convert X:%04d  Y:%04d    ", tp[i].x, tp[i].y);
        }
        display.display();

        display.setColor(display.isEPD() ? TFT_BLACK : TFT_WHITE);
        for (int i = 0; i < nums; ++i)
        {
            int s = tp[i].size + 3;
            switch (tp[i].id)
            {
            case 0:
                display.fillCircle(tp[i].x, tp[i].y, s);
                break;
            case 1:
                display.drawLine(tp[i].x - s, tp[i].y - s, tp[i].x + s, tp[i].y + s);
                display.drawLine(tp[i].x - s, tp[i].y + s, tp[i].x + s, tp[i].y - s);
                break;
            default:
                display.fillTriangle(tp[i].x - s, tp[i].y + s, tp[i].x + s, tp[i].y + s, tp[i].x, tp[i].y - s);
                break;
            }
            display.display();
        }
        drawn = true;
    }
    else if (drawn)
    {
        drawn = false;
        display.waitDisplay();
        display.clear();
        display.display();
    }
    vTaskDelay(1);
}

extern "C" void app_main(void)
{
    Setup();
    while (1)
    {
        Loop();
    }
}
