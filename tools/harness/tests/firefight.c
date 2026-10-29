/*
FIREFIGHT.C

Asset-free regression tests for the modular Halo: Reach / ODST Firefight engine
(port/linux/include/halo_firefight.h, port/linux/game/firefight.c,
port/linux/game/firefight_spawner.c, port/linux/game/firefight_generators.c,
port/linux/game/firefight_network.c):
- ABI & struct size verification (sizeof(struct game_engine_firefight_variant) == 0x18 <= 0x20)
- All 13 Halo: Reach Skulls, 40-skull bridge, per-set vitality escalation, and exclude mask
- Sets -> Rounds -> Waves -> Bonus Round state machine progression & configurable Next Wave / Prep timers
- Shared team lives pool, 0-lives spectate detection, Last Man Standing, wipe defeat
- Generator Defense damage, shield absorption, lockdown invulnerability, and fail modes
- Kill tracking, 10k bonus life threshold, and network state serialization
- Unit Toggle Validation Guard, Faction Diplomacy, Elite Mode, and Actor Variant classification
*/

#include "harness.h"
#include <math.h>
#include <wchar.h>
#include <string.h>

typedef unsigned short word;
typedef unsigned long dword;

#define TICKS_PER_SECOND 30
#define HALO_PORT_MAXIMUM_NETWORK_PLAYERS 128
#define NUMBEROF(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define PIN(v, l, h) MIN(MAX(v, l), h)
#define VALID_INDEX(i, max) ((i) >= 0 && (i) < (max))
#define DATUM_INDEX_TO_ABSOLUTE_INDEX(d) ((d) & 0xFFFF)
#define csmemset memset
#define csmemcpy memcpy
#define usnprintf swprintf
#define ustrcpy wcscpy
#define ustrncpy wcsncpy

enum
{
	game_engine_none = 0,
	game_engine_ctf,
	game_engine_slayer,
	game_engine_oddball,
	game_engine_king,
	game_engine_race,
	game_engine_firefight,
	NUMBER_OF_GAME_ENGINES,
};

enum get_score_type
{
	_get_score_individual = 0,
	_get_score_team,
};

enum
{
	_object_mask_unit = FLAG(0),
	_object_mask_weapon = FLAG(1),
	_object_dead_bit = 0,
	_game_team_default = 0,
	_game_team_player = 1,
	_game_team_human = 2,
	_game_team_covenant = 3,
	_game_team_flood = 4,
	_game_team_sentinel = 5,
	_friendly_fire_off = 0,
	_game_variant_draw_object_in_motion_sensor_bit = 0,
	_game_variant_allow_friendly_navpoints_bit = 1,
	NUMBER_OF_GAME_DIFFICULTY_LEVELS = 4,
	WEAPON_DEFINITION_TAG = 'weap',
	VEHICLE_DEFINITION_TAG = 'vehi',
	BIPED_DEFINITION_TAG = 'bipd',
	_unit_grenade_human_fragmentation = 0,
	_unit_grenade_covenant_plasma = 1,
};

enum
{
	_skull_iron = 0,
	_skull_black_eye,
	_skull_tough_luck,
	_skull_catch,
	_skull_fog,
	_skull_famine,
	_skull_thunderstorm,
	_skull_tilt,
	_skull_mythic,
	_skull_assassin,
	_skull_blind,
	_skull_cowbell,
	_skull_grunt_birthday_party,
	_skull_iwhbyd,
	_skull_ghost,
	_skull_boom,
	_skull_sputnik,
};

struct game_variant;
struct game_variant_options;

#include "firefight_header.inc"

struct game_engine_universal_variant
{
	boolean teams;
	byte pad0[3];
	long flags;
	short goal_radar;
	short odd_man_out;
	long respawn_time_growth;
	long respawn_time;
	long suicide_penalty;
	short lives;
	short pad1;
	real health;
	long score_to_win;
	short weapon_set;
	short vehicle_set;
};

struct game_variant
{
	wchar_t human_readable_game_description[24];
	long game_engine_index;
	struct game_engine_universal_variant universal_variant;
	union
	{
		byte pad[0x20];
		struct game_engine_firefight_variant firefight;
	} game_engine_variant;
	unsigned long flags;
};

struct game_variant_options
{
	short friendly_fire;
	unsigned long firefight_custom_skulls;
	short firefight_generator_health;
	long firefight_score_life_threshold;
	byte firefight_round_1_skull;
	byte firefight_round_2_skull;
	byte firefight_round_3_skull;
	byte firefight_faction_diplomacy;
	unsigned long firefight_skull_exclude_mask;
	unsigned long firefight_starting_skulls[2];
	unsigned short firefight_unit_toggles;
	byte firefight_set_escalation;
	byte firefight_dropship_mode;
	byte firefight_next_wave_mode;
	byte firefight_player_faction_mode;
	byte firefight_early_vehicle;
	byte firefight_late_vehicle;
	byte firefight_prep_countdown;
	byte firefight_skull_progression_mode;
	byte firefight_player_shields;
	byte firefight_player_speed;
	byte firefight_player_gravity;
	byte firefight_infinite_ammo;
	byte firefight_last_man_boost;
	byte firefight_wave_size_scale;
	byte firefight_boss_wave_mode;
	byte firefight_ai_weapon_sandbox;
	byte firefight_friendly_ai;
	byte firefight_ragdolls;
};

struct player_datum
{
	short identifier;
	boolean quit_out_of_game;
	long unit_index;
	short team_index;
	long respawn_timer;
	real speed_multiplier;
	struct
	{
		short kills[4];
		short assists[4];
		short deaths;
	} statistics;
};

struct object_datum
{
	struct
	{
		short type;
		short owner_team_index;
		unsigned long damage_flags;
		real_point3d position;
		real body_vitality;
		real shield_vitality;
		real maximum_body_vitality;
		real maximum_shield_vitality;
	} object;
};

struct unit_datum
{
	short identifier;
	long definition_index;
	struct
	{
		short type;
		short owner_team_index;
		unsigned long damage_flags;
		real_point3d position;
		real body_vitality;
		real shield_vitality;
		real maximum_body_vitality;
		real maximum_shield_vitality;
	} object;
	struct
	{
		long player_index;
		unsigned long control_flags;
		long weapon_object_indices[4];
		byte grenade_counts[2];
	} unit;
};

struct weapon_magazine_definition
{
	short rounds_total_maximum;
	short rounds_loaded_maximum;
};

struct weapon_definition
{
	struct
	{
		struct
		{
			long count;
			struct weapon_magazine_definition *elements;
		} magazines;
	} weapon;
};

struct weapon_datum
{
	long definition_index;
	struct
	{
		real age;
		real heat;
		struct
		{
			short state;
			short state_timer;
			short rounds_total;
			short rounds_loaded;
		} magazines[2];
	} weapon;
};

static struct
{
	boolean bottomless_clip;
	boolean infinite_ammo;
} cheat;

#define TAG_BLOCK_GET_ELEMENT(block, idx, type) (&((type *)((block)->elements))[(idx)])
static struct weapon_magazine_definition fake_mag_defs[2] = { { 600, 60 }, { 600, 60 } };
static struct weapon_definition fake_weap_def = { { { 1, fake_mag_defs } } };
static struct weapon_definition *weapon_definition_get(long idx) { (void)idx; return &fake_weap_def; }
static boolean skulls_unlock_next_by_milestone(char const *reason) { (void)reason; return FALSE; }

static struct game_variant test_variant;
static struct game_variant_options test_options;
static void *game_engine = (void *)1;
static struct data_array *player_data;
static struct data_array *unit_data;
static long fake_tick = 100;
static boolean fake_distributed_client = FALSE;
static long fake_team_score = 0;
static real fake_skulls_mult = 1.0f;
static int spawned_waves = 0;
static int ordnance_drops = 0;
static int vehicles_spawned = 0;
static int friendly_ai_spawned = 0;
static boolean allegiance_matrix[8][8];

#define global_variant test_variant
static long global_scenario_index = NONE;

static struct game_variant_options *game_variant_options_get(void) { return &test_options; }
static void game_set_game_variant_options(struct game_variant_options const *opts) { if (opts) test_options = *opts; }
static boolean network_game_distributed_client(void) { return fake_distributed_client; }
static long game_time_get(void) { return fake_tick; }
static void game_difficulty_level_set(short d) { (void)d; }
static real campaign_score_skulls_multiplier(void) { return fake_skulls_mult; }
static long campaign_score_team_points(void) { return fake_team_score; }
static long campaign_score_player_points(long p) { (void)p; return fake_team_score; }
static void campaign_score_level_won(void) {}
static void firefight_stats_record_match_end(char const *m) { (void)m; }
static void firefight_hud_post_rasterize(void) {}

static void game_allegiance_create(short t1, boolean b1, short t2, boolean b2, short a, short b, boolean c)
{
	(void)b1; (void)b2; (void)a; (void)b; (void)c;
	if (t1 >= 0 && t1 < 8 && t2 >= 0 && t2 < 8)
	{
		allegiance_matrix[t1][t2] = TRUE;
		allegiance_matrix[t2][t1] = TRUE;
	}
}

static void game_allegiance_remove(short t1, short t2)
{
	if (t1 >= 0 && t1 < 8 && t2 >= 0 && t2 < 8)
	{
		allegiance_matrix[t1][t2] = FALSE;
		allegiance_matrix[t2][t1] = FALSE;
	}
}

struct data_iterator
{
	struct data_array *data;
	long index;
	long datum_index;
};

static void data_iterator_new(struct data_iterator *it, struct data_array *data)
{
	it->data = data;
	it->index = 0;
	it->datum_index = NONE;
}

static void *data_iterator_next(struct data_iterator *it)
{
	while (it->data && it->index < it->data->maximum_count)
	{
		long idx = it->index++;
		struct datum_header *h = (struct datum_header *)(it->data->data + idx * it->data->size);
		if (h->identifier)
		{
			it->datum_index = ((long)(unsigned short)h->identifier << 16) | idx;
			return h;
		}
	}
	return NULL;
}

static struct player_datum *player_try_and_get(long player_index)
{
	if (player_index == NONE || !player_data)
		return NULL;
	return (struct player_datum *)datum_get(player_data, player_index);
}

static struct unit_datum *unit_try_and_get(long unit_index)
{
	if (unit_index == NONE || !unit_data)
		return NULL;
	return (struct unit_datum *)datum_get(unit_data, unit_index);
}

static void *object_try_and_get_and_verify_type(long object_index, unsigned long mask)
{
	(void)mask;
	return unit_try_and_get(object_index);
}

static void object_get_origin(long object_index, real_point3d *out_pos)
{
	struct unit_datum *u = unit_try_and_get(object_index);
	if (u && out_pos)
		*out_pos = u->object.position;
	else if (out_pos)
		memset(out_pos, 0, sizeof(*out_pos));
}

static real distance_squared3d(real_point3d const *a, real_point3d const *b)
{
	real dx = a->x - b->x, dy = a->y - b->y, dz = a->z - b->z;
	return dx * dx + dy * dy + dz * dz;
}

static real distance3d(real_point3d const *a, real_point3d const *b)
{
	return sqrtf(distance_squared3d(a, b));
}

static long tag_loaded(unsigned long group, char const *name)
{
	(void)group;
	if (name && !strcmp(name, "characters\\elite\\elite_mp"))
		return 777;
	return NONE;
}

struct tag_reference
{
	long group_tag;
	char *name;
	long name_length;
	long index;
};

struct actor_variant_definition
{
	unsigned long flags;
	struct tag_reference actor_reference;
	struct tag_reference unit_reference;
};

static struct
{
	char const *name;
	long actor_idx;
	long unit_idx;
} const mock_actv_table[] =
{
	{ "vehicles\\warthog\\warthog gunner", NONE, NONE },
	{ "vehicles\\scorpion\\scorpion pilot", NONE, NONE },
	{ "characters\\cortana\\cortana", 9, 19 },
	{ "characters\\marine_armored\\marine_armored", 10, 20 },
	{ "characters\\grunt\\grunt minor", 11, 21 },
	{ "characters\\jackal\\jackal major", 12, 22 },
	{ "characters\\elite\\elite major", 13, 23 },
	{ "characters\\hunter\\hunter", 14, 24 },
	{ "characters\\flood_infection\\flood_infection", 15, 25 },
	{ "characters\\sentinel\\sentinel", 16, 26 },
};

static struct actor_variant_definition mock_actv_defs[NUMBEROF(mock_actv_table)];

static struct actor_variant_definition *actor_variant_definition_get(long tag_index)
{
	if (tag_index >= 100 && tag_index < 100 + NUMBEROF(mock_actv_table))
	{
		int idx = tag_index - 100;
		mock_actv_defs[idx].actor_reference.index = mock_actv_table[idx].actor_idx;
		mock_actv_defs[idx].unit_reference.index = mock_actv_table[idx].unit_idx;
		return &mock_actv_defs[idx];
	}
	return NULL;
}

static char const *tag_get_name(long def_index)
{
	if (def_index >= 100 && def_index < 100 + NUMBEROF(mock_actv_table))
		return mock_actv_table[def_index - 100].name;
	return "levels\\test\\ff_test";
}

enum
{
	MAX_ROSTER_VARIANTS = 64,
	GENERATOR_LOCKDOWN_DURATION_TICKS = 30 * TICKS_PER_SECOND,
	GENERATOR_LOCKDOWN_COOLDOWN_TICKS = 60 * TICKS_PER_SECOND,
	GENERATOR_SHIELD_STUN_TICKS = 6 * TICKS_PER_SECOND,
};

struct firefight_roster_entry
{
	long variant_tag_index;
	short faction;
	short unit_toggle_bit;
	byte tier;
	byte subtype;
	boolean is_human_ally_or_enemy;
};

struct firefight_net_packet
{
	short current_set;
	short current_round;
	short current_wave;
	short phase_state;
	long phase_timer_ticks;
	long session_elapsed_ticks;
	long next_wave_timer_ticks;
	short shared_lives_remaining;
	short enemies_alive;
	unsigned long active_skulls;
	long team_score;
	real active_multiplier;
	real set_vitality_scale;
	byte in_bonus_round;
	byte match_won;
	byte match_lost;
	byte generator_count;
	short gen_health[FIREFIGHT_MAX_GENERATORS];
	short gen_max_health[FIREFIGHT_MAX_GENERATORS];
	short gen_shield[FIREFIGHT_MAX_GENERATORS];
	short gen_max_shield[FIREFIGHT_MAX_GENERATORS];
	short gen_lockdown_ticks[FIREFIGHT_MAX_GENERATORS];
	byte gen_alive[FIREFIGHT_MAX_GENERATORS];
	short banner_type;
	short banner_data;
	short banner_timer_ticks;
	long player_score[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	short player_kills[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	short player_assists[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
};

static struct firefight_globals ff_state;
static struct firefight_roster_entry g_roster[MAX_ROSTER_VARIANTS];
static short g_roster_count = 0;

void firefight_spawner_initialize_map(void) {}
void firefight_spawner_dispose_map(void) {}
void firefight_prune_dead_tracked_enemies(void)
{
	short i, alive = 0;
	for (i = 0; i < FIREFIGHT_MAX_TRACKED_ENEMIES; i++)
	{
		if (ff_state.tracked_enemy_units[i] != NONE)
			alive++;
	}
	if (alive > 0)
		ff_state.enemies_alive = alive;
}
void firefight_update_dropships_and_vehicles(void) {}
void firefight_despawn_round_vehicles(void) {}
void firefight_despawn_set_vehicles(void) {}
void firefight_cleanup_natural_multiplayer_sandbox(boolean clear_weapons_and_equipment) { (void)clear_weapons_and_equipment; }
short firefight_queued_enemies_count(void) { return ff_state.enemies_queued; }
void firefight_spawn_ordnance_drop(boolean heavy_drop) { (void)heavy_drop; ordnance_drops++; }
void firefight_spawn_friendly_reinforcements(short count) { friendly_ai_spawned += count; }
void firefight_spawn_scheduled_round_vehicles(short round_num, boolean pre_boss_wave)
{
	(void)round_num;
	(void)pre_boss_wave;
	vehicles_spawned++;
}
boolean firefight_wave_will_have_heavy_or_boss(short set_num, short round_num, short wave_num)
{
	(void)set_num;
	(void)round_num;
	return wave_num == FIREFIGHT_WAVES_PER_ROUND;
}
short firefight_spawn_wave(short set_num, short round_num, short wave_num, boolean bonus_wave)
{
	(void)set_num;
	(void)round_num;
	(void)wave_num;
	spawned_waves++;
	ff_state.enemies_alive = bonus_wave ? 12 : 8;
	return ff_state.enemies_alive;
}
void firefight_generators_update(void) {}
void firefight_generators_initialize(short count, short max_health) { (void)count; (void)max_health; }
static long g_generator_scenery_indices[FIREFIGHT_MAX_GENERATORS] = { NONE, NONE, NONE };

#include "under_test.inc"

static void reset_world(void)
{
	int i;

	memset(&test_variant, 0, sizeof(test_variant));
	memset(&test_options, 0, sizeof(test_options));
	memset(&ff_state, 0, sizeof(ff_state));
	memset(g_roster, 0, sizeof(g_roster));
	memset(allegiance_matrix, 0, sizeof(allegiance_matrix));
	g_roster_count = 0;

	for (i = 0; i < FIREFIGHT_MAX_TRACKED_ENEMIES; i++)
		ff_state.tracked_enemy_units[i] = NONE;

	test_variant.game_engine_index = game_engine_firefight;
	test_variant.universal_variant.teams = TRUE;
	test_variant.game_engine_variant.firefight.starting_lives = 7;
	test_variant.game_engine_variant.firefight.bonus_lives_per_round = 1;
	test_variant.game_engine_variant.firefight.bonus_lives_per_set = 3;
	test_variant.game_engine_variant.firefight.max_lives_cap = 20;
	test_variant.game_engine_variant.firefight.difficulty = _firefight_difficulty_heroic;
	test_variant.game_engine_variant.firefight.ordnance_drops = _firefight_ordnance_per_round;
	test_variant.game_engine_variant.firefight.bonus_round_enabled = 1;
	test_variant.game_engine_variant.firefight.progression_skulls = 1;
	test_variant.game_engine_variant.firefight.vehicle_drops_enabled = 1;

	firefight_variant_options_default(&test_options);
	test_options.firefight_round_1_skull = _firefight_skull_tough_luck_bit + 1;
	test_options.firefight_round_2_skull = _firefight_skull_catch_bit + 1;
	test_options.firefight_round_3_skull = _firefight_skull_cloud_bit + 1;
	test_options.firefight_generator_health = 3000;
	test_options.firefight_score_life_threshold = 10000;

	player_data = game_state_data_new("players", 16, sizeof(struct player_datum));
	unit_data = game_state_data_new("units", 64, sizeof(struct unit_datum));
	fake_tick = 100;
	fake_distributed_client = FALSE;
	fake_team_score = 0;
	fake_skulls_mult = 1.0f;
	spawned_waves = 0;
	ordnance_drops = 0;
	vehicles_spawned = 0;
	friendly_ai_spawned = 0;

	ff_state.current_set = 1;
	ff_state.current_round = 1;
	ff_state.current_wave = 1;
	ff_state.phase_state = _ff_phase_pre_round;
	ff_state.phase_timer_ticks = 5 * TICKS_PER_SECOND;
	ff_state.shared_lives_remaining = 7;
	ff_state.max_lives_cap = 20;
	ff_state.next_bonus_life_score = 10000;
	firefight_recalculate_skulls_and_multiplier();
}

int main(int argc, char **argv)
{
	const char *case_name = argc > 1 ? argv[1] : "";

	CASE("abi_and_variant_size")
	{
		CHECK(sizeof(struct game_engine_firefight_variant) == 0x18 &&
			sizeof(struct game_engine_firefight_variant) <= 0x20,
			"expected game_engine_firefight_variant size 0x18 (<= 0x20 union cap), got 0x%zx",
			sizeof(struct game_engine_firefight_variant));
		CHECK(NUMBER_OF_FIREFIGHT_SKULLS == 13,
			"expected 13 Reach skulls, got %d", NUMBER_OF_FIREFIGHT_SKULLS);
		CHECK(NUMBER_OF_FIREFIGHT_UNIT_TOGGLES == 12,
			"expected 12 unit toggle bits, got %d", NUMBER_OF_FIREFIGHT_UNIT_TOGGLES);
		CHECK(FIREFIGHT_ROUNDS_PER_SET == 5 && FIREFIGHT_WAVES_PER_ROUND == 3,
			"expected 5 rounds x 3 waves per set");
		return 0;
	}

	CASE("skull_multipliers_and_progression")
	{
		reset_world();

		/* Heroic base multiplier = 2.00x with 1.0x skull multiplier in Set 1 */
		CHECK(fabsf(firefight_score_multiplier_get() - 2.00f) < 0.001f,
			"expected 2.00f base Heroic multiplier, got %f", firefight_score_multiplier_get());

		/* Enable Iron + Mythic custom skulls and test exclude mask */
		test_variant.game_engine_variant.firefight.custom_skulls =
			FLAG(_firefight_skull_iron_bit) | FLAG(_firefight_skull_mythic_bit) | FLAG(_firefight_skull_blind_bit);
		test_options.firefight_skull_exclude_mask = FLAG(_firefight_skull_blind_bit);
		fake_skulls_mult = 2.25f;
		firefight_recalculate_skulls_and_multiplier();
		CHECK(firefight_skull_active(_firefight_skull_iron_bit), "expected Iron skull active");
		CHECK(firefight_skull_active(_firefight_skull_mythic_bit), "expected Mythic skull active");
		CHECK(!firefight_skull_active(_firefight_skull_blind_bit), "Excluded Blind skull must remain inactive");
		CHECK(firefight_40_skull_active(_skull_iron) && firefight_40_skull_active(_skull_mythic),
			"40-skull bridge must report Iron and Mythic active");
		CHECK(fabsf(firefight_score_multiplier_get() - 4.50f) < 0.001f,
			"expected 4.50f multiplier, got %f", firefight_score_multiplier_get());

		/* Progressive per-set enemy vitality escalation: Medium (+25% per Set after Set 1) */
		ff_state.current_set = 3;
		test_options.firefight_set_escalation = _firefight_escalation_medium;
		firefight_recalculate_skulls_and_multiplier();
		CHECK(fabsf(firefight_set_vitality_scale_get() - 1.50f) < 0.001f,
			"expected 1.50x vitality scale at Set 3 on Medium (+25%%/set), got %f",
			firefight_set_vitality_scale_get());

		/* Bonus Round automatically activates Tough Luck, Catch, Cowbell, and Grunt Birthday Party */
		test_variant.game_engine_variant.firefight.custom_skulls = 0;
		ff_state.in_bonus_round = TRUE;
		firefight_recalculate_skulls_and_multiplier();
		CHECK(firefight_skull_active(_firefight_skull_tough_luck_bit), "Bonus Round must activate Tough Luck");
		CHECK(firefight_skull_active(_firefight_skull_catch_bit), "Bonus Round must activate Catch");
		CHECK(firefight_skull_active(_firefight_skull_grunt_birthday_party_bit), "Bonus Round must activate GBP");
		return 0;
	}

	CASE("wave_round_set_progression")
	{
		long p0, u0;
		int round, wave;

		reset_world();
		test_variant.game_engine_variant.firefight.set_limit = 2;
		test_options.firefight_prep_countdown = _firefight_prep_5s;
		test_options.firefight_friendly_ai = _firefight_friendly_ai_round_duo;
		p0 = datum_new(player_data);
		u0 = datum_new(unit_data);
		player_try_and_get(p0)->unit_index = u0;

		firefight_begin_pre_round(1);
		CHECK(ff_state.phase_state == _ff_phase_pre_round && ff_state.current_round == 1,
			"expected Round 1 pre_round");

		for (round = 1; round <= 5; round++)
		{
			CHECK(ff_state.current_round == round, "expected round %d, got %d", round, ff_state.current_round);
			while (ff_state.phase_state == _ff_phase_pre_round)
				firefight_update();

			/* Verify Round skull overrides for Rounds 1, 2, 3 */
			if (round == 1)
				CHECK(firefight_skull_active(_firefight_skull_tough_luck_bit), "Round 1 must activate Tough Luck");
			if (round == 2)
				CHECK(firefight_skull_active(_firefight_skull_catch_bit), "Round 2 must activate Catch");
			if (round == 3)
				CHECK(firefight_skull_active(_firefight_skull_cloud_bit), "Round 3 must activate Cloud");

			for (wave = 1; wave <= 3; wave++)
			{
				CHECK(ff_state.current_wave == wave, "expected wave %d, got %d", wave, ff_state.current_wave);
				while (ff_state.phase_state == _ff_phase_wave_spawning)
					firefight_update();
				CHECK(ff_state.phase_state == _ff_phase_wave_active, "expected wave_active");

				if (wave < 3)
				{
					/* Waves 1..2 transition when enemies_alive <= 2 after 10s elapsed */
					ff_state.enemies_alive = 3;
					ff_state.wave_elapsed_ticks = 10 * TICKS_PER_SECOND;
					firefight_update();
					CHECK(ff_state.phase_state == _ff_phase_wave_active,
						"wave %d should remain active with 3 enemies alive", wave);
					CHECK(ff_state.straggler_navpoints_active == TRUE,
						"straggler navpoints must activate when <= 3 enemies remain");

					ff_state.enemies_alive = 2;
					firefight_update();
					CHECK(ff_state.phase_state == _ff_phase_wave_clear,
						"wave %d should enter wave_clear when <= 2 enemies remain", wave);
				}
				else
				{
					/* Wave 3 (Final Wave of Round) requires 0 enemies alive */
					ff_state.enemies_alive = 1;
					ff_state.wave_elapsed_ticks = 20 * TICKS_PER_SECOND;
					firefight_update();
					CHECK(ff_state.phase_state == _ff_phase_wave_active,
						"Wave 3 must NOT end while 1 enemy remains");

					ff_state.enemies_alive = 0;
					firefight_update();
					CHECK(ff_state.phase_state == _ff_phase_wave_clear,
						"Wave 3 must enter wave_clear at 0 enemies");
				}

				while (ff_state.phase_state == _ff_phase_wave_clear)
					firefight_update();
			}
		}

		/* After Round 5 Wave 3, Bonus Round (1800 ticks = 60s) must begin */
		CHECK(spawned_waves >= 15 && ordnance_drops >= 5 && vehicles_spawned >= 5 && friendly_ai_spawned >= 10,
			"expected wave spawns, ordnance drops, scheduled round vehicles, and friendly AI");
		CHECK(ff_state.phase_state == _ff_phase_bonus_round && ff_state.in_bonus_round == TRUE,
			"expected Bonus Round after Round 5 Wave 3, got phase %d", ff_state.phase_state);

		ff_state.phase_timer_ticks = 1;
		firefight_update();
		CHECK(ff_state.phase_state == _ff_phase_set_complete && ff_state.in_bonus_round == FALSE,
			"expected set_complete after Bonus Round timer expires");

		/* Advance through SET_COMPLETE into Set 2 */
		while (ff_state.phase_state == _ff_phase_set_complete)
			firefight_update();
		CHECK(ff_state.current_set == 2 && ff_state.phase_state == _ff_phase_pre_round,
			"expected Set 2 pre_round");
		CHECK(firefight_skull_active(_firefight_skull_tilt_bit),
			"Set 2 must activate persistent Tilt Set Skull");

		/* Completing Set 2 when set_limit == 2 triggers Victory */
		ff_state.phase_state = _ff_phase_set_complete;
		ff_state.phase_timer_ticks = 1;
		firefight_update();
		CHECK(ff_state.match_won == TRUE && ff_state.phase_state == _ff_phase_victory,
			"expected Victory when set_limit is reached");
		return 0;
	}

	CASE("shared_lives_respawn_and_spectate")
	{
		long p0, p1, u0, u1;
		struct player_datum *pl0, *pl1;
		int t;

		reset_world();
		ff_state.shared_lives_remaining = 1;
		p0 = datum_new(player_data);
		p1 = datum_new(player_data);
		u0 = datum_new(unit_data);
		u1 = datum_new(unit_data);
		pl0 = player_try_and_get(p0);
		pl1 = player_try_and_get(p1);
		pl0->unit_index = u0;
		pl1->unit_index = u1;

		/* Player 0 dies and consumes the last reserve life (1 -> 0); Player 1 is Last Man Standing */
		pl0->unit_index = NONE;
		firefight_player_killed_player(NONE, NONE, p0, FALSE);
		CHECK(ff_state.shared_lives_remaining == 0, "expected 0 reserve lives left");
		CHECK(firefight_player_out_of_lives(p0), "expected out_of_lives when shared_lives_remaining == 0");
		CHECK(firefight_spectating_active(), "expected spectating_active when 1 of 2 players is alive with 0 lives");
		CHECK(ff_state.last_man_standing_announced == TRUE &&
			ff_state.banner_type == _ff_banner_last_man_standing,
			"expected LAST MAN STANDING banner when 1 player remains with 0 lives");

		/* Awarding a bonus life restores shared lives and clears out_of_lives */
		firefight_add_shared_lives(2);
		CHECK(ff_state.shared_lives_remaining == 2, "expected 2 shared lives after bonus");
		CHECK(!firefight_player_out_of_lives(p0), "Player 0 is no longer out of lives");

		/* Consume lives and kill both players -> wipe defeat after 90 ticks (3 seconds) */
		ff_state.shared_lives_remaining = 0;
		pl0->unit_index = NONE;
		pl1->unit_index = NONE;
		for (t = 0; t < 3 * TICKS_PER_SECOND; t++)
			firefight_update();
		CHECK(ff_state.match_lost == TRUE && ff_state.phase_state == _ff_phase_defeat,
			"expected Defeat after all Spartans KIA with 0 lives for 3 seconds");
		return 0;
	}

	CASE("generator_defense_and_lockdown")
	{
		long p0, u0;
		struct unit_datum *unit0;
		short gen_idx = NONE;

		reset_world();
		p0 = datum_new(player_data);
		u0 = datum_new(unit_data);
		player_try_and_get(p0)->unit_index = u0;
		unit0 = unit_try_and_get(u0);
		unit0->object.position.x = 10.0f;
		unit0->object.position.y = 10.0f;
		unit0->object.position.z = 0.0f;

		ff_state.generator_count = 2;
		ff_state.generators_alive = 2;
		test_variant.game_engine_variant.firefight.generator_count = 2;
		test_variant.game_engine_variant.firefight.generator_fail_mode = _firefight_generator_fail_any;

		ff_state.generators[0].active = TRUE;
		ff_state.generators[0].alive = TRUE;
		ff_state.generators[0].object_index = 500;
		ff_state.generators[0].position = unit0->object.position;
		ff_state.generators[0].max_health = 2000;
		ff_state.generators[0].health = 2000;
		ff_state.generators[0].max_shield = 500;
		ff_state.generators[0].shield = 500;

		ff_state.generators[1].active = TRUE;
		ff_state.generators[1].alive = TRUE;
		ff_state.generators[1].object_index = 501;
		ff_state.generators[1].max_health = 2000;
		ff_state.generators[1].health = 2000;
		ff_state.generators[1].max_shield = 500;
		ff_state.generators[1].shield = 500;

		CHECK(firefight_is_generator_object(500, &gen_idx) && gen_idx == 0,
			"expected object 500 to be Generator 0");

		/* Friendly fire from Spartan player p0 is ignored */
		firefight_apply_generator_damage(500, p0, _game_team_player, 500.0f);
		CHECK(ff_state.generators[0].shield == 500 && ff_state.generators[0].health == 2000,
			"Spartan friendly fire must not damage Generator");

		/* Enemy Covenant damage hits shield first, then health */
		firefight_apply_generator_damage(500, NONE, _game_team_covenant, 2000.0f);
		CHECK(ff_state.generators[0].shield == 0 && ff_state.generators[0].health < 2000,
			"expected shield depleted and health damaged, got shield=%d health=%d",
			ff_state.generators[0].shield, ff_state.generators[0].health);

		/* Trigger Generator Lockdown near Generator 0 -> invulnerable during lockdown */
		{
			short hp_before = ff_state.generators[0].health;
			firefight_try_trigger_generator_lockdown(p0);
			CHECK(ff_state.generators[0].lockdown_ticks == GENERATOR_LOCKDOWN_DURATION_TICKS,
				"expected Generator 0 lockdown active");
			firefight_apply_generator_damage(500, NONE, _game_team_covenant, 1000.0f);
			CHECK(ff_state.generators[0].health == hp_before,
				"locked-down generator must ignore incoming damage");
		}

		/* Destroying Generator 0 when generator_fail_mode == _firefight_generator_fail_any ends game */
		ff_state.generators[0].lockdown_ticks = 0;
		firefight_apply_generator_damage(500, NONE, _game_team_covenant, 10000.0f);
		CHECK(ff_state.generators[0].alive == FALSE && ff_state.generators_alive == 1,
			"expected Generator 0 destroyed");
		CHECK(ff_state.match_lost == TRUE && ff_state.phase_state == _ff_phase_defeat,
			"expected Defeat when any generator is destroyed in _firefight_generator_fail_any mode");
		return 0;
	}

	CASE("scoring_sprees_multikills_and_network_sync")
	{
		long p0, enemy_u;
		byte net_buf[2048];
		long written;

		reset_world();
		p0 = datum_new(player_data);
		enemy_u = datum_new(unit_data);

		ff_state.tracked_enemy_units[0] = enemy_u;
		ff_state.tracked_enemy_tiers[0] = _ff_tier_boss_heavy;
		ff_state.enemies_alive = 1;
		ff_state.shared_lives_remaining = 5;
		fake_team_score = 10250;

		/* Kill boss enemy and cross 10,000 points -> +1 Bonus Life */
		firefight_notify_actor_or_unit_killed(NONE, enemy_u, p0, TRUE, FALSE, FALSE, FALSE);
		CHECK(ff_state.enemies_alive == 0, "tracked enemy kill must decrement enemies_alive");
		CHECK(ff_state.player_stats[0].kills == 1 && ff_state.player_stats[0].boss_kills == 1,
			"expected 1 kill and 1 boss kill for p0");
		CHECK(ff_state.shared_lives_remaining == 6 && ff_state.next_bonus_life_score == 20000,
			"crossing 10,000 pts must award +1 Bonus Life and advance threshold to 20,000");

		/* Round-trip network state serialization */
		ff_state.current_set = 3;
		ff_state.current_round = 2;
		ff_state.current_wave = 4;
		ff_state.session_elapsed_ticks = 2700;
		written = game_engine_firefight_write_network_state(net_buf, sizeof(net_buf));
		CHECK(written > 0, "expected non-zero network packet size");

		memset(&ff_state, 0, sizeof(ff_state));
		CHECK(game_engine_firefight_read_network_state(net_buf, written, FALSE),
			"expected network state read to succeed");
		CHECK(ff_state.current_set == 3 && ff_state.current_round == 2 && ff_state.current_wave == 4 &&
			ff_state.shared_lives_remaining == 6 && ff_state.session_elapsed_ticks == 2700 &&
			ff_state.player_stats[0].kills == 1,
			"network state round-trip mismatch");
		return 0;
	}

	CASE("unit_toggle_validation_and_faction_diplomacy")
	{
		int i;

		reset_world();

		/* 1. Unit Toggle Validation Guard: cannot disable all enemy units for active faction */
		CHECK(!firefight_validate_unit_toggles(_firefight_faction_mixed, 0),
			"mask=0 must be rejected for Mixed faction");
		CHECK(!firefight_validate_unit_toggles(_firefight_faction_covenant,
			(1U << _ff_unit_flood_combat_bit) | (1U << _ff_unit_sentinels_bit)),
			"Covenant-only faction must reject mask with 0 Covenant units enabled");
		CHECK(firefight_validate_unit_toggles(_firefight_faction_covenant,
			(1U << _ff_unit_covenant_grunts_bit)),
			"Covenant-only faction must accept mask with Grunts enabled");
		CHECK(!firefight_validate_unit_toggles(_firefight_faction_sentinels,
			FIREFIGHT_UNIT_MASK_ALL & ~(1U << _ff_unit_sentinels_bit)),
			"Sentinels-only faction must reject mask when Sentinels bit is off");

		/* 2. Enemy Inter-Faction Diplomacy: Allied vs Players (default) vs Classic Infighting */
		test_options.firefight_faction_diplomacy = _firefight_diplomacy_allied_against_players;
		firefight_setup_faction_allegiances();
		CHECK(allegiance_matrix[_game_team_covenant][_game_team_flood] == TRUE &&
			allegiance_matrix[_game_team_covenant][_game_team_sentinel] == TRUE &&
			allegiance_matrix[_game_team_flood][_game_team_sentinel] == TRUE,
			"Allied diplomacy must unite Covenant, Flood, and Sentinels");

		test_options.firefight_faction_diplomacy = _firefight_diplomacy_classic_infighting;
		firefight_setup_faction_allegiances();
		CHECK(allegiance_matrix[_game_team_covenant][_game_team_flood] == FALSE &&
			allegiance_matrix[_game_team_covenant][_game_team_sentinel] == FALSE &&
			allegiance_matrix[_game_team_flood][_game_team_sentinel] == FALSE,
			"Classic Infighting diplomacy must remove Covenant/Flood/Sentinel alliances");

		/* 3. Spartan-only player biped enforcement & UNSC Humans hostility */
		test_options.firefight_player_faction_mode = _firefight_player_faction_elite;
		test_variant.game_engine_variant.firefight.wave_faction = _firefight_faction_unsc_humans;
		firefight_setup_faction_allegiances();
		CHECK(!firefight_elite_mode_active(), "expected Elite player mode disabled (Spartan only)");
		CHECK(allegiance_matrix[_game_team_player][_game_team_human] == FALSE,
			"Fighting UNSC Humans must make Humans hostile to players");
		CHECK(firefight_resolve_player_biped_tag(0, 123) == 123,
			"Player biped tag must always resolve to Spartan biped (123)");

		/* 4. Actor variant classifier filters out vehicle gunners/pilots and Cortana */
		for (i = 0; i < NUMBEROF(mock_actv_table); i++)
			classify_actor_variant(100 + i);
		CHECK(g_roster_count == 7,
			"expected 7 valid combat actor variants (filtering 2 vehicle crew + Cortana), got %d",
			(int)g_roster_count);
		return 0;
	}

	fprintf(stderr, "unknown case: %s\n", case_name);
	return 2;
}
