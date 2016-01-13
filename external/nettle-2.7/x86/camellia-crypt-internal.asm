C nettle, low-level cryptographics library
C 
C Copyright (C) 2010, Niels Möller
C  
C The nettle library is free software; you can redistribute it and/or modify
C it under the terms of the GNU Lesser General Public License as published by
C the Free Software Foundation; either version 2.1 of the License, or (at your
C option) any later version.
C 
C The nettle library is distributed in the hope that it will be useful, but
C WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
C or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
C License for more details.
C 
C You should have received a copy of the GNU Lesser General Public License
C along with the nettle library; see the file COPYING.LIB.  If not, write to
C the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
C MA 02111-1301, USA.

C Register usage:

C Camellia state, 128-bit value in little endian order.
C L0, H0 corresponds to D1 in the spec and i0 in the C implementation.
C while L1, H1 corresponds to D2/i1.
define(<L0>,<%eax>)
define(<H0>,<%ebx>)
define(<L1>,<%ecx>)
define(<H1>,<%edx>)

define(<TMP>,<%ebp>)
define(<KEY>,<%esi>)
define(<T>,<%edi>)

C Locals on the stack

define(<FRAME_L0>,	<(%esp)>)
define(<FRAME_H0>,	<4(%esp)>)
define(<FRAME_L1>,	<8(%esp)>)
define(<FRAME_H1>,	<12(%esp)>)
define(<FRAME_CNT>,	<16(%esp)>)
	
C Arguments on stack.
define(<FRAME_CTX>,	<40(%esp)>)
define(<FRAME_TABLE>,	<44(%esp)>)
define(<FRAME_LENGTH>,	<48(%esp)>)
define(<FRAME_DST>,	<52(%esp)>)
define(<FRAME_SRC>,	<56(%esp)>)

define(<SP1110>, <(T,$1,4)>)
define(<SP0222>, <1024(T,$1,4)>)
define(<SP3033>, <2048(T,$1,4)>)
define(<SP4404>, <3072(T,$1,4)>)

C ROUND(xl, xh, yl, yh, key-offset)
C xl and xh are rotated 16 bits at the end
C yl and yh are read from stack, and left in registers
define(<ROUND>, <
	movzbl	LREG($1), TMP
	movl	SP1110(TMP), $4
	movzbl	HREG($1), TMP
	xorl	SP4404(TMP), $4
	roll	<$>16, $1

	movzbl	LREG($2), TMP
	movl	SP4404(TMP), $3
	movzbl	HREG($2), TMP
	xorl	SP3033(TMP), $3
	roll	<$>16, $2

	movzbl	LREG($1), TMP
	xorl	SP3033(TMP), $4
	movzbl	HREG($1), TMP
	xorl	SP0222(TMP), $4

	movzbl	LREG($2), TMP
	xorl	SP0222(TMP), $3
	movzbl	HREG($2), TMP
	xorl	SP1110(TMP), $3

	xorl	$5(KEY), $4
	xorl	$5 + 4(KEY), $3

	xorl	$3, $4
	rorl	<$>8, $3
	xorl	$4, $3

	xorl	FRAME_$3, $3
	xorl	FRAME_$4, $4
>)

C Six rounds, with inputs and outputs in registers.
define(<ROUND6>, <
	movl	L0, FRAME_L0
	movl	H0, FRAME_H0
	movl	L1, FRAME_L1
	movl	H1, FRAME_H1

	ROUND(L0,H0,<L1>,<H1>,0)
	movl	L1, FRAME_L1
	movl	H1, FRAME_H1
	ROUND(L1,H1,<L0>,<H0>,8)
	movl	L0, FRAME_L0
	movl	H0, FRAME_H0
	ROUND(L0,H0,<L1>,<H1>,16)
	movl	L1, FRAME_L1
	movl	H1, FRAME_H1
	ROUND(L1,H1,<L0>,<H0>,24)
	movl	L0, FRAME_L0
	movl	H0, FRAME_H0
	ROUND(L0,H0,<L1>,<H1>,32)
	ROUND(L1,H1,<L0>,<H0>,40)
	roll	<$>16, L1
	roll	<$>16, H1
>)

C FL(x0, x1, key-offset)
define(<FL>, <
	movl	$3 + 4(KEY), TMP
	andl	$2, TMP
	roll	<$>1, TMP
	xorl	TMP, $1
	movl	$3(KEY), TMP
	orl	$1, TMP
	xorl	TMP, $2
>)
C FLINV(x0, x1, key-offset)
define(<FLINV>, <
	movl	$3(KEY), TMP
	orl	$1, TMP
	xorl	TMP, $2
	movl	$3 + 4(KEY), TMP
	andl	$2, TMP
	roll	<$>1, TMP
	xorl	TMP, $1
>)

.file "camellia-encrypt-internal.asm"
	
	C _camellia_crypt(struct camellia_context *ctx, 
	C	          const struct camellia_table *T,
	C	          unsigned length, uint8_t *dst,
	C	          uint8_t *src)
	.text
	ALIGN(16)
PROLOGUE(_nettle_camellia_crypt)
	C save all registers that need to be saved
	pushl	%ebx		C  32(%esp)
	pushl	%ebp		C  28(%esp)
	pushl	%esi		C  24(%esp)
	pushl	%edi		C  20(%esp)

	subl	$20, %esp 

	movl	FRAME_LENGTH, %ebp
	testl	%ebp,%ebp
	jz	.Lend

.Lblock_loop:
	C Load data, note that we'll happily do unaligned loads
	movl	FRAME_SRC, TMP
	movl	(TMP), H0
	bswap	H0
	movl	4(TMP), L0
	bswap	L0
	movl	8(TMP), H1
	bswap	H1
	movl	12(TMP), L1
	bswap	L1
	addl	$16, FRAME_SRC
	movl	FRAME_CTX, KEY
	movl	(KEY), TMP
	subl	$8, TMP
	movl	TMP, FRAME_CNT
	C 	Whitening using first subkey 
	addl	$ALIGNOF_UINT64_T + 8, KEY
	xorl	-8(KEY), L0
	xorl	-4(KEY), H0

	movl	FRAME_TABLE, T

	ROUND6
.Lround_loop:
	addl	$64, KEY
	FL(L0, H0, -16)
	FLINV(L1, H1, -8)
	ROUND6
	subl 	$8, FRAME_CNT	
	ja	.Lround_loop

	movl	FRAME_DST, TMP
	bswap	H0
	movl	H0,8(TMP)
	bswap	L0
	movl	L0,12(TMP)
	xorl	52(KEY), H1
	bswap	H1
	movl	H1, 0(TMP)
	xorl	48(KEY), L1
	bswap	L1
	movl	L1, 4(TMP)
	addl	$16, FRAME_DST
	subl	$16, FRAME_LENGTH
	ja	.Lblock_loop

.Lend:
	addl	$20, %esp
	popl	%edi
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret
EPILOGUE(_nettle_camellia_crypt)
