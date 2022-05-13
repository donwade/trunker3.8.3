#include "filter.h"
#include "layouts.h"
extern time_t now, nowFine;
extern unsigned short idMask[14];
extern unsigned short radioMask[14];

// Global items
#define IS_DATA_CHANNEL(x) (((x) & 0xff00) == 0x1f00)
extern void stats(const char *);
extern void showPrompt(BOOL clearit = TRUE);
extern void doKeyEvt();
extern void setCurrentOptions(const char *);
extern const char *getCurrentOptions();
extern void drawBar(int rowNum, const char *lab);

#include "trunkvar.h"
