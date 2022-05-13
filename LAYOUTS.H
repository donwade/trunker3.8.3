/* screen positions for trunker
*/
// column for priority field
#define PRTYSTART ((short)12)
// column for details area
#define DETSTART ((short)15)
#define MAXFREQS 28

// these #defines are used for computing screen coordinates

#define ROWAREA 80 
#define ACTUALROWS 43
#define STATROW ((short)1)
#define FREQROW (STATROW+2)
#define PROMPTBAR (ACTUALROWS-2)
#define PROMPTROW (PROMPTBAR+1)
#define FBROW (PROMPTROW+1)
#define PAGELOGBOTTOM (PROMPTBAR-1)
#define PAGELOGBAR (FREQROW + 3 + MAXFREQS+1)
#define PAGELOGTOP (PAGELOGBAR + 1)
#define PAGELINES  (1+(PAGELOGBOTTOM-PAGELOGTOP))
