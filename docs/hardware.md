# Hardware

The board is a **Waveshare ESP32-S3-Touch-AMOLED-1.64**. Revision **V1.1** is the one the firmware expects. Waveshare's own examples for that revision are the files with `-v2` in the name. Keybuddy calls it V2. The silkscreen on the right of the module is the thing to read.

V1.1 is the one to buy if you have a choice. It adds reverse-polarity protection on the battery, a 500 mA charge current, a wired touch interrupt, and a tear pin. The case and the default firmware both assume it.

## Why this one

It is a keychain. The module outline is 29.12 × 44.00 mm, the PCB is 28.60 × 43.50 mm, and the glass shows 22.34 × 36.06 mm at 280 × 456. There is a 1.25 mm PicoBlade (MX1.25) socket for a single LiPo, USB-C, a BOOT button, a reset button, and a QMI8658. Nothing else you have to leave behind to make it pocketable.

The 1.8 inch board from the same family is easier to find (The Pi Hut stocks it) and it has a speaker, a microphone, an RTC and a PMIC. It is also a different shape. This case will not take it.

The Pi Hut does not list this exact ESP32-S3 1.64 module. They do list an RP2350 board in the same 1.64 inch glass, which is a different chip and will not run this firmware.

## What to buy

Prices move. Look them up; nothing below has one baked in.

| Thing | Notes | Where to look in the UK |
| --- | --- | --- |
| Waveshare ESP32-S3-Touch-AMOLED-1.64, V1.1 | ESP32-S3R8, 16 MB flash, 8 MB octal PSRAM. Leave the 2×11 2.54 mm headers off if it has to close. | Waveshare's shop, RobotShop EU, Amazon marketplace |
| 1S LiPo, PicoBlade / MX1.25 2-pin | The bay is 31 × 26 × 3.7 mm, leads toward the USB end. A 302535 sits in it. | The Pi Hut sells PicoBlade cells |
| PLA | The shell snaps. No screws, no inserts. A second colour makes the little side button easier to see. | Whoever you already buy filament from |
| USB-C cable that carries data | A charge-only cable will power it and refuse to flash it. | The drawer, then a shop |

A TPU reel is only for `enclosure/stl/bumper.stl`, which is a later skin, not part of the first build.

## Polarity

Read the silkscreen next to the battery socket before you plug anything in. V1.1 will survive a reversed cell. V1 will not. Red on the cell is a suggestion, not a standard.

## Pins

Nothing here needs a soldering iron. The battery plugs in. USB-C is the data and the charge path. BOOT (GPIO0, active low) is the button under the side plunger. Reset stays the button on the module; the shell leaves it inside.

| Signal | V1.1 | V1 |
| --- | --- | --- |
| LCD CS | GPIO46 | GPIO9 |
| LCD SCK | GPIO10 | GPIO10 |
| LCD D0–D3 | GPIO11–14 | GPIO11–14 |
| LCD RST | GPIO21 | GPIO21 |
| Touch and IMU I2C | SDA GPIO47, SCL GPIO48 | same |
| FT3168 address | 0x38 | 0x38 |
| Touch INT | GPIO18 | not connected |
| QMI8658 | 0x6B or 0x6A, WHO_AM_I 0x05 | same |
| IMU INT1 | GPIO9 | GPIO46 |
| Battery ADC | GPIO4, 3:1 divider | GPIO4 |
| BOOT | GPIO0 | GPIO0 |

The SD slot is unused (CS 38, MOSI 39, MISO 40, SCK 41). Wi-Fi and Bluetooth are switched off at boot. There is no speaker.

## If the picture is wrong

The panel is a CO5300 in quad SPI. A working port of this glass (trane77) needed the X window shifted by 20 pixels; that is the default (`PB_LCD_GAP_X`). If the image sits hard against one edge, change the gap. If red and blue are swapped, build with `-DPB_LCD_BGR=1`. Both are compile flags in `firmware/src/port_esp.cpp`.

V1 boards build with `pio run -e esp32s3-v1`. That moves chip-select back to GPIO9 and gives up touch-to-wake, because the touch interrupt is not wired.
