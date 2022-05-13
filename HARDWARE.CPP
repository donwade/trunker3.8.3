#include <platform.h>

extern ComPort *mySlicerPort, *myScannerPort;
extern Scanner *myScanner;
static BOOL exiting = FALSE;
unsigned short ltick = 0;
unsigned short lastbit = 0;

void docleanup()
{
        if( !exiting ) {
                exiting = TRUE;
                localCleanup();
		if(myScanner)
		{
                	myScanner->shutDown();
                	delete myScanner;
			myScanner = 0;
		}
		if( mySlicerPort )
		{
                	if( myScannerPort && mySlicerPort != myScannerPort ) {
                        	delete myScannerPort;
                	}
			myScannerPort = 0;
	                delete mySlicerPort;
			mySlicerPort = 0;
		} else {
		    	if( myScannerPort ) {
			    delete myScannerPort;
			    myScannerPort = 0;
			}
		}
        }
        _settextcursor ((short) 0x0707);
        _wrapon (_GWRAPON);
        _setvideomoderows (_DEFAULTMODE,25);
	_setbkcolor(color_obk);
}


//
//  set up the 8253 timer chip.  NOTE: ctr zero, the one we are using is 
//  incremented every 840nSec, is main system time keeper for dos set ctr 
//  0 to mode 2, binary this gives us the max count 
//
void 
set8253 ()                
{                              
  outportb (0x43, 0x34);      
  outportb (0x40, 0x00);       
  outportb (0x40, 0x00);
}

void
hardwareInit ()
{
        atexit (docleanup);
        set8253 ();
}
