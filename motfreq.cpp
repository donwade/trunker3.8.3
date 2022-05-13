#include <platform.h>
#include <osw.h>
#include <trunksys.h>
#include <motfreq.h>
#include "debug.h"

static char myres[14];
/*
 * Input Range (Hex)     800 Plan Range         Splinter Plan Range
 * 000 - 2cf             851.0125 - 868.9875    851.0000 - 868.9750 * pivot = 257 = 865.9875
 * 2d0 - 2f7             866.0000 - 866.9750    866.0000 - 866.9750 * same * split *
 * 32f - 33f             867.0125 - 867.4000    867.0000 - 867.4000 * same * split *
 * 3c1 - 3fe             867.4250 - 868.9500    867.4250 - 868.9500 * same * split *
 * 3be -                 867.9750               867.9750            * same * split *
 *
 * c v856.2250,0d1,ffff   - matches splinter
 * v856.4250,0d9,ffff   - matches splinter
 * v858.2250,121,ffff   - matches splinter
 * v866.4875,26b,ffff   * matches NORMAL
 * v867.4875,293,ffff   * matches NORMAL
 * dv867.9875,2a7,ffff   * matches NORMAL
 * dv868.4875,2bb,ffff
 *
 */

unsigned uhf800BandMapper(unsigned short theCode)   /* trunker code '8' */
{
    if (theCode <= 0x2cf)
        return (851012500 + (25000 * theCode));
    else if (theCode <= 0x2f7)
        return (866000000 + (25000 * (theCode - 0x2d0)));
    else if (theCode >= 0x32f && theCode <= 0x33f)
        return (867000000 + (25000 * (theCode - 0x32f)));
    else if (theCode >= 0x3c1 && theCode <= 0x3fe)
        return (867425000 + (25000 * (theCode - 0x3c1)));
    else if (theCode == 0x3BE)
        return (868975000);
    else
    {
        lprintf2("cannot map code %d", theCode);
        return (theCode);
    }
}

// base 141.015000 offset 15khz base 380
unsigned vhfHiFreqMapper(unsigned short theCode)
{
    unsigned retval = 141015000 + (15000 * (theCode - 380));
    //if (retval < 140000000) raise (SIGTRAP);
    
    return (141015000 + (15000 * (theCode - 380)));
}

// base 421000000 offset 12.5khz base 380
//421.00000 	12.5	501
//412.50000 	12.5	380

unsigned uhf400FreqMapper(unsigned short theCode)
{
	//421.00000 	12.5	501 
	//412.50000 	12.5	380  << need 380?
    return (421000000 + (12500 * (theCode - 501)));
}

unsigned unknownBandMapper(unsigned short theCode)
{
    return vhfHiFreqMapper(theCode);
}

char *mapVhfHiCodeToString(unsigned short theCode)
{
    static char myres[50];

	unsigned frequency;
	frequency = vhfHiFreqMapper(theCode);
	
    sprintf(myres, "%8.4f", frequency/1000000.);
    return myres;
}


char *mapUhfLoCodeToString(unsigned short theCode)
{
    static char myres[50];

    sprintf(myres,"%8.4f",  uhf400FreqMapper(theCode) / 1000000.);
    return myres;
}


char *mapOtherCodeToString(unsigned short theCode)
{
	return mapVhfHiCodeToString(theCode);
}



static inline char *map800CodeToString(unsigned short theCode)   /* trunker code '8' */
{
    
    unsigned freq;
    freq = uhf800BandMapper(theCode);   /* trunker code '8' */
    sprintf(myres, "%8.4f", (double)freq/1000000.);
    return myres;
}


unsigned uhf900BandMapper(unsigned short theCode)   /* trunker code '9' */
{
    return 935012500 + (12500 * theCode);
}

static inline char *map900CodeToString(unsigned short theCode)
{
    sprintf(myres, "%8.4f", uhf900BandMapper(theCode)/1000000.);
    return myres;
}

unsigned mapSysIdCodeToFreq(unsigned short theCode, unsigned short sysId)
{
   switch (sysId)
  {
      case 0x822f:
      case 0x782d:  //toronto
        return vhfHiFreqMapper(theCode);

        case 0xa332:
          return uhf400FreqMapper(theCode);
  }
/*
    case '8': return uhf800BandMapper(theCode);

    case 'S': return uhf800BandMapper(theCode);

    case '9': return uhf900BandMapper(theCode);

    case '1': return vhfHiFreqMapper(theCode);

    case '4': return uhf400FreqMapper(theCode);

    default: return unknownBandMapper(theCode);
    }
*/
    return 0;
}



char *MotorolaMap::mapCodeToF(unsigned short theCode, char bp)
{
    switch (bp)
    {
    case '8': return map800CodeToString(theCode);

    case 'S': return map800CodeToString(theCode);

    case '9': return map900CodeToString(theCode);

    case '1': return mapVhfHiCodeToString(theCode);
	
    case '4': return mapUhfLoCodeToString(theCode);
	
    default: return mapOtherCodeToString(theCode);
    }
}


BOOL MotorolaMap::isFrequency(unsigned short theCode, char bp)
{
    if (bp == '9')
    {
        return (theCode >= 0 && theCode <= 0x1DE) ? TRUE : FALSE;
    }
    else            // all others - fairly permissive - covers any schemes I've heard of...
    {               // doesn't interfere with any of the OSWs I know of... consistent with
        // data sent to me anonymously....
        return ((theCode >= 0 && theCode <= 0x2F7)
                || (theCode >= 0x32f && theCode <= 0x33F)
                || (theCode >= 0x3c1 && theCode <= 0x3FE)
                || (theCode == 0x3BE)) ? TRUE : FALSE;
    }
}
