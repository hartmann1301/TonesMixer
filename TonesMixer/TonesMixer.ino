#include <Arduboy2.h>
#include <ArduboyTones.h>
#include <math.h>

#include "TonesArray.h"

#define MAX_TONES 100
#define PRINT_TONES 57600

#ifdef ESP8266
#define PS2_DAT       D6 // brown/green
#define PS2_CMD       D0 // orange 
#define PS2_SEL       D5 // yellow
#define PS2_CLK       D8 // blue

#include <PS2X_lib.h>
#include <SSD1306Brzo.h>

PS2X ps2x;
SSD1306Brzo oled(0x3C, D2, D1);
#endif

Arduboy2 arduboy;
ArduboyTones sound(arduboy.audio.enabled);

#ifdef ESP8266
void getButtons() {
  arduboy.setExternalButtons(ps2x.getArduboyButtons());
}
#endif

uint32_t nextButtonPress = 0;
uint32_t nextTone = 0;
uint32_t startPlaying = 0;

bool isRightOff = false;
bool isNoteMode = false;
bool isPlayingTones = false;
int8_t playingIndex = 0;

int8_t pressedUp = 0;
int8_t pressedDown = 0;

struct note {
  uint8_t frequencyIndex = 50;
  int16_t duration = 200;
  bool active = false;
};

uint8_t notesCursor = 0;
uint8_t notesLength = 0;
note notes[MAX_TONES];

uint8_t addNoteCnt = 0;
uint16_t lastFrequencyAdded = 0;

void addShortRest() {

  addNote(int16_t(0), 50);
}


void addNote(char* n, int16_t d) {

  uint8_t fIndex = getFrequencyIndex(n);
  uint16_t frequency = getFrequency(fIndex);
  
  // add am small Rest after same notes
  if (lastFrequencyAdded == frequency)
    addShortRest();

  addNote(fIndex, d);

  lastFrequencyAdded = frequency;
}

void addNote(int16_t fi, int16_t d) {

  notes[addNoteCnt].frequencyIndex = fi;
  notes[addNoteCnt].duration = d;
  notes[addNoteCnt].active = true;

  notesLength = max(addNoteCnt, notesLength);
  addNoteCnt++;
}

int16_t getFrequency(uint8_t frequencyIndex) {
  return pgm_read_word(tonesFrequencys + frequencyIndex);
}

char* getFrequencyName(uint8_t frequencyIndex) {
  static char noteName[5] = "";

  for (int8_t i = 0; i < 4; i++) {
#ifdef ESP8266
    noteName[i] = tonesNames[frequencyIndex * 4 + i];
#else
    noteName[i] = pgm_read_byte(tonesNames + frequencyIndex * 4 + i);
#endif
  }

  noteName[4] = '\0';

  return noteName;
}

uint8_t getFrequencyIndex(char* noteName) {

  for (int8_t i = 0; i < TONES_ARRAY_LENGTH; i++) {

    String string1 = String(getFrequencyName(i));
    String string2 = String(noteName);

    string1.trim();
    string2.trim();

    //Serial.println("is " + string1 + " equal to " + string2);
    if (string1 == string2) {
      return i;
    }
  }
  Serial.print("warning could not find ");
  Serial.println(noteName);
  return 0;
}

void setFrequency(int8_t diff) {
  uint8_t index = notes[notesCursor].frequencyIndex;

  //Serial.println("diff " + String(diff) + " index " + String(index));

  if (index + diff < 0)
    diff = -index;

  if (index + diff > TONES_ARRAY_LENGTH - 1) {
    diff = 0;
    index = TONES_ARRAY_LENGTH - 1;
  }

  index += diff;

  notes[notesCursor].frequencyIndex = index;

  //Serial.println("set index:" + String(index) + " is frequenz:" + String(getFrequency(index)));

  // play example
  sound.tone(getFrequency(index), notes[notesCursor].duration);
}

int16_t getDuration() {
  int16_t dur = 0;

  for (uint8_t i = 0; i < MAX_TONES; i++) {
    if (notes[i].active == true)
      dur += notes[i].duration;
  }

  return dur;
}

void setDuration(int8_t diff) {
  notes[notesCursor].duration += diff;
  notes[notesCursor].duration = constrain(notes[notesCursor].duration, 50, 2000);
}

void checkPlayingTones() {
  if (!isPlayingTones)
    return;

  if (millis() < nextTone)
    return;

  if (notesLength + 1 == playingIndex) {
    sound.noTone();
    //Serial.println(String(millis()) + " done index:" + String(playingIndex));

    playingIndex = 0;
    isPlayingTones = false;
    return;
  }

  int16_t f = getFrequency(notes[playingIndex].frequencyIndex);
  int16_t d = notes[playingIndex].duration;

  //Serial.println(String(millis()) + " tone(" + String(f) + ", " + String(d));
  sound.tone(f, d);

  nextTone = millis() + notes[playingIndex].duration;
  playingIndex++;
}

#ifdef PRINT_TONES
void printTones() {
  Serial.print(F("array = {"));

  for (uint8_t i = 0; i < MAX_TONES; i++) {
    if (notes[i].active == false)
      continue;

    String freqencyName = getFrequencyName(notes[i].frequencyIndex);
    freqencyName.trim();
    // use the frequency numbers

    // use defined note names instead of int value
    //Serial.print(getFrequency(notes[i].frequencyIndex), DEC);
    Serial.print(freqencyName);

    Serial.print(F(", "));
    Serial.print(notes[i].duration, DEC);

    if (i < MAX_TONES - 1) {
      if (notes[i + 1].active)
        Serial.print(F(", "));
    }
  }

  Serial.println(F("};"));
}
#endif

#define LINE_1 25
#define LINE_2 35
#define LINE_3 45
#define TONE_AREA 20

void draw() {

  float xPos = 1;
  float fullDuration = getDuration();

  int16_t toneHighest = 16;
  int16_t toneLowest = 15804;
  for (uint8_t i = 0; i <= notesLength; i++) {
    int16_t tmpFrequency = getFrequency(notes[i].frequencyIndex);

    if (tmpFrequency == 0)
      continue;

    toneHighest = max(toneHighest, tmpFrequency);
    toneLowest = min(toneLowest, tmpFrequency);
  }
  //Serial.println("hightest:" + String(toneHighest) + " lowest:" + String(toneLowest));

  if (isPlayingTones) {
    float percent = float(millis() - startPlaying) / fullDuration;

    arduboy.fillRect(percent * 126 + 1, 0, 1, TONE_AREA - 1, WHITE);
  }

  float logLow = log(toneLowest) - 0.1;
  float logHigh = log(toneHighest) + 0.1;
  //Serial.println("logLow:" + String(logLow) + " logHigh:" + String(logHigh));

  for (uint8_t i = 0; i <= notesLength; i++) {
    int16_t frequency = getFrequency(notes[i].frequencyIndex);
    float len = (float(notes[i].duration) / fullDuration) * 126;

    float logDiff = logHigh - logLow;
    float logHelper = (log(frequency) - logLow) / logDiff * TONE_AREA;

    uint8_t yPos = TONE_AREA - uint8_t(logHelper);

    //Serial.println("logDiff:" + String(logDiff) + " frequency:" + String(frequency));
    //Serial.println("logHelper:" + String(logHelper) + " yPos:" + String(yPos));

    if (frequency == 0)
      yPos = TONE_AREA - 1;

    if (i == notesCursor) {
      if (!isNoteMode || arduboy.everyXFrames(2))
        arduboy.fillRect(xPos, yPos - 1, max(len, 1), 3, WHITE);
    } else {
      if (frequency != 0)
        arduboy.drawLine(xPos, yPos, xPos + max(len, 1), yPos, WHITE);
    }

    xPos += len;
  }

  arduboy.drawLine(0, 0, 0, TONE_AREA - 1, WHITE);

  if (!isRightOff || arduboy.everyXFrames(2))
    arduboy.drawLine(127, 0, 127, TONE_AREA - 1, WHITE);

  arduboy.setCursor(0, LINE_1);
  arduboy.print(F("Note:"));

  arduboy.setCursor(83, LINE_1);
  arduboy.print(notesCursor + 1, DEC);

  arduboy.setCursor(95, LINE_1);
  arduboy.print('/');

  arduboy.setCursor(105, LINE_1);
  arduboy.print(notesLength + 1, DEC);

  arduboy.setCursor(0, LINE_2);
  if (isRightOff) {
    arduboy.print(F("  Up: add Note"));

  } else {
    arduboy.print("Freqency:");

    arduboy.setCursor(65, LINE_2);
    arduboy.print(getFrequency(notes[notesCursor].frequencyIndex), DEC);
    arduboy.print(F("Hz "));
    arduboy.print(getFrequencyName(notes[notesCursor].frequencyIndex));
  }

  arduboy.setCursor(0, LINE_3);
  if (isRightOff) {
    arduboy.print(F("Down: delete Last"));

  } else {
    arduboy.print(F("Duration:"));

    arduboy.setCursor(85, LINE_3);
    arduboy.print(notes[notesCursor].duration, DEC);
    arduboy.print(F("ms"));
  }

  if (!isRightOff) {
    arduboy.setCursor(0, 56);
    arduboy.print("A:");
    if (isNoteMode) {
      arduboy.print("Back");
    } else {
      arduboy.print("Choose");
    }
  }

  if (!isNoteMode) {
    arduboy.setCursor(57, 56);
    arduboy.print("B:Play ");
    arduboy.print(fullDuration / 1000, 1);
    arduboy.print('s');
  }
}

void startPlayingTones() {
#ifdef PRINT_TONES
  printTones();
#endif

  startPlaying = millis();

  //Serial.println(String(millis()) + " Start Playing Tones");
  isPlayingTones = true;
  playingIndex = 0;
}

void deleteLastNote() {
  if (notesCursor == 1)
    return;

  //Serial.println("deleteLastNote " + String(notesLength + 1));

  notes[notesLength].active = false;
  notesLength--;
  notesCursor--;
}

void addNote() {
  if (notesCursor == MAX_TONES - 1)
    return;

  notesCursor++;
  notesLength++;
  notes[notesLength].active = true;

  //Serial.println("addNote " + String(notesLength + 1));
}

void checkPressTime(int8_t &buttonVar, bool currState) {
  if (currState) {
    if (buttonVar < 1) {
      buttonVar = 1;
    } else if (buttonVar < 127) {
      buttonVar++;
    }
  } else {
    if (buttonVar > -1) {
      buttonVar = -1;
    } else if (buttonVar > -127) {
      buttonVar--;
    }
  }
}

void checkButtonsNote() {

  checkPressTime(pressedUp, arduboy.pressed(UP_BUTTON));
  checkPressTime(pressedDown, arduboy.pressed(DOWN_BUTTON));

  //Serial.println(String(millis()) + " "  + String(pressedUp));

  if (arduboy.pressed(UP_BUTTON))
    setFrequency(1 + pressedUp / 4);

  if (arduboy.pressed(DOWN_BUTTON))
    setFrequency(-(1 + pressedDown / 4));

  if (arduboy.pressed(LEFT_BUTTON))
    setDuration(-50);

  if (arduboy.pressed(RIGHT_BUTTON))
    setDuration(50);

  //if (arduboy.pressed(B_BUTTON))
  //TODO: toggle loudness
}

void checkButtonsMenu() {

  if (arduboy.pressed(UP_BUTTON) && isRightOff)
    addNote();

  if (arduboy.pressed(DOWN_BUTTON) && isRightOff)
    deleteLastNote();

  if (arduboy.pressed(LEFT_BUTTON) && notesCursor > 0)
    notesCursor--;

  if (arduboy.pressed(RIGHT_BUTTON) && notesCursor <= notesLength)
    notesCursor++;

  if (arduboy.pressed(B_BUTTON)) {

    if (isPlayingTones) {
      isPlayingTones = false;
    } else {
      startPlayingTones();
    }
  }

  isRightOff = (notesLength == notesCursor - 1);
}

void checkButtons() {

  if (millis() < nextButtonPress)
    return;

  if (isNoteMode) {
    checkButtonsNote();

  } else {
    checkButtonsMenu();
  }

  if (arduboy.pressed(A_BUTTON) && !isRightOff)
    isNoteMode = !isNoteMode;

  nextButtonPress = millis() + 100;
}

void addAlleMeineEntchen() {
  addNote("C4", 250);
  addNote("D4", 250);
  addNote("E4", 250);
  addNote("F4", 250);

  addNote("G4", 500);
  addNote("G4", 500);

  addNote("A4", 250);
  addNote("A4", 250);
  addNote("A4", 250);
  addNote("A4", 250);

  addNote("G4", 1000);

  addNote("A4", 250);
  addNote("A4", 250);
  addNote("A4", 250);
  addNote("A4", 250);

  addNote("G4", 1000);

  addNote("F4", 250);
  addNote("F4", 250);
  addNote("F4", 250);
  addNote("F4", 250);

  addNote("E4", 500);
  addNote("E4", 500);

  addNote("G4", 250);
  addNote("G4", 250);
  addNote("G4", 250);
  addNote("G4", 250);

  addNote("C4", 1000);
}

void setup() {

#ifdef PRINT_TONES
  Serial.begin(PRINT_TONES);
  Serial.println(F("TonesMixer"));
#endif

#ifdef ESP8266
  ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, false, false);

  arduboy.setExternalButtonsHandler(getButtons);
#endif

  addAlleMeineEntchen();

  arduboy.begin();
  arduboy.audio.on();
  arduboy.setFrameRate(30);
}

void loop() {
  checkPlayingTones();

  if (!(arduboy.nextFrame()))
    return;

  //uint32_t start = millis();

  arduboy.fillScreen(BLACK);

  checkButtons();

  draw();

  //arduboy.setCursor(2, 0);
  //arduboy.print(millis() - start);

  arduboy.display();
}
