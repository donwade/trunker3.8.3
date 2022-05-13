#ifndef __MISSING_H__
#define __MISSING_H__
#include <time.h>
#include <ncurses.h>

#define _GWRAPOFF 0

#define _GWRAPON 1
#define DEFAULTMODE 1
#define _DEFAULTMODE 1
#define _GCLEARSCREEN 1

#define __far

extern WINDOW *stdscr;

extern int _outtext(char *p);
extern int _wrapon(int);
void outportb(unsigned addr, unsigned char data);
void outp(unsigned addr, unsigned data);
extern void shouldNever(const char *cp);
extern int _setvideomoderows(int mode, int cols);
extern void _setbkcolor(int color);
extern int kbhit(void);
extern void strupr(char *);
extern _setvideomode(int mode);

typedef struct
{
    int row;
    int col;
} rccoord;

extern void _settextposition(char *fn_name, int lineno, int row, int col);
#define settextposition(row, col) _settextposition(__FILE__, __LINE__, row, col)

extern int __outtext(char *file, int line, char *msg);
#define _outtext(msg) __outtext(__FILE__, __LINE__, msg)

extern int _QuickOut(char *file, int line, int y, int x, int color, char *msg);
#define QuickOut(y, x, color, msg) _QuickOut(__FILE__, __LINE__, y, x, color, msg)


extern void settextcursor(int cursor);
extern void settextcolor(int color);

extern rccoord  _gettextposition(void);
extern unsigned short _getbkcolor(void);

extern void  _clearscreen(int unknown);
extern void unlink(char *file);
extern void _outmem(char *p, int s);

extern int access(char *fn, int status);
#define F_OK 1

extern _ctime(time_t now, char *P);

extern void *_dos_getvect(int irqnum);
extern void *_dos_setvect(void *, int irqnum);

extern int DU_lock_linear(void *p, int size);

#define cprintf(x,...) _cprintf(__FILE__, __LINE__, x, __VA_ARGS__)
extern void _cprintf(char *file, int line, char *fmt, ...);

#endif  //__MISSING_H__
