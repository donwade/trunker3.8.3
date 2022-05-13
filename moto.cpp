#include <missing.h>
#include <platform.h>
#include <osw.h>
#include <trunksys.h>
#include <motfreq.h>
#include <time.h>
#include <unistd.h>

#include "debug.h"
#include <cassert>
#include <pll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "scanner.h"
//#include "debug.h"

extern Scanner *myScanner;
extern unsigned long usr_time;

#define SOCK_ITEMS 2048  //Max length of buffer (was 512=BAD)
int squelchHangTime = 1;

struct osw_stru
{
    unsigned short	cmd;
    unsigned short	id;
    BOOL			grp;
    BOOL			isFreq;
};
class motoSystem {
public:
void handleBit(BOOL theData);
virtual ~motoSystem()
{
};
motoSystem()
{
}


void periodic(BOOL force = FALSE);
private:
char *getEquipName(int);
void noteOlderType();
void noteIdentifier(unsigned short possibleId, BOOL isOld);
void twoOSWcall(unsigned short flags = 0);
void show_bad_osw();
void show_good_osw(struct osw_stru *Inposw);
void proc_osw(unsigned char sl);
void pigout(int skipover);
};


class moto3600 : public motoPLL36, public motoSystem {
public:
moto3600()
{
}


virtual ~moto3600()
{
}


//protected:
virtual void handleBit(BOOL theData)
{
    motoSystem::handleBit(theData);
}


};
#define OSW_BACKGROUND_IDLE     0x02f8
#define OSW_FIRST_CODED_PC      0x0304
#define OSW_FIRST_NORMAL        0x0308
#define OSW_FIRST_TY2AS1        0x0309
#define OSW_EXTENDED_FCN        0x030b
#define OSW_AFFIL_FCN           0x030d
#define OSW_TY2_AFFILIATION     0x0310
#define OSW_TY1_STATUS_MIN      0x0310
#define OSW_TY2_MESSAGE         0x0311
#define OSW_TY1_STATUS_MAX      0x0317
#define OSW_TY1_ALERT           0x0318
#define OSW_TY1_EMERGENCY       0x0319
#define OSW_TY2_CALL_ALERT      0x0319
#define OSW_FIRST_ASTRO         0x0321
#define OSW_SYSTEM_CLOCK        0x0322
#define OSW_SCAN_MARKER         0x032b
#define OSW_EMERG_ANNC          0x032e
#define OSW_AMSS_ID_MIN         0x0360    // site "cell" number
#define OSW_AMSS_ID_MAX         0x039f
#define OSW_CW_ID               0x03a0
#define OSW_SYS_NETSTAT         0x03bf
#define OSW_SYS_STATUS          0x03c0

BOOL wantScanner = TRUE;

time_t now;
time_t start_time;
time_t exit_time;
time_t loop_time;

time_t nowFine;
int wordsPerSec;
int squelchDelay;
extern char scanTypeEnv [64];
static unsigned short passCounter = 0;
BOOL syncState = FALSE;

FreqMap *myFreqMap;
TrunkSys *mySys;

unsigned long goodcount;
unsigned long badcount;

// global variables
volatile BOOL InSync;

BOOL ob[100];    // output buffer for packet before being sent to screen

char tempArea [512];
unsigned short nsysId, sysId, cwFreq;
long theDelay;
BOOL Verbose;
BOOL inSync;
BOOL freqHopping;
BOOL apco25;
BOOL uplinksOnly;

unsigned long stackOverFlows;
unsigned long wrapArounds;

int netCounts;

static enum
{
    Identifying,
    GettingOldId,
    OperNewer,
    OperOlder
}  osw_state = Identifying;
static Filter filterCmd(4);
static Filter filterSysId(3);
static struct osw_stru stack[5];
static int numStacked;
static int numConsumed;
static int idlesWithNoId;
static char *tonenames[8] = {
    "105.88",
    "76.76",
    "83.72",
    "90",
    "97.3",
    "116.3",
    "128.57",
    "138.46"
};
char *motoSystem::getEquipName(int n)
{
    static char fmt[38];

    if (n >= 0x30 && n <= 0x4b)
        sprintf(fmt, "RIB");
    else if (n >= 0x60 && n <= 0x7b)
        sprintf(fmt, "TIB");
    else
        sprintf(fmt, "other equipment");

    return fmt;
}


void motoSystem::periodic(BOOL force)
{
    static time_t winCtr = 0;
    static char tt[2];

    register unsigned short i;

    if (force || ((now - winCtr) >= 5))
    {
        if (mySys)
        {
            mySys->flushFreqs();
            mySys->clean_stale_patches();

            tt[1] = '\0';

            for (i = 0; i < 8; ++i)
            {
                tt[0] = mySys->sysType()[i];

                if (mySys->bankActive[i] == 'y')
                {
                    QuickOut(STATUS_LINE + 1, 70 + i, color_norm, tt);
                }
                else if (mySys->bankPresent(i))
                {
                    QuickOut(STATUS_LINE + 1, 70 + i, color_norm, tt);
                    mySys->bankActive[i] = 'y';
                }
                else
                {
                    QuickOut(STATUS_LINE + 1, 70 + i, color_quiet, tt);
                }
            }
        }

        stats(scanTypeEnv);
        winCtr = now;
    }
}


void motoSystem::noteOlderType()
{
    if (mySys)
    {
        mySys->unsafe();
        delete mySys;
        mySys = 0;
    }

    osw_state = GettingOldId;
}


void motoSystem::noteIdentifier(unsigned short possibleId, BOOL isOld)
{
    idlesWithNoId = 0;                                     // got identifying info...

    if (filterSysId.filter(possibleId) == possibleId)
    {
        if (possibleId != sysId && mySys)
        {
            mySys->unsafe();
            delete mySys;
            mySys = 0;
            netCounts = 0;
        }

        if (mySys)
        {
            mySys->endScan();                                                                             // take opportunity to age assignments
        }
        else
        {
            sysId = possibleId;
            mySys = new TrunkSys(sysId, myScanner, "M");
            mySys->setBasicType("Analyzing");
            periodic(TRUE);                                                                             // force update now!
            passCounter = 0xD000;
            mySys->setBasicType("old Type I");

            if (!mySys)
                shouldNever("cannot alloc TrunkSys objects. Hit return...");

            osw_state = isOld ? OperOlder : OperNewer;
        }
    }
    else
    {
    }
}


void motoSystem::twoOSWcall(unsigned short flags)
{
/*
 *              This function is called with the following on the stack:
 *
 *              [2]     <calling radio-type 2>   <G/I>   <don't care>
 *              [1] <destination ID or GRP>  <G/I>   <frequency #>
 *
 *      it is used for: type II calls (initial)
 *                      type II radio masquerading as Type 1
 *                      Astro call
 *                      Coded PC grant
 *                      Hybrid call of type II radio in type 1 bank
 *
 *      pre-condition: mySys must be set!
 */

    unsigned short blockNum;
    char banktype;

    if (stack[1].grp)
    {
        blockNum = (unsigned short)((stack[1].id >> 13) & 7);
        banktype = mySys->types[blockNum];

        if (banktype == '?')
        {
            mySys->types[blockNum] = '2';
            mySys->note_type_2_block(blockNum);
            banktype = '2';
        }

        if (isTypeI(banktype))
            mySys->types[blockNum] = banktype = makeHybrid(banktype);

        if (isTypeII(banktype))
        {
            mySys->type2Gcall(
                stack[1].cmd,                                                                                                                                           // frequency
                (unsigned short)(stack[1].id & 0xfff0),                                                                                                                 // group ID
                stack[2].id,                                                                                                                                            // calling ID
                (unsigned short)((stack[1].id & 0x000f) | flags)                                // flags
                );
        }
        else if (isTypeI(banktype))
        {
            mySys->typeIGcall(
                stack[1].cmd,                                                                                                                                           // frequency
                banktype,
                typeIgroup(stack[1].id, banktype),                                                                                                                      // group ID
                typeIradio(stack[1].id, banktype)                                               // calling ID
                );
        }
        else if (isHybrid(banktype))
        {
            mySys->hybridGcall(
                stack[1].cmd,                                                                                                                                           // frequency
                banktype,
                typeIgroup(stack[1].id, banktype),                                                                                                                      // group ID
                stack[2].id,                                                                                                                                            // true ID
                typeIradio(stack[1].id, banktype)                                               // alias ID
                );
        }
        else
        {
            mySys->type0Gcall(
                stack[1].cmd,                                                                                                                                   // frequency
                stack[1].id,                                                                                                                                    // group ID
                stack[2].id,                                                                                                                                    // calling ID
                flags                                                                                                                                           // flags - don't get from group low bits
                );
        }
    }
    else
    {
        mySys->type0Icall(
            stack[1].cmd,                                                                                               // frequency
            stack[1].id,                                                                                                // destination
            stack[2].id,                                                                                                // talker
            FALSE, FALSE, (flags & OTHER_DIGITAL) ? TRUE : FALSE
            );
    }
}


void motoSystem::show_bad_osw()
{
    // clear out work in progress...

    numStacked = 0;
    numConsumed = 0;
}


void motoSystem::show_good_osw(register struct osw_stru *Inposw)
{
    unsigned short blockNum;
    char banktype;
    unsigned short tt1, tt2;
    static unsigned int ott1, ott2;

    ++nowFine;                                                                  // fine grain counter = #osw's = 90hz

    if (mySys && !(nowFine % 22))       // about 4x/sec + at scan marker
        mySys->endScan();

    if (Inposw->cmd == OSW_BACKGROUND_IDLE)
    {
        // idle word - normally ignore but may indicate transition into
        // un-numbered system or out of numbered system
        if (++idlesWithNoId > 20 && (osw_state == Identifying || osw_state == OperNewer))
            noteOlderType();

        // returning here because idles sometimes break up a 3-OSW sequence!

        return;
    }

    if (Inposw->cmd >= OSW_AMSS_ID_MIN && Inposw->cmd <= OSW_AMSS_ID_MAX)
    {
        // on one system, the cell id also got stuck in the middle of the 3-word sequence!

        // Site ID

        if (mySys)
        {
            if (mySys->isNetworkable())
            {
                mySys->note_site((unsigned short)(Inposw->cmd - OSW_AMSS_ID_MIN) );
            }
            else
            {
                if (++netCounts > 5)
                {
                    mySys->setNetworkable();
                    netCounts = 0;
                }
            }
        }

        idlesWithNoId = 0;

        // returning here because site IDs sometimes break up a 3-OSW sequence!

        return;
    }

    // maintain a sliding stack of 5 OSWs. If previous iteration used more than one,
    // don't utilize stack until all used ones have slid past.

    switch (numStacked)    // note: drop-thru is intentional!
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
        break;

    default:
        shouldNever("corrupt value for nstacked");
        break;
    }

    if (numStacked < 5)
        ++numStacked;

    if (numConsumed > 0)
        if (--numConsumed > 0)
            return;

    if (numStacked < 3)
        return;                                                                         // at least need a window of 3 and 5 is better.

    // look for some larger patterns first... parts of the sequences could be
    // mis-interpreted if taken out of context.

    if (stack[2].cmd == OSW_FIRST_NORMAL || stack[2].cmd == OSW_FIRST_TY2AS1)
    {
        switch (stack[1].cmd)
        {
        case OSW_EXTENDED_FCN:

            if (stack[1].id & 0xfff0 == 0x26f0)                                      // ack type 2 text msg
            {
                if (mySys && Verbose)
                    mySys->note_text_msg(stack[2].id, "Msg", stack[1].id & ((unsigned short)15));
            }
            else if (stack[1].id & 0xfff8 == 0x26E0)                                        // ack type 2 status
            {
                if (mySys && Verbose)
                    mySys->note_text_msg(stack[2].id, "Status", stack[1].id & ((unsigned short)7));
            }
            else if (stack[0].isFreq && (stack[0].id & 0xff00) == 0x1F00 &&
                     (stack[1].id & 0xfc00) == 0x2800 && stack[0].cmd == (stack[1].id & 0x3FF))
            {
                /* non-smartzone identity:  sysid[10]
                 * <sysid>              x   308
                 * <001010><ffffffffff> x   30b
                 * 1Fxx                 x   <ffffffffff>
                 */
                noteIdentifier(stack[2].id, FALSE);

                if (mySys)
                    mySys->noteDataChan(stack[0].cmd);

                numConsumed = 3;                                                                                                                                                 // we used up all 3
                return;
            }
            else if ((stack[1].id & 0xfc00) == 0x2800)
            {
                /* smartzone identity: sysid[10]
                 *
                 *    <sysid>              x   308
                 *    <001010><ffffffffff> x   30b
                 */
                noteIdentifier(stack[2].id, FALSE);

                if (mySys)
                    mySys->noteDataChan((unsigned short)(stack[1].id & 0x03ff));
            }
            else if ((stack[1].id & 0xfc00) == 0x6000)
            {
                /* smartzone peer info:                         sysid[24]
                 *              <sysid>              x   308
                 *              <011000><ffffffffff> x   30b
                 */
                // evidence of smartzone, but can't use this pair for id
                idlesWithNoId = 0;                                                                                                                                                 // got identifying info...

                if (mySys)
                    if (mySys->isNetworkable() && !mySys->isNetworked())
                        mySys->setNetworked();

            }
            else if ((stack[1].id & 0xfc00) == 0x2400)                         // various
            {
                idlesWithNoId = 0;

                if ((stack[1].id & 0x03ff) == 0x21c)
                    if (mySys && Verbose)
                        mySys->note_affiliation(stack[2].id, 0);

            }
            else if (stack[1].id == 0x2021)
            {
                if (mySys)
                    mySys->mscancel(stack[2].id);
            }
            else
            {
                idlesWithNoId = 0;

                /* unknown sequence.
                 * example: <000000> fffa x 308 then 0321 x 30b gateway close?
                 * example: <001000> 3890 G 308      2021 G 30b
                 *       <001011> 811d I 308      2c4a I 30b
                 *       <001001> 83d3 I 308      261c I 30b
                 *
                 * known:   <011000>
                 *       <001010>
                 *
                 *
                 */
            }

            numConsumed = 2;
            return;

        case 0x320:

            if (stack[0].cmd == OSW_EXTENDED_FCN)
            {
                // definitely smart-zone: 308 320 30b
                //  <sysId>                             308
                //  <cell id 6 bits><10 bits see below> 320
                //      <011000><ffffffffff>            30b
                //
                /*
                 *  10 bits:  bbb0v?AaES
                 *
                 *      bbb - band: [800,?,800,821,900,?,?]
                 *  32  v   - VOC
                 *  8   A   - Astro
                 *  4   a   - analog
                 *  2   E   - 12kbit encryption
                 *  1   S   - site is Active (if zero?)
                 */
                numConsumed = 3;
                idlesWithNoId = 0;                                                                                                                                                 // got identifying info...

                if ((stack[0].id & 0xfc00) == 0x6000)
                {
                    if (mySys && (stack[2].id == mySys->sysId))
                    {
                    	// add 1 to cell number, real people start counting from 1
                        mySys->note_neighbor((unsigned short)((stack[1].id >> 10) & 0x3F) + 1, (unsigned short)(stack[0].id & 0x3ff));

                        if (mySys->isNetworkable() && !mySys->isNetworked())
                            mySys->setNetworked();

                        // if this describes 'us', note any astro or VOC characteristics
                        // for alternate data channel, note that.

                        if (mySys->isNetworked() && ((unsigned short)((stack[1].id >> 10) & 0x3F) == mySys->siteId))
                        {
                            if (stack[0].grp)                                                                                                                                                                                      // if active data channel
                            {
                                if (stack[1].id & 8)
                                    mySys->setSubType(" Astro");

                                if (stack[1].id & 32)
                                    mySys->setOneChannel("/VOC");
                            }
                            else                                                                                                                                                                                                                                                                  // is an alternate data channel frequency
                            {
                                mySys->noteAltDchan((unsigned short)(stack[0].id & 0x3ff));
                            }
                        }
                    }
                }

                return;
            }

            break;

        case OSW_TY2_AFFILIATION:

            // type II requires double word 308/310. single word version is type 1 status,
            // whatever that means.
            if (mySys && Verbose)
                mySys->note_affiliation(stack[2].id, stack[1].id);

            break;

        case OSW_TY2_MESSAGE:
            //        sprintf(tempArea,"Radio %04x, Message %d [1..8]\n",stack[2].id,1+(stack[1].id&7));

            if (mySys && Verbose)
            {
                mySys->note_text_msg(stack[2].id,
                                     "Msg",
                                     (unsigned short)(stack[1].id & 7)
                                     );
            }

            break;

        case OSW_TY2_CALL_ALERT:

            // type II call alert. we can ignore the 'ack', 0x31A
            if (mySys && Verbose)
                mySys->note_page(stack[2].id, stack[1].id);

            break;

        case OSW_SYSTEM_CLOCK:                                                                  // system clock

            if (mySys)
            {
                tt1 = stack[2].id;
                tt2 = stack[1].id;

                if (tt2 != ott2 || tt1 != ott1)
                {
                    ott2 = tt2;
                    ott1 = tt1;
                    sprintf(tempArea, "%02u/%02u/%02u %02u:%02u",
                            (tt1 >> 5) & 0x000f,
                            (tt1) & 0x001f,
                            (tt1 >> 9),
                            (tt2 >> 8) & 0x001f,
                            (tt2) & 0x00ff);
                    QuickOut(STATUS_LINE + 1, 66, color_norm, tempArea);
                    lprintf2("clock %s ", tempArea);
                }
            }

            break;

        case OSW_EMERG_ANNC:

            if (mySys)
                mySys->noteEmergencySignal(stack[2].id);

            break;

        case OSW_AFFIL_FCN:

            if (mySys)
            {
                switch (stack[1].id & 0x000f)
                {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:

                    if (Verbose)
                    {
                        mySys->note_text_msg(stack[2].id,
                                             "Status",
                                             (unsigned short)(stack[1].id & 0x0007)
                                             );
                    }

                    break;

                case 8:
                    mySys->noteEmergencySignal(stack[2].id);
                    break;

                case 9:     // ChangeMe request
                    lprintf1("OSW_AFFIL_FCN: change me request");
                    mySys->logStringT("OSW_AFFIL_FCN: change me request", NEIGHBORS);
                    break;

                case 10:    // Ack Dynamic Group ID
                case 11:    // Ack Dynamic Multi ID
                    break;

                default:
                    break;
                }
            }

            break;

        // Type I Dynamic Regrouping ("patch")

        case 0x340:                                                                     // size code A (motorola codes)
        case 0x341:                                                                     // size code B
        case 0x342:                                                                     // size code C
        case 0x343:                                                                     // size code D
        case 0x344:                                                                     // size code E
        case 0x345:                                                                     // size code F
        case 0x346:                                                                     // size code G
        case 0x347:                                                                     // size code H
        case 0x348:                                                                     // size code I
        case 0x349:                                                                     // size code J
        case 0x34a:                                                                     // size code K

            if (mySys)
            {
                // patch notification
                // patch 'tag' is in stack[1].id
                //group associated is stack[2].id

                switch (stack[2].id & 7)
                {
                case 0:                                                                                                                                                         // normal for type 1 groups ???
                case 3:                                                                                                                                                         // patch
                case 4:                                                                                                                                                         // emergency patch

                    mySys->note_patch(/*tag*/ stack[1].id, /*group*/ stack[2].id,
                                              (char)('A' + (stack[1].cmd - 0x340)));
                    break;

                case 5:
                case 7:
                    mySys->note_patch(/*tag*/ stack[1].id, /*group*/ stack[2].id,
                                              (char)('A' + (stack[1].cmd - 0x340)), TRUE);
                    break;

                default:
                    break;
                }

                nowFine -= 2;                                                                                                                                                 // time machine - these get in the way of notseen calc
            }

            break;

        case 0x34c:                                                                 // size code M trunker = L

            if (mySys)
            {
                // patch notification
                // patch 'tag' is in stack[1].id
                //group associated is stack[2].id
                switch (stack[2].id & 7)
                {
                case 0:                                                                                                                                                         // normal for type 1 groups ???
                case 3:                                                                                                                                                         // patch
                case 4:                                                                                                                                                         // emergency patch


                    mySys->note_patch(/*tag*/ stack[1].id, /*group*/ stack[2].id, 'L');
                    break;

                case 5:
                case 7:
                    mySys->note_patch(/*tag*/ stack[1].id, /*group*/ stack[2].id,
                                              'L', TRUE);
                    break;

                default:
                    break;
                }

                nowFine -= 2;                                                                                                                                                 // time machine - these get in the way of notseen calc
            }

            break;

        case 0x34e:                                                                 // size code O Trunker = M

            if (mySys)
            {
                // patch notification
                // patch 'tag' is in stack[1].id
                //group associated is stack[2].id
                switch (stack[2].id & 7)
                {
                case 0:                                                                                                                                                         // normal for type 1 groups ???
                case 3:                                                                                                                                                         // patch
                case 4:                                                                                                                                                         // emergency patch


                    mySys->note_patch(/*tag*/ stack[1].id, /*group*/ stack[2].id, 'M');
                    break;

                case 5:
                case 7:
                    mySys->note_patch(/*tag*/ stack[1].id, /*group*/ stack[2].id,
                                              'M', TRUE);
                    break;

                default:
                    break;
                }

                nowFine -= 2;                                                                                                                                                 // time machine - these get in the way of notseen calc
            }

            break;

        case 0x350:                                                                 // size code Q Trunker = N

            if (mySys)
            {
                // patch notification
                // patch 'tag' is in stack[1].id
                //group associated is stack[2].id
                switch (stack[2].id & 7)
                {
                case 0:                                                                                                                                                         // normal for type 1 groups ???
                case 3:                                                                                                                                                         // patch
                case 4:                                                                                                                                                         // emergency patch


                    mySys->note_patch(/*tag*/ stack[1].id, /*group*/ stack[2].id, 'N');
                    break;

                case 5:
                case 7:
                    mySys->note_patch(/*tag*/ stack[1].id, /*group*/ stack[2].id,
                                              'N', TRUE);
                    break;

                default:
                    break;
                }

                nowFine -= 2;                                                                                                                                                 // time machine - these get in the way of notseen calc
            }

            break;

        default:

            if (mySys)
            {
                if (stack[1].isFreq)
                {
                    if (stack[2].cmd == OSW_FIRST_TY2AS1 && stack[1].grp)
                    {
                        blockNum = (unsigned short)((stack[1].id >> 13) & 7);
                        banktype = mySys->types[blockNum];

                        if (banktype == '2')
                        {
                            mySys->types[blockNum] = '?';
                            settextposition(FBROW, 1);
                            sprintf(tempArea, "Block %d <0..7> is Type IIi                         ", blockNum);
                            _outtext(tempArea);
                        }

                        mySys->setSubType("i");
                    }

                    if (stack[2].id != 0)                                                                                           // zero id used special way in hybrid system
                        twoOSWcall();
                }
				else
				{
					char msg[100];
					sprintf(msg, "*** unhandled message OSW = 0x%X", stack[1].cmd);
                    lprintf2("%s", msg);
					mySys->logString(msg, _SCREEN);
				}
            }
            else
            {
                // otherwise 'busy', others...
                lprintf2("not handled OSW=0x%X ", stack[1].cmd);
            }
        }

        numConsumed = 2;
        return;
    }

    // uhf/vhf equivalent of 308/320/30b

    if (stack[1].cmd == 0x0320 && stack[0].cmd == OSW_EXTENDED_FCN && stack[2].isFreq)
    {
        numConsumed = 3;
        idlesWithNoId = 0;                                                                         // got identifying info...

        if ((stack[0].id & 0xfc00) == 0x6000)
        {
            if (mySys && (stack[2].id == mySys->sysId))
            {
            	// increase cell number by 1, real people start counting from ONE
                if (myFreqMap->isFrequency((unsigned short)(stack[0].id & 0x3ff) + 1, mySys->getBandPlan()))
                    mySys->note_neighbor((unsigned short)((stack[1].id >> 10) & 0x3F), (unsigned short)(stack[0].id & 0x3ff));

                if (mySys->isNetworkable() && !mySys->isNetworked())
                    mySys->setNetworked();

                // if this describes 'us', note any astro or VOC characteristics
                // if this describes an alternate data frequency, note that.

                if (mySys->isNetworked() && ((unsigned short)((stack[1].id >> 10) & 0x3F) == mySys->siteId))
                {
                    if (stack[0].grp)                                                                                                              // if active data channel
                    {
                        if (stack[1].id & 8)
                            mySys->setSubType(" Astro");

                        if (stack[1].id & 32)
                            mySys->setOneChannel("/VOC");
                    }
                    else                                                                                                                                                                                          // is an alternate data channel frequency
                    {
                        mySys->noteAltDchan((unsigned short)(stack[0].id & 0x3ff));
                    }
                }
            }
        }

        return;
    }

    /* astro call or 'coded pc grant' */

    if ((stack[2].cmd == OSW_FIRST_ASTRO || stack[2].cmd == OSW_FIRST_CODED_PC) && stack[1].isFreq)
    {
        if (mySys)
        {
            if (stack[2].id)
            {
                if (stack[2].cmd == OSW_FIRST_ASTRO)
                    mySys->setSubType(" Astro");

                twoOSWcall(OTHER_DIGITAL);
            }
        }

        numConsumed = 2;
        return;
    }

    // have handled all known dual-osw sequences above. these tend to be single or correlated

    switch (stack[2].cmd)
    {
    case OSW_SYS_NETSTAT:

        if (mySys && !mySys->isNetworkable())
        {
            if (++netCounts > 5)
            {
                mySys->setNetworkable();
                netCounts = 0;
            }
        }

        idlesWithNoId = 0;
        break;

    case OSW_SYS_STATUS:                                     // system status

        if (mySys)
        {
            register unsigned short statnum;

            statnum = (unsigned short)((stack[2].id >> 13) & 7);                                                                             // high order 3 bits opcode

            if (statnum == 1)
            {
                if (stack[2].id & ((unsigned short)0x1000))
                    mySys->setBasicType("Type II");
                else
                    mySys->setBasicType("Type I");

                statnum = (unsigned short)(stack[2].id >> 5);
                statnum &= 7;
                mySys->setConTone(tonenames[statnum]);
                //lprintf2("xconnect tone %s\n", tonenames[statnum]);
            }
        }

        break;

    case OSW_SCAN_MARKER:
        noteIdentifier(stack[2].id, FALSE);

        if (osw_state == OperNewer)
            if (mySys)
                mySys->setBasicType("Type II");

        break;

    case OSW_TY1_EMERGENCY:                                             // type 1 emergency signal

        if (mySys)
        {
            if (numStacked > 3) // need to be unambiguous with type 2 call alert
            {
                blockNum = (unsigned short)((stack[2].id >> 13) & 7);
                banktype = mySys->types[blockNum];

                if (isTypeI(banktype) || isHybrid(banktype))
                    mySys->noteEmergencySignal(typeIradio(stack[2].id, banktype));
                else
                    mySys->noteEmergencySignal(stack[2].id);
            }
        }

        break;

    case OSW_TY1_ALERT:

        if (mySys)
        {
            blockNum = (unsigned short)((stack[2].id >> 13) & 7);
            banktype = mySys->types[blockNum];

            if (isTypeI(banktype) || isHybrid(banktype))
                mySys->note_page(typeIradio(stack[2].id, banktype));
            else
                mySys->note_page(stack[2].id);
        }

        break;

    case OSW_CW_ID:                                     // this seems to catch the appropriate diagnostic. thanks, wayne!

        if (mySys)
        {
            if ((stack[2].id & 0xe000) == 0xe000)
            {
                cwFreq = (unsigned short)(stack[2].id & 0x3ff);
                mySys->noteCwId(cwFreq, stack[2].id & 0x1000);
            }
            else
            {
                static unsigned int lastDiag = 0;
                static time_t lastT = 0;

                if ((stack[2].id != lastDiag) || ((now - lastT) > 120))
                {
                    switch (stack[2].id & 0x0F00)
                    {
                    case 0x0A00:
                        sprintf(tempArea, "Diag %04x: %s Enabled", stack[2].id, getEquipName(stack[2].id & 0x00ff));
                        break;

                    case 0x0B00:
                        sprintf(tempArea, "Diag %04x: %s Disabled", stack[2].id, getEquipName(stack[2].id & 0x00ff));
                        break;

                    case 0x0C00:
                        sprintf(tempArea, "Diag %04x: %s Malfunction", stack[2].id, getEquipName(stack[2].id & 0x00ff));
                        break;

                    default:
                        sprintf(tempArea, "Diagnostic code (other): %04x", stack[2].id);
                    }

                    mySys->logStringT(tempArea, DIAGNOST);
                    lastT = now;
                    lastDiag = stack[2].id;
                }
            }
        }

        break;

    default:

        if (mySys)
        {
            if (stack[2].cmd >= OSW_TY1_STATUS_MIN &&
                stack[2].cmd <= OSW_TY1_STATUS_MAX &&
                numStacked > 4)
            {
                // place to put type 1 status messages XXX

                if (mySys && Verbose)
                {
                    mySys->note_text_msg(stack[2].id,
                                         "Status",
                                         (unsigned short)(stack[2].cmd - OSW_TY1_STATUS_MIN)
                                         );
                }

                break;
            }

            // this is the most frequent case that actually touches things....

            if (stack[2].isFreq && (numStacked > 4))
            {
                if (mySys->vhf_uhf &&                                                                                                                                                                   // we have such a system
                    stack[1].isFreq &&                                                                                                                                                                  // this is a pair of frequencies
                    (mySys->mapInpFreq(stack[2].cmd) == stack[1].cmd)                                                           // the first frequency is input to second
                    )
                {
                    // special processing for vhf/uhf systems. They use the following
                    // pairs during call setup:
                    /*
                     *      <originating radio id>           G/I <input frequency>
                     *      <destination group or radio> G/I <output frequency>
                     */
                    if (stack[1].grp)
                    {
                        blockNum = (unsigned short)((stack[1].id >> 13) & 7);
                        banktype = mySys->types[blockNum];

                        // on uhf/vhf systems, only allowed to be type II!
                        if (banktype != '0' && banktype != '2')
                        {
                            mySys->types[blockNum] = '2';
                            mySys->note_type_2_block(blockNum);
                        }
                    }

                    if (stack[1].grp && !stack[2].grp)                                                                                                            // supposedly astro indication
                        twoOSWcall(OTHER_DIGITAL);
                    else
                        twoOSWcall();

                    numConsumed = 2;
                    return;                                                                                                                                                                                 // the return below pops only one word, we used 2 here
                }
                else
                {
                    // bare assignment
                    // this is still a little iffy...one is not allowed to use IDs that
                    // mimic the system id in an old system, so seems to work ok...
                    if (((stack[2].id & 0xff00) == 0x1f00) && (osw_state == OperOlder))
                    {
                        noteIdentifier(stack[2].id, TRUE);

                        if (mySys)
                            mySys->noteDataChan(stack[2].cmd);
                    }
                    else
                    {
                        //              in the case of late entry message, just tickle current one
                        //              if possible.... preserve flags since may be lost in single
                        //              word case

                        if (stack[2].grp)
                        {
                            blockNum = (unsigned short)((stack[2].id >> 13) & 7);
                            banktype = mySys->types[blockNum];

                            if (isTypeII(banktype))
                            {
                                mySys->type2Gcall(stack[2].cmd,                                                                                                                                                                                                                                                                                                         // frequency
                                                  (unsigned short)(stack[2].id & 0xfff0),                                                                                                                                                                                                                                                                               // group ID
                                                  (unsigned short)(stack[2].id & 0x000f)                                                                                                                                                                                                        // flags
                                                  );
                            }
                            else if (isTypeI(banktype))
                            {
                                mySys->typeIGcall(
                                    stack[2].cmd,                                                                                                                                                                                                                                                                                                                                       // frequency
                                    banktype,
                                    typeIgroup(stack[2].id, banktype),                                                                                                                                                                                                                                                                                                                  // group ID
                                    typeIradio(stack[2].id, banktype)                                                                                                                                                                                                                                           // calling ID
                                    );
                            }
                            else if (isHybrid(banktype))
                            {
                                mySys->hybridGcall(
                                    stack[2].cmd,                                                                                                                                                                                                                                                                                                                                       // frequency
                                    banktype,
                                    typeIgroup(stack[2].id, banktype),                                                                                                                                                                                                                                                                                                                  // group ID
                                    typeIradio(stack[2].id, banktype)                                                                                                                                                                                                                                           // calling ID
                                    );
                            }
                            else
                            {
                                mySys->type0Gcall(
                                    stack[2].cmd,                                                                                                                                                                                                                                                                                                                       // frequency
                                    stack[2].id,                                                                                                                                                                                                                                                                                                                        // group ID
                                    0                                                                                                                                                                                                                                                                                                                                   // no flags
                                    );
                            }
                        }
                        else
                        {
                            mySys->type0Icall(
                                stack[2].cmd,                                                                                                                                                                                                                                                                           // freq#
                                stack[2].id,
                                FALSE, FALSE, FALSE);                                                                                                                                                                                                                                   // talker
                        }
                    }
                }
            }
        }
        else if (osw_state == GettingOldId)
        {
            if (stack[2].isFreq && ((stack[2].id & 0xff00) == 0x1f00))
            {
                noteIdentifier(stack[2].id, TRUE);
                noteIdentifier(stack[2].id, TRUE);
            }
        }

        break;
    }

    numConsumed = 1;
    return;
}


// process de-interleaved incoming data stream
// inputs: -1 resets this routine in prepartion for a new OSW
//                      0 zero bit
//              1 one bit
// once an OSW is received it will be placed into the buffer bosw[]
// bosw[0] is the most recent entry
//
static int ct;
void motoSystem::proc_osw(unsigned char sl)
{
    register int l;
    int sr, sax, f1, f2, iid, cmd, neb;
    static char osw [50], gob [100];
    static struct osw_stru bosw;

    gob [ct] = sl;
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
            osw [l >> 1] = gob [l];                                                                             // osw gets the DATA bits

            if (gob [l])                                        // gob becomes the syndrome
            {
                gob [l] ^= 0x01;
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
                gob [l + 1] ^= 0x01;
                gob [l + 3] ^= 0x01;
            }
        }

        //
        //  Run through error detection routine
        //
        for (l = 0; l < 27; l++)
        {
            if (sr & 0x01)
                sr = (sr >> 1) ^ 0x0225;
            else
                sr = sr >> 1;

            if (osw [l])
                sax = sax ^ sr;
        }

        for (l = 0; l < 10; l++)
        {
            f1 = (osw [36 - l]) ? 0 : 1;
            f2 = sax & 0x01;

            sax = sax >> 1;

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
                iid = iid << 1;

                if (!osw [l])
                    iid++;
            }

            bosw.id = (unsigned short)(iid ^ 0x33c7);
            bosw.grp = (osw [16] ^ 0x01) ? TRUE : FALSE;

            for (cmd = 0, l = 17; l < 27; l++)
            {
                cmd = cmd << 1;

                if (!osw [l])
                    cmd++;
            }

            bosw.cmd = (unsigned short)(cmd ^ 0x032a);
            ++goodcount;

            if (mySys)
                bosw.isFreq = myFreqMap->isFrequency(bosw.cmd, mySys->getBandPlan());
            else
                bosw.isFreq = myFreqMap->isFrequency(bosw.cmd, '-');

            show_good_osw(&bosw);
        }
        else
        {
            ++badcount;
            show_bad_osw();
        }
    }
}


//
// de-interleave OSW stored in array ob[] and process
//
void motoSystem::pigout(int skipover)
{
    register int i1, i2;

    ct = 0;                                     // avoid inlining just to set one variable!

    for (i1 = 0; i1 < 19; ++i1)
        for (i2 = 0; i2 < 4; ++i2)
            proc_osw((unsigned char)ob [((i2 * 19) + i1) + skipover]);

}


//
//       this routine looks for the frame sync and splits off the 76 bit
// data frame and sends it on for further processing
//
void motoSystem::handleBit(BOOL gin)
{
    static int sr = 0;
    static int fs = 0xac;
    static int bs = 0;

    //
    // keep up 8 bit sliding register for sync checking purposes
    //
    sr = (sr << 1) & 0xff;

    if (gin)
        sr = sr | 0x01;

    // original ob [bs - 1] = gin;
    if (bs)
        ob [bs - 1] = gin;                                                                          //new dwade

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
        pigout(bs - 84);
        bs = 0;
        InSync = TRUE;
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
        InSync = FALSE;
    }
}


void localCleanup()
{
    delete mySys;
    mySys = 0;
}


#include <stdio.h>

//float gqrx = (1.0/ 48000.0);	//seconds per sample.
//float tickwidth = (4.0 / 4.77e6); //seconds per one tick (ibm xtal tick)
//float ticksPerSample = ( (1.0 /tickwidth) /(1.0/gqrx));

#define GQRX_AUDIO_PORT 7355
#define IBM_CLOCK 1.19318e6             // system ticks per second
#define GQRX_AUDIO_SAMPLE_RATE 48000.0  // gqrx samples per second

const float ticksPerSample = (IBM_CLOCK / GQRX_AUDIO_SAMPLE_RATE);

/*
   Params:
      fd       -  (int) socket file descriptor
      buffer - (char*) buffer to hold data
      len     - (int) maximum number of bytes to recv()
      flags   - (int) flags (as the fourth param to recv() )
      to       - (int) timeout in milliseconds
   Results:
      int      - The same as recv, but -2 == TIMEOUT
   Notes:
      You can only use it on file descriptors that are sockets!
      'to' must be different to 0
      'buffer' must not be NULL and must point to enough memory to hold at least 'len' bytes
      I WILL mix the C and C++ commenting styles...
*/

int recv_fromV2(int fd, char *buffer, int len, int flags, int to) {

   fd_set readset;
   int result, iof = -1;
   struct timeval tv;

   // Initialize the set
   FD_ZERO(&readset);
   FD_SET(fd, &readset);  //add in callers fd

   
   // Initialize time out struct
   tv.tv_sec = 0;
   tv.tv_usec = to * 1000;
   // select()
   result = select(fd+1, &readset, NULL, NULL, &tv);

   // Check status
   if (result < 0)
   {
      return -1;  //timeout?
   }
   else if (result > 0 && FD_ISSET(fd, &readset)) 
   {
      // Set non-blocking mode
      if ((iof = fcntl(fd, F_GETFL, 0)) != -1)
         fcntl(fd, F_SETFL, iof | O_NONBLOCK);
      
      result = recv(fd, buffer, len, flags);

      // set as before
      if (iof != -1) fcntl(fd, F_SETFL, iof);
      return result;
   }
   return -2;
}



int mainLoop()
{
    BOOL dataBit;
    unsigned int overflowCounter;
    register unsigned int i = 0;

    wordsPerSec = 3600 / 84;
    squelchDelay = (3600 * 7) / (84 * 8); // extend from 1/2 second to 7/8 second
    InSync = FALSE;
    theDelay = DELAY_ON;
    Verbose = TRUE;
    inSync = FALSE;
    freqHopping = FALSE;  //temporary till later
    apco25 = TRUE;
    uplinksOnly = FALSE;
    
    register moto3600 *mptr;
    mptr = new moto3600;

    if (!mptr)
        shouldNever("alloc of moto3600 object");

    //      Panic if we can't create objects
    //
    if (!myScanner)
        shouldNever("Could not create scanner object");

#if 1

    if (actualRows < MAX_ROWS)
        shouldNever("terminal needs 38 vert lines");

    //      Core loop that makes this thing work
    //
#endif

    showPrompt();

    //
    //      Create the TrunkSys object, default to system ID of 6969 until we
    //      figure out what we're really listening to.
    //
    mySys = new TrunkSys(0x6969, myScanner, "M");
    myFreqMap = new MotorolaMap;
    cpstn = 0;
    cpstn = 0;

#if 0
    FILE *fd = fopen("/home/dwade/moto3600.txt", "rb");
#else

    struct sockaddr_in rxLocalAddr, remoteAddr;
    struct sockaddr_in txLocalAddr;

    int rxSock;
    int txSock;
	int rv;
    int recv_len;
    int ctr;
    static short int hival = (short int)(0x8000);
    static short int loval = (short int)(0xFFFF);
    static long sumOfSamples = 0;
    static long totalNumSamples = 0;
    static long running_average = 0;
    static int slow_average = 0xDEADBEEF ;

    int remoteAdrLen;

    signed short rxSockData[SOCK_ITEMS];

    //create a rx UDP audio socket
    if ((rxSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        exit("gqrx audio rx socked failed");

    //create a UDP audio forward socket
    if ((txSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        exit("dsd/ibme forwarder sockect fail");

    // zero out the structure
    memset((char *)&rxLocalAddr, 0, sizeof(rxLocalAddr));

    rxLocalAddr.sin_family = AF_INET;
    rxLocalAddr.sin_port = htons(GQRX_AUDIO_PORT);
    rxLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // zero out the structure
    memset((char *)&txLocalAddr, 0, sizeof(txLocalAddr));

    txLocalAddr.sin_family = AF_INET;
    txLocalAddr.sin_port = htons(GQRX_AUDIO_PORT+1);
    txLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);


    //bind socket to port
    if (bind(rxSock, (struct sockaddr *)&rxLocalAddr, sizeof(rxLocalAddr)) == -1)
        exit("gqrx - enable audio udp streaming !!!");

    //beeper(1000,100,100);
    
#endif

	time(&start_time);
	
	exit_time = start_time + usr_time * 60;

    for (;;)
    {
        
        time(&now);  // per-event time 
		
        if (usr_time && (now > exit_time))
        {
			if (mySys) mySys->doSave();
			lprintf1("exit by timer counting down to zero");
        	printf("\n\n\n\exit by timer counting down to zero\n\n\n");
        	break;
        }

		static time_t last_time = 0;
		loop_time = now - last_time;
        if (loop_time)
        {
        	char cTime[50];
        	last_time = now;
        	sprintf(cTime, "%07d", now - exit_time);
			QuickOut(STATUS_LINE + 1, 60, color_quiet, cTime);
		}
		
#if 0
	// legacy msdos from 1990 (prince still to make party like its 1999)
    int r;
    char cbit;
    time(&now);                                                                             // per-event time

	for (overflowCounter = BufLen; i != cpstn; --overflowCounter)
	{
		//
		//	We never caught up to the input side during loop, drop around
		//	and note that we are in trouble.
		//
		if (overflowCounter == 0)
		{
			++wrapArounds;
			break;
		}

		//
		//	dataBit becomes the data bit, being either 0 or 1.
		//

		if (!slicerInverting)																											   // for some reason, motorola is opposite of edacs, mdt!
			dataBit = (fdata [i] & 0x8000) ? FALSE : TRUE;

		els
			dataBit = (fdata [i] & 0x8000) ? TRUE : FALSE;

		mptr->inputData(dataBit, (unsigned short)(fdata[i] & 0x7fff));

		if (++i > BufLen)
			i = 0;
	}

#else

        //keep listening for data

        //try to receive some data, this is a blocking call
        remoteAdrLen = sizeof(remoteAddr);

        // we will get a bunch of u_int16's
        recv_len = recv_fromV2(rxSock, (char*) rxSockData, sizeof(rxSockData), 0, 3000);
        if (recv_len < 0)
        {
            lprintf1("gqrx no UDP streaming data seen");
            sleep(3);
            exit("no gqrx udp audio. Enable in grqx please");
        }

        // forward off to ibme deocoder DSD
        rv = sendto(txSock, rxSockData, recv_len, 0, (sockaddr*) &txLocalAddr, sizeof(txLocalAddr));


        //print details of the client/peer and the data received
        //lprintf3("Received packet from %s:%d\n", inet_ntoa(remoteAddr.sin_addr), ntohs(remoteAddr.sin_port));
        //lprintf2("Data size: %d\n" , recv_len);


        for (i = 0; i < recv_len / 2; i++)
        {
            if (rxSockData[i] > hival)
                hival = rxSockData[i];

            if (rxSockData[i] < loval)
                loval = rxSockData[i];

            sumOfSamples += rxSockData[i];
            rxSockData[i] = (rxSockData[i] > slow_average) ? 1 : 0;
        }

        totalNumSamples += recv_len / 2;

        static bool lastBitState;
        static float bitTime;

            for (i = 0; i < recv_len / 2; i++)
            {
                if (lastBitState != rxSockData[i])
                {
                    mptr->inputData(lastBitState, bitTime);
                    bitTime = 0;
                    lastBitState = rxSockData[i];
                }

                ;
                bitTime += ticksPerSample;
            }

        if ( totalNumSamples > 250000)
        {
            running_average = sumOfSamples / totalNumSamples;
            if (slow_average == 0xDEADBEEF) slow_average = running_average;
            
            slow_average += (running_average > slow_average) ? +1 : -1;

             lprintf6("lo= %5d  avg=%6d hi=%5d diff=%d slow=%d",
                           loval,
                           running_average,
                           hival,
                           hival - loval,
                           slow_average);

            sumOfSamples = 0;
            totalNumSamples = 0;
            hival = (short int)(0x8000);
            loval = (short int)(0xFFFF);
        }

#endif

        //
        //      Every 32768 times?
        //
        if (squelchHangTime)
        {
            squelchHangTime--;

            float strength;
            strength = myScanner->getSNR();
            
            if (strength < myScanner->squelch_level)
            {
                if (squelchHangTime == 0)
                {
                    lprintf3("back2ctl chanscan %f < %f", strength, myScanner->squelch_level);
                    myScanner->setBandWidth(BW_P25CNTL); //back to ctl channel
                    myScanner->setFrequency(control_channel,__FILE__, __LINE__);
                }

                if (squelchHangTime < 2)
                {
                    lprintf4("time-hold scan %f < %f @ %d", strength, myScanner->squelch_level, squelchHangTime);
                }    
            }
            else
            {
//              lprintf3("stop/hold scan %f > %f", strength, myScanner->squelch_level);
                squelchHangTime = SQUELCH_HANG;
            }
        }

        if ((passCounter++ << 1) == 0)
        {
            mptr->periodic();
            netCounts = 0;
        }

        //
        //      Every eighth time
        //
#if 0        
        if (!(passCounter & 0x00FF))
        {
			int index = (passCounter >> 8 & 0x3);
			static char ani[] = "|/-\\"; 
            gotoxy(52, STATUS_LINE);
            settextcolor(color_norm);
			char spin[2] = "x\0";
			spin[0] = ani[index];
			
            _outtext(spin);
        }
#endif

        static rccoord wasat = { 0 };
        static int first = 1;

        if (first)
            first = 0;
        else
            settextposition(wasat.row, wasat.col);

        //
        //      If user typed any keys, process them
        //
        if (kbhit())
        {
            doKeyEvt();
            wasat = _gettextposition();
        }
    }

    //fclose (fd);
}
