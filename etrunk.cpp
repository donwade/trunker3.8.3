#include <platform.h>
#include <osw.h>
#include <trunksys.h>
#include <time.h>
extern int Special_Baud;

#define MAXLCN 27
#define SCATMAX 8
BOOL wantScanner = TRUE;
int isScat = 0;
time_t now = 0;
time_t nowFine = 0;
#define WORDSPERSEC ((2 * (Special_Baud ? Special_Baud : 9600)) / (48 + 120 + 120))
int squelchDelay = WORDSPERSEC / 2;
int wordsPerSec = WORDSPERSEC;
BOOL syncState = FALSE;


FreqMap  *myFreqMap = 0;
TrunkSys  *mySys = 0;

unsigned long goodcount = 0;
unsigned long badcount = 0;
char tempArea [512];
unsigned short sysId = 0,  cwFreq = 0;
long theDelay = DELAY_ON;
BOOL Verbose = TRUE;
BOOL showFrameSync = FALSE;
unsigned long stackOverFlows = 0L;
unsigned long wrapArounds = 0L;

/* global variables */
volatile BOOL InSync = FALSE;
static Filter filterSysId(3);
extern char *mapOtherCodeToF(unsigned short);
class Emap : public FreqMap {
public:
virtual BOOL isFrequency(unsigned short theCode, char)
{
	return ( theCode > 0 && theCode < 0x1d ) ? TRUE : FALSE;
}
virtual char *mapCodeToF(unsigned short theCode, char)
{
	return mapOtherCodeToF(theCode);
}
};
void
noteIdentifier(unsigned short possibleId)
{
	if (filterSysId.filter(possibleId) == possibleId)
	{
		if ( possibleId != sysId && mySys)
		{
			mySys->unsafe();
			delete mySys;
			mySys = 0;
		}
		if (mySys)
			mySys->endScan();                                     // take opportunity to age assignments
		else
		{
			sysId = possibleId;
			mySys = new TrunkSys(sysId, myScanner, Special_Baud ? "E48" : "E");
			if ( !mySys )
				shouldNever("cannot alloc TrunkSys objects");
			register int i;
			for (i = 0; i < 8; ++i)
				if ( mySys->types[i] == '?')
					mySys->types[i] = '0';
			mySys->setBasicType("E-GE");
			mySys->antiScannerTones = TRUE;
			isScat = 0;
		}
	}
}

//#define LIGHTGRAY WHITE
//*                    global variables                                */
int lc = 0;
char ob[1000];          /* buffer for raw data frames                         */
int obp = 0;            /* pointer to current position in array ob            */
static int dotting = 0; /* dotting sequence detection flag      */
static int mode = 0;    /* mode flag; 0 means we're on control  */
                        /* channel; 1 means active frequency    */


/**********************************************************************/
/*               higher level stuff follows                           */
/**********************************************************************/

/* structure for storing the control channel data frame information  */
struct ccdat
{
	long rawBits;               /* for dumb dumping */
//   int cmd;  /* command */
	unsigned short lcn;         /* logical channel number */
//   int stat; /* status bits */
	unsigned short id;          /* agency/fleet/subfleet id */
	BOOL bad;                   /* CRC check flag - when set to 1 it means data is corrupt */
};

/* array acn[] indicates which channels are currently active; aid[]  */
/* indicates which id is using the active channel                    */
/* acn[] is actually a countdown timer decremented each time a       */
/* control channel command is received and is reset whenever that    */
/* channel appears in control channel data stream. When this reaches */
/* zero the channel is assumed to have become inactive.              */
/* aid[] is used to make sure a new group appearing on an active     */
/* channel is recognized even if acn[] had not yet reached zero      */


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
	static short idup = 0, cdup = 0;
	static char dup[4] = { 45, 47, 124, 92 };

	unsigned short osysid;
	unsigned short ofreq;

	static struct ccdat savedDat;
	static BOOL haveSaved = FALSE;
	static BOOL isGrp;
	static BOOL isPhone;
	static BOOL isDigital;
	static BOOL isEmergency;
	static unsigned short netFlags;

	/* update the "I'm still receiving data" spinwheel character on screen
	 */
	if (showFrameSync)
	{
		if ( ++cdup > 9)
		{
			gotoxy(51, 1);
			idup = (unsigned short)((idup + 1) & 0x03);
			putch(dup[idup]);
			cdup = 0;
		}
	}
	/* process only good information blocks  */
	if (info.bad)
	{
		++badcount;
		haveSaved = FALSE;
	}
	else
	{
		++nowFine;                                      // fine grain counter = #osw's = 90hz
		if (mySys && !(nowFine & 7) )                   // about 15x/sec + at scan marker
			mySys->endScan();
		++goodcount;

		if ( (info.rawBits & 0x8000000) == 0L)                      // 010 aegis digital, 000 normal
		{                                                               // 0x1 emergency / priority
			/*
			   Call Grant, probably same for group and priority group call

			   patent 4,939,746:

			   0xx iiiiiiii ccccc t ggg gggg gggg
			 |   |        |     | |
			 |   |        |     | +-11 bits called group
			 |   |        |     +--- 1 bit  transmission or message mode
			 |   |        +--------- 5 bits channel #
			 |   +------------------ 8 bits 1/2 calling logical ID
			 |||+---------------------- 3 bits Message Type A = 0xx

			   first word is 8 low bits, second word
			   is 8 high bits, including redundant 2 bits!

			   second bit is digital, 3rd bit is priority/emergency

			   the I-call equivalent of this is a pair of F6 commands

			 */

			if ( haveSaved && ((savedDat.rawBits & 0xE000000L) == (info.rawBits & 0xE000000L)))
			{
				if ((savedDat.rawBits & 0x001F000) == (info.rawBits & 0x001F000)
				    && (savedDat.id == info.id) )
				{
					if ( mySys )
					{
						static unsigned short caller;
						netFlags = (info.rawBits & 0x4000000L) ? OTHER_DIGITAL : 0;
						if (info.rawBits & 0x2000000L)
						{
							if ( mySys->patchesActive && mySys->getPatchDes(info.id, TRUE) )
								netFlags |= 4;                                                              // patch + emergency
							else
								netFlags |= 2;                                                              // emergency only
						}
						else if (mySys->patchesActive && mySys->getPatchDes(info.id, TRUE) )
							netFlags |= 3;                                                              // patch only

						caller = (unsigned short)((savedDat.rawBits >> 11) & 0x3f00);
						caller |= (unsigned short)((info.rawBits >> 17) & 0x00ff);
						mySys->type0Gcall((unsigned short)((info.rawBits >> 12) & 0x01f),
						                  info.id,
						                  caller,
						                  netFlags);
						if (isScat)
							mySys->endScan();
					}
				}
				haveSaved = FALSE;
			}
			else
			{
				savedDat = info;
				haveSaved = TRUE;
			}
		}
		else if ((info.rawBits & 0xFC00000) == 0xF000000)
		{
			// patch alias - again thanks to Wayne:
			// 111100 aaaaaaaaaaa ggggggggggg
			// the patch alias will be in the range 7f0...7ff???? will allow all!


			if (mySys)
			{
				// have to do this twice: once for the base group, and once for the
				// group that is being re-assigned to that base group. that way the group
				// id will be found in the list of groups associated with the patch

				mySys->note_patch((unsigned short)((info.rawBits >> 11) & 0x07ff),
				                  (unsigned short)((info.rawBits >> 11) & 0x07ff),
				                  '0', FALSE);
				mySys->note_patch((unsigned short)((info.rawBits >> 11) & 0x07ff),
				                  (unsigned short)(info.id),
				                  '0', FALSE);
			}
		}
		else if ((info.rawBits & 0xFF80000L) == 0xFD00000L)
		{
			/* Site ID Message

			    actual data at Wilmington, Ohio shows:

			        FD01141

			        1111 1101 0000 0001 0001 0100 0001
			        aaab bbcc cddc cccc ppph ffii iiii  - consistent with 5,077,828
			        1111 1111 1                         - use this as a mask
			        1111 1101 0                                                 - match with this
			 */
			osysid = (unsigned short)(info.rawBits & 0x03f);            // 6-bit sysid
			ofreq  = (unsigned short)(((info.rawBits >> 12)) & 0x01f);  // 5-bit freq#
			if ( info.rawBits & 0x100L )                                // home system
			{                   // we have 3-4 chances to detect this condition to jump
				                                                        // off of the data channel!

				// just after a scat call we get 3 or 4 ids with home, then without.

				if ( mySys )
					mySys->noteDataChan(ofreq);
				noteIdentifier(osysid);
			}
			else if ((((int)info.rawBits) & 0x0c0) == 0x0c0 )                             // SCAT!
			{
				noteIdentifier(osysid);
				if ( mySys )
				{
					if ( isScat < SCATMAX && ++isScat >= SCATMAX )
						mySys->setOneChannel("/SCAT");
					mySys->noteDataChan(ofreq);
				}
			}
			else
			{
				// adjoining system
				if (mySys)
					mySys->setNetworked();
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

			if ( haveSaved && ((savedDat.rawBits & 0xFC00000) == 0xF400000))                            // i-call
			{
				if ((savedDat.rawBits & 0x00F8000) == (info.rawBits & 0x00F8000))                       // same lcn
				{
					if (mySys && (info.lcn <= MAXLCN))
					{
						unsigned short src, dst;
						src = (unsigned short)(savedDat.rawBits  & 0x3FFF);
						dst = (unsigned short)(info.rawBits  & 0x3FFF);
						if (!src)
							src = 0x4000;                                                                               // not a real id
						if (!dst)
							dst = 0x4000;                                                                               // not a real id
						mySys->type0Icall(info.lcn, dst, src, FALSE, FALSE, FALSE);
						if (isScat)
							mySys->endScan();
					}
				}
				haveSaved = FALSE;
			}
			else
			{
				savedDat = info;
				haveSaved = TRUE;
			}
		}
		else if ( (info.rawBits & 0xFC00000) == 0xEC00000)  // 111-011 various calls, Late  entry
		{               // 1110 11 [type]       LLLL Ltii iiii iiii iiii
			                                                //
			                                                /*
			                                                          revised:

			                                                          Group calls:

			                                                          1110 11 10 LLLL L0xx xGGG GGGG GGGG     - group call
			                                                          1110 11 00 LLLL L0xx xGGG GGGG GGGG     - +emergency
			                                                          1110 11 11 LLLL L0xx xGGG GGGG GGGG     - +digital
			                                                          1110 11 01 LLLL L0xx xGGG GGGG GGGG     - +digital +emergency

			                                                          I-calls:

			                                                          1110 11 10 LLLL L1II IIII IIII IIII     - analog I call
			                                                          1110 11 11 LLLL L1II IIII IIII IIII     - ?digital I call?
			                                                          1110 11 00 LLLL L1II IIII IIII IIII     - ?analog phone patch
			                                                          1110 11 01 LLLL L1II IIII IIII IIII     - ?digital phone patch ? not sure
			                                                 */

			static unsigned short relId;                    // group OR radio ID


			if ( info.rawBits & 0x04000 )                                           // individual
			{
				relId = (WORD)(info.rawBits & 0x0003FFF);                           // 14-bit radio logical ID
				isGrp = FALSE;
				isEmergency = FALSE;
				switch (info.rawBits & 0x0300000L)
				{
				case 0x0000000L:
					isPhone = TRUE;         isDigital = FALSE; break;
				case 0x0100000L:
					isPhone = TRUE;         isDigital = TRUE; break;
				case 0x0200000L:
					isPhone = FALSE;        isDigital = FALSE; break;
				case 0x0300000L:
					isPhone = FALSE;        isDigital = TRUE; break;
				}
			}
			else
			{
				relId = (WORD)(info.id);                                          // 11-bit group ID
				isGrp = TRUE;
				isPhone = FALSE;
				isEmergency = (info.rawBits & 0x0200000L) ? FALSE : TRUE;
				isDigital =   (info.rawBits & 0x0100000) ? TRUE : FALSE;
			}
			if ( mySys && (info.lcn <= MAXLCN))
			{
				if ( isGrp )
				{
					netFlags = (isDigital ? OTHER_DIGITAL : 0);
					if ( isEmergency )
					{
						if ( mySys->patchesActive && mySys->getPatchDes(relId, TRUE)  )
							netFlags |= 4;                                                              // emergency + patch = 4
						else
							netFlags |= 2;                                                              // emergency
					}
					else if ( mySys->patchesActive && mySys->getPatchDes(relId, TRUE) )
						netFlags |= 3;                                                              // patch
					mySys->type0Gcall(info.lcn, relId, netFlags);
				}
				else
				{
					if ( isPhone && !relId )
						relId = 0x4000;                                                                 // dummy id for phone
					mySys->type0Icall(info.lcn, relId,
					                  isDigital ? TRUE : FALSE,
					                  isPhone ? TRUE : FALSE,
					                  FALSE);
				}
			}
			haveSaved = FALSE;
			//} else if((info.rawBits & 0xE000000) == 0xA000000 ) { // 101x xxxx xxxx xxxx xxxx xxxx xxxx
			// data call - need better breakdown of bits
			//      haveSaved = FALSE;
		}
		else if ((info.rawBits & 0xE000000) == 0xC000000)   // 110x xxxx xxxx xxxx xxxx xxxx xxxx
		{               // affiliation - thanks to Wayne:
			                                                // ccc ggggggggggg iiiiiiiiiiiiii

			if ( mySys )
			{
				mySys->note_affiliation(
				    (unsigned short)(info.rawBits & 0x3fffL),
				    (unsigned short)((info.rawBits >> 14) & 0x07ffL)
				    );
			}
			haveSaved = FALSE;

		}
		else
			haveSaved = FALSE;
	}
}

/************************************************************************/
/*                        proc_cmd                                      */
/*                                                                      */
/* This routine processes a data frame by checking the CRC and nicely   */
/* formatting the resulting data into structure info                    */
/************************************************************************/
inline void
proc_cmd( short c)
{
	static short ecc = 0, sre = 0, cc = 0, ud = 0xA9C, nbb = 0, tb, orf, i;
	static unsigned char oub[50];
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
			orf <<= 1;
			orf += oub[i];
			newRaw <<= 1;
			newRaw += oub[i];
		}
		orf &= 255;
//    info.cmd = orf;

		/* pick off LCN   (five bits)  */
		for (i = 8; i <= 12; ++i)
		{
			orf <<= 1;
			orf += oub[i];
			newRaw <<= 1;
			newRaw += oub[i];
		}
		orf &= ((short)0x1f);
		info.lcn = orf;

		/* pick off four status bits */
		for (i = 13; i <= 16; ++i)
		{
			orf <<= 1;
			orf += oub[i];
			newRaw <<= 1;
			newRaw += oub[i];
		}
		orf &= 15;
//    info.stat = orf;

		/* pick off 11 ID bits    */
		for (i = 17; i <= 27; ++i)
		{
			orf <<= 1;
			orf += oub[i];
			newRaw <<= 1;
			newRaw += oub[i];
		}
		orf &= (short)0x07ff;
		info.id = orf;
		info.rawBits = newRaw;
		info.bad = (nbb == 0) ? 0 : 1;

		show(info);

		nbb = 0;
		wordNum = 0 - c;
	}
	else
	{

		/* bits 1 through 28 will be run through crc routine */
		if (cc < 28)
		{
			oub[cc++] = (unsigned char)c;
			if ( c == 1)
				ecc = ecc ^ sre;
			if ( (sre & 0x01) > 0)
				sre = (short)((sre >> 1) ^ ud);
			else
				sre >>= 1;
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
			ecc = (short)((ecc << 1) & 0xfff);
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
	register short i, l;
	static short tal[9] = { 0, 1, 0, 0, 1, 1, 0, 1 };

	/* do the first data frame */
	proc_cmd(-1);                                                               /* reset proc_cmd routine */
	for (i = 0; i < 40; i++)
	{
		l = (short)( (ob[i] << 2) + (ob[i + 40] << 1) + ob[i + 80]);                /* form triplet */
		l = tal[l];                                                                 /* look up the correct bit value in the table */
		proc_cmd(l);                                                                /* send out bit */
	}

	/* do the second data frame */
	proc_cmd(-2);
	for (i = 0; i < 40; i++)
	{
		l = (short)( (ob[i + 120] << 2) + (ob[i + 160] << 1) + ob[i + 200]);
		l = tal[l];
		proc_cmd(l);
	}

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
		if ( i < 13)                  // ignore 3 bits in this check
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
			InSync = TRUE;
			if ( hof > 300 )
				badcount += 1 + 2 * (hof / 287);
		}
		/* mode 1 means we're on voice channel and so we just found dotting */
		else if ((mode == 1) && (nh < 2) )
			dotting++;
	}
	else if ( hof > 287 )
		InSync = FALSE;


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
inline void
periodic()
{
	static time_t winCtr = 0;

	if ( (now - winCtr) >= 5 )
	{
		if (mySys)
		{
			mySys->flushFreqs();
			mySys->clean_stale_patches();
			_settextcolor(color_norm);
			_settextposition(STATROW + 1, 19);
			cprintf(mySys->sysType());
		}
		stats(scanTypeEnv);
		winCtr = now;
	}
}

int mainLoop()
{

	char dataBit;
	unsigned short passCounter = 0;
	unsigned int overflowCounter;
	register unsigned int i = 0;
	const double ticksPerBit = 1.0 / ((Special_Baud ? ((double)Special_Baud) : 9600.0) * 838.8e-9);
	const double ticksPerBitOver2 = 0.5 / ((Special_Baud ? ((double)Special_Baud) : 9600.0) * 838.8e-9);
	register double exc = 0.;
	register double accumulatedClock = 0.;
	register double fastBoundaryCompare;

	//
	//      Panic if we can't create objects
	//
	if (!myScanner)
		shouldNever("Could not create scanner object");


	if (actualRows < 43)
		shouldNever("need 43 row mode");

	//      Core loop that makes this thing work
	//
	showPrompt();

	//
	//      Create the TrunkSys object, default to system ID of 1234 until we
	//      figure out what we're really listening to.
	//
	mySys = new TrunkSys(0x1234, myScanner, Special_Baud ? "E48" : "E");
	myFreqMap = new Emap;
	cpstn = 0;
	cpstn = 0;

	for (;; )
	{
		time(&now);                             // per-event time

		for (overflowCounter = BufLen; i != cpstn; --overflowCounter)
		{
			//
			//  We never caught up to the input side during loop, drop around
			//  and note that we are in trouble.
			//
			if (overflowCounter == 0)
			{
				++wrapArounds;
				break;
			}

			//
			//  dataBit becomes the data bit, being either 0 or 1.
			//
			if (Special_Baud)
			{
				if (slicerInverting)                                              // 4800 edacs is opposite 9600 edacs
					dataBit = (fdata [i] & 0x8000) ? 1 : 0;
				else
					dataBit = (fdata [i] & 0x8000) ? 0 : 1;
			}
			else
			{
				if (slicerInverting)                                              // for some reason, motorola is opposite of edacs, mdt!
					dataBit = (fdata [i] & 0x8000) ? 0 : 1;
				else
					dataBit = (fdata [i] & 0x8000) ? 1 : 0;
			}

			//
			//      Add in new number of cycles to clock
			//
			accumulatedClock += fdata [i] & 0x7fff;

			//
			//  Send raw bit stream to first processing routine.  Since we're
			//  in a while loop, wer assigned the math to a temporary variable,
			//  so we're not recalculating everytime.
			//
			fastBoundaryCompare = exc + ticksPerBitOver2;

			while (accumulatedClock >= fastBoundaryCompare)
			{
				frame_sync(dataBit);
				accumulatedClock -= ticksPerBit;
			}

			//
			//  PHASE LOCK LOOP ALGORITHM!
			//

			// for performance reasons, do this less frequently, perhaps on rising edge..

			if ( dataBit )
				exc += (accumulatedClock - exc) / 15.;

			if (++i > BufLen)
				i = 0;
		}
		//
		//      Every 32768 times?
		//
		if ((passCounter++ << 1) == 0)
			periodic();

		//
		//      Every eighth time
		//
		if (showFrameSync && !(passCounter & 0x007))
		{
			if (InSync != syncState)
			{
				if (syncState)
					syncState = FALSE;
				else
					syncState = TRUE;

				gotoxy(52, STATROW);
				_settextcolor(color_norm);
				_outtext(syncState ? "@" : "_");
			}
		}

		//
		//      If user typed any keys, process them
		//
		static rccoord wasat;
		static BOOL coordset = FALSE;                    // true if we have saved coordinates from key fcns

		if (coordset)
			_settextposition(wasat.row, wasat.col);
		//
		//      If user typed any keys, process them
		//
		if (kbhit())
		{
			doKeyEvt();
			wasat = _gettextposition();
			coordset = TRUE;
		}
	}
}
void localCleanup()
{
	delete mySys;
	mySys = 0;
}

