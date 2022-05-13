#ifndef _COMPORT_H
#define _COMPORT_H

typedef enum
{
    NONE = 0,
    COM1 = 1,
    COM2 = 2,
    COM3 = 3,
    COM4 = 4
}
comportPort;

typedef enum
{
    RBR = 0,
    THR = 0,
    IER = 1,
    IIR = 2,
    LCR = 3,
    MCR = 4,
    LSR = 5,
    MSR = 6,
    SCR = 7,
    DLL = 0,
    DLH = 1
}
comportRegisters;

typedef enum
{
    PARITY_NONE = 0x00,
    PARITY_ODD = 0x08,
    PARITY_EVEN = 0x18,
    PARITY_MASK = 0x18
}
comportParity;

typedef enum
{
    BITS_5 = 0x00,
    BITS_6 = 0x01,
    BITS_7 = 0x02,
    BITS_8 = 0x03,
    BITS_MASK = 0x03
}
comportBits;

typedef enum
{
    STOP_1 = 0x00,
    STOP_2 = 0x04,
    STOP_MASK = 0x04,
}
comportStop;

typedef enum
{
    NORMAL = 0x00,
    DLAB = 0x80,
    DLAB_MASK = 0x80,
}
comportDLAB;

typedef enum
{
    DTR_LOW = 0x00,
    DTR_HIGH = 0x01,
    DTR_MASK = 0x01
}
comportDTR;

typedef enum
{
    RTS_LOW = 0x00,
    RTS_HIGH = 0x02,
    RTS_MASK = 0x02
}
comportRTS;

typedef enum
{
    INT_DISABLED = 0x00,
    INT_ENABLED = 0x08,
    INT_MASK = 0x08
}
comportInterrupts;

typedef enum
{
    IER_NONE = 0x00,
    IER_ERBFI = 0x01,
    IER_ETBEI = 0x02,
    IER_ELSI = 0x04,
    IER_EDSSI = 0x08
}
comportIER;

typedef enum
{
    LSR_DR = 0x01,
    LSR_OE = 0x02,
    LSR_PE = 0x04,
    LSR_FE = 0x08,
    LSR_BI = 0x10,
    LSR_THRE = 0x20,
    LSR_TEMT = 0x40,
    LSR_FIFOERR = 0x80
}
comportLSR;

typedef enum
{
    MSR_DCTS = 0x01,
    MSR_DDSR = 0x02,
    MSR_TERI = 0x04,
    MSR_DDCD = 0x08,
    MSR_CTS = 0x10,
    MSR_DSR = 0x20,
    MSR_RI = 0x40,
    MSR_DCD = 0x80
}
comportMSR;

#define inp(x) 1
#define __interrupt
#define _far

class ComPort
{
private:
comportPort portID;

int portBase;
int portIRQ;
double currentBaudRate;

int originalIER;
int originalLCR;
int originalMCR;

void(__interrupt _far * originalIntvec) (void);

protected:
void init(comportPort port, double baudrate, comportBits bits, comportParity parity, comportStop stop, comportDTR dtr, comportRTS rts);

public:
ComPort (int port); // really telnet port number for gqrx
ComPort (comportPort comport);
ComPort (comportPort comport, double baudrate);
ComPort (comportPort port, double baudrate, comportBits bits, comportParity parity, comportStop stop, comportDTR dtr, comportRTS rts);

~ComPort ();
comportPort getPort(void)
{
    return portID;
}


int inline getRBR(void)
{
    return inp(portBase + RBR);
}


int inline getIER(void)
{
    return inp(portBase + IER);
}


int inline getIIR(void)
{
    return inp(portBase + IIR);
}


int inline getLCR(void)
{
    return inp(portBase + LCR);
}


int inline getMCR(void)
{
    return inp(portBase + MCR);
}


int inline getLSR(void)
{
    return inp(portBase + LSR);
}


int inline getMSR(void)
{
    return inp(portBase + MSR);
}


void setHandler(void(__interrupt _far *newIntvec) (void));
void removeHandler(void);
void setInterrupts(comportIER ier);
void setInterruptsGlobal(comportInterrupts ints);
void setBaud(double baudrate);
void setParity(comportParity parity);
void setBits(comportBits bits);
void setStop(comportStop stop);
void setDTR(comportDTR dtr);
void setRTS(comportRTS cts);
void txChar(unsigned char byte);
int  rxStatus(void);
unsigned char rxChar(void);
void dump(char *header);
void dump(void);
};

#endif
