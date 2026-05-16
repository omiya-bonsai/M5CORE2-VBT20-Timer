#include <M5Unified.h>
#include "config.h"

enum TimerState
{
    TIMER_READY,
    TIMER_RUNNING,
    TIMER_PAUSED,
    TIMER_DONE
};

enum UiMode
{
    UI_TIMER,
    UI_SETTINGS
};

M5Canvas canvas(&M5.Display);

TimerState timerState = TIMER_READY;
UiMode uiMode = UI_TIMER;

uint32_t durationSec = DEFAULT_TIMER_MINUTES * 60;
uint32_t remainingSec = durationSec;
uint32_t lastTickMs = 0;
uint32_t lastDrawMs = 0;

bool halfNotified = false;
bool imuTiltArmed = true;
uint32_t lastImuAdjustMs = 0;
bool soundEnabled = SOUND_ENABLED_DEFAULT;

uint16_t colorBg, colorText, colorDim;
uint16_t colorReady, colorRun, colorPause, colorDone;

uint16_t hsvTo565(float h, float s, float v)
{
    h = fmodf(h, 1.0f);
    if (h < 0.0f)
        h += 1.0f;

    float c = v * s;
    float hp = h * 6.0f;
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));

    float r = 0.0f, g = 0.0f, b = 0.0f;

    if (hp < 1.0f)
    {
        r = c;
        g = x;
    }
    else if (hp < 2.0f)
    {
        r = x;
        g = c;
    }
    else if (hp < 3.0f)
    {
        g = c;
        b = x;
    }
    else if (hp < 4.0f)
    {
        g = x;
        b = c;
    }
    else if (hp < 5.0f)
    {
        r = x;
        b = c;
    }
    else
    {
        r = c;
        b = x;
    }

    float m = v - c;
    uint8_t rr = (uint8_t)((r + m) * 255.0f);
    uint8_t gg = (uint8_t)((g + m) * 255.0f);
    uint8_t bb = (uint8_t)((b + m) * 255.0f);

    return M5.Display.color565(rr, gg, bb);
}

uint16_t rainbowColorByIndex(int index, int total)
{
    int steps = constrain(RAINBOW_COLOR_STEPS, 7, 15);

    if (total <= 1 || steps <= 1)
        return hsvTo565(0.0f, 1.0f, 1.0f);

    // Convert continuous position into a limited color palette index.
    int paletteIndex = (index * steps) / total;
    if (paletteIndex >= steps)
        paletteIndex = steps - 1;

    float h = (float)paletteIndex / (float)steps;
    return hsvTo565(h, 1.0f, 1.0f);
}

void beep(uint16_t freq, uint16_t ms)
{
#if USE_SPEAKER
    if (soundEnabled)
    {
        M5.Speaker.tone(freq, ms);
    }
#endif
}

void resetTimer()
{
    timerState = TIMER_READY;
    durationSec = DEFAULT_TIMER_MINUTES * 60;
    remainingSec = durationSec;
    halfNotified = false;
}

String formatTime(uint32_t sec)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%02lu:%02lu", sec / 60, sec % 60);
    return String(buf);
}

float remainRatio()
{
    if (durationSec == 0)
        return 0.0f;
    return (float)remainingSec / (float)durationSec;
}

void adjustMinutes(int delta)
{
    if (timerState == TIMER_RUNNING)
        return;

    int minutes = durationSec / 60;
    minutes += delta;
    minutes = constrain(minutes, MIN_TIMER_MINUTES, MAX_TIMER_MINUTES);

    durationSec = minutes * 60;
    remainingSec = durationSec;
    halfNotified = false;
}

void handleButtons()
{
    if (uiMode == UI_SETTINGS)
    {
        if (M5.BtnA.wasPressed() || M5.BtnC.wasPressed())
        {
            soundEnabled = !soundEnabled;
            beep(soundEnabled ? 2400 : 1200, 40);
        }

        if (M5.BtnB.wasPressed())
        {
            uiMode = UI_TIMER;
            beep(2000, 40);
        }
        return;
    }

    if (timerState == TIMER_READY && M5.BtnB.pressedFor(SETTINGS_LONG_PRESS_MS))
    {
        uiMode = UI_SETTINGS;
        beep(1800, 60);
        delay(350);
        return;
    }

    // Core2: A=-1分 / B=開始・停止 / C=+1分
    if (M5.BtnA.wasPressed())
    {
        adjustMinutes(-1);
        beep(1800, 25);
    }

    if (M5.BtnC.wasPressed())
    {
        adjustMinutes(1);
        beep(2200, 25);
    }

    if (M5.BtnB.wasPressed())
    {
        if (timerState == TIMER_READY)
        {
            timerState = TIMER_RUNNING;
            lastTickMs = millis();
            beep(2600, 70);
        }
        else if (timerState == TIMER_RUNNING)
        {
            timerState = TIMER_PAUSED;
            beep(1800, 50);
        }
        else if (timerState == TIMER_PAUSED)
        {
            timerState = TIMER_RUNNING;
            lastTickMs = millis();
            beep(2600, 50);
        }
        else if (timerState == TIMER_DONE)
        {
            resetTimer();
            beep(2200, 100);
        }
    }

    if (timerState != TIMER_READY && M5.BtnB.pressedFor(LONG_PRESS_RESET_MS))
    {
        resetTimer();
        beep(1200, 120);
        delay(500);
    }
}

void handleImuMinuteAdjust()
{
#if USE_IMU_MINUTE_ADJUST
    if (uiMode == UI_SETTINGS)
        return;

    if (timerState == TIMER_RUNNING)
        return;

    float ax = 0.0f, ay = 0.0f, az = 0.0f;
    if (!M5.Imu.getAccel(&ax, &ay, &az))
        return;

    float lateral = (fabsf(ax) >= fabsf(ay)) ? ax : ay;

    if (!imuTiltArmed)
    {
        if (fabsf(lateral) <= IMU_TILT_RELEASE_G)
        {
            imuTiltArmed = true;
        }
        return;
    }

    if (fabsf(lateral) < IMU_TILT_THRESHOLD_G)
        return;

    uint32_t now = millis();
    if (now - lastImuAdjustMs < IMU_ADJUST_DEBOUNCE_MS)
        return;

    int delta = (lateral > 0.0f) ? 1 : -1;
#if IMU_TILT_INVERT
    delta = -delta;
#endif

    adjustMinutes(delta);
    beep((delta > 0) ? 2200 : 1800, 25);

    imuTiltArmed = false;
    lastImuAdjustMs = now;
#endif
}

void updateTimer()
{
    if (timerState != TIMER_RUNNING)
        return;

    uint32_t now = millis();

    if (now - lastTickMs >= 1000)
    {
        lastTickMs += 1000;

        if (remainingSec > 0)
        {
            remainingSec--;
        }

        if (HALF_TIME_NOTIFY && !halfNotified && remainingSec <= durationSec / 2)
        {
            halfNotified = true;
            beep(2000, 120);
        }

        if (remainingSec == 0)
        {
            timerState = TIMER_DONE;
            for (int i = 0; i < 3; i++)
            {
                beep(2600, 120);
                delay(140);
            }
        }
    }
}

void drawBarSegments()
{
    float ratio = remainRatio();
    int activeSegments = ceil(ratio * BAR_SEGMENTS);
    int offSegments = BAR_SEGMENTS - activeSegments;
    bool blinkEnabled = (timerState == TIMER_RUNNING);
    bool blinkVisible = ((millis() / SEGMENT_BLINK_INTERVAL_MS) % 2) == 0;

    int topSegments = BAR_SEGMENTS / 4;
    int rightSegments = BAR_SEGMENTS / 4;
    int bottomSegments = BAR_SEGMENTS / 4;
    int leftSegments = BAR_SEGMENTS - (topSegments + rightSegments + bottomSegments);

    int topW = (SCREEN_WIDTH - (topSegments - 1) * BAR_GAP) / topSegments;
    int rightH = (SCREEN_HEIGHT - 2 * BAR_THICKNESS - (rightSegments - 1) * BAR_GAP) / rightSegments;
    int bottomW = (SCREEN_WIDTH - (bottomSegments - 1) * BAR_GAP) / bottomSegments;
    int leftH = (SCREEN_HEIGHT - 2 * BAR_THICKNESS - (leftSegments - 1) * BAR_GAP) / leftSegments;

    // Perimeter index order is clockwise; rotate start to 12 o'clock (top center).
    int startIndex = topSegments / 2;

    for (int step = 0; step < BAR_SEGMENTS; step++)
    {
        int idx = (startIndex + step) % BAR_SEGMENTS;
        bool isActive = (step >= offSegments);
        uint16_t c = colorDim;
        if (isActive)
        {
            bool isNextToOff = (activeSegments > 0) && (step == offSegments);
            if (blinkEnabled && isNextToOff && !blinkVisible)
            {
                c = colorBg;
            }
            else
            {
                c = rainbowColorByIndex(idx, BAR_SEGMENTS);
            }
        }

        if (idx < topSegments)
        {
            int i = idx;
            int x = i * (topW + BAR_GAP);
            canvas.fillRect(x, 0, topW, BAR_THICKNESS, c);
        }
        else if (idx < topSegments + rightSegments)
        {
            int i = idx - topSegments;
            int y = BAR_THICKNESS + i * (rightH + BAR_GAP);
            canvas.fillRect(SCREEN_WIDTH - BAR_THICKNESS, y, BAR_THICKNESS, rightH, c);
        }
        else if (idx < topSegments + rightSegments + bottomSegments)
        {
            int i = idx - (topSegments + rightSegments);
            int x = SCREEN_WIDTH - bottomW - i * (bottomW + BAR_GAP);
            canvas.fillRect(x, SCREEN_HEIGHT - BAR_THICKNESS, bottomW, BAR_THICKNESS, c);
        }
        else
        {
            int i = idx - (topSegments + rightSegments + bottomSegments);
            int y = SCREEN_HEIGHT - BAR_THICKNESS - leftH - i * (leftH + BAR_GAP);
            canvas.fillRect(0, y, BAR_THICKNESS, leftH, c);
        }
    }
}

void drawStatus()
{
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(2);

    String label;
    uint16_t c;

    switch (timerState)
    {
    case TIMER_READY:
        label = "READY";
        c = colorReady;
        break;
    case TIMER_RUNNING:
        label = "RUNNING";
        c = colorRun;
        break;
    case TIMER_PAUSED:
        label = "PAUSED";
        c = colorPause;
        break;
    case TIMER_DONE:
        label = "TIME UP";
        c = colorDone;
        break;
    }

    canvas.setTextColor(c, colorBg);
    canvas.drawString(label, SCREEN_WIDTH / 2, 32);
}

void drawMainTime()
{
    canvas.setTextDatum(middle_center);

    if (timerState == TIMER_DONE)
    {
        canvas.setTextColor(colorDone, colorBg);
        canvas.setTextSize(5);
        canvas.drawString("DONE", SCREEN_WIDTH / 2, 100);
    }
    else
    {
        canvas.setTextColor(colorText, colorBg);
        canvas.setTextSize(6);
        canvas.drawString(formatTime(remainingSec), SCREEN_WIDTH / 2, 105);
    }
}

void drawInfo()
{
    float ratio = remainRatio();
    int percent = round(ratio * 100);

    canvas.setTextDatum(middle_center);
    canvas.setTextColor(colorText, colorBg);
    canvas.setTextSize(2);

    String info = String(durationSec / 60) + " min / " + String(percent) + "%";
    canvas.drawString(info, SCREEN_WIDTH / 2, 182);
}

void drawHelp()
{
#if DRAW_HELP_TEXT
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(1);
    canvas.setTextColor(colorDim, colorBg);

    if (uiMode == UI_SETTINGS)
    {
        canvas.drawString("A/C:toggle sound   B:back", SCREEN_WIDTH / 2, 222);
    }
    else if (timerState == TIMER_READY)
    {
#if USE_IMU_MINUTE_ADJUST
        canvas.drawString("A:-1 B:start C:+1 tilt:+/-1 holdB:settings", SCREEN_WIDTH / 2, 222);
#else
        canvas.drawString("A:-1min B:start C:+1min holdB:settings", SCREEN_WIDTH / 2, 222);
#endif
    }
    else if (timerState == TIMER_RUNNING)
    {
        canvas.drawString("B:pause", SCREEN_WIDTH / 2, 222);
    }
    else if (timerState == TIMER_PAUSED)
    {
        canvas.drawString("B:resume   hold B:reset", SCREEN_WIDTH / 2, 222);
    }
    else
    {
        canvas.drawString("B:reset", SCREEN_WIDTH / 2, 222);
    }
#endif
}

void drawSettings()
{
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(colorText, colorBg);

    canvas.setTextSize(2);
    canvas.drawString("SETTINGS", SCREEN_WIDTH / 2, 56);

    canvas.setTextSize(3);
    canvas.setTextColor(soundEnabled ? colorRun : colorDone, colorBg);
    canvas.drawString(soundEnabled ? "SOUND: ON" : "SOUND: OFF", SCREEN_WIDTH / 2, 118);

    canvas.setTextSize(1);
    canvas.setTextColor(colorDim, colorBg);
    canvas.drawString("A/C toggle  B back", SCREEN_WIDTH / 2, 154);
}

void drawDisplay()
{
    canvas.fillScreen(colorBg);

    if (uiMode == UI_SETTINGS)
    {
        drawBarSegments();
        drawSettings();
        drawHelp();
        canvas.pushSprite(0, 0);
        return;
    }

    drawStatus();
    drawMainTime();
    drawBarSegments();
    drawInfo();
    drawHelp();

    canvas.pushSprite(0, 0);
}

void setup()
{
    auto cfg = M5.config();
    cfg.internal_spk = true;
    M5.begin(cfg);

    M5.Display.setRotation(DISPLAY_ROTATION);
    M5.Display.setBrightness(DISPLAY_BRIGHTNESS);

    canvas.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);

    colorBg = M5.Display.color565(0, 0, 0);
    colorText = M5.Display.color565(245, 245, 245);
    colorDim = M5.Display.color565(55, 55, 55);
    colorReady = M5.Display.color565(120, 180, 255);
    colorRun = M5.Display.color565(80, 220, 120);
    colorPause = M5.Display.color565(255, 190, 60);
    colorDone = M5.Display.color565(255, 70, 50);

    resetTimer();
    drawDisplay();
}

void loop()
{
    M5.update();

    handleButtons();
    handleImuMinuteAdjust();
    updateTimer();

    uint32_t now = millis();
    if (now - lastDrawMs >= DRAW_INTERVAL_MS)
    {
        lastDrawMs = now;
        drawDisplay();
    }
}