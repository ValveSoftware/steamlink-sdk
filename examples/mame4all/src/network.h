/***************************************************************************

  network.h

 ***************************************************************************/
#ifdef MAME_NET

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include "osdepend.h"

#define NET_ERROR         (-1)

#define NET_SPECTATOR      -1
#define NET_MAX_PLAYERS     8

//TODO: figure out where to put these options if anywhere
#define NET_DEF_LAN_TIMEOUT 500000  /* lan default timeout value in us (microseconds) */
#define NET_DEF_DUP_TIMEOUT 1000000 /* dial-up default timeout value in us (microseconds) */
/* see below for protocol defines */

//TODO: move this stuff to TCPIP.h as it is specific to TCP/IP implementation
#define NET_ID_PLAYER_CLIENT 0
#define NET_ID_PLAYER_SERVER 1
#define NET_ID_HEADLESS_SERVER 2
#define NET_ID_SPECTATOR 3
#define NET_TEXT_SPECTATOR "Spectator"
#define NET_TEXT_PLAYER_CLIENT "Player"
#define NET_TEXT_PLAYER_SERVER "Player/Server"
#define NET_TEXT_HEADLESS_SERVER "Headless server"

#define NUM_UI_KEYS  4
/* UI Keys to synchronize:
    0 = KEYCODE_FAST_EXIT
	1 = KEYCODE_RESET_MACHINE
	2 = KEYCODE_PAUSE
	3 = KEYCODE_UNPAUSE
*/


int net_init();
int net_exit( char type );
int net_game_init();
int net_game_exit();
int net_vers_check( int player );
int net_game_check( int player );
int net_sync( char type );
int net_input_sync( unsigned char input_port_value[], unsigned char input_port_default[], int num_ports );
int net_analog_sync( unsigned char input_port_value[], int port, int analog_player_port[], int default_player );
int net_send( int player, char type, unsigned char *msg, int size );
int net_recv( int player, char type, unsigned char *msg, int size );
int net_add_player();
int net_remove_player( int player );
int net_active();
int net_chat_send( unsigned char *buf, int *size );
int net_chat_recv(int player, unsigned char *buf, int *size );

/**************************************************************************
NetMAME PROTOCOL DEFINITION

Revision History

  Rev1 1998June15 Peter Eberhardy with feedback from Michael Adcock
       Preliminary spec identifying most synchronous mode commands.
  Rev2 1998June30 Peter Eberhardy
       Revised to begin support for asynchronous mode.
  Rev3 1998July08 Peter Eberhardy
       Implemented asynchronous (joining/leaving chat) mode.
  Rev4 1998July31 Michael Adcock
       Implemented analog control synchronizing.
  Rev5 1998August09 Michael Adcock
	   Figured out the first generation chat dialog.
  Rev6 1998September7 Peter Eberhardy
	   Implemented first generation virtual key synchronizing.
  Rev7 1998October20 Peter Eberhardy
       Brought NetMAME32 source in sync with .34b4 source base.
       Worked on more chat code.


Introduction

  The protocol is really not all that necessary. All the information
  passed between the server and the remote players should be synchronized.
  So you should always expect the packet you receive except in specific
  circumstances. (Some have termed this a wire protocol)

  Chat mode data can be received asynchronously and is also used when
  entering the configuration/option menus in MAME. When exiting chat mode
  or configuration/option menus the server and remote players resume
  synchronous mode.

  When a packet is received that is not expected the synchronization may
  change modes or an error has been detected and network support will
  attempt to abort, possibly aborting emulation or hanging. (Technically
  hanging or a crash is failed error detection)


Packet format

		+-+-+-----+
		|X|Y| buf |
		+-+-+-----+
	where XYZ is 2 byte header
		X=Packet Type (1 byte)
		Z=buf size (1 byte)
		buf=packet type specific message (NET_MAX_PACKET-NET_MAX_HEADER bytes)


Porting guidelines

  Other TCP/IP implementations should be similar to the Win32 Winsock
  implementation. If time permits I'll port X-MAME as well. Cross-platform
  games should work just fine if these guidelines are followed.

  In general, I advise receiving the full packet data size everytime you
  try to receive data - including synchronous reads. This should afford
  better error handling.

  Non TCP/IP implementations should also be easy to implement. NetMAME
  really doesn't care about the underlying protocol only the NetMAME
  packets. As time permits I'll also look at doing an IPX implementation
  which utilizes broadcasts or a multi-cast TCP/IP implementation.



Function notes

  //TODO: lots more detail here...

  int net_init();
    This functon handles network specific initialization and will need to
	call osd_net_init to handle the OS specific network initialization.

  int net_exit();
  int net_vers_check();
  int net_sync( char type );
  int net_input_sync( unsigned char input_port_value[], unsigned char input_port_default[], int num_ports );
  int net_analog_sync( unsigned char input_port_value[], int port, int analog_player_port[], int default_player );
  int net_send( int player, char type, char reserved, unsigned char *msg, int size );
  int net_recv( int player, char type, char reserved, unsigned char *msg, int size );
  int net_add_player();
  int net_remove_player();
  int net_active();
  int net_chat_send( unsigned char buf[], int *size );
  int net_chat_recv( unsigned char buf[], int *size );

To-Do

  I'd like to implement a generic packet transmission benchmark similar
  to the FPS counter. Currently (rev2) it is unknown how NetMAME will perform
  under conditions with less than 10Mbs Ethernet bandwidth available to
  each node.


References

  Contact Peter (eberhardy@acm.org) or Michael for more information and code comments.

  Peter recommends _UNIX Network Programming_ by W. Richard Stevens as a
  wonderful source for understanding network programming in general and
  specifically TCP/IP programming over Berkeley-style sockets or Winsock.

**************************************************************************/

/*TODO: register mame port number and use a more portable method for NET_MAX_HEAD-_TYPE and _SIZE */
#define NET_MAME_PORT  6263 /* need to register this */
#define NET_MAX_PACKET 256  /* full packet */
#define NET_MAX_HEAD_TYPE sizeof(char) /* needs to be 1 */
#define NET_MAX_HEAD_SIZE sizeof(int)  /* needs to be 1 */
#define NET_MAX_HEADER (NET_MAX_HEAD_TYPE + NET_MAX_HEAD_SIZE)
#define NET_MAX_DATA   (NET_MAX_PACKET-NET_MAX_HEADER) /* leftover for data */

#define NET_BYTE unsigned char /* TODO: remove this...? */

/* Packet header defines */
#define NET_0NOT_USED 0x00 /* zero should be reserved, possible error condition if no data sent */

#define NET_INIT_INIT 0x01 /* Setup connection */
#define NET_INIT_VERS 0x02 /* MAME/network version checking */
	/* char  0: start of mameversion string
	   char 25: start of netversion string */
#define NET_INIT_GAME 0x03 /* Game/ROM version checking */
	/* char  0: start of GameDriver->name */

#define NET_SYNC_INIT 0x10 /* Basic synchronization */
#define NET_SYNC_INPT 0x11 /* Sync basic input      */
#define NET_SYNC_ANLG 0x12 /* Sync analog input     */
#define NET_SYNC_USER 0x13 /* Sync user interface input (F3,F8,F10,F12,TAB,P,etc) */
#define NET_SYNC_BOOM 0x14 /* Sync RAM - reserved for save state restarting and debugging */
#define NET_SYNC_REDO 0x15 /* Machine reset!!!      */
#define NET_SYNC_PAWS 0x16 /* Toggle pause          */

#define NET_QUIT_QUIT 0x20 /* Normal termination    */
#define NET_QUIT_ABRT 0x21 /* Abnormal termination  */
#define NET_QUIT_OKAY 0x22 /* Peer initiated quit (this is actually a placeholder and doesn't get sent) */

#define NET_CHAT_INIT 0x30 /* Enter chat mode       */
#define NET_CHAT_CHAT 0x31 /* Chat exchange         */
#define NET_CHAT_QUIT 0x32 /* Exit chat mode        */

#endif /* __NETWORK_H__ */

#endif /* MAME_NET */
