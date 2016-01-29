/***************************************************************************

  network.c

 ***************************************************************************/


#include <stdio.h>
#include <stdlib.h>

#ifdef MAME_NET

#include "network.h"
#include "osdepend.h"
#include "mame.h"
#include "common.h"   /* Machine struct       */
#include "driver.h"   /* GameDriver struct    */


/* Uncomment to turn on printf code */
/* #define NET_DEBUG */

#ifdef NET_DEBUG
#include <ctype.h>

#define dprintf(x) printf x;
#else
#define dprintf(x)
#endif

/* private funcs */
static int _real_net_sync( int first, char type );
static void net_packet_type_to_string( char type, char packet_type_str[] );

static int vercmp(char *str_a, char *str_b);

/* private vars */
static char netversion[] = "Rev7.3 ()"; /* RevX.Y X = Protocol version, Y = Tweak/Bug fix revision */
static char *mameversion = build_version; /* from mame.h and version.c */
static int is_active = 0;   /* Set to true if the net has been initialized */
static int in_game = 0;     /* Set to true if we are in a game */
static int is_client = 1;
static int num_clients = 0;

/*******************************************************************
 external functions
 *******************************************************************/

int net_init()
{
    dprintf(("net_init() called\n"))
    in_game = 0;
	/* TODO: move is_client setting into os-dependent settings?
	         should os-independent stuff know about this at all?
	*/
	if ( ( is_client = osd_net_init() ) == NET_ERROR )
    {
        dprintf(("osd_net_init() - FAILED\n"))
		return 1;
    }

	is_active = 1; /* we're using network */

	/* if we're the client we can do the version checking now
	   server checks in net_add_player
	*/
	if ( is_client )
	{
		if ( net_vers_check( 0 ) )
        {
            dprintf(("net_version_check() - FAILED\n"))
			return 1;
        }
	}

    dprintf(("net_init() - SUCCESS\n"))
	/* made it this far, must be ok */
	return 0;
}


int net_exit( char type )
{
     dprintf(("net_exit() called\n"))

	/* guard to make sure we're still active */
	if (!is_active)
		return 0;

	/* what kind of exit are we experiencing */
	switch ( type )
	{
		/* if we initiate we'll need to send */
		case NET_QUIT_QUIT:
		case NET_QUIT_ABRT:
			net_sync( type );
			break;

		/* if its peer initiated we just handle it w/o sending */
		case NET_QUIT_OKAY:
			break;
	}

	is_active = 0;
    dprintf(("net_exit() returning osd_net_exit()\n"))
	return osd_net_exit();
}

int net_game_init()
{
    dprintf(("net_game_init() called\n"))
    if (osd_net_game_init() != 0)
    {
        dprintf(("net_game_init - FAILED\n"))
        return 1;
    }

    /* we can do the game checking now */
	if ( is_client )
	{
		if ( net_game_check( 0 ) )
        {
            dprintf(("net_game_check - FAILED\n"))
			return 1;
        }
        dprintf(("net_game_check - SUCCESS\n"))
	}
    else
    {
        int i;

        for (i = 0; i < num_clients; i++)
        {
		    if ( net_game_check( i ) )
            {
                dprintf(("net_game_init player (%d) - FAILED\n", i))
			    return 1;
            }
            dprintf(("net_game_init player (%d) - SUCCESS\n", i))
        }
    }
    in_game = is_active = 1;
    dprintf(("net_game_init() return\n"))
    return 0;
}

int net_game_exit()
{
    in_game = 0;
    dprintf(("net_game_exit called - returning osd_net_game_exit()\n"))
    return osd_net_game_exit();
}

/*
	do consistency check of mame/netmame support and prompt user for input on failure
*/
int net_vers_check( int player )
{
	char szVerSelf[NET_MAX_DATA];
	char szVerPeer[NET_MAX_DATA];
	char cAck = 1; /* assuming false */

	/* setup app and network support version info */
	memset( szVerSelf, 0, sizeof(szVerSelf) ); /* zero out array first */
	strcpy( szVerSelf, build_version );
	strcpy( (szVerSelf+25), netversion );

    dprintf(("net_vers_check()\n"))

	/* do comparisons */
	if ( is_client )
	{
		net_send( 0, NET_INIT_VERS, szVerSelf, NET_MAX_DATA );
		net_recv( 0, NET_INIT_VERS, &cAck, 1 );
	}
	else /* server */
	{
		/* Clients send to us and we check versions */
		net_recv( player, NET_INIT_VERS, szVerPeer, NET_MAX_DATA );

		/* Check the app version */
		if ( vercmp( szVerSelf, szVerPeer ) != 0 )
		{
			printf("Client %d does not match MAME version"
					"\nServer:%s"
					"\nClient:%s\n"
					, player, szVerSelf, szVerPeer );
		}

		/* Check the network version */
		else if ( vercmp( (szVerSelf+25), (szVerPeer+25) ) != 0 )
		{
			printf("Client %d does not match network support version"
					"\nServer:%s"
					"\nClient:%s"
					, player, (szVerSelf+25), (szVerPeer+25) );
			/* TODO:some sort of prompt for return value...??? */
		}

		/* else we are successful */
		else
			cAck = 0;

		/* let client know results of version check */
		net_send( player, NET_INIT_VERS, &cAck, 1 );
	}

    dprintf(("net_vers_check() - %s\n", (cAck) ? "FAILED" : "SUCCESS"))

	return cAck;
}


/*
   *** rjr *** Compare versions; ignore dates (in parentheses)
   So recompiling doesn't bring the world to a screeching halt!
*/
static int vercmp(char *str_a, char *str_b)
{
	char string_a[NET_MAX_DATA];
	char string_b[NET_MAX_DATA];

	char *p;

	strcpy(string_a, str_a);
	strcpy(string_b, str_b);

	if (p = strchr(string_a, '('))
		*p = '\0';

	if (p = strchr(string_b, '('))
		*p = '\0';

	dprintf(("vercmp(%s,%s)\n", string_a, string_b))

	return strcmp(string_a, string_b);
}


#ifdef NET_DEBUG
/* Dump a packet in hex and ascii */
char * array_to_string(int len, char *array)
{
    static char str[500] = "";
    char temp[50];
    char *ptr = array;
    int  i;

    for (i = 0; i < len; i++, ptr++)
    {
        if (i)
            strcat(str,",");
        sprintf(temp,"%02x,'%c'", *ptr & 0xFF, (isalnum(*ptr)) ? *ptr : '.');
        strcat(str, temp);
    }
    return str;
}

#endif
/*
	do consistency check of game/roms and prompt user for input on failure
*/
int net_game_check( int player )
{
	char szVerSelf[NET_MAX_DATA];
	char szVerPeer[NET_MAX_DATA];
	char cAck = 1; /* assuming false */

    dprintf(("net_game_check (%d) as %s, Game('%s')\n",
        player, (is_client) ? "Client" : "Server", Machine->gamedrv->name))
	/* Setup rom version info */
	strcpy( szVerSelf, Machine->gamedrv->name );
	/* TODO: strcpy checksum info? */

	/* do comparisons */
	if ( is_client )
	{
		net_send( 0, NET_INIT_GAME, szVerSelf, strlen(szVerSelf));
		net_recv( 0, NET_INIT_GAME, &cAck, 1 );
	}
	else /* server */
	{
        /* Clients send to us and we check versions */
        memset(szVerPeer, '\0', sizeof(szVerPeer));
        net_recv( player, NET_INIT_GAME, szVerPeer, NET_MAX_DATA);

        dprintf(("Server NET_INIT_GAME: %s\n", array_to_string(8, szVerPeer)))
		if ( strcmp( (szVerSelf), (szVerPeer)) != 0 )
		{
			printf("Client %d does not match rom name"
					"\nServer:%s"
					"\nClient:%s"
					, player, (szVerSelf), (szVerPeer) );
		}

		/* else we are successful */
		else
			cAck = 0;

		/* let client know results of version check */
		net_send( player, NET_INIT_GAME, &cAck, 1 );
	}
    dprintf(("net_game_check (%d) - %s\n", player, (cAck) ? "FAILED" : "SUCCESS"))
	return cAck;
}

int net_sync( char type )
{
    dprintf(("net_sync() called\n"))
	switch ( type )
	{
		case NET_SYNC_INIT:
		case NET_SYNC_REDO:
			return _real_net_sync( is_client, type );
			break;

		case NET_QUIT_QUIT:
		case NET_QUIT_ABRT:
			/* we still sync first if we've decided to exit */
			return _real_net_sync( 1, type );
			break;

		default:
			printf("net_sync: asked to sync a type we don't recognize at this point\n");
	}
	return 1; /* error shouldn't reach this far */
}

static int _real_net_sync( int first, char type )
{
	static NET_BYTE szMsg[] = "PeterAndMichael";
	static int size = 16;

    dprintf(("_real_net_sync() called as %d - first is %s\n",
        (is_client) ? "Client" : "Server",
        (first)     ? "TRUE"   : "FALSE"))

	if ( is_client )
	{
		if ( first )
		{
			net_send( 0, type, szMsg, size );
			net_recv( 0, type, szMsg, NET_MAX_DATA );
		}
		else
		{
			net_recv( 0, type, szMsg, NET_MAX_DATA );
			net_send( 0, type, szMsg, size );
		}
	}

	else
	{
		int i;
		if ( first )
		{
			for ( i=0; i < num_clients; i++)
			{
				net_send( 0, type, szMsg, size );
				net_recv( 0, type, szMsg, NET_MAX_DATA );
			}
		}
		else
		{
			for ( i=0; i < num_clients; i++)
			{
				net_recv( 0, type, szMsg, NET_MAX_DATA );
				net_send( 0, type, szMsg, size );
			}
		}
	}

	/*  error checking? */
    dprintf(("_real_net_sync() called\n"))
	return 0;
}

int net_analog_sync(unsigned char input_port_value[], int port,
					int analog_player_port[], int default_player)
{
	unsigned char junk[MAX_INPUT_PORTS]; /* used just for synchronizing - values don't matter */

    dprintf(("net_analog_sync() called\n"))
	if ( is_client )
	{
		if ( analog_player_port[port] == default_player ) /* we control this port */
		{
			/* we tell the server our input */
			net_send( 0, NET_SYNC_ANLG, &input_port_value[port], 1 );
			net_recv( 0, NET_SYNC_ANLG, &junk[port], 1 );
		}
		else
		{
			/* we receive the correct input from the server */
			net_send( 0, NET_SYNC_ANLG, &junk[port], 1 );
			net_recv( 0, NET_SYNC_ANLG, &input_port_value[port], 1 );
		}
	}
	else
	{
		if ( analog_player_port[port] == default_player ) /* we control this port */
		{
			/* we tell the client our input */
			net_recv( 0, NET_SYNC_ANLG, &junk[port], 1 );
			net_send( 0, NET_SYNC_ANLG, &input_port_value[port], 1 );
		}
		else
		{
			/* we receive the correct input from the client */
			net_recv( 0, NET_SYNC_ANLG, &input_port_value[port], 1 );
			net_send( 0, NET_SYNC_ANLG, &junk[port], 1 );
		}
	}
    dprintf(("net_analog_sync() return\n"))

	return 0;
}


int net_input_sync( unsigned char input_port_value[], unsigned char input_port_default[], int num_ports )
{
	int port;

    dprintf(("net_input_sync() called\n"))
	if ( is_client )
	{
		/* mark default bits */
		for(port=0; port < num_ports; port++)
			input_port_value[port] ^= input_port_default[port];

		/* send our changes to server for merging */
		net_send( 0, NET_SYNC_INPT, input_port_value, num_ports );

		/* receive final merged input from server */
		net_recv( 0, NET_SYNC_INPT, input_port_value, num_ports );
	}

	else /* server */
	{
		int client;
		static unsigned char changed_input_port_value[MAX_INPUT_PORTS];
		static unsigned char old_input_port_value[MAX_INPUT_PORTS]; /* used by server in case network dies */

		/* save values incase network dies */
		memcpy(old_input_port_value, input_port_value, num_ports);

		/* receive input from client and merge */
		for (client=0; client < num_clients; client++)
		{
			/* mark default bits */
			for(port=0; port < num_ports; port++)
				input_port_value[port] ^= input_port_default[port];

			net_recv( client, NET_SYNC_INPT, changed_input_port_value, num_ports );

    		for( port=0; port < num_ports; port++ )
			{
				/* merge this clients changes */
				input_port_value[port] |= changed_input_port_value[port]; /* or local and remote changes */
				input_port_value[port] ^= input_port_default[port];       /* toggle changed bits */
			}
		}

		/* now check if clients still exist */
		if ( is_active )
		{
			for (client=0; client < num_clients; client++)  /* send final input to client */
				net_send( client, NET_SYNC_INPT, input_port_value, num_ports );
		}
		else /* net connection is now down, restore our values */
		{
			memcpy(input_port_value, old_input_port_value, num_ports);
		}
	}
    dprintf(("net_input_sync() return\n"))

/* TODO: error checking */
	return 0;
}

/* format packet and pass to os-specific sender */
int net_send( int player, char type, unsigned char *msg, int size )
{
	/* format packet */
	static unsigned char buf[NET_MAX_PACKET];
	int new_size = size + NET_MAX_HEADER;
	int error = 0;
#ifdef NET_DEBUG
	char packet_str[14];
	net_packet_type_to_string( type, packet_str );
    if (type == NET_INIT_GAME && size > 1)
    {
        dprintf(("net_send: %s data(%s), len(%d)\n", packet_str, msg, size))
    }
    else
        dprintf(("net_send: %s, size(%d)\n", packet_str, size ))
#endif
	memset( buf, '\0', NET_MAX_PACKET ); /* clear full buffer */
	memcpy( buf, &type, NET_MAX_HEAD_TYPE );
	memcpy( (buf + NET_MAX_HEAD_TYPE), &size, NET_MAX_HEAD_SIZE );
	memcpy( (buf + NET_MAX_HEADER), msg, size );

	error = osd_net_send( player, buf, &new_size );

	if ( new_size != size + (int)NET_MAX_HEADER )
	{
		printf( "net_send: detected error while attempting to send\n" );
		printf( "net_send:   newsize:%d size+header:%d\n",new_size,size+NET_MAX_HEADER );
		error = 1;
	}

	return error;
}

/* receive from os-specific sender and check packet format */
int net_recv( int player, char type, unsigned char *msg, int size )
{
	/* decode packet and check against expected*/
	static unsigned char buf[NET_MAX_PACKET];
	int error;
	char new_type;
	int new_size;

	/* clear buffer before receiving */
net_recv_again:
	error = 0;
	new_type = 0;
	new_size = size + NET_MAX_HEADER;
	memset( buf, 0, NET_MAX_PACKET );
	error = osd_net_recv( player, buf, &new_size );

#ifdef NET_DEBUG
    {
        char packet_str[14];
	    net_packet_type_to_string( type, packet_str );
        dprintf(("net_recv: %s, size(%d)\n", packet_str, new_size ))
    }
#endif

    memcpy( &new_type, buf, NET_MAX_HEAD_TYPE );

	/* also check packet type to see that we got what we wanted*/
	if ( new_type != type )
	{
		/* if we initiated a quit, we don't care about what we received */
		/* TODO: check type against a mask to see if we care instead of long if block... */
		if ( ( type == NET_QUIT_QUIT ) || ( type == NET_QUIT_ABRT ) || ( type == NET_QUIT_OKAY ) )
		{
			return 0; /* leave buffer alone */
		}

		/* error condition, see if we can handle new_type and recover */
		error = 1;
		switch ( new_type )
		{
			/* check for reset */
			case NET_SYNC_INIT:
			case NET_SYNC_REDO:
				/* TODO: reset */
				return 0;

			/* check for pause */
			case NET_SYNC_PAWS:
				/* TODO: wait for another paws */
				goto net_recv_again;

			/* quit conditions */
			case NET_QUIT_QUIT:
			case NET_QUIT_ABRT:
			case NET_QUIT_OKAY:
				printf( "we've detected a quit or abort\n" );
				/* leave buffer alone - do not use merged buffer*/
				return net_remove_player( player );

			/* this should never happen - but does */
			/*TODO: handle this instead of dropping player */
			case NET_0NOT_USED:
				printf( "bad data received - aborting player\n" );
                {
                    char expected_packet_str[14];
		    		char received_packet_str[14];
	    			net_packet_type_to_string( type, expected_packet_str );
    				net_packet_type_to_string( new_type, received_packet_str );
                    dprintf(("net_recv: received packet:%s not expected:%s\n", received_packet_str, expected_packet_str ))
                }
				/* leave buffer alone - do not use merged buffer*/
				return net_remove_player( player );

			default:
			{
				char expected_packet_str[14];
				char received_packet_str[14];
				net_packet_type_to_string( type, expected_packet_str );
				net_packet_type_to_string( new_type, received_packet_str );
                printf( "net_recv: received packet:%s not expected:%s\n", received_packet_str, expected_packet_str );
				/* leave buffer alone - do not use merged buffer*/
				return error;
			}
		}
	}

	/* copy received buffer back into expected location */
	memcpy( msg, (buf+NET_MAX_HEADER), (new_size-NET_MAX_HEADER) );

	return error;

}

int net_add_player()
{
    dprintf(("net_add_player()\n"))
	/* TODO: make this also work for client for consistency */

	/* check if osd adding a client failed */
	if ( ( num_clients = osd_net_add_player() ) < 0 )
    {
        dprintf(("osd_net_add_player() - FAILED\n"))
		return -1;
    }

	/* if it succeeded then we check the version */
	if ( net_vers_check( num_clients-1 ) )
    {
        dprintf(("net_vers_check() - FAILED\n"))
		return -1;
    }

	/* TODO: need to shift player info around (currently just sockets - eventually input assignments too ) */
	/* TODO: setup player names and such */
    dprintf(("net_add_player() num_clients = %d\n", num_clients))
	return num_clients; /* we return the new total number of clients */
}

int net_remove_player( int player )
{
    dprintf(("net_remove_player(%d)\n", player ))

	/* TODO: need to shift player info around (currently just sockets - eventually input assignments too ) */
	if ( is_client )
	{
		net_exit( NET_QUIT_OKAY );
		return 0;
	}
	else
	{
		int new_players = osd_net_remove_player( player );
		/* TODO: need to allow server to remain active */
		if ( new_players == 0 )
			net_exit( NET_QUIT_OKAY );

		return new_players;
	}
}

/* Only returns true if the net is initialized and we are playing a game */
int net_active()
{
#ifdef NET_DEBUG
    static int last = 0;
    static int iactive = 0;
    static int igame = 0;

    if (iactive != is_active || igame != in_game)
    {
        iactive = is_active;
        igame   = in_game;
        last = (iactive && igame);
        dprintf(("net_active() = %s is_active = %s, in_game = %s\n",
            (last)    ? "TRUE" : "FALSE",
            (iactive) ? "TRUE" : "FALSE",
            (igame)   ? "TRUE" : "FALSE"))
    }
#endif
	return (is_active && in_game);
}

static void net_packet_type_to_string( char type, char packet_type_str[] )
{
	switch ( type )
	{
		case NET_INIT_INIT:
			strcpy( packet_type_str, "NET_INIT_INIT" );
			break;
		case NET_INIT_VERS:
			strcpy( packet_type_str, "NET_INIT_VERS" );
			break;
		case NET_INIT_GAME:
			strcpy( packet_type_str, "NET_INIT_GAME" );
			break;
		case NET_SYNC_INIT:
			strcpy( packet_type_str, "NET_SYNC_INIT" );
			break;
		case NET_SYNC_INPT:
			strcpy( packet_type_str, "NET_SYNC_INPT" );
			break;
		case NET_SYNC_ANLG:
			strcpy( packet_type_str, "NET_SYNC_ANLG" );
			break;
		case NET_SYNC_USER:
			strcpy( packet_type_str, "NET_SYNC_USER" );
			break;
		case NET_SYNC_BOOM:
			strcpy( packet_type_str, "NET_SYNC_BOOM" );
			break;
		case NET_SYNC_REDO:
			strcpy( packet_type_str, "NET_SYNC_REDO" );
			break;
		case NET_QUIT_QUIT:
			strcpy( packet_type_str, "NET_QUIT_QUIT" );
			break;
		case NET_QUIT_ABRT:
			strcpy( packet_type_str, "NET_QUIT_ABRT" );
			break;
		case NET_QUIT_OKAY:
			strcpy( packet_type_str, "NET_QUIT_OKAY" );
			break;
		case NET_CHAT_INIT:
			strcpy( packet_type_str, "NET_CHAT_INIT" );
			break;
		case NET_CHAT_CHAT:
			strcpy( packet_type_str, "NET_CHAT_CHAT" );
			break;
		case NET_CHAT_QUIT:
			strcpy( packet_type_str, "NET_CHAT_QUIT" );
			break;
		case NET_0NOT_USED:
			strcpy( packet_type_str, "NET_0NOT_USED" );
			break;
		default:
			strcpy( packet_type_str, "UNRECOGNIZED!" );
			break;
	}
}

int net_chat_send( unsigned char *msg, int *size )
{
	net_send( 0, NET_CHAT_CHAT, msg, *size );
	return 0;
}

int net_chat_recv(int player, unsigned char *msg, int *size )
{
	net_recv( player, NET_CHAT_CHAT, msg, *size );
	return 0;
}

#endif /* MAME_NET */
