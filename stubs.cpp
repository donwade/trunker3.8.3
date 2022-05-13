#include <platform.h>
#include <osw.h>
#include <trunksys.h>
#include <time.h>

#define WORDSPERSEC ((2 * (9600)) / (48 + 120 + 120))
int squelchDelay = WORDSPERSEC / 2; // WANT
// int  wordsPerSec = WORDSPERSEC;
BOOL syncState = FALSE;             // WANT


// FreqMap  *myFreqMap = 0;
TrunkSys  *mySys = 0;           // WANT

unsigned long goodcount = 0;    // WANT
unsigned long badcount = 0;     // WANT
//char  tempArea [512];
//unsigned short   sysId = 0,  cwFreq = 0;
//long  theDelay = DELAY_ON;
BOOL Verbose = TRUE;            // WANT
BOOL showFrameSync = FALSE;     // WANT
//unsigned long  stackOverFlows = 0L;
unsigned long wrapArounds = 0L; // WANT

// global variables
char *sysType = "unknown";      // WANT
volatile BOOL InSync = FALSE;   // WANT

