#include <platform.h>
#include <infoact.h>

InfoControl::InfoControl()
{
    this->reset();
    // reset should set all defaults that are not _IGNORE!
    updatePrompt();
}


InfoAction InfoControl::desiredAction(InfoType i) const
{
    return (InfoAction)actions[((int)i) - 'a'];
}


const char *InfoControl::getActionPrompt()
{
    return (const char *)promptstr;
}


void InfoControl::toggleAction(InfoType i)
{
    int ii = ((int)i) - 'a';

    switch (actions[ii])
    {
    case _IGNORE:   actions[ii] = _SCREEN; break;

    case _SCREEN:   actions[ii] = _DISK; break;

    case _DISK:             actions[ii] = _BOTH; break;

    case _BOTH:             actions[ii] = _IGNORE; break;

    default:                actions[ii] = _IGNORE; break;

    case _SHORTLIFE: actions[ii] = _LONGLIFE; break;

    case _LONGLIFE: actions[ii] = _INFINLIFE; break;

    case _INFINLIFE: actions[ii] = _SHORTLIFE; break;

    case _DECIMAL: actions[ii] = _HEX; break;

    case _HEX: actions[ii] = _DECIMAL; break;

    case _NORM: actions[ii] = _REVERSE; break;

    case _REVERSE: actions[ii] = _NORM; break;
    }

    updatePrompt();
}


void InfoControl::updatePrompt()
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


const char *InfoControl::pAction(InfoAction i)
{
    switch (i)
    {
    case _IGNORE:   return "ign";

    case _SCREEN:   return "scr";

    case _DISK:             return "dsk";

    case _BOTH:             return "bth";

    case _SHORTLIFE:        return "  3";

    case _LONGLIFE:  return " 10";

    case _INFINLIFE: return "inf";

    case _DECIMAL: return "Dec";

    case _HEX: return "Hex";

    case _NORM: return "nrm";

    case _REVERSE: return "rev";
    }

    return "???";
}
