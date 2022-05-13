#include <platform.h>
#include <osw.h>
#include <trunksys.h>
#include <time.h>
#include <stdlib.h>
#include <malloc.h>

static void
clearRow(unsigned short rnum)
{
	static unsigned char blanks[81];
	static int first = 1;

	if ( first )
	{
		memset(blanks, ' ', 80);
		blanks[80] = '\0';
		first = 0;
	}
	_settextposition(rnum, 1);
	_outmem(blanks, 80);
}
void
scrlog::clrbuf()
{
	memset(rowbuf, ' ', nrows * 80);
}
void
scrlog::validateParms(int maxrows, unsigned short firstrow)
{
	if (maxrows < 2 || maxrows > 40)
		shouldNever("screenlog parms - bad maxrows");
	if ( firstrow < 1 || (firstrow + maxrows) >= 48)
		shouldNever("screenlog parms - bad first row");
}
scrlog::scrlog(int maxrows, unsigned short firstrow)
{
	scrlog::validateParms(maxrows, firstrow);
	nrows = maxrows;
	startrow = firstrow;
	rowbuf = (char*)malloc((1 + nrows) * 80 );
	if (!rowbuf)
		shouldNever("alloc of scrlog buffer failed");
	clrbuf();
	drawBar(firstrow - 1, " Detail Log ");
	this->redraw();
}
void
scrlog::autoSize(int nfreq)
{
	// compute row and size based on number of frequencies
	int nr;
	unsigned short fr;

	if (nfreq <= 0)
		nfreq = 0;

	// compute position of first log line vertically:

	fr = (unsigned short)(
	    6               // 5 lines of header so start at 6
	    + nfreq         // one line for each frequency
	    + 1);           // 'bar'

	// compute size of log area

	nr = 41 - fr;
	if (nr < 5)
		shouldNever("scrlog::autosize problem");
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

void
scrlog::addString(const char *st)
{
	char inbuf[81];
	register int i; register char *cp;

	i = strlen(st);
	if ( i >= 80 )
		memcpy(inbuf, st, 80);
	else
	{
		memcpy(inbuf, st, i);
		while (i < 80)
			inbuf[i++] = ' ';
	}
	inbuf[80] = '\0';
	// check for dups!
	for (i = 0, cp = rowbuf; i < nrows; ++i, cp += 80)
		if (memcmp(inbuf + 8, cp + 8, 72) == 0)                    // kludge to skip time value
			return;

	// implementation note store in time order but display upward or downward according to
	// user preference
	memmove(rowbuf + 80, rowbuf, (nrows - 1) * 80);
	memcpy(rowbuf, inbuf, 80);
	this->redraw();
}
void
scrlog::reshape(int newmaxrows, unsigned short newfirstrow)
{
	scrlog::validateParms(newmaxrows, newfirstrow);
	rowbuf = (char*)realloc(rowbuf, 80 * (newmaxrows + 1));
	if (!rowbuf)
		shouldNever("realloc of scrlog buffer failed");
	// leaked the buffer here but will die anyway....
	clearRow((unsigned short)(startrow - 1));
	clearRow(startrow);
	startrow = newfirstrow;
	if ( newmaxrows > nrows )
		memset(rowbuf + (nrows * 80), ' ', (newmaxrows - nrows) * 80);
	nrows = newmaxrows;
	drawBar(startrow - 1, " Detail Log ");
	this->redraw();
}
void
scrlog::redraw()
{
	register int i;
	register char *cp;

	_settextcolor(color_norm);
	if (userChoices.desiredAction(SCROLLDIR) == NORM)
	{
		for (i = 0, cp = rowbuf + ((nrows - 1) * 80); i < nrows; ++i, cp -= 80)
		{
			_settextposition((unsigned short)(i + startrow), 1);
			_outmem((unsigned char*)cp, 79);
		}
	}
	else
	{
		for (i = 0, cp = rowbuf; i < nrows; ++i, cp += 80)
		{
			_settextposition((unsigned short)(i + startrow), 1);
			_outmem((unsigned char*)cp, 79);
		}
	}
}



