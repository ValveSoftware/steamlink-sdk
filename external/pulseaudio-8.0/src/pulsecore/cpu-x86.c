/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering
  Copyright 2009 Wim Taymans <wim.taymans@collabora.co.uk>

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>

#include <pulsecore/log.h>

#include "cpu-x86.h"

#if defined (__i386__) || defined (__amd64__)
static void get_cpuid(uint32_t op, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ __volatile__ (
        "  push %%"PA_REG_b"   \n\t"
        "  cpuid               \n\t"
        "  mov %%ebx, %%esi    \n\t"
        "  pop %%"PA_REG_b"    \n\t"

        : "=a" (*a), "=S" (*b), "=c" (*c), "=d" (*d)
        : "0" (op)
    );
}
#endif

void pa_cpu_get_x86_flags(pa_cpu_x86_flag_t *flags) {
#if defined (__i386__) || defined (__amd64__)
    uint32_t eax, ebx, ecx, edx;
    uint32_t level;

    *flags = 0;

    /* get standard level */
    get_cpuid(0x00000000, &level, &ebx, &ecx, &edx);
    if (level >= 1) {
        get_cpuid(0x00000001, &eax, &ebx, &ecx, &edx);

        if (edx & (1<<15))
          *flags |= PA_CPU_X86_CMOV;

        if (edx & (1<<23))
          *flags |= PA_CPU_X86_MMX;

        if (edx & (1<<25))
          *flags |= PA_CPU_X86_SSE;

        if (edx & (1<<26))
          *flags |= PA_CPU_X86_SSE2;

        if (ecx & (1<<0))
          *flags |= PA_CPU_X86_SSE3;

        if (ecx & (1<<9))
          *flags |= PA_CPU_X86_SSSE3;

        if (ecx & (1<<19))
          *flags |= PA_CPU_X86_SSE4_1;

        if (ecx & (1<<20))
          *flags |= PA_CPU_X86_SSE4_2;
    }

    /* get extended level */
    get_cpuid(0x80000000, &level, &ebx, &ecx, &edx);
    if (level >= 0x80000001) {
        get_cpuid(0x80000001, &eax, &ebx, &ecx, &edx);

        if (edx & (1<<22))
          *flags |= PA_CPU_X86_MMXEXT;

        if (edx & (1<<23))
          *flags |= PA_CPU_X86_MMX;

        if (edx & (1<<30))
          *flags |= PA_CPU_X86_3DNOWEXT;

        if (edx & (1<<31))
          *flags |= PA_CPU_X86_3DNOW;
    }

    pa_log_info("CPU flags: %s%s%s%s%s%s%s%s%s%s%s",
    (*flags & PA_CPU_X86_CMOV) ? "CMOV " : "",
    (*flags & PA_CPU_X86_MMX) ? "MMX " : "",
    (*flags & PA_CPU_X86_SSE) ? "SSE " : "",
    (*flags & PA_CPU_X86_SSE2) ? "SSE2 " : "",
    (*flags & PA_CPU_X86_SSE3) ? "SSE3 " : "",
    (*flags & PA_CPU_X86_SSSE3) ? "SSSE3 " : "",
    (*flags & PA_CPU_X86_SSE4_1) ? "SSE4_1 " : "",
    (*flags & PA_CPU_X86_SSE4_2) ? "SSE4_2 " : "",
    (*flags & PA_CPU_X86_MMXEXT) ? "MMXEXT " : "",
    (*flags & PA_CPU_X86_3DNOW) ? "3DNOW " : "",
    (*flags & PA_CPU_X86_3DNOWEXT) ? "3DNOWEXT " : "");
#endif /* defined (__i386__) || defined (__amd64__) */
}

bool pa_cpu_init_x86(pa_cpu_x86_flag_t *flags) {
#if defined (__i386__) || defined (__amd64__)
    pa_cpu_get_x86_flags(flags);

    /* activate various optimisations */
    if (*flags & PA_CPU_X86_MMX) {
        pa_volume_func_init_mmx(*flags);
        pa_remap_func_init_mmx(*flags);
    }

    if (*flags & (PA_CPU_X86_SSE | PA_CPU_X86_SSE2)) {
        pa_volume_func_init_sse(*flags);
        pa_remap_func_init_sse(*flags);
        pa_convert_func_init_sse(*flags);
    }

    return true;
#else /* defined (__i386__) || defined (__amd64__) */
    return false;
#endif /* defined (__i386__) || defined (__amd64__) */
}
