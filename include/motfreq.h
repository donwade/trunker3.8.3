class MotorolaMap : public FreqMap {
public:
virtual char *mapCodeToF(unsigned short theCode, char bp);
virtual BOOL isFrequency(unsigned short theCode, char bp);
};

unsigned mapSysIdCodeToFreq(unsigned short theCode, unsigned short sysId);
