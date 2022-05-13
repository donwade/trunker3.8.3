#include <malloc.h>
#include <designat.h>
#define SCANCMDLEN 50
#define FREQDISPLEN 13
#define NOTSEENDELAY squelchDelay 
extern void shouldNever(const char *);
extern int  wordsPerSec;
extern int  squelchDelay;
class  FreqMap {
public:
        virtual char *mapCodeToF(unsigned short theCode, char bp) = 0;
        virtual BOOL   isFrequency(unsigned short theCode, char bp) = 0;
};
extern FreqMap *myFreqMap;
class  Scanner;
class  designation;
typedef enum {
        Icall   = 'I',
        Gcall   = 'G',
        HybGcall= 'H',
        Special = 'S',
        Phone   = 'p',
        Idle    = ' '
} CallType;
class  frequencyAssignment
{
private:
        static double actualFreq;
        int maxI,maxG,actI,actG;
        CallType curType;
        CallType maxType;
	BOOL seenVoice;
	
public:
        designation **idArray;                
        designation *callingDes;
        designation *destDes;
        time_t refreshFine;
        BOOL visited;
        BOOL dirty;
        unsigned short x, y, flags;
        int scancmdlen;
        unsigned short freqNum;
        unsigned short inpFreqNum;
        unsigned char prty;
        enum {  FREQEMPTY,FREQDATA,FREQCW,FREQCALL,FREQHOLD} fState;
        BOOL selected;
        unsigned short leftColor, rightColor, flagColor;
	long hits;
        char freqdisp[FREQDISPLEN];
        char scancmd[SCANCMDLEN];
	frequencyAssignment();
	virtual ~frequencyAssignment() {
	    if( idArray ) {
		free(idArray);
		idArray = 0;
	    }
	}
        unsigned char getPrio() {return prty;}
        void setPrio(unsigned char p) { prty = p; }
        void freshen();
    	void noteInactive();
        void create(int idx, unsigned short f, unsigned short fInp = 0, const char *actual = 0, char bp = 0);
        void assignCW(BOOL);
        void assignD();
        void unassign();
	void clobber(); // when did a copy, need to null out without freeing memory
        void show();
	BOOL isScannable() { return scancmdlen || scancmd[0]; }
        void refreshGcall(designation *d1, designation *d2, designation *d3);
        void refreshGcall(designation *d1, designation *d2);
        void refreshIcall(designation *d1, designation *d2);
        BOOL setDesig(int idx, designation *d);
        CallType getMaxType() const { return maxType; }
        void setMaxType(CallType t, designation *d1, designation *d2 = 0, designation *d3 = 0);
        void associateId(designation *d);
        BOOL disAssociateId(designation *d);
private:	
        CallType getCurType() const { return curType; }
        void calcDisplay();
        void recalcDisplay();
        void recalcTypeFewer();
        void recalcTypeMore();
};
