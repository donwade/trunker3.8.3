#include <platform.h>
#include <osw.h>
#include <trunksys.h>
#include <time.h>
#include <io.h>

msgList::msgList(const char *fn) : msgs(0), theCount(0)
{
	register FILE *fp;

	fp = fopen(fn, "rt");
	if ( fp == NULL )
		return;

	char tmpbuf[100];
	register char *cp;
	register struct msgHolder *nm;
	char inptype;
	unsigned short inpcode;
	unsigned scancode;

	// read in and populate structures
	while ( fgets(tmpbuf, sizeof(tmpbuf), fp) != NULL)
	{
		if ( strlen(tmpbuf) < 5)
			continue;
		if ( tmpbuf[0] == ';' || tmpbuf[0] == '#' )
			continue;
		switch (tmpbuf[0])
		{
		case 'm':
		case 'M':
			inptype = 'M';
			break;
		case 's':
		case 'S':
			inptype = 'S';
			break;
		default:
			continue;
		}
		if (tmpbuf[1] != ',')
			continue;
		inpcode = 0;
		if (sscanf(&tmpbuf[2], "%u", &scancode) == 1 && scancode >= 1 && scancode <= 16)
		{
			inpcode = (unsigned short)scancode;
			cp = strchr(&tmpbuf[2], ',');
			if (!cp)
				continue;
			++cp;
			nm = (struct msgHolder*)calloc(1, sizeof(struct msgHolder));
			if (!nm)
				shouldNever("could not get message buffer");
			nm->mtype = inptype;
			nm->mnumber = inpcode;
			nm->mtext = strdup(cp);
			if (nm->mtext == NULL)
				shouldNever("string for message text");
			cp = strchr(nm->mtext, '\n');
			if ( cp )
				*cp = '\0';
			cp = strchr(nm->mtext, '\r');
			if ( cp )
				*cp = '\0';
			nm->next = this->msgs;
			this->msgs = nm;
			this->theCount++;
		}
	}
	fclose(fp);
}
const char *
msgList::getMsgText(char mtype, unsigned short mnumber)
{
	register struct msgHolder *curp;
	static char myFormat[100];

	for ( curp = msgs; curp; curp = curp->next )
		if ( curp->mtype == mtype && curp->mnumber == mnumber)
			break;
	if ( curp )
		// send message from file
		return curp->mtext;
	else
	{
		// send canned message
		sprintf(myFormat, "Message of type '%c', #%hd", mtype, mnumber);
		return myFormat;
	}
}
msgList::~msgList()
{
	register struct msgHolder *curp, *np;

	curp = msgs;
	while (curp)
	{
		np = curp;
		curp = np->next;
		free(np->mtext);
		delete np;
	}
}

InfoControl userChoices;


// char TrunkSys::logWin[PAGELINES][1+ROWAREA];
// int TrunkSys::nextLogRow = -1;
static char tempArea[500];
unsigned char scanThreshold = 50;
void
setCurrentDisposition(register const char *cp)
{
	register InfoType t = (InfoType)0;
	register InfoAction a = (InfoAction)0;

	for (; *cp && *cp >= 'a' && *cp <= 'z'; ++cp )
	{
		if (t == (InfoType)0)
			t = (InfoType) * cp;
		else
		{
			a = (InfoAction) * cp;
			userChoices.setAction(t, a);
			t = (InfoType)0;
		}
	}
}

void
putCurrentDisposition(FILE *fp)
{
	BOOL first = TRUE;
	register int i;
	register InfoAction a;

	for (i = 'a'; i <= 'z'; ++i)
	{
		a = userChoices.desiredAction((InfoType)i);
		if ( a != IGNORE )
		{
			if (first)
			{
				first = FALSE;
				fprintf(fp, "DISPOSITION=");
			}
			fprintf(fp, "%c%c", i, (int)a);
		}
	}
	if (!first)
		fputc('\n', fp);
}

FreqList::FreqList(const char * fn, int& naid,  char *tp ) :
	sysName(0), myBandPlan('-')
{
	userChoices.reset();
	aid = new frequencyAssignment[MAXFREQS + 1];           // allocate a block of frequency structures
	if ( !aid )
		shouldNever("Can't allocate block of frequencies!");
	naid = 0;
	savenaid = &naid;
	register FILE *fp = fopen(fn, "rt");
	if ( fp )
	{
		if (fgets(tempArea, sizeof(tempArea), fp))                         // gets
		{
			register frequencyAssignment *f = aid;
			register char *sep;
			sep = strchr(tempArea, '\n');
			if (sep)
				*sep = '\0';                                                 // null out newline if there...
			sep = strchr(tempArea, '\r');
			if (sep)
				*sep = '\0';                                                 // null out return if there...
			if (tempArea[0])
				if (!sysName)
					sysName = strdup(tempArea);

			unsigned short ifnum, ifnum2;
			long ihits;
			while ( fgets(tempArea, sizeof(tempArea), fp) )
			{
				if ( strncmp(tempArea, "MAP=", 4) == 0)
				{
					if ( tp)
					{
						strncpy(tp, &tempArea[4], 8);
						tp[8] = 0;
					}
				}
				else if (strncmp(tempArea, "PLAN=", 5) == 0)
				{
					switch (tempArea[5])
					{
					case '8':
					case '9':
					case '0':
					case 'S':
						myBandPlan = tempArea[5];
						break;
					case 's':
						myBandPlan = 'S';
						break;
					default:
						shouldNever("PLAN= values are 8, 9, 0, and S");
						break;
					}
				}
				else if (strncmp(tempArea, "OPTIONS=", 8) == 0)
					setCurrentOptions(&tempArea[8]);
				else if (strncmp(tempArea, "DESCR=", 6) == 0)
					;                                                             // ignore
				else if (strncmp(tempArea, "DISPOSITION=", 12) == 0)
					setCurrentDisposition(&tempArea[12]);
				else if (strncmp(tempArea, "THRESHOLD=", 10) == 0)
				{
					unsigned int theNewThresh;
					if ( sscanf(&tempArea[10], "%u", &theNewThresh) == 1
					     && theNewThresh < 100)
						scanThreshold = (unsigned char)theNewThresh;
				}
				else
				{
					sep = strchr(tempArea, ',');
					if (!sep)
						continue;
					ifnum2 = -1;
					ihits = 0L;
					if ( sscanf(sep + 1, "%4hx,%4hx,%lx", &ifnum, &ifnum2, &ihits) < 1)
						continue;
					if ( naid < MAXFREQS)
					{
						f->create(naid, ifnum, ifnum2, &tempArea[3]);
						memcpy(&f->freqdisp[0], tempArea, 3);                                                                        //copy tags c,d,v
						f->show();
						f->hits = ihits;
						++naid; ++f;
					}
					else
					{
						QuickOut(FBROW, 1, color_norm, "Only 28 Frequencies!                                                   ");
						break;
					}
				}
			}
		}
		fclose(fp);
	}
	if (!sysName)
	{
		switch (fn[4])
		{
		case 'r':
		case 'R':
		case 'c':
		case 'C':
			sysName = strdup("UnNamed Cell");
			// leave bandplan at '-' if not set so can propagate downward
			break;
		default:
			sysName = strdup("UnNamed System");
			if (myBandPlan == '-')
				myBandPlan = '8';
			break;
		}
	}
	userChoices.updatePrompt();
}

void
FreqList::doSave(const char *tfn, const char *theMap, BOOL odd400, BOOL doFreqs, const char *nn)
{
	if (*savenaid > 0)
	{
		prepareFile(tfn);
		register FILE *fp = fopen(tfn, "wt");
		if ( fp )
		{
			register frequencyAssignment *f;
			register int i;
			fprintf(fp, "%s\n", nn ? nn : sysName);
			if (theMap)
				fprintf(fp, "MAP=%.8s\n", theMap);
			fprintf(fp, "OPTIONS=%s\n", getCurrentOptions());
			if (myBandPlan != '-')
				fprintf(fp, "PLAN=%c\n", myBandPlan);
			fprintf(fp, "DESCR=%s\n", mySys->describe());
			fprintf(fp, "THRESHOLD=%u\n", (unsigned int)scanThreshold);
			putCurrentDisposition(fp);
			if (doFreqs)
			{
				for (i = 0, f = aid; i < *savenaid; ++i, ++f)
				{
					if ( f->freqdisp[0] != ' ' ||
					     f->freqdisp[1] != ' ' ||
					     f->freqdisp[2] != ' ' )
					{
						if ( odd400 )
							fprintf(fp, "%s,%hx,%hx,%lx\n",
							        f->freqdisp, f->freqNum, f->inpFreqNum, f->hits);
						else
							fprintf(fp, "%s,%hx,%hx,%lx\n",
							        f->freqdisp, f->freqNum, -1, f->hits);
					}
				}
			}
			fclose(fp);
		}
		else
			shouldNever("could not open sys.txt file for write");
	}
}

FreqList::~FreqList()
{
	register int i;

	delete [] aid;
	free((void*)sysName);
	_settextcolor(color_norm);
	sprintf(tempArea, "%-*.*s", 80, 80, "");
	for ( i = 0; i < MAXFREQS; ++i)
	{
		_settextposition((short)(FREQROW + 3 + i), (short)1);
		_outtext(tempArea);
	}
}

TrunkSys::TrunkSys(unsigned short nid, Scanner *sp, const char *variety) :
	doAuto(TRUE), myScanner(sp), siteId(-1), fSite(4),
	vhf_uhf(FALSE), bandPlan('-'), state(Searching),
	networkable(FALSE), networked(FALSE), antiScannerTones(FALSE),
	lastBeep(0), beepTimer(0), beepDuration(0), strtime(0), siteName(0),
	basicType("unknown"), subType(0), conTone(0), patchesActive(FALSE), oneChType(0)

{
	memset(newFreqs, 0, sizeof(newFreqs));
	memset(neighbors, -1, sizeof(neighbors));
	sprintf(sysPrefix, "%.4hx", nid);
	sysId = nid;
	this->init(variety);
}

void TrunkSys::setSearchMode()
{
	if ( state != Searching )
	{
		sprintf(tempArea, "Searching                                        ");
		QuickOut(3, 17, color_norm, tempArea);
		state = Searching;
		endScanSearch();
	}
}
void
TrunkSys::setManualHold(unsigned short theId, BOOL isPatch)
{
	if (isPatch)
	{
		state = HoldPatch;
		holdDes1 = getPatchDes(theId);
		if (userChoices.desiredAction(FORMAT) == HEX)
			sprintf(tempArea, "Holding: Patch=#%04hX %-*.*s", holdDes1->theCode,
			        TITLESIZE, TITLESIZE, holdDes1->titleString());
		else
			sprintf(tempArea, "Holding: Patch=%5hu %-*.*s", holdDes1->theCode,
			        TITLESIZE, TITLESIZE, holdDes1->titleString());
	}
	else
	{
		state = HoldGrp;
		holdDes1 = getGroupDes(theId);                         // may have to lop off some bits, map to type 1
		if (userChoices.desiredAction(FORMAT) == HEX)
			sprintf(tempArea, "Holding: Group=#%04hX %-*.*s", holdDes1->theCode,
			        TITLESIZE, TITLESIZE, holdDes1->titleString());
		else
			sprintf(tempArea, "Holding: Group=%5hu %-*.*s", holdDes1->theCode,
			        TITLESIZE, TITLESIZE, holdDes1->titleString());
	}
	QuickOut(3, 17, color_norm, tempArea);
}
const char *
TrunkSys::describe()
{
	static char descbuf[100];

	strcpy(descbuf, basicType);
	if (subType)
		strcat(descbuf, subType);
	if (oneChType)
		strcat(descbuf, oneChType);
	if (isNetworked())
		strcat(descbuf, " NetWkd");
	else if (isNetworkable())
		strcat(descbuf, " NetCap");
	if (conTone)
	{
		strcat(descbuf, " ");
		strcat(descbuf, conTone);
		strcat(descbuf, " Hz");
	}
	return (const char*)&descbuf[0];
}
void
TrunkSys::holdThis(register frequencyAssignment *raid)
{
	if ( raid->destDes->desType != desIndiv )
	{
		holdDes1 = raid->destDes;
		if ( raid->destDes->desType == desPatch )
		{
			state = HoldPatch;
			if (userChoices.desiredAction(FORMAT) == HEX)
				sprintf(tempArea, "Holding: Patch=#%04hX %-*.*s", holdDes1->theCode,
				        TITLESIZE, TITLESIZE, holdDes1->titleString());
			else
				sprintf(tempArea, "Holding: Patch=%5hu %-*.*s", holdDes1->theCode,
				        TITLESIZE, TITLESIZE, holdDes1->titleString());
		}
		else
		{
			state = HoldGrp;
			if (userChoices.desiredAction(FORMAT) == HEX)
				sprintf(tempArea, "Holding: Group=#%04hX %-*.*s", holdDes1->theCode,
				        TITLESIZE, TITLESIZE, holdDes1->titleString());
			else
				sprintf(tempArea, "Holding: Group=%5hu %-*.*s", holdDes1->theCode,
				        TITLESIZE, TITLESIZE, holdDes1->titleString());
		}
	}
	else
	{
		holdDes1 = raid->destDes;
		if ( holdDes2 )
		{
			state = Hold2Indiv;
			holdDes2 = raid->callingDes;
			if (userChoices.desiredAction(FORMAT) == HEX)
				sprintf(tempArea, "Holding: I=#%04hX/%04hX                      ", holdDes1->theCode,
				        holdDes2->theCode);
			else
				sprintf(tempArea, "Holding: I=%5hu/%5hu                       ", holdDes1->theCode,
				        holdDes2->theCode);
		}
		else
		{
			state = Hold1Indiv;
			if (userChoices.desiredAction(FORMAT) == HEX)
				sprintf(tempArea, "Holding: I=#%04hX %-*.*s", holdDes1->theCode,
				        TITLESIZE, TITLESIZE, holdDes1->titleString());
			else
				sprintf(tempArea, "Holding: I=5%hu %-*.*s", holdDes1->theCode,
				        TITLESIZE, TITLESIZE, holdDes1->titleString());
		}
	}
	QuickOut(3, 17, color_norm, tempArea);
}
void
TrunkSys::jogDown()
{
	register int i;                                     // iterator for known frequencies
	register frequencyAssignment *raid;                 // for optimization, points to each object in turn
	frequencyAssignment *jogFirst = 0;
	frequencyAssignment *jogAfter = 0;
	BOOL gotSel = FALSE;

	for (i = 0, raid = aid; i < naid; ++raid, ++i)
	{
		// if there is a current entry, use it to determine state
		if (raid->selected)
			gotSel = TRUE;
		else if ( raid->fState == frequencyAssignment::FREQHOLD ||
		          raid->fState == frequencyAssignment::FREQCALL )
		{
			if ( !jogFirst )
				jogFirst = raid;
			if ( !jogAfter && gotSel )
				jogAfter = raid;
		}
	}
	if ( jogAfter )
		raid = jogAfter;
	else
		raid = jogFirst;
	if (raid)
		holdThis(raid);
}
void
TrunkSys::jogUp()
{
	register int i;                                     // iterator for known frequencies
	register frequencyAssignment *raid;                 // for optimization, points to each object in turn
	frequencyAssignment *jogLast = 0;
	frequencyAssignment *jogBefore = 0;
	BOOL gotSel = FALSE;

	for (i = 0, raid = aid; i < naid; ++raid, ++i)
	{
		// if there is a current entry, use it to determine state
		if (raid->selected)
			gotSel = TRUE;
		else if ( raid->fState == frequencyAssignment::FREQHOLD ||
		          raid->fState == frequencyAssignment::FREQCALL )
		{
			if ( !gotSel )
				jogBefore = raid;
			jogLast = raid;
		}
	}
	if ( jogBefore )
		raid = jogBefore;
	else
		raid = jogLast;
	if (raid)
		holdThis(raid);
}
void
TrunkSys::setHoldMode()
{
	if (state == Searching)
	{
		// examine 'current' entry, determine if group or icall
		// if current, set state to holdgroup, else holdindiv
		register int i;                                                     // iterator for known frequencies
		register frequencyAssignment *raid;                                 // for optimization, points to each object in turn
		for (i = 0, raid = aid; i < naid; ++raid, ++i)                      // one pass over ALL objects
		{               // if there is a current entry, use it to determine state
			if ( raid->selected )
			{
				holdThis(raid);
				break;
			}
		}
	}
}
void
TrunkSys::note_neighbor(unsigned short cellNum, unsigned short itsFreq)
{
	if ( siteId == cellNum )
		return;
	// maintain a map of system id to frequency code. if its value
	// changes, log that in printable form.
	unsigned short value;
	value = this->neighbors[cellNum];
	if (value == itsFreq)
		return;                        // dup
	neighbors[cellNum] = itsFreq;
	sprintf(tempArea, "Cell # %d using/may use f=%s",
	        cellNum + 1, myFreqMap->mapCodeToF(itsFreq, fList->getBandPlan()));
	logStringT(tempArea, NEIGHBORS);
}

void
TrunkSys::initF(unsigned short sysid, short cellnum)
{
	if ( cellnum >= 0)
	{
		sprintf(sysFn, "%04hxc%hd.txt", sysid, (WORD)(cellnum + 1));
		if ( access(sysFn, F_OK) )
		{
			sprintf(sysFn, "%04hxr%hd.txt", sysid, (WORD)cellnum);
			if ( access(sysFn, F_OK) )
				// neither there, use the new!
				sprintf(sysFn, "%04hxc%hd.txt", sysid, (WORD)(cellnum + 1));
		}
	}
	fList = new FreqList(sysFn, naid, types);
	if ( !fList )
		shouldNever("cannot alloc list objects");
	// fix to new file name in case had used old style
	if ( cellnum >= 0 )
		sprintf(sysFn, "%04hxc%hd.txt", sysid, (WORD)(cellnum + 1));
	aid = fList->getList();
	if ( fList->getBandPlan() == '-' )
		fList->setBandPlan(bandPlan);                           // propagate downward
	else
		bandPlan = fList->getBandPlan();                        // populate initially
	register int i;
	register frequencyAssignment *raid = aid;
	for (i = 0; i < naid; ++i, ++raid)
	{
		if ( raid->inpFreqNum > 0 && raid->inpFreqNum != 0xffff )                   // if at least ONE is > 0, consider UHF.
			// existing data files may be all zeros....
			vhf_uhf = TRUE;
		myScanner->setFreqString(raid);                                             // initialize scanner string
	}
	// use 'i' to set size of log window initially, then change when freq added
	myLogWin->autoSize(naid);
	memset(neighbors, -1, sizeof(neighbors));
}
unsigned short
TrunkSys::mapInpFreq(unsigned short ifreq)
{
	register int i;
	register frequencyAssignment *raid = aid;

	for (i = 0; i < naid; ++i, ++raid)
		if ( raid->inpFreqNum == ifreq )
			return raid->freqNum;
	return -1;
}
void TrunkSys::init(const char *variety)
{
	static char lineChars[] =
	    "-----------------------------------------------";
	char myTplace[150];

	sprintf(myTplace, "%s %s", variety,
#ifdef UT
	        "Trk UT"
#else
#ifdef BT
	        "Tr383 b6"
#else
	        "Trunker v3.8.3"
#endif
#endif
	        );
	strcpy(types, "????????");
	_settextcolor(color_quiet);
	_settextposition(FREQROW, 1);
	sprintf(tempArea, "   Output   Pr %*.*s", 80 - 15, 80 - 15, myTplace);
	_outtext(tempArea);
	_settextposition(FREQROW + 1, 1);
	sprintf(tempArea, "  Frequency ty %-*.*s   ID  T  ID   %-*.*s",
	        TITLESIZE, TITLESIZE, "Destination Title", TITLESIZE, TITLESIZE, "Caller Title");
	_outtext(tempArea);
	_settextposition(FREQROW + 2, 1);
	sprintf(tempArea, "cdv-------- -- %*.*s ----- - ----- %-*.*s",
	        TITLESIZE, TITLESIZE, lineChars, TITLESIZE, TITLESIZE, lineChars);
	_outtext(tempArea);
	_settextposition(STATROW + 1, 1);
	_outtext("SysId: ____ Type: ________ Title:                                             ");
	sprintf(tempArea, "%-*.*s", sizeof(sysPrefix) - 1, sizeof(sysPrefix) - 1, sysPrefix);
	QuickOut(STATROW + 1, 8, color_norm, tempArea);
	sprintf(sysFn, "%sSys.txt", sysPrefix);
	sprintf(oldFn, "%sSys.txt", sysPrefix);
	sprintf(idsFn, "%sIds.txt", sysPrefix);
	sprintf(grpFn, "%sGrp.txt", sysPrefix);
	sprintf(patchFn, "%sPtch.txt", sysPrefix);
	sprintf(msgFn, "%smsg.txt", sysPrefix);
	sprintf(pageFn, "%sPage.txt", sysPrefix);
	patchList = new PatchDesigList(patchFn);
	groupList = new GroupDesigList(grpFn);
	idList = new DesigList(idsFn);
	msgPtr = new msgList(msgFn);
	if ( !groupList || !idList || !patchList)
		shouldNever("cannot alloc list objects.");
	_settextposition(FBROW, 1);
	cprintf("Imported %d Groups, %d Radios, %d Patches, %d MsgText", groupList->entries(),
	        idList->entries(),
	        patchList->entries(),
	        msgPtr->getCount());
	myLogWin = new scrlog(6, 35);            // start with "old" settings
	initF(sysId, -1);
	if (!myLogWin)
		shouldNever("alloc of log window failed");
	searchDes1 = searchDes2 = 0;
	searchPrty = 100;
	sysName = strdup(fList->getSysName());
	unsigned short i;
	for (i = 0; i < 8; ++i)
	{
		if ( types[i] == '?' )
			types[i] = groupList->sysBlkTypes[i];
		else
		{
			// group list needs to be synchronized with override in sys file
			if ( types[i] == '2' )
				groupList->note_type_2_block(i);
			else if ( types[i] >= 'A' && types[i] <= 'M' )
			{
				groupList->note_type_1_block(i, types[i]);
				idList->note_type_1_block(i, types[i]);
			}
			else if ( types[i] >= 'a' && types[i] <= 'm' )
			{
				groupList->note_type_1_block(i, (char)('A' + (types[i] - 'a')));
				idList->note_type_1_block(i, types[i]);
			}
		}
		if (types[i] == '?' && getBandPlan() != '8' && getBandPlan() != 'S' )
		{
			types[i] = '2';
			groupList->note_type_2_block(i);
		}
		if ( groupList->bankPresent(i) )
			bankActive[i] = 'y';
		else
			bankActive[i] = 'n';
	}
	bankActive[8] = '\0';
	pageLog = fopen(pageFn, "at");            // append, text
	if (pageLog == NULL)
		shouldNever("Page Log Open Failed");
	aid = fList->getList();

	_settextcolor(color_norm);
	sprintf(tmp, "%-*.*s", 80 - 35, 80 - 35, getSiteName());
	_settextposition(STATROW + 1, 35);
	_outtext(tmp);
	drawBar(PROMPTBAR, " Command / Response Area ");
	myScanner->goPark();
}
void TrunkSys::note_site(unsigned short newSite)
{
	if (newSite != siteId && (fSite.filter(newSite) == newSite) && sysId != 0x1234)
	{
		if ( siteName )
		{
			// changing to alternate site in same system
			delete siteName;
			siteName = 0;
		}
		delete fList;
		fList = 0;
		delete myLogWin;
		myLogWin = new scrlog(5, 36);                        // start with "old" settings
		initF(sysId, newSite);
		if (!myLogWin)
			shouldNever("alloc of log window failed");
		aid = fList->getList();
		myLogWin->autoSize(naid);
		_settextcolor(color_norm);
		siteName = strdup(fList->getSysName());
		sprintf(tmp, "%-*.*s", 80 - 35, 80 - 35, siteName);
		_settextposition(STATROW + 1, 35);
		_outtext(tmp);
		siteId = newSite;
		networkable = FALSE;
		networked = FALSE;
		basicType = "unknown";
		subType = 0;
		conTone = 0;
		oneChType = 0;
		memset(newFreqs, 0, sizeof(newFreqs));
		memset(neighbors, -1, sizeof(neighbors));
	}
}

TrunkSys::~TrunkSys()
{

	myScanner->topIs(0);
	if (doAuto)
		doSave();
	delete groupList;
	delete idList;
	delete patchList;
	delete fList;
	delete sysName;
	delete siteName;
	delete msgPtr;
	delete myLogWin;

	if ( pageLog )
		fclose(pageLog);

	_settextcolor(color_quiet);
	_settextposition(STATROW + 1, 1);
	_outtext("SysId: ____ Type: ________ Title:                                        ");
	sprintf(tempArea, "%-*.*s", sizeof(sysPrefix) - 1, sizeof(sysPrefix) - 1, "");
	QuickOut(STATROW + 1, 8, color_norm, tempArea);
	sprintf(tmp, "%-*.*s", 31, 31, "");
	_settextposition(STATROW + 1, 35);
	_outtext(tmp);
	_settextposition(STATROW + 1, 19);
	_outtext("        ");
}
void
TrunkSys::doSave()
{
	groupList->doSave(grpFn);
	idList->doSave(idsFn);
	fList->doSave(sysFn, &types[0], vhf_uhf);
	patchList->doSave(patchFn);
	switch (sysFn[4])
	{
	case 'r':
	case 'R':
	case 'c':
	case 'C':
		fList->doSave(oldFn, &types[0], vhf_uhf, FALSE, sysName);                     // save without freqs
		break;
	default:
		break;
	}
	_settextposition(FBROW, 1);
	cprintf("Saved %d Groups, %d Radios, %d Patches                     ", groupList->entries(),
	        idList->entries(),
	        patchList->entries());
}
void
TrunkSys::unsafe()
{
	if (doAuto)
	{
		doAuto = FALSE;
		_settextposition(STATROW + 1, 12);
		_settextcolor(color_norm);
		_outtext("u");

	}
}
struct patchParm
{
	designation *inpD;
	patchedGroups *outD;
};
static void
patchIter(patchedGroups *pg, void *f)
{
	struct patchParm *pp = (struct patchParm*)f;

	if ( pp->outD )
		return;                         // found it already
	if ( pg->theGroups.contains(pp->inpD->theCode))
		pp->outD = pg;
}
designation *
TrunkSys::MapPatchDesig(register designation *d)
{
	patchParm pp;

	pp.inpD = d;
	pp.outD = 0;
	patchList->forAll(&patchIter, (void*)&pp);
	if ( pp.outD )
		return pp.outD;
	else
		return d;
}
const char *
TrunkSys::timeString()
{
	if ( strtime != now )
	{
		strtime = now;
		_ctime(&now, timeArray);
		char *t = &timeArray[0];
		char *t2;
		t = strchr(t, ' ');
		if ( t )
			++t;
		else
			t = &timeArray[0];
		t2 = strrchr(t, ' ');
		if ( t2 )
			*(t2 + 1) = '\0';
		strcpy(timeArray, t);
	}
	return timeArray;
}

void
TrunkSys::logStringT(register char *cp, InfoType it)
{
	logString(cp, userChoices.desiredAction(it));
}
void
TrunkSys::logString(register char *cp, InfoAction ia)
{
	register const char *tp = this->timeString();
	register unsigned int tl = strlen(tp) + 1;
	register unsigned int cl = strlen(cp);
	char myTime[9];
	char linebuffer[85];


	if ( (ia == DISK || ia == BOTH) && (pageLog != NULL) )
	{
		// first log whole thing to file
		fputs(tp, pageLog);
		fputc(' ', pageLog);
		fputs(cp, pageLog);
		fputc('\n', pageLog);
	}
	if ( (ia == SCREEN || ia == BOTH) )
	{
		// then use shorter form to screen
		strncpy(myTime, &tp[7], 8);
		myTime[8] = 0;
		sprintf(linebuffer, "%s %-*.*s", myTime, ROWAREA - 10, ROWAREA - 10, cp);
		myLogWin->addString(linebuffer);
	}

}
struct patchCparm
{
	int patchLifeLimit;
};
static void
patchClean(patchedGroups *pg, void *f)
{
	struct patchCparm *pp = (struct patchCparm*)f;

	if (!pg->theGroups.isEmpty() && ((now - pg->lastNoted) > pp->patchLifeLimit))
	{
		if (userChoices.desiredAction(FORMAT) == HEX)
			sprintf(tempArea, "patch id #%.4hX not present.", pg->theCode);
		else
			sprintf(tempArea, "patch id %5hu not present.", pg->theCode);
		mySys->logStringT(tempArea, MEMBERS);
		pg->theGroups.clear();
	}
	if (!pg->theGroups.isEmpty() )
		mySys->patchesActive = TRUE;
}
void
TrunkSys::clean_stale_patches()
{
	if ( !patchList->isEmpty())
	{
		patchCparm tp;

		switch (userChoices.desiredAction(LIFETIME))
		{
		case SHORTLIFE:
			tp.patchLifeLimit = 3;
			break;
		case LONGLIFE:
			tp.patchLifeLimit = 10;
			break;
		default:
			return;                                     // never clean patches
		}
		patchesActive = FALSE;
		patchList->forAll(&patchClean, (void*)&tp);
	}
}
struct patchMparm
{
	unsigned short tagOfNewLoc;
	designation *pgrp;
};
static void
patchOmit(patchedGroups *pg, void *f)
{
	struct patchMparm *pp = (struct patchMparm*)f;

	if ( pg->theCode != pp->tagOfNewLoc )
	{
		if ( !pg->theGroups.isEmpty())
		{
			if (pg->theGroups.contains(pp->pgrp->theCode))
			{
				pg->delGroup(pp->pgrp);
				if (userChoices.desiredAction(FORMAT) == HEX)
					sprintf(tempArea, "Remove from Patch/MSEL #%.4hX, group #%.4hX(%s)", pg->theCode, pp->pgrp->theCode, pp->pgrp->titleString());
				else
					sprintf(tempArea, "Remove from Patch/MSEL %5hu, group %5hu(%s)", pg->theCode, pp->pgrp->theCode, pp->pgrp->titleString());
				mySys->logStringT(tempArea, MEMBERS);
			}
		}
	}
}
void
TrunkSys::note_patch(unsigned short patchTag, unsigned short grpId, char bankType, BOOL msel)
{
	patchedGroups *pg;
	unsigned short key = patchTag;

	unsigned short theBank = (unsigned short)((grpId >> 13) & 7);
	char theType = types[theBank];

	// 3.8.3 beta 2: if a type 2 patch is active when first hit the system, don't jump to conclusions that
	// it is a type 1 bank - it could also be type 2, since the same patch code is used.

	if ( (theType == '?' && bankType != 'A') || isTypeI(theType) )
		types[theBank] = bankType;
	else if ( isHybrid(theType) )
		types[theBank] = makeHybrid(bankType);
	register designation *grp = getGroupDes(grpId, bankType);             // will map to non-flag version
	grp->lastNoted = now;
	patchesActive = TRUE;
	pg = patchList->find(key);
	if (!pg)
	{
		// not such collection now, make a new one
		pg = new patchedGroups((unsigned short)patchTag, *grp);
		if (!pg)
			shouldNever("failed to allocate class patchedGroups!");
		patchList->insert(pg->theCode, pg);

		// log creation of new patch
		if ( msel )
		{
			if (userChoices.desiredAction(FORMAT) == HEX)
				sprintf(tempArea, "New MSEL id #%.4hX, group #%.4hX(%s)", (WORD)patchTag, (WORD)grpId, grp->titleString());
			else
				sprintf(tempArea, "New MSEL id %5hu, group %5hu(%s)", (WORD)patchTag, (WORD)grpId, grp->titleString());
		}
		else
		{
			if (userChoices.desiredAction(FORMAT) == HEX)
				sprintf(tempArea, "New Patch id #%.4hX, group #%.4hX(%s)", (WORD)patchTag, (WORD)grpId, grp->titleString());
			else
				sprintf(tempArea, "New Patch id %5hu, group %5hu(%s)", (WORD)patchTag, (WORD)grpId, grp->titleString());
		}
		logStringT(tempArea, MEMBERS);
	}
	else
	{
		if ( !pg->theGroups.contains(grp->theCode) )                    // now see if already mapped
		{
			pg->addGroup(grp);                                          // now can map to current assignment!

			// log addition of group to patch

			if (msel)
			{
				if (userChoices.desiredAction(FORMAT) == HEX)
					sprintf(tempArea, "Add2MSEL id #%.4hX, group #%.4hX(%s)", (WORD)patchTag, (WORD)grpId, grp->titleString());
				else
					sprintf(tempArea, "Add2MSEL id %5hu, group %5hu(%s)", (WORD)patchTag, (WORD)grpId, grp->titleString());
			}
			else
			{
				if (userChoices.desiredAction(FORMAT) == HEX)
					sprintf(tempArea, "Add2Patch id #%.4hX, group #%.4hX(%s)", (WORD)patchTag, (WORD)grpId, grp->titleString());
				else
					sprintf(tempArea, "Add2Patch id %5hu, group %5hu(%s)", (WORD)patchTag, (WORD)grpId, grp->titleString());
			}
			logStringT(tempArea, MEMBERS);
		}
	}
	pg->lastNoted = now;
	// check for existence in other patch groups... and remove

	struct patchMparm pmp;
	pmp.tagOfNewLoc = pg->theCode;
	pmp.pgrp = grp;
	patchList->forAll(&patchOmit, (void*)&pmp);
}
struct mselCparm
{
	BOOL found;
	unsigned short ownerId;
};
void
mselCan(patchedGroups *pg, void *f)
{
	struct mselCparm *pp = (struct mselCparm*)f;

	if ( pp->found || pg->theGroups.isEmpty())
		return;
	if (pg->theGroups.contains(pp->ownerId))
	{
		pp->found = TRUE;
		if (userChoices.desiredAction(FORMAT) == HEX)
			sprintf(tempArea, "MSEL id #%.4hX closed.", pg->theCode);
		else
			sprintf(tempArea, "MSEL id %5hu closed.", pg->theCode);
		mySys->logStringT(tempArea, MEMBERS);
		pg->theGroups.clear();
	}
}
void
TrunkSys::mscancel(unsigned short grpId)
{
	designation *thegrp = getGroupDes(grpId);

	if ( !patchList->isEmpty())
	{
		mselCparm tp;
		tp.found = FALSE;
		tp.ownerId = thegrp->theCode;
		patchList->forAll(&mselCan, (void*)&tp);
	}
}

void
TrunkSys::note_page(unsigned short toId, unsigned short fromId)
{
	static time_t prevTime = 0;
	static unsigned short lastPfrom = 0;
	static unsigned short lastPto = 0;

	// if duplicate page occurrence (normally all OSWs are repeated several
	// times for redundancy against errors...) suppress;

	if ( lastPfrom == fromId && lastPto == toId && ((now - prevTime) < 30))
		return;
	lastPfrom = fromId;
	lastPto = toId;
	prevTime = now;

	designation *fromD = getIndivDes(fromId);
	designation *toD = getIndivDes(toId);
	fromD->lastNoted = now;
	toD->lastNoted = now;
	if ( !filterLog || fromD->getPrio() <= scanThreshold ||
	     toD->getPrio() <= scanThreshold )
	{
		if (userChoices.desiredAction(FORMAT) == HEX)
			sprintf(tempArea, "%*.*s #%.4hX Pg-> #%.4hX %-*.*s", TITLESIZE, TITLESIZE, fromD->titleString(), fromId, toId, TITLESIZE, TITLESIZE, toD->titleString());
		else
			sprintf(tempArea, "%*.*s %5hu Pg-> %5hu %-*.*s", TITLESIZE, TITLESIZE, fromD->titleString(), fromId, toId, TITLESIZE, TITLESIZE, toD->titleString());
		logStringT(tempArea, CALLALERT);
	}
}
void
TrunkSys::note_text_msg(unsigned short toId, const char *typ, unsigned short val)
{
	static time_t prevTime = 0;
	static unsigned short lastPto = 0;
	static unsigned short lastVal = 0;
	static char lastTyp = '_';

	// if duplicate page occurrence (normally all OSWs are repeated several
	// times for redundancy against errors...) suppress;

	if ( (lastPto == toId) && (lastVal == val) && (lastTyp == *typ) && ((now - prevTime) < 30))
		return;
	lastPto = toId;
	prevTime = now;
	lastVal = val;
	lastTyp = *typ;

	designation *toD = getIndivDes(toId);
	toD->lastNoted = now;
	if ( !filterLog || toD->getPrio() <= scanThreshold )
	{
		if (userChoices.desiredAction(FORMAT) == HEX)
			sprintf(tempArea, "%*.*s <from> #%.4hX %-*.*s", TITLESIZE, TITLESIZE, msgPtr->getMsgText(*typ, ++val), toId, TITLESIZE, TITLESIZE, toD->titleString());
		else
			sprintf(tempArea, "%*.*s <from> %5hu %-*.*s", TITLESIZE, TITLESIZE, msgPtr->getMsgText(*typ, ++val), toId, TITLESIZE, TITLESIZE, toD->titleString());
		logStringT(tempArea, CALLALERT);
	}
}
void
TrunkSys::note_page(unsigned short toId)
{
	static time_t prevTime = 0;
	static unsigned short lastPto = 0;

	// if duplicate page occurrence (normally all OSWs are repeated several
	// times for redundancy against errors...) suppress;

	if ( (lastPto == toId) && ((now - prevTime) < 30))
		return;
	lastPto = toId;
	prevTime = now;

	designation *toD = getIndivDes(toId);
	toD->lastNoted = now;
	if (!filterLog || toD->getPrio() <= scanThreshold )
	{
		if (userChoices.desiredAction(FORMAT) == HEX)
			sprintf(tempArea, "%*.*s      Pg-> #%.4hX %-*.*s", TITLESIZE, TITLESIZE, "", toId, TITLESIZE, TITLESIZE, toD->titleString());
		else
			sprintf(tempArea, "%*.*s      Pg-> %5hu %-*.*s", TITLESIZE, TITLESIZE, "", toId, TITLESIZE, TITLESIZE, toD->titleString());
		logStringT(tempArea, CALLALERT);
	}
}
void
TrunkSys::noteEmergencySignal(unsigned short radioId)
{
	static time_t prevTime = 0;
	static unsigned short lastSignal = 0;

	// if duplicate page occurrence (normally all OSWs are repeated several
	// times for redundancy against errors...) suppress;

	if ( lastSignal == radioId &&  ((now - prevTime) < 30))
		return;
	lastSignal = radioId;
	prevTime = now;

	designation *d = getIndivDes(radioId);
	d->lastNoted = now;
	if (userChoices.desiredAction(FORMAT) == HEX)
		sprintf(tempArea, "%*.*s #%.4hX Emergency Signal!", TITLESIZE, TITLESIZE, d->titleString(), radioId);
	else
		sprintf(tempArea, "%*.*s %5hu Emergency Signal!", TITLESIZE, TITLESIZE, d->titleString(), radioId);
	logStringT(tempArea, EMERG);
}

void
TrunkSys::note_type_2_block(unsigned short blknum)
{
	// check for frequencies using that block as a group call and with
	// attributes set. truncate and re-assign.

	register int i;                                             // iterator for known frequencies
	register frequencyAssignment *raid;                         // for optimization, points to each object in turn

	for (i = 0, raid = aid; i < naid; ++raid, ++i)              // one pass over ALL objects
	{
		if ( raid->fState == frequencyAssignment::FREQCALL ||
		     raid->fState == frequencyAssignment::FREQHOLD )
		{
			if ( raid->destDes->desType == desGroup
			     && raid->destDes->blockNum() == blknum
			     && raid->destDes->flags())
				raid->destDes = groupList->get((unsigned short)((raid->destDes->theCode) & ~0x000f));
		}
	}
	// wipe out any group IDs in that block with the attribute set and
	// delete the group ID.

	groupList->note_type_2_block(blknum);
}


frequencyAssignment *
TrunkSys::expandFreqs(unsigned short freqnumber, int thresh)
{
	// assign new frequency in sorted order

	register short i;                           // iterator for known frequencies
	register frequencyAssignment *raid;         // for optimization, points to each object in turn

	int numTimes;

	numTimes = newFreqs[freqnumber];
	if ( numTimes )
	{
		if (++numTimes >= thresh)
		{
			if ( naid < MAXFREQS)
			{
				// make room by squeezing down the log area by one line
				myLogWin->reshape(myLogWin->getNrows() - 1,
				                  (unsigned short)(myLogWin->getStartRow() + 1));
				newFreqs[freqnumber] = 0;
				for (i = 0, raid = aid; i < naid; ++raid, ++i)                                             // one pass over ALL objects
				{
					if (raid->freqNum > freqnumber)
					{
						// working backwords, boost later ones
						register int j;
						register frequencyAssignment *prev;
						for ( raid = aid + naid, j = naid, prev = raid - 1;
						      j > i; --j, --raid, --prev)
						{
							*raid = *prev;
							++(raid->y);
							raid->show();
							prev->clobber();
						}
						break;
					}

				}
				raid->create(i, freqnumber, -1, 0, bandPlan);
				// assign screen position & freq,auto-displays
				myScanner->setFreqString(raid);
				++naid;
				return raid;
			}
			else
			{
				QuickOut(FBROW, 1, color_norm, "Too Many Frequencies                                      ");
				return 0;
			}
		}
		else
		{
			++newFreqs[freqnumber];
			return 0;
		}
	}
	else
	{
		newFreqs[freqnumber] = 1;
		return 0;
	}
}

void
TrunkSys::note_affiliation(unsigned short radioId, unsigned short groupId)
{
	static time_t lastAffil = 0;
	static unsigned short lastRadio = 0;
	static unsigned short lastGroup = 0;

	if (lastRadio != radioId || lastGroup != groupId || ((now - lastAffil) > 5) )
	{
		register designation *toDes = groupId ? getGroupDes(groupId) : 0;
		register designation *fromDes = getIndivDes(radioId);
		fromDes->lastNoted = now;
		if ( toDes)
		{
			toDes->lastNoted = now;
			fixNew(fromDes, toDes);
		}

		// filter so we don't do extra lookups...

		lastRadio = radioId;
		lastGroup = groupId;
		lastAffil = now;
		char *t1, *t2;
		t1 = fromDes->titleString();
		if ( toDes)
			t2 = toDes->titleString();
		// optional data logging:
		if (!filterLog || fromDes->getPrio() <= scanThreshold ||
		    (toDes && toDes->getPrio() <= scanThreshold) )
		{
			if (userChoices.desiredAction(FORMAT) == HEX)
			{
				if ( toDes )
					sprintf(tempArea, "%*.*s #%.4hX Af-> #%.4hX %-*.*s", TITLESIZE, TITLESIZE, t1, fromDes->theCode, toDes->theCode, TITLESIZE, TITLESIZE, t2);
				else
					sprintf(tempArea, "%*.*s #%.4hX Unaf      %-*.*s", TITLESIZE, TITLESIZE, t1, fromDes->theCode, TITLESIZE, TITLESIZE, "");
			}
			else
			{
				if ( toDes )
					sprintf(tempArea, "%*.*s %5hu Af-> %5hu %-*.*s", TITLESIZE, TITLESIZE, t1, fromDes->theCode, toDes->theCode, TITLESIZE, TITLESIZE, t2);
				else
					sprintf(tempArea, "%*.*s %5hu Unaf      %-*.*s", TITLESIZE, TITLESIZE, t1, fromDes->theCode, TITLESIZE, TITLESIZE, "");
			}
			logStringT(tempArea, AFFIL);
		}
	}
}
void
TrunkSys::noteDataChan(unsigned short dataFreq)
{

	register int i;                             // iterator for known frequencies
	register frequencyAssignment *raid;         // for optimization, points to each object in turn
	BOOL foundIt;

	for (i = 0, raid = aid, foundIt = FALSE; i < naid; ++raid, ++i)           // one pass over ALL objects
	{
		if ( raid->freqNum == dataFreq)
		{
			raid->assignD();
			if ( bandPlan == '-')
			{
				if ( raid->freqdisp[3] == '8' )
					setBandPlan('8');
				else if ( raid->freqdisp[3] == '9' )
					setBandPlan('9');
				else
					setBandPlan('0');
			}
			foundIt = TRUE;
		}
		else if ( raid->fState == frequencyAssignment::FREQDATA )
			raid->unassign();
	}
	if (!foundIt)
		expandFreqs(dataFreq, 2);
}
void
TrunkSys::noteAltDchan(unsigned short dataFreq)
{

	register int i;                             // iterator for known frequencies
	register frequencyAssignment *raid;         // for optimization, points to each object in turn
	BOOL foundIt;

	for (i = 0, raid = aid, foundIt = FALSE; i < naid && !foundIt; ++raid, ++i)           // search for match
	{
		if ( raid->freqNum == dataFreq)
		{
			if ( raid->freqdisp[1] == ' ')
				raid->freqdisp[1] = 'a';
			foundIt = TRUE;
		}
	}
	if (!foundIt)
		expandFreqs(dataFreq, 2);
}

designation *
TrunkSys::getGroupDes(unsigned short theId, char bankType)
{
	char theType = types[(theId >> 13) & 7];

	if (theType == '2')
		theId &= 0xfff0;
	else if (theType >= 'A' && theType <= 'N' )
	{
		if ( bankType && bankType != theType )
			types[(theId >> 13) & 7] = theType = bankType;
		theId &= ~idMask[theType - 'A'];
	}
	else if (theType >= 'a' && theType <= 'n')
	{
		if ( bankType )
		{
			bankType = (char)('a' + (bankType - 'A'));
			if ( bankType != theType )
				types[(theId >> 13) & 7] = theType = bankType;
		}
		theId &= ~idMask[theType - 'a'];
	}
	return groupList->get(theId);
}
designation *
TrunkSys::getIndivDes(unsigned short theId)
{
	return idList->get(theId);
}
designation *
TrunkSys::getPatchDes(unsigned short theId, BOOL retNull)
{
	return (designation*)patchList->get(theId, retNull);
}
void
TrunkSys::noteCwId(unsigned short cwFreq, BOOL onoff)
{

	register int i;                             // iterator for known frequencies
	register frequencyAssignment *raid;         // for optimization, points to each object in turn
	BOOL foundIt = FALSE;

	for (i = 0, raid = aid; i < naid; ++raid, ++i)           // one pass over ALL objects
	{
		if ( raid->freqNum == cwFreq || raid->inpFreqNum == cwFreq)
		{
			raid->assignCW(onoff);
			foundIt = TRUE;
			break;
		}
	}
	if ( !foundIt )
		expandFreqs(cwFreq, 2);                         // can't filter much since command not repeated
}
frequencyAssignment *
TrunkSys::findHeldCall()
{
	//      traverses all assigned frequencies, checking for changed or outdated entries.
	//      should be called when a dirty entry exists or at least a couple times per second.
	//      will analyze priorities and redirect scanner frequency if appropriate.

	register int i;                                             // iterator for known frequencies
	register frequencyAssignment *raid;                         // for optimization, points to each object in turn

	for (i = 0, raid = aid; i < naid; ++raid, ++i)              // one pass over ALL objects
	{
		if ( raid->fState == frequencyAssignment::FREQCALL ||
		     raid->fState == frequencyAssignment::FREQHOLD )
		{
			// selected: pull down if better or equal priority than previous best,
			// even if in delay mode.

			if (raid->destDes == searchDes1 || raid->callingDes == searchDes1 ||
			    (searchDes2 && raid->destDes == searchDes2) )
			{
				if ( raid->getPrio() <= searchPrty )
					searchPrty = raid->getPrio();
				else
					raid->setPrio(searchPrty);
				return raid;
			}
		}
	}
	searchDes1 = searchDes2 = 0;
	searchPrty = 100;
	return 0;
}
void
TrunkSys::endScanSearch()
{

	//      traverses all assigned frequencies, checking for changed or outdated entries.
	//      should be called when a dirty entry exists or at least a couple times per second.
	//      will analyze priorities and redirect scanner frequency if appropriate.

	register int i;                                     // iterator for known frequencies
	register frequencyAssignment *raid;                 // for optimization, points to each object in turn
	register unsigned char prtyMax = 100;
	register frequencyAssignment *topFreq = 0;

	if ( searchDes1 )
	{
		topFreq = findHeldCall();                               // find the one we have been 'watching'
		prtyMax = searchPrty;
	}
	for (i = 0, raid = aid; i < naid; ++raid, ++i)              // one pass over ALL objects
	{
		if ( raid->fState == frequencyAssignment::FREQCALL ||
		     raid->fState == frequencyAssignment::FREQHOLD )
		{
			if (raid->visited )
				raid->freshen();
			else
				raid->noteInactive();
		}
		if ( raid->dirty )
			raid->show();

		raid->selected = FALSE;

		if (topFreq && (raid == topFreq))
		{
			// we have already covered this one
		}
		else
		{
			// not selected: pull down if better priority and not in delay mode
			if ( raid->fState == frequencyAssignment::FREQCALL  && raid->getPrio() < prtyMax
			     && raid->isScannable() )                                      // only jump here if can actually scan to it!
			{
				prtyMax = raid->getPrio();
				topFreq = raid;
			}
		}
		raid->visited = FALSE;
	}
	if (prtyMax <= scanThreshold)
	{
		searchDes1 = topFreq->destDes;
		searchDes2 = topFreq->callingDes;
		searchPrty = prtyMax;
		topFreq->selected = TRUE;
		if ( topFreq->fState == frequencyAssignment::FREQCALL || !antiScannerTones )
			myScanner->topIs(topFreq);
		else
			myScanner->topIs(0);                            // push to idle channel
	}
	else
		myScanner->topIs(0);                            // push to idle channel
}
void
TrunkSys::endScanHold()
{

	//      traverses all assigned frequencies, checking for changed or outdated entries.
	//      should be called when a dirty entry exists or at least a couple times per second.
	//      will analyze priorities and redirect scanner frequency if appropriate.

	register int i;                                     // iterator for known frequencies
	register frequencyAssignment *raid;                 // for optimization, points to each object in turn
	register frequencyAssignment *topFreq = 0;

	for (i = 0, raid = aid; i < naid; ++raid, ++i)              // one pass over ALL objects
	{
		if ( raid->fState == frequencyAssignment::FREQCALL ||
		     raid->fState == frequencyAssignment::FREQHOLD )
		{
			if (raid->visited )
				raid->freshen();
			else
				raid->noteInactive();
		}
		if ( raid->dirty )
			raid->show();

		// simple algorithm for hold mode: either an entry matches the held item or not.

		switch (state)
		{
		case HoldGrp:
		case HoldPatch:
			if ( raid->destDes == holdDes1 )
				topFreq = raid;
			break;
		case Hold1Indiv:
			if ( raid->destDes == holdDes1 || raid->callingDes == holdDes1 )
				topFreq = raid;
			break;
		case Hold2Indiv:
			if ((raid->destDes == holdDes1 && raid->callingDes == holdDes2) ||
			    (raid->destDes == holdDes2 && raid->callingDes == holdDes1) )
				topFreq = raid;
			break;
		}

		raid->selected = FALSE;
		raid->visited = FALSE;
	}
	if (topFreq)
	{
		topFreq->selected = TRUE;
		if ( topFreq->fState == frequencyAssignment::FREQCALL || !antiScannerTones )
			myScanner->topIs(topFreq);
		else
			myScanner->topIs(0);                            // push to idle channel
	}
	else
		myScanner->topIs(0);                            // push to idle channel
}
unsigned short
TrunkSys::getRecentGroup()
{
	switch (state)
	{
	case Searching:
		if (searchDes1 && (searchDes1->desType == desGroup))
			return searchDes1->theCode;
		else
			return 0;
	case HoldGrp:
		return holdDes1->theCode;
	case HoldPatch:
	case Hold1Indiv:
	case Hold2Indiv:
		return 0;
	}
	return 0;
}
unsigned short
TrunkSys::getRecentRadio()
{
	switch (state)
	{
	case Searching:
		if (searchDes1 && (searchDes1->desType == desIndiv))
			return searchDes1->theCode;
		else if (searchDes2)                            // icall2 - return the radio
			return searchDes2->theCode;
		return 0;
	case HoldGrp:
	case HoldPatch:
	{
		register int i;
		register frequencyAssignment *raid = aid;

		for (i = 0; i < naid; ++i, ++raid)
		{
			// if one is active, see which radio it was.
			if ( raid->selected )
			{
				if (raid->callingDes->desType == desIndiv)
					return raid->callingDes->theCode;
				break;
			}
		}
	}
		return 0;
	case Hold1Indiv:
	case Hold2Indiv:
		return holdDes1->theCode;
	}
	return 0;
}
void
TrunkSys::noneActive()
{
	register int i;                                         // iterator for known frequencies
	register frequencyAssignment *raid;                     // for optimization, points to each object in turn

	for (i = 0, raid = aid; i < naid; ++raid, ++i)          // one pass over ALL objects
		if ( raid->fState == frequencyAssignment::FREQCALL)
			raid->visited = FALSE;
	endScan();
}
void
TrunkSys::type0Icall(unsigned short freq, unsigned short dest, unsigned short talker,
                     BOOL isdigital, BOOL isfone, BOOL encry)
{
	register frequencyAssignment *f = getChannel(freq);

	if (!f)
		return;                                                                 // this channel is new and not yet filtered, ignore call
	designation *d1, *d2;
	if ( Verbose )
	{
		d1 = getIndivDes(dest);
		d1->lastNoted = now;
		d2 = getIndivDes(talker);
		d2->lastNoted = now;
		d2->setDispMode('0');
	}
	else
	{
		d1 = getIndivDes(talker);
		d1->lastNoted = now;
		d2 = 0;
	}
	d1->setDispMode('0');
	f->flags = 0;
	f->setMaxType(Icall, d1, d2);                               // set up size and type of call, primary participants
	if ( isdigital )
		f->flags |= OTHER_DIGITAL;
	if (encry)
		f->flags |= 8;
	if (isfone)
		f->flags |= PHONE_PATCH;
	f->visited = TRUE;
}
void
TrunkSys::type0Icall(unsigned short freq, unsigned short talker,
                     BOOL isdigital, BOOL isfone, BOOL encry)
{
	register frequencyAssignment *f = getChannel(freq);

	if (!f)
		return;                                                                 // this channel is new and not yet filtered, ignore call
	designation *d1 = getIndivDes(talker);

	d1->lastNoted = now;
	d1->setDispMode('0');
	if ( f->getMaxType() == Icall )
	{
		if ( f->destDes != d1 )
		{
			if ( !(f->callingDes) )
			{
				f->flags = 0;
				f->setMaxType(Icall, f->destDes, d1);
				if ( isdigital )
					f->flags |= OTHER_DIGITAL;
				if (isfone)
					f->flags |= PHONE_PATCH;
				if (encry)
					f->flags |= 8;

			}
			else if (f->callingDes != d1)
			{
				f->unassign();
				f->flags = 0;
				f->setMaxType(Icall, d1);
				if ( isdigital )
					f->flags |= OTHER_DIGITAL;
				if (isfone)
					f->flags |= PHONE_PATCH;
				if (encry)
					f->flags |= 8;
			}
		}
	}
	else
	{
		f->unassign();
		f->flags = 0;
		f->setMaxType(Icall, d1);                               // set up size and type of call, primary participants
		if ( isdigital )
			f->flags |= OTHER_DIGITAL;
		if (isfone)
			f->flags |= PHONE_PATCH;
		if (encry)
			f->flags |= 8;
	}
	f->visited = TRUE;
}

void
TrunkSys::type2Gcall(unsigned short freq, unsigned short dest, unsigned short talker, unsigned short flags)
{
	register frequencyAssignment *f = getChannel(freq);

	if (!f)
		return;                                                                 // this channel is new and not yet filtered, ignore call
	designation *d1 = getGroupDes(dest);
	designation *d2;
	unsigned short fl = (unsigned short)(flags & ((unsigned short)7));

	if ( fl == 3 || fl == 4 || fl == 5 || fl == 7 )             // ignore encrypt bit for this
	{
		d1 = mapPatchDesig(d1);                                 // may point elsewhere or same
		if ( d1->desType == desGroup )
			d1->setDispMode('2');
		else
			d1->setDispMode('0');

	}
	else
		d1->setDispMode('2');
	d1->lastNoted = now;
	if (Verbose)
	{
		d2      = getIndivDes(talker);
		d2->lastNoted = now;
		d2->setDispMode('0');
		fixNew(d2, d1);                                                                 // if new radio, note where it was found
	}
	else
		d2 = 0;
	f->flags = flags;
	f->setMaxType(Gcall, d1, d2);                               // set up size and type of call, primary participants
	f->visited = TRUE;
}
void
TrunkSys::type2Gcall(unsigned short freq, unsigned short dest, unsigned short flags)
{
	register frequencyAssignment *f = getChannel(freq);

	if (!f)
		return;                                                                 // this channel is new and not yet filtered, ignore call
	designation *d1 = getGroupDes(dest);
	unsigned short fl = (unsigned short)(flags & ((unsigned short)7));

	if ( fl == 3 || fl == 4 || fl == 5 || fl == 7)              // ignore encrypt bit for this
	{
		d1 = mapPatchDesig(d1);                                 // may point elsewhere or same
		if ( d1->desType == desGroup )
			d1->setDispMode('2');
		else
			d1->setDispMode('0');
	}
	else
		d1->setDispMode('2');
	d1->lastNoted = now;
	if ( f->getMaxType() == Gcall )
	{
		if ( f->destDes != d1 )
		{
			f->unassign();
			f->flags = flags;
			f->setMaxType(Gcall, d1);
		}
	}
	else
	{
		f->unassign();
		f->flags = flags;
		f->setMaxType(Gcall, d1);                               // set up size and type of call, primary participants
	}
	f->visited = TRUE;
}
void
TrunkSys::type0Gcall(unsigned short freq, unsigned short dest, unsigned short talker, unsigned short flags)
{
	register frequencyAssignment *f = getChannel(freq);
	designation *d1;

	if (!f)
		return;                                                                 // this channel is new and not yet filtered, ignore call
	if ( (flags & 7) == 3 || (flags & 7) == 4 )
	{
		d1 = getPatchDes(dest, TRUE);
		if ( !d1 )
			d1 = getGroupDes(dest);
	}
	else
		d1 = getGroupDes(dest);
	d1->lastNoted = now;
	designation *d2;

	if ( Verbose )
	{
		d2      = getIndivDes(talker);
		d2->lastNoted = now;
		d1->setDispMode('0');
		d2->setDispMode('0');
		fixNew(d2, d1);                                                                 // if new radio, note where it was found
	}
	else
	{
		d2      = 0;
		d1->setDispMode('0');
	}
	f->flags = flags;
	f->setMaxType(Gcall, d1, d2);                               // set up size and type of call, primary participants
	f->visited = TRUE;
}
void
TrunkSys::type0Gcall(unsigned short freq, unsigned short dest, unsigned short flags)
{
	register frequencyAssignment *f = getChannel(freq);
	designation *d1;

	if (!f)
		return;                                                                 // this channel is new and not yet filtered, ignore call
	if ( (flags & 7) == 3 || (flags & 7) == 4 )
	{
		d1 = getPatchDes(dest, TRUE);
		if (!d1)
			d1 = getGroupDes(dest);
	}
	else
		d1 = getGroupDes(dest);
	d1->lastNoted = now;
	d1->setDispMode('0');
	if ( f->getMaxType() == Gcall )
	{
		if ( f->destDes != d1 )
		{
			f->unassign();
			f->flags = flags;
			f->setMaxType(Gcall, d1);
		}
		// else nothing since group matches & no id info available
	}
	else
	{
		f->unassign();
		f->flags = flags;
		f->setMaxType(Gcall, d1);                               // set up size and type of call, primary participants
	}
	f->visited = TRUE;
}
void
TrunkSys::hybridGcall(unsigned short freq, char bankType, unsigned short dest, unsigned short talker,
                      unsigned short talkAlias)
{
	register frequencyAssignment *f = getChannel(freq);

	if (!f)
		return;                                                                 // this channel is new and not yet filtered, ignore call
	designation *d1 = getGroupDes(dest);
	d1->lastNoted = now;
	designation *d2;
	designation *d3;

	d2 = mapPatchDesig(d1);
	if ( d2 == d1 )
		d1->setDispMode(bankType);
	else
	{
		d1 = d2;
		d2 = 0;
	}

	if ( Verbose )
	{
		d2 = getIndivDes(talker);
		d2->lastNoted = now;
		d2->setDispMode('2');
		d3 = getIndivDes(talkAlias);
		d3->lastNoted = now;
		d3->setDispMode(bankType);
		fixNew(d2, d1);                                                                 // if new radio, note where it was found
		fixType1(d3, d1);
	}
	else
	{
		d2 = 0;
		d3 = 0;
	}
	f->flags = 0;
	f->setMaxType(HybGcall, d1, d2, d3);                                // set up size and type of call, primary participants
	f->visited = TRUE;
}
void
TrunkSys::hybridGcall(unsigned short freq, char bankType, unsigned short dest, unsigned short talker)
{

	register frequencyAssignment *f = getChannel(freq);

	if (!f)
		return;                                                                 // this channel is new and not yet filtered, ignore call
	designation *d1 = getGroupDes(dest);
	d1->lastNoted = now;
	designation *d2;

	d2 = mapPatchDesig(d1);
	if ( d2 == d1 )
		d1->setDispMode(bankType);
	else
	{
		d1 = d2;
		d2 = 0;
	}

	if ( Verbose )
	{
		d2      = getIndivDes(talker);
		d2->lastNoted = now;
		d2->setDispMode('2');
		fixNew(d2, d1);                                                                 // if new radio, note where it was found
	}
	else
		d2 = 0;
	f->flags = 0;
	f->setMaxType(HybGcall, d1, d2, 0);                                 // set up size and type of call, primary participants
	f->visited = TRUE;
}
void
TrunkSys::typeIGcall(unsigned short freq, char bankType, unsigned short dest, unsigned short talker)
{

	register frequencyAssignment *f = getChannel(freq);

	if (!f)
		return;                                                                 // this channel is new and not yet filtered, ignore call
	designation *d1 = getGroupDes(dest);
	d1->lastNoted = now;
	designation *d2;

	d2 = mapPatchDesig(d1);
	if ( d2 == d1 )
		d1->setDispMode(bankType);
	else
	{
		d1 = d2;
		d2 = 0;
		d1->lastNoted = now;
	}
	if ( Verbose )
	{
		d2      = getIndivDes(talker);
		d2->lastNoted = now;
		d2->setDispMode('2');
		fixType1(d2, d1);                                                                       // if new radio, note where it was found
	}
	else
		d2 = 0;
	f->flags = 0;
	f->setMaxType(Gcall, d1, d2, 0);                            // set up size and type of call, primary participants
	f->visited = TRUE;
}
frequencyAssignment *
TrunkSys::getChannel(unsigned short freqnumber)
{
	register int i;                                         // iterator for known frequencies
	register frequencyAssignment *raid;                     // for optimization, points to each object in turn

	for (i = 0, raid = aid; i < naid; ++raid, ++i)          // one pass over ALL objects
		if ( raid->freqNum == freqnumber)
			return raid;
	return expandFreqs(freqnumber);
}
void TrunkSys::unAssociateId(designation *d)
{
	register int i;                                         // iterator for known frequencies
	register frequencyAssignment *raid;                     // for optimization, points to each object in turn

	for (i = 0, raid = aid; i < naid; ++raid, ++i)          // one pass over ALL objects
		if ( raid->disAssociateId(d) )
			return;
}
void
TrunkSys::disAssociateId(designation *d)
{
	mySys->unAssociateId(d);
}

