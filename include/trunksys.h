#include <infoact.h>

#define OTHER_DIGITAL 16
#define PHONE_PATCH 32
#define ENCRYPTED 8

extern InfoControl userChoices;
extern FreqMap *myFreqMap;
extern unsigned char scanThreshold;
unsigned vhfHiFreqMapper(unsigned short theCode);
unsigned uhf400FreqMapper(unsigned short theCode);


struct msgHolder
{
    unsigned short		mnumber;
    char				mtype;
    char *				mtext;
    struct msgHolder *	next;
};
class msgList {
private:
struct msgHolder *msgs;
int theCount;
public:
msgList(const char *fn);
const char *getMsgText(char mtype, unsigned short mnumber);
int getCount()
{
    return theCount;
}


virtual ~msgList();
};


/*
 * --------------------------------------------------------------------------------------------
 *      Class freqList - used to maintain an array of frequencies and other information associated
 *                                       with a trunked system or cell thereof. This class also reads and writes
 *                                       the xxxxSYS.TXT and xxxxRnn.TXT files at the request of its client.
 *
 *                                       special arguments allow a truncated version of xxxxSYS.TXT to be created
 *                                       in association with the currently active cell of a multi-cell network.
 * --------------------------------------------------------------------------------------------
 */

class FreqList {
private:
char *sysName;                                  // text from first line of xxxxsys.txt file - descriptive
channelDescriptor *aid;                       // points to array of assignment structures
int *savenaid;                                  // points to place where creator wants to know how many in use
char myBandPlan;                                // 8=800 moto, 9=900 moto, S=800 moto splinter, anything else
                                                // starts in hex and must be manually set

public:                                         // public interface

FreqList(const char *	fn,                     // name of file to read
         int&			naid,                   // place to set number of frequencies currently populated
         char *			putMap = 0);    // place to put map designation

void doSave(const char *tfn,                    // file name to create/overwrite
            const char *theMap,                 // map of bank sizes
            BOOL		odd4,                   // true if uhf/vhf
            BOOL		saveFreqs = TRUE,       // set false to make dummy system file for multi-cell
            const char *altName = 0);   // over-ride system name for dummy system file

virtual ~FreqList();
// Access Functions - set and get private data
channelDescriptor *getList() const
{
    return aid;
}


const char *getSysName() const
{
    return sysName;
}


char getBandPlan() const;
void setBandPlan(char p);
};
/*
 * --------------------------------------------------------------------------------------------
 *      Class TrunkSys - this class is the primary class. it maintains most of the screen areas
 *                       and is intended to provide sufficient functionality and generality to
 *                       be used for more than one type of trunked system.
 *
 *                       the client of this class would be responsible for mapping any data channel
 *                       activity into method calls that this class provides.
 *
 *                       at this point, the functionality for a e/ge type 'special call' is in this
 *                       class but has not been tested.
 * --------------------------------------------------------------------------------------------
 */
class TrunkSys {
public:  // public attributes and constants



enum SysState                                   // public states for general operational modes:
{ Searching,                                    //   choose next call by priority
  HoldGrp,                                      //   only following a specified group ID from keys or manual spec
  HoldPatch,                                    //   only following a specified patch ID from keys or manual spec
  Hold1Indiv,                                   //   holding on an individual radio ID - from up/down/hold keys only
  Hold2Indiv                                    //   holding on a pair of radio IDs - from up/down/hold keys only
};

#define BEEPGUARD     5                         // how long to wait before allowing another beep
#define BEEPDURATION (wordsPerSec / 6)  // duration of the beep itself

time_t lastBeep;                                // to avoid many beeps in a row
time_t beepTimer;                               // to measure the beep itself
unsigned beepDuration;
unsigned short sysId;                           // hex system ID (moto unique, e/ge non-unique)
BOOL antiScannerTones;                          // set TRUE for fast clipping of audio
char types[9];  // types by block
char bankActive[9];     // bank active by block
short siteId;                                   // repeater #
BOOL vhf_uhf;                                   // set if have to match input with output frequencies
BOOL patchesActive;                             // if patches being broadcast now

private:                                        // private attributes

char *sysName;                                  // text associated with the whole system
char *siteName;                                 // text associated with the repeater / Site
GroupDesigList *groupList;                      // list of group IDs and attributes (title, color, etc)
DesigList *idList;                              // list of radio IDs and attributes (title, color, etc)
PatchDesigList *patchList;                      // list of Patches by association code (ditto)
FreqList *fList;                                // collection of frequencies for the system
channelDescriptor *listOfLchans;                       // array of frequency assignments
Scanner *myScanner;                             // points to external object for scanner control
FILE *pageLog;                                  // paging history file
unsigned short newFreqs[0x400];                 // used to count new frequencies before accepting them
unsigned short neighbors[65];                   // for current frequency of neighbors
char neighborsFreq[65][100];                    // for current frequency of neighbors
Filter fSite;                                   // after 'n', assume stable
int numLchans;                                  // actual number of utilized frequencies, set by FreqList
BOOL doAuto;                                    // used to disable save in the 'escape' case
BOOL networkable;                               // gross system characteristic
BOOL networked;                                 // indicates xxxxRNN.txt file in use
char bandPlan;                                  // band plan - '0', '8', '9', 'S'
char sysPrefix[50];                     // to store first 4 characters of file names
char sysFn[60];                         // xxxxSys.data or xxxxRn*.data or xxxxCn*.data
char oldFn[60];                         // xxxxSys.data
char idsFn[60];                         // xxxxIds.data
char grpFn[60];                         // xxxxGrp.data
char pageFn[60];                        // xxxxPage.data
char patchFn[60];                       // xxxxPtch.data
char msgFn[60];                         // xxxxMsg.data
char neighFn[60];                       // xxxxNeigh.data
char tmp[120];                          // scratch pad
msgList *msgPtr;
enum            SysState state;                 // state of trunking system object
designation *holdDes1,                          // one of two possible 'hold' designations
            *holdDes2;                          // second designation or null
designation *searchDes1,                        // currently active designation(s)
            *searchDes2;
unsigned char searchPrty;                       // prty of current search call
time_t strtime;                                 // system time for time/date utility
char timeArray[50];     // place to format time/date
char *basicType;
char *subType;
char *oneChType;
char *conTone;
scrlog *myLogWin;

public:  // public methods
// ctor/dtor/init
void refreshLog()
{
    myLogWin->redraw();
}


TrunkSys(unsigned short nid, Scanner *scn, const char *variety = "M");    // numbered system
virtual ~TrunkSys();
// access methods to get/set parameters
void setBasicType(char *cp)
{
    basicType = cp;
}


void setSubType(char *cp)
{
    subType = cp;
}


void setConTone(char *cp)
{
    conTone = cp;
}


void setOneChannel(char *cp)
{
    oneChType = cp;
}

bool isMobileFreq(unsigned long freq)
{
	return freq < 140000000 ? 1 : 0;
}

const char *timeString();  // time / date string
enum SysState getState() const
{
    return this->state;
}


const char *describe();
void setBandPlan(char p)
{
    bandPlan = p;     fList->setBandPlan(p);
}


char getBandPlan()
{
    return fList->getBandPlan();
}


const char *sysType()
{
    return (const char *)&types[0];
}


BOOL isNetworkable() const
{
    return networkable;
}


BOOL isNetworked() const
{
    return networked;
}


void setNetworked()
{
    networked = TRUE;
}


void setNetworkable()
{
    networkable = TRUE;
}


const char *getSysName()  const
{
    return sysName;
}


const char *getSiteName() const
{
    return siteName ? siteName : sysName;
}


BOOL bankPresent(unsigned short i) const
{
    return groupList->bankPresent(i);
}


// functional interface to do actual trunking:

void type0Icall(unsigned short freq, unsigned short dest, unsigned short talker, BOOL digital, BOOL fone, BOOL encrypted);
void type0Icall(unsigned short freq, unsigned short talker, BOOL digital, BOOL fone, BOOL encrypted);
void type0Gcall(unsigned short freq, unsigned short dest, unsigned short talker, unsigned short flags);
void type0Gcall(unsigned short freq, unsigned short dest, unsigned short flags);
void type2Gcall(unsigned short freq, unsigned short dest, unsigned short talker, unsigned short flags);
void type2Gcall(unsigned short freq, unsigned short dest, unsigned short flags);
void hybridGcall(unsigned short freq, char bankType, unsigned short dest, unsigned short talker, unsigned short talkAlias);
void hybridGcall(unsigned short freq, char bankType, unsigned short dest, unsigned short talker);
void typeIGcall(unsigned short freq, char bankType, unsigned short dest, unsigned short talker);
void noteCwId(unsigned short cwFreq, BOOL onoff);
void noteDataChan(unsigned short dataFreq);
void noteAltDchan(unsigned short dataFreq);
void noneActive(); // optimization if know no calls present

// Other trunking operations besides calls:

void note_page(unsigned short fromid, unsigned short toid);     // id 'fromid' paged 'toid'
void note_page(unsigned short toid);    // type 1 page to 'toid'
void note_neighbor(unsigned short itsId, unsigned short itsFreq);       // defunct feature
void note_type_2_block(unsigned short blknum);
void noteEmergencySignal(unsigned short fromid);// id with emergency button usually type 1
void note_patch(unsigned short patchTag, unsigned short grpID, char bankType, BOOL msel = FALSE);
void mscancel(unsigned short grpId);
void note_affiliation(unsigned short radioId, unsigned short groupId);
void note_site(unsigned short newSite);
void note_text_msg(unsigned short radioId, const char *typ, unsigned short val);
void endScan()  // be sure to call this at least 6x/sec
{
    if (lastBeep)       // test for guard interval expiration
        if ((now - lastBeep) > BEEPGUARD)
            lastBeep = 0;

    if (beepTimer)// test for the length of the beep itself
    {
        if ((nowFine - beepTimer) > beepDuration)
        {
            nosound();
            beepTimer = 0;
        }
    }

    if (state == Searching)
        endScanSearch();
    else
        endScanHold();
}


//-- Utility Methods -------------------------------------------------------------

designation *getGroupDes(unsigned short, char bankType = 0);
designation *getIndivDes(unsigned short);
designation *getPatchDes(unsigned short, BOOL retNull = FALSE);
void logStringT(register char *cp, InfoType it);
void logString(register char *cp, InfoAction ia = _BOTH);
unsigned short mapInpFreq(unsigned short ifreq);// maps vhf/uhf input frequency to output frequency code
void flushFreqs()
{
    memset(newFreqs, 0, sizeof(newFreqs));
}                                                                       // for frequency filter, to clear periodically


void unsafe();                          // to disable auto-save on destruct
void fixNew(designation *i, designation *g);    // internal, auto-annotation
void beginBeep(unsigned short tfreq, unsigned dur = BEEPDURATION)       // to trigger beep to speaker
{
    if (!lastBeep && !beepTimer)
    {
        beepDuration = dur;
        sound(tfreq);
        beepTimer = nowFine;
        lastBeep = now;
    }
}


channelDescriptor *findHeldCall();    // searches frequency table to see if matches held pattern
designation *mapPatchDesig(designation *d)      // to find a patch associated with a group
{
    if (patchesActive)
        return MapPatchDesig(d);
    else
        return d;
}


static void disAssociateId(designation *);
unsigned short getRecentGroup();
unsigned short getRecentRadio();

// user interface-related methods

void doSave();                  // save all data files
void jogUp();                   // move upward one call
void jogDown();                 // move downward one call
void setSearchMode();           // to end 'hold' mode
void setHoldMode();             // enter 'hold' mode
void holdThis(channelDescriptor *pLchanData);       // enter 'hold mode' for specified call
void setManualHold(unsigned short theId, BOOL isPatch = FALSE); // enter manual hold for group or patch
// individual manual hold is not supported!
void clean_stale_patches();     // check for patches no longer seen

private:                                                        // private methods

void init(const char *);        // common part of constructor
channelDescriptor *getChannel(unsigned short fn);     // returns zero if not yet usable because of filtering
void unAssociateId(designation *);      // internal
channelDescriptor *expandFreqs(unsigned short newF, int thresh = 3);
designation *MapPatchDesig(designation *);      // internal
void endScanSearch();           // internal
void endScanHold();             // internal
void initF(unsigned short sysId, short cellnum);
};
// Inlines: use a little space, save much time!
void inline TrunkSys::fixNew(designation *i, designation *g)
{
    // when a user first shows up in a group, tags that user's title string with
    // the group information.

    if (i->isUnAnnotated() && !(g->isUnAnnotated()))
    {
        if (userChoices.desiredAction(FORMAT) == _HEX)
            sprintf(tmp, "[#%.04hX]%s", g->theCode, g->titleString());
        else
            sprintf(tmp, "[%.05hu]%s", g->theCode, g->titleString());

        i->setEditTitle(tmp);
    }
}


inline BOOL isHybrid(char bankcode)
{
    return bankcode >= 'a' && bankcode <= 'n';
}


inline BOOL isTypeI(char bankcode)
{
    return bankcode >= 'A' && bankcode <= 'N';
}


inline int typeToIdx(char bankcode)
{
    return isHybrid(bankcode) ? (bankcode - 'a') : (bankcode - 'A');
}


inline BOOL isTypeIorHybrid(char bankcode)
{
    return isHybrid(bankcode) || isTypeI(bankcode);
}


inline BOOL isTypeII(char bankcode)
{
    return bankcode == '2';
}


inline unsigned short typeIgroup(unsigned short g, char bankcode)
{
    return (unsigned short)(g & ~idMask[typeToIdx(bankcode)]);
}


inline unsigned short typeIradio(unsigned short g, char bankcode)
{
    return (unsigned short)(g & radioMask[typeToIdx(bankcode)]);     // omit subfleet
}


inline char makeHybrid(char bankcode)
{
    return (char)((bankcode - 'A') + 'a');
}
