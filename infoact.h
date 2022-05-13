typedef enum
{
	AFFIL = 'a',
	DIAGNOST = 'd',
	MEMBERS = 'p',
	CALLALERT = 'c',
	EMERG = 'e',
	LIFETIME = 'l',
	NEIGHBORS = 'n',
	FORMAT = 'f',
	SCROLLDIR = 's'
} InfoType;

typedef enum
{
	IGNORE = 'i',
	SCREEN = 's',
	DISK = 'd',
	BOTH = 'b',
	SHORTLIFE = 'x',
	LONGLIFE = 'y',
	INFINLIFE = 'z',
	DECIMAL = 'u',
	HEX = 'v',
	NORM = 'n',
	REVERSE = 'r'
} InfoAction;

class InfoControl  {
public:
InfoAction desiredAction(InfoType) const;
const char * getActionPrompt();
void toggleAction(InfoType);
void setAction(InfoType t, InfoAction a)
{
	actions[(int)t - 'a'] = a;
}
void reset()
{
	memset(actions, (char)IGNORE, sizeof(actions));
	// for irregular ones not covered above
	actions[LIFETIME - 'a'] = SHORTLIFE;
	actions[FORMAT - 'a'] = HEX;
	actions[SCROLLDIR - 'a'] = REVERSE;
}
void updatePrompt();
InfoControl();
private:
char promptstr[100];
const char * pName(InfoType);
const char * pAction(InfoAction);
char actions[26];
};




