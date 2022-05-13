#include <missing.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>

#include <ptrmap.h>
#include <trunkvar.h>
#include <scanner.h>
#include <platform.h>
//#include <process.h>
#include <layouts.h>

#include <io.h>
#include <dpmiutil.h>
#include "debug.h"
char *TRACKSCAN_PARK = NULL;
unsigned int TRACKTIMER = 0;
extern char chatty;
extern void docleanup(void);

unsigned short
    color_obk = BLACK,          // = original color
    color_quiet = GREEN,
    color_bg = BLACK,
    color_norm = RED,
    color_warn = BRIGHTYELLOW,
    color_active = BRIGHTGREEN,
    color_err = BRIGHTRED;

extern BOOL wantScanner;
extern unsigned int control_channel;

static char tmpArea[150];
ComPort *mySlicerPort = 0;
Scanner *myScanner = 0;

volatile BOOL slicerInverting = FALSE;
volatile unsigned char pinToRead;
char wantstring[20];
BOOL seekNew = FALSE;
BOOL doEmergency = TRUE;
BOOL allowLurkers = FALSE;
BOOL trackDigital = FALSE;
BOOL filterLog = FALSE;

//-----------------------------------------------------------------
#include <stdarg.h>
#include <syslog.h>
#include "debug.h"

static int ulogOpens = 0;
int sleep_ms(unsigned timeMs)
{
    struct timespec tim, tim2;

    tim.tv_sec = timeMs / 1000;
    tim.tv_nsec = (timeMs - tim.tv_sec * 1000) * 1000000;

    if (nanosleep(&tim, &tim2) < 0)
    {
        printf("Nano sleep system call failed \n");
        return -1;
    }

    return 0;
}


void lprintf(char const *pfname, char const *fmt, ...)
{
    va_list arg_list;

    va_start(arg_list, fmt);

    char msg[300];
    sprintf(msg, "wtrunk: %s ", pfname);
    vsprintf(msg + strlen(msg), fmt, arg_list);
    syslog(LOG_INFO, (const char *)msg);

    va_end(arg_list);
}


int open_syslog(void)
{
    ulogOpens++;

    if (ulogOpens == 1)
    {
        openlog("slog", LOG_PID | LOG_CONS, LOG_USER);
        lprintf1("opening syslog ----------------------------\n");
    }

    return 0;
}


int close_syslog(void)
{
    if (ulogOpens == 1)
    {
        ulogOpens--;
        lprintf1("closing syslog ---------------------------");
        closelog();
    }
    else
    if (ulogOpens)
    {
        ulogOpens--;
    }

    return 0;
}


//-----------------------------------------------------------------
void lock_linear(void *start, unsigned size)
{
#if 0
    int dpmi_code = DU_lock_linear(start, size);

    if (dpmi_code != 0)
    {
        sprintf(tmpArea, "Cannot lock region %p, size %u, error %d\n", start,
                size, dpmi_code);
        shouldNever(tmpArea);
    }

#endif
}


#define LOCK_VAR(v) lock_linear((void *)(&v), sizeof(v))
/*
 *      NOTE: THIS IS NOW A GENERAL PURPOSE MAIN ROUTINE TO BE USED COMMONLY FOR
 *      APPLICATIONS SUCH AS:
 *              MDT
 *              Motorola Trunking
 *              EDACS Trunking
 *              Etc.
 *
 *      It initializes things, then calls mainLoop();
 */
//
//       Items read from environment variables
//
char scanTypeEnv [64];
char scanBaudEnv [64];
char scanPortEnv [64];
char slicerPortEnv [64];
char slicerModeEnv [64];
char slicerPinEnv[64];
int newRadio = 51;
int newGroup = 0;
double slicerPark;
unsigned short actualRows;
void pauseCR()
{
    int c;

    for (c = getchar(); c != '\n' && c != '\r'; c = getchar())
    {
        // empty
    }
}


void shouldNever(const char *cp)
{
    settextposition(10, 1);
    cprintf("\r\n%s!\r\nhit return...\r\n", cp);
    raise(SIGTRAP);
    pauseCR();
    exit("should never ever have gotten here");
}


//
//       Tell the user how the command line works
//
void syntax(const char *arg0)
{
    printf("\n%s has no arguments.  Parameters are passed via environment\n", arg0);
    printf("\tvariables. (Best to put them in file named by TRACKENV variable)\n");
    printf("Set TRACKSCAN_ to:\n");
    printf("\tPCR1000\t\t38400 bps\n");
    printf("\tR10\t\t9600 bps\n");
    printf("\tR7000\t\t9600 bps\n");
    printf("\tR7100\t\t9600 bps\n");
    printf("\tR8500\t\t9600 bps\n");
    printf("\tAR8200\t\t19200 bps\n");
    printf("\tAR8000\t\t9600 bps\tCR mode\n");
    printf("\tAR5000\t\t9600 bps\n");
    printf("\tAR2700\t\t4800 bps\n");
    printf("\tAR3000\t\t4800 bps\n");
    printf("\tAR3000A\t\t9600 bps\n");
    printf("\tKENWOOD\t\t4800 bps\n");
    printf("\tKENWOOD9600\t9600 bps\n");
    printf("\tFRG9600\t\t4800 bps\n");
    printf("\tBC895\t\t9600 bps\n");
    printf("\tBC245\t\t9600 bps\n");
    printf("\tOPTOCOM\t\t9600 bps\tOPTOCOM\n");
    printf("\tOPTO\t\t9600 bps\tOS456/OS535\n");
    printf("\tNONE\n");
    printf("\n");
    printf("--more-- hit return when ready...");
    pauseCR();
    printf("Set TRACKSCAN_PORT to:\n");
    printf("\tCOM1\t(port 3f8, IRQ 4)\n");
    printf("\tCOM2\t(port 2f8, IRQ 3)\n");
    printf("\tCOM3\t(port 3e8, IRQ 4)\n");
    printf("\tCOM4\t(port 2e8, IRQ 3)\n");
    printf("\n");
    printf("Set TRACKSCAN_BAUD to:\n");
    printf("\t  300  1200  2400    4800    9600\n");
    printf("\t19200 38400 57600  115200\n");
    printf("\n");
    printf("Set TRACKSLICERPORT to:\n");
    printf("\tCOM1\t(port 3f8, IRQ 4)\n");
    printf("\tCOM2\t(port 2f8, IRQ 3)\n");
    printf("\tCOM3\t(port 3e8, IRQ 4)\n");
    printf("\tCOM4\t(port 2e8, IRQ 3)\n");
    printf("\n");
    printf("Set TRACKSLICERMODE to:\n");
    printf("\tNORMAL\tInverting not required\n");
    printf("\tINVERT\tInverting required\n");
    printf("\n");
    printf("Set TRACKSLICERPIN to:\n");
    printf("\tCTS, DSR, or DCD\n");
    printf("\n");
    printf("--more-- hit return when ready...");
    pauseCR();
    printf("Set TRACKCOLORS=0,2,7,14,14,4\n");
    printf("\t<background,quiet,normal,active,warn,error>\n");
    printf("\n");
    printf("Set NEWRADIO=51   or other priority for new radios while seekNew is ON\n");
    printf("Set NEWGROUP=0    or other priority for new groups while seekNew is ON\n");
    printf("\n");
    printf("Set TRACKSCAN_PARK to an unused frequency such as 854.0000\n\n");
    printf("Set TRACKSCAN_BAUD only if you need/want to override the default baud rate.\n");
    printf("Some scanners support only one baud rate, while others (such as the R7000)\n");
    printf("can be set via switches.  Other scanners, such as the PCR-1000 can be run\n");
    printf("at different baud rates via serial commands\n");
    printf("\n");
    printf("Depending on the scanner and slicer combination, you may or may not be\n");
    printf("able to use the same COM port for both.  The largest determining factor\n");
    printf("will be DTR requirements.  The some slicers require DTR low, while scanners\n");
    printf("like the PCR1000 require DTR high.\n");
    printf("\nset NOEMERGENCY=1 to suppress special processing of emergency calls (trunking)\n");
    printf("\nset TRACKDIGITAL=1 to allow the audio scanner to jump to digital calls\n");
    printf("\nset FILTERLOG=1 to filter out pages etc. where prty > threshold\n");
    printf("\nset TYPE2LURK=1 to if type 2 radios lurk within non-hybrid type 1 banks\n");

    exit("usage done :)");
}


void preloadEnv()
{
    // this function has two jobs:
    // 1) load environment settings from a file - this helps overcome shell limitations
    // 2) analyze certain required variables common across platform

    char *ep = getenv("TRACKENV");
    char *rp;
    FILE *fp;
    char inbuf[512];

    if (ep && *ep)
    {
        fp = fopen(ep, "rt");

        if (fp)
        {
            while (fgets(inbuf, sizeof(inbuf), fp))
            {
                if (strchr(inbuf, '=') && inbuf[0] != '=')
                {
                    rp = strrchr(inbuf, '\n');

                    if (rp)
                        *rp = 0;

                    if (strchr(inbuf, ' ') || putenv(strdup(inbuf)))
                    {
                        printf("bad line in TRACKENV file:\n%s\nsample line is:\nTRACKSCAN_=AR8000\n", inbuf);
                        printf("hit return to exit\n");
                        pauseCR();
                        exit("TRACKENV file format error");
                    }
                }
            }

            fclose(fp);
        }
        else
        {
            perror(ep);
            printf("hit return to exit\n");
            pauseCR();
            exit("can't open TRACKENV file");
        }
    }

    ep = getenv("NOEMERGENCY");

    if (ep)
        if (*ep == '1')
            doEmergency = FALSE;

    ep = getenv("TYPE2LURK");

    if (ep)
        if (*ep == '1')
            allowLurkers = TRUE;

    ep = getenv("TRACKDIGITAL");

    if (ep)
        if (*ep == '1')
            trackDigital = TRUE;

    ep = getenv("FILTERLOG");

    if (ep)
        if (*ep == '1')
            filterLog = TRUE;

    ep = getenv("NEWRADIO");

    if (ep)
    {
        if (*ep >= '0' && *ep <= '9')
        {
            newRadio = atol(ep);

            if (newRadio < 0)
                newRadio = 0;

            if (newRadio > 99)
                newRadio = 99;
        }
        else
        {
            shouldNever("NEWRADIO should be numeric!");
        }
    }

    ep = getenv("NEWGROUP");

    if (ep)
    {
        if (*ep >= '0' && *ep <= '9')
        {
            newGroup = atol(ep);

            if (newGroup < 0)
                newGroup = 0;

            if (newGroup > 99)
                newGroup = 99;
        }
        else
        {
            shouldNever("NEWGROUP should be numeric!");
        }
    }

    ep = getenv("TRACKCOLORS");

    if (ep)
    {
        if (sscanf(ep, "%hu,%hu,%hu,%hu,%hu,%hu",
                   &color_bg, &color_quiet, &color_norm, &color_active,
                   &color_warn, &color_err) != 6)
            shouldNever("TRACKCOLORS=... is 6 comma-separated numbers");

        if (color_bg > 31 || color_quiet > 31 || color_norm > 31 || color_active > 31
            || color_warn > 31 || color_err > 31)
            shouldNever("colors are from 0 to 31");
    }
}


//
//       Initialize the video to 43 line mode, set flags
//
extern WINDOW *stdscr;
void ncurses_setup()
{
    int retval;
    int i;
    int j;
    char msg[100];

	savetty();
	
    actualRows = _setvideomoderows(_DEFAULTMODE, MAX_ROWS);

    if (actualRows == 0)
    {
        printf("Failed to set video modes.\n");
        exit("cant set ncurses video rows, make screen bigger");
    }

    initscr();
    use_default_colors();
    start_color();
    /*
     *
     * /* colors  -1 means use TERMINAL definiton of BLACK not dinhgy ncurses black*/
    init_pair(0, -1, -1);
    init_pair(1, COLOR_RED, -1);
    init_pair(2, COLOR_GREEN, -1);
    init_pair(3, COLOR_YELLOW, -1);
    init_pair(4, COLOR_BLUE, -1);
    init_pair(5, COLOR_MAGENTA, -1);
    init_pair(6, COLOR_CYAN, -1);
    init_pair(7, COLOR_WHITE, -1);

    stdscr = newwin(MAX_ROWS, COL80, 0, 0); //23x40 window, start at y=1 x=0
    addstr("x");

    for (i = 0; i < MAX_ROWS; i++)
    {
        move(i, 0);
        sprintf(msg, "%c ", !(i%10) ? '*' : 0x30 + i % 10);
        addstr(msg);
        refresh();
    }

    for (i = 0; i < COL80; i++)
    {
        move(0, i);
        sprintf(msg, "%c ", !(i%10) ? '*' : 0x30 + i % 10 );
        addstr(msg);
        refresh();
    }

#if 0
    move(0, 5);
    attron(A_BOLD | COLOR_PAIR(2));
    addstr("BOLD Hello, World.");
    move(1, 5);
    attron(A_DIM | COLOR_PAIR(2));
    addstr("No bold Hi MOM!");
    refresh();
#endif
//        settextcursor ((short) 0x2000);
//        _wrapon (_GWRAPOFF);
//        color_obk = (unsigned short)_getbkcolor();
//        _setbkcolor(color_bg);
//        _clearscreen(_GCLEARSCREEN);
}

//
//       Read the environment variables to configure the scanner type and
//       COM port
//

ComPort *myScannerPort = 0;
void scanInit()
{
    long baudRate;
    const char *scannerPortEnvPtr;
    const char *scannerBaudEnvPtr;
    const char *scannerTypeEnvPtr;

    //
    //      Figure out what COM port the scanner is attached to.  If the
    //      TRACKSCAN_PORT variable isn't set, default to COM1
    //
    if (wantScanner)
        scannerPortEnvPtr = getenv("TRACKSCAN_PORT");
    else
        scannerPortEnvPtr = "NONE";

    if (scannerPortEnvPtr && *scannerPortEnvPtr)
    {
        strcpy(scanPortEnv, scannerPortEnvPtr);
        strupr(scanPortEnv);

        //
        //      If user used COMx: instead of COMx, perform a colonectomy
        //
        if (strchr(scanPortEnv, ':'))
        {
            *strchr(scanPortEnv, ':') = '\0';

            if (!strcmp(scanPortEnv, "COM1"))
                myScannerPort = new ComPort(COM1);
            else if (!strcmp(scanPortEnv, "COM2"))
                myScannerPort = new ComPort(COM2);
            else if (!strcmp(scanPortEnv, "COM3"))
                myScannerPort = new ComPort(COM3);
            else if (!strcmp(scanPortEnv, "COM4"))
                myScannerPort = new ComPort(COM4);
            else
                strcpy(scanPortEnv, "None");
        }
        else
        {
            myScannerPort = new ComPort(atoi(scanPortEnv));
        }
    }
    else
    {
        strcpy(scanPortEnv, "COM1");
        myScannerPort = new ComPort(COM1);
    }

    //
    //  See if the user overrode the default baud rate.  We only accept valid
    //  baud rates.
    //
    scannerBaudEnvPtr = getenv("TRACKSCAN_BAUD");

    if (scannerBaudEnvPtr && *scannerBaudEnvPtr)
    {
        baudRate = atol(scannerBaudEnvPtr);

        switch (baudRate)
        {
        case 300:
        case 1200:
        case 2400:
        case 4800:
        case 9600:
        case 19200:
        case 38400:
        case 57600:
        case 115200:
            sprintf(scanBaudEnv, "%l", baudRate);
            break;

        default:
            baudRate = 0;
            strcpy(scanBaudEnv, "Default");
            break;
        }
    }
    else
    {
        baudRate = 0;
    }

    if (!myScannerPort)
        shouldNever("Could not create comport object");

    //
    //      Now figure out what kind of scanner we have.  (We cheat for the R7100,
    //  since it's just a newer R7000).
    //
    scannerTypeEnvPtr = getenv("TRACKSCAN_");

    if (scannerTypeEnvPtr && *scannerTypeEnvPtr)
    {
        strcpy(scanTypeEnv, scannerTypeEnvPtr);
        strupr(scanTypeEnv);

        if (!strcmp(scanTypeEnv, "PCR1000"))
        {
            if (!baudRate)
                myScanner = new PCR1000(myScannerPort);
            else
                myScanner = new PCR1000(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "OPTOCOM"))
        {
            if (!baudRate)
                myScanner = new OPTOCOM(myScannerPort);
            else
                myScanner = new OPTOCOM(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "OPTO"))
        {
            if (!baudRate)
                myScanner = new OPTO(myScannerPort);
            else
                myScanner = new OPTO(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "R10"))
        {
            if (!baudRate)
                myScanner = new R10(myScannerPort);
            else
                myScanner = new R10(myScannerPort, (double)baudRate);                                                                                                                   // putenv

        }
        else if (!strcmp(scanTypeEnv, "R8500"))
        {
            if (!baudRate)
                myScanner = new R8500(myScannerPort);
            else
                myScanner = new R8500(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "R7100"))
        {
            if (!baudRate)
                myScanner = new R7100(myScannerPort);
            else
                myScanner = new R7100(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "R7000"))
        {
            if (!baudRate)
                myScanner = new R7000(myScannerPort);
            else
                myScanner = new R7000(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "AR8000"))
        {
            if (!baudRate)
                myScanner = new AR8000(myScannerPort);
            else
                myScanner = new AR8000(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "AR8200"))
        {
            if (!baudRate)
                myScanner = new AR8200(myScannerPort);
            else
                myScanner = new AR8200(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "FRG9600"))
        {
            if (!baudRate)
                myScanner = new FRG9600(myScannerPort);
            else
                myScanner = new FRG9600(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "AR5000"))
        {
            if (!baudRate)
                myScanner = new AR5000(myScannerPort);
            else
                myScanner = new AR5000(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "AR2700"))
        {
            if (!baudRate)
                myScanner = new AR2700(myScannerPort);
            else
                myScanner = new AR2700(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "AR3000A"))
        {
            if (!baudRate)
                myScanner = new AR3000A(myScannerPort);
            else
                myScanner = new AR3000A(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "AR3000"))
        {
            if (!baudRate)
                myScanner = new AR3000(myScannerPort);
            else
                myScanner = new AR3000(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "KENWOOD"))
        {
            if (!baudRate)
                myScanner = new Kenwood(myScannerPort);
            else
                myScanner = new Kenwood(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "KENWOOD9600"))
        {
            if (!baudRate)
                myScanner = new Kenwood(myScannerPort, 9600.00);
            else
                myScanner = new Kenwood(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "BC895") || !strcmp(scanTypeEnv, "BC245"))
        {
            if (!baudRate)
                myScanner = new BC895(myScannerPort);
            else
                myScanner = new BC895(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "GQRX"))
        {
            if (!baudRate)
                myScanner = new GQRX(myScannerPort);
            else
                myScanner = new GQRX(myScannerPort, (double)baudRate);
        }
        else if (!strcmp(scanTypeEnv, "NONE"))
        {
            strcpy(scanTypeEnv, "None");
            myScanner = new NOSCANNER;
        }
        else
        {
            strcpy(scanTypeEnv, "Unknown");
        }
    }

    //
    //      Default to AR8000 if no TRACKSCAN_ variable, or it's unrecognized
    //
    if (!myScanner)
    {
        strcpy(scanTypeEnv, "ar8000");

        if (!baudRate)
            myScanner = new AR8000(myScannerPort);
        else
            myScanner = new AR8000(myScannerPort, (double)baudRate);
    }

    if (!myScanner)
    {
        myScanner = new NOSCANNER;
        strcpy(scanTypeEnv, "<fail>");
    }

    if (myScanner)
    {
		char *tenv = getenv("TRACKSCAN_PARK");
		if (!TRACKSCAN_PARK)
		{
			TRACKSCAN_PARK = (char*) malloc(100);
		
			if (tenv)
				strcpy(TRACKSCAN_PARK, tenv);
			else
				strcpy(TRACKSCAN_PARK,"x");
		}
		else
		{
			// passed in from command line
			tenv = TRACKSCAN_PARK;
		}
		
        if (tenv && *tenv && sscanf(tenv, "%lf", &slicerPark) == 1 && slicerPark > 1.0)
        {
            control_channel = mhz2ul(tenv);
			myScanner->runCal(control_channel);
            myScanner->setPark(slicerPark);
        }
    }
}


//
//       Read the TRACKSLICERPIN, TRACKSLICERPORT, and TRACKSLICERMODE variables
//
void slicerInit()
{
    const char *slicerPortEnvPtr;
    const char *slicerModeEnvPtr;
    const char *slicerPinEnvPtr;

    //      Figure out what pin the the slicer is attached to.       If the
    //      TRACKSLICERPIN variable isn't set, default to CTS
    //
    slicerPinEnvPtr = getenv("TRACKSLICERPIN");

    if (slicerPinEnvPtr && *slicerPinEnvPtr)
    {
        strcpy(slicerPinEnv, slicerPinEnvPtr);
        strupr(slicerPinEnv);

        //
        if (!strcmp(slicerPinEnv, "CTS"))
        {
            pinToRead = MSR_CTS;
            strcpy(wantstring, "wantCTS");
        }
        else if (!strcmp(slicerPinEnv, "DSR"))
        {
            pinToRead = MSR_DSR;
            strcpy(wantstring, "wantDSR");
        }
        else if (!strcmp(slicerPinEnv, "DCD"))
        {
            pinToRead = MSR_DCD;
            strcpy(wantstring, "wantDCD");
        }
        else
        {
            pinToRead = MSR_CTS;
            strcpy(wantstring, "WantCTS");
        }
    }
    else
    {
        pinToRead = MSR_CTS;
        strcpy(wantstring, "WantCTS");
    }

    comportPort t = NONE;

    //
    //      Figure out what COM port the slicer is attached to.      If the
    //      TRACKSLICERPORT variable isn't set, default to COM1
    //
    slicerPortEnvPtr = getenv("TRACKSLICERPORT");

    if (slicerPortEnvPtr && *slicerPortEnvPtr)
    {
        strcpy(slicerPortEnv, slicerPortEnvPtr);
        strupr(slicerPortEnv);

        //
        //      If user used COMx: instead of COMx, perform a colonectomy
        //
        if (strchr(slicerPortEnv, ':'))
            *strchr(slicerPortEnv, ':') = '\0';

        if (!strcmp(slicerPortEnv, "COM1"))
        {
            t = COM1;
        }
        else if (!strcmp(slicerPortEnv, "COM2"))
        {
            t = COM2;
        }
        else if (!strcmp(slicerPortEnv, "COM3"))
        {
            t = COM3;
        }
        else if (!strcmp(slicerPortEnv, "COM4"))
        {
            t = COM4;
        }
        else
        {
            strcpy(slicerPortEnv, "None");
            t = NONE;
        }
    }
    else
    {
        strcpy(slicerPortEnv, "COM1");
        t = COM1;
    }

    if (myScannerPort->getPort() == t)
        mySlicerPort = myScannerPort;
    else
        mySlicerPort = new ComPort(t);

    //
    //      Now figure out slicer runs in normal or inverting mode
    //
    slicerModeEnvPtr = getenv("TRACKSLICERMODE");

    if (slicerModeEnvPtr && *slicerModeEnvPtr)
    {
        strcpy(slicerModeEnv, slicerModeEnvPtr);
        strupr(slicerModeEnv);

        if (!strcmp(slicerModeEnv, "NORMAL"))
        {
            slicerInverting = FALSE;
        }
        else if (!strcmp(slicerModeEnv, "INVERT"))
        {
            slicerInverting = TRUE;
        }
        else
        {
            strcpy(slicerModeEnv, "FALSE");
            slicerInverting = FALSE;
        }
    }
    else
    {
        strcpy(slicerModeEnv, "FALSE");
        slicerInverting = FALSE;
    }

    //
    //      Panic if we can't create objects
    //
    if (!mySlicerPort)
        shouldNever("Could not create slicer object");

    if (mySlicerPort->getPort() != NONE)
    {
        lock_linear((void *)&com1int, 0x400);                                        // generous from 0x122
        lock_linear((void *)(&fdata[0]), sizeof(fdata));
        lock_linear((void *)(&cpstn), sizeof(unsigned int));
        LOCK_VAR(ltick);
        LOCK_VAR(lastbit);
        LOCK_VAR(bitsAtZero);
        LOCK_VAR(bitsAtOne);
        LOCK_VAR(mySlicerPort);
        lock_linear((void *)(&pinToRead), sizeof(unsigned char));

        lock_linear((void *)mySlicerPort, sizeof(*mySlicerPort));

        mySlicerPort->setHandler(com1int);
        mySlicerPort->setDTR(DTR_LOW);
        mySlicerPort->setRTS(RTS_HIGH);
        mySlicerPort->setInterrupts(IER_EDSSI);
        mySlicerPort->setInterruptsGlobal(INT_ENABLED);
    }
}


int Special_Baud = 0;

#include <ncurses.h>

int init_telnet(char *hostname, unsigned port);
int send_telnet(const char *buffer, size_t size);
void close_telnet(void);
char *recieve_telnet(void);

char FINAL_REASON[150] = "non supplied\0";

void byebye(char *file, int line, char *reason)
{
    sprintf(FINAL_REASON, "%s:%d  exit with reason %s", file, line, reason);
    lprintf1("-----------------------------------------");
    lprintf2("%s", FINAL_REASON);
    lprintf1("-----------------------------------------");
    
	
	docleanup();
	
#undef exit
    exit(0);
}


unsigned int mhz2ul(char *info)
{
    char temp[100];
    char *dot;
    char hold;
    unsigned int mhz = 0;
    unsigned int khz = 0;
    unsigned int hz = 0;

    memset(temp, 0, sizeof(temp));
    strcpy(temp, info);
    dot = strstr(temp, ".");

    if (dot)
    {
        *dot = 0;
        int len;

        mhz = atoi(temp);
        mhz *= 1000000;
        dot++;
        memmove(temp, dot, &temp[sizeof(temp) - 1] - dot);

        dot = temp;
        hold = *(dot + 3);
        *(dot + 3) = 0;
        len = strlen(dot);
        len = 3 - len;

        khz = atol(dot);
        khz = khz * pow(10, len);
        khz *= 1000;

        *(dot + 3) = hold;
        dot = dot + 3;
        len = strlen(dot);
        len = 3 - len;
        hz = atol(dot);
        hz = hz * pow(10, len);

        mhz += khz + hz;
    }
    else
    {
        mhz = atoi(temp) * 1000000;
    }

    return mhz;
}


#if 0
unsigned int test;
test = mhz2ul("142.8");t
test = mhz2ul("142.");
test = mhz2ul("142");
test = mhz2ul("999.888777");
#endif

unsigned long usr_time= 0;
#include <getopt.h>

void usage(void)
{
	printf ("mtrunk -t time (Minutes) -f freq (mHz)\n");
}

extern int optind;

int main(int argc, char **argv)
{
    int rv;
    int opt;
    char *optarg;
    
	//----------------------------------------------
	while ((opt = getopt(argc, argv, "f:t:v:h")) != -1) {
		switch (opt) {
			case 'f': 
				TRACKSCAN_PARK = (char*) malloc(100);
				sscanf(argv[optind -1], "%s", TRACKSCAN_PARK);
				//strcpy(TRACKSCAN_PARK, argv[optind-1]);
				break;

			case 't':
				sscanf(argv[optind-1],"%lu", &usr_time);
				break;
				
			case 'v':
				int on;
				sscanf(argv[optind -1], "%d", &on);
				chatty = (on) ? 1 : 0;
				break;

			case 'h':
			default:
				usage();
				exit("help called");
			break;
		} 
	}

    open_syslog();

    preloadEnv();
    ncurses_setup();
    scanInit();
    slicerInit();
    //
    //      Setup timer mode, and the atexit cleanup function.
    //
    hardwareInit();

    mainLoop();
    
    exit("normal exit");


    return 0;
}


void debugSystem(void)
{
    _setvideomode(_DEFAULTMODE);

    printf("scanTypeEnv = %s\n", scanTypeEnv);
    printf("scanPortEnv = %s\n", scanPortEnv);
    printf("slicerPortEnv = %s\n", slicerPortEnv);
    printf("slicerModeEnv = %s\n", slicerModeEnv);
    mySlicerPort->dump("Slicer Port");
    myScanner->getComPort()->dump("Scanner Port");
    pauseCR();
}


void prepareFile(register const char *tfn)
{
    char bfn[300];
    register char *cp;

    strcpy(bfn, tfn);
//    cp = strrchr(bfn, '.');
//    if (cp)
//        strcpy(cp, ".bak");

    unlink(bfn);
    rename(tfn, bfn);
    unlink(tfn);
}
