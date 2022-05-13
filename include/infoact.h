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
    _IGNORE = 'i',
    _SCREEN = 's',
    _DISK = 'd',
    _BOTH = 'b',
    _SHORTLIFE = 'x',
    _LONGLIFE = 'y',
    _INFINLIFE = 'z',
    _DECIMAL = 'u',
    _HEX = 'v',
    _NORM = 'n',
    _REVERSE = 'r'
} InfoAction;

class InfoControl {
public:
	InfoAction desiredAction(InfoType) const;
	const char *getActionPrompt();
	void toggleAction(InfoType);
	void setAction(InfoType t, InfoAction a)
	{
	    actions[(int)t - 'a'] = a;
	}


	void reset()
	{
	    memset(actions, (char)_IGNORE, sizeof(actions));
	    // for irregular ones not covered above
	    actions[LIFETIME - 'a'] = _SHORTLIFE;
	    actions[FORMAT - 'a'] = _DECIMAL; //_HEX;
	    actions[SCROLLDIR - 'a'] = _REVERSE;
	}


	void updatePrompt();
	InfoControl();
	private:
	char promptstr[100];
	const char *pName(InfoType);
	const char *pAction(InfoAction);
	char actions[26];
};
