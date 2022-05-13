#include <missing.h>
#include <platform.h>
#include <osw.h>
#include <trunksys.h>
#include <time.h>
#include <stdlib.h>
#include <malloc.h>
#include "layouts.h"
static void clearRow(unsigned short rnum)
{
    static unsigned char blanks[COL80 + 1];
    static int first = 1;

    if (first)
    {
        memset(blanks, ' ', COL80);
        blanks[COL80] = '\0';
        first = 0;
    }

    settextposition(rnum, 1);
    _outmem(blanks, COL80);
}


void scrlog::clrbuf()
{
    memset(rowbuf, ' ', nrows * COL80);
}


void scrlog::validateParms(int maxrows, unsigned short firstrow)
{
    if (maxrows < 2 || maxrows > 40)
        shouldNever("screenlog parms - bad maxrows");

    if (firstrow < 1 || (firstrow + maxrows) >= 48)
        shouldNever("screenlog parms - bad first row");
}


scrlog::scrlog(int maxrows, unsigned short firstrow)
{
    scrlog::validateParms(maxrows, firstrow);
    nrows = maxrows;
    startrow = firstrow;
    rowbuf = (char *)malloc((1 + nrows) * COL80);

    if (!rowbuf)
        shouldNever("alloc of scrlog buffer failed");

    clrbuf();
    drawBar(firstrow - 1, " Detail Log ");
    this->redraw();
}


void scrlog::autoSize(int nfreq)
{
    // compute row and size based on number of frequencies
    int nr;
    unsigned short fr;

    if (nfreq <= 0)
        nfreq = 0;

    // compute position of first log line vertically:

    fr = (unsigned short)(
        6                                       // 5 lines of header so start at 6
        + nfreq                                 // one line for each frequency
        + 1);   // 'bar'

    // compute size of log area

    nr = 41 - fr;

    if (nr < 5)
        shouldNever("scrlog::autosize problem");

    startrow += 1;
    startrow -= 1;

    if (fr != startrow)
        this->reshape(nr, fr);
}


scrlog::~scrlog()
{
    if (rowbuf)
    {
        clearRow((unsigned short)(startrow - 1));
        clearRow(startrow);
        free(rowbuf);
    }
}


void scrlog::addString(const char *st)
{
    char inbuf[COL80 + 1];
    register int i; register char *cp;

    i = strlen(st);

    if (i >= COL80)
    {
        memcpy(inbuf, st, COL80);
    }
    else
    {
        memcpy(inbuf, st, i);

        while (i < COL80)
            inbuf[i++] = ' ';
    }

    inbuf[COL80] = '\0';

    // check for dups!
    for (i = 0, cp = rowbuf; i < nrows; ++i, cp += COL80)
        if (memcmp(inbuf + 8, cp + 8, 72) == 0)    // kludge to skip time value
            return;

    // implementation note store in time order but display upward or downward according to
    // user preference
    memmove(rowbuf + COL80, rowbuf, (nrows - 1) * COL80);
    memcpy(rowbuf, inbuf, COL80);
    this->redraw();
}


void scrlog::reshape(int newmaxrows, unsigned short newfirstrow)
{
    scrlog::validateParms(newmaxrows, newfirstrow);
    rowbuf = (char *)realloc(rowbuf, COL80 * (newmaxrows + 1));

    if (!rowbuf)
        shouldNever("realloc of scrlog buffer failed");

    // leaked the buffer here but will die anyway....
    clearRow((unsigned short)(startrow - 1));
    clearRow(startrow);
    startrow = newfirstrow;

    if (newmaxrows > nrows)
        memset(rowbuf + (nrows * COL80), ' ', (newmaxrows - nrows) * COL80);

    nrows = newmaxrows;
    drawBar(startrow - 1, " Detail Log ");
    this->redraw();
}


void scrlog::redraw()
{
    register int i;
    register char *cp;

    settextcolor(color_norm);

    if (userChoices.desiredAction(SCROLLDIR) == _NORM)
    {
        for (i = 0, cp = rowbuf + ((nrows - 1) * COL80); i < nrows; ++i, cp -= COL80)
        {
            settextposition((unsigned short)(i + startrow), 1);
            _outmem((unsigned char *)cp, COL79);
        }
    }
    else
    {
        for (i = 0, cp = rowbuf; i < nrows; ++i, cp += COL80)
        {
            settextposition((unsigned short)(i + startrow), 1);
            _outmem((unsigned char *)cp, COL79);
        }
    }

    refresh();
}
