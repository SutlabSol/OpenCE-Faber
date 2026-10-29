/*
HALO_PORT_LIMITS.H

Multiplayer session limits of the native builds (Windows, Linux, Android),
force-included by halo_linux_prefix.h and halo_windows_prefix.h.
*/

#ifndef __HALO_PORT_LIMITS_H
#define __HALO_PORT_LIMITS_H

/* ---------- session limits */

#define HALO_PORT_MAXIMUM_NETWORK_PLAYERS 128
#define HALO_PORT_MAXIMUM_NETWORK_MACHINES 128
#define HALO_PORT_FD_SETSIZE 256

/* ---------- memory capacity for these limits (game state and pools) */

#include "halo_port_capacity.h"

/* ---------- struct network_game layout */

#define HALO_PORT_NETWORK_MACHINE_SIZE 0x44
#define HALO_PORT_NETWORK_PLAYER_SIZE 0x20
#define HALO_PORT_NETWORK_GAME_MACHINES_OFFSET 0x114
#define HALO_PORT_NETWORK_GAME_PLAYER_COUNT_OFFSET \
	(HALO_PORT_NETWORK_GAME_MACHINES_OFFSET + HALO_PORT_MAXIMUM_NETWORK_MACHINES * HALO_PORT_NETWORK_MACHINE_SIZE)
#define HALO_PORT_NETWORK_GAME_PLAYERS_OFFSET (HALO_PORT_NETWORK_GAME_PLAYER_COUNT_OFFSET + 2)
#define HALO_PORT_NETWORK_GAME_PLAYERS_END \
	(HALO_PORT_NETWORK_GAME_PLAYERS_OFFSET + HALO_PORT_MAXIMUM_NETWORK_PLAYERS * HALO_PORT_NETWORK_PLAYER_SIZE)
#define HALO_PORT_NETWORK_GAME_RANDOM_SEED_OFFSET (HALO_PORT_NETWORK_GAME_PLAYERS_END + 2)
/* (then the gametype's PC + OpenCE + Firefight options, struct game_variant_options: 0x58 bytes;
stock OpenCE is 0x2C bytes) */
#define HALO_PORT_NETWORK_GAME_VARIANT_OPTIONS_OFFSET (HALO_PORT_NETWORK_GAME_PLAYERS_END + 0xA)
#define HALO_PORT_STOCK_NETWORK_GAME_LOCAL_DATA_OFFSET (HALO_PORT_NETWORK_GAME_PLAYERS_END + 0x36)
#define HALO_PORT_STOCK_NETWORK_GAME_SIZE (HALO_PORT_NETWORK_GAME_PLAYERS_END + 0x3A)
#define HALO_PORT_NETWORK_GAME_LOCAL_DATA_OFFSET (HALO_PORT_NETWORK_GAME_PLAYERS_END + 0x62)
#define HALO_PORT_NETWORK_GAME_SIZE (HALO_PORT_NETWORK_GAME_PLAYERS_END + 0x66)

/* ---------- system link protocol */

#define HALO_PORT_NETWORK_GAME_MESSAGE_VERSION 2

/* The native builds' network code has a version of its own (an unsigned
16-bit number): machines of different versions cannot play together, and a
client does not join a host of another version, but tells the player which
is newer (network_client_manager.c). A host advertises it, with its netcode,
in its game's advertisement's reserved bytes (network_server_message_handler.c),
which hosts built before there was a version send as zeros: version 0.
Raise it with any change to what the machines send each other. */
#define HALO_PORT_NETWORK_VERSION 39
/* ... the advertisement's reserved bytes: the version (a little-endian word),
then flags, then the game's kind (an OpenCE gametype's mode, and Hardcore:
port/linux/game/game_modes.h's game_modes_kind), for the server browser */
#define HALO_PORT_ADVERTISED_VERSION_OFFSET 0
#define HALO_PORT_ADVERTISED_FLAGS_OFFSET 2
#define HALO_PORT_ADVERTISED_KIND_OFFSET 3
/* ... the host plays the distributed netcode (always, since the lockstep
netcode was removed; hosts of version 4 built before then may not) */
#define HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG 0x01
#define HALO_PORT_ADVERTISED_IN_PROGRESS_FLAG 0x02
#define HALO_PORT_ADVERTISED_DEDICATED_FLAG 0x04

#define HALO_PORT_ADVERTISED_RANDOMIZER_FLAG 0x20
#define HALO_PORT_ADVERTISED_RANDOMIZER_SETUP_OFFSET 3
#define HALO_PORT_ADVERTISED_RANDOMIZER_ROLL_OFFSET 6
#define HALO_PORT_ADVERTISED_RANDOMIZER_WEAPONS_OFFSET 10
#define HALO_PORT_ADVERTISED_RANDOMIZER_SETUP_SIZE 12

#define HALO_PORT_ADVERTISED_SKULLS_FLAG 0x08
#define HALO_PORT_ADVERTISED_SKULLS_OFFSET 15
#define HALO_PORT_ADVERTISED_SKULLS_SIZE 8

#define HALO_PORT_ADVERTISED_RANDOMIZER_CRAZY_OFFSET 23
#define HALO_PORT_ADVERTISED_RANDOMIZER_CRAZY_SET 0x08
#define HALO_PORT_ADVERTISED_SKULL_DROPS_FLAG 0x10
#define HALO_PORT_ADVERTISED_TAG_SHARE_FLAG 0x40

#define HALO_PORT_ADVERTISED_BASE_VERSION_OFFSET 24
#define HALO_PORT_MOD_NETWORK_VERSION 5
#define HALO_PORT_MODDED_VERSION_MARK 0xFF00
#define HALO_PORT_ADVERTISED_MODDED_VERSION (HALO_PORT_MODDED_VERSION_MARK | HALO_PORT_MOD_NETWORK_VERSION)

#define HALO_PORT_MAXIMUM_NETWORK_MESSAGE_SIZE 0x1000
#define HALO_PORT_NETWORK_PACKET_SIZE 0x1100
/* the game settings record (HALO_PORT_NETWORK_GAME_SIZE, 13,136 bytes at 128
machines and players) does not fit one message; it is sent in pieces of
this many bytes (4 pieces), each 3,594 bytes on the wire */
#define HALO_PORT_NETWORK_GAME_SETTINGS_FRAGMENT_SIZE 0xE00

#endif /* __HALO_PORT_LIMITS_H */
