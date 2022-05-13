#include <platform.h>
#include <osw.h>
#include <trunksys.h>
#include <time.h>
#define LOGCOLUMN 41
#define WORDSPERSEC ((2 * 9600) / (48 + 120 + 120))
int wordsPerSec = WORDSPERSEC;
char tempArea [512];
unsigned short sysId = 0,  cwFreq = 0;
long theDelay = DELAY_ON;

unsigned long stackOverFlows = 0L;
BOOL _near wantScanner = FALSE;
class EdacsMap : public FreqMap {

public:
char *mapCodeToF(unsigned short, char)
{
	return "x";
}
int isFrequency(unsigned short, char)
{
	return TRUE;
}
};
FreqMap *myFreqMap = new EdacsMap;
/* EDACS control channel monitoring program... */
#define NUMCH 27
#define NUMCHPL2 29
static double chan[30] = {
	856.0375, 857.0375, 858.0375, 859.0375, 860.0375
};
/* this identifies the control channel in the above table             */
int control = 1; /* this number is the lcn of the control channel - 1 */


/*                    global variables                                */
int lc = 0;
char ob[1000];          /* buffer for raw data frames                         */
int obp = 0;            /* pointer to current position in array ob            */
static int dotting = 0; /* dotting sequence detection flag      */
static int mode = 0;    /* mode flag; 0 means we're on control  */
                        /* channel; 1 means active frequency    */

char tmpArea[150];
/************************************************************************/
/*                         set_freak                                    */
/*                                                                      */
/* This routine must be written by the user for his/her specific radio. */
/* The input is the desired frequency the scanner should go to.         */
/* Characters are sent to the scanner using the send_char routine.      */
/************************************************************************/
void set_freak(double)
{
	/* put your code here */
	/* call send_char() to send a byte to your scanner */

}


/**********************************************************************/
/*               higher level stuff follows                           */
/**********************************************************************/

/* structure for storing the control channel data frame information  */
struct ccdat
{
	int cmd;                /* command */
	int lcn;                /* logical channel number */
	int stat;               /* status bits */
	int id;                 /* agency/fleet/subfleet id */
	int bad;                /* CRC check flag - when set to 1 it means data is corrupt */
	long rawBits;           /* for dumb dumping */
};

/* array acn[] indicates which channels are currently active; aid[]  */
/* indicates which id is using the active channel                    */
/* acn[] is actually a countdown timer decremented each time a       */
/* control channel command is received and is reset whenever that    */
/* channel appears in control channel data stream. When this reaches */
/* zero the channel is assumed to have become inactive.              */
/* aid[] is used to make sure a new group appearing on an active     */
/* channel is recognized even if acn[] had not yet reached zero      */
int acn[NUMCHPL2], aid[NUMCHPL2], nacn = 0;

/************************************************************************/
/*                          show_setup                                  */
/*                                                                      */
/* Purpose: setup things for screen display                         */
/************************************************************************/
void show_setup()
{
	static int i;

	clrscr();
	for (i = 0; i < NUMCHPL2; i++)
	{
		acn[i] = 0; aid[i] = 0xffff;
	}
	QuickOut(1, 1, color_quiet, "Edacs(tp) 22-Apr-1998");
	QuickOut(2, 1, color_quiet, "-------------------- ");
	QuickOut(3, 63, color_quiet, "# channels: 27");
	QuickOut(4, 61, color_quiet, "Exit: Space or Esc ");
	QuickOut(6, LOGCOLUMN, color_quiet, "Misc Control data");
	QuickOut(7, LOGCOLUMN, color_quiet, "-------------------------------------");
	QuickOut(1, 75, color_norm, "-");
	nacn = 0;
}

/************************************************************************/
/*                          show_active                                 */
/*                                                                      */
/* this routine gets called ONCE whenever a new ID becomes active on    */
/* a given channel number                                               */
/************************************************************************/
inline void
show_active(int lcn, int id, const char *nar)
{
	register unsigned int linx, liny;

	/* this is where you will want to insert some code that decides if   */
	/* you want to switch to this particular active frequency. Right     */
	/* now this commented out code fragment would have sent your scanner */
	/* off to any active frequency.                                      */
	/* you'll probably also want to filter out data channel assignemnts  */
/*  set_freak(chan[info.lcn-1]);    */
/*  mode = 1;                       */
/*  dotting = 0;                    */

	linx = 1;
	liny = 2 + lcn;

	if (liny >= (actualRows - 1))
	{
		linx = 30;
		liny -= actualRows - 4;
	}

	gotoxy(linx, liny);
	textcolor(color_active);
	sprintf(tmpArea, "%3d: %4X %s", lcn, id, nar);
	_outtext(tmpArea);
	textcolor(color_norm);
	aid[lcn] = id;

}

/************************************************************************/
/* show_inactive gets called when a channel is no longer active         */
/* it simply writes out a bunch of spaces to erase the old              */
/* information                                                          */
/************************************************************************/
inline void
show_inactive(int lcn)
{
	register unsigned int linx, liny;

	linx = 6;
	liny = 2 + lcn;

	if (liny >= (actualRows - 1))
	{
		linx = 35;
		liny -= actualRows - 4;
	}

	gotoxy(linx, liny);
	aid[lcn] = 0xffff;
	cprintf("                     ");

}
/* scroll raw commands on window in right hand side of display */
void emit(const char *cp, int where = 0)
{
	static unsigned int lc = 8;
	static char lastBuf[150];
	static int lastwhere = 0;

	if (strcmp(lastBuf, cp))
	{
		strcpy(lastBuf, cp);
		if ( lastwhere == 1 && where == 2)
		{
			gotoxy(LOGCOLUMN + 15, lc);
			cprintf("%-*.*s", 15, 15, cp);
		}
		else
		{
			if (lc == (actualRows - 1))
				lc = 8;                                                   // movetext(60,9,79,24,60,8);
			else
				++lc;
			if ( where == 2 )
			{
				gotoxy(LOGCOLUMN + 15, lc);
				cprintf("%-*.*s", 15, 15, cp);
			}
			else
			{
				gotoxy(LOGCOLUMN, lc);
				cprintf("%-*.*s", 30, 30, cp);
			}
			gotoxy(LOGCOLUMN, (lc >= (actualRows - 1)) ? 8 : lc + 1);
			cprintf("                    ");
		}
		lastwhere = where;
	}
}

/* scroll raw commands on window in right hand side of display */
inline void
show_raw(long rb, int wnum)
{
	sprintf(tmpArea, "%.7lX", rb);
	emit(tmpArea, wnum);
}

/************************************************************************/
/*                        proc_cmd                                      */
/*                                                                      */
/* This routine figures out when a group becomes active on a new        */
/* channel and when a channel is has become inactive. This info is      */
/* passed to the two routines above which show the changes on the       */
/* screen                                                               */
/************************************************************************/
void
show(struct ccdat& info, int theWord = 0)
{
	static int idup = 0, cdup = 0, i, sysid = 0x0000, freq = -1;
	//static int ccdup = 0;
	static char dup[4] = { 45, 47, 124, 92 };

	int nsysid, osysid;
	int nfreq, ofreq;

	static struct ccdat savedDat;
	static BOOL haveSaved = FALSE;

	/* update the "I'm still receiving data" spinwheel character on screen
	 */
	if ( ++cdup > 9)
	{
		gotoxy(75, 1);
		idup = (idup + 1) & 0x03;
		putch(dup[idup]);
		cdup = 0;
	}
	/* process only good information blocks  */
	if (info.bad == 0)
	{

		if ( (info.rawBits & 0xA000000L) == 0L)                   // 000 for normal, 010 for aegis digital
		{               /*
			               Call Grant, probably same for group and priority group call

			               patent 4,939,746:

			               000 iiiiiiii ccccc t ggg gggg gggg
			             |   |        |     | |
			             |   |        |     | +-11 bits called group
			             |   |        |     +--- 1 bit  transmission or message mode
			             |   |        +--------- 5 bits channel #
			             |   +------------------ 8 bits 1/2 calling logical ID
			             |||+---------------------- 3 bits Message Type A = 000

			               the I-call equivalent of this is a pair of F6 commands

			             */

			if ( haveSaved && ((savedDat.rawBits & 0xE000000) == (info.rawBits & 0xA000000L)))
			{
				if ((savedDat.rawBits & 0x001F000) == (info.rawBits & 0x001F000)
				    &&
				    (savedDat.id == info.id)
				    )
				{
					static int caller;
					caller = (int)((savedDat.rawBits >> 11) & 0x3f00);
					caller |= (int)((info.rawBits >> 17) & 0x00ff);
					sprintf(tmpArea, "%cGrant f=%2d i%4X->g%4X",
					        (info.rawBits & 0x4000000L) ? 'd' : 'a',
					        (int)((info.rawBits >> 12) & 0x01f),
					        caller,
					        info.id);
					emit(tmpArea);

					// eventually this will be the key place to signal the beginning of
					// a conversation, since the destination group, calling radio ID, and
					// frequency are all provided in this pair. the late-entry message below
					// is equivalent to the single-OSW group frequency assignment in Motorola
					// systems, whereas this pair is equivalent to the double-OSW call initiation
					// pair in motorola.
				}
				else
				{
					// some other form, as yet not understood, so dump it:
					emit("fragment");
					show_raw(savedDat.rawBits, 1);
					show_raw(info.rawBits, 2);
				}
				haveSaved = FALSE;
			}
			else
			{
				savedDat = info;
				haveSaved = TRUE;
			}
		}
		else if ((info.rawBits & 0xFFE0000L) == 0xFC60000L)
		{
			/* Channel Configuration Message: Reference: 5,077,828

			   111 111 00011 ccccc ffffffffffff
			   aaa bbb ccccc

			   perhaps there are two versions of edacs?
			 */

			// really important - the map between logical frequency and
			// fcc frequency!
			sprintf(tmpArea, "Fmap %d->%d",
			        (int)((info.rawBits >> 12) & 0x1f),
			        (int)(info.rawBits & 0x0fff));
			emit(tmpArea);
			haveSaved = FALSE;
		}
		else if ((info.rawBits & 0xFF80000L) == 0xFD00000L)
		{
			/* Site ID Message

			   5,077,828 shows:

			        111 111 010 dd ccccc ppp h ff iiiiii
			        aaa bbb ccc

			    actual data at Wilmington, Ohio shows:

			        FD01141

			        1111 1101 0000 0001 0001 0100 0001
			        aaab bbcc cddc cccc ppph ffii iiii  - consistent with 5,077,828
			        1111 1111 1                         - use this as a mask
			        1111 1101 0							- match with this
			 */

			osysid = ((int)info.rawBits) & 0x03f;                                   // 6-bit sysid
			ofreq = ((int)(info.rawBits >> 12)) & 0x01f;                            // 5-bit freq#
			if ( info.rawBits & 0x100L )                                            // home system
			{
				if (sysid != osysid || ((int)ofreq) != freq)
				{
					sysid = osysid;
					freq = (int)ofreq;
					textcolor(color_active);
					gotoxy(35, 1);
					cprintf("Sys Id: %d Freq: %3d, prty %d", sysid, freq,
					        (int)((info.rawBits >> 9) & 0x07));
					textcolor(color_norm);
				}
			}
			else if ((((int)info.rawBits) & 0x0c0) == 0x0c0 )                                // SCAT!
			{
				sysid = osysid;
				freq = (int)ofreq;
				textcolor(color_active);
				gotoxy(35, 1);
				cprintf("SCATid: %d Freq: %3d, prty %d", sysid, freq,
				        (int)((info.rawBits >> 9) & 0x07));
				textcolor(color_norm);
			}
			else
			{
				// adjoining system
				sprintf(tmpArea, "Neighbor f=%d,id=%d", (WORD)nfreq, (WORD)nsysid);
				emit(tmpArea);
			}
			haveSaved = FALSE;
		}
		else if ((info.rawBits & 0xFF00000L) == 0xFC00000L )
			/* idle code as far as I can tell...
			        I've seen data such as:
			        FC20000   -  unknown
			        FC20010   -  unknown
			        FC20200   -  unknown
			        FC400xx   -  unknown, possibly related to model / #channels
			                                 clinton county, ohio has 5 channels and shows fc40005
			                                 I've also seen  fc40002 and fc40000
			        FC8FFFF   -  regroup plan bitmap (low half)
			        FC9FFFF   -  regroup plan bitmap (high half)
			        FCExxxx   -  unknown
			                        FCE113b
			                        FCE11db
			                        FCE114f
			                        FCEc00c

			 */
			haveSaved = FALSE;
		else if ( (info.rawBits & 0xFC00000) == 0xF400000  )                   // 111-101 (the 'f6' case)
		{               // Individual Call <pair>

			if ( haveSaved && ((savedDat.rawBits & 0xFC00000) == 0xF400000))                                // i-call
			{
				if ((savedDat.rawBits & 0x00F8000) == (info.rawBits & 0x00F8000))                           // same lcn
				{
					sprintf(tmpArea, "f=%2d i%4X->i%4X",
					        info.lcn,
					        (WORD)(savedDat.rawBits  & 0x3FFF),
					        (WORD)(info.rawBits  & 0x3FFF));
					emit(tmpArea);
				}
				else
				{
					// some other form, as yet not understood, so dump it:
					emit("fragment");
					show_raw(savedDat.rawBits, 1);
					show_raw(info.rawBits, 2);
				}
				haveSaved = FALSE;
			}
			else
			{
				savedDat = info;
				haveSaved = TRUE;
			}

		}
		else if ( (info.rawBits & 0xFC00000) == 0xEC00000)                      // 111-011 voice calls
		{               /*
			                    111-011 - voice call late entry
			                    111-011-tt llll lIxx xxxx xxxx xxxx
			                    111-011-tt              type = 10: analog, 11: digital, 00: phone 01 ????
			                               llll l
			                                             I set for i-call and phone patch
			                                               ii iiii iiii iiii - individual address
			                                               ?? gggg gggg gggg - group address
			             */
			int relId;                                                          // group OR radio ID
			const char *narrative;
			if ( info.rawBits & 0x04000 )                                       // individual
			{
				relId = (WORD)(info.rawBits & 0x0003FFF);                       // 14-bit radio logical ID
				if ( info.rawBits & 0x0200000 )
					narrative = "Indiv";
				else
					narrative = "Phone";
			}
			else
			{
				relId = (WORD)(info.rawBits & 0x00007FF);                                          // 11-bit group ID
				narrative = "Group";
			}
			if ( aid[info.lcn] != relId)
				show_active(info.lcn, relId, narrative);
			/* update activity timer for this active channel */
			acn[info.lcn] = 8 + (nacn << 2);
			haveSaved = FALSE;
		}
		else if ((info.rawBits & 0xE000000) == 0xA000000 )                     // 101x xxxx xxxx xxxx xxxx xxxx xxxx
		{               /* examples:
			                    a0246c2 a2246c2   possibly 1/2 caller + group + lcn?
			                    a84c6db a94c6db
			             */

			sprintf(tmpArea, "Data %07lX", info.rawBits);
			emit(tmpArea);
			haveSaved = FALSE;
		}
		else
		{
			show_raw(info.rawBits, theWord);
			haveSaved = FALSE;
		}
	}

	/* see if any channel numbers have dropped off */
	/* this is done by waiting for a certain number of control channel */
	/* commands to go by before assuming the channel is no longer      */
	/* active.                                                         */
	nacn = 0;              /* nacn holds the number of active channels */
	for (i = 1; i <= NUMCH; i++)
	{
		if (acn[i])
		{
			++nacn;
			if (--acn[i] == 0)
			{
				/* LCN has become inactive */
				show_inactive(i);
				aid[i] = 0xffff;

			}
		}
	}
}

/************************************************************************/
/*                        proc_cmd                                      */
/*                                                                      */
/* This routine processes a data frame by checking the CRC and nicely   */
/* formatting the resulting data into structure info                    */
/************************************************************************/
inline void
proc_cmd(int c)
{
	static int ecc = 0, sre = 0, cc = 0, ud = 0xA9C, nbb = 0, tb, orf, i;
	static char oub[50];
	static struct ccdat info;
	static int wordNum;
	long newRaw = 0;

	if (c < 0)
	{
		cc = 0;
		sre = 0x7D7;
		ecc = 0x000;
		oub[28] = 0;

		/* pick off, store  command   (eight bits) */
		for (i = 0; i <= 7; ++i)
		{
			orf = orf << 1;
			orf += oub[i];
			newRaw <<= 1;
			newRaw += oub[i];
		}
		orf = orf & 0xff;
		info.cmd = orf;

		/* pick off LCN   (five bits)  */
		for (i = 8; i <= 12; ++i)
		{
			orf = orf << 1;
			orf += oub[i];
			newRaw <<= 1;
			newRaw += oub[i];
		}
		orf = orf & 0x1f;
		info.lcn = orf;

		/* pick off four status bits */
		for (i = 13; i <= 16; ++i)
		{
			orf = orf << 1;
			orf += oub[i];
			newRaw <<= 1;
			newRaw += oub[i];
		}
		orf = orf & 0x0f;
		info.stat = orf;

		/* pick off 11 ID bits    */
		for (i = 17; i <= 27; ++i)
		{
			orf = orf << 1;
			orf += oub[i];
			newRaw <<= 1;
			newRaw += oub[i];
		}
		orf = orf & 0x07ff;
		info.id = orf;
		info.rawBits = newRaw;
		info.bad = (nbb == 0) ? 0 : 1;

		show(info, wordNum);

		nbb = 0;
		wordNum = 0 - c;
	}
	else
	{

		/* bits 1 through 28 will be run through crc routine */
		if (cc < 28)
		{
			oub[cc++] = c;
			if ( c == 1)
				ecc = ecc ^ sre;
			if ( (sre & 0x01) > 0)
				sre = (sre >> 1) ^ ud;
			else
				sre = sre >> 1;
		}
		else
		{
			++cc;
			/* for the rest of the bits - check if they match calculated crc */
			/* and keep track of the number of wrong bits in variable nbb    */
			if ( (ecc & 0x800) > 0)
				tb = 1;
			else
				tb = 0;
			ecc = (ecc << 1) & 0xfff;
			if (tb != c)
				nbb++;
		}
	}
}

/************************************************************************/
/*                        proc_frame                                    */
/*                                                                      */
/* This routine processes the two raw data frames stored in array ob[]. */
/* Each data frame is repeated three times with the middle repetition   */
/* inverted. So bits at offsets of 0, 40, and 80 should all be carrying */
/* the same information - either 010 or 101 if no errors have occured). */
/* Error correction is done by ignoring any single wayward bit in each  */
/* triplet. For example a 000 triplet is assumed to have actually been  */
/* a 010; a 001 -> 101; 011 -> 010; et cetera. Array tal[] holds a table*/
/* giving the "corrected" bit value for every possible triplet. Two     */
/* or three wrong bits in a triplet cannot be corrected. The resulting  */
/* data bits are send on the proc_cmd routine for the remaining         */
/* processing.                                                          */
/************************************************************************/
inline void
proc_frame()
{
	register int i, l;
	static int tal[9] = { 0, 1, 0, 0, 1, 1, 0, 1 };

	/* do the first data frame */
	proc_cmd(-1);                                                           /* reset proc_cmd routine */
	for (i = 0; i < 40; i++)
	{
		l = (int)( (ob[i] << 2) + (ob[i + 40] << 1) + ob[i + 80]);              /* form triplet */
		l = tal[l];                                                             /* look up the correct bit value in the table */
		proc_cmd(l);                                                            /* send out bit */
	}

	/* do the second data frame */
	proc_cmd(-2);
	for (i = 0; i < 40; i++)
	{
		l = (int)( (ob[i + 120] << 2) + (ob[i + 160] << 1) + ob[i + 200]);
		l = tal[l];
		proc_cmd(l);
	}

}
/************************************************************************/
/*                       frame_sync					*/
/*									*/
/* This routine takes the raw bit stream and tries to find the 48 bit   */
/* frame sync sequence. When found it stores the next 240 raw data bits */
/* that make up a data frame and stores them in global array ob[].      */
/* Routine proc_frame is then called to process the data frame and the  */
/* cycle starts again. When mode = 1 it will only look for the dotting  */
/* sequence and updat the dotting flag.                                 */
/************************************************************************/
inline void
frame_sync(char gin)
{
	static int sr0 = 0, sr1 = 0, sr2 = 0, hof = 300, xr0, xr1, xr2, i, nh = 0, ninv = 0;
	static int fsy[3][3] = {
		0x1555, 0x5712, 0x5555,                 /* control channel frame sync            */
		0x5555, 0x5555, 0x5555,                 /* dotting sequence                      */
		0xAAAA, 0xAAAA, 0x85D3                  /* data channel frame sync (not used)    */
	};

	/* update registers holding 48 bits */
	sr0 = sr0 << 1;
	if ( (sr1 & 0x8000) != 0)
		sr0 = sr0 ^ 0x01;
	sr1 = sr1 << 1;
	if ( (sr2 & 0x8000) != 0)
		sr1 = sr1 ^ 0x01;
	sr2 = sr2 << 1;
	sr2 = sr2 ^ (int)gin;

	/* update ob array */
	if (obp < 600)
	{
		ob[obp] = gin;
		obp++;
	}

	/* find number of bits not matching sync pattern */
	xr0 = sr0 ^ fsy[mode][0];
	xr1 = sr1 ^ fsy[mode][1];
	xr2 = sr2 ^ fsy[mode][2];
	nh = 0;
	for (i = 0; i < 16; i++)
	{
		if ( i < 13)
			if ( (xr0 & 0x01))
				++nh;
		xr0 = xr0 >> 1;
		if ( (xr1 & 0x01))
			++nh;
		xr1 = xr1 >> 1;
		if ( (xr2 & 0x01))
			++nh;
		xr2 = xr2 >> 1;
	}

	/* if there are less than 3 mismatches with sync pattern and we aren't */
	/* inside a data frame (hold-off counter less than 288) sync up  */
	if ( nh < 5)                          /* mode zero means we're monitoring control channel */
	{
		if ( (hof > 287) && (mode == 0) )
		{
			proc_frame();
			obp = 0;
			hof = 0;
			ninv = 0;
		}
		/* mode 1 means we're on voice channel and so we just found dotting */
		else if ((mode == 1) && (nh < 2) )
			dotting++;
	}

	/* check for polarity inversion - if all frame sync bits mismatch 12  */
	/* times in a row without ever getting a good match assume that one   */
	/* must invert the bits                                               */
	if ((nh == 48) && (mode == 0))
	{
		ninv++;
		if (ninv > 12)
		{
			slicerInverting = !slicerInverting;
			gotoxy(65, 1);
			if (slicerInverting)
				cprintf("INVERT");
			else
				cprintf("NORMAL");
			ninv = 0;
		}
	}

	if (hof < 1000)
		hof++;

}

time_t now, nowFine;

int mainLoop()
{
	register unsigned int i = 0;
	int ef = 0, ch;
	register char s = 0;
	register double dt, exc = 0.0, clk = 0.0, xct, dto2;

	clrscr();

	/* dt is the number of expected clock ticks per bit */
	dt =  1.0 / (9600.0 * 838.22e-9);
	dto2 = 0.5 * dt;

	printf("If program just sits here press any key to exit...\n");
	while ( (cpstn < 3)
	        & (kbhit() == 0) )
		;
	if (cpstn < 3)
	{
		printf("HEY - no data seems to be coming in over your interface.\n\n");
		exit(1);
	}
	else
		printf("Interface seems to work properly...\n\n");

	show_setup();


	while (ef == 0)
	{

		/* check if key has been hit */
		if (kbhit() != 0)
		{
			ch = getch();
			/* a space or Esc key press sets the exit flag */
			if ((ch == 32) | (ch == 27))
				ef = 1;
			else
			{
				/* any other key press returns the scanner to the control */
				/* channel if it had switched to an active channel        */
				set_freak(chan[control]);
				mode = 0;
				i = cpstn;
			}
		}

		if (i != cpstn)
		{
			if ( fdata[i] & 0x8000 )
				s = slicerInverting ? 0 : 1;
			else
				s = slicerInverting ? 1 : 0;

			/* add in new number of cycles to clock  */
			clk += (fdata[i] & 0x7fff);
			xct = exc + dto2;                         /* exc is current boundary */
			while ( clk >= xct )
			{
				frame_sync(s);
				clk = clk - dt;
			}
			/* clk now holds new boundary position. update exc slowly... */
			/* .05 by trial and error, and only on one edge of the signal... */
			if ( s )
				exc = exc + 0.05 * (clk - exc);
//       exc += (clk - exc)/10;

			if ( ++i > BufLen)
				i = 0;

			/* If we are sitting on an active channel and the dotting sequence */
			/* is detected, we should switch back to the control channel       */
			if ( (mode == 1) && (dotting > 0))
			{
				set_freak(chan[control]);
				mode = 0;
				dotting = 0;
				i = cpstn;
			}
		}

	}
	exit(0);
	return 0;
}
void localCleanup()
{
	gotoxy(1, 23);
	printf("\n");
}

