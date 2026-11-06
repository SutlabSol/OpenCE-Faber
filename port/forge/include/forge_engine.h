/*
 * OpenCE : Faber ("OpenCE - Faber.exe")
 * Runtime Engine Header - Shared across all Forge runtime modules.
 * Strictly modular inside FORGE CODE/ with zero modifications to source/ or port/.
 */

#ifndef FORGE_ENGINE_H
#define FORGE_ENGINE_H

#include <stdint.h>
#include "forge_cache_builder.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Forge Release Version & Network Protocol Isolation (v2.3.3, 0x462B) */

#define FORGE_VERSION_STRING "2.3.3"
#define FORGE_NETWORK_VERSION 0x462B
/* Standard (non-Forge) sessions use OpenCE's own HALO_PORT_NETWORK_VERSION
 * (port/linux/include/halo_port_limits.h), read in forge_get_network_version(),
 * so an upstream protocol bump never leaves Forge unable to join stock games. */
#define _forge_ai_team_friendly_human _forge_ai_team_friendly

extern int g_forge_session_enabled;
long forge_get_network_version(void);

/* Engine function (source/main/main.h) used by several Forge modules */
char *main_get_map_name(void);
char const *forge_get_active_map_name(void);

#ifdef _WIN32
__declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);
#else
static inline short GetAsyncKeyState(int vKey) { (void)vKey; return 0; }
#endif

#ifndef VK_LBUTTON
#define VK_LBUTTON   0x01
#define VK_RBUTTON   0x02
#endif

#ifndef VK_BACK
#define VK_BACK      0x08
#define VK_TAB       0x09
#define VK_RETURN    0x0D
#define VK_SHIFT     0x10
#define VK_CONTROL   0x11
#define VK_ESCAPE    0x1B
#define VK_SPACE     0x20
#define VK_PRIOR     0x21
#define VK_NEXT      0x22
#define VK_END       0x23
#define VK_HOME      0x24
#define VK_LEFT      0x25
#define VK_UP        0x26
#define VK_RIGHT     0x27
#define VK_DOWN      0x28
#define VK_DELETE    0x2E
#define VK_F1        0x70
#define VK_F2        0x71
#define VK_F3        0x72
#define VK_F4        0x73
#define VK_F5        0x74
#define VK_F6        0x75
#define VK_OEM_PLUS  0xBB
#define VK_OEM_MINUS 0xBD
#define VK_OEM_4     0xDB
#define VK_OEM_6     0xDD
#endif

/* ---------- Forge Editor Modes */

enum forge_editor_mode
{
	_forge_mode_play = 0,         /* Normal Spartan / Player control */
	_forge_mode_3d_monitor = 1,   /* Mode 2: 3D In-Engine 343 Guilty Spark Monitor & Physics Beam */
	NUMBER_OF_FORGE_MODES
};

enum forge_ui_modal
{
	_forge_modal_none = 0,
	_forge_modal_spawn_palette,
	_forge_modal_object_properties,

	_forge_modal_save_copy_as
};

enum forge_palette_category
{
	_forge_cat_weapons = 0,
	_forge_cat_equipment,
	_forge_cat_vehicles,
	_forge_cat_bipeds,
	_forge_cat_scenery,
	_forge_cat_barriers_devices,
	_forge_cat_lights,
	_forge_cat_spawns_teleporters,
	_forge_cat_objectives,
	_forge_cat_ai_squads,
	NUMBER_OF_FORGE_PALETTE_CATEGORIES
};

#define _forge_cat_count NUMBER_OF_FORGE_PALETTE_CATEGORIES

enum forge_snap_angle_mode
{
	_forge_snap_off = 0,
	_forge_snap_15_deg,
	_forge_snap_45_deg,
	_forge_snap_90_deg,
	NUMBER_OF_FORGE_SNAP_MODES
};

/* ---------- Firefight Custom Spawn Marker ('ffsp' / netgame_flag type 15) */

#define FORGE_NETGAME_FLAG_FIREFIGHT_SPAWN 15

enum forge_ff_spawn_type
{
	_forge_ff_spawn_ai_infantry_only = 0,   /* Standard enemy infantry wave spawn */
	_forge_ff_spawn_ai_and_vehicles = 1,    /* Enemy infantry + approved enemy vehicles */
	_forge_ff_spawn_enemy_vehicle_only = 2, /* Approved enemy vehicle spawn (Ghost/Wraith/Banshee/Scorpion) */
	_forge_ff_spawn_enemy_ghost_only = 3,   /* Approved Enemy Ghost spawn */
	_forge_ff_spawn_enemy_heavy_veh = 4,    /* Approved Enemy Wraith / Scorpion / Banshee spawn */
	_forge_ff_spawn_dropship_zone = 5,      /* Approved Covenant Spirit / Pelican dropship zone */
	_forge_ff_spawn_player_vehicle = 6,     /* Approved Player Vehicle Pad (Round 4 Warthog / Round 5 Scorpion) */
	NUMBER_OF_FORGE_FF_SPAWN_TYPES = 7
};

enum forge_ff_round_filter
{
	_forge_ff_round_all = 0,       /* All Rounds */
	_forge_ff_round_1_only = 1,    /* Round 1 Only */
	_forge_ff_round_2_only = 2,    /* Round 2 Only */
	_forge_ff_round_3_only = 3,    /* Round 3 Only */
	_forge_ff_round_4_only = 4,    /* Round 4 Only */
	_forge_ff_round_5_only = 5,    /* Round 5 Only */
	_forge_ff_round_early_1_3 = 6, /* Rounds 1-3 (Early) */
	_forge_ff_round_late_4_5 = 7,  /* Rounds 4-5 (Late) */
	_forge_ff_round_bonus = 8,     /* Bonus Round Only */
	NUMBER_OF_FORGE_FF_ROUND_FILTERS = 9
};

enum forge_ff_wave_filter
{
	_forge_ff_wave_all = 0,        /* All Waves (1-3) */
	_forge_ff_wave_1_only = 1,     /* Wave 1 Only (Initial) */
	_forge_ff_wave_2_only = 2,     /* Wave 2 Only (Reinforcements) */
	_forge_ff_wave_3_boss = 3,     /* Wave 3 Only (Boss Wave) */
	NUMBER_OF_FORGE_FF_WAVE_FILTERS = 4
};

#define FORGE_FF_PACK_USAGE(type, rnd, wav) \
	((int16_t)((((int)(type)) & 0x0F) | ((((int)(rnd)) & 0x0F) << 4) | ((((int)(wav)) & 0x0F) << 8)))
#define FORGE_FF_UNPACK_TYPE(u)  (((int)(u)) & 0x0F)
#define FORGE_FF_UNPACK_ROUND(u) ((((int)(u)) >> 4) & 0x0F)
#define FORGE_FF_UNPACK_WAVE(u)  ((((int)(u)) >> 8) & 0x0F)

/* ---------- Runtime Palette Catalog Entry */

struct forge_catalog_entry
{
	uint32_t group_tag;
	uint8_t category;
	uint8_t source_cache_id; /* 0 = forgeasset.map (universal), 1 = customforgeasset.map (SP only) */
	uint16_t pad;
	char tag_path[FORGE_TAG_PATH_MAX];
	char display_name[64];
};

#define FORGE_MAX_CATALOG_ENTRIES 2048

/* ---------- Live Placed Instance Tracking */

struct forge_live_instance
{
	int32_t active;
	int32_t object_index;      /* Engine object datum index, or -1 if marker-only */
	int32_t actor_index;       /* Engine AI actor datum index, or -1 */
	int32_t edit_entry_index;  /* Index in edit_state */
	uint32_t group_tag;
	uint8_t source_cache_id;
	uint8_t is_ai_unit;
	uint8_t is_default_scenario_object; /* 1 if adopted from the base map's default scenario objects */
	uint8_t pad_inst;
	uint16_t gametype_flags;   /* 0 = All Modes, bit0=CTF, bit1=Slayer, bit2=Oddball, bit3=KOTH, bit4=Race */
	int16_t netgame_flag_type; /* Scenario netgame_flag type (6=tele_from, 7=tele_to, 8=hill, 2=oddball, 0=ctf) or -1 */
	int16_t team_designator;   /* Reference/Channel ID or Team Index */
	float orig_x, orig_y, orig_z; /* Original scenario position if is_default_scenario_object == 1 */
	float x, y, z;
	float yaw, pitch, roll;
	char tag_path[FORGE_TAG_PATH_MAX];
	char label[64];
};

#define FORGE_MAX_LIVE_INSTANCES 1024

/* ---------- 3D Snap Point Configuration Per Tag */

#define FORGE_MAX_SNAP_POINTS_PER_TAG 16
#define FORGE_MAX_SNAP_TAG_CONFIGS    512

enum forge_snap_normal_dir
{
	_forge_snap_dir_front = 0,  /* +X (1, 0, 0) */
	_forge_snap_dir_right = 1,  /* -Y (0, -1, 0) */
	_forge_snap_dir_back  = 2,  /* -X (-1, 0, 0) */
	_forge_snap_dir_left  = 3,  /* +Y (0, 1, 0) */
	_forge_snap_dir_up    = 4,  /* +Z (0, 0, 1) */
	_forge_snap_dir_down  = 5,  /* -Z (0, 0, -1) */
	NUMBER_OF_FORGE_SNAP_DIRS
};

enum forge_snap_preset_id
{
	_forge_snap_preset_custom       = 0,
	_forge_snap_preset_front_center = 1,
	_forge_snap_preset_back_center  = 2,
	_forge_snap_preset_left_center  = 3,
	_forge_snap_preset_right_center = 4,
	_forge_snap_preset_top_center   = 5,
	_forge_snap_preset_bot_center   = 6,
	_forge_snap_preset_front_base   = 7,
	_forge_snap_preset_back_base    = 8,
	_forge_snap_preset_left_base    = 9,
	_forge_snap_preset_right_base   = 10,
	NUMBER_OF_FORGE_SNAP_PRESETS
};

struct forge_snap_point
{
	float x, y, z;
	float nx, ny, nz;
	uint8_t normal_dir;
	uint8_t preset_id;
	uint16_t pad;
};

struct forge_tag_snap_config
{
	uint32_t group_tag;
	char tag_path[FORGE_TAG_PATH_MAX];
	int point_count;
	struct forge_snap_point points[FORGE_MAX_SNAP_POINTS_PER_TAG];
};

/* ---------- Global Runtime Forge State */

struct forge_runtime_state
{
	int initialized;
	enum forge_editor_mode mode;
	enum forge_ui_modal active_modal;
	enum forge_palette_category current_category;
	int h3_menu_stage; /* 0 = Halo 3 category list, 1 = Halo 3 item list */
	int palette_cursor;
	int palette_scroll;
	int props_cursor;
	enum forge_snap_angle_mode snap_mode;
	int ai_team_mode; /* _forge_ai_team_friendly_human or _forge_ai_team_enemy_covenant */
	int third_person_view; /* Toggleable via console `third_person` or F4 to inspect 343 Guilty Spark */

	/* Monitor 6DOF Free-Cam State */
	int monitor_cam_initialized;
	float cam_x, cam_y, cam_z;
	float cam_yaw, cam_pitch, cam_roll;
	float cam_speed;

	/* Physics Beam / Object Grab State */
	int32_t grabbed_object_index;
	int32_t grabbed_live_index;
	int grabbed_is_new_spawn;
	float grabbed_orig_x, grabbed_orig_y, grabbed_orig_z;
	float grabbed_orig_yaw, grabbed_orig_pitch, grabbed_orig_roll;
	float grab_distance;
	float grab_yaw, grab_pitch, grab_roll;
	int32_t hovered_object_index;
	int32_t hovered_live_index;
	char hovered_object_label[128];
	float ray_hit_x, ray_hit_y, ray_hit_z;
	int ray_hit_valid;

	/* 10-Point Linked KOTH Hill Circle Placement State */
	int koth_group_placing;
	int koth_group_hill_id;
	float koth_group_radius;
	int32_t koth_group_live_indices[10];

	/* Catalogs: 0 = map-local [MAP] (transient), 1 = stock cached_tags, 2 = custom cached_tags [C] */
	struct forge_catalog_entry catalog[FORGE_MAX_CATALOG_ENTRIES];
	int catalog_count;

	/* Save Map state: prompts for name on initial save, then quick-updates on subsequent saves */
	int has_saved_map_once;
	char save_map_name[64];
	int save_map_name_len;

	/* Status banner message */
	char status_message[128];
	uint32_t status_message_until_ms;

	/* Scenario Edit State & Live Instances */
	struct forge_scenario_edit_state edit_state;
	struct forge_live_instance live_instances[FORGE_MAX_LIVE_INSTANCES];
	int live_instance_count;

	/* Forge Controls & Controller State */
	int using_controller;
	int bind_vk[16];
	int bind_gp[16];
	float controller_look_sens;
	float controller_fly_speed;

	/* Per-Player Multiplayer Monitor vs. Spartan State & Orientation */
	uint8_t player_is_monitor[128];
	uint8_t player_has_monitor_pos[128];
	float player_monitor_pos[128][3];
	float player_monitor_yaw[128];
	float player_monitor_pitch[128];
	uint8_t player_has_render_pos[128];
	float player_monitor_render_pos[128][3];
	float player_monitor_render_yaw[128];
	float player_monitor_render_pitch[128];

	/* 3D Magnetic Snap Point Lock State */
	int snap_point_locked;
	float snap_lock_world_x, snap_lock_world_y, snap_lock_world_z;
	char snap_lock_label[64];
};

enum forge_bind_action
{
	FORGE_BIND_TOGGLE_MODE = 0,
	FORGE_BIND_PALETTE = 1,
	FORGE_BIND_THIRD_PERSON = 2,
	FORGE_BIND_QUICK_SAVE = 3,
	FORGE_BIND_ROTATE_HOLD = 4,
	FORGE_BIND_SNAP_FLOOR = 5,
	FORGE_BIND_SNAP_SURFACE = 6,
	FORGE_BIND_ROT_SNAP = 7,
	FORGE_BIND_OBJECT_OPTIONS = 8,
	FORGE_BIND_DELETE_OBJECT = 9,
	FORGE_BIND_COPY_OBJECT = 10,
	FORGE_BIND_COUNT = 11
};

enum forge_gamepad_button
{
	FORGE_GP_NONE = 0,
	FORGE_GP_A = 1,
	FORGE_GP_B = 2,
	FORGE_GP_X = 3,
	FORGE_GP_Y = 4,
	FORGE_GP_BLACK = 5,
	FORGE_GP_WHITE = 6,
	FORGE_GP_LTRIGGER = 7,
	FORGE_GP_RTRIGGER = 8,
	FORGE_GP_DPAD_UP = 9,
	FORGE_GP_DPAD_DOWN = 10,
	FORGE_GP_DPAD_LEFT = 11,
	FORGE_GP_DPAD_RIGHT = 12,
	FORGE_GP_START = 13,
	FORGE_GP_BACK = 14,
	FORGE_GP_LTHUMB = 15,
	FORGE_GP_RTHUMB = 16,
	FORGE_GP_COUNT = 17
};

#define FORGE_GP_RB     FORGE_GP_BLACK
#define FORGE_GP_LB     FORGE_GP_WHITE
#define FORGE_GP_LT     FORGE_GP_LTRIGGER
#define FORGE_GP_RT     FORGE_GP_RTRIGGER
#define FORGE_GP_LSTICK FORGE_GP_LTHUMB
#define FORGE_GP_RSTICK FORGE_GP_RTHUMB

extern struct forge_runtime_state g_forge;

void forge_controls_init_defaults(void);
void forge_controls_load(void);
void forge_controls_save(void);
char const *forge_vk_display_name(int vk);
char const *forge_gp_display_name(int gp);
char const *forge_gp_icon_token(int gp);
char const *forge_action_prompt(int action);
int forge_gp_button_down(int gp);
int forge_action_down(int action);
void forge_poll_controller_state(float *out_lx, float *out_ly, float *out_rx, float *out_ry, float *out_lt, float *out_rt);

/* ---------- Prototypes: forge_runtime_tags.c */

void forge_runtime_init(void);
void forge_runtime_reload_catalogs(void);
int forge_is_multiplayer_session(void);
int forge_catalog_entry_allowed(struct forge_catalog_entry const *entry);
long forge_resolve_engine_tag(uint32_t group_tag, char const *tag_path, uint32_t *out_actual_group);
uint32_t forge_get_base_map_tag_count(void);
int forge_ui_save_copy_as(char const *new_map_name);
void forge_trigger_save_map_action(void);
void forge_load_saved_map_state_if_present(char const *map_name);
int forge_cache_active_map_tag(uint32_t group_tag, char const *tag_path);
int forge_add_custom_map_tag_to_cache(
	char const *donor_map_path,
	char const *maps_dir,
	uint32_t root_group,
	char const *root_tag_path,
	char const *user_custom_name);
void forge_normalize_mcc_hud_tags(void);
int forge_is_mcc_hud_active(void);
void forge_set_status(char const *fmt, ...);
void forge_draw_screen_rect(short x0, short y0, short x1, short y1, uint32_t argb);
int forge_monitor_render_player_unit(long object_index, void const *lighting, unsigned long flags);
void forge_render_world_3d_overlays(void const *lighting);

/* ---------- Prototypes: forge_ai.c */

int32_t forge_ai_spawn_unit_live(
	char const *actor_variant_path,
	float x, float y, float z, float yaw,
	int16_t team_designator,
	int32_t *out_unit_index);

/* ---------- Prototypes: forge_3d_mode.c */

void forge_3d_update(float dt);
void forge_3d_apply_monitor_camera(short local_player_index);
int forge_3d_fill_live_observer_result(void *out_observer_result);
int forge_is_main_menu_active(void);
void forge_3d_draw_hud(short local_player_index, short window_x0, short window_y0, short window_x1, short window_y1, long font_index, short line_height);
int32_t forge_3d_spawn_object_at(
	uint32_t group_tag,
	char const *tag_path,
	uint8_t source_cache_id,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int16_t team_designator,
	int broadcast_net);
int32_t forge_3d_spawn_object_at_slot(
	int32_t forced_slot,
	uint32_t group_tag,
	char const *tag_path,
	uint8_t source_cache_id,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int16_t team_designator,
	int broadcast_net);
int forge_object_is_network_shared(uint32_t group_tag);
void forge_3d_move_instance(int32_t live_idx, float x, float y, float z, float yaw, float pitch, float roll, int broadcast_net);
void forge_3d_move_instance_ext(int32_t live_idx, float x, float y, float z, float yaw, float pitch, float roll, int is_held, int broadcast_net);
void forge_3d_delete_instance(int32_t live_idx, int broadcast_net);
int forge_is_unit_in_monitor_mode(long unit_index);
int forge_get_local_player_abs_index(void);
void forge_render_begin_monitor_visibility(void);
void forge_render_end_monitor_visibility(void);
void forge_apply_host_player_monitor_state(int player_abs_idx, int is_monitor, float x, float y, float z, float yaw, float pitch);
int32_t forge_3d_adopt_or_find_object_instance(long object_index);
void forge_cancel_or_release_held_item(void);
int forge_has_held_item_to_cancel(void);

/* ---------- Prototypes: forge_network.c */

uint32_t forge_catalog_entry_hash(uint32_t group_tag, char const *tag_path);
struct forge_catalog_entry const *forge_find_catalog_entry_by_hash(uint32_t hash);
void forge_network_reset_session(void);
void forge_network_sync_tick(void);
void forge_network_send_monitor_mode(int is_monitor, float x, float y, float z, float yaw, float pitch);
void forge_network_send_spawn_ext(
	int32_t live_idx,
	uint32_t group_tag,
	char const *tag_path,
	uint8_t source_cache_id,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int16_t team_designator,
	long host_obj_idx,
	long host_def_idx);
void forge_network_send_transform_ext(
	int32_t live_idx,
	long obj_idx,
	float x, float y, float z,
	float yaw, float pitch, float roll,
	int is_held,
	int immediate);
void forge_network_send_delete_ext(int32_t live_idx, long obj_idx);
int forge_network_intercept_chat_packet(long machine_index, char const *text, int is_from_client_to_host);
void forge_network_pre_handle_object_changes(void const *entries, short count);
void forge_network_post_handle_object_changes(void const *entries, short count);
void forge_runtime_on_map_unload(void);
int forge_window_is_focused(void);
short forge_key_state(int vk);

int forge_get_readable_donor_map_path(char const *map_path, char *out_path, size_t out_sz);
struct bitmap_data *forge_tag_preview_get_bitmap(void);
void forge_tag_preview_clear(void);
void forge_tag_preview_set_map_tag(char const *map_path, uint32_t group_tag, char const *tag_path);
void forge_tag_preview_set_installed_tag(uint32_t group_tag, char const *tag_path, int is_custom);
uint32_t forge_tag_preview_get_current_size_bytes(void);
int forge_extract_tag_from_donor_map_to_ocetags(
	char const *donor_map_path,
	uint32_t root_group,
	char const *root_tag_path,
	char const *user_custom_name);
int forge_delete_installed_tag_entry(uint32_t group_tag, char const *tag_path, int is_custom);
int forge_reset_stock_tags_only(void);
int forge_reset_custom_tags_only(void);

/* ---------- 3D Snap Point Persistence & Preview Editor API */

void forge_snap_points_load_all(void);
int forge_snap_points_save_all(void);
struct forge_tag_snap_config *forge_snap_points_get_for_tag(uint32_t group_tag, char const *tag_path, int create_if_missing);
int forge_snap_points_count_for_tag(uint32_t group_tag, char const *tag_path);
void forge_snap_points_get_preview_bounds(float out_min[3], float out_max[3]);
void forge_snap_points_set_editor_overlay(int active, uint32_t group_tag, char const *tag_path, int active_pt_idx, float cam_yaw_deg, float cam_pitch_deg, float cam_zoom);
void forge_snap_points_auto_generate(uint32_t group_tag, char const *tag_path, int mode);
void forge_snap_points_apply_preset_to_point(struct forge_snap_point *pt, int preset_id, int normal_dir);
void forge_snap_points_fill_normal_from_dir(struct forge_snap_point *pt, int normal_dir);

#ifdef _WIN32
#undef GetAsyncKeyState
#define GetAsyncKeyState(vk) forge_key_state(vk)
#endif

#ifdef __cplusplus
}
#endif

#endif /* FORGE_ENGINE_H */
