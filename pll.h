
class pll {
public:
pll(double bps, BOOL sense) :
	pllRate(bps),
	pllSense(sense),
	pllLocked(FALSE),
	ticksPerBit(1.0 / (bps * 838.8e-9)),
	ticksPerBitOver2((0.5 / (3600.0 * 838.8e-9))),
	exc(0.),
	accumulatedClock(0.),
	fastBoundaryCompare(0.)
{
}
BOOL isLocked()
{
	return pllLocked;
}
virtual ~pll()
{
}
void inputData(BOOL dataval, unsigned short durticks)
{
	if ( !pllSense )
		dataval = dataval ? FALSE : TRUE;
	accumulatedClock += durticks;
	fastBoundaryCompare = exc + ticksPerBitOver2;
	while (accumulatedClock >= fastBoundaryCompare)
	{
		handleBit(dataval);
		accumulatedClock -= ticksPerBit;
	}
	if ( dataval )
		exc += (accumulatedClock - exc) / 15.;
}
private:
double pllRate;
BOOL pllSense;
BOOL pllLocked;
double ticksPerBit;
double ticksPerBitOver2;
double exc;
double accumulatedClock;
double fastBoundaryCompare;
protected:
void setLocked(BOOL locked)
{
	pllLocked = locked;
}
virtual void handleBit(BOOL theData) = 0;         // must be provided by subclass
};
class motoPLL36 : public pll
{
public:
motoPLL36() : pll(3600., TRUE)
{
}
virtual ~motoPLL36()
{
}
};
class ericPLL48 : public pll
{
public:
ericPLL48() : pll(4800., TRUE)
{
}
virtual ~ericPLL48()
{
}
};
class ericPLL96 : public pll
{
public:
ericPLL96() : pll(9600., FALSE)
{
}
virtual ~ericPLL96()
{
}
};

