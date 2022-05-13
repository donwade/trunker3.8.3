#include <missing.h>
#include <platform.h>
#include <osw.h>
#include <trunksys.h>

static char tempArea[150];
extern unsigned short bitsAtZero;
extern unsigned short bitsAtOne;
void stats(const char *scanType)
{
    static time_t tprev = 0;
    static time_t tstart = 0;
    static unsigned long pgood = 0;
    static unsigned long pbad = 0;
    register time_t diff;

    if (tstart)
    {
        if ((diff = now - tprev) >= 2)
        {
            register double g;
            register double b;
            register double e;

            gotoxy(61, STATUS_LINE);

            if (goodcount > (pgood + 20))
            {
                // we got something
                b = badcount;
                g = goodcount;
                e = 100. * (g / (g + b));

                if (e > 80.)
                {
                    settextcolor(color_norm);
                    _outtext("Hi-Qual");
                }
                else
                {
                    settextcolor(color_warn);
                    _outtext("DX Rcv ");
                }
            }
            else
            {
                settextcolor(color_err);
                // CRUSTIFY-OFF
                if (bitsAtZero & pinToRead && bitsAtOne & pinToRead)
                    _outtext("sig?inv");
                else
                    _outtext(wantstring);
                // CRUSTIFY-ON
                e = 0.;
                bitsAtZero = 0;
                bitsAtOne = 0;
            }

            gotoxy(74, STATUS_LINE);
            sprintf(tempArea, "%5.1f", e);
            _outtext(tempArea);
            tprev = now;
            pgood = goodcount;
            pbad = badcount;

            if (wrapArounds > 1)
            {
                settextcolor(color_err);
                gotoxy(68, STATUS_LINE + 1);
                sprintf(tempArea, "Data Lost %lu times!", wrapArounds);
                _outtext(tempArea);
            }
        }

        if (mySys)
            sprintf(tempArea, "%-*.*s", 27, 27, mySys->describe());
        else
            sprintf(tempArea, "%-*.*s", 27, 27, "Analyzing");

        QuickOut(STATUS_LINE, 1, color_norm, tempArea);

        if (mySys && mySys->siteId >= 0)
        {
            sprintf(tempArea, "<%2d>", (WORD)(1 + mySys->siteId));
            QuickOut(STATUS_LINE, 9 + 19, color_norm, tempArea);
        }
        else
        {
            QuickOut(STATUS_LINE, 9 + 19, color_norm, "    ");
        }
    }
    else
    {
        tstart = now;
        tprev = tstart;
        pgood = goodcount;
        pbad = badcount;
        QuickOut(STATUS_LINE, 42, color_norm, scanType);
        QuickOut(STATUS_LINE, 33, color_quiet, "Scanner:");
        QuickOut(STATUS_LINE, 53, color_quiet, "Status: Please  Acc: Wait %");
    }
}
