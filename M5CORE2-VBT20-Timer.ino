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

enum NotificationMode
{
    NOTIFY_LOUD,
    NOTIFY_SOFT,
    NOTIFY_SILENT,
    NOTIFY_VIBRATE,
    NOTIFY_MODE_COUNT
};

enum SettingsItem
{
    SETTING_SOUND,
    SETTING_VIBRATION,
    SETTING_NOTIFY_MODE,
    SETTING_ITEM_COUNT
};

M5Canvas canvas(&M5.Display);

TimerState timerState = TIMER_READY;
UiMode uiMode = UI_TIMER;
NotificationMode notificationMode = NOTIFY_LOUD;
uint8_t settingsCursor = SETTING_SOUND;

uint32_t durationSec = DEFAULT_TIMER_MINUTES * 60;
uint32_t remainingSec = durationSec;
uint32_t lastTickMs = 0;
uint32_t lastDrawMs = 0;

bool halfNotified = false;
bool imuTiltArmed = true;
uint32_t lastImuAdjustMs = 0;
uint32_t lastHoldAdjustMsA = 0;
uint32_t lastHoldAdjustMsC = 0;
bool soundEnabled = SOUND_ENABLED_DEFAULT;
bool vibrationEnabled = VIBRATION_ENABLED_DEFAULT;

uint16_t colorBg, colorText, colorDim;
uint16_t colorReady, colorRun, colorPause, colorDone;

const char *notifyModeLabel()
{
    switch (notificationMode)
    {
    case NOTIFY_LOUD:
        return "LOUD";
    case NOTIFY_SOFT:
        return "SOFT";
    case NOTIFY_SILENT:
        return "SILENT";
    case NOTIFY_VIBRATE:
        return "VIBRATE";
    default:
        return "LOUD";
    }
}

bool isSoundNotifyEnabled()
{
    if (!soundEnabled)
        return false;
    return notificationMode == NOTIFY_LOUD || notificationMode == NOTIFY_SOFT;
}

bool isVibrationNotifyEnabled()
{
    if (!vibrationEnabled)
        return false;
    return notificationMode == NOTIFY_LOUD || notificationMode == NOTIFY_SOFT || notificationMode == NOTIFY_VIBRATE;
}

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
    if (isSoundNotifyEnabled())
    {
        uint16_t adjustedFreq = freq;
        uint16_t adjustedMs = ms;
        if (notificationMode == NOTIFY_SOFT)
        {
            adjustedFreq = (uint16_t)(freq * 0.85f);
            adjustedMs = max((uint16_t)20, (uint16_t)(ms * 0.55f));
        }
        M5.Speaker.tone(adjustedFreq, adjustedMs);
    }
#endif
}

void vibratePulse(uint16_t ms, uint8_t level = VIBRATION_LEVEL)
{
#if USE_VIBRATION
    if (!isVibrationNotifyEnabled())
        return;

    uint8_t adjustedLevel = level;
    if (notificationMode == NOTIFY_SOFT)
    {
        adjustedLevel = (uint8_t)(level * 0.6f);
    }

    M5.Power.setVibration(adjustedLevel);
    delay(ms);
    M5.Power.setVibration(0);
#endif
}

void vibratePattern(uint8_t count, uint16_t onMs)
{
#if USE_VIBRATION
    if (!isVibrationNotifyEnabled())
        return;

    for (uint8_t i = 0; i < count; i++)
    {
        vibratePulse(onMs);
        if (i + 1 < count)
        {
            delay(VIB_GAP_MS);
        }
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

void adjustMinutesWithFeedback(int delta)
{
    uint32_t before = durationSec;
    adjustMinutes(delta);
    if (durationSec != before)
    {
        beep((delta > 0) ? 2200 : 1800, 25);
        vibratePattern(1, VIB_SHORT_MS);
    }
}

void cycleNotificationMode()
{
    notificationMode = (NotificationMode)((notificationMode + 1) % NOTIFY_MODE_COUNT);
}

bool isTouchClicked()
{
    if (!M5.Touch.isEnabled())
        return false;
    return M5.Touch.getDetail().wasClicked();
}

void notifyHalfTime()
{
    if (notificationMode == NOTIFY_LOUD || notificationMode == NOTIFY_SOFT)
    {
        beep(2000, 90);
        vibratePattern(1, VIB_SHORT_MS);
    }
    else if (notificationMode == NOTIFY_VIBRATE)
    {
        vibratePattern(1, VIB_SHORT_MS);
    }
}

void notifyTimeUp()
{
    if (notificationMode == NOTIFY_LOUD || notificationMode == NOTIFY_SOFT)
    {
        for (int i = 0; i < 3; i++)
        {
            beep(2600, 120);
            delay(140);
        }
    }

    if (notificationMode == NOTIFY_LOUD || notificationMode == NOTIFY_SOFT || notificationMode == NOTIFY_VIBRATE)
    {
        vibratePattern(3, VIB_MEDIUM_MS);
    }
}

void handleButtons()
{
    if (uiMode == UI_SETTINGS)
    {
        if (M5.BtnA.wasPressed())
        {
            settingsCursor = (settingsCursor + SETTING_ITEM_COUNT - 1) % SETTING_ITEM_COUNT;
            beep(1800, 25);
        }

        if (M5.BtnC.wasPressed())
        {
            settingsCursor = (settingsCursor + 1) % SETTING_ITEM_COUNT;
            beep(2200, 25);
        }

        if (isTouchClicked())
        {
            if (settingsCursor == SETTING_SOUND)
            {
                soundEnabled = !soundEnabled;
            }
            else if (settingsCursor == SETTING_VIBRATION)
            {
                vibrationEnabled = !vibrationEnabled;
            }
            else if (settingsCursor == SETTING_NOTIFY_MODE)
            {
                cycleNotificationMode();
            }

            beep(2100, 35);
            vibratePattern(1, VIB_SHORT_MS);
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
        adjustMinutesWithFeedback(-1);
        lastHoldAdjustMsA = millis();
    }

    if (M5.BtnC.wasPressed())
    {
        adjustMinutesWithFeedback(1);
        lastHoldAdjustMsC = millis();
    }

    if (M5.BtnA.wasReleased())
    {
        lastHoldAdjustMsA = 0;
    }

    if (M5.BtnC.wasReleased())
    {
        lastHoldAdjustMsC = 0;
    }

    uint32_t now = millis();
    if (M5.BtnA.pressedFor(BUTTON_HOLD_START_MS) && now - lastHoldAdjustMsA >= BUTTON_HOLD_REPEAT_MS)
    {
        adjustMinutesWithFeedback(-BUTTON_HOLD_STEP_MINUTES);
        lastHoldAdjustMsA = now;
    }

    if (M5.BtnC.pressedFor(BUTTON_HOLD_START_MS) && now - lastHoldAdjustMsC >= BUTTON_HOLD_REPEAT_MS)
    {
        adjustMinutesWithFeedback(BUTTON_HOLD_STEP_MINUTES);
        lastHoldAdjustMsC = now;
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

    uint32_t before = durationSec;
    adjustMinutes(delta);
    if (durationSec != before)
    {
        beep((delta > 0) ? 2200 : 1800, 25);
        vibratePattern(1, VIB_SHORT_MS);
    }

    imuTiltArmed = false;
    lastImuAdjustMs = now;
#endif
}

void handleTouchMinuteAdjust()
{
#if USE_TOUCH_MINUTE_ADJUST
    if (uiMode == UI_SETTINGS)
        return;

    if (timerState == TIMER_RUNNING)
        return;

    if (!M5.Touch.isEnabled())
        return;

    auto td = M5.Touch.getDetail();
    int delta = 0;

    if (td.wasFlicked())
    {
        int dx = td.distanceX();
        int dy = td.distanceY();
        if (abs(dx) >= TOUCH_SWIPE_THRESHOLD_PX && abs(dx) > abs(dy))
        {
            delta = (dx > 0) ? 1 : -1;
        }
    }
    else if (td.wasClicked())
    {
        int centerX = SCREEN_WIDTH / 2;
        if (td.x <= centerX - TOUCH_TAP_CENTER_DEADZONE_PX)
        {
            delta = -1;
        }
        else if (td.x >= centerX + TOUCH_TAP_CENTER_DEADZONE_PX)
        {
            delta = 1;
        }
    }

    if (delta == 0)
        return;

    uint32_t before = durationSec;
    adjustMinutes(delta);
    if (durationSec != before)
    {
        beep((delta > 0) ? 2200 : 1800, 25);
        vibratePattern(1, VIB_SHORT_MS);
    }
#endif
}

uint16_t getDoneFlashColor()
{
    float phase = (float)(millis() % FLASH_CYCLE_MS) / (float)FLASH_CYCLE_MS;
    float wave = (1.0f - cosf(phase * 2.0f * PI)) * 0.5f;
    uint8_t level = (uint8_t)(wave * FLASH_MAX_BRIGHTNESS);

    if (notificationMode == NOTIFY_LOUD || notificationMode == NOTIFY_VIBRATE)
    {
        return M5.Display.color565(level, 0, 0);
    }
    return M5.Display.color565(level, level, level);
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
            notifyHalfTime();
        }

        if (remainingSec == 0)
        {
            timerState = TIMER_DONE;
            notifyTimeUp();
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
        if (remainingSec <= 300)
        {
            c = M5.Display.color565(255, 140, 50);
        }
        else
        {
            c = halfNotified ? M5.Display.color565(120, 210, 255) : colorRun;
        }
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
        uint16_t mainColor = colorText;
        if (timerState == TIMER_RUNNING)
        {
            if (remainingSec <= 30)
            {
                float phase = (float)(millis() % FLASH_CYCLE_MS) / (float)FLASH_CYCLE_MS;
                float wave = (1.0f - cosf(phase * 2.0f * PI)) * 0.5f;
                uint8_t r = (uint8_t)(120 + wave * 135);
                uint8_t g = (uint8_t)(45 + wave * 85);
                uint8_t b = (uint8_t)(20 + wave * 40);
                mainColor = M5.Display.color565(r, g, b);
            }
            else if (remainingSec <= 60)
            {
                bool visible = ((millis() / SEGMENT_BLINK_INTERVAL_MS) % 2) == 0;
                mainColor = visible ? M5.Display.color565(255, 120, 45) : colorDim;
            }
            else if (remainingSec <= 300)
            {
                mainColor = M5.Display.color565(255, 140, 50);
            }
        }

        canvas.setTextColor(mainColor, colorBg);
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
    canvas.drawString(info, SCREEN_WIDTH / 2, 178);

    canvas.setTextSize(1);
    canvas.setTextColor(colorDim, colorBg);
    String modeInfo = String("MODE: ") + String(notifyModeLabel());
    canvas.drawString(modeInfo, SCREEN_WIDTH / 2, 196);
}

void drawHelp()
{
#if DRAW_HELP_TEXT
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(1);
    canvas.setTextColor(colorDim, colorBg);

    if (uiMode == UI_SETTINGS)
    {
        canvas.drawString("A/C:item  tap:change  B:back", SCREEN_WIDTH / 2, 222);
    }
    else if (timerState == TIMER_READY)
    {
#if USE_IMU_MINUTE_ADJUST
        canvas.drawString("A/C tap:+/-1 hold:+/-3 touch:+/-1 tilt:+/-1", SCREEN_WIDTH / 2, 214);
        canvas.drawString("B:start   holdB:settings", SCREEN_WIDTH / 2, 226);
#else
        canvas.drawString("A/C tap:+/-1 hold:+/-3 touch:+/-1", SCREEN_WIDTH / 2, 214);
        canvas.drawString("B:start   holdB:settings", SCREEN_WIDTH / 2, 226);
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
    canvas.drawString("SETTINGS", SCREEN_WIDTH / 2, 44);

    canvas.setTextSize(2);
    canvas.setTextColor((settingsCursor == SETTING_SOUND) ? colorRun : colorText, colorBg);
    canvas.drawString(String((settingsCursor == SETTING_SOUND) ? "> " : "  ") + "SOUND: " + (soundEnabled ? "ON" : "OFF"), SCREEN_WIDTH / 2, 90);

    canvas.setTextColor((settingsCursor == SETTING_VIBRATION) ? colorRun : colorText, colorBg);
    canvas.drawString(String((settingsCursor == SETTING_VIBRATION) ? "> " : "  ") + "VIBE: " + (vibrationEnabled ? "ON" : "OFF"), SCREEN_WIDTH / 2, 120);

    canvas.setTextColor((settingsCursor == SETTING_NOTIFY_MODE) ? colorRun : colorText, colorBg);
    canvas.drawString(String((settingsCursor == SETTING_NOTIFY_MODE) ? "> " : "  ") + "MODE: " + notifyModeLabel(), SCREEN_WIDTH / 2, 150);

    canvas.setTextSize(1);
    canvas.setTextColor(colorDim, colorBg);
    canvas.drawString("A/C item  tap change  B back", SCREEN_WIDTH / 2, 184);
}

void drawDisplay()
{
    uint16_t savedBg = colorBg;
    if (timerState == TIMER_DONE)
    {
        colorBg = getDoneFlashColor();
    }

    canvas.fillScreen(colorBg);

    if (uiMode == UI_SETTINGS)
    {
        drawBarSegments();
        drawSettings();
        drawHelp();
        canvas.pushSprite(0, 0);
        colorBg = savedBg;
        return;
    }

    drawStatus();
    drawMainTime();
    drawBarSegments();
    drawInfo();
    drawHelp();

    canvas.pushSprite(0, 0);
    colorBg = savedBg;
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
    handleTouchMinuteAdjust();
    handleImuMinuteAdjust();
    updateTimer();

    uint32_t now = millis();
    if (now - lastDrawMs >= DRAW_INTERVAL_MS)
    {
        lastDrawMs = now;
        drawDisplay();
    }
}