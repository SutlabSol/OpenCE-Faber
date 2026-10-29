/*
FIRING_RANGE.C

Custom 4-Zone Forerunner Firing Range & Testing Facility (firing_range.h):
- Zone 1 (North): 40m x 25m x 50m Open-Skybox Vehicle & Flight Test Area with
  winding loop track, infield scenery (trees, rocks, bushes), exterior Vehicle
  Spawn Pad, Vehicle Spawner C-Uplink, and Track AI Spawner C-Uplink (with
  Auto-Despawn Vehicles on Exit toggle and automatic Track AI kill on exit).
- Zone 2 (Middle-Right): 35m x 2m Gun Range Hall with Weapon Table, Weapon
  Dispenser C-Uplink (left), Target Settings C-Uplink (right, 5m-30m range +
  biped/dummy selector), and live DPS / TTK / Spread Circle Diameter / Accuracy
  HUD telemetry.
- Zone 3 (South of Gun Range Entrance): Animations & Real-Time Mirror Area.
- Zone 4 (South-East): Enclosed AI Fight Room with exterior Fight Room AI
  C-Uplink terminal spawning active AI that move and shoot back, cannot leave
  the room, and are killed if the player exits.
- Distant Horizon: 3 a30-style Forerunner beam emitter spire structures.
*/

/* ---------- headers */

#include "cseries.h"
#include "errors.h"
#include "cache/cache_files.h"
#include "game/game.h"
#include "game/game_globals.h"
#include "ai/actor_definitions.h"
#include "ai/actors.h"
#include "game/players.h"
#include "input/input.h"
#include "interface/hud_messaging.h"
#include "interface/interface.h"
#include "items/weapons.h"
#include "math/integer_math.h"
#include "models/model_animation_definitions.h"
#include "objects/objects.h"
#include "physics/collision_bsp_definitions.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "shaders/shader_definitions.h"
#include "structures/structure_bsp_definitions.h"
#include "tag_files/tag_groups.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/unicode.h"
#include "units/unit_control_data.h"
#include "units/units.h"
#include "firing_range.h"
#include "tag_injector.h"

void unit_network_add_weapon(long unit_index, long weapon_index, short slot);
void unit_network_forget_weapon(long unit_index, short slot);

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- constants */

enum
{
	SCENARIO_GROUP_TAG = 'scnr',
	SHADER_ENVIRONMENT_GROUP_TAG = 'senv',
	BITMAP_GROUP_TAG = 'bitm',
	WEAPON_GROUP_TAG = 'weap',
	ACTOR_VARIANT_GROUP_TAG = 'actv',
	VEHICLE_GROUP_TAG = 'vehi',
	SCENERY_GROUP_TAG = 'scen',

	MAXIMUM_RANGE_WEAPONS = 96,
	MAXIMUM_RANGE_AIS = 64,
	MAXIMUM_RANGE_VEHICLES = 32,
	MAXIMUM_RANGE_FIGHTERS = 8,
	MAXIMUM_RANGE_TRACK_AIS = 8,
	MAXIMUM_RANGE_SCENERY = 16,
	RANGE_NAME_LENGTH = 48,
	STRUCTURE_BSP_HEADER_SIGNATURE = 'sbsp',

	RANGE_TILES = 16,
	RANGE_SUBCLUSTERS = 4,

	_range_face_floor = 0,
	_range_face_east,
	_range_face_west,
	_range_face_north,
	_range_face_south,
	_range_face_ceiling,
	NUMBER_OF_ROOM_FACES,
	NUMBER_OF_RANGE_WALLS = 4,
	NUMBER_OF_BENCH_FACES = 5,
	BOX_CORNERS = 8,

	/* Static facility collision blocks:
	   - 2 Track South Divider Walls (left & right of gate)
	   - 3 Track Infield Winding Loop Islands
	   - 1 Gun Range Weapon Table
	   - 3 Gun Range Hall Walls (South, North, East Backstop)
	   - 5 Fight Room Walls (North, South, East, West-North, West-South) */
	NUMBER_OF_FACILITY_BLOCKS = 14,
	MAXIMUM_RANGE_BLOCKS = NUMBER_OF_FACILITY_BLOCKS,

	MAXIMUM_RANGE_FACES = NUMBER_OF_ROOM_FACES + MAXIMUM_RANGE_BLOCKS * NUMBER_OF_BENCH_FACES,
	MAXIMUM_RANGE_PLANES = MAXIMUM_RANGE_FACES + MAXIMUM_RANGE_BLOCKS * 2,
	MAXIMUM_RANGE_NODES = MAXIMUM_RANGE_PLANES,
	MAXIMUM_RANGE_COLLISION_VERTICES = BOX_CORNERS * (1 + MAXIMUM_RANGE_BLOCKS),
	MAXIMUM_RANGE_EDGES = 12 * (1 + MAXIMUM_RANGE_BLOCKS),

	RANGE_FLOOR_SURFACES = RANGE_TILES * RANGE_TILES * 2,
	RANGE_WALL_SURFACES = RANGE_TILES * 2,
	RANGE_FLOOR_VERTICES = (RANGE_TILES + 1) * (RANGE_TILES + 1),
	RANGE_WALL_VERTICES = (RANGE_TILES + 1) * 2,

	/* Detail geometry quads:
	   - Collision blocks' faces (14 * 5 = 70)
	   - Mirror floor line + Vehicle Spawn Pad border (2 flat quads)
	   - Mirror 3D frame (16 quads)
	   - 3 Distant a30-style Forerunner Beam Emitter Towers (3 * 16 = 48 quads) */
	RANGE_MIRROR_FRAME_QUADS = 2 * 5 + 6,
	RANGE_EMITTER_TOWER_QUADS = 3 * 16,
	RANGE_FLAT_DETAIL_QUADS = 2,
	MAXIMUM_RANGE_DETAIL_QUADS = MAXIMUM_RANGE_BLOCKS * NUMBER_OF_BENCH_FACES +
		RANGE_FLAT_DETAIL_QUADS + RANGE_MIRROR_FRAME_QUADS + RANGE_EMITTER_TOWER_QUADS,

	NUMBER_OF_RANGE_COLLISION_MATERIALS = 3,
	NUMBER_OF_RANGE_MATERIALS = 3,
	RANGE_ROOM_SUBCLUSTERS = RANGE_SUBCLUSTERS * RANGE_SUBCLUSTERS + NUMBER_OF_RANGE_WALLS * RANGE_SUBCLUSTERS,

	MAXIMUM_RANGE_STARTING_LOCATIONS = 64,
	RANGE_REFILL_TICKS = 2 * 30,
	RANGE_CHECK_TICKS = 15,
	RANGE_DUMMY_HEAL_TICKS = 5 * 30,
	RANGE_DUMMY_RESPAWN_TICKS = 2 * 30,
	MAXIMUM_RANGE_CLEARED = 256,

	/* 5 Interactive C-Uplink Computer Terminals */
	_range_terminal_vehicle_spawner = 0,
	_range_terminal_track_ai_spawner,
	_range_terminal_weapon_dispenser,
	_range_terminal_target_settings,
	_range_terminal_fight_room_ai,
	NUMBER_OF_RANGE_TERMINALS,

	RANGE_SPREAD_SAMPLES = 64,
};

/* Facility Dimensions (1 world unit ≈ 3.0m / 10ft):
   - Outer compound: X [-16..+16], Y [-14..+16], Z [0..100] (50m+ open skybox)
   - Zone 1 (North Vehicle & Flight Test Area): X [-15.9..+15.9], Y [+4.0..+15.9] (~40m+ x 25m+ x 50m+)
   - Zone 2 (Middle-Right Gun Range Hall): X [+1.3..+13.9] (12.6u ≈ 35m long), Y [+0.25..+1.95] (1.7u ≈ 2m+ wide)
   - Zone 3 (Animations & Mirror Area): X [+1.0..+4.5], Y [-3.2..-1.2]
   - Zone 4 (Bottom-Right Enclosed AI Fight Room): X [+6.0..+14.2], Y [-11.8..-3.2] */
#define RANGE_WALL_HEIGHT 6.0f
#define RANGE_CEILING 100.0f
#define RANGE_TEXTURE_SCALE 0.5f
#define RANGE_START_HEIGHT 0.25f
#define RANGE_START_SPACING 0.9f
#define RANGE_START_FACING 0.0f

/* Zone 1: Vehicle & Flight Test Area */
#define RANGE_TRACK_Y0 4.0f
#define RANGE_TRACK_Y1 15.8f
#define RANGE_TRACK_GATE_X0 -10.5f
#define RANGE_TRACK_GATE_X1 -6.5f
#define RANGE_VPAD_X0 -12.0f
#define RANGE_VPAD_X1 -8.5f
#define RANGE_VPAD_Y0 0.2f
#define RANGE_VPAD_Y1 3.2f

/* Zone 2: Gun Range Hall (35m x 2m) */
#define RANGE_GUN_TABLE_X0 0.6f
#define RANGE_GUN_TABLE_X1 1.1f
#define RANGE_GUN_TABLE_Y0 0.55f
#define RANGE_GUN_TABLE_Y1 1.65f
#define RANGE_GUN_TABLE_Z1 0.34f
#define RANGE_GUN_HALL_X0 1.3f
#define RANGE_GUN_HALL_X1 13.8f
#define RANGE_GUN_HALL_Y0 0.25f
#define RANGE_GUN_HALL_Y1 1.95f
#define RANGE_WEAPON_DROP 0.08f
#define RANGE_WEAPON_NEAR 0.8f
#define RANGE_DUMMY_DRIFT 0.25f

/* Zone 3: Mirror & Animations Area */
#define RANGE_MIRROR_X 2.6f
#define RANGE_MIRROR_Y -2.8f
#define RANGE_MIRROR_HALF_WIDTH 1.8f
#define RANGE_MIRROR_DEPTH 2.2f
#define RANGE_MIRROR_LINE_WIDTH 0.1f
#define RANGE_MIRROR_LINE_HEIGHT 0.01f
#define RANGE_MIRROR_DRIFT 0.25f
#define RANGE_MIRROR_POST_WIDTH 0.15f
#define RANGE_MIRROR_FRAME_HEIGHT 1.4f
#define RANGE_MIRROR_BAR_HEIGHT 0.12f
#define RANGE_ANIM_PEDESTAL_X 4.5f
#define RANGE_ANIM_PEDESTAL_Y -2.0f

/* Zone 4: Enclosed AI Fight Room */
#define RANGE_FIGHT_X0 6.0f
#define RANGE_FIGHT_X1 14.2f
#define RANGE_FIGHT_Y0 -11.8f
#define RANGE_FIGHT_Y1 -3.2f
#define RANGE_FIGHT_DOOR_Y0 -6.2f
#define RANGE_FIGHT_DOOR_Y1 -4.4f

#define RANGE_TERMINAL_NEAR 1.65f

static char const range_map_name[] = "levels\\test\\bloodgulch\\bloodgulch";
static struct
{
	unsigned long group_tag;
	char const *path;
} const range_shaders[NUMBER_OF_RANGE_MATERIALS] =
{
	{ SHADER_ENVIRONMENT_GROUP_TAG, "levels\\b30\\shaders\\metal flat generic" },
	{ SHADER_ENVIRONMENT_GROUP_TAG, "levels\\b30\\shaders\\metal panels generic ext" },
	{ SHADER_ENVIRONMENT_GROUP_TAG, "levels\\test\\bloodgulch\\shaders\\cap_metal" },
};

struct range_terminal_info
{
	char const *name;
	char const *header;
	real x;
	real y;
	real z;
	real forward_x;
	real forward_y;
};

static struct range_terminal_info const range_terminals[NUMBER_OF_RANGE_TERMINALS] =
{
	{ "Vehicle Spawner", "VEHICLE SPAWNER TERMINAL", -10.25f, -0.6f, 0.0f, 0.0f, -1.0f },
	{ "Track AI Spawner", "TRACK AI SPAWNER TERMINAL", -5.2f, 3.3f, 0.0f, 0.0f, -1.0f },
	{ "Weapon Dispenser", "WEAPON DISPENSER TERMINAL", 0.2f, 0.1f, 0.0f, -1.0f, 0.0f },
	{ "Target Settings", "GUN RANGE TARGET SETTINGS", 0.2f, 2.1f, 0.0f, -1.0f, 0.0f },
	{ "Fight Room AI", "FIGHT ROOM AI TERMINAL", 5.2f, -4.0f, 0.0f, -1.0f, 0.0f },
};

static short const range_distances_meters[6] = { 5, 10, 15, 20, 25, 30 };

/* ---------- structures */

struct range_structure_bsp_header
{
	void *base_address;
	long vertex_buffer_count;
	void *vertex_buffers;
	long index_buffer_count;
	void *index_buffers;
	unsigned long signature;
};

struct range_collision_leaf
{
	word flags;
	short bsp2d_reference_count;
	long first_bsp2d_reference_index;
};

struct range_bsp2d_reference
{
	long plane_designator;
	long root_index;
};

struct range_cluster
{
	short sky_index;
	short fog_designator;
	short background_sound_palette_index;
	short sound_environment_palette_index;
	short weather_palette_index;
	short transition_structure_bsp_index;
	short first_runtime_decal_index;
	word runtime_decal_count;
	long unused[6];
	struct tag_block predicted_resources;
	struct tag_block subclusters;
	word first_lens_flare_marker_index;
	word lens_flare_marker_count;
	struct tag_block surface_indices;
	struct tag_block mirrors;
	struct tag_block portal_indices;
};

struct range_subcluster
{
	real_rectangle3d world_bounds;
	struct tag_block surface_indices;
};

struct range_surface_reference
{
	long surface_index;
	long bsp3d_node_index;
};

struct range_vertex
{
	real_point3d position;
	real_vector3d normal;
	real_vector3d binormal;
	real_vector3d tangent;
	real_point2d texcoord;
};

struct range_lightmap_vertex
{
	real_vector3d incident_radiosity;
	real_point2d texcoord;
};

typedef char range_structure_bsp_header_size_assert[sizeof(struct range_structure_bsp_header) == 0x18 ? 1 : -1];
typedef char range_collision_leaf_size_assert[sizeof(struct range_collision_leaf) == 0x08 ? 1 : -1];
typedef char range_bsp2d_reference_size_assert[sizeof(struct range_bsp2d_reference) == 0x08 ? 1 : -1];
typedef char range_cluster_size_assert[sizeof(struct range_cluster) == 0x68 ? 1 : -1];
typedef char range_subcluster_size_assert[sizeof(struct range_subcluster) == 0x24 ? 1 : -1];
typedef char range_vertex_size_assert[sizeof(struct range_vertex) == 0x38 ? 1 : -1];
typedef char range_lightmap_vertex_size_assert[sizeof(struct range_lightmap_vertex) == 0x14 ? 1 : -1];

struct range_writer
{
	byte *base;
	unsigned long size;
	unsigned long used;
	boolean full;
};

struct range_face
{
	real_vector3d normal;
	long corners[4];
	short material;
	byte flags;
};

struct range_layout
{
	struct range_face faces[MAXIMUM_RANGE_FACES];
	short face_count;
	real_point3d vertices[MAXIMUM_RANGE_COLLISION_VERTICES];
	short vertex_count;
	real_rectangle3d room;
	short bench_count;
	real_rectangle3d blocks[MAXIMUM_RANGE_BLOCKS];
	short block_count;
	real_plane3d planes[MAXIMUM_RANGE_PLANES];
	short plane_count;
	struct bsp3d_node nodes[MAXIMUM_RANGE_NODES];
	short node_count;
	short face_nodes[MAXIMUM_RANGE_FACES];
};

struct range_material_build
{
	struct range_vertex *vertices;
	struct range_lightmap_vertex *lightmap_vertices;
	long vertex_count;
	long first_surface_index;
	long surface_count;
};

/* ---------- globals */

static struct
{
	boolean requested;
	boolean active;
	char profile[FIRING_RANGE_NAME_LENGTH];
	boolean bsp_written;

	struct scenario *scenario;
	long emptied_counts[16];
	short emptied_count;
	struct player_starting_location starting_locations[MAXIMUM_RANGE_STARTING_LOCATIONS];
	short starting_location_count;
	long players_count;

	/* Weapons available in the Weapon Dispenser */
	long weapons[MAXIMUM_RANGE_WEAPONS];
	char weapon_names[MAXIMUM_RANGE_WEAPONS][RANGE_NAME_LENGTH];
	short weapon_count;
	short selected_weapon;
	boolean equip_directly;
	long table_weapon_object;
	long table_empty_since;
	long next_check_time;

	/* Vehicles available in the Vehicle Spawner */
	long vehicles[MAXIMUM_RANGE_VEHICLES];
	char vehicle_names[MAXIMUM_RANGE_VEHICLES][RANGE_NAME_LENGTH];
	short vehicle_count;
	long spawned_vehicle;

	/* AI Actor Variants available in the Track AI, Target Settings & Fight Room */
	struct
	{
		long variant;
		char name[RANGE_NAME_LENGTH];
	} ais[MAXIMUM_RANGE_AIS];
	short ai_count;

	/* Zone 3: Animation preview & Mirror */
	short anim_ai_index;
	long anim_unit;
	short anim_sequence;
	long mirror_unit;
	long mirrored_unit;

	/* Zone 1: Vehicle Track AI & Boundary State */
	long track_ais[MAXIMUM_RANGE_TRACK_AIS];
	short next_track_ai;
	boolean auto_despawn_track_vehicles;
	boolean player_was_in_track;
	long track_scenery_defs[8];
	short track_scenery_def_count;
	long track_scenery_objs[MAXIMUM_RANGE_SCENERY];
	boolean scenery_spawned;

	/* Zone 2: Gun Range Target & Telemetry */
	short target_distance_index; /* 0..5 -> 5m..30m */
	short target_mode; /* 0: Spartan Dummy, 1: Stationary AI, 2: Active AI */
	short target_ai_index;
	long target_unit;
	long target_first_hit;
	long target_last_hit;
	long target_dead_since;
	real telemetry_total_damage;
	real telemetry_last_dps;
	real telemetry_last_ttk;
	real telemetry_spread_diameter;
	long telemetry_shots_fired;
	long telemetry_shots_hit;
	short spread_count;
	real spread_y[RANGE_SPREAD_SAMPLES];
	real spread_z[RANGE_SPREAD_SAMPLES];
	short last_weapon_rounds;

	/* Zone 4: Enclosed Fight Room AI & Boundary State */
	long fighters[MAXIMUM_RANGE_FIGHTERS];
	short next_fighter;
	short fight_weapon_mode; /* 0: Default, 1: Random, 2: Table Weapon */
	boolean player_was_in_fight_room;
	unsigned long random;

	/* C-Uplink Scenery Objects & Modal UI State */
	long uplink_definition;
	long uplink_objects[NUMBER_OF_RANGE_TERMINALS];
	short near_terminal;
	short open_terminal;
	short modal_cursor;
	short modal_scroll;
	boolean action_held;
	boolean nav_up_held;
	boolean nav_down_held;
	boolean nav_back_held;
} firing_range;

/* ---------- private code: the bsp's room */

static void *range_allocate(
	struct range_writer *writer,
	unsigned long size)
{
	unsigned long start = (writer->used + 15UL) & ~15UL;
	void *result;

	if (writer->full || start > writer->size || size > writer->size - start)
	{
		writer->full = TRUE;
		return NULL;
	}
	result = writer->base + start;
	csmemset(result, 0, size);
	writer->used = start + size;

	return result;
}

static void *range_block(
	struct range_writer *writer,
	struct tag_block *block,
	long count,
	unsigned long element_size)
{
	void *elements = count > 0 ? range_allocate(writer, (unsigned long)count * element_size) : NULL;

	block->count = elements ? count : 0;
	block->address = elements;
	block->definition = NULL;

	return elements;
}

static void range_reference(
	struct tag_reference *reference,
	unsigned long group_tag,
	long index)
{
	reference->group_tag = group_tag;
	reference->index = index;
	reference->name = index == NONE ? NULL : tag_get_name(index);
	reference->name_length = reference->name ? (long)strlen(reference->name) : 0;
}

/* ---------- private code: geometry */

static void range_point(
	real_point3d *point,
	real x,
	real y,
	real z)
{
	point->x = x;
	point->y = y;
	point->z = z;
}

static void range_vector(
	real_vector3d *vector,
	real i,
	real j,
	real k)
{
	vector->i = i;
	vector->j = j;
	vector->k = k;
}

static void range_cross(
	real_vector3d const *a,
	real_vector3d const *b,
	real_vector3d *result)
{
	range_vector(result, a->j * b->k - a->k * b->j, a->k * b->i - a->i * b->k, a->i * b->j - a->j * b->i);
}

static void range_ring_order(
	real_point3d const *points,
	short count,
	real_vector3d const *normal,
	short *order)
{
	real_vector3d u;
	real_vector3d v;
	real_point3d middle;
	real angles[8];
	short index;

	range_point(&middle, 0.0f, 0.0f, 0.0f);
	if (fabsf(normal->k) < 0.9f)
		range_vector(&u, normal->j, -normal->i, 0.0f);
	else
		range_vector(&u, 1.0f, 0.0f, 0.0f);
	{
		real length = sqrtf(u.i * u.i + u.j * u.j + u.k * u.k);

		u.i /= length;
		u.j /= length;
		u.k /= length;
	}
	range_cross(normal, &u, &v);
	for (index = 0; index < count; index++)
	{
		middle.x += points[index].x / count;
		middle.y += points[index].y / count;
		middle.z += points[index].z / count;
	}
	for (index = 0; index < count; index++)
	{
		real dx = points[index].x - middle.x;
		real dy = points[index].y - middle.y;
		real dz = points[index].z - middle.z;

		angles[index] = atan2f(dx * v.i + dy * v.j + dz * v.k, dx * u.i + dy * u.j + dz * u.k);
		order[index] = index;
	}
	for (index = 1; index < count; index++)
	{
		short moved = order[index];
		short place = index;

		while (place > 0 && angles[order[place - 1]] > angles[moved])
		{
			order[place] = order[place - 1];
			place--;
		}
		order[place] = moved;
	}
}

/* ---------- private code: the 4-zone layout */

static void range_tree_make(
	struct range_layout *layout);

static void range_box_face(
	struct range_layout *layout,
	long first_corner,
	short axis,
	boolean high,
	boolean inward,
	short material,
	byte flags)
{
	struct range_face *face = &layout->faces[layout->face_count++];
	real sign = (high ? 1.0f : -1.0f) * (inward ? -1.0f : 1.0f);
	short corner;
	short count = 0;

	range_vector(&face->normal, axis == 0 ? sign : 0.0f, axis == 1 ? sign : 0.0f, axis == 2 ? sign : 0.0f);
	for (corner = 0; corner < BOX_CORNERS; corner++)
	{
		if (((corner >> axis) & 1) == (high ? 1 : 0))
			face->corners[count++] = first_corner + corner;
	}
	face->material = material;
	face->flags = flags;
}

static long range_box_corners(
	struct range_layout *layout,
	real x0,
	real x1,
	real y0,
	real y1,
	real z0,
	real z1)
{
	long first = layout->vertex_count;
	short corner;

	for (corner = 0; corner < BOX_CORNERS; corner++)
		range_point(&layout->vertices[layout->vertex_count++], corner & 1 ? x1 : x0, corner & 2 ? y1 : y0,
			corner & 4 ? z1 : z0);
	return first;
}

static void range_table_weapon_place(
	real_point3d *place)
{
	range_point(place, (RANGE_GUN_TABLE_X0 + RANGE_GUN_TABLE_X1) * 0.5f,
		(RANGE_GUN_TABLE_Y0 + RANGE_GUN_TABLE_Y1) * 0.5f,
		RANGE_GUN_TABLE_Z1 + RANGE_WEAPON_DROP);
}

static void range_layout_make(
	struct range_layout *layout)
{
	real_rectangle3d *room = &layout->room;
	long first;
	short block;

	csmemset(layout, 0, sizeof(*layout));
	room->x0 = -16.0f;
	room->x1 = 16.0f;
	room->y0 = -14.0f;
	room->y1 = 16.0f;
	room->z0 = 0.0f;
	room->z1 = RANGE_CEILING;

	first = range_box_corners(layout, room->x0, room->x1, room->y0, room->y1, room->z0, room->z1);
	range_box_face(layout, first, 2, FALSE, TRUE, 0, 0);
	range_box_face(layout, first, 0, TRUE, TRUE, 1, 0);
	range_box_face(layout, first, 0, FALSE, TRUE, 1, 0);
	range_box_face(layout, first, 1, TRUE, TRUE, 1, 0);
	range_box_face(layout, first, 1, FALSE, TRUE, 1, 0);
	range_box_face(layout, first, 2, TRUE, TRUE, 1, FLAG(1));

	/* 14 Guillotine-Separable Facility Blocks matching the 4 Floor Plans: */
	{
		real_rectangle3d const facility_blocks[NUMBER_OF_FACILITY_BLOCKS] =
		{
			/* Zone 1: North Vehicle & Flight Test Area (Y >= 4.0f) */
			{ -15.8f, RANGE_TRACK_GATE_X0, 4.0f, 4.3f, 0.0f, 1.5f }, /* Track South Wall West of Gate */
			{ RANGE_TRACK_GATE_X1, 15.8f, 4.0f, 4.3f, 0.0f, 1.5f },  /* Track South Wall East of Gate */
			{ -11.0f, -6.0f, 7.2f, 11.2f, 0.0f, 0.22f },             /* Winding Track West Hairpin Island */
			{ -4.0f, 3.0f, 6.2f, 9.6f, 0.0f, 0.22f },                /* Winding Track Center S-Bend Island */
			{ 5.0f, 10.8f, 7.0f, 11.8f, 0.0f, 0.22f },               /* Winding Track East Chicane Island */

			/* Zone 2: Middle-Right 35m x 2m Gun Range Hall (Y in [0.0f, 2.2f]) */
			{ RANGE_GUN_TABLE_X0, RANGE_GUN_TABLE_X1, RANGE_GUN_TABLE_Y0, RANGE_GUN_TABLE_Y1, 0.0f, RANGE_GUN_TABLE_Z1 }, /* Weapon Table */
			{ RANGE_GUN_HALL_X0, RANGE_GUN_HALL_X1, 0.0f, RANGE_GUN_HALL_Y0, 0.0f, 1.5f },   /* Gun Range South Wall */
			{ RANGE_GUN_HALL_X0, RANGE_GUN_HALL_X1, RANGE_GUN_HALL_Y1, 2.2f, 0.0f, 1.5f },   /* Gun Range North Wall */
			{ 13.9f, 14.2f, 0.0f, 2.2f, 0.0f, 1.8f },                                        /* Gun Range East Backstop */

			/* Zone 4: Bottom-Right Enclosed AI Fight Room (Y in [-11.8f, -3.2f]) */
			{ RANGE_FIGHT_X0, RANGE_FIGHT_X1, -3.5f, RANGE_FIGHT_Y1, 0.0f, 1.6f },           /* Fight Room North Wall */
			{ RANGE_FIGHT_X0, RANGE_FIGHT_X1, RANGE_FIGHT_Y0, -11.5f, 0.0f, 1.6f },          /* Fight Room South Wall */
			{ 13.9f, RANGE_FIGHT_X1, -11.4f, -3.6f, 0.0f, 1.6f },                            /* Fight Room East Wall */
			{ RANGE_FIGHT_X0, 6.3f, RANGE_FIGHT_DOOR_Y1, -3.6f, 0.0f, 1.6f },                /* Fight Room West Wall (North of Door) */
			{ RANGE_FIGHT_X0, 6.3f, -11.4f, RANGE_FIGHT_DOOR_Y0, 0.0f, 1.6f },               /* Fight Room West Wall (South of Door) */
		};

		layout->bench_count = 1;
		for (block = 0; block < NUMBER_OF_FACILITY_BLOCKS; block++)
			layout->blocks[layout->block_count++] = facility_blocks[block];
	}
	for (block = 0; block < layout->block_count; block++)
	{
		real_rectangle3d const *bounds = &layout->blocks[block];

		first = range_box_corners(layout, bounds->x0, bounds->x1, bounds->y0, bounds->y1, bounds->z0, bounds->z1);
		range_box_face(layout, first, 2, TRUE, FALSE, 2, 0);
		range_box_face(layout, first, 0, TRUE, FALSE, 2, 0);
		range_box_face(layout, first, 0, FALSE, FALSE, 2, 0);
		range_box_face(layout, first, 1, TRUE, FALSE, 2, 0);
		range_box_face(layout, first, 1, FALSE, FALSE, 2, 0);
	}
	range_tree_make(layout);
}

/* ---------- private code: the collision bsp */

static short range_node_add(
	struct range_layout *layout,
	short plane,
	long back,
	long front)
{
	short node = layout->node_count++;

	layout->nodes[node].plane_designator = plane;
	layout->nodes[node].children[0] = back;
	layout->nodes[node].children[1] = front;
	return node;
}

static void range_block_span(
	real_rectangle3d const *bounds,
	short axis,
	real *low,
	real *high)
{
	*low = axis == 0 ? bounds->x0 : axis == 1 ? bounds->y0 : bounds->z0;
	*high = axis == 0 ? bounds->x1 : axis == 1 ? bounds->y1 : bounds->z1;
}

static long range_blocks_tree(
	struct range_layout *layout,
	short const *blocks,
	short count)
{
	long leaf = (long)(0 | LONG_MIN);
	short axis;

	if (count == 1)
	{
		short face = (short)(NUMBER_OF_ROOM_FACES + blocks[0] * NUMBER_OF_BENCH_FACES);
		short node = layout->node_count;
		short index;

		for (index = 0; index < NUMBER_OF_BENCH_FACES; index++)
		{
			layout->face_nodes[face + index] = range_node_add(layout, (short)(face + index),
				index + 1 < NUMBER_OF_BENCH_FACES ? node + index + 1 : NONE, leaf);
		}
		return node;
	}
	for (axis = 0; axis < 3; axis++)
	{
		short sorted[MAXIMUM_RANGE_BLOCKS];
		short best = NONE;
		real best_plane = 0.0f;
		short index;

		for (index = 0; index < count; index++)
		{
			short place = index;
			real low;
			real high;
			real other_low;

			range_block_span(&layout->blocks[blocks[index]], axis, &low, &high);
			while (place > 0)
			{
				range_block_span(&layout->blocks[sorted[place - 1]], axis, &other_low, &high);
				if (other_low <= low)
					break;
				sorted[place] = sorted[place - 1];
				place--;
			}
			sorted[place] = blocks[index];
		}
		for (index = 1; index < count; index++)
		{
			real below_high = -1.0e9f;
			real above_low;
			real low;
			real high;
			short other;

			for (other = 0; other < index; other++)
			{
				range_block_span(&layout->blocks[sorted[other]], axis, &low, &high);
				below_high = MAX(below_high, high);
			}
			range_block_span(&layout->blocks[sorted[index]], axis, &above_low, &high);
			if (below_high < above_low &&
				(best == NONE || abs(index * 2 - count) < abs(best * 2 - count)))
			{
				best = index;
				best_plane = (below_high + above_low) * 0.5f;
			}
		}
		if (best != NONE)
		{
			short plane = layout->plane_count++;
			short node;

			range_vector(&layout->planes[plane].n, axis == 0 ? 1.0f : 0.0f, axis == 1 ? 1.0f : 0.0f,
				axis == 2 ? 1.0f : 0.0f);
			layout->planes[plane].d = best_plane;
			node = range_node_add(layout, plane, NONE, NONE);
			layout->nodes[node].children[1] = range_blocks_tree(layout, sorted + best, (short)(count - best));
			layout->nodes[node].children[0] = range_blocks_tree(layout, sorted, best);
			return node;
		}
	}
	error(_error_silent, "firing range: %d of the plane's blocks cannot be parted", count);
	return NONE;
}

static void range_tree_make(
	struct range_layout *layout)
{
	short face;

	layout->plane_count = layout->face_count;
	layout->node_count = 0;
	for (face = 0; face < layout->face_count; face++)
	{
		struct range_face const *source = &layout->faces[face];
		real_point3d const *point = &layout->vertices[source->corners[0]];

		layout->planes[face].n = source->normal;
		layout->planes[face].d = source->normal.i * point->x + source->normal.j * point->y +
			source->normal.k * point->z;
	}
	for (face = 0; face < NUMBER_OF_ROOM_FACES; face++)
		layout->face_nodes[face] = range_node_add(layout, face, NONE, face + 1);
	{
		short blocks[MAXIMUM_RANGE_BLOCKS];
		short block;

		for (block = 0; block < layout->block_count; block++)
			blocks[block] = block;
		layout->nodes[NUMBER_OF_ROOM_FACES - 1].children[1] = layout->block_count ?
			range_blocks_tree(layout, blocks, layout->block_count) : (long)(0 | LONG_MIN);
	}
}

static boolean range_collision_bsp_write(
	struct range_writer *writer,
	struct range_layout const *layout,
	struct collision_bsp *bsp)
{
	short face_count = layout->face_count;
	struct bsp3d_node *nodes = range_block(writer, &bsp->bsp3d.nodes, layout->node_count, sizeof(*nodes));
	real_plane3d *planes = range_block(writer, &bsp->bsp3d.planes, layout->plane_count, sizeof(*planes));
	struct range_collision_leaf *leaf = range_block(writer, &bsp->leaves, 1, sizeof(*leaf));
	struct range_bsp2d_reference *references = range_block(writer, &bsp->bsp2d_references, face_count,
		sizeof(*references));
	struct collision_surface *surfaces = range_block(writer, &bsp->surfaces, face_count, sizeof(*surfaces));
	struct collision_edge *edges = range_allocate(writer, MAXIMUM_RANGE_EDGES * sizeof(*edges));
	struct collision_vertex *vertices = range_block(writer, &bsp->vertices, layout->vertex_count, sizeof(*vertices));
	long face_edges[MAXIMUM_RANGE_FACES][4];
	long edge_count = 0;
	short face;
	short vertex;

	if (writer->full)
		return FALSE;
	csmemcpy(nodes, layout->nodes, layout->node_count * sizeof(*nodes));
	csmemcpy(planes, layout->planes, layout->plane_count * sizeof(*planes));
	bsp->bsp2d.nodes.count = 0;
	for (vertex = 0; vertex < layout->vertex_count; vertex++)
	{
		vertices[vertex].point = layout->vertices[vertex];
		vertices[vertex].first_edge_index = NONE;
	}
	for (face = 0; face < face_count; face++)
	{
		struct range_face const *source = &layout->faces[face];
		real_point3d points[4];
		short order[4];
		short index;

		for (index = 0; index < 4; index++)
			points[index] = layout->vertices[source->corners[index]];
		references[face].plane_designator = face;
		references[face].root_index = (long)((unsigned long)face | (unsigned long)LONG_MIN);
		surfaces[face].plane_designator = face;
		surfaces[face].flags = source->flags;
		surfaces[face].breakable_surface_index = (byte)NONE;
		surfaces[face].material_index = source->material;

		range_ring_order(points, 4, &source->normal, order);
		for (index = 0; index < 4; index++)
		{
			long from = source->corners[order[index]];
			long to = source->corners[order[(index + 1) % 4]];
			long edge;

			for (edge = 0; edge < edge_count; edge++)
			{
				if (edges[edge].vertex_indices[0] == to && edges[edge].vertex_indices[1] == from)
					break;
			}
			if (edge == edge_count)
			{
				if (edge_count == MAXIMUM_RANGE_EDGES)
					return FALSE;
				edges[edge].vertex_indices[0] = from;
				edges[edge].vertex_indices[1] = to;
				edges[edge].surface_indices[0] = face;
				edges[edge].surface_indices[1] = NONE;
				edges[edge].edge_indices[0] = NONE;
				edges[edge].edge_indices[1] = NONE;
				edge_count++;
			}
			else
			{
				edges[edge].surface_indices[1] = face;
			}
			face_edges[face][index] = edge;
			if (vertices[from].first_edge_index == NONE)
				vertices[from].first_edge_index = edge;
		}
		surfaces[face].first_edge_index = face_edges[face][0];
	}
	for (face = 0; face < face_count; face++)
	{
		short index;

		for (index = 0; index < 4; index++)
		{
			struct collision_edge *edge = &edges[face_edges[face][index]];

			edge->edge_indices[edge->surface_indices[0] == face ? 0 : 1] = face_edges[face][(index + 1) % 4];
		}
	}
	bsp->edges.count = edge_count;
	bsp->edges.address = edges;
	bsp->edges.definition = NULL;
	leaf->flags = 0;
	leaf->bsp2d_reference_count = face_count;
	leaf->first_bsp2d_reference_index = 0;

	return TRUE;
}

/* ---------- private code: the render surfaces */

static long range_vertex_add(
	struct range_material_build *build,
	real_point3d const *position,
	real_vector3d const *normal,
	real_vector3d const *tangent,
	real u,
	real v)
{
	struct range_vertex *vertex = &build->vertices[build->vertex_count];
	struct range_lightmap_vertex *lightmap_vertex = &build->lightmap_vertices[build->vertex_count];

	vertex->position = *position;
	vertex->normal = *normal;
	vertex->tangent = *tangent;
	range_cross(normal, tangent, &vertex->binormal);
	vertex->texcoord.x = u;
	vertex->texcoord.y = v;
	lightmap_vertex->incident_radiosity = *normal;

	return build->vertex_count++;
}

static void range_quad_add(
	struct structure_surface *surfaces,
	long *surface_index,
	struct range_material_build const *build,
	long const *corners,
	real_vector3d const *normal)
{
	real_point3d points[4];
	short order[4];
	long ring[4];
	short index;

	for (index = 0; index < 4; index++)
		points[index] = build->vertices[corners[index]].position;
	range_ring_order(points, 4, normal, order);
	for (index = 0; index < 4; index++)
		ring[index] = corners[order[index]];
	surfaces[*surface_index].vertex_indices[0] = (word)ring[0];
	surfaces[*surface_index].vertex_indices[1] = (word)ring[2];
	surfaces[*surface_index].vertex_indices[2] = (word)ring[1];
	(*surface_index)++;
	surfaces[*surface_index].vertex_indices[0] = (word)ring[0];
	surfaces[*surface_index].vertex_indices[1] = (word)ring[3];
	surfaces[*surface_index].vertex_indices[2] = (word)ring[2];
	(*surface_index)++;
}

static void range_flat_quad_add(
	struct range_material_build *build,
	struct structure_surface *surfaces,
	long *surface_index,
	real_point3d const *points,
	real_vector3d const *normal)
{
	real_vector3d tangent;
	long corners[4];
	short index;

	if (fabsf(normal->k) > 0.5f)
		range_vector(&tangent, 1.0f, 0.0f, 0.0f);
	else
		range_vector(&tangent, -normal->j, normal->i, 0.0f);
	for (index = 0; index < 4; index++)
	{
		real u = (points[index].x * tangent.i + points[index].y * tangent.j) * RANGE_TEXTURE_SCALE * 4.0f;
		real v = (fabsf(normal->k) > 0.5f ? points[index].y : points[index].z) * RANGE_TEXTURE_SCALE * 4.0f;

		corners[index] = range_vertex_add(build, &points[index], normal, &tangent, u, v);
	}
	range_quad_add(surfaces, surface_index, build, corners, normal);
}

static void range_floor_build(
	struct range_layout const *layout,
	struct range_material_build *build,
	struct structure_surface *surfaces,
	long *surface_index)
{
	real_vector3d normal;
	real_vector3d tangent;
	real tile_x = (layout->room.x1 - layout->room.x0) / RANGE_TILES;
	real tile_y = (layout->room.y1 - layout->room.y0) / RANGE_TILES;
	short row;
	short column;

	range_vector(&normal, 0.0f, 0.0f, 1.0f);
	range_vector(&tangent, 1.0f, 0.0f, 0.0f);
	build->first_surface_index = *surface_index;
	for (row = 0; row <= RANGE_TILES; row++)
	{
		for (column = 0; column <= RANGE_TILES; column++)
		{
			real_point3d position;

			range_point(&position, layout->room.x0 + column * tile_x, layout->room.y0 + row * tile_y, 0.0f);
			range_vertex_add(build, &position, &normal, &tangent, position.x * RANGE_TEXTURE_SCALE,
				position.y * RANGE_TEXTURE_SCALE);
		}
	}
	for (row = 0; row < RANGE_TILES; row++)
	{
		for (column = 0; column < RANGE_TILES; column++)
		{
			long corners[4];

			corners[0] = row * (RANGE_TILES + 1) + column;
			corners[1] = corners[0] + 1;
			corners[2] = corners[0] + RANGE_TILES + 1;
			corners[3] = corners[2] + 1;
			range_quad_add(surfaces, surface_index, build, corners, &normal);
		}
	}
	build->surface_count = *surface_index - build->first_surface_index;
}

static void range_walls_build(
	struct range_layout const *layout,
	struct range_material_build *build,
	struct structure_surface *surfaces,
	long *surface_index)
{
	real_rectangle3d const *room = &layout->room;
	short wall;

	build->first_surface_index = *surface_index;
	for (wall = 0; wall < NUMBER_OF_RANGE_WALLS; wall++)
	{
		real_vector3d normal = layout->faces[_range_face_east + wall].normal;
		real_vector3d tangent;
		long first_vertex = build->vertex_count;
		boolean across_x = fabsf(normal.i) > 0.5f;
		real fixed = across_x ? (normal.i < 0.0f ? room->x1 : room->x0) : (normal.j < 0.0f ? room->y1 : room->y0);
		real sign;
		real start;
		real panel;
		short step;

		range_vector(&tangent, -normal.j, normal.i, 0.0f);
		sign = across_x ? tangent.j : tangent.i;
		start = across_x ? MIN(room->y0 * sign, room->y1 * sign) : MIN(room->x0 * sign, room->x1 * sign);
		panel = (across_x ? room->y1 - room->y0 : room->x1 - room->x0) / RANGE_TILES;
		for (step = 0; step <= RANGE_TILES; step++)
		{
			real along = start + step * panel;
			real_point3d bottom;
			real_point3d top;

			if (across_x)
				range_point(&bottom, fixed, along * sign, 0.0f);
			else
				range_point(&bottom, along * sign, fixed, 0.0f);
			top = bottom;
			top.z = RANGE_WALL_HEIGHT;
			range_vertex_add(build, &bottom, &normal, &tangent, along * RANGE_TEXTURE_SCALE, 0.0f);
			range_vertex_add(build, &top, &normal, &tangent, along * RANGE_TEXTURE_SCALE,
				RANGE_WALL_HEIGHT * RANGE_TEXTURE_SCALE);
		}
		for (step = 0; step < RANGE_TILES; step++)
		{
			long corners[4];

			corners[0] = first_vertex + step * 2;
			corners[1] = corners[0] + 1;
			corners[2] = corners[0] + 2;
			corners[3] = corners[0] + 3;
			range_quad_add(surfaces, surface_index, build, corners, &normal);
		}
	}
	build->surface_count = *surface_index - build->first_surface_index;
}

static void range_box_quads_at(
	struct range_material_build *build,
	struct structure_surface *surfaces,
	long *surface_index,
	real x0,
	real x1,
	real y0,
	real y1,
	real z0,
	real z1,
	boolean include_bottom)
{
	short face;

	for (face = 0; face < 6; face++)
	{
		short axis = (short)(face / 2);
		boolean high = (face & 1) != 0;
		real_point3d corners[8];
		real_point3d points[4];
		real_vector3d normal;
		short corner;
		short count = 0;

		if (!include_bottom && axis == 2 && !high)
			continue;
		range_vector(&normal, axis == 0 ? (high ? 1.0f : -1.0f) : 0.0f, axis == 1 ? (high ? 1.0f : -1.0f) : 0.0f,
			axis == 2 ? (high ? 1.0f : -1.0f) : 0.0f);
		for (corner = 0; corner < BOX_CORNERS; corner++)
		{
			range_point(&corners[corner], corner & 1 ? x1 : x0, corner & 2 ? y1 : y0, corner & 4 ? z1 : z0);
			if (((corner >> axis) & 1) == (high ? 1 : 0))
				points[count++] = corners[corner];
		}
		range_flat_quad_add(build, surfaces, surface_index, points, &normal);
	}
}

/* Constructs an a30-style distant Forerunner Beam Emitter Spire (16 quads) */
static void range_emitter_tower_add(
	struct range_material_build *build,
	struct structure_surface *surfaces,
	long *surface_index,
	real cx,
	real cy)
{
	/* Base Pylon (5 quads) */
	range_box_quads_at(build, surfaces, surface_index, cx - 2.4f, cx + 2.4f, cy - 2.4f, cy + 2.4f, 0.0f, 14.0f, FALSE);
	/* Upper Angled Emitter Head (6 quads) */
	range_box_quads_at(build, surfaces, surface_index, cx - 1.5f, cx + 1.5f, cy - 3.2f, cy + 3.2f, 14.0f, 22.0f, TRUE);
	/* Skyward Energy Beam Column (5 quads reaching to Z = 95.0f) */
	range_box_quads_at(build, surfaces, surface_index, cx - 0.35f, cx + 0.35f, cy - 0.35f, cy + 0.35f, 22.0f, 95.0f, FALSE);
}

static void range_details_build(
	struct range_layout const *layout,
	struct range_material_build *build,
	struct structure_surface *surfaces,
	long *surface_index)
{
	real_point3d points[4];
	real_vector3d up;
	short face;

	build->first_surface_index = *surface_index;
	for (face = NUMBER_OF_ROOM_FACES; face < layout->face_count; face++)
	{
		short index;

		for (index = 0; index < 4; index++)
			points[index] = layout->vertices[layout->faces[face].corners[index]];
		range_flat_quad_add(build, surfaces, surface_index, points, &layout->faces[face].normal);
	}
	range_vector(&up, 0.0f, 0.0f, 1.0f);
	/* Mirror line on floor */
	range_point(&points[0], RANGE_MIRROR_X - RANGE_MIRROR_HALF_WIDTH, RANGE_MIRROR_Y - RANGE_MIRROR_LINE_WIDTH * 0.5f,
		RANGE_MIRROR_LINE_HEIGHT);
	range_point(&points[1], RANGE_MIRROR_X + RANGE_MIRROR_HALF_WIDTH, RANGE_MIRROR_Y - RANGE_MIRROR_LINE_WIDTH * 0.5f,
		RANGE_MIRROR_LINE_HEIGHT);
	range_point(&points[2], RANGE_MIRROR_X + RANGE_MIRROR_HALF_WIDTH, RANGE_MIRROR_Y + RANGE_MIRROR_LINE_WIDTH * 0.5f,
		RANGE_MIRROR_LINE_HEIGHT);
	range_point(&points[3], RANGE_MIRROR_X - RANGE_MIRROR_HALF_WIDTH, RANGE_MIRROR_Y + RANGE_MIRROR_LINE_WIDTH * 0.5f,
		RANGE_MIRROR_LINE_HEIGHT);
	range_flat_quad_add(build, surfaces, surface_index, points, &up);
	/* Vehicle Spawn Pad floor plate */
	range_point(&points[0], RANGE_VPAD_X0, RANGE_VPAD_Y0, RANGE_MIRROR_LINE_HEIGHT);
	range_point(&points[1], RANGE_VPAD_X1, RANGE_VPAD_Y0, RANGE_MIRROR_LINE_HEIGHT);
	range_point(&points[2], RANGE_VPAD_X1, RANGE_VPAD_Y1, RANGE_MIRROR_LINE_HEIGHT);
	range_point(&points[3], RANGE_VPAD_X0, RANGE_VPAD_Y1, RANGE_MIRROR_LINE_HEIGHT);
	range_flat_quad_add(build, surfaces, surface_index, points, &up);
	/* Mirror 3D Frame (16 quads) */
	range_box_quads_at(build, surfaces, surface_index,
		RANGE_MIRROR_X - RANGE_MIRROR_HALF_WIDTH - RANGE_MIRROR_POST_WIDTH,
		RANGE_MIRROR_X - RANGE_MIRROR_HALF_WIDTH,
		RANGE_MIRROR_Y - RANGE_MIRROR_POST_WIDTH * 0.5f,
		RANGE_MIRROR_Y + RANGE_MIRROR_POST_WIDTH * 0.5f,
		0.0f, RANGE_MIRROR_FRAME_HEIGHT, FALSE);
	range_box_quads_at(build, surfaces, surface_index,
		RANGE_MIRROR_X + RANGE_MIRROR_HALF_WIDTH,
		RANGE_MIRROR_X + RANGE_MIRROR_HALF_WIDTH + RANGE_MIRROR_POST_WIDTH,
		RANGE_MIRROR_Y - RANGE_MIRROR_POST_WIDTH * 0.5f,
		RANGE_MIRROR_Y + RANGE_MIRROR_POST_WIDTH * 0.5f,
		0.0f, RANGE_MIRROR_FRAME_HEIGHT, FALSE);
	range_box_quads_at(build, surfaces, surface_index,
		RANGE_MIRROR_X - RANGE_MIRROR_HALF_WIDTH,
		RANGE_MIRROR_X + RANGE_MIRROR_HALF_WIDTH,
		RANGE_MIRROR_Y - RANGE_MIRROR_POST_WIDTH * 0.5f,
		RANGE_MIRROR_Y + RANGE_MIRROR_POST_WIDTH * 0.5f,
		RANGE_MIRROR_FRAME_HEIGHT - RANGE_MIRROR_BAR_HEIGHT, RANGE_MIRROR_FRAME_HEIGHT, TRUE);
	/* 3 Distant a30-style Forerunner Beam Emitter Spire Structures */
	range_emitter_tower_add(build, surfaces, surface_index, -28.0f, 28.0f);
	range_emitter_tower_add(build, surfaces, surface_index, 28.0f, 30.0f);
	range_emitter_tower_add(build, surfaces, surface_index, 30.0f, -24.0f);
	build->surface_count = *surface_index - build->first_surface_index;
}
static void range_bounds_add(
	real_rectangle3d *bounds,
	real_point3d const *point,
	boolean first)
{
	if (first)
	{
		bounds->x0 = bounds->x1 = point->x;
		bounds->y0 = bounds->y1 = point->y;
		bounds->z0 = bounds->z1 = point->z;
		return;
	}
	bounds->x0 = MIN(bounds->x0, point->x);
	bounds->x1 = MAX(bounds->x1, point->x);
	bounds->y0 = MIN(bounds->y0, point->y);
	bounds->y1 = MAX(bounds->y1, point->y);
	bounds->z0 = MIN(bounds->z0, point->z);
	bounds->z1 = MAX(bounds->z1, point->z);
}

static boolean range_subcluster_write(
	struct range_writer *writer,
	struct range_subcluster *subcluster,
	struct structure_surface const *surfaces,
	struct range_material_build const *build,
	long const *surface_list,
	long count)
{
	long *indices = range_block(writer, &subcluster->surface_indices, count, sizeof(long));
	long index;

	if (!indices)
		return FALSE;
	for (index = 0; index < count; index++)
	{
		struct structure_surface const *surface = &surfaces[surface_list[index]];
		short corner;

		indices[index] = surface_list[index];
		for (corner = 0; corner < 3; corner++)
		{
			range_bounds_add(&subcluster->world_bounds, &build->vertices[surface->vertex_indices[corner]].position,
				index == 0 && corner == 0);
		}
	}
	return TRUE;
}

/* ---------- private code: the bsp */

static long range_shader(
	short material)
{
	long shader = tag_loaded(range_shaders[material].group_tag, range_shaders[material].path);

	if (shader == NONE && material == 2)
		shader = tag_loaded(range_shaders[1].group_tag, range_shaders[1].path);
	if (shader == NONE)
		error(_error_silent, "firing range: the map has no shader %s", range_shaders[material].path);
	return shader;
}

static short range_shader_material_type(
	long shader)
{
	struct shader *definition = shader == NONE ? NULL : (struct shader *)tag_get(SHADER_ENVIRONMENT_GROUP_TAG, shader);

	return definition ? definition->base.physics.material_type : 0;
}

static void range_default_lighting(
	struct render_lighting *lighting)
{
	lighting->ambient_color.red = 0.35f;
	lighting->ambient_color.green = 0.38f;
	lighting->ambient_color.blue = 0.44f;
	lighting->distant_light_count = 2;
	lighting->distant_lights[0].color.red = 0.88f;
	lighting->distant_lights[0].color.green = 0.90f;
	lighting->distant_lights[0].color.blue = 0.95f;
	range_vector(&lighting->distant_lights[0].direction, 0.38f, 0.3f, -0.87f);
	lighting->distant_lights[1].color.red = 0.22f;
	lighting->distant_lights[1].color.green = 0.28f;
	lighting->distant_lights[1].color.blue = 0.36f;
	range_vector(&lighting->distant_lights[1].direction, -0.38f, -0.3f, -0.87f);
	lighting->reflection_tint_color.alpha = 1.0f;
	lighting->reflection_tint_color.red = 0.5f;
	lighting->reflection_tint_color.green = 0.55f;
	lighting->reflection_tint_color.blue = 0.6f;
	range_vector(&lighting->shadow_vector, 0.38f, 0.3f, -0.87f);
	lighting->shadow_color.red = 0.4f;
	lighting->shadow_color.green = 0.42f;
	lighting->shadow_color.blue = 0.48f;
}

static void range_material_counts(
	struct range_layout const *layout,
	short material,
	long *vertices,
	long *surfaces)
{
	long quads = (layout->face_count - NUMBER_OF_ROOM_FACES) + RANGE_FLAT_DETAIL_QUADS +
		RANGE_MIRROR_FRAME_QUADS + RANGE_EMITTER_TOWER_QUADS;

	switch (material)
	{
	case 0:
		*vertices = RANGE_FLOOR_VERTICES;
		*surfaces = RANGE_FLOOR_SURFACES;
		break;
	case 1:
		*vertices = NUMBER_OF_RANGE_WALLS * RANGE_WALL_VERTICES;
		*surfaces = NUMBER_OF_RANGE_WALLS * RANGE_WALL_SURFACES;
		break;
	case 2:
		*vertices = quads * 4;
		*surfaces = quads * 2;
		break;
	default:
		*vertices = 4;
		*surfaces = 2;
		break;
	}
}

static boolean range_structure_bsp_build(
	struct range_writer *writer,
	struct range_layout const *layout,
	struct structure_bsp **result)
{
	struct structure_bsp *bsp = range_allocate(writer, sizeof(*bsp));
	struct collision_bsp *collision = bsp ? range_block(writer, &bsp->collision_bsp, 1, sizeof(*collision)) : NULL;
	long shaders[NUMBER_OF_RANGE_MATERIALS];
	struct structure_collision_material *collision_materials;
	struct structure_surface *surfaces;
	struct structure_lightmap *lightmap;
	struct structure_material *materials;
	struct range_material_build builds[NUMBER_OF_RANGE_MATERIALS];
	struct range_cluster *cluster;
	struct range_subcluster *subclusters;
	struct structure_leaf *leaf;
	struct range_surface_reference *references;
	byte_rectangle3d *nodes;
	unsigned long *visibility;
	long surface_count = 0;
	long surface_index = 0;
	short material_count = NUMBER_OF_RANGE_MATERIALS;
	short material_index;
	long index;

	*result = bsp;
	if (!collision || !range_collision_bsp_write(writer, layout, collision))
		return FALSE;
	for (material_index = 0; material_index < NUMBER_OF_RANGE_MATERIALS; material_index++)
	{
		if ((shaders[material_index] = range_shader(material_index)) == NONE)
			return FALSE;
	}

	range_reference(&bsp->lightmap_group, BITMAP_GROUP_TAG, NONE);
	range_default_lighting(&bsp->default_lighting);
	bsp->world_bounds = layout->room;

	for (material_index = 0; material_index < material_count; material_index++)
	{
		long vertex_count;
		long material_surfaces;

		range_material_counts(layout, material_index, &vertex_count, &material_surfaces);
		surface_count += material_surfaces;
	}
	collision_materials = range_block(writer, &bsp->collision_materials, NUMBER_OF_RANGE_COLLISION_MATERIALS,
		sizeof(*collision_materials));
	surfaces = range_block(writer, &bsp->surfaces, surface_count, sizeof(*surfaces));
	lightmap = range_block(writer, &bsp->lightmaps, 1, sizeof(*lightmap));
	materials = lightmap ? range_block(writer, &lightmap->materials, material_count, sizeof(*materials)) : NULL;
	if (!collision_materials || !surfaces || !materials)
		return FALSE;
	for (material_index = 0; material_index < material_count; material_index++)
	{
		long vertex_count;
		long material_surfaces;
		unsigned long vertex_bytes;
		byte *vertex_data;

		range_material_counts(layout, material_index, &vertex_count, &material_surfaces);
		vertex_bytes = (unsigned long)vertex_count *
			(sizeof(struct range_vertex) + sizeof(struct range_lightmap_vertex));
		if (!(vertex_data = range_allocate(writer, vertex_bytes)))
			return FALSE;
		if (material_index < NUMBER_OF_RANGE_COLLISION_MATERIALS)
		{
			range_reference(&collision_materials[material_index].shader, SHADER_ENVIRONMENT_GROUP_TAG,
				shaders[material_index]);
			collision_materials[material_index].runtime_physics_material_type =
				range_shader_material_type(shaders[material_index]);
		}
		builds[material_index].vertices = (struct range_vertex *)vertex_data;
		builds[material_index].lightmap_vertices = (struct range_lightmap_vertex *)(vertex_data +
			(unsigned long)vertex_count * sizeof(struct range_vertex));
		builds[material_index].vertex_count = 0;
		materials[material_index].uncompressed_vertex_data.size = (long)vertex_bytes;
		materials[material_index].uncompressed_vertex_data.address = vertex_data;
	}
	range_floor_build(layout, &builds[0], surfaces, &surface_index);
	range_walls_build(layout, &builds[1], surfaces, &surface_index);
	range_details_build(layout, &builds[2], surfaces, &surface_index);
	if (surface_index != surface_count)
		return FALSE;
	lightmap->bitmap_index = NONE;
	for (material_index = 0; material_index < material_count; material_index++)
	{
		struct structure_material *material = &materials[material_index];

		range_reference(&material->shader, range_shaders[material_index].group_tag, shaders[material_index]);
		material->first_surface_index = builds[material_index].first_surface_index;
		material->surface_count = builds[material_index].surface_count;
		material->breakable_surface_index = NONE;
		material->vertices.type = _rasterizer_vertex_type_environment_uncompressed;
		material->vertices.count = builds[material_index].vertex_count;
		material->lightmap_vertices.type = _rasterizer_vertex_type_environment_lightmap_uncompressed;
		material->lightmap_vertices.count = builds[material_index].vertex_count;
		range_point(&material->centroid, 0.0f, 0.0f, material_index == 1 ? RANGE_WALL_HEIGHT * 0.5f : 0.0f);
		material->lighting = bsp->default_lighting;
	}

	cluster = range_block(writer, &bsp->clusters, 1, sizeof(*cluster));
	subclusters = cluster ? range_block(writer, &cluster->subclusters, RANGE_ROOM_SUBCLUSTERS + 1,
		sizeof(*subclusters)) : NULL;
	visibility = range_allocate(writer, sizeof(unsigned long));
	if (!subclusters || !visibility)
		return FALSE;
	cluster->sky_index = 0;
	cluster->fog_designator = NONE;
	cluster->background_sound_palette_index = NONE;
	cluster->sound_environment_palette_index = NONE;
	cluster->weather_palette_index = NONE;
	cluster->transition_structure_bsp_index = NONE;
	cluster->first_runtime_decal_index = NONE;
	*visibility = 1;
	bsp->cluster_data.size = sizeof(unsigned long);
	bsp->cluster_data.address = visibility;
	for (index = 0; index < RANGE_SUBCLUSTERS * RANGE_SUBCLUSTERS; index++)
	{
		long list[(RANGE_TILES / RANGE_SUBCLUSTERS) * (RANGE_TILES / RANGE_SUBCLUSTERS) * 2];
		long count = 0;
		short block_row = (short)(index / RANGE_SUBCLUSTERS);
		short block_column = (short)(index % RANGE_SUBCLUSTERS);
		short row;
		short column;

		for (row = 0; row < RANGE_TILES / RANGE_SUBCLUSTERS; row++)
		{
			for (column = 0; column < RANGE_TILES / RANGE_SUBCLUSTERS; column++)
			{
				long tile = (block_row * (RANGE_TILES / RANGE_SUBCLUSTERS) + row) * RANGE_TILES +
					block_column * (RANGE_TILES / RANGE_SUBCLUSTERS) + column;

				list[count++] = builds[0].first_surface_index + tile * 2;
				list[count++] = builds[0].first_surface_index + tile * 2 + 1;
			}
		}
		if (!range_subcluster_write(writer, &subclusters[index], surfaces, &builds[0], list, count))
			return FALSE;
	}
	for (index = 0; index < NUMBER_OF_RANGE_WALLS * RANGE_SUBCLUSTERS; index++)
	{
		long list[RANGE_WALL_SURFACES / RANGE_SUBCLUSTERS];
		long first = builds[1].first_surface_index + index * (RANGE_WALL_SURFACES / RANGE_SUBCLUSTERS);
		long count;

		for (count = 0; count < RANGE_WALL_SURFACES / RANGE_SUBCLUSTERS; count++)
			list[count] = first + count;
		if (!range_subcluster_write(writer, &subclusters[RANGE_SUBCLUSTERS * RANGE_SUBCLUSTERS + index], surfaces,
			&builds[1], list, count))
			return FALSE;
	}
	{
		long list[MAXIMUM_RANGE_DETAIL_QUADS * 2];
		long count;

		for (count = 0; count < builds[2].surface_count; count++)
			list[count] = builds[2].first_surface_index + count;
		if (!range_subcluster_write(writer, &subclusters[RANGE_ROOM_SUBCLUSTERS], surfaces, &builds[2], list, count))
			return FALSE;
	}

	{
		byte *pathfinding = range_block(writer, &bsp->pathfinding_surfaces, layout->face_count, sizeof(byte));

		if (!pathfinding)
			return FALSE;
		pathfinding[_range_face_floor] = FLAG(6);
	}

	leaf = range_block(writer, &bsp->leaves, 1, sizeof(*leaf));
	references = range_block(writer, &bsp->surface_references, surface_count, sizeof(*references));
	nodes = range_block(writer, &bsp->nodes, layout->node_count, sizeof(*nodes));
	if (!leaf || !references || !nodes)
		return FALSE;
	leaf->bounds.x0 = leaf->bounds.y0 = leaf->bounds.z0 = 0;
	leaf->bounds.x1 = leaf->bounds.y1 = leaf->bounds.z1 = 0xFF;
	leaf->cluster_index = 0;
	leaf->surface_reference_count = (short)surface_count;
	leaf->first_surface_reference_index = 0;
	for (index = 0; index < surface_count; index++)
	{
		long detail = index - builds[2].first_surface_index;

		references[index].surface_index = index;
		if (index < RANGE_FLOOR_SURFACES)
			references[index].bsp3d_node_index = layout->face_nodes[_range_face_floor];
		else if (detail < 0)
			references[index].bsp3d_node_index = layout->face_nodes[_range_face_east +
				(index - RANGE_FLOOR_SURFACES) / RANGE_WALL_SURFACES];
		else if (detail < (layout->face_count - NUMBER_OF_ROOM_FACES) * 2)
			references[index].bsp3d_node_index = layout->face_nodes[NUMBER_OF_ROOM_FACES + detail / 2];
		else
			references[index].bsp3d_node_index = layout->face_nodes[_range_face_floor];
	}
	for (index = 0; index < layout->node_count; index++)
	{
		nodes[index].x0 = nodes[index].y0 = nodes[index].z0 = 0;
		nodes[index].x1 = nodes[index].y1 = nodes[index].z1 = 0xFF;
	}

	return !writer->full;
}

/* ---------- private code: the scenario & tag roster scan */

static struct tag_block *range_emptied_block(
	struct scenario *scenario,
	short index)
{
	switch (index)
	{
	case 0: return &scenario->scenery;
	case 1: return &scenario->bipeds;
	case 2: return &scenario->vehicles;
	case 3: return &scenario->equipment;
	case 4: return &scenario->weapons;
	case 5: return &scenario->machines;
	case 6: return &scenario->controls;
	case 7: return &scenario->light_fixtures;
	case 8: return &scenario->sound_scenery;
	case 9: return &scenario->netgame_flags;
	case 10: return &scenario->netgame_equipment;
	case 11: return &scenario->decals;
	}
	return NULL;
}

static void range_format_display_name(
	char const *path,
	char *out,
	long out_size)
{
	char const *leaf = path ? tag_name_strip_path(path) : "unknown";
	long i;
	boolean cap = TRUE;

	for (i = 0; leaf[i] && i + 1 < out_size; i++)
	{
		char c = leaf[i];

		if (c == '_' || c == '-')
		{
			out[i] = ' ';
			cap = TRUE;
		}
		else if (cap && c >= 'a' && c <= 'z')
		{
			out[i] = (char)(c - 'a' + 'A');
			cap = FALSE;
		}
		else
		{
			out[i] = c;
			cap = (c == ' ');
		}
	}
	out[i] = 0;
}

/* Scans all loaded tags in the map so stock/Firefight/donor tags and custom
   injected tags are all available in the 5 C-Uplink computer terminals */
static void range_scan_loaded_tags(
	void)
{
	struct tag_iterator iterator;
	long tag_index;

	firing_range.uplink_definition = tag_loaded(SCENERY_GROUP_TAG, "scenery\\c_uplink\\c_uplink");
	if (firing_range.uplink_definition == NONE)
		firing_range.uplink_definition = tag_loaded(SCENERY_GROUP_TAG, "levels\\a30\\scenery\\c_uplink\\c_uplink");

	/* 1. Weapons */
	tag_iterator_new(&iterator, WEAPON_GROUP_TAG);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		char const *name = tag_get_name(tag_index);
		short k;

		if (!name || strstr(name, "flag") || strstr(name, "ball") || strstr(name, "target") ||
			strstr(name, "gun_turret") || strstr(name, "wraith") || strstr(name, "scorpion") ||
			strstr(name, "banshee") || strstr(name, "ghost") || strstr(name, "warthog") ||
			strstr(name, "pelican") || strstr(name, "dropship"))
			continue;
		for (k = 0; k < firing_range.weapon_count; k++)
		{
			if (firing_range.weapons[k] == tag_index)
				break;
		}
		if (k < firing_range.weapon_count || firing_range.weapon_count >= MAXIMUM_RANGE_WEAPONS)
			continue;
		range_format_display_name(name, firing_range.weapon_names[firing_range.weapon_count], RANGE_NAME_LENGTH);
		firing_range.weapons[firing_range.weapon_count++] = tag_index;
	}

	/* 2. Vehicles */
	firing_range.vehicle_count = 0;
	tag_iterator_new(&iterator, VEHICLE_GROUP_TAG);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		char const *name = tag_get_name(tag_index);
		short k;

		if (!name || strstr(name, "pelican") || strstr(name, "dropship"))
			continue;
		for (k = 0; k < firing_range.vehicle_count; k++)
		{
			if (firing_range.vehicles[k] == tag_index)
				break;
		}
		if (k < firing_range.vehicle_count || firing_range.vehicle_count >= MAXIMUM_RANGE_VEHICLES)
			continue;
		range_format_display_name(name, firing_range.vehicle_names[firing_range.vehicle_count], RANGE_NAME_LENGTH);
		firing_range.vehicles[firing_range.vehicle_count++] = tag_index;
	}

	/* 3. Actor Variants */
	tag_iterator_new(&iterator, ACTOR_VARIANT_GROUP_TAG);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		char const *name = tag_get_name(tag_index);
		struct actor_variant_definition *variant = actor_variant_definition_get(tag_index);
		short k;

		if (!name || !variant || variant->unit_reference.index == NONE)
			continue;
		for (k = 0; k < firing_range.ai_count; k++)
		{
			if (firing_range.ais[k].variant == tag_index)
				break;
		}
		if (k < firing_range.ai_count || firing_range.ai_count >= MAXIMUM_RANGE_AIS)
			continue;
		range_format_display_name(name, firing_range.ais[firing_range.ai_count].name, RANGE_NAME_LENGTH);
		firing_range.ais[firing_range.ai_count].variant = tag_index;
		firing_range.ai_count++;
	}

	/* 4. Scenery (trees, rocks, bushes for the Vehicle Track Infield) */
	firing_range.track_scenery_def_count = 0;
	tag_iterator_new(&iterator, SCENERY_GROUP_TAG);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		char const *name = tag_get_name(tag_index);

		if (!name)
			continue;
		if (firing_range.uplink_definition == NONE && strstr(name, "c_uplink"))
			firing_range.uplink_definition = tag_index;
		if (firing_range.track_scenery_def_count < 8 &&
			(strstr(name, "tree") || strstr(name, "rock") || strstr(name, "bush") ||
			 strstr(name, "boulder") || strstr(name, "shrub")))
		{
			firing_range.track_scenery_defs[firing_range.track_scenery_def_count++] = tag_index;
		}
	}
}

static void range_scenario_patch(
	struct scenario *scenario)
{
	struct tag_block *block;
	short index;

	firing_range.scenario = scenario;
	firing_range.emptied_count = 0;
	while ((block = range_emptied_block(scenario, firing_range.emptied_count)) != NULL &&
		firing_range.emptied_count < (short)NUMBEROF(firing_range.emptied_counts))
	{
		firing_range.emptied_counts[firing_range.emptied_count++] = block->count;
		block->count = 0;
	}
	firing_range.players_count = scenario->players.count;
	firing_range.starting_location_count = (short)MIN(scenario->players.count, MAXIMUM_RANGE_STARTING_LOCATIONS);
	scenario->players.count = firing_range.starting_location_count;
	for (index = 0; index < firing_range.starting_location_count; index++)
	{
		struct player_starting_location *location = TAG_BLOCK_GET_ELEMENT(&scenario->players, index,
			struct player_starting_location);

		firing_range.starting_locations[index] = *location;
		range_point(&location->position, -2.5f - (index / 4) * RANGE_START_SPACING,
			0.5f + ((index % 4) - 1.5f) * RANGE_START_SPACING, RANGE_START_HEIGHT);
		location->facing = RANGE_START_FACING;
		location->structure_bsp_reference_index = 0;
	}
	range_scan_loaded_tags();
	error(_error_silent, "firing range: %s plays the 4-zone Forerunner range (%d weapons, %d vehicles, %d AI variants)",
		range_map_name, firing_range.weapon_count, firing_range.vehicle_count, firing_range.ai_count);
}

static void range_scenario_restore(
	void)
{
	struct scenario *scenario = firing_range.scenario;
	short index;

	if (!scenario)
		return;
	for (index = 0; index < firing_range.emptied_count; index++)
		range_emptied_block(scenario, index)->count = firing_range.emptied_counts[index];
	for (index = 0; index < firing_range.starting_location_count; index++)
		*TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location) =
			firing_range.starting_locations[index];
	scenario->players.count = firing_range.players_count;
	firing_range.scenario = NULL;
	firing_range.emptied_count = 0;
	firing_range.starting_location_count = 0;
}

static boolean range_map_is(
	char const *scenario_name)
{
	char const *leaf = scenario_name ? strrchr(scenario_name, '\\') : NULL;
	char const *range_leaf = strrchr(range_map_name, '\\') + 1;

	leaf = leaf ? leaf + 1 : scenario_name;
	return leaf && !csstrcasecmp(leaf, range_leaf);
}

static void range_hud_say(
	char const *text)
{
	wchar_t wide[128];
	short index;

	for (index = 0; text[index] && index < (short)NUMBEROF(wide) - 1; index++)
		wide[index] = (wchar_t)(unsigned char)text[index];
	wide[index] = 0;
	hud_print_message(0, wide);
}

static void range_unit_disarm(
	long unit_index)
{
	struct unit_datum *unit = unit_get(unit_index);
	short slot;

	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long weapon = unit->unit.weapon_object_indices[slot];

		if (weapon == NONE)
			continue;
		unit_network_forget_weapon(unit_index, slot);
		object_delete(weapon);
	}
}

/* ---------- private code: C-Uplink terminals & Track Scenery ---------- */

static void range_scenery_and_uplinks_update(
	void)
{
	short t;

	/* Spawn the 5 C-Uplink computer terminals */
	if (firing_range.uplink_definition != NONE)
	{
		for (t = 0; t < NUMBER_OF_RANGE_TERMINALS; t++)
		{
			if (firing_range.uplink_objects[t] == NONE ||
				!object_try_and_get_and_verify_type(firing_range.uplink_objects[t], _object_mask_scenery))
			{
				struct object_placement_data placement;

				object_placement_data_new(&placement, firing_range.uplink_definition, NONE);
				range_point(&placement.position, range_terminals[t].x, range_terminals[t].y, range_terminals[t].z);
				range_vector(&placement.forward, range_terminals[t].forward_x, range_terminals[t].forward_y, 0.0f);
				range_vector(&placement.up, 0.0f, 0.0f, 1.0f);
				firing_range.uplink_objects[t] = object_new(&placement);
			}
		}
	}

	/* Spawn trees, rocks, and bushes inside the Vehicle Track Infield */
	if (!firing_range.scenery_spawned && firing_range.track_scenery_def_count > 0)
	{
		static struct { real x; real y; } const spots[9] =
		{
			{ -9.5f, 10.2f }, { -8.2f, 8.2f }, { -6.5f, 9.5f },
			{ -2.8f, 7.8f },  { 0.0f, 8.2f },  { 2.2f, 7.2f },
			{ 6.2f, 8.0f },   { 8.5f, 10.8f }, { 9.8f, 8.4f },
		};
		short i;

		for (i = 0; i < 9 && i < MAXIMUM_RANGE_SCENERY; i++)
		{
			struct object_placement_data placement;
			long def = firing_range.track_scenery_defs[i % firing_range.track_scenery_def_count];

			object_placement_data_new(&placement, def, NONE);
			range_point(&placement.position, spots[i].x, spots[i].y, 0.22f);
			range_vector(&placement.forward, 1.0f, 0.0f, 0.0f);
			range_vector(&placement.up, 0.0f, 0.0f, 1.0f);
			firing_range.track_scenery_objs[i] = object_new(&placement);
		}
		firing_range.scenery_spawned = TRUE;
	}
}

/* ---------- private code: Zone 2 Gun Range Weapon Table & Target ---------- */

static long range_weapon_lying(
	long definition_index,
	real_point3d const *place)
{
	struct object_iterator iterator;

	object_iterator_new(&iterator, _object_mask_weapon, 0);
	while (object_iterator_next(&iterator))
	{
		struct weapon_datum *weapon = weapon_get(iterator.index);
		real dx = weapon->object.position.x - place->x;
		real dy = weapon->object.position.y - place->y;
		real dz = weapon->object.position.z - place->z;

		if (weapon->definition_index == definition_index && weapon->object.parent_object_index == NONE &&
			TEST_FLAG(weapon->object.flags, _object_connected_to_map_bit) &&
			dx * dx + dy * dy + dz * dz < RANGE_WEAPON_NEAR * RANGE_WEAPON_NEAR)
		{
			return iterator.index;
		}
	}
	return NONE;
}

static long range_table_weapon_lay(
	void)
{
	struct object_placement_data placement;
	real_point3d place;

	if (firing_range.weapon_count <= 0)
		return NONE;
	if (firing_range.selected_weapon < 0 || firing_range.selected_weapon >= firing_range.weapon_count)
		firing_range.selected_weapon = 0;
	range_table_weapon_place(&place);
	object_placement_data_new(&placement, firing_range.weapons[firing_range.selected_weapon], NONE);
	placement.position = place;
	range_vector(&placement.forward, 0.0f, 1.0f, 0.0f);
	range_vector(&placement.up, 0.0f, 0.0f, 1.0f);
	return object_new(&placement);
}

static void range_dispense_weapon(
	short weapon_index)
{
	char text[160];

	if (weapon_index < 0 || weapon_index >= firing_range.weapon_count)
		return;
	firing_range.selected_weapon = weapon_index;
	if (firing_range.table_weapon_object != NONE &&
		object_try_and_get_and_verify_type(firing_range.table_weapon_object, _object_mask_weapon) &&
		weapon_get(firing_range.table_weapon_object)->object.parent_object_index == NONE)
	{
		object_delete(firing_range.table_weapon_object);
	}
	firing_range.table_weapon_object = range_table_weapon_lay();
	firing_range.table_empty_since = NONE;

	if (firing_range.equip_directly && local_player_get_player_index(0) != NONE)
	{
		long player_unit = player_get(local_player_get_player_index(0))->unit_index;

		if (player_unit != NONE && object_try_and_get_and_verify_type(player_unit, _object_mask_unit))
		{
			struct unit_datum *u = unit_get(player_unit);
			short slot = u->unit.current_weapon_index >= 0 && u->unit.current_weapon_index < 2 ?
				u->unit.current_weapon_index : 0;
			struct object_placement_data placement;
			long w_obj;

			if (u->unit.weapon_object_indices[slot] != NONE)
			{
				long old_w = u->unit.weapon_object_indices[slot];

				unit_network_forget_weapon(player_unit, slot);
				object_delete(old_w);
			}
			object_placement_data_new(&placement, firing_range.weapons[weapon_index], NONE);
			placement.position = u->object.position;
			w_obj = object_new(&placement);
			if (w_obj != NONE)
			{
				unit_network_add_weapon(player_unit, w_obj, slot);
				u->unit.desired_weapon_index = slot;
			}
		}
	}
	snprintf(text, sizeof(text), "WEAPON DISPENSER: %s dispensed on Gun Range table.",
		firing_range.weapon_names[weapon_index]);
	range_hud_say(text);
}

static void range_weapons_update(
	long now)
{
	real_point3d place;

	if (firing_range.weapon_count <= 0)
		return;
	range_table_weapon_place(&place);
	firing_range.table_weapon_object = range_weapon_lying(
		firing_range.weapons[firing_range.selected_weapon], &place);
	if (firing_range.table_weapon_object != NONE)
	{
		firing_range.table_empty_since = NONE;
		return;
	}
	if (firing_range.table_empty_since == NONE)
		firing_range.table_empty_since = now - (firing_range.next_check_time ? 0 : RANGE_REFILL_TICKS);
	if (now - firing_range.table_empty_since >= RANGE_REFILL_TICKS)
	{
		firing_range.table_weapon_object = range_table_weapon_lay();
		firing_range.table_empty_since = NONE;
	}
}

static void range_target_place(
	real_point3d *place)
{
	short idx = firing_range.target_distance_index;
	real meters;
	real dist_units;

	if (idx < 0 || idx > 5)
		idx = 2;
	meters = (real)range_distances_meters[idx];
	dist_units = meters * 0.35f; /* 5m -> 1.75u, 30m -> 10.5u along the 12.5u (35m) hall */
	range_point(place, RANGE_GUN_HALL_X0 + dist_units,
		(RANGE_GUN_HALL_Y0 + RANGE_GUN_HALL_Y1) * 0.5f, RANGE_START_HEIGHT);
}

static long range_dummy_definition(
	void)
{
	struct game_globals *globals = scenario_get_game_globals();
	long player_index = local_player_get_player_index(0);
	long unit_index = player_index == NONE ? NONE : player_get(player_index)->unit_index;

	if (globals && globals->multiplayer_information.count > 0 && globals->multiplayer_information.address)
	{
		long unit = TAG_BLOCK_GET_ELEMENT(&globals->multiplayer_information, 0,
			struct game_globals_multiplayer_information)->unit.index;

		if (unit != NONE)
			return unit;
	}
	return unit_index == NONE ? NONE : unit_get(unit_index)->definition_index;
}

static void range_reset_telemetry(
	void)
{
	firing_range.target_first_hit = NONE;
	firing_range.target_last_hit = NONE;
	firing_range.telemetry_total_damage = 0.0f;
	firing_range.telemetry_last_dps = 0.0f;
	firing_range.telemetry_last_ttk = 0.0f;
	firing_range.telemetry_spread_diameter = 0.0f;
	firing_range.telemetry_shots_fired = 0;
	firing_range.telemetry_shots_hit = 0;
	firing_range.spread_count = 0;
}

static long range_target_spawn(
	void)
{
	real_point3d place;
	real_vector3d forward;
	long unit_index = NONE;

	firing_range.target_first_hit = NONE;
	firing_range.target_last_hit = NONE;
	firing_range.target_dead_since = NONE;
	range_target_place(&place);
	range_vector(&forward, -1.0f, 0.0f, 0.0f);

	if (firing_range.target_mode == 2 && firing_range.ai_count > 0)
	{
		short ai = firing_range.target_ai_index;

		if (ai < 0 || ai >= firing_range.ai_count)
			ai = 0;
		unit_index = actor_port_place_free(firing_range.ais[ai].variant, &place, &forward);
		if (unit_index != NONE && object_try_and_get_and_verify_type(unit_index, _object_mask_unit))
			range_unit_disarm(unit_index);
		return unit_index;
	}
	else if (firing_range.target_mode == 1 && firing_range.ai_count > 0)
	{
		short ai = firing_range.target_ai_index;
		struct actor_variant_definition *variant;
		struct object_placement_data placement;

		if (ai < 0 || ai >= firing_range.ai_count)
			ai = 0;
		variant = actor_variant_definition_get(firing_range.ais[ai].variant);
		if (variant && variant->unit_reference.index != NONE)
		{
			object_placement_data_new(&placement, variant->unit_reference.index, NONE);
			placement.position = place;
			placement.forward = forward;
			range_vector(&placement.up, 0.0f, 0.0f, 1.0f);
			unit_index = object_new(&placement);
			if (unit_index != NONE && object_try_and_get_and_verify_type(unit_index, _object_mask_unit))
			{
				actor_customize_unit(firing_range.ais[ai].variant, unit_index);
				range_unit_disarm(unit_index);
				unit_get(unit_index)->object.owner_team_index = _game_team_covenant;
				SET_FLAG(unit_get(unit_index)->unit.flags, _unit_ignored_by_actors_bit, TRUE);
				return unit_index;
			}
		}
	}

	/* Default (mode 0): Spartan Multiplayer Target Dummy */
	{
		long definition = range_dummy_definition();
		struct object_placement_data placement;

		if (definition == NONE)
			return NONE;
		object_placement_data_new(&placement, definition, NONE);
		placement.position = place;
		placement.forward = forward;
		range_vector(&placement.up, 0.0f, 0.0f, 1.0f);
		unit_index = object_new(&placement);
		if (unit_index == NONE || !object_try_and_get_and_verify_type(unit_index, _object_mask_unit))
		{
			if (unit_index != NONE)
				object_delete(unit_index);
			firing_range.target_dead_since = game_time_get();
			return NONE;
		}
		range_unit_disarm(unit_index);
		unit_get(unit_index)->object.owner_team_index = _game_team_covenant;
		SET_FLAG(unit_get(unit_index)->unit.flags, _unit_ignored_by_actors_bit, TRUE);
		return unit_index;
	}
}

static void range_target_respawn_now(
	void)
{
	if (firing_range.target_unit != NONE &&
		object_try_and_get_and_verify_type(firing_range.target_unit, _object_mask_unit))
	{
		range_unit_disarm(firing_range.target_unit);
		object_delete(firing_range.target_unit);
	}
	firing_range.target_unit = range_target_spawn();
}

static void range_dummies_update(
	long now)
{
	struct unit_datum *unit = firing_range.target_unit == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(firing_range.target_unit, _object_mask_unit);
	real_point3d place;
	real dx;
	real dy;

	/* Track shots fired by monitoring local player's active weapon rounds */
	if (local_player_get_player_index(0) != NONE)
	{
		long p_unit = player_get(local_player_get_player_index(0))->unit_index;

		if (p_unit != NONE && object_try_and_get_and_verify_type(p_unit, _object_mask_unit))
		{
			struct unit_datum *pu = unit_get(p_unit);
			short w_slot = pu->unit.current_weapon_index;
			long w_idx = (w_slot >= 0 && w_slot < MAXIMUM_WEAPONS_PER_UNIT) ?
				pu->unit.weapon_object_indices[w_slot] : NONE;

			if (w_idx != NONE && object_try_and_get_and_verify_type(w_idx, _object_mask_weapon))
			{
				short rounds = weapon_get(w_idx)->weapon.magazines[0].rounds_loaded;

				if (firing_range.last_weapon_rounds >= 0 && rounds < firing_range.last_weapon_rounds)
					firing_range.telemetry_shots_fired += (firing_range.last_weapon_rounds - rounds);
				firing_range.last_weapon_rounds = rounds;
			}
			else
			{
				firing_range.last_weapon_rounds = -1;
			}
		}
	}

	if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
	{
		if (unit && firing_range.target_dead_since == NONE)
		{
			long first_hit = firing_range.target_first_hit;
			char text[160];

			firing_range.target_dead_since = now;
			if (first_hit == NONE || now <= first_hit)
			{
				firing_range.telemetry_last_ttk = 0.03f;
				snprintf(text, sizeof(text), "GUN RANGE: Target eliminated instantly (1 tick) | Spread: %.2fm",
					(double)firing_range.telemetry_spread_diameter);
			}
			else
			{
				firing_range.telemetry_last_ttk = (real)(now - first_hit) / TICKS_PER_SECOND;
				firing_range.telemetry_last_dps = firing_range.telemetry_total_damage /
					MAX(firing_range.telemetry_last_ttk, 0.03f);
				snprintf(text, sizeof(text), "GUN RANGE: TTK %.2fs | DPS %.1f | Spread %.2fm",
					(double)firing_range.telemetry_last_ttk,
					(double)firing_range.telemetry_last_dps,
					(double)firing_range.telemetry_spread_diameter);
			}
			range_hud_say(text);
			return;
		}
		if (firing_range.target_dead_since != NONE &&
			now - firing_range.target_dead_since < RANGE_DUMMY_RESPAWN_TICKS)
			return;
		if (unit)
			object_delete(firing_range.target_unit);
		firing_range.target_unit = range_target_spawn();
		return;
	}

	if (firing_range.target_last_hit != NONE &&
		now - firing_range.target_last_hit >= RANGE_DUMMY_HEAL_TICKS)
	{
		unit->object.body_vitality = 1.0f;
		if (unit->object.maximum_shield_vitality > 0.0f)
			unit->object.shield_vitality = 1.0f;
		unit->object.shield_stun_ticks = 0;
		SET_FLAG(unit->object.damage_flags, _object_shield_depleted_bit, FALSE);
		firing_range.target_first_hit = NONE;
		firing_range.target_last_hit = NONE;
	}

	/* Keep target within Gun Range Hall */
	range_target_place(&place);
	if (firing_range.target_mode != 2)
	{
		dx = unit->object.position.x - place.x;
		dy = unit->object.position.y - place.y;
		if (dx * dx + dy * dy > RANGE_DUMMY_DRIFT * RANGE_DUMMY_DRIFT)
		{
			real_vector3d forward;
			real_vector3d up;

			place.z = unit->object.position.z;
			range_vector(&forward, -1.0f, 0.0f, 0.0f);
			range_vector(&up, 0.0f, 0.0f, 1.0f);
			object_set_position(firing_range.target_unit, &place, &forward, &up);
		}
	}
	else
	{
		/* Active AI inside Gun Range Hall: clamp inside hall walls */
		real cx = PIN(unit->object.position.x, RANGE_GUN_HALL_X0 + 0.4f, RANGE_GUN_HALL_X1 - 0.4f);
		real cy = PIN(unit->object.position.y, RANGE_GUN_HALL_Y0 + 0.35f, RANGE_GUN_HALL_Y1 - 0.35f);

		if (cx != unit->object.position.x || cy != unit->object.position.y)
		{
			real_point3d clamped;

			range_point(&clamped, cx, cy, unit->object.position.z);
			object_set_position(firing_range.target_unit, &clamped, &unit->object.forward, &unit->object.up);
		}
	}
}

/* ---------- private code: Zone 3 Mirror & Animation Pedestal ---------- */

static void range_mirror_point(
	real_point3d const *point,
	real_point3d *result)
{
	range_point(result, point->x, 2.0f * RANGE_MIRROR_Y - point->y, point->z);
}

static void range_mirror_vector(
	real_vector3d const *vector,
	real_vector3d *result)
{
	range_vector(result, vector->i, -vector->j, vector->k);
}

static boolean range_before_mirror(
	struct unit_datum const *unit)
{
	real_point3d const *position = &unit->object.position;

	return position->y > RANGE_MIRROR_Y && position->y < RANGE_MIRROR_Y + RANGE_MIRROR_DEPTH &&
		fabsf(position->x - RANGE_MIRROR_X) < RANGE_MIRROR_HALF_WIDTH;
}

static void range_mirror_delete(
	void)
{
	if (firing_range.mirror_unit != NONE &&
		object_try_and_get_and_verify_type(firing_range.mirror_unit, _object_mask_unit))
	{
		unit_delete_all_weapons(firing_range.mirror_unit);
		object_delete(firing_range.mirror_unit);
	}
	firing_range.mirror_unit = NONE;
	firing_range.mirrored_unit = NONE;
}

static void range_mirror_create(
	long unit_index)
{
	struct unit_datum *unit = unit_get(unit_index);
	struct object_placement_data placement;
	long mirror;

	object_placement_data_new(&placement, unit->definition_index, NONE);
	range_mirror_point(&unit->object.position, &placement.position);
	range_mirror_vector(&unit->object.forward, &placement.forward);
	range_vector(&placement.up, 0.0f, 0.0f, 1.0f);
	mirror = object_new(&placement);
	if (mirror == NONE || !object_try_and_get_and_verify_type(mirror, _object_mask_unit))
	{
		if (mirror != NONE)
			object_delete(mirror);
		return;
	}
	unit_set_actively_controlled(mirror, TRUE);
	firing_range.mirror_unit = mirror;
	firing_range.mirrored_unit = unit_index;
}

static void range_mirror_equip(
	long unit_index)
{
	struct unit_datum *unit = unit_get(unit_index);
	struct unit_datum *mirror = unit_get(firing_range.mirror_unit);
	short slot;

	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long held = unit->unit.weapon_object_indices[slot];
		long copied = mirror->unit.weapon_object_indices[slot];
		long wanted = held == NONE ? NONE : weapon_get(held)->definition_index;
		struct object_placement_data placement;
		long copy;

		if (wanted == (copied == NONE ? NONE : weapon_get(copied)->definition_index))
			continue;
		if (copied != NONE)
		{
			unit_network_forget_weapon(firing_range.mirror_unit, slot);
			object_delete(copied);
		}
		if (wanted == NONE)
			continue;
		object_placement_data_new(&placement, wanted, NONE);
		placement.position = mirror->object.position;
		copy = object_new(&placement);
		if (copy != NONE)
			unit_network_add_weapon(firing_range.mirror_unit, copy, slot);
	}
	if (unit->unit.current_weapon_index >= 0 && unit->unit.current_weapon_index < MAXIMUM_WEAPONS_PER_UNIT &&
		(mirror->unit.current_weapon_index == NONE ||
		mirror->unit.weapon_object_indices[mirror->unit.current_weapon_index] == NONE))
	{
		mirror->unit.desired_weapon_index = unit->unit.current_weapon_index;
	}
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long held = unit->unit.weapon_object_indices[slot];
		long copied = mirror->unit.weapon_object_indices[slot];

		if (held != NONE && copied != NONE)
		{
			struct weapon_datum *from = weapon_get(held);
			struct weapon_datum *to = weapon_get(copied);

			csmemcpy(to->weapon.magazines, from->weapon.magazines, sizeof(to->weapon.magazines));
			to->weapon.heat = from->weapon.heat;
		}
	}
	csmemcpy(mirror->unit.grenade_counts, unit->unit.grenade_counts, sizeof(mirror->unit.grenade_counts));
}

static void range_mirror_update(
	void)
{
	long player_index = local_player_get_player_index(0);
	long unit_index = player_index == NONE ? NONE : player_get(player_index)->unit_index;
	struct unit_datum *unit = unit_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	boolean before = unit && unit->object.parent_object_index == NONE && range_before_mirror(unit);

	if (firing_range.mirror_unit != NONE &&
		(!object_try_and_get_and_verify_type(firing_range.mirror_unit, _object_mask_unit) ||
		firing_range.mirrored_unit != unit_index))
	{
		range_mirror_delete();
	}
	if (!before)
	{
		range_mirror_delete();
		return;
	}
	if (firing_range.mirror_unit == NONE)
		range_mirror_create(unit_index);
	if (firing_range.mirror_unit != NONE)
		range_mirror_equip(unit_index);
}

static void range_anim_pedestal_update(
	void)
{
	struct unit_datum *unit;
	long graph_index;
	struct animation_graph *graph;
	short tries;

	if (firing_range.ai_count <= 0)
		return;
	if (firing_range.anim_ai_index < 0 || firing_range.anim_ai_index >= firing_range.ai_count)
		firing_range.anim_ai_index = 0;

	if (firing_range.anim_unit == NONE ||
		!object_try_and_get_and_verify_type(firing_range.anim_unit, _object_mask_unit))
	{
		struct actor_variant_definition *variant =
			actor_variant_definition_get(firing_range.ais[firing_range.anim_ai_index].variant);
		struct object_placement_data placement;

		if (!variant || variant->unit_reference.index == NONE)
			return;
		object_placement_data_new(&placement, variant->unit_reference.index, NONE);
		range_point(&placement.position, RANGE_ANIM_PEDESTAL_X, RANGE_ANIM_PEDESTAL_Y, RANGE_START_HEIGHT);
		range_vector(&placement.forward, -1.0f, 0.0f, 0.0f);
		range_vector(&placement.up, 0.0f, 0.0f, 1.0f);
		firing_range.anim_unit = object_new(&placement);
		if (firing_range.anim_unit == NONE ||
			!object_try_and_get_and_verify_type(firing_range.anim_unit, _object_mask_unit))
		{
			firing_range.anim_unit = NONE;
			return;
		}
		actor_customize_unit(firing_range.ais[firing_range.anim_ai_index].variant, firing_range.anim_unit);
		range_unit_disarm(firing_range.anim_unit);
		firing_range.anim_sequence = NONE;
	}

	unit = unit_get(firing_range.anim_unit);
	graph_index = unit->object.animation.animation_graph_index;
	graph = graph_index == NONE ? NULL : animation_graph_definition_get(graph_index);
	if (graph && graph->animations.count > 0 && !unit_is_playing_custom_animation(firing_range.anim_unit))
	{
		for (tries = 0; tries < graph->animations.count; tries++)
		{
			short next = (short)((firing_range.anim_sequence + 1 + graph->animations.count) % graph->animations.count);
			struct animation *animation = TAG_BLOCK_GET_ELEMENT(&graph->animations, next, struct animation);

			firing_range.anim_sequence = next;
			if (animation->type != _animation_base ||
				animation_graph_get_animation_by_name(graph_index, animation->name) != next)
				continue;
			if (unit_start_user_animation(firing_range.anim_unit, graph_index, animation->name, TRUE))
				break;
		}
	}
	{
		real dx = unit->object.position.x - RANGE_ANIM_PEDESTAL_X;
		real dy = unit->object.position.y - RANGE_ANIM_PEDESTAL_Y;

		if (dx * dx + dy * dy > 0.05f * 0.05f)
		{
			real_point3d place;
			real_vector3d forward;
			real_vector3d up;

			range_point(&place, RANGE_ANIM_PEDESTAL_X, RANGE_ANIM_PEDESTAL_Y, unit->object.position.z);
			range_vector(&forward, -1.0f, 0.0f, 0.0f);
			range_vector(&up, 0.0f, 0.0f, 1.0f);
			object_set_position(firing_range.anim_unit, &place, &forward, &up);
		}
	}
}

/* ---------- private code: Zone 1 Vehicle Track & Zone 4 Fight Room ---------- */

static void range_spawn_vehicle(
	short vehicle_index)
{
	struct object_placement_data placement;
	char text[160];

	if (firing_range.spawned_vehicle != NONE &&
		object_try_and_get_and_verify_type(firing_range.spawned_vehicle, _object_mask_vehicle))
	{
		object_delete(firing_range.spawned_vehicle);
		firing_range.spawned_vehicle = NONE;
	}
	if (vehicle_index < 0 || vehicle_index >= firing_range.vehicle_count)
	{
		range_hud_say("VEHICLE SPAWNER: Active vehicle despawned.");
		return;
	}
	object_placement_data_new(&placement, firing_range.vehicles[vehicle_index], NONE);
	range_point(&placement.position, (RANGE_VPAD_X0 + RANGE_VPAD_X1) * 0.5f,
		(RANGE_VPAD_Y0 + RANGE_VPAD_Y1) * 0.5f, 0.45f);
	range_vector(&placement.forward, 0.0f, 1.0f, 0.0f);
	range_vector(&placement.up, 0.0f, 0.0f, 1.0f);
	firing_range.spawned_vehicle = object_new(&placement);
	snprintf(text, sizeof(text), "VEHICLE SPAWNER: %s deployed on pad.", firing_range.vehicle_names[vehicle_index]);
	range_hud_say(text);
}

static void range_track_clear(
	boolean clear_vehicles)
{
	struct object_iterator iterator;
	long cleared[MAXIMUM_RANGE_CLEARED];
	short cleared_count = 0;
	short i;

	for (i = 0; i < MAXIMUM_RANGE_TRACK_AIS; i++)
	{
		if (firing_range.track_ais[i] != NONE &&
			object_try_and_get_and_verify_type(firing_range.track_ais[i], _object_mask_unit))
		{
			range_unit_disarm(firing_range.track_ais[i]);
			object_delete(firing_range.track_ais[i]);
		}
		firing_range.track_ais[i] = NONE;
	}

	object_iterator_new(&iterator, _object_mask_unit | _object_mask_weapon | _object_mask_vehicle |
		_object_mask_equipment | _object_mask_garbage, 0);
	while (cleared_count < MAXIMUM_RANGE_CLEARED && object_iterator_next(&iterator))
	{
		struct object_datum *obj = object_get(iterator.index);

		if (obj->object.parent_object_index != NONE)
			continue;
		if (obj->object.position.y < RANGE_TRACK_Y0)
			continue;
		if (TEST_FLAG(_object_mask_vehicle, obj->object.type))
		{
			if (!clear_vehicles)
				continue;
		}
		else if (TEST_FLAG(_object_mask_unit, obj->object.type))
		{
			if (local_player_get_player_index(0) != NONE &&
				player_get(local_player_get_player_index(0))->unit_index == iterator.index)
				continue;
		}
		cleared[cleared_count++] = iterator.index;
	}
	for (i = 0; i < cleared_count; i++)
	{
		if (firing_range.spawned_vehicle == cleared[i])
			firing_range.spawned_vehicle = NONE;
		object_delete(cleared[i]);
	}
}

static void range_track_spawn_ai(
	short ai)
{
	real_point3d place;
	real_vector3d forward;
	long unit_index;
	long prev;
	char text[160];

	if (ai < 0 || ai >= firing_range.ai_count)
		return;
	prev = firing_range.track_ais[firing_range.next_track_ai];
	if (prev != NONE && object_try_and_get_and_verify_type(prev, _object_mask_unit))
	{
		range_unit_disarm(prev);
		object_delete(prev);
	}
	firing_range.track_ais[firing_range.next_track_ai] = NONE;
	range_point(&place, -4.5f + (firing_range.next_track_ai % 4) * 3.0f, 12.2f, RANGE_START_HEIGHT);
	range_vector(&forward, 0.0f, -1.0f, 0.0f);
	unit_index = actor_port_place_free(firing_range.ais[ai].variant, &place, &forward);
	if (unit_index == NONE)
	{
		range_hud_say("TRACK AI: Could not spawn actor.");
		return;
	}
	firing_range.track_ais[firing_range.next_track_ai] = unit_index;
	firing_range.next_track_ai = (short)((firing_range.next_track_ai + 1) % MAXIMUM_RANGE_TRACK_AIS);
	snprintf(text, sizeof(text), "TRACK AI: %s deployed inside Vehicle Track.", firing_range.ais[ai].name);
	range_hud_say(text);
}

static void range_fight_room_clear(
	void)
{
	struct object_iterator iterator;
	long cleared[MAXIMUM_RANGE_CLEARED];
	short cleared_count = 0;
	short i;

	for (i = 0; i < MAXIMUM_RANGE_FIGHTERS; i++)
	{
		if (firing_range.fighters[i] != NONE &&
			object_try_and_get_and_verify_type(firing_range.fighters[i], _object_mask_unit))
		{
			range_unit_disarm(firing_range.fighters[i]);
			object_delete(firing_range.fighters[i]);
		}
		firing_range.fighters[i] = NONE;
	}

	object_iterator_new(&iterator, _object_mask_unit | _object_mask_weapon | _object_mask_equipment |
		_object_mask_garbage, 0);
	while (cleared_count < MAXIMUM_RANGE_CLEARED && object_iterator_next(&iterator))
	{
		struct object_datum *obj = object_get(iterator.index);

		if (obj->object.parent_object_index != NONE)
			continue;
		if (obj->object.position.x < RANGE_FIGHT_X0 - 0.5f || obj->object.position.x > RANGE_FIGHT_X1 + 0.5f ||
			obj->object.position.y < RANGE_FIGHT_Y0 - 0.5f || obj->object.position.y > RANGE_FIGHT_Y1 + 0.5f)
			continue;
		if (TEST_FLAG(_object_mask_unit, obj->object.type) &&
			local_player_get_player_index(0) != NONE &&
			player_get(local_player_get_player_index(0))->unit_index == iterator.index)
			continue;
		cleared[cleared_count++] = iterator.index;
	}
	for (i = 0; i < cleared_count; i++)
		object_delete(cleared[i]);
}

static void range_ai_fight(
	short ai)
{
	real_point3d place;
	real_vector3d forward;
	long unit_index;
	long previous;
	char text[160];

	if (ai < 0 || ai >= firing_range.ai_count)
		return;
	previous = firing_range.fighters[firing_range.next_fighter];
	if (previous != NONE && object_try_and_get_and_verify_type(previous, _object_mask_unit))
	{
		range_unit_disarm(previous);
		object_delete(previous);
	}
	firing_range.fighters[firing_range.next_fighter] = NONE;
	range_point(&place, (RANGE_FIGHT_X0 + RANGE_FIGHT_X1) * 0.5f + 1.0f,
		(RANGE_FIGHT_Y0 + RANGE_FIGHT_Y1) * 0.5f, RANGE_START_HEIGHT);
	range_vector(&forward, -1.0f, 0.0f, 0.0f);
	unit_index = actor_port_place_free(firing_range.ais[ai].variant, &place, &forward);
	if (unit_index == NONE)
	{
		range_hud_say("FIGHT ROOM: AI could not be placed.");
		return;
	}
	if (firing_range.fight_weapon_mode > 0 && firing_range.weapon_count > 0)
	{
		short weapon = firing_range.selected_weapon;
		struct object_placement_data placement;
		long weapon_index;

		if (firing_range.fight_weapon_mode == 1)
		{
			firing_range.random = firing_range.random * 1103515245UL + 12345UL + (unsigned long)game_time_get();
			weapon = (short)((firing_range.random >> 16) % (unsigned long)firing_range.weapon_count);
		}
		if (weapon >= 0 && weapon < firing_range.weapon_count)
		{
			range_unit_disarm(unit_index);
			object_placement_data_new(&placement, firing_range.weapons[weapon], NONE);
			placement.position = place;
			weapon_index = object_new(&placement);
			if (weapon_index != NONE)
			{
				unit_network_add_weapon(unit_index, weapon_index, 0);
				unit_get(unit_index)->unit.desired_weapon_index = 0;
			}
		}
	}
	firing_range.fighters[firing_range.next_fighter] = unit_index;
	firing_range.next_fighter = (short)((firing_range.next_fighter + 1) % MAXIMUM_RANGE_FIGHTERS);
	/*Also update Zone 3 animation preview to this AI species*/
	firing_range.anim_ai_index = ai;
	if (firing_range.anim_unit != NONE &&
		object_try_and_get_and_verify_type(firing_range.anim_unit, _object_mask_unit))
	{
		object_delete(firing_range.anim_unit);
		firing_range.anim_unit = NONE;
	}
	snprintf(text, sizeof(text), "FIGHT ROOM: Active %s spawned!", firing_range.ais[ai].name);
	range_hud_say(text);
}

/* Enforces Zone 1 (Vehicle Track) and Zone 4 (Fight Room) AI containment and
   auto-kill / auto-despawn when the player exits the zone */
static void range_zones_boundary_update(
	void)
{
	long player_index = local_player_get_player_index(0);
	long unit_index = player_index == NONE ? NONE : player_get(player_index)->unit_index;
	struct unit_datum *player_unit = unit_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	short i;

	/* Clamp Track AI inside Zone 1 (Y >= 4.4f) so they cannot exit gate */
	for (i = 0; i < MAXIMUM_RANGE_TRACK_AIS; i++)
	{
		long ai_unit = firing_range.track_ais[i];

		if (ai_unit != NONE && object_try_and_get_and_verify_type(ai_unit, _object_mask_unit))
		{
			struct unit_datum *u = unit_get(ai_unit);
			real cx = PIN(u->object.position.x, -15.2f, 15.2f);
			real cy = PIN(u->object.position.y, RANGE_TRACK_Y0 + 0.5f, RANGE_TRACK_Y1 - 0.5f);

			if (cx != u->object.position.x || cy != u->object.position.y)
			{
				real_point3d clamped;

				range_point(&clamped, cx, cy, u->object.position.z);
				object_set_position(ai_unit, &clamped, &u->object.forward, &u->object.up);
			}
		}
	}

	/* Clamp Fight Room AI inside Zone 4 so they cannot leave doorway */
	for (i = 0; i < MAXIMUM_RANGE_FIGHTERS; i++)
	{
		long ai_unit = firing_range.fighters[i];

		if (ai_unit != NONE && object_try_and_get_and_verify_type(ai_unit, _object_mask_unit))
		{
			struct unit_datum *u = unit_get(ai_unit);
			real cx = PIN(u->object.position.x, RANGE_FIGHT_X0 + 0.45f, RANGE_FIGHT_X1 - 0.45f);
			real cy = PIN(u->object.position.y, RANGE_FIGHT_Y0 + 0.45f, RANGE_FIGHT_Y1 - 0.45f);

			if (cx != u->object.position.x || cy != u->object.position.y)
			{
				real_point3d clamped;

				range_point(&clamped, cx, cy, u->object.position.z);
				object_set_position(ai_unit, &clamped, &u->object.forward, &u->object.up);
			}
		}
	}

	if (!player_unit)
		return;

	{
		struct object_datum *pos_obj = (player_unit->object.parent_object_index != NONE &&
			object_try_and_get_and_verify_type(player_unit->object.parent_object_index, 0xFFFFFFFFUL)) ?
			object_get(player_unit->object.parent_object_index) : &player_unit->object;
		real px = pos_obj->object.position.x;
		real py = pos_obj->object.position.y;
		boolean in_track = (py > RANGE_TRACK_Y0 + 0.2f);
		boolean in_fight_room = (px > RANGE_FIGHT_X0 + 0.1f && px < RANGE_FIGHT_X1 &&
			py > RANGE_FIGHT_Y0 && py < RANGE_FIGHT_Y1);

		if (firing_range.player_was_in_track && !in_track && py < RANGE_TRACK_Y0 - 0.2f)
		{
			range_track_clear(firing_range.auto_despawn_track_vehicles);
			firing_range.player_was_in_track = FALSE;
			range_hud_say(firing_range.auto_despawn_track_vehicles ?
				"TRACK EXIT: Track AI killed and vehicles despawned." :
				"TRACK EXIT: Track AI killed.");
		}
		else if (in_track)
		{
			firing_range.player_was_in_track = TRUE;
		}

		if (firing_range.player_was_in_fight_room && !in_fight_room &&
			(px < RANGE_FIGHT_X0 - 0.2f || py > RANGE_FIGHT_Y1 + 0.2f || py < RANGE_FIGHT_Y0 - 0.2f))
		{
			range_fight_room_clear();
			firing_range.player_was_in_fight_room = FALSE;
			range_hud_say("FIGHT ROOM EXIT: Active AI terminated.");
		}
		else if (in_fight_room)
		{
			firing_range.player_was_in_fight_room = TRUE;
		}
	}
}

/* ---------- private code: C-Uplink Modal Menu Logic & Drawing ---------- */

static short range_terminal_near(
	struct unit_datum const *unit)
{
	short best = NONE;
	real best_dist = RANGE_TERMINAL_NEAR * RANGE_TERMINAL_NEAR;
	short t;

	for (t = 0; t < NUMBER_OF_RANGE_TERMINALS; t++)
	{
		real dx = unit->object.position.x - range_terminals[t].x;
		real dy = unit->object.position.y - range_terminals[t].y;
		real d2 = dx * dx + dy * dy;

		if (d2 < best_dist)
		{
			best = t;
			best_dist = d2;
		}
	}
	return best;
}

static short range_modal_item_count(
	short terminal)
{
	switch (terminal)
	{
	case _range_terminal_vehicle_spawner:
		return (short)(1 + firing_range.vehicle_count);
	case _range_terminal_track_ai_spawner:
		return (short)(2 + firing_range.ai_count);
	case _range_terminal_weapon_dispenser:
		return (short)(1 + firing_range.weapon_count);
	case _range_terminal_target_settings:
		return (short)(4 + firing_range.ai_count);
	case _range_terminal_fight_room_ai:
		return (short)(2 + firing_range.ai_count);
	}
	return 0;
}

static void range_modal_item_label(
	short terminal,
	short row,
	char *out,
	long out_size)
{
	out[0] = 0;
	switch (terminal)
	{
	case _range_terminal_vehicle_spawner:
		if (row == 0)
			snprintf(out, out_size, "[Despawn Active Vehicle]");
		else if (row - 1 < firing_range.vehicle_count)
			snprintf(out, out_size, "Spawn %s", firing_range.vehicle_names[row - 1]);
		break;

	case _range_terminal_track_ai_spawner:
		if (row == 0)
			snprintf(out, out_size, "Auto-Despawn Vehicles on Exit: %s",
				firing_range.auto_despawn_track_vehicles ? "ON" : "OFF");
		else if (row == 1)
			snprintf(out, out_size, "[Clear Track AI & Bodies]");
		else if (row - 2 < firing_range.ai_count)
			snprintf(out, out_size, "Spawn Track AI: %s", firing_range.ais[row - 2].name);
		break;

	case _range_terminal_weapon_dispenser:
		if (row == 0)
			snprintf(out, out_size, "Dispense Mode: %s",
				firing_range.equip_directly ? "Table + Equip to Player" : "Spawn on Table Only");
		else if (row - 1 < firing_range.weapon_count)
			snprintf(out, out_size, "%s%s",
				(row - 1 == firing_range.selected_weapon) ? "* " : "",
				firing_range.weapon_names[row - 1]);
		break;

	case _range_terminal_target_settings:
		if (row == 0)
			snprintf(out, out_size, "Target Range: %dm (Click to Cycle 5m-30m)",
				range_distances_meters[firing_range.target_distance_index]);
		else if (row == 1)
		{
			char const *const modes[3] = { "Spartan Dummy", "Stationary AI Biped", "Active AI Biped" };
			snprintf(out, out_size, "Target Mode: %s", modes[firing_range.target_mode % 3]);
		}
		else if (row == 2)
			snprintf(out, out_size, "[Reset Telemetry Stats (DPS / Spread / Accuracy)]");
		else if (row == 3)
			snprintf(out, out_size, "Use Spartan Multiplayer Target Dummy");
		else if (row - 4 < firing_range.ai_count)
			snprintf(out, out_size, "Target Biped: %s", firing_range.ais[row - 4].name);
		break;

	case _range_terminal_fight_room_ai:
		if (row == 0)
		{
			char const *const wmodes[3] = { "Default AI Weapon", "Random Weapon", "Current Table Weapon" };
			snprintf(out, out_size, "AI Loadout: %s", wmodes[firing_range.fight_weapon_mode % 3]);
		}
		else if (row == 1)
			snprintf(out, out_size, "[Clear Fight Room AI & Bodies]");
		else if (row - 2 < firing_range.ai_count)
			snprintf(out, out_size, "Fight %s", firing_range.ais[row - 2].name);
		break;
	}
}

static void range_modal_activate(
	short terminal,
	short row)
{
	switch (terminal)
	{
	case _range_terminal_vehicle_spawner:
		if (row == 0)
			range_spawn_vehicle(NONE);
		else
			range_spawn_vehicle((short)(row - 1));
		firing_range.open_terminal = NONE;
		break;

	case _range_terminal_track_ai_spawner:
		if (row == 0)
			firing_range.auto_despawn_track_vehicles = !firing_range.auto_despawn_track_vehicles;
		else if (row == 1)
		{
			range_track_clear(FALSE);
			range_hud_say("TRACK AI: Cleared all AI in Vehicle Track.");
		}
		else
		{
			range_track_spawn_ai((short)(row - 2));
			firing_range.open_terminal = NONE;
		}
		break;

	case _range_terminal_weapon_dispenser:
		if (row == 0)
			firing_range.equip_directly = !firing_range.equip_directly;
		else
		{
			range_dispense_weapon((short)(row - 1));
			firing_range.open_terminal = NONE;
		}
		break;

	case _range_terminal_target_settings:
		if (row == 0)
		{
			firing_range.target_distance_index = (short)((firing_range.target_distance_index + 1) % 6);
			range_target_respawn_now();
		}
		else if (row == 1)
		{
			firing_range.target_mode = (short)((firing_range.target_mode + 1) % 3);
			range_target_respawn_now();
		}
		else if (row == 2)
		{
			range_reset_telemetry();
			range_hud_say("GUN RANGE: Telemetry stats reset.");
		}
		else if (row == 3)
		{
			firing_range.target_mode = 0;
			range_target_respawn_now();
		}
		else if (row - 4 < firing_range.ai_count)
		{
			firing_range.target_ai_index = (short)(row - 4);
			if (firing_range.target_mode == 0)
				firing_range.target_mode = 1;
			range_target_respawn_now();
		}
		break;

	case _range_terminal_fight_room_ai:
		if (row == 0)
			firing_range.fight_weapon_mode = (short)((firing_range.fight_weapon_mode + 1) % 3);
		else if (row == 1)
		{
			range_fight_room_clear();
			range_hud_say("FIGHT ROOM: Cleared all AI and dropped weapons.");
		}
		else
		{
			range_ai_fight((short)(row - 2));
			firing_range.open_terminal = NONE;
		}
		break;
	}
}

/* Halo 1 CE Escape-menu styled rounded box (#2896ff 2px border, #0a2649 -> #050d1c
   75% vertical gradient, optional black 75% header strip) */
static void range_draw_ce_rounded_box(
	short x0,
	short y0,
	short x1,
	short y1,
	short header_h)
{
	enum { BANDS = 8 };
	short inner_x0 = (short)(x0 + 2);
	short inner_y0 = (short)(y0 + 2);
	short inner_x1 = (short)(x1 - 2);
	short inner_y1 = (short)(y1 - 2);
	short body_y0 = (short)(inner_y0 + (header_h > 0 ? header_h : 0));
	short body_h = (short)(inner_y1 - body_y0);
	short b;
	rectangle2d r;
	pixel32 const border_color = 0xFF2896FFu;

	if (header_h > 0 && inner_x1 > inner_x0 && body_y0 > inner_y0)
	{
		r.x0 = inner_x0;
		r.y0 = inner_y0;
		r.x1 = inner_x1;
		r.y1 = (short)(body_y0 < inner_y1 ? body_y0 : inner_y1);
		draw_quad(&r, 0xC0000000u);
	}
	if (body_h > 0 && inner_x1 > inner_x0)
	{
		for (b = 0; b < BANDS; b++)
		{
			short by0 = (short)(body_y0 + (body_h * b) / BANDS);
			short by1 = (short)(body_y0 + (body_h * (b + 1)) / BANDS);
			unsigned int red = (unsigned int)(0x0A - ((0x0A - 0x05) * b) / (BANDS - 1));
			unsigned int grn = (unsigned int)(0x26 - ((0x26 - 0x0D) * b) / (BANDS - 1));
			unsigned int blu = (unsigned int)(0x49 - ((0x49 - 0x1C) * b) / (BANDS - 1));
			pixel32 fill = (pixel32)((0xC0u << 24) | (red << 16) | (grn << 8) | blu);

			if (by1 > by0)
			{
				r.x0 = inner_x0;
				r.y0 = by0;
				r.x1 = inner_x1;
				r.y1 = by1;
				draw_quad(&r, fill);
			}
		}
	}
	r.x0 = (short)(x0 + 2);
	r.y0 = y0;
	r.x1 = (short)(x1 - 2);
	r.y1 = (short)(y0 + 2);
	draw_quad(&r, border_color);

	r.x0 = (short)(x0 + 2);
	r.y0 = (short)(y1 - 2);
	r.x1 = (short)(x1 - 2);
	r.y1 = y1;
	draw_quad(&r, border_color);

	r.x0 = x0;
	r.y0 = (short)(y0 + 2);
	r.x1 = (short)(x0 + 2);
	r.y1 = (short)(y1 - 2);
	draw_quad(&r, border_color);

	r.x0 = (short)(x1 - 2);
	r.y0 = (short)(y0 + 2);
	r.x1 = x1;
	r.y1 = (short)(y1 - 2);
	draw_quad(&r, border_color);

	if (header_h > 0 && body_y0 + 1 < inner_y1)
	{
		r.x0 = inner_x0;
		r.y0 = body_y0;
		r.x1 = inner_x1;
		r.y1 = (short)(body_y0 + 1);
		draw_quad(&r, 0xA02896FFu);
	}
}

static void range_draw_unicode_line(
	long font_index,
	short x0,
	short y0,
	short x1,
	short y1,
	short justification,
	real_argb_color const *color,
	char const *ascii)
{
	wchar_t wide[128];
	rectangle2d bounds;
	short i;

	for (i = 0; ascii[i] && i < (short)NUMBEROF(wide) - 1; i++)
		wide[i] = (wchar_t)(unsigned char)ascii[i];
	wide[i] = 0;
	bounds.x0 = x0;
	bounds.y0 = y0;
	bounds.x1 = x1;
	bounds.y1 = y1;
	draw_string_set_draw_mode(font_index, NONE, justification, 0, color);
	rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, wide);
}

void firing_range_draw(
	void)
{
	long font_index;
	struct font_header *font;
	short line_h;
	rectangle2d window;
	real_argb_color white = { 1.0f, 1.0f, 1.0f, 1.0f };
	real_argb_color cyan = { 1.0f, 0.45f, 0.78f, 1.0f };
	real_argb_color gold = { 1.0f, 1.0f, 0.84f, 0.35f };

	if (!firing_range.active || !firing_range.bsp_written)
		return;
	font_index = interface_get_tag_index(_interface_font_terminal);
	if (font_index == NONE || !(font = font_definition_get(font_index)))
		return;
	line_h = (short)(font->ascending_height + font->descending_height + 2);
	window = render.camera.window_bounds;

	/* 1. Live Gun Range Telemetry Box (Top-Right) */
	if (firing_range.telemetry_shots_fired > 0 || firing_range.telemetry_shots_hit > 0 ||
		firing_range.near_terminal == _range_terminal_weapon_dispenser ||
		firing_range.near_terminal == _range_terminal_target_settings)
	{
		short box_w = 224;
		short header_h = (short)(line_h + 6);
		short box_h = (short)(header_h + line_h * 5 + 12);
		short x0 = (short)(window.x1 - box_w - 16);
		short y0 = (short)(window.y0 + 16);
		real acc = firing_range.telemetry_shots_fired > 0 ?
			(100.0f * (real)firing_range.telemetry_shots_hit / (real)firing_range.telemetry_shots_fired) : 0.0f;
		real dps = firing_range.telemetry_last_dps;
		char line[96];

		if (firing_range.target_first_hit != NONE && firing_range.target_last_hit != NONE &&
			firing_range.target_last_hit > firing_range.target_first_hit)
		{
			real dt = (real)(firing_range.target_last_hit - firing_range.target_first_hit) / TICKS_PER_SECOND;
			dps = firing_range.telemetry_total_damage / MAX(dt, 0.03f);
		}
		range_draw_ce_rounded_box(x0, y0, (short)(x0 + box_w), (short)(y0 + box_h), header_h);
		range_draw_unicode_line(font_index, (short)(x0 + 8), (short)(y0 + 4), (short)(x0 + box_w - 8),
			(short)(y0 + header_h), 2, &cyan, "GUN RANGE TELEMETRY");

		snprintf(line, sizeof(line), "Target Distance: %dm",
			range_distances_meters[firing_range.target_distance_index]);
		range_draw_unicode_line(font_index, (short)(x0 + 10), (short)(y0 + header_h + 4),
			(short)(x0 + box_w - 10), (short)(y0 + header_h + 4 + line_h), 0, &white, line);

		snprintf(line, sizeof(line), "DPS: %.1f  |  Last TTK: %.2fs",
			(double)dps, (double)firing_range.telemetry_last_ttk);
		range_draw_unicode_line(font_index, (short)(x0 + 10), (short)(y0 + header_h + 4 + line_h),
			(short)(x0 + box_w - 10), (short)(y0 + header_h + 4 + line_h * 2), 0, &gold, line);

		snprintf(line, sizeof(line), "Spread Circle Dia: %.2f m",
			(double)firing_range.telemetry_spread_diameter);
		range_draw_unicode_line(font_index, (short)(x0 + 10), (short)(y0 + header_h + 4 + line_h * 2),
			(short)(x0 + box_w - 10), (short)(y0 + header_h + 4 + line_h * 3), 0, &white, line);

		snprintf(line, sizeof(line), "Accuracy: %.1f%% (%ld / %ld)",
			(double)acc, firing_range.telemetry_shots_hit, firing_range.telemetry_shots_fired);
		range_draw_unicode_line(font_index, (short)(x0 + 10), (short)(y0 + header_h + 4 + line_h * 3),
			(short)(x0 + box_w - 10), (short)(y0 + header_h + 4 + line_h * 4), 0, &white, line);

		snprintf(line, sizeof(line), "Weapon: %s",
			firing_range.weapon_count > 0 ? firing_range.weapon_names[firing_range.selected_weapon] : "None");
		range_draw_unicode_line(font_index, (short)(x0 + 10), (short)(y0 + header_h + 4 + line_h * 4),
			(short)(x0 + box_w - 10), (short)(y0 + header_h + 4 + line_h * 5), 0, &cyan, line);
	}

	/* 2. Approach Prompt Banner ("Press E to open <name> options") */
	if (firing_range.open_terminal == NONE && firing_range.near_terminal != NONE)
	{
		char prompt[96];
		short pw = 340;
		short ph = (short)(line_h + 12);
		short px0 = (short)((window.x0 + window.x1 - pw) / 2);
		short py0 = (short)(window.y1 - ph - 38);

		snprintf(prompt, sizeof(prompt), "Press E to open %s options",
			range_terminals[firing_range.near_terminal].name);
		range_draw_ce_rounded_box(px0, py0, (short)(px0 + pw), (short)(py0 + ph), 0);
		range_draw_unicode_line(font_index, (short)(px0 + 8), (short)(py0 + 6), (short)(px0 + pw - 8),
			(short)(py0 + ph), 2, &gold, prompt);
	}

	/* 3. Interactive Halo 1 CE Escape-Menu Modal Popup */
	if (firing_range.open_terminal != NONE)
	{
		enum { VISIBLE_ROWS = 10 };
		short term = firing_range.open_terminal;
		short total = range_modal_item_count(term);
		short row_h = (short)(line_h + 4);
		short header_h = (short)(line_h + 10);
		short footer_h = (short)(line_h + 8);
		short box_w = 380;
		short box_h = (short)(header_h + VISIBLE_ROWS * row_h + footer_h + 12);
		short x0 = (short)((window.x0 + window.x1 - box_w) / 2);
		short y0 = (short)((window.y0 + window.y1 - box_h) / 2);
		short r;

		range_draw_ce_rounded_box(x0, y0, (short)(x0 + box_w), (short)(y0 + box_h), header_h);
		range_draw_unicode_line(font_index, (short)(x0 + 10), (short)(y0 + 6), (short)(x0 + box_w - 10),
			(short)(y0 + header_h), 2, &cyan, range_terminals[term].header);

		for (r = 0; r < VISIBLE_ROWS; r++)
		{
			short item = (short)(firing_range.modal_scroll + r);
			short ry0 = (short)(y0 + header_h + 6 + r * row_h);
			char label[96];

			if (item >= total)
				break;
			if (item == firing_range.modal_cursor)
			{
				rectangle2d sel;

				sel.x0 = (short)(x0 + 6);
				sel.y0 = ry0;
				sel.x1 = (short)(x0 + box_w - 6);
				sel.y1 = (short)(ry0 + row_h - 1);
				draw_quad(&sel, 0x902896FFu);
			}
			range_modal_item_label(term, item, label, sizeof(label));
			range_draw_unicode_line(font_index, (short)(x0 + 14), (short)(ry0 + 2), (short)(x0 + box_w - 14),
				(short)(ry0 + row_h), 0, item == firing_range.modal_cursor ? &gold : &white, label);
		}

		range_draw_unicode_line(font_index, (short)(x0 + 10), (short)(y0 + box_h - footer_h),
			(short)(x0 + box_w - 10), (short)(y0 + box_h - 4), 2, &cyan,
			"W/S or Up/Down: Navigate   |   E / Enter: Select   |   Q / Crouch: Close");
	}
}

/* ---------- public code ---------- */

char const *firing_range_map_name(
	void)
{
	return range_map_name;
}

void firing_range_request(
	char const *profile)
{
	firing_range.requested = TRUE;
	snprintf(firing_range.profile, sizeof(firing_range.profile), "%s",
		(profile && profile[0]) ? profile : "creation_hub");
	error(_error_silent, "firing range: asked for, with the profile %s", firing_range.profile);
}

boolean firing_range_active(
	void)
{
	return firing_range.active;
}

char const *firing_range_profile(
	void)
{
	return firing_range.active ? firing_range.profile : "";
}

char const *firing_range_level_profile(
	char const *scenario_name)
{
	return firing_range.requested && range_map_is(scenario_name) ?
		(firing_range.profile[0] ? firing_range.profile : "creation_hub") : NULL;
}

void firing_range_tags_injected(
	long profile,
	long const *roots,
	long const *root_tags,
	long count)
{
	long index;

	firing_range.weapon_count = 0;
	firing_range.ai_count = 0;
	for (index = 0; index < count; index++)
	{
		unsigned long group = roots[index] == NONE ? 0 : tag_get_group_tag(roots[index]);
		char name[RANGE_NAME_LENGTH];
		short known;

		if (group != WEAPON_GROUP_TAG && group != ACTOR_VARIANT_GROUP_TAG)
			continue;
		if (profile != NONE)
			tag_injector_tag_display_name(profile, root_tags[index], name, sizeof(name));
		else
			range_format_display_name(tag_get_name(roots[index]), name, sizeof(name));
		if (group == WEAPON_GROUP_TAG)
		{
			for (known = 0; known < firing_range.weapon_count && firing_range.weapons[known] != roots[index]; known++)
				;
			if (known < firing_range.weapon_count || firing_range.weapon_count == MAXIMUM_RANGE_WEAPONS)
				continue;
			snprintf(firing_range.weapon_names[firing_range.weapon_count], RANGE_NAME_LENGTH, "%s", name);
			firing_range.weapons[firing_range.weapon_count++] = roots[index];
			continue;
		}
		for (known = 0; known < firing_range.ai_count && firing_range.ais[known].variant != roots[index]; known++)
			;
		if (known < firing_range.ai_count || firing_range.ai_count == MAXIMUM_RANGE_AIS)
			continue;
		firing_range.ais[firing_range.ai_count].variant = roots[index];
		snprintf(firing_range.ais[firing_range.ai_count].name, RANGE_NAME_LENGTH, "%s", name);
		firing_range.ai_count++;
	}
}

void firing_range_tags_loaded(
	char const *scenario_name,
	long scenario_tag_index)
{
	struct scenario *scenario;
	short slot;

	firing_range.active = FALSE;
	firing_range.mirror_unit = NONE;
	firing_range.mirrored_unit = NONE;
	firing_range.anim_unit = NONE;
	firing_range.anim_ai_index = 0;
	firing_range.anim_sequence = NONE;
	firing_range.next_check_time = 0;
	firing_range.table_weapon_object = NONE;
	firing_range.table_empty_since = NONE;
	firing_range.selected_weapon = 0;
	firing_range.equip_directly = TRUE;
	firing_range.spawned_vehicle = NONE;
	firing_range.auto_despawn_track_vehicles = TRUE;
	firing_range.player_was_in_track = FALSE;
	firing_range.player_was_in_fight_room = FALSE;
	firing_range.scenery_spawned = FALSE;
	firing_range.target_distance_index = 2; /* 15m default */
	firing_range.target_mode = 0; /* Spartan Dummy default */
	firing_range.target_ai_index = 0;
	firing_range.target_unit = NONE;
	firing_range.last_weapon_rounds = -1;
	range_reset_telemetry();
	firing_range.next_fighter = 0;
	firing_range.next_track_ai = 0;
	firing_range.fight_weapon_mode = 0;
	firing_range.near_terminal = NONE;
	firing_range.open_terminal = NONE;
	firing_range.modal_cursor = 0;
	firing_range.modal_scroll = 0;
	firing_range.action_held = FALSE;
	firing_range.nav_up_held = FALSE;
	firing_range.nav_down_held = FALSE;
	firing_range.nav_back_held = FALSE;

	for (slot = 0; slot < MAXIMUM_RANGE_FIGHTERS; slot++)
		firing_range.fighters[slot] = NONE;
	for (slot = 0; slot < MAXIMUM_RANGE_TRACK_AIS; slot++)
		firing_range.track_ais[slot] = NONE;
	for (slot = 0; slot < NUMBER_OF_RANGE_TERMINALS; slot++)
		firing_range.uplink_objects[slot] = NONE;
	for (slot = 0; slot < MAXIMUM_RANGE_SCENERY; slot++)
		firing_range.track_scenery_objs[slot] = NONE;

	if (!firing_range.requested || !range_map_is(scenario_name))
	{
		firing_range.requested = FALSE;
		firing_range.weapon_count = 0;
		firing_range.vehicle_count = 0;
		firing_range.ai_count = 0;
		return;
	}
	scenario = scenario_tag_index == NONE ? NULL : (struct scenario *)tag_get(SCENARIO_GROUP_TAG, scenario_tag_index);
	if (!scenario || scenario->structure_bsp_references.count <= 0)
	{
		firing_range.requested = FALSE;
		firing_range.weapon_count = 0;
		return;
	}
	firing_range.active = TRUE;
	range_scenario_patch(scenario);
}

void firing_range_tags_unloaded(
	void)
{
	range_scenario_restore();
	firing_range.active = FALSE;
	firing_range.weapon_count = 0;
	firing_range.vehicle_count = 0;
	firing_range.ai_count = 0;
	firing_range.mirror_unit = NONE;
	firing_range.mirrored_unit = NONE;
	firing_range.anim_unit = NONE;
	firing_range.open_terminal = NONE;
}

boolean firing_range_structure_bsp_write(
	void *base_address,
	long size)
{
	static struct range_layout layout;
	struct range_writer writer;
	struct range_structure_bsp_header *header;
	struct structure_bsp *bsp = NULL;

	if (!firing_range.active || !base_address || size <= 0)
		return FALSE;
	range_layout_make(&layout);
	writer.base = base_address;
	writer.size = (unsigned long)size;
	writer.used = 0;
	writer.full = FALSE;
	header = range_allocate(&writer, sizeof(*header));
	if (!header || !range_structure_bsp_build(&writer, &layout, &bsp))
	{
		error(_error_silent, "firing range: the plane does not fit the map's %ld bytes of structure bsp", size);
		return FALSE;
	}
	header->base_address = bsp;
	header->vertex_buffer_count = 0;
	header->vertex_buffers = header;
	header->index_buffer_count = 0;
	header->index_buffers = header;
	header->signature = STRUCTURE_BSP_HEADER_SIGNATURE;
	firing_range.bsp_written = TRUE;
	error(_error_silent, "firing range: 4-zone Forerunner structure bsp written (%lu bytes of %ld): %ld surfaces, %d "
		"collision surfaces, %d blocks", writer.used, size, bsp->surfaces.count, layout.face_count,
		layout.block_count);

	return TRUE;
}

boolean firing_range_structure_bsp_written(
	void)
{
	return firing_range.bsp_written;
}

void firing_range_structure_bsp_unloaded(
	void)
{
	firing_range.bsp_written = FALSE;
}

void firing_range_tick(
	void)
{
	long now;

	if (!firing_range.active || !firing_range.bsp_written)
		return;
	now = game_time_get();
	if (local_player_get_player_index(0) != NONE &&
		player_get(local_player_get_player_index(0))->unit_index != NONE)
	{
		range_scenery_and_uplinks_update();
		if (now >= firing_range.next_check_time)
		{
			range_weapons_update(now);
			firing_range.next_check_time = now + RANGE_CHECK_TICKS;
		}
		range_dummies_update(now);
		range_anim_pedestal_update();
		range_zones_boundary_update();
	}
	range_mirror_update();
}

void firing_range_player_control(
	long unit_index,
	struct unit_control_data *control)
{
	struct unit_control_data mirrored;
	struct unit_datum *unit;
	struct unit_datum *mirror;
	real_point3d place;
	real dx;
	real dy;

	if (!firing_range.active || local_player_get_player_index(0) == NONE ||
		player_get(local_player_get_player_index(0))->unit_index != unit_index)
		return;
	unit = unit_get(unit_index);

	/* C-Uplink Terminal Detection & Modal Input */
	{
		short near_term = unit->object.parent_object_index == NONE ? range_terminal_near(unit) : NONE;
		boolean action = TEST_FLAG(control->control_flags, _unit_control_action_bit) ||
			input_key_is_down(_key_return) || input_key_is_down(_keypad_enter);
		boolean back = TEST_FLAG(control->control_flags, _unit_control_crouch_modifier_bit) ||
			TEST_FLAG(control->control_flags, _unit_control_use_equipment_bit) ||
			TEST_FLAG(control->control_flags, _unit_control_swap_weapons_bit) ||
			input_key_is_down(_key_backspace) || input_key_is_down(_key_q);
		boolean up = control->throttle.i > 0.45f || input_key_is_down(_key_up_arrow);
		boolean down = control->throttle.i < -0.45f || input_key_is_down(_key_down_arrow);

		firing_range.near_terminal = near_term;
		if (firing_range.open_terminal != NONE)
		{
			short total = range_modal_item_count(firing_range.open_terminal);

			/* Freeze player movement and firing while C-Uplink modal is open */
			control->throttle.i = 0.0f;
			control->throttle.j = 0.0f;
			control->throttle.k = 0.0f;
			control->primary_trigger = 0.0f;
			control->control_flags = 0;

			if (near_term == NONE || (back && !firing_range.nav_back_held))
			{
				firing_range.open_terminal = NONE;
			}
			else if (up && !firing_range.nav_up_held && total > 0)
			{
				firing_range.modal_cursor = (short)((firing_range.modal_cursor - 1 + total) % total);
				if (firing_range.modal_cursor < firing_range.modal_scroll)
					firing_range.modal_scroll = firing_range.modal_cursor;
				else if (firing_range.modal_cursor >= firing_range.modal_scroll + 10)
					firing_range.modal_scroll = (short)(firing_range.modal_cursor - 9);
			}
			else if (down && !firing_range.nav_down_held && total > 0)
			{
				firing_range.modal_cursor = (short)((firing_range.modal_cursor + 1) % total);
				if (firing_range.modal_cursor < firing_range.modal_scroll)
					firing_range.modal_scroll = firing_range.modal_cursor;
				else if (firing_range.modal_cursor >= firing_range.modal_scroll + 10)
					firing_range.modal_scroll = (short)(firing_range.modal_cursor - 9);
			}
			else if (action && !firing_range.action_held && total > 0)
			{
				range_modal_activate(firing_range.open_terminal, firing_range.modal_cursor);
			}
		}
		else if (near_term != NONE && action && !firing_range.action_held)
		{
			firing_range.open_terminal = near_term;
			firing_range.modal_cursor = 0;
			firing_range.modal_scroll = 0;
			control->control_flags &= (word)~FLAG(_unit_control_action_bit);
		}

		firing_range.action_held = action;
		firing_range.nav_up_held = up;
		firing_range.nav_down_held = down;
		firing_range.nav_back_held = back;
	}

	if (firing_range.mirror_unit == NONE || unit_index != firing_range.mirrored_unit ||
		!object_try_and_get_and_verify_type(firing_range.mirror_unit, _object_mask_unit))
		return;
	mirror = unit_get(firing_range.mirror_unit);
	mirrored = *control;
	mirrored.control_flags &= (word)~FLAG(_unit_control_action_bit);
	range_mirror_vector(&control->facing_vector, &mirrored.facing_vector);
	range_mirror_vector(&control->aiming_vector, &mirrored.aiming_vector);
	range_mirror_vector(&control->looking_vector, &mirrored.looking_vector);
	mirrored.throttle.j = -control->throttle.j;
	unit_control(firing_range.mirror_unit, &mirrored);
	range_mirror_point(&unit->object.position, &place);
	dx = mirror->object.position.x - place.x;
	dy = mirror->object.position.y - place.y;
	if (dx * dx + dy * dy > RANGE_MIRROR_DRIFT * RANGE_MIRROR_DRIFT)
	{
		real_vector3d forward;

		range_mirror_vector(&unit->object.forward, &forward);
		object_set_position(firing_range.mirror_unit, &place, &forward, &unit->object.up);
	}
}

boolean firing_range_damage_ignored(
	long owner_object_index,
	long object_index)
{
	if (!firing_range.active)
		return FALSE;
	if (firing_range.mirror_unit != NONE &&
		(owner_object_index == firing_range.mirror_unit || object_index == firing_range.mirror_unit))
		return TRUE;
	if (firing_range.anim_unit != NONE && object_index == firing_range.anim_unit)
		return TRUE;
	return FALSE;
}

void firing_range_record_damage(
	long owner_object_index,
	long object_index,
	real damage_amount,
	real_point3d const *hit_point)
{
	long now;
	short i;
	short j;

	if (!firing_range.active || object_index == NONE || object_index != firing_range.target_unit)
		return;
	if (TEST_FLAG(object_get(object_index)->object.damage_flags, _object_dead_bit))
		return;
	(void)owner_object_index;
	now = game_time_get();
	if (firing_range.target_first_hit == NONE)
	{
		firing_range.target_first_hit = now;
		firing_range.telemetry_total_damage = 0.0f;
		firing_range.spread_count = 0;
	}
	firing_range.target_last_hit = now;
	firing_range.telemetry_total_damage += damage_amount;
	firing_range.telemetry_shots_hit++;
	if (firing_range.telemetry_shots_fired < firing_range.telemetry_shots_hit)
		firing_range.telemetry_shots_fired = firing_range.telemetry_shots_hit;

	if (hit_point)
	{
		struct object_datum *target_obj = object_get(object_index);
		real rel_y = hit_point->y - target_obj->object.position.y;
		real rel_z = hit_point->z - target_obj->object.position.z;
		short idx = (short)(firing_range.spread_count % RANGE_SPREAD_SAMPLES);
		real max_d2 = 0.0f;

		firing_range.spread_y[idx] = rel_y;
		firing_range.spread_z[idx] = rel_z;
		if (firing_range.spread_count < RANGE_SPREAD_SAMPLES)
			firing_range.spread_count++;
		for (i = 0; i < firing_range.spread_count; i++)
		{
			for (j = (short)(i + 1); j < firing_range.spread_count; j++)
			{
				real dy = firing_range.spread_y[i] - firing_range.spread_y[j];
				real dz = firing_range.spread_z[i] - firing_range.spread_z[j];
				real d2 = dy * dy + dz * dz;

				if (d2 > max_d2)
					max_d2 = d2;
			}
		}
		/* Convert world units to meters (1 world unit ≈ 3.048m) */
		firing_range.telemetry_spread_diameter = sqrtf(max_d2) * 3.048f;
	}
}

boolean firing_range_multiplayer_target(
	long object_index)
{
	return firing_range.active && object_index != NONE && firing_range.target_unit == object_index &&
		firing_range.target_mode == 0;
}
