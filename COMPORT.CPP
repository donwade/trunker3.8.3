#include <platform.h>

ComPort::ComPort (comportPort port)
{
	init (port, 9600.0, BITS_8, PARITY_NONE, STOP_2, DTR_HIGH, RTS_HIGH);
}

ComPort::ComPort (comportPort port, double baudrate)
{
	init (port, baudrate, BITS_8, PARITY_NONE, STOP_2, DTR_HIGH, RTS_HIGH);
}

ComPort::ComPort (comportPort port, double baudrate, comportBits bits,
				   comportParity parity, comportStop stop, comportDTR dtr,
				   comportRTS rts)
{
	init (port, baudrate, bits, parity, stop, dtr, rts);
}

void
ComPort::init ( comportPort port, double baudrate, comportBits bits,
				comportParity parity, comportStop stop, comportDTR dtr,
				comportRTS rts)
{
	switch (port)
	{
		case COM1 :
			portID = port;
			portBase = 0x3f8;
			portIRQ = 4;
			break;
		
		case COM2 :
			portID = port;
			portBase = 0x2f8;
			portIRQ = 3;
			break;
		
		case COM3 :
			portID = port;
			portBase = 0x3e8;
			portIRQ = 4;
			break;
		
		case COM4 :
			portID = port;
			portBase = 0x2e8;
			portIRQ = 3;
			break;

		case NONE :
		default :
			portID = NONE;
			portBase = -1;
			portIRQ = -1;
			return;
	}		

	originalIntvec = _dos_getvect (portIRQ + 8);

	originalIER = inp (portBase + IER);
	originalLCR = inp (portBase + LCR);
	originalMCR = inp (portBase + MCR);

	setBaud (baudrate);
	setBits (bits);
	setParity (parity);
	setStop (stop);
	setDTR (dtr);
	setRTS (rts);
	setInterrupts (IER_NONE);
	setInterruptsGlobal (INT_DISABLED);
}

ComPort::~ComPort ()
{
	if( portID != NONE ) {
		setInterrupts (IER_NONE);
		setInterruptsGlobal (INT_DISABLED);
	}
}

void
ComPort::setHandler (void (__interrupt __far *newIntvec) (void))
{
	if( portID != NONE ) {
		_dos_setvect (portIRQ + 8, newIntvec);
	}
}
void
ComPort::removeHandler (void)
{
	if( portID != NONE ) {
		_dos_setvect (portIRQ + 8, originalIntvec);
	}
}

void
ComPort::setInterrupts (comportIER ier)
{
	if( portID != NONE ) {
		outp (portBase + IER, ier);
	}
}

void
ComPort::setInterruptsGlobal (comportInterrupts ints)
{   
	if( portID != NONE ) {

		int currentMCR;
		int throwAway;
	
		currentMCR = inp (portBase + MCR);
		currentMCR &= ~INT_MASK;
		currentMCR |= ints;
		outp (portBase + MCR, currentMCR);
	
		throwAway = inp (portBase + LSR);
		throwAway = inp (portBase + MSR);
		throwAway = inp (portBase + RBR);
		throwAway = inp (portBase + IIR);
		throwAway = inp (portBase + IIR);
	
		if (ints == INT_ENABLED)
			outp (0x21, inp (0x21) & ~(1 << portIRQ));
		else
			outp (0x21, inp (0x21) | (1 << portIRQ));
	}
}

void
ComPort::setBaud (double baudrate)
{
	if( portID != NONE ) {

		unsigned int currentLCR;
		unsigned int divisorLatch;
	
		currentBaudRate = baudrate;
		divisorLatch = (int) (1843200.0 / (16.0 * baudrate));
	
		currentLCR = inp (portBase + LCR);
		outp (portBase + LCR, DLAB | currentLCR);
		outp (portBase + DLL, divisorLatch & 0xff);
		outp (portBase + DLH, divisorLatch >> 8);
		outp (portBase + LCR, currentLCR & ~DLAB);
	}
}
void
ComPort::setBits (comportBits bits)
{
	if( portID != NONE ) {
		int currentLCR;
	
		currentLCR = inp (portBase + LCR);
		currentLCR &= ~BITS_MASK;
		currentLCR |= bits;
		outp (portBase + LCR, currentLCR);
	}
}

void
ComPort::setParity (comportParity parity)
{
	if( portID != NONE ) {
		int currentLCR;
	
		currentLCR = inp (portBase + LCR);
		currentLCR &= ~PARITY_MASK;
		currentLCR |= parity;
		outp (portBase + LCR, currentLCR);
	}
}

void 
ComPort::setStop (comportStop stop)
{
	if( portID != NONE ) {

		int currentLCR;
	
		currentLCR = inp (portBase + LCR);
		currentLCR &= ~STOP_MASK;
		currentLCR |= stop;
		outp (portBase + LCR, currentLCR);
	}
}

void 
ComPort::setDTR (comportDTR dtr)
{
	if( portID != NONE ) {
		int currentMCR;
	
		currentMCR = inp (portBase + MCR);
		currentMCR &= ~DTR_MASK;
		currentMCR |= dtr;
		outp (portBase + MCR, currentMCR);
	}
}

void 
ComPort::setRTS (comportRTS rts)
{
	if( portID != NONE ) {
		int currentMCR;
	
		currentMCR = inp (portBase + MCR);
		currentMCR &= ~RTS_MASK;
		currentMCR |= rts;
		outp (portBase + MCR, currentMCR);
	}
}

void 
ComPort::txChar (unsigned char byte)
{
	if( portID != NONE ) {
	
		while (!(inp (portBase + LSR) & 0x20))
			;
	
		outp (portBase + THR, byte);
	}
}

int  
ComPort::rxStatus (void)
{
	if( portID != NONE ) {
		return (inp (portBase + LSR) & LSR_DR);
	} else return 0;
}

unsigned char 
ComPort::rxChar (void)
{
	if( portID != NONE ) {
		while (!rxStatus ())
			;
	
		return ((unsigned char)inp (portBase + RBR));
	} else return 0;
}

void
ComPort::dump (char *header)
{
	printf ("%s\n", header);
	dump ();
}

void
ComPort::dump (void)
{
	if( portID != NONE ) {
		printf ("  ComBase = %04x\n", portBase);
		printf ("  ComIRQ  = %d\n", portIRQ);
		printf ("  Baud    = %f\n", currentBaudRate);
		printf ("  IER     = %02x\n", getIER ());
		printf ("  LCR     = %02x\n", getLCR ());
		printf ("  MCR     = %02x\n", getMCR ());
		printf ("  LSR     = %02x\n", getLSR ());
		printf ("  MSR     = %02x\n", getMSR ());
	    printf ("  PIC     = %02x\n", inp (0x21));
		printf ("  Intvec  = %08lx\n", _dos_getvect (portIRQ + 8));
		printf ("  Oldvec  = %08lx\n", originalIntvec); 
	} else printf("port = NONE!\n");
}
