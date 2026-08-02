If you want to test builds please use /home/leigh/.platformio/penv/bin/pio instead of pio command.

The device is a bedside user configurable sound and LED ramp up alarm clock with physical rotary encoder + button navigation, audio output, temperature/humidity/pressure sensing, auto‑brightness display, and two independent LED channels.

The UI is built entirely with LVGL and uses rotary encoder + button navigation (no touch, no swipe gestures). All UI elements must not exceed display boundaries (no scrolling).

Module Rules
    UI never directly accesses hardware.
    Managers handle hardware and logic.
    All persistent values stored in NVS.
    No blocking delays; use timers or LVGL tasks.