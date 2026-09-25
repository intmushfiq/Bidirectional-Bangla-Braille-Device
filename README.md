# Bidirectional Bangla Braille Translation System

An ESP32-based assistive device that translates **in both directions between six-dot Braille and Bangla text**.

- **WRITE mode (Braille → Bangla):** a blind user types Braille on a six-key keypad. The device shows the Bangla text on an OLED display and speaks each letter through a speaker.
- **READ mode (Bangla → Braille):** a sighted user types Bangla on a web page hosted by the ESP32. The device raises the Braille pattern one letter at a time on a refreshable cell of six solenoids.

EEE 416 (Microprocessor and Embedded Systems Laboratory), Jan 2026 — Section C2, Group 04
Department of EEE, Bangladesh University of Engineering and Technology (BUET)

>  Demo video: *add YouTube link*

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
