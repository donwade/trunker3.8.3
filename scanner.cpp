#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <missing.h>
#include <ptrmap.h>
#include <trunkvar.h>
#include <scanner.h>
#include <freqasg.h>
#include <platform.h>
#include <layouts.h>
#include <unistd.h>
#include "debug.h"

#pragma  warning(once:4309)

int init_telnet(char *hostname, unsigned port);
int send_telnet(const char *buffer, size_t size);
void close_telnet(void);
char *recieve_telnet(void);
extern int squelchHangTime;

unsigned control_channel = 101600000;

Scanner::Scanner () : lp(0), canPark(FALSE), myPort(0), volume(130), squelch(80)
{
}


void Scanner::setComPort(ComPort *comport, double baudrate)
{
    myPort = comport;
    myPort->setBaud(baudrate);
}


void Scanner::goPark()
{
    if (canPark)
    {
        //if(this->parking.scancmdlen) {
        if (this->scancmdlen)
            putStringN(this->scancmdlen, this->parking.scancmd);
        else
            putString(this->parking.scancmd);
    }

    lp = 0;
}


void Scanner::topIs(channelDescriptor *fp)
{
    static channelDescriptor keep;                                 //new dwade

    if (fp != lp)
    {
        if (lp)
            QuickOut(keep.y, keep.x + PRTYSTART - 1, color_quiet, (const char *)" ");

        if (fp && (fp->scancmd [0] || fp->scancmdlen))
        {
            QuickOut(fp->y, fp->x + PRTYSTART - 1, color_quiet, "*");

            if (fp->scancmdlen)
                putStringN(fp->scancmdlen, fp->scancmd);
            else
                putString(fp->scancmd);
        }

        if (!fp)
        {
            goPark();                                                                     // clears lp
        }
        else
        {
            lp = fp;
            keep = *fp;
        }
    }
}


PCR1000::PCR1000 (ComPort *comport, double baudrate) : Scanner()
{
    Scanner::setComPort(comport, baudrate);

    comport->setBaud(9600.0);
    putString("H101\n");        // set power on

    if (baudrate == 300.0)
    {
        putString("G100\n");
    }
    else if (baudrate == 1200.0)
    {
        putString("G101\n");
    }
    else if (baudrate == 4800.0)
    {
        putString("G102\n");
    }
    else if (baudrate == 9600.0)
    {
        putString("G103\n");
    }
    else if (baudrate == 19200.0)
    {
        putString("G104\n");
    }
    else if (baudrate == 38400.0)
    {
        putString("G105\n");
    }
    else
    {
        baudrate = 38400.0;
        putString("G105\n");
    }

    comport->setBaud(baudrate);
    putString("G300\n");        // set return data off
    putString("J4501\n");       // set AGC on
    putString("J4600\n");       // set NB off
    putString("J4700\n");       // set attenuator off
    putString("J5100\n");       // set tone squelch off

    setVolume(volume);
    setSquelch(squelch);
}


void PCR1000::shutDown(void)
{
    putString("H100\n");        // turn power off
}


void PCR1000::setFreqString(channelDescriptor *f, char *fname, int line)
{
    f->scancmd [0] = 0;                                         // unknown mapping
    f->scancmdlen = 0;                                          // double-whammy

    if (strstr(f->freqdisp, "."))
    {
        f->scancmd [0] = 'K';   // K
        f->scancmd [1] = '0';   // 0
        f->scancmd [2] = '0';   // 0
        f->scancmd [3] = f->freqdisp [3];       // 8
        f->scancmd [4] = f->freqdisp [4];       // x
        f->scancmd [5] = f->freqdisp [5];       // x
        f->scancmd [6] = f->freqdisp [7];       // f
        f->scancmd [7] = f->freqdisp [8];       // f
        f->scancmd [8] = f->freqdisp [9];       // f
        f->scancmd [9] = f->freqdisp [10];      // f
        f->scancmd [10] = '0';  // 0
        f->scancmd [11] = '0';  // 0
        f->scancmd [12] = '0';  // 0
        f->scancmd [13] = '5';  // 5
        f->scancmd [14] = '0';  // 0
        f->scancmd [15] = '2';  // 2
        f->scancmd [16] = '0';  // 0
        f->scancmd [17] = '0';  // 0
        f->scancmd [18] = '\n'; // 0
        f->scancmd [19] = '\0';
    }
}


void PCR1000::setVolume(int volume)
{
    char volumeCmd [32];

    sprintf(volumeCmd, "J40%02x\n", volume & 0xff);
    putString(volumeCmd);
}


void PCR1000::setSquelch(int squelch)
{
    char squelchCmd [32];

    sprintf(squelchCmd, "J41%02x\n", squelch & 0xff);
    putString(squelchCmd);
}


GQRX::GQRX (ComPort *comport, double baudrate) : Scanner()
{
    setComPort((unsigned)comport->getPort());
}


void GQRX::setVolume(int volume)
{
    char msg[50];
    char *pResp;

    if (volume == lastVolume) return;
    lastVolume = volume;
    
#if 0
    sprintf(msg, "l VOL \n");
    send_telnet(msg, strlen(msg));

    pResp = recieve_telnet();

    if (pResp)
    {
        strcpy(msg, pResp);
        char *pcrlf = strstr(msg, "\n");

        if (pcrlf) *pcrlf = 0;

        lprintf2("current volume is %f", atof(msg));
    }

#endif

    sprintf(msg, "L VOL %d.0\n", volume);
    if (send_telnet(msg, strlen(msg)) < 0)
    {
        exit("can't set volume");
    }

    pResp = recieve_telnet();

    if (pResp)
    {
        strcpy(msg, pResp);
        lprintf2("set volume to %d.0", volume);
    }
    else
    {
        lprintf1("no repsonse");
    }

    return;
}

void GQRX::setBandWidth(int width)   //DOES THIS COMMAND WORK?
{
    char msg[50];
    sprintf(msg, "M FM %d", width);  // set mode + width
    send_telnet(msg, strlen(msg));
    strcpy(msg, recieve_telnet());
    
    lprintf2("setB/W to %d", width);
    return;
}

#define CAL_STEPSIZE 1000

float GQRX::runCal(unsigned freq)
{
    char msg[100];
    int i, j;

    float snr_reading = 0.0;
	unsigned low_floor_freq = freq;
	
    noise_floor = 0.0;

    setVolume(-80);
    setBandWidth(CAL_STEPSIZE);

    for (i = 0; i < 10; i++)
    {
        for (j = -1; j < +2; j++)
        {
        	if (j == 0) continue;
        	
			unsigned tgt = freq + (CAL_STEPSIZE)*j * i;
			setFrequency(tgt, __FUNCTION__, __LINE__);
			sleep_ms(400);
			
            snr_reading = getSNR();
            if (snr_reading == 0 ) continue; //bogus
            
            lprintf4("%d Mhz = %4.2f db floor = %4.2f db", tgt, snr_reading, noise_floor);

            if (signal_peak < snr_reading) signal_peak = snr_reading;
            if (noise_floor > snr_reading) 
            {
            	low_floor_freq = tgt;
            	noise_floor = snr_reading;
            }

        }
    }

    float SNR = signal_peak - noise_floor;
    
    squelch_level = noise_floor + SNR - 3;

    lprintf3("hi/low = %f/%f", signal_peak, noise_floor);
    lprintf3("control chan SNR= %f squelch @ %f", SNR, squelch_level);

    // show lowest noise floor 
    setFrequency(low_floor_freq, __FUNCTION__, __LINE__);
	sleep(3);
	
    GQRX::setSquelch((int)(squelch_level));
    setBandWidth(BW_P25CNTL);

    setFrequency(freq, __FUNCTION__, __LINE__);
    if (SNR < 9) 
    {
    	lprintf2("SNR value too low to continue at %4.1f db", SNR);
    	exit("SNR value too low");
    }
    return SNR;
    
}

void GQRX::setSquelch(int squelch)
{
    char msg[50];

    if (lastSquelch == squelch) return;
    lastSquelch = squelch;

#if 0 // get current squelch 
    sprintf(msg, "l SQL \n");
    send_telnet(msg, strlen(msg));
    strcpy(msg, recieve_telnet());

    char *pcrlf = strstr(msg, "\n");

    if (pcrlf) *pcrlf = 0;

    //lprintf2("current squelch to %f", atof(msg));
#endif

    sprintf(msg, "L SQL %d.0\n", squelch);
    send_telnet(msg, strlen(msg));
    strcpy(msg, recieve_telnet());
    
    lprintf2("set squelch to %d.0", squelch);

    return;
}


void GQRX::shutDown(void)
{
    close_telnet();
    return;
}


void GQRX::putString(const char *message)
{
    send_telnet(message, strlen(message));
}


int GQRX::putStringN(const int len, const char *message)
{
    char msg[2];

    msg[1] = '\0';

    while (*message)
    {
        msg[0] = *message;
        send_telnet(msg, 1);
        message++;
    }

    return len;
}


int GQRX::setComPort(unsigned port)
{
    init_telnet("localhost", port);
    return 1;
}


float GQRX::getSNR(void)
{
    float ret;
    char strength[100];
    char msg[100];

    strcpy(msg, "l STRENGTH\n");        // get power level
    send_telnet(msg, strlen(msg));

    strcpy(strength, recieve_telnet());
    char *p = strstr(strength, "\n");

    if (p)
        *p = 0;

    ret = atof(strength);
    return ret;
}
    
#define GROSS_FREQ_ERROR 135000000

#include <signal.h>
#include <assert.h>

static bool virgin = 1;
int GQRX::setFrequency(unsigned frequency, char *fname, int line)
{
    char msg[50];
    char bMobileFound = 0;

//    bMobileFound = !!(frequency < 140000000);

//	if (frequency < 140000000)
//	{
//			raise(SIGTRAP);
//			assert(frequency < 140000000);
//	}

    if (!frequency  || frequency < GROSS_FREQ_ERROR)
    {
        lprintf2("gross ERROR frequency change to %d ignored", frequency);
        return 0;
    }

    if (frequency == control_channel)
    {
        if (lastFrequency != frequency)
        {
            setVolume(-80);
            setSquelch(-120);
            setBandWidth(BW_P25CNTL);

            sprintf(msg, "F %d\n", frequency);
            send_telnet(msg, strlen(msg));
            strcpy(msg, recieve_telnet());

            lprintf4("back2ctl frequency %d,  %s:%d", frequency, fname, line);
        }
        return 1;
    }
    
    else 
    {
        if (uplinksOnly)
        {
        	if (!bMobileFound)
        	{
            	lprintf4("%s:%d ignoring frequency change to %d ", fname, line, frequency);
            	return 0;
        	}
        }
	
        if (lastFrequency == frequency) 
        {
            //lprintf2("ignore repeat freq request %d ", frequency);
            squelchHangTime = SQUELCH_HANG;
            return 1;
        }
        
        lastFrequency = frequency;
        
        setSquelch(squelch_level);
        setVolume(24);

        squelchHangTime = SQUELCH_HANG;
        sprintf(msg, "F %d\n", frequency);
        send_telnet(msg, strlen(msg));
        strcpy(msg, recieve_telnet());

        lprintf4("moving to frequency %d,  %s:%d", frequency, fname, line);

        return 1;
    }
}

void GQRX::setFreqString(channelDescriptor *f, char* fname, int line)
{
    int freq = (int)(atof(&f->freqdisp[3]) * 1000000.0);
    setFrequency(freq, fname, line);
}


R10::R10 (ComPort *comport, double baudrate) : Scanner()
{
    char Init [] = { 0xfe, 0xfe, 0x52, 0xe0, 0x06, 0x05, 0x02, 0xfd };

    Scanner::setComPort(comport, baudrate);

    putStringN(8, Init);
}


void R10::setFreqString(channelDescriptor *f, char *fname, int line)
{
    f->scancmd [0] = 0;                                                                                         // unknown mapping
    f->scancmdlen = 0;                                                                                          // double-whammy

    if (strstr(f->freqdisp, "."))
    {
        f->scancmd [0] = 0xfe;                                                                                  // Preamble
        f->scancmd [1] = 0xfe;                                                                                  // Preamble
        f->scancmd [2] = 0x52;                                                                                  // Radio type (R10)
        f->scancmd [3] = 0xe0;                                                                                  // Transmit address (controller)
        f->scancmd [4] = 0x05;                                                                                  // Write operating frequency
        f->scancmd [5] = 0x00;                                                                                  // 10Hz/1Hz
        f->scancmd [6] = (char)(((f->freqdisp [9] & 0x0f) << 4) | (f->freqdisp [10] & 0x0f));   // 1Khz/100Hz
        f->scancmd [7] = (char)(((f->freqdisp [7] & 0x0f) << 4) | (f->freqdisp [8] & 0x0f));    // 100Khz / 10Khz
        f->scancmd [8] = (char)(((f->freqdisp [4] & 0x0f) << 4) | (f->freqdisp [5] & 0x0f));    // 10Mhz/1Mhz
        f->scancmd [9] = (char)(f->freqdisp [3] & 0x0f);                        // 100's Mhz
        f->scancmd [10] = 0xfd;                                                                                 // Terminator

        //
        //  Setting this to a non-zero means treat it as a counted string
        //
        f->scancmdlen = 11;
    }
}


OPTO::OPTO (ComPort *comport, double baudrate) : Scanner()
{
    char Init [] = { 0xfe, 0xfe, 0x80, 0xe0, 0x7F, 0x02, 0xfd };        //FE  FE  80  E0  7F  02  FD

    char Init2[] = { 0xfe, 0xfe, 0x80, 0xe0, 0x01, 0x05, 0xfd };        //FE  FE  80  E0  01  05  FD

    Scanner::setComPort(comport, baudrate);

    putStringN(7, Init);        // remote mode
    putStringN(7, Init2);       // narrow FM
}


void OPTO::setFreqString(channelDescriptor *f, char *fname, int line)
{
    f->scancmd [0] = 0;                                                                                         // unknown mapping
    f->scancmdlen = 0;                                                                                          // double-whammy

    if (strstr(f->freqdisp, "."))
    {
        f->scancmd [0] = 0xfe;                                                                                  // Preamble
        f->scancmd [1] = 0xfe;                                                                                  // Preamble
        f->scancmd [2] = 0x80;                                                                                  // Radio type (OPTO)
        f->scancmd [3] = 0xe0;                                                                                  // Transmit address (controller)
        f->scancmd [4] = 0x00;                                                                                  // xfer operating frequency
        f->scancmd [5] = 0x00;                                                                                  // 10Hz/1Hz
        f->scancmd [6] = (char)(((f->freqdisp [9] & 0x0f) << 4) | (f->freqdisp [10] & 0x0f));   // 1Khz/100Hz
        f->scancmd [7] = (char)(((f->freqdisp [7] & 0x0f) << 4) | (f->freqdisp [8] & 0x0f));    // 100Khz / 10Khz
        f->scancmd [8] = (char)(((f->freqdisp [4] & 0x0f) << 4) | (f->freqdisp [5] & 0x0f));    // 10Mhz/1Mhz
        f->scancmd [9] = (char)(f->freqdisp [3] & 0x0f);                        // 100's Mhz
        f->scancmd [10] = 0xfd;                                                                                 // Terminator

        //
        //  Setting this to a non-zero means treated it as a counted string
        //
        f->scancmdlen = 11;
    }
}


void OPTOCOM::shutDown(void)
{
    char final[] = { 0xfe, 0xfe, 0x80, 0xe0, 0x7f, 0x18, 0x01, 0xfd };

    putStringN(8, final);      // auto-scan-mode
}


OPTOCOM::OPTOCOM (ComPort *comport, double baudrate) : Scanner()
{
    char Init2 [] = { 0xfe, 0xfe, 0x80, 0xe0, 0x7f, 0x18, 0x00, 0xfd };
    char Init3 [] = { 0xfe, 0xfe, 0x80, 0xe0, 0x01, 0x05, 0xfd };

    Scanner::setComPort(comport, baudrate);

    putStringN(8, Init2);
    putStringN(7, Init3);
}


void OPTOCOM::setFreqString(channelDescriptor *f, char *fname, int line)
{
    f->scancmd [0] = 0;                                                                                         // unknown mapping
    f->scancmdlen = 0;                                                                                          // double-whammy

    if (strstr(f->freqdisp, "."))
    {
        f->scancmd [0] = 0xfe;                                                                                  // Preamble
        f->scancmd [1] = 0xfe;                                                                                  // Preamble
        f->scancmd [2] = 0x80;                                                                                  // Radio type (OPTO)
        f->scancmd [3] = 0xe0;                                                                                  // Transmit address (controller)
        f->scancmd [4] = 0x00;                                                                                  // xfer operating frequency
        f->scancmd [5] = 0x00;                                                                                  // 10Hz/1Hz
        f->scancmd [6] = (char)(((f->freqdisp [9] & 0x0f) << 4) | (f->freqdisp [10] & 0x0f));   // 1Khz/100Hz
        f->scancmd [7] = (char)(((f->freqdisp [7] & 0x0f) << 4) | (f->freqdisp [8] & 0x0f));    // 100Khz / 10Khz
        f->scancmd [8] = (char)(((f->freqdisp [4] & 0x0f) << 4) | (f->freqdisp [5] & 0x0f));    // 10Mhz/1Mhz
        f->scancmd [9] = (char)(f->freqdisp [3] & 0x0f);                        // 100's Mhz
        f->scancmd [10] = 0xfd;                                                                                 // Terminator

        //
        //  Setting this to a non-zero means treated it as a counted string
        //
        f->scancmdlen = 11;
    }
}


void OPTO::shutDown(void)
{
    char final[] = { 0xfe, 0xfe, 0x80, 0xe0, 0x7f, 0x01, 0xfd };        // FE  FE  80  E0  7F  01  FD

    putStringN(7, final);                                               // front panel mode
}


R8500::R8500 (ComPort *comport, double baudrate) : Scanner()
{
    char Init [] = { 0xfe, 0xfe, 0x4a, 0xe0, 0x06, 0x05, 0x02, 0xfd };

    Scanner::setComPort(comport, baudrate);

    putStringN(8, Init);
}


void R8500::setFreqString(channelDescriptor *f, char *fname, int line)
{
    f->scancmd [0] = 0;                                                                                         // unknown mapping
    f->scancmdlen = 0;                                                                                          // double-whammy

    if (strstr(f->freqdisp, "."))
    {
        f->scancmd [0] = 0xfe;                                                                                  // Preamble
        f->scancmd [1] = 0xfe;                                                                                  // Preamble
        f->scancmd [2] = 0x4a;                                                                                  // Radio type (R8500)
        f->scancmd [3] = 0xe0;                                                                                  // Transmit address (controller)
        f->scancmd [4] = 0x05;                                                                                  // Write operating frequency
        f->scancmd [5] = 0x00;                                                                                  // 10Hz/1Hz
        f->scancmd [6] = (char)(((f->freqdisp [9] & 0x0f) << 4) | (f->freqdisp [10] & 0x0f));   // 1Khz/100Hz
        f->scancmd [7] = (char)(((f->freqdisp [7] & 0x0f) << 4) | (f->freqdisp [8] & 0x0f));    // 100Khz / 10Khz
        f->scancmd [8] = (char)(((f->freqdisp [4] & 0x0f) << 4) | (f->freqdisp [5] & 0x0f));    // 10Mhz/1Mhz
        f->scancmd [9] = (char)(f->freqdisp [3] & 0x0f);                        // 100's Mhz
        f->scancmd [10] = 0xfd;                                                                                 // Terminator

        //
        //  Setting this to a non-zero means treated it as a counted string
        //
        f->scancmdlen = 11;
    }
}


R7100::R7100 (ComPort *comport, double baudrate) : Scanner()
{
    char Init [] = { 0xfe, 0xfe, 0x34, 0xe0, 0x06, 0x05, 0x02, 0xfd };

    Scanner::setComPort(comport, baudrate);

    putStringN(8, Init);
}


void R7100::setFreqString(channelDescriptor *f, char *fname, int line)
{
    f->scancmd [0] = 0;                                                                                         // unknown mapping
    f->scancmdlen = 0;                                                                                          // double-whammy

    if (strstr(f->freqdisp, "."))
    {
        f->scancmd [0] = 0xfe;                                                                                  // Preamble
        f->scancmd [1] = 0xfe;                                                                                  // Preamble
        f->scancmd [2] = 0x34;                                                                                  // Radio type (R7100)
        f->scancmd [3] = 0xe0;                                                                                  // Transmit address (controller)
        f->scancmd [4] = 0x05;                                                                                  // Write operating frequency
        f->scancmd [5] = 0x00;                                                                                  // 10Hz/1Hz
        f->scancmd [6] = (char)(((f->freqdisp [9] & 0x0f) << 4) | (f->freqdisp [10] & 0x0f));   // 1Khz/100Hz
        f->scancmd [7] = (char)(((f->freqdisp [7] & 0x0f) << 4) | (f->freqdisp [8] & 0x0f));    // 100Khz / 10Khz
        f->scancmd [8] = (char)(((f->freqdisp [4] & 0x0f) << 4) | (f->freqdisp [5] & 0x0f));    // 10Mhz/1Mhz
        f->scancmd [9] = (char)(f->freqdisp [3] & 0x0f);                        // 100's Mhz
        f->scancmd [10] = 0xfd;                                                                                 // Terminator

        //
        //  Setting this to a non-zero means treated it as a counted string
        //
        f->scancmdlen = 11;
    }
}


R7000::R7000 (ComPort *comport, double baudrate) : Scanner()
{
    char Init [] = { 0xfe, 0xfe, 0x08, 0xe0, 0x06, 0x05, 0x02, 0xfd };

    Scanner::setComPort(comport, baudrate);

    putStringN(8, Init);
}


void R7000::setFreqString(channelDescriptor *f, char *fname, int line)
{
    f->scancmd [0] = 0;                                                                                         // unknown mapping
    f->scancmdlen = 0;                                                                                          // double-whammy

    if (strstr(f->freqdisp, "."))
    {
        f->scancmd [0] = 0xfe;                                                                                  // Preamble
        f->scancmd [1] = 0xfe;                                                                                  // Preamble
        f->scancmd [2] = 0x08;                                                                                  // Radio type (R7000)
        f->scancmd [3] = 0xe0;                                                                                  // Transmit address (controller)
        f->scancmd [4] = 0x05;                                                                                  // Write operating frequency
        f->scancmd [5] = 0x00;                                                                                  // 10Hz/1Hz
        f->scancmd [6] = (char)(((f->freqdisp [9] & 0x0f) << 4) | (f->freqdisp [10] & 0x0f));   // 1Khz/100Hz
        f->scancmd [7] = (char)(((f->freqdisp [7] & 0x0f) << 4) | (f->freqdisp [8] & 0x0f));    // 100Khz / 10Khz
        f->scancmd [8] = (char)(((f->freqdisp [4] & 0x0f) << 4) | (f->freqdisp [5] & 0x0f));    // 10Mhz/1Mhz
        f->scancmd [9] = (char)(f->freqdisp [3] & 0x0f);                        // 100's Mhz
        f->scancmd [10] = 0xfd;                                                                                 // Terminator

        //
        //  Setting this to a non-zero means treated it as a counted string
        //
        f->scancmdlen = 11;
    }
}


FRG9600::FRG9600 (ComPort *comport, double baudrate) : Scanner()
{
    Scanner::setComPort(comport, baudrate);
}


void FRG9600::setFreqString(channelDescriptor *f, char *fname, int line)
{
    f->scancmd [0] = 0;                                                                         // unknown mapping
    f->scancmdlen = 0;                                                                          // double-whammy

    if (strstr(f->freqdisp, "."))       // 0123456789a
    {                                                                                           // cdv851.0125
        f->scancmd [0] = 0x0A;                                                                  // cmd = 0x0a - set freq
        f->scancmd [1] = (char)(((f->freqdisp [3] & 0x0f) << 4) | (f->freqdisp [4] & 0x0f));    // 100Mhz/10Mhz
        f->scancmd [2] = (char)(((f->freqdisp [5] & 0x0f) << 4) | (f->freqdisp [7] & 0x0f));    // 1Mhz/100khz
        f->scancmd [3] = (char)(((f->freqdisp [8] & 0x0f) << 4) | (f->freqdisp [9] & 0x0f));    // 10khz 1khz
        f->scancmd [4] = (char)(((f->freqdisp [10] & 0x0f) << 4));      // 100hz 10hz

        //
        //  Setting this to a non-zero means treated it as a counted string
        //
        f->scancmdlen = 5;
    }
}


AORScanner::AORScanner (ComPort *comport, double baudrate) : Scanner()
{
    Scanner::setComPort(comport, baudrate);
}


AR8000::AR8000 (ComPort *comport, double baudrate) : AORScanner(comport, baudrate)
{
    // in case bandplan is messed up, I use manual override of mode and step.

    putString("DD\r");                  // establish vfo operation
    putString("MD1\r");                 // establish NFM mode
    putString("AT0\r");                 // attenuator off
    putString("ST012500\r");            // step 12.5 no offset
}


AR8200::AR8200 (ComPort *comport, double baudrate) : AORScanner(comport, baudrate)
{
    // I assume bandplan is correct

    putString("AU1\r");                 // establish NFM mode
    putString("AT0\r");                 // attenuator off
    putString("ST012500\r");            // step 12.5 no offset
}


void AORScanner::setFreqString(channelDescriptor *f, char *fname, int line)
{
    f->scancmd [0] = 0;                                         // unknown mapping
    f->scancmdlen = 0;                                          // double-whammy

    if (strstr(f->freqdisp, "."))
    {
        f->scancmd [0] = 'R';   // R
        f->scancmd [1] = 'F';   // F
        f->scancmd [2] = '0';   // 0
        f->scancmd [3] = f->freqdisp [3];       // 8
        f->scancmd [4] = f->freqdisp [4];       // x
        f->scancmd [5] = f->freqdisp [5];       // x
        f->scancmd [6] = f->freqdisp [7];       // f
        f->scancmd [7] = f->freqdisp [8];       // f
        f->scancmd [8] = f->freqdisp [9];       // f
        f->scancmd [9] = f->freqdisp [10];      // f
        f->scancmd [10] = '0';  // 0
        f->scancmd [11] = '0';  // 0
        f->scancmd [12] = '\r';
        f->scancmd [13] = '\0';
    }
}


AR5000::AR5000 (ComPort *comport, double baudrate) : AORScanner(comport, baudrate)
{
    // I assume bandplan is correct

    putString("AU1\r");                 // establish auto mode so goes to nfm etc.
    putString("AT0\r");                 // attenuator off
}


void AORScanner::shutDown(void)
{
    putString("EX\r");                  // establish NFM mode
}


AR3000::AR3000 (ComPort *comport, double baudrate) : Scanner()
{
    Scanner::setComPort(comport, baudrate);

    putString("12.5S");                 // establish vfo operation
}


void AR3000::setFreqString(channelDescriptor *f, char *fname, int line)
{
    f->scancmd [0] = 0;                                         // unknown mapping
    f->scancmdlen = 0;                                          // double-whammy

    if (strstr(f->freqdisp, "."))
    {
        strcpy(f->scancmd, &f->freqdisp [3]);
        strcat(f->scancmd, "N");
    }
}


AR3000A::AR3000A (ComPort *comport, double baudrate) : Scanner()
{
    Scanner::setComPort(comport, baudrate);

    putString("12.5S");                 // establish vfo operation
}


void AR3000A::setFreqString(channelDescriptor *f, char *fname, int line)
{
    f->scancmd [0] = 0;                                         // unknown mapping
    f->scancmdlen = 0;                                          // double-whammy

    if (strstr(f->freqdisp, "."))
    {
        strcpy(f->scancmd, &f->freqdisp [3]);
        strcat(f->scancmd, "N\r");
    }
}


AR2700::AR2700 (ComPort *comport, double baudrate) : AORScanner(comport, baudrate)
{
    // I assume bandplan is correct

    putString("AU0\r");         // establish vfo operation
    putString("MD1\r");         // establish NFM mode
    putString("AT0\r");         // attenuator off
}


Kenwood::Kenwood (ComPort *comport, double baudrate) : Scanner()
{
    Scanner::setComPort(comport, baudrate);

    putString("MD4;");      // establish NFM mode
}


void Kenwood::setFreqString(channelDescriptor *f, char *fname, int line)
{
    f->scancmd [0] = 0;                                         // unknown mapping
    f->scancmdlen = 0;                                          // double-whammy

    if (strstr(f->freqdisp, "."))
    {
        f->scancmd [0] = 'F';   // F
        f->scancmd [1] = 'A';   // A
        f->scancmd [2] = '0';   // 0
        f->scancmd [3] = '0';   // 0
        f->scancmd [4] = f->freqdisp [3];       // 8
        f->scancmd [5] = f->freqdisp [4];       // x
        f->scancmd [6] = f->freqdisp [5];       // x
        f->scancmd [7] = f->freqdisp [7];       // f
        f->scancmd [8] = f->freqdisp [8];       // f
        f->scancmd [9] = f->freqdisp [9];       // f
        f->scancmd [10] = f->freqdisp [10];     // f
        f->scancmd [11] = '0';  // 0
        f->scancmd [12] = '0';  // 0
        f->scancmd [13] = ';';  // ;
        f->scancmd [14] = '\0';
    }
}


BC895::BC895(ComPort *comport, double baudrate) : Scanner()
{
    Scanner::setComPort(comport, baudrate);

    putString("KEY01\r");      // stop scanning
}


void BC895::setFreqString(channelDescriptor *f, char *fname, int line)
{
    f->scancmd [0] = 0;                                         // unknown mapping
    f->scancmdlen = 0;                                          // double-whammy

    if (strstr(f->freqdisp, "."))
    {
        f->scancmd [0] = 'R';   // R
        f->scancmd [1] = 'F';   // F
        f->scancmd [2] = '0';   // 0
        f->scancmd [3] = f->freqdisp [3];       // 8
        f->scancmd [4] = f->freqdisp [4];       // x
        f->scancmd [5] = f->freqdisp [5];       // x
        f->scancmd [6] = f->freqdisp [7];       // f
        f->scancmd [7] = f->freqdisp [8];       // f
        f->scancmd [8] = f->freqdisp [9];       // f
        f->scancmd [9] = f->freqdisp [10];      // f
        f->scancmd [10] = '\r';
        f->scancmd [11] = '\0';
    }
}


void NOSCANNER::setFreqString(channelDescriptor *f, char *fname, int line)
{
    f->scancmd [0] = 0;                                         // unknown mapping
    f->scancmdlen = 0;                                          // double-whammy
}
