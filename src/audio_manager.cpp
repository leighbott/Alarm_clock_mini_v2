#include "audio_manager.h"
#include "pins_config.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Audio.h>

static Audio      audio;
static bool       sd_ok      = false;
static bool       playing    = false;
static uint8_t    volume_pct = 0;

// ── Required library callbacks (must exist even if unused) ────────────────────
void audio_info(const char *)          {}
void audio_id3data(const char *)       {}
void audio_eof_mp3(const char *)       { playing = false; }
void audio_showstation(const char *)   {}
void audio_showstreamtitle(const char*) {}
void audio_bitrate(const char *)       {}
void audio_commercial(const char *)    {}
void audio_icyurl(const char *)        {}
void audio_lasthost(const char *)      {}

// ── Helpers ───────────────────────────────────────────────────────────────────
static uint8_t pct_to_lib(uint8_t pct) {
    return (uint8_t)((uint32_t)pct * 21 / 100);
}

// ── Public API ────────────────────────────────────────────────────────────────
bool audio_manager_init() {
    // ── Hardware diagnostic: check MISO line before touching SPI ─────────────
    pinMode(PIN_SD_MISO, INPUT_PULLUP);
    delay(1);
    int miso_idle = digitalRead(PIN_SD_MISO);
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, LOW);
    delay(1);
    int miso_selected = digitalRead(PIN_SD_MISO);
    digitalWrite(PIN_SD_CS, HIGH);
    Serial.printf("Audio: MISO idle=%d (CS high), selected=%d (CS low) — expect 1,1 if card present\n",
                  miso_idle, miso_selected);

    // ── Init SPI exactly as working project: global SPI, CS=-1 ───────────────
    SPI.begin(PIN_TFT_SCK, PIN_SD_MISO, PIN_TFT_MOSI, -1);

    if (!SD.begin(PIN_SD_CS, SPI, 400000)) {
        Serial.println("Audio: SD card not found");
        sd_ok = false;
    } else {
        Serial.printf("Audio: SD OK — type:%d size:%lluMB\n",
                      SD.cardType(), SD.cardSize() / (1024 * 1024));
        sd_ok = true;
    }

    audio.setPinout(PIN_I2S_BCLK, PIN_I2S_LRCLK, PIN_I2S_DOUT);
    audio.setVolume(0);

    Serial.println("Audio: I2S OK");
    return sd_ok;
}

void audio_manager_loop() {
    audio.loop();
}

void audio_manager_play(const char *path) {
    if (!sd_ok) {
        audio_manager_play_beep();
        return;
    }
    audio.connecttoFS(SD, path);
    audio.setVolume(pct_to_lib(volume_pct));
    playing = true;
    Serial.printf("Audio: playing %s\n", path);
}

void audio_manager_play_beep() {
    if (sd_ok) {
        audio.connecttoFS(SD, "/beeps/1000hz.wav");
        audio.setVolume(pct_to_lib(volume_pct));
        playing = true;
        Serial.println("Audio: playing beep fallback");
    } else {
        Serial.println("Audio: no SD — beep skipped");
    }
}

void audio_manager_stop() {
    audio.stopSong();
    playing = false;
}

void audio_manager_set_volume(uint8_t pct) {
    volume_pct = pct > 100 ? 100 : pct;
    audio.setVolume(pct_to_lib(volume_pct));
}

uint8_t audio_manager_get_volume() {
    return volume_pct;
}

bool audio_manager_is_sd_ok()   { return sd_ok;   }
bool audio_manager_is_playing() { return playing;  }
