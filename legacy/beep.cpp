/* OUTP.C: This program uses _inp and _outp to make sound of variable
 * tone and duration.
 */

#include <conio.h>
#include <stdio.h>
#include <time.h>
#define outportb outp
#define outport outpw
#define inportb inp
#define inport inpw

static int control = inportb( 0x61 );

extern void Beep( unsigned frequency, unsigned duration ); /* Prototypes */
extern void Sleep( clock_t _wait );
extern void Beep( unsigned frequency );
/* Sounds the speaker for a time specified in microseconds by duration
 * at a pitch specified in hertz by frequency.
 */
void Beep( unsigned frequency, unsigned duration )
{
	/* If frequency is 0, Beep doesn't try to make a sound. */
	if ( frequency )
	{
		/* 75 is about the shortest reliable duration of a sound. */
		if ( duration < 75 )
			duration = 75;

		/* Prepare timer by sending 10111100 to port 43. */
		outportb( 0x43, 0xb6 );

		/* Divide input frequency by timer ticks per second and
		 * write (byte by byte) to timer.
		 */
		frequency = (unsigned)(1193180L / frequency);
		outportb( 0x42, (char)frequency );
		outportb( 0x42, (char)(frequency >> 8) );

		/* Turn on the speaker (with bits 0 and 1). */
		outportb( 0x61, control | 0x3 );
	}

	Sleep( (clock_t)duration );

	/* Turn speaker back on if necessary. */
	if ( frequency )
		outportb( 0x61, control );
}
void Beep(unsigned frequency)
{
	/* Save speaker control byte. */

	/* If frequency is 0, Beep doesn't try to make a sound. */
	if ( frequency )
	{

		/* Prepare timer by sending 10111100 to port 43. */
		outportb( 0x43, 0xb6 );

		/* Divide input frequency by timer ticks per second and
		 * write (byte by byte) to timer.
		 */
		frequency = (unsigned)(1193180L / frequency);
		outportb( 0x42, (char)frequency );
		outportb( 0x42, (char)(frequency >> 8) );


		/* Turn on the speaker (with bits 0 and 1). */
		outportb( 0x61, control | 0x3 );
	}
	else
		outportb( 0x61, control );
}

/* Pauses for a specified number of microseconds. */
void Sleep( clock_t _wait )
{
	clock_t goal;

	goal = _wait + clock();
	while ( goal > clock() )
		;
}




