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

#ifndef _MIPS32Opcode_h_
#define _MIPS32Opcode_h_

#if USE(MIPS32_DISASSEMBLER)

#include <stdint.h>
#include <wtf/Assertions.h>

class Mips32Opcode {
public:
    Mips32Opcode() {}

    const char* disassemble(uint32_t*);

private:
    enum OpcodePrintFormat {
        Unknown = 0,
        Rs,
        Rd,
        Addr,
        RdRs,
        RsRt,
        RtUImm,
        RdRtRs,
        RdRsRt,
        RdRtSa,
        RtRsImm,
        RsRtAddr,
        RtOffsetBase,
        FdFs,
        FdFsFt,
        FtOffsetBase
    };

    const char *registerName(uint8_t r);
    const char *fpRegisterName(uint8_t r);
    void formatSpecialEncodingOpcode(uint8_t op1, uint8_t op2, uint8_t dest, uint8_t shift, uint8_t function);
    void formatSpecial2EncodingOpcode(uint8_t op1, uint8_t op2, uint8_t dest, uint8_t function);
    void formatJumpEncodingOpcode(uint32_t iOp, uint32_t index, uint32_t* opcodePtr);
    void formatREGIMMEncodingOpcode(uint8_t rs, uint8_t rt, int16_t imm, uint32_t* opcodePtr);
    void formatImmediateEncodingOpcode(uint32_t iOp, uint8_t rs, uint8_t rt, int16_t imm, uint32_t* opcodePtr);
    void formatCOP1Opcode(uint8_t fmt, uint8_t ft, uint8_t fs, uint8_t fd, uint8_t func);
    void formatCOP1FPCompareOpcode(uint8_t fmt, uint8_t ft, uint8_t fs, uint8_t cc, uint8_t cond);
    void formatCOP1BCOpcode(uint8_t cc, uint8_t ndtf, int16_t offset, uint32_t* opcodePtr);

    static const int bufferSize = 81;

    char m_formatBuffer[bufferSize];
};

#endif

#endif // _MIPS32Opcode_h_
