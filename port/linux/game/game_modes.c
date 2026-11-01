/*
GAME_MODES.C

OpenCE's gametypes: SWAT, Infection and Gun Game, modes of Slayer, and
Hardcore, rules over any game engine's. Which a gametype plays is in its PC
options (game_engine.h's struct game_variant_options, from mode on), which a
saved gametype keeps (playlist_profile.c), the network game carries to every
machine (network_game_manager.h) and the game plays by
(game_variant_options_get).

- SWAT: no shields, and a shot to the head of any weapon kills (damage.c).
  Its built-in gametype starts everyone with a pistol and an assault rifle,
  with no weapons on the map, no grenades and no motion sensor.
- Infection (infection.c): on teams. Humans are red, zombies blue; the host
  chooses the first zombies after a countdown, a human killed joins them, and
  the zombies win when no human is left, the humans when the time runs out.
- Gun Game (gun_game.c): free for all. Each kill gives the killer the next
  weapon of a list; a kill with the last wins.
- Hardcore: no motion sensor, no aim assist, and a kill restores the
  killer's health and shields.

The built-in gametypes (game_modes_gametype) are written with the Xbox's
default ones, after them, and the Select Gametype screen lists them in a bank
of their own (menu_functions.c's OPENCE).
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_engine_playlist.h"
#include "game/players.h"
#include "networking/network_game_globals.h"
#include "objects/objects.h"
#include "units/units.h"
#include "game_modes.h"
#include "gun_game.h"
#include "infection.h"

#include <string.h>

/* game_engine.c's values of the variant's (its enums goal_radar,
game_engine_weapons and game_engine_vehicles) */
enum
{
	GOAL_RADAR_NAV_POINT = 1,
	WEAPON_SET_NO_GRENADES = 10,
	VEHICLE_SET_NONE = 1
};

/* Infection's options when a gametype has none (game_modes_options_sanitize) */
enum
{
	INFECTION_DEFAULT_INITIAL_ZOMBIES = 1,
	INFECTION_DEFAULT_ZOMBIE_SPEED = 150,
	INFECTION_DEFAULT_SKULL_DAMAGE = 200,
	INFECTION_DEFAULT_RESPAWN_TIME = 5,
	INFECTION_DEFAULT_COUNTDOWN = 10
};

enum
{
	_gametype_swat = 0,
	_gametype_infection,
	_gametype_gun_game,
	_gametype_hardcore_slayer,
	_gametype_hardcore_ctf,
	_gametype_hardcore_oddball,
	_gametype_hardcore_king,
	/* Halo 3 Ranked Playlists & Gametypes */
	/* 1. Lone Wolves (FFA, 6-8 players, Top 50% wins) */
	_gametype_ranked_lw_slayer,
	_gametype_ranked_lw_slayer_pro,
	_gametype_ranked_lw_oddball,
	_gametype_ranked_lw_crazy_king,
	_gametype_ranked_lw_shotty_snipers,
	/* 2. Team Slayer (4v4, first to 50 kills) */
	_gametype_ranked_ts_slayer,
	_gametype_ranked_ts_slayer_pro,
	_gametype_ranked_ts_shotty_snipers,
	/* 3. Team Doubles (2v2 tactical combat) */
	_gametype_ranked_doubles_slayer,
	_gametype_ranked_doubles_2flag_ctf,
	_gametype_ranked_doubles_1flag_ctf,
	_gametype_ranked_doubles_oddball,
	_gametype_ranked_doubles_king,
	/* 4. Team Objective (4v4 objective modes) */
	_gametype_ranked_obj_multiflag_ctf,
	_gametype_ranked_obj_1flag_ctf,
	_gametype_ranked_obj_oddball,
	_gametype_ranked_obj_crazy_king,
	_gametype_ranked_obj_team_king,
	/* 5. MLG (4v4 tournament ruleset) */
	_gametype_ranked_mlg_slayer,
	_gametype_ranked_mlg_ctf_3flag,
	_gametype_ranked_mlg_ctf_5flag,
	_gametype_ranked_mlg_king,
	_gametype_ranked_mlg_oddball,
	/* 6. Team SWAT (4v4 precision mode, first to 50 kills) */
	_gametype_ranked_swat_standard,
	_gametype_ranked_swat_magnums,
	/* 7. Team Snipers (4v4 long-range precision, first to 50 kills) */
	_gametype_ranked_snipers_standard,
	_gametype_ranked_snipers_shotty,
	/* 8. Squad Battle / Ranked BTB (8v8 vehicle combat) */
	_gametype_ranked_squad_btb_slayer,
	_gametype_ranked_squad_multiflag_ctf,
	_gametype_ranked_squad_1flag_ctf,
	NUMBER_OF_GAMETYPES
};

static struct
{
	/* debug.network_test's */
	char const *name;
	/* the gametype lists' */
	wchar_t const *display_name;
	/* the variant's own (11 characters at most: the lobby's and the server
	browser's) */
	wchar_t const *variant_name;
	byte ranked_playlist;
	byte ranked_max_players;
} const game_modes_gametypes[NUMBER_OF_GAMETYPES] =
{
	{ "swat", L"SWAT", L"SWAT", _ranked_playlist_none, 0 },
	{ "infection", L"Infection", L"Infection", _ranked_playlist_none, 0 },
	{ "gun_game", L"Gun Game", L"Gun Game", _ranked_playlist_none, 0 },
	{ "hardcore", L"Hardcore Slayer", L"Hardcore", _ranked_playlist_none, 0 },
	{ "hardcore_ctf", L"Hardcore CTF", L"HC CTF", _ranked_playlist_none, 0 },
	{ "hardcore_oddball", L"Hardcore Oddball", L"HC Oddball", _ranked_playlist_none, 0 },
	{ "hardcore_king", L"Hardcore King", L"HC King", _ranked_playlist_none, 0 },
	/* 1. Lone Wolves (FFA, 8 players) */
	{ "ranked_lw_slayer", L"LW: Slayer", L"LW Slayer", _ranked_playlist_lone_wolves, 8 },
	{ "ranked_lw_slayer_pro", L"LW: Slayer Pro", L"LW Pro", _ranked_playlist_lone_wolves, 8 },
	{ "ranked_lw_oddball", L"LW: Oddball", L"LW Oddball", _ranked_playlist_lone_wolves, 8 },
	{ "ranked_lw_crazy_king", L"LW: Crazy King", L"LW King", _ranked_playlist_lone_wolves, 8 },
	{ "ranked_lw_shotty_snipers", L"LW: Shotty Snipers", L"LW ShotSnip", _ranked_playlist_lone_wolves, 8 },
	/* 2. Team Slayer (4v4, 8 players) */
	{ "ranked_ts_slayer", L"TS: Team Slayer", L"TS Slayer", _ranked_playlist_team_slayer, 8 },
	{ "ranked_ts_slayer_pro", L"TS: Team Slayer Pro", L"TS Pro", _ranked_playlist_team_slayer, 8 },
	{ "ranked_ts_shotty_snipers", L"TS: Shotty Snipers", L"TS ShotSnip", _ranked_playlist_team_slayer, 8 },
	/* 3. Team Doubles (2v2, 4 players) */
	{ "ranked_doubles_slayer", L"Doubles: Team Slayer", L"2v2 Slayer", _ranked_playlist_team_doubles, 4 },
	{ "ranked_doubles_2flag_ctf", L"Doubles: 2 Flag CTF", L"2v2 2-CTF", _ranked_playlist_team_doubles, 4 },
	{ "ranked_doubles_1flag_ctf", L"Doubles: 1 Flag CTF", L"2v2 1-CTF", _ranked_playlist_team_doubles, 4 },
	{ "ranked_doubles_oddball", L"Doubles: Team Oddball", L"2v2 Oddball", _ranked_playlist_team_doubles, 4 },
	{ "ranked_doubles_king", L"Doubles: King of the Hill", L"2v2 King", _ranked_playlist_team_doubles, 4 },
	/* 4. Team Objective (4v4, 8 players) */
	{ "ranked_obj_multiflag_ctf", L"Obj: Multi-Flag CTF", L"Obj 2-CTF", _ranked_playlist_team_objective, 8 },
	{ "ranked_obj_1flag_ctf", L"Obj: 1 Flag CTF", L"Obj 1-CTF", _ranked_playlist_team_objective, 8 },
	{ "ranked_obj_oddball", L"Obj: Team Oddball", L"Obj Oddball", _ranked_playlist_team_objective, 8 },
	{ "ranked_obj_crazy_king", L"Obj: Crazy King", L"Obj CrzKing", _ranked_playlist_team_objective, 8 },
	{ "ranked_obj_team_king", L"Obj: Team King", L"Obj King", _ranked_playlist_team_objective, 8 },
	/* 5. MLG (4v4 Hardcore, 8 players) */
	{ "ranked_mlg_slayer", L"MLG: Team Slayer", L"MLG Slayer", _ranked_playlist_mlg, 8 },
	{ "ranked_mlg_ctf_3flag", L"MLG: CTF 3-Flag", L"MLG 3-CTF", _ranked_playlist_mlg, 8 },
	{ "ranked_mlg_ctf_5flag", L"MLG: CTF 5-Flag", L"MLG 5-CTF", _ranked_playlist_mlg, 8 },
	{ "ranked_mlg_king", L"MLG: King of the Hill", L"MLG King", _ranked_playlist_mlg, 8 },
	{ "ranked_mlg_oddball", L"MLG: Oddball", L"MLG Oddball", _ranked_playlist_mlg, 8 },
	/* 6. Team SWAT (4v4, 8 players) */
	{ "ranked_swat_standard", L"SWAT: Team SWAT", L"SWAT", _ranked_playlist_team_swat, 8 },
	{ "ranked_swat_magnums", L"SWAT: Magnums", L"SWAT Magnum", _ranked_playlist_team_swat, 8 },
	/* 7. Team Snipers (4v4, 8 players) */
	{ "ranked_snipers_standard", L"Snipers: Team Snipers", L"Team Sniper", _ranked_playlist_team_snipers, 8 },
	{ "ranked_snipers_shotty", L"Snipers: Shotty Snipers", L"Shot Sniper", _ranked_playlist_team_snipers, 8 },
	/* 8. Squad Battle / Ranked BTB (8v8, 16 players) */
	{ "ranked_squad_btb_slayer", L"Squad: BTB Slayer", L"BTB Slayer", _ranked_playlist_squad_battle, 16 },
	{ "ranked_squad_multiflag_ctf", L"Squad: Multi-Flag CTF", L"BTB 2-CTF", _ranked_playlist_squad_battle, 16 },
	{ "ranked_squad_1flag_ctf", L"Squad: 1 Flag CTF", L"BTB 1-CTF", _ranked_playlist_squad_battle, 16 },
};

/* the kill being recorded is a melee's (game_modes_kill_by_melee) */
static boolean game_modes_melee_kill;

/* ---- the built-in gametypes */

short game_modes_gametype_count(
	void)
{
	return NUMBER_OF_GAMETYPES;
}

wchar_t const *game_modes_gametype_name(
	short index)
{
	return VALID_INDEX(index, NUMBER_OF_GAMETYPES) ? game_modes_gametypes[index].display_name : L"";
}

/* a variant without a motion sensor: no players on it, and a goal (a flag,
the ball, the hill) shown by its nav point instead */
static void game_modes_no_motion_sensor(
	struct game_variant *variant,
	struct game_variant_options *options)
{
	SET_FLAG(variant->universal_variant.flags, _game_variant_draw_object_in_motion_sensor_bit, FALSE);
	variant->universal_variant.goal_radar = GOAL_RADAR_NAV_POINT;
	options->radar_players = _radar_players_none;
}

void game_modes_gametype(
	short index,
	struct game_variant *variant,
	struct game_variant_options *options)
{
	struct game_variant temporary;

	switch (index)
	{
	case _gametype_hardcore_ctf:
	case _gametype_ranked_doubles_2flag_ctf:
	case _gametype_ranked_doubles_1flag_ctf:
	case _gametype_ranked_obj_multiflag_ctf:
	case _gametype_ranked_obj_1flag_ctf:
	case _gametype_ranked_mlg_ctf_3flag:
	case _gametype_ranked_mlg_ctf_5flag:
	case _gametype_ranked_squad_multiflag_ctf:
	case _gametype_ranked_squad_1flag_ctf:
		*variant = *build_game_variant_ctf(&temporary);
		break;
	case _gametype_hardcore_oddball:
	case _gametype_ranked_doubles_oddball:
	case _gametype_ranked_obj_oddball:
	case _gametype_ranked_mlg_oddball:
		*variant = *build_game_variant_team_oddball(&temporary);
		break;
	case _gametype_ranked_lw_oddball:
		*variant = *build_game_variant_oddball(&temporary);
		break;
	case _gametype_hardcore_king:
	case _gametype_ranked_doubles_king:
	case _gametype_ranked_obj_crazy_king:
	case _gametype_ranked_obj_team_king:
	case _gametype_ranked_mlg_king:
		*variant = *build_game_variant_team_king(&temporary);
		break;
	case _gametype_ranked_lw_crazy_king:
		*variant = *build_game_variant_crazy_king(&temporary);
		break;
	case _gametype_gun_game:
	case _gametype_ranked_lw_slayer:
	case _gametype_ranked_lw_slayer_pro:
	case _gametype_ranked_lw_shotty_snipers:
		*variant = *build_game_variant_slayer(&temporary);
		break;
	default:
		*variant = *build_game_variant_team_slayer(&temporary);
		break;
	}
	game_variant_options_default(variant, options);

	switch (index)
	{
	case _gametype_swat:
	case _gametype_ranked_swat_standard:
		options->mode = _game_mode_swat;
		SET_FLAG(variant->universal_variant.flags, _game_variant_no_shields_bit, TRUE);
		variant->universal_variant.weapon_set = WEAPON_SET_NO_GRENADES;
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.respawn_time = 150;
		variant->universal_variant.score_to_win = 50;
		game_modes_no_motion_sensor(variant, options);
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_pistol;
		options->secondary_weapon = _loadout_weapon_assault_rifle;
		options->no_map_weapons = TRUE;
		options->time_limit = 15;
		break;
	case _gametype_ranked_swat_magnums:
		options->mode = _game_mode_swat;
		SET_FLAG(variant->universal_variant.flags, _game_variant_no_shields_bit, TRUE);
		variant->universal_variant.weapon_set = WEAPON_SET_NO_GRENADES;
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.respawn_time = 150;
		variant->universal_variant.score_to_win = 50;
		game_modes_no_motion_sensor(variant, options);
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_pistol;
		options->secondary_weapon = _loadout_weapon_none;
		options->no_map_weapons = TRUE;
		options->time_limit = 15;
		break;
	case _gametype_infection:
		options->mode = _game_mode_infection;
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.respawn_time = INFECTION_DEFAULT_RESPAWN_TIME * TICKS_PER_SECOND;
		variant->universal_variant.suicide_penalty = 0;
		options->time_limit = 5;
		options->friendly_fire = _friendly_fire_off;
		break;
	case _gametype_gun_game:
		options->mode = _game_mode_gun_game;
		variant->universal_variant.weapon_set = WEAPON_SET_NO_GRENADES;
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.respawn_time = 90;
		variant->universal_variant.suicide_penalty = 0;
		options->no_map_weapons = TRUE;
		break;
	/* 1. Lone Wolves (FFA) */
	case _gametype_ranked_lw_slayer:
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 25;
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_assault_rifle;
		options->secondary_weapon = _loadout_weapon_pistol;
		options->time_limit = 12;
		break;
	case _gametype_ranked_lw_slayer_pro:
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 25;
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_pistol;
		options->secondary_weapon = _loadout_weapon_assault_rifle;
		options->time_limit = 12;
		break;
	case _gametype_ranked_lw_oddball:
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 2; /* 2 min (120s) */
		options->time_limit = 10;
		break;
	case _gametype_ranked_lw_crazy_king:
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 2; /* 2 min (120s) */
		variant->game_engine_variant.king.moving_hill = TRUE;
		options->time_limit = 10;
		break;
	case _gametype_ranked_lw_shotty_snipers:
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 25;
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_shotgun;
		options->secondary_weapon = _loadout_weapon_sniper_rifle;
		options->no_map_weapons = TRUE;
		options->time_limit = 12;
		break;
	/* 2. Team Slayer (4v4) */
	case _gametype_ranked_ts_slayer:
		variant->universal_variant.score_to_win = 50;
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_assault_rifle;
		options->secondary_weapon = _loadout_weapon_pistol;
		options->time_limit = 15;
		break;
	case _gametype_ranked_ts_slayer_pro:
		variant->universal_variant.score_to_win = 50;
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_pistol;
		options->secondary_weapon = _loadout_weapon_assault_rifle;
		options->time_limit = 15;
		break;
	case _gametype_ranked_ts_shotty_snipers:
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 50;
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_shotgun;
		options->secondary_weapon = _loadout_weapon_sniper_rifle;
		options->no_map_weapons = TRUE;
		options->time_limit = 15;
		break;
	/* 3. Team Doubles (2v2) */
	case _gametype_ranked_doubles_slayer:
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 25;
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_pistol;
		options->secondary_weapon = _loadout_weapon_assault_rifle;
		options->time_limit = 12;
		break;
	case _gametype_ranked_doubles_2flag_ctf:
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 3;
		variant->game_engine_variant.ctf.assault = FALSE;
		variant->game_engine_variant.ctf.flag_at_home_to_score = TRUE;
		variant->game_engine_variant.ctf.flag_must_reset = TRUE;
		options->time_limit = 15;
		break;
	case _gametype_ranked_doubles_1flag_ctf:
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 3;
		variant->game_engine_variant.ctf.assault = TRUE;
		variant->game_engine_variant.ctf.single_flag_time = 180 * TICKS_PER_SECOND;
		options->time_limit = 15;
		break;
	case _gametype_ranked_doubles_oddball:
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 3; /* 3 min (180s) */
		options->time_limit = 12;
		break;
	case _gametype_ranked_doubles_king:
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 2; /* 2 min (120s) */
		variant->game_engine_variant.king.moving_hill = TRUE;
		options->time_limit = 12;
		break;
	/* 4. Team Objective (4v4) */
	case _gametype_ranked_obj_multiflag_ctf:
		variant->universal_variant.score_to_win = 3;
		variant->game_engine_variant.ctf.assault = FALSE;
		variant->game_engine_variant.ctf.flag_at_home_to_score = TRUE;
		options->time_limit = 15;
		break;
	case _gametype_ranked_obj_1flag_ctf:
		variant->universal_variant.score_to_win = 3;
		variant->game_engine_variant.ctf.assault = TRUE;
		variant->game_engine_variant.ctf.single_flag_time = 180 * TICKS_PER_SECOND;
		options->time_limit = 15;
		break;
	case _gametype_ranked_obj_oddball:
		variant->universal_variant.score_to_win = 4; /* 4 min (240s) */
		options->time_limit = 15;
		break;
	case _gametype_ranked_obj_crazy_king:
		variant->universal_variant.score_to_win = 4; /* 4 min (240s) */
		variant->game_engine_variant.king.moving_hill = TRUE;
		options->time_limit = 15;
		break;
	case _gametype_ranked_obj_team_king:
		variant->universal_variant.score_to_win = 3; /* 3 min (180s) */
		variant->game_engine_variant.king.moving_hill = FALSE;
		options->time_limit = 15;
		break;
	/* 5. MLG (4v4 Hardcore Tournament Rules) */
	case _gametype_ranked_mlg_slayer:
		options->hardcore = TRUE;
		options->friendly_fire = _friendly_fire_on;
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 50;
		game_modes_no_motion_sensor(variant, options);
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_pistol;
		options->secondary_weapon = _loadout_weapon_assault_rifle;
		options->time_limit = 15;
		break;
	case _gametype_ranked_mlg_ctf_3flag:
		options->hardcore = TRUE;
		options->friendly_fire = _friendly_fire_on;
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 3;
		variant->game_engine_variant.ctf.assault = FALSE;
		variant->game_engine_variant.ctf.flag_must_reset = TRUE;
		variant->game_engine_variant.ctf.flag_at_home_to_score = TRUE;
		game_modes_no_motion_sensor(variant, options);
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_pistol;
		options->secondary_weapon = _loadout_weapon_assault_rifle;
		options->time_limit = 15;
		break;
	case _gametype_ranked_mlg_ctf_5flag:
		options->hardcore = TRUE;
		options->friendly_fire = _friendly_fire_on;
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 5;
		variant->game_engine_variant.ctf.assault = FALSE;
		variant->game_engine_variant.ctf.flag_must_reset = TRUE;
		variant->game_engine_variant.ctf.flag_at_home_to_score = TRUE;
		game_modes_no_motion_sensor(variant, options);
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_pistol;
		options->secondary_weapon = _loadout_weapon_assault_rifle;
		options->time_limit = 20;
		break;
	case _gametype_ranked_mlg_king:
		options->hardcore = TRUE;
		options->friendly_fire = _friendly_fire_on;
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 4; /* 4 min (240s) */
		variant->game_engine_variant.king.moving_hill = TRUE;
		game_modes_no_motion_sensor(variant, options);
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_pistol;
		options->secondary_weapon = _loadout_weapon_assault_rifle;
		options->time_limit = 15;
		break;
	case _gametype_ranked_mlg_oddball:
		options->hardcore = TRUE;
		options->friendly_fire = _friendly_fire_on;
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 4; /* 4 min (240s) */
		game_modes_no_motion_sensor(variant, options);
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_pistol;
		options->secondary_weapon = _loadout_weapon_assault_rifle;
		options->time_limit = 15;
		break;
	/* 7. Team Snipers (4v4) */
	case _gametype_ranked_snipers_standard:
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 50;
		game_modes_no_motion_sensor(variant, options);
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_sniper_rifle;
		options->secondary_weapon = _loadout_weapon_pistol;
		options->no_map_weapons = TRUE;
		options->time_limit = 15;
		break;
	case _gametype_ranked_snipers_shotty:
		variant->universal_variant.vehicle_set = VEHICLE_SET_NONE;
		variant->universal_variant.score_to_win = 50;
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_sniper_rifle;
		options->secondary_weapon = _loadout_weapon_shotgun;
		options->no_map_weapons = TRUE;
		options->time_limit = 15;
		break;
	/* 8. Squad Battle / Ranked BTB (8v8 Vehicle Combat) */
	case _gametype_ranked_squad_btb_slayer:
		variant->universal_variant.score_to_win = 100;
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_pistol;
		options->secondary_weapon = _loadout_weapon_assault_rifle;
		options->vehicle_respawn_time = 90;
		options->time_limit = 20;
		break;
	case _gametype_ranked_squad_multiflag_ctf:
		variant->universal_variant.score_to_win = 3;
		variant->game_engine_variant.ctf.assault = FALSE;
		variant->game_engine_variant.ctf.flag_at_home_to_score = TRUE;
		options->vehicle_respawn_time = 90;
		options->time_limit = 20;
		break;
	case _gametype_ranked_squad_1flag_ctf:
		variant->universal_variant.score_to_win = 3;
		variant->game_engine_variant.ctf.assault = TRUE;
		variant->game_engine_variant.ctf.single_flag_time = 180 * TICKS_PER_SECOND;
		options->vehicle_respawn_time = 90;
		options->time_limit = 20;
		break;
	default:
		options->hardcore = TRUE;
		options->friendly_fire = _friendly_fire_on;
		game_modes_no_motion_sensor(variant, options);
		break;
	}
	/* (each team's vehicles the variant's, as game_variant_options_default
	gives them; Squad Battle / Ranked BTB enables all PC map vehicles) */
	if (VALID_INDEX(index, NUMBER_OF_GAMETYPES) &&
		game_modes_gametypes[index].ranked_playlist == _ranked_playlist_squad_battle)
	{
		options->vehicle_set[0] = options->vehicle_set[1] = VARIANT_VEHICLE_SET_PC;
	}
	else
	{
		options->vehicle_set[0] = options->vehicle_set[1] = (byte)variant->universal_variant.vehicle_set;
	}
	if (VALID_INDEX(index, NUMBER_OF_GAMETYPES))
	{
		options->ranked_playlist = game_modes_gametypes[index].ranked_playlist;
		options->ranked_max_players = game_modes_gametypes[index].ranked_max_players;
	}
	game_modes_options_sanitize(options);
	csmemset(variant->human_readable_game_description, 0, sizeof(variant->human_readable_game_description));
	if (VALID_INDEX(index, NUMBER_OF_GAMETYPES))
	{
		ustrncpy(variant->human_readable_game_description, game_modes_gametypes[index].variant_name,
			NUMBEROF(variant->human_readable_game_description) - 1);
	}
}

boolean game_modes_gametype_by_name(
	char const *name,
	struct game_variant *variant,
	struct game_variant_options *options)
{
	short index;

	for (index = 0; index < NUMBER_OF_GAMETYPES; index++)
	{
		if (!strcmp(name, game_modes_gametypes[index].name))
		{
			game_modes_gametype(index, variant, options);
			return TRUE;
		}
	}
	return FALSE;
}

/* ---- a gametype's options */

void game_modes_options_default(
	struct game_variant_options *options)
{
	options->mode = _game_mode_none;
	options->hardcore = FALSE;
	options->infection_initial_zombies = INFECTION_DEFAULT_INITIAL_ZOMBIES;
	options->infection_zombie_speed = INFECTION_DEFAULT_ZOMBIE_SPEED;
	options->infection_skull_damage = INFECTION_DEFAULT_SKULL_DAMAGE;
	options->infection_respawn_time = INFECTION_DEFAULT_RESPAWN_TIME;
	options->infection_countdown = INFECTION_DEFAULT_COUNTDOWN;
	options->infection_zombie_shields = TRUE;
	options->gun_game_order = _gun_game_order_strongest_first;
	options->gun_game_melee_demotes = TRUE;
	options->ranked_playlist = _ranked_playlist_none;
	options->ranked_max_players = 0;
	csmemset(options->reserved, 0, sizeof(options->reserved));
}

/* a byte option within its range, else its default */
static byte game_modes_option(
	byte value,
	byte minimum,
	byte maximum,
	byte default_value)
{
	return value >= minimum && value <= maximum ? value : default_value;
}

void game_modes_options_sanitize(
	struct game_variant_options *options)
{
	if (options->mode >= NUMBER_OF_GAME_MODES)
		options->mode = _game_mode_none;
	options->hardcore = options->hardcore != FALSE;
	options->force_team_spawns = options->force_team_spawns != FALSE;
	options->infection_initial_zombies = game_modes_option(options->infection_initial_zombies, 1, 16,
		INFECTION_DEFAULT_INITIAL_ZOMBIES);
	options->infection_zombie_speed = game_modes_option(options->infection_zombie_speed, 100, 200,
		INFECTION_DEFAULT_ZOMBIE_SPEED);
	options->infection_skull_damage = game_modes_option(options->infection_skull_damage, 100, 250,
		INFECTION_DEFAULT_SKULL_DAMAGE);
	options->infection_respawn_time = game_modes_option(options->infection_respawn_time, 1, 30,
		INFECTION_DEFAULT_RESPAWN_TIME);
	options->infection_countdown = game_modes_option(options->infection_countdown, 0, 30,
		INFECTION_DEFAULT_COUNTDOWN);
	options->infection_zombie_shields = options->infection_zombie_shields != FALSE;
	if (options->gun_game_order >= NUMBER_OF_GUN_GAME_ORDERS)
		options->gun_game_order = _gun_game_order_strongest_first;
	options->gun_game_melee_demotes = options->gun_game_melee_demotes != FALSE;
	if (options->ranked_playlist >= NUMBER_OF_RANKED_PLAYLISTS)
	{
		options->ranked_playlist = _ranked_playlist_none;
		options->ranked_max_players = 0;
	}
	else if (options->ranked_playlist == _ranked_playlist_none)
	{
		options->ranked_max_players = 0;
	}
	else
	{
		options->ranked_max_players = game_modes_option(options->ranked_max_players, 2, 16, 8);
	}
	options->force_team_spawns = options->force_team_spawns != FALSE;
	csmemset(options->reserved, 0, sizeof(options->reserved));
}

short game_modes_variant_mode(
	struct game_variant const *variant,
	struct game_variant_options const *options)
{
	if (!variant || !options || variant->game_engine_index != game_engine_slayer)
		return _game_mode_none;
	switch (options->mode)
	{
	case _game_mode_swat:
		return _game_mode_swat;
	case _game_mode_infection:
		return variant->universal_variant.teams ? _game_mode_infection : _game_mode_none;
	case _game_mode_gun_game:
		return !variant->universal_variant.teams ? _game_mode_gun_game : _game_mode_none;
	}
	return _game_mode_none;
}

byte game_modes_kind(
	struct game_variant const *variant,
	struct game_variant_options const *options)
{
	return (byte)(game_modes_variant_mode(variant, options) |
		(options && options->hardcore ? GAME_MODES_KIND_HARDCORE : 0));
}

wchar_t const *game_modes_kind_name(
	long game_engine_index,
	byte kind,
	boolean abbreviated)
{
	/* (each kind's name, and its name with Hardcore's, in full and short) */
	static wchar_t const *const mode_names[NUMBER_OF_GAME_MODES][2][2] =
	{
		{ { NULL, NULL }, { NULL, NULL } },
		{ { L"SWAT", L"SWAT" }, { L"HARDCORE SWAT", L"HC SWAT" } },
		{ { L"INFECTION", L"INFECTION" }, { L"HARDCORE INFECTION", L"HC INFECTION" } },
		{ { L"GUN GAME", L"GUN GAME" }, { L"HARDCORE GUN GAME", L"HC GUN GAME" } },
	};
	static wchar_t const *const hardcore_names[][2] =
	{
		{ NULL, NULL },
		{ L"HARDCORE CTF", L"HC CTF" },
		{ L"HARDCORE SLAYER", L"HC SLAYER" },
		{ L"HARDCORE ODDBALL", L"HC ODDBALL" },
		{ L"HARDCORE KING", L"HC KING" },
		{ L"HARDCORE RACE", L"HC RACE" },
	};
	boolean hardcore = (kind & GAME_MODES_KIND_HARDCORE) != 0;
	short mode = (short)(kind & ~GAME_MODES_KIND_HARDCORE);

	/* (another machine's: a mode of Slayer only, and one this build knows) */
	if (game_engine_index == game_engine_slayer && mode > _game_mode_none && mode < NUMBER_OF_GAME_MODES)
		return mode_names[mode][hardcore][abbreviated != FALSE];
	if (!hardcore || game_engine_index <= game_engine_none || game_engine_index >= (long)NUMBEROF(hardcore_names))
		return NULL;
	return hardcore_names[game_engine_index][abbreviated != FALSE];
}

wchar_t const *game_modes_variant_kind_name(
	struct game_variant const *variant,
	struct game_variant_options const *options)
{
	return variant && options ?
		game_modes_kind_name(variant->game_engine_index, game_modes_kind(variant, options), FALSE) : NULL;
}

boolean game_modes_variant_is_ranked(
	struct game_variant const *variant,
	struct game_variant_options const *options)
{
	short default_index;

	if (options && options->ranked_playlist > _ranked_playlist_none &&
		options->ranked_playlist < NUMBER_OF_RANKED_PLAYLISTS)
	{
		return TRUE;
	}
	if (!variant || !(variant->flags & 1))
		return FALSE;
	default_index = (short)((word)variant->flags >> 8) - GAME_MODES_FIRST_DEFAULT_PLAYLIST_PROFILE;
	return VALID_INDEX(default_index, NUMBER_OF_GAMETYPES) &&
		game_modes_gametypes[default_index].ranked_playlist > _ranked_playlist_none;
}

wchar_t const *game_modes_ranked_playlist_name(
	byte playlist)
{
	switch (playlist)
	{
	case _ranked_playlist_lone_wolves:
		return L"Lone Wolves";
	case _ranked_playlist_team_slayer:
		return L"Team Slayer";
	case _ranked_playlist_team_doubles:
		return L"Team Doubles";
	case _ranked_playlist_team_objective:
		return L"Team Objective";
	case _ranked_playlist_mlg:
		return L"MLG";
	case _ranked_playlist_team_swat:
		return L"Team SWAT";
	case _ranked_playlist_team_snipers:
		return L"Team Snipers";
	case _ranked_playlist_squad_battle:
		return L"Squad Battle";
	default:
		return NULL;
	}
}

wchar_t const *game_modes_ranked_playlist_format(
	byte playlist)
{
	switch (playlist)
	{
	case _ranked_playlist_lone_wolves:
		return L"FFA (Top 50% Wins)";
	case _ranked_playlist_team_doubles:
		return L"2v2 Teams";
	case _ranked_playlist_squad_battle:
		return L"8v8 Teams";
	case _ranked_playlist_team_slayer:
	case _ranked_playlist_team_objective:
	case _ranked_playlist_mlg:
	case _ranked_playlist_team_swat:
	case _ranked_playlist_team_snipers:
		return L"4v4 Teams";
	default:
		return NULL;
	}
}

boolean game_modes_variant_is_opence(
	struct game_variant const *variant,
	struct game_variant_options const *options)
{
	if (game_modes_variant_is_ranked(variant, options))
		return FALSE;
	return options && (options->hardcore || game_modes_variant_mode(variant, options) != _game_mode_none);
}

/* ---- the game being played */

short game_modes_mode(
	void)
{
	if (!game_engine_running())
		return _game_mode_none;
	return game_modes_variant_mode(game_engine_get_variant(), game_variant_options_get());
}

boolean game_modes_hardcore(
	void)
{
	return game_engine_running() && game_variant_options_get()->hardcore;
}

struct game_engine *game_modes_engine(
	struct game_engine *engine)
{
	switch (game_modes_mode())
	{
	case _game_mode_swat:
		/* (SWAT is played without shields, whatever its variant's) */
		SET_FLAG(game_engine_get_variant()->universal_variant.flags, _game_variant_no_shields_bit, TRUE);
		break;
	case _game_mode_infection:
		return infection_engine_get(engine);
	case _game_mode_gun_game:
		return gun_game_engine_get(engine);
	}
	return engine;
}

void game_modes_postspawn(
	long player_index)
{
	switch (game_modes_mode())
	{
	case _game_mode_infection:
		infection_postspawn(player_index);
		break;
	case _game_mode_gun_game:
		gun_game_postspawn(player_index);
		break;
	}
}

void game_modes_player_killed(
	long killing_player_index,
	long dead_player_index,
	boolean friendly_fire)
{
	struct player_datum *killer;
	struct unit_datum *unit;

	/* Hardcore: the host restores the killer's health, and their shields
	unless an overshield has them past full */
	if (!game_modes_hardcore() || network_game_distributed_client() || friendly_fire ||
		killing_player_index == NONE || killing_player_index == dead_player_index)
	{
		return;
	}
	killer = player_try_and_get(killing_player_index);
	if (!killer || killer->unit_index == NONE)
		return;
	unit = unit_try_and_get(killer->unit_index);
	if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		return;
	unit->object.body_vitality = 1.0f;
	if (unit->object.maximum_shield_vitality > 0.0f && unit->object.shield_vitality < 1.0f)
	{
		unit->object.shield_vitality = 1.0f;
		SET_FLAG(unit->object.damage_flags, _object_shield_depleted_bit, FALSE);
		SET_FLAG(unit->object.damage_flags, _object_shield_charging_bit, FALSE);
	}
}

void game_modes_kill_by_melee(
	boolean melee)
{
	game_modes_melee_kill = melee;
}

boolean game_modes_killed_by_melee(
	void)
{
	return game_modes_melee_kill;
}

boolean game_modes_headshot_kills(
	long object_index,
	boolean shot)
{
	struct unit_datum *unit;

	if (!shot || game_modes_mode() != _game_mode_swat)
		return FALSE;
	unit = (struct unit_datum *)object_try_and_get_and_verify_type(object_index, _object_mask_biped);
	return unit && unit->unit.player_index != NONE;
}

boolean game_modes_motion_sensor_hidden(
	void)
{
	return game_modes_hardcore();
}

boolean game_modes_aim_assist_off(
	void)
{
	return game_modes_hardcore();
}
