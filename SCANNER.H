#ifndef __SCANNER_H_
#define __SCANNER_H_

#include "comport.h"

class  frequencyAssignment;

class  Scanner 
{
private:
        frequencyAssignment *lp;
        frequencyAssignment parking;
        ComPort *myPort;
        BOOL canPark;
protected:
        void putStringN (const int len, const char *cp)
        {
                int i;

                if (myPort && len)
                        for (i = 0; i < len; i++)
                                myPort->txChar (*cp++);
        }

        void putString (const char *cp)
        {
                if (myPort)
                        while (*cp) 
                                myPort->txChar (*cp++);
        }
public:
        int volume;
        int squelch;

        Scanner ();

        ComPort *getComPort (void) {return (myPort);}
        void setComPort (ComPort *comport, double baudrate = 9600.0);
        void topIs (frequencyAssignment* fp);
        void goPark();
    void setPark(double pf) {
        sprintf(this->parking.freqdisp,"cdv%8.4lf",pf);
        this->setFreqString(&this->parking);
        canPark = TRUE;
    }

        virtual void setFreqString (frequencyAssignment *) = 0;
        virtual void setVolume (int) {}
        virtual void setSquelch (int) {}
        virtual void shutDown (void) {}
	virtual ~Scanner(){}
};

class PCR1000 : public Scanner 
{
public: 
        PCR1000 (ComPort *comport, double baudrate = 38400.0);
        void setFreqString (frequencyAssignment *f);
        void setVolume (int volume);
        void setSquelch (int squelch);
        virtual void shutDown (void);
};
class OPTO : public Scanner 
{
public: 
        OPTO (ComPort *comport, double baudrate = 9600.0);
        void setFreqString (frequencyAssignment *f);
        virtual void shutDown (void);
};
class OPTOCOM : public Scanner 
{
public: 
        OPTOCOM (ComPort *comport, double baudrate = 9600.0);
        void setFreqString (frequencyAssignment *f);
        virtual void shutDown (void);
};

class R10 : public Scanner 
{
public: 
        R10 (ComPort *comport, double baudrate = 9600.0);
        void setFreqString (frequencyAssignment *f);
};

class R8500 : public Scanner 
{
public: 
        R8500 (ComPort *comport, double baudrate = 9600.0);
        void setFreqString (frequencyAssignment *f);
};

class R7100 : public Scanner 
{
public: 
        R7100 (ComPort *comport, double baudrate = 9600.0);
        void setFreqString (frequencyAssignment *f);
};

class R7000 : public Scanner 
{
public: 
        R7000 (ComPort *comport, double baudrate = 9600.0);
        void setFreqString (frequencyAssignment *f);
};
class FRG9600 : public Scanner 
{
public: 
        FRG9600 (ComPort *comport, double baudrate = 4800.0);
        void setFreqString (frequencyAssignment *f);
};

class AORScanner : public Scanner
{
        public:
        
        AORScanner(ComPort *comport, double baudrate);
        virtual void setFreqString (frequencyAssignment *f);
        virtual void shutDown (void);
};
class AR8000: public AORScanner 
{
public: 
        AR8000 (ComPort *comport, double baudrate = 9600.0);
};
class AR8200: public AORScanner 
{
public: 
        AR8200 (ComPort *comport, double baudrate = 19200.0);
};
class AR5000: public AORScanner 
{
public: 
        AR5000 (ComPort *comport, double baudrate = 9600.0);
};

class AR3000 : public Scanner 
{
public: 
        AR3000 (ComPort *comport, double baudrate = 4800.0);
        void setFreqString (frequencyAssignment *f);
};

class AR3000A : public Scanner 
{
public: 
        AR3000A (ComPort *comport, double baudrate = 9600.0);
        void setFreqString (frequencyAssignment *f);
};

class AR2700 : public AORScanner 
{
public: 
        AR2700 (ComPort *comport, double baudrate = 4800.0);
};

class Kenwood : public Scanner 
{
public: 
        Kenwood (ComPort *comport, double baudrate = 4800.00);
        void setFreqString (frequencyAssignment *f);
};

class BC895 : public Scanner 
{
public: 
        BC895 (ComPort *comport, double baudrate = 9600.0);
        virtual void setFreqString (frequencyAssignment *f);
};

class NOSCANNER : public Scanner 
{
public:
        NOSCANNER (){}
        void setFreqString (frequencyAssignment *f);
};
#endif
