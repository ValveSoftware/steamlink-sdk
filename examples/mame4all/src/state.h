#ifndef _STATE_H
#define _STATE_H

#include "osd_cpu.h"

/* Amount of memory to allocate while reading variable size arrays */
/* Tradeoff between calling realloc all the time and wasting memory */
#define CHUNK_SIZE  64

/* Interface to state save/load functions */

/* Close a state file; free temporary memory at the same time */
void state_close(void *state);

/* Create a new state file; name should normally be the games name */
void *state_create(const char *name);

/* Open an existing state file */
void *state_open(const char *name);

/* Save data of various element size and signedness */
void state_save_UINT8(void *state, const char *module,int instance,
	const char *name, const UINT8 *val, unsigned size);
void state_save_INT8(void *state, const char *module,int instance,
	const char *name, const INT8 *val, unsigned size);
void state_save_UINT16(void *state, const char *module,int instance,
	const char *name, const UINT16 *val, unsigned size);
void state_save_INT16(void *state, const char *module,int instance,
	const char *name, const INT16 *val, unsigned size);
void state_save_UINT32(void *state, const char *module,int instance,
	const char *name, const UINT32 *val, unsigned size);
void state_save_INT32(void *state, const char *module,int instance,
	const char *name, const INT32 *val, unsigned size);

/* Load data of various element size and signedness */
void state_load_UINT8(void *state, const char *module,int instance,
	const char *name, UINT8 *val, unsigned size);
void state_load_INT8(void *state, const char *module,int instance,
	const char *name, INT8 *val, unsigned size);
void state_load_UINT16(void *state, const char *module,int instance,
	const char *name, UINT16 *val, unsigned size);
void state_load_INT16(void *state, const char *module,int instance,
	const char *name, INT16 *val, unsigned size);
void state_load_UINT32(void *state, const char *module,int instance,
	const char *name, UINT32 *val, unsigned size);
void state_load_INT32(void *state, const char *module,int instance,
	const char *name, INT32 *val, unsigned size);

#endif
