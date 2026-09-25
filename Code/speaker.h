// =====================================================
// speaker.h
// =====================================================


// SD card layout:  /mp3/0001.mp3 ... /mp3/0104.mp3

#pragma once

#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>


// =====================================================
// CONFIG
// =====================================================

#define DF_RX_PIN        16      // ESP32 RX2  <- DFPlayer TX
#define DF_TX_PIN        17      // ESP32 TX2  -> DFPlayer RX (via 1k)

#define DF_VOLUME        30      // 0 - 30

#define DF_CMD_GAP_MS    40      // minimum time between two commands

#define DF_FINISH_MIN_MS      300
#define DF_FOLLOW_TIMEOUT_MS  2500


// =====================================================
// FEEDBACK TRACKS
// =====================================================

#define TRACK_KSSA          91   // ক্ষ
#define TRACK_JNYA          92   // জ্ঞ
#define TRACK_SPACE         93   // space
#define TRACK_BACKSPACE     94   // backspace
#define TRACK_NEWLINE       95   // new line      (ENTER hold)
#define TRACK_ALL_CLEARED   96   // all cleared   (BACKSPACE hold)
#define TRACK_WRONG         97   // wrong entry   (invalid pattern)
#define TRACK_CONJUNCT_1    98   // conjunct mode 1  (dot 4)
#define TRACK_CONJUNCT_2    99   // conjunct mode 2  (dots 46)
#define TRACK_DOT5         100   // dot 5            (ঋ / ৎ prefix)
#define TRACK_NUMBER_MODE  101   // number mode      (dots 45)

#define TRACK_DECIMAL      102   // দশমিক  (dots 256 inside number mode)
                                 // outside number mode 256 is । -> track 79

#define TRACK_READ_MODE    103   // "Reading mode"  (mode switch / power up)
#define TRACK_WRITE_MODE   104   // "Writing mode"  (mode switch / power up)


struct SpeakEntry
{
    const char *text;
    uint16_t    track;
};

const SpeakEntry SPEAK_TABLE[] =
{
    // ---------------- Vowels (1 - 11) ----------------
    { "অ",   1 }, { "আ",   2 }, { "ই",   3 }, { "ঈ",   4 },
    { "উ",   5 }, { "ঊ",   6 }, { "ঋ",   7 }, { "এ",   8 },
    { "ঐ",   9 }, { "ও",  10 }, { "ঔ",  11 },

    // ---------------- Consonants (12 - 50) ----------------
    { "ক",  12 }, { "খ",  13 }, { "গ",  14 }, { "ঘ",  15 },
    { "ঙ",  16 }, { "চ",  17 }, { "ছ",  18 }, { "জ",  19 },
    { "ঝ",  20 }, { "ঞ",  21 }, { "ট",  22 }, { "ঠ",  23 },
    { "ড",  24 }, { "ঢ",  25 }, { "ণ",  26 }, { "ত",  27 },
    { "থ",  28 }, { "দ",  29 }, { "ধ",  30 }, { "ন",  31 },
    { "প",  32 }, { "ফ",  33 }, { "ব",  34 }, { "ভ",  35 },
    { "ম",  36 }, { "য",  37 }, { "র",  38 }, { "ল",  39 },
    { "শ",  40 }, { "ষ",  41 }, { "স",  42 }, { "হ",  43 },

    { "ড়", 44 },      // ড়
    { "ঢ়", 45 },      // ঢ়
    { "য়", 46 },      // য়

    { "ৎ",  47 }, { "ং",  48 }, { "ঃ",  49 }, { "ঁ",  50 },

    // ---------------- Digits (51 - 60) ----------------
    { "০",  51 }, { "১",  52 }, { "২",  53 }, { "৩",  54 },
    { "৪",  55 }, { "৫",  56 }, { "৬",  57 }, { "৭",  58 },
    { "৮",  59 }, { "৯",  60 },

    // ---------------- Kar signs (61 - 70) ----------------
    { "া",  61 }, { "ি",  62 }, { "ী",  63 }, { "ু",  64 },
    { "ূ",  65 }, { "ে",  66 }, { "ৈ",  67 }, { "ো",  68 },
    { "ৌ",  69 }, { "ৃ",  70 },

    // ---------------- Punctuation ----------------
    { ",",  77 }, { ";",  78 }, { "।",  79 },
    { "?",  80 }, { "!",  81 }, { "-",  85 },

    // ---------------- Direct conjuncts ----------------
    { "ক্ষ", TRACK_KSSA },   // ক্ষ
    { "জ্ঞ", TRACK_JNYA },   // জ্ঞ

    // ---------------- Decimal point (number mode only) ----------------
    { ".", TRACK_DECIMAL },           // দশমিক
};

const uint8_t SPEAK_TABLE_SIZE =
    sizeof(SPEAK_TABLE) / sizeof(SPEAK_TABLE[0]);


// =====================================================
// INTERNAL STATE
// =====================================================

HardwareSerial dfSerial(2);
DFRobotDFPlayerMini dfPlayer;

bool speakerReady = false;

uint16_t      speakPending = 0;      // latest requested track
uint16_t      speakFollow  = 0;      // track to play after the current one
unsigned long speakLastCmd = 0;


// =====================================================
// LOOKUP
// =====================================================

uint16_t trackForText(const String &text)
{
    for (uint8_t i = 0; i < SPEAK_TABLE_SIZE; i++)
    {
        if (text == SPEAK_TABLE[i].text)
            return SPEAK_TABLE[i].track;
    }

    return 0;
}


// =====================================================
// REQUEST A SOUND
// =====================================================

void speakTrack(uint16_t track)
{
    if (!speakerReady || track == 0)
        return;

    speakPending = track;

    // A new sound cancels anything that was queued.
    speakFollow = 0;
}


void speakTrackNext(uint16_t track)
{
    if (!speakerReady || track == 0)
        return;

    speakFollow = track;
}


void speakText(const String &text)
{
    uint16_t track = trackForText(text);

    if (track == 0)
    {
        Serial.print("[speak] no audio for: ");
        Serial.println(text);
        return;
    }

    speakTrack(track);
}


void speakStop()
{
    speakPending = 0;
    speakFollow  = 0;

    if (speakerReady)
        dfPlayer.stop();
}


// =====================================================
// SERVICE  (call once per loop, never blocks)
// =====================================================

void speakerService()
{
    if (!speakerReady)
        return;

    // Read and discard status messages from the module
    // ("finished", errors) so the UART buffer never
    // fills up. Errors are printed for debugging.

    while (dfPlayer.available())
    {
        uint8_t type  = dfPlayer.readType();
        int     value = dfPlayer.read();

        if (type == DFPlayerPlayFinished)
        {
            // Current sound finished -> start the queued
            // one (ignore stale reports of older sounds).

            if (
                speakFollow != 0 &&
                speakPending == 0 &&
                millis() - speakLastCmd >= DF_FINISH_MIN_MS
            )
            {
                speakPending = speakFollow;
                speakFollow  = 0;
            }
        }
        else if (type == DFPlayerError)
        {
            Serial.print("[speak] DFPlayer error ");
            Serial.println(value);
        }
        else if (type == DFPlayerCardRemoved)
        {
            Serial.println("[speak] SD card removed");
        }
    }

    unsigned long now = millis();

    // Fallback for modules that never report "finished".

    if (
        speakFollow != 0 &&
        speakPending == 0 &&
        now - speakLastCmd >= DF_FOLLOW_TIMEOUT_MS
    )
    {
        speakPending = speakFollow;
        speakFollow  = 0;
    }

    if (speakPending == 0)
        return;

    if (now - speakLastCmd < DF_CMD_GAP_MS)
        return;

    // A new play command interrupts the current sound.
    dfPlayer.playMp3Folder(speakPending);

    speakPending = 0;
    speakLastCmd = now;
}


// =====================================================
// START
// =====================================================

void speakerBegin()
{
    dfSerial.begin(9600, SERIAL_8N1, DF_RX_PIN, DF_TX_PIN);
    delay(500);

    bool answered = dfPlayer.begin(dfSerial, true, true);

    // Switch to no-ACK, without another reset.
    dfPlayer.begin(dfSerial, false, false);

    dfPlayer.setTimeOut(300);

    delay(800);                          // let the SD card mount

    dfPlayer.volume(DF_VOLUME);
    delay(50);
    dfPlayer.EQ(DFPLAYER_EQ_NORMAL);
    delay(50);

    speakerReady = true;

    int files = dfPlayer.readFileCounts();

    Serial.print("[speak] DFPlayer ");
    Serial.print(answered ? "found" : "did not ACK (clone mode)");
    Serial.print(". Files on SD: ");

    if (files < 0)
        Serial.println("no reply");
    else
        Serial.println(files);

    if (files >= 0 && files < 104)
        Serial.println("[speak] Expected 104 files in /mp3.");
}
