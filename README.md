# Bidirectional Bangla Braille Translation System

An ESP32-based assistive device that translates **in both directions between six-dot Braille and Bangla text**.

- **WRITE mode (Braille → Bangla):** a blind user types Braille on a six-key keypad. The device shows the Bangla text on an OLED display and speaks each letter through a speaker.
- **READ mode (Bangla → Braille):** a sighted user types Bangla on a web page hosted by the ESP32. The device raises the Braille pattern one letter at a time on a refreshable cell of six solenoids.

EEE 416 (Microprocessor and Embedded Systems Laboratory), Jan 2026 — Section C2, Group 04
Department of EEE, Bangladesh University of Engineering and Technology (BUET)

>  Demo video: *add YouTube link*

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
