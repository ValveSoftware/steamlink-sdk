/* generic byte-sized write handler */
#define WRITEBYTE(name,type,abits)														\
void name(offs_t address,data_t data)													\
{																						\
	MHELE hw;																			\
																						\
	/* first-level lookup */															\
	hw = cur_mwhard[(UINT32)address >> (ABITS2_##abits + ABITS_MIN_##abits)];			\
																						\
	/* for compatibility with setbankhandler, 8-bit systems must call handlers */		\
	/* for banked memory reads/writes */												\
	if (type == TYPE_8BIT && hw == HT_RAM)												\
	{																					\
		cpu_bankbase[HT_RAM][address] = data;											\
		return; 																		\
	}																					\
	else if (type != TYPE_8BIT && hw <= HT_BANKMAX) 									\
	{																					\
		if (type == TYPE_16BIT_BE)														\
			cpu_bankbase[hw][BYTE_XOR_BE(address) - memorywriteoffset[hw]] = data;		\
		else if (type == TYPE_16BIT_LE) 												\
			cpu_bankbase[hw][BYTE_XOR_LE(address) - memorywriteoffset[hw]] = data;		\
		return; 																		\
	}																					\
																						\
	/* second-level lookup */															\
	if (hw >= MH_HARDMAX)																\
	{																					\
		hw -= MH_HARDMAX;																\
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_##abits) & MHMASK(ABITS2_##abits))];	\
																						\
		/* for compatibility with setbankhandler, 8-bit systems must call handlers */	\
		/* for banked memory reads/writes */											\
		if (type == TYPE_8BIT && hw == HT_RAM)											\
		{																				\
			cpu_bankbase[HT_RAM][address] = data;										\
			return; 																	\
		}																				\
		else if (type != TYPE_8BIT && hw <= HT_BANKMAX) 								\
		{																				\
			if (type == TYPE_16BIT_BE)													\
				cpu_bankbase[hw][BYTE_XOR_BE(address) - memorywriteoffset[hw]] = data;	\
			else if (type == TYPE_16BIT_LE) 											\
				cpu_bankbase[hw][BYTE_XOR_LE(address) - memorywriteoffset[hw]] = data;	\
			return; 																	\
		}																				\
	}																					\
																						\
	/* fall back to handler */															\
	if (type != TYPE_8BIT)																\
	{																					\
		int shift = (address & 1) << 3; 												\
		if (type == TYPE_16BIT_BE)														\
			shift ^= 8; 																\
		data = (0xff000000 >> shift) | ((data & 0xff) << shift);						\
		address &= ~1;																	\
	}																					\
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data);					\
}

/* generic word-sized write handler (16-bit aligned only!) */
#define WRITEWORD(name,type,abits,align)												\
void name##_word(offs_t address,data_t data)											\
{																						\
	MHELE hw;																			\
																						\
	/* only supports 16-bit memory systems */											\
	if (type == TYPE_8BIT)																\
		printf("Unsupported type for WRITEWORD macro!\n");                              \
																						\
	/* handle aligned case first */ 													\
	if (align == ALWAYS_ALIGNED || !(address & 1))										\
	{																					\
		/* first-level lookup */														\
		hw = cur_mwhard[(UINT32)address >> (ABITS2_##abits + ABITS_MIN_##abits)];		\
		if (hw <= HT_BANKMAX)															\
		{																				\
			WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);		\
			return; 																	\
		}																				\
																						\
		/* second-level lookup */														\
		if (hw >= MH_HARDMAX)															\
		{																				\
			hw -= MH_HARDMAX;															\
			hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_##abits) & MHMASK(ABITS2_##abits))]; \
			if (hw <= HT_BANKMAX)														\
			{																			\
				WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);	\
				return; 																\
			}																			\
		}																				\
																						\
		/* fall back to handler */														\
		(*memorywritehandler[hw])(address - memorywriteoffset[hw], data & 0xffff);		\
	}																					\
																						\
	/* unaligned case */																\
	else if (type == TYPE_16BIT_BE) 													\
	{																					\
		name(address, data >> 8);														\
		name(address + 1, data & 0xff); 												\
	}																					\
	else if (type == TYPE_16BIT_LE) 													\
	{																					\
		name(address, data & 0xff); 													\
		name(address + 1, data >> 8);													\
	}																					\
}

/* generic dword-sized write handler (16-bit aligned only!) */
#define WRITELONG(name,type,abits,align)												\
void name##_dword(offs_t address,data_t data)											\
{																						\
	UINT16 word1, word2;																\
	MHELE hw1, hw2; 																	\
																						\
	/* only supports 16-bit memory systems */											\
	if (type == TYPE_8BIT)																\
		printf("Unsupported type for WRITELONG macro!\n");                              \
																						\
	/* handle aligned case first */ 													\
	if (align == ALWAYS_ALIGNED || !(address & 1))										\
	{																					\
		int address2 = (address + 2) & ADDRESS_MASK(abits); 							\
																						\
		/* first-level lookup */														\
		hw1 = cur_mwhard[(UINT32)address >> (ABITS2_##abits + ABITS_MIN_##abits)];		\
		hw2 = cur_mwhard[(UINT32)address2 >> (ABITS2_##abits + ABITS_MIN_##abits)]; 	\
																						\
		/* second-level lookup */														\
		if (hw1 >= MH_HARDMAX)															\
		{																				\
			hw1 -= MH_HARDMAX;															\
			hw1 = writehardware[(hw1 << MH_SBITS) + (((UINT32)address >> ABITS_MIN_##abits) & MHMASK(ABITS2_##abits))]; \
		}																				\
		if (hw2 >= MH_HARDMAX)															\
		{																				\
			hw2 -= MH_HARDMAX;															\
			hw2 = writehardware[(hw2 << MH_SBITS) + (((UINT32)address2 >> ABITS_MIN_##abits) & MHMASK(ABITS2_##abits))];	\
		}																				\
																						\
		/* extract words */ 															\
		if (type == TYPE_16BIT_BE)														\
		{																				\
			word1 = data >> 16; 														\
			word2 = data & 0xffff;														\
		}																				\
		else if (type == TYPE_16BIT_LE) 												\
		{																				\
			word1 = data & 0xffff;														\
			word2 = data >> 16; 														\
		}																				\
																						\
		/* process each word */ 														\
		if (hw1 <= HT_BANKMAX)															\
			WRITE_WORD(&cpu_bankbase[hw1][address - memorywriteoffset[hw1]], word1);	\
		else																			\
			(*memorywritehandler[hw1])(address - memorywriteoffset[hw1], word1);		\
		if (hw2 <= HT_BANKMAX)															\
			WRITE_WORD(&cpu_bankbase[hw2][address2 - memorywriteoffset[hw2]], word2);	\
		else																			\
			(*memorywritehandler[hw2])(address2 - memorywriteoffset[hw2], word2);		\
	}																					\
																						\
	/* unaligned case */																\
	else if (type == TYPE_16BIT_BE) 													\
	{																					\
		name(address, data >> 24);														\
		name##_word(address + 1, (data >> 8) & 0xffff); 								\
		name(address + 3, data & 0xff); 												\
	}																					\
	else if (type == TYPE_16BIT_LE) 													\
	{																					\
		name(address, data & 0xff); 													\
		name##_word(address + 1, (data >> 8) & 0xffff); 								\
		name(address + 3, data >> 24);													\
	}																					\
}


/* the handlers we need to generate */
//WRITEBYTE(cpu_writemem16,	 TYPE_8BIT, 	16)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem16(offs_t address,data_t data)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mwhard[(UINT32)address >> (ABITS2_16 + ABITS_MIN_16)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw == HT_RAM)
	{
		cpu_bankbase[HT_RAM][address] = data;
		return; 
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_16) & MHMASK(ABITS2_16))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw == HT_RAM)
		{
			cpu_bankbase[HT_RAM][address] = data;
			return; 
		}
	}

	/* fall back to handler */
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data);
}

//WRITEBYTE(cpu_writemem20,	 TYPE_8BIT, 	20)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem20(offs_t address,data_t data)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mwhard[(UINT32)address >> (ABITS2_20 + ABITS_MIN_20)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw == HT_RAM)
	{
		cpu_bankbase[HT_RAM][address] = data;
		return; 
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_20) & MHMASK(ABITS2_20))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw == HT_RAM)
		{
			cpu_bankbase[HT_RAM][address] = data;
			return; 
		}
	}

	/* fall back to handler */
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data);
}

//WRITEBYTE(cpu_writemem21,	 TYPE_8BIT, 	21)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem21(offs_t address,data_t data)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mwhard[(UINT32)address >> (ABITS2_21 + ABITS_MIN_21)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw == HT_RAM)
	{
		cpu_bankbase[HT_RAM][address] = data;
		return; 
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_21) & MHMASK(ABITS2_21))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw == HT_RAM)
		{
			cpu_bankbase[HT_RAM][address] = data;
			return; 
		}
	}

	/* fall back to handler */
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data);
}


//WRITEBYTE(cpu_writemem16bew, TYPE_16BIT_BE, 16BEW)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem16bew(offs_t address,data_t data)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mwhard[(UINT32)address >> (ABITS2_16BEW + ABITS_MIN_16BEW)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw <= HT_BANKMAX) 
	{
		cpu_bankbase[hw][BYTE_XOR_BE(address) - memorywriteoffset[hw]] = data;
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_16BEW) & MHMASK(ABITS2_16BEW))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw <= HT_BANKMAX) 
		{
			cpu_bankbase[hw][BYTE_XOR_BE(address) - memorywriteoffset[hw]] = data;
		}
	}

	/* fall back to handler */
	{
	    int shift = ((address & 1) << 3)^8; 
	    data = (0xff000000 >> shift) | ((data & 0xff) << shift);
	    address &= ~1;
    }
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data);
}

//WRITEWORD(cpu_writemem16bew, TYPE_16BIT_BE, 16BEW, ALWAYS_ALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem16bew_word(offs_t address,data_t data)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mwhard[(UINT32)address >> (ABITS2_16BEW + ABITS_MIN_16BEW)];
	if (hw <= HT_BANKMAX)
	{
		WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
		return; 
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_16BEW) & MHMASK(ABITS2_16BEW))]; 
		if (hw <= HT_BANKMAX)
		{
			WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
			return; 
		}
	}

	/* fall back to handler */
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data & 0xffff);
}

//WRITEBYTE(cpu_writemem16lew, TYPE_16BIT_LE, 16LEW)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem16lew(offs_t address,data_t data)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mwhard[(UINT32)address >> (ABITS2_16LEW + ABITS_MIN_16LEW)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw <= HT_BANKMAX) 
	{
		cpu_bankbase[hw][BYTE_XOR_LE(address) - memorywriteoffset[hw]] = data;
		return; 
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_16LEW) & MHMASK(ABITS2_16LEW))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw <= HT_BANKMAX) 
		{
			cpu_bankbase[hw][BYTE_XOR_LE(address) - memorywriteoffset[hw]] = data;
			return; 
		}
	}

	/* fall back to handler */
	{
	    int shift = (address & 1) << 3; 
	    data = (0xff000000 >> shift) | ((data & 0xff) << shift);
	    address &= ~1;
	}
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data);
}

//WRITEWORD(cpu_writemem16lew, TYPE_16BIT_LE, 16LEW, ALWAYS_ALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem16lew_word(offs_t address,data_t data)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mwhard[(UINT32)address >> (ABITS2_16LEW + ABITS_MIN_16LEW)];
	if (hw <= HT_BANKMAX)
	{
		WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
		return; 
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_16LEW) & MHMASK(ABITS2_16LEW))]; 
		if (hw <= HT_BANKMAX)
		{
			WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
			return; 
		}
	}

	/* fall back to handler */
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data & 0xffff);
}

//WRITEBYTE(cpu_writemem24,	 TYPE_8BIT, 	24)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem24(offs_t address,data_t data)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mwhard[(UINT32)address >> (ABITS2_24 + ABITS_MIN_24)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw == HT_RAM)
	{
		cpu_bankbase[HT_RAM][address] = data;
		return; 
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_24) & MHMASK(ABITS2_24))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw == HT_RAM)
		{
			cpu_bankbase[HT_RAM][address] = data;
			return; 
		}
	}

	/* fall back to handler */
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data);
}


//WRITEBYTE(cpu_writemem24bew, TYPE_16BIT_BE, 24BEW)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem24bew(offs_t address,data_t data)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mwhard[(UINT32)address >> (ABITS2_24BEW + ABITS_MIN_24BEW)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw <= HT_BANKMAX) 
	{
		cpu_bankbase[hw][BYTE_XOR_BE(address) - memorywriteoffset[hw]] = data;
		return; 
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_24BEW) & MHMASK(ABITS2_24BEW))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw <= HT_BANKMAX) 
		{
			cpu_bankbase[hw][BYTE_XOR_BE(address) - memorywriteoffset[hw]] = data;
			return; 
		}
	}

	/* fall back to handler */
	{
	    int shift = ((address & 1) << 3)^8; 
	    data = (0xff000000 >> shift) | ((data & 0xff) << shift);
	    address &= ~1;
	}
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data);
}

//WRITEWORD(cpu_writemem24bew, TYPE_16BIT_BE, 24BEW, CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem24bew_word(offs_t address,data_t data)
{
	MHELE hw;

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		/* first-level lookup */
		hw = cur_mwhard[(UINT32)address >> (ABITS2_24BEW + ABITS_MIN_24BEW)];
		if (hw <= HT_BANKMAX)
		{
			WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
			return; 
		}

		/* second-level lookup */
		if (hw >= MH_HARDMAX)
		{
			hw -= MH_HARDMAX;
			hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_24BEW) & MHMASK(ABITS2_24BEW))]; 
			if (hw <= HT_BANKMAX)
			{
				WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
				return; 
			}
		}

		/* fall back to handler */
		(*memorywritehandler[hw])(address - memorywriteoffset[hw], data & 0xffff);
	}

	/* unaligned case */
	else
	{
		cpu_writemem24bew(address, data >> 8);
		cpu_writemem24bew(address + 1, data & 0xff); 
	}
}

//WRITELONG(cpu_writemem24bew, TYPE_16BIT_BE, 24BEW, CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem24bew_dword(offs_t address,data_t data)
{
	UINT16 word1, word2;
	MHELE hw1, hw2; 

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		int address2 = (address + 2) & ADDRESS_MASK(24BEW); 

		/* first-level lookup */
		hw1 = cur_mwhard[(UINT32)address >> (ABITS2_24BEW + ABITS_MIN_24BEW)];
		hw2 = cur_mwhard[(UINT32)address2 >> (ABITS2_24BEW + ABITS_MIN_24BEW)]; 

		/* second-level lookup */
		if (hw1 >= MH_HARDMAX)
		{
			hw1 -= MH_HARDMAX;
			hw1 = writehardware[(hw1 << MH_SBITS) + (((UINT32)address >> ABITS_MIN_24BEW) & MHMASK(ABITS2_24BEW))]; 
		}
		if (hw2 >= MH_HARDMAX)
		{
			hw2 -= MH_HARDMAX;
			hw2 = writehardware[(hw2 << MH_SBITS) + (((UINT32)address2 >> ABITS_MIN_24BEW) & MHMASK(ABITS2_24BEW))];
		}

		/* extract words */ 
		word1 = data >> 16; 
		word2 = data & 0xffff;

		/* process each word */ 
		if (hw1 <= HT_BANKMAX)
			WRITE_WORD(&cpu_bankbase[hw1][address - memorywriteoffset[hw1]], word1);
		else
			(*memorywritehandler[hw1])(address - memorywriteoffset[hw1], word1);
		if (hw2 <= HT_BANKMAX)
			WRITE_WORD(&cpu_bankbase[hw2][address2 - memorywriteoffset[hw2]], word2);
		else
			(*memorywritehandler[hw2])(address2 - memorywriteoffset[hw2], word2);
	}

	/* unaligned case */
	else
	{
		cpu_writemem24bew(address, data >> 24);
		cpu_writemem24bew_word(address + 1, (data >> 8) & 0xffff); 
		cpu_writemem24bew(address + 3, data & 0xff); 
	}
}


//WRITEBYTE(cpu_writemem26lew, TYPE_16BIT_LE, 26LEW)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem26lew(offs_t address,data_t data)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mwhard[(UINT32)address >> (ABITS2_26LEW + ABITS_MIN_26LEW)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw <= HT_BANKMAX) 
	{
		cpu_bankbase[hw][BYTE_XOR_LE(address) - memorywriteoffset[hw]] = data;
		return; 
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_26LEW) & MHMASK(ABITS2_26LEW))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw <= HT_BANKMAX) 
		{
			cpu_bankbase[hw][BYTE_XOR_LE(address) - memorywriteoffset[hw]] = data;
			return; 
		}
	}

	/* fall back to handler */
	{
	    int shift = (address & 1) << 3; 
	    data = (0xff000000 >> shift) | ((data & 0xff) << shift);
	    address &= ~1;
	}
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data);
}

//WRITEWORD(cpu_writemem26lew, TYPE_16BIT_LE, 26LEW, ALWAYS_ALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem26lew_word(offs_t address,data_t data)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mwhard[(UINT32)address >> (ABITS2_26LEW + ABITS_MIN_26LEW)];
	if (hw <= HT_BANKMAX)
	{
		WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
		return; 
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_26LEW) & MHMASK(ABITS2_26LEW))]; 
		if (hw <= HT_BANKMAX)
		{
			WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
			return; 
		}
	}

	/* fall back to handler */
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data & 0xffff);
}

//WRITELONG(cpu_writemem26lew, TYPE_16BIT_LE, 26LEW, ALWAYS_ALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem26lew_dword(offs_t address,data_t data)
{
	UINT16 word1, word2;
	MHELE hw1, hw2; 

	int address2 = (address + 2) & ADDRESS_MASK(26LEW); 

	/* first-level lookup */
	hw1 = cur_mwhard[(UINT32)address >> (ABITS2_26LEW + ABITS_MIN_26LEW)];
	hw2 = cur_mwhard[(UINT32)address2 >> (ABITS2_26LEW + ABITS_MIN_26LEW)]; 

	/* second-level lookup */
	if (hw1 >= MH_HARDMAX)
	{
		hw1 -= MH_HARDMAX;
		hw1 = writehardware[(hw1 << MH_SBITS) + (((UINT32)address >> ABITS_MIN_26LEW) & MHMASK(ABITS2_26LEW))]; 
	}
	if (hw2 >= MH_HARDMAX)
	{
		hw2 -= MH_HARDMAX;
		hw2 = writehardware[(hw2 << MH_SBITS) + (((UINT32)address2 >> ABITS_MIN_26LEW) & MHMASK(ABITS2_26LEW))];
	}

	/* extract words */ 
	word1 = data & 0xffff;
	word2 = data >> 16; 

	/* process each word */ 
	if (hw1 <= HT_BANKMAX)
		WRITE_WORD(&cpu_bankbase[hw1][address - memorywriteoffset[hw1]], word1);
	else
		(*memorywritehandler[hw1])(address - memorywriteoffset[hw1], word1);
	if (hw2 <= HT_BANKMAX)
		WRITE_WORD(&cpu_bankbase[hw2][address2 - memorywriteoffset[hw2]], word2);
	else
		(*memorywritehandler[hw2])(address2 - memorywriteoffset[hw2], word2);
}

//WRITEBYTE(cpu_writemem29,	 TYPE_16BIT_LE, 29)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem29(offs_t address,data_t data)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mwhard[(UINT32)address >> (ABITS2_29 + ABITS_MIN_29)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw <= HT_BANKMAX) 
	{
		cpu_bankbase[hw][BYTE_XOR_LE(address) - memorywriteoffset[hw]] = data;
		return; 
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_29) & MHMASK(ABITS2_29))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw <= HT_BANKMAX) 
		{
			cpu_bankbase[hw][BYTE_XOR_LE(address) - memorywriteoffset[hw]] = data;
			return; 
		}
	}

	/* fall back to handler */
	{
	    int shift = (address & 1) << 3; 
	    data = (0xff000000 >> shift) | ((data & 0xff) << shift);
	    address &= ~1;
	}
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data);
}

//WRITEWORD(cpu_writemem29,	 TYPE_16BIT_LE, 29,    CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem29_word(offs_t address,data_t data)
{
	MHELE hw;

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		/* first-level lookup */
		hw = cur_mwhard[(UINT32)address >> (ABITS2_29 + ABITS_MIN_29)];
		if (hw <= HT_BANKMAX)
		{
			WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
			return; 
		}

		/* second-level lookup */
		if (hw >= MH_HARDMAX)
		{
			hw -= MH_HARDMAX;
			hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_29) & MHMASK(ABITS2_29))]; 
			if (hw <= HT_BANKMAX)
			{
				WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
				return; 
			}
		}

		/* fall back to handler */
		(*memorywritehandler[hw])(address - memorywriteoffset[hw], data & 0xffff);
	}
	/* unaligned case */
	else
	{
		cpu_writemem29(address, data & 0xff); 
		cpu_writemem29(address + 1, data >> 8);
	}
}

//WRITELONG(cpu_writemem29,	 TYPE_16BIT_LE, 29,    CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem29_dword(offs_t address,data_t data)
{
	UINT16 word1, word2;
	MHELE hw1, hw2; 

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		int address2 = (address + 2) & ADDRESS_MASK(29); 

		/* first-level lookup */
		hw1 = cur_mwhard[(UINT32)address >> (ABITS2_29 + ABITS_MIN_29)];
		hw2 = cur_mwhard[(UINT32)address2 >> (ABITS2_29 + ABITS_MIN_29)]; 

		/* second-level lookup */
		if (hw1 >= MH_HARDMAX)
		{
			hw1 -= MH_HARDMAX;
			hw1 = writehardware[(hw1 << MH_SBITS) + (((UINT32)address >> ABITS_MIN_29) & MHMASK(ABITS2_29))]; 
		}
		if (hw2 >= MH_HARDMAX)
		{
			hw2 -= MH_HARDMAX;
			hw2 = writehardware[(hw2 << MH_SBITS) + (((UINT32)address2 >> ABITS_MIN_29) & MHMASK(ABITS2_29))];
		}

		/* extract words */ 
		word1 = data & 0xffff;
		word2 = data >> 16; 

		/* process each word */ 
		if (hw1 <= HT_BANKMAX)
			WRITE_WORD(&cpu_bankbase[hw1][address - memorywriteoffset[hw1]], word1);
		else
			(*memorywritehandler[hw1])(address - memorywriteoffset[hw1], word1);
		if (hw2 <= HT_BANKMAX)
			WRITE_WORD(&cpu_bankbase[hw2][address2 - memorywriteoffset[hw2]], word2);
		else
			(*memorywritehandler[hw2])(address2 - memorywriteoffset[hw2], word2);
	}

	/* unaligned case */
	else
	{
		cpu_writemem29(address, data & 0xff); 
		cpu_writemem29_word(address + 1, (data >> 8) & 0xffff); 
		cpu_writemem29(address + 3, data >> 24);
	}
}

//WRITEBYTE(cpu_writemem32,	 TYPE_16BIT_BE, 32)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem32(offs_t address,data_t data)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mwhard[(UINT32)address >> (ABITS2_32 + ABITS_MIN_32)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw <= HT_BANKMAX) 
	{
		cpu_bankbase[hw][BYTE_XOR_BE(address) - memorywriteoffset[hw]] = data;
		return; 
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_32) & MHMASK(ABITS2_32))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw <= HT_BANKMAX) 
		{
			cpu_bankbase[hw][BYTE_XOR_BE(address) - memorywriteoffset[hw]] = data;
			return; 
		}
	}

	/* fall back to handler */
	{
	    int shift = ((address & 1) << 3)^8; 
	    data = (0xff000000 >> shift) | ((data & 0xff) << shift);
	    address &= ~1;
	}
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data);
}

//WRITEWORD(cpu_writemem32,	 TYPE_16BIT_BE, 32,    CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem32_word(offs_t address,data_t data)
{
	MHELE hw;

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		/* first-level lookup */
		hw = cur_mwhard[(UINT32)address >> (ABITS2_32 + ABITS_MIN_32)];
		if (hw <= HT_BANKMAX)
		{
			WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
			return; 
		}

		/* second-level lookup */
		if (hw >= MH_HARDMAX)
		{
			hw -= MH_HARDMAX;
			hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_32) & MHMASK(ABITS2_32))]; 
			if (hw <= HT_BANKMAX)
			{
				WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
				return; 
			}
		}

		/* fall back to handler */
		(*memorywritehandler[hw])(address - memorywriteoffset[hw], data & 0xffff);
	}
	/* unaligned case */
	else
	{
		cpu_writemem32(address, data >> 8);
		cpu_writemem32(address + 1, data & 0xff); 
	}
}

//WRITELONG(cpu_writemem32,	 TYPE_16BIT_BE, 32,    CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem32_dword(offs_t address,data_t data)
{
	UINT16 word1, word2;
	MHELE hw1, hw2; 

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		int address2 = (address + 2) & ADDRESS_MASK(32); 

		/* first-level lookup */
		hw1 = cur_mwhard[(UINT32)address >> (ABITS2_32 + ABITS_MIN_32)];
		hw2 = cur_mwhard[(UINT32)address2 >> (ABITS2_32 + ABITS_MIN_32)]; 

		/* second-level lookup */
		if (hw1 >= MH_HARDMAX)
		{
			hw1 -= MH_HARDMAX;
			hw1 = writehardware[(hw1 << MH_SBITS) + (((UINT32)address >> ABITS_MIN_32) & MHMASK(ABITS2_32))]; 
		}
		if (hw2 >= MH_HARDMAX)
		{
			hw2 -= MH_HARDMAX;
			hw2 = writehardware[(hw2 << MH_SBITS) + (((UINT32)address2 >> ABITS_MIN_32) & MHMASK(ABITS2_32))];
		}

		/* extract words */ 
		word1 = data >> 16; 
		word2 = data & 0xffff;

		/* process each word */ 
		if (hw1 <= HT_BANKMAX)
			WRITE_WORD(&cpu_bankbase[hw1][address - memorywriteoffset[hw1]], word1);
		else
			(*memorywritehandler[hw1])(address - memorywriteoffset[hw1], word1);
		if (hw2 <= HT_BANKMAX)
			WRITE_WORD(&cpu_bankbase[hw2][address2 - memorywriteoffset[hw2]], word2);
		else
			(*memorywritehandler[hw2])(address2 - memorywriteoffset[hw2], word2);
	}

	/* unaligned case */
	else
	{
		cpu_writemem32(address, data >> 24);
		cpu_writemem32_word(address + 1, (data >> 8) & 0xffff); 
		cpu_writemem32(address + 3, data & 0xff); 
	}
}

//WRITEBYTE(cpu_writemem32lew, TYPE_16BIT_LE, 32LEW)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem32lew(offs_t address,data_t data)
{
	MHELE hw;

	/* first-level lookup */
	hw = cur_mwhard[(UINT32)address >> (ABITS2_32LEW + ABITS_MIN_32LEW)];

	/* for compatibility with setbankhandler, 8-bit systems must call handlers */
	/* for banked memory reads/writes */
	if (hw <= HT_BANKMAX) 
	{
		cpu_bankbase[hw][BYTE_XOR_LE(address) - memorywriteoffset[hw]] = data;
		return; 
	}

	/* second-level lookup */
	if (hw >= MH_HARDMAX)
	{
		hw -= MH_HARDMAX;
		hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_32LEW) & MHMASK(ABITS2_32LEW))];

		/* for compatibility with setbankhandler, 8-bit systems must call handlers */
		/* for banked memory reads/writes */
		if (hw <= HT_BANKMAX) 
		{
			cpu_bankbase[hw][BYTE_XOR_LE(address) - memorywriteoffset[hw]] = data;
			return; 
		}
	}

	/* fall back to handler */
	{
	    int shift = (address & 1) << 3; 
	    data = (0xff000000 >> shift) | ((data & 0xff) << shift);
	    address &= ~1;
	}
	(*memorywritehandler[hw])(address - memorywriteoffset[hw], data);
}

//WRITEWORD(cpu_writemem32lew, TYPE_16BIT_LE, 32LEW, CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem32lew_word(offs_t address,data_t data)
{
	MHELE hw;

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		/* first-level lookup */
		hw = cur_mwhard[(UINT32)address >> (ABITS2_32LEW + ABITS_MIN_32LEW)];
		if (hw <= HT_BANKMAX)
		{
			WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
			return; 
		}

		/* second-level lookup */
		if (hw >= MH_HARDMAX)
		{
			hw -= MH_HARDMAX;
			hw = writehardware[(hw << MH_SBITS) + (((UINT32)address >> ABITS_MIN_32LEW) & MHMASK(ABITS2_32LEW))]; 
			if (hw <= HT_BANKMAX)
			{
				WRITE_WORD(&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
				return; 
			}
		}

		/* fall back to handler */
		(*memorywritehandler[hw])(address - memorywriteoffset[hw], data & 0xffff);
	}

	/* unaligned case */
	else
	{
		cpu_writemem32lew(address, data & 0xff); 
		cpu_writemem32lew(address + 1, data >> 8);
	}
}

//WRITELONG(cpu_writemem32lew, TYPE_16BIT_LE, 32LEW, CAN_BE_MISALIGNED)
#ifdef MAME_MEMINLINE
INLINE
#endif
void cpu_writemem32lew_dword(offs_t address,data_t data)
{
	UINT16 word1, word2;
	MHELE hw1, hw2; 

	/* handle aligned case first */ 
	if (!(address & 1))
	{
		int address2 = (address + 2) & ADDRESS_MASK(32LEW); 

		/* first-level lookup */
		hw1 = cur_mwhard[(UINT32)address >> (ABITS2_32LEW + ABITS_MIN_32LEW)];
		hw2 = cur_mwhard[(UINT32)address2 >> (ABITS2_32LEW + ABITS_MIN_32LEW)]; 

		/* second-level lookup */
		if (hw1 >= MH_HARDMAX)
		{
			hw1 -= MH_HARDMAX;
			hw1 = writehardware[(hw1 << MH_SBITS) + (((UINT32)address >> ABITS_MIN_32LEW) & MHMASK(ABITS2_32LEW))]; 
		}
		if (hw2 >= MH_HARDMAX)
		{
			hw2 -= MH_HARDMAX;
			hw2 = writehardware[(hw2 << MH_SBITS) + (((UINT32)address2 >> ABITS_MIN_32LEW) & MHMASK(ABITS2_32LEW))];
		}

		/* extract words */ 
		word1 = data & 0xffff;
		word2 = data >> 16; 

		/* process each word */ 
		if (hw1 <= HT_BANKMAX)
			WRITE_WORD(&cpu_bankbase[hw1][address - memorywriteoffset[hw1]], word1);
		else
			(*memorywritehandler[hw1])(address - memorywriteoffset[hw1], word1);
		if (hw2 <= HT_BANKMAX)
			WRITE_WORD(&cpu_bankbase[hw2][address2 - memorywriteoffset[hw2]], word2);
		else
			(*memorywritehandler[hw2])(address2 - memorywriteoffset[hw2], word2);
	}

	/* unaligned case */
	else
	{
		cpu_writemem32lew(address, data & 0xff); 
		cpu_writemem32lew_word(address + 1, (data >> 8) & 0xffff); 
		cpu_writemem32lew(address + 3, data >> 24);
	}
}
