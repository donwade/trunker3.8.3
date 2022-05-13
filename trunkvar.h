//
//        trunkvar.h - declarations of global variables. given the heritage and execution
//                                  environment of this program, they seem like a necessary evil...
//
// This flags are associated with the current value of options as turned on/off by keystrokes

extern long theDelay;   // delay time in 90ths of second, zero if disables
// the values it takes on are:
#define DELAY_ON  (wordsPerSec << 1)
#define DELAY_OFF 0

extern BOOL Verbose;                        // TRUE means show individual calling IDs, else not
extern BOOL seekNew;                        // TRUE means slew priority toward new entries
extern BOOL showFrameSync;                  // frame indicator
extern volatile BOOL InSync;
extern BOOL syncState;
extern BOOL allowLurkers;

// these globals keep information about screen characteristics

extern unsigned short actualRows;

// Counters related to quality and performance

extern unsigned long goodcount,  badcount;      // number of good/bad OSWs received
extern unsigned long wrapArounds;               // if you get these, you need a faster CPU

// rather than doing time() a lot, is done periodically and stuck in a variable:


// a 'fine grain' clock is maintained by simply counting OSWs

extern time_t nowFine;

// other globals:
class TrunkSys;
extern TrunkSys * mySys;

extern char * mapOtherCodeToF(unsigned short);
extern volatile unsigned char pinToRead;
extern char wantstring[20];
extern int newRadio;
extern int newGroup;


