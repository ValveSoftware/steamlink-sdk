C nettle, low-level cryptographics library
C 
C Copyright (C) 2001, 2002, 2005 Rafael R. Sevilla, Niels Möller
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

include_src(<x86/aes.m4>)

C Register usage:

C AES state
define(<SA>,<%eax>)
define(<SB>,<%ebx>)
define(<SC>,<%ecx>)
define(<SD>,<%edx>)

C Primary use of these registers. They're also used temporarily for other things.
define(<T>,<%ebp>)
define(<TMP>,<%edi>)
define(<KEY>,<%esi>)

define(<FRAME_CTX>,	<40(%esp)>)
define(<FRAME_TABLE>,	<44(%esp)>)
define(<FRAME_LENGTH>,	<48(%esp)>)
define(<FRAME_DST>,	<52(%esp)>)
define(<FRAME_SRC>,	<56(%esp)>)

define(<FRAME_KEY>,	<16(%esp)>)
define(<FRAME_COUNT>,	<12(%esp)>)
define(<TA>,		<8(%esp)>)
define(<TB>,		<4(%esp)>)
define(<TC>,		<(%esp)>)

C The aes state is kept in %eax, %ebx, %ecx and %edx
C
C %esi is used as temporary, to point to the input, and to the
C subkeys, etc.
C
C %ebp is used as the round counter, and as a temporary in the final round.
C
C %edi is a temporary, often used as an accumulator.

	.file "aes-decrypt-internal.asm"
	
	C _aes_decrypt(struct aes_context *ctx, 
	C	       const struct aes_table *T,
	C	       unsigned length, uint8_t *dst,
	C	       uint8_t *src)
	.text
	ALIGN(16)
PROLOGUE(_nettle_aes_decrypt)
	C save all registers that need to be saved
	pushl	%ebx		C  20(%esp)
	pushl	%ebp		C  16(%esp)
	pushl	%esi		C  12(%esp)
	pushl	%edi		C  8(%esp)

	subl	$20, %esp	C  loop counter and save area for the key pointer

	movl	FRAME_LENGTH, %ebp
	testl	%ebp,%ebp
	jz	.Lend

	shrl	$4, FRAME_LENGTH

.Lblock_loop:
	movl	FRAME_CTX,KEY	C  address of context struct ctx
	
	movl	FRAME_SRC,TMP	C  address of plaintext
	AES_LOAD(SA, SB, SC, SD, TMP, KEY)
	addl	$16, FRAME_SRC	C Increment src pointer
	movl	FRAME_TABLE, T

	C  get number of rounds to do from ctx struct	
	movl	AES_NROUNDS (KEY),TMP
	subl	$1,TMP

	C Loop counter on stack
	movl	TMP, FRAME_COUNT

	addl	$16,KEY		C  point to next key
	movl	KEY,FRAME_KEY
	ALIGN(16)
.Lround_loop:
	AES_ROUND(T, SA,SD,SC,SB, TMP, KEY)
	movl	TMP, TA

	AES_ROUND(T, SB,SA,SD,SC, TMP, KEY)
	movl	TMP, TB

	AES_ROUND(T, SC,SB,SA,SD, TMP, KEY)
	movl	TMP, TC

	AES_ROUND(T, SD,SC,SB,SA, SD, KEY)
	
	movl	TA, SA
	movl	TB, SB
	movl	TC, SC
	
	movl	FRAME_KEY, KEY

	xorl	(KEY),SA	C  add current session key to plaintext
	xorl	4(KEY),SB
	xorl	8(KEY),SC
	xorl	12(KEY),SD
	addl	$16,FRAME_KEY	C  point to next key
	decl	FRAME_COUNT
	jnz	.Lround_loop

	C last round

	AES_FINAL_ROUND(SA,SD,SC,SB,T, TMP, KEY)
	movl	TMP, TA

	AES_FINAL_ROUND(SB,SA,SD,SC,T, TMP, KEY)
	movl	TMP, TB

	AES_FINAL_ROUND(SC,SB,SA,SD,T, TMP, KEY)
	movl	TMP, TC

	AES_FINAL_ROUND(SD,SC,SB,SA,T, SD, KEY)

	movl	TA, SA
	movl	TB, SB
	movl	TC, SC

	C Inverse S-box substitution
	mov	$3,TMP
.Lsubst:
	AES_SUBST_BYTE(SA,SB,SC,SD,T, KEY)

	decl	TMP
	jnz	.Lsubst

	C Add last subkey, and store decrypted data
	movl	FRAME_DST,TMP
	movl	FRAME_KEY, KEY
	AES_STORE(SA,SB,SC,SD, KEY, TMP)
	
	addl	$16, FRAME_DST		C Increment destination pointer
	decl	FRAME_LENGTH

	jnz	.Lblock_loop

.Lend:
	addl	$20, %esp
	popl	%edi
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret
EPILOGUE(_nettle_aes_decrypt)
