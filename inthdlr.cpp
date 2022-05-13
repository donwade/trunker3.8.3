#include <platform.h>

unsigned short bitsAtZero;
unsigned short bitsAtOne;
volatile unsigned int cpstn = 0;   // current position in buffer
volatile unsigned short fdata [BufLen + 1];

extern ComPort *mySlicerPort;

//
//                                      this is serial com port interrupt
// we assume here that it only gets called when one of the status
// lines on the serial port changes (that's all you have hooked up).
// All this handler does is read the system timer (which increments
// every 840 nanoseconds) and stores it in the fdata array. The MSB
// is set to indicate whether the status line is zero. In this way
// the fdata array is continuously updated with the appropriate the
// length and polarity of each data pulse for further processing by
// the main program.
//
void __far __interrupt com1int(void)
{
	unsigned short d1, d2, d3, tick, dtick;

	//
	// the system timer is a 16 bit counter whose value counts down
	// from 65535 to zero and repeats ad nauseum. For those who really
	// care, every time the count reaches zero the system timer
	// interrupt is called (remember that thing that gets called every
	// 55 milliseconds and does housekeeping such as checking the
	// keyboard.
	//

	// do this asap

	outportb(0x43, 0x00);                                   // latch counter until we read it
	d1 = (unsigned char)inportb(0x40);                      // get low count
	d2 = (unsigned char)inportb(0x40);                      // get high count
	d3 = (unsigned short)mySlicerPort->getMSR();            // get modem interrupt status

	bitsAtOne |= d3;
	bitsAtZero |= ~d3;

	//
	// only compute data if the requested bit changed! other interrupts may be used some day.
	// get difference between current, last counter reading
	//
	if ((lastbit & pinToRead) != (d3 & pinToRead))  // filter out impulse noise
	{           //
		                                            // by filtering out spurious interrupts, we significantly improve the
		                                            // noise floor.  If the bit appears unchanged because it took a long time
		                                            // to handle the interrupt, the missed bit will be detected by error checks.
		                                            //

		tick  = (unsigned short)((d2 << 8) + d1);
		dtick = (unsigned short)(ltick - tick);
		ltick = tick;

		if (d3 & pinToRead)                             // read current value
			dtick |= 0x8000;
		else
			dtick &= 0x7fff;

		//
		// put freq in fdata array
		//
		fdata [cpstn] = dtick;

		//
		// make sure cpstn doesnt leave array
		//
		if (++cpstn > BufLen)
			cpstn = 0;

		lastbit = d3;
	}

	//
	//  Clear status bits in UART
	//
	d1 = (unsigned short)mySlicerPort->getIIR();                // clear IIR
	d1 = (unsigned short)mySlicerPort->getLSR();                // clear LSR
	d1 = (unsigned short)mySlicerPort->getMSR();                // clear MSR

	if (d1 & 0x01)
		d1 = (unsigned short)mySlicerPort->getRBR();                            // clear RX - someday may process data

	//
	//  This sends non-specific End-Of-Interrupt to PIC
	//
	outportb(0x20, 0x20);
}

