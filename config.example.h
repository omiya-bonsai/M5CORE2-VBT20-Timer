#pragma once

// ------------------------------------------------------------
// M5Stack Core2 VBT20-like Visual Timer
// ------------------------------------------------------------

#define DEFAULT_TIMER_MINUTES 25
#define MIN_TIMER_MINUTES 1
#define MAX_TIMER_MINUTES 199

#define HALF_TIME_NOTIFY true
#define LONG_PRESS_RESET_MS 1200
#define DRAW_INTERVAL_MS 100
#define SEGMENT_BLINK_INTERVAL_MS 600

#define SOUND_ENABLED_DEFAULT true
#define SETTINGS_LONG_PRESS_MS 900

#define USE_IMU_MINUTE_ADJUST false
#define IMU_TILT_THRESHOLD_G 0.55f
#define IMU_TILT_RELEASE_G 0.25f
#define IMU_ADJUST_DEBOUNCE_MS 250
#define IMU_TILT_INVERT false

#define USE_TOUCH_MINUTE_ADJUST true
#define TOUCH_SWIPE_THRESHOLD_PX 40
#define TOUCH_TAP_CENTER_DEADZONE_PX 24

#define USE_SPEAKER true
#define USE_VIBRATION true

#define VIBRATION_LEVEL 180
#define VIB_SHORT_MS 40
#define VIB_MEDIUM_MS 70
#define VIB_LONG_MS 160
#define VIB_GAP_MS 90

// Core2 LCD: 320x240
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

#define DISPLAY_ROTATION 1
#define DISPLAY_BRIGHTNESS 180

// 外周ゲージの分割数（4辺合計）
#define BAR_SEGMENTS 36

// 外周ゲージの太さ
#define BAR_THICKNESS 11

// レインボーの離散色数（7〜15推奨）
#define RAINBOW_COLOR_STEPS 8

#define BAR_WIDTH 280
#define BAR_HEIGHT 26
#define BAR_GAP 5

#define BAR_X ((SCREEN_WIDTH - BAR_WIDTH) / 2)
#define BAR_Y 145

#define DRAW_HELP_TEXT true