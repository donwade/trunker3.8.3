#include <platform.h>
#include <osw.h>
#include <time.h>
static char *myversion = "18-Feb-2001";
BOOL wantScanner = FALSE;
long theDelay;
time_t now, nowFine;
class MotoMap : public FreqMap {
public:
virtual char *mapCodeToF(unsigned short theCode, char)
{
	static char myres[14];

	if ( theCode <= 0x2cf )
		sprintf(myres, "%8.4f", 851.0125 + (0.025 * ((double)theCode)));
	else if ( theCode <= 0x2f7 )
		sprintf(myres, "%8.4f", 848.0000 + (0.025 * ((double)theCode)));
	else if ( theCode >= 0x32f && theCode <= 0x33f )
		sprintf(myres, "%8.4f", 846.6250 + (0.025 * ((double)theCode)));
	else if ( theCode >= 0x3c1 && theCode <= 0x3fe )
		sprintf(myres, "%8.4f", 843.4000 + (0.025 * ((double)theCode)));
	else
		sprintf(myres, "0x%03hx   ", (WORD)theCode);                               // 3ff, 3be
	return myres;
};
virtual int isFrequency(unsigned short theCode, char)
{
	if ( theCode >= 0 && theCode <= 0x2cf)
		return TRUE;
	if (theCode >= 0 && theCode <= 0x2f7)
		return TRUE;
	if (theCode >= 0x32f && theCode <= 0x33f)
		return TRUE;
	if (theCode >= 0x3c1 && theCode <= 0x3fe)
		return TRUE;
	return FALSE;
};
};
FreqMap *myFreqMap = new MotoMap;
char tempArea2[150];

#define NBYTESFORSTDIO 5120
char niceBuffer[NBYTESFORSTDIO];

char tempArea[150];
/* global variables */

static char ob[100];   /* output buffer for packet before being sent to screen */
struct osw_stru
{
	unsigned short cmd;
	unsigned short id;
	char grp;
};

static struct osw_stru stack[5];
static short numStacked = 0;
static short numConsumed = 0;
static FILE *dumpOut;
void
checkDate()
{
	static time_t lastT = 0;
	register char *cp1, *cp2;

	now = time(0);

	if (now != lastT)
	{
		cp1 = ctime(&now);
		cp2 = strrchr(cp1, '\n');
		if ( cp2 )
			*cp2 = '\0';
		cp2 = strrchr(cp1, '\r');
		if ( cp2 )
			*cp2 = '\0';
		fprintf(dumpOut, "\n========================\n%s           Dumper Version %s\n========================", cp1, myversion);
		lastT = now;
	}
}
void
emit(const char * op, BOOL indent = TRUE)
{
	if ( indent )
		putc('\t', dumpOut);
	else
		checkDate();
	fputs(op, dumpOut);
}

void
show_bad_osw()
{
	// clear out work in progress...

	checkDate();
	if ( numStacked )
		emit("\n<chksum, clearing stack>\n", FALSE);
	numStacked = 0;
	numConsumed = 0;
}
void didConsume(register short n)
{
	register short i = 2;

	checkDate();

	if ( n >= 1 && n <= 3)
	{
		numConsumed = n;
		for (; n > 0; --n, --i)
		{
			sprintf(tempArea2, "\n%04hx %c %04hx", stack[i].id, stack[i].grp ? 'G' : 'I', stack[i].cmd);
			emit(tempArea2, FALSE);
		}
	}
}
void
didUnder(register struct osw_stru* Inposw)
{
	checkDate();
	sprintf(tempArea2, "\n%04hx %c %04hx", Inposw->id, Inposw->grp ? 'G' : 'I', Inposw->cmd);
	emit(tempArea2, FALSE);
}
void
doIdSequence()
{
	if ( myFreqMap->isFrequency(stack[0].cmd, '8') &&
	     (stack[0].id & 0xff00) == 0x1F00 &&
	     (stack[1].id & 0xfc00) == 0x2800 &&
	     stack[0].cmd == (stack[1].id & 0x3FF)

	     )
	{
		didConsume(3);                         // must occur BEFORE emit...
		/* non-smartzone identity:                    sysid[10]
		                <sysid>              x   308
		                <001010><ffffffffff> x   30b
		                1Fxx                 x   <ffffffffff>
		 */
		sprintf(tempArea, "Non-Net ID=%04hx, DChan=%04hx, Freq=%s\n",
		        stack[2].id, stack[0].cmd, myFreqMap->mapCodeToF(stack[0].cmd, '8'));
	}
	else
	{
		if (stack[1].id & 0xfff0 == 0x26f0)                          // ack type 2 text msg
		{
			didConsume(2);
			sprintf(tempArea, "Radio %.4hx Message %hd\n", stack[2].id, stack[1].id & ((unsigned short)15));
		}
		else if (stack[1].id & 0xfff8 == 0x26E0)                            // ack type 2 status
		{
			didConsume(2);
			sprintf(tempArea, "Radio %.4hx Status %hd\n", stack[2].id, stack[1].id & ((unsigned short)7));
		}
		else
			switch (stack[1].id & 0xfc00)
			{
			case 0x2800:
				didConsume(2);
				/* smartzone identity:
				                <sysid>              x   308
				                <001010><ffffffffff> x   30b
				 */
				sprintf(tempArea, "    Net ID=%04hx, DChan=%04hx\n",
				        stack[2].id, stack[1].id & 0x03ff);
				break;
			case 0x6000:
				didConsume(2);
				/* smartzone peer info:
				                <sysid>              x   308
				                <011000><ffffffffff> x   30b
				 */
				sprintf(tempArea, "Smartzone PeerInfo, SysId=%04hx, DChan=%04hx, Freq=%s\n",
				        stack[2].id, stack[1].id & 0x03ff, myFreqMap->mapCodeToF((unsigned short)(stack[1].id & 0x03ff), '8'));
				break;
			case 0x2c00:                             // various rejections:
				didConsume(2);
				switch ( stack[1].id & 0x03ff)
				{
				case 0x04a:
					sprintf(tempArea, "Radio %04hx waved off because of Radio ID", stack[2].id);
					break;
				case 0x040:
					sprintf(tempArea, "Radio %04hx waved off because of Group ID", stack[2].id);
					break;
				case 0x016:
					sprintf(tempArea, "Group %04hx Error???", stack[2].id);
					break;
				case 0x002:
					sprintf(tempArea, "Radio %04hx is invalid", stack[2].id);
					break;
				case 0x005:
					sprintf(tempArea, "Radio %04hx using invalid group ID", stack[2].id);
					break;
				case 0x003:
					sprintf(tempArea, "Radio %04hx target radio unavailable", stack[2].id);
					break;
				default:
					sprintf(tempArea, "*** Radio %04hx, Unknown Sub-Code %04hx",
					        stack[2].id, stack[1].id & 0x03ff);
					break;
				}
				break;
			case 0x2400:                             // various
				didConsume(2);
				switch ( stack[1].id & 0x03ff)
				{
				case 0x219:
					sprintf(tempArea, "Radio %04hx Crypt Key Failure\n", stack[2].id);
					break;
				case 0x21b:
					sprintf(tempArea, "Radio %04hx Radio Check\n", stack[2].id);
					break;
				case 0x21c:
					sprintf(tempArea, "Radio %04hx De-Affiliated\n", stack[2].id);
					break;
				default:
					sprintf(tempArea, "*** Radio %04hx, Unknown Sub-Code %04hx\n",
					        stack[2].id, stack[1].id & 0x03ff);
					break;
				}
				break;
			case 0x2000:                             //function 0x2000
				didConsume(2);
				switch ( stack[1].id & 0x03ff)
				{
				case 0x021:
					sprintf(tempArea, "Cancel Multi Select %04x", stack[2].id);
					break;
				case 0x02a:
					sprintf(tempArea, "Radio %04x Disabled in System Database\n", stack[2].id);
					break;                                                    // radio NOT enabled in SAC.
				default:
					sprintf(tempArea, "Radio %04x, 2000 Unk Sub-Code %04x\n",
					        stack[2].id, stack[1].id & 0x03ff);
					break;
				}
				break;
			case 0x8000:                              //sims II Functions  0x8000
				didConsume(2);
				switch ( stack[1].id & 0x03ff)
				{
				//case 0x235: // failsoft assigned to channel
				//            sprintf(tempArea,"Failsoft Assigned, Id %04x channel %04x\n",stack[2].id,stack[2].cmd);
				//            break;
				//acks
				case 0x301:                                         // ack failsoft assign ack
					sprintf(tempArea, "Failsoft Assign Ack, Id %04x\n", stack[2].id);
					break;
				case 0x302:                                         // selector lock disabled ack
					sprintf(tempArea, "Cancel Selector Locked Ack, Id %04x\n", stack[2].id);
					break;
				case 0x303:                                         // selector lock ack
					sprintf(tempArea, "Selector Locked Ack, Id %04x\n", stack[2].id);
					break;
				case 0x305:                                         // failsoft cancel ack
					sprintf(tempArea, "Cancel Failsoft Ack, Id %04x\n", stack[2].id);
					break;
				case 0x307:                                         // inhibit radio ack
					sprintf(tempArea, "Radio Inhibit Ack, Id %04x\n", stack[2].id);
					break;
				case 0x308:                                         // inhibit radio canceled ack
					sprintf(tempArea, "Cancel Radio Inhibit Ack, Id %04\n", stack[2].id);
					break;

				//commands
				case 0x312:                                         //selector lock disabled
					sprintf(tempArea, "Disable Selector Lock, Id %04x", stack[2].id);
					break;
				case 0x313:                                         //selector lock
					sprintf(tempArea, "Selector Locked, Id %04x\n", stack[2].id);
					break;
				case 0x315:                                         //failsoft cancel
					sprintf(tempArea, "Failsoft cancelled, Id %04x\n", stack[2].id);
					break;
				case 0x317:                                         //inhibit radio
					sprintf(tempArea, "Radio Inhibit, Id %04x\n", stack[2].id);
					break;
				case 0x318:                                         //inhibit radio canceled
					sprintf(tempArea, "Cancel Radio Inhibit, Id %04x\n", stack[2].id);
					break;
				default:
					sprintf(tempArea, "Radio %04x, 8000 Unknown Sub-Code %04x\n",
					        stack[2].id, stack[1].id & 0x03ff);
					break;
				}

				break;
			default:
				didConsume(2);
				sprintf(tempArea, "Unknown Info, Tag=%04hx, DChan=%04hx, Freq=%s\n",
				        stack[2].id, stack[1].id & 0x03ff, myFreqMap->mapCodeToF((unsigned short)(stack[1].id & 0x03ff), '8'));
				break;
			}
	}
	emit(tempArea);
}
const char *
siteDetails(unsigned short id)
{
/*
   courtesy of wayne:
   6x3x1x6?
 | | | |
 | | | +----- see bits 0-5 below
 | | +------- always zero
 | +--------- band see bits 7-9 below
 |||+----------- 6-bit cell #

   Bits 7-9 Band
        000: 800
        001: ?
        010: 800
        011: 821
        100: 900
        110: ?
        111: Frequency Defined

   Bit 6 will always be 0 (With respect to 1FC0-1FFF range)
   Bit 5 indicates VOC
   Bit 4 indicates Wide Area Trunking (If 0, Site Trunking)
   Bit 3 indicates Astro
   Bit 2 indicates Analog
   Bit 1 indicates 12kbit Encryption
   Bit 0 indicates Active or Inactive
 */
	static char mydetail[120];

	switch ((id >> 7) & 7)
	{
	case 0:
	case 2:
		strcpy(mydetail, "800");
		break;
	case 3:
		strcpy(mydetail, "821");
		break;
	case 4:
		strcpy(mydetail, "900");
		break;
	default:
		strcpy(mydetail, "?");
		break;
	}
	if ( id & 32 )
		strcat(mydetail, ", VOC");
	if ( id & 16 )
		strcat(mydetail, ", Wide");
	else
		strcat(mydetail, ", Site");
	if (id & 8)
		strcat(mydetail, ", Astro");
	if (id & 4)
		strcat(mydetail, ", Analog");
	if (id & 2)
		strcat(mydetail, ", 12khz Encrypted");
	if (id & 1)
		strcat(mydetail, ", Active");
	return mydetail;
}
void
doMultiWord()
{
	register unsigned int tt1, tt2;

	switch (stack[1].cmd)
	{
	case 0x30b:
		doIdSequence();
		return;                         // some cases > 2 words
	case 0x323:
		sprintf(tempArea, "Emergency Announcement: Radio %04hx, Group %04hx\n", stack[2].id, stack[1].id);
		break;
	case 0x30d:         // "message" ?
		switch (stack[1].id & 0x000f)
		{
		case 8:
			sprintf(tempArea, "Emergency Send: Radio %04x, Group %04x\n", stack[2].id, stack[1].id & 0xfff0);
			break;
		default:
			sprintf(tempArea, "Radio %04x, Affiliation Function %d\n", stack[2].id, stack[1].id & 0x000f);
			break;
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			sprintf(tempArea, "Radio %04x, TY2 Status %d\n", stack[2].id, stack[1].id & 0x000f);
			break;
		}
		break;

	case 0x311:         // "message" ?
		sprintf(tempArea, "Radio %04x, Message %d [1..8]\n", stack[2].id, 1 + (stack[1].id & 7));
		break;

	case 0x320:
		if (stack[0].cmd == 0x30b)
		{
			// definitely smart-zone: 308 320 30b
			// _____ID__________________________  G/I  _____CMD_____
			//  <sysId>                           G    308 (or uhf/vhf "input"?)
			//  <cell id 6 bits><see subr()>      G    320
			//  <011000>        <ffffffffff>      G/I  30b
			didConsume(3);
			if ( stack[0].grp )
			{
				sprintf(tempArea, "NetInfo, Sys=%04x, Cell=<%d>, uses DChan=%04x, Band=%s\n",
				        stack[2].id, ((stack[1].id >> 10) & 0x3F) + 1, stack[0].id & 0x03ff,
				        siteDetails(stack[1].id));
			}
			else
			{
				sprintf(tempArea, "NetInfo, Sys=%04x, Cell=<%d>, alt  DChan=%04x, Band=%s\n",
				        stack[2].id, ((stack[1].id >> 10) & 0x3F) + 1, stack[0].id & 0x03ff,
				        siteDetails(stack[1].id));
			}
			emit(tempArea);
			return;                                     // must return here for >2 words
		}
		else
		{
			didConsume(2);
			emit("? Unknown 308/320 Sequence ?");
		}
		return;

	case 0x0310:
		sprintf(tempArea, "Affilation %04x -> %04hx\n", stack[2].id, stack[1].id);
		break;
	case 0x0319:
		sprintf(tempArea, "Page Signal Sent %04hx -> %04hx\n", stack[1].id, stack[2].id);
		break;
	case 0x031a:
		sprintf(tempArea, "Page Acknowledge %04hx -> %04hx\n", stack[1].id, stack[2].id);
		break;
	case 0x340:
		sprintf(tempArea, "Patch# %04hx [Size=A or type 2] includes group %04x\n",
		        stack[1].id,
		        stack[2].id);
		break;
	case 0x341:
		sprintf(tempArea, "Patch# %04hx [Size=B] includes group %04hx\n",
		        stack[1].id,
		        stack[2].id);
		break;
	case 0x342:
		sprintf(tempArea, "Patch# %04hx [Size=C] includes group %04hx\n",
		        stack[1].id,
		        stack[2].id);
		break;
	case 0x343:
		sprintf(tempArea, "Patch# %04hx [Size=D] includes group %04hx\n",
		        stack[1].id,
		        stack[2].id);
		break;
	case 0x344:
		sprintf(tempArea, "Patch# %04hx [Size=E] includes group %04hx\n",
		        stack[1].id,
		        stack[2].id);
		break;
	case 0x345:
		sprintf(tempArea, "Patch# %04hx [Size=F] includes group %04hx\n",
		        stack[1].id,
		        stack[2].id);
		break;
	case 0x346:
		sprintf(tempArea, "Patch# %04hx [Size=G] includes group %04hx\n",
		        stack[1].id,
		        stack[2].id);
		break;
	case 0x347:
		sprintf(tempArea, "Patch# %04hx [Size=H] includes group %04hx\n",
		        stack[1].id,
		        stack[2].id);
		break;
	case 0x348:
		sprintf(tempArea, "Patch# %04hx [Size=I] includes group %04hx\n",
		        stack[1].id,
		        stack[2].id);
		break;
	case 0x349:
		sprintf(tempArea, "Patch# %04hx [Size=J] includes group %04hx\n",
		        stack[1].id,
		        stack[2].id);
		break;
	case 0x34a:
		sprintf(tempArea, "Patch# %04hx [Size=K] includes group %04hx\n",
		        stack[1].id,
		        stack[2].id);
		break;
	case 0x34c:
		sprintf(tempArea, "Patch# %04hx [Size Moto=M Trunker=L] includes group %04hx\n",
		        stack[1].id,
		        stack[2].id);
		break;
	case 0x34e:
		sprintf(tempArea, "Patch# %04hx [Size Moto=O Trunker=M] includes group %04hx\n",
		        stack[1].id,
		        stack[2].id);
		break;
	case 0x350:
		sprintf(tempArea, "Patch# %04hx [Size Moto=Q Trunker=N] includes group %04hx\n",
		        stack[1].id,
		        stack[2].id);
		break;
	case 0x0303:
	case 0x0302:
	case 0x0300:
		sprintf(tempArea, "Busy\t%c%.4hx\t%c%.4hx\n",
		        'I',
		        stack[2].id,
		        stack[1].grp ? 'G' : 'I',
		        stack[1].id
		        );
		break;
	case 0x0322:                // time and date stamp
		tt1 = stack[2].id;
		tt2 = stack[1].id;

		sprintf(tempArea, "Sys Clock (m/d/y): %02u/%02u/%02u %02u:%02u\n",
		        (tt1 >> 5) & 0x000f,
		        (tt1)   & 0x001f,
		        (tt1 >> 9),
		        (tt2 >> 8) & 0x001f,
		        (tt2)   & 0x00ff);
		break;
	default:
		if (myFreqMap->isFrequency(stack[1].cmd, '8'))
		{
			// handle type II call
			sprintf(tempArea, "\tCall\t%c%.4hx\t%c%.4hx\t%.4hx\t%s\n",
			        'I',
			        stack[2].id,
			        stack[1].grp ? 'G' : 'I',
			        stack[1].id,
			        stack[1].cmd,
			        myFreqMap->mapCodeToF(stack[1].cmd, '8')
			        );
			break;
		}
		else
		{
			didConsume(2);
			emit("unknown double-OSW sequence!\n");
		}
		return;
	}
	didConsume(2);
	emit(tempArea);
}
void
doStatus(register unsigned short stat)
{
	register unsigned short statnum;

	statnum = (unsigned short)((stack[2].id >> 13) & 7);         // high order 3 bits opcode
	if (statnum == 1)
	{
		if (stat & ((unsigned short)0x1000))
			strcpy(&tempArea[0], "     System is Type II or IIi, ");
		else
			strcpy(&tempArea[0], "     System is Type I, ");
		statnum = (unsigned short)(stat >> 5);
		statnum &= 7;
		switch (statnum)
		{
		case 0: strcat(tempArea, "Connect Tone=105.88 Hz\n"); break;
		case 1: strcat(tempArea, "Connect Tone=76.76 Hz\n"); break;
		case 2: strcat(tempArea, "Connect Tone=83.72 Hz\n"); break;
		case 3: strcat(tempArea, "Connect Tone=90. Hz\n"); break;
		case 4: strcat(tempArea, "Connect Tone=97.3 Hz\n"); break;
		case 5: strcat(tempArea, "Connect Tone=116.13 Hz\n"); break;
		case 6: strcat(tempArea, "Connect Tone=128.57 Hz\n"); break;
		case 7: strcat(tempArea, "Connect Tone=138.46 Hz\n"); break;
		}
	}
	else
		sprintf(tempArea, "    Status[%hu] = %.04hx\n", statnum, stat & 0x1fff);
	emit(tempArea);
}
void
show_good_osw(register struct osw_stru* Inposw)
{
	unsigned short i;

	if ( Inposw->cmd == 0x2f8 )
	{
		didUnder(Inposw);
		emit("idle code\n");

		// returning here because idles sometimes break up a 3-OSW sequence!

		return;
	}
	if ( Inposw->cmd >= 0x360 && Inposw->cmd <= 0x39F )
	{

		// on one system, the cell id also got stuck in the middle of the 3-word sequence!

		// Site ID
		didUnder(Inposw);
		sprintf(tempArea, "Cell/Site ID <%d>.\n", 1 + (Inposw->cmd - 0x360));
		emit(tempArea);
		// returning here because site IDs sometimes break up a 3-OSW sequence!

		return;
	}

	// maintain a sliding stack of 5 OSWs. If previous iteration used more than one,
	// don't touch stack until all used ones have slid past.

	switch (numStacked)
	{
	case 5:
	case 4:
		stack[4] = stack[3];
	case 3:
		stack[3] = stack[2];
	case 2:
		stack[2] = stack[1];
	case 1:
		stack[1] = stack[0];
	case 0:
		stack[0] = *Inposw;
	}
	if (numStacked < 5)
		++numStacked;
	if ( numConsumed > 0)
		--numConsumed;
	if ( numConsumed || numStacked < 3 )
		return;                  // at least need a window of 3 and 5 is better.

	// look for some larger patterns first... parts of the sequences could be
	// mis-interpreted if taken out of context.

	if (stack[2].cmd == 0x308)
	{
		doMultiWord();
		return;
	}             // end of is 0x308

	// another configuration seen in London, U.K. in 4-500mhz range :
	/*
	                <sysID> x <freq>
	                <?>     x 320
	                <?>     x 30b

	 */
	if ( stack[1].cmd == 0x0320 && stack[0].cmd == 0x030b && myFreqMap->isFrequency(stack[2].cmd, '8'))
	{
		doMultiWord();                         // uhf equivalent of 308/320/30b, with 308 replaced by input frequency
		return;
	}
	if ( stack[2].cmd == 0x0321 && myFreqMap->isFrequency(stack[1].cmd, '8'))
	{
		didConsume(2);
		sprintf(tempArea, "\tAstro\t%c%.4hx\t%c%.4hx\t%.4hx\t%s\t%s\n",
		        'I',
		        stack[2].id,
		        stack[1].grp ? 'G' : 'I',
		        stack[1].id,
		        stack[1].cmd,
		        myFreqMap->mapCodeToF(stack[1].cmd, '8'),
		        stack[1].grp ? ((stack[1].id & 8) ? "Encrypted" : "Clear") : ""
		        );
		emit(tempArea);
		return;
	}
	if ( stack[2].cmd == 0x309)
	{
		if (myFreqMap->isFrequency(stack[1].cmd, '8'))
		{
			// handle type II radio as type I
			didConsume(2);
			sprintf(tempArea, "\t2 as 1\t%c%.4hx\t%c%.4hx\t%.4hx\t%s\n",
			        'I',
			        stack[2].id,
			        stack[1].grp ? 'G' : 'I',
			        stack[1].id,
			        stack[1].cmd,
			        myFreqMap->mapCodeToF(stack[1].cmd, '8')
			        );
			emit(tempArea);
		}
		else
			doMultiWord();
		return;
	}
	if ( stack[2].cmd == 0x304)
	{
		didConsume(2);
		if (myFreqMap->isFrequency(stack[1].cmd, '8'))
		{
			// handle type II radio as type I
			sprintf(tempArea, "\tCoded\t%c%.4hx\t%c%.4hx\t%.4hx\t%s\n",
			        'I',
			        stack[2].id,
			        stack[1].grp ? 'G' : 'I',
			        stack[1].id,
			        stack[1].cmd,
			        myFreqMap->mapCodeToF(stack[1].cmd, '8')
			        );
			emit(tempArea);
		}
		else
			emit("unknown double-OSW sequence!\n");
		return;
	}
	didConsume(1);              // all of the following are single OSW
	switch ( stack[2].cmd )
	{
	case 0x300:
	case 0x301:
	case 0x302:
	case 0x303:
		sprintf(tempArea, "Busy\t%c%.4hx\n",
		        stack[2].grp ? 'G' : 'I',
		        stack[2].id
		        );
		break;
	case 0x030c:         // ty1 phone status
		sprintf(tempArea, "Type II Phone Status %d\n", stack[2].id & 0x000f);
		break;
	case 0x30f:
		sprintf(tempArea, "Type I Phone Disconnect         ID=%04hx\n", stack[2].id);
		break;
	case 0x318:
		sprintf(tempArea, "Type I Call Alert               ID=%04hx\n", stack[2].id);
		break;
	case 0x319:
		sprintf(tempArea, "Type I EMERGENCY                ID=%04hx\n", stack[2].id);
		break;
	case 0x324:
		sprintf(tempArea, "End of Call or Reject           ID=%04hx\n", stack[2].id);
		break;
	case 0x0325:         // ty1 phone status
		sprintf(tempArea, "Type II Interconnect Transpond, ID=%04hx\n", stack[2].id);
		break;
	case 0x32b:         // scan marker
		sprintf(tempArea, "ScanMark, SysId=%04x\n", stack[2].id);
		break;
	case 0x32d:
		emit("System Wide Announcement\n");
		return;
	case 0x3a0:         // diagnostic: cw On, Off
		emit("Diagnostic code %hx\n", stack[2].id);
		return;
	case 0x3a8:
		emit("System Test\n");
		return;
	case 0x3b0:
		emit("CSC Version #\n");
		return;
	case 0x3bf:
		i = (unsigned short)((stack[2].id >> 13) & 7);                         // high order 3 bits opcode
		sprintf(tempArea, "Net Status[%hu] = %.04hx\n",
		        i, stack[2].id & 0x1fff);
		break;
	case 0x3c0:         // system status
		doStatus(stack[2].id);
		return;
	default:
		if (myFreqMap->isFrequency(stack[2].cmd, '8'))
		{
			if (numStacked > 3  )
			{
				// bare assignment
				// this is still a little iffy...
				sprintf(tempArea, "Continuation\t\t%c%.4hx\t%.4hx\t%s\n",
				        stack[2].grp ? 'G' : 'I',
				        stack[2].id,
				        stack[2].cmd,
				        myFreqMap->mapCodeToF(stack[2].cmd, '8'));
				emit(tempArea);
				if ((stack[2].id & 0xff00) == 0x1f00)
					emit("*or* old Sysid\n", FALSE);
				return;
			}
			else
			{
				sprintf(tempArea, "<uncertain>\t\t%c%.4hx\t%.4hx\t%s\n",
				        stack[2].grp ? 'G' : 'I',
				        stack[2].id,
				        stack[2].cmd,
				        myFreqMap->mapCodeToF(stack[2].cmd, '8'));
				break;
			}

		}
		else
		{
			emit("** Unknown single OSW\n");
			return;
		}
	}
	emit(tempArea);
	return;
}

// process de-interleaved incoming data stream
// inputs: -1 resets this routine in prepartion for a new OSW
//                      0 zero bit
//              1 one bit
// once an OSW is received it will be placed into the buffer bosw[]
// bosw[0] is the most recent entry
//
void
inline proc_osw(short sl)
{
	register short l;
	short sr, sax, f1, f2, iid, cmd, neb;
	static short ct;
	static char osw [50], gob [100];
	static struct osw_stru bosw;

	if (sl == -1)
		ct = 0;
	else
	{
		gob [ct] = (char)sl;
		ct++;

		if (ct == 76)
		{
			sr = 0x036E;
			sax = 0x0393;
			neb = 0;

			//
			// run through convolutional error correction routine
			// Reference : US PATENT # 4055832
			//
			for (l = 0; l < 76; l += 2)
			{
				osw [l >> 1] = gob [l];                                                 // osw gets the DATA bits

				if (gob [l])                                                            // gob becomes the syndrome
				{
					gob [l]     ^= 0x01;
					gob [l + 1] ^= 0x01;
					gob [l + 3] ^= 0x01;
				}
			}

			//
			//  Now correct errors
			//
			for (l = 0; l < 76; l += 2)
			{
				if ((gob [l + 1]) && (gob [l + 3]))
				{
					osw [l >> 1] ^= 0x01;
					gob [l + 1]  ^= 0x01;
					gob [l + 3]  ^= 0x01;
				}
			}

			//
			//  Run through error detection routine
			//
			for (l = 0; l < 27; l++)
			{
				if (sr & 0x01)
					sr = (short)((sr >> 1) ^ 0x0225);
				else
					sr >>= 1;

				if (osw [l])
					sax = sax ^ sr;
			}

			for (l = 0; l < 10; l++)
			{
				f1 = (osw [36 - l]) ? 0 : 1;
				f2 = (short)(sax & 0x01);

				sax >>= 1;

				//
				// neb counts # of wrong bits
				//
				if (f1 != f2)
					neb++;
			}


			//
			// if no errors - OSW received properly; process it
			//
			if (neb == 0)
			{
				for (iid = 0, l = 0; l < 16; l++)
				{
					iid <<= 1;

					if (!osw [l])
						iid++;
				}

				bosw.id = (unsigned short)(iid ^ 0x33c7);
				bosw.grp = (osw [16] ^ 0x01) ? TRUE : FALSE;

				for (cmd = 0, l = 17; l < 27; l++)
				{
					cmd <<= 1;

					if (!osw [l])
						cmd++;
				}

				bosw.cmd = (unsigned short)(cmd ^ 0x032a);
				++goodcount;

				show_good_osw(&bosw);
			}
			else
			{
				++badcount;
				show_bad_osw();
			}
		}
	}
}

//
// de-interleave OSW stored in array ob[] and process
//
void inline
pigout(short skipover)
{
	register short i1, i2;

	proc_osw(-1);

	for (i1 = 0; i1 < 19; i1++)
		for (i2 = 0; i2 < 4; i2++)
			proc_osw((short)ob [((i2 * 19) + i1) + skipover]);
}

//
//       this routine looks for the frame sync and splits off the 76 bit
// data frame and sends it on for further processing
//
void inline
frame_sync(char gin)
{
	static short sr = 0;
	static short fs = 0xac;
	static short bs = 0;

	//
	// keep up 8 bit sliding register for sync checking purposes
	//
	sr <<= 1;
	sr &= ((short)0xff);

	if (gin)
		sr |= ((short)1);

	ob [bs - 1] = gin;

	//
	//  If sync seq and enough bits are found - data block
	//
	if ((sr == fs) && (bs > 83))
	{
		//
		//  The original code didn't handle excess bits at beginning.  Not
		//  sure it will make much difference, but I send the last 84 bits
		//  out, not the first 84 bits.  The latest frame is stored in array
		//  ob [].
		//
		pigout((short)(bs - 84));
		bs = 0;
	}

	if (bs < 98)
	{
		//
		//  After lots of garbage, this will show up as a error frame, but
		//  future ones will be 'in sync'
		//
		++bs;
	}
	else
	{
		//
		//  High-precision receiver status
		//
		//InSync = FALSE;
	}
}
inline void
periodic()
{
	static time_t winCtr = 0;

	if ( (now - winCtr) >= 5 )
	{
		stats("N/A");
		winCtr = now;
	}
}
int wordsPerSec;
void localCleanup()
{
	if ( dumpOut)
		fclose(dumpOut);
}
int
mainLoop()
{
	char dataBit;
	unsigned short passCounter = 0;
	unsigned short overflowCounter;
	register unsigned short i = 0;
	const double ticksPerBit = 1.0 / (3600.0 * 838.8e-9);
	const double ticksPerBitOver2 = (1.0 / (3600.0 * 838.8e-9)) * 0.5;
	register double exc = 0.;
	register double accumulatedClock = 0.;
	register double fastBoundaryCompare;

	wordsPerSec = 4800 / (112 + 40);
	dumpOut = fopen("dump.txt", "wt");
	if (dumpOut == NULL )
	{
		printf("cannot open dump.txt for write!");
		exit(1);
	}
	setvbuf(dumpOut, niceBuffer, _IOFBF, sizeof(niceBuffer));
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
			if (!slicerInverting)                                      // for some reason, motorola is opposite of edacs, mdt!
				dataBit = (fdata [i] & 0x8000) ? 0 : 1;
			else
				dataBit = (fdata [i] & 0x8000) ? 1 : 0;

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
		//      If user typed any keys, process them
		//
		if (kbhit())
			exit(0);
	}
}
