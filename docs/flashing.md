# Flashing

You need PlatformIO and a USB-C cable that does data. The module is 16 MB of flash and 8 MB of octal PSRAM. A generic "ESP32-S3 4 MB" environment will boot and then fall over.

## From the command line

```bash
pip install platformio
cd firmware
pio run -e esp32s3 -t upload
pio device monitor
```

The serial port is the USB CDC port, 115200. Hold BOOT while you plug it in if the upload never starts; let go once esptool has it.

V1 boards (chip-select on GPIO9, no touch interrupt):

```bash
pio run -e esp32s3-v1 -t upload
```

The first build downloads the toolchain. It is large, and it only happens once.

## From the simulator, before you own the board

```bash
cd firmware
pio run -e sim
./.pio/build/sim/program run      # a few scripted frames, then exits
./.pio/build/sim/program dump     # writes frames/frame_NNN.ppm
./.pio/build/sim/program sheet    # every shape, one frame
```

## Passwords

There aren't any. The radio is turned off in `setup()`. If you later make it join a network, copy `firmware/include/secrets.example.h` to `firmware/include/secrets.h` and fill that in. `secrets.h` is gitignored. Don't put the password in `bots.json` or in a source file.

## Changing the bots

Edit `assets/bots.json`. `shape` is one of circle, cloud, squircle, plus, peanut, blob, flower, drop, pill, triangle, house, star, hexagon. `color` is a name (white, brown, red, orange, amber, green, teal, blue, purple, pink, grey) or a hex like `#1E88FF`.

```bash
python3 tools/png_to_c.py
cd firmware && pio run -e esp32s3 -t upload
```

A PNG instead of a drawn mark: put the file under `assets/`, set `"image": "avatars/your.png"` (the path is relative to `assets/`), run the same script. The PNG is scaled into the mark and it still bobs. It does not grow eyes. Eyes are drawn on top of the procedural shapes only.

`python3 tools/png_to_c.py --check` fails if the generated C is stale. `--placeholders` rewrites the two stand-in PNGs, which the firmware does not use.

## If the glass looks wrong

In `firmware/platformio.ini`, on the `esp32s3` environment:

- Picture shoved to one side: `-DPB_LCD_GAP_X=20` is the default. Try another number.
- Red and blue swapped: `-DPB_LCD_BGR=1`.
- You would rather it didn't wake when a key brushes the glass: `-DPB_WAKE_ON_TOUCH=0`.
