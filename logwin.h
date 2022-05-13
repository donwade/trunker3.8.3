// will find dups within 'LOGWIN_DUP_LIMIT' seconds

#define LOGWIN_DUP_LIMIT 3

class LogEntry {
public:
LogEntry(time_t tparm, const char * cparm) : next(0), prev(0), tval(tparm), cval(strdup(cparm))
{
	if (!cval)
		shouldNever("could not allocate log string text");
}
int nearlyEqual(const LogEntry& cprto) const
{
	if ((
	        (
	            (cprto->getCval() - getCval()) < LOGWIN_DUP_LIMIT
	        ) ||
	        (
	            (getCval() - cprto->getCval()) < LOGWIN_DUP_LIMIT
	        )
	        ) && !strcmp(cprto->getTval(), getTval())
	    )
		return 1;
	else
		return 0;

}
virtual ~LogEntry()
{
	if ( cval )
	{
		free(cval);
		cval = 0;
	}
}
time_t getTval() const
{
	return tval;
}
const char * getCval() const
{
	return cval;
}
LogEntry * next;
LogEntry * prev;
private:
time_t tval;
char * cval;
}


class Logwindow
{
private:
int maxrows;            // maximum rows available on screen
int currows;            // current rows available on screen
int bufrows;            // number of rows in buffer
int scrtop;             // number of row at top of screen
void delLast();
void addNew(ListEntry *);
void redraw();
int isDup();
LogEntry * head;
LogEntry * tail;
int numEntries;
int visib;             // true if allowed to touch screen

public:
void logText(const char *, int logToFile, int logToScreen);
void resize(int numrows)
{
	if ( numrows  > maxrows )
		shouldNever();
	else
	{
		currows = numrows; redraw();
	}
}
void pageUp();
void pageDown();
void pageHome();
void pageEnd();
void hide()
{
	visib = 0;
}
void unhide()
{
	visib = 1; redraw();
}
Logwindow(int maxview, int maxbuffer, int curview); // initialize vars and paint screen
virtual ~Logwindow();                               // have to free all on list
}
// other: who is affiliated at one time
// other: who is in the patch at present?

