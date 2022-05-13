/*
 * dpmiutil.h - DPMI utilities (interface header)
 */

#ifndef INCLUDED_DPMIUTIL_H_
#define INCLUDED_DPMIUTIL_H_

typedef unsigned short DU_WORD;
typedef unsigned long DU_DWORD;
typedef unsigned char DU_BYTE;

#define DU_LOWORD(l) ((DU_WORD)(DU_DWORD)(l))
#define DU_HIWORD(l) ((DU_WORD)((DU_DWORD)(l) >> 16U))
#define DU_MAKELONG(high, low) \
    ((DU_DWORD)(DU_WORD)(high) << 16U | (DU_WORD)(low))

typedef struct tagDU_REGS
{
	DU_DWORD edi, esi, ebp;
	DU_DWORD reserved1;
	DU_DWORD ebx, edx, ecx, eax;
	DU_WORD flags;
	DU_WORD es, ds, fs, gs;
	DU_WORD ip, cs;
	DU_WORD sp, ss;
} DU_REGS;

extern long DU_alloc_desc
(
    short desc_count_
);

#pragma aux DU_alloc_desc = \
    "xor eax, eax" \
    "int 31h" \
    "jnc @1" \
    "neg eax" \
    "@1:" \
    parm [cx]

/* Allocate low memory - returns zero on success, the DPMI error code
   on failure */
extern int DU_alloc_dosmem
(
    DU_WORD paragraph_count_,
    DU_WORD * segment_address_,
    DU_WORD * selector_
);

/* Free memory allocated with DU_alloc_dosmem() */
extern int DU_free_dosmem
(
    DU_WORD selector_
);

#pragma aux DU_free_dosmem = \
    "mov eax,101h" \
    "int 31h" \
    "jc @1" \
    "xor eax,eax" \
    "@1:" \
    parm [dx]

/* get a real-mode interrupt vector */
extern DU_DWORD DU_get_dos_vect
(
    DU_BYTE which_vect_
);

#pragma aux DU_get_dos_vect = \
    "mov ax,200h" \
    "int 31h" \
    "shl ecx,16" \
    "mov cx,dx" \
    parm [bl] modify [ax edx] value [ecx]

/* set a real-mode interrupt vector */
extern void DU_set_dos_vect
(
    DU_BYTE which_vect_,
    DU_WORD handler_segment_,
    DU_WORD handler_offset_
);

#pragma aux DU_set_dos_vect = \
    "mov ax,201h" \
    "int 31h" \
    parm [bl] [cx] [dx] modify [ax]

extern void DU_set_dos_vect2
(
    DU_BYTE which_vect_,
    DU_DWORD handler_
);

/* DPMI function 204h and 205h not implemented, use
   _dos_getvect/setvect */

/* simulate real-mode interrupt.  Returns zero on success, a DPMI
   error code on failure */
extern DU_WORD DU_sim_real_int
(
    DU_BYTE which_vect_,
    DU_BYTE flags_,
    DU_WORD copy_from_stack_,
    DU_REGS * regs_
);

/* lock a region of memory */
extern int DU_lock_linear
(
    void * start_,
    DU_DWORD size_
);

#pragma aux DU_lock_linear = \
    "mov eax,600h" \
    "shld ebx,ecx,16" \
    "shld esi,edi,16" \
    "int 31h" \
    "jc @1" \
    "xor eax,eax" \
    "@1:" \
    parm [ecx] [edi] modify [ebx esi] value [eax]


#endif // ifndef INCLUDED_DPMIUTIL_H_
