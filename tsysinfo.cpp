#include <platform.h>
#include <trunkvars.h>

class tsysInfo : public CObject {
public:
int sysId;
int repeaterNum;
int numMapEntries;
const char *title;
struct
{
	int freq1;
	int freq2;
	char formatted[9];
} locMap[29];
char bandPlan;
char bankTypes[9];

~tsysInfo()
{
	free((void*)title);
}
tsysInfo(int sid, int rnum) : CObject(),
	title(strdup("Unknown")),
	sysId(sid),
	repeaterNum(rnum),
	numMapEntries(0),
	bandPlan('-')
{
	register FILE *fp;
	char fname[20];

	strcpy(bankTypes, "????????");
	if ( rnum >= 0 )
		sprintf(fname, "%.4hxc%d.txt", sysId, rnum + 1);
	else
		sprintf(fname, "%.4hxsys.txt", sysId);
	fp = fopen(fname, "rt");
	if ( !fp && rnum >= 0 )
	{
		sprintf(fname, "%.4hxr%d.txt", sysId, rnum);
		fp = fopen(fname, "rt");
		sprintf(fname, "%.4hxc%d.txt", sysId, rnum + 1);            // move to new name
	}
	if ( fp )                                                       // found it, load from file
	{
		char tempArea[120];
		if (fgets(tempArea, sizeof(tempArea), fp))                      // gets
		{
			register char *sep;
			sep = strchr(tempArea, '\n');
			if (sep)
				*sep = '\0';                                                         // null out newline if there...
			sep = strchr(tempArea, '\r');
			if (sep)
				*sep = '\0';                                                         // null out return if there...
			if (tempArea[0])
			{
				free((void*)title);
				title = strdup(tempArea);
			}
			unsigned short ifnum, ifnum2;
			while ( fgets(tempArea, sizeof(tempArea), fp) )
			{
				if ( strncmp(tempArea, "MAP=", 4) == 0)
				{
					strncpy(bankTypes, &tempArea[4], 8);
					bankTypes[8] = 0;
				}
				else if (strncmp(tempArea, "PLAN=", 5) == 0)
				{
					switch (tempArea[5])
					{
					case '8':
					case '9':
					case '0':
					case 'S':
						bandPlan = tempArea[5];
						break;
					case 's':
						bandPlan = 'S';
						break;
					default:
						bandPlan = '-';
						break;
					}
				}
				else if (strncmp(tempArea, "OPTIONS=", 8) == 0)
				{
					// ignore options
				}
				else
				{
					double d;
					sep = strchr(tempArea, ',');
					if (!sep)
						continue;
					ifnum2 = -1;
					if ( sscanf(sep + 1, "%4hx,%4hx", &ifnum, &ifnum2) < 1)
						continue;
					if (numMapEntries < 29)
					{
						d = atof(&tempArea[3]);
						if ( d != 0.0 )
						{
							sprintf(&locMap[numMapEntries].formatted[0], "%8.4f", d);
							locMap[numMapEntries].freq1 = ifnum;
							locMap[numMapEntries].freq2 = ifnum2;
							++numMapEntries;
						}
					}
				}
			}
		}
		fclose(fp);
	}
	else
	{
		// nothing to do, all attributes already initialized
	}
}
};

