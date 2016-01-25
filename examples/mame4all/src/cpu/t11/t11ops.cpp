/*** t11: Portable DEC T-11 emulator ******************************************

	Copyright (C) Aaron Giles 1998

    Actual opcode implementation.  Excuse the excessive use of macros, it
    was the only way I could bear to type all this in!

*****************************************************************************/


/* given a register index 'r', this computes the effective address for a byte-sized operation
   and puts the result in 'ea' */
#define MAKE_EAB_RGD(r) ea = REGD(r)
#define MAKE_EAB_IN(r)  ea = REGD(r); REGW(r) += ((r) < 6 ? 1 : 2)
#define MAKE_EAB_INS(r) ea = REGD(r); REGW(r) += ((r) < 6 ? 1 : 2)
#define MAKE_EAB_IND(r) ea = REGD(r); REGW(r) += 2; ea = RWORD(ea)
#define MAKE_EAB_DE(r)  REGW(r) -= ((r) < 6 ? 1 : 2); ea = REGD(r)
#define MAKE_EAB_DED(r) REGW(r) -= 2; ea = REGD(r); ea = RWORD(ea)
#define MAKE_EAB_IX(r)  ea = ROPCODE(); ea = (ea + REGD(r)) & 0xffff
#define MAKE_EAB_IXD(r) ea = ROPCODE(); ea = (ea + REGD(r)) & 0xffff; ea = RWORD(ea)

/* given a register index 'r', this computes the effective address for a word-sized operation
   and puts the result in 'ea' */
/* note that word accesses ignore the low bit!! this fixes APB! */
#define MAKE_EAW_RGD(r) MAKE_EAB_RGD(r)
#define MAKE_EAW_IN(r)  ea = REGD(r); REGW(r) += 2
#define MAKE_EAW_IND(r) MAKE_EAB_IND(r)
#define MAKE_EAW_DE(r)  REGW(r) -= 2; ea = REGD(r)
#define MAKE_EAW_DED(r) MAKE_EAB_DED(r)
#define MAKE_EAW_IX(r)  MAKE_EAB_IX(r)
#define MAKE_EAW_IXD(r) MAKE_EAB_IXD(r)

/* extracts the source/destination register index from the opcode into 'sreg' or 'dreg' */
#define GET_SREG sreg = (t11.op >> 6) & 7
#define GET_DREG dreg = t11.op & 7

/* for a byte-sized source operand: extracts 'sreg', computes 'ea', and loads the value into 'source' */
#define GET_SB_RG  GET_SREG; source = REGB(sreg)
#define GET_SB_RGD GET_SREG; MAKE_EAB_RGD(sreg); source = RBYTE(ea)
#define GET_SB_IN  GET_SREG; if (sreg == 7) { source = ROPCODE(); } else { MAKE_EAB_IN(sreg); source = RBYTE(ea); }
#define GET_SB_IND GET_SREG; if (sreg == 7) { ea = ROPCODE(); } else { MAKE_EAB_IND(sreg); } source = RBYTE(ea)
#define GET_SB_DE  GET_SREG; MAKE_EAB_DE(sreg); source = RBYTE(ea)
#define GET_SB_DED GET_SREG; MAKE_EAB_DED(sreg); source = RBYTE(ea)
#define GET_SB_IX  GET_SREG; MAKE_EAB_IX(sreg); source = RBYTE(ea)
#define GET_SB_IXD GET_SREG; MAKE_EAB_IXD(sreg); source = RBYTE(ea)

/* for a word-sized source operand: extracts 'sreg', computes 'ea', and loads the value into 'source' */
#define GET_SW_RG  GET_SREG; source = REGD(sreg)
#define GET_SW_RGD GET_SREG; MAKE_EAW_RGD(sreg); source = RWORD(ea)
#define GET_SW_IN  GET_SREG; if (sreg == 7) { source = ROPCODE(); } else { MAKE_EAW_IN(sreg); source = RWORD(ea); }
#define GET_SW_IND GET_SREG; if (sreg == 7) { ea = ROPCODE(); } else { MAKE_EAW_IND(sreg); } source = RWORD(ea)
#define GET_SW_DE  GET_SREG; MAKE_EAW_DE(sreg); source = RWORD(ea)
#define GET_SW_DED GET_SREG; MAKE_EAW_DED(sreg); source = RWORD(ea)
#define GET_SW_IX  GET_SREG; MAKE_EAW_IX(sreg); source = RWORD(ea)
#define GET_SW_IXD GET_SREG; MAKE_EAW_IXD(sreg); source = RWORD(ea)

/* for a byte-sized destination operand: extracts 'dreg', computes 'ea', and loads the value into 'dest' */
#define GET_DB_RG  GET_DREG; dest = REGB(dreg)
#define GET_DB_RGD GET_DREG; MAKE_EAB_RGD(dreg); dest = RBYTE(ea)
#define GET_DB_IN  GET_DREG; MAKE_EAB_IN(dreg); dest = RBYTE(ea)
#define GET_DB_IND GET_DREG; if (dreg == 7) { ea = ROPCODE(); } else { MAKE_EAB_IND(dreg); } dest = RBYTE(ea)
#define GET_DB_DE  GET_DREG; MAKE_EAB_DE(dreg); dest = RBYTE(ea)
#define GET_DB_DED GET_DREG; MAKE_EAB_DED(dreg); dest = RBYTE(ea)
#define GET_DB_IX  GET_DREG; MAKE_EAB_IX(dreg); dest = RBYTE(ea)
#define GET_DB_IXD GET_DREG; MAKE_EAB_IXD(dreg); dest = RBYTE(ea)

/* for a word-sized destination operand: extracts 'dreg', computes 'ea', and loads the value into 'dest' */
#define GET_DW_RG  GET_DREG; dest = REGD(dreg)
#define GET_DW_RGD GET_DREG; MAKE_EAW_RGD(dreg); dest = RWORD(ea)
#define GET_DW_IN  GET_DREG; MAKE_EAW_IN(dreg); dest = RWORD(ea)
#define GET_DW_IND GET_DREG; if (dreg == 7) { ea = ROPCODE(); } else { MAKE_EAW_IND(dreg); } dest = RWORD(ea)
#define GET_DW_DE  GET_DREG; MAKE_EAW_DE(dreg); dest = RWORD(ea)
#define GET_DW_DED GET_DREG; MAKE_EAW_DED(dreg); dest = RWORD(ea)
#define GET_DW_IX  GET_DREG; MAKE_EAW_IX(dreg); dest = RWORD(ea)
#define GET_DW_IXD GET_DREG; MAKE_EAW_IXD(dreg); dest = RWORD(ea)

/* writes a value to a previously computed 'ea' */
#define PUT_DB_EA(v) WBYTE(ea, (v))
#define PUT_DW_EA(v) WWORD(ea, (v))

/* writes a value to a previously computed 'dreg' register */
#define PUT_DB_DREG(v) REGB(dreg) = (v)
#define PUT_DW_DREG(v) REGW(dreg) = (v)

/* for a byte-sized destination operand: extracts 'dreg', computes 'ea', and writes 'v' to it */
#define PUT_DB_RG(v)  GET_DREG; REGB(dreg) = (v)
#define PUT_DB_RGD(v) GET_DREG; MAKE_EAB_RGD(dreg); WBYTE(ea, (v))
#define PUT_DB_IN(v)  GET_DREG; MAKE_EAB_IN(dreg); WBYTE(ea, (v))
#define PUT_DB_IND(v) GET_DREG; if (dreg == 7) { ea = ROPCODE(); } else { MAKE_EAB_IND(dreg); } WBYTE(ea, (v))
#define PUT_DB_DE(v)  GET_DREG; MAKE_EAB_DE(dreg); WBYTE(ea, (v))
#define PUT_DB_DED(v) GET_DREG; MAKE_EAB_DED(dreg); WBYTE(ea, (v))
#define PUT_DB_IX(v)  GET_DREG; MAKE_EAB_IX(dreg); WBYTE(ea, (v))
#define PUT_DB_IXD(v) GET_DREG; MAKE_EAB_IXD(dreg); WBYTE(ea, (v))

/* for a word-sized destination operand: extracts 'dreg', computes 'ea', and writes 'v' to it */
#define PUT_DW_RG(v)  GET_DREG; REGW(dreg) = (v)
#define PUT_DW_RGD(v) GET_DREG; MAKE_EAW_RGD(dreg); WWORD(ea, (v))
#define PUT_DW_IN(v)  GET_DREG; MAKE_EAW_IN(dreg); WWORD(ea, (v))
#define PUT_DW_IND(v) GET_DREG; if (dreg == 7) { ea = ROPCODE(); } else { MAKE_EAW_IND(dreg); } WWORD(ea, (v))
#define PUT_DW_DE(v)  GET_DREG; MAKE_EAW_DE(dreg); WWORD(ea, (v))
#define PUT_DW_DED(v) GET_DREG; MAKE_EAW_DED(dreg); WWORD(ea, (v))
#define PUT_DW_IX(v)  GET_DREG; MAKE_EAW_IX(dreg); WWORD(ea, (v))
#define PUT_DW_IXD(v) GET_DREG; MAKE_EAW_IXD(dreg); WWORD(ea, (v))

/* flag clearing; must be done before setting */
#define CLR_ZV   (PSW &= ~(ZFLAG | VFLAG))
#define CLR_NZV  (PSW &= ~(NFLAG | ZFLAG | VFLAG))
#define CLR_NZVC (PSW &= ~(NFLAG | ZFLAG | VFLAG | CFLAG))

/* set individual flags byte-sized */
#define SETB_N (PSW |= (result >> 4) & 0x08)
#define SETB_Z (PSW |= ((result & 0xff) == 0) << 2)
#define SETB_V (PSW |= ((source ^ dest ^ result ^ (result >> 1)) >> 6) & 0x02)
#define SETB_C (PSW |= (result >> 8) & 0x01)
#define SETB_NZ SETB_N; SETB_Z
#define SETB_NZV SETB_N; SETB_Z; SETB_V
#define SETB_NZVC SETB_N; SETB_Z; SETB_V; SETB_C

/* set individual flags word-sized */
#define SETW_N (PSW |= (result >> 12) & 0x08)
#define SETW_Z (PSW |= ((result & 0xffff) == 0) << 2)
#define SETW_V (PSW |= ((source ^ dest ^ result ^ (result >> 1)) >> 14) & 0x02)
#define SETW_C (PSW |= (result >> 16) & 0x01)
#define SETW_NZ SETW_N; SETW_Z
#define SETW_NZV SETW_N; SETW_Z; SETW_V
#define SETW_NZVC SETW_N; SETW_Z; SETW_V; SETW_C

/* operations */
/* ADC: dst += C */
#define ADC_R(d)    int dreg, source, dest, result;     source = GET_C; GET_DW_##d; CLR_NZVC; result = dest + source; SETW_NZVC; PUT_DW_DREG(result)
#define ADC_M(d)    int dreg, source, dest, result, ea; source = GET_C; GET_DW_##d; CLR_NZVC; result = dest + source; SETW_NZVC; PUT_DW_EA(result)
#define ADCB_R(d)   int dreg, source, dest, result;     source = GET_C; GET_DB_##d; CLR_NZVC; result = dest + source; SETB_NZVC; PUT_DB_DREG(result)
#define ADCB_M(d)   int dreg, source, dest, result, ea; source = GET_C; GET_DB_##d; CLR_NZVC; result = dest + source; SETB_NZVC; PUT_DB_EA(result)
/* ADD: dst += src */
#define ADD_R(s,d)  int sreg, dreg, source, dest, result;     GET_SW_##s; GET_DW_##d; CLR_NZVC; result = dest + source; SETW_NZVC; PUT_DW_DREG(result)
#define ADD_X(s,d)  int sreg, dreg, source, dest, result, ea; GET_SW_##s; GET_DW_##d; CLR_NZVC; result = dest + source; SETW_NZVC; PUT_DW_DREG(result)
#define ADD_M(s,d)  int sreg, dreg, source, dest, result, ea; GET_SW_##s; GET_DW_##d; CLR_NZVC; result = dest + source; SETW_NZVC; PUT_DW_EA(result)
/* ASL: dst = (dst << 1); C = (dst >> 7) */
#define ASL_R(d)    int dreg, dest, result;     GET_DW_##d; CLR_NZVC; result = dest << 1; SETW_NZ; PSW |= (dest >> 15) & 1; PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DW_DREG(result)
#define ASL_M(d)    int dreg, dest, result, ea; GET_DW_##d; CLR_NZVC; result = dest << 1; SETW_NZ; PSW |= (dest >> 15) & 1; PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DW_EA(result)
#define ASLB_R(d)   int dreg, dest, result;     GET_DB_##d; CLR_NZVC; result = dest << 1; SETB_NZ; PSW |= (dest >> 7) & 1;  PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DB_DREG(result)
#define ASLB_M(d)   int dreg, dest, result, ea; GET_DB_##d; CLR_NZVC; result = dest << 1; SETB_NZ; PSW |= (dest >> 7) & 1;  PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DB_EA(result)
/* ASR: dst = (dst << 1); C = (dst >> 7) */
#define ASR_R(d)    int dreg, dest, result;     GET_DW_##d; CLR_NZVC; result = (dest >> 1) | (dest & 0x8000); SETW_NZ; PSW |= dest & 1; PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DW_DREG(result)
#define ASR_M(d)    int dreg, dest, result, ea; GET_DW_##d; CLR_NZVC; result = (dest >> 1) | (dest & 0x8000); SETW_NZ; PSW |= dest & 1; PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DW_EA(result)
#define ASRB_R(d)   int dreg, dest, result;     GET_DB_##d; CLR_NZVC; result = (dest >> 1) | (dest & 0x80);   SETB_NZ; PSW |= dest & 1; PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DB_DREG(result)
#define ASRB_M(d)   int dreg, dest, result, ea; GET_DB_##d; CLR_NZVC; result = (dest >> 1) | (dest & 0x80);   SETB_NZ; PSW |= dest & 1; PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DB_EA(result)
/* BIC: dst &= ~src */
#define BIC_R(s,d)  int sreg, dreg, source, dest, result;     GET_SW_##s; GET_DW_##d; CLR_NZV; result = dest & ~source; SETW_NZ; PUT_DW_DREG(result)
#define BIC_X(s,d)  int sreg, dreg, source, dest, result, ea; GET_SW_##s; GET_DW_##d; CLR_NZV; result = dest & ~source; SETW_NZ; PUT_DW_DREG(result)
#define BIC_M(s,d)  int sreg, dreg, source, dest, result, ea; GET_SW_##s; GET_DW_##d; CLR_NZV; result = dest & ~source; SETW_NZ; PUT_DW_EA(result)
#define BICB_R(s,d) int sreg, dreg, source, dest, result;     GET_SB_##s; GET_DB_##d; CLR_NZV; result = dest & ~source; SETB_NZ; PUT_DB_DREG(result)
#define BICB_X(s,d) int sreg, dreg, source, dest, result, ea; GET_SB_##s; GET_DB_##d; CLR_NZV; result = dest & ~source; SETB_NZ; PUT_DB_DREG(result)
#define BICB_M(s,d) int sreg, dreg, source, dest, result, ea; GET_SB_##s; GET_DB_##d; CLR_NZV; result = dest & ~source; SETB_NZ; PUT_DB_EA(result)
/* BIS: dst |= src */
#define BIS_R(s,d)  int sreg, dreg, source, dest, result;     GET_SW_##s; GET_DW_##d; CLR_NZV; result = dest | source; SETW_NZ; PUT_DW_DREG(result)
#define BIS_X(s,d)  int sreg, dreg, source, dest, result, ea; GET_SW_##s; GET_DW_##d; CLR_NZV; result = dest | source; SETW_NZ; PUT_DW_DREG(result)
#define BIS_M(s,d)  int sreg, dreg, source, dest, result, ea; GET_SW_##s; GET_DW_##d; CLR_NZV; result = dest | source; SETW_NZ; PUT_DW_EA(result)
#define BISB_R(s,d) int sreg, dreg, source, dest, result;     GET_SB_##s; GET_DB_##d; CLR_NZV; result = dest | source; SETB_NZ; PUT_DB_DREG(result)
#define BISB_X(s,d) int sreg, dreg, source, dest, result, ea; GET_SB_##s; GET_DB_##d; CLR_NZV; result = dest | source; SETB_NZ; PUT_DB_DREG(result)
#define BISB_M(s,d) int sreg, dreg, source, dest, result, ea; GET_SB_##s; GET_DB_##d; CLR_NZV; result = dest | source; SETB_NZ; PUT_DB_EA(result)
/* BIT: flags = dst & src */
#define BIT_R(s,d)  int sreg, dreg, source, dest, result;     GET_SW_##s; GET_DW_##d; CLR_NZV; result = dest & source; SETW_NZ;
#define BIT_M(s,d)  int sreg, dreg, source, dest, result, ea; GET_SW_##s; GET_DW_##d; CLR_NZV; result = dest & source; SETW_NZ;
#define BITB_R(s,d) int sreg, dreg, source, dest, result;     GET_SB_##s; GET_DB_##d; CLR_NZV; result = dest & source; SETB_NZ;
#define BITB_M(s,d) int sreg, dreg, source, dest, result, ea; GET_SB_##s; GET_DB_##d; CLR_NZV; result = dest & source; SETB_NZ;
/* BR: if (condition) branch */
#define BR(c)		if (c) { PC += 2 * (signed char)(t11.op & 0xff); }
/* CLR: dst = 0 */
#define CLR_R(d)    int dreg;     PUT_DW_##d(0); CLR_NZVC; SET_Z
#define CLR_M(d)    int dreg, ea; PUT_DW_##d(0); CLR_NZVC; SET_Z
#define CLRB_R(d)   int dreg;     PUT_DB_##d(0); CLR_NZVC; SET_Z
#define CLRB_M(d)   int dreg, ea; PUT_DB_##d(0); CLR_NZVC; SET_Z
/* CMP: flags = src - dst */
#define CMP_R(s,d)  int sreg, dreg, source, dest, result;     GET_SW_##s; GET_DW_##d; CLR_NZVC; result = source - dest; SETW_NZVC;
#define CMP_M(s,d)  int sreg, dreg, source, dest, result, ea; GET_SW_##s; GET_DW_##d; CLR_NZVC; result = source - dest; SETW_NZVC;
#define CMPB_R(s,d) int sreg, dreg, source, dest, result;     GET_SB_##s; GET_DB_##d; CLR_NZVC; result = source - dest; SETB_NZVC;
#define CMPB_M(s,d) int sreg, dreg, source, dest, result, ea; GET_SB_##s; GET_DB_##d; CLR_NZVC; result = source - dest; SETB_NZVC;
/* COM: dst = ~dst */
#define COM_R(d)    int dreg, dest, result;     GET_DW_##d; CLR_NZVC; result = ~dest; SETW_NZ; SET_C; PUT_DW_DREG(result)
#define COM_M(d)    int dreg, dest, result, ea; GET_DW_##d; CLR_NZVC; result = ~dest; SETW_NZ; SET_C; PUT_DW_EA(result)
#define COMB_R(d)   int dreg, dest, result;     GET_DB_##d; CLR_NZVC; result = ~dest; SETB_NZ; SET_C; PUT_DB_DREG(result)
#define COMB_M(d)   int dreg, dest, result, ea; GET_DB_##d; CLR_NZVC; result = ~dest; SETB_NZ; SET_C; PUT_DB_EA(result)
/* DEC: dst -= 1 */
#define DEC_R(d)    int dreg, dest, result;     GET_DW_##d; CLR_NZV; result = dest - 1; SETW_NZ; if (dest == 0x8000) SET_V; PUT_DW_DREG(result)
#define DEC_M(d)    int dreg, dest, result, ea; GET_DW_##d; CLR_NZV; result = dest - 1; SETW_NZ; if (dest == 0x8000) SET_V; PUT_DW_EA(result)
#define DECB_R(d)   int dreg, dest, result;     GET_DB_##d; CLR_NZV; result = dest - 1; SETB_NZ; if (dest == 0x80)   SET_V; PUT_DB_DREG(result)
#define DECB_M(d)   int dreg, dest, result, ea; GET_DB_##d; CLR_NZV; result = dest - 1; SETB_NZ; if (dest == 0x80)   SET_V; PUT_DB_EA(result)
/* INC: dst += 1 */
#define INC_R(d)    int dreg, dest, result;     GET_DW_##d; CLR_NZV; result = dest + 1; SETW_NZ; if (dest == 0x7fff) SET_V; PUT_DW_DREG(result)
#define INC_M(d)    int dreg, dest, result, ea; GET_DW_##d; CLR_NZV; result = dest + 1; SETW_NZ; if (dest == 0x7fff) SET_V; PUT_DW_EA(result)
#define INCB_R(d)   int dreg, dest, result;     GET_DB_##d; CLR_NZV; result = dest + 1; SETB_NZ; if (dest == 0x7f)   SET_V; PUT_DB_DREG(result)
#define INCB_M(d)   int dreg, dest, result, ea; GET_DB_##d; CLR_NZV; result = dest + 1; SETB_NZ; if (dest == 0x7f)   SET_V; PUT_DB_EA(result)
/* JMP: PC = ea */
#define JMP(d)      int dreg, ea; GET_DREG; MAKE_EAW_##d(dreg); PC = ea
/* JSR: PUSH src, src = PC, PC = ea */
#define JSR(d)      int sreg, dreg, ea; GET_SREG; GET_DREG; MAKE_EAW_##d(dreg); PUSH(REGW(sreg)); REGW(sreg) = PC; PC = ea
/* MFPS: dst = flags */
#define MFPS_R(d)   int dreg, result;     result = PSW; CLR_NZV; SETB_NZ; PUT_DW_##d((signed char)result)
#define MFPS_M(d)   int dreg, result, ea; result = PSW; CLR_NZV; SETB_NZ; PUT_DB_##d(result)
/* MOV: dst = src */
#define MOV_R(s,d)  int sreg, dreg, source, result;     GET_SW_##s; CLR_NZV; result = source; SETW_NZ; PUT_DW_##d(result)
#define MOV_M(s,d)  int sreg, dreg, source, result, ea; GET_SW_##s; CLR_NZV; result = source; SETW_NZ; PUT_DW_##d(result)
#define MOVB_R(s,d) int sreg, dreg, source, result;     GET_SB_##s; CLR_NZV; result = source; SETB_NZ; PUT_DW_##d((signed char)result)
#define MOVB_X(s,d) int sreg, dreg, source, result, ea; GET_SB_##s; CLR_NZV; result = source; SETB_NZ; PUT_DW_##d((signed char)result)
#define MOVB_M(s,d) int sreg, dreg, source, result, ea; GET_SB_##s; CLR_NZV; result = source; SETB_NZ; PUT_DB_##d(result)
/* MTPS: flags = src */
#define MTPS_R(d)   int dreg, dest;     GET_DW_##d; PSW = (PSW & ~0xef) | (dest & 0xef); t11_check_irqs()
#define MTPS_M(d)   int dreg, dest, ea; GET_DW_##d; PSW = (PSW & ~0xef) | (dest & 0xef); t11_check_irqs()
/* NEG: dst = -dst */
#define NEG_R(d)    int dreg, dest, result;     GET_DW_##d; CLR_NZVC; result = -dest; SETW_NZ; if (dest == 0x8000) SET_V; if (result) SET_C; PUT_DW_DREG(result)
#define NEG_M(d)    int dreg, dest, result, ea; GET_DW_##d; CLR_NZVC; result = -dest; SETW_NZ; if (dest == 0x8000) SET_V; if (result) SET_C; PUT_DW_EA(result)
#define NEGB_R(d)   int dreg, dest, result;     GET_DB_##d; CLR_NZVC; result = -dest; SETB_NZ; if (dest == 0x80)   SET_V; if (result) SET_C; PUT_DB_DREG(result)
#define NEGB_M(d)   int dreg, dest, result, ea; GET_DB_##d; CLR_NZVC; result = -dest; SETB_NZ; if (dest == 0x80)   SET_V; if (result) SET_C; PUT_DB_EA(result)
/* ROL: dst = (dst << 1) | C; C = (dst >> 7) */
#define ROL_R(d)    int dreg, dest, result;     GET_DW_##d; result = (dest << 1) | GET_C; CLR_NZVC; SETW_NZ; PSW |= (dest >> 15) & 1; PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DW_DREG(result)
#define ROL_M(d)    int dreg, dest, result, ea; GET_DW_##d; result = (dest << 1) | GET_C; CLR_NZVC; SETW_NZ; PSW |= (dest >> 15) & 1; PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DW_EA(result)
#define ROLB_R(d)   int dreg, dest, result;     GET_DB_##d; result = (dest << 1) | GET_C; CLR_NZVC; SETB_NZ; PSW |= (dest >> 7) & 1;  PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DB_DREG(result)
#define ROLB_M(d)   int dreg, dest, result, ea; GET_DB_##d; result = (dest << 1) | GET_C; CLR_NZVC; SETB_NZ; PSW |= (dest >> 7) & 1;  PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DB_EA(result)
/* ROR: dst = (dst >> 1) | (C << 7); C = dst & 1 */
#define ROR_R(d)    int dreg, dest, result;     GET_DW_##d; result = (dest >> 1) | (GET_C << 15); CLR_NZVC; SETW_NZ; PSW |= dest & 1; PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DW_DREG(result)
#define ROR_M(d)    int dreg, dest, result, ea; GET_DW_##d; result = (dest >> 1) | (GET_C << 15); CLR_NZVC; SETW_NZ; PSW |= dest & 1; PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DW_EA(result)
#define RORB_R(d)   int dreg, dest, result;     GET_DB_##d; result = (dest >> 1) | (GET_C << 7);  CLR_NZVC; SETB_NZ; PSW |= dest & 1; PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DB_DREG(result)
#define RORB_M(d)   int dreg, dest, result, ea; GET_DB_##d; result = (dest >> 1) | (GET_C << 7);  CLR_NZVC; SETB_NZ; PSW |= dest & 1; PSW |= ((PSW << 1) ^ (PSW >> 2)) & 2; PUT_DB_EA(result)
/* SBC: dst -= C */
#define SBC_R(d)    int dreg, source, dest, result;     source = GET_C; GET_DW_##d; CLR_NZVC; result = dest - source; SETW_NZVC; PUT_DW_DREG(result)
#define SBC_M(d)    int dreg, source, dest, result, ea; source = GET_C; GET_DW_##d; CLR_NZVC; result = dest - source; SETW_NZVC; PUT_DW_EA(result)
#define SBCB_R(d)   int dreg, source, dest, result;     source = GET_C; GET_DB_##d; CLR_NZVC; result = dest - source; SETB_NZVC; PUT_DB_DREG(result)
#define SBCB_M(d)   int dreg, source, dest, result, ea; source = GET_C; GET_DB_##d; CLR_NZVC; result = dest - source; SETB_NZVC; PUT_DB_EA(result)
/* SUB: dst -= src */
#define SUB_R(s,d)  int sreg, dreg, source, dest, result;     GET_SW_##s; GET_DW_##d; CLR_NZVC; result = dest - source; SETW_NZVC; PUT_DW_DREG(result)
#define SUB_X(s,d)  int sreg, dreg, source, dest, result, ea; GET_SW_##s; GET_DW_##d; CLR_NZVC; result = dest - source; SETW_NZVC; PUT_DW_DREG(result)
#define SUB_M(s,d)  int sreg, dreg, source, dest, result, ea; GET_SW_##s; GET_DW_##d; CLR_NZVC; result = dest - source; SETW_NZVC; PUT_DW_EA(result)
#define SUBB_R(s,d) int sreg, dreg, source, dest, result;     GET_SB_##s; GET_DB_##d; CLR_NZVC; result = dest - source; SETB_NZVC; PUT_DB_DREG(result)
#define SUBB_X(s,d) int sreg, dreg, source, dest, result, ea; GET_SB_##s; GET_DB_##d; CLR_NZVC; result = dest - source; SETB_NZVC; PUT_DB_DREG(result)
#define SUBB_M(s,d) int sreg, dreg, source, dest, result, ea; GET_SB_##s; GET_DB_##d; CLR_NZVC; result = dest - source; SETB_NZVC; PUT_DB_EA(result)
/* SWAB: dst = (dst >> 8) + (dst << 8) */
#define SWAB_R(d)   int dreg, dest, result;     GET_DW_##d; CLR_NZVC; result = ((dest >> 8) & 0xff) + (dest << 8); SETB_NZ; PUT_DW_DREG(result)
#define SWAB_M(d)   int dreg, dest, result, ea; GET_DW_##d; CLR_NZVC; result = ((dest >> 8) & 0xff) + (dest << 8); SETB_NZ; PUT_DW_EA(result)
/* SXT: dst = sign-extend dst */
#define SXT_R(d)    int dreg, result;     CLR_ZV; if (GET_N) result = -1; else { result = 0; SET_Z; } PUT_DW_##d(result)
#define SXT_M(d)    int dreg, result, ea; CLR_ZV; if (GET_N) result = -1; else { result = 0; SET_Z; } PUT_DW_##d(result)
/* TST: dst = ~dst */
#define TST_R(d)    int dreg, dest, result;     GET_DW_##d; CLR_NZVC; result = dest; SETW_NZ;
#define TST_M(d)    int dreg, dest, result, ea; GET_DW_##d; CLR_NZVC; result = dest; SETW_NZ;
#define TSTB_R(d)   int dreg, dest, result;     GET_DB_##d; CLR_NZVC; result = dest; SETB_NZ;
#define TSTB_M(d)   int dreg, dest, result, ea; GET_DB_##d; CLR_NZVC; result = dest; SETB_NZ;
/* XOR: dst ^= src */
#define XOR_R(d)    int sreg, dreg, source, dest, result;     GET_SREG; source = REGW(sreg); GET_DW_##d; CLR_NZV; result = dest ^ source; SETW_NZ; PUT_DW_DREG(result)
#define XOR_M(d)    int sreg, dreg, source, dest, result, ea; GET_SREG; source = REGW(sreg); GET_DW_##d; CLR_NZV; result = dest ^ source; SETW_NZ; PUT_DW_EA(result)



static void op_0000(void)
{
	switch (t11.op & 0x3f)
	{
		case 0x00:	/* HALT  */ t11_ICount = 0; break;
		case 0x01:	/* WAIT  */ t11.wait_state = 1; t11_ICount = 0; break;
		case 0x02:	/* RTI   */ PC = POP(); PSW = POP(); t11_check_irqs(); break;
		case 0x03:	/* BPT   */ PUSH(PSW); PUSH(PC); PC = RWORD(0x0c); PSW = RWORD(0x0e); t11_check_irqs(); break;
		case 0x04:	/* IOT   */ PUSH(PSW); PUSH(PC); PC = RWORD(0x10); PSW = RWORD(0x12); t11_check_irqs(); break;
		case 0x05:	/* RESET */ break;
		case 0x06:	/* RTT   */ PC = POP(); PSW = POP(); t11_check_irqs(); break;
		default: 	illegal(); break;
	}
}

static void illegal(void)
{
	PUSH(PSW);
	PUSH(PC);
	PC = RWORD(0x08);
	PSW = RWORD(0x0a);
	t11_check_irqs();
PC = 0;
}

static void jmp_rgd(void)       { JMP(RGD); }
static void jmp_in(void)        { JMP(IN);  }
static void jmp_ind(void)       { JMP(IND); }
static void jmp_de(void)        { JMP(DE);  }
static void jmp_ded(void)       { JMP(DED); }
static void jmp_ix(void)        { JMP(IX);  }
static void jmp_ixd(void)       { JMP(IXD); }

static void rts(void)
{
	int dreg;
	GET_DREG;
	PC = REGD(dreg);
	REGW(dreg) = POP();
}

static void ccc(void)			{ PSW &= ~(t11.op & 15); }
static void scc(void)			{ PSW |=  (t11.op & 15); }

static void swab_rg(void)       { SWAB_R(RG); }
static void swab_rgd(void)      { SWAB_M(RGD); }
static void swab_in(void)       { SWAB_M(IN); }
static void swab_ind(void)      { SWAB_M(IND); }
static void swab_de(void)       { SWAB_M(DE); }
static void swab_ded(void)      { SWAB_M(DED); }
static void swab_ix(void)       { SWAB_M(IX); }
static void swab_ixd(void)      { SWAB_M(IXD); }

static void br(void)            { BR(1); }
static void bne(void)           { BR(!GET_Z); }
static void beq(void)           { BR( GET_Z); }
static void bge(void)           { BR(!((GET_N >> 2) ^ GET_V)); }
static void blt(void)           { BR(((GET_N >> 2) ^ GET_V)); }
static void bgt(void)           { BR(!GET_Z && !((GET_N >> 2) ^ GET_V)); }
static void ble(void)           { BR( GET_Z || ((GET_N >> 2) ^ GET_V)); }

static void jsr_rgd(void)       { JSR(RGD); }
static void jsr_in(void)        { JSR(IN);  }
static void jsr_ind(void)       { JSR(IND); }
static void jsr_de(void)        { JSR(DE);  }
static void jsr_ded(void)       { JSR(DED); }
static void jsr_ix(void)        { JSR(IX);  }
static void jsr_ixd(void)       { JSR(IXD); }

static void clr_rg(void)        { CLR_R(RG);  }
static void clr_rgd(void)       { CLR_M(RGD); }
static void clr_in(void)        { CLR_M(IN);  }
static void clr_ind(void)       { CLR_M(IND); }
static void clr_de(void)        { CLR_M(DE);  }
static void clr_ded(void)       { CLR_M(DED); }
static void clr_ix(void)        { CLR_M(IX);  }
static void clr_ixd(void)       { CLR_M(IXD); }

static void com_rg(void)        { COM_R(RG);  }
static void com_rgd(void)       { COM_M(RGD); }
static void com_in(void)        { COM_M(IN);  }
static void com_ind(void)       { COM_M(IND); }
static void com_de(void)        { COM_M(DE);  }
static void com_ded(void)       { COM_M(DED); }
static void com_ix(void)        { COM_M(IX);  }
static void com_ixd(void)       { COM_M(IXD); }

static void inc_rg(void)        { INC_R(RG);  }
static void inc_rgd(void)       { INC_M(RGD); }
static void inc_in(void)        { INC_M(IN);  }
static void inc_ind(void)       { INC_M(IND); }
static void inc_de(void)        { INC_M(DE);  }
static void inc_ded(void)       { INC_M(DED); }
static void inc_ix(void)        { INC_M(IX);  }
static void inc_ixd(void)       { INC_M(IXD); }

static void dec_rg(void)        { DEC_R(RG);  }
static void dec_rgd(void)       { DEC_M(RGD); }
static void dec_in(void)        { DEC_M(IN);  }
static void dec_ind(void)       { DEC_M(IND); }
static void dec_de(void)        { DEC_M(DE);  }
static void dec_ded(void)       { DEC_M(DED); }
static void dec_ix(void)        { DEC_M(IX);  }
static void dec_ixd(void)       { DEC_M(IXD); }

static void neg_rg(void)        { NEG_R(RG);  }
static void neg_rgd(void)       { NEG_M(RGD); }
static void neg_in(void)        { NEG_M(IN);  }
static void neg_ind(void)       { NEG_M(IND); }
static void neg_de(void)        { NEG_M(DE);  }
static void neg_ded(void)       { NEG_M(DED); }
static void neg_ix(void)        { NEG_M(IX);  }
static void neg_ixd(void)       { NEG_M(IXD); }

static void adc_rg(void)        { ADC_R(RG);  }
static void adc_rgd(void)       { ADC_M(RGD); }
static void adc_in(void)        { ADC_M(IN);  }
static void adc_ind(void)       { ADC_M(IND); }
static void adc_de(void)        { ADC_M(DE);  }
static void adc_ded(void)       { ADC_M(DED); }
static void adc_ix(void)        { ADC_M(IX);  }
static void adc_ixd(void)       { ADC_M(IXD); }

static void sbc_rg(void)        { SBC_R(RG);  }
static void sbc_rgd(void)       { SBC_M(RGD); }
static void sbc_in(void)        { SBC_M(IN);  }
static void sbc_ind(void)       { SBC_M(IND); }
static void sbc_de(void)        { SBC_M(DE);  }
static void sbc_ded(void)       { SBC_M(DED); }
static void sbc_ix(void)        { SBC_M(IX);  }
static void sbc_ixd(void)       { SBC_M(IXD); }

static void tst_rg(void)        { TST_R(RG);  }
static void tst_rgd(void)       { TST_M(RGD); }
static void tst_in(void)        { TST_M(IN);  }
static void tst_ind(void)       { TST_M(IND); }
static void tst_de(void)        { TST_M(DE);  }
static void tst_ded(void)       { TST_M(DED); }
static void tst_ix(void)        { TST_M(IX);  }
static void tst_ixd(void)       { TST_M(IXD); }

static void ror_rg(void)        { ROR_R(RG);  }
static void ror_rgd(void)       { ROR_M(RGD); }
static void ror_in(void)        { ROR_M(IN);  }
static void ror_ind(void)       { ROR_M(IND); }
static void ror_de(void)        { ROR_M(DE);  }
static void ror_ded(void)       { ROR_M(DED); }
static void ror_ix(void)        { ROR_M(IX);  }
static void ror_ixd(void)       { ROR_M(IXD); }

static void rol_rg(void)        { ROL_R(RG);  }
static void rol_rgd(void)       { ROL_M(RGD); }
static void rol_in(void)        { ROL_M(IN);  }
static void rol_ind(void)       { ROL_M(IND); }
static void rol_de(void)        { ROL_M(DE);  }
static void rol_ded(void)       { ROL_M(DED); }
static void rol_ix(void)        { ROL_M(IX);  }
static void rol_ixd(void)       { ROL_M(IXD); }

static void asr_rg(void)        { ASR_R(RG);  }
static void asr_rgd(void)       { ASR_M(RGD); }
static void asr_in(void)        { ASR_M(IN);  }
static void asr_ind(void)       { ASR_M(IND); }
static void asr_de(void)        { ASR_M(DE);  }
static void asr_ded(void)       { ASR_M(DED); }
static void asr_ix(void)        { ASR_M(IX);  }
static void asr_ixd(void)       { ASR_M(IXD); }

static void asl_rg(void)        { ASL_R(RG);  }
static void asl_rgd(void)       { ASL_M(RGD); }
static void asl_in(void)        { ASL_M(IN);  }
static void asl_ind(void)       { ASL_M(IND); }
static void asl_de(void)        { ASL_M(DE);  }
static void asl_ded(void)       { ASL_M(DED); }
static void asl_ix(void)        { ASL_M(IX);  }
static void asl_ixd(void)       { ASL_M(IXD); }

static void sxt_rg(void)        { SXT_R(RG);  }
static void sxt_rgd(void)       { SXT_M(RGD); }
static void sxt_in(void)        { SXT_M(IN);  }
static void sxt_ind(void)       { SXT_M(IND); }
static void sxt_de(void)        { SXT_M(DE);  }
static void sxt_ded(void)       { SXT_M(DED); }
static void sxt_ix(void)        { SXT_M(IX);  }
static void sxt_ixd(void)       { SXT_M(IXD); }

static void mov_rg_rg(void)     { MOV_R(RG,RG);   }
static void mov_rg_rgd(void)    { MOV_M(RG,RGD);  }
static void mov_rg_in(void)     { MOV_M(RG,IN);   }
static void mov_rg_ind(void)    { MOV_M(RG,IND);  }
static void mov_rg_de(void)     { MOV_M(RG,DE);   }
static void mov_rg_ded(void)    { MOV_M(RG,DED);  }
static void mov_rg_ix(void)     { MOV_M(RG,IX);   }
static void mov_rg_ixd(void)    { MOV_M(RG,IXD);  }
static void mov_rgd_rg(void)    { MOV_M(RGD,RG);  }
static void mov_rgd_rgd(void)   { MOV_M(RGD,RGD); }
static void mov_rgd_in(void)    { MOV_M(RGD,IN);  }
static void mov_rgd_ind(void)   { MOV_M(RGD,IND); }
static void mov_rgd_de(void)    { MOV_M(RGD,DE);  }
static void mov_rgd_ded(void)   { MOV_M(RGD,DED); }
static void mov_rgd_ix(void)    { MOV_M(RGD,IX);  }
static void mov_rgd_ixd(void)   { MOV_M(RGD,IXD); }
static void mov_in_rg(void)     { MOV_M(IN,RG);   }
static void mov_in_rgd(void)    { MOV_M(IN,RGD);  }
static void mov_in_in(void)     { MOV_M(IN,IN);   }
static void mov_in_ind(void)    { MOV_M(IN,IND);  }
static void mov_in_de(void)     { MOV_M(IN,DE);   }
static void mov_in_ded(void)    { MOV_M(IN,DED);  }
static void mov_in_ix(void)     { MOV_M(IN,IX);   }
static void mov_in_ixd(void)    { MOV_M(IN,IXD);  }
static void mov_ind_rg(void)    { MOV_M(IND,RG);  }
static void mov_ind_rgd(void)   { MOV_M(IND,RGD); }
static void mov_ind_in(void)    { MOV_M(IND,IN);  }
static void mov_ind_ind(void)   { MOV_M(IND,IND); }
static void mov_ind_de(void)    { MOV_M(IND,DE);  }
static void mov_ind_ded(void)   { MOV_M(IND,DED); }
static void mov_ind_ix(void)    { MOV_M(IND,IX);  }
static void mov_ind_ixd(void)   { MOV_M(IND,IXD); }
static void mov_de_rg(void)     { MOV_M(DE,RG);   }
static void mov_de_rgd(void)    { MOV_M(DE,RGD);  }
static void mov_de_in(void)     { MOV_M(DE,IN);   }
static void mov_de_ind(void)    { MOV_M(DE,IND);  }
static void mov_de_de(void)     { MOV_M(DE,DE);   }
static void mov_de_ded(void)    { MOV_M(DE,DED);  }
static void mov_de_ix(void)     { MOV_M(DE,IX);   }
static void mov_de_ixd(void)    { MOV_M(DE,IXD);  }
static void mov_ded_rg(void)    { MOV_M(DED,RG);  }
static void mov_ded_rgd(void)   { MOV_M(DED,RGD); }
static void mov_ded_in(void)    { MOV_M(DED,IN);  }
static void mov_ded_ind(void)   { MOV_M(DED,IND); }
static void mov_ded_de(void)    { MOV_M(DED,DE);  }
static void mov_ded_ded(void)   { MOV_M(DED,DED); }
static void mov_ded_ix(void)    { MOV_M(DED,IX);  }
static void mov_ded_ixd(void)   { MOV_M(DED,IXD); }
static void mov_ix_rg(void)     { MOV_M(IX,RG);   }
static void mov_ix_rgd(void)    { MOV_M(IX,RGD);  }
static void mov_ix_in(void)     { MOV_M(IX,IN);   }
static void mov_ix_ind(void)    { MOV_M(IX,IND);  }
static void mov_ix_de(void)     { MOV_M(IX,DE);   }
static void mov_ix_ded(void)    { MOV_M(IX,DED);  }
static void mov_ix_ix(void)     { MOV_M(IX,IX);   }
static void mov_ix_ixd(void)    { MOV_M(IX,IXD);  }
static void mov_ixd_rg(void)    { MOV_M(IXD,RG);  }
static void mov_ixd_rgd(void)   { MOV_M(IXD,RGD); }
static void mov_ixd_in(void)    { MOV_M(IXD,IN);  }
static void mov_ixd_ind(void)   { MOV_M(IXD,IND); }
static void mov_ixd_de(void)    { MOV_M(IXD,DE);  }
static void mov_ixd_ded(void)   { MOV_M(IXD,DED); }
static void mov_ixd_ix(void)    { MOV_M(IXD,IX);  }
static void mov_ixd_ixd(void)   { MOV_M(IXD,IXD); }

static void cmp_rg_rg(void)     { CMP_R(RG,RG);   }
static void cmp_rg_rgd(void)    { CMP_M(RG,RGD);  }
static void cmp_rg_in(void)     { CMP_M(RG,IN);   }
static void cmp_rg_ind(void)    { CMP_M(RG,IND);  }
static void cmp_rg_de(void)     { CMP_M(RG,DE);   }
static void cmp_rg_ded(void)    { CMP_M(RG,DED);  }
static void cmp_rg_ix(void)     { CMP_M(RG,IX);   }
static void cmp_rg_ixd(void)    { CMP_M(RG,IXD);  }
static void cmp_rgd_rg(void)    { CMP_M(RGD,RG);  }
static void cmp_rgd_rgd(void)   { CMP_M(RGD,RGD); }
static void cmp_rgd_in(void)    { CMP_M(RGD,IN);  }
static void cmp_rgd_ind(void)   { CMP_M(RGD,IND); }
static void cmp_rgd_de(void)    { CMP_M(RGD,DE);  }
static void cmp_rgd_ded(void)   { CMP_M(RGD,DED); }
static void cmp_rgd_ix(void)    { CMP_M(RGD,IX);  }
static void cmp_rgd_ixd(void)   { CMP_M(RGD,IXD); }
static void cmp_in_rg(void)     { CMP_M(IN,RG);   }
static void cmp_in_rgd(void)    { CMP_M(IN,RGD);  }
static void cmp_in_in(void)     { CMP_M(IN,IN);   }
static void cmp_in_ind(void)    { CMP_M(IN,IND);  }
static void cmp_in_de(void)     { CMP_M(IN,DE);   }
static void cmp_in_ded(void)    { CMP_M(IN,DED);  }
static void cmp_in_ix(void)     { CMP_M(IN,IX);   }
static void cmp_in_ixd(void)    { CMP_M(IN,IXD);  }
static void cmp_ind_rg(void)    { CMP_M(IND,RG);  }
static void cmp_ind_rgd(void)   { CMP_M(IND,RGD); }
static void cmp_ind_in(void)    { CMP_M(IND,IN);  }
static void cmp_ind_ind(void)   { CMP_M(IND,IND); }
static void cmp_ind_de(void)    { CMP_M(IND,DE);  }
static void cmp_ind_ded(void)   { CMP_M(IND,DED); }
static void cmp_ind_ix(void)    { CMP_M(IND,IX);  }
static void cmp_ind_ixd(void)   { CMP_M(IND,IXD); }
static void cmp_de_rg(void)     { CMP_M(DE,RG);   }
static void cmp_de_rgd(void)    { CMP_M(DE,RGD);  }
static void cmp_de_in(void)     { CMP_M(DE,IN);   }
static void cmp_de_ind(void)    { CMP_M(DE,IND);  }
static void cmp_de_de(void)     { CMP_M(DE,DE);   }
static void cmp_de_ded(void)    { CMP_M(DE,DED);  }
static void cmp_de_ix(void)     { CMP_M(DE,IX);   }
static void cmp_de_ixd(void)    { CMP_M(DE,IXD);  }
static void cmp_ded_rg(void)    { CMP_M(DED,RG);  }
static void cmp_ded_rgd(void)   { CMP_M(DED,RGD); }
static void cmp_ded_in(void)    { CMP_M(DED,IN);  }
static void cmp_ded_ind(void)   { CMP_M(DED,IND); }
static void cmp_ded_de(void)    { CMP_M(DED,DE);  }
static void cmp_ded_ded(void)   { CMP_M(DED,DED); }
static void cmp_ded_ix(void)    { CMP_M(DED,IX);  }
static void cmp_ded_ixd(void)   { CMP_M(DED,IXD); }
static void cmp_ix_rg(void)     { CMP_M(IX,RG);   }
static void cmp_ix_rgd(void)    { CMP_M(IX,RGD);  }
static void cmp_ix_in(void)     { CMP_M(IX,IN);   }
static void cmp_ix_ind(void)    { CMP_M(IX,IND);  }
static void cmp_ix_de(void)     { CMP_M(IX,DE);   }
static void cmp_ix_ded(void)    { CMP_M(IX,DED);  }
static void cmp_ix_ix(void)     { CMP_M(IX,IX);   }
static void cmp_ix_ixd(void)    { CMP_M(IX,IXD);  }
static void cmp_ixd_rg(void)    { CMP_M(IXD,RG);  }
static void cmp_ixd_rgd(void)   { CMP_M(IXD,RGD); }
static void cmp_ixd_in(void)    { CMP_M(IXD,IN);  }
static void cmp_ixd_ind(void)   { CMP_M(IXD,IND); }
static void cmp_ixd_de(void)    { CMP_M(IXD,DE);  }
static void cmp_ixd_ded(void)   { CMP_M(IXD,DED); }
static void cmp_ixd_ix(void)    { CMP_M(IXD,IX);  }
static void cmp_ixd_ixd(void)   { CMP_M(IXD,IXD); }

static void bit_rg_rg(void)     { BIT_R(RG,RG);   }
static void bit_rg_rgd(void)    { BIT_M(RG,RGD);  }
static void bit_rg_in(void)     { BIT_M(RG,IN);   }
static void bit_rg_ind(void)    { BIT_M(RG,IND);  }
static void bit_rg_de(void)     { BIT_M(RG,DE);   }
static void bit_rg_ded(void)    { BIT_M(RG,DED);  }
static void bit_rg_ix(void)     { BIT_M(RG,IX);   }
static void bit_rg_ixd(void)    { BIT_M(RG,IXD);  }
static void bit_rgd_rg(void)    { BIT_M(RGD,RG);  }
static void bit_rgd_rgd(void)   { BIT_M(RGD,RGD); }
static void bit_rgd_in(void)    { BIT_M(RGD,IN);  }
static void bit_rgd_ind(void)   { BIT_M(RGD,IND); }
static void bit_rgd_de(void)    { BIT_M(RGD,DE);  }
static void bit_rgd_ded(void)   { BIT_M(RGD,DED); }
static void bit_rgd_ix(void)    { BIT_M(RGD,IX);  }
static void bit_rgd_ixd(void)   { BIT_M(RGD,IXD); }
static void bit_in_rg(void)     { BIT_M(IN,RG);   }
static void bit_in_rgd(void)    { BIT_M(IN,RGD);  }
static void bit_in_in(void)     { BIT_M(IN,IN);   }
static void bit_in_ind(void)    { BIT_M(IN,IND);  }
static void bit_in_de(void)     { BIT_M(IN,DE);   }
static void bit_in_ded(void)    { BIT_M(IN,DED);  }
static void bit_in_ix(void)     { BIT_M(IN,IX);   }
static void bit_in_ixd(void)    { BIT_M(IN,IXD);  }
static void bit_ind_rg(void)    { BIT_M(IND,RG);  }
static void bit_ind_rgd(void)   { BIT_M(IND,RGD); }
static void bit_ind_in(void)    { BIT_M(IND,IN);  }
static void bit_ind_ind(void)   { BIT_M(IND,IND); }
static void bit_ind_de(void)    { BIT_M(IND,DE);  }
static void bit_ind_ded(void)   { BIT_M(IND,DED); }
static void bit_ind_ix(void)    { BIT_M(IND,IX);  }
static void bit_ind_ixd(void)   { BIT_M(IND,IXD); }
static void bit_de_rg(void)     { BIT_M(DE,RG);   }
static void bit_de_rgd(void)    { BIT_M(DE,RGD);  }
static void bit_de_in(void)     { BIT_M(DE,IN);   }
static void bit_de_ind(void)    { BIT_M(DE,IND);  }
static void bit_de_de(void)     { BIT_M(DE,DE);   }
static void bit_de_ded(void)    { BIT_M(DE,DED);  }
static void bit_de_ix(void)     { BIT_M(DE,IX);   }
static void bit_de_ixd(void)    { BIT_M(DE,IXD);  }
static void bit_ded_rg(void)    { BIT_M(DED,RG);  }
static void bit_ded_rgd(void)   { BIT_M(DED,RGD); }
static void bit_ded_in(void)    { BIT_M(DED,IN);  }
static void bit_ded_ind(void)   { BIT_M(DED,IND); }
static void bit_ded_de(void)    { BIT_M(DED,DE);  }
static void bit_ded_ded(void)   { BIT_M(DED,DED); }
static void bit_ded_ix(void)    { BIT_M(DED,IX);  }
static void bit_ded_ixd(void)   { BIT_M(DED,IXD); }
static void bit_ix_rg(void)     { BIT_M(IX,RG);   }
static void bit_ix_rgd(void)    { BIT_M(IX,RGD);  }
static void bit_ix_in(void)     { BIT_M(IX,IN);   }
static void bit_ix_ind(void)    { BIT_M(IX,IND);  }
static void bit_ix_de(void)     { BIT_M(IX,DE);   }
static void bit_ix_ded(void)    { BIT_M(IX,DED);  }
static void bit_ix_ix(void)     { BIT_M(IX,IX);   }
static void bit_ix_ixd(void)    { BIT_M(IX,IXD);  }
static void bit_ixd_rg(void)    { BIT_M(IXD,RG);  }
static void bit_ixd_rgd(void)   { BIT_M(IXD,RGD); }
static void bit_ixd_in(void)    { BIT_M(IXD,IN);  }
static void bit_ixd_ind(void)   { BIT_M(IXD,IND); }
static void bit_ixd_de(void)    { BIT_M(IXD,DE);  }
static void bit_ixd_ded(void)   { BIT_M(IXD,DED); }
static void bit_ixd_ix(void)    { BIT_M(IXD,IX);  }
static void bit_ixd_ixd(void)   { BIT_M(IXD,IXD); }

static void bic_rg_rg(void)     { BIC_R(RG,RG);   }
static void bic_rg_rgd(void)    { BIC_M(RG,RGD);  }
static void bic_rg_in(void)     { BIC_M(RG,IN);   }
static void bic_rg_ind(void)    { BIC_M(RG,IND);  }
static void bic_rg_de(void)     { BIC_M(RG,DE);   }
static void bic_rg_ded(void)    { BIC_M(RG,DED);  }
static void bic_rg_ix(void)     { BIC_M(RG,IX);   }
static void bic_rg_ixd(void)    { BIC_M(RG,IXD);  }
static void bic_rgd_rg(void)    { BIC_X(RGD,RG);  }
static void bic_rgd_rgd(void)   { BIC_M(RGD,RGD); }
static void bic_rgd_in(void)    { BIC_M(RGD,IN);  }
static void bic_rgd_ind(void)   { BIC_M(RGD,IND); }
static void bic_rgd_de(void)    { BIC_M(RGD,DE);  }
static void bic_rgd_ded(void)   { BIC_M(RGD,DED); }
static void bic_rgd_ix(void)    { BIC_M(RGD,IX);  }
static void bic_rgd_ixd(void)   { BIC_M(RGD,IXD); }
static void bic_in_rg(void)     { BIC_X(IN,RG);   }
static void bic_in_rgd(void)    { BIC_M(IN,RGD);  }
static void bic_in_in(void)     { BIC_M(IN,IN);   }
static void bic_in_ind(void)    { BIC_M(IN,IND);  }
static void bic_in_de(void)     { BIC_M(IN,DE);   }
static void bic_in_ded(void)    { BIC_M(IN,DED);  }
static void bic_in_ix(void)     { BIC_M(IN,IX);   }
static void bic_in_ixd(void)    { BIC_M(IN,IXD);  }
static void bic_ind_rg(void)    { BIC_X(IND,RG);  }
static void bic_ind_rgd(void)   { BIC_M(IND,RGD); }
static void bic_ind_in(void)    { BIC_M(IND,IN);  }
static void bic_ind_ind(void)   { BIC_M(IND,IND); }
static void bic_ind_de(void)    { BIC_M(IND,DE);  }
static void bic_ind_ded(void)   { BIC_M(IND,DED); }
static void bic_ind_ix(void)    { BIC_M(IND,IX);  }
static void bic_ind_ixd(void)   { BIC_M(IND,IXD); }
static void bic_de_rg(void)     { BIC_X(DE,RG);   }
static void bic_de_rgd(void)    { BIC_M(DE,RGD);  }
static void bic_de_in(void)     { BIC_M(DE,IN);   }
static void bic_de_ind(void)    { BIC_M(DE,IND);  }
static void bic_de_de(void)     { BIC_M(DE,DE);   }
static void bic_de_ded(void)    { BIC_M(DE,DED);  }
static void bic_de_ix(void)     { BIC_M(DE,IX);   }
static void bic_de_ixd(void)    { BIC_M(DE,IXD);  }
static void bic_ded_rg(void)    { BIC_X(DED,RG);  }
static void bic_ded_rgd(void)   { BIC_M(DED,RGD); }
static void bic_ded_in(void)    { BIC_M(DED,IN);  }
static void bic_ded_ind(void)   { BIC_M(DED,IND); }
static void bic_ded_de(void)    { BIC_M(DED,DE);  }
static void bic_ded_ded(void)   { BIC_M(DED,DED); }
static void bic_ded_ix(void)    { BIC_M(DED,IX);  }
static void bic_ded_ixd(void)   { BIC_M(DED,IXD); }
static void bic_ix_rg(void)     { BIC_X(IX,RG);   }
static void bic_ix_rgd(void)    { BIC_M(IX,RGD);  }
static void bic_ix_in(void)     { BIC_M(IX,IN);   }
static void bic_ix_ind(void)    { BIC_M(IX,IND);  }
static void bic_ix_de(void)     { BIC_M(IX,DE);   }
static void bic_ix_ded(void)    { BIC_M(IX,DED);  }
static void bic_ix_ix(void)     { BIC_M(IX,IX);   }
static void bic_ix_ixd(void)    { BIC_M(IX,IXD);  }
static void bic_ixd_rg(void)    { BIC_X(IXD,RG);  }
static void bic_ixd_rgd(void)   { BIC_M(IXD,RGD); }
static void bic_ixd_in(void)    { BIC_M(IXD,IN);  }
static void bic_ixd_ind(void)   { BIC_M(IXD,IND); }
static void bic_ixd_de(void)    { BIC_M(IXD,DE);  }
static void bic_ixd_ded(void)   { BIC_M(IXD,DED); }
static void bic_ixd_ix(void)    { BIC_M(IXD,IX);  }
static void bic_ixd_ixd(void)   { BIC_M(IXD,IXD); }

static void bis_rg_rg(void)     { BIS_R(RG,RG);   }
static void bis_rg_rgd(void)    { BIS_M(RG,RGD);  }
static void bis_rg_in(void)     { BIS_M(RG,IN);   }
static void bis_rg_ind(void)    { BIS_M(RG,IND);  }
static void bis_rg_de(void)     { BIS_M(RG,DE);   }
static void bis_rg_ded(void)    { BIS_M(RG,DED);  }
static void bis_rg_ix(void)     { BIS_M(RG,IX);   }
static void bis_rg_ixd(void)    { BIS_M(RG,IXD);  }
static void bis_rgd_rg(void)    { BIS_X(RGD,RG);  }
static void bis_rgd_rgd(void)   { BIS_M(RGD,RGD); }
static void bis_rgd_in(void)    { BIS_M(RGD,IN);  }
static void bis_rgd_ind(void)   { BIS_M(RGD,IND); }
static void bis_rgd_de(void)    { BIS_M(RGD,DE);  }
static void bis_rgd_ded(void)   { BIS_M(RGD,DED); }
static void bis_rgd_ix(void)    { BIS_M(RGD,IX);  }
static void bis_rgd_ixd(void)   { BIS_M(RGD,IXD); }
static void bis_in_rg(void)     { BIS_X(IN,RG);   }
static void bis_in_rgd(void)    { BIS_M(IN,RGD);  }
static void bis_in_in(void)     { BIS_M(IN,IN);   }
static void bis_in_ind(void)    { BIS_M(IN,IND);  }
static void bis_in_de(void)     { BIS_M(IN,DE);   }
static void bis_in_ded(void)    { BIS_M(IN,DED);  }
static void bis_in_ix(void)     { BIS_M(IN,IX);   }
static void bis_in_ixd(void)    { BIS_M(IN,IXD);  }
static void bis_ind_rg(void)    { BIS_X(IND,RG);  }
static void bis_ind_rgd(void)   { BIS_M(IND,RGD); }
static void bis_ind_in(void)    { BIS_M(IND,IN);  }
static void bis_ind_ind(void)   { BIS_M(IND,IND); }
static void bis_ind_de(void)    { BIS_M(IND,DE);  }
static void bis_ind_ded(void)   { BIS_M(IND,DED); }
static void bis_ind_ix(void)    { BIS_M(IND,IX);  }
static void bis_ind_ixd(void)   { BIS_M(IND,IXD); }
static void bis_de_rg(void)     { BIS_X(DE,RG);   }
static void bis_de_rgd(void)    { BIS_M(DE,RGD);  }
static void bis_de_in(void)     { BIS_M(DE,IN);   }
static void bis_de_ind(void)    { BIS_M(DE,IND);  }
static void bis_de_de(void)     { BIS_M(DE,DE);   }
static void bis_de_ded(void)    { BIS_M(DE,DED);  }
static void bis_de_ix(void)     { BIS_M(DE,IX);   }
static void bis_de_ixd(void)    { BIS_M(DE,IXD);  }
static void bis_ded_rg(void)    { BIS_X(DED,RG);  }
static void bis_ded_rgd(void)   { BIS_M(DED,RGD); }
static void bis_ded_in(void)    { BIS_M(DED,IN);  }
static void bis_ded_ind(void)   { BIS_M(DED,IND); }
static void bis_ded_de(void)    { BIS_M(DED,DE);  }
static void bis_ded_ded(void)   { BIS_M(DED,DED); }
static void bis_ded_ix(void)    { BIS_M(DED,IX);  }
static void bis_ded_ixd(void)   { BIS_M(DED,IXD); }
static void bis_ix_rg(void)     { BIS_X(IX,RG);   }
static void bis_ix_rgd(void)    { BIS_M(IX,RGD);  }
static void bis_ix_in(void)     { BIS_M(IX,IN);   }
static void bis_ix_ind(void)    { BIS_M(IX,IND);  }
static void bis_ix_de(void)     { BIS_M(IX,DE);   }
static void bis_ix_ded(void)    { BIS_M(IX,DED);  }
static void bis_ix_ix(void)     { BIS_M(IX,IX);   }
static void bis_ix_ixd(void)    { BIS_M(IX,IXD);  }
static void bis_ixd_rg(void)    { BIS_X(IXD,RG);  }
static void bis_ixd_rgd(void)   { BIS_M(IXD,RGD); }
static void bis_ixd_in(void)    { BIS_M(IXD,IN);  }
static void bis_ixd_ind(void)   { BIS_M(IXD,IND); }
static void bis_ixd_de(void)    { BIS_M(IXD,DE);  }
static void bis_ixd_ded(void)   { BIS_M(IXD,DED); }
static void bis_ixd_ix(void)    { BIS_M(IXD,IX);  }
static void bis_ixd_ixd(void)   { BIS_M(IXD,IXD); }

static void add_rg_rg(void)     { ADD_R(RG,RG);   }
static void add_rg_rgd(void)    { ADD_M(RG,RGD);  }
static void add_rg_in(void)     { ADD_M(RG,IN);   }
static void add_rg_ind(void)    { ADD_M(RG,IND);  }
static void add_rg_de(void)     { ADD_M(RG,DE);   }
static void add_rg_ded(void)    { ADD_M(RG,DED);  }
static void add_rg_ix(void)     { ADD_M(RG,IX);   }
static void add_rg_ixd(void)    { ADD_M(RG,IXD);  }
static void add_rgd_rg(void)    { ADD_X(RGD,RG);  }
static void add_rgd_rgd(void)   { ADD_M(RGD,RGD); }
static void add_rgd_in(void)    { ADD_M(RGD,IN);  }
static void add_rgd_ind(void)   { ADD_M(RGD,IND); }
static void add_rgd_de(void)    { ADD_M(RGD,DE);  }
static void add_rgd_ded(void)   { ADD_M(RGD,DED); }
static void add_rgd_ix(void)    { ADD_M(RGD,IX);  }
static void add_rgd_ixd(void)   { ADD_M(RGD,IXD); }
static void add_in_rg(void)     { ADD_X(IN,RG);   }
static void add_in_rgd(void)    { ADD_M(IN,RGD);  }
static void add_in_in(void)     { ADD_M(IN,IN);   }
static void add_in_ind(void)    { ADD_M(IN,IND);  }
static void add_in_de(void)     { ADD_M(IN,DE);   }
static void add_in_ded(void)    { ADD_M(IN,DED);  }
static void add_in_ix(void)     { ADD_M(IN,IX);   }
static void add_in_ixd(void)    { ADD_M(IN,IXD);  }
static void add_ind_rg(void)    { ADD_X(IND,RG);  }
static void add_ind_rgd(void)   { ADD_M(IND,RGD); }
static void add_ind_in(void)    { ADD_M(IND,IN);  }
static void add_ind_ind(void)   { ADD_M(IND,IND); }
static void add_ind_de(void)    { ADD_M(IND,DE);  }
static void add_ind_ded(void)   { ADD_M(IND,DED); }
static void add_ind_ix(void)    { ADD_M(IND,IX);  }
static void add_ind_ixd(void)   { ADD_M(IND,IXD); }
static void add_de_rg(void)     { ADD_X(DE,RG);   }
static void add_de_rgd(void)    { ADD_M(DE,RGD);  }
static void add_de_in(void)     { ADD_M(DE,IN);   }
static void add_de_ind(void)    { ADD_M(DE,IND);  }
static void add_de_de(void)     { ADD_M(DE,DE);   }
static void add_de_ded(void)    { ADD_M(DE,DED);  }
static void add_de_ix(void)     { ADD_M(DE,IX);   }
static void add_de_ixd(void)    { ADD_M(DE,IXD);  }
static void add_ded_rg(void)    { ADD_X(DED,RG);  }
static void add_ded_rgd(void)   { ADD_M(DED,RGD); }
static void add_ded_in(void)    { ADD_M(DED,IN);  }
static void add_ded_ind(void)   { ADD_M(DED,IND); }
static void add_ded_de(void)    { ADD_M(DED,DE);  }
static void add_ded_ded(void)   { ADD_M(DED,DED); }
static void add_ded_ix(void)    { ADD_M(DED,IX);  }
static void add_ded_ixd(void)   { ADD_M(DED,IXD); }
static void add_ix_rg(void)     { ADD_X(IX,RG);   }
static void add_ix_rgd(void)    { ADD_M(IX,RGD);  }
static void add_ix_in(void)     { ADD_M(IX,IN);   }
static void add_ix_ind(void)    { ADD_M(IX,IND);  }
static void add_ix_de(void)     { ADD_M(IX,DE);   }
static void add_ix_ded(void)    { ADD_M(IX,DED);  }
static void add_ix_ix(void)     { ADD_M(IX,IX);   }
static void add_ix_ixd(void)    { ADD_M(IX,IXD);  }
static void add_ixd_rg(void)    { ADD_X(IXD,RG);  }
static void add_ixd_rgd(void)   { ADD_M(IXD,RGD); }
static void add_ixd_in(void)    { ADD_M(IXD,IN);  }
static void add_ixd_ind(void)   { ADD_M(IXD,IND); }
static void add_ixd_de(void)    { ADD_M(IXD,DE);  }
static void add_ixd_ded(void)   { ADD_M(IXD,DED); }
static void add_ixd_ix(void)    { ADD_M(IXD,IX);  }
static void add_ixd_ixd(void)   { ADD_M(IXD,IXD); }

static void xor_rg(void)        { XOR_R(RG);  }
static void xor_rgd(void)       { XOR_M(RGD); }
static void xor_in(void)        { XOR_M(IN);  }
static void xor_ind(void)       { XOR_M(IND); }
static void xor_de(void)        { XOR_M(DE);  }
static void xor_ded(void)       { XOR_M(DED); }
static void xor_ix(void)        { XOR_M(IX);  }
static void xor_ixd(void)       { XOR_M(IXD); }

static void sob(void)
{
	int sreg, source;

	GET_SREG; source = REGD(sreg);
	source -= 1;
	REGW(sreg) = source;
	if (source)
		PC -= 2 * (t11.op & 0x3f);
}

static void bpl(void)           { BR(!GET_N); }
static void bmi(void)           { BR( GET_N); }
static void bhi(void)           { BR(!GET_C && !GET_Z); }
static void blos(void)          { BR( GET_C ||  GET_Z); }
static void bvc(void)           { BR(!GET_V); }
static void bvs(void)           { BR( GET_V); }
static void bcc(void)           { BR(!GET_C); }
static void bcs(void)           { BR( GET_C); }

static void emt(void)
{
	PUSH(PSW);
	PUSH(PC);
	PC = RWORD(0x18);
	PSW = RWORD(0x1a);
	t11_check_irqs();
}

static void trap(void)
{
	PUSH(PSW);
	PUSH(PC);
	PC = RWORD(0x1c);
	PSW = RWORD(0x1e);
	t11_check_irqs();
}

static void clrb_rg(void)       { CLRB_R(RG);  }
static void clrb_rgd(void)      { CLRB_M(RGD); }
static void clrb_in(void)       { CLRB_M(IN);  }
static void clrb_ind(void)      { CLRB_M(IND); }
static void clrb_de(void)       { CLRB_M(DE);  }
static void clrb_ded(void)      { CLRB_M(DED); }
static void clrb_ix(void)       { CLRB_M(IX);  }
static void clrb_ixd(void)      { CLRB_M(IXD); }

static void comb_rg(void)       { COMB_R(RG);  }
static void comb_rgd(void)      { COMB_M(RGD); }
static void comb_in(void)       { COMB_M(IN);  }
static void comb_ind(void)      { COMB_M(IND); }
static void comb_de(void)       { COMB_M(DE);  }
static void comb_ded(void)      { COMB_M(DED); }
static void comb_ix(void)       { COMB_M(IX);  }
static void comb_ixd(void)      { COMB_M(IXD); }

static void incb_rg(void)       { INCB_R(RG);  }
static void incb_rgd(void)      { INCB_M(RGD); }
static void incb_in(void)       { INCB_M(IN);  }
static void incb_ind(void)      { INCB_M(IND); }
static void incb_de(void)       { INCB_M(DE);  }
static void incb_ded(void)      { INCB_M(DED); }
static void incb_ix(void)       { INCB_M(IX);  }
static void incb_ixd(void)      { INCB_M(IXD); }

static void decb_rg(void)       { DECB_R(RG);  }
static void decb_rgd(void)      { DECB_M(RGD); }
static void decb_in(void)       { DECB_M(IN);  }
static void decb_ind(void)      { DECB_M(IND); }
static void decb_de(void)       { DECB_M(DE);  }
static void decb_ded(void)      { DECB_M(DED); }
static void decb_ix(void)       { DECB_M(IX);  }
static void decb_ixd(void)      { DECB_M(IXD); }

static void negb_rg(void)       { NEGB_R(RG);  }
static void negb_rgd(void)      { NEGB_M(RGD); }
static void negb_in(void)       { NEGB_M(IN);  }
static void negb_ind(void)      { NEGB_M(IND); }
static void negb_de(void)       { NEGB_M(DE);  }
static void negb_ded(void)      { NEGB_M(DED); }
static void negb_ix(void)       { NEGB_M(IX);  }
static void negb_ixd(void)      { NEGB_M(IXD); }

static void adcb_rg(void)       { ADCB_R(RG);  }
static void adcb_rgd(void)      { ADCB_M(RGD); }
static void adcb_in(void)       { ADCB_M(IN);  }
static void adcb_ind(void)      { ADCB_M(IND); }
static void adcb_de(void)       { ADCB_M(DE);  }
static void adcb_ded(void)      { ADCB_M(DED); }
static void adcb_ix(void)       { ADCB_M(IX);  }
static void adcb_ixd(void)      { ADCB_M(IXD); }

static void sbcb_rg(void)       { SBCB_R(RG);  }
static void sbcb_rgd(void)      { SBCB_M(RGD); }
static void sbcb_in(void)       { SBCB_M(IN);  }
static void sbcb_ind(void)      { SBCB_M(IND); }
static void sbcb_de(void)       { SBCB_M(DE);  }
static void sbcb_ded(void)      { SBCB_M(DED); }
static void sbcb_ix(void)       { SBCB_M(IX);  }
static void sbcb_ixd(void)      { SBCB_M(IXD); }

static void tstb_rg(void)       { TSTB_R(RG);  }
static void tstb_rgd(void)      { TSTB_M(RGD); }
static void tstb_in(void)       { TSTB_M(IN);  }
static void tstb_ind(void)      { TSTB_M(IND); }
static void tstb_de(void)       { TSTB_M(DE);  }
static void tstb_ded(void)      { TSTB_M(DED); }
static void tstb_ix(void)       { TSTB_M(IX);  }
static void tstb_ixd(void)      { TSTB_M(IXD); }

static void rorb_rg(void)       { RORB_R(RG);  }
static void rorb_rgd(void)      { RORB_M(RGD); }
static void rorb_in(void)       { RORB_M(IN);  }
static void rorb_ind(void)      { RORB_M(IND); }
static void rorb_de(void)       { RORB_M(DE);  }
static void rorb_ded(void)      { RORB_M(DED); }
static void rorb_ix(void)       { RORB_M(IX);  }
static void rorb_ixd(void)      { RORB_M(IXD); }

static void rolb_rg(void)       { ROLB_R(RG);  }
static void rolb_rgd(void)      { ROLB_M(RGD); }
static void rolb_in(void)       { ROLB_M(IN);  }
static void rolb_ind(void)      { ROLB_M(IND); }
static void rolb_de(void)       { ROLB_M(DE);  }
static void rolb_ded(void)      { ROLB_M(DED); }
static void rolb_ix(void)       { ROLB_M(IX);  }
static void rolb_ixd(void)      { ROLB_M(IXD); }

static void asrb_rg(void)       { ASRB_R(RG);  }
static void asrb_rgd(void)      { ASRB_M(RGD); }
static void asrb_in(void)       { ASRB_M(IN);  }
static void asrb_ind(void)      { ASRB_M(IND); }
static void asrb_de(void)       { ASRB_M(DE);  }
static void asrb_ded(void)      { ASRB_M(DED); }
static void asrb_ix(void)       { ASRB_M(IX);  }
static void asrb_ixd(void)      { ASRB_M(IXD); }

static void aslb_rg(void)       { ASLB_R(RG);  }
static void aslb_rgd(void)      { ASLB_M(RGD); }
static void aslb_in(void)       { ASLB_M(IN);  }
static void aslb_ind(void)      { ASLB_M(IND); }
static void aslb_de(void)       { ASLB_M(DE);  }
static void aslb_ded(void)      { ASLB_M(DED); }
static void aslb_ix(void)       { ASLB_M(IX);  }
static void aslb_ixd(void)      { ASLB_M(IXD); }

static void mtps_rg(void)       { MTPS_R(RG);  }
static void mtps_rgd(void)      { MTPS_M(RGD); }
static void mtps_in(void)       { MTPS_M(IN);  }
static void mtps_ind(void)      { MTPS_M(IND); }
static void mtps_de(void)       { MTPS_M(DE);  }
static void mtps_ded(void)      { MTPS_M(DED); }
static void mtps_ix(void)       { MTPS_M(IX);  }
static void mtps_ixd(void)      { MTPS_M(IXD); }

static void mfps_rg(void)       { MFPS_R(RG);  }
static void mfps_rgd(void)      { MFPS_M(RGD); }
static void mfps_in(void)       { MFPS_M(IN);  }
static void mfps_ind(void)      { MFPS_M(IND); }
static void mfps_de(void)       { MFPS_M(DE);  }
static void mfps_ded(void)      { MFPS_M(DED); }
static void mfps_ix(void)       { MFPS_M(IX);  }
static void mfps_ixd(void)      { MFPS_M(IXD); }

static void movb_rg_rg(void)     { MOVB_R(RG,RG);   }
static void movb_rg_rgd(void)    { MOVB_M(RG,RGD);  }
static void movb_rg_in(void)     { MOVB_M(RG,IN);   }
static void movb_rg_ind(void)    { MOVB_M(RG,IND);  }
static void movb_rg_de(void)     { MOVB_M(RG,DE);   }
static void movb_rg_ded(void)    { MOVB_M(RG,DED);  }
static void movb_rg_ix(void)     { MOVB_M(RG,IX);   }
static void movb_rg_ixd(void)    { MOVB_M(RG,IXD);  }
static void movb_rgd_rg(void)    { MOVB_X(RGD,RG);  }
static void movb_rgd_rgd(void)   { MOVB_M(RGD,RGD); }
static void movb_rgd_in(void)    { MOVB_M(RGD,IN);  }
static void movb_rgd_ind(void)   { MOVB_M(RGD,IND); }
static void movb_rgd_de(void)    { MOVB_M(RGD,DE);  }
static void movb_rgd_ded(void)   { MOVB_M(RGD,DED); }
static void movb_rgd_ix(void)    { MOVB_M(RGD,IX);  }
static void movb_rgd_ixd(void)   { MOVB_M(RGD,IXD); }
static void movb_in_rg(void)     { MOVB_X(IN,RG);   }
static void movb_in_rgd(void)    { MOVB_M(IN,RGD);  }
static void movb_in_in(void)     { MOVB_M(IN,IN);   }
static void movb_in_ind(void)    { MOVB_M(IN,IND);  }
static void movb_in_de(void)     { MOVB_M(IN,DE);   }
static void movb_in_ded(void)    { MOVB_M(IN,DED);  }
static void movb_in_ix(void)     { MOVB_M(IN,IX);   }
static void movb_in_ixd(void)    { MOVB_M(IN,IXD);  }
static void movb_ind_rg(void)    { MOVB_X(IND,RG);  }
static void movb_ind_rgd(void)   { MOVB_M(IND,RGD); }
static void movb_ind_in(void)    { MOVB_M(IND,IN);  }
static void movb_ind_ind(void)   { MOVB_M(IND,IND); }
static void movb_ind_de(void)    { MOVB_M(IND,DE);  }
static void movb_ind_ded(void)   { MOVB_M(IND,DED); }
static void movb_ind_ix(void)    { MOVB_M(IND,IX);  }
static void movb_ind_ixd(void)   { MOVB_M(IND,IXD); }
static void movb_de_rg(void)     { MOVB_X(DE,RG);   }
static void movb_de_rgd(void)    { MOVB_M(DE,RGD);  }
static void movb_de_in(void)     { MOVB_M(DE,IN);   }
static void movb_de_ind(void)    { MOVB_M(DE,IND);  }
static void movb_de_de(void)     { MOVB_M(DE,DE);   }
static void movb_de_ded(void)    { MOVB_M(DE,DED);  }
static void movb_de_ix(void)     { MOVB_M(DE,IX);   }
static void movb_de_ixd(void)    { MOVB_M(DE,IXD);  }
static void movb_ded_rg(void)    { MOVB_X(DED,RG);  }
static void movb_ded_rgd(void)   { MOVB_M(DED,RGD); }
static void movb_ded_in(void)    { MOVB_M(DED,IN);  }
static void movb_ded_ind(void)   { MOVB_M(DED,IND); }
static void movb_ded_de(void)    { MOVB_M(DED,DE);  }
static void movb_ded_ded(void)   { MOVB_M(DED,DED); }
static void movb_ded_ix(void)    { MOVB_M(DED,IX);  }
static void movb_ded_ixd(void)   { MOVB_M(DED,IXD); }
static void movb_ix_rg(void)     { MOVB_X(IX,RG);   }
static void movb_ix_rgd(void)    { MOVB_M(IX,RGD);  }
static void movb_ix_in(void)     { MOVB_M(IX,IN);   }
static void movb_ix_ind(void)    { MOVB_M(IX,IND);  }
static void movb_ix_de(void)     { MOVB_M(IX,DE);   }
static void movb_ix_ded(void)    { MOVB_M(IX,DED);  }
static void movb_ix_ix(void)     { MOVB_M(IX,IX);   }
static void movb_ix_ixd(void)    { MOVB_M(IX,IXD);  }
static void movb_ixd_rg(void)    { MOVB_X(IXD,RG);  }
static void movb_ixd_rgd(void)   { MOVB_M(IXD,RGD); }
static void movb_ixd_in(void)    { MOVB_M(IXD,IN);  }
static void movb_ixd_ind(void)   { MOVB_M(IXD,IND); }
static void movb_ixd_de(void)    { MOVB_M(IXD,DE);  }
static void movb_ixd_ded(void)   { MOVB_M(IXD,DED); }
static void movb_ixd_ix(void)    { MOVB_M(IXD,IX);  }
static void movb_ixd_ixd(void)   { MOVB_M(IXD,IXD); }

static void cmpb_rg_rg(void)     { CMPB_R(RG,RG);   }
static void cmpb_rg_rgd(void)    { CMPB_M(RG,RGD);  }
static void cmpb_rg_in(void)     { CMPB_M(RG,IN);   }
static void cmpb_rg_ind(void)    { CMPB_M(RG,IND);  }
static void cmpb_rg_de(void)     { CMPB_M(RG,DE);   }
static void cmpb_rg_ded(void)    { CMPB_M(RG,DED);  }
static void cmpb_rg_ix(void)     { CMPB_M(RG,IX);   }
static void cmpb_rg_ixd(void)    { CMPB_M(RG,IXD);  }
static void cmpb_rgd_rg(void)    { CMPB_M(RGD,RG);  }
static void cmpb_rgd_rgd(void)   { CMPB_M(RGD,RGD); }
static void cmpb_rgd_in(void)    { CMPB_M(RGD,IN);  }
static void cmpb_rgd_ind(void)   { CMPB_M(RGD,IND); }
static void cmpb_rgd_de(void)    { CMPB_M(RGD,DE);  }
static void cmpb_rgd_ded(void)   { CMPB_M(RGD,DED); }
static void cmpb_rgd_ix(void)    { CMPB_M(RGD,IX);  }
static void cmpb_rgd_ixd(void)   { CMPB_M(RGD,IXD); }
static void cmpb_in_rg(void)     { CMPB_M(IN,RG);   }
static void cmpb_in_rgd(void)    { CMPB_M(IN,RGD);  }
static void cmpb_in_in(void)     { CMPB_M(IN,IN);   }
static void cmpb_in_ind(void)    { CMPB_M(IN,IND);  }
static void cmpb_in_de(void)     { CMPB_M(IN,DE);   }
static void cmpb_in_ded(void)    { CMPB_M(IN,DED);  }
static void cmpb_in_ix(void)     { CMPB_M(IN,IX);   }
static void cmpb_in_ixd(void)    { CMPB_M(IN,IXD);  }
static void cmpb_ind_rg(void)    { CMPB_M(IND,RG);  }
static void cmpb_ind_rgd(void)   { CMPB_M(IND,RGD); }
static void cmpb_ind_in(void)    { CMPB_M(IND,IN);  }
static void cmpb_ind_ind(void)   { CMPB_M(IND,IND); }
static void cmpb_ind_de(void)    { CMPB_M(IND,DE);  }
static void cmpb_ind_ded(void)   { CMPB_M(IND,DED); }
static void cmpb_ind_ix(void)    { CMPB_M(IND,IX);  }
static void cmpb_ind_ixd(void)   { CMPB_M(IND,IXD); }
static void cmpb_de_rg(void)     { CMPB_M(DE,RG);   }
static void cmpb_de_rgd(void)    { CMPB_M(DE,RGD);  }
static void cmpb_de_in(void)     { CMPB_M(DE,IN);   }
static void cmpb_de_ind(void)    { CMPB_M(DE,IND);  }
static void cmpb_de_de(void)     { CMPB_M(DE,DE);   }
static void cmpb_de_ded(void)    { CMPB_M(DE,DED);  }
static void cmpb_de_ix(void)     { CMPB_M(DE,IX);   }
static void cmpb_de_ixd(void)    { CMPB_M(DE,IXD);  }
static void cmpb_ded_rg(void)    { CMPB_M(DED,RG);  }
static void cmpb_ded_rgd(void)   { CMPB_M(DED,RGD); }
static void cmpb_ded_in(void)    { CMPB_M(DED,IN);  }
static void cmpb_ded_ind(void)   { CMPB_M(DED,IND); }
static void cmpb_ded_de(void)    { CMPB_M(DED,DE);  }
static void cmpb_ded_ded(void)   { CMPB_M(DED,DED); }
static void cmpb_ded_ix(void)    { CMPB_M(DED,IX);  }
static void cmpb_ded_ixd(void)   { CMPB_M(DED,IXD); }
static void cmpb_ix_rg(void)     { CMPB_M(IX,RG);   }
static void cmpb_ix_rgd(void)    { CMPB_M(IX,RGD);  }
static void cmpb_ix_in(void)     { CMPB_M(IX,IN);   }
static void cmpb_ix_ind(void)    { CMPB_M(IX,IND);  }
static void cmpb_ix_de(void)     { CMPB_M(IX,DE);   }
static void cmpb_ix_ded(void)    { CMPB_M(IX,DED);  }
static void cmpb_ix_ix(void)     { CMPB_M(IX,IX);   }
static void cmpb_ix_ixd(void)    { CMPB_M(IX,IXD);  }
static void cmpb_ixd_rg(void)    { CMPB_M(IXD,RG);  }
static void cmpb_ixd_rgd(void)   { CMPB_M(IXD,RGD); }
static void cmpb_ixd_in(void)    { CMPB_M(IXD,IN);  }
static void cmpb_ixd_ind(void)   { CMPB_M(IXD,IND); }
static void cmpb_ixd_de(void)    { CMPB_M(IXD,DE);  }
static void cmpb_ixd_ded(void)   { CMPB_M(IXD,DED); }
static void cmpb_ixd_ix(void)    { CMPB_M(IXD,IX);  }
static void cmpb_ixd_ixd(void)   { CMPB_M(IXD,IXD); }

static void bitb_rg_rg(void)     { BITB_R(RG,RG);   }
static void bitb_rg_rgd(void)    { BITB_M(RG,RGD);  }
static void bitb_rg_in(void)     { BITB_M(RG,IN);   }
static void bitb_rg_ind(void)    { BITB_M(RG,IND);  }
static void bitb_rg_de(void)     { BITB_M(RG,DE);   }
static void bitb_rg_ded(void)    { BITB_M(RG,DED);  }
static void bitb_rg_ix(void)     { BITB_M(RG,IX);   }
static void bitb_rg_ixd(void)    { BITB_M(RG,IXD);  }
static void bitb_rgd_rg(void)    { BITB_M(RGD,RG);  }
static void bitb_rgd_rgd(void)   { BITB_M(RGD,RGD); }
static void bitb_rgd_in(void)    { BITB_M(RGD,IN);  }
static void bitb_rgd_ind(void)   { BITB_M(RGD,IND); }
static void bitb_rgd_de(void)    { BITB_M(RGD,DE);  }
static void bitb_rgd_ded(void)   { BITB_M(RGD,DED); }
static void bitb_rgd_ix(void)    { BITB_M(RGD,IX);  }
static void bitb_rgd_ixd(void)   { BITB_M(RGD,IXD); }
static void bitb_in_rg(void)     { BITB_M(IN,RG);   }
static void bitb_in_rgd(void)    { BITB_M(IN,RGD);  }
static void bitb_in_in(void)     { BITB_M(IN,IN);   }
static void bitb_in_ind(void)    { BITB_M(IN,IND);  }
static void bitb_in_de(void)     { BITB_M(IN,DE);   }
static void bitb_in_ded(void)    { BITB_M(IN,DED);  }
static void bitb_in_ix(void)     { BITB_M(IN,IX);   }
static void bitb_in_ixd(void)    { BITB_M(IN,IXD);  }
static void bitb_ind_rg(void)    { BITB_M(IND,RG);  }
static void bitb_ind_rgd(void)   { BITB_M(IND,RGD); }
static void bitb_ind_in(void)    { BITB_M(IND,IN);  }
static void bitb_ind_ind(void)   { BITB_M(IND,IND); }
static void bitb_ind_de(void)    { BITB_M(IND,DE);  }
static void bitb_ind_ded(void)   { BITB_M(IND,DED); }
static void bitb_ind_ix(void)    { BITB_M(IND,IX);  }
static void bitb_ind_ixd(void)   { BITB_M(IND,IXD); }
static void bitb_de_rg(void)     { BITB_M(DE,RG);   }
static void bitb_de_rgd(void)    { BITB_M(DE,RGD);  }
static void bitb_de_in(void)     { BITB_M(DE,IN);   }
static void bitb_de_ind(void)    { BITB_M(DE,IND);  }
static void bitb_de_de(void)     { BITB_M(DE,DE);   }
static void bitb_de_ded(void)    { BITB_M(DE,DED);  }
static void bitb_de_ix(void)     { BITB_M(DE,IX);   }
static void bitb_de_ixd(void)    { BITB_M(DE,IXD);  }
static void bitb_ded_rg(void)    { BITB_M(DED,RG);  }
static void bitb_ded_rgd(void)   { BITB_M(DED,RGD); }
static void bitb_ded_in(void)    { BITB_M(DED,IN);  }
static void bitb_ded_ind(void)   { BITB_M(DED,IND); }
static void bitb_ded_de(void)    { BITB_M(DED,DE);  }
static void bitb_ded_ded(void)   { BITB_M(DED,DED); }
static void bitb_ded_ix(void)    { BITB_M(DED,IX);  }
static void bitb_ded_ixd(void)   { BITB_M(DED,IXD); }
static void bitb_ix_rg(void)     { BITB_M(IX,RG);   }
static void bitb_ix_rgd(void)    { BITB_M(IX,RGD);  }
static void bitb_ix_in(void)     { BITB_M(IX,IN);   }
static void bitb_ix_ind(void)    { BITB_M(IX,IND);  }
static void bitb_ix_de(void)     { BITB_M(IX,DE);   }
static void bitb_ix_ded(void)    { BITB_M(IX,DED);  }
static void bitb_ix_ix(void)     { BITB_M(IX,IX);   }
static void bitb_ix_ixd(void)    { BITB_M(IX,IXD);  }
static void bitb_ixd_rg(void)    { BITB_M(IXD,RG);  }
static void bitb_ixd_rgd(void)   { BITB_M(IXD,RGD); }
static void bitb_ixd_in(void)    { BITB_M(IXD,IN);  }
static void bitb_ixd_ind(void)   { BITB_M(IXD,IND); }
static void bitb_ixd_de(void)    { BITB_M(IXD,DE);  }
static void bitb_ixd_ded(void)   { BITB_M(IXD,DED); }
static void bitb_ixd_ix(void)    { BITB_M(IXD,IX);  }
static void bitb_ixd_ixd(void)   { BITB_M(IXD,IXD); }

static void bicb_rg_rg(void)     { BICB_R(RG,RG);   }
static void bicb_rg_rgd(void)    { BICB_M(RG,RGD);  }
static void bicb_rg_in(void)     { BICB_M(RG,IN);   }
static void bicb_rg_ind(void)    { BICB_M(RG,IND);  }
static void bicb_rg_de(void)     { BICB_M(RG,DE);   }
static void bicb_rg_ded(void)    { BICB_M(RG,DED);  }
static void bicb_rg_ix(void)     { BICB_M(RG,IX);   }
static void bicb_rg_ixd(void)    { BICB_M(RG,IXD);  }
static void bicb_rgd_rg(void)    { BICB_X(RGD,RG);  }
static void bicb_rgd_rgd(void)   { BICB_M(RGD,RGD); }
static void bicb_rgd_in(void)    { BICB_M(RGD,IN);  }
static void bicb_rgd_ind(void)   { BICB_M(RGD,IND); }
static void bicb_rgd_de(void)    { BICB_M(RGD,DE);  }
static void bicb_rgd_ded(void)   { BICB_M(RGD,DED); }
static void bicb_rgd_ix(void)    { BICB_M(RGD,IX);  }
static void bicb_rgd_ixd(void)   { BICB_M(RGD,IXD); }
static void bicb_in_rg(void)     { BICB_X(IN,RG);   }
static void bicb_in_rgd(void)    { BICB_M(IN,RGD);  }
static void bicb_in_in(void)     { BICB_M(IN,IN);   }
static void bicb_in_ind(void)    { BICB_M(IN,IND);  }
static void bicb_in_de(void)     { BICB_M(IN,DE);   }
static void bicb_in_ded(void)    { BICB_M(IN,DED);  }
static void bicb_in_ix(void)     { BICB_M(IN,IX);   }
static void bicb_in_ixd(void)    { BICB_M(IN,IXD);  }
static void bicb_ind_rg(void)    { BICB_X(IND,RG);  }
static void bicb_ind_rgd(void)   { BICB_M(IND,RGD); }
static void bicb_ind_in(void)    { BICB_M(IND,IN);  }
static void bicb_ind_ind(void)   { BICB_M(IND,IND); }
static void bicb_ind_de(void)    { BICB_M(IND,DE);  }
static void bicb_ind_ded(void)   { BICB_M(IND,DED); }
static void bicb_ind_ix(void)    { BICB_M(IND,IX);  }
static void bicb_ind_ixd(void)   { BICB_M(IND,IXD); }
static void bicb_de_rg(void)     { BICB_X(DE,RG);   }
static void bicb_de_rgd(void)    { BICB_M(DE,RGD);  }
static void bicb_de_in(void)     { BICB_M(DE,IN);   }
static void bicb_de_ind(void)    { BICB_M(DE,IND);  }
static void bicb_de_de(void)     { BICB_M(DE,DE);   }
static void bicb_de_ded(void)    { BICB_M(DE,DED);  }
static void bicb_de_ix(void)     { BICB_M(DE,IX);   }
static void bicb_de_ixd(void)    { BICB_M(DE,IXD);  }
static void bicb_ded_rg(void)    { BICB_X(DED,RG);  }
static void bicb_ded_rgd(void)   { BICB_M(DED,RGD); }
static void bicb_ded_in(void)    { BICB_M(DED,IN);  }
static void bicb_ded_ind(void)   { BICB_M(DED,IND); }
static void bicb_ded_de(void)    { BICB_M(DED,DE);  }
static void bicb_ded_ded(void)   { BICB_M(DED,DED); }
static void bicb_ded_ix(void)    { BICB_M(DED,IX);  }
static void bicb_ded_ixd(void)   { BICB_M(DED,IXD); }
static void bicb_ix_rg(void)     { BICB_X(IX,RG);   }
static void bicb_ix_rgd(void)    { BICB_M(IX,RGD);  }
static void bicb_ix_in(void)     { BICB_M(IX,IN);   }
static void bicb_ix_ind(void)    { BICB_M(IX,IND);  }
static void bicb_ix_de(void)     { BICB_M(IX,DE);   }
static void bicb_ix_ded(void)    { BICB_M(IX,DED);  }
static void bicb_ix_ix(void)     { BICB_M(IX,IX);   }
static void bicb_ix_ixd(void)    { BICB_M(IX,IXD);  }
static void bicb_ixd_rg(void)    { BICB_X(IXD,RG);  }
static void bicb_ixd_rgd(void)   { BICB_M(IXD,RGD); }
static void bicb_ixd_in(void)    { BICB_M(IXD,IN);  }
static void bicb_ixd_ind(void)   { BICB_M(IXD,IND); }
static void bicb_ixd_de(void)    { BICB_M(IXD,DE);  }
static void bicb_ixd_ded(void)   { BICB_M(IXD,DED); }
static void bicb_ixd_ix(void)    { BICB_M(IXD,IX);  }
static void bicb_ixd_ixd(void)   { BICB_M(IXD,IXD); }

static void bisb_rg_rg(void)     { BISB_R(RG,RG);   }
static void bisb_rg_rgd(void)    { BISB_M(RG,RGD);  }
static void bisb_rg_in(void)     { BISB_M(RG,IN);   }
static void bisb_rg_ind(void)    { BISB_M(RG,IND);  }
static void bisb_rg_de(void)     { BISB_M(RG,DE);   }
static void bisb_rg_ded(void)    { BISB_M(RG,DED);  }
static void bisb_rg_ix(void)     { BISB_M(RG,IX);   }
static void bisb_rg_ixd(void)    { BISB_M(RG,IXD);  }
static void bisb_rgd_rg(void)    { BISB_X(RGD,RG);  }
static void bisb_rgd_rgd(void)   { BISB_M(RGD,RGD); }
static void bisb_rgd_in(void)    { BISB_M(RGD,IN);  }
static void bisb_rgd_ind(void)   { BISB_M(RGD,IND); }
static void bisb_rgd_de(void)    { BISB_M(RGD,DE);  }
static void bisb_rgd_ded(void)   { BISB_M(RGD,DED); }
static void bisb_rgd_ix(void)    { BISB_M(RGD,IX);  }
static void bisb_rgd_ixd(void)   { BISB_M(RGD,IXD); }
static void bisb_in_rg(void)     { BISB_X(IN,RG);   }
static void bisb_in_rgd(void)    { BISB_M(IN,RGD);  }
static void bisb_in_in(void)     { BISB_M(IN,IN);   }
static void bisb_in_ind(void)    { BISB_M(IN,IND);  }
static void bisb_in_de(void)     { BISB_M(IN,DE);   }
static void bisb_in_ded(void)    { BISB_M(IN,DED);  }
static void bisb_in_ix(void)     { BISB_M(IN,IX);   }
static void bisb_in_ixd(void)    { BISB_M(IN,IXD);  }
static void bisb_ind_rg(void)    { BISB_X(IND,RG);  }
static void bisb_ind_rgd(void)   { BISB_M(IND,RGD); }
static void bisb_ind_in(void)    { BISB_M(IND,IN);  }
static void bisb_ind_ind(void)   { BISB_M(IND,IND); }
static void bisb_ind_de(void)    { BISB_M(IND,DE);  }
static void bisb_ind_ded(void)   { BISB_M(IND,DED); }
static void bisb_ind_ix(void)    { BISB_M(IND,IX);  }
static void bisb_ind_ixd(void)   { BISB_M(IND,IXD); }
static void bisb_de_rg(void)     { BISB_X(DE,RG);   }
static void bisb_de_rgd(void)    { BISB_M(DE,RGD);  }
static void bisb_de_in(void)     { BISB_M(DE,IN);   }
static void bisb_de_ind(void)    { BISB_M(DE,IND);  }
static void bisb_de_de(void)     { BISB_M(DE,DE);   }
static void bisb_de_ded(void)    { BISB_M(DE,DED);  }
static void bisb_de_ix(void)     { BISB_M(DE,IX);   }
static void bisb_de_ixd(void)    { BISB_M(DE,IXD);  }
static void bisb_ded_rg(void)    { BISB_X(DED,RG);  }
static void bisb_ded_rgd(void)   { BISB_M(DED,RGD); }
static void bisb_ded_in(void)    { BISB_M(DED,IN);  }
static void bisb_ded_ind(void)   { BISB_M(DED,IND); }
static void bisb_ded_de(void)    { BISB_M(DED,DE);  }
static void bisb_ded_ded(void)   { BISB_M(DED,DED); }
static void bisb_ded_ix(void)    { BISB_M(DED,IX);  }
static void bisb_ded_ixd(void)   { BISB_M(DED,IXD); }
static void bisb_ix_rg(void)     { BISB_X(IX,RG);   }
static void bisb_ix_rgd(void)    { BISB_M(IX,RGD);  }
static void bisb_ix_in(void)     { BISB_M(IX,IN);   }
static void bisb_ix_ind(void)    { BISB_M(IX,IND);  }
static void bisb_ix_de(void)     { BISB_M(IX,DE);   }
static void bisb_ix_ded(void)    { BISB_M(IX,DED);  }
static void bisb_ix_ix(void)     { BISB_M(IX,IX);   }
static void bisb_ix_ixd(void)    { BISB_M(IX,IXD);  }
static void bisb_ixd_rg(void)    { BISB_X(IXD,RG);  }
static void bisb_ixd_rgd(void)   { BISB_M(IXD,RGD); }
static void bisb_ixd_in(void)    { BISB_M(IXD,IN);  }
static void bisb_ixd_ind(void)   { BISB_M(IXD,IND); }
static void bisb_ixd_de(void)    { BISB_M(IXD,DE);  }
static void bisb_ixd_ded(void)   { BISB_M(IXD,DED); }
static void bisb_ixd_ix(void)    { BISB_M(IXD,IX);  }
static void bisb_ixd_ixd(void)   { BISB_M(IXD,IXD); }

static void sub_rg_rg(void)     { SUB_R(RG,RG);   }
static void sub_rg_rgd(void)    { SUB_M(RG,RGD);  }
static void sub_rg_in(void)     { SUB_M(RG,IN);   }
static void sub_rg_ind(void)    { SUB_M(RG,IND);  }
static void sub_rg_de(void)     { SUB_M(RG,DE);   }
static void sub_rg_ded(void)    { SUB_M(RG,DED);  }
static void sub_rg_ix(void)     { SUB_M(RG,IX);   }
static void sub_rg_ixd(void)    { SUB_M(RG,IXD);  }
static void sub_rgd_rg(void)    { SUB_X(RGD,RG);  }
static void sub_rgd_rgd(void)   { SUB_M(RGD,RGD); }
static void sub_rgd_in(void)    { SUB_M(RGD,IN);  }
static void sub_rgd_ind(void)   { SUB_M(RGD,IND); }
static void sub_rgd_de(void)    { SUB_M(RGD,DE);  }
static void sub_rgd_ded(void)   { SUB_M(RGD,DED); }
static void sub_rgd_ix(void)    { SUB_M(RGD,IX);  }
static void sub_rgd_ixd(void)   { SUB_M(RGD,IXD); }
static void sub_in_rg(void)     { SUB_X(IN,RG);   }
static void sub_in_rgd(void)    { SUB_M(IN,RGD);  }
static void sub_in_in(void)     { SUB_M(IN,IN);   }
static void sub_in_ind(void)    { SUB_M(IN,IND);  }
static void sub_in_de(void)     { SUB_M(IN,DE);   }
static void sub_in_ded(void)    { SUB_M(IN,DED);  }
static void sub_in_ix(void)     { SUB_M(IN,IX);   }
static void sub_in_ixd(void)    { SUB_M(IN,IXD);  }
static void sub_ind_rg(void)    { SUB_X(IND,RG);  }
static void sub_ind_rgd(void)   { SUB_M(IND,RGD); }
static void sub_ind_in(void)    { SUB_M(IND,IN);  }
static void sub_ind_ind(void)   { SUB_M(IND,IND); }
static void sub_ind_de(void)    { SUB_M(IND,DE);  }
static void sub_ind_ded(void)   { SUB_M(IND,DED); }
static void sub_ind_ix(void)    { SUB_M(IND,IX);  }
static void sub_ind_ixd(void)   { SUB_M(IND,IXD); }
static void sub_de_rg(void)     { SUB_X(DE,RG);   }
static void sub_de_rgd(void)    { SUB_M(DE,RGD);  }
static void sub_de_in(void)     { SUB_M(DE,IN);   }
static void sub_de_ind(void)    { SUB_M(DE,IND);  }
static void sub_de_de(void)     { SUB_M(DE,DE);   }
static void sub_de_ded(void)    { SUB_M(DE,DED);  }
static void sub_de_ix(void)     { SUB_M(DE,IX);   }
static void sub_de_ixd(void)    { SUB_M(DE,IXD);  }
static void sub_ded_rg(void)    { SUB_X(DED,RG);  }
static void sub_ded_rgd(void)   { SUB_M(DED,RGD); }
static void sub_ded_in(void)    { SUB_M(DED,IN);  }
static void sub_ded_ind(void)   { SUB_M(DED,IND); }
static void sub_ded_de(void)    { SUB_M(DED,DE);  }
static void sub_ded_ded(void)   { SUB_M(DED,DED); }
static void sub_ded_ix(void)    { SUB_M(DED,IX);  }
static void sub_ded_ixd(void)   { SUB_M(DED,IXD); }
static void sub_ix_rg(void)     { SUB_X(IX,RG);   }
static void sub_ix_rgd(void)    { SUB_M(IX,RGD);  }
static void sub_ix_in(void)     { SUB_M(IX,IN);   }
static void sub_ix_ind(void)    { SUB_M(IX,IND);  }
static void sub_ix_de(void)     { SUB_M(IX,DE);   }
static void sub_ix_ded(void)    { SUB_M(IX,DED);  }
static void sub_ix_ix(void)     { SUB_M(IX,IX);   }
static void sub_ix_ixd(void)    { SUB_M(IX,IXD);  }
static void sub_ixd_rg(void)    { SUB_X(IXD,RG);  }
static void sub_ixd_rgd(void)   { SUB_M(IXD,RGD); }
static void sub_ixd_in(void)    { SUB_M(IXD,IN);  }
static void sub_ixd_ind(void)   { SUB_M(IXD,IND); }
static void sub_ixd_de(void)    { SUB_M(IXD,DE);  }
static void sub_ixd_ded(void)   { SUB_M(IXD,DED); }
static void sub_ixd_ix(void)    { SUB_M(IXD,IX);  }
static void sub_ixd_ixd(void)   { SUB_M(IXD,IXD); }
