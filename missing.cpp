#include <stdio.h>
#include <stdlib.h>
#include <missing.h>
#include <string.h>
#include <ctype.h>
#include <ncurses.h>
#include <assert.h>

#include <signal.h>

#include "layouts.h"
#include "debug.h"
#include "layouts.h"

char chatty = 0;

bool validateXY(int y, int x)
{
    if (x > COL80 || y > MAX_ROWS)
    {
        return 0;

        raise(SIGTRAP);
        assert(x < COL80 && y < MAX_ROWS);
    }

    return 1;
}


int _wrapon(int foo)
{
    return 1;
}


#if 0
void shouldNever(const char *cp)
{
    printf("EXITING: %s\n", cp);
    exit("need reason");
}


#endif
void outportb(unsigned addr, unsigned char data)
{
//    printf("outportb[%d]=%d\n", addr, data);
}


void outp(unsigned addr, unsigned data)
{
//    printf("outp[%d]=%d\n", addr, data);
}


static int numrows = MAX_ROWS;
int _setvideomoderows(int mode, int rows)
{
//    printf("_setvideomoderows %d, %d\n", mode, rows);
    numrows = rows;
    return numrows;
}


int _setvideomode(int mode)
{
//    printf("_setvideomode %d\n", mode);
    return 1;
}


void _setbkcolor(int color)
{
//    printf("_setbkcolor %d\n", color);
}


int kbhit(void)
{
    fd_set rfds;
    struct timeval tv;
    int retval, len;
    char buff[255] = { 0 };

    /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);

    tv.tv_sec = 0;
    tv.tv_usec = 100;

    retval = select(1, &rfds, NULL, NULL, &tv);

    if (retval == -1)
        return 0;                                       // gross error
    else if (retval)
        return 1;                                       // have data

    return 0;                                           //
}


static rccoord whereami = { 0 };
rccoord _gettextposition()
{
    return whereami;
}


static unsigned curbkcolor;
extern unsigned short _getbkcolor(void)
{
    return curbkcolor;
}


void  _clearscreen(int unknown)
{
    //printf("_clearscreen %d\n", unknown);
}


void strupr(char *in)
{
    while (in && *in)
    {
        *in = toupper(*in);
        in++;
    }
}


void unlink(char *file)
{
//    printf("unlink %s\n", file);
}


#define MATCH "222"

void stopOnText(char *msg)
{
     //if (strstr(msg, MATCH))  raise(SIGTRAP);
}

void _cprintf(char *file, int line, char *fmt, ...)
{
    char msg[120];
    va_list parm2;
	char msg2[200];

    va_start(parm2, fmt);
    vsprintf(msg, fmt, parm2);

	sprintf(msg2, "%s | %s:%d", msg, file, line);
    if (chatty) lprintf1(msg2);
    addstr(msg2);

    stopOnText(msg2);
    
    va_end(parm2);
    refresh();
}


void _outmem(char *p, int s)
{
    int i;
    int len;
    char msg[COL80];

    if ( s < sizeof(msg))
        len = s;
    else
        len = sizeof(msg);
        
    memcpy(msg, p, len);
    msg[COL79] = 0;
    
    //if (chatty) lprintf2("_outmem %s\n", msg);
    addstr(msg);
    stopOnText(msg);

    refresh();
}


void _settextposition(char *file, int line, int y, int x)
{
    if (x != whereami.col || y != whereami.row)
    {
        if (validateXY(y, x))
        {
            //lprintf5("x=%2d y=%2d %s:%d ", x, y, file, line);
            move(y, x);
            whereami.col = x;
            whereami.row = y;
        }
        else
        {
            //lprintf5("SKIP x=%2d y=%2d %s:%d ", x, y, file, line);
        }
    }
}


int access(char *fn, int status)
{
//    printf("access %s %d\n", fn, status);
    return F_OK;
}

#include <time.h>

int _ctime(time_t now, char *p)
{
    time_t rawtime;
    struct tm *info;
    
    time( &rawtime );
    
    info = localtime( &rawtime );
    strcpy(p, asctime(info));
    
    return 1;
}


static int dontcare;
void *_dos_getvect(int irqnum)
{
//    printf("_dos_getvect %d\n", irqnum);
    return &dontcare;
}


void *_dos_setvect(void *p, int irqnum)
{
//    printf("_dos_setvect %p, %d\n", p, irqnum);
}


int DU_lock_linear(void *p, int size)
{
//    printf("DU_lock_linear %p, %d\n", p, size);
}


void settextcolor(int x)
{
    bool brite = !(x & 0xF8);
    attron(brite ? A_BOLD : A_DIM);
    x &= 0x7;
    //lprintf3("%s %d", brite ? "BRT" : "DIM", x);
    attron( (brite ? A_BOLD : A_DIM) | COLOR_PAIR(x));
}


int _QuickOut(char *file, int line, int y, int x, int color, char *msg)
{
    if (validateXY(y, x))
    {
        move(y, x);
        whereami.col = x;
        whereami.row = y;

        settextcolor(color);
        if (chatty) lprintf7("%d:%d \| [%d]\"%s\" | %s:%d", x, y, strlen(msg), msg, file, line);
        addstr(msg);
        stopOnText(msg);
        refresh();
        return 1;
    }
    else
    {
        //lprintf7("REJECT %d:%d \| [%d]\"%s\" %s:%d", x, y, strlen(msg), msg, file, line);
        assert(_QuickOut == 0);
        return 0;
    }
}


int __outtext(char *file, int line, char *p)
{
    if (chatty) lprintf5("[%d]\"%s\" \| %s:%d", strlen(p), p, file, line);
    addstr(p);
    stopOnText(p);
    refresh();
    return 1;
}


static char textCursor = 0x99;
void settextcursor(int cursor)
{
    textCursor = cursor;
    //printf("settextcursor %d\n", cursor);
}
