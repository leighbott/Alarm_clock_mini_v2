If you want to test builds please use ~/.platformio/penv/bin/pio instead of pio command.

1. System Overview

The device is a bedside alarm clock with physical rotary encoder + button navigation, audio output, temperature/humidity/pressure sensing, auto‑brightness, and two independent LED channels for sunrise and manual lighting.
All settings must persist across power loss using ESP32 NVS.

The UI is built entirely with LVGL and uses rotary encoder + button navigation (no touch, no swipe gestures).

2. Hardware Components
Core MCU

    ESP32‑S3 N16R8 dev board

        16 MB Flash, 8 MB OPI PSRAM

Display

    TWO ST7789V SPI TFT displays, sharing , 320x240 pixels, landscape

    Connected via FSPI (shared with SD card)


Input

    Two rotary encoders each with a button(hardware PCNT quadrature, half-quad mode via ESP32Encoder)

    No touchscreen.

Sensors

    LDR (light-dependent resistor) with series resistor
    
        Used for auto‑brightness

    I2C AHT20 temperature/humidity sensor

        I²C address: 0x38

        Read every 1 second

    I2C BMP280 barometric pressure sensor

        I²C address: 0x76

        Read every 1 second


    I2C DS3231 RTC module

        I²C address: 0x68
        Primary time source

    All I2C sensors on Wire1



LED Channels

    Two independent single‑colour, 2‑wire LEDs

    Driven via MOSFET/transistor

    Controlled using ESP32 LEDC PWM

    Channels:

        Front LED

        Back LED
Audio

    MAX98357A I²S amplifier

    MicroSD card for MP3 alarm sounds

        Shared FSPI bus

    I²S audio playback via ESP32-audioI2S (schreibfaul1)

    If no MP3 is available → no sound, but alarm still runs normally

Pin definitions
    Defined in pin_definitions.csv

3. Software Architecture
Core Modules

    ui_main_screen

    ui_main_settings_menu

    ui_time_date_menu

    ui_alarm_menu
        ui_sdcard_menu (mp3 selection within alarm menu)

    ui_alarm_quick_modify

    ui_other_settings_menu

    alarm_manager

    sunrise_manager

    brightness_manager

    input_manager (encoders + buttons)

    sensor_manager

    storage_manager (NVS)

    rtc_manager

    audio_manager

    led_manager

    pins_config.h (central GPIO pin definitions)

Module Rules

    UI never directly accesses hardware.

    Managers handle hardware and logic.

    All persistent values stored in NVS.

    No blocking delays; use timers or LVGL tasks.

4. Boot Sequence

    Initialize NVS

    Load all settings

    Initialize RTC (DS3231 on Wire1)

    Initialize sensors (AHT20, BMP280 — all on Wire1)

    Initialize LED PWM channels

    Initialize audio system (MAX98357A)

    Initialize LVGL + display (ST7789 via FSPI)

    Build main screen

    Start periodic timers (1‑second tasks)

5. Main Screen Layout

    Time: HH:mm ss

        Colon flashes every second

    Date: Weekday, DD(ordinal) MMM

    Next alarm time (or “OFF”)

    Time until next alarm

    Temperature (°C) and humidity (%)

No touch, no swipe gestures. All navigation uses rotary encoders and physical buttons. All screen elements must fit on the screen without scrolling.

6. Settings Menu Structure

All menus have the same header/title row at the top with the exact same formatting. 'Cancel' text on the top right, 'Accept' text on the top left, menu name in the middle.

Time & Date

    Infinite scrolling wheels for:

        Hour

        Minute

        Day

        Month

        Year

    Writes directly to RTC

Display

    Minimum display brightness toggle (screen completely off or brightness value 1 options)

    Auto display brightness on/off toggle

    Boost display brightness value

Alarm

    Current time in header

    Alarm on/off

    Alarm time (hour/minute)

    MP3 sound selection from SD card

    End alarm volume

    End sunrise (LED brightness) value

    Volume ramp duration

    Sunrise ramp duration

    Snooze on/off

    Snooze duration

    Hold to dismiss time

    Repeat mode: once / weekdays


7. Alarm Behaviour
Triggering

    Alarm triggers when:
    currentHour == alarmHour && currentMinute == alarmMinute && notTriggeredYetToday

    Seconds are ignored for robustness.

    Checked every 1 second.

Sunrise Behaviour

    Begins at:
    alarmTime - sunriseLeadInMinutes

    Ramp over the configured ramp duration. Exponential curve ramp to relate to human perception of brightness.

    Uses front and back LED channels. Front LEDs disabled while user input is detected (button press held or pressed to disable/snooze alarm), but front LED is restored if the user doesn't complete the snooze or dismiss alarm function.


Alarm Sound

    Play selected MP3 from SD card.

    Volume ramps linearly from 0 → target volume.

    If MP3 missing or SD removed:

        Defaults to beep sound (short sine wave bursts every second).

Snooze

    Button press during alarm = snooze immediately.

    Snooze duration configurable.

    Turn off both front and back LEDs. Set both LEDs to user set end brightness value after snooze duration (no ramp).

    Alarm plays at user set end volume after snooze duration (no ramp).

Dismissal

    Hold button for user configured number of seconds during alarm → dismiss.

    Sound and Front LEDs stop immediately on button press. Back LEDs remain on.

    Shrinking circle animation shows hold progress.

    After dismissal:

        Front LEDs turn off.
        Back LEDs remain on.

8. Physical Input Rules

    Encoder 1: adjusts front LED brightness (on main screen only) / navigates menu icons to select what variable to change or select.

    Encoder 2: adjusts back LED brightness (on main screen only) / changes value of selected variable.

    Encoder Button 1: toggles front LED (on main screen only) / back/cancel in menus.

    Encoder Button 2: toggles back LED (on main screen only) / confirm/save in menus.

    Any input on main screen = Boost screen brightness to user set boost brightness value for 2 seconds

9. LED Behaviour

Sunrise Override

    Sunrise temporarily overrides manual LED settings.

10. Brightness Management
Set display to user set boost display brightness on any screen except main screen.

Auto‑Brightness

    Uses simple LDR with resistor

    Smoothed to avoid flicker

    Minimum brightness enforced

Boost

    Any user interaction → boost screen brightness to user set boost brightness value for 2 seconds

    Then returns to auto‑brightness or manual setting

11. Sensor Timing
Every 1 second

    Update clock

    Toggle colon

    Check alarm trigger

    Read AHT20 + BMP280 (with retry on failure)

Event‑Driven

    Encoder rotation / button press

    Alarm start/stop

    SD card insert/remove

    UI interactions

12. Persistent Storage (NVS)
Stored Values

All user configurable settings (Alarm, time and date, display brightness, volume, etc.)

Rules

    Write only when settings change.

    Never write inside loops or periodic tasks.

    On boot, load all settings before UI initialization.

13. Error Handling

    SD missing → Default to beep sound, alarm still functions.

    AHT20/BMP280 failure → Display "--°C / --%".

    RTC failure → Display “RTC ERROR”.

    LED failure → Ignore sunrise effect, alarm still functions.

14. State Machines
Alarm State

    Idle

    Sunrise

    Alarm active

    Snoozed

    Dismissing (touch‑hold)

    Dismissed

Display Brightness State

    Auto

    Boost

    Manual override

Input State

    Idle

    Encoder rotating

    Button pressed

    Button held

    Hold‑complete

15. MAX98357A Audio — How It Was Made to Work

Hardware
Amplifier: MAX98357A I²S amp
SD card (source of audio files): Shared FSPI bus with the display
Library
Use schreibfaul1/ESP32-audioI2S, pinned to tag 3.0.8:
https://github.com/schreibfaul1/ESP32-audioI2S.git#3.0.8

This library requires C++17. In platformio.ini:
build_unflags = -std=gnu++11 -std=gnu++14

(No explicit -std=gnu++17 is needed — removing the defaults lets PlatformIO pick 17 automatically.)

Critical Build Patch (pre-build script)
The library at tag 3.0.8 uses std::optional but omits #include <optional> from its header. It will not compile without this fix. A pre-build script (patch_libs.py) injects the missing include automatically before every build:
# Replaces the first occurrence of #include <locale>
# with #include <locale>\n#include <optional> in Audio.h

Register it in platformio.ini:
extra_scripts = pre:scripts/patch_libs.py

SD + SPI Bus Sharing
The display (LovyanGFX) is initialised first and configures the FSPI bus. When audio_init() runs, it calls:
SPI.begin(PIN_TFT_SCK, PIN_SD_MISO, PIN_TFT_MOSI, -1);
SD.begin(PIN_SD_CS, SPI);

This re-uses the already-configured bus, adding MISO (which the display doesn't need). No second SPI bus or separate SPIClass instance is required.

Initialisation Order
Must happen in this sequence:

Display init (LovyanGFX — configures FSPI pins)
audio_init() — calls SPI.begin(...) then SD.begin(...), then audio.setPinout(BCLK, LRCK, DOUT)

Volume Mapping
The library uses a 0–21 integer scale. User-facing volume is 0–100%. Map with:
uint8_t lib_vol = (uint8_t)((uint32_t)vol_pct * 21 / 100);

Required Callbacks
The library mandates these global C functions exist (even if empty):
void audio_info(const char*) {}
void audio_id3data(const char*) {}
void audio_eof_mp3(const char*) {}
void audio_showstation(const char*) {}
void audio_showstreamtitle(const char*) {}
void audio_bitrate(const char*) {}
void audio_commercial(const char*) {}
void audio_icyurl(const char*) {}
void audio_lasthost(const char*) {}

Loop Requirement
audio.loop() must be called every loop() iteration — the library is not interrupt-driven.

Beep Fallback
When no MP3 is available, pre-generated .wav files stored on the SD card under /beeps/{freq}hz.wav are used as a beep fallback (e.g. 1000hz.wav). These are simple sine-wave bursts generated offline by a Python script.

