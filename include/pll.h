
class pll {
public:
pll(double bps, BOOL sense) :
    pllRate(bps),
    pllSense(sense),
    pllLocked(FALSE),
    ticksPerDataBit(1.0 / (bps * 838.8e-9)),
    ticksPerHalfDataBit((0.5 / (bps * 838.8e-9))),
    exc(0.),
    accumulatedClock(0.),
    fastBoundaryCompare(0.)
{
    lprintf3("pll set for ticksPerDataBit=%5.1f uS at databaud=%5.1f bps \n", ticksPerDataBit, bps);
    sleep(3);
}


BOOL isLocked()
{
    return pllLocked;
}


virtual ~pll()
{
}


void inputData(BOOL dataval, float bitTime)
{
    if (!pllSense)
        dataval = dataval ? FALSE : TRUE;

    accumulatedClock += bitTime;
    fastBoundaryCompare = exc + ticksPerHalfDataBit;

    while (accumulatedClock >= fastBoundaryCompare)
    {
        handleBit(dataval);
        accumulatedClock -= ticksPerDataBit;
    }

    if (dataval)
        exc += (accumulatedClock - exc) / 15.;
}


private:
double pllRate;
BOOL pllSense;
BOOL pllLocked;
double ticksPerDataBit;
double ticksPerHalfDataBit;
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

void handleBit(BOOL theData)
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
