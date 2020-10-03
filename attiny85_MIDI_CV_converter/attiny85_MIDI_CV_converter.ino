#include <ReceiveOnlySoftwareSerial.h>

// MIDI to CV using ATTiny85
// NB: Use Sparkfun USB ATTiny85 Programmer
//     Set Arduino env to USBTinyISP
//     Set to 8MHz Internal Clock (required for MIDI baud)

#define MIDIRX 4  // 4=PB4/D4 in Arduino terms = Pin 3 for ATTiny85
//#define MIDITX 3  // 3=PB3/D3 in Arduino terms = Pin 2 for ATTiny85
#define MIDICH 1
#define MIDILONOTE 36
#define MIDIHINOTE 96

// Output:
//  PB2 (Ardiuno) = Pin 7 = Gate Output
//  PB1 (Arduino) = Pin 6 = Pitch CV Output
//
// PB5 set as digital output
// PB1 used as PWM output for Timer 1 compare OC1A
#define GATE    2  // PB2 (Pin 7) Gate
#define PITCHCV 1  // PB1 (Pin 6) Pitch CV

#define LED 0

byte notestore;

ReceiveOnlySoftwareSerial midiSerial(MIDIRX);      // nick gammon's library. use this; serial works much better with this.

void setup() {
  midiSerial.begin (31250); // MIDI Baud rate

  pinMode (GATE, OUTPUT);
  pinMode (PITCHCV, OUTPUT);
  pinMode (LED, OUTPUT);

  // Use Timer 1 for PWM output based on Compare Register A
  // However, set max compare value to 239 in Compare Register C
  // This means that output continually swings between 0 and 239
  // MIDI note ranges accepted are as follows:
  //    Lowest note = 36 (C2)
  //    Highest note = 96 (C7)
  // So there are 60 notes that can be received, thus making each
  // PWM compare value 240/60 i.e. steps of 4.
  //
  // So, for each note received, PWM Compare value = (note-36)*4.
  //
  // Timer 1 Control Register:
  //   PWM1A = PWM based on OCR1A
  //   COM1A1 = Clear OC1A (PB1) output line
  //   CS10 = Prescaler = PCK/CK i.e. run at Clock speed
  //   PWM1B is not enabled (GTCCR = 0)
  //
  TCCR1 = _BV(PWM1A)|_BV(COM1A1)|_BV(CS10);
  GTCCR = 0;
  OCR1C = 239;
  OCR1A = 0; // Initial Pitch CV = 0 (equivalent to note C2)
  digitalWrite(GATE,LOW); // Initial Gate = low
}

void setTimerPWM (uint16_t value) {
  OCR1A = value;
}

void loop() {
  do{
    if (midiSerial.available()) {
      // pass any data off to the MIDI handler a byte at a time
      doMIDI (midiSerial.read());
    } 
  }
  while (midiSerial.available() > 2);//when at least three bytes available
}

uint8_t MIDIRunningStatus=0;    //status bit.
uint8_t MIDINote=0;         //note databit 1
uint8_t MIDILevel=0;        //note databit2
uint8_t pitchFine=0;        //pitch databit1
uint8_t pitchCourse=0;        //pitch databit2
uint16_t pitchFull=8192;
uint8_t pitchAmt=0;
uint8_t ccbyte=0;         //if received ; sustain databit1
uint8_t sustainbyte = 0;      //sustain databit2
bool pitchFinebool=false;     //if received pitch databit1
bool sustaining = false;      //to see if we hold the notes or not
bool noteplaying = false;     //to see if we hold the notes or not.
bool ourchannel = false;    //fix to other channel interference glitch

void doMIDI (uint8_t midibyte) {
  // MIDI supports the idea of Running Status.
  // If the command is the same as the previous one, 
  // then the status (command) byte doesn't need to be sent again.
  //
  // The basis for handling this can be found here:
  //  http://midi.teragonaudio.com/tech/midispec/run.htm
  //
  // copied below:
  //   Buffer is cleared (ie, set to 0) at power up.
  //   Buffer stores the status when a Voice Category Status (ie, 0x80 to 0xEF) is received.
  //   Buffer is cleared when a System Common Category Status (ie, 0xF0 to 0xF7) is received.
  //   Nothing is done to the buffer when a RealTime Category message is received.
  //   Any data bytes are ignored when the buffer is 0.
  //

  if ((midibyte >= 0x80) && (midibyte <= 0xEF)) {    //if it's a status message
    //
    // MIDI Voice category message
    //
    // Start handling the RunningStatus
  
    if ((midibyte & 0x0F) == (MIDICH-1)) {
      // Store, but remove channel information now we know its for us
      MIDIRunningStatus = midibyte & 0xF0;
      ourchannel = true;
      MIDINote = 0;
      MIDILevel = 0;
    pitchFinebool=false;
    ccbyte = 0;
    }
  else {
    ourchannel = false;
      // Not on our channel, so ignore
    }
  }
  
  
  else if (midibyte <= 0x7F && ourchannel) {            //if its a note
    //
    // MIDI Data
    //
    // Note: Channel handling has already been performed
    //       (and removed) above, so only need consider
    //       ourselves with the basic commands here.
    
  if (MIDIRunningStatus == 0x90) {                    //note on
    noteplaying = true;
      if (MIDINote == 0) {
        MIDINote = midibyte;
    notestore = midibyte;
      } 
    else {
        // If we already have a note, assume its the level
        MIDILevel = midibyte;
        
        // Now we have a note/velocity pair, act on it
        if (MIDILevel == 0 && notestore == MIDINote && !sustaining) {
          midiNoteOff (MIDINote);
        }
    else {
          midiNoteOn (MIDINote);
        }
        MIDINote = 0;
        //MIDILevel = 0;
      }
    }


  else if (MIDIRunningStatus == 0x80) {                     //note off
    noteplaying = false;
      // First find the note
      if (MIDINote == 0) {
        MIDINote = midibyte;
      }
    else {
        // If we already have a note, assume its the level
        MIDILevel = midibyte;

        // Now we have a note/velocity pair, act on it
    if (notestore == MIDINote && !sustaining) {
      midiNoteOff (MIDINote);
    }
        MIDINote = 0;
        //MIDILevel = 0;
      }
    }
  
  else if (MIDIRunningStatus == 0xE0){        //pitch bend
    if(pitchFinebool == false){
      pitchFine = midibyte;
      pitchFinebool = true;
    }
    else{
      pitchCourse = midibyte;
      
      //concatenate the bits
      pitchFull = pitchCourse;
      pitchFull = pitchFull << 7;     //?
      pitchFull += pitchFine; 
      
      //assuming midi_note is already calculated
      midiPitchBend(notestore);
      pitchFinebool = false;
      //pitchCourse=0;
      //pitchFull=8192;
    }
  }
  
  else if (MIDIRunningStatus == 0xB0){        //control paramenter s
    if(ccbyte == 0){
      ccbyte = midibyte;
    }
    else{
      sustainbyte = midibyte;
      if(ccbyte == 0x40){         //sustain param
      //setTimerPWM(100);
      if(sustainbyte>63){
        sustaining = true;
      }
      else{
        sustaining = false;
        if(!noteplaying){
          midiNoteOff(notestore);
        }
      } 
      }
      ccbyte = 0;
    }
  }
  
  
  }
}

/****************************************************************************************************/
//void midiNoteOn (byte midi_note, byte midi_level) {
void midiNoteOn (byte midi_note) {
  // check note in the correct range of 36 (C2) to 90 (C7)
  if (midi_note < MIDILONOTE) midi_note = MIDILONOTE;
  if (midi_note > MIDIHINOTE) midi_note = MIDIHINOTE;
  
  // Scale to range 0 to 239, with 1 note = 4 steps
  midi_note = midi_note - MIDILONOTE;
  
  // Set the voltage of the Pitch CV and Enable the Gate
  
  setTimerPWM(midi_note*4);
  if(pitchFull > 8192){//positive
  pitchAmt = (pitchFull - 8192)/1023;
  setTimerPWM(midi_note*4 + pitchAmt);
  }
  else if(pitchFull < 8192){//negative
  pitchAmt = (8192 - pitchFull)/1023;
  setTimerPWM(midi_note*4 - pitchAmt);
  }

  digitalWrite (GATE, HIGH);
  digitalWrite(LED, HIGH);
  
}

//void midiNoteOff (byte midi_note, byte midi_level) {
void midiNoteOff (byte midi_note) {
  // check note in the correct range of 36 (C2) to 90 (C7)
  if (midi_note < MIDILONOTE) midi_note = MIDILONOTE;
  if (midi_note > MIDIHINOTE) midi_note = MIDIHINOTE;
  
  // Scale to range 0 to 239, with 1 note = 4 steps
  midi_note = midi_note - MIDILONOTE;
  
  // Set the voltage of the Pitch CV and Enable the Gate
  digitalWrite (GATE, LOW);
  digitalWrite(LED, LOW);
  /*setTimerPWM(midi_note*4);
  if(pitchFull >= 8192){//positive
  pitchAmt = (pitchFull - 8192)/1023;
  setTimerPWM(midi_note*4 + pitchAmt);
  }
  else if(pitchFull < 8192){//negative
  pitchAmt = (8192 - pitchFull)/1023;
  setTimerPWM(midi_note*4 - pitchAmt);
  }*/
  
}

void midiPitchBend (byte midi_note){
  // check note in the correct range of 36 (C2) to 90 (C7)
  if (midi_note < MIDILONOTE) midi_note = MIDILONOTE;
  if (midi_note > MIDIHINOTE) midi_note = MIDIHINOTE;
  
  // Scale to range 0 to 239, with 1 note = 4 steps
  midi_note = midi_note - MIDILONOTE;
  
  // Set the voltage of the Pitch CV and Enable the Gate
  //pitchAmt = (pitchFull - 8192)/2048;
  //pitchAmt = -4;
  if(pitchFull >= 8192){//positive
  pitchAmt = (pitchFull - 8192)/1023;
  setTimerPWM(midi_note*4 + pitchAmt);
  }
  else if(pitchFull < 8192){//negative
  pitchAmt = (8192 - pitchFull)/1023;
  setTimerPWM(midi_note*4 - pitchAmt);
  }
  
}
