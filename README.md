# Bidirectional Bangla Braille Translation System

An ESP32-based assistive device that translates **in both directions between six-dot Braille and Bangla text**.

- **WRITE mode (Braille → Bangla):** a blind user types Braille on a six-key keypad. The device shows the Bangla text on an OLED display and speaks each letter through a speaker.
- **READ mode (Bangla → Braille):** a sighted user types Bangla on a web page hosted by the ESP32. The device raises the Braille pattern one letter at a time on a refreshable cell of six solenoids.

EEE 416 (Microprocessor and Embedded Systems Laboratory), Jan 2026 — Section C2, Group 04
Department of EEE, Bangladesh University of Engineering and Technology (BUET)

> 📺 Demo video: *add YouTube link*

---

## Features

- Two-way translation in one device: Braille → Bangla and Bangla → Braille
- Bangla Braille rules: kar signs, silent অ, two- and three-letter conjuncts (যুক্তাক্ষর), ক্ষ / জ্ঞ, ঋ / ৎ, digits, decimal point and punctuation
- Bangla text rendered on a 128 × 64 OLED with automatic word wrap
- Spoken feedback: 104 pre-recorded Bangla audio clips (every letter, digit and command)
- Web input: the ESP32 runs its own Wi-Fi access point and web page, so no app or router is needed
- Input check: text that is not Bangla shows a Bangla warning on the OLED and lowers all dots
- A latching MODE switch toggles READ / WRITE. Each mode keeps its own text and position.

---

## Hardware

| Part | Qty | Purpose |
|---|---|---|
| ESP32 DevKit (30-pin) | 1 | Main controller, Wi-Fi access point and web server |
| SH1106 1.3″ OLED, 128 × 64 (I²C) | 1 | Bangla text display |
| PCF8575 16-bit I/O expander (I²C, 0x20) | 1 | Reads all 9 push buttons |
| Push buttons | 6 latching + 3 others | 6 Braille dots, ENTER / NEXT, BACKSPACE / PREVIOUS, MODE |
| DFPlayer Mini + microSD card | 1 | Audio playback |
| 4 Ω 3 W speaker | 1 | Audio output |
| 5 V push-pull solenoids | 6 | Refreshable Braille cell (one per dot) |
| IRLB8721 logic-level N-MOSFET | 6 | Low-side solenoid drivers |
| 1N5819 Schottky diode | 6 | Flyback protection across each coil |
| 100 Ω resistor | 6 | MOSFET gate resistor |
| 10 kΩ resistor | 6 | Gate pull-down (dots stay down at boot) |
| 1 kΩ resistor | 1 | Series resistor on DFPlayer RX |
| LiPo battery + buck converter (5 V) | 1 | Solenoid supply |
| DC barrel jack (5 V) | 1 | ESP32 and DFPlayer supply |

---

## Wiring

### I²C bus (OLED + PCF8575)

| ESP32 | Signal |
|---|---|
| GPIO 21 | SDA |
| GPIO 22 | SCL |

### Push buttons (on the PCF8575)

All buttons are normally open and close to **GND**, so a pressed button reads **LOW**.

| PCF8575 pin | Button |
|---|---|
| P10 | Dot 1 |
| P11 | Dot 2 |
| P04 | Dot 3 |
| P07 | Dot 4 |
| P06 | Dot 5 |
| P05 | Dot 6 |
| P01 | ENTER (WRITE) / NEXT (READ) |
| P00 | BACKSPACE (WRITE) / PREVIOUS (READ) |
| P17 | MODE — latching: pressed = READ, released = WRITE |

### Solenoids

| ESP32 | Dot |
|---|---|
| GPIO 25 | Dot 1 |
| GPIO 33 | Dot 2 |
| GPIO 32 | Dot 3 |
| GPIO 14 | Dot 4 |
| GPIO 27 | Dot 5 |
| GPIO 26 | Dot 6 |

Each GPIO drives one identical low-side stage: GPIO → 100 Ω → IRLB8721 gate, with 10 kΩ from gate to GND. The solenoid sits between +5 V and the drain, and a 1N5819 across the coil has its cathode to +5 V.

### DFPlayer Mini

| ESP32 | DFPlayer |
|---|---|
| GPIO 17 (TX2) | RX, through a 1 kΩ resistor |
| GPIO 16 (RX2) | TX |

SPK_1 / SPK_2 go to the speaker. Never connect either speaker pin to GND. BUSY is unused.

> On ESP32-WROVER modules GPIO 16/17 are used by PSRAM. Change `DF_RX_PIN` / `DF_TX_PIN` in `speaker.h` to 27 / 26 in that case.

### Power

- **Solenoids:** 5 V from the LiPo battery through the buck converter
- **ESP32 and DFPlayer:** 5 V from the DC barrel jack
- All grounds are connected together (common ground)

---

## Software

### Files

```
Bangla_Braille_Merged/
├── Bangla_Braille_Merged.ino   # main firmware: WRITE mode, READ mode, mode switching
├── speaker.h                   # DFPlayer control and character → track table
├── WebPage.h                   # HTML page served by the ESP32
└── Kalpurush_20pt.h            # Bangla font used by BanglaText
```

### Libraries

Install these in the Arduino IDE (ESP32 board package by Espressif):

- [U8g2](https://github.com/olikraus/u8g2): OLED driver
- [BanglaText](https://github.com/mamunul/BanglaText): Bangla text shaping and rendering (install from ZIP)
- PCF8575: I²C I/O expander (the one providing `digitalReadAll()`)
- [DFRobotDFPlayerMini](https://github.com/DFRobot/DFRobotDFPlayerMini): audio
- `WiFi` and `WebServer`: included with the ESP32 core

### Upload

1. Open `Bangla_Braille_Merged.ino` in Arduino IDE 2.x.
2. Select **ESP32 Dev Module** and the correct port.
3. Upload. The serial monitor runs at **115200 baud**.

### Audio files

1. Format the microSD card as FAT32.
2. Create a folder named `mp3` in the root.
3. Copy the 104 clips as `0001.mp3` … `0104.mp3`.

| Tracks | Content |
|---|---|
| 0001–0011 | Vowels অ … ঔ |
| 0012–0050 | Consonants ক … হ, ড় ঢ় য়, ৎ ং ঃ ঁ |
| 0051–0060 | Digits ০ … ৯ |
| 0061–0070 | Kar signs |
| 0077–0085 | Punctuation |
| 0091–0104 | ক্ষ, জ্ঞ, space, backspace, new line, all cleared, wrong entry, conjunct / dot 5 / number / decimal prompts, "Reading mode", "Writing mode" |

The full mapping is in `SPEAK_TABLE` in `speaker.h`.

---

## How to use

### READ mode — Bangla → Braille

1. Latch the **MODE** button.
2. On a phone or laptop, connect to Wi-Fi **`Braille_Bangla`** (password `123456789`).
3. Open **http://192.168.4.1**, type Bangla text and press **অনুবাদ করুন**.
4. The OLED shows the text. Press **ENTER / NEXT** to raise the next Braille cell and **BACKSPACE / PREVIOUS** to go back.
   - Each raised cell is refreshed in a 2 s ON / 1 s OFF cycle to limit coil heating.
   - Up to 512 cells are stored.

If the text contains no Bangla, the OLED shows **দয়া করে বাংলা লিখুন** and all dots stay down.

### WRITE mode — Braille → Bangla

1. Release the **MODE** button.
2. Hold the dot keys for a letter, then press **ENTER**. The letter appears on the OLED and is spoken.

| Action | Result |
|---|---|
| ENTER with no dots | Space |
| Hold ENTER 2 s | New line |
| BACKSPACE | Delete last letter (ড়, ক্ষ … removed as one unit) |
| Hold BACKSPACE 2 s | Clear all text |

Special Braille sequences:

| Cells | Meaning |
|---|---|
| Vowel right after a consonant | Written as its kar sign (ম + আ → মা) |
| অ inside a word | Silent: keeps the next vowel independent (ব অ ই → বই) |
| Dot 4, then 2 consonants | Two-letter conjunct |
| Dots 4-6, then 3 consonants | Three-letter conjunct |
| Dot 5 + র / ত | ঋ / ৎ |
| Dots 4-5, then digit cells | Number (ends at a space or new line) |

---

## How it works

- **Braille cells as bitmasks:** each cell is one byte, and dot *d* sets bit *d − 1*. For example, ন (dots 1-3-4-5) = `0b011101` = 29.
- **WRITE mode:** the 6-bit pattern is checked for control cells (space, conjunct, ঋ / ৎ prefix, number) and otherwise looked up in `brailleToBangla()`. Composition rules then attach kar signs and build conjuncts before updating the OLED and audio.
- **READ mode:** web text is cleaned and validated (Bangla block U+0980–U+09FF), then decoded from UTF-8 (`readUTF8()`). An ordered rule dispatch (`translateBangla()`) turns it into a buffer of cell masks. NEXT and PREVIOUS only move the cell index.
- **Bangla on the OLED:** BanglaText shapes the text and draws it pixel by pixel through a callback that maps its coordinates onto the rotated OLED. Lines wrap by pixel width at 128 px.

---

## Braille standard

Letter patterns follow the Bangladesh (Bangla) Braille chart in *World Braille Usage*, 3rd ed. (Perkins / ICEB / Library of Congress, 2013), and the Bangla Braille rules described by Rahman, Rahman & Kaykobad (2013).

**Known deviation:** the firmware uses dots **4-5** as the number prefix, while *World Braille Usage* lists the number sign as dots **3-4-5-6**.

---

## Limitations

- Single Braille cell: text is read one character at a time
- Audio is limited to the pre-recorded clips (no text-to-speech)
- The web page works only on the ESP32's own Wi-Fi network
- Solenoids draw high current and warm up with long use
- The default Wi-Fi password should be changed before real-world use

---

## Team

| Name | Student ID |
|---|---|
| Mushfiqur Rahman | 2106181 |
| Chayan Paul | 2106182 |
| Shamin Yeaser | 2106183 |
| Kayes Sami Malitha | 2106184 |

---

## Credits and references

- **Audio clips:** letter recordings from [Bengali-Alphabet](https://github.com/lifeparticle/Bengali-Alphabet) by Mahbub Zaman, used under the MIT License. See `audio/LICENSE`.
- **Braille chart:** *World Braille Usage*, 3rd ed., Perkins, ICEB and National Library Service for the Blind and Physically Handicapped, Library of Congress, 2013, "Bangladesh," pp. 9–10.
- **Braille rules:** M. S. Rahman, M. M. Rahman and M. Kaykobad, "Bangla Braille Adaptation," in *Technical Challenges and Design Issues in Bangla Language Processing*, IGI Global, 2013.
- **Libraries:** [BanglaText](https://github.com/mamunul/BanglaText) by Mamunul Mazid · [U8g2](https://github.com/olikraus/u8g2) by Oli Kraus · [DFRobotDFPlayerMini](https://github.com/DFRobot/DFRobotDFPlayerMini) by DFRobot

---

## License

This project is released under the [MIT License](LICENSE). Third-party audio clips and libraries keep their own licenses.
