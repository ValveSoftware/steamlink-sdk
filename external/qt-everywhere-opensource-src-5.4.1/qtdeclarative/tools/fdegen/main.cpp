/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the V4VM module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <libdwarf.h>
#include <dwarf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define DEBUG

#ifdef DEBUG
#include <libelf.h>
#endif

#include <qglobal.h>

enum DwarfRegs {
#if defined(Q_PROCESSOR_X86_64)
    // X86-64
    RAX = 0,
    RDX = 1,
    RCX = 2,
    RBX = 3,
    RSI = 4,
    RDI = 5,
    RBP = 6,
    RSP = 7,
    R8 = 8,
    R9 = 9,
    R10 = 10,
    R11 = 11,
    R12 = 12,
    R13 = 13,
    R14 = 14,
    R15 = 15,
    RIP = 16,

    InstructionPointerRegister = RIP,
    StackPointerRegister = RSP,
    StackFrameRegister = RBP
#elif defined(Q_PROCESSOR_X86)
    // x86
    EAX = 0,
    EDX = 1,
    ECX = 2,
    EBX = 3,
    ESP = 4,
    EBP = 5,
    ESI = 6,
    EDI = 7,
    EIP = 8,

    InstructionPointerRegister = EIP,
    StackPointerRegister = ESP,
    StackFrameRegister = EBP
#else
#error Not ported yet
#endif
};

static const DwarfRegs calleeSavedRegisters[] = {
#if defined(Q_PROCESSOR_X86_64)
    R12,
    R14
#elif defined(Q_PROCESSOR_X86)
    ESI,
    EDI
#endif
};
static const int calleeSavedRegisterCount = sizeof(calleeSavedRegisters) / sizeof(calleeSavedRegisters[0]);

#if QT_POINTER_SIZE == 8
#define Elf_Ehdr Elf64_Ehdr
#define elf_newehdr elf64_newehdr
#define Elf_Shdr Elf64_Shdr
#define elf_getshdr elf64_getshdr
#else
#define Elf_Ehdr Elf32_Ehdr
#define elf_newehdr elf32_newehdr
#define Elf_Shdr Elf32_Shdr
#define elf_getshdr elf32_getshdr
#endif

static void die(const char *msg)
{
    fprintf(stderr, "error: %s\n", msg);
    exit(1);
}

static int createSectionCallback(
        char *name,
        int size,
        Dwarf_Unsigned  /*type*/,
        Dwarf_Unsigned  /*flags*/,
        Dwarf_Unsigned  /*link*/,
        Dwarf_Unsigned  /*info*/,
        Dwarf_Unsigned* /*sect_name_index*/,
        void *          /*user_data*/,
        int*            /*error*/)
{
    if (strcmp(name, ".debug_frame"))
        return 0;
    fprintf(stderr, "createsection called with %s and size %d\n", name, size);
    return 1;
}

static unsigned char cie_init_instructions[] = {
    DW_CFA_def_cfa, StackPointerRegister, /*offset in bytes */QT_POINTER_SIZE,
    DW_CFA_offset | InstructionPointerRegister, 1,
};

int main()
{
    Dwarf_Error error = 0;
    Dwarf_P_Debug dw = dwarf_producer_init_c(DW_DLC_WRITE | DW_DLC_SIZE_64,
                                             createSectionCallback,
                                             /* error handler */0,
                                             /* error arg */0,
                                             /* user data */0,
                                             &error);
    if (error != 0)
        die("dwarf_producer_init_c failed");

    Dwarf_Unsigned cie = dwarf_add_frame_cie(dw,
                                             "",
                                             /* code alignment factor */QT_POINTER_SIZE,
                                             /* data alignment factor */-QT_POINTER_SIZE,
                                             /* return address reg*/InstructionPointerRegister,
                                             cie_init_instructions,
                                             sizeof(cie_init_instructions),
                                             &error);
    if (error != 0)
        die("dwarf_add_frame_cie failed");

    Dwarf_P_Fde fde = dwarf_new_fde(dw, &error);
    if (error != 0)
        die("dwarf_new_fde failed");

    /* New entry in state machine for code offset 1 after push rbp instruction */
    dwarf_add_fde_inst(fde,
                       DW_CFA_advance_loc,
                       /*offset in code alignment units*/ 1,
                       /* unused*/ 0,
                       &error);

    /* After "push rbp" the offset to the CFA is now 2 instead of 1 */
    dwarf_add_fde_inst(fde,
                       DW_CFA_def_cfa_offset_sf,
                       /*offset in code alignment units*/ -2,
                       /* unused*/ 0,
                       &error);

    /* After "push rbp" the value of rbp is now stored at offset 1 from CFA */
    dwarf_add_fde_inst(fde,
                       DW_CFA_offset,
                       StackFrameRegister,
                       2,
                       &error);

    /* New entry in state machine for code offset 3 for mov rbp, rsp instruction */
    dwarf_add_fde_inst(fde,
                       DW_CFA_advance_loc,
                       /*offset in code alignment units*/ 3,
                       /* unused */ 0,
                       &error);

    /* After "mov rbp, rsp" the cfa is reachable via rbp */
    dwarf_add_fde_inst(fde,
                       DW_CFA_def_cfa_register,
                       StackFrameRegister,
                       /* unused */0,
                       &error);

    /* Callee saved registers */
    for (int i = 0; i < calleeSavedRegisterCount; ++i) {
        dwarf_add_fde_inst(fde,
                           DW_CFA_offset,
                           calleeSavedRegisters[i],
                           i + 3,
                           &error);
    }

    dwarf_add_frame_fde(dw, fde,
                        /* die */0,
                        cie,
                        /*virt addr */0,
                        /* length of code */0,
                        /* symbol index */0,
                        &error);
    if (error != 0)
        die("dwarf_add_frame_fde failed");

    dwarf_transform_to_disk_form(dw, &error);
    if (error != 0)
        die("dwarf_transform_to_disk_form failed");

    Dwarf_Unsigned len = 0;
    Dwarf_Signed elfIdx = 0;
    unsigned char *bytes = (unsigned char *)dwarf_get_section_bytes(dw, /* section */1,
                                              &elfIdx, &len, &error);
    if (error != 0)
        die("dwarf_get_section_bytes failed");

    // libdwarf doesn't add a terminating FDE with zero length, so let's add one
    // ourselves.
    unsigned char *newBytes = (unsigned char *)alloca(len + 4);
    memcpy(newBytes, bytes, len);
    newBytes[len] = 0;
    newBytes[len + 1] = 0;
    newBytes[len + 2] = 0;
    newBytes[len + 3] = 0;
    newBytes[len + 4] = 0;
    bytes = newBytes;
    len += 4;

    // Reset CIE-ID back to 0 as expected for .eh_frames
    bytes[4] = 0;
    bytes[5] = 0;
    bytes[6] = 0;
    bytes[7] = 0;

    unsigned fde_offset = bytes[0] + 4;

    bytes[fde_offset + 4] = fde_offset + 4;

    printf("static const unsigned char cie_fde_data[] = {\n");
    int i = 0;
    while (i < len) {
        printf("    ");
        for (int j = 0; i < len && j < 8; ++j, ++i)
            printf("0x%x, ", bytes[i]);
        printf("\n");
    }
    printf("};\n");

    printf("static const int fde_offset = %d;\n", fde_offset);
    printf("static const int initial_location_offset = %d;\n", fde_offset + 8);
    printf("static const int address_range_offset = %d;\n", fde_offset + 8 + QT_POINTER_SIZE);

#ifdef DEBUG
    {
        if (elf_version(EV_CURRENT) == EV_NONE)
            die("wrong elf version");
        int fd = open("debug.o", O_WRONLY | O_CREAT, 0777);
        if (fd < 0)
            die("cannot create debug.o");

        Elf *e = elf_begin(fd, ELF_C_WRITE, 0);
        if (!e)
            die("elf_begin failed");

        Elf_Ehdr *ehdr = elf_newehdr(e);
        if (!ehdr)
            die(elf_errmsg(-1));

        ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
#if defined(Q_PROCESSOR_X86_64)
        ehdr->e_machine = EM_X86_64;
#elif defined(Q_PROCESSOR_X86)
        ehdr->e_machine = EM_386;
#else
#error port me :)
#endif
        ehdr->e_type = ET_EXEC;
        ehdr->e_version = EV_CURRENT;

        Elf_Scn *section = elf_newscn(e);
        if (!section)
            die("elf_newscn failed");

        Elf_Data *data = elf_newdata(section);
        if (!data)
            die(elf_errmsg(-1));
        data->d_align = 4;
        data->d_off = 0;
        data->d_buf = bytes;
        data->d_size = len;
        data->d_type = ELF_T_BYTE;
        data->d_version = EV_CURRENT;

        Elf_Shdr *shdr = elf_getshdr(section);
        if (!shdr)
            die(elf_errmsg(-1));

        shdr->sh_name = 1;
        shdr->sh_type = SHT_PROGBITS;
        shdr->sh_entsize = 0;

        char stringTable[] = {
            0,
            '.', 'e', 'h', '_', 'f', 'r', 'a', 'm', 'e', 0,
            '.', 's', 'h', 's', 't', 'r', 't', 'a', 'b', 0
        };

        section = elf_newscn(e);
        if (!section)
            die("elf_newscn failed");

        data = elf_newdata(section);
        if (!data)
            die(elf_errmsg(-1));
        data->d_align = 1;
        data->d_off = 0;
        data->d_buf = stringTable;
        data->d_size = sizeof(stringTable);
        data->d_type = ELF_T_BYTE;
        data->d_version = EV_CURRENT;

        shdr = elf_getshdr(section);
        if (!shdr)
            die(elf_errmsg(-1));

        shdr->sh_name = 11;
        shdr->sh_type = SHT_STRTAB;
        shdr->sh_flags = SHF_STRINGS | SHF_ALLOC;
        shdr->sh_entsize = 0;

        ehdr->e_shstrndx = elf_ndxscn(section);

        if (elf_update(e, ELF_C_WRITE) < 0)
            die(elf_errmsg(-1));

        elf_end(e);
        close(fd);
    }
#endif

    dwarf_producer_finish(dw, &error);
    if (error != 0)
        die("dwarf_producer_finish failed");
    return 0;
}
