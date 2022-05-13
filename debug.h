#include <signal.h>

#include "missing.h"

void lprintf(char const *pfname, char const *fmt, ...);
#define lprintf1(x)     lprintf(__FUNCTION__, x)
#define lprintf2(x, y)   lprintf(__FUNCTION__, x, y)
#define lprintf3(x, y, z) lprintf(__FUNCTION__, x, y, z)
#define lprintf4(w, x, y, z) lprintf(__FUNCTION__, w, x, y, z)
#define lprintf5(v, w, x, y, z) lprintf(__FUNCTION__, v, w, x, y, z)
#define lprintf6(u, v, w, x, y, z) lprintf(__FUNCTION__, u, v, w, x, y, z)
#define lprintf7(t, u, v, w, x, y, z) lprintf(__FUNCTION__, t, u, v, w, x, y, z)

int sleep_ms(unsigned timeMs);
unsigned int mhz2ul(char *info);
void byebye(char *file, int line, char *reason);
#define exit(reason) byebye (__FUNCTION__,__LINE__, reason)

extern unsigned int control_channel;
#define SQUELCH_HANG 500

extern int beeper (unsigned pitch, unsigned ms, unsigned w);

