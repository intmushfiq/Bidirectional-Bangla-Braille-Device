# Bidirectional Bangla Braille Translation System

An ESP32-based assistive device that translates in both directions between six-dot Braille and Bangla text.

WRITE mode (Braille → Bangla): a blind user types Braille on a six-key keypad. The device shows the Bangla text on an OLED display and speaks each letter through a speaker.
READ mode (Bangla → Braille): a sighted user types Bangla on a web page hosted by the ESP32. The device raises the Braille pattern one letter at a time on a refreshable cell of six solenoids.

EEE 416 (Microprocessor and Embedded Systems Laboratory), Jan 2026 — Section C2, Group 04 Department of EEE, Bangladesh University of Engineering and Technology (BUET)

Demo video: add YouTube link

How to use
READ mode — Bangla → Braille
Latch the MODE button.
On a phone or laptop, connect to Wi-Fi Braille_Bangla (password 123456789).
Open http://192.168.4.1, type Bangla text and press অনুবাদ করুন.
The OLED shows the text. Press ENTER / NEXT to raise the next Braille cell and BACKSPACE / PREVIOUS to go back.
Each raised cell is refreshed in a 2 s ON / 1 s OFF cycle to limit coil heating.
Up to 512 cells are stored.

If the text contains no Bangla, the OLED shows দয়া করে বাংলা লিখুন and all dots stay down.

WRITE mode — Braille → Bangla
Release the MODE button.
Hold the dot keys for a letter, then press ENTER. The letter appears on the OLED and is spoken.

Team
Name	Student ID
Mushfiqur Rahman	2106181
Chayan Paul	2106182
Shamin Yeaser	2106183
Kayes Sami Malitha	2106184
Credits and references
Audio clips: letter recordings from Bengali-Alphabet by Mahbub Zaman, used under the MIT License. See audio/LICENSE.
Braille chart: World Braille Usage, 3rd ed., Perkins, ICEB and National Library Service for the Blind and Physically Handicapped, Library of Congress, 2013, "Bangladesh," pp. 9–10.
Braille rules: M. S. Rahman, M. M. Rahman and M. Kaykobad, "Bangla Braille Adaptation," in Technical Challenges and Design Issues in Bangla Language Processing, IGI Global, 2013.
Libraries: BanglaText by Mamunul Mazid · U8g2 by Oli Kraus · DFRobotDFPlayerMini by DFRobot
