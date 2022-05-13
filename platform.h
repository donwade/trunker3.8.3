//
//        This provides the inteface between a 'standard' main for writing RF signal processing
//        programs and the application-dependent code.
//

// standard includes:

#include <conio.h>
#include <string.h>
#include <dos.h>
#include <graph.h>
#include <sys/types.h>
#include <stdlib.h>
#include <malloc.h>
typedef int BOOL;
typedef short WORD;
#define TRUE    1
#define FALSE   0
#include <stdio.h>
#define strcat  _fstrcat
#define NOFLAGS -1
#include <trunkvar.h>
extern void shouldNever(const char * cp);
// includes critical to the standard interface:
#include <ptrmap.h>
#include <comport.h>
#include <freqasg.h>
#include <scanner.h>
#include <scrlog.h>
#include <colors.h>

// globals that are critical to the standard interface:

extern ComPort * mySlicerPort;
extern Scanner * myScanner;
extern volatile BOOL slicerInverting;
extern BOOL doEmergency;
extern BOOL trackDigital;
extern BOOL filterLog;
extern volatile unsigned char pinToRead;
extern char wantstring[20];
extern char scanTypeEnv [64];
extern char scanBaudEnv [64];
extern char scanPortEnv [64];
extern char slicerPortEnv [64];
extern char slicerModeEnv [64];
extern char slicerPinEnv[64];
extern unsigned short actualRows;  // on video screen

extern void __far __interrupt com1int(void);
extern int mainLoop();
extern void hardwareInit();
extern void send_char(char);
extern void set8250(double bps);
extern void localCleanup();
extern void prepareFile(const char * tfn);
// communication of data is via this array and a counter into it

#define BufLen   8192
extern volatile unsigned short fdata [BufLen + 1];
extern unsigned short bitsAtZero;
extern unsigned short bitsAtOne;
extern volatile unsigned int cpstn;       // current position in buffer

// watcom

#define outportb outp
#define outport  outpw
#define inportb  inp
#define inport   inpw
#define clrscr()              _clearscreen(0)
#define setvect  _dos_setvect
#define getvect  _dos_getvect
#define gotoxy(x, y)          _settextposition(y, x)
// microsoft
# define textcolor(x)         _settextcolor(x)
# define QuickOut(y, x, c, t) { _settextposition(((short)(y)), ((short)(x))); _settextcolor(((short)(c))); _outtext(((char*)(t))); }
extern unsigned short ltick;
extern unsigned short lastbit;
extern unsigned short
    color_obk,              // = BLACK?
    color_quiet,            // = GREEN
    color_bg,               // = BLACK
    color_norm,             // = WHITE
    color_warn,             // = YELLOW
    color_active,           // = YELLOW
    color_err;              // = LIGHTRED



