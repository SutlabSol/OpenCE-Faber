/*
GAME_ENGINE_LIST.C

symbols in this file:
002DE510 0020:
	_game_engines (0000)
*/

/* ---------- headers */

#include "cseries/cseries.h"

#include "game/game_engine.h"
#include "game/game_engine_race.h"
#include "game/game_engine_slayer.h"
#include "game/game_engine_list.h"

/* ---------- constants */

/* ---------- macros */

/* ---------- structures */

/* ---------- prototypes */

/* ---------- globals */

extern struct game_engine stub_engine;
extern struct game_engine king_engine;
extern struct game_engine oddball_engine;
extern struct game_engine ctf_engine;
/* port: register firefight_engine at slot 6 (game_engine_firefight) */
extern struct game_engine firefight_engine;

struct game_engine *game_engines[] =
{
	NULL,
	&ctf_engine,
	&slayer_engine,
	&oddball_engine,
	&king_engine,
	&race_engine,
	&firefight_engine, /* port: game_engine_firefight = 6 */
	&stub_engine,      /* port: game_engine_stub = 7 */
};

/* ---------- public code */

/* ---------- private code */
