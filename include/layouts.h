/* screen positions for trunker
 */
// column for priority field
#define PRTYSTART ((short)12)
// column for details area
#define DETSTART ((short)15)
#define MAXFREQS 28

// these #defines are used for computing screen coordinates

#define COL80 80
#define COL79 (COL80-1)

#define MAX_ROWS 38

#define STATUS_LINE             1
#define FREQ_LINE               (STATUS_LINE + 2) ///3
#define SUMMARY_LINE            6
#define PAGELOGBAR              (FREQ_LINE + 3 + MAXFREQS + 1)
#define PAGELOGTOP              (PAGELOGBAR + 1)

#define DASHED_LINE     (MAX_ROWS - 2)      //36
#define PAGELOGBOTTOM   (DASHED_LINE - 2)   //34
#define PROMPT_LINE     (DASHED_LINE + 1)   //36
#define FBROW           (PROMPT_LINE + 1)   //37

#define PAGELINES               (1 + (PAGELOGBOTTOM - PAGELOGTOP))      //1+35-
