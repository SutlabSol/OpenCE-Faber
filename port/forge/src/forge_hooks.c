/*
 * OpenCE : Faber ("OpenCE - Faber.exe")
 * Engine Symbol Bridge Hooks (linked via llvm-objcopy --redefine-sym).
 * Strictly modular inside FORGE CODE/ with zero modifications to source/ or port/.
 */

#include "cseries.h"
#include "game/game.h"
#include "game/players.h"
#include "camera/director.h"
#include "interface/hud.h"
#include "interface/hud_draw.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "main/console.h"
#include "models/models.h"
#include "render/render.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "saved games/player_profile.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/unicode.h"
#include "network_chat.h"
#include "custom_edition_maps.h"
#include <xtl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "forge_engine.h"

#undef fopen
#undef fclose
#undef fread
#undef fseek
#undef malloc
#undef free
#undef realloc

#include "game/game_engine.h"
#include "objects/objects.h"
#include "units/units.h"

extern unsigned long system_milliseconds(void);
extern long hud_get_font_index(void);
extern boolean firefight_is_generator_hidden_biped(long object_index);
extern void *cache_files_tag_instances(long *count);
extern void cache_files_set_tag_instances(void *instances, long count);
extern char const *pc_menu_function_name(long function_index);
extern char const *pc_menu_game_data_input_name(long function_index);
extern void main_menu_switch_to_single_player(void);
extern void game_connection_set(short connection);
extern void dispose_global_network_game_client(void);
extern void dispose_global_network_game_server(void);
extern void network_game_accept_remote_connections(boolean accept);
extern boolean network_game_is_active(void);
extern long player_ui_get_active_player_profile_index(short local_player_index);
extern boolean player_ui_get_active_player_profile(short local_player_index, struct player_profile *profile);
extern void player_ui_set_active_player_profile(short local_player_index, long profile_index, struct player_profile const *profile);
extern void player_ui_set_single_player_local_player_controller(short local_player_index, short controller_index);
extern void player_ui_remember_player1_profile(boolean remember);
extern void ui_play_audio_feedback_sound(short index);
extern void display_error_text_deferred(wchar_t const *error_text, short text_style);
extern short ui_widget_port_gametypes(long *indices, short maximum, short *last_used);
extern boolean ui_widget_port_gametype_choose(long profile_index);
extern boolean ui_widget_port_open(void *widget, char const *name, boolean *widget_deleted);
extern void ui_widget_port_go_back(void *widget);
extern boolean ui_widget_port_game_variant_set(struct game_variant *variant, struct game_variant_options const *options);
extern struct game_variant *build_game_variant_slayer(struct game_variant *variant);
extern void game_variant_options_default(struct game_variant const *variant, struct game_variant_options *options);

static short s_forge_last_map_kind = 1; /* 1 = MAP_KIND_MULTIPLAYER, 3 = MAP_KIND_CUSTOM_MULTIPLAYER, 4 = MAP_KIND_FORGE_MAPS */
static short s_forge_active_map_kind = 1;
static int s_is_forge_map_cache_valid = 0;
static short s_non_forge_ce_indices[4096];
static short s_non_forge_ce_count = 0;
static short s_forge_only_ce_indices[4096];
static short s_forge_only_ce_count = 0;
static long s_save_forge_btn_tag = NONE;

/* Original engine symbols redefined via llvm-objcopy in FORGE CODE/build/wrapped_obj/ */
extern void orig_network_chat_update(void);
extern void orig_network_chat_draw(short local_player_index);
extern boolean orig_network_chat_typing(void);
extern void orig_network_chat_handle_up(long machine_index, void const *entries, boolean from_stream);
extern void orig_network_chat_handle_down(void const *entries, boolean from_stream);
extern void orig_network_objects_handle_changes(void const *entries, short count);
extern void orig_director_update(real time_delta_sec);
extern short orig_director_get_perspective(short local_player_index);
extern struct observer_result const *orig_render_interpolation_camera(
	short local_player_index,
	struct observer_result const *fallback);
extern void orig_first_person_weapon_draw(void);
extern void orig_hud_render_weapon_interface(struct player_datum *player);
extern void orig_render_model(
	long model_index,
	real level_of_detail_pixels,
	struct real_matrix4x3 const *node_matrices,
	char const *region_permutation_indices,
	union real_rgb_color const *change_colors,
	real const *function_values,
	struct render_lighting const *lighting,
	union real_point3d const *centroid,
	real radius,
	struct render_model_effect const *model_effect,
	long unique_identifier,
	short forced_shader_permutation_index,
	unsigned long flags);
extern boolean orig_hs_compile_and_evaluate(char const *source);
extern void orig_hs_runtime_update(void);
extern void orig_cinematic_start(void);
extern boolean orig_scenario_trigger_volume_test_object(short trigger_volume_index, long object_index);
extern long orig_game_engine_remap_object_definition(long definition_index);
extern void orig_game_engine_post_rasterize_objects(void);
extern void rasterizer_models_begin(boolean sky);
extern void rasterizer_models_end(void);
extern void orig_menu_tags_loaded(char const *map_name);
extern void orig_menu_tags_unloaded(void);
extern boolean orig_pc_menu_event_function_invoke(
	void *widget,
	void *event,
	long function_index,
	boolean *widget_deleted);
extern void orig_pc_menu_game_data_function_invoke(
	void *widget,
	long function);

static unsigned long g_last_forge_frame_ms = 0;
static char g_last_forge_map_name[128] = "";

/* ---------- Widget Instance Layout (matches port/linux/game/menu_functions.c) */

struct forge_widget_instance
{
	long definition_tag_index;
	char const *name;
	short local_player_index;
	short horizontal_offset;
	short vertical_offset;
	short type;
	boolean visible;
	boolean render_regardless_of_controller_index;
	boolean disabled;
	boolean pause_game_time;
	boolean delete_recursion_lock;
	boolean widget_is_error_dialog;
	boolean close_if_local_player_controller_present;
	byte pad17;
	long creation_time;
	unsigned long milliseconds_to_auto_close;
	unsigned long auto_close_fade_time;
	real alpha_modifier;
	struct forge_widget_instance *previous;
	struct forge_widget_instance *next;
	struct forge_widget_instance *parent;
	struct forge_widget_instance *child;
	struct forge_widget_instance *focused_child;
	union
	{
		struct
		{
			wchar_t *text;
			short string_list_index;
		} text_box;
		struct
		{
			short selected_index;
			short last_list_tab_direction;
			void *list_items;
			word number_of_items;
			struct forge_widget_instance *extended_description;
			wchar_t *item_text;
		} list;
	} parameters;
	struct
	{
		short current_frame_index;
		short first_frame_index;
		short last_frame_index;
		short number_of_sprite_frames;
	} animation;
};

static struct forge_widget_instance *fw_descendant(struct forge_widget_instance *w, char const *name, long *nth)
{
	struct forge_widget_instance *c;
	for (c = w ? w->child : NULL; c; c = c->next)
	{
		struct forge_widget_instance *found;
		if (c->name && !strcmp(c->name, name) && (*nth)-- == 0)
			return c;
		found = fw_descendant(c, name, nth);
		if (found)
			return found;
	}
	return NULL;
}

static struct forge_widget_instance *fw_named(struct forge_widget_instance *w, char const *name, long nth)
{
	return w ? fw_descendant(w, name, &nth) : NULL;
}

static void fw_text_set_ascii(struct forge_widget_instance *tb, char const *ascii)
{
	int i;
	if (!tb || !ascii)
		return;
	tb->parameters.text_box.string_list_index = NONE;
	if (tb->definition_tag_index != NONE)
	{
		uint8_t *def = (uint8_t *)tag_get('DeLa', tb->definition_tag_index);
		if (def)
			*(int32_t *)(def + 0xF8) = NONE;
	}
	if (!tb->parameters.text_box.text)
	{
		tb->parameters.text_box.text = (wchar_t *)ui_widget_realloc(NULL, 256 * sizeof(wchar_t), __FILE__, __LINE__);
	}
	if (tb->parameters.text_box.text)
	{
		for (i = 0; ascii[i] && i < 254; i++)
			tb->parameters.text_box.text[i] = (wchar_t)(unsigned char)ascii[i];
		tb->parameters.text_box.text[i] = 0;
	}
}

/* ---------- Forge Edit Maps (User Created / Downloaded) & Inject Custom Tags Native Menus */

#define FORGE_MAX_EDIT_MAPS 64

static struct
{
	int tab; /* 0 = USER CREATED, 1 = DOWNLOADED */
	int count;
	int first;
	int chosen;
	char map_leaves[FORGE_MAX_EDIT_MAPS][64];
	char map_titles[FORGE_MAX_EDIT_MAPS][64];
	char map_authors[FORGE_MAX_EDIT_MAPS][64];
} s_forge_menu;

static int is_forge_created_map_file(char const *map_path, char const *leaf_no_ext, int *out_is_user_created, char *out_author, size_t author_sz)
{
	FILE *f;
	uint8_t hdr[2048];
	uint32_t marker = 0;
	char txt_path[512];
	int has_flg1 = 0;
	int user_created = 0;

	if (out_author && author_sz > 0)
		snprintf(out_author, author_sz, "Community Creator");

	f = fopen(map_path, "rb");
	if (f)
	{
		if (fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr))
		{
			uint32_t state_off = *(uint32_t const *)(hdr + 0x1E4);
			uint32_t state_sz = *(uint32_t const *)(hdr + 0x1E8);
			char const * internal_name = (char const *)(hdr + 0x20);
			marker = *(uint32_t const *)(hdr + 0x1FC);
			if (marker == 0x464C4731u) /* 'FLG1' */
			{
				has_flg1 = 1;
				user_created = 1;
				if (state_off >= 0x800u && state_sz > 0 && state_sz < 4u * 1024u * 1024u)
				{
					char buf[1024];
					uint32_t rsz = (state_sz < sizeof(buf) - 1u) ? state_sz : (uint32_t)(sizeof(buf) - 1u);
					if (fseek(f, (long)state_off, SEEK_SET) == 0 && fread(buf, 1, rsz, f) == rsz)
					{
						buf[rsz] = 0;
						if (strstr(buf, "[OpenCE Forge Map]") != NULL)
							has_flg1 = 1;
						if (strstr(buf, "ForgeSource=UserCreated") != NULL)
							user_created = 1;
					}
				}
			}
			else if (_strnicmp(internal_name, "levels\\forge\\", 13) == 0 ||
			         _strnicmp(internal_name, "levels/forge/", 13) == 0)
			{
				has_flg1 = 1;
				user_created = 1;
			}
		}
		fclose(f);
	}

	if (leaf_no_ext && strlen(leaf_no_ext) > 6 &&
	    _stricmp(leaf_no_ext + strlen(leaf_no_ext) - 6, "_forge") == 0)
	{
		has_flg1 = 1;
		user_created = 1;
	}

	snprintf(txt_path, sizeof(txt_path), "OpenCE Forge/forge_maps/%s/%s.txt", leaf_no_ext, leaf_no_ext);
	f = fopen(txt_path, "rb");
	if (!f)
	{
		snprintf(txt_path, sizeof(txt_path), "Faber_data/forge_maps/%s/%s.txt", leaf_no_ext, leaf_no_ext);
		f = fopen(txt_path, "rb");
	}
	if (!f)
	{
		snprintf(txt_path, sizeof(txt_path), "forge_maps/%s/%s.txt", leaf_no_ext, leaf_no_ext);
		f = fopen(txt_path, "rb");
	}
	if (!f)
	{
		snprintf(txt_path, sizeof(txt_path), "custom_maps/%s.txt", leaf_no_ext);
		f = fopen(txt_path, "rb");
	}
	if (f)
	{
		char buf[1024];
		size_t n = fread(buf, 1, sizeof(buf) - 1, f);
		buf[n] = 0;
		fclose(f);
		if (strstr(buf, "[OpenCE Forge Map]") != NULL)
			has_flg1 = 1;
		if (strstr(buf, "ForgeSource=UserCreated") != NULL)
			user_created = 1;
	}

	if (out_is_user_created)
		*out_is_user_created = user_created;
	return has_flg1;
}

static int is_custom_map_leaf_forge_made(char const *leaf_no_ext)
{
	char check_path[512];
	FILE *f;
	static char const *const s_forge_check_fmts[] = {
		"OpenCE Forge/forge_maps/%s.map",
		"OpenCE Forge/forge_maps/%s/%s.map",
		"OpenCE Forge/forge_maps/%s/%s.txt",
		"Faber_data/forge_maps/%s/%s.txt",
		"Faber_data/forge_maps/%s/%s.map",
		"forge_maps/%s.map",
		"forge_maps/%s/%s.txt",
		"forge_maps/%s/%s.map"
	};
	int i;

	if (!leaf_no_ext || !leaf_no_ext[0])
		return 0;

	if (strlen(leaf_no_ext) > 6 &&
	    _stricmp(leaf_no_ext + strlen(leaf_no_ext) - 6, "_forge") == 0)
	{
		return 1;
	}

	for (i = 0; i < (int)(sizeof(s_forge_check_fmts) / sizeof(s_forge_check_fmts[0])); i++)
	{
		if (strstr(s_forge_check_fmts[i], "%s/%s") != NULL)
			snprintf(check_path, sizeof(check_path), s_forge_check_fmts[i], leaf_no_ext, leaf_no_ext);
		else
			snprintf(check_path, sizeof(check_path), s_forge_check_fmts[i], leaf_no_ext);
		f = fopen(check_path, "rb");
		if (f)
		{
			fclose(f);
			return 1;
		}
	}

	/* Also check if custom_maps/<name>.map has the 'FLG1' Forge header marker or levels\forge\ scenario */
	snprintf(check_path, sizeof(check_path), "custom_maps/%s.map", leaf_no_ext);
	return is_forge_created_map_file(check_path, leaf_no_ext, NULL, NULL, 0);
}

static void ensure_forge_ce_map_partition_built(void)
{
	short total_ce, i;

	if (s_is_forge_map_cache_valid)
		return;
	s_is_forge_map_cache_valid = 1;
	s_non_forge_ce_count = 0;
	s_forge_only_ce_count = 0;

	total_ce = custom_edition_maps_count(FALSE);
	for (i = 0; i < total_ce; i++)
	{
		short disp = custom_edition_maps_display_index_of(FALSE, i);
		char const *lvl = custom_edition_maps_level_name(disp);
		char const *leaf = lvl;
		if (lvl)
		{
			char const *bs = strrchr(lvl, '\\');
			char const *fs = strrchr(lvl, '/');
			if (bs && (!fs || bs > fs))
				leaf = bs + 1;
			else if (fs)
				leaf = fs + 1;
		}
		if (leaf && is_custom_map_leaf_forge_made(leaf))
		{
			if (s_forge_only_ce_count < 4096)
				s_forge_only_ce_indices[s_forge_only_ce_count++] = i;
		}
		else
		{
			if (s_non_forge_ce_count < 4096)
				s_non_forge_ce_indices[s_non_forge_ce_count++] = i;
		}
	}
}

void forge_menu_custom_edition_maps_look_again(void)
{
	s_is_forge_map_cache_valid = 0;
	custom_edition_maps_look_again();
}

short forge_menu_custom_edition_maps_count(boolean campaign)
{
	if (campaign)
		return custom_edition_maps_count(campaign);
	ensure_forge_ce_map_partition_built();
	if (s_forge_active_map_kind == 4)
		return s_forge_only_ce_count;
	return custom_edition_maps_count(FALSE);
}

short forge_menu_custom_edition_maps_display_index_of(boolean campaign, short index)
{
	if (campaign)
		return custom_edition_maps_display_index_of(campaign, index);
	ensure_forge_ce_map_partition_built();
	if (s_forge_active_map_kind == 4)
	{
		if (index < 0 || index >= s_forge_only_ce_count)
			return NONE;
		return custom_edition_maps_display_index_of(FALSE, s_forge_only_ce_indices[index]);
	}
	return custom_edition_maps_display_index_of(FALSE, index);
}

extern short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);
extern boolean ui_widget_port_multiplayer_map_choose(short level_index);

static short s_last_mp_maps_returned = 0;

short forge_menu_multiplayer_maps(char const *const **names, short *last_used)
{
	short total = ui_widget_port_multiplayer_maps(names, last_used);
	short all_ce = custom_edition_maps_count(FALSE);
	short xbox_cnt = (short)(total > all_ce ? (total - all_ce) : 13);
	short part_cnt;

	/* Map screen always initializes on MAP_KIND_MULTIPLAYER (1) */
	s_forge_active_map_kind = 1;
	s_forge_last_map_kind = 1;
	part_cnt = forge_menu_custom_edition_maps_count(FALSE);
	if (last_used && *last_used >= xbox_cnt)
		*last_used = 0;
	s_last_mp_maps_returned = (short)(xbox_cnt + part_cnt);
	return s_last_mp_maps_returned;
}

boolean forge_menu_multiplayer_map_choose(short level_index)
{
	char const *const *names = NULL;
	short last_used = 0;
	short total = ui_widget_port_multiplayer_maps(&names, &last_used);
	short all_ce = custom_edition_maps_count(FALSE);
	short xbox_cnt = (short)(total > all_ce ? (total - all_ce) : 13);

	ensure_forge_ce_map_partition_built();

	/* In menu_functions.c (map_list_choose), when kind is CUSTOM MULTIPLAYER (3) or FORGE MAPS (4),
	 * it passes: xbox_multiplayer_map_count(map_list.map_count) + chosen, where
	 * xbox_multiplayer_map_count(c) = MAX(c - forge_menu_custom_edition_maps_count(FALSE), 0).
	 * Recover the exact 0-based `chosen` index regardless of how many Forge or Custom Edition maps exist! */
	if (s_forge_active_map_kind == 4 || s_forge_active_map_kind == 3)
	{
		short active_ce = forge_menu_custom_edition_maps_count(FALSE);
		short mp_count = (s_last_mp_maps_returned > 0) ? s_last_mp_maps_returned : (short)(xbox_cnt + all_ce);
		short base = (short)((mp_count > active_ce) ? (mp_count - active_ce) : 0);
		short chosen = (short)(level_index - base);

		if (s_forge_active_map_kind == 4)
		{
			if (chosen < 0 || chosen >= s_forge_only_ce_count)
				return FALSE;
			level_index = (short)(xbox_cnt + s_forge_only_ce_indices[chosen]);
		}
		else
		{
			if (chosen < 0 || chosen >= all_ce)
				return FALSE;
			level_index = (short)(xbox_cnt + chosen);
		}
	}
	return ui_widget_port_multiplayer_map_choose(level_index);
}

static void scan_forge_edit_maps(int wanted_tab)
{
	WIN32_FIND_DATAA fd;
	HANDLE h;

	s_forge_menu.tab = wanted_tab;
	s_forge_menu.count = 0;
	s_forge_menu.first = 0;
	s_forge_menu.chosen = 0;

	/* Scan OpenCE Forge/forge_maps, Faber_data/forge_maps, and forge_maps subfolders & .map files */
	{
		static char const *const forge_scan_dirs[] = {
			"OpenCE Forge\\forge_maps\\*",
			"Faber_data\\forge_maps\\*",
			"forge_maps\\*"
		};
		int di;
		for (di = 0; di < 3; di++)
		{
			h = FindFirstFileA(forge_scan_dirs[di], &fd);
			if (h == INVALID_HANDLE_VALUE)
				continue;
			do
			{
				char full_path[512];
				char leaf[64];
				char author[64];
				int is_user = 0;
				int k, already_listed = 0;
				FILE *tf;

				if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
					continue;

				if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					snprintf(leaf, sizeof(leaf), "%s", fd.cFileName);
				}
				else
				{
					char *dot;
					snprintf(leaf, sizeof(leaf), "%s", fd.cFileName);
					dot = strrchr(leaf, '.');
					if (!dot || _stricmp(dot, ".map") != 0)
						continue;
					*dot = 0;
				}

				for (k = 0; k < s_forge_menu.count; k++)
				{
					if (_stricmp(s_forge_menu.map_leaves[k], leaf) == 0)
					{
						already_listed = 1;
						break;
					}
				}
				if (already_listed)
					continue;

				snprintf(full_path, sizeof(full_path), "custom_maps/%s.map", leaf);
				tf = fopen(full_path, "rb");
				if (tf)
				{
					fclose(tf);
				}
				else
				{
					static char const *const map_cands[] = {
						"OpenCE Forge/forge_maps/%s.map",
						"OpenCE Forge/forge_maps/%s/%s.map",
						"Faber_data/forge_maps/%s/%s.map",
						"forge_maps/%s.map",
						"forge_maps/%s/%s.map"
					};
					int ci;
					for (ci = 0; ci < 5; ci++)
					{
						if (strstr(map_cands[ci], "%s/%s"))
							snprintf(full_path, sizeof(full_path), map_cands[ci], leaf, leaf);
						else
							snprintf(full_path, sizeof(full_path), map_cands[ci], leaf);
						tf = fopen(full_path, "rb");
						if (tf)
						{
							fclose(tf);
							break;
						}
					}
				}
				if (is_forge_created_map_file(full_path, leaf, &is_user, author, sizeof(author)))
				{
					if ((wanted_tab == 0 && is_user) || (wanted_tab == 1 && !is_user))
					{
						if (s_forge_menu.count < FORGE_MAX_EDIT_MAPS)
						{
							int idx = s_forge_menu.count++;
							snprintf(s_forge_menu.map_leaves[idx], sizeof(s_forge_menu.map_leaves[0]), "%s", leaf);
							snprintf(s_forge_menu.map_titles[idx], sizeof(s_forge_menu.map_titles[0]), "%s", leaf);
							snprintf(s_forge_menu.map_authors[idx], sizeof(s_forge_menu.map_authors[0]), "%s",
								is_user ? "User Created Forge Map" : "Downloaded Forge Map");
						}
					}
				}
			} while (FindNextFileA(h, &fd));
			CloseHandle(h);
		}
	}

	/* Also scan legacy custom_maps\*.map Forge maps */
	h = FindFirstFileA("custom_maps\\*.map", &fd);
	if (h == INVALID_HANDLE_VALUE)
		return;
	do
	{
		char full_path[512];
		char leaf[64];
		char author[64];
		char *dot;
		int is_user = 0;
		int k, already_listed = 0;

		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		if (_stricmp(fd.cFileName, "bitmaps.map") == 0 ||
			_stricmp(fd.cFileName, "sounds.map") == 0 ||
			_stricmp(fd.cFileName, "loc.map") == 0)
		{
			continue;
		}

		snprintf(leaf, sizeof(leaf), "%s", fd.cFileName);
		dot = strrchr(leaf, '.');
		if (dot)
			*dot = 0;

		for (k = 0; k < s_forge_menu.count; k++)
		{
			if (_stricmp(s_forge_menu.map_leaves[k], leaf) == 0)
			{
				already_listed = 1;
				break;
			}
		}
		if (already_listed)
			continue;

		snprintf(full_path, sizeof(full_path), "custom_maps/%s", fd.cFileName);
		if (is_forge_created_map_file(full_path, leaf, &is_user, author, sizeof(author)))
		{
			if ((wanted_tab == 0 && is_user) || (wanted_tab == 1 && !is_user))
			{
				if (s_forge_menu.count < FORGE_MAX_EDIT_MAPS)
				{
					int idx = s_forge_menu.count++;
					snprintf(s_forge_menu.map_leaves[idx], sizeof(s_forge_menu.map_leaves[0]), "%s", leaf);
					snprintf(s_forge_menu.map_titles[idx], sizeof(s_forge_menu.map_titles[0]), "%s", leaf);
					snprintf(s_forge_menu.map_authors[idx], sizeof(s_forge_menu.map_authors[0]), "%s",
						is_user ? "User Created Forge Map" : "Downloaded Forge Map");
				}
			}
		}
	} while (FindNextFileA(h, &fd));
	CloseHandle(h);
}

static void update_forge_edit_maps_widget(struct forge_widget_instance *list)
{
	struct forge_widget_instance *spinner = fw_named(list, "edit_maps_kind_spinner", 0);
	struct forge_widget_instance *desc = list->parameters.list.extended_description;
	struct forge_widget_instance *child;
	int kind = spinner ? (int)spinner->parameters.list.selected_index : 0;
	int row_idx = 0;
	int focused_entry = -1;

	if (kind < 0) kind = 0;
	if (kind > 1) kind = 1;
	if (kind != s_forge_menu.tab)
	{
		scan_forge_edit_maps(kind);
	}

	for (child = list->child; child; child = child->next, row_idx++)
	{
		int item_slot;
		if (row_idx < 1)
			continue;
		if (!child->name || strncmp(child->name, "list_item_", 10) != 0)
			break;
		item_slot = s_forge_menu.first + (row_idx - 1);
		if (s_forge_menu.count == 0)
		{
			child->visible = (row_idx == 1) ? TRUE : FALSE;
			if (row_idx == 1)
			{
				fw_text_set_ascii(
					fw_named(child, "list_item_text", 0),
					kind == 0 ? "(No User Created Maps Yet)" : "(No Downloaded Forge Maps)");
			}
		}
		else
		{
			child->visible = (item_slot < s_forge_menu.count) ? TRUE : FALSE;
			if (item_slot < s_forge_menu.count)
			{
				fw_text_set_ascii(fw_named(child, "list_item_text", 0), s_forge_menu.map_titles[item_slot]);
				if (child == list->focused_child)
					focused_entry = item_slot;
			}
		}
		if (fw_named(child, "list_item_arrows", 0))
			fw_named(child, "list_item_arrows", 0)->visible = (child == list->focused_child);
		if (fw_named(child, "scroll_up_button", 0))
			fw_named(child, "scroll_up_button", 0)->visible = FALSE;
		if (fw_named(child, "scroll_down_button", 0))
			fw_named(child, "scroll_down_button", 0)->visible = FALSE;
	}

	if (focused_entry >= 0 && focused_entry < s_forge_menu.count)
		s_forge_menu.chosen = focused_entry;

	if (desc)
	{
		struct forge_widget_instance *sp_name = fw_named(desc, "replay_level_right_name", 0);
		struct forge_widget_instance *sp_pic = fw_named(desc, "replay_level_right_pic", 0);
		struct forge_widget_instance *sp_data = fw_named(desc, "replay_level_right_data", 0);
		struct forge_widget_instance *mp_name = fw_named(desc, "mp_map_right_name", 0);
		struct forge_widget_instance *mp_pic = fw_named(desc, "mp_map_right_pic", 0);
		struct forge_widget_instance *mp_data = fw_named(desc, "mp_map_right_data", 0);
		long d_idx;

		/* Always hide the single-player right-hand widgets ('Truth and Reconciliation' default) and difficulty icons */
		if (sp_name) sp_name->visible = FALSE;
		if (sp_pic) sp_pic->visible = FALSE;
		if (sp_data) sp_data->visible = FALSE;
		for (d_idx = 0; d_idx < 3; d_idx++)
		{
			struct forge_widget_instance *di = fw_named(desc, "difficulty_indicator", d_idx);
			if (di) di->visible = FALSE;
		}

		if (s_forge_menu.count > 0 && s_forge_menu.chosen >= 0 && s_forge_menu.chosen < s_forge_menu.count)
		{
			char level_name[128];
			char info[192];
			short disp_idx;

			snprintf(level_name, sizeof(level_name), "custom_maps\\%s", s_forge_menu.map_leaves[s_forge_menu.chosen]);
			disp_idx = custom_edition_maps_display_index(level_name);
			snprintf(info, sizeof(info), "%s\r\n%s.map\r\nPress OK to Edit in Forge",
				s_forge_menu.map_authors[s_forge_menu.chosen],
				s_forge_menu.map_leaves[s_forge_menu.chosen]);

			if (disp_idx != NONE)
			{
				wchar_t *desc_w = custom_edition_maps_description(disp_idx);
				if (desc_w)
				{
					int k;
					for (k = 0; info[k] && k < 254; k++)
						desc_w[k] = (wchar_t)(unsigned char)info[k];
					desc_w[k] = 0;
				}
				if (mp_name)
				{
					mp_name->visible = TRUE;
					mp_name->parameters.text_box.string_list_index = disp_idx;
				}
				if (mp_pic)
				{
					mp_pic->visible = TRUE;
					mp_pic->animation.current_frame_index = disp_idx;
				}
				if (mp_data)
				{
					mp_data->visible = TRUE;
					mp_data->parameters.text_box.string_list_index = disp_idx;
				}
			}
			else
			{
				if (mp_name) mp_name->visible = FALSE;
				if (mp_pic) mp_pic->visible = FALSE;
				if (mp_data) mp_data->visible = FALSE;
			}
		}
		else
		{
			if (mp_name) mp_name->visible = FALSE;
			if (mp_pic) mp_pic->visible = FALSE;
			if (mp_data) mp_data->visible = FALSE;
		}
	}
}

/* =========================================================================
 * FORGE TAG OPTIONS: 4-OPTION SUBMENU HANDLERS
 *   1. ADD TAGS (Map Picker -> Map Tag Browser -> Custom Name Prompt -> OCEZ Extract)
 *   2. INSTALLED TAGS (5-Category Cache Browser -> 3D Preview -> Delete Dialog)
 *   3. RESET STOCK TAGS (Confirm -> Clear Stock Cache, Keep Custom Tags)
 *   4. RESET CUSTOM TAGS (Confirm -> Clear Custom Tags)
 * ========================================================================= */

#define FORGE_TAG_OPT_MAX_MAPS 512
#define FORGE_TAG_OPT_MAX_TAGS 1024

struct forge_tag_opt_map_entry
{
	char title[64];
	char file_path[256];
	short stock_sp_idx;
	short stock_mp_idx;
	short ce_disp_idx;
};

struct forge_tag_opt_tag_entry
{
	uint32_t group_tag;
	uint8_t is_custom;
	char short_name[64];
	char display_row[64];
	char tag_path[256];
	uint32_t size_bytes;
};

static struct
{
	/* Step A: Map Picker */
	int map_kind;
	int map_count;
	int map_first;
	int map_chosen;
	int map_prev_row_focus;
	struct forge_tag_opt_map_entry maps[FORGE_TAG_OPT_MAX_MAPS];
	char active_donor_map_path[256];

	/* Step B: Map Tag Browser */
	int browser_cat;
	int browser_count;
	int browser_first;
	int browser_chosen;
	int browser_prev_row_focus;
	struct forge_tag_opt_tag_entry browser_tags[FORGE_TAG_OPT_MAX_TAGS];

	/* Step C: Custom Tag Name Prompt */
	char custom_name[48];
	int custom_name_len;
	unsigned long name_open_ms;
	int prev_key_down[256];

	/* Option 2: Installed Tags Browser */
	int inst_cat;
	int inst_count;
	int inst_first;
	int inst_chosen;
	int inst_prev_row_focus;
	struct forge_tag_opt_tag_entry inst_tags[FORGE_TAG_OPT_MAX_TAGS];

	/* Option 5: Snap Points Browser & Interactive 3D Snap Point Editor */
	int snap_cat; /* 0 = SCENERY, 1 = DOORS, 2 = CUSTOM SCENERY, 3 = CUSTOM DOORS */
	int snap_count;
	int snap_first;
	int snap_chosen;
	int snap_prev_row_focus;
	struct forge_tag_opt_tag_entry snap_tags[FORGE_TAG_OPT_MAX_TAGS];
	int snap_editor_active;
	int snap_editor_row;
	int snap_active_pt_idx;
	int snap_autogen_mode;
	float snap_cam_yaw_deg;
	float snap_cam_pitch_deg;
	float snap_cam_zoom;
	int snap_prev_left;
	int snap_prev_right;
	int snap_prev_del;
} s_forge_tag_opt = { -1, 0, 0, 0, -1, { { { 0 } } }, "", -1, 0, 0, 0, -1, { { 0 } }, "", 0, 0, { 0 }, -1, 0, 0, 0, -1, { { 0 } }, -1, 0, 0, 0, -1, { { 0 } }, 0, 0, 0, 0, 38.0f, 22.0f, 1.0f, 0, 0, 0 };

static void forge_format_tag_type_label(uint32_t grp, char *out, size_t out_sz)
{
	if (grp == 'scen') snprintf(out, out_sz, "Scenery (scen)");
	else if (grp == 'vehi') snprintf(out, out_sz, "Vehicle (vehi)");
	else if (grp == 'mach') snprintf(out, out_sz, "Device Machine (mach)");
	else if (grp == 'ctrl') snprintf(out, out_sz, "Device Control (ctrl)");
	else if (grp == 'lifi') snprintf(out, out_sz, "Light Fixture (lifi)");
	else if (grp == 'weap') snprintf(out, out_sz, "Weapon (weap)");
	else if (grp == 'eqip') snprintf(out, out_sz, "Equipment (eqip)");
	else if (grp == 'garb') snprintf(out, out_sz, "Garbage (garb)");
	else if (grp == 'bipd') snprintf(out, out_sz, "Biped (bipd)");
	else
	{
		char gs[5];
		gs[0] = (char)((grp >> 24) & 0xFF);
		gs[1] = (char)((grp >> 16) & 0xFF);
		gs[2] = (char)((grp >> 8) & 0xFF);
		gs[3] = (char)(grp & 0xFF);
		gs[4] = '\0';
		snprintf(out, out_sz, "Tag (%s)", gs);
	}
}

static void forge_format_size_bytes(uint32_t bytes, char *out, size_t out_sz)
{
	if (bytes == 0)
		snprintf(out, out_sz, "Cached");
	else if (bytes >= 1024u * 1024u)
		snprintf(out, out_sz, "%.2f MB", (double)bytes / (1024.0 * 1024.0));
	else if (bytes >= 1024u)
		snprintf(out, out_sz, "%.1f KB", (double)bytes / 1024.0);
	else
		snprintf(out, out_sz, "%u B", (unsigned int)bytes);
}

static void forge_wrap_title_two_lines(char const *src, char *out, size_t out_sz)
{
	size_t len;
	if (!src || !src[0])
	{
		snprintf(out, out_sz, "-");
		return;
	}
	len = strlen(src);
	if (len <= 13u)
	{
		snprintf(out, out_sz, "%s", src);
		return;
	}
	{
		size_t split = 13u;
		size_t i;
		char line1[20], line2[20];
		for (i = 13u; i >= 5u; i--)
		{
			if (src[i] == ' ' || src[i] == '_' || src[i] == '-')
			{
				split = i;
				break;
			}
		}
		memcpy(line1, src, split);
		line1[split] = '\0';
		while (src[split] == ' ' || src[split] == '_')
			split++;
		snprintf(line2, sizeof(line2), "%.14s", src + split);
		snprintf(out, out_sz, "%s\r\n%s", line1, line2);
	}
}

static void forge_wrap_info_line(char const *prefix, char const *val, char *out, size_t out_sz)
{
	char full[96];
	size_t len;
	snprintf(full, sizeof(full), "%s%s", prefix ? prefix : "", (val && val[0]) ? val : "-");
	len = strlen(full);
	if (len <= 18u)
	{
		snprintf(out, out_sz, "%s", full);
		return;
	}
	{
		size_t split = 18u;
		size_t i;
		char line1[24], line2[24];
		for (i = 18u; i >= 6u; i--)
		{
			if (full[i] == ' ' || full[i] == '_' || full[i] == '-')
			{
				split = i;
				break;
			}
		}
		memcpy(line1, full, split);
		line1[split] = '\0';
		while (full[split] == ' ' || full[split] == '_')
			split++;
		snprintf(line2, sizeof(line2), "%.16s", full + split);
		snprintf(out, out_sz, "%s\r\n  %s", line1, line2);
	}
}

static int forge_resolve_donor_map_file_path(char const *level_or_stem, int is_custom, char *out_path, size_t out_sz)
{
	char cand[512];
	char const *leaf = level_or_stem;
	FILE *fp;
	if (!level_or_stem || !level_or_stem[0])
		return 0;
	{
		char const *bs = strrchr(level_or_stem, '\\');
		char const *fs = strrchr(level_or_stem, '/');
		if (bs && (!fs || bs > fs)) leaf = bs + 1;
		else if (fs) leaf = fs + 1;
	}
	snprintf(cand, sizeof(cand), "%s.map", level_or_stem);
	fp = fopen(cand, "rb");
	if (fp) { fclose(fp); snprintf(out_path, out_sz, "%s", cand); return 1; }
	if (is_custom)
	{
		snprintf(cand, sizeof(cand), "custom_maps/%s.map", leaf);
		fp = fopen(cand, "rb");
		if (fp) { fclose(fp); snprintf(out_path, out_sz, "%s", cand); return 1; }
		snprintf(cand, sizeof(cand), "OpenCE Forge/forge_maps/%s/%s.map", leaf, leaf);
		fp = fopen(cand, "rb");
		if (fp) { fclose(fp); snprintf(out_path, out_sz, "%s", cand); return 1; }
		snprintf(cand, sizeof(cand), "Faber_data/forge_maps/%s/%s.map", leaf, leaf);
		fp = fopen(cand, "rb");
		if (fp) { fclose(fp); snprintf(out_path, out_sz, "%s", cand); return 1; }
		snprintf(cand, sizeof(cand), "forge_maps/%s/%s.map", leaf, leaf);
		fp = fopen(cand, "rb");
		if (fp) { fclose(fp); snprintf(out_path, out_sz, "%s", cand); return 1; }
	}
	snprintf(cand, sizeof(cand), "maps/%s.map", leaf);
	fp = fopen(cand, "rb");
	if (fp) { fclose(fp); snprintf(out_path, out_sz, "%s", cand); return 1; }
	snprintf(cand, sizeof(cand), "custom_maps/%s.map", leaf);
	fp = fopen(cand, "rb");
	if (fp) { fclose(fp); snprintf(out_path, out_sz, "%s", cand); return 1; }
	snprintf(cand, sizeof(cand), "C:/Program Files (x86)/Microsoft Games/Halo Custom Edition/maps/%s.map", leaf);
	fp = fopen(cand, "rb");
	if (fp) { fclose(fp); snprintf(out_path, out_sz, "%s", cand); return 1; }
	snprintf(cand, sizeof(cand), "C:/Program Files/Microsoft Games/Halo Custom Edition/maps/%s.map", leaf);
	fp = fopen(cand, "rb");
	if (fp) { fclose(fp); snprintf(out_path, out_sz, "%s", cand); return 1; }
	return 0;
}

static void forge_scan_add_tags_maps_by_kind(int kind)
{
	boolean is_sp;
	short cnt;
	int i;

	if (kind < 0) kind = 0;
	if (kind > 1) kind = 1;
	s_forge_tag_opt.map_kind = kind;
	s_forge_tag_opt.map_count = 0;
	s_forge_tag_opt.map_first = 0;
	s_forge_tag_opt.map_chosen = 0;
	s_forge_tag_opt.map_prev_row_focus = -1;

	is_sp = (kind == 0) ? TRUE : FALSE;
	custom_edition_maps_look_again();
	cnt = custom_edition_maps_count(is_sp);
	for (i = 0; i < cnt && s_forge_tag_opt.map_count < FORGE_TAG_OPT_MAX_MAPS; i++)
	{
		short disp = custom_edition_maps_display_index_of(is_sp, (short)i);
		char const *lvl = custom_edition_maps_level_name(disp);
		char const *leaf = lvl;
		char fpath[256];
		if (!lvl || !lvl[0])
			continue;
		{
			char const *bs = strrchr(lvl, '\\');
			char const *fs = strrchr(lvl, '/');
			if (bs && (!fs || bs > fs)) leaf = bs + 1;
			else if (fs) leaf = fs + 1;
		}
		if (forge_resolve_donor_map_file_path(lvl, 1, fpath, sizeof(fpath)))
		{
			struct forge_tag_opt_map_entry *e = &s_forge_tag_opt.maps[s_forge_tag_opt.map_count++];
			wchar_t const *wtitle = custom_edition_maps_name(disp);
			if (wtitle && wtitle[0])
			{
				int c = 0;
				while (wtitle[c] && c < (int)sizeof(e->title) - 1)
				{
					e->title[c] = (char)(wtitle[c] & 0xFF);
					c++;
				}
				e->title[c] = '\0';
			}
			else
			{
				snprintf(e->title, sizeof(e->title), "%s", leaf);
			}
			snprintf(e->file_path, sizeof(e->file_path), "%s", fpath);
			e->stock_sp_idx = NONE;
			e->stock_mp_idx = NONE;
			e->ce_disp_idx = disp;
		}
	}
}

static int forge_FocusRowIndexOfList(struct forge_widget_instance *list, struct forge_widget_instance **out_rows)
{
	struct forge_widget_instance *c;
	int r = 0, focused_r = -1;
	for (c = list ? list->child : NULL; c; c = c->next)
	{
		if (c->name && strncmp(c->name, "list_item_", 10) == 0 && r < 10)
		{
			if (out_rows)
				out_rows[r] = c;
			if (c == list->focused_child)
				focused_r = r;
			r++;
		}
	}
	return focused_r;
}

static void forge_handle_10row_edge_scroll(
	struct forge_widget_instance *list,
	struct forge_widget_instance **rows,
	int focused_r,
	int *io_prev_focus,
	int *io_first,
	int *io_chosen,
	int total_count)
{
	if (total_count <= 0)
	{
		*io_first = 0;
		*io_chosen = 0;
		*io_prev_focus = focused_r;
		return;
	}
	if (focused_r >= 0)
	{
		/* Check if user pressed UP from row 0 or DOWN from row 9 */
		if (*io_prev_focus == 0 && focused_r != 0 && focused_r != 1 && *io_first > 0)
		{
			/* Focus wrapped from row 0 up to spinner -> scroll list up and keep focus on row 0 */
			(*io_first)--;
			list->focused_child = rows[0];
			focused_r = 0;
		}
		else if (*io_prev_focus == 9 && focused_r != 9 && focused_r != 8 && (*io_first + 10) < total_count)
		{
			(*io_first)++;
			list->focused_child = rows[9];
			focused_r = 9;
		}
		else if (focused_r == 0 && *io_prev_focus == 1 && *io_first > 0 && (*io_first + focused_r) > 0)
		{
			/* Reached top visible row while more items exist above: scroll up */
			(*io_first)--;
			list->focused_child = rows[1];
			focused_r = 1;
		}
		else if (focused_r == 9 && *io_prev_focus == 8 && (*io_first + 10) < total_count)
		{
			/* Reached bottom visible row while more items exist below: scroll down */
			(*io_first)++;
			list->focused_child = rows[8];
			focused_r = 8;
		}

		if (*io_first + focused_r >= total_count)
		{
			int valid_r = total_count - 1 - *io_first;
			if (valid_r < 0) valid_r = 0;
			if (valid_r > 9) valid_r = 9;
			if (rows[valid_r])
				list->focused_child = rows[valid_r];
			focused_r = valid_r;
		}
		*io_chosen = *io_first + focused_r;
	}
	else if (*io_prev_focus == 0 && list->focused_child && list->focused_child->name &&
			 (strcmp(list->focused_child->name, "forge_add_tags_map_kind_row") == 0 ||
			  strcmp(list->focused_child->name, "forge_tag_cat_row") == 0 ||
			  strcmp(list->focused_child->name, "forge_snap_cat_row") == 0) &&
			 *io_first > 0)
	{
		(*io_first)--;
		list->focused_child = rows[0];
		focused_r = 0;
		*io_chosen = *io_first;
	}
	else if (*io_prev_focus == 9 && list->focused_child && list->focused_child->name &&
			 strcmp(list->focused_child->name, "forge_browser_button_bar") == 0 &&
			 (*io_first + 10) < total_count)
	{
		(*io_first)++;
		list->focused_child = rows[9];
		focused_r = 9;
		*io_chosen = *io_first + 9;
	}

	if (*io_chosen < 0) *io_chosen = 0;
	if (*io_chosen >= total_count) *io_chosen = total_count - 1;
	*io_prev_focus = focused_r;
}

static void update_forge_add_tags_map_widget(struct forge_widget_instance *list)
{
	struct forge_widget_instance *spinner = fw_named(list, "forge_add_tags_map_kind_spinner", 0);
	struct forge_widget_instance *desc = list->parameters.list.extended_description;
	struct forge_widget_instance *rows[10] = { NULL };
	int kind = spinner ? (int)spinner->parameters.list.selected_index : 1;
	int focused_r, r;

	if (kind < 0) kind = 0;
	if (kind > 1) kind = 1;
	if (spinner && spinner->parameters.list.selected_index != kind)
		spinner->parameters.list.selected_index = (short)kind;
	if (kind != s_forge_tag_opt.map_kind)
		forge_scan_add_tags_maps_by_kind(kind);

	focused_r = forge_FocusRowIndexOfList(list, rows);
	forge_handle_10row_edge_scroll(
		list, rows, focused_r,
		&s_forge_tag_opt.map_prev_row_focus,
		&s_forge_tag_opt.map_first,
		&s_forge_tag_opt.map_chosen,
		s_forge_tag_opt.map_count);

	for (r = 0; r < 10; r++)
	{
		struct forge_widget_instance *child = rows[r];
		int slot = s_forge_tag_opt.map_first + r;
		if (!child) continue;
		if (s_forge_tag_opt.map_count == 0)
		{
			child->visible = (r == 0) ? TRUE : FALSE;
			if (r == 0)
				fw_text_set_ascii(fw_named(child, "list_item_text", 0), "(No Maps Found)");
			if (fw_named(child, "list_item_arrows", 0))
				fw_named(child, "list_item_arrows", 0)->visible = FALSE;
		}
		else
		{
			child->visible = (slot < s_forge_tag_opt.map_count) ? TRUE : FALSE;
			if (slot < s_forge_tag_opt.map_count)
				fw_text_set_ascii(fw_named(child, "list_item_text", 0), s_forge_tag_opt.maps[slot].title);
			if (fw_named(child, "list_item_arrows", 0))
				fw_named(child, "list_item_arrows", 0)->visible = (slot == s_forge_tag_opt.map_chosen);
		}
		if (fw_named(child, "scroll_up_button", 0))
			fw_named(child, "scroll_up_button", 0)->visible = (r == 0 && s_forge_tag_opt.map_first > 0);
		if (fw_named(child, "scroll_down_button", 0))
			fw_named(child, "scroll_down_button", 0)->visible = (r == 9 && (s_forge_tag_opt.map_first + 10) < s_forge_tag_opt.map_count);
	}

	if (desc)
	{
		struct forge_widget_instance *sp_name = fw_named(desc, "replay_level_right_name", 0);
		struct forge_widget_instance *sp_pic = fw_named(desc, "replay_level_right_pic", 0);
		struct forge_widget_instance *sp_data = fw_named(desc, "replay_level_right_data", 0);
		struct forge_widget_instance *mp_name = fw_named(desc, "mp_map_right_name", 0);
		struct forge_widget_instance *mp_pic = fw_named(desc, "mp_map_right_pic", 0);
		struct forge_widget_instance *mp_data = fw_named(desc, "mp_map_right_data", 0);
		long d_idx;

		for (d_idx = 0; d_idx < 3; d_idx++)
		{
			struct forge_widget_instance *di = fw_named(desc, "difficulty_indicator", d_idx);
			if (di) di->visible = FALSE;
		}
		if (sp_name) sp_name->visible = FALSE;
		if (sp_pic) sp_pic->visible = FALSE;
		if (sp_data) sp_data->visible = FALSE;
		if (mp_name) mp_name->visible = FALSE;
		if (mp_pic) mp_pic->visible = FALSE;
		if (mp_data) mp_data->visible = FALSE;

		if (s_forge_tag_opt.map_count > 0 && s_forge_tag_opt.map_chosen >= 0 &&
			s_forge_tag_opt.map_chosen < s_forge_tag_opt.map_count)
		{
			struct forge_tag_opt_map_entry const *me = &s_forge_tag_opt.maps[s_forge_tag_opt.map_chosen];
			if (me->stock_sp_idx != NONE)
			{
				if (sp_name) { sp_name->visible = TRUE; sp_name->parameters.text_box.string_list_index = me->stock_sp_idx; }
				if (sp_pic)  { sp_pic->visible = TRUE;  sp_pic->animation.current_frame_index = me->stock_sp_idx; }
				if (sp_data) { sp_data->visible = TRUE; sp_data->parameters.text_box.string_list_index = me->stock_sp_idx; }
			}
			else if (me->stock_mp_idx != NONE)
			{
				if (mp_name) { mp_name->visible = TRUE; mp_name->parameters.text_box.string_list_index = me->stock_mp_idx; }
				if (mp_pic)  { mp_pic->visible = TRUE;  mp_pic->animation.current_frame_index = me->stock_mp_idx; }
				if (mp_data) { mp_data->visible = TRUE; mp_data->parameters.text_box.string_list_index = me->stock_mp_idx; }
			}
			else if (me->ce_disp_idx != NONE)
			{
				if (mp_name) { mp_name->visible = TRUE; mp_name->parameters.text_box.string_list_index = me->ce_disp_idx; }
				if (mp_pic)  { mp_pic->visible = TRUE;  mp_pic->animation.current_frame_index = me->ce_disp_idx; }
				if (mp_data) { mp_data->visible = TRUE; mp_data->parameters.text_box.string_list_index = me->ce_disp_idx; }
			}
		}
	}
}

static int forge_group_matches_tag_opt_cat(uint32_t grp, int cat)
{
	if (cat == 0) return (grp == 'scen');
	if (cat == 1) return (grp == 'vehi');
	if (cat == 2) return (grp == 'mach' || grp == 'ctrl' || grp == 'lifi');
	if (cat == 3) return (grp == 'weap');
	if (cat == 4) return (grp == 'eqip' || grp == 'garb');
	return 0;
}

static void forge_scan_donor_map_tags_by_cat(int cat)
{
	FILE *fp;
	char open_path[512];
	uint8_t hdr[2048];
	uint32_t head_magic, map_ver, tag_off, tag_sz, inst_addr, inst_off, tag_base;
	int32_t tag_count, i;
	uint8_t *tag_buf;

	if (cat < 0) cat = 0;
	if (cat > 4) cat = 4;
	s_forge_tag_opt.browser_cat = cat;
	s_forge_tag_opt.browser_count = 0;
	s_forge_tag_opt.browser_first = 0;
	s_forge_tag_opt.browser_chosen = 0;
	s_forge_tag_opt.browser_prev_row_focus = -1;

	if (!s_forge_tag_opt.active_donor_map_path[0])
		return;
	if (!forge_get_readable_donor_map_path(s_forge_tag_opt.active_donor_map_path, open_path, sizeof(open_path)))
		return;

	fp = fopen(open_path, "rb");
	if (!fp)
		return;
	if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr))
	{
		fclose(fp);
		return;
	}
	head_magic = *(uint32_t const *)(hdr + 0x00);
	map_ver = *(uint32_t const *)(hdr + 0x04);
	if (head_magic != 0x68656164u)
	{
		fclose(fp);
		return;
	}

	tag_off = *(uint32_t const *)(hdr + 0x10);
	tag_sz = *(uint32_t const *)(hdr + 0x14);
	if (tag_off < 2048u || tag_sz < 64u || tag_sz > 128u * 1024u * 1024u)
	{
		fclose(fp);
		return;
	}
	tag_buf = (uint8_t *)malloc(tag_sz);
	if (!tag_buf || fseek(fp, (long)tag_off, SEEK_SET) != 0 || fread(tag_buf, 1, tag_sz, fp) != tag_sz)
	{
		free(tag_buf);
		fclose(fp);
		return;
	}
	fclose(fp);

	inst_addr = *(uint32_t const *)(tag_buf + 0x00);
	tag_count = *(int32_t const *)(tag_buf + 0x0C);
	inst_off = (map_ver == 5u) ? 0x24u : 0x28u;
	tag_base = inst_addr - inst_off;
	if (tag_count <= 0 || tag_count > 32768 || inst_off + (uint32_t)tag_count * 32u > tag_sz)
	{
		free(tag_buf);
		return;
	}

	for (i = 0; i < tag_count && s_forge_tag_opt.browser_count < FORGE_TAG_OPT_MAX_TAGS; i++)
	{
		uint32_t io = inst_off + (uint32_t)i * 32u;
		uint32_t grp = *(uint32_t const *)(tag_buf + io + 0x00);
		uint32_t name_addr = *(uint32_t const *)(tag_buf + io + 0x10);
		uint32_t data_addr = *(uint32_t const *)(tag_buf + io + 0x14);
		char const *tname;
		char const *leaf;
		char const *p;
		struct forge_tag_opt_tag_entry *te;

		if (!forge_group_matches_tag_opt_cat(grp, cat))
			continue;
		if (name_addr < tag_base || (name_addr - tag_base) >= tag_sz)
			continue;
		if (data_addr < tag_base || (data_addr - tag_base) + 0x38u > tag_sz)
			continue;
		tname = (char const *)(tag_buf + (name_addr - tag_base));
		if (!tname[0])
			continue;

		leaf = tname;
		for (p = tname; *p; p++)
		{
			if (*p == '\\' || *p == '/')
				leaf = p + 1;
		}
		te = &s_forge_tag_opt.browser_tags[s_forge_tag_opt.browser_count++];
		te->group_tag = grp;
		te->is_custom = 0;
		snprintf(te->short_name, sizeof(te->short_name), "%s", leaf);
		snprintf(te->display_row, sizeof(te->display_row), "%s", leaf);
		snprintf(te->tag_path, sizeof(te->tag_path), "%s", tname);
		te->size_bytes = 0;
	}

	free(tag_buf);
}

static void forge_populate_10row_tag_list(
	struct forge_widget_instance *const rows[10],
	struct forge_tag_opt_tag_entry const *entries,
	int count,
	int first,
	int chosen,
	char const *empty_label)
{
	int r;
	for (r = 0; r < 10; r++)
	{
		struct forge_widget_instance *child = rows[r];
		int slot = first + r;
		if (!child) continue;
		if (count == 0)
		{
			child->visible = (r == 0) ? TRUE : FALSE;
			if (r == 0)
				fw_text_set_ascii(fw_named(child, "list_item_text", 0), empty_label);
			if (fw_named(child, "list_item_arrows", 0))
				fw_named(child, "list_item_arrows", 0)->visible = FALSE;
		}
		else
		{
			child->visible = (slot < count) ? TRUE : FALSE;
			if (slot < count)
				fw_text_set_ascii(fw_named(child, "list_item_text", 0), entries[slot].display_row);
			if (fw_named(child, "list_item_arrows", 0))
				fw_named(child, "list_item_arrows", 0)->visible = (slot == chosen);
		}
		if (fw_named(child, "scroll_up_button", 0))
			fw_named(child, "scroll_up_button", 0)->visible = (r == 0 && first > 0);
		if (fw_named(child, "scroll_down_button", 0))
			fw_named(child, "scroll_down_button", 0)->visible = (r == 9 && (first + 10) < count);
	}
}

static void update_forge_add_tags_browser_widget(struct forge_widget_instance *list)
{
	struct forge_widget_instance *spinner = fw_named(list, "forge_tag_cat_spinner", 0);
	struct forge_widget_instance *desc = list->parameters.list.extended_description;
	struct forge_widget_instance *rows[10] = { NULL };
	int cat = spinner ? (int)spinner->parameters.list.selected_index : 0;
	int focused_r;

	if (cat < 0) cat = 0;
	if (cat > 4) cat = 4;
	if (cat != s_forge_tag_opt.browser_cat)
		forge_scan_donor_map_tags_by_cat(cat);

	focused_r = forge_FocusRowIndexOfList(list, rows);
	forge_handle_10row_edge_scroll(
		list, rows, focused_r,
		&s_forge_tag_opt.browser_prev_row_focus,
		&s_forge_tag_opt.browser_first,
		&s_forge_tag_opt.browser_chosen,
		s_forge_tag_opt.browser_count);

	forge_populate_10row_tag_list(
		rows,
		s_forge_tag_opt.browser_tags,
		s_forge_tag_opt.browser_count,
		s_forge_tag_opt.browser_first,
		s_forge_tag_opt.browser_chosen,
		"(No Tags In Category)");

	if (desc)
	{
		struct forge_widget_instance *r_name = fw_named(desc, "forge_tag_right_name", 0);
		struct forge_widget_instance *r_data = fw_named(desc, "forge_tag_right_data", 0);
		if (s_forge_tag_opt.browser_count > 0 &&
			s_forge_tag_opt.browser_chosen >= 0 &&
			s_forge_tag_opt.browser_chosen < s_forge_tag_opt.browser_count)
		{
			struct forge_tag_opt_tag_entry *te = &s_forge_tag_opt.browser_tags[s_forge_tag_opt.browser_chosen];
			char wrapped_title[64];
			char type_str[48];
			char size_str[32];
			char info_buf[240];

			forge_tag_preview_set_map_tag(s_forge_tag_opt.active_donor_map_path, te->group_tag, te->tag_path);
			te->size_bytes = forge_tag_preview_get_current_size_bytes();

			forge_wrap_title_two_lines(te->short_name, wrapped_title, sizeof(wrapped_title));
			forge_format_tag_type_label(te->group_tag, type_str, sizeof(type_str));
			forge_format_size_bytes(te->size_bytes, size_str, sizeof(size_str));
			{
				char name_line[80];
				forge_wrap_info_line("- ", te->short_name, name_line, sizeof(name_line));
				snprintf(info_buf, sizeof(info_buf), "%s\r\n- %s\r\n- %s", name_line, type_str, size_str);
			}
			fw_text_set_ascii(r_name, wrapped_title);
			fw_text_set_ascii(r_data, info_buf);
		}
		else
		{
			forge_tag_preview_clear();
			fw_text_set_ascii(r_name, "NO TAG");
			fw_text_set_ascii(r_data, "- None\r\n- -\r\n- 0 KB");
		}
	}
}

static void forge_scan_installed_tags_by_cat(int cat)
{
	int i;
	if (cat < 0) cat = 0;
	if (cat > 4) cat = 4;
	s_forge_tag_opt.inst_cat = cat;
	s_forge_tag_opt.inst_count = 0;

	forge_runtime_init();
	forge_runtime_reload_catalogs();

	for (i = 0; i < g_forge.catalog_count && s_forge_tag_opt.inst_count < FORGE_TAG_OPT_MAX_TAGS; i++)
	{
		struct forge_catalog_entry const *ce = &g_forge.catalog[i];
		struct forge_tag_opt_tag_entry *te;
		char const *leaf = ce->tag_path;
		char const *p;

		if (ce->source_cache_id == 0)
			continue;
		if (ce->group_tag == 'spwn' || ce->group_tag == 'tele' || ce->group_tag == 'flag' || ce->group_tag == 'ffsp')
			continue;
		if (!forge_group_matches_tag_opt_cat(ce->group_tag, cat))
			continue;

		for (p = ce->tag_path; *p; p++)
		{
			if (*p == '\\' || *p == '/')
				leaf = p + 1;
		}
		te = &s_forge_tag_opt.inst_tags[s_forge_tag_opt.inst_count++];
		te->group_tag = ce->group_tag;
		te->is_custom = (ce->source_cache_id == 2) ? 1u : 0u;
		snprintf(te->short_name, sizeof(te->short_name), "%s", leaf);
		snprintf(te->display_row, sizeof(te->display_row), "%s", ce->display_name);
		snprintf(te->tag_path, sizeof(te->tag_path), "%s", ce->tag_path);
		te->size_bytes = 0;
	}

	if (s_forge_tag_opt.inst_chosen >= s_forge_tag_opt.inst_count)
		s_forge_tag_opt.inst_chosen = (s_forge_tag_opt.inst_count > 0) ? (s_forge_tag_opt.inst_count - 1) : 0;
	if (s_forge_tag_opt.inst_first > s_forge_tag_opt.inst_chosen)
		s_forge_tag_opt.inst_first = s_forge_tag_opt.inst_chosen;
}

static void update_forge_installed_tags_widget(struct forge_widget_instance *list)
{
	struct forge_widget_instance *spinner = fw_named(list, "forge_tag_cat_spinner", 0);
	struct forge_widget_instance *desc = list->parameters.list.extended_description;
	struct forge_widget_instance *rows[10] = { NULL };
	int cat = spinner ? (int)spinner->parameters.list.selected_index : 0;
	int focused_r;

	if (cat < 0) cat = 0;
	if (cat > 4) cat = 4;
	if (cat != s_forge_tag_opt.inst_cat)
	{
		s_forge_tag_opt.inst_first = 0;
		s_forge_tag_opt.inst_chosen = 0;
		s_forge_tag_opt.inst_prev_row_focus = -1;
		forge_scan_installed_tags_by_cat(cat);
	}

	focused_r = forge_FocusRowIndexOfList(list, rows);
	forge_handle_10row_edge_scroll(
		list, rows, focused_r,
		&s_forge_tag_opt.inst_prev_row_focus,
		&s_forge_tag_opt.inst_first,
		&s_forge_tag_opt.inst_chosen,
		s_forge_tag_opt.inst_count);

	forge_populate_10row_tag_list(
		rows,
		s_forge_tag_opt.inst_tags,
		s_forge_tag_opt.inst_count,
		s_forge_tag_opt.inst_first,
		s_forge_tag_opt.inst_chosen,
		"(No Installed Tags)");

	if (desc)
	{
		struct forge_widget_instance *r_name = fw_named(desc, "forge_tag_right_name", 0);
		struct forge_widget_instance *r_data = fw_named(desc, "forge_tag_right_data", 0);
		if (s_forge_tag_opt.inst_count > 0 &&
			s_forge_tag_opt.inst_chosen >= 0 &&
			s_forge_tag_opt.inst_chosen < s_forge_tag_opt.inst_count)
		{
			struct forge_tag_opt_tag_entry *te = &s_forge_tag_opt.inst_tags[s_forge_tag_opt.inst_chosen];
			char wrapped_title[64];
			char type_str[48];
			char size_str[32];
			char info_buf[240];

			forge_tag_preview_set_installed_tag(te->group_tag, te->tag_path, te->is_custom);
			te->size_bytes = forge_tag_preview_get_current_size_bytes();

			forge_wrap_title_two_lines(te->display_row, wrapped_title, sizeof(wrapped_title));
			forge_format_tag_type_label(te->group_tag, type_str, sizeof(type_str));
			forge_format_size_bytes(te->size_bytes, size_str, sizeof(size_str));
			{
				char name_line[80];
				forge_wrap_info_line("- ", te->display_row, name_line, sizeof(name_line));
				snprintf(info_buf, sizeof(info_buf), "%s\r\n- %s\r\n- %s", name_line, type_str, size_str);
			}
			fw_text_set_ascii(r_name, wrapped_title);
			fw_text_set_ascii(r_data, info_buf);
		}
		else
		{
			forge_tag_preview_clear();
			fw_text_set_ascii(r_name, "NO TAG");
			fw_text_set_ascii(r_data, "- None\r\n- -\r\n- 0 KB");
		}
	}
}

static char const *const s_snap_autogen_names[4] = {
	"4-SIDE BOX",
	"2-END BRIDGE",
	"6-FACE CUBE",
	"4 BASE EDGES"
};

static char const *const s_snap_preset_names[NUMBER_OF_FORGE_SNAP_PRESETS] = {
	"CUSTOM OFFSET",
	"FRONT FACE",
	"BACK FACE",
	"LEFT FACE",
	"RIGHT FACE",
	"TOP FACE",
	"BOTTOM FACE",
	"FRONT BASE",
	"BACK BASE",
	"LEFT BASE",
	"RIGHT BASE"
};

static char const *const s_snap_dir_names[NUMBER_OF_FORGE_SNAP_DIRS] = {
	"FRONT (+X)",
	"RIGHT (-Y)",
	"BACK (-X)",
	"LEFT (+Y)",
	"UP (+Z)",
	"DOWN (-Z)"
};

static void forge_refresh_snap_tag_row_label(struct forge_tag_opt_tag_entry *te)
{
	int pt_cnt;
	if (!te)
		return;
	pt_cnt = forge_snap_points_count_for_tag(te->group_tag, te->tag_path);
	if (pt_cnt > 0)
		snprintf(te->display_row, sizeof(te->display_row), "%.46s [%d]", te->short_name, pt_cnt);
	else
		snprintf(te->display_row, sizeof(te->display_row), "%s", te->short_name);
}

static void forge_scan_snap_tags_by_cat(int cat)
{
	int i;
	if (cat < 0) cat = 0;
	if (cat > 3) cat = 3;
	s_forge_tag_opt.snap_cat = cat;
	s_forge_tag_opt.snap_count = 0;

	forge_runtime_init();
	forge_runtime_reload_catalogs();
	forge_snap_points_load_all();

	for (i = 0; i < g_forge.catalog_count && s_forge_tag_opt.snap_count < FORGE_TAG_OPT_MAX_TAGS; i++)
	{
		struct forge_catalog_entry const *ce = &g_forge.catalog[i];
		struct forge_tag_opt_tag_entry *te;
		int match = 0;

		if (ce->source_cache_id == 0)
			continue;
		if (ce->group_tag == 'spwn' || ce->group_tag == 'tele' || ce->group_tag == 'flag' || ce->group_tag == 'ffsp')
			continue;
		if (!forge_catalog_entry_allowed(ce))
			continue;

		if (cat == 0) /* SCENERY (stock) */
			match = (ce->category == _forge_cat_scenery && ce->source_cache_id != 2);
		else if (cat == 1) /* DOORS (stock) */
			match = (ce->category == _forge_cat_barriers_devices && ce->source_cache_id != 2);
		else if (cat == 2) /* CUSTOM SCENERY */
			match = (ce->category == _forge_cat_scenery && ce->source_cache_id == 2);
		else if (cat == 3) /* CUSTOM DOORS */
			match = (ce->category == _forge_cat_barriers_devices && ce->source_cache_id == 2);

		if (!match)
			continue;

		te = &s_forge_tag_opt.snap_tags[s_forge_tag_opt.snap_count++];
		te->group_tag = ce->group_tag;
		te->is_custom = (ce->source_cache_id == 2) ? 1u : 0u;
		snprintf(te->short_name, sizeof(te->short_name), "%s", ce->display_name);
		snprintf(te->tag_path, sizeof(te->tag_path), "%s", ce->tag_path);
		te->size_bytes = 0;
		forge_refresh_snap_tag_row_label(te);
	}

	if (s_forge_tag_opt.snap_chosen >= s_forge_tag_opt.snap_count)
		s_forge_tag_opt.snap_chosen = (s_forge_tag_opt.snap_count > 0) ? (s_forge_tag_opt.snap_count - 1) : 0;
	if (s_forge_tag_opt.snap_first > s_forge_tag_opt.snap_chosen)
		s_forge_tag_opt.snap_first = s_forge_tag_opt.snap_chosen;
}

static void forge_snap_editor_step_row(int row, int dir)
{
	struct forge_tag_opt_tag_entry *te;
	struct forge_tag_snap_config *cfg;
	struct forge_snap_point *pt = NULL;
	float step;

	if (s_forge_tag_opt.snap_count <= 0 ||
		s_forge_tag_opt.snap_chosen < 0 ||
		s_forge_tag_opt.snap_chosen >= s_forge_tag_opt.snap_count)
		return;

	te = &s_forge_tag_opt.snap_tags[s_forge_tag_opt.snap_chosen];
	cfg = forge_snap_points_get_for_tag(te->group_tag, te->tag_path, 1);
	if (!cfg)
		return;

	if (cfg->point_count > 0)
	{
		if (s_forge_tag_opt.snap_active_pt_idx < 0)
			s_forge_tag_opt.snap_active_pt_idx = 0;
		if (s_forge_tag_opt.snap_active_pt_idx >= cfg->point_count)
			s_forge_tag_opt.snap_active_pt_idx = cfg->point_count - 1;
		pt = &cfg->points[s_forge_tag_opt.snap_active_pt_idx];
	}

	step = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ? 0.02f : 0.08f;

	if (row == 0) /* AUTO-GEN MODE */
	{
		s_forge_tag_opt.snap_autogen_mode = (s_forge_tag_opt.snap_autogen_mode + dir + 4) % 4;
		forge_snap_points_auto_generate(te->group_tag, te->tag_path, s_forge_tag_opt.snap_autogen_mode);
		s_forge_tag_opt.snap_active_pt_idx = 0;
		forge_refresh_snap_tag_row_label(te);
		ui_play_audio_feedback_sound(6);
	}
	else if (row == 2 && cfg->point_count > 0) /* ACTIVE POINT */
	{
		s_forge_tag_opt.snap_active_pt_idx =
			(s_forge_tag_opt.snap_active_pt_idx + dir + cfg->point_count) % cfg->point_count;
		ui_play_audio_feedback_sound(6);
	}
	else if (row == 3 && pt) /* FACE/EDGE PRESET */
	{
		int pr = (int)pt->preset_id + dir;
		if (pr < 1) pr = NUMBER_OF_FORGE_SNAP_PRESETS - 1;
		if (pr >= NUMBER_OF_FORGE_SNAP_PRESETS) pr = 1;
		forge_snap_points_apply_preset_to_point(pt, pr, -1);
		forge_snap_points_save_all();
		ui_play_audio_feedback_sound(6);
	}
	else if (row == 4 && pt) /* OFFSET X */
	{
		pt->x += (float)dir * step;
		pt->preset_id = _forge_snap_preset_custom;
		forge_snap_points_save_all();
		ui_play_audio_feedback_sound(6);
	}
	else if (row == 5 && pt) /* OFFSET Y */
	{
		pt->y += (float)dir * step;
		pt->preset_id = _forge_snap_preset_custom;
		forge_snap_points_save_all();
		ui_play_audio_feedback_sound(6);
	}
	else if (row == 6 && pt) /* OFFSET Z */
	{
		pt->z += (float)dir * step;
		pt->preset_id = _forge_snap_preset_custom;
		forge_snap_points_save_all();
		ui_play_audio_feedback_sound(6);
	}
	else if (row == 7 && pt) /* NORMAL DIR */
	{
		int nd = ((int)pt->normal_dir + dir + NUMBER_OF_FORGE_SNAP_DIRS) % NUMBER_OF_FORGE_SNAP_DIRS;
		forge_snap_points_fill_normal_from_dir(pt, nd);
		forge_snap_points_save_all();
		ui_play_audio_feedback_sound(6);
	}
}

static void forge_snap_editor_activate_row(struct forge_widget_instance *list, int row)
{
	struct forge_tag_opt_tag_entry *te;
	struct forge_tag_snap_config *cfg;

	if (s_forge_tag_opt.snap_count <= 0 ||
		s_forge_tag_opt.snap_chosen < 0 ||
		s_forge_tag_opt.snap_chosen >= s_forge_tag_opt.snap_count)
		return;

	te = &s_forge_tag_opt.snap_tags[s_forge_tag_opt.snap_chosen];
	cfg = forge_snap_points_get_for_tag(te->group_tag, te->tag_path, 1);
	if (!cfg)
		return;

	if (row == 0) /* AUTO-GEN: apply current mode */
	{
		forge_snap_points_auto_generate(te->group_tag, te->tag_path, s_forge_tag_opt.snap_autogen_mode);
		s_forge_tag_opt.snap_active_pt_idx = 0;
		forge_refresh_snap_tag_row_label(te);
		ui_play_audio_feedback_sound(2);
	}
	else if (row == 1) /* ADD SNAP POINT */
	{
		if (cfg->point_count < FORGE_MAX_SNAP_POINTS_PER_TAG)
		{
			int idx = cfg->point_count++;
			int preset = (idx % 6) + 1;
			forge_snap_points_apply_preset_to_point(&cfg->points[idx], preset, -1);
			s_forge_tag_opt.snap_active_pt_idx = idx;
			forge_snap_points_save_all();
			forge_refresh_snap_tag_row_label(te);
			ui_play_audio_feedback_sound(2);
		}
		else
		{
			ui_play_audio_feedback_sound(4);
		}
	}
	else if (row >= 2 && row <= 7)
	{
		forge_snap_editor_step_row(row, 1);
	}
	else if (row == 8) /* DELETE POINT (or Shift = CLEAR ALL) */
	{
		if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0)
		{
			cfg->point_count = 0;
			s_forge_tag_opt.snap_active_pt_idx = 0;
			forge_snap_points_save_all();
			forge_refresh_snap_tag_row_label(te);
			ui_play_audio_feedback_sound(2);
		}
		else if (cfg->point_count > 0)
		{
			int del_i = s_forge_tag_opt.snap_active_pt_idx;
			int k;
			if (del_i < 0) del_i = 0;
			if (del_i >= cfg->point_count) del_i = cfg->point_count - 1;
			for (k = del_i; k + 1 < cfg->point_count; k++)
				cfg->points[k] = cfg->points[k + 1];
			cfg->point_count--;
			if (s_forge_tag_opt.snap_active_pt_idx >= cfg->point_count)
				s_forge_tag_opt.snap_active_pt_idx = (cfg->point_count > 0) ? (cfg->point_count - 1) : 0;
			forge_snap_points_save_all();
			forge_refresh_snap_tag_row_label(te);
			ui_play_audio_feedback_sound(2);
		}
		else
		{
			ui_play_audio_feedback_sound(4);
		}
	}
	else if (row == 9) /* SAVE & RETURN TO LIST */
	{
		struct forge_widget_instance *cat_row = fw_named(list, "forge_snap_cat_row", 0);
		struct forge_widget_instance *rows[10] = { NULL };
		int vis_r;
		forge_snap_points_save_all();
		forge_refresh_snap_tag_row_label(te);
		s_forge_tag_opt.snap_editor_active = 0;
		forge_snap_points_set_editor_overlay(0, te->group_tag, te->tag_path, -1, 0.0f, 0.0f, 1.0f);
		if (cat_row)
			cat_row->visible = TRUE;
		forge_FocusRowIndexOfList(list, rows);
		vis_r = s_forge_tag_opt.snap_chosen - s_forge_tag_opt.snap_first;
		if (vis_r < 0) vis_r = 0;
		if (vis_r > 9) vis_r = 9;
		if (rows[vis_r])
			list->focused_child = rows[vis_r];
		ui_play_audio_feedback_sound(2);
	}
}

static void update_forge_snap_points_widget(struct forge_widget_instance *list)
{
	struct forge_widget_instance *cat_row = fw_named(list, "forge_snap_cat_row", 0);
	struct forge_widget_instance *spinner = fw_named(list, "forge_snap_cat_spinner", 0);
	struct forge_widget_instance *desc = list->parameters.list.extended_description;
	struct forge_widget_instance *rows[10] = { NULL };
	int cat = spinner ? (int)spinner->parameters.list.selected_index : 0;
	int focused_r, r;

	if (cat < 0) cat = 0;
	if (cat > 3) cat = 3;
	if (cat != s_forge_tag_opt.snap_cat)
	{
		s_forge_tag_opt.snap_first = 0;
		s_forge_tag_opt.snap_chosen = 0;
		s_forge_tag_opt.snap_prev_row_focus = -1;
		forge_scan_snap_tags_by_cat(cat);
	}

	focused_r = forge_FocusRowIndexOfList(list, rows);

	if (s_forge_tag_opt.snap_editor_active)
	{
		struct forge_tag_opt_tag_entry *te = NULL;
		struct forge_tag_snap_config *cfg = NULL;
		struct forge_snap_point const *pt = NULL;
		int left_down, right_down, del_down;
		float lx = 0.0f, ly = 0.0f, rx = 0.0f, ry = 0.0f, lt = 0.0f, rt = 0.0f;

		if (cat_row)
			cat_row->visible = FALSE;
		if (list->focused_child == cat_row && rows[0])
		{
			list->focused_child = rows[0];
			focused_r = 0;
		}
		if (focused_r >= 0 && focused_r < 10)
			s_forge_tag_opt.snap_editor_row = focused_r;

		if (s_forge_tag_opt.snap_count > 0 &&
			s_forge_tag_opt.snap_chosen >= 0 &&
			s_forge_tag_opt.snap_chosen < s_forge_tag_opt.snap_count)
		{
			te = &s_forge_tag_opt.snap_tags[s_forge_tag_opt.snap_chosen];
			cfg = forge_snap_points_get_for_tag(te->group_tag, te->tag_path, 1);
		}

		/* Poll Left/Right for stepping active row & Q/E/Right-Stick for 3D camera orbit */
		forge_poll_controller_state(&lx, &ly, &rx, &ry, &lt, &rt);
		left_down = ((GetAsyncKeyState(VK_LEFT) & 0x8000) != 0) ||
			((GetAsyncKeyState('A') & 0x8000) != 0) ||
			forge_gp_button_down(FORGE_GP_DPAD_LEFT) ||
			(lx < -0.55f);
		right_down = ((GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0) ||
			((GetAsyncKeyState('D') & 0x8000) != 0) ||
			forge_gp_button_down(FORGE_GP_DPAD_RIGHT) ||
			(lx > 0.55f);
		del_down = ((GetAsyncKeyState(VK_DELETE) & 0x8000) != 0) ||
			((GetAsyncKeyState(VK_BACK) & 0x8000) != 0);

		if (focused_r >= 0 && focused_r < 10)
		{
			if (left_down && !s_forge_tag_opt.snap_prev_left)
				forge_snap_editor_step_row(focused_r, -1);
			if (right_down && !s_forge_tag_opt.snap_prev_right)
				forge_snap_editor_step_row(focused_r, 1);
		}
		if (del_down && !s_forge_tag_opt.snap_prev_del)
			forge_snap_editor_activate_row(list, 8);

		s_forge_tag_opt.snap_prev_left = left_down;
		s_forge_tag_opt.snap_prev_right = right_down;
		s_forge_tag_opt.snap_prev_del = del_down;

		/* Orbit 3D preview camera with Q/E or Right Stick, zoom with +/- */
		if ((GetAsyncKeyState('Q') & 0x8000) != 0)
			s_forge_tag_opt.snap_cam_yaw_deg -= 2.8f;
		if ((GetAsyncKeyState('E') & 0x8000) != 0)
			s_forge_tag_opt.snap_cam_yaw_deg += 2.8f;
		if (rx < -0.2f || rx > 0.2f)
			s_forge_tag_opt.snap_cam_yaw_deg += rx * 3.2f;
		if (ry < -0.2f || ry > 0.2f)
		{
			s_forge_tag_opt.snap_cam_pitch_deg += ry * 2.2f;
			if (s_forge_tag_opt.snap_cam_pitch_deg < -65.0f) s_forge_tag_opt.snap_cam_pitch_deg = -65.0f;
			if (s_forge_tag_opt.snap_cam_pitch_deg >  65.0f) s_forge_tag_opt.snap_cam_pitch_deg =  65.0f;
		}
		if ((GetAsyncKeyState(VK_OEM_PLUS) & 0x8000) != 0)
		{
			s_forge_tag_opt.snap_cam_zoom += 0.03f;
			if (s_forge_tag_opt.snap_cam_zoom > 2.2f) s_forge_tag_opt.snap_cam_zoom = 2.2f;
		}
		if ((GetAsyncKeyState(VK_OEM_MINUS) & 0x8000) != 0)
		{
			s_forge_tag_opt.snap_cam_zoom -= 0.03f;
			if (s_forge_tag_opt.snap_cam_zoom < 0.5f) s_forge_tag_opt.snap_cam_zoom = 0.5f;
		}

		if (cfg && cfg->point_count > 0)
		{
			if (s_forge_tag_opt.snap_active_pt_idx < 0)
				s_forge_tag_opt.snap_active_pt_idx = 0;
			if (s_forge_tag_opt.snap_active_pt_idx >= cfg->point_count)
				s_forge_tag_opt.snap_active_pt_idx = cfg->point_count - 1;
			pt = &cfg->points[s_forge_tag_opt.snap_active_pt_idx];
		}

		for (r = 0; r < 10; r++)
		{
			struct forge_widget_instance *child = rows[r];
			char row_buf[80];
			if (!child)
				continue;
			child->visible = TRUE;
			if (fw_named(child, "scroll_up_button", 0))
				fw_named(child, "scroll_up_button", 0)->visible = FALSE;
			if (fw_named(child, "scroll_down_button", 0))
				fw_named(child, "scroll_down_button", 0)->visible = FALSE;
			if (fw_named(child, "list_item_arrows", 0))
				fw_named(child, "list_item_arrows", 0)->visible = (r == s_forge_tag_opt.snap_editor_row);

			switch (r)
			{
			case 0:
				snprintf(row_buf, sizeof(row_buf), "AUTO-GEN: <%s>",
					s_snap_autogen_names[s_forge_tag_opt.snap_autogen_mode & 3]);
				break;
			case 1:
				snprintf(row_buf, sizeof(row_buf), "[+] ADD SNAP POINT (%d/%d)",
					cfg ? cfg->point_count : 0, FORGE_MAX_SNAP_POINTS_PER_TAG);
				break;
			case 2:
				if (cfg && cfg->point_count > 0)
					snprintf(row_buf, sizeof(row_buf), "ACTIVE PT: <#%d / %d>",
						s_forge_tag_opt.snap_active_pt_idx + 1, cfg->point_count);
				else
					snprintf(row_buf, sizeof(row_buf), "ACTIVE PT: <NONE>");
				break;
			case 3:
				if (pt)
				{
					int pr = (pt->preset_id < NUMBER_OF_FORGE_SNAP_PRESETS) ? (int)pt->preset_id : 0;
					snprintf(row_buf, sizeof(row_buf), "PRESET: <%s>", s_snap_preset_names[pr]);
				}
				else
				{
					snprintf(row_buf, sizeof(row_buf), "PRESET: <->");
				}
				break;
			case 4:
				if (pt)
					snprintf(row_buf, sizeof(row_buf), "OFFSET X (FWD): <%+.2f>", (double)pt->x);
				else
					snprintf(row_buf, sizeof(row_buf), "OFFSET X (FWD): <->");
				break;
			case 5:
				if (pt)
					snprintf(row_buf, sizeof(row_buf), "OFFSET Y (SIDE): <%+.2f>", (double)pt->y);
				else
					snprintf(row_buf, sizeof(row_buf), "OFFSET Y (SIDE): <->");
				break;
			case 6:
				if (pt)
					snprintf(row_buf, sizeof(row_buf), "OFFSET Z (VERT): <%+.2f>", (double)pt->z);
				else
					snprintf(row_buf, sizeof(row_buf), "OFFSET Z (VERT): <->");
				break;
			case 7:
				if (pt)
				{
					int nd = (pt->normal_dir < NUMBER_OF_FORGE_SNAP_DIRS) ? (int)pt->normal_dir : 0;
					snprintf(row_buf, sizeof(row_buf), "NORMAL: <%s>", s_snap_dir_names[nd]);
				}
				else
				{
					snprintf(row_buf, sizeof(row_buf), "NORMAL: <->");
				}
				break;
			case 8:
				snprintf(row_buf, sizeof(row_buf), "[-] DELETE POINT (SHIFT=ALL)");
				break;
			case 9:
			default:
				snprintf(row_buf, sizeof(row_buf), "[SAVE & RETURN TO TAG LIST]");
				break;
			}
			fw_text_set_ascii(fw_named(child, "list_item_text", 0), row_buf);
		}

		if (desc && te)
		{
			struct forge_widget_instance *r_name = fw_named(desc, "forge_tag_right_name", 0);
			struct forge_widget_instance *r_data = fw_named(desc, "forge_tag_right_data", 0);
			float bmin[3], bmax[3];
			char wrapped_title[64];
			char info_buf[256];

			forge_tag_preview_set_installed_tag(te->group_tag, te->tag_path, te->is_custom);
			forge_snap_points_set_editor_overlay(
				1,
				te->group_tag,
				te->tag_path,
				s_forge_tag_opt.snap_active_pt_idx,
				s_forge_tag_opt.snap_cam_yaw_deg,
				s_forge_tag_opt.snap_cam_pitch_deg,
				s_forge_tag_opt.snap_cam_zoom);
			forge_snap_points_get_preview_bounds(bmin, bmax);

			forge_wrap_title_two_lines(te->short_name, wrapped_title, sizeof(wrapped_title));
			if (pt)
			{
				int nd = (pt->normal_dir < NUMBER_OF_FORGE_SNAP_DIRS) ? (int)pt->normal_dir : 0;
				snprintf(info_buf, sizeof(info_buf),
					"- Pt #%d of %d\r\n"
					"- Pos: %.2f, %.2f, %.2f\r\n"
					"- Dir: %s\r\n"
					"- Box: %.1fx%.1fx%.1f\r\n"
					"- [Q/E] Orbit [+/-] Zoom\r\n"
					"- [L/R] Adjust [B] Done",
					s_forge_tag_opt.snap_active_pt_idx + 1,
					cfg ? cfg->point_count : 0,
					(double)pt->x, (double)pt->y, (double)pt->z,
					s_snap_dir_names[nd],
					(double)(bmax[0] - bmin[0]),
					(double)(bmax[1] - bmin[1]),
					(double)(bmax[2] - bmin[2]));
			}
			else
			{
				snprintf(info_buf, sizeof(info_buf),
					"- No Snap Points\r\n"
					"- Box: %.1fx%.1fx%.1f\r\n"
					"- Press A on AUTO-GEN\r\n"
					"  or ADD SNAP POINT\r\n"
					"- [Q/E] Orbit [+/-] Zoom\r\n"
					"- [B/ESC] Save & Back",
					(double)(bmax[0] - bmin[0]),
					(double)(bmax[1] - bmin[1]),
					(double)(bmax[2] - bmin[2]));
			}
			fw_text_set_ascii(r_name, wrapped_title);
			fw_text_set_ascii(r_data, info_buf);
		}
		return;
	}

	/* Browse Mode: Tag List with 4-Category Spinner (SCENERY | DOORS | CUSTOM SCENERY | CUSTOM DOORS) */
	if (cat_row)
		cat_row->visible = TRUE;

	forge_handle_10row_edge_scroll(
		list, rows, focused_r,
		&s_forge_tag_opt.snap_prev_row_focus,
		&s_forge_tag_opt.snap_first,
		&s_forge_tag_opt.snap_chosen,
		s_forge_tag_opt.snap_count);

	forge_populate_10row_tag_list(
		rows,
		s_forge_tag_opt.snap_tags,
		s_forge_tag_opt.snap_count,
		s_forge_tag_opt.snap_first,
		s_forge_tag_opt.snap_chosen,
		"(No Tags In Category)");

	if (desc)
	{
		struct forge_widget_instance *r_name = fw_named(desc, "forge_tag_right_name", 0);
		struct forge_widget_instance *r_data = fw_named(desc, "forge_tag_right_data", 0);
		if (s_forge_tag_opt.snap_count > 0 &&
			s_forge_tag_opt.snap_chosen >= 0 &&
			s_forge_tag_opt.snap_chosen < s_forge_tag_opt.snap_count)
		{
			struct forge_tag_opt_tag_entry *te = &s_forge_tag_opt.snap_tags[s_forge_tag_opt.snap_chosen];
			float bmin[3], bmax[3];
			int pt_cnt;
			char wrapped_title[64];
			char type_str[48];
			char info_buf[240];

			forge_tag_preview_set_installed_tag(te->group_tag, te->tag_path, te->is_custom);
			forge_snap_points_set_editor_overlay(0, te->group_tag, te->tag_path, -1, 0.0f, 0.0f, 1.0f);
			forge_snap_points_get_preview_bounds(bmin, bmax);
			pt_cnt = forge_snap_points_count_for_tag(te->group_tag, te->tag_path);

			forge_wrap_title_two_lines(te->short_name, wrapped_title, sizeof(wrapped_title));
			forge_format_tag_type_label(te->group_tag, type_str, sizeof(type_str));
			snprintf(info_buf, sizeof(info_buf),
				"- %s\r\n"
				"- Snap Points: %d\r\n"
				"- Size: %.1fx%.1fx%.1f\r\n"
				"- [A/CHOOSE] Edit 3D\r\n"
				"  Snap Points",
				type_str,
				pt_cnt,
				(double)(bmax[0] - bmin[0]),
				(double)(bmax[1] - bmin[1]),
				(double)(bmax[2] - bmin[2]));
			fw_text_set_ascii(r_name, wrapped_title);
			fw_text_set_ascii(r_data, info_buf);
		}
		else
		{
			forge_tag_preview_clear();
			fw_text_set_ascii(r_name, "NO TAG");
			fw_text_set_ascii(r_data, "- None\r\n- Snap Points: 0");
		}
	}
}

static unsigned long s_forge_name_modal_active_ms = 0;

static void update_forge_name_tag_modal(struct forge_widget_instance *screen)
{
	struct forge_widget_instance *tb = fw_named(screen, "forge_name_tag_text_box", 0);
	struct forge_widget_instance *input_btn = fw_named(screen, "forge_name_tag_input_btn", 0);
	int vk, shift_down;
	char input_buf[128];

	s_forge_name_modal_active_ms = system_milliseconds();
	shift_down = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
	for (vk = 1; vk < 255; vk++)
	{
		int down = (GetAsyncKeyState(vk) & 0x8000) != 0;
		if (down && !s_forge_tag_opt.prev_key_down[vk])
		{
			char ch = 0;
			if (vk == VK_BACK)
			{
				if (s_forge_tag_opt.custom_name_len > 0)
					s_forge_tag_opt.custom_name[--s_forge_tag_opt.custom_name_len] = '\0';
			}
			else if (vk >= 'A' && vk <= 'Z')
			{
				ch = shift_down ? (char)vk : (char)(vk - 'A' + 'a');
			}
			else if (vk >= '0' && vk <= '9')
			{
				ch = (char)vk;
			}
			else if (vk == VK_SPACE)
			{
				ch = ' ';
			}
			else if (vk == 0xBD /* VK_OEM_MINUS */ || vk == 0x6D /* VK_SUBTRACT */)
			{
				ch = shift_down ? '_' : '-';
			}
			if (ch != 0 && s_forge_tag_opt.custom_name_len < 30)
			{
				s_forge_tag_opt.custom_name[s_forge_tag_opt.custom_name_len++] = ch;
				s_forge_tag_opt.custom_name[s_forge_tag_opt.custom_name_len] = '\0';
			}
		}
		s_forge_tag_opt.prev_key_down[vk] = down;
	}

	if (input_btn)
	{
		fw_text_set_ascii(tb, "Enter Forge menu name for tag:");
		snprintf(input_buf, sizeof(input_buf), "[C] %s_", s_forge_tag_opt.custom_name);
		fw_text_set_ascii(input_btn, input_buf);
		input_btn->animation.current_frame_index = 1;
	}
	else
	{
		snprintf(input_buf, sizeof(input_buf),
			"Enter Forge menu name for tag:\r\n\r\n[C] %s_",
			s_forge_tag_opt.custom_name);
		fw_text_set_ascii(tb, input_buf);
	}
}

static void update_forge_installed_tag_action_modal(struct forge_widget_instance *screen)
{
	struct forge_widget_instance *tb = fw_named(screen, "forge_installed_tag_action_text_box", 0);
	char prompt_buf[240];

	if (s_forge_tag_opt.inst_count > 0 &&
		s_forge_tag_opt.inst_chosen >= 0 &&
		s_forge_tag_opt.inst_chosen < s_forge_tag_opt.inst_count)
	{
		struct forge_tag_opt_tag_entry const *te = &s_forge_tag_opt.inst_tags[s_forge_tag_opt.inst_chosen];
		char type_str[48];
		char size_str[32];
		forge_format_tag_type_label(te->group_tag, type_str, sizeof(type_str));
		forge_format_size_bytes(te->size_bytes, size_str, sizeof(size_str));
		snprintf(prompt_buf, sizeof(prompt_buf),
			"- %s\r\n- %s\r\n- %s\r\n\r\nDelete this tag from the cache?",
			te->display_row, type_str, size_str);
	}
	else
	{
		snprintf(prompt_buf, sizeof(prompt_buf), "Delete this tag from the cache?");
	}
	fw_text_set_ascii(tb, prompt_buf);
}


boolean forge_player_profile_get(long player_profile_index, struct player_profile *profile)
{
	boolean ok = player_profile_get(player_profile_index, profile);
	if (ok && profile && g_forge_session_enabled)
	{
		int lv;
		for (lv = 0; lv < NUMBER_OF_SINGLE_PLAYER_LEVELS; lv++)
			profile->single_player_map_flags[lv] |= 1;
	}
	return ok;
}

boolean pc_menu_event_function_invoke(
	void *widget_ptr,
	void *event_ptr,
	long function_index,
	boolean *widget_deleted)
{
	struct forge_widget_instance *widget = (struct forge_widget_instance *)widget_ptr;
	char const *fn_name = pc_menu_function_name(function_index);
	char const *w_name = (widget && widget->name) ? widget->name : "";
	char const *p_name = (widget && widget->parent && widget->parent->name) ? widget->parent->name : "";

	/* Scope Forge session strictly to Multiplayer -> Forge flows */
	if ((fn_name && strcmp(fn_name, "gamemode select forge") == 0) ||
		(fn_name && strcmp(fn_name, "gamemode select choose") == 0 && strcmp(w_name, "gm_forge_item") == 0) ||
		strncmp(w_name, "forge_", 6) == 0 ||
		strncmp(p_name, "forge_", 6) == 0)
	{
		g_forge_session_enabled = 1;
		forge_runtime_init();
	}
	else if (fn_name &&
		((strcmp(fn_name, "gamemode select choose") == 0 && strcmp(w_name, "gm_forge_item") != 0) ||
		 strcmp(fn_name, "gamemode select standard") == 0 ||
		 strcmp(fn_name, "gamemode select ranked") == 0 ||
		 strcmp(fn_name, "gamemode select firefight") == 0 ||
		 strcmp(fn_name, "new campaign decision") == 0 ||
		 strcmp(fn_name, "campaign menu init") == 0 ||
		 strcmp(fn_name, "port coop begin") == 0 ||
		 strcmp(fn_name, "main menu initialize") == 0))
	{
		g_forge_session_enabled = 0;
		s_forge_active_map_kind = 1;
	}

	/* Forge -> Edit Maps screen initialization & activation */
	if (fn_name && strcmp(fn_name, "load game menu init") == 0 &&
		strcmp(w_name, "forge_edit_maps_list") == 0)
	{
		g_forge_session_enabled = 1;
		dispose_global_network_game_client();
		dispose_global_network_game_server();
		network_game_accept_remote_connections(FALSE);
		custom_edition_maps_look_again();
		scan_forge_edit_maps(0);
		if (fw_named(widget, "list_item_1", 0))
			widget->focused_child = fw_named(widget, "list_item_1", 0);
		return TRUE;
	}
	if (fn_name && strcmp(fn_name, "load game menu init") == 0 &&
		strcmp(w_name, "forge_add_tags_map_list") == 0)
	{
		struct forge_widget_instance *sp = fw_named(widget, "forge_add_tags_map_kind_spinner", 0);
		int init_kind = 1;
		g_forge_session_enabled = 1;
		forge_tag_preview_clear();
		custom_edition_maps_look_again();
		if (custom_edition_maps_count(FALSE) == 0 && custom_edition_maps_count(TRUE) > 0)
			init_kind = 0;
		if (sp)
			sp->parameters.list.selected_index = (short)init_kind;
		forge_scan_add_tags_maps_by_kind(init_kind);
		if (fw_named(widget, "list_item_1", 0))
			widget->focused_child = fw_named(widget, "list_item_1", 0);
		return TRUE;
	}
	if (fn_name && strcmp(fn_name, "load game menu init") == 0 &&
		strcmp(w_name, "forge_add_tags_browser_list") == 0)
	{
		struct forge_widget_instance *sp = fw_named(widget, "forge_tag_cat_spinner", 0);
		g_forge_session_enabled = 1;
		forge_tag_preview_clear();
		if (sp)
			sp->parameters.list.selected_index = 0; /* Default to SCENERY */
		forge_scan_donor_map_tags_by_cat(0);
		if (fw_named(widget, "list_item_1", 0))
			widget->focused_child = fw_named(widget, "list_item_1", 0);
		return TRUE;
	}
	if (fn_name && strcmp(fn_name, "load game menu init") == 0 &&
		strcmp(w_name, "forge_installed_tags_list") == 0)
	{
		struct forge_widget_instance *sp = fw_named(widget, "forge_tag_cat_spinner", 0);
		g_forge_session_enabled = 1;
		forge_tag_preview_clear();
		if (sp)
			sp->parameters.list.selected_index = 0; /* Default to SCENERY */
		s_forge_tag_opt.inst_first = 0;
		s_forge_tag_opt.inst_chosen = 0;
		s_forge_tag_opt.inst_prev_row_focus = -1;
		forge_scan_installed_tags_by_cat(0);
		if (fw_named(widget, "list_item_1", 0))
			widget->focused_child = fw_named(widget, "list_item_1", 0);
		return TRUE;
	}
	if (fn_name && strcmp(fn_name, "load game menu init") == 0 &&
		strcmp(w_name, "forge_snap_points_list") == 0)
	{
		struct forge_widget_instance *sp = fw_named(widget, "forge_snap_cat_spinner", 0);
		struct forge_widget_instance *cat_row = fw_named(widget, "forge_snap_cat_row", 0);
		g_forge_session_enabled = 1;
		forge_tag_preview_clear();
		s_forge_tag_opt.snap_editor_active = 0;
		s_forge_tag_opt.snap_editor_row = 0;
		s_forge_tag_opt.snap_active_pt_idx = 0;
		s_forge_tag_opt.snap_cam_yaw_deg = 38.0f;
		s_forge_tag_opt.snap_cam_pitch_deg = 22.0f;
		s_forge_tag_opt.snap_cam_zoom = 1.0f;
		s_forge_tag_opt.snap_first = 0;
		s_forge_tag_opt.snap_chosen = 0;
		s_forge_tag_opt.snap_prev_row_focus = -1;
		if (cat_row)
			cat_row->visible = TRUE;
		if (sp)
			sp->parameters.list.selected_index = 0; /* Default to SCENERY */
		forge_scan_snap_tags_by_cat(0);
		if (fw_named(widget, "list_item_1", 0))
			widget->focused_child = fw_named(widget, "list_item_1", 0);
		return TRUE;
	}

	if (fn_name && strcmp(fn_name, "port map list back") == 0)
	{
		struct forge_widget_instance *anc;
		for (anc = widget; anc; anc = anc->parent)
		{
			if (anc->name &&
				(strcmp(anc->name, "forge_snap_points_list") == 0 ||
				 strcmp(anc->name, "forge_snap_points_screen") == 0))
			{
				struct forge_widget_instance *list = fw_named(anc, "forge_snap_points_list", 0);
				if (!list && strcmp(anc->name, "forge_snap_points_list") == 0)
					list = anc;
				if (s_forge_tag_opt.snap_editor_active)
				{
					struct forge_widget_instance *cat_row = list ? fw_named(list, "forge_snap_cat_row", 0) : NULL;
					struct forge_widget_instance *rows[10] = { NULL };
					int vis_r;
					forge_snap_points_save_all();
					if (s_forge_tag_opt.snap_count > 0 &&
						s_forge_tag_opt.snap_chosen >= 0 &&
						s_forge_tag_opt.snap_chosen < s_forge_tag_opt.snap_count)
					{
						struct forge_tag_opt_tag_entry *te = &s_forge_tag_opt.snap_tags[s_forge_tag_opt.snap_chosen];
						forge_refresh_snap_tag_row_label(te);
						forge_snap_points_set_editor_overlay(0, te->group_tag, te->tag_path, -1, 0.0f, 0.0f, 1.0f);
					}
					s_forge_tag_opt.snap_editor_active = 0;
					if (cat_row)
						cat_row->visible = TRUE;
					if (list)
					{
						forge_FocusRowIndexOfList(list, rows);
						vis_r = s_forge_tag_opt.snap_chosen - s_forge_tag_opt.snap_first;
						if (vis_r < 0) vis_r = 0;
						if (vis_r > 9) vis_r = 9;
						if (rows[vis_r])
							list->focused_child = rows[vis_r];
					}
					ui_play_audio_feedback_sound(3);
					return TRUE;
				}
				forge_snap_points_set_editor_overlay(0, 0, NULL, -1, 0.0f, 0.0f, 1.0f);
				forge_tag_preview_clear();
				ui_play_audio_feedback_sound(3);
				ui_widget_port_go_back(widget);
				if (widget_deleted)
					*widget_deleted = TRUE;
				return TRUE;
			}
		}
	}

	if (fn_name && strcmp(fn_name, "load game menu activated") == 0)
	{
		struct forge_widget_instance *anc;
		for (anc = widget; anc; anc = anc->parent)
		{
			if (!anc->name)
				continue;
			if (strcmp(anc->name, "forge_edit_maps_list") == 0)
			{
				if (s_forge_menu.count > 0 && s_forge_menu.chosen >= 0 && s_forge_menu.chosen < s_forge_menu.count)
				{
					char level_path[128];
					g_forge_session_enabled = 1;
					dispose_global_network_game_client();
					dispose_global_network_game_server();
					network_game_accept_remote_connections(FALSE);
					custom_edition_maps_look_again();
					snprintf(level_path, sizeof(level_path), "custom_maps\\%s", s_forge_menu.map_leaves[s_forge_menu.chosen]);
					player_ui_set_single_player_local_player_controller(0, 0);
					main_set_map_name(level_path);
					main_set_difficulty(1);
					game_connection_set(0);
					main_menu_switch_to_single_player();
					player_ui_remember_player1_profile(TRUE);
					ui_play_audio_feedback_sound(2);
					return TRUE;
				}
				ui_play_audio_feedback_sound(4);
				return FALSE;
			}
			if (strcmp(anc->name, "forge_add_tags_map_list") == 0)
			{
				if (s_forge_tag_opt.map_count > 0 &&
					s_forge_tag_opt.map_chosen >= 0 &&
					s_forge_tag_opt.map_chosen < s_forge_tag_opt.map_count)
				{
					snprintf(
						s_forge_tag_opt.active_donor_map_path,
						sizeof(s_forge_tag_opt.active_donor_map_path),
						"%s",
						s_forge_tag_opt.maps[s_forge_tag_opt.map_chosen].file_path);
					s_forge_tag_opt.browser_cat = -1;
					ui_play_audio_feedback_sound(2);
					ui_widget_port_open(
						widget_ptr,
						"pc\\main_menu\\forge_select\\forge_add_tags_browser_screen",
						widget_deleted);
					return TRUE;
				}
				ui_play_audio_feedback_sound(4);
				return FALSE;
			}
			if (strcmp(anc->name, "forge_add_tags_browser_list") == 0)
			{
				if (s_forge_tag_opt.browser_count > 0 &&
					s_forge_tag_opt.browser_chosen >= 0 &&
					s_forge_tag_opt.browser_chosen < s_forge_tag_opt.browser_count)
				{
					int vk;
					snprintf(
						s_forge_tag_opt.custom_name,
						sizeof(s_forge_tag_opt.custom_name),
						"%.30s",
						s_forge_tag_opt.browser_tags[s_forge_tag_opt.browser_chosen].short_name);
					s_forge_tag_opt.custom_name_len = (int)strlen(s_forge_tag_opt.custom_name);
					s_forge_tag_opt.name_open_ms = system_milliseconds();
					for (vk = 1; vk < 255; vk++)
						s_forge_tag_opt.prev_key_down[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
					ui_play_audio_feedback_sound(2);
					ui_widget_port_open(
						widget_ptr,
						"pc\\main_menu\\forge_select\\forge_name_tag_screen",
						widget_deleted);
					return TRUE;
				}
				ui_play_audio_feedback_sound(4);
				return FALSE;
			}
			if (strcmp(anc->name, "forge_installed_tags_list") == 0)
			{
				if (s_forge_tag_opt.inst_count > 0 &&
					s_forge_tag_opt.inst_chosen >= 0 &&
					s_forge_tag_opt.inst_chosen < s_forge_tag_opt.inst_count)
				{
					ui_play_audio_feedback_sound(2);
					ui_widget_port_open(
						widget_ptr,
						"pc\\main_menu\\forge_select\\forge_installed_tag_action_screen",
						widget_deleted);
					return TRUE;
				}
				ui_play_audio_feedback_sound(4);
				return FALSE;
			}
			if (strcmp(anc->name, "forge_snap_points_list") == 0)
			{
				if (s_forge_tag_opt.snap_editor_active)
				{
					struct forge_widget_instance *rows[10] = { NULL };
					int f_row = forge_FocusRowIndexOfList(anc, rows);
					if (f_row < 0)
						f_row = s_forge_tag_opt.snap_editor_row;
					if (f_row >= 0 && f_row < 10)
						forge_snap_editor_activate_row(anc, f_row);
					return TRUE;
				}
				if (s_forge_tag_opt.snap_count > 0 &&
					s_forge_tag_opt.snap_chosen >= 0 &&
					s_forge_tag_opt.snap_chosen < s_forge_tag_opt.snap_count)
				{
					struct forge_tag_opt_tag_entry const *te = &s_forge_tag_opt.snap_tags[s_forge_tag_opt.snap_chosen];
					struct forge_widget_instance *cat_row = fw_named(anc, "forge_snap_cat_row", 0);
					forge_tag_preview_set_installed_tag(te->group_tag, te->tag_path, te->is_custom);
					s_forge_tag_opt.snap_editor_active = 1;
					s_forge_tag_opt.snap_editor_row = 0;
					s_forge_tag_opt.snap_active_pt_idx = 0;
					s_forge_tag_opt.snap_prev_left = 1;
					s_forge_tag_opt.snap_prev_right = 1;
					s_forge_tag_opt.snap_prev_del = 1;
					if (cat_row)
						cat_row->visible = FALSE;
					if (fw_named(anc, "list_item_1", 0))
						anc->focused_child = fw_named(anc, "list_item_1", 0);
					ui_play_audio_feedback_sound(2);
					return TRUE;
				}
				ui_play_audio_feedback_sound(4);
				return FALSE;
			}
		}
	}

	/* Settings & Forge Tag Options modal buttons: DELETE / RESET STOCK / RESET CUSTOM */
	if (fn_name && strcmp(fn_name, "port mods close") == 0)
	{
		if (w_name && strcmp(w_name, "forge_installed_tag_btn_delete") == 0)
		{
			if (s_forge_tag_opt.inst_count > 0 &&
				s_forge_tag_opt.inst_chosen >= 0 &&
				s_forge_tag_opt.inst_chosen < s_forge_tag_opt.inst_count)
			{
				struct forge_tag_opt_tag_entry const *te = &s_forge_tag_opt.inst_tags[s_forge_tag_opt.inst_chosen];
				forge_delete_installed_tag_entry(te->group_tag, te->tag_path, te->is_custom);
				forge_scan_installed_tags_by_cat(s_forge_tag_opt.inst_cat >= 0 ? s_forge_tag_opt.inst_cat : 0);
				ui_play_audio_feedback_sound(2);
				return TRUE;
			}
			return FALSE;
		}
		if (w_name && strcmp(w_name, "forge_reset_stock_btn_confirm") == 0)
		{
			forge_reset_stock_tags_only();
			s_forge_tag_opt.inst_cat = -1;
			ui_play_audio_feedback_sound(2);
			return TRUE;
		}
		if (w_name && strcmp(w_name, "forge_reset_custom_btn_confirm") == 0)
		{
			forge_reset_custom_tags_only();
			s_forge_tag_opt.inst_cat = -1;
			ui_play_audio_feedback_sound(2);
			return TRUE;
		}
	}

	/* Forge -> Custom Tag Name Modal & SYNC CUSTOM TAGS */
	if (fn_name && strcmp(fn_name, "port mods init") == 0 &&
		(strcmp(w_name, "forge_name_tag_screen") == 0 || strcmp(w_name, "forge_installed_tag_action_screen") == 0))
	{
		int vk;
		for (vk = 1; vk < 255; vk++)
			s_forge_tag_opt.prev_key_down[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
		return TRUE;
	}
	if (fn_name && strcmp(fn_name, "port mods save") == 0 &&
		strcmp(w_name, "forge_name_tag_btn_confirm") == 0)
	{
		if (s_forge_tag_opt.browser_count > 0 &&
			s_forge_tag_opt.browser_chosen >= 0 &&
			s_forge_tag_opt.browser_chosen < s_forge_tag_opt.browser_count)
		{
			struct forge_tag_opt_tag_entry const *te = &s_forge_tag_opt.browser_tags[s_forge_tag_opt.browser_chosen];
			if (forge_extract_tag_from_donor_map_to_ocetags(
					s_forge_tag_opt.active_donor_map_path,
					te->group_tag,
					te->tag_path,
					s_forge_tag_opt.custom_name[0] ? s_forge_tag_opt.custom_name : te->short_name))
			{
				s_forge_tag_opt.inst_cat = -1;
				ui_play_audio_feedback_sound(2);
				return TRUE;
			}
		}
		ui_play_audio_feedback_sound(4);
		return FALSE;
	}

	/* Refresh Custom Edition & saved Forge maps on Single Player / Multiplayer level list init */
	if (fn_name && strcmp(fn_name, "initialize sp level list solo") == 0)
	{
		boolean res;
		struct forge_widget_instance *spinner;
		s_forge_active_map_kind = 1;
		s_forge_last_map_kind = -1;
		if (g_forge_session_enabled)
		{
			dispose_global_network_game_client();
			dispose_global_network_game_server();
			network_game_accept_remote_connections(FALSE);
		}
		res = orig_pc_menu_event_function_invoke(widget_ptr, event_ptr, function_index, widget_deleted);
		if (g_forge_session_enabled)
		{
			spinner = fw_named(widget, "list_item_0_map_kind_spinner", 0);
			if (spinner)
				spinner->parameters.list.selected_index = 1;
		}
		return res;
	}
	if (fn_name && strcmp(fn_name, "mp level list initialize") == 0)
	{
		boolean res;
		struct forge_widget_instance *spinner;
		s_forge_active_map_kind = 1;
		s_forge_last_map_kind = -1;
		res = orig_pc_menu_event_function_invoke(widget_ptr, event_ptr, function_index, widget_deleted);
		spinner = fw_named(widget, "list_item_0_map_kind_spinner", 0);
		if (spinner)
			spinner->parameters.list.selected_index = 1;
		return res;
	}

	if (fn_name && strcmp(fn_name, "solo level set map") == 0)
	{
		boolean res;
		struct forge_widget_instance *spinner = fw_named(widget, "list_item_0_map_kind_spinner", 0);
		if (!g_forge_session_enabled)
		{
			s_forge_active_map_kind = 1;
			return orig_pc_menu_event_function_invoke(widget_ptr, event_ptr, function_index, widget_deleted);
		}
		if (spinner && s_forge_last_map_kind == 4)
			spinner->parameters.list.selected_index = 3;
		s_forge_active_map_kind = s_forge_last_map_kind;
		res = orig_pc_menu_event_function_invoke(widget_ptr, event_ptr, function_index, widget_deleted);
		if (spinner && s_forge_last_map_kind == 4)
			spinner->parameters.list.selected_index = 4;
		if (g_forge_session_enabled && res)
		{
			char cur_map_copy[128];
			char const *cur = main_get_map_name();
			snprintf(cur_map_copy, sizeof(cur_map_copy), "%s", (cur && cur[0]) ? cur : "levels\\test\\bloodgulch\\bloodgulch");
			dispose_global_network_game_client();
			dispose_global_network_game_server();
			network_game_accept_remote_connections(FALSE);
			player_ui_set_single_player_local_player_controller(0, 0);
			main_set_map_name(cur_map_copy);
			main_set_difficulty(1);
			game_connection_set(0);
			main_menu_switch_to_single_player();
			player_ui_remember_player1_profile(TRUE);
			ui_play_audio_feedback_sound(2);
			return FALSE; /* Returning FALSE prevents opening difficulty_select_list_screen */
		}
		return res;
	}

	/* Point 7: Multiplayer Map Select (FORGE MAPS tab works in Standard/Ranked MP & Forge; Forge skips Gametype Select) */
	if (fn_name && strcmp(fn_name, "mp level select") == 0)
	{
		boolean res;
		struct forge_widget_instance *spinner = fw_named(widget, "list_item_0_map_kind_spinner", 0);
		if (spinner && s_forge_last_map_kind == 4)
			spinner->parameters.list.selected_index = 3;
		s_forge_active_map_kind = s_forge_last_map_kind;
		res = orig_pc_menu_event_function_invoke(widget_ptr, event_ptr, function_index, widget_deleted);
		if (spinner && s_forge_last_map_kind == 4)
			spinner->parameters.list.selected_index = 4;
		if (g_forge_session_enabled && res)
		{
			long gtypes[64];
			short last_gt = 0;
			struct game_variant forge_var;
			struct game_variant_options forge_opts;

			if (ui_widget_port_gametypes(gtypes, 64, &last_gt) > 0)
				ui_widget_port_gametype_choose(gtypes[0]);

			memset(&forge_var, 0, sizeof(forge_var));
			build_game_variant_slayer(&forge_var);
			ustrncpy(
				forge_var.human_readable_game_description,
				L"Forge",
				NUMBEROF(forge_var.human_readable_game_description) - 1);
			forge_var.universal_variant.teams = FALSE;
			forge_var.universal_variant.score_to_win = 0x3FFFFFFF; /* Endless Forge session: never ends on kills */
			forge_var.universal_variant.respawn_time = 90;
			forge_var.universal_variant.suicide_penalty = 0;

			game_variant_options_default(&forge_var, &forge_opts);
			forge_opts.time_limit = 0; /* No time limit */
			forge_opts.vehicle_set[0] = VARIANT_VEHICLE_SET_PC; /* All map vehicles enabled */
			forge_opts.vehicle_set[1] = VARIANT_VEHICLE_SET_PC;
			ui_widget_port_game_variant_set(&forge_var, &forge_opts);

			ui_widget_port_open(
				widget_ptr,
				"pc\\main_menu\\multiplayer_type_select\\server_settings\\server_settings_screen",
				widget_deleted);
			return FALSE; /* Returning FALSE prevents XML handler from opening gametype_select_screen_wrapper */
		}
		return res;
	}

	/* In-Game Escape Pause Menu -> "SAVE MAP.." button handler
	 * Note: ui_widget_event_handler_function_invoke subtracts PC_MENU_FUNCTION_BASE (0x100 = 256) before calling pc_menu_event_function_invoke */
	if (function_index == 0x7777 ||
		function_index == (0x7777 - 256) ||
		function_index == (0x7777 - 102) ||
		(widget && widget->definition_tag_index == s_save_forge_btn_tag && s_save_forge_btn_tag != NONE) ||
		(w_name && strcmp(w_name, "save_forge_button") == 0))
	{
		ui_widgets_close_all();
		if (widget_deleted)
			*widget_deleted = TRUE;
		ui_play_audio_feedback_sound(2);
		forge_trigger_save_map_action();
		return TRUE;
	}

	/* Settings -> Controls Setup ("FORGE" and "FORGE PAD" tabs) */
	if (fn_name &&
		(strcmp(fn_name, "controls screen init") == 0 ||
		 strcmp(fn_name, "controls screen defaults") == 0 ||
		 strcmp(fn_name, "controls screen change set") == 0 ||
		 strcmp(fn_name, "controls binding slot") == 0 ||
		 strcmp(fn_name, "controls begin binding") == 0))
	{
		extern boolean forge_controls_menu_event(struct forge_widget_instance *w, void *event_ptr, char const *fn_name, long function_index, boolean *widget_deleted);
		if (forge_controls_menu_event(widget, event_ptr, fn_name, function_index, widget_deleted))
			return TRUE;
	}

	return orig_pc_menu_event_function_invoke(widget_ptr, event_ptr, function_index, widget_deleted);
}

/* ---------- Settings -> Controls Setup ("FORGE" & "FORGE PAD" Tabs) Implementation */

static struct
{
	int initialized;
	int32_t pending_vk[FORGE_BIND_COUNT];
	int32_t pending_gp[FORGE_BIND_COUNT];
	float pending_look_sens;
	float pending_fly_speed;
	int slot; /* 0 = Keyboard/Mouse, 1 = Controller */
	int capturing_action; /* -1 when idle, 0..FORGE_BIND_COUNT-1 when waiting for input */
	int capturing_slot;
	unsigned long capture_start_ms;
	int prev_vk_down[256];
	int prev_gp_down[FORGE_GP_COUNT];
} s_forge_controls_ui = { 0, { 0 }, { 0 }, 1.0f, 1.0f, 0, -1, 0, 0, { 0 }, { 0 } };

static char const *const s_forge_bind_labels[FORGE_BIND_COUNT] = {
	"FORGE / SPARTAN",
	"OBJECT PALETTE",
	"3RD-PERSON VIEW",
	"QUICK SAVE MAP",
	"ROTATE ITEM",
	"SNAP TO FLOOR",
	"SNAP TO SURFACE",
	"ROTATION SNAP",
	"OBJECT OPTIONS",
	"DELETE OBJECT",
	"COPY OBJECT"
};

static struct forge_widget_instance *fw_screen_of(struct forge_widget_instance *w)
{
	while (w && w->parent)
		w = w->parent;
	return w;
}

static struct forge_widget_instance *fw_control_row(struct forge_widget_instance *w)
{
	for (; w; w = w->parent)
	{
		if (w->name && strncmp(w->name, "op_command_", 11) == 0)
			return w;
	}
	return NULL;
}

static int fw_controls_group(struct forge_widget_instance *w)
{
	struct forge_widget_instance *sp = fw_named(fw_screen_of(w), "group_spinner", 0);
	return sp ? (int)sp->parameters.list.selected_index : 0;
}

static void forge_controls_ui_ensure_loaded(void)
{
	int i;
	if (g_forge.bind_vk[0] == 0 && g_forge.bind_gp[0] == 0)
	{
		forge_controls_init_defaults();
		forge_controls_load();
	}
	if (!s_forge_controls_ui.initialized)
	{
		for (i = 0; i < FORGE_BIND_COUNT; i++)
		{
			s_forge_controls_ui.pending_vk[i] = g_forge.bind_vk[i];
			s_forge_controls_ui.pending_gp[i] = g_forge.bind_gp[i];
		}
		s_forge_controls_ui.pending_look_sens = (g_forge.controller_look_sens >= 0.25f) ? g_forge.controller_look_sens : 1.0f;
		s_forge_controls_ui.pending_fly_speed = (g_forge.controller_fly_speed >= 0.25f) ? g_forge.controller_fly_speed : 1.0f;
		s_forge_controls_ui.capturing_action = -1;
		s_forge_controls_ui.initialized = 1;
	}
}

static void forge_controls_snapshot_input(void)
{
	int vk, gp;
	float lx, ly, rx, ry, lt, rt;
	forge_poll_controller_state(&lx, &ly, &rx, &ry, &lt, &rt);
	for (vk = 1; vk < 255; vk++)
		s_forge_controls_ui.prev_vk_down[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
	for (gp = 1; gp < FORGE_GP_COUNT; gp++)
		s_forge_controls_ui.prev_gp_down[gp] = forge_gp_button_down(gp);
}

boolean forge_controls_menu_event(
	struct forge_widget_instance *widget,
	void *event_ptr,
	char const *fn_name,
	long function_index,
	boolean *widget_deleted)
{
	int group = fw_controls_group(widget);
	int i;

	if (strcmp(fn_name, "controls screen init") == 0)
	{
		s_forge_controls_ui.initialized = 0;
		forge_controls_ui_ensure_loaded();
		orig_pc_menu_event_function_invoke(widget, event_ptr, function_index, widget_deleted);
		return TRUE;
	}
	if (strcmp(fn_name, "controls screen defaults") == 0)
	{
		forge_controls_ui_ensure_loaded();
		if (group == 4 || group == 5)
		{
			int32_t saved_vk[FORGE_BIND_COUNT];
			int32_t saved_gp[FORGE_BIND_COUNT];
			float saved_ls = g_forge.controller_look_sens;
			float saved_fs = g_forge.controller_fly_speed;
			for (i = 0; i < FORGE_BIND_COUNT; i++)
			{
				saved_vk[i] = g_forge.bind_vk[i];
				saved_gp[i] = g_forge.bind_gp[i];
			}
			forge_controls_init_defaults();
			for (i = 0; i < FORGE_BIND_COUNT; i++)
			{
				s_forge_controls_ui.pending_vk[i] = g_forge.bind_vk[i];
				s_forge_controls_ui.pending_gp[i] = g_forge.bind_gp[i];
				g_forge.bind_vk[i] = saved_vk[i];
				g_forge.bind_gp[i] = saved_gp[i];
			}
			s_forge_controls_ui.pending_look_sens = 1.0f;
			s_forge_controls_ui.pending_fly_speed = 1.0f;
			g_forge.controller_look_sens = saved_ls;
			g_forge.controller_fly_speed = saved_fs;
			s_forge_controls_ui.capturing_action = -1;
			return TRUE;
		}
		return orig_pc_menu_event_function_invoke(widget, event_ptr, function_index, widget_deleted);
	}
	if (strcmp(fn_name, "controls screen change set") == 0)
	{
		forge_controls_ui_ensure_loaded();
		for (i = 0; i < FORGE_BIND_COUNT; i++)
		{
			g_forge.bind_vk[i] = s_forge_controls_ui.pending_vk[i];
			g_forge.bind_gp[i] = s_forge_controls_ui.pending_gp[i];
		}
		g_forge.controller_look_sens = s_forge_controls_ui.pending_look_sens;
		g_forge.controller_fly_speed = s_forge_controls_ui.pending_fly_speed;
		forge_controls_save();
		return orig_pc_menu_event_function_invoke(widget, event_ptr, function_index, widget_deleted);
	}
	if (strcmp(fn_name, "controls binding slot") == 0)
	{
		forge_controls_ui_ensure_loaded();
		if (group == 4)
		{
			s_forge_controls_ui.slot = !s_forge_controls_ui.slot;
			orig_pc_menu_event_function_invoke(widget, event_ptr, function_index, widget_deleted);
			return TRUE;
		}
		if (group == 5)
		{
			struct forge_widget_instance *row = fw_control_row(widget);
			int row_idx = row ? (atoi(row->name + 11) - 1) : -1;
			int dir = 1;
			if ((GetAsyncKeyState(VK_LEFT) & 0x8000) || (GetAsyncKeyState('A') & 0x8000) || forge_gp_button_down(FORGE_GP_DPAD_LEFT))
			{
				dir = -1;
			}
			else if (event_ptr)
			{
				short const *ev_s = (short const *)event_ptr;
				if ((ev_s[0] == 2 && ev_s[2] == 14) || (ev_s[0] == 0 && ev_s[2] < 0))
					dir = -1;
			}
			if (row_idx == 0)
			{
				s_forge_controls_ui.pending_look_sens += (float)dir * 0.25f;
				if (s_forge_controls_ui.pending_look_sens < 0.25f) s_forge_controls_ui.pending_look_sens = 0.25f;
				if (s_forge_controls_ui.pending_look_sens > 2.50f) s_forge_controls_ui.pending_look_sens = 2.50f;
				return TRUE;
			}
			if (row_idx == 1)
			{
				s_forge_controls_ui.pending_fly_speed += (float)dir * 0.25f;
				if (s_forge_controls_ui.pending_fly_speed < 0.25f) s_forge_controls_ui.pending_fly_speed = 0.25f;
				if (s_forge_controls_ui.pending_fly_speed > 2.50f) s_forge_controls_ui.pending_fly_speed = 2.50f;
				return TRUE;
			}
			s_forge_controls_ui.slot = 1;
			return TRUE;
		}
		s_forge_controls_ui.slot = !s_forge_controls_ui.slot;
		return orig_pc_menu_event_function_invoke(widget, event_ptr, function_index, widget_deleted);
	}
	if (strcmp(fn_name, "controls begin binding") == 0)
	{
		forge_controls_ui_ensure_loaded();
		if (group == 4)
		{
			struct forge_widget_instance *row = fw_control_row(widget);
			int row_idx = row ? (atoi(row->name + 11) - 1) : -1;
			if (row_idx >= 0 && row_idx < FORGE_BIND_COUNT)
			{
				s_forge_controls_ui.capturing_action = row_idx;
				s_forge_controls_ui.capturing_slot = s_forge_controls_ui.slot;
				s_forge_controls_ui.capture_start_ms = system_milliseconds();
				forge_controls_snapshot_input();
				return TRUE;
			}
		}
		else if (group == 5)
		{
			struct forge_widget_instance *row = fw_control_row(widget);
			int row_idx = row ? (atoi(row->name + 11) - 1) : -1;
			if (row_idx == 0)
			{
				s_forge_controls_ui.pending_look_sens += 0.25f;
				if (s_forge_controls_ui.pending_look_sens > 2.50f)
					s_forge_controls_ui.pending_look_sens = 0.25f;
				return TRUE;
			}
			if (row_idx == 1)
			{
				s_forge_controls_ui.pending_fly_speed += 0.25f;
				if (s_forge_controls_ui.pending_fly_speed > 2.50f)
					s_forge_controls_ui.pending_fly_speed = 0.25f;
				return TRUE;
			}
		}
		return orig_pc_menu_event_function_invoke(widget, event_ptr, function_index, widget_deleted);
	}
	return FALSE;
}

static void forge_controls_menu_update(struct forge_widget_instance *list)
{
	int group = fw_controls_group(list);
	struct forge_widget_instance *row;
	struct forge_widget_instance *focused_row = fw_control_row(list ? list->focused_child : NULL);
	float lx, ly, rx, ry, lt, rt;

	forge_controls_ui_ensure_loaded();

	if (group < 4)
	{
		s_forge_controls_ui.capturing_action = -1;
		for (row = list ? list->child : NULL; row; row = row->next)
		{
			if (row->name && strncmp(row->name, "op_command_", 11) == 0)
			{
				int idx = atoi(row->name + 11) - 1;
				if (idx >= 0 && idx < 7)
				{
					row->vertical_offset = (short)(103 + idx * 30);
				}
				else if (idx >= 7)
				{
					row->visible = FALSE;
					if (list->focused_child == row)
					{
						list->focused_child = list->child;
						list->parameters.list.selected_index = 0;
					}
				}
			}
		}
		return;
	}

	forge_poll_controller_state(&lx, &ly, &rx, &ry, &lt, &rt);

	/* Poll for new Keyboard/Mouse or Controller button press when capturing */
	if (s_forge_controls_ui.capturing_action >= 0 && s_forge_controls_ui.capturing_action < FORGE_BIND_COUNT)
	{
		unsigned long now_ms = system_milliseconds();
		int act = s_forge_controls_ui.capturing_action;
		int vk, gp;

		if (now_ms - s_forge_controls_ui.capture_start_ms >= 120)
		{
			int esc_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
			int del_down = ((GetAsyncKeyState(VK_DELETE) & 0x8000) || (GetAsyncKeyState(VK_BACK) & 0x8000)) != 0;

			if (esc_down && !s_forge_controls_ui.prev_vk_down[VK_ESCAPE])
			{
				s_forge_controls_ui.capturing_action = -1;
			}
			else if (del_down && (!s_forge_controls_ui.prev_vk_down[VK_DELETE] && !s_forge_controls_ui.prev_vk_down[VK_BACK]))
			{
				if (s_forge_controls_ui.capturing_slot == 0)
					s_forge_controls_ui.pending_vk[act] = 0;
				else
					s_forge_controls_ui.pending_gp[act] = FORGE_GP_NONE;
				s_forge_controls_ui.capturing_action = -1;
			}
			else
			{
				/* Check physical gamepad buttons first -> binds Controller column (slot 1) */
				for (gp = 1; gp < FORGE_GP_COUNT; gp++)
				{
					int down = forge_gp_button_down(gp);
					if (down && !s_forge_controls_ui.prev_gp_down[gp])
					{
						int other;
						for (other = 0; other < FORGE_BIND_COUNT; other++)
						{
							if (s_forge_controls_ui.pending_gp[other] == gp)
								s_forge_controls_ui.pending_gp[other] = FORGE_GP_NONE;
						}
						s_forge_controls_ui.pending_gp[act] = gp;
						s_forge_controls_ui.capturing_action = -1;
						break;
					}
				}

				/* Check keyboard & mouse buttons -> binds Key/Mouse column (slot 0) */
				if (s_forge_controls_ui.capturing_action >= 0)
				{
					for (vk = 1; vk < 254; vk++)
					{
						int down;
						if (vk == VK_ESCAPE || vk == VK_DELETE || vk == VK_BACK)
							continue;
						if (vk == VK_SHIFT || vk == VK_CONTROL || vk == 0x12 /* VK_MENU */)
							continue;
						if ((vk == VK_LBUTTON || vk == VK_RETURN) && (now_ms - s_forge_controls_ui.capture_start_ms < 260))
							continue;
						down = (GetAsyncKeyState(vk) & 0x8000) != 0;
						if (down && !s_forge_controls_ui.prev_vk_down[vk])
						{
							char const *vname = forge_vk_display_name(vk);
							if (vname && strcmp(vname, "-") != 0)
							{
								int other;
								for (other = 0; other < FORGE_BIND_COUNT; other++)
								{
									if (s_forge_controls_ui.pending_vk[other] == vk)
										s_forge_controls_ui.pending_vk[other] = 0;
								}
								s_forge_controls_ui.pending_vk[act] = vk;
								s_forge_controls_ui.capturing_action = -1;
								break;
							}
						}
					}
				}
			}
		}

		for (vk = 1; vk < 255; vk++)
			s_forge_controls_ui.prev_vk_down[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
		for (gp = 1; gp < FORGE_GP_COUNT; gp++)
			s_forge_controls_ui.prev_gp_down[gp] = forge_gp_button_down(gp);
	}

	/* Populate rows for group == 4 (FORGE) or group == 5 (FORGE PAD) */
	for (row = list ? list->child : NULL; row; row = row->next)
	{
		int idx;
		struct forge_widget_instance *lbl_w;
		struct forge_widget_instance *bind0_w;
		struct forge_widget_instance *bind1_w;
		char col0[64], col1[64];

		if (!row->name || strncmp(row->name, "op_command_", 11) != 0)
			continue;
		idx = atoi(row->name + 11) - 1;
		lbl_w = fw_named(row, "command_label", 0);
		bind0_w = fw_named(row, "command_binding", 0);
		bind1_w = fw_named(row, "command_binding", 1);

		if (group == 4)
		{
			if (idx < 0 || idx >= FORGE_BIND_COUNT)
			{
				row->visible = FALSE;
				continue;
			}
			row->visible = TRUE;
			row->vertical_offset = (short)(100 + idx * 26);
			fw_text_set_ascii(lbl_w, s_forge_bind_labels[idx]);

			if (s_forge_controls_ui.capturing_action == idx && s_forge_controls_ui.capturing_slot == 0)
				snprintf(col0, sizeof(col0), "PRESS KEY/PAD");
			else if (row == focused_row && s_forge_controls_ui.slot == 0)
				snprintf(col0, sizeof(col0), "> %s <", forge_vk_display_name(s_forge_controls_ui.pending_vk[idx]));
			else
				snprintf(col0, sizeof(col0), "%s", forge_vk_display_name(s_forge_controls_ui.pending_vk[idx]));

			if (s_forge_controls_ui.capturing_action == idx && s_forge_controls_ui.capturing_slot == 1)
				snprintf(col1, sizeof(col1), "PRESS KEY/PAD");
			else if (row == focused_row && s_forge_controls_ui.slot == 1)
				snprintf(col1, sizeof(col1), "> %s <", forge_gp_display_name(s_forge_controls_ui.pending_gp[idx]));
			else
				snprintf(col1, sizeof(col1), "%s", forge_gp_display_name(s_forge_controls_ui.pending_gp[idx]));

			fw_text_set_ascii(bind0_w, col0);
			fw_text_set_ascii(bind1_w, col1);
		}
		else if (group == 5)
		{
			if (idx == 0 || idx == 1)
			{
				int pct = (int)((idx == 0 ? s_forge_controls_ui.pending_look_sens : s_forge_controls_ui.pending_fly_speed) * 100.0f + 0.5f);
				row->visible = TRUE;
				row->vertical_offset = (short)(103 + idx * 30);
				fw_text_set_ascii(lbl_w, (idx == 0) ? "PAD LOOK SENS" : "PAD FLY SPEED");
				if (row == focused_row)
					snprintf(col0, sizeof(col0), "< %d%% >", pct);
				else
					snprintf(col0, sizeof(col0), "%d%%", pct);
				snprintf(col1, sizeof(col1), "LEFT / RIGHT");
				fw_text_set_ascii(bind0_w, col0);
				fw_text_set_ascii(bind1_w, col1);
			}
			else
			{
				row->visible = FALSE;
				if (list->focused_child == row)
				{
					list->focused_child = list->child;
					list->parameters.list.selected_index = 0;
				}
			}
		}
	}

	if (list && list->parameters.list.extended_description)
	{
		list->parameters.list.extended_description->parameters.text_box.string_list_index =
			(s_forge_controls_ui.capturing_action >= 0) ? 2 : (focused_row ? 1 : 0);
	}
}

void pc_menu_game_data_function_invoke(
	void *widget_ptr,
	long function)
{
	struct forge_widget_instance *widget = (struct forge_widget_instance *)widget_ptr;
	char const *fn_name = pc_menu_game_data_input_name(function);
	char const *w_name = (widget && widget->name) ? widget->name : "";
	char const *p_name = (widget && widget->parent && widget->parent->name) ? widget->parent->name : "";

	if (strncmp(w_name, "forge_", 6) == 0 || strncmp(p_name, "forge_", 6) == 0)
	{
		g_forge_session_enabled = 1;
	}

	if (fn_name && strcmp(fn_name, "load game list update") == 0 &&
		strcmp(w_name, "forge_edit_maps_list") == 0)
	{
		update_forge_edit_maps_widget(widget);
		return;
	}
	if (fn_name && strcmp(fn_name, "load game list update") == 0 &&
		strcmp(w_name, "forge_add_tags_map_list") == 0)
	{
		update_forge_add_tags_map_widget(widget);
		return;
	}
	if (fn_name && strcmp(fn_name, "load game list update") == 0 &&
		strcmp(w_name, "forge_add_tags_browser_list") == 0)
	{
		update_forge_add_tags_browser_widget(widget);
		return;
	}
	if (fn_name && strcmp(fn_name, "load game list update") == 0 &&
		strcmp(w_name, "forge_installed_tags_list") == 0)
	{
		update_forge_installed_tags_widget(widget);
		return;
	}
	if (fn_name && strcmp(fn_name, "load game list update") == 0 &&
		strcmp(w_name, "forge_snap_points_list") == 0)
	{
		update_forge_snap_points_widget(widget);
		return;
	}
	if (fn_name && strcmp(fn_name, "port mods update") == 0 &&
		strcmp(w_name, "forge_name_tag_screen") == 0)
	{
		update_forge_name_tag_modal(widget);
		return;
	}
	if (fn_name && strcmp(fn_name, "port mods update") == 0 &&
		strcmp(w_name, "forge_installed_tag_action_screen") == 0)
	{
		update_forge_installed_tag_action_modal(widget);
		return;
	}
	if (fn_name && strcmp(fn_name, "controls update menu") == 0)
	{
		int group = fw_controls_group(widget);
		if (group >= 4)
		{
			forge_controls_menu_update(widget);
			return;
		}
		orig_pc_menu_game_data_function_invoke(widget_ptr, function);
		forge_controls_menu_update(widget);
		return;
	}

	/* Point 5: Cycle Forge map chooser across 3 tabs when in Forge mode:
	 *   1 = MULTIPLAYER
	 *   3 = CUSTOM MULTIPLAYER (non-Forge custom maps only)
	 *   4 = FORGE MAPS (Forge-made/edited maps only)
	 */
	if (fn_name &&
		(strcmp(fn_name, "mp map list update") == 0 ||
		 (g_forge_session_enabled && strcmp(fn_name, "solo map list update") == 0)))
	{
		struct forge_widget_instance *spinner = fw_named(widget, "list_item_0_map_kind_spinner", 0);
		short kind = spinner ? spinner->parameters.list.selected_index : 1;

		if (spinner)
		{
			if (s_forge_last_map_kind < 0)
			{
				kind = 1;
			}
			else if (kind != s_forge_last_map_kind)
			{
				if (s_forge_last_map_kind == 1)
					kind = (kind == 0) ? 4 : 3;
				else if (s_forge_last_map_kind == 3)
					kind = (kind == 2) ? 1 : 4;
				else if (s_forge_last_map_kind == 4)
					kind = (kind == 3) ? 3 : 1;
				else
					kind = 1;
			}
			else if (kind != 1 && kind != 3 && kind != 4)
			{
				kind = 1;
			}

			/* Whenever switching tabs, pulse menu_functions' internal kind for one call so s_levels resets first=chosen=0 */
			if (kind != s_forge_last_map_kind)
			{
				s_forge_active_map_kind = 1;
				spinner->parameters.list.selected_index = (kind == 1) ? 0 : 1;
				orig_pc_menu_game_data_function_invoke(widget_ptr, function);
				s_forge_last_map_kind = kind;
			}

			s_forge_active_map_kind = kind;
			spinner->parameters.list.selected_index = (kind == 4) ? 3 : kind;
			orig_pc_menu_game_data_function_invoke(widget_ptr, function);
			spinner->parameters.list.selected_index = kind;
			return;
		}
	}
	else if (!g_forge_session_enabled && fn_name && strcmp(fn_name, "solo map list update") == 0)
	{
		struct forge_widget_instance *spinner = fw_named(widget, "list_item_0_map_kind_spinner", 0);
		if (spinner)
		{
			if (spinner->parameters.list.selected_index == 4)
			{
				spinner->parameters.list.selected_index = (s_forge_last_map_kind == 0) ? 3 : 0;
			}
			s_forge_last_map_kind = spinner->parameters.list.selected_index;
			s_forge_active_map_kind = 3;
		}
	}

	orig_pc_menu_game_data_function_invoke(widget_ptr, function);

	/* Point 7: Hide gametype rows on Server Setup screen when hosting a Multiplayer Forge session */
	if (g_forge_session_enabled && fn_name && strcmp(fn_name, "server settings update") == 0)
	{
		static char const *const gt_rows[] = {
			"op_game_type", "op_player_options", "op_item_options",
			"op_vehicle_options", "op_indicator_options", "op_team_options"
		};
		int r;
		for (r = 0; r < 6; r++)
		{
			struct forge_widget_instance *row_w = fw_named(widget, gt_rows[r], 0);
			if (row_w)
				row_w->visible = FALSE;
		}
	}
}

/* ---------- Frame Update, HUD, Camera, 343 Guilty Spark Model & Console Hooks */

int forge_is_main_menu_active(void)
{
	/* (try_and_get: menu_tags_loaded runs inside scenario_tags_load, before
	 * scenario_load sets global_scenario, and global_scenario_get asserts on NULL) */
	struct scenario *scen = global_scenario_try_and_get();
	char const *loaded = cache_file_loaded_map_name();
	char const *cur_map = main_get_map_name();
	if (!scen || scen->type == 2 /* _scenario_type_main_menu */)
		return 1;
	if (loaded && (_stricmp(loaded, "levels\\ui\\ui") == 0 || _stricmp(loaded, "ui") == 0))
		return 1;
	if (cur_map && (_stricmp(cur_map, "levels\\ui\\ui") == 0 || _stricmp(cur_map, "ui") == 0))
		return 1;
	return 0;
}

/* Whether two map names name the same map: their leaves (after the last \\ or /), in any case.
 * cache_file_loaded_map_name() and main_get_map_name() can name one map differently. */
static int forge_same_map_leaf(char const *a, char const *b)
{
	char const *la, *lb, *s;
	if (!a || !b)
		return 0;
	la = a;
	for (s = a; *s; s++)
		if (*s == '\\' || *s == '/')
			la = s + 1;
	lb = b;
	for (s = b; *s; s++)
		if (*s == '\\' || *s == '/')
			lb = s + 1;
	return _stricmp(la, lb) == 0;
}

/* Forget the per-map Forge session: what is held, hovered and open, the live instances and, with
 * clear_edits, the map's edits, its save name and the Forge network session. Runs when a map is
 * unloaded and when a running game moves to another map; never on a freshly loaded map's first
 * tick, which would wipe the placements forge_load_saved_map_state_if_present just restored. */
static void forge_reset_transient_session_state(int clear_edits)
{
	g_forge.monitor_cam_initialized = 0;
	g_forge.active_modal = _forge_modal_none;
	g_forge.grabbed_live_index = -1;
	g_forge.grabbed_object_index = NONE;
	g_forge.grabbed_is_new_spawn = 0;
	g_forge.hovered_object_index = NONE;
	g_forge.hovered_live_index = -1;
	g_forge.hovered_object_label[0] = 0;
	g_forge.live_instance_count = 0;
	if (clear_edits)
	{
		g_forge.has_saved_map_once = 0;
		g_forge.save_map_name[0] = 0;
		g_forge.save_map_name_len = 0;
		memset(&g_forge.edit_state, 0, sizeof(g_forge.edit_state));
		forge_network_reset_session();
	}
}

static int forge_variant_named_forge(wchar_t const *name)
{
	static wchar_t const forge_name[] = { 'F', 'o', 'r', 'g', 'e', 0 };
	int i;
	for (i = 0; forge_name[i]; i++)
		if (name[i] != forge_name[i])
			return 0;
	return name[i] == 0;
}

/* A game started or joined through the standard menus rather than Forge's own entry point still
 * runs Forge's variant ("Forge", endless score_to_win, forge_hooks.c's variant builder), which the
 * host's game settings carry to every client: such a game is a Forge session on this machine too. */
static void forge_detect_forge_variant(void)
{
	struct game_variant const *v;
	if (g_forge_session_enabled)
		return;
	v = game_engine_get_variant();
	if (!v)
		return;
	if (v->universal_variant.score_to_win == 0x3FFFFFFF ||
		forge_variant_named_forge(v->human_readable_game_description))
	{
		g_forge_session_enabled = 1;
	}
}

void network_chat_update(void)
{
	unsigned long now = system_milliseconds();
	float dt = 0.016f;
	char const *cur_map = forge_get_active_map_name();

	orig_network_chat_update();

	/* When on the UI main menu (levels\ui\ui), main_load_ui_scenario leaves main_get_map_name()
	 * set to the previous playable map while global_scenario_get()->type == _scenario_type_main_menu (2).
	 * Reset Forge monitor/modal state and never run Forge camera/update hooks on the UI background scene! */
	if (forge_is_main_menu_active())
	{
		extern unsigned char firefight_stats_poll_pending_link_popup(
			char *out_code, size_t code_size, char *out_device_id, size_t id_size);
		char link_code[16];
		char dev_id[32];

		if (firefight_stats_poll_pending_link_popup(link_code, sizeof(link_code), dev_id, sizeof(dev_id)))
		{
			char text[256];
			static wchar_t msg[256];
			size_t c;

			snprintf(
				text,
				sizeof(text),
				"\rEnter Code on Halostats\r%s",
				link_code);
			for (c = 0; text[c] != '\0' && c + 1 < sizeof(msg) / sizeof(msg[0]); c++)
				msg[c] = (wchar_t)(unsigned char)text[c];
			msg[c] = 0;
			ui_play_audio_feedback_sound(2);
			display_error_text_deferred(msg, 0);
		}

		g_last_forge_map_name[0] = 0;
		g_forge.monitor_cam_initialized = 0;
		g_forge.active_modal = _forge_modal_none;
		g_forge.mode = _forge_mode_play;
		g_forge.grabbed_live_index = -1;
		g_forge.grabbed_object_index = NONE;
		g_forge.hovered_object_index = NONE;
		g_forge.hovered_live_index = -1;
		return;
	}

	forge_detect_forge_variant();

	/* Reset per-map Forge state only when a running game moves to another map. A freshly loaded
	 * map's first tick has no previous name (menu_tags_loaded/unloaded clear it, and the unload
	 * already reset the session), so it only records the name: resetting there wiped the edits
	 * and live placements just restored, and dropped the host's placed objects when a client joined. */
	if (cur_map && cur_map[0] && !forge_same_map_leaf(cur_map, g_last_forge_map_name))
	{
		int had_previous_map = (g_last_forge_map_name[0] != 0);
		snprintf(g_last_forge_map_name, sizeof(g_last_forge_map_name), "%s", cur_map);
		if (had_previous_map)
			forge_reset_transient_session_state(1);
		forge_normalize_mcc_hud_tags();
	}

	if (g_last_forge_frame_ms != 0)
	{
		unsigned long diff = now - g_last_forge_frame_ms;
		if (diff > 0 && diff < 250)
			dt = (float)diff / 1000.0f;
	}
	g_last_forge_frame_ms = now;

	forge_3d_update(dt);
}

void network_chat_draw(short local_player_index)
{
	long font_index;
	struct font_header *font;
	short line_height = 13;
	rectangle2d window;

	orig_network_chat_draw(local_player_index);

	/* Never draw Forge HUD overlay on the main menu (levels\ui\ui) or outside a Forge session */
	if (!g_forge_session_enabled || forge_is_main_menu_active())
		return;

	font_index = hud_get_font_index();
	if (font_index != NONE)
	{
		font = font_definition_get(font_index);
		if (font)
			line_height = (short)(font->ascending_height + font->descending_height + font->leading_height);
	}
	if (line_height <= 0)
		line_height = 13;

	window = render.camera.window_bounds;
	offset_rectangle2d(&window, -render.camera.viewport_bounds.x0, -render.camera.viewport_bounds.y0);

	forge_3d_draw_hud(
		local_player_index,
		window.x0,
		window.y0,
		window.x1,
		window.y1,
		font_index,
		line_height);
}

static unsigned long s_forge_esc_cancel_ms = 0;

boolean network_chat_typing(void)
{
	if (orig_network_chat_typing())
		return TRUE;
	/* Block native keyboard/Escape input when a Forge modal is open or while holding a grabbed item in Monitor Mode */
	if (g_forge_session_enabled && g_forge.initialized &&
		!forge_is_main_menu_active())
	{
		if (g_forge.active_modal != _forge_modal_none ||
			forge_has_held_item_to_cancel() ||
			(s_forge_esc_cancel_ms != 0 && (system_milliseconds() - s_forge_esc_cancel_ms) < 250UL))
		{
			return TRUE;
		}
	}
	return FALSE;
}

void network_chat_handle_up(long machine_index, void const *entries, boolean from_stream)
{
	if (entries && from_stream)
	{
		char const *text = (char const *)entries + 4;
		if (forge_network_intercept_chat_packet(machine_index, text, 1))
			return;
	}
	orig_network_chat_handle_up(machine_index, entries, from_stream);
}

void network_chat_handle_down(void const *entries, boolean from_stream)
{
	if (entries && from_stream)
	{
		char const *text = (char const *)entries + 4;
		if (forge_network_intercept_chat_packet(NONE, text, 0))
			return;
	}
	orig_network_chat_handle_down(entries, from_stream);
}

void network_objects_handle_changes(void const *entries, short count)
{
	forge_network_pre_handle_object_changes(entries, count);
	orig_network_objects_handle_changes(entries, count);
	forge_network_post_handle_object_changes(entries, count);
}

void director_update(real time_delta_sec)
{
	orig_director_update(time_delta_sec);
	if (g_forge_session_enabled && g_forge.initialized && !forge_is_main_menu_active())
	{
		forge_3d_apply_monitor_camera(0);
	}
}

short director_get_perspective(short local_player_index)
{
	if (g_forge_session_enabled && g_forge.initialized && !forge_is_main_menu_active())
	{
		if (g_forge.mode == _forge_mode_3d_monitor && g_forge.third_person_view)
		{
			return _director_perspective_third_person; /* 3rd-person perspective so Monitor is drawn */
		}
	}
	return orig_director_get_perspective(local_player_index);
}

struct observer_result const *render_interpolation_camera(
	short local_player_index,
	struct observer_result const *fallback)
{
	struct observer_result const *base = orig_render_interpolation_camera(local_player_index, fallback);
	if (g_forge_session_enabled && g_forge.initialized && !forge_is_main_menu_active())
	{
		forge_render_begin_monitor_visibility();
	}
	if (base && g_forge_session_enabled && g_forge.initialized &&
		!forge_is_main_menu_active() && local_player_index == 0 &&
		g_forge.mode == _forge_mode_3d_monitor)
	{
		static struct observer_result s_live_obs;
		s_live_obs = *base;
		if (forge_3d_fill_live_observer_result(&s_live_obs))
			return &s_live_obs;
	}
	return base;
}

void first_person_weapon_draw(void)
{
	if (g_forge_session_enabled && g_forge.initialized &&
		g_forge.mode != _forge_mode_play &&
		!forge_is_main_menu_active())
	{
		/* Hide player's first-person hands & weapon model when in Forge Mode */
		return;
	}
	orig_first_person_weapon_draw();
}

extern void rasterizer_set_stencil_mode(long stencil_mode);
extern void rasterizer_set_frustum_z(real z_near, real z_far);

void game_engine_post_rasterize_objects(void)
{
	if (g_forge_session_enabled && g_forge.initialized && !forge_is_main_menu_active())
	{
		forge_render_end_monitor_visibility();
	}
	orig_game_engine_post_rasterize_objects();

	if (g_forge_session_enabled && g_forge.initialized && !forge_is_main_menu_active())
	{
		/* Render 3D Translucent Objective/Spawn/Teleporter Markers and 3D Spawn Menu Preview
		 * AFTER sky, opaque objects, BSP lightmaps, BSP diffuse textures, and fog have been drawn!
		 * Reset stencil mode to NONE and restore camera frustum constants so world markers never get rejected! */
		rasterizer_set_stencil_mode(0);
		rasterizer_set_frustum_z(0.0f, 0.0f);
		rasterizer_models_begin(FALSE);
		forge_render_world_3d_overlays(NULL);
		rasterizer_models_end();
	}
}

void hud_render_weapon_interface(struct player_datum *player)
{
	if (g_forge_session_enabled && g_forge.initialized &&
		g_forge.mode != _forge_mode_play &&
		!forge_is_main_menu_active())
	{
		/* Hide weapon HUD, weapon reticle, and 'No Weapon' prompt in Forge mode (unarmed reticle drawn in forge_3d_draw_hud) */
		return;
	}
	orig_hud_render_weapon_interface(player);
}

extern void orig_hud_draw_bitmap_direct(
	struct bitmap_data const *bitmap,
	short placement,
	point2d const *point,
	real_rectangle2d const *clip,
	real scale,
	real theta,
	pixel32 color,
	boolean is_interface_bitmap);

extern void orig_hud_draw_numbers(
	short local_player_index,
	struct hud_absolute_placement_definition const *absolute_placement,
	struct number_hud_element_definition const *numbers,
	short value,
	short decimal_value,
	short draw_flags,
	long flash_reference_time,
	real override_scale);

static int s_forge_in_hud_draw_numbers = 0;

void hud_draw_numbers(
	short local_player_index,
	struct hud_absolute_placement_definition const *absolute_placement,
	struct number_hud_element_definition const *numbers,
	short value,
	short decimal_value,
	short draw_flags,
	long flash_reference_time,
	real override_scale)
{
	s_forge_in_hud_draw_numbers = 1;
	orig_hud_draw_numbers(
		local_player_index,
		absolute_placement,
		numbers,
		value,
		decimal_value,
		draw_flags,
		flash_reference_time,
		override_scale);
	s_forge_in_hud_draw_numbers = 0;
}

void hud_draw_bitmap_direct(
	struct bitmap_data const *bitmap,
	short placement,
	point2d const *point,
	real_rectangle2d const *clip,
	real scale,
	real theta,
	pixel32 color,
	boolean is_interface_bitmap)
{
	if (forge_is_mcc_hud_active() && bitmap)
	{
		uint16_t w = *(uint16_t const *)((uint8_t const *)bitmap + 0x04);
		uint16_t h = *(uint16_t const *)((uint8_t const *)bitmap + 0x06);
		if (!(w >= 512u && h >= 512u) && (!s_forge_in_hud_draw_numbers || scale > 0.6f))
			scale *= 0.5f;
	}
	orig_hud_draw_bitmap_direct(
		bitmap,
		placement,
		point,
		clip,
		scale,
		theta,
		color,
		is_interface_bitmap);
}

void render_model(
	long model_index,
	real level_of_detail_pixels,
	struct real_matrix4x3 const *node_matrices,
	char const *region_permutation_indices,
	union real_rgb_color const *change_colors,
	real const *function_values,
	struct render_lighting const *lighting,
	union real_point3d const *centroid,
	real radius,
	struct render_model_effect const *model_effect,
	long unique_identifier,
	short forced_shader_permutation_index,
	unsigned long flags)
{
	if (unique_identifier != NONE && firefight_is_generator_hidden_biped(unique_identifier))
		return;

	/* Prevent custom scenery, machines, controls, and light fixtures from vanishing due to aggressive LOD cutoffs */
	if (unique_identifier != NONE && model_index != NONE)
	{
		struct object_datum *lod_obj = (struct object_datum *)object_try_and_get_and_verify_type(
			unique_identifier,
			_object_mask_scenery | _object_mask_machine | _object_mask_light_fixture | _object_mask_control);
		if (lod_obj)
		{
			uint8_t *mbuf = (uint8_t *)tag_get('mode', model_index);
			if (mbuf)
			{
				int32_t g_cnt = *(int32_t const *)(mbuf + 0xD0u);
				float hi_cut = *(float const *)(mbuf + 0x18u);
				*(float *)(mbuf + 0x08u) = 0.0f;
				if (level_of_detail_pixels < 32.0f)
					level_of_detail_pixels = 32.0f;
				if (g_cnt <= 1 && level_of_detail_pixels < hi_cut + 1.0f)
					level_of_detail_pixels = hi_cut + 1.0f;
			}
		}
	}

	if (g_forge_session_enabled && g_forge.initialized && !forge_is_main_menu_active())
	{
		if (unique_identifier != NONE)
		{
			/* Point 10: Hide floating gun or child items attached to any player biped that is in Monitor mode */
			{
				struct object_datum *obj = (struct object_datum *)object_try_and_get_and_verify_type(unique_identifier, (unsigned long)-1);
				if (obj && obj->object.parent_object_index != NONE &&
					forge_is_unit_in_monitor_mode(obj->object.parent_object_index))
				{
					return;
				}
			}
			if (g_forge.mode == _forge_mode_3d_monitor &&
				!g_forge.third_person_view &&
				player_control_get_unit_index(0) == unique_identifier)
			{
				return;
			}

			/* Render dynamic Cached Tag objects and Monitor player units */
			if (forge_monitor_render_player_unit(unique_identifier, lighting, flags))
				return;

			/* For the currently held Forge object, bypass 30 Hz render interpolation and use live 60+ Hz node matrices */
			if (unique_identifier == g_forge.grabbed_object_index && g_forge.grabbed_live_index >= 0)
			{
				struct object_datum *grab_obj = (struct object_datum *)object_try_and_get_and_verify_type(unique_identifier, (unsigned long)-1);
				if (grab_obj && grab_obj->object.node_matrices.size > 0)
				{
					struct real_matrix4x3 const *live_mats =
						(struct real_matrix4x3 const *)object_header_block_get(unique_identifier, &grab_obj->object.node_matrices);
					if (live_mats)
						node_matrices = live_mats;
				}
			}
		}
	}

	orig_render_model(
		model_index,
		level_of_detail_pixels,
		node_matrices,
		region_permutation_indices,
		change_colors,
		function_values,
		lighting,
		centroid,
		radius,
		model_effect,
		unique_identifier,
		forced_shader_permutation_index,
		flags);
}

boolean hs_compile_and_evaluate(char const *source)
{
	if (source)
	{
		char const *s = source;
		while (*s == ' ' || *s == '\t')
			s++;
		if (_stricmp(s, "third_person") == 0 ||
			_stricmp(s, "forge_third_person") == 0 ||
			_stricmp(s, "3rd_person") == 0)
		{
			g_forge.third_person_view = !g_forge.third_person_view;
			console_printf(
				FALSE,
				"Forge 3rd-Person View: %s",
				g_forge.third_person_view ? "ON" : "OFF");
			forge_set_status(
				"3rd-Person Forge View: %s",
				g_forge.third_person_view ? "ON" : "OFF");
			return TRUE;
		}
		if (_stricmp(s, "forge") == 0 || _stricmp(s, "forge_mode") == 0)
		{
			g_forge_session_enabled = 1;
			g_forge.mode = (g_forge.mode == _forge_mode_play) ? _forge_mode_3d_monitor : _forge_mode_play;
			console_printf(
				FALSE,
				"Mode: %s",
				g_forge.mode == _forge_mode_3d_monitor ? "Forge Mode" : "Spartan Play");
			return TRUE;
		}
	}
	return orig_hs_compile_and_evaluate(source);
}

/* ---------- Point 1: Singleplayer Maps Load Straight In (No Cutscenes, No Script Trigger Points) */

void hs_runtime_update(void)
{
	if (g_forge_session_enabled && !forge_is_main_menu_active())
	{
		/* Suppress scenario startup/continuous scripts in Forge mode so SP maps load straight in with no cutscenes */
		return;
	}
	orig_hs_runtime_update();
}

void cinematic_start(void)
{
	if (g_forge_session_enabled && !forge_is_main_menu_active())
	{
		return;
	}
	orig_cinematic_start();
}

boolean scenario_trigger_volume_test_object(short trigger_volume_index, long object_index)
{
	if (g_forge_session_enabled && !forge_is_main_menu_active())
	{
		/* Suppress all script trigger volumes and BSP switch trigger volumes in Forge mode */
		return FALSE;
	}
	return orig_scenario_trigger_volume_test_object(trigger_volume_index, object_index);
}

/* ---------- Point 5: Bypass Multiplayer Gametype Vehicle/Weapon Remapping in Forge Mode */

extern boolean custom_edition_cache_tags_loaded(void);

long game_engine_remap_object_definition(long definition_index)
{
	if (g_forge_session_enabled || g_forge.has_saved_map_once || custom_edition_cache_tags_loaded())
	{
		/* Never remap or reject Scorpion, Banshee, Rocket Warthog, Shade Turret, or custom Forge vehicles! */
		return definition_index;
	}
	return orig_game_engine_remap_object_definition(definition_index);
}

/* ---------- Point 2 & 6: In-Game Escape Pause Menu "SAVE FORGE MAP" Patch & Map Unload Cleanup */

struct forge_pause_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	char *name;
	void *base_address;
	unsigned long unused[2];
};

static void *s_pause_saved_instances = NULL;
static long s_pause_saved_count = 0;
static void *s_pause_alloc_instances = NULL;

void menu_tags_unloaded(void)
{
	forge_runtime_on_map_unload();
	s_save_forge_btn_tag = NONE;
	g_last_forge_map_name[0] = 0;
	/* (the map's edits, save name and Forge network session go with it, so the next map starts
	 * clean without network_chat_update resetting it after its saved state is restored) */
	forge_reset_transient_session_state(1);
	if (s_pause_saved_instances)
	{
		cache_files_set_tag_instances(s_pause_saved_instances, s_pause_saved_count);
		s_pause_saved_instances = NULL;
		s_pause_saved_count = 0;
	}
	if (s_pause_alloc_instances)
	{
		free(s_pause_alloc_instances);
		s_pause_alloc_instances = NULL;
	}
	orig_menu_tags_unloaded();
}

void menu_tags_loaded(char const *map_name)
{
	orig_menu_tags_loaded(map_name);

	s_save_forge_btn_tag = NONE;
	g_last_forge_map_name[0] = 0;
	g_forge.monitor_cam_initialized = 0;
	g_forge.live_instance_count = 0;
	g_forge.grabbed_live_index = -1;
	g_forge.grabbed_object_index = NONE;
	g_forge.hovered_live_index = -1;
	g_forge.hovered_object_index = NONE;

	if (map_name && (_stricmp(map_name, "ui") == 0 || _stricmp(map_name, "levels\\ui\\ui") == 0))
	{
		g_forge_session_enabled = 0;
		g_forge.mode = _forge_mode_play;
		g_forge.active_modal = _forge_modal_none;
	}
	else if (game_connection() == 0 /* _game_connection_local */ || !network_game_is_active())
	{
		dispose_global_network_game_client();
		dispose_global_network_game_server();
		network_game_accept_remote_connections(FALSE);
	}
	/* (g_last_forge_map_name stays empty: global_scenario is still NULL here, inside
	 * scenario_tags_load, so forge_get_active_map_name() could only name the UI map) */
	if (map_name && _stricmp(map_name, "ui") != 0 && _stricmp(map_name, "levels\\ui\\ui") != 0)
		forge_detect_forge_variant();

	/* Patch "SAVE FORGE MAP" button into the in-game Escape Pause Menu when in a Forge session */
	if (g_forge_session_enabled && map_name && strcmp(map_name, "ui") != 0)
	{
		long settings_btn_tag = tag_loaded('DeLa', "pc\\pause\\settings_button");
		long existing_count = 0;
		struct forge_pause_tag_instance *instances =
			(struct forge_pause_tag_instance *)cache_files_tag_instances(&existing_count);

		if (settings_btn_tag != NONE && instances && existing_count > 0 && existing_count < 65000)
		{
			uint8_t const *settings_def = (uint8_t const *)tag_get('DeLa', settings_btn_tag);
			struct forge_pause_tag_instance *grown =
				(struct forge_pause_tag_instance *)malloc((size_t)(existing_count + 2) * sizeof(*grown));
			uint8_t *btn_def = (uint8_t *)malloc(0x3EC);
			struct tag_block *ustr_def = (struct tag_block *)calloc(1, sizeof(struct tag_block));
			struct tag_data *ustr_entry = (struct tag_data *)calloc(1, sizeof(struct tag_data));
			wchar_t *wtext = (wchar_t *)calloc(32, sizeof(wchar_t));

			if (grown && btn_def && ustr_def && ustr_entry && wtext && settings_def)
			{
				static char const *const label = "SAVE MAP..";
				long max_salt = 0xE174;
				long i, ustr_tag, btn_tag;
				int32_t h_count;
				uint8_t const *h_src;

				for (i = 0; label[i]; i++)
					wtext[i] = (wchar_t)(unsigned char)label[i];
				wtext[i] = 0;

				ustr_entry->size = (long)((i + 1) * sizeof(wchar_t));
				ustr_entry->address = wtext;
				ustr_def->count = 1;
				ustr_def->address = ustr_entry;

				memcpy(grown, instances, (size_t)existing_count * sizeof(*grown));
				for (i = 0; i < existing_count; i++)
				{
					if (instances[i].tag_index != NONE)
					{
						long s = (long)(((unsigned long)instances[i].tag_index >> 16) & 0x7FFFu);
						if (s > max_salt)
							max_salt = s;
					}
				}

				ustr_tag = ((max_salt + 1) << 16) | (existing_count & 0xFFFF);
				btn_tag = ((max_salt + 2) << 16) | ((existing_count + 1) & 0xFFFF);
				s_save_forge_btn_tag = btn_tag;

				memset(&grown[existing_count], 0, sizeof(grown[0]));
				grown[existing_count].group_tag = 'ustr';
				grown[existing_count].parent_group_tags[0] = NONE;
				grown[existing_count].parent_group_tags[1] = NONE;
				grown[existing_count].tag_index = ustr_tag;
				grown[existing_count].name = "pc\\pause\\save_forge_button text";
				grown[existing_count].base_address = ustr_def;

				memcpy(btn_def, settings_def, 0x3EC);
				snprintf((char *)(btn_def + 0x04), 32, "save_forge_button");
				/* Set text_label_string_list at offset 0xEC to our ustr_tag */
				*(int32_t *)(btn_def + 0xEC + 0x00) = 'ustr';
				*(char const **)(btn_def + 0xEC + 0x04) = "pc\\pause\\save_forge_button text";
				*(int32_t *)(btn_def + 0xEC + 0x08) = 0;
				*(int32_t *)(btn_def + 0xEC + 0x0C) = ustr_tag;

				/* Clone event_handlers at offset 0x54 -> run function 0x7777 & close all pause widgets */
				h_count = *(int32_t const *)(settings_def + 0x54);
				h_src = *(uint8_t const *const *)(settings_def + 0x58);
				if (h_count > 0 && h_count <= 16 && h_src)
				{
					uint8_t *h_dst = (uint8_t *)malloc((size_t)h_count * 0x48u);
					if (h_dst)
					{
						int h;
						memcpy(h_dst, h_src, (size_t)h_count * 0x48u);
						for (h = 0; h < h_count; h++)
						{
							uint8_t *hp = h_dst + (size_t)h * 0x48u;
							/* _event_handler_run_function_bit (bit 7) | _event_handler_close_all_widgets_bit (bit 2) */
							*(int32_t *)(hp + 0x00) = (1 << 7) | (1 << 2);
							*(int16_t *)(hp + 0x06) = 0x7777;
							*(int32_t *)(hp + 0x08) = 'DeLa';
							*(void **)(hp + 0x0C) = NULL;
							*(int32_t *)(hp + 0x10) = 0;
							*(int32_t *)(hp + 0x14) = NONE;
						}
						*(uint8_t **)(btn_def + 0x58) = h_dst;
					}
				}

				memset(&grown[existing_count + 1], 0, sizeof(grown[0]));
				grown[existing_count + 1].group_tag = 'DeLa';
				grown[existing_count + 1].parent_group_tags[0] = NONE;
				grown[existing_count + 1].parent_group_tags[1] = NONE;
				grown[existing_count + 1].tag_index = btn_tag;
				grown[existing_count + 1].name = "pc\\pause\\save_forge_button";
				grown[existing_count + 1].base_address = btn_def;

				s_pause_saved_instances = instances;
				s_pause_saved_count = existing_count;
				s_pause_alloc_instances = grown;
				cache_files_set_tag_instances(grown, existing_count + 2);

				/* Insert save_forge_button into the pause menu's column list right before settings_button */
				for (i = 0; i < existing_count; i++)
				{
					uint8_t *w_def;
					int32_t c_count, c, found_at = -1;
					uint8_t *c_arr;

					if (grown[i].group_tag != 'DeLa' || !grown[i].base_address)
						continue;
					w_def = (uint8_t *)grown[i].base_address;
					if (*(int16_t const *)(w_def + 0x00) != 3) /* _widget_type_column_list */
						continue;
					c_count = *(int32_t const *)(w_def + 0x3E0);
					c_arr = *(uint8_t *const *)(w_def + 0x3E4);
					if (c_count <= 0 || c_count > 16 || !c_arr)
						continue;
					for (c = 0; c < c_count; c++)
					{
						int32_t child_tag = *(int32_t const *)(c_arr + (size_t)c * 0x50u + 0x0C);
						long abs_idx = child_tag & 0xFFFF;
						if (child_tag == settings_btn_tag ||
							(child_tag != NONE && abs_idx >= 0 && abs_idx < existing_count &&
							 grown[abs_idx].name && strcmp(grown[abs_idx].name, "pc\\pause\\settings_button") == 0))
						{
							found_at = c;
							break;
						}
					}
					if (found_at >= 0)
					{
						uint8_t *new_children = (uint8_t *)malloc((size_t)(c_count + 1) * 0x50u);
						if (new_children)
						{
							int16_t old_y0 = *(int16_t const *)(c_arr + 0 * 0x50u + 0x36);
							int16_t spacing = (c_count + 1 <= 4) ? 29 : 25;
							int16_t base_y = (old_y0 >= 60) ? (int16_t)(old_y0 - 20) : 0;
							long list_tag_idx = grown[i].tag_index;
							long s_idx;

							if (found_at > 0)
								memcpy(new_children, c_arr, (size_t)found_at * 0x50u);
							memcpy(new_children + (size_t)found_at * 0x50u, c_arr + (size_t)found_at * 0x50u, 0x50u);
							*(int32_t *)(new_children + (size_t)found_at * 0x50u + 0x00) = 'DeLa';
							*(char const **)(new_children + (size_t)found_at * 0x50u + 0x04) = "pc\\pause\\save_forge_button";
							*(int32_t *)(new_children + (size_t)found_at * 0x50u + 0x08) = 0;
							*(int32_t *)(new_children + (size_t)found_at * 0x50u + 0x0C) = btn_tag;

							for (c = found_at; c < c_count; c++)
							{
								uint8_t *dst_c = new_children + (size_t)(c + 1) * 0x50u;
								memcpy(dst_c, c_arr + (size_t)c * 0x50u, 0x50u);
							}
							for (c = 0; c < c_count + 1; c++)
							{
								uint8_t *dst_c = new_children + (size_t)c * 0x50u;
								*(int16_t *)(dst_c + 0x36) = (int16_t)(base_y + c * spacing);
							}
							*(int32_t *)(w_def + 0x3E0) = c_count + 1;
							*(uint8_t **)(w_def + 0x3E4) = new_children;
							*(int16_t *)(w_def + 0x28) = (int16_t)(*(int16_t const *)(w_def + 0x28) + spacing); /* bounds.y1 */

							/* Shift the pause list widget itself upward inside its parent screen widget so all 4 options sit centered inside the box */
							if (old_y0 < 60)
							{
								int16_t lift = (old_y0 > 10) ? 14 : 22;
								for (s_idx = 0; s_idx < existing_count; s_idx++)
								{
									uint8_t *sw_def;
									int32_t sc_count, sc;
									uint8_t *sc_arr;
									if (grown[s_idx].group_tag != 'DeLa' || !grown[s_idx].base_address)
										continue;
									sw_def = (uint8_t *)grown[s_idx].base_address;
									sc_count = *(int32_t const *)(sw_def + 0x3E0);
									sc_arr = *(uint8_t *const *)(sw_def + 0x3E4);
									if (sc_count <= 0 || sc_count > 16 || !sc_arr)
										continue;
									for (sc = 0; sc < sc_count; sc++)
									{
										if (*(int32_t const *)(sc_arr + (size_t)sc * 0x50u + 0x0C) == list_tag_idx)
										{
											*(int16_t *)(sc_arr + (size_t)sc * 0x50u + 0x36) =
												(int16_t)(*(int16_t const *)(sc_arr + (size_t)sc * 0x50u + 0x36) - lift);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	/* Ensure local tag cache and .ocetags packages are extracted and loaded AFTER pause menu tag expansion */
	if (g_forge_session_enabled)
		forge_runtime_init();
}

/* ---------- Point 11: Redirect custom_maps\<name>.(map|bmp|txt) to forge_maps\<name>\<name>.(map|bmp|txt) */

extern void *file_reference_create_from_path(void *reference, char const *full_path, boolean is_directory);
extern FILE *halo_windows_fopen(char const *filename, char const *mode);

static char const *resolve_forge_maps_redirect_path(char const *path, char *out_buf, size_t out_sz)
{
	char const *leaf = NULL;
	char const *dot;
	char sub_dir[64];
	char disk_check[512];
	size_t base_len;
	int has_d = 0;
	FILE *tf;

	if (!path || !path[0])
		return path;

	if (_strnicmp(path, "d:\\custom_maps\\", 15) == 0 || _strnicmp(path, "d:/custom_maps/", 15) == 0)
	{
		has_d = 1;
		leaf = path + 15;
	}
	else if (_strnicmp(path, "custom_maps\\", 12) == 0 || _strnicmp(path, "custom_maps/", 12) == 0)
	{
		has_d = 0;
		leaf = path + 12;
	}
	else
	{
		return path;
	}

	if (strchr(leaf, '\\') != NULL || strchr(leaf, '/') != NULL)
		return path;

	if (_stricmp(leaf, "bitmaps.map") == 0 ||
		_stricmp(leaf, "sounds.map") == 0 ||
		_stricmp(leaf, "loc.map") == 0)
	{
		snprintf(disk_check, sizeof(disk_check), "custom_maps\\%s", leaf);
		tf = fopen(disk_check, "rb");
		if (tf)
		{
			fclose(tf);
			return path;
		}
		snprintf(disk_check, sizeof(disk_check), "Faber_data\\forge_maps\\%s", leaf);
		tf = fopen(disk_check, "rb");
		if (tf)
		{
			fclose(tf);
			snprintf(out_buf, out_sz, "%sFaber_data\\forge_maps\\%s", has_d ? "d:\\" : "", leaf);
			return out_buf;
		}
		return path;
	}

	dot = strrchr(leaf, '.');
	base_len = dot ? (size_t)(dot - leaf) : strlen(leaf);
	if (base_len == 0 || base_len >= sizeof(sub_dir))
		return path;

	memcpy(sub_dir, leaf, base_len);
	sub_dir[base_len] = '\0';

	/* If custom.bmp exists inside OpenCE Forge\forge_maps\<name>\custom.bmp, Faber_data\forge_maps\<name>\custom.bmp, or forge_maps\<name>\custom.bmp, prefer it as the custom map preview! */
	if (dot && _stricmp(dot, ".bmp") == 0)
	{
		static char const *const s_bmp_dirs[] = {
			"OpenCE Forge\\forge_maps",
			"Faber_data\\forge_maps",
			"forge_maps"
		};
		int bd;
		for (bd = 0; bd < 3; bd++)
		{
			snprintf(disk_check, sizeof(disk_check), "%s\\%s\\custom.bmp", s_bmp_dirs[bd], sub_dir);
			tf = fopen(disk_check, "rb");
			if (tf)
			{
				fclose(tf);
				snprintf(out_buf, out_sz, "%s%s\\%s\\custom.bmp", has_d ? "d:\\" : "", s_bmp_dirs[bd], sub_dir);
				return out_buf;
			}
			snprintf(disk_check, sizeof(disk_check), "%s\\%s\\%s", s_bmp_dirs[bd], sub_dir, leaf);
			tf = fopen(disk_check, "rb");
			if (tf)
			{
				fclose(tf);
				snprintf(out_buf, out_sz, "%s%s\\%s\\%s", has_d ? "d:\\" : "", s_bmp_dirs[bd], sub_dir, leaf);
				return out_buf;
			}
		}
	}

	/* If the file exists directly in custom_maps\<leaf> (e.g. custom_maps\<name>.map or custom_maps\<name>.bmp), use it */
	snprintf(disk_check, sizeof(disk_check), "custom_maps\\%s", leaf);
	tf = fopen(disk_check, "rb");
	if (tf)
	{
		fclose(tf);
		return path;
	}

	{
		static char const *const s_map_dirs[] = {
			"OpenCE Forge\\forge_maps",
			"Faber_data\\forge_maps",
			"forge_maps"
		};
		int md;
		for (md = 0; md < 3; md++)
		{
			snprintf(disk_check, sizeof(disk_check), "%s\\%s\\%s", s_map_dirs[md], sub_dir, leaf);
			tf = fopen(disk_check, "rb");
			if (tf)
			{
				fclose(tf);
				snprintf(out_buf, out_sz, "%s%s\\%s\\%s", has_d ? "d:\\" : "", s_map_dirs[md], sub_dir, leaf);
				return out_buf;
			}
			snprintf(disk_check, sizeof(disk_check), "%s\\%s", s_map_dirs[md], leaf);
			tf = fopen(disk_check, "rb");
			if (tf)
			{
				fclose(tf);
				snprintf(out_buf, out_sz, "%s%s\\%s", has_d ? "d:\\" : "", s_map_dirs[md], leaf);
				return out_buf;
			}
		}
	}
	return path;
}

void *forge_ce_file_reference_create_from_path(void *reference, char const *full_path, boolean is_directory)
{
	char redirected[512];
	char const *actual = resolve_forge_maps_redirect_path(full_path, redirected, sizeof(redirected));
	return file_reference_create_from_path(reference, actual, is_directory);
}

#define FORGE_EMBED_FILE_SENTINEL ((FILE *)(uintptr_t)0x7F046E99u)

static struct
{
	int active;
	uint8_t *data;
	size_t size;
	size_t pos;
} s_embed_vfile;

static FILE *try_open_embedded_map_sidecar(char const *filename)
{
	char const *leaf;
	char const *dot;
	size_t stem_len;
	char stem[128];
	char map_path[512];
	int is_bmp = 0, is_txt = 0;
	FILE *mf;
	uint8_t hdr[0x200];
	uint32_t sig = 0, emb_off = 0, emb_sz = 0;

	if (!filename || !filename[0])
		return NULL;
	leaf = strrchr(filename, '\\');
	if (!leaf)
		leaf = strrchr(filename, '/');
	leaf = leaf ? (leaf + 1) : filename;
	dot = strrchr(leaf, '.');
	if (!dot)
		return NULL;
	if (_stricmp(dot, ".bmp") == 0)
		is_bmp = 1;
	else if (_stricmp(dot, ".txt") == 0)
		is_txt = 1;
	else
		return NULL;

	stem_len = (size_t)(dot - leaf);
	if (stem_len == 0 || stem_len >= sizeof(stem))
		return NULL;
	memcpy(stem, leaf, stem_len);
	stem[stem_len] = 0;

	snprintf(map_path, sizeof(map_path), "custom_maps/%s.map", stem);
	mf = fopen(map_path, "rb");
	if (!mf)
	{
		static char const *const sidecar_cands[] = {
			"OpenCE Forge/forge_maps/%s.map",
			"OpenCE Forge/forge_maps/%s/%s.map",
			"Faber_data/forge_maps/%s/%s.map",
			"forge_maps/%s.map",
			"forge_maps/%s/%s.map"
		};
		int ci;
		for (ci = 0; ci < 5 && !mf; ci++)
		{
			if (strstr(sidecar_cands[ci], "%s/%s"))
				snprintf(map_path, sizeof(map_path), sidecar_cands[ci], stem, stem);
			else
				snprintf(map_path, sizeof(map_path), sidecar_cands[ci], stem);
			mf = fopen(map_path, "rb");
		}
	}
	if (!mf)
		return NULL;
	if (fread(hdr, 1, sizeof(hdr), mf) != sizeof(hdr))
	{
		fclose(mf);
		return NULL;
	}
	memcpy(&sig, hdr + 0x1FCu, 4);
	if (sig == 0x464C4731u)
	{
		if (is_bmp)
		{
			memcpy(&emb_off, hdr + 0x1ECu, 4);
			memcpy(&emb_sz, hdr + 0x1F0u, 4);
		}
		else if (is_txt)
		{
			memcpy(&emb_off, hdr + 0x1F4u, 4);
			memcpy(&emb_sz, hdr + 0x1F8u, 4);
		}
	}
	if (emb_off >= 0x800u && emb_sz > 0 && emb_sz <= 4u * 1024u * 1024u)
	{
		uint8_t *buf = (uint8_t *)malloc(emb_sz);
		if (buf)
		{
			if (fseek(mf, (long)emb_off, SEEK_SET) == 0 && fread(buf, 1, emb_sz, mf) == emb_sz)
			{
				fclose(mf);
				if (s_embed_vfile.data)
					free(s_embed_vfile.data);
				s_embed_vfile.active = 1;
				s_embed_vfile.data = buf;
				s_embed_vfile.size = emb_sz;
				s_embed_vfile.pos = 0;
				return FORGE_EMBED_FILE_SENTINEL;
			}
			free(buf);
		}
	}
	fclose(mf);
	return NULL;
}

FILE *forge_ce_fopen(char const *filename, char const *mode)
{
	char redirected[512];
	char const *actual = resolve_forge_maps_redirect_path(filename, redirected, sizeof(redirected));
	FILE *fp = halo_windows_fopen(actual, mode);
	if (!fp)
		fp = try_open_embedded_map_sidecar(filename);
	return fp;
}

size_t forge_ce_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	if (stream == FORGE_EMBED_FILE_SENTINEL)
	{
		size_t total = size * nmemb;
		size_t avail;
		if (!s_embed_vfile.active || !s_embed_vfile.data || size == 0 || nmemb == 0)
			return 0;
		avail = (s_embed_vfile.pos < s_embed_vfile.size) ? (s_embed_vfile.size - s_embed_vfile.pos) : 0;
		if (total > avail)
			total = (avail / size) * size;
		if (total > 0)
		{
			memcpy(ptr, s_embed_vfile.data + s_embed_vfile.pos, total);
			s_embed_vfile.pos += total;
		}
		return total / size;
	}
	return fread(ptr, size, nmemb, stream);
}

int forge_ce_fseek(FILE *stream, long offset, int whence)
{
	if (stream == FORGE_EMBED_FILE_SENTINEL)
	{
		long target = 0;
		if (!s_embed_vfile.active)
			return -1;
		if (whence == SEEK_SET)
			target = offset;
		else if (whence == SEEK_CUR)
			target = (long)s_embed_vfile.pos + offset;
		else if (whence == SEEK_END)
			target = (long)s_embed_vfile.size + offset;
		else
			return -1;
		if (target < 0 || (size_t)target > s_embed_vfile.size)
			return -1;
		s_embed_vfile.pos = (size_t)target;
		return 0;
	}
	return fseek(stream, offset, whence);
}

long forge_ce_ftell(FILE *stream)
{
	if (stream == FORGE_EMBED_FILE_SENTINEL)
	{
		if (!s_embed_vfile.active)
			return -1L;
		return (long)s_embed_vfile.pos;
	}
	return ftell(stream);
}

int forge_ce_fclose(FILE *stream)
{
	if (stream == FORGE_EMBED_FILE_SENTINEL)
	{
		if (s_embed_vfile.data)
		{
			free(s_embed_vfile.data);
			s_embed_vfile.data = NULL;
		}
		s_embed_vfile.active = 0;
		s_embed_vfile.size = 0;
		s_embed_vfile.pos = 0;
		return 0;
	}
	return fclose(stream);
}

#define FORGE_CE_FIND_SENTINEL_HANDLE ((HANDLE)(uintptr_t)0x7F046E01u)

static struct
{
	boolean in_use;
	HANDLE primary_handle;
	HANDLE forge_dirs_handle;
	int forge_dir_idx;
} s_forge_ce_find = { FALSE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, 0 };

static BOOL forge_ce_advance_forge_dirs(LPWIN32_FIND_DATAA data)
{
	static char const *const find_patterns[3] = {
		"OpenCE Forge\\forge_maps\\*.*",
		"Faber_data\\forge_maps\\*.*",
		"d:\\forge_maps\\*.*"
	};
	static char const *const find_roots[3] = {
		"OpenCE Forge\\forge_maps",
		"Faber_data\\forge_maps",
		"forge_maps"
	};
	WIN32_FIND_DATAA dir_data;
	BOOL got_entry = FALSE;

	while (s_forge_ce_find.forge_dir_idx < 3)
	{
		char const *root = find_roots[s_forge_ce_find.forge_dir_idx];
		if (s_forge_ce_find.forge_dirs_handle == INVALID_HANDLE_VALUE)
		{
			s_forge_ce_find.forge_dirs_handle = FindFirstFileA(find_patterns[s_forge_ce_find.forge_dir_idx], &dir_data);
			if (s_forge_ce_find.forge_dirs_handle == INVALID_HANDLE_VALUE)
			{
				s_forge_ce_find.forge_dir_idx++;
				continue;
			}
			got_entry = TRUE;
		}
		else
		{
			got_entry = FindNextFileA(s_forge_ce_find.forge_dirs_handle, &dir_data);
		}

		while (got_entry)
		{
			if (dir_data.cFileName[0] != '.' &&
				strlen(dir_data.cFileName) > 0 &&
				strlen(dir_data.cFileName) < 64)
			{
				char stem[64];
				char map_check[512];
				FILE *tf;

				if (dir_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					snprintf(stem, sizeof(stem), "%s", dir_data.cFileName);
				}
				else
				{
					char *dot;
					snprintf(stem, sizeof(stem), "%s", dir_data.cFileName);
					dot = strrchr(stem, '.');
					if (!dot || _stricmp(dot, ".map") != 0)
					{
						got_entry = FindNextFileA(s_forge_ce_find.forge_dirs_handle, &dir_data);
						continue;
					}
					*dot = 0;
				}

				/* Skip if custom_maps\<name>.map already exists (already enumerated by primary_handle) */
				snprintf(map_check, sizeof(map_check), "custom_maps\\%s.map", stem);
				tf = fopen(map_check, "rb");
				if (tf)
				{
					fclose(tf);
					got_entry = FindNextFileA(s_forge_ce_find.forge_dirs_handle, &dir_data);
					continue;
				}

				if (dir_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					snprintf(map_check, sizeof(map_check), "%s\\%s\\%s.map", root, stem, stem);
				else
					snprintf(map_check, sizeof(map_check), "%s\\%s.map", root, stem);
				tf = fopen(map_check, "rb");
				if (tf)
				{
					fclose(tf);
					memset(data, 0, sizeof(*data));
					data->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
					snprintf(data->cFileName, sizeof(data->cFileName), "%s.map", stem);
					return TRUE;
				}
			}
			got_entry = FindNextFileA(s_forge_ce_find.forge_dirs_handle, &dir_data);
		}

		CloseHandle(s_forge_ce_find.forge_dirs_handle);
		s_forge_ce_find.forge_dirs_handle = INVALID_HANDLE_VALUE;
		s_forge_ce_find.forge_dir_idx++;
	}

	return FALSE;
}

HANDLE WINAPI forge_ce_FindFirstFileA(LPCSTR file_name, LPWIN32_FIND_DATAA data)
{
	if (!s_forge_ce_find.in_use && file_name &&
		(_stricmp(file_name, "d:\\custom_maps\\*.*") == 0 ||
		 _stricmp(file_name, "custom_maps\\*.*") == 0))
	{
		s_forge_ce_find.in_use = TRUE;
		s_forge_ce_find.primary_handle = FindFirstFileA(file_name, data);
		s_forge_ce_find.forge_dirs_handle = INVALID_HANDLE_VALUE;
		s_forge_ce_find.forge_dir_idx = 0;
		if (s_forge_ce_find.primary_handle != INVALID_HANDLE_VALUE)
			return FORGE_CE_FIND_SENTINEL_HANDLE;
		if (forge_ce_advance_forge_dirs(data))
			return FORGE_CE_FIND_SENTINEL_HANDLE;
		s_forge_ce_find.in_use = FALSE;
		return INVALID_HANDLE_VALUE;
	}
	return FindFirstFileA(file_name, data);
}

BOOL WINAPI forge_ce_FindNextFileA(HANDLE find_file, LPWIN32_FIND_DATAA data)
{
	if (find_file == FORGE_CE_FIND_SENTINEL_HANDLE)
	{
		if (s_forge_ce_find.primary_handle != INVALID_HANDLE_VALUE)
		{
			if (FindNextFileA(s_forge_ce_find.primary_handle, data))
				return TRUE;
			CloseHandle(s_forge_ce_find.primary_handle);
			s_forge_ce_find.primary_handle = INVALID_HANDLE_VALUE;
		}
		return forge_ce_advance_forge_dirs(data);
	}
	return FindNextFileA(find_file, data);
}

BOOL WINAPI forge_ce_CloseHandle(HANDLE hObject)
{
	if (hObject == FORGE_CE_FIND_SENTINEL_HANDLE)
	{
		if (s_forge_ce_find.primary_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(s_forge_ce_find.primary_handle);
			s_forge_ce_find.primary_handle = INVALID_HANDLE_VALUE;
		}
		if (s_forge_ce_find.forge_dirs_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(s_forge_ce_find.forge_dirs_handle);
			s_forge_ce_find.forge_dirs_handle = INVALID_HANDLE_VALUE;
		}
		s_forge_ce_find.in_use = FALSE;
		return TRUE;
	}
	return CloseHandle(hObject);
}

/* ---------- Disable Menu, Mouse, Keyboard & Controller Interaction When OpenCE - Faber.exe Is Unfocused */

struct forge_sdl_ui_pointer
{
	float x, y;
	float click_x, click_y;
	BOOL moved;
	int left_clicks, right_clicks;
	int wheel_steps;
	int downs;
	float down_x, down_y;
	BOOL touch;
};

struct forge_sdl_input_state
{
	unsigned char keys[512];
	unsigned char mouse_buttons[8];
	float mouse_dx, mouse_dy;
	float mouse_wheel;
	BOOL focused;
	BOOL mouse_released;
	BOOL ui_pointer;
	BOOL menus;
};

struct forge_sdl_keystroke
{
	BYTE virtual_key;
	CHAR ascii;
	BYTE flags;
	BOOL overlay;
};

extern BOOL orig_platform_ui_pointer_read(struct forge_sdl_ui_pointer *pointer);
extern void orig_platform_input_read(struct forge_sdl_input_state *state, BOOL consume_motion);
extern BOOL orig_platform_next_keystroke(struct forge_sdl_keystroke *keystroke);
extern DWORD WINAPI orig_XInputGetState(HANDLE hDevice, PXINPUT_STATE pState);

#ifdef _WIN32
__declspec(dllimport) void *WINAPI GetForegroundWindow(void);
__declspec(dllimport) DWORD WINAPI GetWindowThreadProcessId(void *hWnd, LPDWORD lpdwProcessId);
__declspec(dllimport) DWORD WINAPI GetCurrentProcessId(void);
#endif

int forge_window_is_focused(void)
{
#ifdef _WIN32
	static DWORD s_self_pid = 0;
	void *fg;
	DWORD fg_pid = 0;

	if (s_self_pid == 0)
		s_self_pid = GetCurrentProcessId();
	fg = GetForegroundWindow();
	if (!fg)
		return 0;
	GetWindowThreadProcessId(fg, &fg_pid);
	return (fg_pid == s_self_pid) ? 1 : 0;
#else
	return 1;
#endif
}

#undef GetAsyncKeyState
__declspec(dllimport) SHORT WINAPI GetAsyncKeyState(int vKey);

short forge_key_state(int vk)
{
	if (!forge_window_is_focused())
		return 0;
	return GetAsyncKeyState(vk);
}

BOOL platform_ui_pointer_read(struct forge_sdl_ui_pointer *pointer)
{
	BOOL active = orig_platform_ui_pointer_read(pointer);
	if (!forge_window_is_focused())
	{
		if (pointer)
		{
			pointer->moved = FALSE;
			pointer->left_clicks = 0;
			pointer->right_clicks = 0;
			pointer->wheel_steps = 0;
			pointer->downs = 0;
		}
		return FALSE;
	}
	return active;
}

void platform_input_read(struct forge_sdl_input_state *state, BOOL consume_motion)
{
	orig_platform_input_read(state, consume_motion);
	if (state && (!forge_window_is_focused() || !state->focused))
	{
		memset(state->keys, 0, sizeof(state->keys));
		memset(state->mouse_buttons, 0, sizeof(state->mouse_buttons));
		state->mouse_dx = 0.0f;
		state->mouse_dy = 0.0f;
		state->mouse_wheel = 0.0f;
		state->focused = FALSE;
	}
	else if (state)
	{
		if (s_forge_name_modal_active_ms != 0 &&
			(system_milliseconds() - s_forge_name_modal_active_ms) < 250UL)
		{
			/* Suppress Backspace from triggering menu back navigation while typing in forge_name_tag_screen */
			state->keys[42 /* SDL_SCANCODE_BACKSPACE */] = 0;
		}
		if (forge_has_held_item_to_cancel() &&
			state->keys[41 /* SDL_SCANCODE_ESCAPE */])
		{
			s_forge_esc_cancel_ms = system_milliseconds();
		}
		if (s_forge_esc_cancel_ms != 0 &&
			(system_milliseconds() - s_forge_esc_cancel_ms) < 250UL)
		{
			state->keys[41 /* SDL_SCANCODE_ESCAPE */] = 0;
		}
	}
}

BOOL platform_next_keystroke(struct forge_sdl_keystroke *keystroke)
{
	if (!forge_window_is_focused())
	{
		struct forge_sdl_keystroke dummy;
		while (orig_platform_next_keystroke(&dummy))
		{
		}
		return FALSE;
	}
	while (orig_platform_next_keystroke(keystroke))
	{
		if (keystroke &&
			(keystroke->virtual_key == VK_BACK || keystroke->ascii == '\b') &&
			s_forge_name_modal_active_ms != 0 &&
			(system_milliseconds() - s_forge_name_modal_active_ms) < 250UL)
		{
			continue;
		}
		if (keystroke &&
			(keystroke->virtual_key == VK_ESCAPE || keystroke->ascii == 27))
		{
			if (forge_has_held_item_to_cancel())
			{
				s_forge_esc_cancel_ms = system_milliseconds();
				continue;
			}
			if (s_forge_esc_cancel_ms != 0 &&
				(system_milliseconds() - s_forge_esc_cancel_ms) < 250UL)
			{
				continue;
			}
		}
		return TRUE;
	}
	return FALSE;
}

DWORD WINAPI XInputGetState(HANDLE hDevice, PXINPUT_STATE pState)
{
	DWORD rc = orig_XInputGetState(hDevice, pState);
	if (!forge_window_is_focused() && pState)
	{
		memset(&pState->Gamepad, 0, sizeof(pState->Gamepad));
	}
	return rc;
}




