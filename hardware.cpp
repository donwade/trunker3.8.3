#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <termios.h>
#include <missing.h>

#include <ptrmap.h>
#include <trunkvar.h>
#include <scanner.h>
#include <unistd.h>
#include <platform.h>

#include "missing.h"
#include "debug.h"
extern void close_telnet(void);
int close_syslog(void);

extern ComPort *mySlicerPort, *myScannerPort;
extern Scanner *myScanner;
static BOOL exiting = FALSE;
unsigned short ltick = 0;
unsigned short lastbit = 0;
static struct termios orig_tios;
extern char FINAL_REASON[];
static char bCleanupDone = 0;


void docleanup(void)
{
	if (bCleanupDone == 0)
	{
		bCleanupDone = 1;
		lprintf1("docleanup called from atexit");
		
	    if (!exiting)
	    {
	        exiting = TRUE;
	        localCleanup();

	        if (myScanner)
	        {
	            myScanner->shutDown();
	            delete myScanner;
	            myScanner = 0;
	        }

	        if (mySlicerPort)
	        {
	            if (myScannerPort && mySlicerPort != myScannerPort)
	                delete myScannerPort;

	            myScannerPort = 0;
	            delete mySlicerPort;
	            mySlicerPort = 0;
	        }
	        else
	        {
	            if (myScannerPort)
	            {
	                delete myScannerPort;
	                myScannerPort = 0;
	            }
	        }
	    }

		lprintf1("terminal settings being restored");
		
	    tcsetattr(STDOUT_FILENO, TCSADRAIN, &orig_tios);

		sleep(4);
		
		//close_telnet(); moved to at exit
		//close_syslog();

	    lprintf1("CAUTION: more ncurses being called" );
	    settextcursor((short)0x0707);
	    _wrapon(_GWRAPON);
	    _setvideomoderows(_DEFAULTMODE, 25);
	    _setbkcolor(color_obk);

		clear();
		refresh();
		delwin(stdscr);
		endwin();
		lprintf1("endwin called. no further ncurses calls please");

		close_telnet();
		close_syslog();
		
		printf("\n\'%s\'\n", FINAL_REASON);
	}
	
#undef exit
	exit(1);
}

//
//  set up the 8253 timer chip.  NOTE: ctr zero, the one we are using is
//  incremented every 840nSec, is main system time keeper for dos set ctr
//  0 to mode 2, binary this gives us the max count
//
void set8253()
{
    outportb(0x43, 0x34);
    outportb(0x40, 0x00);
    outportb(0x40, 0x00);
}


void hardwareInit()
{
    struct termios tios;
    /* get current terminal settings, set raw mode, make sure we
     * register atexit handler to restore terminal settings
     */
    tcgetattr(STDOUT_FILENO, &orig_tios);
    atexit(docleanup);
    tios = orig_tios;
    cfmakeraw(&tios);
    tcsetattr(STDOUT_FILENO, TCSADRAIN, &tios);
	lprintf1("terminal init set to raw");
	
    set8253();
}
