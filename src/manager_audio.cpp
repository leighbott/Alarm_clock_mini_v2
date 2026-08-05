#include "manager_audio.h"
#include "pins_config.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Audio.h>
#include <ctype.h>
#include <string.h>

static Audio      audio;
static bool       sd_ok      = false;
static bool       playing    = false;
static uint8_t    volume_pct = 0;
static bool       loop_enabled = false;
static char       loop_path[64] = {0};
static bool       preview_active = false;
static char       preview_path[64] = {0};

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

static bool has_ext_ignore_case(const char *path, const char *ext) {
    if (!path || !ext) return false;
    const size_t path_len = strlen(path);
    const size_t ext_len = strlen(ext);
    if (path_len < ext_len || ext_len == 0) return false;

    const char *tail = path + (path_len - ext_len);
    for (size_t i = 0; i < ext_len; ++i) {
        if (tolower((unsigned char)tail[i]) != tolower((unsigned char)ext[i])) {
            return false;
        }
    }
    return true;
}

static void copy_name(char *dst, size_t dst_len, const char *src) {
    if (!dst || dst_len == 0) return;
    dst[0] = '\0';
    if (!src || src[0] == '\0') return;
    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

static void basename_from_path(const char *path, char *out, size_t out_len) {
    if (!out || out_len == 0) return;
    out[0] = '\0';
    if (!path || path[0] == '\0') return;

    const char *slash = strrchr(path, '/');
    const char *name = slash ? (slash + 1) : path;
    if (!name || name[0] == '\0') {
        copy_name(out, out_len, "/");
        return;
    }
    copy_name(out, out_len, name);
}

static void join_path(const char *dir_path, const char *entry_name, char *out, size_t out_len) {
    if (!out || out_len == 0) return;
    out[0] = '\0';
    if (!entry_name || entry_name[0] == '\0') return;

    if (entry_name[0] == '/') {
        copy_name(out, out_len, entry_name);
        return;
    }

    const char *base = (dir_path && dir_path[0]) ? dir_path : "/";
    if (strcmp(base, "/") == 0) {
        snprintf(out, out_len, "/%s", entry_name);
        return;
    }

    const size_t base_len = strlen(base);
    if (base[base_len - 1] == '/') {
        snprintf(out, out_len, "%s%s", base, entry_name);
    } else {
        snprintf(out, out_len, "%s/%s", base, entry_name);
    }
}

static void parent_path(const char *path, char *out, size_t out_len) {
    if (!out || out_len == 0) return;
    out[0] = '\0';

    if (!path || path[0] == '\0' || strcmp(path, "/") == 0) {
        copy_name(out, out_len, "/");
        return;
    }

    char tmp[64];
    copy_name(tmp, sizeof(tmp), path);
    size_t len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
        --len;
    }

    char *last = strrchr(tmp, '/');
    if (!last || last == tmp) {
        copy_name(out, out_len, "/");
        return;
    }

    *last = '\0';
    copy_name(out, out_len, tmp);
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

    if (loop_enabled && sd_ok && !playing && loop_path[0] != '\0') {
        audio.connecttoFS(SD, loop_path);
        audio.setVolume(pct_to_lib(volume_pct));
        playing = true;
    }
}

void audio_manager_play(const char *path) {
    loop_enabled = false;
    loop_path[0] = '\0';
    preview_active = false;
    preview_path[0] = '\0';

    if (!sd_ok) {
        audio_manager_play_beep();
        return;
    }
    audio.connecttoFS(SD, path);
    audio.setVolume(pct_to_lib(volume_pct));
    playing = true;
    Serial.printf("Audio: playing %s\n", path);
}

void audio_manager_play_loop(const char *path) {
    if (!path || path[0] == '\0') {
        audio_manager_play("/test.mp3");
        return;
    }

    strncpy(loop_path, path, sizeof(loop_path) - 1);
    loop_path[sizeof(loop_path) - 1] = '\0';
    loop_enabled = true;
    preview_active = false;
    preview_path[0] = '\0';

    if (!sd_ok) {
        audio_manager_play_beep();
        return;
    }

    if (!SD.exists(loop_path)) {
        Serial.printf("Audio: loop file missing: %s\n", loop_path);
        loop_enabled = false;
        loop_path[0] = '\0';
        audio_manager_play_beep();
        return;
    }

    audio.connecttoFS(SD, loop_path);
    audio.setVolume(pct_to_lib(volume_pct));
    playing = true;
    Serial.printf("Audio: looping %s\n", loop_path);
}

void audio_manager_play_beep() {
    loop_enabled = false;
    loop_path[0] = '\0';
    preview_active = false;
    preview_path[0] = '\0';

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
    loop_enabled = false;
    loop_path[0] = '\0';
    preview_active = false;
    preview_path[0] = '\0';
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

bool audio_manager_is_supported_tune(const char *path) {
    return has_ext_ignore_case(path, ".mp3") || has_ext_ignore_case(path, ".wav");
}

bool audio_manager_browse_dir(const char *dir_path,
                              AudioBrowserEntry *entries,
                              size_t max_entries,
                              size_t *out_count) {
    if (out_count) *out_count = 0;
    if (!entries || max_entries == 0 || !sd_ok) return false;

    const char *path = (dir_path && dir_path[0] != '\0') ? dir_path : "/";
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return false;
    }

    size_t count = 0;
    if (strcmp(path, "/") != 0 && count < max_entries) {
        AudioBrowserEntry &entry = entries[count++];
        copy_name(entry.name, sizeof(entry.name), "..");
        parent_path(path, entry.path, sizeof(entry.path));
        entry.is_directory = true;
        entry.is_playable = false;
    }

    File child = dir.openNextFile();
    while (child && count < max_entries) {
        AudioBrowserEntry &entry = entries[count];

        const char *child_name = child.name();
        char display_name[32];
        basename_from_path(child_name, display_name, sizeof(display_name));
        copy_name(entry.name, sizeof(entry.name), display_name);

        char full_path[64];
        join_path(path, child_name, full_path, sizeof(full_path));
        copy_name(entry.path, sizeof(entry.path), full_path);

        entry.is_directory = child.isDirectory();
        entry.is_playable = (!entry.is_directory) && audio_manager_is_supported_tune(entry.path);

        ++count;
        child.close();
        child = dir.openNextFile();
    }

    if (child) child.close();
    dir.close();

    if (out_count) *out_count = count;
    return true;
}

void audio_manager_toggle_preview(const char *path) {
    if (!path || !path[0] || !audio_manager_is_supported_tune(path)) return;

    if (preview_active && strcmp(preview_path, path) == 0 && playing) {
        audio.stopSong();
        playing = false;
        preview_active = false;
        preview_path[0] = '\0';
        return;
    }

    loop_enabled = false;
    loop_path[0] = '\0';

    if (!sd_ok || !SD.exists(path)) return;

    audio.connecttoFS(SD, path);
    audio.setVolume(pct_to_lib(volume_pct));
    playing = true;
    preview_active = true;
    copy_name(preview_path, sizeof(preview_path), path);
}

void audio_manager_stop_preview() {
    if (!preview_active) return;
    audio.stopSong();
    playing = false;
    preview_active = false;
    preview_path[0] = '\0';
}

bool audio_manager_is_previewing(const char *path) {
    if (!preview_active || !path || !path[0]) return false;
    if (!playing) return false;
    return strcmp(preview_path, path) == 0;
}
