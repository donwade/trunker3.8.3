#define TITLESIZE 25 
#define BEEPINDICATOR 32
extern BOOL seekNew;
extern unsigned char scanThreshold;

class  patchedGroups;
class  designation
{
protected:
        char title[TITLESIZE+1];        // 27

public:
        time_t lastNoted;
        unsigned long hits;
        unsigned short theCode;
        unsigned short usage;
        unsigned char prio;
        char color;
        char typeCode; 
        char desType;
#define desGroup 'G'
#define desIndiv 'I'
#define desPatch 'P'
                                                                

// static member functions
        
        static designation  *import(FILE *f, const char *tp);

// non-inline member functions

        void init(unsigned short cd, unsigned char ic,unsigned char ip,const char *it,time_t itm,unsigned long ihits);
        void init(unsigned short cd) { theCode = cd; }
        const char *getDisplayText(int flg = 0);
        
        int isEncoded() {
            return(title[0] == '[' && title[6] == ']' );
        }
        int isUnAnnotated() {
            // convention: un-annotated line looks like:
            // ?
            // [nnnnn]....   for decimal format
            // [#xxxx]....   for hex format
            return (
                (title[0] == '?' && title[1] == '\0') ||
                isEncoded()
            );
        }
        int isAnnotated() { return ! isUnAnnotated();}
        int isHexUnk() { // assumed that is already tested for isEncoded()
            return(title[1] == '#');
        }
        int isDecUnk() { // assumed that is already tested for isEncoded()
            return(title[1] >= '0' && title[1] <= '9');
        }
        void fixCoding();
                                  
// inline member functions                                

        char *titleString() { return &this->title[0]; }                          
        designation(); 
        unsigned char getPrio() {
                if( prio ) {
                        return prio;
                } else if( desType == desIndiv ) {
			return seekNew ? (unsigned char) newRadio : (unsigned char)(99);
               } else {
                        return seekNew ? (unsigned char) newGroup : (unsigned char)(scanThreshold);
                }
        }
        void setEditPrio(unsigned char p)       {prio = p;}
        void setEditColor(unsigned char c)      {color = c;}
        unsigned char getEditPrio()                      {return prio;}
        void setDispMode(char md)                       {typeCode = md;}
        void setEditTitle(const char *t)        {title[sizeof(title)-1] = '\0';
                                                   strncpy(title,t,sizeof(title)-1);
                                                  }
        BOOL export(FILE *f);
        unsigned short blockNum() {return (unsigned short)((theCode>>13)&((unsigned short)7));}
        unsigned short flags()    {return (unsigned short)(theCode & ((unsigned short)0x0f));}      
};
class patchedGroups : public designation
{
   private:     
        void buildInfo(designation *id);
   public:
        ptrMap<designation> theGroups;
        static patchedGroups *import(FILE *f, const char *tp);
        patchedGroups(unsigned short theCode, designation& firstgrp);
        patchedGroups(unsigned short theCode);
        patchedGroups();
        virtual ~patchedGroups();
        int GetCount() { return theGroups.entries();}
        void addGroup(designation* newgrp);
        void delGroup(designation* nomore);
        void makeEmpty() { theGroups.clear(); }
        BOOL isMember(designation * lookgrp) {
                return theGroups.contains(lookgrp->theCode);
        }
};
        
class DesigList : public  ptrMap<designation> {
public: 
        const char *fn;
        DesigList(const char *ifn);
        DesigList() : fn(0){}
        void note_type_1_block(register unsigned short blknum, register char sizeLetter);
        virtual ~DesigList(){ this->freeAll();}
        virtual void doSave(const char *tfn);
        virtual designation *get(unsigned short, BOOL = FALSE);
};

/*      Class GroupDesigList is a minor variant of DesigList that adds the GROUP bit
        to the code attribute so this list's entries can be known as groups instead of
        radio ids. ditto for PatchDesigList.
*/

class GroupDesigList : public DesigList {
public:
        char *sysBlkTypes;
        GroupDesigList(const char *ifn);
        virtual void doSave(const char *tfn);
        void note_type_2_block(unsigned short blknum);
        void note_type_1_block(register unsigned short blknum, register char sizeLetter);
        virtual designation *get(unsigned short d, BOOL x = FALSE) {
                designation *res = DesigList::get(d,x);
                if( res ) {
                        res->desType = desGroup;
                }
                return res;
        }
        BOOL bankPresent(unsigned short blknum);
        virtual ~GroupDesigList() {}
};

class PatchDesigList : public ptrMap<patchedGroups> {
public:
        const char *fn;
        PatchDesigList(const char *ifn);
        PatchDesigList():fn(0){}
        void doSave(const char *tfn);
        patchedGroups *get(unsigned short d,BOOL x = FALSE);
        virtual ~PatchDesigList(){}
};

extern void fixType1(designation *i, designation *g);
