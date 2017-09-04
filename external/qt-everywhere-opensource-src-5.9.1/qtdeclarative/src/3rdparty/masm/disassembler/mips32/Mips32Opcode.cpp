/*
 * Copyright (C) 2015 Cisco Systems, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY CISCO SYSTEMS, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CISCO SYSTEMS, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(MIPS32_DISASSEMBLER)

#include "Mips32Opcode.h"

#include <stdio.h>

#define OPCODE_FMT "%s\t"
#define COP1_OPCODE_FMT "%s.%s\t"
#define FORMAT_INSTR(_format, ...) \
    snprintf(m_formatBuffer, bufferSize - 1, _format, ##__VA_ARGS__)

const char *Mips32Opcode::registerName(uint8_t r)
{
    static const char *gpRegisters[] = {
        "zero", "AT", "v0", "v1", "a0", "a1", "a2", "a3",
        "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
        "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
        "t8", "t9", "kt0", "kt1", "gp", "sp", "s8", "ra"
    };

    return (r < sizeof(gpRegisters)) ? gpRegisters[r] : "invalid";
}

const char *Mips32Opcode::fpRegisterName(uint8_t r)
{
    static const char *fpRegisters[] = {
        "$f0", "$f1", "$f2", "$f3", "$f4", "$f5", "$f6", "$f7",
        "$f8", "$f9", "$f10", "$f11", "$f12", "$f13", "$f14", "$f15",
        "$f16", "$f17", "$f18", "$f19", "$f20", "$f21", "$f22", "$f23",
        "$f24", "$f25", "$f26", "$f27", "$f28", "$f29", "$f30", "$f31"
    };

    return (r < sizeof(fpRegisters)) ? fpRegisters[r] : "invalid";
}

void Mips32Opcode::formatSpecialEncodingOpcode(uint8_t op1, uint8_t op2, uint8_t dest, uint8_t shift, uint8_t function)
{
    const char *opcode;
    OpcodePrintFormat format = Unknown;
    switch (function) {
    case 0x00:
        format = RdRtSa;
        opcode = "sll";
        break;
    case 0x02:
        format = RdRtSa;
        opcode = "srl";
        break;
    case 0x03:
        format = RdRtSa;
        opcode = "sra";
        break;
    case 0x04:
        format = RdRtRs;
        opcode = "sllv";
        break;
    case 0x06:
        format = RdRtRs;
        opcode = "srlv";
        break;
    case 0x07:
        format = RdRtRs;
        opcode = "srav";
        break;
    case 0x08:
        format = Rs;
        opcode = "jr";
        break;
    case 0x09:
        format = (dest != 0x1f) ? RdRs : Rs;
        opcode = "jalr";
        break;
    case 0x10:
        format = Rd;
        opcode = "mfhi";
        break;
    case 0x11:
        format = Rs;
        opcode = "mthi";
        break;
    case 0x12:
        format = Rd;
        opcode = "mflo";
        break;
    case 0x13:
        format = Rs;
        opcode = "mtlo";
        break;
    case 0x18:
        format = RsRt;
        opcode = "mult";
        break;
    case 0x19:
        format = RsRt;
        opcode = "multu";
        break;
    case 0x1a:
        format = RsRt;
        opcode = "div";
        break;
    case 0x1b:
        format = RsRt;
        opcode = "divu";
        break;
    case 0x20:
        format = RdRsRt;
        opcode = "add";
        break;
    case 0x21:
        if (op2) {
            format = RdRsRt;
            opcode = "addu";
        } else {
            format = RdRs;
            opcode = "move";
        }
        break;
    case 0x22:
        format = RdRsRt;
        opcode = "sub";
        break;
    case 0x23:
        format = RdRsRt;
        opcode = "subu";
        break;
    case 0x24:
        format = RdRsRt;
        opcode = "and";
        break;
    case 0x25:
        format = RdRsRt;
        opcode = "or";
        break;
    case 0x26:
        format = RdRsRt;
        opcode = "xor";
        break;
    case 0x27:
        format = RdRsRt;
        opcode = "nor";
        break;
    case 0x2a:
        format = RdRsRt;
        opcode = "slt";
        break;
    case 0x2b:
        format = RdRsRt;
        opcode = "sltu";
        break;
    }

    switch (format) {
    case Rs:
        FORMAT_INSTR(OPCODE_FMT "%s", opcode, registerName(op1));
        break;
    case Rd:
        FORMAT_INSTR(OPCODE_FMT "%s", opcode, registerName(dest));
        break;
    case RdRs:
        FORMAT_INSTR(OPCODE_FMT "%s, %s", opcode, registerName(dest), registerName(op1));
        break;
    case RsRt:
        FORMAT_INSTR(OPCODE_FMT "%s, %s", opcode, registerName(op1), registerName(op2));
        break;
    case RdRtRs:
        FORMAT_INSTR(OPCODE_FMT "%s, %s, %s", opcode, registerName(dest), registerName(op2), registerName(op1));
        break;
    case RdRsRt:
        FORMAT_INSTR(OPCODE_FMT "%s, %s, %s", opcode, registerName(dest), registerName(op1), registerName(op2));
        break;
    case RdRtSa:
        FORMAT_INSTR(OPCODE_FMT "%s, %s, %d", opcode, registerName(dest), registerName(op2), shift);
        break;
    default:
        FORMAT_INSTR("unknown special encoding opcode 0x%x", function);
        break;
    }
}

void Mips32Opcode::formatSpecial2EncodingOpcode(uint8_t op1, uint8_t op2, uint8_t dest, uint8_t function)
{
    if (function == 0x02) {
        FORMAT_INSTR(OPCODE_FMT "%s, %s, %s", "mul", registerName(dest), registerName(op1), registerName(op2));
        return;
    }

    FORMAT_INSTR("unknown special2 encoding opcode 0x%x", function);
}

void Mips32Opcode::formatJumpEncodingOpcode(uint32_t iOp, uint32_t index, uint32_t* opcodePtr)
{
    if ((iOp != 0x02) && (iOp != 0x03)) {
        FORMAT_INSTR("unknown jump encoding opcode 0x%x", iOp);
        return;
    }

    FORMAT_INSTR(OPCODE_FMT "0x%x", (iOp == 0x02) ? "j" : "jal",
        (reinterpret_cast<unsigned>(opcodePtr+1) & 0xf0000000) | (index << 2));
}

void Mips32Opcode::formatREGIMMEncodingOpcode(uint8_t rs, uint8_t rt, int16_t imm, uint32_t* opcodePtr)
{
    const char *opcodes[] = { "bltz", "bgez", "bltzl", "bgezl" };
    if (rt < sizeof(opcodes))
        FORMAT_INSTR(OPCODE_FMT "%s, 0x%x", opcodes[rt], registerName(rs), reinterpret_cast<unsigned>(opcodePtr+1) + (imm << 2));
    else
        FORMAT_INSTR("unknown REGIMM encoding opcode 0x%x", rt);
}

void Mips32Opcode::formatImmediateEncodingOpcode(uint32_t iOp, uint8_t rs, uint8_t rt, int16_t imm, uint32_t* opcodePtr)
{
    const char *opcode;
    OpcodePrintFormat format = Unknown;
    switch (iOp) {
        case 0x04:
            if (!rs && !rt) {
                format = Addr;
                opcode = "b";
            } else {
                format = RsRtAddr;
                opcode = "beq";
            }
            break;
        case 0x05:
            format = RsRtAddr;
            opcode = "bne";
            break;
        case 0x06:
            format = RsRtAddr;
            opcode = "blez";
            break;
        case 0x07:
            format = RsRtAddr;
            opcode = "bgtz";
            break;
        case 0x08:
            format = RtRsImm;
            opcode = "addi";
            break;
        case 0x09:
            if (rs) {
                format = RtRsImm;
                opcode = "addiu";
            } else {
                format = RtUImm;
                opcode = "li";
            }
            break;
        case 0x0a:
            format = RtRsImm;
            opcode = "slti";
            break;
        case 0x0b:
            format = RtRsImm;
            opcode = "sltiu";
            break;
        case 0x0c:
            format = RtRsImm;
            opcode = "andi";
            break;
        case 0x0d:
            format = RtRsImm;
            opcode = "ori";
            break;
        case 0x0e:
            format = RtRsImm;
            opcode = "xori";
            break;
        case 0x0f:
            format = RtUImm;
            opcode = "lui";
            break;
        case 0x20:
            format = RtOffsetBase;
            opcode = "lb";
            break;
        case 0x21:
            format = RtOffsetBase;
            opcode = "lh";
            break;
        case 0x22:
            format = RtOffsetBase;
            opcode = "lwl";
            break;
        case 0x23:
            format = RtOffsetBase;
            opcode = "lw";
            break;
        case 0x24:
            format = RtOffsetBase;
            opcode = "lbu";
            break;
        case 0x25:
            format = RtOffsetBase;
            opcode = "lhu";
            break;
        case 0x26:
            format = RtOffsetBase;
            opcode = "lwr";
            break;
        case 0x28:
            format = RtOffsetBase;
            opcode = "sb";
            break;
        case 0x29:
            format = RtOffsetBase;
            opcode = "sh";
            break;
        case 0x2a:
            format = RtOffsetBase;
            opcode = "swl";
            break;
        case 0x2b:
            format = RtOffsetBase;
            opcode = "sw";
            break;
        case 0x2e:
            format = RtOffsetBase;
            opcode = "swr";
            break;
        case 0x35:
            format = FtOffsetBase;
            opcode = "ldc1";
            break;
        case 0x3d:
            format = FtOffsetBase;
            opcode = "sdc1";
            break;
    }

    switch (format) {
    case Addr:
        FORMAT_INSTR(OPCODE_FMT "0x%x", opcode, reinterpret_cast<unsigned>(opcodePtr+1) + (imm << 2));
        break;
    case RtUImm:
        FORMAT_INSTR(OPCODE_FMT "%s, 0x%hx", opcode, registerName(rt), imm);
        break;
    case RtRsImm:
        FORMAT_INSTR(OPCODE_FMT "%s, %s, %d", opcode, registerName(rt), registerName(rs), imm);
        break;
    case RsRtAddr:
        FORMAT_INSTR(OPCODE_FMT "%s, %s, 0x%x", opcode, registerName(rs), registerName(rt),
            reinterpret_cast<unsigned>(opcodePtr+1) + (imm << 2));
        break;
    case RtOffsetBase:
        FORMAT_INSTR(OPCODE_FMT "%s, %d(%s)", opcode, registerName(rt), imm, registerName(rs));
        break;
    case FtOffsetBase:
        FORMAT_INSTR(OPCODE_FMT "%s, %d(%s)", opcode, fpRegisterName(rt), imm, registerName(rs));
        break;
    default:
        FORMAT_INSTR("unknown immediate encoding opcode 0x%x", iOp);
        break;
    }
}

void Mips32Opcode::formatCOP1Opcode(uint8_t fmt, uint8_t ft, uint8_t fs, uint8_t fd, uint8_t func)
{
    const char *opcode;
    const char *suffix;
    OpcodePrintFormat format = Unknown;

    if (fmt < 0x10) {
        switch (fmt) {
        case 0x00:
            opcode = "mfc1";
            break;
        case 0x04:
            opcode = "mtc1";
            break;
        default:
            FORMAT_INSTR("unknown COP1 rs 0x%x", fmt);
            return;
        }
        FORMAT_INSTR(OPCODE_FMT "%s, %s", opcode, registerName(ft), fpRegisterName(fs));
        return;
    }

    switch (fmt) {
    case 0x10:
        suffix = "s";
        break;
    case 0x11:
        suffix = "d";
        break;
    case 0x14:
        suffix = "w";
        break;
    case 0x15:
        suffix = "l";
        break;
    case 0x16:
        suffix = "ps";
        break;
    default:
        FORMAT_INSTR("unknown COP1 fmt 0x%x", fmt);
        return;
    }

    switch (func) {
    case 0x00:
        format = FdFsFt;
        opcode = "add";
        break;
    case 0x01:
        format = FdFsFt;
        opcode = "sub";
        break;
    case 0x02:
        format = FdFsFt;
        opcode = "mul";
        break;
    case 0x03:
        format = FdFsFt;
        opcode = "div";
        break;
    case 0x04:
        format = FdFs;
        opcode = "sqrt";
        break;
    case 0x05:
        format = FdFs;
        opcode = "abs";
        break;
    case 0x06:
        format = FdFs;
        opcode = "mov";
        break;
    case 0x07:
        format = FdFs;
        opcode = "neg";
        break;
    case 0x08:
        format = FdFs;
        opcode = "round.l";
        break;
    case 0x09:
        format = FdFs;
        opcode = "trunc.l";
        break;
    case 0x0a:
        format = FdFs;
        opcode = "ceil.l";
        break;
    case 0x0b:
        format = FdFs;
        opcode = "floor.l";
        break;
    case 0x0c:
        format = FdFs;
        opcode = "round.w";
        break;
    case 0x0d:
        format = FdFs;
        opcode = "trunc.w";
        break;
    case 0x0e:
        format = FdFs;
        opcode = "ceil.w";
        break;
    case 0x0f:
        format = FdFs;
        opcode = "floor.w";
        break;
    case 0x20:
        format = FdFs;
        opcode = "cvt.s";
        break;
    case 0x21:
        format = FdFs;
        opcode = "cvt.d";
        break;
    case 0x24:
        format = FdFs;
        opcode = "cvt.w";
        break;
    case 0x25:
        format = FdFs;
        opcode = "cvt.l";
        break;
    }

    switch (format) {
    case FdFs:
        FORMAT_INSTR(COP1_OPCODE_FMT "%s, %s", opcode, suffix, fpRegisterName(fd), fpRegisterName(fs));
        break;
    case FdFsFt:
        FORMAT_INSTR(COP1_OPCODE_FMT "%s, %s, %s", opcode, suffix, fpRegisterName(fd), fpRegisterName(fs), fpRegisterName(ft));
        break;
    default:
        FORMAT_INSTR("unknown COP1 opcode 0x%x", func);
        break;
    }
}

void Mips32Opcode::formatCOP1FPCompareOpcode(uint8_t fmt, uint8_t ft, uint8_t fs, uint8_t cc, uint8_t cond)
{
    const char *suffix;
    static const char *opcodes[] = {
        "c.f", "c.un", "c.eq", "c.ueq", "c.olt", "c.ult", "c.ole", "c.ule",
        "c.sf", "c.ngle", "c.seq", "c.ngl", "c.lt", "c.nge", "c.le", "c.ngt"
    };
    ASSERT(cond < sizeof(opcdoes));

    switch (fmt) {
    case 0x10:
        suffix = "s";
        break;
    case 0x11:
        suffix = "d";
        break;
    case 0x16:
        suffix = "ps";
        break;
    default:
        FORMAT_INSTR("unknown COP1 fmt 0x%x", fmt);
        return;
    }

    if (!cc)
        FORMAT_INSTR(COP1_OPCODE_FMT "%s, %s", opcodes[cond], suffix, fpRegisterName(fs), fpRegisterName(ft));
    else
        FORMAT_INSTR(COP1_OPCODE_FMT "%d, %s, %s", opcodes[cond], suffix, cc, fpRegisterName(fs), fpRegisterName(ft));
}

void Mips32Opcode::formatCOP1BCOpcode(uint8_t cc, uint8_t ndtf, int16_t offset, uint32_t* opcodePtr)
{
    static const char *opcodes[] = { "bc1f", "bc1t", "bc1fl", "bc1tl" };
    ASSERT(ndtf < sizeof(opcodes));

    if (!cc)
        FORMAT_INSTR(OPCODE_FMT "0x%x", opcodes[ndtf], reinterpret_cast<unsigned>(opcodePtr+1) + (offset << 2));
    else
        FORMAT_INSTR(OPCODE_FMT "%d, 0x%x", opcodes[ndtf], cc, reinterpret_cast<unsigned>(opcodePtr+1) + (offset << 2));
}

const char* Mips32Opcode::disassemble(uint32_t* opcodePtr)
{
    uint32_t opcode = *opcodePtr;
    uint32_t iOp = (opcode >> 26) & 0x3f;

    if (!opcode)
        FORMAT_INSTR(OPCODE_FMT, "nop");
    else if (!iOp) {
        uint8_t op1 = (opcode >> 21) & 0x1f;
        uint8_t op2 = (opcode >> 16) & 0x1f;
        uint8_t dst = (opcode >> 11) & 0x1f;
        uint8_t shft = (opcode >> 6) & 0x1f;
        uint8_t func = opcode & 0x3f;
        formatSpecialEncodingOpcode(op1, op2, dst, shft, func);
    } else if ((iOp == 0x02) || (iOp == 0x03)) {
        uint32_t index = opcode & 0x3ffffff;
        formatJumpEncodingOpcode(iOp, index, opcodePtr);
    } else if (iOp == 0x11) {
        uint8_t fmt = (opcode >> 21) & 0x1f;
        if (fmt == 0x08) {
            uint8_t cc = (opcode >> 18) & 0x07;
            uint8_t ndtf = (opcode >> 16) & 0x03;
            int16_t offset = opcode & 0xffff;
            formatCOP1BCOpcode(cc, ndtf, offset, opcodePtr);
        } else if ((opcode & 0xf0) == 0x30) {
            uint8_t ft = (opcode >> 16) & 0x1f;
            uint8_t fs = (opcode >> 11) & 0x1f;
            uint8_t cc = (opcode >> 8) & 0x07;
            uint8_t cond = opcode & 0x0f;
            formatCOP1FPCompareOpcode(fmt, ft, fs, cc, cond);
        } else {
            uint8_t ft = (opcode >> 16) & 0x1f;
            uint8_t fs = (opcode >> 11) & 0x1f;
            uint8_t fd = (opcode >> 6) & 0x1f;
            uint8_t func = opcode & 0x3f;
            formatCOP1Opcode(fmt, ft, fs, fd, func);
        }
    } else if (iOp == 0x1c) {
        uint8_t op1 = (opcode >> 21) & 0x1f;
        uint8_t op2 = (opcode >> 16) & 0x1f;
        uint8_t dst = (opcode >> 11) & 0x1f;
        uint8_t func = opcode & 0x3f;
        formatSpecial2EncodingOpcode(op1, op2, dst, func);
    } else {
        uint8_t rs = (opcode >> 21) & 0x1f;
        uint8_t rt = (opcode >> 16) & 0x1f;
        int16_t imm = opcode & 0xffff;
        if (iOp == 0x01)
            formatREGIMMEncodingOpcode(rs, rt, imm, opcodePtr);
        else
            formatImmediateEncodingOpcode(iOp, rs, rt, imm, opcodePtr);
    }

    return m_formatBuffer;
}

#endif // USE(MIPS32_DISASSEMBLER)
