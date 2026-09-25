// =====================================================
// Bangla Braille Device 
// =====================================================

// Files needed in the same sketch folder: speaker.h, WebPage.h, Kalpurush_20pt.h

#include <Arduino.h>
#include <BanglaText.h>
#include "Kalpurush_20pt.h"
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PCF8575.h>
#include "speaker.h"                                   


// =====================================================
// OLED
// =====================================================
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(
    U8G2_R2,
    /* reset=*/ U8X8_PIN_NONE
);


// =====================================================
// PCF8575 I/O EXPANDER
// =====================================================
PCF8575 pcf(&Wire, 0x20);


// =====================================================
// Wi-Fi Access Point
// =====================================================
const char* ssid = "Braille_Bangla";
const char* password = "123456789";

WebServer server(80);

// =====================================================
// Webpage header file
// =====================================================
#include "WebPage.h"


// =====================================================
// BANGLA FONT  (shared by both modes)
// =====================================================
BTfont font;
BanglaTextRenderer *textRenderer;

// =====================================================
// MODE SELECTION                                
// =====================================================

#define MODE_BIT 15          // P17               

#define MODE_DEBOUNCE 50     // ms

// false = WRITE mode (Braille -> Bangla keyboard)
// true  = READ  mode (Bangla -> Braille solenoids)
bool readMode = false;                          

// =====================================================
// POWER-UP TITLE                                
// =====================================================
#define DEVICE_TITLE "বাংলা ব্রেইল অনুবাদক যন্ত্র"

// false until something has been written in WRITE mode
bool writeStarted = false;                       

// READ mode text renderer (defined in the READ section)
void displayReadText(String text);               


// =====================================================
//
//          WRITE MODE  (Braille -> Bangla)
//
// =====================================================

// =====================================================
// PINS  (PCF8575 bit indices)
// =====================================================
#define DOT1_BIT 8      // P10
#define DOT2_BIT 9      // P11
#define DOT3_BIT 4      // P04
#define DOT4_BIT 7      // P07
#define DOT5_BIT 6      // P06
#define DOT6_BIT 5      // P05

#define ENTER_BIT 1     // P01
#define BACKSPACE_BIT 0 // P00

#define DEBOUNCE_DELAY 40

#define ENTER_REPEAT_GUARD 180
#define ENTER_HOLD_TIME 2000


// =====================================================
// CONTROL PATTERNS
// =====================================================
#define PATTERN_SPACE          0     // no dots
#define PATTERN_CONJUNCT2      8     // (4)
#define PATTERN_SPECIAL_VOWEL  16    // (5)
#define PATTERN_NUMBER         24    // (45)
#define PATTERN_CONJUNCT3      40    // (46)


// =====================================================
// MULTI-CODE-POINT LETTERS
// =====================================================
#define BN_HASANTA "\u09CD"
#define BN_NUKTA   "\u09BC"
#define BN_ZWJ     "\u200D"              // zero width joiner
#define BN_RA      "\u09B0"              // র
#define BN_RRA     "\u09A1\u09BC"        // ড়
#define BN_RHA     "\u09A2\u09BC"        // ঢ়
#define BN_YYA     "\u09AF\u09BC"        // য়
#define BN_KSSA    "\u0995\u09CD\u09B7"  // ক্ষ
#define BN_JNYA    "\u099C\u09CD\u099E"  // জ্ঞ


// =====================================================
// DISPLAY LAYOUT
// =====================================================
#define LINE_HEIGHT 21
#define MAX_LINES 3

// Usable width for text, in pixels.
// A line wider than this wraps automatically.
#define DISPLAY_WIDTH 128
#define MAX_WRAPPED_LINES 40


// =====================================================
// OUTPUT TEXT
// =====================================================

String outputText = "";


// =====================================================
// SPECIAL অ STATE
// =====================================================

bool suppressedA = false;


// =====================================================
// SPECIAL ঋ / ৎ PREFIX STATE
// =====================================================
bool pendingSpecialVowel = false;


// =====================================================
// NUMBER MODE STATE
// =====================================================
bool numberMode = false;
size_t numberStartLength = 0;

// True right after a SPACE ended number mode, until the
// next action.
bool numberEndedBySpace = false;


// =====================================================
// CONJUNCT STATE
// =====================================================
uint8_t pendingConjunct = 0;

String conjunctBuffer = "";

bool conjunctZWJ = false;


// =====================================================
// BUTTON SNAPSHOT
// =====================================================
uint16_t buttonState = 0xFFFF;


// =====================================================
// ENTER ACTION
// =====================================================
enum EnterAction
{
    ENTER_ACTION_NONE,
    ENTER_ACTION_NEWLINE,
    ENTER_ACTION_SPACE,
    ENTER_ACTION_PATTERN
};


// =====================================================
// FUNCTION DECLARATIONS (WRITE mode)
// =====================================================

void draw_pixel(int16_t x, int16_t y);

uint16_t readExpander();
bool isDown(uint8_t bit);

uint8_t readBraille();
void printDots(uint8_t pattern);

String brailleToBangla(uint8_t pattern);
String brailleNumberToString(uint8_t pattern);
bool isPunctuation(const String &text);

uint16_t textWidth(const String &text);
uint32_t codePointAt(const String &text, int i, int &len);
int nextClusterEnd(const String &text, int start);
uint8_t buildDisplayLines(const String &text, String *lines, uint8_t maxLines);
void renderLines(const String &text);
void displayText();
void displayConjunctPreview();
void addNewLine();

bool isBanglaVowel(const String &text);
bool isBanglaConsonant(const String &text);

String vowelToKar(const String &vowel);

int prevCodePointStart(const String &text, int end);
int lastUnitStart(const String &text);
int nextUnitEnd(const String &text, int start);
uint8_t countUnits(const String &text);

String getLastCharacter(const String &text);
void removeLastCharacter();

bool isWordStart();

void processCharacter(const String &character);
void processNormalCharacter(const String &character);
void processNumber(uint8_t pattern);

void processA();
void addVowel(const String &vowel);
void addNonVowel(const String &character);

void startNumberMode();
void endNumberMode();

void startConjunct(uint8_t count);
void processConjunctCharacter(const String &character);
void finishConjunct();
void cancelConjunct();

void backspaceText();
void clearAllText();
void handleBackspaceButton();

EnterAction handleEnterButton(uint8_t &pattern);

void writeModeLoop();                             


// =====================================================
// PIXEL CALLBACK (WRITE mode)
// =====================================================

// Vertical offset used when rendering multiple display lines.
int16_t displayLineOffset = 0;

void draw_pixel(int16_t x, int16_t y)
{
    u8g2.drawPixel(y, 17 - x + displayLineOffset);
}


// =====================================================
// READ EXPANDER
// =====================================================
uint16_t readExpander()
{
    PCF8575::DigitalInput in = pcf.digitalReadAll();

    uint16_t w = 0;

    if (in.p0)  w |= (1 << 0);
    if (in.p1)  w |= (1 << 1);
    if (in.p2)  w |= (1 << 2);
    if (in.p3)  w |= (1 << 3);
    if (in.p4)  w |= (1 << 4);
    if (in.p5)  w |= (1 << 5);
    if (in.p6)  w |= (1 << 6);
    if (in.p7)  w |= (1 << 7);

    if (in.p8)  w |= (1 << 8);    // silkscreen P10
    if (in.p9)  w |= (1 << 9);    // silkscreen P11
    if (in.p10) w |= (1 << 10);   // silkscreen P12
    if (in.p11) w |= (1 << 11);   // silkscreen P13
    if (in.p12) w |= (1 << 12);   // silkscreen P14
    if (in.p13) w |= (1 << 13);   // silkscreen P15
    if (in.p14) w |= (1 << 14);   // silkscreen P16
    if (in.p15) w |= (1 << 15);   // silkscreen P17

    return w;
}


// =====================================================
// BUTTON TEST
// =====================================================
bool isDown(uint8_t bit)
{
    return !(buttonState & (1 << bit));
}


// =====================================================
// READ BRAILLE
// =====================================================

uint8_t readBraille()
{
    uint8_t pattern = 0;

    if (isDown(DOT1_BIT)) pattern |= (1 << 0);
    if (isDown(DOT2_BIT)) pattern |= (1 << 1);
    if (isDown(DOT3_BIT)) pattern |= (1 << 2);
    if (isDown(DOT4_BIT)) pattern |= (1 << 3);
    if (isDown(DOT5_BIT)) pattern |= (1 << 4);
    if (isDown(DOT6_BIT)) pattern |= (1 << 5);

    return pattern;
}


// =====================================================
// PRINT DOTS
// =====================================================

void printDots(uint8_t pattern)
{
    Serial.print("Dots: ");

    bool any = false;

    for (int i = 0; i < 6; i++)
    {
        if (pattern & (1 << i))
        {
            Serial.print(i + 1);
            Serial.print(" ");
            any = true;
        }
    }

    if (!any)
        Serial.print("NONE");

    Serial.println();
}


// =====================================================
// BRAILLE -> BANGLA
// =====================================================
String brailleToBangla(uint8_t p)
{
    // -----------------------------------------------------
    // PUNCTUATION
    // -----------------------------------------------------
    if (p == 2)  return ",";       // 2
    if (p == 6)  return ";";       // 23
    if (p == 50) return "।";       // 256
    if (p == 38) return "?";       // 236
    if (p == 22) return "!";       // 235
    if (p == 36) return "-";       // 36


    // -----------------------------------------------------
    // VOWELS
    // -----------------------------------------------------

    if (p == 1)  return "অ";       // 1
    if (p == 28) return "আ";       // 345
    if (p == 10) return "ই";       // 24
    if (p == 20) return "ঈ";       // 35
    if (p == 37) return "উ";       // 136
    if (p == 51) return "ঊ";       // 1256
    if (p == 17) return "এ";       // 15
    if (p == 12) return "ঐ";       // 34
    if (p == 21) return "ও";       // 135
    if (p == 42) return "ঔ";       // 246


    // -----------------------------------------------------
    // CONSONANTS
    // -----------------------------------------------------

    if (p == 5)  return "ক";       // 13
    if (p == 45) return "খ";       // 1346
    if (p == 27) return "গ";       // 1245
    if (p == 35) return "ঘ";       // 126
    if (p == 44) return "ঙ";       // 346

    if (p == 9)  return "চ";       // 14
    if (p == 33) return "ছ";       // 16
    if (p == 26) return "জ";       // 245
    if (p == 53) return "ঝ";       // 1356
    if (p == 18) return "ঞ";       // 25

    if (p == 62) return "ট";       // 23456
    if (p == 58) return "ঠ";       // 2456
    if (p == 43) return "ড";       // 1246
    if (p == 63) return "ঢ";       // 123456
    if (p == 60) return "ণ";       // 3456

    if (p == 30) return "ত";       // 2345
    if (p == 57) return "থ";       // 1456
    if (p == 25) return "দ";       // 145
    if (p == 46) return "ধ";       // 2346
    if (p == 29) return "ন";       // 1345

    if (p == 15) return "প";       // 1234
    if (p == 11) return "ফ";       // 124
    if (p == 3)  return "ব";       // 12
    if (p == 39) return "ভ";       // 1236
    if (p == 13) return "ম";       // 134

    if (p == 61) return "য";       // 13456
    if (p == 23) return "র";       // 1235
    if (p == 7)  return "ল";       // 123

    if (p == 41) return "শ";       // 146
    if (p == 47) return "ষ";       // 12346
    if (p == 14) return "স";       // 234
    if (p == 19) return "হ";       // 125

    if (p == 59) return BN_RRA;    // 12456  ড়
    if (p == 55) return BN_RHA;    // 12356  ঢ়
    if (p == 34) return BN_YYA;    // 26     য়


    // -----------------------------------------------------
    // SPECIAL DIRECT CONJUNCT CHARACTERS
    // -----------------------------------------------------

    if (p == 31) return BN_KSSA;   // 12345  ক্ষ
    if (p == 49) return BN_JNYA;   // 156    জ্ঞ


    // -----------------------------------------------------
    // SPECIAL SIGNS
    // -----------------------------------------------------

    if (p == 48) return "ং";       // 56
    if (p == 32) return "ঃ";       // 6
    if (p == 4)  return "ঁ";       // 3


    return "";
}


// =====================================================
// BRAILLE NUMBER DIGIT
// =====================================================

// Number prefix: 45
String brailleNumberToString(uint8_t p)
{
    if (p == 1)  return "১";       // 1
    if (p == 3)  return "২";       // 12
    if (p == 9)  return "৩";       // 14
    if (p == 25) return "৪";       // 145
    if (p == 17) return "৫";       // 15
    if (p == 11) return "৬";       // 124
    if (p == 27) return "৭";       // 1245
    if (p == 19) return "৮";       // 125
    if (p == 10) return "৯";       // 24
    if (p == 26) return "০";       // 245

    return "";
}


// =====================================================
// PUNCTUATION CHECK (WRITE mode, String version)
// =====================================================

bool isPunctuation(const String &text)
{
    return (
        text == "," ||
        text == ";" ||
        text == "।" ||
        text == "?" ||
        text == "!" ||
        text == "-"
    );
}


// =====================================================
// LETTER UNITS (UTF-8)
// =====================================================

// Start index of the UTF-8 code point that ends at 'end'.
int prevCodePointStart(const String &text, int end)
{
    int i = end - 1;

    while (i > 0 && ((uint8_t)text[i] & 0xC0) == 0x80)
        i--;

    return i;
}


// Start index of the last letter unit in text.
int lastUnitStart(const String &text)
{
    int len = text.length();

    if (len == 0)
        return 0;

    if (text.endsWith(BN_KSSA) || text.endsWith(BN_JNYA))
    {
        int s = len - 9;

        bool partOfLonger =
            s >= 3 && text.substring(s - 3, s) == BN_HASANTA;

        if (!partOfLonger)
            return s;
    }

    int i = prevCodePointStart(text, len);

    // Nukta belongs to the letter before it.
    if (i > 0 && text.substring(i) == BN_NUKTA)
        i = prevCodePointStart(text, i);

    // A hasanta after ZWJ (from র‍্য) is removed together
    if (
        i >= 3 &&
        text.substring(i) == BN_HASANTA &&
        text.substring(i - 3, i) == BN_ZWJ
    )
        i -= 3;

    return i;
}

int nextUnitEnd(const String &text, int start)
{
    int n = text.length();

    int i = start + 1;

    while (i < n && ((uint8_t)text[i] & 0xC0) == 0x80)
        i++;

    if (i < n && text.substring(i, i + 3) == BN_NUKTA)
        i += 3;

    return i;
}


// ড় counts as ONE consonant).
uint8_t countUnits(const String &text)
{
    uint8_t count = 0;

    for (int i = 0; i < (int)text.length(); )
    {
        i = nextUnitEnd(text, i);
        count++;
    }

    return count;
}


// =====================================================
// LAST CHARACTER
// =====================================================

String getLastCharacter(const String &text)
{
    if (text.length() == 0)
        return "";

    return text.substring(lastUnitStart(text));
}


// =====================================================
// REMOVE LAST CHARACTER
// =====================================================

void removeLastCharacter()
{
    if (outputText.length() == 0)
        return;

    outputText.remove(lastUnitStart(outputText));


    // Dangling reph cleanup.

    if (outputText.endsWith(BN_RA BN_ZWJ BN_HASANTA))
    {
        // remove ZWJ (3 bytes) + hasanta (3 bytes)
        outputText.remove(outputText.length() - 6);
    }
    else if (outputText.endsWith(BN_RA BN_HASANTA))
    {
        // remove hasanta (3 bytes)
        outputText.remove(outputText.length() - 3);
    }
}


// =====================================================
// BANGLA VOWEL CHECK
// =====================================================

bool isBanglaVowel(const String &text)
{
    return (
        text == "অ" ||
        text == "আ" ||
        text == "ই" ||
        text == "ঈ" ||
        text == "উ" ||
        text == "ঊ" ||
        text == "ঋ" ||
        text == "এ" ||
        text == "ঐ" ||
        text == "ও" ||
        text == "ঔ"
    );
}


// =====================================================
// BANGLA CONSONANT CHECK
// =====================================================

bool isBanglaConsonant(const String &text)
{
    return (
        text == "ক" ||
        text == "খ" ||
        text == "গ" ||
        text == "ঘ" ||
        text == "ঙ" ||

        text == "চ" ||
        text == "ছ" ||
        text == "জ" ||
        text == "ঝ" ||
        text == "ঞ" ||

        text == "ট" ||
        text == "ঠ" ||
        text == "ড" ||
        text == "ঢ" ||
        text == "ণ" ||

        text == "ত" ||
        text == "থ" ||
        text == "দ" ||
        text == "ধ" ||
        text == "ন" ||

        text == "প" ||
        text == "ফ" ||
        text == "ব" ||
        text == "ভ" ||
        text == "ম" ||

        text == "য" ||
        text == "র" ||
        text == "ল" ||

        text == "শ" ||
        text == "ষ" ||
        text == "স" ||
        text == "হ" ||

        text == BN_RRA ||
        text == BN_RHA ||
        text == BN_YYA
    );
}


// =====================================================
// VOWEL -> KAR
// =====================================================

String vowelToKar(const String &vowel)
{
    if (vowel == "আ") return "া";
    if (vowel == "ই") return "ি";
    if (vowel == "ঈ") return "ী";
    if (vowel == "উ") return "ু";
    if (vowel == "ঊ") return "ূ";
    if (vowel == "ঋ") return "ৃ";
    if (vowel == "এ") return "ে";
    if (vowel == "ঐ") return "ৈ";
    if (vowel == "ও") return "ো";
    if (vowel == "ঔ") return "ৌ";

    return "";
}


// =====================================================
// WORD START
// =====================================================

bool isWordStart()
{
    if (outputText.length() == 0)
        return true;

    String last = getLastCharacter(outputText);

    // A new word starts after a space, a NEW LINE or
    // any punctuation mark (with or without a space):
    //
    //   আমি,অজয়  ->  অ is shown, not suppressed

    return (
        last == " "  ||
        last == "\n" ||
        isPunctuation(last)
    );
}


// =====================================================
// TEXT WIDTH (pixels)
// =====================================================

uint16_t textWidth(const String &text)
{
    if (text.length() == 0)
        return 0;

    BTTextSize size =
        textRenderer->getTextSize(text.c_str());

    if (size.width < 0)
        return 0;

    return size.width;
}


// =====================================================
// UTF-8 CODE POINT AT POSITION
// =====================================================

uint32_t codePointAt(const String &text, int i, int &len)
{
    uint8_t b1 = (uint8_t)text[i];

    if (b1 < 0x80)
    {
        len = 1;
        return b1;
    }

    if ((b1 & 0xE0) == 0xC0 && i + 1 < (int)text.length())
    {
        len = 2;
        return ((uint32_t)(b1 & 0x1F) << 6) |
               ((uint8_t)text[i + 1] & 0x3F);
    }

    if ((b1 & 0xF0) == 0xE0 && i + 2 < (int)text.length())
    {
        len = 3;
        return ((uint32_t)(b1 & 0x0F) << 12) |
               ((uint32_t)((uint8_t)text[i + 1] & 0x3F) << 6) |
               ((uint8_t)text[i + 2] & 0x3F);
    }

    len = 1;
    return b1;
}


// =====================================================
// NEXT CLUSTER END
// =====================================================

bool isAttachedSign(uint32_t c)
{
    return (
        (c >= 0x0981 && c <= 0x0983) ||   // ঁ ং ঃ
        c == 0x09BC ||                    // nukta
        (c >= 0x09BE && c <= 0x09CC) ||   // kar signs
        c == 0x09CD ||                    // hasanta
        c == 0x09D7 ||                    // au length mark
        c == 0x200C ||                    // ZWNJ
        c == 0x200D                       // ZWJ
    );
}


int nextClusterEnd(const String &text, int start)
{
    int len = 0;

    codePointAt(text, start, len);

    int i = start + len;

    bool joinNext = false;

    while (i < (int)text.length())
    {
        uint32_t next = codePointAt(text, i, len);

        if (joinNext || isAttachedSign(next))
        {
            // After hasanta / ZWJ / ZWNJ the following
            // letter also belongs to this cluster.

            joinNext = (
                next == 0x09CD ||
                next == 0x200C ||
                next == 0x200D
            );

            i += len;
        }
        else
        {
            break;
        }
    }

    return i;
}


// =====================================================
// BUILD DISPLAY LINES (word wrap)
// =====================================================

void storeLine(String *lines, uint8_t maxLines, uint8_t &total, const String &line)
{
    lines[total % maxLines] = line;

    if (total < 255)
        total++;
}


uint8_t buildDisplayLines(const String &text, String *lines, uint8_t maxLines)
{
    uint8_t total = 0;

    int lineStart = 0;

    while (true)
    {
        int newlinePos = text.indexOf('\n', lineStart);

        String hardLine =
            (newlinePos == -1)
                ? text.substring(lineStart)
                : text.substring(lineStart, newlinePos);


        // ---------------------------------------------
        // Word wrap one hard line
        // ---------------------------------------------

        String current = "";

        int start = 0;

        while (start < (int)hardLine.length())
        {
            // Skip spaces

            while (
                start < (int)hardLine.length() &&
                hardLine.charAt(start) == ' '
            )
            {
                start++;
            }

            if (start >= (int)hardLine.length())
                break;


            // Next word

            int spacePos = hardLine.indexOf(' ', start);

            String word =
                (spacePos == -1)
                    ? hardLine.substring(start)
                    : hardLine.substring(start, spacePos);

            start =
                (spacePos == -1)
                    ? hardLine.length()
                    : spacePos + 1;


            // ---------------------------------------------
            // Split the word into segments that end after punctuation
            // ---------------------------------------------

            int segStart = 0;

            bool firstSegment = true;

            while (segStart < (int)word.length())
            {
                int segEnd = segStart;

                while (segEnd < (int)word.length())
                {
                    int len = 0;

                    codePointAt(word, segEnd, len);

                    String ch = word.substring(segEnd, segEnd + len);

                    segEnd += len;

                    if (isPunctuation(ch))
                    {
                        // Keep a run of punctuation together.

                        bool nextIsPunct = false;

                        if (segEnd < (int)word.length())
                        {
                            int len2 = 0;

                            codePointAt(word, segEnd, len2);

                            nextIsPunct = isPunctuation(
                                word.substring(segEnd, segEnd + len2)
                            );
                        }

                        if (!nextIsPunct)
                            break;
                    }
                }

                String segment = word.substring(segStart, segEnd);

                segStart = segEnd;


                // A space goes before the first segment of a word only

                String glue =
                    (firstSegment && current.length() > 0)
                        ? " "
                        : "";

                firstSegment = false;


                // Does it fit on the current line?

                String testLine = current + glue + segment;

                if (textWidth(testLine) <= DISPLAY_WIDTH)
                {
                    current = testLine;
                    continue;
                }


                // Does not fit: move the whole segment down.

                if (current.length() > 0)
                {
                    storeLine(lines, maxLines, total, current);

                    current = "";
                }


                if (textWidth(segment) <= DISPLAY_WIDTH)
                {
                    current = segment;
                    continue;
                }


                // Segment alone is wider than the display:
                // break it at cluster boundaries.

                String piece = "";

                int i = 0;

                while (i < (int)segment.length())
                {
                    int end = nextClusterEnd(segment, i);

                    String cluster = segment.substring(i, end);

                    if (
                        piece.length() > 0 &&
                        textWidth(piece + cluster) > DISPLAY_WIDTH
                    )
                    {
                        storeLine(lines, maxLines, total, piece);

                        piece = "";
                    }

                    piece += cluster;

                    i = end;
                }

                current = piece;
            }
        }

        storeLine(lines, maxLines, total, current);


        if (newlinePos == -1)
            break;

        lineStart = newlinePos + 1;
    }

    return total;
}


// =====================================================
// RENDER LINES
// =====================================================

void renderLines(const String &text)
{
    // -------------------------------------------------
    // Power-up title                   
    // -------------------------------------------------

    if (!writeStarted)
    {
        if (text.length() == 0)
        {
            displayReadText(DEVICE_TITLE);
            return;
        }

        writeStarted = true;
    }


    String lines[MAX_LINES];

    uint8_t total =
        buildDisplayLines(text, lines, MAX_LINES);


    uint8_t shown =
        (total < MAX_LINES) ? total : MAX_LINES;

    uint8_t first = total - shown;


    u8g2.clearBuffer();

    for (uint8_t n = 0; n < shown; n++)
    {
        const String &line =
            lines[(first + n) % MAX_LINES];

        if (line.length() == 0)
            continue;

        displayLineOffset = n * LINE_HEIGHT;

        textRenderer->renderText(
            line.c_str(),
            draw_pixel
        );
    }

    displayLineOffset = 0;

    u8g2.sendBuffer();
}


// =====================================================
// DISPLAY NORMAL TEXT (WRITE mode)
// =====================================================

void displayText()
{
    renderLines(outputText);
}


// =====================================================
// DISPLAY CONJUNCT PREVIEW
// =====================================================
void displayConjunctPreview()
{
    String preview = outputText;

    uint8_t count = 0;

    for (int i = 0; i < (int)conjunctBuffer.length(); )
    {
        int start = i;

        i = nextUnitEnd(conjunctBuffer, i);

        String consonant =
            conjunctBuffer.substring(start, i);

        // Add hoshonto (with ZWJ in ZWJ mode) before
        // every consonant after the first.

        if (count > 0)
        {
            if (conjunctZWJ)
                preview += BN_ZWJ;

            preview += BN_HASANTA;
        }

        preview += consonant;

        count++;
    }

    renderLines(preview);
}


// =====================================================
// ADD NEW LINE
// =====================================================
//
// ENTER held for >= 2 seconds creates a newline.
//
// =====================================================

void addNewLine()
{
    // Do not create multiple empty lines from one long hold.
    if (
        outputText.length() > 0 &&
        outputText[outputText.length() - 1] == '\n'
    )
    {
        Serial.println("New line already active.");
        return;
    }

    outputText += '\n';

    suppressedA = false;

    displayText();

    Serial.println("ENTER held 2 seconds -> NEW LINE");
}


// =====================================================
// NUMBER MODE
// =====================================================

void startNumberMode()
{
    // A numeric prefix cancels any pending letter-level

    if (pendingConjunct != 0)
    {
        cancelConjunct();

        Serial.println(
            "Conjunct cancelled by number prefix."
        );
    }

    pendingSpecialVowel = false;
    suppressedA = false;

    numberMode = true;
    numberStartLength = outputText.length();

    Serial.println("45 -> NUMBER MODE started.");
}


void endNumberMode()
{
    if (!numberMode)
        return;

    numberMode = false;
    numberStartLength = 0;

    Serial.println("NUMBER MODE ended.");
}


// =====================================================
// PROCESS NUMBER
// =====================================================

void processNumber(uint8_t pattern)
{
    // -----------------------------------------------------
    // 256 inside number mode = decimal point (দশমিক)
    //
    //   45 1 256 5  ->  ১.৫
    //   45 256 5    ->  .৫
    // -----------------------------------------------------

    if (pattern == 50)             // 256
    {
        outputText += ".";

        displayText();

        speakText(".");    // <<< SPEAKER

        Serial.println("Decimal point: .");

        return;
    }


    String digit = brailleNumberToString(pattern);

    if (digit.length() > 0)
    {
        outputText += digit;

        displayText();

        speakText(digit);    // <<< SPEAKER

        Serial.print("Number: ");
        Serial.println(digit);

        return;
    }


    String punctuation = brailleToBangla(pattern);

    if (isPunctuation(punctuation))
    {
        outputText += punctuation;

        displayText();

        speakText(punctuation);    // <<< SPEAKER

        Serial.print("Punctuation: ");
        Serial.println(punctuation);

        return;
    }


    Serial.println(
        "Invalid number-mode pattern. Ignored."
    );

    speakTrack(TRACK_WRONG);    // <<< SPEAKER
}


// =====================================================
// PROCESS CHARACTER
// =====================================================

void processCharacter(const String &character)
{
    // -----------------------------------------------------
    // Special ঋ / ৎ prefix
    // -----------------------------------------------------

    if (pendingSpecialVowel)
    {
        pendingSpecialVowel = false;

        if (character == "র")
        {
            processNormalCharacter("ঋ");
            return;
        }

        if (character == "ত")
        {
            processNormalCharacter("ৎ");
            return;
        }
    }

    // -----------------------------------------------------
    // Conjunct mode
    // -----------------------------------------------------

    if (pendingConjunct != 0)
    {
        processConjunctCharacter(character);
        return;
    }


    // -----------------------------------------------------
    // Normal mode
    // -----------------------------------------------------

    processNormalCharacter(character);
}


// =====================================================
// NORMAL CHARACTER PROCESSING
// =====================================================

void processNormalCharacter(const String &character)
{
    // অ

    if (character == "অ")
    {
        processA();
        return;
    }


    // Vowel

    if (isBanglaVowel(character))
    {
        addVowel(character);
        return;
    }


    // Space

    if (character == " ")
    {
        suppressedA = false;

        outputText += " ";

        displayText();

        return;
    }


    // Consonant / other

    addNonVowel(character);
}


// =====================================================
// SPECIAL অ PROCESSING
// =====================================================

void processA()
{
    speakText("অ");    

    if (isWordStart())
    {
        outputText += "অ";

        suppressedA = false;

        Serial.println("অ displayed.");

        displayText();
    }
    else
    {
        suppressedA = true;

        Serial.println("অ suppressed.");
    }
}


// =====================================================
// ADD VOWEL
// =====================================================

void addVowel(const String &vowel)
{
    // -----------------------------------------------------
    // Suppressed অ
    // -----------------------------------------------------

    if (suppressedA)
    {
        outputText += vowel;

        suppressedA = false;

        speakText(vowel);    

        Serial.print(
            "Independent vowel after suppressed অ: "
        );

        Serial.println(vowel);

        displayText();

        return;
    }


    // -----------------------------------------------------
    // Consonant + vowel = kar
    // -----------------------------------------------------

    String last = getLastCharacter(outputText);

    if (
        isBanglaConsonant(last) ||
        last == BN_KSSA ||
        last == BN_JNYA
    )
    {
        String kar = vowelToKar(vowel);

        if (kar.length() > 0)
        {
            outputText += kar;

            displayText();

            speakText(kar);    // <<< SPEAKER

            return;
        }
    }


    // -----------------------------------------------------
    // Independent vowel
    // -----------------------------------------------------

    outputText += vowel;

    displayText();

    speakText(vowel);    // <<< SPEAKER
}


// =====================================================
// ADD NON-VOWEL
// =====================================================

void addNonVowel(const String &character)
{
    suppressedA = false;

    outputText += character;

    displayText();

    speakText(character);    // <<< SPEAKER
}


// =====================================================
// START CONJUNCT
// =====================================================
//
// DOT4 + ENTER:
//
//     next TWO consonants
//
// DOT4 + DOT6 + ENTER:
//
//     next THREE consonants
//
// OLED remains unchanged here.
//
// =====================================================

void startConjunct(uint8_t count)
{
    if (count != 2 && count != 3)
        return;


    // -----------------------------------------------------
    // DOT4 again, right after DOT4 (no consonant yet):
    // toggle ZWJ joining.
    // -----------------------------------------------------

    if (
        count == 2 &&
        pendingConjunct == 2 &&
        conjunctBuffer.length() == 0
    )
    {
        conjunctZWJ = !conjunctZWJ;

        Serial.println(
            conjunctZWJ
                ? "DOT4 twice -> 2-consonant ZWJ conjunct (র‍্য)"
                : "DOT4 again -> normal 2-consonant conjunct"
        );

        return;
    }


    pendingConjunct = count;

    conjunctBuffer = "";

    conjunctZWJ = false;

    suppressedA = false;

    Serial.print(
        "Conjunct mode started. Waiting for "
    );

    Serial.print(count);

    Serial.println(" consonants.");

    // NO displayText() here.
}


// =====================================================
// PROCESS CONJUNCT CHARACTER
// =====================================================

void processConjunctCharacter(const String &character)
{
    // -----------------------------------------------------
    // Only consonants can enter conjunct buffer.
    // -----------------------------------------------------

    if (!isBanglaConsonant(character))
    {
        Serial.println(
            "Non-consonant received. "
            "Cancelling conjunct mode."
        );

        // Save buffered consonants.
        String stored = conjunctBuffer;

        cancelConjunct();

        // Put stored consonants into normal text.
        for (int i = 0; i < (int)stored.length(); )
        {
            int start = i;

            i = nextUnitEnd(stored, i);

            String oneCharacter =
                stored.substring(start, i);

            addNonVowel(oneCharacter);
        }

        // Process current character normally.
        processNormalCharacter(character);

        return;
    }


    // -----------------------------------------------------
    // Add consonant to buffer.
    // -----------------------------------------------------

    conjunctBuffer += character;

    speakText(character);    // <<< SPEAKER


    Serial.print("Conjunct buffer: ");
    Serial.println(conjunctBuffer);


    // -----------------------------------------------------
    // Display preview immediately.
    // -----------------------------------------------------

    displayConjunctPreview();


    // -----------------------------------------------------
    // (ড় / ঢ় / য় count as ONE consonant.)
    // -----------------------------------------------------

    if (countUnits(conjunctBuffer) == pendingConjunct)
    {
        finishConjunct();
    }
}


// =====================================================
// FINISH CONJUNCT
// =====================================================

void finishConjunct()
{
    if (pendingConjunct == 0)
        return;


    String result = "";

    uint8_t count = 0;


    for (int i = 0; i < (int)conjunctBuffer.length(); )
    {
        int start = i;

        i = nextUnitEnd(conjunctBuffer, i);

        String consonant =
            conjunctBuffer.substring(start, i);


        if (count > 0)
        {
            if (conjunctZWJ)
                result += BN_ZWJ;

            result += BN_HASANTA;
        }

        result += consonant;

        count++;
    }


    // Commit to actual text.

    outputText += result;

    suppressedA = false;


    Serial.print("Completed conjunct: ");
    Serial.println(result);


    pendingConjunct = 0;

    conjunctBuffer = "";

    conjunctZWJ = false;


    displayText();
}


// =====================================================
// CANCEL CONJUNCT
// =====================================================

void cancelConjunct()
{
    pendingConjunct = 0;

    conjunctBuffer = "";

    conjunctZWJ = false;
}


// =====================================================
// BACKSPACE
// =====================================================

void backspaceText()
{
    numberEndedBySpace = false;

    // -----------------------------------------------------
    // Number mode
    // -----------------------------------------------------

    if (numberMode)
    {
        if (outputText.length() > numberStartLength)
        {
            removeLastCharacter();

            Serial.print("After number-mode backspace: ");
            Serial.println(outputText);

            displayText();
        }
        else
        {
            numberMode = false;
            numberStartLength = 0;

            Serial.println(
                "Backspace: number prefix cancelled."
            );
        }

        return;
    }


    // Cancel pending DOT5 prefix first.

    if (pendingSpecialVowel)
    {
        pendingSpecialVowel = false;

        Serial.println(
            "Backspace: pending DOT5 prefix cancelled."
        );

        return;
    }


    // Cancel active conjunct first.

    if (pendingConjunct != 0)
    {
        cancelConjunct();

        Serial.println(
            "Backspace: conjunct mode cancelled."
        );

        displayText();

        return;
    }


    // Cancel suppressed অ.

    if (suppressedA)
    {
        suppressedA = false;

        Serial.println(
            "Backspace: suppressed অ cancelled."
        );

        return;
    }


    // Nothing to delete.

    if (outputText.length() == 0)
    {
        Serial.println("Nothing to delete.");
        return;
    }


    // Remove last letter unit.

    removeLastCharacter();

    Serial.print("After backspace: ");
    Serial.println(outputText);

    displayText();
}


// =====================================================
// CLEAR ALL
// =====================================================

void clearAllText()
{
    outputText = "";

    suppressedA = false;

    pendingSpecialVowel = false;

    numberMode = false;
    numberStartLength = 0;
    numberEndedBySpace = false;

    pendingConjunct = 0;

    conjunctBuffer = "";

    conjunctZWJ = false;

    Serial.println("Everything cleared.");

    displayText();
}


// =====================================================
// BACKSPACE BUTTON
// =====================================================
// P00:
// Short press = backspace
// Hold >= 2 seconds = clear all
// =====================================================

void handleBackspaceButton()
{
    static bool buttonWasPressed = false;
    static bool longPressTriggered = false;

    static unsigned long pressStartTime = 0;
    static unsigned long lastDebounceTime = 0;


    bool pressed = isDown(BACKSPACE_BIT);


    // Press

    if (pressed && !buttonWasPressed)
    {
        unsigned long now = millis();

        if (now - lastDebounceTime > 100)
        {
            lastDebounceTime = now;

            pressStartTime = now;

            longPressTriggered = false;

            buttonWasPressed = true;
        }
    }


    // Hold

    if (pressed && buttonWasPressed)
    {
        if (
            !longPressTriggered &&
            millis() - pressStartTime >= ENTER_HOLD_TIME
        )
        {
            longPressTriggered = true;

            clearAllText();

            speakTrack(TRACK_ALL_CLEARED);    // <<< SPEAKER
        }
    }


    // Release

    if (!pressed && buttonWasPressed)
    {
        unsigned long now = millis();

        if (now - lastDebounceTime > 100)
        {
            lastDebounceTime = now;

            if (!longPressTriggered)
            {
                backspaceText();

                speakTrack(TRACK_BACKSPACE);    // <<< SPEAKER
            }

            buttonWasPressed = false;
        }
    }
}


// =====================================================
// ENTER BUTTON
// =====================================================
EnterAction handleEnterButton(uint8_t &pattern)
{
    static bool wasPressed = false;
    static bool pending = false;
    static bool longPressTriggered = false;

    static unsigned long pressStartTime = 0;
    static unsigned long lastEnterTime = 0;

    static uint8_t capturedPattern = 0;

    EnterAction action = ENTER_ACTION_NONE;

    bool pressed = isDown(ENTER_BIT);

    unsigned long now = millis();


    // Press

    if (
        pressed &&
        !wasPressed &&
        !pending &&
        now - lastEnterTime > ENTER_REPEAT_GUARD
    )
    {
        lastEnterTime = now;

        capturedPattern = readBraille();

        pressStartTime = now;

        longPressTriggered = false;

        pending = true;

        wasPressed = true;
    }


    // Hold

    if (
        pressed &&
        pending &&
        !longPressTriggered &&
        now - pressStartTime >= ENTER_HOLD_TIME
    )
    {
        longPressTriggered = true;

        pending = false;

        action = ENTER_ACTION_NEWLINE;
    }


    // Release

    if (!pressed && wasPressed)
    {
        if (pending && !longPressTriggered)
        {
            pattern = capturedPattern;

            if (capturedPattern == PATTERN_SPACE)
            {
                action = ENTER_ACTION_SPACE;
            }
            else
            {
                action = ENTER_ACTION_PATTERN;
            }
        }

        pending = false;

        wasPressed = false;

        delay(DEBOUNCE_DELAY);

        buttonState = readExpander();
    }


    return action;
}


// =====================================================
// WRITE MODE LOOP                            
// =====================================================

void writeModeLoop()
{
    // BACKSPACE

    handleBackspaceButton();


    // ENTER

    uint8_t pattern = PATTERN_SPACE;

    EnterAction action = handleEnterButton(pattern);

    bool afterNumberSpace = false;

    if (action != ENTER_ACTION_NONE)
    {
        afterNumberSpace = numberEndedBySpace;

        numberEndedBySpace = false;
    }


    // -------------------------------------------------
    // NEW LINE
    // -------------------------------------------------

    if (action == ENTER_ACTION_NEWLINE)
    {
        endNumberMode();

        addNewLine();

        speakTrack(TRACK_NEWLINE);    // <<< SPEAKER
    }


    // -------------------------------------------------
    // SPACE
    // -------------------------------------------------

    else if (action == ENTER_ACTION_SPACE)
    {
        printDots(PATTERN_SPACE);

        bool wasNumber = numberMode;

        endNumberMode();

        processNormalCharacter(" ");

        speakTrack(TRACK_SPACE);    // <<< SPEAKER

        numberEndedBySpace = wasNumber;

        Serial.print("Current text: ");
        Serial.println(outputText);
    }


    // -------------------------------------------------
    // BRAILLE PATTERN
    // -------------------------------------------------

    else if (action == ENTER_ACTION_PATTERN)
    {
        printDots(pattern);


        // ---------------------------------------------
        // NUMBER MODE
        // ---------------------------------------------

        if (numberMode)
        {
            processNumber(pattern);
        }


        // NUMBER PREFIX (45)

        else if (pattern == PATTERN_NUMBER)
        {
            startNumberMode();

            speakTrack(TRACK_NUMBER_MODE);    // <<< SPEAKER
        }


        // DOT4 -> 2-consonant conjunct

        else if (pattern == PATTERN_CONJUNCT2)
        {
            Serial.println("DOT4 -> 2-consonant mode");

            startConjunct(2);

            speakTrack(TRACK_CONJUNCT_1);    // <<< SPEAKER
        }


        // DOT4 + DOT6 -> 3-consonant conjunct

        else if (pattern == PATTERN_CONJUNCT3)
        {
            Serial.println("DOT4+DOT6 -> 3-consonant mode");

            startConjunct(3);

            speakTrack(TRACK_CONJUNCT_2);    // <<< SPEAKER
        }


        // DOT5 -> waits for র / ত

        else if (
            pattern == PATTERN_SPECIAL_VOWEL &&
            pendingConjunct == 0
        )
        {
            pendingSpecialVowel = true;

            Serial.println("DOT5 -> waiting for র or ত");

            speakTrack(TRACK_DOT5);    // <<< SPEAKER
        }


        // NORMAL BRAILLE

        else
        {
            String result = brailleToBangla(pattern);

            if (result.length() > 0)
            {
                Serial.print("Character: ");
                Serial.println(result);


                // ------------------------------------
                // Dari right after the SPACE that ended number mode
                // ------------------------------------

                if (
                    result == "।" &&
                    afterNumberSpace &&
                    getLastCharacter(outputText) == " "
                )
                {
                    removeLastCharacter();

                    Serial.println(
                        "Dari attached to number (space removed)."
                    );
                }


                processCharacter(result);

                Serial.print("Current text: ");
                Serial.println(outputText);
            }
            else
            {
                Serial.println("Invalid Braille pattern.");

                speakTrack(TRACK_WRONG);    // <<< SPEAKER
            }
        }
    }
}


// =====================================================
//
//          READ MODE  (Bangla -> Braille)
//
// =====================================================


// =====================================================
// OLED DISPLAY SETTINGS (READ mode)
// =====================================================

// Start text 10 pixels down from the top
const int OFFSET_Y = 10;

// Distance between text rows
const int READ_LINE_HEIGHT = 22;                   

// Current row being rendered
int currentLineOffset = 0;

// Height used to flip renderer coordinates
int currentTextHeight = 18;


// =====================================================
// TEXT KEPT FOR READ MODE                       
// =====================================================

String readModeText = DEVICE_TITLE;              


// =====================================================
// BanglaTextRenderer pixel callback (READ mode)
// =====================================================

void draw_pixel_read(int16_t x, int16_t y)
{
    int oledX = y;

    int oledY =
        (currentTextHeight - 1 - x)
        + OFFSET_Y
        + currentLineOffset;

    u8g2.drawPixel(oledX, oledY);
}


// =====================================================
// DISPLAY BANGLA TEXT (READ mode)
// =====================================================

void displayReadText(String text)
{
    u8g2.clearBuffer();

    // -------------------------------------------------
    // Simple word-by-word wrapping
    // -------------------------------------------------

    String line = "";
    int start = 0;

    while (start < text.length())
    {
        // Skip spaces
        while (
            start < text.length() &&
            text.charAt(start) == ' '
        )
        {
            start++;
        }

        if (start >= text.length())
        {
            break;
        }

        // Find next space
        int spacePos =
            text.indexOf(' ', start);

        String word;

        if (spacePos == -1)
        {
            word = text.substring(start);

            start = text.length();
        }
        else
        {
            word =
                text.substring(start, spacePos);

            start = spacePos + 1;
        }


        // Try adding this word to current line

        String testLine;

        if (line.length() == 0)
        {
            testLine = word;
        }
        else
        {
            testLine =
                line + " " + word;
        }


        BTTextSize size =
            textRenderer->getTextSize(
                testLine.c_str()
            );


        // If it fits, keep it on this line

        if (size.width <= DISPLAY_WIDTH)
        {
            line = testLine;
        }
        else
        {
            // Render current line first

            if (line.length() > 0)
            {
                BTTextSize lineSize =
                    textRenderer->getTextSize(
                        line.c_str()
                    );

                currentTextHeight =
                    lineSize.height;


                textRenderer->renderText(
                    line.c_str(),
                    draw_pixel_read
                );


                currentLineOffset +=
                    READ_LINE_HEIGHT;
            }


            // Start new line with word

            line = word;
        }
    }


    // -------------------------------------------------
    // Render final line
    // -------------------------------------------------

    if (line.length() > 0)
    {
        BTTextSize lineSize =
            textRenderer->getTextSize(
                line.c_str()
            );

        currentTextHeight =
            lineSize.height;


        textRenderer->renderText(
            line.c_str(),
            draw_pixel_read
        );
    }


    u8g2.sendBuffer();


    // Reset for next sentence

    currentLineOffset = 0;
}


// =====================================================
// BRAILLE SECTION
// =====================================================
//
// Bit 0 = Dot 1
// Bit 1 = Dot 2
// Bit 2 = Dot 3
// Bit 3 = Dot 4
// Bit 4 = Dot 5
// Bit 5 = Dot 6
//
// =====================================================


// =====================================================
// SOLENOID PIN CONFIGURATION
// =====================================================

const uint8_t SOLENOID_PINS[6] = {
    25,     // Dot 1
    33,     // Dot 2
    32,     // Dot 3
    14,     // Dot 4
    27,     // Dot 5
    26      // Dot 6
};


// =====================================================
// NEXT / PREVIOUS BUTTONS (on PCF8575)
// =====================================================
// Same physical buttons as the keyboard:
//   P01 = ENTER      -> NEXT     in READ mode
//   P00 = BACKSPACE  -> PREVIOUS in READ mode
// =====================================================

#define NEXT_BIT ENTER_BIT                         
#define PREV_BIT BACKSPACE_BIT                     


// =====================================================
// BUTTON DEBOUNCE
// =====================================================

const uint16_t DEBOUNCE_TIME = 30;


// =====================================================
// SOLENOID TIMING
// =====================================================
//
// ON  = 2 seconds
// OFF = 1 second
//
// =====================================================

const unsigned long SOLENOID_ON_TIME  = 2000;
const unsigned long SOLENOID_OFF_TIME = 1000;

bool solenoidState = false;

unsigned long solenoidTimer = 0;


// =====================================================
// BRAILLE BUFFER
// =====================================================

#define MAX_BRAILLE_CELLS 512

uint8_t brailleBuffer[MAX_BRAILLE_CELLS];

uint16_t brailleLength = 0;

int16_t currentCell = 0;


// =====================================================
// INVALID INPUT WARNING
// =====================================================

bool warningMode = false;
bool warningSoundPending = false;

bool warningVisible = false;

unsigned long warningTimer = 0;

const unsigned long WARNING_BLINK_TIME = 500;


// =====================================================
// UNICODE DEFINITIONS
// =====================================================


// =====================================================
// INDEPENDENT VOWELS
// =====================================================

#define U_A      0x0985   // অ
#define U_AA     0x0986   // আ
#define U_I      0x0987   // ই
#define U_II     0x0988   // ঈ
#define U_U      0x0989   // উ
#define U_UU     0x098A   // ঊ
#define U_RI     0x098B   // ঋ
#define U_E      0x098F   // এ
#define U_AI     0x0990   // ঐ
#define U_O      0x0993   // ও
#define U_AU     0x0994   // ঔ


// =====================================================
// CONSONANTS
// =====================================================

#define U_KA     0x0995   // ক
#define U_KHA    0x0996   // খ
#define U_GA     0x0997   // গ
#define U_GHA    0x0998   // ঘ
#define U_NGA    0x0999   // ঙ

#define U_CA     0x099A   // চ
#define U_CHA    0x099B   // ছ
#define U_JA     0x099C   // জ
#define U_JHA    0x099D   // ঝ
#define U_NYA    0x099E   // ঞ

#define U_TTA    0x099F   // ট
#define U_TTHA   0x09A0   // ঠ
#define U_DDA    0x09A1   // ড
#define U_DDHA   0x09A2   // ঢ
#define U_NNA    0x09A3   // ণ

#define U_TA     0x09A4   // ত
#define U_THA    0x09A5   // থ
#define U_DA     0x09A6   // দ
#define U_DHA    0x09A7   // ধ
#define U_NA     0x09A8   // ন

#define U_PA     0x09AA   // প
#define U_PHA    0x09AB   // ফ
#define U_BA     0x09AC   // ব
#define U_BHA    0x09AD   // ভ
#define U_MA     0x09AE   // ম

#define U_YA     0x09AF   // য
#define U_RA     0x09B0   // র
#define U_LA     0x09B2   // ল

#define U_SHA    0x09B6   // শ
#define U_SSA    0x09B7   // ষ
#define U_SA     0x09B8   // স
#define U_HA     0x09B9   // হ

#define U_RRA    0x09DC   // ড়
#define U_RHA    0x09DD   // ঢ়
#define U_YYA    0x09DF   // য়


// =====================================================
// DEPENDENT VOWELS / KAR
// =====================================================

#define U_AA_KAR 0x09BE   // া
#define U_I_KAR  0x09BF   // ি
#define U_II_KAR 0x09C0   // ী
#define U_U_KAR  0x09C1   // ু
#define U_UU_KAR 0x09C2   // ূ
#define U_RI_KAR 0x09C3   // ৃ
#define U_E_KAR  0x09C7   // ে
#define U_AI_KAR 0x09C8   // ৈ
#define U_O_KAR  0x09CB   // ো
#define U_AU_KAR 0x09CC   // ৌ


// =====================================================
// SPECIAL CHARACTERS
// =====================================================

#define U_CHANDRA    0x0981   // ঁ
#define U_ANUSVARA   0x0982   // ং
#define U_VISARGA    0x0983   // ঃ

#define U_HASANTA    0x09CD   // ্
#define U_KHANDA_TA  0x09CE   // ৎ


// =====================================================
// DIGITS
// =====================================================

#define U_DIGIT_0    0x09E6   // ০
#define U_DIGIT_1    0x09E7   // ১
#define U_DIGIT_2    0x09E8   // ২
#define U_DIGIT_3    0x09E9   // ৩
#define U_DIGIT_4    0x09EA   // ৪
#define U_DIGIT_5    0x09EB   // ৫
#define U_DIGIT_6    0x09EC   // ৬
#define U_DIGIT_7    0x09ED   // ৭
#define U_DIGIT_8    0x09EE   // ৮
#define U_DIGIT_9    0x09EF   // ৯


// =====================================================
// PUNCTUATION
// =====================================================

// Only the dari has its own Bangla code point.

#define U_DARI         0x0964   // ।  Bangla dari (full stop)
#define U_COMMA        0x002C   // ,
#define U_SEMICOLON    0x003B   // ;
#define U_QUESTION     0x003F   // ?
#define U_EXCLAMATION  0x0021   // !
#define U_HYPHEN       0x002D   // -

// Decimal point (দশমিক)
// Braille: dots 256 (same as dari)
#define U_DECIMAL      0x002E   


// =====================================================
// ZERO WIDTH JOINER (ZWJ)
// =====================================================
//
// Unicode U+200D, UTF-8 bytes E2 80 8D
//
// Invisible. Kept in the text so the OLED can draw
// র‍্য (র + য-ফলা, e.g. র‍্যাব) differently from
// র্য (reph, e.g. কার্য).
// =====================================================

#define U_ZWJ          0x200D



// =====================================================
// NUMBER PREFIX
// =====================================================
//
// Dots 4,5 mark the start of a number.
// A blank cell marks the end of number mode.
//
// =====================================================

#define NUMBER_PREFIX 45


// =====================================================
// CHECK IF TEXT IS FULLY BANGLA
// =====================================================

bool isAllowedCharacter(uint32_t c)
{
    // Bangla Unicode block

    if (
        c >= 0x0980 &&
        c <= 0x09FF
    )
    {
        return true;
    }


    switch (c)
    {
        case U_DARI:
        case U_COMMA:
        case U_SEMICOLON:
        case U_QUESTION:
        case U_EXCLAMATION:
        case U_HYPHEN:
        case U_ZWJ:
        case ' ':

            return true;

        default:

            return false;
    }
}


// Defined further below

bool isDigitChar(uint32_t c);


bool decodeCheckedUTF8(
    const String &text,
    int &index,
    uint32_t &c
)
{
    uint8_t b1 =
        (uint8_t)text[index];


    // ASCII

    if (b1 < 0x80)
    {
        c = b1;

        index++;

        return true;
    }


    // 2-byte

    if ((b1 & 0xE0) == 0xC0)
    {
        if (index + 1 >= text.length())
        {
            return false;
        }

        uint8_t b2 =
            (uint8_t)text[index + 1];

        c =
            ((uint32_t)(b1 & 0x1F) << 6) |
            (b2 & 0x3F);

        index += 2;

        return true;
    }


    // 3-byte

    if ((b1 & 0xF0) == 0xE0)
    {
        if (index + 2 >= text.length())
        {
            return false;
        }

        uint8_t b2 =
            (uint8_t)text[index + 1];

        uint8_t b3 =
            (uint8_t)text[index + 2];

        c =
            ((uint32_t)(b1 & 0x0F) << 12) |
            ((uint32_t)(b2 & 0x3F) << 6) |
            (b3 & 0x3F);

        index += 3;

        return true;
    }


    // 4-byte or invalid

    return false;
}


// -----------------------------------------------------
// Decimal point rule
// -----------------------------------------------------

bool isValidDecimalPoint(
    uint32_t prev,
    bool     hasNext,
    uint32_t next
)
{
    bool prevDigit =
        isDigitChar(prev);

    bool nextDigit =
        hasNext && isDigitChar(next);

    bool prevIsStartOrSpace =
        (prev == 0 || prev == ' ');


    return
        prevDigit ||
        (nextDigit && prevIsStartOrSpace);
}


bool isValidBanglaText(const String &text)
{
    int index = 0;

    // True once any non-space character is seen

    bool foundContent = false;

    // Previous character (0 = start of text)

    uint32_t prev = 0;


    while (index < text.length())
    {
        uint32_t c = 0;


        if (!decodeCheckedUTF8(text, index, c))
        {
            Serial.println(
                "Invalid or unsupported UTF-8 character found."
            );

            return false;
        }


        // -------------------------------------------------
        // Decimal point: check neighbours
        // -------------------------------------------------

        if (c == U_DECIMAL)
        {
            uint32_t next = 0;

            int peek = index;

            bool hasNext =
                peek < text.length() &&
                decodeCheckedUTF8(text, peek, next);


            if (!isValidDecimalPoint(prev, hasNext, next))
            {
                Serial.println(
                    "'.' is only allowed inside a number."
                );

                return false;
            }
        }


        // -------------------------------------------------
        // Any other character must be in the allowed list
        // -------------------------------------------------

        else if (!isAllowedCharacter(c))
        {
            Serial.print(
                "Non-Bangla character found: U+"
            );

            Serial.println(
                (unsigned long)c,
                HEX
            );

            return false;
        }


        // ZWJ is invisible: it is not content and does
        // not change the "previous character" used by
        // the decimal point rule.

        if (c == U_ZWJ)
        {
            continue;
        }


        if (c != ' ')
        {
            foundContent = true;
        }


        prev = c;
    }


    return foundContent;
}


// =====================================================
// BRAILLE DOT MASK
// =====================================================

uint8_t dotMask(uint8_t dot)
{
    if (dot < 1 || dot > 6)
    {
        return 0;
    }

    return (1 << (dot - 1));
}


// =====================================================
// CONVERT PATTERN TO MASK
// =====================================================

uint8_t patternToMask(uint32_t pattern)
{
    uint8_t mask = 0;

    while (pattern > 0)
    {
        uint8_t d =
            pattern % 10;

        pattern /= 10;

        if (d >= 1 && d <= 6)
        {
            mask |= dotMask(d);
        }
    }

    return mask;
}


// =====================================================
// APPEND ONE CELL
// =====================================================

void appendCell(uint8_t mask)
{
    if (
        brailleLength >=
        MAX_BRAILLE_CELLS
    )
    {
        return;
    }

    brailleBuffer[brailleLength] =
        mask;

    brailleLength++;
}


// =====================================================
// APPEND DOT PATTERN
// =====================================================

void appendPattern(uint32_t pattern)
{
    appendCell(
        patternToMask(pattern)
    );
}


// =====================================================
// CHARACTER CLASSIFICATION
// =====================================================

bool isConsonant(uint32_t c)
{
    switch (c)
    {
        case U_KA:
        case U_KHA:
        case U_GA:
        case U_GHA:
        case U_NGA:

        case U_CA:
        case U_CHA:
        case U_JA:
        case U_JHA:
        case U_NYA:

        case U_TTA:
        case U_TTHA:
        case U_DDA:
        case U_DDHA:
        case U_NNA:

        case U_TA:
        case U_THA:
        case U_DA:
        case U_DHA:
        case U_NA:

        case U_PA:
        case U_PHA:
        case U_BA:
        case U_BHA:
        case U_MA:

        case U_YA:
        case U_RA:
        case U_LA:

        case U_SHA:
        case U_SSA:
        case U_SA:
        case U_HA:

        case U_RRA:
        case U_RHA:
        case U_YYA:

            return true;

        default:

            return false;
    }
}


// =====================================================
// INDEPENDENT VOWEL CHECK
// =====================================================

bool isIndependentVowel(uint32_t c)
{
    switch (c)
    {
        case U_A:
        case U_AA:
        case U_I:
        case U_II:
        case U_U:
        case U_UU:
        case U_RI:
        case U_E:
        case U_AI:
        case U_O:
        case U_AU:

            return true;

        default:

            return false;
    }
}


// =====================================================
// DEPENDENT VOWEL CHECK
// =====================================================

bool isDependentVowel(uint32_t c)
{
    switch (c)
    {
        case U_AA_KAR:
        case U_I_KAR:
        case U_II_KAR:
        case U_U_KAR:
        case U_UU_KAR:
        case U_RI_KAR:
        case U_E_KAR:
        case U_AI_KAR:
        case U_O_KAR:
        case U_AU_KAR:

            return true;

        default:

            return false;
    }
}


// =====================================================
// SPECIAL CHARACTER CHECK
// =====================================================

bool isSpecialCharacter(uint32_t c)
{
    switch (c)
    {
        case U_CHANDRA:
        case U_ANUSVARA:
        case U_VISARGA:
        case U_KHANDA_TA:

            return true;

        default:

            return false;
    }
}


// =====================================================
// DIGIT CHECK
// =====================================================

bool isDigitChar(uint32_t c)
{
    switch (c)
    {
        case U_DIGIT_0:
        case U_DIGIT_1:
        case U_DIGIT_2:
        case U_DIGIT_3:
        case U_DIGIT_4:
        case U_DIGIT_5:
        case U_DIGIT_6:
        case U_DIGIT_7:
        case U_DIGIT_8:
        case U_DIGIT_9:

            return true;

        default:

            return false;
    }
}


// =====================================================
// BANGLA DIGIT → BRAILLE (without number prefix)
// =====================================================
//
// ১ = 1      ৬ = 124
// ২ = 12     ৭ = 1245
// ৩ = 14     ৮ = 125
// ৪ = 145    ৯ = 24
// ৫ = 15     ০ = 245
//
// =====================================================

bool appendDigit(uint32_t c)
{
    switch (c)
    {
        case U_DIGIT_1:
            appendPattern(1);
            return true;

        case U_DIGIT_2:
            appendPattern(12);
            return true;

        case U_DIGIT_3:
            appendPattern(14);
            return true;

        case U_DIGIT_4:
            appendPattern(145);
            return true;

        case U_DIGIT_5:
            appendPattern(15);
            return true;

        case U_DIGIT_6:
            appendPattern(124);
            return true;

        case U_DIGIT_7:
            appendPattern(1245);
            return true;

        case U_DIGIT_8:
            appendPattern(125);
            return true;

        case U_DIGIT_9:
            appendPattern(24);
            return true;

        case U_DIGIT_0:
            appendPattern(245);
            return true;

        default:
            return false;
    }
}


// =====================================================
// PUNCTUATION CHECK (READ mode)
// =====================================================

bool isPunctuationCP(uint32_t c)
{
    switch (c)
    {
        case U_COMMA:
        case U_SEMICOLON:
        case U_DARI:
        case U_QUESTION:
        case U_EXCLAMATION:
        case U_HYPHEN:

            return true;

        default:

            return false;
    }
}


// =====================================================
// PUNCTUATION → BRAILLE
// =====================================================

void appendPunctuation(uint32_t c)
{
    switch (c)
    {
        case U_COMMA:
            appendPattern(2);
            break;

        case U_SEMICOLON:
            appendPattern(23);
            break;

        case U_DARI:
            appendPattern(256);
            break;

        case U_QUESTION:
            appendPattern(236);
            break;

        case U_EXCLAMATION:
            appendPattern(235);
            break;

        case U_HYPHEN:
            appendPattern(36);
            break;

        default:
            break;
    }
}


// =====================================================
// BANGLA CHARACTER → BRAILLE
// =====================================================

bool appendCharacter(uint32_t c)
{
    switch (c)
    {
        // =================================================
        // INDEPENDENT VOWELS
        // =================================================

        case U_A:

            appendPattern(1);

            return true;


        case U_AA:

            appendPattern(345);

            return true;


        case U_I:

            appendPattern(24);

            return true;


        case U_II:

            appendPattern(35);

            return true;


        case U_U:

            appendPattern(136);

            return true;


        case U_UU:

            appendPattern(1256);

            return true;


        // =================================================
        // INDEPENDENT ঋ
        // =================================================

        case U_RI:

            appendPattern(5);
            appendPattern(1235);

            return true;


        case U_E:

            appendPattern(15);

            return true;


        case U_AI:

            appendPattern(34);

            return true;


        case U_O:

            appendPattern(135);

            return true;


        case U_AU:

            appendPattern(246);

            return true;


        // =================================================
        // CONSONANTS
        // =================================================

        case U_KA:
            appendPattern(13);
            return true;

        case U_KHA:
            appendPattern(1346);
            return true;

        case U_GA:
            appendPattern(1245);
            return true;

        case U_GHA:
            appendPattern(126);
            return true;

        case U_NGA:
            appendPattern(346);
            return true;


        case U_CA:
            appendPattern(14);
            return true;

        case U_CHA:
            appendPattern(16);
            return true;

        case U_JA:
            appendPattern(245);
            return true;

        case U_JHA:
            appendPattern(1356);
            return true;

        case U_NYA:
            appendPattern(25);
            return true;


        case U_TTA:
            appendPattern(23456);
            return true;

        case U_TTHA:
            appendPattern(2456);
            return true;

        case U_DDA:
            appendPattern(1246);
            return true;

        case U_DDHA:
            appendPattern(123456);
            return true;

        case U_NNA:
            appendPattern(3456);
            return true;


        case U_TA:
            appendPattern(2345);
            return true;

        case U_THA:
            appendPattern(1456);
            return true;

        case U_DA:
            appendPattern(145);
            return true;

        case U_DHA:
            appendPattern(2346);
            return true;

        case U_NA:
            appendPattern(1345);
            return true;


        case U_PA:
            appendPattern(1234);
            return true;

        case U_PHA:
            appendPattern(124);
            return true;

        case U_BA:
            appendPattern(12);
            return true;

        case U_BHA:
            appendPattern(1236);
            return true;

        case U_MA:
            appendPattern(134);
            return true;


        case U_YA:
            appendPattern(13456);
            return true;

        case U_RA:
            appendPattern(1235);
            return true;

        case U_LA:
            appendPattern(123);
            return true;


        case U_SHA:
            appendPattern(146);
            return true;

        case U_SSA:
            appendPattern(12346);
            return true;

        case U_SA:
            appendPattern(234);
            return true;

        case U_HA:
            appendPattern(125);
            return true;


        case U_RRA:
            appendPattern(12456);
            return true;

        case U_RHA:
            appendPattern(12356);
            return true;

        case U_YYA:
            appendPattern(26);
            return true;


        // =================================================
        // DEPENDENT VOWELS
        // =================================================

        case U_AA_KAR:

            appendPattern(345);

            return true;


        case U_I_KAR:

            appendPattern(24);

            return true;


        case U_II_KAR:

            appendPattern(35);

            return true;


        case U_U_KAR:

            appendPattern(136);

            return true;


        case U_UU_KAR:

            appendPattern(1256);

            return true;


        // =================================================
        // DEPENDENT ঋ / ৃ
        //
        // ৃ = dot 5 + র
        //
        // No অ is inserted here.
        // =================================================

        case U_RI_KAR:

            appendPattern(5);
            appendPattern(1235);

            return true;


        case U_E_KAR:

            appendPattern(15);

            return true;


        case U_AI_KAR:

            appendPattern(34);

            return true;


        case U_O_KAR:

            appendPattern(135);

            return true;


        case U_AU_KAR:

            appendPattern(246);

            return true;


        // =================================================
        // ANUSVARA
        //
        // ং = dots 56
        // =================================================

        case U_ANUSVARA:

            appendPattern(56);

            return true;


        // =================================================
        // VISARGA
        //
        // ঃ = dot 6
        // =================================================

        case U_VISARGA:

            appendPattern(6);

            return true;


        // =================================================
        // CANDRABINDU
        //
        // ঁ = dot 3
        // =================================================

        case U_CHANDRA:

            appendPattern(3);

            return true;


        // =================================================
        // KHANDA TA
        //
        // ৎ = dot 5 + ত
        // =================================================

        case U_KHANDA_TA:

            appendPattern(5);
            appendPattern(2345);

            return true;


        default:

            return false;
    }
}


// =====================================================
// DIRECT CONJUNCT CHECK
// =====================================================
//
// ক্ষ = 12345
// জ্ঞ = 156
//
// =====================================================

bool isDirectConjunct(
    uint32_t first,
    uint32_t second
)
{
    if (
        first == U_KA &&
        second == U_SSA
    )
    {
        return true;
    }


    if (
        first == U_JA &&
        second == U_NYA
    )
    {
        return true;
    }


    return false;
}


// =====================================================
// APPEND DIRECT CONJUNCT
// =====================================================

void appendDirectConjunct(
    uint32_t first,
    uint32_t second
)
{
    if (
        first == U_KA &&
        second == U_SSA
    )
    {
        appendPattern(12345);
    }

    else if (
        first == U_JA &&
        second == U_NYA
    )
    {
        appendPattern(156);
    }
}


// =====================================================
// UTF-8 DECODER
// =====================================================

uint32_t readUTF8(
    const String &text,
    int &index
)
{
    uint8_t c =
        (uint8_t)text[index++];


    // ASCII

    if (c < 0x80)
    {
        return c;
    }


    // 2-byte UTF-8

    if ((c & 0xE0) == 0xC0)
    {
        uint8_t c2 =
            (uint8_t)text[index++];


        return
            ((uint32_t)(c & 0x1F) << 6) |
            (c2 & 0x3F);
    }


    // 3-byte UTF-8

    if ((c & 0xF0) == 0xE0)
    {
        uint8_t c2 =
            (uint8_t)text[index++];

        uint8_t c3 =
            (uint8_t)text[index++];


        return
            ((uint32_t)(c & 0x0F) << 12) |
            ((uint32_t)(c2 & 0x3F) << 6) |
            (c3 & 0x3F);
    }


    // 4-byte UTF-8

    if ((c & 0xF8) == 0xF0)
    {
        uint8_t c2 =
            (uint8_t)text[index++];

        uint8_t c3 =
            (uint8_t)text[index++];

        uint8_t c4 =
            (uint8_t)text[index++];


        return
            ((uint32_t)(c & 0x07) << 18) |
            ((uint32_t)(c2 & 0x3F) << 12) |
            ((uint32_t)(c3 & 0x3F) << 6) |
            (c4 & 0x3F);
    }


    return 0;
}


// =====================================================
// TRANSLATE COMPLETE BANGLA TEXT
// =====================================================

void translateBangla(
    const String &text
)
{
    // -------------------------------------------------
    // Clear previous Braille buffer
    // -------------------------------------------------

    brailleLength = 0;

    currentCell = 0;


    // =================================================
    // LEADING BLANK CELL
    // =================================================

    // Cell 0 is always blank, so nothing is raised when new text arrives
    appendCell(0);


    // =================================================
    // STEP 1
    //
    // Decode complete UTF-8 string into Unicode array
    // =================================================

    const uint16_t MAX_CHARS = 256;

    uint32_t chars[MAX_CHARS];

    uint16_t charCount = 0;

    int index = 0;


    while (
        index < text.length() &&
        charCount < MAX_CHARS
    )
    {
        uint32_t c =
            readUTF8(text, index);


        // Skip ZWJ: conjunct detection needs
        // consonant + ্ + consonant with nothing in between, so র‍্য still gets its dot-4 prefix.

        if (c == U_ZWJ)
        {
            continue;
        }


        chars[charCount++] = c;
    }



    // =================================================
    // STEP 2
    //
    // Process Unicode array
    // =================================================

    uint16_t i = 0;


    // True while inside a number (after the 45 prefix)

    bool numberMode = false;


    while (i < charCount)
    {
        uint32_t c =
            chars[i];


        // =================================================
        // DIGITS
        //
        // First digit of a number gets the 45 prefix.
        // Following digits are added without prefix.
        // =================================================

        if (isDigitChar(c))
        {
            if (!numberMode)
            {
                appendPattern(NUMBER_PREFIX);

                numberMode = true;
            }


            appendDigit(c);

            i++;

            continue;
        }


        // =================================================
        // DECIMAL POINT (দশমিক) = dots 256
        // =================================================

        if (c == U_DECIMAL)
        {
            if (!numberMode)
            {
                appendPattern(NUMBER_PREFIX);

                numberMode = true;
            }


            appendPattern(256);

            i++;

            continue;
        }


        // =================================================
        // PUNCTUATION does NOT end number mode
        // 
        // =================================================

        if (isPunctuationCP(c))                    
        {
            // Dari directly after a number
            // Inside number mode 256 is read back as the decimal point
            

            if (c == U_DARI && numberMode)
            {
                appendCell(0);

                numberMode = false;
            }


            appendPunctuation(c);

            i++;

            continue;
        }


        // =================================================
        // SPACE
        // =================================================

        // Every space becomes one blank Braille cell.
        if (c == ' ')
        {
            appendCell(0);

            numberMode = false;

            i++;

            continue;
        }


        // =================================================
        // NEWLINE / TAB
        // =================================================

        if (
            c == '\n' ||
            c == '\r' ||
            c == '\t'
        )
        {
            appendCell(0);

            numberMode = false;

            i++;

            continue;
        }


        // =================================================
        // END OF NUMBER MODE
        // =================================================

        if (numberMode)
        {
            appendCell(0);

            numberMode = false;
        }


        // =================================================
        // DIRECT CONJUNCTS
        //
        // ক্ষ
        // জ্ঞ
        // =================================================

        if (
            i + 2 < charCount &&
            isConsonant(c) &&
            chars[i + 1] == U_HASANTA &&
            isConsonant(chars[i + 2])
        )
        {
            uint32_t second =
                chars[i + 2];


            if (
                isDirectConjunct(
                    c,
                    second
                )
            )
            {
                appendDirectConjunct(
                    c,
                    second
                );


                // Skip:
                //
                // consonant
                // hasanta
                // consonant

                i += 3;


                // Dependent vowel immediately after conjunct

                if (
                    i < charCount &&
                    isDependentVowel(chars[i])
                )
                {
                    appendCharacter(chars[i]);

                    i++;
                }


                continue;
            }
        }


        // =================================================
        // GENERAL CONJUNCT
        // =================================================

        if (isConsonant(c))
        {
            uint16_t j = i;

            uint8_t consonantCount = 1;


            // Find connected consonants

            while (
                j + 2 < charCount &&
                chars[j + 1] == U_HASANTA &&
                isConsonant(chars[j + 2])
            )
            {
                consonantCount++;

                j += 2;
            }


            // ------------------------------------------------
            // Conjunct found
            // ------------------------------------------------

            if (consonantCount >= 2)
            {
                // Two consonants:
                // dot 4
                //
                // Three or more:
                // dots 4,6

                if (consonantCount == 2)
                {
                    appendPattern(4);
                }
                else
                {
                    appendPattern(46);
                }


                // ------------------------------------------------
                // Add each consonant
                // ------------------------------------------------

                uint16_t p = i;


                for (
                    uint8_t n = 0;
                    n < consonantCount;
                    n++
                )
                {
                    appendCharacter(
                        chars[p]
                    );

                    p += 2;
                }


                // Move to last consonant

                i = j;


                // ------------------------------------------------
                // Dependent vowel after conjunct
                // ------------------------------------------------

                if (
                    i + 1 < charCount &&
                    isDependentVowel(chars[i + 1])
                )
                {
                    appendCharacter(
                        chars[i + 1]
                    );

                    i++;
                }


                i++;

                continue;
            }
        }


        // =================================================
        // NORMAL CONSONANT
        // =================================================

        if (isConsonant(c))
        {
            appendCharacter(c);

            i++;

            continue;
        }


        // =================================================
        // DEPENDENT VOWEL
        // =================================================

        if (isDependentVowel(c))
        {
            appendCharacter(c);

            i++;

            continue;
        }


        // =================================================
        // INDEPENDENT VOWEL
        // =================================================

        if (isIndependentVowel(c))
        {
            // ------------------------------------------------
            // Independent vowel after consonant
            //
            // All independent vowels except অ.
            // ------------------------------------------------

            if (
                i > 0 &&
                isConsonant(chars[i - 1]) &&
                (
                    c == U_AA ||
                    c == U_I  ||
                    c == U_II ||
                    c == U_U  ||
                    c == U_UU ||
                    c == U_RI ||
                    c == U_E  ||
                    c == U_AI ||
                    c == U_O  ||
                    c == U_AU
                )
            )
            {
                appendCharacter(U_A);
            }


            appendCharacter(c);

            i++;

            continue;
        }


        // =================================================
        // SPECIAL CHARACTERS
        // =================================================
        //
        // ং → 56
        // ঃ → 6
        // ঁ → 3
        // ৎ → 5 + ত
        //
        // =================================================

        if (isSpecialCharacter(c))
        {
            appendCharacter(c);

            i++;

            continue;
        }


        // =================================================
        // HASANTA BY ITSELF
        // =================================================

        if (c == U_HASANTA)
        {
            i++;

            continue;
        }


        // =================================================
        // UNKNOWN CHARACTER
        // =================================================

        Serial.print(
            "Unsupported Unicode: 0x"
        );

        Serial.println(
            (unsigned long)c,
            HEX
        );


        i++;
    }


    // =================================================
    // TRAILING BLANK CELL
    //
    // One blank cell after the last word, so the final NEXT press raises nothing, which marks the end.
    // 
    // =================================================

    appendCell(0);


    // =================================================
    // Translation complete
    // =================================================

    if (brailleLength > 0)
    {
        currentCell = 0;
    }
}


// =====================================================
// SOLENOID CONTROL
// =====================================================

void clearSolenoids()
{
    for (uint8_t i = 0; i < 6; i++)
    {
        digitalWrite(
            SOLENOID_PINS[i],
            LOW
        );
    }
}


// =====================================================
// SHOW ONE BRAILLE CELL
// =====================================================

void showBrailleCell(
    uint8_t mask
)
{
    for (uint8_t i = 0; i < 6; i++)
    {
        if (mask & (1 << i))
        {
            digitalWrite(
                SOLENOID_PINS[i],
                HIGH
            );
        }
        else
        {
            digitalWrite(
                SOLENOID_PINS[i],
                LOW
            );
        }
    }
}


// =====================================================
// START SOLENOID CYCLE
// =====================================================

void startSolenoidCycle()
{
    if (brailleLength == 0)
    {
        clearSolenoids();

        solenoidState = false;

        return;
    }


    showBrailleCell(
        brailleBuffer[currentCell]
    );


    solenoidState = true;

    solenoidTimer = millis();
}


// =====================================================
// UPDATE SOLENOID CYCLE
// =====================================================

// 2 seconds ON, 1 second OFF, repeating on the current cell
void updateSolenoidCycle()
{
    if (brailleLength == 0)
    {
        clearSolenoids();

        return;
    }


    unsigned long now =
        millis();


    // =================================================
    // CURRENTLY ON
    // =================================================

    if (solenoidState)
    {
        if (
            now - solenoidTimer >=
            SOLENOID_ON_TIME
        )
        {
            clearSolenoids();

            solenoidState = false;

            solenoidTimer = now;
        }
    }


    // =================================================
    // CURRENTLY OFF
    // =================================================

    else
    {
        if (
            now - solenoidTimer >=
            SOLENOID_OFF_TIME
        )
        {
            showBrailleCell(
                brailleBuffer[currentCell]
            );

            solenoidState = true;

            solenoidTimer = now;
        }
    }
}


// =====================================================
// WARNING DISPLAY
// =====================================================

void updateWarningDisplay()
{
    if (!warningMode)
    {
        return;
    }


    unsigned long now =
        millis();


    if (
        now - warningTimer >=
        WARNING_BLINK_TIME
    )
    {
        warningTimer = now;

        warningVisible =
            !warningVisible;


        if (warningVisible)
        {
            displayReadText(
                "দয়া করে বাংলা লিখুন"
            );
        }
        else
        {
            u8g2.clearBuffer();

            u8g2.sendBuffer();
        }
    }
}


// =====================================================
// START WARNING MODE
// =====================================================

void startWarningMode()
{
    // No Braille output

    brailleLength = 0;

    currentCell = 0;


    // Turn all solenoids OFF

    clearSolenoids();


    solenoidState = false;


    // Enable warning

    warningMode = true;

    warningVisible = true;

    warningTimer = millis();

    if (readMode)
    {
        warningSoundPending = false;

        speakTrack(TRACK_WRONG);  // 0097.mp3

        displayReadText(
            "দয়া করে বাংলা লিখুন"
        );
    }
    else
    {
        warningSoundPending = true;
    }
}


// =====================================================
// STOP WARNING MODE
// =====================================================

void stopWarningMode()
{
    warningMode = false;

    warningVisible = false;

    warningSoundPending = false;                   
}


// =====================================================
// PRINT CURRENT CELL
// =====================================================

void printCurrentCell()
{
    if (brailleLength == 0)
    {
        return;
    }


    uint8_t mask =
        brailleBuffer[currentCell];


    Serial.print("Cell ");
    Serial.print(currentCell + 1);
    Serial.print("/");
    Serial.print(brailleLength);

    Serial.print("   Dots: ");


    bool anyDot = false;


    for (
        uint8_t d = 1;
        d <= 6;
        d++
    )
    {
        if (mask & dotMask(d))
        {
            Serial.print(d);
            Serial.print(" ");

            anyDot = true;
        }
    }


    if (!anyDot)
    {
        Serial.print("BLANK");
    }


    Serial.println();
}


// =====================================================
// SHOW CURRENT CELL
// =====================================================

void showCurrentCell()
{
    if (brailleLength == 0)
    {
        clearSolenoids();

        return;
    }


    // Start fresh 2-second ON period

    startSolenoidCycle();


    printCurrentCell();
}


// =====================================================
// NEXT CELL
// =====================================================

void nextCell()
{
    if (brailleLength == 0)
    {
        return;
    }


    if (
        currentCell <
        brailleLength - 1
    )
    {
        currentCell++;

        showCurrentCell();
    }
    else
    {
        Serial.println(
            "Already at last cell."
        );
    }
}


// =====================================================
// PREVIOUS CELL
// =====================================================

void previousCell()
{
    if (brailleLength == 0)
    {
        return;
    }


    if (currentCell > 0)
    {
        currentCell--;

        showCurrentCell();
    }
    else
    {
        Serial.println(
            "Already at first cell."
        );
    }
}


// =====================================================
// PRINT COMPLETE BRAILLE BUFFER
// =====================================================

void printBrailleBuffer()
{
    Serial.println();

    Serial.print(
        "Total Braille cells: "
    );

    Serial.println(
        brailleLength
    );


    Serial.println(
        "Complete Braille buffer:"
    );


    for (
        uint16_t i = 0;
        i < brailleLength;
        i++
    )
    {
        Serial.print("[");


        uint8_t mask =
            brailleBuffer[i];


        bool anyDot = false;


        for (
            uint8_t d = 1;
            d <= 6;
            d++
        )
        {
            if (
                mask & dotMask(d)
            )
            {
                Serial.print(d);
                Serial.print(",");

                anyDot = true;
            }
        }


        if (!anyDot)
        {
            Serial.print("BLANK");
        }


        Serial.print("] ");


        if (
            (i + 1) % 10 == 0
        )
        {
            Serial.println();
        }
    }


    Serial.println();
}


// =====================================================
// RECEIVE TEXT FROM WEBPAGE
// =====================================================

void handleSend()
{
    if (server.hasArg("text"))
    {
        String receivedText =
            server.arg("text");


        // -------------------------------------------------
        // Convert new lines to spaces
        // -------------------------------------------------

        receivedText.replace(
            "\r\n",
            " "
        );

        receivedText.replace(
            "\r",
            " "
        );

        receivedText.replace(
            "\n",
            " "
        );


        // -------------------------------------------------
        // Remove leading/trailing spaces
        // -------------------------------------------------

        receivedText.trim();

        receivedText.replace(
            "\xE2\x80\x8C",
            ""
        );


        // -------------------------------------------------
        // Serial output
        // -------------------------------------------------

        Serial.println();
        Serial.println(
            "======================================"
        );

        Serial.println(
            "New text received"
        );

        Serial.println(
            "======================================"
        );


        Serial.print(
            "Received: "
        );

        Serial.println(
            receivedText
        );


        // =================================================
        // BANGLA CHECK
        // =================================================

        if (!isValidBanglaText(receivedText))
        {
            Serial.println(
                "Input is not fully Bangla."
            );


            Serial.println(
                "Braille output disabled."
            );

            startWarningMode();


            // Do not translate.

        }
        else
        {
            // =================================================
            // Valid Bangla input
            // =================================================

            stopWarningMode();


            // Keep the text for READ mode          

            readModeText = receivedText;


            // =================================================
            // 1. DISPLAY TEXT ON OLED (READ mode only)
            // =================================================

            if (readMode)                         
            {
                displayReadText(
                    receivedText
                );
            }


            // =================================================
            // 2. TRANSLATE SAME TEXT TO BRAILLE
            // =================================================

            translateBangla(
                receivedText
            );


            // =================================================
            // 3. PRINT BRAILLE BUFFER
            // =================================================

            printBrailleBuffer();


            // =================================================
            // 4. START FIRST BRAILLE CELL (READ mode only)
            // =================================================

            if (readMode)                          
            {
                if (brailleLength > 0)
                {
                    currentCell = 0;

                    showCurrentCell();
                }
                else
                {
                    clearSolenoids();

                    Serial.println(
                        "No Braille cells generated."
                    );
                }
            }
            else
            {
                Serial.println(
                    "Stored. Will be shown when READ mode is selected."
                );
            }
        }
    }


    // -------------------------------------------------
    // Return to webpage
    // -------------------------------------------------

    server.sendHeader(
        "Location",
        "/"
    );

    server.send(
        303
    );
}


// =====================================================
// READ MODE LOOP                               
// =====================================================

void readModeLoop()
{
    // =================================================
    // WARNING BLINK
    // =================================================

    updateWarningDisplay();


    // =================================================
    // SOLENOID 2s ON / 1s OFF
    // =================================================

    updateSolenoidCycle();


    // =================================================
    // NEXT BUTTON (ENTER, PCF8575 P01)
    // =================================================

    if (isDown(NEXT_BIT))
    {
        // 30 ms debounce

        delay(
            DEBOUNCE_TIME
        );

        buttonState = readExpander();


        if (isDown(NEXT_BIT))
        {
            nextCell();


            // Wait for release

            while (isDown(NEXT_BIT))
            {
                delay(1);

                buttonState = readExpander();
            }
        }
    }


    // =================================================
    // PREVIOUS BUTTON (BACKSPACE, PCF8575 P00)
    // =================================================

    if (isDown(PREV_BIT))
    {
        // 30 ms debounce

        delay(
            DEBOUNCE_TIME
        );

        buttonState = readExpander();


        if (isDown(PREV_BIT))
        {
            previousCell();


            // Wait for release

            while (isDown(PREV_BIT))
            {
                delay(1);

                buttonState = readExpander();
            }
        }
    }
}


// =====================================================
//                 MODE SWITCHING              
// =====================================================

void enterReadMode()
{
    readMode = true;


    // Mode announcement first                    

    speakTrack(TRACK_READ_MODE);     // 0103.mp3

    Serial.println();
    Serial.println("MODE -> READ  (Bangla to Braille)");
    Serial.println("ENTER = NEXT, BACKSPACE = PREVIOUS");


    if (warningMode)
    {
        warningVisible = true;

        warningTimer = millis();


        // Warning arrived while in WRITE mode:
        // play 0097 once, AFTER the mode sound. 

        if (warningSoundPending)
        {
            warningSoundPending = false;

            speakTrackNext(TRACK_WRONG);  // 0097.mp3
        }


        displayReadText(
            "দয়া করে বাংলা লিখুন"
        );
    }
    else
    {
        displayReadText(readModeText);
    }


    // Resume on the same cell (clears solenoids if the buffer is empty).

    showCurrentCell();
}


// -----------------------------------------------------
// Enter WRITE mode
// -----------------------------------------------------

void enterWriteMode()
{
    readMode = false;


    // Mode announcement                         

    speakTrack(TRACK_WRITE_MODE);    // 0104.mp3

    clearSolenoids();

    solenoidState = false;


    Serial.println();
    Serial.println("MODE -> WRITE (Braille to Bangla)");


    if (pendingConjunct != 0 && conjunctBuffer.length() > 0)
    {
        displayConjunctPreview();
    }
    else
    {
        displayText();
    }
}


// -----------------------------------------------------
// MODE switch (P17, latching)
//
// LOW  (latched)  -> READ  mode
// HIGH (released) -> WRITE mode
//
// -----------------------------------------------------

bool handleModeButton()
{
    static bool lastLevel = false;

    static unsigned long levelSince = 0;


    bool latched = isDown(MODE_BIT);

    unsigned long now = millis();


    // Restart the stability timer on every level change

    if (latched != lastLevel)
    {
        lastLevel = latched;

        levelSince = now;
    }


    // Stable and different from the current mode?

    if (
        latched != readMode &&
        now - levelSince >= MODE_DEBOUNCE
    )
    {
        if (latched)
            enterReadMode();
        else
            enterWriteMode();

        return true;
    }


    return false;
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);


    // =================================================
    // I2C
    // =================================================
    //
    // GPIO 21 = SDA
    // GPIO 22 = SCL
    //
    // Shared by OLED and PCF8575
    //
    // =================================================

    Wire.begin(21, 22);


    // -------------------------------------------------
    // PCF8575
    // -------------------------------------------------

    for (uint8_t i = 0; i < 16; i++)
        pcf.pinMode(i, INPUT);

    if (pcf.begin())
    {
        Serial.println("PCF8575 found at 0x20");
    }
    else
    {
        Serial.println("PCF8575 NOT found! Check wiring.");
    }


    // OLED

    u8g2.begin();

    u8g2.clearBuffer();


    // -------------------------------------------------
    // Bus speed
    // -------------------------------------------------

    Wire.setClock(100000);


    // Font

    font = Kalpurush_20pt;

    textRenderer = new BanglaTextRenderer(&font);


    // =================================================
    // SOLENOIDS (all down)
    // =================================================

    for (uint8_t i = 0; i < 6; i++)
    {
        pinMode(SOLENOID_PINS[i], OUTPUT);

        digitalWrite(SOLENOID_PINS[i], LOW);
    }


    // =================================================
    // Wi-Fi ACCESS POINT (always on)
    // =================================================

    WiFi.softAP(ssid, password);

    Serial.println();
    Serial.println("Wi-Fi started!");

    Serial.print("Network: ");
    Serial.println(ssid);

    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());


    // =================================================
    // WEB SERVER (always on)
    // =================================================

    server.on("/", handleRoot);

    server.on("/send", handleSend);

    server.begin();

    Serial.println("Web server started!");


    // -------------------------------------------------
    // Initial mode follows the latching switch  
    // -------------------------------------------------

    buttonState = readExpander();

    if (isDown(MODE_BIT))
    {
        enterReadMode();
    }
    else
    {
        readMode = false;

        displayText();
    }


    // Serial

    Serial.println();
    Serial.println("========================================");
    Serial.println("Bangla Braille Device Ready");
    Serial.println("========================================");
    Serial.println();
    Serial.println("MODE switch = P17 (latching)");
    Serial.println(" P17 released (HIGH): WRITE mode, Braille -> Bangla");
    Serial.println(" P17 latched  (LOW) : READ  mode, Bangla  -> Braille");
    Serial.print(" Current mode: ");
    Serial.println(readMode ? "READ" : "WRITE");
    Serial.println();
    Serial.println("---------- WRITE mode ----------");
    Serial.println("ENTER=P01  BACKSPACE=P00");
    Serial.println();
    Serial.println("ENTER short press = confirm pattern");
    Serial.println("ENTER short press, no dots = SPACE");
    Serial.println("ENTER hold >= 2 sec = NEW LINE");
    Serial.println();
    Serial.println("P00 short press = BACKSPACE");
    Serial.println("P00 hold >= 2 sec = CLEAR ALL");
    Serial.println();
    Serial.println("---------- READ mode -----------");
    Serial.println("P01 (ENTER)     = NEXT cell");
    Serial.println("P00 (BACKSPACE) = PREVIOUS cell");
    Serial.println("Invalid web text -> warning + sound 0097");
    Serial.println("========================================");


    // Audio (takes ~2 s; the keyboard works without it)

    speakerBegin();    // <<< SPEAKER


    // Announce the starting mode once            

    speakTrack(readMode ? TRACK_READ_MODE : TRACK_WRITE_MODE);
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
    // =================================================
    // WEB SERVER - always, in both modes  
    // =================================================

    server.handleClient();


    // =================================================
    // READ ALL BUTTONS
    // =================================================

    buttonState = readExpander();


    // =================================================
    // MODE BUTTON (P17)               
    // =================================================

    bool modeChanged = handleModeButton();


    // =================================================
    // ACTIVE MODE                          
    // =================================================

    if (!modeChanged)
    {
        if (readMode)
            readModeLoop();
        else
            writeModeLoop();
    }


    // Send the latest requested sound 

    speakerService();    // <<< SPEAKER


    // Original keyboard loop delay (WRITE mode only)

    if (!readMode)
        delay(5);
}
