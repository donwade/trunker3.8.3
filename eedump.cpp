#include <platform.h>

static FILE *myFile;
#define NBYTESFORSTDIO 5120
static char niceBuffer[NBYTESFORSTDIO];
extern int Special_Baud;

BOOL wantScanner = FALSE;
class EdacsMap : public FreqMap {
public:
virtual char *mapCodeToF(unsigned short, char)
{
	return "x";
};
virtual int isFrequency(unsigned short, char)
{
	return TRUE;
};
};
FreqMap *myFreqMap = new EdacsMap;
int wordsPerSec = (Special_Baud ? 2400 : 4800) / (112 + 40);
/* EDACS control channel monitoring program... */
#define NUMCH 31
#define NUMCHPL2 33
/* this identifies the control channel in the above table             */
/*                    global variables                                */
static char ob[1000];   /* buffer for raw data frames                         */
int obp = 0;            /* pointer to current position in array ob            */
static int dotting = 0; /* dotting sequence detection flag      */
static int mode = 0;    /* mode flag; 0 means we're on control  */
                        /* channel; 1 means active frequency    */

static char tmpArea[150];

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


/************************************************************************/
/*                          show_setup                                  */
/*                                                                      */
/* Purpose: setup things for screen display                             */
/************************************************************************/
void show_setup()
{
	// static int i;
	clrscr();
	if ( Special_Baud )
		QuickOut(1, 1, color_quiet, "Watcom Eedump-4800 18-Feb-2000");
	else
		QuickOut(1, 1, color_quiet, "Watcom Eedump 18-Feb-2000");
	QuickOut(4, 61, color_quiet, "Exit: Space or Esc ");
}

/************************************************************************/
/*                          show_active                                 */
/*                                                                      */
/* this routine gets called ONCE whenever a new ID becomes active on    */
/* a given channel number                                               */
/************************************************************************/
inline void
show_active(unsigned short lcn, unsigned short id, const char *nar)
{
	register unsigned short linx, liny;

	/* this is where you will want to insert some code that decides if   */
	/* you want to switch to this particular active frequency. Right     */
	/* now this commented out code fragment would have sent your scanner */
	/* off to any active frequency.                                      */
	/* you'll probably also want to filter out data channel assignemnts  */
/*  set_freak(chan[info.lcn-1]);    */
/*  mode = 1;                       */
/*  dotting = 0;                    */

	linx = 1;
	liny = lcn;
	liny += 2;

	if (liny >= (actualRows - 1))
	{
		linx = 30;
		liny -= actualRows - 4;
	}

	gotoxy(linx, liny);
	textcolor(color_active);
	cprintf("%3d: %4X %s", lcn, id, nar);
	textcolor(color_norm);
}
/* scroll raw commands on window in right hand side of display */
void inline
emit(const char *cp)
{
	fprintf(myFile, "%-20s", cp);
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
show(struct ccdat& info)
{
	static int idup = 0, cdup = 0, sysid = 0x0000, freq = -1;
	//static int ccdup = 0;
	static char dup[4] = { 45, 47, 124, 92 };

	int nsysid, osysid;
	long nfreq, ofreq;

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

	/*
	      0xx			Call Grant, normally in matched pairs	&8.... == 0....
	      101			Data Call				&E.... == A....
	      110			Affiliation				&E.... == C....
	      111 011			Voice Call				&FC... == EC...
	      111 100			Patch Alias				&FC... == F0...
	      111 101			Individual Call				&FC... == F4...
	      111 111 00011		Channel Configuration			&FFE.. == FC6..
	      111 111 010		Site ID / Neighbor			&FF8.. == FD0
	      111 111 110		New Site ID? New Neighbor?		&FF8.. == FF0
	      111 111 00		Idle / other (like above)		&FF... == FC

	 */

	/* process only good information blocks  */
	if (info.bad == 0)
	{
		if ( (info.rawBits & 0x8000000) == 0L)  // 000 - normal grant,	010 aegis digital grant
		{               // 001 - emergency,     011 aegis emergency
			                                    /*
			                                       Call Grant, probably same for group and emergency group call

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

			if ( haveSaved && ((savedDat.rawBits & 0xE000000) == (info.rawBits & 0xE000000)))
			{
				if ((savedDat.rawBits & 0x001F000) == (info.rawBits & 0x001F000)
				    &&
				    (savedDat.id == info.id)
				    )
				{
					static int caller;
					caller = (int)((savedDat.rawBits >> 11) & 0x3f00);
					caller |= (int)((info.rawBits >> 17) & 0x00ff);
					sprintf(tmpArea, "%s %sGrant f=%2d i%4X->g%4X",
					        (info.rawBits & 0x4000000L) ? "digital" : "analog",
					        (info.rawBits & 0x2000000L) ? "emergency " : "",
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
					// some other form, as yet not understood, so dump it:
					emit("fragmented!");
				haveSaved = FALSE;
			}
			else
			{
				savedDat = info;
				haveSaved = TRUE;
				emit("");
			}
		}
		else if ((info.rawBits & 0xE000000) == 0xA000000 )                       // 101x xxxx xxxx xxxx xxxx xxxx xxxx
		{
			emit("Data");
			haveSaved = FALSE;
		}
		else if ((info.rawBits & 0xE000000) == 0xC000000)   // 110x xxxx xxxx xxxx xxxx xxxx xxxx
		{               // affiliation - thanks to Wayne:
			                                                // ccc ggggggggggg iiiiiiiiiiiiii
			sprintf(tmpArea, "Aff I%04hx->G%04hx",
			        (unsigned short)(info.rawBits & 0x3fffL),
			        (unsigned short)((info.rawBits >> 14) & 0x07ffL)
			        );

			emit(tmpArea);
			haveSaved = FALSE;
		}
		else if ( (info.rawBits & 0xFC00000) == 0xEC00000)                      // 111-011 voice calls
		{               /*
			                    111-011 - voice call late entry
			                    111-011-tt llll lIxx xxxx xxxx xxxx
			                    111-011-tt                      type = 10: analog, 11: digital, 00: phone 01 ????
			                               llll l
			                                             I set for i-call and phone patch
			                                               ii iiii iiii iiii - individual address
			                                               ?? gggg gggg gggg - group address
			             */
			static int relId;                                                   // group OR radio ID
			static const char *narrative;
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
				if ( info.rawBits & 0x0200000 )
					narrative = "Group";
				else
					narrative = "Emerg";
			}
			sprintf(tmpArea, "%s %04x, lcn %d", narrative, relId, info.lcn);
			emit(tmpArea);
			haveSaved = FALSE;
		}
		else if ((info.rawBits & 0xFC00000) == 0xF000000)
		{
			// patch alias - again thanks to Wayne:
			// 111100 aaaaaaaaaaa ggggggggggg
			// the patch alias will be in the range 7f0...7ff

			sprintf(tmpArea, "Patch g%4X->p%4X",
			        (WORD)(info.rawBits & 0x07ff),
			        (WORD)((info.rawBits >> 11) & 0x07ff));
			emit(tmpArea);
			haveSaved = FALSE;
		}
		else if ( (info.rawBits & 0xFC00000) == 0xF400000  )                     // 111-101 (the 'f6' case)
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
					// some other form, as yet not understood, so dump it:
					emit("fragment");
				haveSaved = FALSE;
			}
			else
			{
				emit("<saved>");
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

			     5,574,788 shows:

			            11 111 1110 dd ccccc ppp h ff iiiiii
			            11 111 1111 - mask FF80000
			            11 111 1110 - compare FF00000


			    actual data at Wilmington, Ohio shows:

			        FD01141  or fd04141

			        1111 1101 0000 0001 0001 0100 0001  data on lcn 1
			        1111 1101 0000 0100 0001 0100 0001  data on lcn 4
			        aaab bbcc cddc cccc ppph ffii iiii  - consistent with 5,077,828
			        1111 1111 1                         - use this as a mask
			        1111 1101 0                         - match with this
			 */
			// should really check for doubles of this like in call ack

			osysid = ((int)info.rawBits) & 0x03f;                           // 6-bit sysid
			ofreq = (info.rawBits >> 12) & 0x01f;                           // 5-bit freq#
			if ( info.rawBits & 0x100L )                                    // home system
			{
				if (sysid != osysid || ((int)ofreq) != freq)
				{
					sysid = osysid;
					freq = (int)ofreq;
					textcolor(color_active);
					gotoxy(35, 1);
					cprintf("Sys Id: %d Freq: %3d, prty %d", (WORD)sysid, (WORD)freq,
					        (int)((info.rawBits >> 9) & 0x07));
					textcolor(color_norm);
				}
				sprintf(tmpArea, "SysId f=%d,id=%d", (WORD)ofreq, (WORD)osysid);
			}
			else if ((((int)info.rawBits) & 0x0c0) == 0x0c0 )                                // SCAT!
			{
				if (sysid != osysid || ((int)ofreq) != freq)
				{
					sysid = osysid;
					freq = (int)ofreq;
					textcolor(color_active);
					gotoxy(35, 1);
					cprintf("SCATid: %d Freq: %3d, prty %d", (WORD)sysid, (WORD)freq,
					        (int)((info.rawBits >> 9) & 0x07));
					textcolor(color_norm);
				}
				sprintf(tmpArea, "SCATid f=%d,id=%d", (WORD)ofreq, (WORD)osysid);
			}
			else
				// adjoining system
				sprintf(tmpArea, "Ncell f=%d,id=%d", (WORD)ofreq, (WORD)osysid);
			emit(tmpArea);
			haveSaved = FALSE;
		}
		else if ((info.rawBits & 0xFF80000L) == 0xFF00000L)
		{
			// possible 'new' equivalent (perhaps alternate is a better word than new...)
			nsysid = ((int)info.rawBits) & 0x03f;                           // 6-bit sysid
			nfreq = (info.rawBits >> 12) & 0x01f;                           // 5-bit freq#
			if ( info.rawBits & 0x100L )                                    // home system
			{
				if (sysid != nsysid || ((int)nfreq) != freq)
				{
					sysid = nsysid;
					freq = (int)nfreq;
					textcolor(color_active);
					gotoxy(20, 2);
					cprintf("altSysId: %d Freq: %d, prty %d", (WORD)sysid, (WORD)freq,
					        (int)((info.rawBits >> 9) & 0x07));
					textcolor(color_norm);
				}
				sprintf(tmpArea, "altSysid f=%d,id=%d", (WORD)nfreq, (WORD)nsysid);
			}
			else
				// adjoining system
				sprintf(tmpArea, "altNeighbor f=%d,id=%d", (WORD)nfreq, (WORD)nsysid);
			emit(tmpArea);
			haveSaved = FALSE;
		}
		else if ((info.rawBits & 0xFF00000L) == 0xFC00000L )
		{
			/* idle code as far as I can tell...
			        I've seen data such as:
			        FC40000
			        FC8FFFF
			        FC9FFFF
			 */
			haveSaved = FALSE;
			emit("Idle?");
		}
		else
		{
			emit("Unknown");
			haveSaved = FALSE;
		}
	}
	else
		emit("");

}

/************************************************************************/
/*                        proc_cmd                                      */
/*                                                                      */
/* This routine processes a data frame by checking the CRC and nicely   */
/* formatting the resulting data into structure info                    */
/************************************************************************/
void
proc_cmd(int c, struct ccdat& info)
{
	static int ecc = 0, sre = 0, cc = 0, ud = 0xA9C, nbb = 0, tb, orf, i;
	static char oub[50];

	if (c < 0)
	{
		long newRaw = 0;

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
		if (nbb == 0)
			info.bad = 0;
		else
			info.bad = 1;
		nbb = 0;
	}
	else
	{

		/* bits 1 through 28 will be run through crc routine */
		if (cc < 28)
		{
			oub[cc++] = (char)c;
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
	static struct ccdat firstCmd, secondCmd;

	/* do the first data frame */

	for (i = 0; i < 40; i++)
	{
		l = (int)( (ob[i] << 2) + (ob[i + 40] << 1) + ob[i + 80]);          /* form triplet */
		l = tal[l];                                                         /* look up the correct bit value in the table */
		proc_cmd(l, firstCmd);                                              /* send out bit */
	}
	proc_cmd(-1, firstCmd);                                                 /* reset proc_cmd routine */

	/* do the second data frame */
	for (i = 0; i < 40; i++)
	{
		l = (int)( (ob[i + 120] << 2) + (ob[i + 160] << 1) + ob[i + 200]);
		l = tal[l];
		proc_cmd(l, secondCmd);
	}
	proc_cmd(-2, secondCmd);
	if ( firstCmd.bad )
		fprintf(myFile, "*badcrc ");
	else
		fprintf(myFile, "%07lx ", firstCmd.rawBits);
	if ( secondCmd.bad )
		fprintf(myFile, "*badcrc ");
	else
		fprintf(myFile, "%07lx ", secondCmd.rawBits);
	if ( firstCmd.bad )
		emit("-------");
	else
		show(firstCmd);
	if ( secondCmd.bad )
		emit("-------");
	else
		show(secondCmd);
	fputc('\n', myFile);
}
/************************************************************************/
/*                       frame_sync                                     */
/*                                                                      */
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

long theDelay;
int mainLoop()
{
	register unsigned int i = 0;
	int ef = 0, ch;
	register char s = 0;
	register double dt, exc = 0.0, clk = 0.0, xct, dto2;

	//register short dt,exc=0,clk=0,xct,dto2;
	myFile = fopen("edump.txt", "wt");
	if ( !myFile )
	{
		perror("edump.txt");
		_exit(2);
	}
	setvbuf(myFile, niceBuffer, _IOFBF, sizeof(niceBuffer));

	clrscr();

	/* dt is the number of expected clock ticks per bit */
	dt =  1.0 / ((Special_Baud ? 4800.0 : 9600.0) * 838.22e-9);
	dto2 = 0.5 * dt;
//  dt =  124;
//  dto2 = 62;

	printf("E-Dumper %d\r\nIf program just sits here press any key to exit...\r\nand check settings / read trunker/etrunk docs\n", Special_Baud ? 4800 : 9600);
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
				//set_freak(chan[control]);
				mode = 0;
				i = cpstn;
			}
		}

		if (i != cpstn)
		{
			if (Special_Baud)
			{
				if (slicerInverting)                                  // 4800 edacs is opposite 9600 edacs
					s = (fdata [i] & 0x8000) ? 1 : 0;
				else
					s = (fdata [i] & 0x8000) ? 0 : 1;
			}
			else
			{
				if (slicerInverting)                                  // for some reason, motorola is opposite of edacs, mdt!
					s = (fdata [i] & 0x8000) ? 0 : 1;
				else
					s = (fdata [i] & 0x8000) ? 1 : 0;
			}

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
				//set_freak(chan[control]);
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
	if (myFile)
	{
		fclose(myFile);
		myFile = 0;
	}
	gotoxy(1, 23);
	printf("\n");
}

