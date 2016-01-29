/* State save/load functions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "osd_cpu.h"
#include "memory.h"
#include "osdepend.h"
#include "state.h"
#include "mame.h"


/* A forward linked list of the contents of a section */
typedef struct tag_state_var {
    struct tag_state_var *next;
    char *name;
    unsigned size;
    unsigned chunk;
    void *data;
}   state_var;

/* Our state handling structure */
typedef struct {
    void *file;
    const char *cur_module;
    int cur_instance;
    state_var *list;
}   state_handle;

INLINE unsigned xtoul(char **p, int *size)
{
	unsigned val = 0, digit;

    if (size) *size = 0;
	while( isxdigit(**p) )
	{
		digit = toupper(**p) - '0';
		if( digit > 9 ) digit -= 7;
		val = (val << 4) | digit;
		if( size ) (*size)++;
		*p += 1;
	}
	while( isspace(**p) ) *p += 1;
	if (size) (*size) >>= 1;
	return val;
}

INLINE char *ultox(unsigned val, unsigned size)
{
	static char buffer[32+1];
	static char digit[] = "0123456789ABCDEF";
	char *p = &buffer[size];
	*p-- = '\0';
	while( size-- > 0 )
	{
		*p-- = digit[val & 15];
		val >>= 4;
	}
	return buffer;
}

/**************************************************************************
 * my_stricmp
 * Compare strings case insensitive
 **************************************************************************/
INLINE int my_stricmp( const char *dst, const char *src)
{
	while( *src && *dst )
	{
		if( tolower(*src) != tolower(*dst) ) return *dst - *src;
		src++;
		dst++;
	}
	return *dst - *src;
}

/* free a linked list of state_vars (aka section) */
void state_free_section( void *s )
{
	state_handle *state = (state_handle *)s;
    state_var *v1, *v2;
    v2 = state->list;
    while( v2 )
    {
        if( v2->name ) free( v2->name );
        if( v2->data ) free( v2->data );
        v1 = v2;
        v2 = v2->next;
        free( v1 );
    }
    state->list = NULL;
}

void *state_create(const char *name)
{
	state_handle *state;
	state = (state_handle *) malloc( sizeof(state_handle) );
	if( !state ) return NULL;
	state->cur_module = NULL;
	state->cur_instance = 0;
	state->list = NULL;
	state->file = osd_fopen( name, NULL, OSD_FILETYPE_STATE, 1 );
	if( !state->file )
	{
		free(state);
		return NULL;
	}
	return state;
}

void *state_open(const char *name)
{
	state_handle *state;
	state = (state_handle *) malloc( sizeof(state_handle) );
	if( !state ) return NULL;
	state->cur_module = NULL;
	state->cur_instance = 0;
	state->list = NULL;
	state->file = osd_fopen( name, NULL, OSD_FILETYPE_STATE, 0 );
	if( !state->file )
	{
		free(state);
		return NULL;
	}
	return state;
}

void state_close( void *s )
{
	state_handle *state = (state_handle *)s;
    if( !state ) return;
	state_free_section( state );
	if( state->file ) osd_fclose( state->file );
	free( state );
}

/* Output a formatted string to the state file */
static void CLIB_DECL emit(void *s, const char *fmt, ...)
{
    static char buffer[128+1];
	state_handle *state = (state_handle *)s;
    va_list arg;
	int length;

    va_start(arg,fmt);
	length = vsprintf(buffer, fmt, arg);
	va_end(arg);

	if( osd_fwrite(state->file, buffer, length) != length )
	{
		logerror("emit: Error while saving state '%s'\n", buffer);
	}
}

void state_save_section( void *s, const char *module, int instance )
{
	state_handle *state = (state_handle *)s;
    if( !state->cur_module ||
		(state->cur_module && my_stricmp(state->cur_module, module)) ||
		state->cur_instance != instance )
    {
		if( state->cur_module )
			emit(state,"\n");
		state->cur_module = module;
		state->cur_instance = instance;
		emit(state,"[%s.%d]\n", module, instance);
    }
}

void state_save_UINT8( void *s, const char *module,int instance,
	const char *name, const UINT8 *val, unsigned size )
{
	state_handle *state = (state_handle *)s;

    state_save_section( state, module, instance );

    /* If this is to much for a single line use the dump format */
	if( size > 16 )
	{
		unsigned offs = 0;
		while( size-- > 0 )
		{
			if( (offs & 15 ) == 0 )
				emit( state, "%s.%s=", name, ultox(offs,4) );
			emit( state, "%s", ultox(*val++,2) );
			if( (++offs & 15) == 0)
				emit( state, "\n" );
			else
				emit( state, " " );
		}
		if( offs & 15 ) emit( state, "\n" );
	}
	else
	{
		emit( state, "%s=", name );
		while( size-- > 0 )
		{
			emit( state, "%s", ultox(*val++,2) );
			if( size ) emit( state, " " );
        }
		emit( state, "\n" );
    }
}

void state_save_INT8( void *s, const char *module,int instance,
	const char *name, const INT8 *val, unsigned size )
{
	state_save_UINT8( s, module, instance, name, (UINT8*)val, size );
}

void state_save_UINT16(void *s, const char *module,int instance,
	const char *name, const UINT16 *val, unsigned size)
{
	state_handle *state = (state_handle *)s;

    state_save_section( state, module, instance );

    /* If this is to much for a single line use the dump format */
	if( size > 8 )
	{
		unsigned offs = 0;
		while( size-- > 0 )
		{
			if( (offs & 7 ) == 0 )
				emit( state, "%s.%s=", name, ultox(offs,4) );
			emit( state, "%s", ultox(*val++,4) );
			if( (++offs & 7) == 0)
				emit( state, "\n" );
			else
				emit( state, " " );
		}
		if( offs & 7 ) emit( state, "\n" );
	}
	else
	{
		emit( state, "%s=", name );
		while( size-- > 0 )
		{
			emit( state, "%s", ultox(*val++,4) );
			if( size ) emit( state, " " );
        }
		emit( state, "\n" );
    }
}

void state_save_INT16( void *s, const char *module,int instance,
	const char *name, const INT16 *val, unsigned size )
{
	state_save_UINT16( s, module, instance, name, (UINT16*)val, size );
}

void state_save_UINT32( void *s, const char *module,int instance,
	const char *name, const UINT32 *val, unsigned size )
{
	state_handle *state = (state_handle *)s;

    state_save_section( state, module, instance );

    /* If this is to much for a single line use the dump format */
	if( size > 4 )
	{
		unsigned offs = 0;
		while( size-- > 0 )
		{
			if( (offs & 3 ) == 0 )
				emit( state, "%s.%s=", name, ultox(offs,4) );
			emit( state, "%s", ultox(*val++,8) );
			if( (++offs & 3) == 0)
				emit( state, "\n" );
			else
				emit( state, " " );
		}
		if( offs & 3 ) emit( state, "\n" );
	}
	else
	{
		emit( state, "%s=", name );
		while( size-- > 0 )
		{
			emit( state, "%s", ultox(*val++,8) );
			if( size ) emit( state, " " );
        }
		emit( state, "\n" );
    }
}

void state_save_INT32( void *s, const char *module,int instance,
	const char *name, const INT32 *val, unsigned size )
{
	state_save_UINT32( s, module, instance, name, (UINT32*)val, size );
}

/* load a linked list of state_vars (aka section) */
void state_load_section( void *s, const char *module, int instance )
{
	state_handle *state = (state_handle *)s;

    /* Make the buffer twice as big as it was while saving
       the state, so we should always catch a [section] */

    static char buffer[256+1];
    char section[128+1], *p, *d;
	int length, element_size, rewind_file = 1;
	unsigned offs, data;

	if( state->cur_module &&
		!my_stricmp(state->cur_module, module) &&
		state->cur_instance == instance )
		return; /* fine, we already got it */

	if( !state->list )
		state_free_section(state);

	sprintf(section, "[%s.%d]", module, instance);

	for( ; ; )
	{
		length = osd_fread(state->file, buffer, sizeof(buffer) - 1);
		if( length <= 0 )
		{
			if( rewind_file )
			{
				logerror("state_load_section: Section '%s' not found\n", section);
				return;
			}

			rewind_file = 0;
			osd_fseek(state->file, 0, SEEK_SET);
			length = osd_fread(state->file, buffer, sizeof(buffer) - 1);
			if( length <= 0 )
			{
				logerror("state_load_section: Truncated state while loading state '%s'\n", section);
				return;
			}
		}
		buffer[ length ] = '\0';
		p = strchr(buffer, '[');
		if( p && !my_stricmp(p, section) )
		{
			/* skip CR, LF or both */
			p += strlen(section);
			if( *p == '\r' || *p == '\n' ) p++; /* skip CR or LF */
			if( *p == '\r' || *p == '\n' ) p++; /* in any order */
			state->cur_module = module;
			state->cur_instance = instance;
			/* now read all state_vars until the next section or end of state */
			for( ; ; )
			{
				state_var *v;

				/* seek back to the end of line state position */
				osd_fseek( state->file, (int)(p - &buffer[length]), SEEK_CUR );

				length = osd_fread( state->file, buffer, sizeof(buffer) - 1 );

                if( length <= 0 )
                    return;
				buffer[ length ] = '\0';

				p = strchr(buffer, '\n');
				if( !p ) p = strchr(buffer, '\r');
				if( !p )
				{
					logerror("state_load_section: Line to long in section '%s'\n", section);
					return;
				}

				*p = '\0';                  /* cut buffer here */
				p = strchr(buffer, '\n');   /* do we still have a CR? */
				if( p ) *p = '\0';
				p = strchr(buffer, '\r');   /* do we still have a LF? */
				if( p ) *p = '\0';


				if( *buffer == '[' )        /* next section ? */
					return;

				if( *buffer == '\0' ||      /* empty line or comment ? */
					*buffer == '#' ||
					*buffer == ';' )
					continue;

				/* find the state_var data */
				p = strchr(buffer, '=');
				if( !p )
				{
					logerror("state_load_section: Line contains no '=' character\n");
					return;
				}

				/* buffer = state_var[.offs], p = data */
				*p++ = '\0';

				/* is there an offs defined ? */
                d = strchr(buffer, '.');
				if( d )
				{
					/* buffer = state_var, d = offs, p = data */
					*d++ = '\0';
					offs = xtoul(&d,NULL);
					if( offs )
					{
						v = state->list;
						while( v && my_stricmp(v->name, buffer) )
							v = v->next;
						if( !v )
						{
							logerror("state_load_section: Invalid variable continuation found '%s.%04X'\n", buffer, offs);
							return;
						}
					}
				}
				else
				{
					offs = 0;
				}

				if( state->list )
				{
					/* next state_var */
					v = state->list;
					while( v->next ) v = v->next;
					v->next = (struct tag_state_var*)malloc( sizeof(state_var) );
					v = v->next;
				}
				else
				{
					/* first state_var */
					state->list = (state_var*)malloc(sizeof(state_var));
					v = state->list;
				}
				if( !v )
				{
					logerror("state_load_section: Out of memory while reading '%s'\n", section);
					return;
				}
				v->name = (char*)malloc(strlen(buffer) + 1);
				if( !v->name )
				{
					logerror("state_load_section: Out of memory while reading '%s'\n", section);
					return;
				}
				strcpy(v->name, buffer);
				v->size = 0;
				v->data = NULL;

                /* convert the line back into data */
				data = xtoul( &p, &element_size );
				do
				{
					v->size++;
					/* need to allocate first/next chunk of memory? */
					if( v->size * element_size >= v->chunk )
					{
						v->chunk += CHUNK_SIZE;
						if( v->data )
							v->data = realloc(v->data, v->chunk);
						else
							v->data = malloc(v->chunk);
					}
					/* check if the (re-)allocation failed */
					if( !v->data )
					{
						logerror("state_load_section: Out of memory while reading '%s'\n", section);
						return;
					}
					/* store element */
					switch( element_size )
					{
						case 1: *((UINT8*)v->data + v->size) = data;
						case 2: *((UINT16*)v->data + v->size) = data;
						case 4: *((UINT32*)v->data + v->size) = data;
					}
					data = xtoul( &p, NULL );
				} while( *p );
			}
		}
		else
		{
			/* skip back a half buffer size */
			osd_fseek( state->file, - (sizeof(buffer)-1) / 2, SEEK_CUR );
		}
	}
}

void state_load_UINT8( void *s, const char *module, int instance,
	const char *name, UINT8 *val, unsigned size )
{
	state_handle *state = (state_handle *)s;
    state_var *v;

	state_load_section( state, module, instance );

	v = state->list;
	while( v && my_stricmp(v->name, name) ) v = v->next;

    if( v )
	{
		unsigned offs;
		for( offs = 0; offs < size && offs < v->size; offs++ )
			*val++ = *((UINT8*)v->data + offs);
	}
	else
	{
		logerror("state_load_UINT8: variable '%s' not found in section [%s.%d]\n", name, module, instance);
		memset(val, 0, size);
	}
}

void state_load_INT8( void *s, const char *module, int instance,
	const char *name, INT8 *val, unsigned size )
{
	state_handle *state = (state_handle *)s;
    state_var *v;

	state_load_section( state, module, instance );

	v = state->list;
	while( v && my_stricmp(v->name, name) ) v = v->next;

    if( v )
	{
		unsigned offs;
		for( offs = 0; offs < size && offs < v->size; offs++ )
			*val++ = *((INT8*)v->data + offs);
	}
	else
	{
		logerror("state_load_INT8: variable '%s' not found in section [%s.%d]\n", name, module, instance);
		memset(val, 0, size);
    }
}

void state_load_UINT16( void *s, const char *module, int instance,
	const char *name, UINT16 *val, unsigned size )
{
	state_handle *state = (state_handle *)s;
    state_var *v;

    state_load_section( state, module, instance );

    v = state->list;
	while( v && my_stricmp(v->name, name) ) v = v->next;

    if( v )
	{
		unsigned offs;
		for( offs = 0; offs < size && offs < v->size; offs++ )
			*val++ = *((UINT16*)v->data + offs);
	}
	else
	{
		logerror("state_load_UINT16: variable '%s' not found in section [%s.%d]\n", name, module, instance);
		memset(val, 0, size * 2);
    }
}

void state_load_INT16( void *s, const char *module, int instance,
	const char *name, INT16 *val, unsigned size )
{
	state_handle *state = (state_handle *)s;
    state_var *v;

    state_load_section( state, module, instance );

    v = state->list;
	while( v && my_stricmp(v->name, name) ) v = v->next;

    if( v )
	{
		unsigned offs;
		for( offs = 0; offs < size && offs < v->size; offs++ )
			*val++ = *((INT16*)v->data + offs);
	}
	else
	{
		logerror("state_load_INT16: variable '%s' not found in section [%s.%d]\n", name, module, instance);
		memset(val, 0, size * 2);
    }
}

void state_load_UINT32( void *s, const char *module, int instance,
	const char *name, UINT32 *val, unsigned size )
{
	state_handle *state = (state_handle *)s;
    state_var *v;

    state_load_section( state, module, instance );

    v = state->list;
	while( v && my_stricmp(v->name, name) ) v = v->next;

    if( v )
	{
		unsigned offs;
		for( offs = 0; offs < size && offs < v->size; offs++ )
			*val++ = *((UINT32*)v->data + offs);
	}
	else
	{
		logerror("state_load_UINT32: variable'%s' not found in section [%s.%d]\n", name, module, instance);
		memset(val, 0, size * 4);
    }
}

void state_load_INT32( void *s, const char *module, int instance,
	const char *name, INT32 *val, unsigned size )
{
	state_handle *state = (state_handle *)s;
    state_var *v;

    state_load_section( state, module, instance );

    v = state->list;
	while( v && my_stricmp(v->name, name) ) v = v->next;

    if( v )
	{
		unsigned offs;
		for( offs = 0; offs < size && offs < v->size; offs++ )
			*val++ = *((INT32*)v->data + offs);
	}
	else
	{
		logerror("state_load_INT32: variable'%s' not found in section [%s.%d]\n", name, module, instance);
		memset(val, 0, size * 4);
    }
}



