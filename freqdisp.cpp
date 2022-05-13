#include <missing.h>
#include <iostream>
#include <time.h>
#include <platform.h>
#include <osw.h>
#include <trunksys.h>
#include <motfreq.h>
#include "debug.h"
#include "missing.h"

/*
 *      Misc Functions and data
 */
static char tempArea[160];
double channelDescriptor::actualFreq;
static char lineChars[] = "-----------------------------------------------";
unsigned short idMask[14] = {
    15,	  63,  127, 511,
    31,	  31,  63,	127,
    255,  255, 255, 1023,
    2047, 4095
};
/*
 *                                  // S B     ub
 *                                  // i l     Fl
 *                                  // z k Fle ee
 *                                  // e s ets ts IDs
 *                                  // - - --- -- ----
 *      { 7, 13, 127,  6,  3,  4 }, // A 8 128  4   16
 *      { 7, 13,  15,  9,  7,  6 }, // B 8  16  8   64
 *      { 7, 13,   7, 10,  7,  7 }, // C 8   8  8  128
 *      { 7, 13,   0,  0, 15,  9 }, // D 8   1 16  512
 *      { 7, 13,  63,  7,  3,  5 }, // E 8  64  4   32
 *      { 7, 13,  31,  8,  7,  5 }, // F 8  32  8   32
 *      { 7, 13,  31,  8,  3,  6 }, // G 8  32  4   64
 *      { 7, 13,  15,  9,  3,  7 }, // H 8  16  4  128
 *      { 7, 13,   7, 10,  3,  8 }, // I 8   8  4  256
 *      { 7, 13,   3, 11,  7,  8 }, // J 8   4  8  256
 *      { 7, 13,   1, 12, 15,  8 }, // K 8   2 16  256
 *      { 6, 13,   0,  0, 15, 10 }, // L 4   1 16 1024
 *      { 4, 13,   0,  0, 15, 11 }, // M 2   1 16 2048
 *      { 0,  0,   0,  0, 15, 12 }  // N 1   1 16 4096
 */
unsigned short radioMask[14] = {        // leaves out the subfleet part
    0xffcf,                             // A bbb fffffff ss rrrr    1111 1111 1100 1111
    0xfe3f,                             // B bbb ffff sss rrrrrr    1111 1110 0011 1111
    0xfc7f,                             // C bbb fff sss rrrrrrr    1111 1100 0111 1111
    0xe1ff,                             // D bbb  ssss rrrrrrrrr    1110 0001 1111 1111
    0xff9f,                             // E bbb ffffff ss rrrrr    1111 1111 1001 1111
    0xff1f,                             // F bbb fffff sss rrrrr    1111 1111 0001 1111
    0xff3f,                             // G bbb fffff ss rrrrrr    1111 1111 0011 1111
    0xfe7f,                             // H bbb ffff ss rrrrrrr    1111 1110 0111 1111
    0xfcff,                             // I bbb fff ss rrrrrrrr    1111 1100 1111 1111
    0xf8ff,                             // J bbb ff sss rrrrrrrr    1111 1000 1111 1111
    0xf0ff,                             // K bbb f ssss rrrrrrrr    1111 0000 1111 1111
    0xc3ff,                             // L bb  ssss rrrrrrrrrr    1100 0011 1111 1111
    0x87ff,                             // M b  ssss rrrrrrrrrrr    1000 0111 1111 1111
    0x0fff                              // N   ssss rrrrrrrrrrrr    0000 1111 1111 1111
};
const char *excelTime(time_t *tt)
{
    static char myResult[50];
    register struct tm *tmp;

    tmp = localtime(tt);
    sprintf(myResult, "%02d/%02d/%04d,%02d:%02d\n",
            1 + tmp->tm_mon,
            tmp->tm_mday,
            1900 + tmp->tm_year,
            tmp->tm_hour,
            tmp->tm_min);
    return myResult;
}


/*
 * int
 * numcommas(char * i)
 * {
 *  register char *cp = i;
 *  register int res = 0;
 *  while(*cp) {
 *      if(*cp++ == ',') {
 * ++res;
 *      }
 *  }
 *  return res;
 * }
 */
void drawBar(int rowNum, const char *lab)
{
    int lenT, lenL, lenR;

    lenT = strlen(lab);

    if (lenT >= COL79)
    {
        sprintf(tempArea, "%-*.*s", COL79, COL79, lab);
    }
    else
    {
        lenL = (COL79 - lenT) / 2;
        lenR = COL79 - (lenT + lenL);
        sprintf(tempArea, "%-*.*s%s%-*.*s", lenL, lenL, lineChars, lab, lenR, lenR, lineChars);
    }

    tempArea[COL79] = 0;
    QuickOut(rowNum, 1, color_quiet, tempArea);
}


void fixType1(designation *i, designation *g)
{
    if (i->isUnAnnotated() && g->isAnnotated())
    {
        char tmp[120];

        if (userChoices.desiredAction(FORMAT) == _HEX)
            sprintf(tmp, "[#%04hX]%s", g->theCode, g->titleString());
        else
            sprintf(tmp, "[%.05hu]%s", g->theCode, g->titleString());

        i->setEditTitle(tmp);
    }
}




/*
 *      Methods for class patchedGroups
 */

patchedGroups::patchedGroups(unsigned short fCode, designation& firstgrp) : designation()
{
    init(fCode);
    setDispMode('0');
    buildInfo(&firstgrp);
    desType = desPatch;
    theGroups.insert(firstgrp.theCode, &firstgrp);
}


patchedGroups::patchedGroups(unsigned short ncode) : designation()
{
    init(ncode);
    setDispMode('0');
    desType = desPatch;
}


patchedGroups::patchedGroups() : designation()
{
    init(0);
    setDispMode('0');
    desType = desPatch;
}


patchedGroups::~patchedGroups()
{
    theGroups.clear();
}


void patchedGroups::buildInfo(designation *id)
{
    register const char *displayText = getDisplayText();
    register unsigned int dispLen = strlen(displayText);

    strcpy(title, id->titleString());

    if (strlen(titleString()) < (TITLESIZE - (4 + dispLen)))
        strcat(titleString(), " etc");
    else
        strcpy(&titleString()[TITLESIZE - (4 + dispLen)], " etc");

    color = id->color;
    prio = id->getEditPrio();
}


void patchedGroups::addGroup(designation *newgrp)
{
    if ((newgrp->getEditPrio() < this->getEditPrio()) && strstr(title, " etc"))
        buildInfo(newgrp);

    theGroups.insert(newgrp->theCode, newgrp);
}


void patchedGroups::delGroup(designation *nomore)
{
    theGroups.remove(nomore->theCode);
}


/*
 *      Methods for class desigList
 */


DesigList::DesigList(const char *ifn) : fn(strdup(ifn))
{
    register FILE *fp = fopen(fn, "rt");
    designation *d;

    if (fp)
    {
        (void)designation::import(0, 0);

        while ((d = designation::import(fp, "Radio")) != 0)
        {
            if (find(d->theCode))
            {
                sprintf(tempArea, "Radio #%04hx is duplicated!", d->theCode);
                shouldNever(tempArea);
            }

            insert(d->theCode, d);
        }

        fclose(fp);
    }
}


static void desigSave(designation *d, void *f)
{
    d->export1((FILE *)f);
}


static void patchDesigSave(patchedGroups *d, void *f)
{
    d->export1((FILE *)f);
}


inline void exportHeader(FILE *fp)
{
    fprintf(fp, ";VER=3\n;ID_hex,COLOR_dec,PRIO_dec,TITLE_str,LASTNOTED_hex,HITS_hex,USAGE_hex,ID_dec,ID_str,LASTNOTED_mm/dd/yyyy,LASTNOTED_hh:mm\n");
}


void DesigList::doSave(const char *tfn)
{
    if (!isEmpty())
    {
        prepareFile(tfn);
		lprintf2("s file %s open", tfn);
        register FILE *fp = fopen(tfn, "at");

        if (fp)
        {
            exportHeader(fp);
            this->forAll(&desigSave, (void *)fp);
            fclose(fp);
        }
        else
        {
            sprintf(tempArea, "open of %s for write failed", tfn);
            shouldNever(tempArea);
        }
    }
}


void PatchDesigList::doSave(const char *tfn)
{
    if (!isEmpty())
    {
        prepareFile(tfn);
		lprintf2(" j file %s open", tfn);
        register FILE *fp = fopen(tfn, "at");

        if (fp)
        {
            exportHeader(fp);
            this->forAll(&patchDesigSave, (void *)fp);
            fclose(fp);
        }
        else
        {
            sprintf(tempArea, "open of %s for write failed", tfn);
            shouldNever(tempArea);
        }
    }
}


designation *DesigList::get(unsigned short want, BOOL nullIfNone)
{
    designation *lp = this->find(want);

    if (lp != 0)
    {
        return lp;
    }
    else
    {
        if (nullIfNone)
        {
            return 0;
        }
        else
        {
            designation *d = new designation();

            if (!d)
                shouldNever("error allocating blank designation!");

            d->init(want);
            this->insert(d->theCode, d);
            return d;
        }
    }
}


patchedGroups *PatchDesigList::get(unsigned short want, BOOL nullIfNone)
{
    patchedGroups *lp;

    if ((lp = find(want)) != 0)
    {
        return lp;
    }
    else
    {
        if (nullIfNone)
        {
            return 0;
        }
        else
        {
            lp = new patchedGroups(want);

            if (!lp)
                shouldNever("error allocating blank designation!");

            lp->init(want);
            insert(lp->theCode, lp);
            return lp;
        }
    }
}


/*
 *      Methods for class desigList
 */
struct note1rparm
{
    DesigList *		lst;
    unsigned short	subfleet_mask;
    unsigned short	blknum;
    char			sizeLetter;
};
static void note1rIter(designation *d, void *f)
{
    designation *wantd;
    unsigned short searchRadio;

    note1rparm *p = (note1rparm *)f;

    if (d->blockNum() == p->blknum)
    {
        if ((d->theCode & p->subfleet_mask) == 0)
            return;                                                                                                 // this is a good, 'new' record

        // see if there is a radio occurrence for subfleet 0. if not, make one

        if (d->isAnnotated())
        {
            searchRadio = d->theCode & ~p->subfleet_mask;
            wantd = p->lst->get(searchRadio);
            *wantd = *d;
            wantd->theCode = searchRadio;                                                                                                     // avoid corruption on copy!
        }

        p->lst->remove(d->theCode);
        delete d;
    }
}


void DesigList::note_type_1_block(unsigned short blknum, char sizeLetter)
{
    note1rparm tparm;
    int idx;

    if (!isEmpty() && !isHybrid(sizeLetter) && !allowLurkers)
    {
        idx = typeToIdx(sizeLetter);
        tparm.subfleet_mask = ~radioMask[idx];
        tparm.blknum = blknum;
        tparm.sizeLetter = sizeLetter;
        tparm.lst = this;
        forAll(&note1rIter, &tparm);
    }
}


/*
 *      Methods for class GroupDesigList
 */
struct note1parm
{
    GroupDesigList *lst;
    unsigned short	mask;
    unsigned short	blknum;
    char			sizeLetter;
};
static void note1Iter(designation *d, void *f)
{
    note1parm *p = (note1parm *)f;

    if (d->blockNum() == p->blknum)
    {
        if (d->theCode & p->mask)                                                                    // wipe out IDs pretending to be groups
        {
            p->lst->remove(d->theCode);
            delete d;
        }
        else
        {
            d->setDispMode(p->sizeLetter);
        }
    }
}


void GroupDesigList::note_type_1_block(unsigned short blknum, char sizeLetter)
{
    note1parm tparm;

    if (!isEmpty())
    {
        tparm.mask = idMask[sizeLetter - 'A'];
        tparm.blknum = blknum;
        tparm.sizeLetter = sizeLetter;
        tparm.lst = this;
        forAll(&note1Iter, &tparm);
    }
}


struct note2parm
{
    GroupDesigList *lst;
    unsigned short	blknum;
};
static void note2Iter(designation *d, void *f)
{
    note2parm *p = (note2parm *)f;

    if (d->blockNum() == p->blknum)
    {
        if (d->flags())                                                                    // wipe out IDs pretending to be groups
        {
            p->lst->remove(d->theCode);
            delete d;
        }
        else
        {
            d->setDispMode('2');
        }
    }
}


void GroupDesigList::note_type_2_block(unsigned short blknum)
{
    note2parm tparm;

    if (!isEmpty())
    {
        tparm.blknum = blknum;
        tparm.lst = this;
        forAll(&note2Iter, &tparm);
    }
}


struct lookBankParm
{
    unsigned short	blockMask;
    BOOL			foundIt;
};
static void bankIter(designation *d, void *f)
{
    lookBankParm *p = (lookBankParm *)f;

    if (!p->foundIt)
        if ((d->theCode & ((unsigned short)0xE000)) == p->blockMask)
            p->foundIt = TRUE;

}


BOOL GroupDesigList::bankPresent(unsigned short blknum)
{
    lookBankParm pp;

    pp.blockMask = (unsigned short)((blknum & 7) << 13);
    pp.foundIt = FALSE;
    forAll(&bankIter, (void *)&pp);
    return pp.foundIt;
}


GroupDesigList::GroupDesigList(const char *ifn) : DesigList()
{
    fn = strdup(ifn);
    register FILE *fp = fopen(fn, "at");
    designation *d;

    sysBlkTypes = "????????";

    if (fp)
    {
        (void)designation::import(0, 0);

        while ((d = designation::import(fp, "Group")) != 0)
        {
            if (this->find(d->theCode))
            {
                sprintf(tempArea, "Group #%04hx is duplicated!", d->theCode);
                shouldNever(tempArea);
            }

            this->insert(d->theCode, d);
            d->desType = desGroup;
        }

        fclose(fp);
    }
}


PatchDesigList::PatchDesigList(const char *ifn)
{
    fn = strdup(ifn);
    register FILE *fp = fopen(fn, "rt");
    patchedGroups *d;

    if (fp)
    {
        (void)designation::import(0, 0);

        while ((d = patchedGroups::import(fp, "Patch")) != 0)
        {
            if (this->find(d->theCode))
            {
                sprintf(tempArea, "Patch #%04hx is duplicated!", d->theCode);
                shouldNever(tempArea);
            }

            this->insert(d->theCode, d);
        }

        fclose(fp);
    }
}


void GroupDesigList::doSave(const char *tfn)
{
    if (!isEmpty())
    {
        prepareFile(tfn);
		lprintf2("y file %s open", tfn);
        register FILE *fp = fopen(tfn, "at");

        if (fp)
        {
            exportHeader(fp);
            this->forAll(&desigSave, (void *)fp);
            fclose(fp);
        }
        else
        {
            shouldNever("open of group file for write failed!");
        }
    }
}


/*
 *      Methods for class designation
 */
void designation::fixCoding()
{
    static char tmp[16];
    static unsigned short theGrpId;

    if (isEncoded())
    {
        // if user choice != encoded format then recompute
        if (userChoices.desiredAction(FORMAT) == _HEX)
        {
            if (isDecUnk())
            {
                if (sscanf(title, "[%5hu", &theGrpId) == 1)
                {
                    sprintf(tmp, "[#%.04hX]", theGrpId);
                    memcpy(title, tmp, 7);
                }
                else
                {
                    shouldNever("sscanf failed in fixcoding 1");
                }
            }
        }
        else                                                                             // decimal requested
        {
            if (isHexUnk())
            {
                if (sscanf(title, "[#%4hX", &theGrpId) == 1)
                {
                    sprintf(tmp, "[%.05hu]", theGrpId);
                    memcpy(title, tmp, 7);
                }
                else
                {
                    shouldNever("sscanf failed in fixcoding 2");
                }
            }
        }
    }
}


designation *designation::import(FILE *f, const char *tp)
{
    static unsigned short cd;
    static unsigned short ic;
    static unsigned short ip;
    static unsigned short iu;                                     // usage
    static time_t itm;
    register designation *nd;
    static unsigned long ih;
    static int version = 1;
    int z;
    char tt[150];
    char idesc[100];

    if (!f)
    {
        version = 1;
        return 0;
    }

    while (fgets(tt, sizeof(tt), f))
    {
        if (strncmp(tt, "ID_hex,", 6) == 0)
            continue;                                                                                                             // skip title line at beginning - old versions

        if (strncmp(tt, ";VER=2", 6) == 0)
        {
            version = 2;
            continue;                                                                                                             // skip title line at beginning - newer versions
        }

        if (strncmp(tt, ";VER=3", 6) == 0)
        {
            version = 3;
            continue;                                                                                           // skip title line at beginning - newer versions
        }

        if (tt[0] == ';' || tt[0] == '#')                                                                       // general comments but lose them...
            continue;

        if (strchr(tt, ','))
        {
            itm = 0;
            ih = 0;
            iu = 0;

            if (version > 2)                                                                                                              // beginning with version 3, add 'usage'
                z = sscanf(tt, "%hx,%hu,%hu,\"%[^\"]\",%lx,%lx,%hx", &cd, &ic, &ip, idesc, &itm, &ih, &iu);
            else
                z = sscanf(tt, "%hx,%hu,%hu,\"%[^\"]\",%lx,%lx", &cd, &ic, &ip, idesc, &itm, &ih);

            if (z >= 4)
            {
                nd = new designation();

                if (!nd)
                    shouldNever("error allocating designations!");

                if (ip > 99)
                    ip = 99;

                ic &= 0x003F;                                                                                                                                                 // [beep] [flash] [4 bits of color]
                nd->init(cd, (unsigned char)ic, (unsigned char)ip, idesc, itm, ih);
                nd->usage = iu;
                return nd;
            }
            else
            {
                settextposition(10, 1);
                sprintf(tempArea, "bad %s designation entry.\n%s\n", tp, tt);
                shouldNever(tempArea);
            }
        }
    }

    return 0;
}


BOOL designation::export1(FILE *f)
{
    this->fixCoding();

    if (fprintf(f, "%04hx,%hu,%hu,\"%s\",%lx,%lx,%hx,%hu,%s,%s", theCode, (WORD)color, (WORD)prio,
                title, lastNoted, hits, usage, theCode, getDisplayText(),
                lastNoted ? excelTime(&lastNoted) : ",\n") > 0)
    {
        return TRUE;
    }
    else
    {
        shouldNever("export desig, printf failed");
        return FALSE;
    }
}


/*
 *
 */
designation::designation() :                             hits(0L),
    theCode(0),
    color(WHITE),
    prio(0),
    typeCode('?'),
    desType(desIndiv),
    usage(0),
    lastNoted(now)
{
    title[0] = '?';
    title[1] = '\0';
    title[sizeof(title) - 1] = '\0';
}


void designation::init(unsigned short cd,
                       unsigned char ic,
                       unsigned char ip,
                       const char *it,
                       time_t itm, unsigned long ihits
                       )
{
    theCode = cd;
    color = ic;
    prio = ip;
    setEditTitle(it);
    lastNoted = itm;
    hits = ihits;
}


static struct bankInfo
{
    unsigned short blockMask, blockShift, fleetMask, fleetShift, subMask, subShift;
} implBanks[14] = {                 //         S
    // S B     ub
    // i l     Fl
    // z k Fle ee
    // e s ets ts IDs
    // - - --- -- ----
    { 7, 13, 127, 6,  3,  4	 },                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 // A 8 128  4   16
    { 7, 13, 15,  9,  7,  6	 },                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 // B 8  16  8   64
    { 7, 13, 7,	  10, 7,  7	 },                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 // C 8   8  8  128
    { 7, 13, 0,	  0,  15, 9	 },                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 // D 8   1 16  512
    { 7, 13, 63,  7,  3,  5	 },                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 // E 8  64  4   32
    { 7, 13, 31,  8,  7,  5	 },                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 // F 8  32  8   32
    { 7, 13, 31,  8,  3,  6	 },                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 // G 8  32  4   64
    { 7, 13, 15,  9,  3,  7	 },                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 // H 8  16  4  128
    { 7, 13, 7,	  10, 3,  8	 },                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 // I 8   8  4  256
    { 7, 13, 3,	  11, 7,  8	 },                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 // J 8   4  8  256
    { 7, 13, 1,	  12, 15, 8	 },                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 // K 8   2 16  256
    { 6, 13, 0,	  0,  15, 10 },                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 // L 4   1 16 1024
    { 4, 13, 0,	  0,  15, 11 },                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 // M 2   1 16 2048
    { 0, 0,	 0,	  0,  15, 12 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  // N 1   1 16 4096
};
const char *designation::getDisplayText(int)
{
    static char displayText[16];
    char origin = 'A';

    switch (typeCode)
    {
    case '?':
    default:
    case '1':                                           // type 1 'id'
    case '0':                                           // edacs
    case '2':

        if (desType == desPatch)
        {
            if (userChoices.desiredAction(FORMAT) == _HEX)
                sprintf(displayText, "Patch=#%.4hX", theCode);
            else
                sprintf(displayText, "Patch=%5hu", theCode);
        }
        else
        {
            //lprintf3("the code 0x%X / %d", theCode, theCode);

            if(userChoices.desiredAction(FORMAT)==_HEX) 
                sprintf(displayText, "#%.4hX", theCode);
            else
                sprintf(displayText, "*%5hu", theCode);
        }

        return displayText;

    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
    case 'G':
    case 'H':
    case 'I':
    case 'J':
    case 'K':
    case 'L':
    case 'M':
    case 'N':
        break;

    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
    case 'g':
    case 'h':
    case 'i':
    case 'j':
    case 'k':
    case 'l':
    case 'm':
    case 'n':
        origin = 'a';
        break;
    }

    register bankInfo *bi = &implBanks[typeCode - origin];
    int bank = (theCode >> bi->blockShift) & bi->blockMask;
    int fleet = (theCode >> bi->fleetShift) & bi->fleetMask;
    int subfleet = (theCode >> bi->subShift) & bi->subMask;
    sprintf(displayText, "%.1hd %02hd-%02hd %04hX", 
					(WORD)bank, 
					(WORD)fleet, 
					(WORD)subfleet, 
					(WORD)theCode);
    return displayText;
}


/*
 *      Methods for class channelDescriptor
 */
void channelDescriptor::noteInactive()
{
    if (fState == FREQCALL)
    {
        if ((nowFine - refreshFine) > NOTSEENDELAY)
        {
            if (theDelay)
            {
                fState = FREQHOLD;
                dirty = TRUE;
                refreshFine = nowFine;
            }
            else
            {
                unassign();
            }
        }
    }
    else if (fState == FREQHOLD)
    {
        if ((nowFine - refreshFine) > theDelay)
            unassign();
    }
}


void channelDescriptor::freshen()
{
    if (fState == FREQHOLD)
    {
        dirty = TRUE;
        fState = FREQCALL;
    }

    if (fState == FREQCALL)
        refreshFine = nowFine;

    ++(this->hits);
}


void channelDescriptor::createByBandPlan(int idx, unsigned short f, unsigned short f2, const char *narrative, char bplan)
{
    lchan = f;
    inpFreqNum = f2;

    if (narrative)
        sprintf(freqdisp, "   %-8.8s", narrative);
    else
        sprintf(freqdisp, "   %-8.8s", myFreqMap->mapCodeToF(f, bplan));

    x = 1;
    y = (unsigned short)(FREQ_LINE + 3 + idx);
    fState = FREQEMPTY;
    visited = dirty = TRUE;
    refreshFine = 0;
    callingDes = destDes = 0;
    scancmdlen = 0;
    selected = FALSE;
    scancmd[0] = '\0';
    maxI = maxG = actI = actG = 0;
    curType = maxType = Idle;
    idArray = 0;
    prty = 99;
}

void channelDescriptor::createBySysId(register int idx, unsigned short f, unsigned short f2, const char *narrative, unsigned short sysId)
{
    lchan = f;
    inpFreqNum = f2;

    if (narrative)
        sprintf(freqdisp, "   %-8.8s", narrative);
    else
        sprintf(freqdisp, "   %8.4f", (double)mapSysIdCodeToFreq(f, sysId)/1000000.);

    x = 1;
    y = (unsigned short)(FREQ_LINE + 3 + idx);
    fState = FREQEMPTY;
    visited = dirty = TRUE;
    refreshFine = 0;
    callingDes = destDes = 0;
    scancmdlen = 0;
    selected = FALSE;
    scancmd[0] = '\0';
    maxI = maxG = actI = actG = 0;
    curType = maxType = Idle;
    idArray = 0;
    prty = 99;
}


/*
 * ID+0 = Normal talkgroup
 * ID+1 = ATG (All Talk Group, a Type II "fleetwide" mode)
 * ID+2 = Emergency TG
 * ID+3 = Crosspatched talkgroup (to another talkgroup)
 * ID+4 = Emergency crosspatch
 * ID+5 = Emergency multi-select
 * ID+6 = ?
 * ID+7 = Multi-select (initiated by dispatcher)
 *
 * ID+8 = DES (digital encryption)
 * ID+9 = DES ATG
 * ID+10 = Emergency DES TG
 * ID+11 = Crosspatched DES TG
 * ID+12 = DES Emergency crosspatch
 * ID+13 = DES Emergency multi-select
 * ID+14 = ?
 * ID+15 = Multi-select DES TG
 *
 * Added 0 = normal group
 * Added 1 (16 becomes 17) = ATG (All Talk Group, kind of like Type I fleetwide)
 * Added 2 (16 becomes 18) = Emergency (activated emergency banner)
 * Added 3 (16 becomes 19) = Patch (several TGs patched together, bidirectional)
 * Added 4 = Emergency patch
 * Added 5 = Emergency Multi-select
 * Added 7 (16 becomes 23) = Multi-select (dispatcher simulcast)
 * Added 8 (16 becomes 24) = DES (digital encryption DES and DVP)
 */
static char *flagbytes[64] = {
    /* 0*/ "g ", "a ", "e ", "p ", "ep", "ep", "? ", "m ",   // norm,atg,emerg,patch,emerPatch,emergMsel,<unk>,Msel,
    /* 8*/ "G ", "A ", "E ", "P ", "Ep", "Em", "? ", "M ",   // DES applied (see above).

    /*16*/ "gd", "ad", "ed", "pd", "ed", "ed", "?d", "Md",   // digital + all above (2x)
    /*24*/ "Gd", "Ad", "Ed", "Pd", "Ed", "Ed", "?d", "Md",   // for astro...

    /*32*/ "F ", "? ", "? ", "? ", "? ", "? ", "? ", "? ",   // phone patch
    /*40*/ "? ", "? ", "? ", "? ", "? ", "? ", "? ", "? ",
    /*48*/ "Fd", "? ", "? ", "? ", "? ", "? ", "? ", "? ",   // digital phone patch
    /*56*/ "? ", "? ", "? ", "? ", "? ", "? ", "? ", "? ",
};

#define BIT_NOEMERG    0  // bit not used?

#define BIT_EMERG   2  // bit 0 (ROW 0)

#define BIT_DES     3  // bit 1 (ROW 1)
#define BIT_DIGITAL 4  // bit 2 (ROW 2)
#define BIT_ASTRO   8  // bit 4 (ROW 3)
#define BIT_FONE    16 // bit 5 (ROW 4)


static unsigned short infoBits[64] = {
//                       210                   210          210                      210                   210                       210                      210                      210,
//                       000                   001          010                      011                   100                       101                      110                      0111,
//     543 210 543 210
    /* 000-000 000-111*/ 0,                    0,           BIT_EMERG,               0,                    BIT_EMERG,                BIT_EMERG,               0,                       0,                  // moto: norm,atg,emerg,patch,
    /* 001-000 001-111*/ BIT_DES,              BIT_DES,     BIT_DES | BIT_EMERG,     BIT_DES,              BIT_DES | BIT_EMERG,      BIT_DES | BIT_EMERG,     0,                       BIT_DES,            // DES + above
    /* 010-000 010-111*/ BIT_DIGITAL,          BIT_DIGITAL, BIT_DIGITAL | BIT_EMERG, BIT_DIGITAL,          BIT_DIGITAL | BIT_EMERG,  BIT_DIGITAL | BIT_EMERG, BIT_DIGITAL,             BIT_DIGITAL,        // other digital + all above (2x)
    /* 011-000 011-111*/ BIT_ASTRO,            BIT_ASTRO,   BIT_ASTRO | BIT_EMERG,   BIT_ASTRO,            BIT_ASTRO | BIT_EMERG,    BIT_ASTRO | BIT_EMERG,   BIT_ASTRO,               BIT_ASTRO,          // for astro...

    /* 100-000 100-111*/ BIT_FONE,             0,           0,                       0,                    0,                        0,                       0,                   // phone patch
    /* 101-000 101-111*/ 0,                    0,           0,                       0,                    0,                        0,                       0,                       0,
    /* 110-000 110-111*/ BIT_FONE|BIT_DIGITAL, 0,           0,                       0,                    0,                        0,                       0,                       0,                   // digital phone patch
    /* 111-000 111-111*/ 0,                    0,           0,                       0,                    0,                        0,                       0,                       0
};


#define FLSH_LRED (BRIGHTRED + 16 + 32)

static unsigned char flagcolors[64] = {
/*0*/ -1,          WHITE,    FLSH_LRED, WHITE,     FLSH_LRED, FLSH_LRED, BRIGHTBLUE, WHITE,    //norm,atg,emerg,patch, emerPatch,emergMsel,<unk>,Msel,
/*8*/  BRIGHTYELLOW,     BRIGHTYELLOW,   BRIGHTYELLOW,    BRIGHTYELLOW,    BRIGHTYELLOW,    BRIGHTYELLOW,    BRIGHTYELLOW,    BRIGHTYELLOW,   // DES for above
/*16*/ BLUE,       BLUE,     BLUE,      BLUE,      BLUE,      BLUE,      BLUE,      BLUE,     // digital
/*24*/ RED,        RED,      RED,       RED,       RED,       RED,       RED,       RED,      // DES

/*32*/ -1,         GREEN,    GREEN,     GREEN,     GREEN,     GREEN,     GREEN,     GREEN,    // vacant codes
/*40*/ GREEN,      GREEN,    GREEN,     GREEN,     GREEN,     GREEN,     GREEN,     GREEN,    // vacant codes
/*48*/ BRIGHTGREEN, GREEN,    GREEN,     GREEN,     GREEN,     GREEN,     GREEN,     GREEN,    // vacant codes
/*56*/ GREEN,      GREEN,    GREEN,     GREEN,     GREEN,     GREEN,     GREEN,     GREEN     // vacant codes
};

static enum { suppress,
              forceto,
              normal,
              mute                                                                                                                      // suppress but use color
} flagActions[64] = { /*0*/ normal,		   normal,	forceto, normal,                                                                    // typical
                            /*4*/ forceto, forceto, normal, normal,                                                                     // typical
                            /*8*/ mute,	   mute,	mute, mute,                                                                         // DES
                            /*12*/ mute,   mute,	mute, mute,                                                                         // DES
                            /*16*/ mute,   mute,	mute, mute,                                                                         // digital
                            /*20*/ mute,   mute,	mute, mute,                                                                         // digital
                            /*24*/ mute,   mute,	mute, mute,                                                                         // digital des
                            /*28*/ mute,   mute,	mute, mute,                                                                         // digital des

                            /*32*/ normal, mute,	mute, mute,                                                                         // edacs phone patch
                            /*36*/ mute,   mute,	mute, mute,                                                                         // vacant code
                            /*40*/ mute,   mute,	mute, mute,                                                                         // vacant
                            /*44*/ mute,   mute,	mute, mute,                                                                         // vacant
                            /*48*/ mute,   mute,	mute, mute,                                                                         // digital fone
                            /*52*/ mute,   mute,	mute, mute,                                                                         // vacant
                            /*56*/ mute,   mute,	mute, mute,                                                                         // vacant
                            /*60*/ mute,   mute,	mute, mute                                                                          // vacant
};

void channelDescriptor::show()
{
    BOOL stale = FALSE;
    unsigned int theLen;
    const char *displayText;
    char t[4];
    unsigned short tflgs;

    switch (fState)
    {
    case FREQHOLD:
        stale = TRUE;

    // drop thru
    case FREQCALL:

        // Draw Frequency+priority: green for stale, yellow for active

        settextposition(y, x);
        settextcolor(seenVoice ? color_norm : color_quiet);
        strncpy(t, freqdisp, 4);
        t[3] = 0;
        _outtext(t);

        settextcolor((short)stale ? color_quiet : color_active);
        _outtext(&freqdisp[3]);

        settextposition(y, (unsigned short)(x + PRTYSTART));
        sprintf(tempArea, "%.2d", prty > 99 ? 99 : (WORD)prty);
        _outtext(tempArea);

        settextposition(y, (unsigned short)(x + DETSTART));

        if (!stale)
            settextcolor((unsigned short)(leftColor & 31));

        displayText = destDes->getDisplayText(flags);
        theLen = strlen(displayText);

        if (theLen > 5)
            theLen = TITLESIZE - (theLen - 5);
        else
            theLen = TITLESIZE;

        sprintf(tempArea, "%-*.*s %5s ", theLen, theLen, destDes->titleString(), displayText);
        _outtext(tempArea);

        // Draw Flag or G or I tag:     green for stale
        //                              designated color for no flags (possibly grey)
        //                              from the table if flags

        if (!stale)
            settextcolor((unsigned short)(flagColor & 31));

        if (flags == NOFLAGS || flags == 0)
        {
            tempArea[0] = getMaxType();
            tempArea[1] = ' ';
            tempArea[2] = '\0';
            _outtext(tempArea);
        }
        else
        {
            if (getMaxType() == Icall)
            {
                {
                    static char tmpflags[10];
                    register char *cp;
                    strcpy(tmpflags, flagbytes[flags]);

                    for (cp = tmpflags; *cp; ++cp)
                    {
                        if (*cp == 'g')
                            *cp = 'i';
                        else if (*cp == 'G')
                            *cp = 'I';
                    }

                    _outtext(tmpflags);
                }
            }
            else
            {
                _outtext(flagbytes[flags]);
            }

            tflgs = infoBits[flags];

            if (tflgs)
            {
                if (callingDes)
                    callingDes->usage |= tflgs;

                destDes->usage |= tflgs;
            }
        }

        // Draw originating information: GREEN for stale,
        //                               designated color or grey if active and present
        //                               white or grey dash if not present based on priority

        settextcolor((unsigned short)(stale ? color_quiet : (rightColor & 31)));

        if (callingDes)
        {
            if (userChoices.desiredAction(FORMAT) == _HEX)
                sprintf(tempArea, " %.4hX %-*.*s", callingDes->theCode, TITLESIZE, TITLESIZE, callingDes->titleString());
            else
                sprintf(tempArea, "%5hu %-*.*s", callingDes->theCode, TITLESIZE, TITLESIZE, callingDes->titleString());
        }
        else
        {
            sprintf(tempArea, "----- %-*.*s", TITLESIZE, TITLESIZE, "(none)");
        }

        _outtext(tempArea);

        if (!stale)
        {
            if (flagColor & BEEPINDICATOR)
                mySys->beginBeep(1500, wordsPerSec / 2);
            else if (leftColor & BEEPINDICATOR)
                mySys->beginBeep(4000);
            else if (rightColor & BEEPINDICATOR)
                mySys->beginBeep(600);
        }

        break;

    case FREQDATA:
        QuickOut(y, x, BRIGHTBLACK, freqdisp);
        settextposition(y, (unsigned short)(x + PRTYSTART));
        sprintf(tempArea, "%-*.*s", COL80 - (x + PRTYSTART - 1), COL80 - (x + PRTYSTART - 1), "");
        _outtext(tempArea);
        break;

    case FREQEMPTY:
        settextposition(y, x);
        settextcolor(seenVoice ? color_norm : color_quiet);
        strncpy(t, freqdisp, 4);
        t[3] = 0;
        _outtext(t);
        settextcolor(color_quiet);
        _outtext(&freqdisp[3]);
        settextposition(y, (unsigned short)(x + PRTYSTART));
        sprintf(tempArea, "%-*.*s", COL80 - (x + PRTYSTART - 1), COL80 - (x + PRTYSTART - 1),
                "");
        _outtext(tempArea);
        break;

    case FREQCW:
        settextposition(y, x);
        settextcolor(seenVoice ? color_active : color_quiet);
        strncpy(t, freqdisp, 4);
        t[3] = 0;
        _outtext(t);
        settextcolor(color_quiet);
        _outtext(&freqdisp[3]);
        settextposition(y, (unsigned short)(x + PRTYSTART));
        sprintf(tempArea, "%-*.*s", COL80 - (x + PRTYSTART - 1), COL80 - (x + PRTYSTART - 1),
                "   CW Id");
        _outtext(tempArea);
        break;
    }

    dirty = FALSE;
}


void channelDescriptor::unassign()
{
    if (fState != FREQEMPTY)
    {
        curType = Idle;
        maxType = Idle;
        actI = actG = 0;

        if (idArray)
        {
            free(idArray);
            idArray = 0;
        }

        fState = FREQEMPTY;
        destDes = 0;
        callingDes = 0;
        dirty = TRUE;
        selected = FALSE;
        prty = 99;
    }

    visited = TRUE;
}


channelDescriptor::channelDescriptor() : maxI(0), maxG(0), actI(0), actG(0), curType(Idle),
    maxType(Idle), idArray(0), callingDes(0), destDes(0),
    refreshFine(0), visited(FALSE), dirty(FALSE), x(0), y(0), flags(0),
    scancmdlen(0), lchan(0), inpFreqNum(0), prty(0), fState(FREQEMPTY),
    selected(FALSE), leftColor(0), rightColor(0), flagColor(0), hits(0),
    seenVoice(FALSE)
{
    freqdisp[0] = '\0';
    scancmd[0] = '\0';
}


void channelDescriptor::clobber()
{
    maxI = 0;
    maxG = 0;
    actI = 0;
    actG = 0;
    curType = Idle;
    maxType = Idle;
    idArray = 0;
    callingDes = 0;
    destDes = 0;
    refreshFine = 0;
    visited = FALSE;
    dirty = TRUE;
    x = 0;
    y = 0;
    flags = 0;
    scancmdlen = 0;
    lchan = 0;
    inpFreqNum = 0;
    prty = 0;
    fState = FREQEMPTY;
    selected = FALSE;
    leftColor = 0;
    rightColor = 0;
    flagColor = 0;
    hits = 0;
    freqdisp[0] = '\0';
    scancmd[0] = '\0';
}


void channelDescriptor::assignCW(BOOL tf)
{
    if (freqdisp[0] != 'c')
    {
        freqdisp[0] = 'c';
        dirty = TRUE;
        selected = FALSE;
    }

    if (fState == FREQDATA || fState == FREQCALL || fState == FREQHOLD || (fState == FREQCW && !tf))
        unassign();

    if (tf)
    {
        dirty = TRUE;
        fState = FREQCW;
    }

    visited = TRUE;
    prty = 100;
}


void channelDescriptor::assignD()
{
    if (fState == FREQDATA)
    {
        visited = TRUE;
        return;
    }

    if (fState != FREQEMPTY)
        unassign();

    // nothing to data channel
    freqdisp[1] = 'd';
    fState = FREQDATA;
    visited = TRUE;
    dirty = TRUE;
    selected = FALSE;
}


patchedGroups *patchedGroups::import(FILE *f, const char *tp)
{
    static unsigned short cd;
    static unsigned short ic;
    static unsigned short ip, iu;
    static time_t itm;
    register patchedGroups *nd;
    static unsigned long ih;
    int z;
    static int version = 1;
    char tt[150];
    char idesc[100];

    if (!f)
    {
        version = 1;
        return 0;
    }

    while (fgets(tt, sizeof(tt), f))
    {
        if (strncmp(tt, ";ID_hex,", 7) == 0 ||
            strncmp(tt, "ID_hex,", 6) == 0)
            continue;                                                                                                             // skip title line at beginning

        if (strncmp(tt, ";VER=2", 6) == 0)
        {
            version = 2;
            continue;                                                                                                             // skip title line at beginning
        }

        if (strncmp(tt, ";VER=3", 6) == 0)
        {
            version = 3;
            continue;                                                                                                             // skip title line at beginning
        }

        if (tt[0] == ';' || tt[0] == '#')
            continue;

        if (strchr(tt, ','))
        {
            itm = 0;
            ih = 0;
            iu = 0;

            if (version > 2)                                                                                                              // beginning with version 3, include usage
                z = sscanf(tt, "%hx,%hu,%hu,\"%[^\"]\",%lx,%lx,%hx", &cd, &ic, &ip, idesc, &itm, &ih, &iu);
            else
                z = sscanf(tt, "%hx,%hu,%hu,\"%[^\"]\",%lx,%lx", &cd, &ic, &ip, idesc, &itm, &ih);

            if (z >= 4)
            {
                nd = new patchedGroups();

                if (!nd)
                    shouldNever("error allocating designations!");

                if (ip > 99)
                    ip = 99;

                ic &= 0x003F;                                                                                                                                                 // [beep] [flash] [4 bits of color]
                nd->init(cd, (unsigned char)ic, (unsigned char)ip, idesc, itm, ih);
                nd->usage = iu;
                return nd;
            }
            else
            {
                sprintf(tempArea, "bad %s designation entry\n%s", tp, tt);
                shouldNever(tempArea);
            }
        }
    }

    return 0;
}


void channelDescriptor::calcDisplay()
{
    int thisAction = 0;

    if (destDes->getPrio() < prty)
        prty = destDes->getPrio();

    if (callingDes && callingDes->getPrio() < prty)
        prty = callingDes->getPrio();

    if (flags && (flags != NOFLAGS))
    {
        if (trackDigital)
            thisAction = flagActions[flags & ~(8 | OTHER_DIGITAL)];                                                                                                           // ignore digital and encryption here
        else
            thisAction = flagActions[flags];

        switch (thisAction)
        {
        case normal:
        default:
            thisAction = 0;
            break;

        case suppress:
            prty = 100;
            break;

        case mute:
            prty = 100;
            break;

        case forceto:

            if (doEmergency)
                prty = 0;
            else
                thisAction = 0;

            break;
        }
    }
    else
    {
        thisAction = 0;
    }

    // compute colors ONCE!
    leftColor = destDes->color;

    if (thisAction && (destDes->desType != desIndiv) && (flags && (flags != NOFLAGS)) && (prty <= scanThreshold || thisAction == mute))
    {
        if (trackDigital)
            flagColor = flagcolors[flags & ~(8 | OTHER_DIGITAL)];                                                                                                           // ignore digital and encryption here
        else
            flagColor = flagcolors[flags];
    }
    else
    {
        flagColor = leftColor;
    }

    if (callingDes)
        rightColor = callingDes->color;
    else
        rightColor = color_norm;

    dirty = TRUE;
}


void channelDescriptor::recalcDisplay()
{
    int wantG, wantI, gotG, gotI;
    register int i;
    register int limit;

    gotG = 0;
    gotI = 0;
    wantG = actG ? 1 : 0;

    if (actG)
    {
        wantG = 1;
        wantI = 1;
    }
    else
    {
        wantI = 2;
    }

    if (actI < wantI)
        wantI = actI;

    if (actG < wantG)
        wantG = actG;

    if (!wantG && !wantI)
    {
        destDes = 0;
        callingDes = 0;
        dirty = TRUE;
        return;
    }

    for (limit = actI + actG, i = 0; (i < limit) && ((gotI < wantI) || (gotG < wantG)); ++i)
    {
        if (idArray[i]->desType == desIndiv)
        {
            if (gotI < wantI)
            {
                if (!wantG && gotI == 0)
                {
                    if (destDes != idArray[i])
                    {
                        destDes = idArray[i];
                        dirty = TRUE;
                    }
                }
                else
                {
                    if (callingDes != idArray[i])
                    {
                        callingDes = idArray[i];
                        dirty = TRUE;
                    }
                }

                ++gotI;
            }
        }
        else
        {
            if (gotG < wantG)
            {
                if (destDes != idArray[i])
                {
                    destDes = idArray[i];
                    dirty = TRUE;
                }

                ++gotG;
            }
        }
    }

    if ((gotI + gotG) < 2)
    {
        callingDes = 0;
        dirty = TRUE;
    }

    if ((gotI + gotG) == 0)
    {
        destDes = 0;
        dirty = TRUE;
    }

    if (destDes)
    {
        if (dirty)
        {
            prty = 100;
            calcDisplay();
        }
    }
    else
    {
        unassign();
    }
}


void channelDescriptor::recalcTypeFewer()
{
    if ((!actI && !actG) || (maxG == 1 && !actG))
    {
        unassign();
    }
    else if (curType == Special)
    {
        if (actG == 1 && actI < 2)
        {
            curType = Gcall;
            dirty = TRUE;
        }
        else if (actG == 0 && actI == 2)
        {
            curType = Icall;
            dirty = TRUE;
        }
        else if (actG == 0 && actI == 1)
        {
            curType = Phone;
            dirty = TRUE;
        }
    }
}


void channelDescriptor::recalcTypeMore()
{
    if (curType == Phone && actI > 1)
    {
        curType = Icall;
        dirty = TRUE;
    }
    else if (curType == Gcall && (actG > 2 || actI > 1))
    {
        curType = Special;
        dirty = TRUE;
    }
    else if ((curType == Icall || curType == Phone) && actG > 0)
    {
        curType = Gcall;
        dirty = TRUE;
    }
}


void channelDescriptor::setMaxType(CallType t, designation *d1, designation *d2, designation *d3)
{
    if (maxType != t)
    {
        if (maxType != Idle)
            unassign();

        freqdisp[2] = 'v';
        seenVoice = TRUE;
        fState = FREQCALL;
        maxType = t;

        switch (t)
        {
        case Icall:     maxG = 0; maxI = 2; break;

        case Gcall:     maxG = 1; maxI = 1; break;

        case HybGcall:  maxG = 1; maxI = 2; break;

        case Special:   maxG = 50; maxI = 50; break;

        case Phone:             maxG = 0; maxI = 1; break;

        case Idle:              shouldNever("bad switch, setmaxtype to idle"); break;
        }

        idArray = (designation **)calloc(maxG + maxI, sizeof(designation *));

        if (!idArray)
            shouldNever("could not allocate array of pointers to desig");

        TrunkSys::disAssociateId(d1);
        idArray[0] = d1;
        ++(d1->hits);

        if (d1->desType == desIndiv)
            ++actI;
        else
            ++actG;

        if (d2)
        {
            ++(d2->hits);

            if (d1->desType == desGroup &&
                (isTypeI(d1->typeCode) || isHybrid(d1->typeCode)))
            {
                // allow multiple appearances of radio on type 1 calls
            }
            else
            {
                TrunkSys::disAssociateId(d2);
            }

            idArray[1] = d2;

            if (d2->desType == desIndiv)
                ++actI;
            else
                ++actG;

            if (d3)
            {
                ++(d3->hits);
                TrunkSys::disAssociateId(d3);
                idArray[2] = d3;

                if (d3->desType == desIndiv)
                    ++actI;
                else
                    ++actG;
            }
        }

        destDes = d1;
        callingDes = d2;
        calcDisplay();
    }
    else if (t == Gcall && destDes == d1 && d2)
    {
        if (d3)
            refreshGcall(d1, d2, d3);
        else
            refreshGcall(d1, d2);
    }
    else if (t == Icall && (destDes == d1 || callingDes == d1))
    {
        if (d2)
            refreshIcall(d1, d2);
    }
    else if (t == HybGcall && destDes == d1 && !d3)
    {
        // see if either id matches, consider as refresh, else re-assign
        if (actI >= 1 && idArray[1] == d2 ||
            actI >= 2 && idArray[2] == d2)
        {
            // nothing
        }
        else
        {
            unassign();
            setMaxType(t, d1, d2, d3);                                                                                                          // recurse
        }
    }
    else
    {
        if (((actG + actI) >= 1 && idArray[0] != d1) ||
            ((actG + actI) >= 2 && d2 && idArray[1] != d2) ||
            ((actG + actI) >= 3 && d3 && idArray[2] != d3))
        {
            unassign();
            setMaxType(t, d1, d2, d3);                                                                  // recurse
        }                                                                                               // else identical match!

    }
}


BOOL channelDescriptor::setDesig(int idx, designation *d)
{
    if (idArray[idx] == d)
        return FALSE;

    if (idArray[idx])
    {
        if (idArray[idx]->desType == desIndiv)
            --actI;
        else
            --actG;

        idArray[idx] = 0;
    }

    if (d)
    {
        TrunkSys::disAssociateId(d);

        if (d->desType == desIndiv)
            ++actI;
        else
            ++actG;

        idArray[idx] = d;
    }

    return TRUE;
}


void channelDescriptor::refreshGcall(designation *d1, designation *d2)
{
    BOOL anyChanged = FALSE;

    anyChanged = setDesig(0, d1);

    if (anyChanged)                                        // group ID different!
    {
        setDesig(1, d2);
    }
    else if (d2)
    {
        if ((actI + actG) == 3)
        {
            if (idArray[1] == d2 || idArray[2] == d2)                                                                                                                // for hybrid case
            {
            }
            else
            {
                anyChanged = setDesig(1, d2);
                anyChanged |= setDesig(2, 0);
            }
        }
        else
        {
            anyChanged = setDesig(1, d2);
        }
    }

    if (anyChanged)
    {
        destDes = d1;
        callingDes = d2;
        calcDisplay();
    }
}


void channelDescriptor::refreshIcall(designation *d1, designation *d2)
{
    BOOL anyChanged = FALSE;

    anyChanged = setDesig(0, d1);

    if (anyChanged)                                        // group ID different!
        setDesig(1, d2);
    else
        anyChanged = setDesig(1, d2);

    if (anyChanged)
    {
        destDes = d1;
        callingDes = d2;
        calcDisplay();
    }
}


void channelDescriptor::refreshGcall(designation *d1, designation *d2, designation *d3)
{
    BOOL anyChanged = FALSE;

    anyChanged = setDesig(0, d1);

    if (anyChanged)                                        // group ID different!
    {
        setDesig(1, d2);
        setDesig(2, d3);
    }
    else if (d2)
    {
        anyChanged = setDesig(1, d2);

        if (anyChanged)
            setDesig(2, d3);
    }

    if (anyChanged)
    {
        destDes = d1;
        callingDes = d2;
        calcDisplay();
    }
}


void channelDescriptor::associateId(designation *d)
{
    if (!idArray)
        shouldNever("associating ID but no storage array");

    register int i;
    register int lim = actI + actG;

    for (i = 0; i < lim; ++i)
        if (idArray[i] == d)
            return;

    // duplicate found

    if (d->desType == desIndiv && actI == 1 && maxI == 1)
    {
        for (i = 0; i < lim; ++i)
        {
            if (idArray[i]->desType == desIndiv)
            {
                TrunkSys::disAssociateId(d);
                idArray[i] = d;
                recalcDisplay();
                return;
            }
        }

        shouldNever("had one ID but could not find it");
    }

    if (d->desType != desIndiv && actG == 1 && maxG == 1)
    {
        for (i = 0; i < lim; ++i)
        {
            if (idArray[i]->desType != desIndiv)
            {
                TrunkSys::disAssociateId(d);
                idArray[i] = d;
                recalcDisplay();
                return;
            }
        }

        shouldNever("had one grp but could not find it");
    }

    if (d->desType == desIndiv && actI == 2 && maxI == 2)
    {
        unsigned int first = -1, second = -1;

        for (i = 0; i < lim; ++i)
        {
            if (idArray[i]->desType == desIndiv)
            {
                if (first == -1)
                {
                    first = i;
                }
                else if (second == -1)
                {
                    second = i;
                    TrunkSys::disAssociateId(d);
                    idArray[first] = idArray[second];
                    idArray[second] = d;
                    recalcDisplay();
                    return;
                }
                else
                {
                    shouldNever("searching past 2 ids");
                }
            }
        }
    }

    // at this point, we are adding to one of the categories...
    if (d->desType == desIndiv)
    {
        if (actI >= maxI)
        {
            shouldNever("ran out of individual slots");
        }
        else
        {
            TrunkSys::disAssociateId(d);
            idArray[actG + actI] = d;
            ++actI;
        }
    }
    else
    {
        if (actG >= maxG)
        {
            shouldNever("ran out of group slots");
        }
        else
        {
            TrunkSys::disAssociateId(d);
            idArray[actG + actI] = d;
            ++actG;
        }
    }

    recalcTypeMore();
    recalcDisplay();
}


BOOL channelDescriptor::disAssociateId(designation *d)
{
    if (idArray)
    {
        register int i;
        register int lim = actI + actG;

        for (i = 0; i < lim; ++i)
        {
            if (idArray[i] == d)
            {
                while (i < (lim - 1))
                {
                    idArray[i] = idArray[i + 1];
                    ++i;
                }

                if (d->desType == desIndiv)
                {
                    if (actI)
                        --actI;
                    else
                        shouldNever("removed individ but no count");
                }
                else
                {
                    if (actG)
                        --actG;
                    else
                        shouldNever("removed group but no count");
                }

                recalcTypeFewer();

                if (destDes == d || callingDes == d)
                    recalcDisplay();

                return TRUE;
            }
        }
    }

    return FALSE;
}
