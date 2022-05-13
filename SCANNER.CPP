#include <platform.h>
#include <layouts.h>
#pragma  warning(once:4309)

Scanner::Scanner () : lp (0), canPark(FALSE),myPort(0),volume(130),squelch(80) 
{
}

void
Scanner::setComPort (ComPort *comport, double baudrate)
{
        myPort = comport;
        myPort->setBaud (baudrate);
}
void
Scanner::goPark()
{
    if(canPark) {
        if(this->parking.scancmdlen) {
                putStringN(this->parking.scancmdlen,this->parking.scancmd);
        } else {
                putString(this->parking.scancmd);
        }
    }
    lp = 0;
}
void
Scanner::topIs (frequencyAssignment *fp)
{
        if (fp != lp) 
        {
                if (lp) 
                        QuickOut (lp->y, lp->x + PRTYSTART - 1, color_quiet, (const char *)" ");

                if (fp && (fp->scancmd [0] || fp->scancmdlen))
                {
                        QuickOut (fp->y, fp->x + PRTYSTART - 1, color_quiet, "*");

                        if (fp->scancmdlen)
                                putStringN (fp->scancmdlen, fp->scancmd);
                        else
                                putString (fp->scancmd);
                }
        if( !fp )
        {
                goPark(); // clears lp
        } else {
                        lp = fp;
                }
        }
}

PCR1000::PCR1000 (ComPort *comport, double baudrate) : Scanner ()
{
        Scanner::setComPort (comport, baudrate);

        comport->setBaud (9600.0);
        putString ("H101\n");   // set power on

        if (baudrate == 300.0)
                putString ("G100\n");
        else if (baudrate == 1200.0)
                putString ("G101\n");
        else if (baudrate == 4800.0)
                putString ("G102\n");
        else if (baudrate == 9600.0)
                putString ("G103\n");
        else if (baudrate == 19200.0)
                putString ("G104\n");
        else if (baudrate == 38400.0)
                putString ("G105\n");
        else
        {
                baudrate = 38400.0;
                putString ("G105\n");
        }

        comport->setBaud (baudrate);
        putString ("G300\n");   // set return data off
        putString ("J4501\n");  // set AGC on
        putString ("J4600\n");  // set NB off
        putString ("J4700\n");  // set attenuator off
        putString ("J5100\n");  // set tone squelch off

        setVolume (volume);
        setSquelch (squelch);
}

void
PCR1000::shutDown (void)
{
        putString ("H100\n");   // turn power off
}

void
PCR1000::setFreqString (frequencyAssignment *f) 
{
        f->scancmd [0] = 0; // unknown mapping
        f->scancmdlen = 0; // double-whammy
        if (strstr (f->freqdisp, ".")) 
        {
                f->scancmd [0]  = 'K';                          // K
                f->scancmd [1]  = '0';                          // 0
                f->scancmd [2]  = '0';                          // 0
                f->scancmd [3]  = f->freqdisp [3];      // 8
                f->scancmd [4]  = f->freqdisp [4];      // x
                f->scancmd [5]  = f->freqdisp [5];      // x
                f->scancmd [6]  = f->freqdisp [7];      // f
                f->scancmd [7]  = f->freqdisp [8];      // f 
                f->scancmd [8]  = f->freqdisp [9];      // f 
                f->scancmd [9]  = f->freqdisp [10]; // f
                f->scancmd [10] = '0';                          // 0
                f->scancmd [11] = '0';                          // 0
                f->scancmd [12] = '0';                          // 0
                f->scancmd [13] = '5';                          // 5
                f->scancmd [14] = '0';                          // 0
                f->scancmd [15] = '2';                          // 2
                f->scancmd [16] = '0';                          // 0
                f->scancmd [17] = '0';                          // 0
                f->scancmd [18] = '\n';                         // 0
                f->scancmd [19] = '\0';
        } 
}

void
PCR1000::setVolume (int volume)
{
        char volumeCmd [32];

        sprintf (volumeCmd, "J40%02x\n", volume & 0xff);
        putString (volumeCmd);
}

void
PCR1000::setSquelch (int squelch)
{
        char squelchCmd [32];

        sprintf (squelchCmd, "J41%02x\n", squelch & 0xff);
        putString (squelchCmd);
}

R10::R10 (ComPort *comport, double baudrate) : Scanner ()
{
        char Init [] = {0xfe, 0xfe, 0x52, 0xe0, 0x06, 0x05, 0x02, 0xfd};

        Scanner::setComPort (comport, baudrate);

        putStringN (8, Init);
}

void
R10::setFreqString (frequencyAssignment *f) 
{
        f->scancmd [0] = 0; // unknown mapping
        f->scancmdlen = 0; // double-whammy
        if (strstr (f->freqdisp, ".")) 
        {
                f->scancmd [0]  = 0xfe;                                         // Preamble
                f->scancmd [1]  = 0xfe;                                         // Preamble
                f->scancmd [2]  = 0x52;                                         // Radio type (R10)
                f->scancmd [3]  = 0xe0;                                         // Transmit address (controller)
                f->scancmd [4]  = 0x05;                                         // Write operating frequency
                f->scancmd [5]  = 0x00;                                         // 10Hz/1Hz
                f->scancmd [6]  = (char)(((f->freqdisp [9] & 0x0f) << 4) | (f->freqdisp [10] & 0x0f)); // 1Khz/100Hz
                f->scancmd [7]  = (char)(((f->freqdisp [7] & 0x0f) << 4) | (f->freqdisp [8] & 0x0f));  // 100Khz / 10Khz
                f->scancmd [8]  = (char)(((f->freqdisp [4] & 0x0f) << 4) | (f->freqdisp [5] & 0x0f));  // 10Mhz/1Mhz
                f->scancmd [9]  = (char)(f->freqdisp [3] & 0x0f);       // 100's Mhz
                f->scancmd [10] = 0xfd;                                         // Terminator

                //
                //  Setting this to a non-zero means treat it as a counted string
                //
                f->scancmdlen = 11;
        } 
}
OPTO::OPTO (ComPort *comport, double baudrate) : Scanner ()   
{
        char Init [] = {0xfe, 0xfe, 0x80, 0xe0, 0x7F, 0x02, 0xfd}; //FE  FE  80  E0  7F  02  FD
                                                               
        char Init2[] = {0xfe, 0xfe, 0x80, 0xe0, 0x01, 0x05, 0xfd}; //FE  FE  80  E0  01  05  FD
        
        Scanner::setComPort (comport, baudrate);

        putStringN (7, Init); // remote mode
        putStringN (7, Init2);// narrow FM
}

void
OPTO::setFreqString (frequencyAssignment *f) 
{
        f->scancmd [0] = 0; // unknown mapping
        f->scancmdlen = 0; // double-whammy
        if (strstr (f->freqdisp, ".")) 
        {
                f->scancmd [0]  = 0xfe;                                         // Preamble
                f->scancmd [1]  = 0xfe;                                         // Preamble
                f->scancmd [2]  = 0x80;                                         // Radio type (OPTO)
                f->scancmd [3]  = 0xe0;                                         // Transmit address (controller)
                f->scancmd [4]  = 0x00;                                         // xfer operating frequency
                f->scancmd [5]  = 0x00;                                         // 10Hz/1Hz
                f->scancmd [6]  = (char)(((f->freqdisp [9] & 0x0f) << 4) | (f->freqdisp [10] & 0x0f)); // 1Khz/100Hz
                f->scancmd [7]  = (char)(((f->freqdisp [7] & 0x0f) << 4) | (f->freqdisp [8] & 0x0f));  // 100Khz / 10Khz
                f->scancmd [8]  = (char)(((f->freqdisp [4] & 0x0f) << 4) | (f->freqdisp [5] & 0x0f));  // 10Mhz/1Mhz
                f->scancmd [9]  =(char)( f->freqdisp [3] & 0x0f);       // 100's Mhz
                f->scancmd [10] = 0xfd;                                         // Terminator

                //
                //  Setting this to a non-zero means treated it as a counted string
                //
                f->scancmdlen = 11;
        } 
}
void
OPTOCOM::shutDown (void)
{
        char final[] = { 0xfe, 0xfe, 0x80, 0xe0, 0x7f, 0x18, 0x01, 0xfd };
        putStringN (8,final);  // auto-scan-mode
}

OPTOCOM::OPTOCOM (ComPort *comport, double baudrate) : Scanner ()   
{
        char Init2 [] = {0xfe, 0xfe, 0x80, 0xe0, 0x7f, 0x18, 0x00, 0xfd};                                                               
        char Init3 [] = {0xfe, 0xfe, 0x80, 0xe0, 0x01, 0x05, 0xfd};
        
        Scanner::setComPort (comport, baudrate);

        putStringN (8, Init2);
        putStringN (7, Init3);
}

void
OPTOCOM::setFreqString (frequencyAssignment *f) 
{
        f->scancmd [0] = 0; // unknown mapping
        f->scancmdlen = 0; // double-whammy
        if (strstr (f->freqdisp, ".")) 
        {
                f->scancmd [0]  = 0xfe;                                         // Preamble
                f->scancmd [1]  = 0xfe;                                         // Preamble
                f->scancmd [2]  = 0x80;                                         // Radio type (OPTO)
                f->scancmd [3]  = 0xe0;                                         // Transmit address (controller)
                f->scancmd [4]  = 0x00;                                         // xfer operating frequency
                f->scancmd [5]  = 0x00;                                         // 10Hz/1Hz
                f->scancmd [6]  = (char)(((f->freqdisp [9] & 0x0f) << 4) | (f->freqdisp [10] & 0x0f)); // 1Khz/100Hz
                f->scancmd [7]  = (char)(((f->freqdisp [7] & 0x0f) << 4) | (f->freqdisp [8] & 0x0f));  // 100Khz / 10Khz
                f->scancmd [8]  = (char)(((f->freqdisp [4] & 0x0f) << 4) | (f->freqdisp [5] & 0x0f));  // 10Mhz/1Mhz
                f->scancmd [9]  = (char)(f->freqdisp [3] & 0x0f);       // 100's Mhz
                f->scancmd [10] = 0xfd;                                         // Terminator

                //
                //  Setting this to a non-zero means treated it as a counted string
                //
                f->scancmdlen = 11;
        } 
}
void
OPTO::shutDown (void)
{
        char final[] = { 0xfe, 0xfe, 0x80, 0xe0, 0x7f, 0x01, 0xfd };// FE  FE  80  E0  7F  01  FD
        putStringN (7,final);  // front panel mode
}
R8500::R8500 (ComPort *comport, double baudrate) : Scanner ()
{
        char Init [] = {0xfe, 0xfe, 0x4a, 0xe0, 0x06, 0x05, 0x02, 0xfd};

        Scanner::setComPort (comport, baudrate);

        putStringN (8, Init);
}

void
R8500::setFreqString (frequencyAssignment *f) 
{
        f->scancmd [0] = 0; // unknown mapping
        f->scancmdlen = 0; // double-whammy
        if (strstr (f->freqdisp, ".")) 
        {
                f->scancmd [0]  = 0xfe;                                         // Preamble
                f->scancmd [1]  = 0xfe;                                         // Preamble
                f->scancmd [2]  = 0x4a;                                         // Radio type (R8500)
                f->scancmd [3]  = 0xe0;                                         // Transmit address (controller)
                f->scancmd [4]  = 0x05;                                         // Write operating frequency
                f->scancmd [5]  = 0x00;                                         // 10Hz/1Hz
                f->scancmd [6]  = (char)(((f->freqdisp [9] & 0x0f) << 4) | (f->freqdisp [10] & 0x0f)); // 1Khz/100Hz
                f->scancmd [7]  =(char)( ((f->freqdisp [7] & 0x0f) << 4) | (f->freqdisp [8] & 0x0f));  // 100Khz / 10Khz
                f->scancmd [8]  = (char)(((f->freqdisp [4] & 0x0f) << 4) | (f->freqdisp [5] & 0x0f));  // 10Mhz/1Mhz
                f->scancmd [9]  =(char)( f->freqdisp [3] & 0x0f);       // 100's Mhz
                f->scancmd [10] = 0xfd;                                         // Terminator

                //
                //  Setting this to a non-zero means treated it as a counted string
                //
                f->scancmdlen = 11;
        } 
}

R7100::R7100 (ComPort *comport, double baudrate) : Scanner ()
{
        char Init [] = {0xfe, 0xfe, 0x34, 0xe0, 0x06, 0x05, 0x02, 0xfd};

        Scanner::setComPort (comport, baudrate);

        putStringN (8, Init);
}

void
R7100::setFreqString (frequencyAssignment *f) 
{
        f->scancmd [0] = 0; // unknown mapping
        f->scancmdlen = 0; // double-whammy
        if (strstr (f->freqdisp, ".")) 
        {
                f->scancmd [0]  = 0xfe;                                         // Preamble
                f->scancmd [1]  = 0xfe;                                         // Preamble
                f->scancmd [2]  = 0x34;                                         // Radio type (R7100)
                f->scancmd [3]  = 0xe0;                                         // Transmit address (controller)
                f->scancmd [4]  = 0x05;                                         // Write operating frequency
                f->scancmd [5]  = 0x00;                                         // 10Hz/1Hz
                f->scancmd [6]  = (char)(((f->freqdisp [9] & 0x0f) << 4) | (f->freqdisp [10] & 0x0f)); // 1Khz/100Hz
                f->scancmd [7]  = (char)(((f->freqdisp [7] & 0x0f) << 4) | (f->freqdisp [8] & 0x0f));  // 100Khz / 10Khz
                f->scancmd [8]  = (char)(((f->freqdisp [4] & 0x0f) << 4) | (f->freqdisp [5] & 0x0f));  // 10Mhz/1Mhz
                f->scancmd [9]  = (char)(f->freqdisp [3] & 0x0f);       // 100's Mhz
                f->scancmd [10] = 0xfd;                                         // Terminator

                //
                //  Setting this to a non-zero means treated it as a counted string
                //
                f->scancmdlen = 11;
        } 
}

R7000::R7000 (ComPort *comport, double baudrate) : Scanner ()
{
        char Init [] = {0xfe, 0xfe, 0x08, 0xe0, 0x06, 0x05, 0x02, 0xfd};

        Scanner::setComPort (comport, baudrate);

        putStringN (8, Init);
}

void
R7000::setFreqString (frequencyAssignment *f) 
{
        f->scancmd [0] = 0; // unknown mapping
        f->scancmdlen = 0; // double-whammy
        if (strstr (f->freqdisp, ".")) 
        {
                f->scancmd [0]  = 0xfe;                                         // Preamble
                f->scancmd [1]  = 0xfe;                                         // Preamble
                f->scancmd [2]  = 0x08;                                         // Radio type (R7000)
                f->scancmd [3]  = 0xe0;                                         // Transmit address (controller)
                f->scancmd [4]  = 0x05;                                         // Write operating frequency
                f->scancmd [5]  = 0x00;                                         // 10Hz/1Hz
                f->scancmd [6]  = (char)(((f->freqdisp [9] & 0x0f) << 4) | (f->freqdisp [10] & 0x0f)); // 1Khz/100Hz
                f->scancmd [7]  = (char)(((f->freqdisp [7] & 0x0f) << 4) | (f->freqdisp [8] & 0x0f));  // 100Khz / 10Khz
                f->scancmd [8]  = (char)(((f->freqdisp [4] & 0x0f) << 4) | (f->freqdisp [5] & 0x0f));  // 10Mhz/1Mhz
                f->scancmd [9]  = (char)(f->freqdisp [3] & 0x0f);       // 100's Mhz
                f->scancmd [10] = 0xfd;                                         // Terminator

                //
                //  Setting this to a non-zero means treated it as a counted string
                //
                f->scancmdlen = 11;
        } 
}
FRG9600::FRG9600 (ComPort *comport, double baudrate) : Scanner ()
{
        Scanner::setComPort (comport, baudrate);
}

void
FRG9600::setFreqString (frequencyAssignment *f) 
{
        f->scancmd [0] = 0; // unknown mapping
        f->scancmdlen = 0; // double-whammy
        if (strstr (f->freqdisp, ".")) // 0123456789a
                                                                   // cdv851.0125
        
        {
                f->scancmd [0]  = 0x0A;                                         // cmd = 0x0a - set freq
                f->scancmd [1]  = (char)(((f->freqdisp [3] & 0x0f) << 4) | (f->freqdisp [4] & 0x0f));  // 100Mhz/10Mhz
                f->scancmd [2]  = (char)(((f->freqdisp [5] & 0x0f) << 4) | (f->freqdisp [7] & 0x0f));  // 1Mhz/100khz
                f->scancmd [3]  = (char)(((f->freqdisp [8] & 0x0f) << 4) | (f->freqdisp [9] & 0x0f));  // 10khz 1khz
                f->scancmd [4]  = (char)(((f->freqdisp [10] & 0x0f) << 4));                                                        // 100hz 10hz

                //
                //  Setting this to a non-zero means treated it as a counted string
                //
                f->scancmdlen = 5;
        } 
}
AORScanner::AORScanner (ComPort *comport, double baudrate) : Scanner () 
{
        Scanner::setComPort (comport, baudrate);
}

AR8000::AR8000 (ComPort *comport, double baudrate) : AORScanner (comport,baudrate) 
{

        // in case bandplan is messed up, I use manual override of mode and step. 
        
        putString ("DD\r");                     // establish vfo operation
        putString ("MD1\r");            // establish NFM mode
        putString ("AT0\r");            // attenuator off
        putString ("ST012500\r");       // step 12.5 no offset
}
AR8200::AR8200 (ComPort *comport, double baudrate) : AORScanner (comport,baudrate) 
{
        // I assume bandplan is correct

        putString ("AU1\r");            // establish NFM mode
        putString ("AT0\r");            // attenuator off
        putString ("ST012500\r");       // step 12.5 no offset
}

void
AORScanner::setFreqString (frequencyAssignment *f) 
{
        f->scancmd [0] = 0; // unknown mapping
        f->scancmdlen = 0; // double-whammy
        if (strstr (f->freqdisp, ".")) 
        {
                f->scancmd [0]  = 'R';                          // R
                f->scancmd [1]  = 'F';                          // F
                f->scancmd [2]  = '0';                          // 0
                f->scancmd [3]  = f->freqdisp [3];      // 8
                f->scancmd [4]  = f->freqdisp [4];      // x
                f->scancmd [5]  = f->freqdisp [5];      // x
                f->scancmd [6]  = f->freqdisp [7];      // f
                f->scancmd [7]  = f->freqdisp [8];      // f 
                f->scancmd [8]  = f->freqdisp [9];      // f 
                f->scancmd [9]  = f->freqdisp [10]; // f
                f->scancmd [10] = '0';                          // 0
                f->scancmd [11] = '0';                          // 0
                f->scancmd [12] = '\r';
                f->scancmd [13] = '\0';
        } 
}
AR5000::AR5000 (ComPort *comport, double baudrate) : AORScanner (comport,baudrate) 
{
        // I assume bandplan is correct

        putString ("AU1\r");            // establish auto mode so goes to nfm etc.
        putString ("AT0\r");            // attenuator off
}

void
AORScanner::shutDown (void) 
{
        putString ("EX\r");             // establish NFM mode
}

AR3000::AR3000 (ComPort *comport, double baudrate) : Scanner ()
{
        Scanner::setComPort (comport, baudrate);

        putString ("12.5S");            // establish vfo operation
}
void
AR3000::setFreqString (frequencyAssignment *f) 
{
        f->scancmd [0] = 0; // unknown mapping
        f->scancmdlen = 0; // double-whammy
        if (strstr (f->freqdisp, "."))
        {
                strcpy(f->scancmd,&f->freqdisp [3]);
                strcat(f->scancmd,"N");
        }
}

AR3000A::AR3000A (ComPort *comport, double baudrate) : Scanner ()
{
        Scanner::setComPort (comport, baudrate);

        putString ("12.5S");            // establish vfo operation
}

void
AR3000A::setFreqString (frequencyAssignment *f) 
{
        f->scancmd [0] = 0; // unknown mapping
        f->scancmdlen = 0; // double-whammy
        if (strstr (f->freqdisp,".")) 
        {
                strcpy(f->scancmd,&f->freqdisp [3]);
                strcat(f->scancmd,"N\r");
        } 
}

AR2700::AR2700 (ComPort *comport, double baudrate) : AORScanner (comport,baudrate)
{
        // I assume bandplan is correct

        putString ("AU0\r"); // establish vfo operation
        putString ("MD1\r"); // establish NFM mode
        putString ("AT0\r"); // attenuator off
}
Kenwood::Kenwood (ComPort *comport, double baudrate) : Scanner ()
{
        Scanner::setComPort (comport, baudrate);

        putString ("MD4;"); // establish NFM mode
}

void
Kenwood::setFreqString (frequencyAssignment *f)
{
        f->scancmd [0] = 0; // unknown mapping
        f->scancmdlen = 0; // double-whammy
        if (strstr (f->freqdisp, ".")) 
        {
                f->scancmd [0]  = 'F';                          // F
                f->scancmd [1]  = 'A';                          // A
                f->scancmd [2]  = '0';                          // 0
                f->scancmd [3]  = '0';                          // 0
                f->scancmd [4]  = f->freqdisp [3];      // 8
                f->scancmd [5]  = f->freqdisp [4];      // x
                f->scancmd [6]  = f->freqdisp [5];      // x
                f->scancmd [7]  = f->freqdisp [7];      // f
                f->scancmd [8]  = f->freqdisp [8];      // f 
                f->scancmd [9]  = f->freqdisp [9];      // f 
                f->scancmd [10] = f->freqdisp [10]; // f
                f->scancmd [11] = '0';                          // 0
                f->scancmd [12] = '0';                          // 0
                f->scancmd [13] = ';';                          // ;
                f->scancmd [14] = '\0';
        } 
}

BC895::BC895(ComPort *comport, double baudrate) : Scanner ()
{
        Scanner::setComPort (comport, baudrate);

        putString ("KEY01\r"); // stop scanning
}

void
BC895::setFreqString (frequencyAssignment *f)
{
        f->scancmd [0] = 0; // unknown mapping
        f->scancmdlen = 0; // double-whammy
        if (strstr (f->freqdisp, ".")) 
        {
                f->scancmd [0]  = 'R';                          // R
                f->scancmd [1]  = 'F';                          // F
                f->scancmd [2]  = '0';                          // 0
                f->scancmd [3]  = f->freqdisp [3];      // 8
                f->scancmd [4]  = f->freqdisp [4];      // x
                f->scancmd [5]  = f->freqdisp [5];      // x
                f->scancmd [6]  = f->freqdisp [7];      // f
                f->scancmd [7]  = f->freqdisp [8];      // f 
                f->scancmd [8]  = f->freqdisp [9];      // f 
                f->scancmd [9]  = f->freqdisp [10]; // f
                f->scancmd [10] = '\r';
                f->scancmd [11] = '\0';
        } 
}
void 
NOSCANNER::setFreqString (frequencyAssignment *f) 
{
        f->scancmd [0] = 0; // unknown mapping
        f->scancmdlen = 0; // double-whammy
}
