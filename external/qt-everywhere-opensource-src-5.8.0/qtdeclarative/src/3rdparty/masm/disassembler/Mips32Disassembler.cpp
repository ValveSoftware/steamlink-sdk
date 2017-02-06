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
#include "Disassembler.h"

#if USE(MIPS32_DISASSEMBLER)

#include "mips32/Mips32Opcode.h"
#include "MacroAssemblerCodeRef.h"

namespace JSC {

bool tryToDisassemble(const MacroAssemblerCodePtr& codePtr, size_t size, const char* prefix, PrintStream& out)
{
    Mips32Opcode mipsOpcode;

    uint32_t* currentPC = reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(codePtr.executableAddress()) & ~3);
    uint32_t* endPC = currentPC + (size / sizeof(uint32_t));

    while (currentPC < endPC) {
        char pcString[12];
        snprintf(pcString, sizeof(pcString), "0x%x", reinterpret_cast<unsigned>(currentPC));
        out.printf("%s%10s: %s\n", prefix, pcString, mipsOpcode.disassemble(currentPC));
        currentPC++;
    }

    return true;
}

} // namespace JSC

#endif // USE(MIPS32_DISASSEMBLER)

