#include <platform.h>

BOOL wantScanner = FALSE;

int wordsPerSec = 4800 / (112 + 40);
static BOOL wscreen = FALSE;
static BOOL wfile   = FALSE;

time_t now,  nowFine;
long theDelay;
/* NOTE: see important comments that were moved to 'comment.c' */
/* global variables  setvbuf */
static FILE *out;
static int lc = 0;
#define OUTBUFSIZE 1000
static char outBuf[OUTBUFSIZE];     /* output buffer for packet before being sent to screen */
static int outBufPos = 0;           /* pointer to current position in array outBuf          */

#define NBYTESFORSTDIO 5120
static char niceBuffer[NBYTESFORSTDIO];
int pork(int l)
{
	static int s = 0, sl = 0x0000, t1, asp = 0, ll, k, v, b, tap, synd = 0, nsy;
	static char line[200];          /* array used to format 112 bit data chunks */
	static int lc = 0;              /* pointer to position in array line        */

	if (l == -1)
	{
/*    printf ("  %2i\n",asp); */
		sl = 0x0000;
		s = 0;
		synd = 0;
		if ((asp < 12) && (lc > 50))
		{
			ll = 12 - asp;
			for (ll = 0; ll < 6; ll++)
			{
				v = 0;
				for (k = 7; k >= 0; k--)
				{
					b = line[ (ll << 3) + k ];
					v = v << 1;
					if ( b)
						v++;
				}
				outBuf[outBufPos] = (char)v;
				if (outBufPos < (OUTBUFSIZE - 1))
					outBufPos++;
			}
		}
		lc = 0;
		tap = asp;
		asp = 0;
		return tap;
	}
	else
	{
		s++;
		if (s == 1)
		{
			line[lc] = (char)l;
			lc++;
		}

		/* update sliding buffer */
		sl = sl << 1;
		sl = sl & 0x7fff;
		if (l)
			sl++;

		if (s > 1)
		{
			s = 0;

			if ((sl & 0x2000) > 0)
				t1 = 1;
			else
				t1 = 0;
			if ((sl & 0x0800) > 0)
				t1 ^= 1;
			if ((sl & 0x0020) > 0)
				t1 ^= 1;
			if ((sl & 0x0002) > 0)
				t1 ^= 1;
			if ((sl & 0x0001) > 0)
				t1 ^= 1;

			/* attempt to identify, correct certain erroneous bits */

			synd = synd << 1;
			if (t1 == 0)
			{
				asp++;
				synd++;
			}
			nsy = 0;
			/* Corrected per arron5@geocities.com 02/08/97 */
			if ( (synd & 0x0001) > 0)
				nsy++;
			if ( (synd & 0x0002) > 0)
				nsy++;
			if ( (synd & 0x0010) > 0)
				nsy++;
			if ( (synd & 0x0040) > 0)
				nsy++;
			if ( nsy >= 3)                           /* assume bit is correctable */
			{
				putchar('*');
				synd = synd ^ 0x65;
				line[lc - 7] ^= 0x01;                                   /**********************************************/
			}


		}

	}
	return 0;
}

void shob()
{
	int i1, i2, j1, j2, k1;

	/* update file output buffer */
	i1 = outBufPos / 18;
	if ( (outBufPos - (i1 * 18)) > 0)
		i1++;
	for (i2 = outBufPos; i2 <= (outBufPos + 20); i2++)
		outBuf[i2] = 0;
	putchar('\n');
	fputc('\n', out);
	for (j1 = 0; j1 < i1; j1++)
	{
		for (j2 = 0; j2 < 18; j2++)
		{
			k1 = j2 + (j1 * 18);
			if ( !wfile )
				fprintf(out, "%02X ", outBuf[k1] & 0xff);
			if ( !wscreen)
				printf("%02X ", outBuf[k1] & 0xff);
		}
		if ( !wscreen )
			printf("    ");
		if ( !wfile )
			fprintf(out, "    ");
		for (j2 = 0; j2 < 18; j2++)
		{
			k1 = j2 + (j1 * 18);
			if (outBuf[k1] >= 32 && outBuf[k1] <= 0x7f )
			{
				putchar(outBuf[k1]);
				putc(outBuf[k1], out);
			}
			else if ( outBuf[k1] == '\r' || outBuf[k1] == '\n' )
			{
				if (wscreen)
					putchar('\n');
				else
					putchar('.');
				putc('.', out);
			}
			else
			{
				if (!wscreen)
					putchar('.');
				putc('.', out);
			}
		}
		if ( !wscreen)
			putchar('\n');
		if ( !wfile )
			putc('\n', out);
	}

	outBufPos = 0;

}

/**********************************************************************/
/*                      frame_sync                                    */
/**********************************************************************/
/* this routine recieves the raw bit stream and tries to decode it    */
/* the first step is frame synchronization - a particular 40 bit      */
/* long sequence indicates the start of each data frame. Data frames  */
/* are always 112 bits long. After each 112 bit chunk this routine    */
/* tries to see if the message is finished (assumption - it's finished*/
/* if the 40 bit frame sync sequence is detected right after the end  */
/* of the 112 bit data chunk). This routine also goes back to hunting */
/* for the frame sync when the routine processing the 112 bit data    */
/* chunk decides there are too many errors (transmitter stopped or    */
/* bit detection routine skipped or gained an extra bit).             */
/*                                                                    */
/* inputs are fed to this routine one bit at a time                   */

inline void
frame_sync(char gin)
{
	static unsigned int s1 = 0, s2 = 0, s3 = 0, nm, j, t1, t2, t3, ns = 0, st = 0, n, m, l, chu = 0, eef = 0;
	static char ta[200];

	if (st == 1)
	{
		ta[ns] = gin;
		ns++;
		if (ns >= 112)                   /* process 112 bit chunk */
		{
			chu++;
			ns = 0;
			for (n = 0; n < 16; n++)
			{
				for (m = 0; m < 7; m++)
				{
					l = n + (m << 4);
					pork(ta[l]);
				}
			}
			if (pork(-1) > 20)
				eef++;
			else
				eef = 0;
			if (eef > 1)                           /* if two consecutive excess error chunks - bye  */
			{
				st = 0;
				shob();
				eef = 0;
			}
/*      else eef = 0; */
		}
	}

	/* s1,s2,s3 represent a 40 bit long buffer */
	s1 = (s1 << 1) & 0xff;
	if ((s2 & 0x8000) > 0)
		s1++;
	s2 = (s2 << 1);
	if ((s3 & 0x8000) > 0)
		s2++;
	s3 = (s3 << 1);
	if (gin)
		s3++;

	/* xor with 40 bit long sync word */
	t1 = s1 ^ 0x0007;
	t2 = s2 ^ 0x092A;
	t3 = s3 ^ 0x446F;

	/* find how many bits match up */
	/* currently : the frame sync indicates system id / idling / whatever */
	/*        inverted frame sync indicates message follows             */
	nm = 0;
	for (j = 0; j < 16; j++)
	{
		if (t1 & 1)
			nm++;
		if (t2 & 1)
			nm++;
		if (t3 & 1)
			nm++;
		t1 = t1 >> 1;
		t2 = t2 >> 1;
		t3 = t3 >> 1;
	}

	if (nm <  5)
	{
		st = 1;
		ns = 0;
	}
	else if (nm > 35)
	{
		if (st == 1)
			shob();
		st = 0;
		ns = 0;
	}
}
void
localCleanup()
{
	if ( out )
		fclose(out);
}

int
mainLoop()
{
	register unsigned int i = 0;
	char s = 0;
	register double dt, exc = 0.0, clk = 0.0, xct;
	char fn[15];
	struct tm *ht;
	const char *ep;

	ep = getenv("MDTWIDESCREEN");
	if ( ep && (*ep == 'y' || *ep == 'Y') )
		wscreen = TRUE;

	ep = getenv("MDTWIDEFILE");
	if ( ep && (*ep == 'y' || *ep == 'Y') )
		wfile = TRUE;
	now = time(0);

	ht = localtime(&now);
	sprintf(fn, "%.2d%.2d%.2d%.2d.txt",
	        ht->tm_mon + 1,
	        ht->tm_mday,
	        ht->tm_hour,
	        ht->tm_min);
	out = fopen(fn, "wt");
	if (out == NULL)
	{
		printf("couldn't open output file.\n");
		exit(1);
	}
	printf("MDT (trunker platform) Beta 22 Feb 1998\n");
	printf("output in %s\n", fn);
	setvbuf(out, niceBuffer, _IOFBF, sizeof(niceBuffer));

	/* dt is the number of expected clock ticks per bit */
	dt =  1.0 / (4800.0 * 838.8e-9);

	while (kbhit() == 0)
	{
		if (i != cpstn)
		{
			if (slicerInverting)
				s = (fdata [i] & 0x8000) ? 0 : 1;
			else
				s = (fdata [i] & 0x8000) ? 1 : 0;

			/* add in new number of cycles to clock  */
			clk += (fdata[i] & 0x7fff);
			xct = exc + 0.5 * dt;                         /* exc is current boundary */
			while ( clk >= xct )
			{
				frame_sync(s);
				clk = clk - dt;
			}
			/* clk now holds new boundary position. update exc slowly... */
			/* 0.005 sucks; 0.02 better; 0.06 mayber even better; 0.05 seems pretty good */
			if ( s )
				exc = exc + 0.020 * (clk - exc);

			if ( ++i > BufLen)
				i = 0;
		}

	}
	exit(0);
	return 0;
}
