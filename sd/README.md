# sd/ — microSD card staging tree

This directory mirrors the layout expected on the device's FAT32 microSD card
(roadmap §4.1). Copy the **contents** of this directory (not the `sd/`
directory itself) to the root of the microSD card, then insert it into the
device.

`oracle/wisdom.txt` holds the Oracle's entries — one per line, UTF-8,
edited freely (the list may grow or shrink). Stick to ASCII + Portuguese
accents; curly quotes render as missing glyphs on the device. The oracle
frame art goes in `art/oracle/frame.bin` (LVGL RGB565 .bin); until it
exists the app draws a styled placeholder box.
