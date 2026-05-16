[日本語版はこちら](README.ja.md)

# M5CORE2-VBT20-Timer

![Timer exterior](images/timer-exterior.jpg)

A practical visual timer for M5Stack Core2, inspired by VBT20-style progress visualization.

This project focuses on quiet, intuitive operation for study environments such as libraries, classrooms, and self-study rooms.

## Features

- Perimeter segment gauge (4 sides) with limited-step rainbow colors
- Clockwise depletion from the 12 o'clock position
- Single "next-to-disappear" segment blink during countdown
- Timer states: Ready / Running / Paused / Done
- Touch controls (tap/swipe) and button controls for minute adjustment
- Button hold acceleration (fast minute adjustment)
- Notification modes:
	- Loud
	- Soft
	- Silent
	- Vibrate
- Sound ON/OFF and vibration ON/OFF in settings
- Half-time notification
- Endgame visual cues:
	- Warm tint near the end
	- Gentle animation in final phase
- Gentle full-screen flash at time-up (mode-aware color)

## Controls

### Main Screen

- Button A tap: `-1 min`
- Button C tap: `+1 min`
- Button A/C hold: fast adjustment (`BUTTON_HOLD_STEP_MINUTES` each repeat)
- Button B tap:
	- Ready -> Start
	- Running -> Pause
	- Paused -> Resume
	- Done -> Reset
- Button B hold (Ready): open settings
- Button B hold (not Ready): reset timer

### Touch Controls

- Tap left side: `-1 min`
- Tap right side: `+1 min`
- Horizontal swipe:
	- Left swipe: `-1 min`
	- Right swipe: `+1 min`

### Settings Screen

- Button A/C: move item cursor
- Tap: change selected item
- Button B: back to timer screen

Settings items:

- SOUND: ON/OFF
- VIBE: ON/OFF
- MODE: Loud / Soft / Silent / Vibrate

## Build Environment

- Board: M5Stack Core2
- Framework: Arduino
- Library: M5Unified

## Files

- `M5CORE2-VBT20-Timer.ino`: main sketch
- `config.example.h`: template configuration
- `config.h`: local configuration (ignored by git)

## Configuration

Copy and edit local config:

1. Copy `config.example.h` to `config.h`
2. Tune values for your environment

Key parameters include:

- Display and brightness
- Segment count/thickness/gap
- Rainbow color steps
- Blink interval
- Button hold acceleration
- Touch sensitivity
- Flash cycle and brightness
- Vibration level and pulse timings

## Notes

- This project intentionally avoids cloud/mobile dependencies.
- Design goal: quiet, simple, reliable daily use.
