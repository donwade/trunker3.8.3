#include <platform.h>
#include <colors.h>
#include <osw.h>

static char tempArea[150];
extern unsigned short bitsAtZero;
extern unsigned short bitsAtOne;
void stats(const char *)
{
    static time_t tprev = 0;
    static time_t tstart = 0;
    static unsigned long pgood = 0;
    static unsigned long pbad = 0;
    register time_t diff;

    if (tstart)
    {
        if ((diff = now - tprev) > 4)
        {
            register double g;
            register double b, e;

            gotoxy(61, STATUS_LINE);

            if (goodcount > (pgood + 20))
            {
                // we got something
                b = badcount;
                g = goodcount;
                e = 100. * (g / (g + b));

                if (e > 80.)
                {
                    settextcolor(WHITE);
                    _outtext("Hi-Qual");
                }
                else
                {
                    settextcolor(BRIGHTYELLOW);
                    _outtext("DX Rcv ");
                }
            }
            else
            {
                settextcolor(BRIGHTRED);

                if (bitsAtZero & pinToRead && bitsAtOne & pinToRead)
                    _outtext("sig?inv");
                else
                    _outtext(wantstring);

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

            if (wrapArounds)
            {
                settextcolor(BRIGHTRED);
                gotoxy(53, STATUS_LINE + 1);
                sprintf(tempArea, "WrapArounds: %lu", wrapArounds);
                _outtext(tempArea);
            }
        }
    }
    else
    {
        tstart = now;
        tprev = tstart;
        pgood = goodcount;
        pbad = badcount;
        QuickOut(STATUS_LINE, 1, GREEN, "Dumper V3.7i 5/9/99 - Hit any key when finished!");
        QuickOut(STATUS_LINE, 53, GREEN, "Status: Please  Acc: Wait %");
    }
}
