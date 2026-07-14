"""
Pre-build script:
  1. Patches ESP32-audioI2S 3.0.8 Audio.h to add the missing #include <optional>.
  2. Stubs out LVGL's ARM Helium assembly file (not available on Xtensa/ESP32-S3).
"""
import os

Import("env")  # noqa: F821 — PlatformIO injects this

def patch_audio_h(source, target, env):  # noqa: F811
    libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
    env_name    = env.subst("$PIOENV")
    audio_h     = os.path.join(libdeps_dir, env_name, "ESP32-audioI2S-master", "src", "Audio.h")

    if not os.path.isfile(audio_h):
        print(f"patch_libs.py: Audio.h not found at {audio_h} — skipping patch")
        return

    with open(audio_h, "r") as f:
        content = f.read()

    needle      = "#include <locale>"
    replacement = "#include <locale>\n#include <optional>"

    if "#include <optional>" in content:
        print("patch_libs.py: Audio.h already patched — nothing to do")
        return

    if needle not in content:
        print(f"patch_libs.py: '{needle}' not found in Audio.h — skipping patch")
        return

    content = content.replace(needle, replacement, 1)

    with open(audio_h, "w") as f:
        f.write(content)

    print("patch_libs.py: Audio.h patched successfully")


def stub_lvgl_asm(source, target, env):  # noqa: F811
    """Replace LVGL ARM SIMD assembly files (Helium + NEON) with empty stubs.
    These files exist in all LVGL 9.x releases but cannot be assembled
    by the Xtensa toolchain used for ESP32."""
    libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
    env_name    = env.subst("$PIOENV")
    blend_base  = os.path.join(
        libdeps_dir, env_name,
        "lvgl", "src", "draw", "sw", "blend"
    )

    asm_files = [
        os.path.join(blend_base, "helium", "lv_blend_helium.S"),
        os.path.join(blend_base, "neon",   "lv_blend_neon.S"),
    ]

    for asm_file in asm_files:
        if not os.path.isfile(asm_file):
            print(f"patch_libs.py: {os.path.basename(asm_file)} not found — skipping")
            continue

        with open(asm_file, "r") as f:
            content = f.read()

        if "XTENSA_STUB" in content:
            print(f"patch_libs.py: {os.path.basename(asm_file)} already stubbed")
            continue

        with open(asm_file, "w") as f:
            f.write("/* XTENSA_STUB — ARM SIMD not available on Xtensa/ESP32-S3 */\n")

        print(f"patch_libs.py: {os.path.basename(asm_file)} stubbed successfully")


env.AddPreAction("buildprog", patch_audio_h)
env.AddPreAction("buildprog", stub_lvgl_asm)

# Also run immediately so patches are applied before the library is compiled
# (AddPreAction("buildprog") fires after compilation — too late for .S files)
patch_audio_h(None, None, env)
stub_lvgl_asm(None, None, env)
