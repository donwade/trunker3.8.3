#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <missing.h>

#include <ptrmap.h>
#include <trunkvar.h>
#include <scanner.h>
#include <platform.h>
#include "osw.h"
#include "trunksys.h"
#include "missing.h"
#include "debug.h"

extern BOOL freqHopping;


#define min(a, b)  (((a) < (b)) ? (a) : (b))
#define K_UPARROW 512
#define K_DNARROW 513
#define K_LTARROW 514
#define K_RTARROW 516
#define K_INSERT 517
#define K_DELETE 518
#define K_HOME 519
#define K_END 520
static unsigned short begCol, curCount, maxCount;
static enum EditType
{
    EDIT_GROUP,
    EDIT_RADIO,
    EDIT_PATCH
} editType, desType;
static unsigned short theId;
static char theInBuf [100];
static char tempArea [200];
static BOOL manType;
static BOOL saveFrame;

static enum
{
    CMDMODE,
    IDMODE,
    RADIOMODE,
    EDITMODE,
    MANMODE,
    CONFIGMODE,
    OPTIONMODE,
    EXITMODE,
    THRESHMODE
} keymode = CMDMODE;

static enum
{
    MENU1,
    MENU2
}
menuSelect = MENU1;

static enum
{
    RADIO_VOLUME,
    RADIO_SQUELCH
} radioCmd = RADIO_VOLUME;


extern Scanner *myScanner;
void debugSystem(void);
static designation *target;
static int formField = 0;

enum fldTpe { Hex, Dec, Str, Eof };
typedef struct
{
    enum fldTpe		type;
    int				scrwide;
    char *			prompt;
    void (*entryFcn)();
    unsigned short	numval;
    char *			strval;
    int				curpos;
    unsigned short	offset;
    char			edbuf[COL80];
} editfield;
static void showPrty()
{
    sprintf(tempArea, "Priority 0 Best, larger worse. > %d not scanned                             ", (unsigned)scanThreshold);
    QuickOut(FBROW, 1, color_quiet, tempArea);
}


static void showColor()
{
    register unsigned short i;


    settextposition(FBROW, 1);
    settextcolor(color_quiet);
    _outtext("Colors, +16=blink, +32=beep ");

    for (i = 0; i < 16; ++i)
    {
        settextcolor(i);
        sprintf(tempArea, "%2d ", i);
        _outtext(tempArea);
    }

    // show colors and mention +16 to flash, +32 to beep
}


static editfield myForm[] = {
    { Hex, 4,		  "ID:",	 0		   },
    { Str, TITLESIZE, " Title:", 0		   },
    { Dec, 2,		  " Prty:",	 showPrty  },
    { Dec, 2,		  " Color:", showColor },
    { Eof }
};
static void buildPrompt(char *bp, editfield *ef)
{
    char *obp;

    obp = bp;
    *bp = '\0';
    settextposition(PROMPT_LINE, 1);

    for (; ef->type != Eof; ++ef)
    {
        switch (ef->type)
        {
        case Hex:
            sprintf(ef->edbuf, "%-*hx", ef->scrwide, ef->numval);
            break;

        case Dec:
            sprintf(ef->edbuf, "%-*hu", ef->scrwide, ef->numval);
            break;

        case Str:
            sprintf(ef->edbuf, "%-*.*s", ef->scrwide, ef->scrwide, ef->strval);
        }

        settextcolor(color_quiet);
        _outtext(ef->prompt);
        bp += sprintf(bp, "%s", ef->prompt);
        ef->offset = (unsigned short)(1 + (bp - obp));
        bp += sprintf(bp, "%s", ef->edbuf);
        ef->curpos = 0;
        settextcolor(color_norm);
        _outtext(ef->edbuf);
    }
}


extern void fieldAdvance(void);
static void editCommit(void)
{
    if (formField < 4)
    {
        while (formField < 4)
            fieldAdvance();

        // will recurse once to 'else' case
    }
    else
    {
        strcpy(target->titleString(), myForm[1].edbuf);
        target->setEditPrio((unsigned char)myForm[2].numval);
        target->setEditColor((unsigned char)(myForm[3].numval & 63));
        buildPrompt(tempArea, myForm);

        while (strlen(tempArea) < COL80)
            strcat(tempArea, " ");

        QuickOut(FBROW, 1, color_norm, tempArea);
        showPrompt(FALSE);
    }
}


void fieldAdvance(void)
{
    register editfield *ef = &myForm[formField];
    register char *cp;

    for (cp = &ef->edbuf[ef->scrwide - 1]; cp != &ef->edbuf[0] && isspace(*cp); --cp)
    {
        // empty
    }

    *(cp + 1) = '\0';   // null terminate, max one blank for blank field

    switch (ef->type)
    {
    case Hex:
        sscanf(ef->edbuf, "%hx", &ef->numval);
        break;

    case Dec:
        sscanf(ef->edbuf, "%hu", &ef->numval);
        break;
    }

    ++formField;
    ++ef;

    if (ef->type == Eof)
    {
        editCommit();
    }
    else
    {
        if (ef->entryFcn)
            ef->entryFcn();

        settextposition(PROMPT_LINE, ef->offset);
    }
}


static void fieldData(char c)
{
    char z[2];
    register editfield *ef = &myForm[formField];

    z[0] = c;
    z[1] = 0;

    QuickOut(PROMPT_LINE, ef->offset + ef->curpos, color_norm, z);
    ef->edbuf[ef->curpos++] = c;

    if (ef->curpos >= ef->scrwide)
        fieldAdvance();

    // text cursor now properly positioned if not advancing field
}


static void fieldDelRev()
{
    register editfield *ef = &myForm[formField];
    register int i;

    if (ef->curpos)
    {
        for (i = ef->curpos; i < ef->scrwide; ++i)
            ef->edbuf[i - 1] = ef->edbuf[i];

        ef->edbuf[ef->scrwide - 1] = ' ';
        ef->curpos--;
        QuickOut(PROMPT_LINE, ef->offset, color_norm, ef->edbuf);
        settextposition(PROMPT_LINE, (unsigned short)(ef->offset + ef->curpos));
    }
}


static void fieldDelFwd()
{
    register editfield *ef = &myForm[formField];
    register int i;

    if (ef->curpos < (ef->scrwide - 1))
    {
        for (i = ef->curpos + 1; i < ef->scrwide; ++i)
            ef->edbuf[i - 1] = ef->edbuf[i];

        ef->edbuf[ef->scrwide - 1] = ' ';
        QuickOut(PROMPT_LINE, ef->offset, color_norm, ef->edbuf);
        settextposition(PROMPT_LINE, (unsigned short)(ef->offset + ef->curpos));
    }
    else if (ef->curpos == (ef->scrwide - 1))
    {
        ef->edbuf[ef->curpos] = ' ';
        QuickOut(PROMPT_LINE, ef->offset, color_norm, ef->edbuf);
        settextposition(PROMPT_LINE, (unsigned short)(ef->offset + ef->curpos));
    }
}


static void fieldInsert()
{
    register editfield *ef = &myForm[formField];
    register int i;

    if (ef->curpos < (ef->scrwide - 1))
    {
        for (i = ef->scrwide - 1; i > ef->curpos; --i)
            ef->edbuf[i] = ef->edbuf[i - 1];

        ef->edbuf[ef->curpos] = ' ';
        QuickOut(PROMPT_LINE, ef->offset, color_norm, ef->edbuf);
        settextposition(PROMPT_LINE, (unsigned short)(ef->offset + ef->curpos));
    }
}


static void fieldBackspace()
{
    register editfield *ef = &myForm[formField];

    if (ef->curpos)
    {
        --(ef->curpos);
        settextposition(PROMPT_LINE, (unsigned short)(ef->offset + ef->curpos));
    }
}


static void fieldFwdspace()
{
    register editfield *ef = &myForm[formField];

    if (++(ef->curpos) >= ef->scrwide)
        fieldAdvance();
    else
        settextposition(PROMPT_LINE, (unsigned short)(ef->offset + ef->curpos));
}


static void editBegin(unsigned short theId, EditType etype)
{
    sprintf(tempArea, "%*.*s", COL80, COL80, "");
    tempArea [COL79] = 0;
    QuickOut(FBROW, 1, color_quiet, tempArea);
    desType = etype;

    switch (etype)
    {
    case EDIT_RADIO:
        target = mySys->getIndivDes(theId);
        break;

    case EDIT_GROUP:
        target = mySys->getGroupDes(theId);
        break;

    case EDIT_PATCH:
        target = mySys->getPatchDes(theId);
        break;

    default:
        return;
    }

    if (userChoices.desiredAction(FORMAT) == _HEX)
    {
        myForm[0].type = Hex;
        myForm[0].scrwide = 4;
    }
    else
    {
        myForm[0].type = Dec;
        myForm[0].scrwide = 5;
    }

    myForm[0].numval = target->theCode;
    myForm[1].strval = target->titleString();
    myForm[2].numval = target->getEditPrio();
    myForm[3].numval = target->color;
    buildPrompt(tempArea, myForm);
    settextposition(PROMPT_LINE, myForm[1].offset);
    keymode = EDITMODE;
    settextcursor((short)0x0007);     // block cursor
    formField = 1;
}


static void doEditKey(int theKey)
{
    switch (theKey)
    {
    case '\033':
        showPrompt(1);
        break;

    case '\t':
        fieldAdvance();
        break;

    case '\r':
    case '\n':
        editCommit();
        break;

    case K_LTARROW:
        fieldBackspace();
        break;

    case K_RTARROW:
        fieldFwdspace();
        break;

    case '\b':
        fieldDelRev();
        break;

    case K_DELETE:
        fieldDelFwd();
        break;

    case K_INSERT:
        fieldInsert();
        break;

    default:

        switch (myForm[formField].type)
        {
        case Hex:

            if (isxdigit(theKey))
                fieldData((char)theKey);

            break;

        case Dec:

            if (isdigit(theKey))
                fieldData((char)theKey);

            break;

        case Str:

            if (isprint(theKey) && theKey != ',' && theKey != '"')
                fieldData((char)theKey);

        case Eof:
            break;                                                                                                             // should never get here!
        }
    }
}


static char tbuf[3];
static void commitThresh()
{
    unsigned int theNewThresh;

    if (sscanf(tbuf, "%u", &theNewThresh) == 1)
    {
        scanThreshold = (unsigned char)theNewThresh;
        sprintf(tempArea, "Scan Threshold set. Audio will not be monitored for prty > %u    ", (unsigned)scanThreshold);
        QuickOut(FBROW, 1, color_quiet, tempArea);
        showPrompt(FALSE);
    }
    else
    {
        showPrompt(1);
    }
}


static void doThreshKey(int theKey)
{
    if ((theKey >= '0' && theKey <= '9') || theKey == ' ')
    {
        tbuf[curCount++] = (char)theKey;
        QuickOut(PROMPT_LINE, begCol, color_norm, tbuf);
        settextposition(PROMPT_LINE, (unsigned short)(begCol + curCount));

        if (curCount >= maxCount)
            commitThresh();
    }
    else if (theKey == '\033')
    {
        showPrompt(1);
    }
    else if (theKey == '\t' || theKey == '\n' || theKey == '\r')
    {
        commitThresh();
    }
}


static void showTprompt()
{
    sprintf(tempArea, "set scan threshold: priority <= %02u can be monitored                           ", (unsigned int)scanThreshold);
    QuickOut(PROMPT_LINE, 1, color_quiet, tempArea);
    begCol = 33;
    maxCount = 2;
    curCount = 0;
    sprintf(tbuf, "%02u", (unsigned int)scanThreshold);
    QuickOut(PROMPT_LINE, begCol, color_norm, tbuf);
    settextcursor((short)0x0007);     // block cursor
    settextposition(PROMPT_LINE, begCol);
}


static void showIprompt(int theType, const char *theFcn = "")
{
    unsigned short theCode = 0;

    switch (theType)
    {
    case 'g':
    case 'G':

        if (mySys)
            theCode = mySys->getRecentGroup();

        if (userChoices.desiredAction(FORMAT) == _HEX)
        {
            if (theCode)
                sprintf(tempArea, "Group ID: 0x%04hx %-63s", theCode, theFcn);
            else
                sprintf(tempArea, "Group ID: 0x____ %-63s", theFcn);
        }
        else
        {
            if (theCode)
                sprintf(tempArea, "Group ID: %5hu %-63s", theCode, theFcn);
            else
                sprintf(tempArea, "Group ID: _____ %-63s", theFcn);
        }

        editType = EDIT_GROUP;
        break;

    case 'i':
    case 'I':

        if (mySys)
            theCode = mySys->getRecentRadio();

        if (userChoices.desiredAction(FORMAT) == _HEX)
        {
            if (theCode)
                sprintf(tempArea, "Radio ID: 0x%04hx %-63s", theCode, theFcn);
            else
                sprintf(tempArea, "Radio ID: 0x____ %-63s", theFcn);
        }
        else
        {
            if (theCode)
                sprintf(tempArea, "Radio ID: %5hu %-63s", theCode, theFcn);
            else
                sprintf(tempArea, "Radio ID: _____ %-63s", theFcn);
        }

        editType = EDIT_RADIO;
        break;

    case 'p':
    case 'P':

        if (userChoices.desiredAction(FORMAT) == _HEX)
            sprintf(tempArea, "Patch ID: 0x____ %-63s", theFcn);
        else
            sprintf(tempArea, "Patch ID: _____ %-63s", theFcn);

        editType = EDIT_PATCH;
        break;

    default:
        return;
    }

    settextposition(PROMPT_LINE, 1);
    settextcolor(color_quiet);
    _outtext(tempArea);

    if (userChoices.desiredAction(FORMAT) == _HEX)
    {
        begCol = 13;
        maxCount = 4;
    }
    else
    {
        begCol = 11;
        maxCount = 5;
    }

    theId = theCode;

    if (theCode)
    {
        if (userChoices.desiredAction(FORMAT) == _HEX)
            sprintf(theInBuf, "%04hx", theCode);
        else
            sprintf(theInBuf, "%5hu", theCode);

        curCount = 0;
    }
    else
    {
        curCount = 0;
        memset(theInBuf, '\0', 6);
    }

    settextcursor((short)0x0007);     // block cursor
    settextposition(PROMPT_LINE, begCol);
}


static void showRprompt()
{
    sprintf(tempArea,
            "%c=Volume (%d), %c=Squelch (%d), +=Increment, -=Decrement, <esc>,Q=MainMenu%40s",
            radioCmd == RADIO_VOLUME ? 'V' : 'v', myScanner->volume,
            radioCmd == RADIO_SQUELCH ? 'S' : 's', myScanner->squelch,
            "");
    tempArea [COL79] = 0;
    QuickOut(PROMPT_LINE, 1, color_quiet, tempArea);
}


static void showXprompt()
{
    sprintf(tempArea, "enter X to quit without saving, <esc> to resume, or Q to save and quit %10s", "");
    tempArea[COL79] = 0;
    QuickOut(PROMPT_LINE, 1, color_err, tempArea);
}


const char *getCurrentOptions()
{
    static char myOptions[15];

    myOptions[0] = seekNew ? 'N' : 'n';
    myOptions[1] = Verbose ? 'V' : 'v';
    myOptions[2] = (theDelay == DELAY_OFF) ? 'd' : 'D';
    myOptions[3] = freqHopping ? 'F' : 'f';
    myOptions[4] = '\0';
    return myOptions;
}


static void showOprompt()
{
    sprintf(tempArea, "%-*.*s", COL79, COL79, userChoices.getActionPrompt());
    tempArea [COL79] = 0;
    QuickOut(PROMPT_LINE, 1, color_quiet, tempArea);
}


static void showCprompt()
{
#ifdef LEGACY	
    if (!freqHopping)
    {
        freqHopping = TRUE;

        if (InSync)
            syncState = FALSE;
        else
            syncState = TRUE;
    }
	
#endif

    sprintf(tempArea, "m=change slicer mode[%s], p=change slicer pin[%s], <esc>,Q=MainMenu%20s",
            slicerInverting ? "INVERT" : "NORMAL",
            pinToRead == MSR_CTS ? "CTS" : (pinToRead == MSR_DSR ? "DSR" : "DCD"), ""
            );
    tempArea[COL79] = 0;
    QuickOut(PROMPT_LINE, 1, color_quiet, tempArea);
}


static void doConfigKey(register int inchar)
{
    switch (inchar)
    {
    case 'm':
    case 'M':

        if (slicerInverting)
            slicerInverting = FALSE;
        else
            slicerInverting = TRUE;

        showCprompt();
        break;

    case 'p':
    case 'P':

        switch (pinToRead)
        {
        case MSR_CTS:   pinToRead = MSR_DSR; break;

        case MSR_DSR:   pinToRead = MSR_DCD; break;

        case MSR_DCD:   pinToRead = MSR_CTS; break;
        }

        showCprompt();
        break;

    case 'q':
    case 'Q':
    case '\033':

#ifdef LEGACY
        if (!saveFrame)
        {
            freqHopping = FALSE;
            gotoxy(51, STATUS_LINE);
            settextcolor(color_norm);
            _outtext("  ");
        }

        showPrompt(1);
#endif
        break;

    default:
        break;
    }
}


static void doOptionKey(register int inchar)
{
    switch (inchar)
    {
    case (int)AFFIL:
    case (int)DIAGNOST:
    case (int)MEMBERS:
    case (int)CALLALERT:
    case (int)EMERG:
    case (int)NEIGHBORS:
    case (int)LIFETIME:
    case (int)FORMAT:
        userChoices.toggleAction((InfoType)inchar);
        showOprompt();
        break;

    case (int)SCROLLDIR:
        userChoices.toggleAction((InfoType)inchar);

        if (mySys)
            mySys->refreshLog();

        showOprompt();
        break;

    case 'q':
    case 'Q':
    case '\033':
        showPrompt(1);
        break;

    default:
        break;
    }
}


void setCurrentOptions(register const char *cp)
{
    for (; cp && *cp; ++cp)
    {
        switch (*cp)
        {
        case 'n':
            seekNew = FALSE;
            break;

        case 'N':
            seekNew = TRUE;
            break;

        case 'v':
            Verbose = FALSE;
            break;

        case 'V':
            Verbose = TRUE;
            break;

        case 'd':
            theDelay = DELAY_OFF;
            break;

        case 'D':
            theDelay = DELAY_ON;
            break;

        case 'A':
            apco25 = TRUE;
            break;
            
        case 'a':
            apco25 = FALSE;
            break;

        case 'u':
            uplinksOnly = FALSE;
            break;
                
        case 'U':
            uplinksOnly = TRUE;
            break;

        case 'f':
            freqHopping = FALSE;
            gotoxy(51, STATUS_LINE);
            settextcolor(color_norm);
            _outtext("  ");
            break;

        case 'F':
            freqHopping = TRUE;

#ifdef LEGACY
            if (InSync)
                syncState = FALSE;
            else
                syncState = TRUE;
#endif

            break;

        default:
            break;
        }
    }

    showPrompt(FALSE);
}


void showPrompt(BOOL clearit)
{
    settextcursor((short)0x2000);       // no cursor

    if (menuSelect == MENU1)
    {
        sprintf(tempArea,
                "%c=Delay I,G,P=edit %c=Verbose %c=seekNew m,M=manhold %c=Hold/srch space=More",
                theDelay == DELAY_ON ? 'D' : 'd',
                Verbose ? 'V' : 'v',
                seekNew ? 'N' : 'n',
                (mySys && mySys->getState() != TrunkSys::Searching) ? 'H' : 'h');
    }
    else
    {
        sprintf(tempArea,
                "%c=FreqTrack %c=APCO %c=UplinkOnly Radio Save ESC,Q=QuitMenu Cfg Options Thresh space=More",
                freqHopping ? 'F' : 'f',
                apco25 ? 'A' : 'a',
                uplinksOnly ? 'U' : 'u'
                );
    }

    while (strlen(tempArea) < COL79)
        strcat(tempArea, " ");

    tempArea [COL79] = 0;
    QuickOut(PROMPT_LINE, 1, color_quiet, tempArea);
    keymode = CMDMODE;

    if (clearit)
    {
        sprintf(tempArea, "%*.*s", COL80, COL80, "");
        tempArea [COL79] = 0;
        QuickOut(FBROW, 1, color_quiet, tempArea);
    }
}


static void doExitKey(register int inchar)
{
    switch (inchar)
    {

    case 'x':
    case 'X':

        if (mySys)
            mySys->unsafe();

    case 'q':
    case 'Q':
        exit("user quit called");
        break;

    default:
        break;
    }
}


static void doCmdKey(register int inchar)
{
    switch (inchar)
    {
    case 'Q':
    case 'q':
        showXprompt();
        keymode = EXITMODE;
        break;

    case 't':
    case 'T':
        showTprompt();
        keymode = THRESHMODE;
        break;

#if UT || BT
    case 'Z':
    case 'z':
        debugSystem();
        break;
#endif

    case 'n':
    case 'N':
        seekNew = !seekNew;
        showPrompt(1);
        break;

    case K_UPARROW:

        if (mySys)
        {
            if (mySys->getState() == TrunkSys::Searching)
                mySys->setHoldMode();

            mySys->jogUp();
            showPrompt(1);
        }

        break;

    case K_DNARROW:

        if (mySys)
        {
            if (mySys->getState() == TrunkSys::Searching)
                mySys->setHoldMode();

            mySys->jogDown();
            showPrompt(1);
        }

        break;

    case 'h':
    case 'H':

        if (mySys)
        {
            if (mySys->getState() == TrunkSys::Searching)
                mySys->setHoldMode();
            else
                mySys->setSearchMode();

            showPrompt(1);
        }

        break;

    case 'v':
    case 'V':
        Verbose = !Verbose;
        showPrompt(1);
        break;

    case 'D':
    case 'd':

        if (theDelay)
            theDelay = DELAY_OFF;
        else
            theDelay = DELAY_ON;

        showPrompt(1);
        break;

    case 'f':                                           // fF: toggle frame status updates
    case 'F':

        if (freqHopping)
        {
            freqHopping = FALSE;
#ifdef LEGACY
            gotoxy(51, STATUS_LINE);
            settextcolor(color_norm);
            _outtext("  ");
#endif
        }
        else
        {
            freqHopping = TRUE;
#ifdef LEGACY
            if (InSync)
                syncState = FALSE;
            else
                syncState = TRUE;
#endif
        }

        showPrompt(1);
        break;

    case 'A':   // APCO 16
    case 'a':
        if ( apco25)
            apco25 = FALSE;
        else
           apco25 = TRUE;
        
        showPrompt(1);
        break;

    case 'U':   // uplink only
    case 'u':
        if ( uplinksOnly)
            uplinksOnly = FALSE;
        else
           uplinksOnly = TRUE;
        
        showPrompt(1);
        break;

    case 's':
    case 'S':                                           // sS: save

        if (mySys)
            mySys->doSave();

        break;

    case 'M':

        if (mySys)
        {
            showIprompt('p', "(to hold - use lower-case 'm' for Group Hold)");
            keymode = MANMODE;
            manType = TRUE;
        }

        break;

    case 'm':

        if (mySys)
        {
            showIprompt('g', "(to hold - use upper-case 'M' for Patch Hold)");
            keymode = MANMODE;
            manType = FALSE;
        }

        break;

    case 'g':
    case 'G':

        // enter annotation mode:
        if (mySys)
        {
            showIprompt(inchar, "(to edit)");                                                                             // should set cursors, too
            keymode = IDMODE;
        }

        break;

    case 'i':
    case 'I':

        // enter annotation mode:
        if (mySys)
        {
            showIprompt(inchar, "(to edit)");                                                                             // should set cursors, too
            keymode = IDMODE;
        }

        break;

    case 'p':
    case 'P':

        // enter annotation mode:
        if (mySys)
        {
            showIprompt(inchar, "(to edit)");                                                                             // should set cursors, too
            keymode = IDMODE;
        }

        break;

    case 'r':
    case 'R':
        showRprompt();
        radioCmd = RADIO_VOLUME;
        keymode = RADIOMODE;
        break;

    case 'o':
    case 'O':
        showOprompt();
        keymode = OPTIONMODE;
        break;

    case 'c':
    case 'C':
        saveFrame = freqHopping;
        showCprompt();
        keymode = CONFIGMODE;
        break;

    case ' ':

        if (menuSelect == MENU1)
            menuSelect = MENU2;
        else
            menuSelect = MENU1;

        showPrompt();
        break;

    default:
        break;
    }
}


static void doRadioKey(register int inchar)
{
    switch (inchar)
    {
    case 'v':
    case 'V':
        radioCmd = RADIO_VOLUME;
        showRprompt();
        break;

    case 's':
    case 'S':
        radioCmd = RADIO_SQUELCH;
        showRprompt();
        break;

    case '+':

        if (radioCmd == RADIO_VOLUME)
        {
            myScanner->volume = myScanner->volume <= 250 ? myScanner->volume + 5 : 255;
            myScanner->setVolume(myScanner->volume);
        }
        else if (radioCmd == RADIO_SQUELCH)
        {
            myScanner->squelch = myScanner->squelch <= 250 ? myScanner->squelch + 5 : 255;
            myScanner->setSquelch(myScanner->squelch);
        }

        showRprompt();
        break;

    case '-':

        if (radioCmd == RADIO_VOLUME)
        {
            myScanner->volume = myScanner->volume >= 5 ? myScanner->volume - 5 : 0;
            myScanner->setVolume(myScanner->volume);
        }
        else if (radioCmd == RADIO_SQUELCH)
        {
            myScanner->squelch = myScanner->squelch >= 5 ? myScanner->squelch - 5 : 0;
            myScanner->setSquelch(myScanner->squelch);
        }

        showRprompt();
        break;

    case 'q':
    case 'Q':
    case '\033':
        showPrompt(1);
        break;

    default:
        break;
    }
}


static void doIdKey(register int inchar)
{
    int i;

    switch (inchar)
    {
    case '\033':                                     // abort annotation
        showPrompt(1);
        break;

    case '\b':

        if (curCount > 0)
        {
            for (i = curCount - 1; i < 5; ++i)
                theInBuf[i] = theInBuf[i + 1];

            --curCount;
            settextposition(PROMPT_LINE, begCol);
            settextcolor(color_norm);
            sprintf(tempArea, "%-5.5s", theInBuf);
            _outtext(tempArea);
            settextposition(PROMPT_LINE, (unsigned short)(begCol + curCount));
        }

        break;

    case '\t':
    case '\r':
    case '\n':

        if (userChoices.desiredAction(FORMAT) == _HEX)
        {
            if (sscanf(theInBuf, "%hx", &theId) == 1)
            {
                if (keymode == MANMODE)
                {
                    if (mySys)
                        mySys->setManualHold(theId, manType);

                    showPrompt(1);
                }
                else
                {
                    editBegin(theId, editType);                                                                                                                                                    // should set cursors too
                }
            }
            else
            {
                showPrompt(1);
            }
        }
        else
        {
            if (sscanf(theInBuf, "%hu", &theId) == 1)
            {
                if (keymode == MANMODE)
                {
                    if (mySys)
                        mySys->setManualHold(theId, manType);

                    showPrompt(1);
                }
                else
                {
                    editBegin(theId, editType);                                                                                                                                                    // should set cursors too
                }
            }
            else
            {
                showPrompt(1);
            }
        }

        break;

    default:

        if ((inchar >= '0' && inchar <= '9')
            || (((inchar >= 'a' && inchar <= 'f')
                 || (inchar >= 'A' && inchar <= 'F')) && userChoices.desiredAction(FORMAT) == _HEX)
            )
        {
            theInBuf[curCount] = (char)inchar;
            settextposition(PROMPT_LINE, begCol);
            settextcolor(color_norm);
            _outtext(theInBuf);

            if (++curCount >= maxCount)
            {
                if (userChoices.desiredAction(FORMAT) == _HEX)
                {
                    if (sscanf(theInBuf, "%hx", &theId) == 1 && theId)
                    {
                        if (keymode == MANMODE)
                        {
                            if (mySys)
                                mySys->setManualHold(theId, manType);

                            showPrompt(1);
                        }
                        else
                        {
                            editBegin(theId, editType);                                                                                                                                                                                                                            // should set cursors too
                        }
                    }
                    else
                    {
                        showPrompt(1);
                    }
                }
                else
                {
                    if (sscanf(theInBuf, "%hu", &theId) == 1 && theId)
                    {
                        if (keymode == MANMODE)
                        {
                            if (mySys)
                                mySys->setManualHold(theId, manType);

                            showPrompt(1);
                        }
                        else
                        {
                            editBegin(theId, editType);                                                                                                                                                                                                                            // should set cursors too
                        }
                    }
                    else
                    {
                        showPrompt(1);
                    }
                }
            }
            else
            {
                settextposition(PROMPT_LINE, (unsigned short)(begCol + curCount));
            }
        }
    }
}


void doKeyEvt()
{
    register int inchar = getch();

/*
    if (inchar == 0x1b)
    {
        byebye();
        exit(2);
    }
*/

    if (inchar == 0 || inchar == 0x0e)
    {
        inchar = getch();

        switch (inchar)
        {
        case 'H': inchar = K_UPARROW; break;

        case 'P': inchar = K_DNARROW; break;

        case 'K': inchar = K_LTARROW; break;

        case 'M': inchar = K_RTARROW; break;

        case 'R': inchar = K_INSERT; break;

        case 'S': inchar = K_DELETE; break;

        case 'G': inchar = K_HOME; break;

        case 'O': inchar = K_END; break;

        default:  return;
        }
    }

    switch (keymode)
    {
    case CMDMODE:           doCmdKey(inchar);       break;

    case EXITMODE:          doExitKey(inchar);      break;

    case RADIOMODE:         doRadioKey(inchar);     break;

    case CONFIGMODE:        doConfigKey(inchar);    break;

    case IDMODE:            doIdKey(inchar);        break;

    case MANMODE:           doIdKey(inchar);        break;

    case EDITMODE:          doEditKey(inchar);      break;

    case OPTIONMODE:        doOptionKey(inchar);    break;

    case THRESHMODE:        doThreshKey(inchar);    break;
    }
}
