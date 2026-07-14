"""
Pre-build script: patches ESP32-audioI2S 3.0.8 Audio.h to add the missing
#include <optional> that the library requires but omits.
"""
import os

Import("env")  # noqa: F821 — PlatformIO injects this

def patch_audio_h(source, target, env):  # noqa: F811
    packages_dir = env.subst("$PROJECT_PACKAGES_DIR")
    audio_h = os.path.join(
        packages_dir,
        "toolchain-xtensa-esp-elf",  # walk up to find the lib
    )

    # Locate Audio.h inside the .pio/libdeps tree
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

env.AddPreAction("buildprog", patch_audio_h)
