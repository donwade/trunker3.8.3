class MotorolaMap : public FreqMap {
public:
virtual char * mapCodeToF(unsigned short theCode, char bp);
virtual BOOL isFrequency(unsigned short theCode, char bp);
};
