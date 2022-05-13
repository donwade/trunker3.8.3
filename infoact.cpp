#include <platform.h>
#include <infoact.h>

InfoControl::InfoControl()
{
	this->reset();
	// reset should set all defaults that are not IGNORE!
	updatePrompt();
}
InfoAction
InfoControl::desiredAction(InfoType i) const
{
	return (InfoAction)actions[((int)i) - 'a'];
}
const char *
InfoControl::getActionPrompt()
{
	return (const char*)promptstr;
}
void
InfoControl::toggleAction(InfoType i)
{
	int ii = ((int)i) - 'a';

	switch (actions[ii])
	{
	case IGNORE:    actions[ii] = SCREEN; break;
	case SCREEN:    actions[ii] = DISK; break;
	case DISK:              actions[ii] = BOTH; break;
	case BOTH:              actions[ii] = IGNORE; break;
	default:                actions[ii] = IGNORE; break;
	case SHORTLIFE: actions[ii] = LONGLIFE; break;
	case LONGLIFE:  actions[ii] = INFINLIFE; break;
	case INFINLIFE: actions[ii] = SHORTLIFE; break;
	case DECIMAL: actions[ii] = HEX; break;
	case HEX: actions[ii] = DECIMAL; break;
	case NORM: actions[ii] = REVERSE; break;
	case REVERSE: actions[ii] = NORM; break;
	}
	updatePrompt();
}
void
InfoControl::updatePrompt()
{
	char *outstr = promptstr;

	outstr += sprintf(promptstr, "Aff=%s", pAction((InfoAction)actions[(int)AFFIL - 'a']));
	outstr += sprintf(outstr, " Diag=%s", pAction((InfoAction)actions[(int)DIAGNOST - 'a']));
	outstr += sprintf(outstr, " Ptch=%s", pAction((InfoAction)actions[(int)MEMBERS - 'a']));
	outstr += sprintf(outstr, " Call=%s", pAction((InfoAction)actions[(int)CALLALERT - 'a']));
	outstr += sprintf(outstr, " Emrg=%s", pAction((InfoAction)actions[(int)EMERG - 'a']));
	outstr += sprintf(outstr, " Life=%s", pAction((InfoAction)actions[(int)LIFETIME - 'a']));
	outstr += sprintf(outstr, " Neigh=%s", pAction((InfoAction)actions[(int)NEIGHBORS - 'a']));
	outstr += sprintf(outstr, " Fmt=%s", pAction((InfoAction)actions[(int)FORMAT - 'a']));
	outstr += sprintf(outstr, " Scrl=%s", pAction((InfoAction)actions[(int)SCROLLDIR - 'a']));
}

const char *
InfoControl::pAction(InfoAction i)
{
	switch (i)
	{
	case IGNORE:    return "ign";
	case SCREEN:    return "scr";
	case DISK:              return "dsk";
	case BOTH:              return "bth";
	case SHORTLIFE: return "  3";
	case LONGLIFE:  return " 10";
	case INFINLIFE: return "inf";
	case DECIMAL: return "Dec";
	case HEX: return "Hex";
	case NORM: return "nrm";
	case REVERSE: return "rev";
	}
	return "???";
}

