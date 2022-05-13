/*
 * dpmiutil.c - DPMI utilities (implementation)
 */

#include "dpmiutil.h"
#include <i86.h>
int DU_alloc_dosmem
(
    DU_WORD		paragraph_count_,
    DU_WORD *	segment_address_,
    DU_WORD *	selector_
)
{
    union REGS r;

    r.w.ax = 0x100;
    r.w.bx = paragraph_count_;
    int386(0x31, &r, &r);

    if (!r.w.cflag)
    {
        *segment_address_ = r.w.ax;
        *selector_ = r.w.dx;

        return 0;
    }
    else
    {
        return r.w.ax;
    }
}


void DU_set_dos_vect2
(
    DU_BYTE		which_vect_,
    DU_DWORD	handler_
)
{
    DU_set_dos_vect(which_vect_, DU_HIWORD(handler_),
                    DU_LOWORD(handler_));
}


DU_WORD DU_sim_real_int
(
    DU_BYTE		which_vect_,
    DU_BYTE		flags_,
    DU_WORD		copy_from_stack_,
    DU_REGS *	regs_
)
{
    union REGS r;

    r.w.ax = 0x300;
    r.h.bl = which_vect_;
    r.h.bh = flags_;
    r.w.cx = copy_from_stack_;
    r.x.edi = (DU_DWORD)regs_;
    int386(0x31, &r, &r);
    return r.x.cflag ? r.w.ax : 0;
}
