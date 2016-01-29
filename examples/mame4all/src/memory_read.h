
/* generic byte-sized read handler */
#define READBYTE(name,type,abits)														\
data_t name(offs_t address) 															\
{																						\
	MHELE hw;																			\
																						\
	/* first-level lookup */															\
	hw = cur_mrhard[(UINT32)address >> (ABITS2_##abits + ABITS_MIN_##abits)];			\
																						\
	/* for compatibility with setbankhandler, 8-bit systems must call handlers */		\
	/* for banked memory reads/writes */												\
	if (type == TYPE_8BIT && hw == HT_RAM)												\
		return cpu_bankbase[HT_RAM][address];											\
	else if (type != TYPE_8BIT && hw <= HT_BANKMAX) 									\
	{																					\
		if (type == TYPE_16BIT_BE)														\
			return cpu_bankbase[hw][BYTE_XOR_BE(address) - memoryreadoffset[hw]];		\
		else if (type == TYPE_16BIT_LE) 												\
			return cpu_bankbase[hw][BYTE_XOR_LE(address) - memoryreadoffset[hw]];		\
	}																					\
																						\
	/* second-level lookup */															\
	if (hw >= MH_HARDMAX)																\
	{																					\
		hw -= MH_HARDMAX;																\
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_##abits) & MHMASK(ABITS2_##abits))];	\
																						\
		/* for compatibility with setbankhandler, 8-bit systems must call handlers */	\
		/* for banked memory reads/writes */											\
		if (type == TYPE_8BIT && hw == HT_RAM)											\
			return cpu_bankbase[HT_RAM][address];										\
		else if (type != TYPE_8BIT && hw <= HT_BANKMAX) 								\
		{																				\
			if (type == TYPE_16BIT_BE)													\
				return cpu_bankbase[hw][BYTE_XOR_BE(address) - memoryreadoffset[hw]];	\
			else if (type == TYPE_16BIT_LE) 											\
				return cpu_bankbase[hw][BYTE_XOR_LE(address) - memoryreadoffset[hw]];	\
		}																				\
	}																					\
																						\
	/* fall back to handler */															\
	if (type == TYPE_8BIT)																\
		return (*memoryreadhandler[hw])(address - memoryreadoffset[hw]);				\
	else																				\
	{																					\
		int shift = (address & 1) << 3; 												\
		int data = (*memoryreadhandler[hw])((address & ~1) - memoryreadoffset[hw]); 	\
		if (type == TYPE_16BIT_BE)														\
			return (data >> (shift ^ 8)) & 0xff;										\
		else if (type == TYPE_16BIT_LE) 												\
			return (data >> shift) & 0xff;												\
	}																					\
}

/* generic word-sized read handler (16-bit aligned only!) */
#define READWORD(name,type,abits,align) 												\
data_t name##_word(offs_t address)														\
{																						\
	MHELE hw;																			\
																						\
	/* only supports 16-bit memory systems */											\
	if (type == TYPE_8BIT)																\
		printf("Unsupported type for READWORD macro!\n");                               \
																						\
	/* handle aligned case first */ 													\
	if (align == ALWAYS_ALIGNED || !(address & 1))										\
	{																					\
		/* first-level lookup */														\
		hw = cur_mrhard[(UINT32)address >> (ABITS2_##abits + ABITS_MIN_##abits)];		\
		if (hw <= HT_BANKMAX)															\
			return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);		\
																						\
		/* second-level lookup */														\
		if (hw >= MH_HARDMAX)															\
		{																				\
			hw -= MH_HARDMAX;															\
			hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_##abits) & MHMASK(ABITS2_##abits))];	\
			if (hw <= HT_BANKMAX)														\
				return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);	\
		}																				\
																						\
		/* fall back to handler */														\
		return (*memoryreadhandler[hw])(address - memoryreadoffset[hw]);				\
	}																					\
																						\
	/* unaligned case */																\
	else if (type == TYPE_16BIT_BE) 													\
	{																					\
		int data = name(address) << 8;													\
		return data | (name(address + 1) & 0xff);										\
	}																					\
	else if (type == TYPE_16BIT_LE) 													\
	{																					\
		int data = name(address) & 0xff;												\
		return data | (name(address + 1) << 8); 										\
	}																					\
}

/* generic dword-sized read handler (16-bit aligned only!) */
#define READLONG(name,type,abits,align) 												\
data_t name##_dword(offs_t address) 													\
{																						\
	UINT16 word1, word2;																\
	MHELE hw1, hw2; 																	\
																						\
	/* only supports 16-bit memory systems */											\
	if (type == TYPE_8BIT)																\
		printf("Unsupported type for READWORD macro!\n");                               \
																						\
	/* handle aligned case first */ 													\
	if (align == ALWAYS_ALIGNED || !(address & 1))										\
	{																					\
		int address2 = (address + 2) & ADDRESS_MASK(abits); 							\
																						\
		/* first-level lookup */														\
		hw1 = cur_mrhard[(UINT32)address >> (ABITS2_##abits + ABITS_MIN_##abits)];		\
		hw2 = cur_mrhard[(UINT32)address2 >> (ABITS2_##abits + ABITS_MIN_##abits)]; 	\
																						\
		/* second-level lookup */														\
		if (hw1 >= MH_HARDMAX)															\
		{																				\
			hw1 -= MH_HARDMAX;															\
			hw1 = readhardware[(hw1 << MH_SBITS) + (((UINT32)address >> ABITS_MIN_##abits) & MHMASK(ABITS2_##abits))];	\
		}																				\
		if (hw2 >= MH_HARDMAX)															\
		{																				\
			hw2 -= MH_HARDMAX;															\
			hw2 = readhardware[(hw2 << MH_SBITS) + (((UINT32)address2 >> ABITS_MIN_##abits) & MHMASK(ABITS2_##abits))]; \
		}																				\
																						\
		/* process each word */ 														\
		if (hw1 <= HT_BANKMAX)															\
			word1 = READ_WORD(&cpu_bankbase[hw1][address - memoryreadoffset[hw1]]); 	\
		else																			\
			word1 = (*memoryreadhandler[hw1])(address - memoryreadoffset[hw1]); 		\
		if (hw2 <= HT_BANKMAX)															\
			word2 = READ_WORD(&cpu_bankbase[hw2][address2 - memoryreadoffset[hw2]]);	\
		else																			\
			word2 = (*memoryreadhandler[hw2])(address2 - memoryreadoffset[hw2]);		\
																						\
		/* fall back to handler */														\
		if (type == TYPE_16BIT_BE)														\
			return (word1 << 16) | (word2 & 0xffff);									\
		else if (type == TYPE_16BIT_LE) 												\
			return (word1 & 0xffff) | (word2 << 16);									\
	}																					\
																						\
	/* unaligned case */																\
	else if (type == TYPE_16BIT_BE) 													\
	{																					\
		int data = name(address) << 24; 												\
		data |= name##_word(address + 1) << 8;											\
		return data | (name(address + 3) & 0xff);										\
	}																					\
	else if (type == TYPE_16BIT_LE) 													\
	{																					\
		int data = name(address) & 0xff;												\
		data |= name##_word(address + 1) << 8;											\
		return data | (name(address + 3) << 24);										\
	}																					\
}

/* the handlers we need to generate */

//READBYTE(cpu_readmem16,    TYPE_8BIT,	  16)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem16 (offs_t address)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mrhard[(UINT32)address >> (ABITS2_16 + ABITS_MIN_16)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw == HT_RAM)
		return cpu_bankbase[HT_RAM][address];

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_16) & MHMASK(ABITS2_16))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw == HT_RAM)
			return cpu_bankbase[HT_RAM][address];
	}

	/* fall back to handler */
    return (*memoryreadhandler[hw])(address - memoryreadoffset[hw]);
}

//READBYTE(cpu_readmem20,    TYPE_8BIT,	  20)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem20 (offs_t address)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mrhard[(UINT32)address >> (ABITS2_20 + ABITS_MIN_20)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw == HT_RAM)
		return cpu_bankbase[HT_RAM][address];

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_20) & MHMASK(ABITS2_20))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw == HT_RAM)
			return cpu_bankbase[HT_RAM][address];
	}

	/* fall back to handler */
    return (*memoryreadhandler[hw])(address - memoryreadoffset[hw]);
}

//READBYTE(cpu_readmem21,    TYPE_8BIT,	  21)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem21 (offs_t address)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mrhard[(UINT32)address >> (ABITS2_21 + ABITS_MIN_21)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw == HT_RAM)
		return cpu_bankbase[HT_RAM][address];

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_21) & MHMASK(ABITS2_21))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw == HT_RAM)
			return cpu_bankbase[HT_RAM][address];
	}

	/* fall back to handler */
    return (*memoryreadhandler[hw])(address - memoryreadoffset[hw]);
}

//READBYTE(cpu_readmem16bew, TYPE_16BIT_BE, 16BEW)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem16bew(offs_t address) 
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mrhard[(UINT32)address >> (ABITS2_16BEW + ABITS_MIN_16BEW)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw <= HT_BANKMAX) 
	{
		return cpu_bankbase[hw][BYTE_XOR_BE(address) - memoryreadoffset[hw]];
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_16BEW) & MHMASK(ABITS2_16BEW))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw <= HT_BANKMAX) 
		{
			return cpu_bankbase[hw][BYTE_XOR_BE(address) - memoryreadoffset[hw]];
		}
	}

	/* fall back to handler */
	return ((*memoryreadhandler[hw])((address & ~1) - memoryreadoffset[hw]) >> (((address & 1) << 3) ^ 8)) & 0xff;
}


//READWORD(cpu_readmem16bew, TYPE_16BIT_BE, 16BEW, ALWAYS_ALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem16bew_word(offs_t address)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mrhard[(UINT32)address >> (ABITS2_16BEW + ABITS_MIN_16BEW)];
	if (hw <= HT_BANKMAX)
		return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);

    /* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_16BEW) & MHMASK(ABITS2_16BEW))];
		if (hw <= HT_BANKMAX)
			return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);
	}

	/* fall back to handler */
	return (*memoryreadhandler[hw])(address - memoryreadoffset[hw]);
}

//READBYTE(cpu_readmem16lew, TYPE_16BIT_LE, 16LEW)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem16lew (offs_t address) 
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mrhard[(UINT32)address >> (ABITS2_16LEW + ABITS_MIN_16LEW)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw <= HT_BANKMAX) 
	{
		return cpu_bankbase[hw][BYTE_XOR_LE(address) - memoryreadoffset[hw]];
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_16LEW) & MHMASK(ABITS2_16LEW))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw <= HT_BANKMAX) 
		{
			return cpu_bankbase[hw][BYTE_XOR_LE(address) - memoryreadoffset[hw]];
		}
	}

	/* fall back to handler */
	return ((*memoryreadhandler[hw])((address & ~1) - memoryreadoffset[hw]) >> ((address & 1) << 3)) & 0xff;
}

//READWORD(cpu_readmem16lew, TYPE_16BIT_LE, 16LEW, ALWAYS_ALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem16lew_word(offs_t address)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mrhard[(UINT32)address >> (ABITS2_16LEW + ABITS_MIN_16LEW)];
	if (hw <= HT_BANKMAX)
		return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_16LEW) & MHMASK(ABITS2_16LEW))];
		if (hw <= HT_BANKMAX)
			return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);
	}

	/* fall back to handler */
	return (*memoryreadhandler[hw])(address - memoryreadoffset[hw]);
}

//READBYTE(cpu_readmem24,    TYPE_8BIT,	  24)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem24 (offs_t address)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mrhard[(UINT32)address >> (ABITS2_24 + ABITS_MIN_24)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw == HT_RAM)
		return cpu_bankbase[HT_RAM][address];

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_24) & MHMASK(ABITS2_24))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw == HT_RAM)
			return cpu_bankbase[HT_RAM][address];
	}

	/* fall back to handler */
    return (*memoryreadhandler[hw])(address - memoryreadoffset[hw]);
}

//READBYTE(cpu_readmem24bew, TYPE_16BIT_BE, 24BEW)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem24bew (offs_t address) 
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mrhard[(UINT32)address >> (ABITS2_24BEW + ABITS_MIN_24BEW)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw <= HT_BANKMAX) 
	{
		return cpu_bankbase[hw][BYTE_XOR_BE(address) - memoryreadoffset[hw]];
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_24BEW) & MHMASK(ABITS2_24BEW))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw <= HT_BANKMAX) 
		{
			return cpu_bankbase[hw][BYTE_XOR_BE(address) - memoryreadoffset[hw]];
		}
	}

	/* fall back to handler */
	return ((*memoryreadhandler[hw])((address & ~1) - memoryreadoffset[hw]) >> (((address & 1) << 3) ^ 8)) & 0xff;
}


//READWORD(cpu_readmem24bew, TYPE_16BIT_BE, 24BEW, CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem24bew_word(offs_t address)
{
	MHELE hw;

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		/* first-level lookup */
		hw = cur_mrhard[(UINT32)address >> (ABITS2_24BEW + ABITS_MIN_24BEW)];
		if (hw <= HT_BANKMAX)
			return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);

		/* second-level lookup */
		if (hw >= MH_HARDMAX)
		{
			hw -= MH_HARDMAX;
			hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_24BEW) & MHMASK(ABITS2_24BEW))];
			if (hw <= HT_BANKMAX)
				return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);
		}

		/* fall back to handler */
		return (*memoryreadhandler[hw])(address - memoryreadoffset[hw]);
	}
	/* unaligned case */
	return (cpu_readmem24bew(address) << 8) | (cpu_readmem24bew(address + 1) & 0xff);
}

//READLONG(cpu_readmem24bew, TYPE_16BIT_BE, 24BEW, CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem24bew_dword(offs_t address) 
{
	UINT16 word1, word2;
	MHELE hw1, hw2; 

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		int address2 = (address + 2) & ADDRESS_MASK(24BEW); 

		/* first-level lookup */
		hw1 = cur_mrhard[(UINT32)address >> (ABITS2_24BEW + ABITS_MIN_24BEW)];
		hw2 = cur_mrhard[(UINT32)address2 >> (ABITS2_24BEW + ABITS_MIN_24BEW)]; 

		/* second-level lookup */
		if (hw1 >= MH_HARDMAX)
		{
			hw1 -= MH_HARDMAX;
			hw1 = readhardware[(hw1 << MH_SBITS) + (((UINT32)address >> ABITS_MIN_24BEW) & MHMASK(ABITS2_24BEW))];
		}
		if (hw2 >= MH_HARDMAX)
		{
			hw2 -= MH_HARDMAX;
			hw2 = readhardware[(hw2 << MH_SBITS) + (((UINT32)address2 >> ABITS_MIN_24BEW) & MHMASK(ABITS2_24BEW))]; 
		}

		/* process each word */ 
		if (hw1 <= HT_BANKMAX)
			word1 = READ_WORD(&cpu_bankbase[hw1][address - memoryreadoffset[hw1]]); 
		else
			word1 = (*memoryreadhandler[hw1])(address - memoryreadoffset[hw1]); 
		if (hw2 <= HT_BANKMAX)
			word2 = READ_WORD(&cpu_bankbase[hw2][address2 - memoryreadoffset[hw2]]);
		else
			word2 = (*memoryreadhandler[hw2])(address2 - memoryreadoffset[hw2]);

		/* fall back to handler */
		return (word1 << 16) | (word2 & 0xffff);
	}

	/* unaligned case */
	return (cpu_readmem24bew(address) << 24) | (cpu_readmem24bew_word(address + 1) << 8) | (cpu_readmem24bew(address + 3) & 0xff);
}

//READBYTE(cpu_readmem26lew, TYPE_16BIT_LE, 26LEW)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem26lew (offs_t address) 
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mrhard[(UINT32)address >> (ABITS2_26LEW + ABITS_MIN_26LEW)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw <= HT_BANKMAX) 
	{
		return cpu_bankbase[hw][BYTE_XOR_LE(address) - memoryreadoffset[hw]];
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_26LEW) & MHMASK(ABITS2_26LEW))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw <= HT_BANKMAX) 
		{
			return cpu_bankbase[hw][BYTE_XOR_LE(address) - memoryreadoffset[hw]];
		}
	}

	/* fall back to handler */
	return ((*memoryreadhandler[hw])((address & ~1) - memoryreadoffset[hw]) >> ((address & 1) << 3)) & 0xff;
}

//READWORD(cpu_readmem26lew, TYPE_16BIT_LE, 26LEW, ALWAYS_ALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem26lew_word(offs_t address)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mrhard[(UINT32)address >> (ABITS2_26LEW + ABITS_MIN_26LEW)];
	if (hw <= HT_BANKMAX)
		return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_26LEW) & MHMASK(ABITS2_26LEW))];
		if (hw <= HT_BANKMAX)
			return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);
	}

	/* fall back to handler */
	return (*memoryreadhandler[hw])(address - memoryreadoffset[hw]);
}

//READLONG(cpu_readmem26lew, TYPE_16BIT_LE, 26LEW, ALWAYS_ALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem26lew_dword(offs_t address) 
{
	UINT16 word1, word2;
	MHELE hw1, hw2; 

	int address2 = (address + 2) & ADDRESS_MASK(26LEW); 

	/* first-level lookup */
	hw1 = cur_mrhard[(UINT32)address >> (ABITS2_26LEW + ABITS_MIN_26LEW)];
	hw2 = cur_mrhard[(UINT32)address2 >> (ABITS2_26LEW + ABITS_MIN_26LEW)]; 

	/* second-level lookup */
	if (hw1 >= MH_HARDMAX)
	{
		hw1 -= MH_HARDMAX;
		hw1 = readhardware[(hw1 << MH_SBITS) + (((UINT32)address >> ABITS_MIN_26LEW) & MHMASK(ABITS2_26LEW))];
	}
	if (hw2 >= MH_HARDMAX)
	{
		hw2 -= MH_HARDMAX;
		hw2 = readhardware[(hw2 << MH_SBITS) + (((UINT32)address2 >> ABITS_MIN_26LEW) & MHMASK(ABITS2_26LEW))]; 
	}

	/* process each word */ 
	if (hw1 <= HT_BANKMAX)
		word1 = READ_WORD(&cpu_bankbase[hw1][address - memoryreadoffset[hw1]]); 
	else
		word1 = (*memoryreadhandler[hw1])(address - memoryreadoffset[hw1]); 
	if (hw2 <= HT_BANKMAX)
		word2 = READ_WORD(&cpu_bankbase[hw2][address2 - memoryreadoffset[hw2]]);
	else
		word2 = (*memoryreadhandler[hw2])(address2 - memoryreadoffset[hw2]);

	/* fall back to handler */
	return (word1 & 0xffff) | (word2 << 16);
}

//READBYTE(cpu_readmem29,    TYPE_16BIT_LE, 29)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem29 (offs_t address) 
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mrhard[(UINT32)address >> (ABITS2_29 + ABITS_MIN_29)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw <= HT_BANKMAX) 
	{
		return cpu_bankbase[hw][BYTE_XOR_LE(address) - memoryreadoffset[hw]];
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_29) & MHMASK(ABITS2_29))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw <= HT_BANKMAX) 
		{
			return cpu_bankbase[hw][BYTE_XOR_LE(address) - memoryreadoffset[hw]];
		}
	}

	/* fall back to handler */
	return ((*memoryreadhandler[hw])((address & ~1) - memoryreadoffset[hw]) >> ((address & 1) << 3)) & 0xff;
}

//READWORD(cpu_readmem29,    TYPE_16BIT_LE, 29,	 CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem29_word(offs_t address)
{
	MHELE hw;

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		/* first-level lookup */
		hw = cur_mrhard[(UINT32)address >> (ABITS2_29 + ABITS_MIN_29)];
		if (hw <= HT_BANKMAX)
			return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);

		/* second-level lookup */
		if (hw >= MH_HARDMAX)
		{
			hw -= MH_HARDMAX;
			hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_29) & MHMASK(ABITS2_29))];
			if (hw <= HT_BANKMAX)
				return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);
		}

		/* fall back to handler */
		return (*memoryreadhandler[hw])(address - memoryreadoffset[hw]);
	}

	/* unaligned case */
	return (cpu_readmem29(address) & 0xff) | (cpu_readmem29(address + 1) << 8); 
}

//READLONG(cpu_readmem29,    TYPE_16BIT_LE, 29,	 CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem29_dword(offs_t address) 
{
	UINT16 word1, word2;
	MHELE hw1, hw2; 

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		int address2 = (address + 2) & ADDRESS_MASK(29); 

		/* first-level lookup */
		hw1 = cur_mrhard[(UINT32)address >> (ABITS2_29 + ABITS_MIN_29)];
		hw2 = cur_mrhard[(UINT32)address2 >> (ABITS2_29 + ABITS_MIN_29)]; 

		/* second-level lookup */
		if (hw1 >= MH_HARDMAX)
		{
			hw1 -= MH_HARDMAX;
			hw1 = readhardware[(hw1 << MH_SBITS) + (((UINT32)address >> ABITS_MIN_29) & MHMASK(ABITS2_29))];
		}
		if (hw2 >= MH_HARDMAX)
		{
			hw2 -= MH_HARDMAX;
			hw2 = readhardware[(hw2 << MH_SBITS) + (((UINT32)address2 >> ABITS_MIN_29) & MHMASK(ABITS2_29))]; 
		}

		/* process each word */ 
		if (hw1 <= HT_BANKMAX)
			word1 = READ_WORD(&cpu_bankbase[hw1][address - memoryreadoffset[hw1]]); 
		else
			word1 = (*memoryreadhandler[hw1])(address - memoryreadoffset[hw1]); 
		if (hw2 <= HT_BANKMAX)
			word2 = READ_WORD(&cpu_bankbase[hw2][address2 - memoryreadoffset[hw2]]);
		else
			word2 = (*memoryreadhandler[hw2])(address2 - memoryreadoffset[hw2]);

		/* fall back to handler */
		return (word1 & 0xffff) | (word2 << 16);
	}

	/* unaligned case */
	return (cpu_readmem29(address) & 0xff) | (cpu_readmem29_word(address + 1) << 8) | (cpu_readmem29(address + 3) << 24);
}

//READBYTE(cpu_readmem32,    TYPE_16BIT_BE, 32)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem32 (offs_t address) 
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mrhard[(UINT32)address >> (ABITS2_32 + ABITS_MIN_32)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw <= HT_BANKMAX) 
	{
		return cpu_bankbase[hw][BYTE_XOR_BE(address) - memoryreadoffset[hw]];
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_32) & MHMASK(ABITS2_32))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw <= HT_BANKMAX) 
		{
			return cpu_bankbase[hw][BYTE_XOR_BE(address) - memoryreadoffset[hw]];
		}
	}

	/* fall back to handler */
	return ((*memoryreadhandler[hw])((address & ~1) - memoryreadoffset[hw]) >> (((address & 1) << 3) ^ 8)) & 0xff;
}

//READWORD(cpu_readmem32,    TYPE_16BIT_BE, 32,	 CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem32_word(offs_t address)
{
	MHELE hw;

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		/* first-level lookup */
		hw = cur_mrhard[(UINT32)address >> (ABITS2_32 + ABITS_MIN_32)];
		if (hw <= HT_BANKMAX)
			return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);

		/* second-level lookup */
		if (hw >= MH_HARDMAX)
		{
			hw -= MH_HARDMAX;
			hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_32) & MHMASK(ABITS2_32))];
			if (hw <= HT_BANKMAX)
				return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);
		}

		/* fall back to handler */
		return (*memoryreadhandler[hw])(address - memoryreadoffset[hw]);
	}

	/* unaligned case */
	return (cpu_readmem32(address) << 8) | (cpu_readmem32(address + 1) & 0xff);
}


//READLONG(cpu_readmem32,    TYPE_16BIT_BE, 32,	 CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem32_dword(offs_t address) 
{
	UINT16 word1, word2;
	MHELE hw1, hw2; 

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		int address2 = (address + 2) & ADDRESS_MASK(32); 

		/* first-level lookup */
		hw1 = cur_mrhard[(UINT32)address >> (ABITS2_32 + ABITS_MIN_32)];
		hw2 = cur_mrhard[(UINT32)address2 >> (ABITS2_32 + ABITS_MIN_32)]; 

		/* second-level lookup */
		if (hw1 >= MH_HARDMAX)
		{
			hw1 -= MH_HARDMAX;
			hw1 = readhardware[(hw1 << MH_SBITS) + (((UINT32)address >> ABITS_MIN_32) & MHMASK(ABITS2_32))];
		}
		if (hw2 >= MH_HARDMAX)
		{
			hw2 -= MH_HARDMAX;
			hw2 = readhardware[(hw2 << MH_SBITS) + (((UINT32)address2 >> ABITS_MIN_32) & MHMASK(ABITS2_32))]; 
		}

		/* process each word */ 
		if (hw1 <= HT_BANKMAX)
			word1 = READ_WORD(&cpu_bankbase[hw1][address - memoryreadoffset[hw1]]); 
		else
			word1 = (*memoryreadhandler[hw1])(address - memoryreadoffset[hw1]); 
		if (hw2 <= HT_BANKMAX)
			word2 = READ_WORD(&cpu_bankbase[hw2][address2 - memoryreadoffset[hw2]]);
		else
			word2 = (*memoryreadhandler[hw2])(address2 - memoryreadoffset[hw2]);

		/* fall back to handler */
		return (word1 << 16) | (word2 & 0xffff);
	}

	/* unaligned case */
	return (cpu_readmem32(address) << 24) | (cpu_readmem32_word(address + 1) << 8) | (cpu_readmem32(address + 3) & 0xff);
}

//READBYTE(cpu_readmem32lew, TYPE_16BIT_LE, 32LEW)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem32lew (offs_t address) 
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mrhard[(UINT32)address >> (ABITS2_32LEW + ABITS_MIN_32LEW)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw <= HT_BANKMAX) 
	{
		return cpu_bankbase[hw][BYTE_XOR_LE(address) - memoryreadoffset[hw]];
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_32LEW) & MHMASK(ABITS2_32LEW))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw <= HT_BANKMAX) 
		{
			return cpu_bankbase[hw][BYTE_XOR_LE(address) - memoryreadoffset[hw]];
		}
	}

	/* fall back to handler */
	return ((*memoryreadhandler[hw])((address & ~1) - memoryreadoffset[hw]) >> ((address & 1) << 3)) & 0xff;
}

//READWORD(cpu_readmem32lew, TYPE_16BIT_LE, 32LEW, CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem32lew_word(offs_t address)
{
	MHELE hw;

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		/* first-level lookup */
		hw = cur_mrhard[(UINT32)address >> (ABITS2_32LEW + ABITS_MIN_32LEW)];
		if (hw <= HT_BANKMAX)
			return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);

		/* second-level lookup */
		if (hw >= MH_HARDMAX)
		{
			hw -= MH_HARDMAX;
			hw = readhardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_32LEW) & MHMASK(ABITS2_32LEW))];
			if (hw <= HT_BANKMAX)
				return READ_WORD(&cpu_bankbase[hw][address - memoryreadoffset[hw]]);
		}

		/* fall back to handler */
		return (*memoryreadhandler[hw])(address - memoryreadoffset[hw]);
	}

	/* unaligned case */
	return (cpu_readmem32lew(address) & 0xff) | (cpu_readmem32lew(address + 1) << 8); 
}

//READLONG(cpu_readmem32lew, TYPE_16BIT_LE, 32LEW, CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
data_t cpu_readmem32lew_dword(offs_t address) 
{
	UINT16 word1, word2;
	MHELE hw1, hw2; 

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		int address2 = (address + 2) & ADDRESS_MASK(32LEW); 

		/* first-level lookup */
		hw1 = cur_mrhard[(UINT32)address >> (ABITS2_32LEW + ABITS_MIN_32LEW)];
		hw2 = cur_mrhard[(UINT32)address2 >> (ABITS2_32LEW + ABITS_MIN_32LEW)]; 

		/* second-level lookup */
		if (hw1 >= MH_HARDMAX)
		{
			hw1 -= MH_HARDMAX;
			hw1 = readhardware[(hw1 << MH_SBITS) + (((UINT32)address >> ABITS_MIN_32LEW) & MHMASK(ABITS2_32LEW))];
		}
		if (hw2 >= MH_HARDMAX)
		{
			hw2 -= MH_HARDMAX;
			hw2 = readhardware[(hw2 << MH_SBITS) + (((UINT32)address2 >> ABITS_MIN_32LEW) & MHMASK(ABITS2_32LEW))]; 
		}

		/* process each word */ 
		if (hw1 <= HT_BANKMAX)
			word1 = READ_WORD(&cpu_bankbase[hw1][address - memoryreadoffset[hw1]]); 
		else
			word1 = (*memoryreadhandler[hw1])(address - memoryreadoffset[hw1]); 
		if (hw2 <= HT_BANKMAX)
			word2 = READ_WORD(&cpu_bankbase[hw2][address2 - memoryreadoffset[hw2]]);
		else
			word2 = (*memoryreadhandler[hw2])(address2 - memoryreadoffset[hw2]);

		/* fall back to handler */
		return (word1 & 0xffff) | (word2 << 16);
	}

	/* unaligned case */
	return (cpu_readmem32lew(address) & 0xff) | (cpu_readmem32lew_word(address + 1) << 8) | (cpu_readmem32lew(address + 3) << 24);
}
