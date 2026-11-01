/*
 * OpenCE : Faber ("OpenCE - Faber.exe")
 * Runtime Tag Catalog, Cached Tag Folder System (Faber_data/cached_tags/),
 * Dynamic Runtime Tag Table Expansion, Multiplayer Asset Guard, Custom Tag Extraction,
 * Standalone Map Baking, Dynamic Network Version & 343 Guilty Spark Hardware Model Renderer.
 * Strictly modular inside FORGE CODE/ with zero modifications to source/ or port/.
 */

#include "cseries.h"
#include "halo_port_limits.h" /* HALO_PORT_NETWORK_VERSION: OpenCE's protocol for standard sessions */
#include "game/game.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "units/units.h"
#include "units/unit_definitions.h"
#include "models/models.h"
#include "models/model_definitions.h"
#include "physics/physics_definitions.h"
#include "items/weapon_definitions.h"
#include "items/projectile_definitions.h"
#include "math/real_math.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "game/players.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/rasterizer_geometry.h"
#include "rasterizer/rasterizer_model_types.h"
#include "rasterizer/rasterizer_models.h"
#include "rasterizer/rasterizer_swizzle.h"
#include "render/render.h"
#include "shaders/shader_definitions.h"
#include "shaders/shaders.h"
#include "sound/sound_definitions.h"
#include "tag_files/tag_groups.h"
#include "tag_files/tag_files.h"
#include "cache/cache_files.h"
#include "custom_edition_cache.h"
#include "tag_schema.h"
#include <xtl.h>
#include "rasterizer/xbox/rasterizer_xbox.h"
#include "rasterizer/xbox/rasterizer_xbox_pixel_shader.h"
#include "zlib_prefixed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "forge_engine.h"

#undef fopen
#undef fclose
#undef fread
#undef fwrite
#undef fseek
#undef ftell
#undef remove
#undef malloc
#undef calloc
#undef free
#undef realloc

extern unsigned long system_milliseconds(void);
extern void rasterizer_set_frustum_z(real z_near, real z_far);
extern void *cache_files_tag_instances(long *count);
extern void cache_files_set_tag_instances(void *instances, long count);
extern void progress_bar_begin(boolean skip_frame_capture);
extern void progress_bar_display(real progress);
extern void progress_bar_end(void);
extern void progress_bar_eachframe(void);
extern void shell_idle(void);
extern void sound_render(void);
extern void main_present_frame(void);
extern long halo_screen_width(void);
extern void platform_translate_path(const char *xbox_path, char *host_path, unsigned long host_path_size);
extern long bitmap_format_to_d3d_format(short format, word flags);
extern long bitmap_format_to_d3d_linear_format(short format, word flags);
extern void models_fix_transparent_part_links(void);
extern boolean custom_edition_cache_tags_loaded(void);
extern long port_tag_profiles_scan(void);
extern long port_tag_profiles_count(void);
extern long port_tag_profiles_find(const char *folder);
extern long port_tag_profiles_tag_count(long profile);
extern unsigned long port_tag_profiles_tag_group(long profile, long tag);
extern const char *port_tag_profiles_tag_name(long profile, long tag);
extern int port_tag_profiles_tag_enabled(long profile, long tag);
extern const char *port_tag_profiles_tag_option(long profile, long tag, const char *key);
extern void halo_custom_edition_texels_channels(void const *address, unsigned char order);
extern boolean orig_mod_sounds_read(struct sound_permutation const *permutation, void *buffer);
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

struct forge_progress_bar_mode_mirror
{
	long state;
	boolean capture_frame;
	unsigned char padding0[3];
	IDirect3DTexture8 *texture0;
	IDirect3DTexture8 *texture1;
	boolean active;
	unsigned char padding1[3];
};
extern struct forge_progress_bar_mode_mirror progress_bar_mode;
extern real last_t;

static int s_forge_loading_active = 0;
static float s_forge_loading_map_base = 0.0f;
static float s_forge_loading_map_span = 0.0f;
static unsigned long s_forge_loading_last_ms = 0;

static uint8_t sample_dxt5_halo_logo_lum(uint8_t const *dxt5_raw, int sx, int sy)
{
	int bx = sx >> 2;
	int by = sy >> 2;
	int p_idx = ((sy & 3) << 2) | (sx & 3);
	size_t bidx = ((size_t)by * 256u + (size_t)bx) * 16u;
	uint8_t a0 = dxt5_raw[bidx + 0];
	uint8_t a1 = dxt5_raw[bidx + 1];
	uint64_t abits =
		(uint64_t)dxt5_raw[bidx + 2] |
		((uint64_t)dxt5_raw[bidx + 3] << 8) |
		((uint64_t)dxt5_raw[bidx + 4] << 16) |
		((uint64_t)dxt5_raw[bidx + 5] << 24) |
		((uint64_t)dxt5_raw[bidx + 6] << 32) |
		((uint64_t)dxt5_raw[bidx + 7] << 40);
	uint32_t acode = (uint32_t)((abits >> (p_idx * 3)) & 7u);
	uint32_t alpha;
	uint16_t c0 = *(uint16_t const *)(dxt5_raw + bidx + 8);
	uint16_t c1 = *(uint16_t const *)(dxt5_raw + bidx + 10);
	uint32_t cbits = *(uint32_t const *)(dxt5_raw + bidx + 12);
	uint32_t ccode = (cbits >> (p_idx * 2)) & 3u;
	uint32_t r0 = ((c0 >> 11) & 31u) * 255u / 31u;
	uint32_t g0 = ((c0 >> 5) & 63u) * 255u / 63u;
	uint32_t b0 = (c0 & 31u) * 255u / 31u;
	uint32_t r1 = ((c1 >> 11) & 31u) * 255u / 31u;
	uint32_t g1 = ((c1 >> 5) & 63u) * 255u / 63u;
	uint32_t b1 = (c1 & 31u) * 255u / 31u;
	uint32_t l0 = r0 > g0 ? (r0 > b0 ? r0 : b0) : (g0 > b0 ? g0 : b0);
	uint32_t l1 = r1 > g1 ? (r1 > b1 ? r1 : b1) : (g1 > b1 ? g1 : b1);
	uint32_t lum;

	if (acode == 0) alpha = a0;
	else if (acode == 1) alpha = a1;
	else if (a0 > a1) alpha = ((8u - acode) * a0 + (acode - 1u) * a1) / 7u;
	else if (acode < 6u) alpha = ((6u - acode) * a0 + (acode - 1u) * a1) / 5u;
	else if (acode == 6u) alpha = 0u;
	else alpha = 255u;

	if (ccode == 0u) lum = l0;
	else if (ccode == 1u) lum = l1;
	else if (ccode == 2u) lum = (2u * l0 + l1) / 3u;
	else lum = (l0 + 2u * l1) / 3u;

	return (uint8_t)((lum * alpha) / 255u);
}

static void forge_ensure_xbox_loading_tga(void)
{
	char z_tga_host[1024];
	FILE *zf;
	FILE *mf;
	uint8_t *tga_buf;
	uint8_t *dxt5_raw = NULL;
	int have_ui_logo = 0;
	int x, y, slot;

	platform_translate_path("z:\\maps\\loading.tga", z_tga_host, sizeof(z_tga_host));
#ifdef _WIN32
	{
		char z_maps_dir[1024];
		platform_translate_path("z:\\maps", z_maps_dir, sizeof(z_maps_dir));
		CreateDirectoryA(z_maps_dir, NULL);
	}
#endif

	zf = fopen(z_tga_host, "rb");
	if (zf)
	{
		long zsz = 0;
		fseek(zf, 0, SEEK_END);
		zsz = ftell(zf);
		fclose(zf);
		if (zsz == 230418L)
			return;
	}

	mf = fopen("maps/loading.tga", "rb");
	if (mf)
	{
		long msz = 0;
		fseek(mf, 0, SEEK_END);
		msz = ftell(mf);
		if (msz == 230418L)
		{
			tga_buf = (uint8_t *)malloc(230418u);
			if (tga_buf)
			{
				fseek(mf, 0, SEEK_SET);
				if (fread(tga_buf, 1, 230418u, mf) == 230418u)
				{
					FILE *out_z = fopen(z_tga_host, "wb");
					if (out_z)
					{
						fwrite(tga_buf, 1, 230418u, out_z);
						fclose(out_z);
					}
					free(tga_buf);
					fclose(mf);
					return;
				}
				free(tga_buf);
			}
		}
		fclose(mf);
	}

	/* Extract the Halo ring + logo from the player's own Xbox ui.map (ui\shell\main_menu\halo_logo) */
	{
		long logo_tag = tag_loaded('bitm', "ui\\shell\\main_menu\\halo_logo");
		long pix_off = 0x590E00L;
		if (logo_tag != NONE)
		{
			struct bitmap_group *bgrp = (struct bitmap_group *)tag_get('bitm', logo_tag);
			if (bgrp && bgrp->bitmaps.count > 0 && bgrp->bitmaps.address)
			{
				struct bitmap_data *bmp = (struct bitmap_data *)bgrp->bitmaps.address;
				if (bmp->pixels_offset > 2048L && bmp->pixels_size >= 262144L)
					pix_off = bmp->pixels_offset;
			}
		}

		for (slot = 0; slot < 6 && !have_ui_logo; slot++)
		{
			char z_xbox[64];
			char z_cache[1024];
			FILE *cf;
			snprintf(z_xbox, sizeof(z_xbox), "z:\\cache00%d.map", slot);
			platform_translate_path(z_xbox, z_cache, sizeof(z_cache));
			cf = fopen(z_cache, "rb");
			if (cf)
			{
				uint8_t chdr[2048];
				if (fread(chdr, 1, sizeof(chdr), cf) == sizeof(chdr) &&
					*(uint32_t const *)(chdr + 0x00) == 0x68656164u &&
					_stricmp((char const *)(chdr + 0x20), "ui") == 0)
				{
					dxt5_raw = (uint8_t *)malloc(262144u);
					if (dxt5_raw && fseek(cf, pix_off, SEEK_SET) == 0 &&
						fread(dxt5_raw, 1, 262144u, cf) == 262144u)
					{
						have_ui_logo = 1;
					}
				}
				fclose(cf);
			}
		}
	}

	tga_buf = (uint8_t *)calloc(1, 230418u);
	if (!tga_buf)
	{
		free(dxt5_raw);
		return;
	}

	/* 18-byte uncompressed 24-bit 320x240 TGA header */
	tga_buf[2] = 2;
	tga_buf[12] = (uint8_t)(320 & 0xFF);
	tga_buf[13] = (uint8_t)(320 >> 8);
	tga_buf[14] = (uint8_t)(240 & 0xFF);
	tga_buf[15] = (uint8_t)(240 >> 8);
	tga_buf[16] = 24;

	for (y = 0; y < 240; y++)
	{
		for (x = 0; x < 320; x++)
		{
			uint8_t grey = 0;
			uint8_t *dst = tga_buf + 18u + ((size_t)(239 - y) * 320u + (size_t)x) * 3u;

			if (have_ui_logo && dxt5_raw && y >= 56 && y < 184)
			{
				int sx = x * 2;
				int sy = (y - 56) * 2;
				uint8_t v0 = sample_dxt5_halo_logo_lum(dxt5_raw, sx, sy);
				uint8_t v1 = sample_dxt5_halo_logo_lum(dxt5_raw, sx + 1, sy);
				uint8_t v2 = sample_dxt5_halo_logo_lum(dxt5_raw, sx, sy + 1);
				uint8_t v3 = sample_dxt5_halo_logo_lum(dxt5_raw, sx + 1, sy + 1);
				uint8_t vmax = v0 > v1 ? v0 : v1;
				if (v2 > vmax) vmax = v2;
				if (v3 > vmax) vmax = v3;
				grey = vmax;
			}
			/* Subtle horizontal progress track beneath the Halo logo so the sweeping glow bar is always visible */
			if (y >= 190 && y <= 194 && x >= 32 && x < 288)
			{
				if (grey < 110)
					grey = 110;
			}
			dst[0] = grey;
			dst[1] = grey;
			dst[2] = grey;
		}
	}

	{
		FILE *out_z = fopen(z_tga_host, "wb");
		if (out_z)
		{
			fwrite(tga_buf, 1, 230418u, out_z);
			fclose(out_z);
		}
	}

	free(tga_buf);
	free(dxt5_raw);
}

static void forge_xbox_loading_screen_render_frame(float progress)
{
	struct rasterizer_frame_begin_parameters frame_parameters;
	struct rasterizer_window_begin_parameters window_parameters;

	if (!global_d3d_device)
		return;
	if (progress < 0.001f)
		progress = 0.001f;
	if (progress > 1.0f)
		progress = 1.0f;

	shell_idle();
	sound_render();

	memset(&frame_parameters, 0, sizeof(frame_parameters));
	rasterizer_frame_begin(&frame_parameters);
	rasterizer_windows_begin();

	memset(&window_parameters, 0, sizeof(window_parameters));
	window_parameters.camera.position.x = 0.0f;
	window_parameters.camera.position.y = 0.0f;
	window_parameters.camera.position.z = 0.0f;
	window_parameters.camera.forward.i = 0.0f;
	window_parameters.camera.forward.j = 0.0f;
	window_parameters.camera.forward.k = 1.0f;
	window_parameters.camera.up.i = 0.0f;
	window_parameters.camera.up.j = 1.0f;
	window_parameters.camera.up.k = 0.0f;
	window_parameters.camera.mirrored = FALSE;
	window_parameters.camera.vertical_field_of_view = 1.1f;
	window_parameters.camera.viewport_bounds.x0 = 0;
	window_parameters.camera.viewport_bounds.y0 = 0;
	window_parameters.camera.viewport_bounds.x1 = (short)halo_screen_width();
	window_parameters.camera.viewport_bounds.y1 = 480;
	window_parameters.camera.window_bounds = window_parameters.camera.viewport_bounds;
	window_parameters.camera.z_near = 0.01f;
	window_parameters.camera.z_far = 1.0f;
	render_camera_build_frustum(&window_parameters.camera, NULL, &window_parameters.frustum, TRUE);
	window_parameters.rasterizer_target = 0;

	rasterizer_window_begin(&window_parameters);
	progress_bar_display(progress);
	rasterizer_window_end();
	rasterizer_windows_end();
	rasterizer_frame_end();
	main_present_frame();
	s_forge_loading_last_ms = system_milliseconds();
}

static void forge_xbox_loading_screen_begin(void)
{
	if (!global_d3d_device)
		return;
	forge_ensure_xbox_loading_tga();
	progress_bar_eachframe();
	if (progress_bar_mode.texture0)
	{
		IDirect3DTexture8_Release(progress_bar_mode.texture0);
		progress_bar_mode.texture0 = NULL;
	}
	if (progress_bar_mode.texture1)
	{
		IDirect3DTexture8_Release(progress_bar_mode.texture1);
		progress_bar_mode.texture1 = NULL;
	}
	last_t = 0.0f;
	s_forge_loading_active = 1;
	progress_bar_begin(TRUE);
	forge_xbox_loading_screen_render_frame(0.001f);
}

static void forge_xbox_loading_screen_update(float progress)
{
	int catchup = 0;
	if (!global_d3d_device || !s_forge_loading_active)
		return;
	if (progress < 0.01f)
		progress = 0.01f;
	if (progress > 1.0f)
		progress = 1.0f;

	forge_xbox_loading_screen_render_frame(progress);
	while (last_t + 0.09f < progress && catchup < 3)
	{
		Sleep(10);
		forge_xbox_loading_screen_render_frame(progress);
		catchup++;
	}
}

static void forge_xbox_loading_screen_end(void)
{
	int step;
	if (!global_d3d_device || !s_forge_loading_active)
		return;
	for (step = 0; step < 12 && last_t < 0.96f; step++)
	{
		forge_xbox_loading_screen_render_frame(1.0f);
		Sleep(10);
	}
	forge_xbox_loading_screen_render_frame(1.0f);
	progress_bar_end();
	s_forge_loading_active = 0;
	s_forge_loading_map_base = 0.0f;
	s_forge_loading_map_span = 0.0f;
}

struct forge_cache_file_tag_instance
{
	long group_tag;
	long parent_group_tags[2];
	long tag_index;
	char *name;
	void *base_address;
	unsigned long unused[2];
};

struct forge_runtime_state g_forge;
int g_forge_session_enabled = 0;

long forge_get_network_version(void)
{
	return g_forge_session_enabled ? FORGE_NETWORK_VERSION : HALO_PORT_NETWORK_VERSION;
}

void forge_set_status(char const *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(g_forge.status_message, sizeof(g_forge.status_message), fmt, ap);
	va_end(ap);
	g_forge.status_message_until_ms = (uint32_t)system_milliseconds() + 6000u;
}

void forge_draw_screen_rect(short x0, short y0, short x1, short y1, uint32_t argb)
{
	if (!global_d3d_device || x1 <= x0 || y1 <= y0)
		return;

	_rasterizer_debug_immediate_begin_screenspace();
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ALPHABLENDENABLE, TRUE);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_BLENDOP, D3DBLENDOP_ADD);
	pixel_shader.final_combiner_inputs_efg = 0x1400;
	rasterizer_set_pixel_shader(&pixel_shader);

	IDirect3DDevice8_Begin(global_d3d_device, D3DPT_TRIANGLEFAN);
	IDirect3DDevice8_SetVertexDataColor(global_d3d_device, 9, argb);
	IDirect3DDevice8_SetVertexData2s(global_d3d_device, 0, x0, y0);
	IDirect3DDevice8_SetVertexData2s(global_d3d_device, 0, x1, y0);
	IDirect3DDevice8_SetVertexData2s(global_d3d_device, 0, x1, y1);
	IDirect3DDevice8_SetVertexData2s(global_d3d_device, 0, x0, y1);
	IDirect3DDevice8_End(global_d3d_device);
	_rasterizer_debug_immediate_end_screenspace();
}

int forge_is_multiplayer_session(void)
{
	short conn = game_connection();
	return (conn == _game_connection_network_client || conn == _game_connection_network_server) ? 1 : 0;
}

static int is_allowed_forge_light_or_device_tag(uint32_t grp, char const *tag_path)
{
	static struct
	{
		uint32_t grp;
		char const *path;
	} const s_allowed[] = {
		/* Functioning Beam Emitters ('mach', animated emitter + beam effect + hum sound + light) */
		{ 'mach', "levels\\a30\\devices\\beam emitter\\beam emitter" },
		{ 'mach', "levels\\a30\\devices\\beam emitter\\beam emitter red" },
		/* 12 Light Fixtures ('lifi') with 3D fixture models, lens flares, and dynamic illumination */
		{ 'lifi', "levels\\a50\\devices\\light_ceiling\\bright_light_ceiling" },
		{ 'lifi', "levels\\a50\\devices\\light_ceiling\\light_ceiling" },
		{ 'lifi', "levels\\a50\\devices\\light_ceiling_large\\light_ceiling_large" },
		{ 'lifi', "levels\\a50\\devices\\light_ground\\light_ground" },
		{ 'lifi', "levels\\b30\\devices\\light fixtures\\light fixture ceiling1\\light fixture ceiling1" },
		{ 'lifi', "levels\\b30\\devices\\light fixtures\\light fixture floor1\\light fixture floor1" },
		{ 'lifi', "levels\\b30\\devices\\light fixtures\\light fixture wall1\\light fixture wall1" },
		{ 'lifi', "levels\\b40\\devices\\b40_spotlight\\b40_spotlight" },
		{ 'lifi', "levels\\c10\\devices\\ground_bright\\ground_bright" },
		{ 'lifi', "levels\\test\\carousel\\devices\\car_spotlight\\car_spotlight" },
		{ 'lifi', "levels\\test\\damnation\\devices\\dam_spotlight\\dam_spotlight" },
		{ 'lifi', "levels\\test\\damnation\\devices\\light_ceiling\\mp bright_light_ceiling" },
		/* 5 Dynamic Point Light Scenery tags ('scen') with 3D meshes and colored dynamic lights */
		{ 'scen', "levels\\c10\\scenery\\cyan point light\\light cyan dynamic" },
		{ 'scen', "levels\\c10\\scenery\\green point light\\light green dynamic" },
		{ 'scen', "levels\\c10\\scenery\\orange point light\\light orange dynamic" },
		{ 'scen', "levels\\c10\\scenery\\red point light\\light red dynamic" },
		{ 'scen', "levels\\b40\\scenery\\b40_ctorch\\ctorch" }
	};
	size_t i;

	if (!tag_path || !tag_path[0])
		return 0;
	if ((grp == 0 || grp == 'mach') &&
		(strstr(tag_path, "door") != NULL || strstr(tag_path, "Door") != NULL || strstr(tag_path, "DOOR") != NULL))
	{
		if (strstr(tag_path, "doorblinker") == NULL && strstr(tag_path, "door_blast_collision") == NULL)
			return 1;
	}
	for (i = 0; i < sizeof(s_allowed) / sizeof(s_allowed[0]); i++)
	{
		if ((grp == 0 || grp == s_allowed[i].grp) &&
			_stricmp(tag_path, s_allowed[i].path) == 0)
		{
			return 1;
		}
	}
	return 0;
}

static int is_forge_door_tag(uint32_t grp, char const *tag_path)
{
	if (!tag_path || !tag_path[0])
		return 0;
	if ((grp == 0 || grp == 'mach') &&
		(strstr(tag_path, "door") != NULL || strstr(tag_path, "Door") != NULL || strstr(tag_path, "DOOR") != NULL))
	{
		if (strstr(tag_path, "doorblinker") == NULL && strstr(tag_path, "door_blast_collision") == NULL)
			return 1;
	}
	return 0;
}

static int is_forge_light_beacon_emitter_tag(uint32_t grp, char const *tag_path)
{
	char lower_buf[260];
	size_t i;

	if (!tag_path || !tag_path[0])
		return 0;
	if (is_forge_door_tag(grp, tag_path))
		return 0;
	if (grp == 'lifi')
		return 1;
	if (grp != 0 && grp != 'scen' && grp != 'mach')
		return 0;

	for (i = 0; tag_path[i] && i + 1 < sizeof(lower_buf); i++)
	{
		char ch = tag_path[i];
		if (ch >= 'A' && ch <= 'Z')
			ch = (char)(ch - 'A' + 'a');
		lower_buf[i] = ch;
	}
	lower_buf[i] = '\0';

	if (strstr(lower_buf, "light") != NULL ||
		strstr(lower_buf, "spotlight") != NULL ||
		strstr(lower_buf, "beacon") != NULL ||
		strstr(lower_buf, "emitter") != NULL ||
		strstr(lower_buf, "ctorch") != NULL ||
		strstr(lower_buf, "torch") != NULL)
	{
		return 1;
	}
	return 0;
}

static int is_excluded_scenery_or_effect_tag(char const *tag_path)
{
	static char const *const s_excluded_substrings[] = {
		"emitter",
		"particle",
		"effect",
		"explosion",
		"exhaust",
		"lightning",
		"point light",
		"lens flare",
		"dynamic light",
		"nomodel",
		"hologram",
		"holo control",
		"doorblinker",
		"nutblocker",
		"door_blast_collision",
		"countdown timer",
		"temp critical",
		"teleporter_shield",
		"smoke",
		"spark",
		"steam",
		"flame",
		"flare",
		"blood_pool",
		"floor_arrow",
		"cutscene_",
		"lifepod_atmosphere_entry",
		"\\devices\\",
		"scenery\\cortana_chip\\cortana_chip",
		"scenery\\baton\\baton",
		"scenery\\tubewire\\tubewire",
		"scenery\\debris\\rocks_smoldering\\rocks_smoldering",
		"levels\\b40\\scenery\\b40_cpanel\\b40_cpanel",
		"scenery\\helmet_chip\\helmet_chip",
		"scenery\\lockpick\\lockpick",
		"levels\\c10\\scenery\\c10_smallplant\\c10_smallplant",
		"levels\\c10\\scenery\\c10_lilypad\\c10_lilypad",
		"levels\\c10\\scenery\\c10_lilypadbunch\\c10_lilypadbunch",
		"levels\\c10\\scenery\\c10_lilypadcluster\\c10_lilypadcluster",
		"levels\\c10\\scenery\\c10_plantclump\\c10_plantclump",
		"levels\\c10\\scenery\\c10_lilypadbunchb\\c10_lilypadbunchb",
		"levels\\c10\\scenery\\c10_lilypadclusterb\\c10_lilypadclusterb",
		"scenery\\baton\\baton dynamic",
		"scenery\\index\\index",
		"scenery\\index_hold\\index_hold",
		"levels\\c40\\scenery\\c40_snowbushsmall_dark\\c40_snowbushsmall_dark",
		"scenery\\hilltop\\hilltop",
		"scenery\\captain_implant\\captain_implant",
		"flood_captain"
	};
	char lower_buf[260];
	size_t i, k;

	if (!tag_path || !tag_path[0])
		return 1;
	if (is_allowed_forge_light_or_device_tag(0, tag_path))
		return 0;
	if (_strnicmp(tag_path, "cinematics\\", 11) == 0)
		return 1;

	for (i = 0; tag_path[i] && i + 1 < sizeof(lower_buf); i++)
	{
		char ch = tag_path[i];
		if (ch >= 'A' && ch <= 'Z')
			ch = (char)(ch - 'A' + 'a');
		lower_buf[i] = ch;
	}
	lower_buf[i] = '\0';

	for (k = 0; k < sizeof(s_excluded_substrings) / sizeof(s_excluded_substrings[0]); k++)
	{
		if (strstr(lower_buf, s_excluded_substrings[k]) != NULL)
			return 1;
	}
	return 0;
}

static int is_excluded_weapon_or_equipment_tag(char const *tag_path)
{
	static char const *const s_excluded_weap_eqip[] = {
		"gravity rifle",
		"gravity_rifle",
		"full-spectrum",
		"full spectrum",
		"full_spectrum",
		"plasma_cannon",
		"plasma cannon",
		"fuel rod",
		"fuel_rod",
		"double speed",
		"double_speed",
		"target_laser",
		"target laser",
		"energy sword",
		"energy_sword",
		"\\flag",
		"flag\\",
		"\\ball",
		"ball\\"
	};
	char lower_buf[260];
	size_t i, k;

	if (!tag_path || !tag_path[0])
		return 1;

	for (i = 0; tag_path[i] && i + 1 < sizeof(lower_buf); i++)
	{
		char ch = tag_path[i];
		if (ch >= 'A' && ch <= 'Z')
			ch = (char)(ch - 'A' + 'a');
		lower_buf[i] = ch;
	}
	lower_buf[i] = '\0';

	for (k = 0; k < sizeof(s_excluded_weap_eqip) / sizeof(s_excluded_weap_eqip[0]); k++)
	{
		if (strstr(lower_buf, s_excluded_weap_eqip[k]) != NULL)
			return 1;
	}
	return 0;
}

int forge_catalog_entry_allowed(struct forge_catalog_entry const *entry)
{
	if (!entry)
		return 0;
	if (entry->source_cache_id == 2)
		return 1;
	/* Allow map-local tags (source_cache_id == 0) populated by forge_SyncScenarioTagsWithLoadedMap() */
	if (entry->source_cache_id == 0)
	{
		if (entry->group_tag == 'scen' || entry->group_tag == 'vehi' ||
		    entry->group_tag == 'weap' || entry->group_tag == 'eqip' ||
		    entry->group_tag == 'garb' || entry->group_tag == 'mach' ||
		    entry->group_tag == 'ctrl' || entry->group_tag == 'lifi')
		{
			return 1;
		}
	}
	/* Disable default stock bipeds, AI characters, and control devices */
	if (entry->category == _forge_cat_bipeds ||
	    entry->category == _forge_cat_ai_squads ||
	    entry->group_tag == 'bipd' ||
	    entry->group_tag == 'actv' ||
	    entry->group_tag == 'ctrl')
	{
		return 0;
	}
	/* Doors have their own dedicated DOORS category (_forge_cat_barriers_devices) */
	if (entry->category == _forge_cat_barriers_devices)
	{
		return is_forge_door_tag(entry->group_tag, entry->tag_path);
	}
	if (is_forge_door_tag(entry->group_tag, entry->tag_path))
	{
		return (entry->category == _forge_cat_barriers_devices) ? 1 : 0;
	}
	/* Lights, beacons, and emitters have their own dedicated LIGHTS category (_forge_cat_lights) */
	if (entry->category == _forge_cat_lights)
	{
		if (!is_forge_light_beacon_emitter_tag(entry->group_tag, entry->tag_path))
			return 0;
	}
	else if (is_forge_light_beacon_emitter_tag(entry->group_tag, entry->tag_path))
	{
		return 0;
	}
	if (entry->group_tag == 'mach' || entry->group_tag == 'lifi')
	{
		if (entry->source_cache_id == 2)
			return 1;
		return is_allowed_forge_light_or_device_tag(entry->group_tag, entry->tag_path);
	}
	/* Exclude gravity rifle, full spectrum vision, fuel rod (plasma_cannon), and double speed */
	if ((entry->group_tag == 'weap' || entry->group_tag == 'eqip' ||
	     entry->category == _forge_cat_weapons || entry->category == _forge_cat_equipment) &&
	    is_excluded_weapon_or_equipment_tag(entry->tag_path))
	{
		return 0;
	}
	/* Exclude all emitters, particle effects, point lights, lens flares, and device scenery props */
	if (entry->group_tag == 'scen' && is_excluded_scenery_or_effect_tag(entry->tag_path))
	{
		return 0;
	}
	/* Allow standard vehicles and Pelican in the vehicle spawn list */
	if (entry->group_tag == 'vehi' || entry->category == _forge_cat_vehicles)
	{
		static char const *const s_allowed_vehicles[] = {
			"vehicles\\warthog\\warthog",
			"vehicles\\warthog\\mp_warthog",
			"vehicles\\rwarthog\\rwarthog",
			"vehicles\\ghost\\ghost",
			"vehicles\\banshee\\banshee",
			"vehicles\\banshee\\banshee_mp",
			"vehicles\\scorpion\\scorpion",
			"vehicles\\pelican\\pelican",
			"vehicles\\c_gun_turret\\c_gun_turret",
			"vehicles\\c_gun_turret\\c_gun_turret_mp",
			"vehicles\\c gun turret\\c gun turret",
			"vehicles\\c gun turret\\c gun turret_mp"
		};
		int v, allowed = 0;
		for (v = 0; v < (int)(sizeof(s_allowed_vehicles) / sizeof(s_allowed_vehicles[0])); v++)
		{
			if (_stricmp(entry->tag_path, s_allowed_vehicles[v]) == 0)
			{
				allowed = 1;
				break;
			}
		}
		if (!allowed)
			return 0;
	}
	return 1;
}

static void make_short_display_name(char const *tag_path, char const *prefix, char *out, size_t out_sz)
{
	char const *leaf = tag_path;
	char const *p;
	for (p = tag_path; *p; p++)
	{
		if (*p == '\\' || *p == '/')
			leaf = p + 1;
	}
	if (prefix && (_stricmp(prefix, "[c]") == 0))
		snprintf(out, out_sz, "[C] %s", leaf);
	else if (prefix && (_strnicmp(prefix, "[c] ", 4) == 0))
		snprintf(out, out_sz, "[C] %s", prefix + 4);
	else if (prefix && (_stricmp(prefix, "[map]") == 0))
		snprintf(out, out_sz, "[MAP] %s", leaf);
	else if (prefix && (_strnicmp(prefix, "[map] ", 6) == 0))
		snprintf(out, out_sz, "[MAP] %s", prefix + 6);
	else if (prefix && prefix[0] == '[')
		snprintf(out, out_sz, "%s", prefix);
	else if (prefix && prefix[0])
		snprintf(out, out_sz, "%s %s", prefix, leaf);
	else
		snprintf(out, out_sz, "%s", leaf);
}

static void add_catalog_entry(
	uint32_t group_tag,
	uint8_t category,
	uint8_t source_cache_id,
	char const *tag_path,
	char const *display_prefix)
{
	int i;
	struct forge_catalog_entry *e;

	if (!tag_path || !tag_path[0] || g_forge.catalog_count >= FORGE_MAX_CATALOG_ENTRIES)
		return;

	for (i = 0; i < g_forge.catalog_count; i++)
	{
		if (g_forge.catalog[i].group_tag == group_tag &&
		    g_forge.catalog[i].category == category &&
		    _stricmp(g_forge.catalog[i].tag_path, tag_path) == 0)
		{
			if (source_cache_id == 2 && g_forge.catalog[i].source_cache_id != 2)
			{
				g_forge.catalog[i].source_cache_id = 2;
				make_short_display_name(tag_path, display_prefix, g_forge.catalog[i].display_name, sizeof(g_forge.catalog[i].display_name));
			}
			return;
		}
	}

	e = &g_forge.catalog[g_forge.catalog_count++];
	e->group_tag = group_tag;
	e->category = category;
	e->source_cache_id = source_cache_id;
	e->pad = 0;
	snprintf(e->tag_path, sizeof(e->tag_path), "%s", tag_path);
	make_short_display_name(tag_path, display_prefix, e->display_name, sizeof(e->display_name));
}

static void scan_cached_tags_manifest(char const *manifest_path)
{
	FILE *f = fopen(manifest_path, "r");
	char line[512];
	if (!f)
		return;
	while (fgets(line, sizeof(line), f))
	{
		char grp_str[16];
		int cat_id = 0;
		int src_id = 0;
		char tag_path[256];
		char custom_label[128];
		char prefix_buf[140];
		uint32_t grp = 0;
		char *nl;
		char *bar;

		nl = strchr(line, '\r');
		if (nl) *nl = '\0';
		nl = strchr(line, '\n');
		if (nl) *nl = '\0';
		if (line[0] == '#' || line[0] == '\0')
			continue;

		custom_label[0] = '\0';
		if (sscanf(line, "%4s|%d|%d|%255[^\n]", grp_str, &cat_id, &src_id, tag_path) == 4)
		{
			bar = strchr(tag_path, '|');
			if (bar)
			{
				*bar = '\0';
				snprintf(custom_label, sizeof(custom_label), "%s", bar + 1);
			}
			if (strlen(grp_str) == 4 && tag_path[0] != '\0')
			{
				grp = ((uint32_t)(uint8_t)grp_str[0] << 24) |
				      ((uint32_t)(uint8_t)grp_str[1] << 16) |
				      ((uint32_t)(uint8_t)grp_str[2] << 8) |
				      ((uint32_t)(uint8_t)grp_str[3]);
				if (grp == 'ctrl' && src_id != 2)
					continue;
				if ((grp == 'mach' || grp == 'lifi') &&
					src_id != 2 && src_id != 1 &&
					!is_allowed_forge_light_or_device_tag(grp, tag_path))
				{
					continue;
				}
				if (src_id != 2 && grp == 'scen' && is_excluded_scenery_or_effect_tag(tag_path))
					continue;
				if (src_id != 2 && grp == 'vehi' && _stricmp(tag_path, "vehicles\\wraith\\wraith") == 0)
					continue;
				if (src_id != 2 && (grp == 'weap' || grp == 'eqip') && is_excluded_weapon_or_equipment_tag(tag_path))
					continue;

				/* Map group tag directly to the separated WEAPONS vs EQUIPMENT vs VEHICLES vs SCENERY vs DOORS vs LIGHTS tabs */
				if (grp == 'weap')
					cat_id = _forge_cat_weapons;
				else if (grp == 'eqip' || grp == 'garb')
					cat_id = _forge_cat_equipment;
				else if (grp == 'vehi')
					cat_id = _forge_cat_vehicles;
				else if (is_forge_door_tag(grp, tag_path) || grp == 'ctrl')
					cat_id = _forge_cat_barriers_devices;
				else if (is_forge_light_beacon_emitter_tag(grp, tag_path))
					cat_id = _forge_cat_lights;
				else if (grp == 'scen' || grp == 'mach' || grp == 'lifi')
					cat_id = _forge_cat_scenery;
				else if (grp == 'bipd')
					cat_id = _forge_cat_bipeds;
				else if (grp == 'actv')
					cat_id = _forge_cat_ai_squads;
				else
					continue;

				if (src_id == 2)
				{
					if (custom_label[0])
					{
						if (_strnicmp(custom_label, "[c] ", 4) == 0)
							snprintf(prefix_buf, sizeof(prefix_buf), "[C] %s", custom_label + 4);
						else if (_strnicmp(custom_label, "[c]", 3) == 0)
							snprintf(prefix_buf, sizeof(prefix_buf), "[C] %s", custom_label + 3);
						else
							snprintf(prefix_buf, sizeof(prefix_buf), "[C] %s", custom_label);
					}
					else
					{
						snprintf(prefix_buf, sizeof(prefix_buf), "[C]");
					}
					add_catalog_entry(grp, (uint8_t)cat_id, 2, tag_path, prefix_buf);
				}
				else
				{
					add_catalog_entry(
						grp,
						(uint8_t)cat_id,
						src_id ? (uint8_t)src_id : 1u,
						tag_path,
						src_id ? (grp == 'actv' ? "[CUSTOM AI]" : "[CUSTOM]") : (grp == 'actv' ? "[AI]" : ""));
				}
			}
		}
	}
	fclose(f);
}

static int forge_scan_and_rip_ocetags_folders(int force_reextract);

static void forge_ensure_vault_bundle_ocetags_cached(
	long profile,
	long tag_idx,
	uint32_t root_group,
	char const *root_tag_path,
	char const *custom_name);

static void scan_faber_tag_vault_into_catalog(void)
{
	long prof_count, p, tag_count, t;

	port_tag_profiles_scan();
	prof_count = port_tag_profiles_count();
	for (p = 0; p < prof_count; p++)
	{
		tag_count = port_tag_profiles_tag_count(p);
		for (t = 0; t < tag_count; t++)
		{
			uint32_t grp;
			char const *tag_path;
			char const *custom_name;
			char prefix_buf[140];
			uint8_t cat_id;

			if (!port_tag_profiles_tag_enabled(p, t))
				continue;
			grp = (uint32_t)port_tag_profiles_tag_group(p, t);
			tag_path = port_tag_profiles_tag_name(p, t);
			if (!tag_path || !tag_path[0])
				continue;

			if (grp == 'weap')
				cat_id = _forge_cat_weapons;
			else if (grp == 'eqip')
				cat_id = _forge_cat_equipment;
			else if (grp == 'vehi')
				cat_id = _forge_cat_vehicles;
			else if (grp == 'bipd')
				cat_id = _forge_cat_bipeds;
			else if (grp == 'actv')
				cat_id = _forge_cat_ai_squads;
			else if (grp == 'lifi')
				cat_id = _forge_cat_lights;
			else if (grp == 'mach' || grp == 'ctrl')
				cat_id = is_forge_light_beacon_emitter_tag(grp, tag_path) ? _forge_cat_lights : _forge_cat_barriers_devices;
			else if (grp == 'scen')
				cat_id = is_forge_light_beacon_emitter_tag(grp, tag_path) ? _forge_cat_lights : _forge_cat_scenery;
			else
				continue;

			custom_name = port_tag_profiles_tag_option(p, t, "name");
			forge_ensure_vault_bundle_ocetags_cached(p, t, grp, tag_path, custom_name);
			if (custom_name && custom_name[0])
			{
				if (_strnicmp(custom_name, "[c] ", 4) == 0)
					snprintf(prefix_buf, sizeof(prefix_buf), "[C] %s", custom_name + 4);
				else if (_strnicmp(custom_name, "[c]", 3) == 0)
					snprintf(prefix_buf, sizeof(prefix_buf), "[C] %s", custom_name + 3);
				else
					snprintf(prefix_buf, sizeof(prefix_buf), "[C] %s", custom_name);
			}
			else
			{
				snprintf(prefix_buf, sizeof(prefix_buf), "[C]");
			}
			add_catalog_entry(grp, cat_id, 2, tag_path, prefix_buf);
		}
	}
}

void forge_runtime_reload_catalogs(void)
{
	g_forge.catalog_count = 0;

	/* Built-in Spawns & Teleporters (source_cache_id = 1; 0 is reserved for transient [MAP] local tags) */
	add_catalog_entry('spwn', _forge_cat_spawns_teleporters, 1, "spawn\\player_red", "[SPAWN] Red Team Spawn");
	add_catalog_entry('spwn', _forge_cat_spawns_teleporters, 1, "spawn\\player_blue", "[SPAWN] Blue Team Spawn");
	add_catalog_entry('spwn', _forge_cat_spawns_teleporters, 1, "spawn\\player_ffa", "[SPAWN] FFA / Neutral Spawn");
	add_catalog_entry('ffsp', _forge_cat_spawns_teleporters, 1, "spawn\\firefight_spawn", "[FF] Firefight Spawn");
	add_catalog_entry('tele', _forge_cat_spawns_teleporters, 1, "teleporter\\origin", "[TELEPORTER] Origin Node");
	add_catalog_entry('tele', _forge_cat_spawns_teleporters, 1, "teleporter\\destination", "[TELEPORTER] Destination Node");

	/* Built-in Netgame Objectives */
	add_catalog_entry('flag', _forge_cat_objectives, 1, "objective\\ctf_flag_red", "[CTF] Red Flag Base");
	add_catalog_entry('flag', _forge_cat_objectives, 1, "objective\\ctf_flag_blue", "[CTF] Blue Flag Base");
	add_catalog_entry('flag', _forge_cat_objectives, 1, "objective\\oddball_spawn", "[ODDBALL] Ball Spawn Marker");
	add_catalog_entry('flag', _forge_cat_objectives, 1, "objective\\koth_hill", "[KOTH] Hill Marker");
	add_catalog_entry('flag', _forge_cat_objectives, 1, "objective\\race_track", "[RACE] Race Marker / Checkpoint");

	/* 1. Scan custom .ocetags packages in Faber_data/custom_tags/ (if not yet indexed) */
	forge_scan_and_rip_ocetags_folders(0);

	/* 2. Load Custom .ocetags catalog first (so [c] items appear at the top of each category), then Xbox default catalog */
	scan_faber_tag_vault_into_catalog();
	scan_cached_tags_manifest("OpenCE Forge/cached_tags/custom_catalog.txt");
	scan_cached_tags_manifest("Faber_data/cached_tags/custom_catalog.txt");
	scan_cached_tags_manifest("Faber_data/cached_tags/xbox_catalog.txt");

	/* 3. Stable-sort custom .ocetags entries (source_cache_id == 2) to the front of g_forge.catalog */
	if (g_forge.catalog_count > 10)
	{
		struct forge_catalog_entry *sorted =
			(struct forge_catalog_entry *)malloc((size_t)g_forge.catalog_count * sizeof(struct forge_catalog_entry));
		if (sorted)
		{
			int i, out_n = 0;
			for (i = 0; i < g_forge.catalog_count; i++)
			{
				if (g_forge.catalog[i].source_cache_id == 2)
					sorted[out_n++] = g_forge.catalog[i];
			}
			for (i = 0; i < g_forge.catalog_count; i++)
			{
				if (g_forge.catalog[i].source_cache_id == 1)
					sorted[out_n++] = g_forge.catalog[i];
			}
			for (i = 0; i < g_forge.catalog_count; i++)
			{
				if (g_forge.catalog[i].source_cache_id != 2 && g_forge.catalog[i].source_cache_id != 1)
					sorted[out_n++] = g_forge.catalog[i];
			}
			memcpy(g_forge.catalog, sorted, (size_t)out_n * sizeof(struct forge_catalog_entry));
			free(sorted);
		}
	}
}

#ifdef _WIN32
int __stdcall CreateDirectoryA(char const *lpPathName, void *lpSecurityAttributes);
__declspec(dllimport) int __stdcall MessageBoxA(void *hWnd, char const *lpText, char const *lpCaption, unsigned int uType);
__declspec(dllimport) void *__stdcall LoadLibraryA(char const *lpLibFileName);
__declspec(dllimport) void *__stdcall GetProcAddress(void *hModule, char const *lpProcName);

struct forge_browseinfo_a
{
	void *hwndOwner;
	void *pidlRoot;
	char *pszDisplayName;
	char const *lpszTitle;
	unsigned int ulFlags;
	void *lpfn;
	long lParam;
	int iImage;
};

static int forge_browse_for_folder(char const *title, char const *default_dir, char *out_dir, size_t out_sz)
{
	typedef void *(__stdcall *sh_browse_fn)(struct forge_browseinfo_a *);
	typedef int (__stdcall *sh_get_path_fn)(void *, char *);
	void *shell32 = LoadLibraryA("shell32.dll");
	if (shell32)
	{
		sh_browse_fn pBrowse = (sh_browse_fn)GetProcAddress(shell32, "SHBrowseForFolderA");
		sh_get_path_fn pGetPath = (sh_get_path_fn)GetProcAddress(shell32, "SHGetPathFromIDListA");
		if (pBrowse && pGetPath)
		{
			char disp[260];
			char chosen[260];
			struct forge_browseinfo_a bi;
			void *pidl;
			memset(disp, 0, sizeof(disp));
			memset(chosen, 0, sizeof(chosen));
			memset(&bi, 0, sizeof(bi));
			bi.pszDisplayName = disp;
			bi.lpszTitle = title;
			bi.ulFlags = 0x00000001u | 0x00000040u; /* BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE */
			pidl = pBrowse(&bi);
			if (pidl && pGetPath(pidl, chosen) && chosen[0])
			{
				snprintf(out_dir, out_sz, "%s", chosen);
				return 1;
			}
		}
	}
	snprintf(out_dir, out_sz, "%s", default_dir);
	return 0;
}

#endif

static int is_xbox_version5_map_file(char const *map_path)
{
	FILE *fp = fopen(map_path, "rb");
	uint32_t hdr[2];
	if (!fp)
		return 0;
	if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr))
	{
		fclose(fp);
		return 0;
	}
	fclose(fp);
	return (hdr[0] == 0x68656164u /* 'head' */ && hdr[1] == 5u) ? 1 : 0;
}

static int ensure_xbox_map_decompressed_cache(
	char const *map_path,
	char *out_cache_path,
	size_t out_cache_sz);

int forge_get_readable_donor_map_path(char const *map_path, char *out_path, size_t out_sz)
{
	if (!map_path || !map_path[0] || !out_path || out_sz == 0)
		return 0;
	if (is_xbox_version5_map_file(map_path))
		return ensure_xbox_map_decompressed_cache(map_path, out_path, out_sz);
	snprintf(out_path, out_sz, "%s", map_path);
	return 1;
}

/*
 * Checks if a pre-existing decompressed Xbox cache file already exists for `map_path`
 * in OpenCE's Xbox hard-drive cache (z:\cache000.map..z:\cache005.map or z:\cache_donor.map)
 * or if `map_path` itself is already uncompressed on disk.
 * If not, decompresses the Xbox v5 map payload using zlib into OpenCE's z:\cache_donor.map.
 */
static int ensure_xbox_map_decompressed_cache(
	char const *map_path,
	char *out_cache_path,
	size_t out_cache_sz)
{
	FILE *in_fp;
	uint8_t hdr[2048];
	uint32_t head_magic, map_version, decomp_len, tag_off, tag_sz;
	char map_internal_name[36];
	int slot;

	if (!map_path || !map_path[0] || !out_cache_path || out_cache_sz == 0)
		return 0;
	out_cache_path[0] = 0;

	in_fp = fopen(map_path, "rb");
	if (!in_fp)
		return 0;
	if (fread(hdr, 1, sizeof(hdr), in_fp) != sizeof(hdr))
	{
		fclose(in_fp);
		return 0;
	}

	head_magic = *(uint32_t const *)(hdr + 0x00);
	map_version = *(uint32_t const *)(hdr + 0x04);
	decomp_len = *(uint32_t const *)(hdr + 0x08);
	tag_off = *(uint32_t const *)(hdr + 0x10);
	tag_sz = *(uint32_t const *)(hdr + 0x14);
	memset(map_internal_name, 0, sizeof(map_internal_name));
	memcpy(map_internal_name, hdr + 0x20, 31);

	if (head_magic != 0x68656164u || map_version != 5u ||
		tag_off < 2048u || tag_sz < 64u || tag_sz > 64u * 1024u * 1024u)
	{
		fclose(in_fp);
		return 0;
	}

	/* 1. Check if persistent disk cache (Faber_data/cached_tags/xbox_cache/<map_internal_name>.map)
	 * or OpenCE's Xbox hard-drive cache (z:\cache000.map .. z:\cache005.map or z:\cache_donor.map)
	 * already holds this decompressed Xbox map; if so, use it directly without duplicating! */
	for (slot = -2; slot <= 6; slot++)
	{
		char z_xbox[64];
		char z_path[512];
		FILE *zf;
		if (slot == -2)
		{
			snprintf(z_path, sizeof(z_path), "Faber_data/cached_tags/xbox_cache/%s.map", map_internal_name);
		}
		else if (slot == -1)
		{
			snprintf(z_path, sizeof(z_path), "OpenCE Forge/cached_tags/xbox_cache/%s.map", map_internal_name);
		}
		else
		{
			if (slot < 6)
				snprintf(z_xbox, sizeof(z_xbox), "z:\\cache00%d.map", slot);
			else
				snprintf(z_xbox, sizeof(z_xbox), "z:\\cache_donor.map");
			platform_translate_path(z_xbox, z_path, sizeof(z_path));
		}
		zf = fopen(z_path, "rb");
		if (zf)
		{
			uint8_t zhdr[2048];
			long zsz = 0;
			fseek(zf, 0, SEEK_END);
			zsz = ftell(zf);
			fseek(zf, 0, SEEK_SET);
			if (zsz >= (long)(tag_off + tag_sz) && fread(zhdr, 1, sizeof(zhdr), zf) == sizeof(zhdr))
			{
				if (*(uint32_t const *)(zhdr + 0x00) == 0x68656164u &&
					*(uint32_t const *)(zhdr + 0x04) == 5u &&
					*(uint32_t const *)(zhdr + 0x10) == tag_off &&
					*(uint32_t const *)(zhdr + 0x14) == tag_sz &&
					_strnicmp((char const *)(zhdr + 0x20), map_internal_name, 31) == 0)
				{
					fclose(zf);
					fclose(in_fp);
					snprintf(out_cache_path, out_cache_sz, "%s", z_path);
					return 1;
				}
			}
			fclose(zf);
		}
	}

	/* 2. Check if map_path itself is already uncompressed on disk; if so, read it directly in-place! */
	{
		long in_sz = 0;
		fseek(in_fp, 0, SEEK_END);
		in_sz = ftell(in_fp);
		if (decomp_len >= tag_off + tag_sz && in_sz >= (long)decomp_len)
		{
			uint32_t tag_sig = 0;
			if (fseek(in_fp, (long)(tag_off + 0x20u), SEEK_SET) == 0 &&
				fread(&tag_sig, 4, 1, in_fp) == 1 &&
				tag_sig == 0x626C616Du /* 'blam' */)
			{
				fclose(in_fp);
				snprintf(out_cache_path, out_cache_sz, "%s", map_path);
				return 1;
			}
		}
		fseek(in_fp, 2048L, SEEK_SET);
	}

	/* 3. Stream-decompress Xbox v5 map into persistent per-map cache Faber_data/cached_tags/xbox_cache/<map_internal_name>.map */
#ifdef _WIN32
	CreateDirectoryA("Faber_data", NULL);
	CreateDirectoryA("Faber_data/cached_tags", NULL);
	CreateDirectoryA("Faber_data/cached_tags/xbox_cache", NULL);
#endif
	if (map_internal_name[0])
		snprintf(out_cache_path, out_cache_sz, "Faber_data/cached_tags/xbox_cache/%s.map", map_internal_name);
	else
		platform_translate_path("z:\\cache_donor.map", out_cache_path, out_cache_sz);
	{
		FILE *tmp_out = fopen(out_cache_path, "wb");
		if (!tmp_out)
		{
			platform_translate_path("z:\\cache_donor.map", out_cache_path, out_cache_sz);
			tmp_out = fopen(out_cache_path, "wb");
		}
		z_stream strm;
		uint8_t *in_chunk;
		uint8_t *out_chunk;
		size_t const chunk_sz = 262144u;
		int zret = Z_OK;
		int decomp_ok = 0;

		if (!tmp_out)
		{
			fclose(in_fp);
			return 0;
		}
		fwrite(hdr, 1, sizeof(hdr), tmp_out);

		in_chunk = (uint8_t *)malloc(chunk_sz);
		out_chunk = (uint8_t *)malloc(chunk_sz);
		memset(&strm, 0, sizeof(strm));
		if (in_chunk && out_chunk && inflateInit(&strm) == Z_OK)
		{
			decomp_ok = 1;
			do
			{
				strm.avail_in = (uInt)fread(in_chunk, 1, chunk_sz, in_fp);
				if (ferror(in_fp))
				{
					decomp_ok = 0;
					break;
				}
				if (strm.avail_in == 0)
					break;
				strm.next_in = in_chunk;
				do
				{
					size_t have;
					strm.avail_out = (uInt)chunk_sz;
					strm.next_out = out_chunk;
					zret = inflate(&strm, Z_NO_FLUSH);
					if (zret == Z_NEED_DICT || zret == Z_DATA_ERROR || zret == Z_MEM_ERROR)
					{
						decomp_ok = 0;
						break;
					}
					have = chunk_sz - (size_t)strm.avail_out;
					if (have > 0 && fwrite(out_chunk, 1, have, tmp_out) != have)
					{
						decomp_ok = 0;
						break;
					}
					if (s_forge_loading_active && s_forge_loading_map_span > 0.0f)
					{
						unsigned long now_ms = system_milliseconds();
						if (now_ms - s_forge_loading_last_ms >= 15u)
						{
							float map_frac = (decomp_len > 2048u)
								? ((float)strm.total_out / (float)(decomp_len - 2048u))
								: 0.5f;
							if (map_frac < 0.0f) map_frac = 0.0f;
							if (map_frac > 0.99f) map_frac = 0.99f;
							forge_xbox_loading_screen_render_frame(
								s_forge_loading_map_base + map_frac * s_forge_loading_map_span);
						}
					}
				} while (strm.avail_out == 0);
			} while (decomp_ok && zret != Z_STREAM_END);
			inflateEnd(&strm);
		}
		free(in_chunk);
		free(out_chunk);
		fclose(tmp_out);
		fclose(in_fp);

		if (!decomp_ok)
		{
			remove(out_cache_path);
			out_cache_path[0] = 0;
			return 0;
		}
	}
	return 1;
}

static int forge_index_xbox_map_file_to_catalog(
	char const *map_path,
	FILE *xbox_manifest_fp,
	FILE *credits_fp)
{
	char cache_path[512];
	FILE *f;
	uint8_t hdr[2048];
	uint32_t tag_off, tag_sz, inst_addr, tag_base, inst_off;
	int32_t tag_count, i;
	uint8_t *tag_buf;
	int indexed_count = 0;
	char map_internal_name[64];

	if (!ensure_xbox_map_decompressed_cache(map_path, cache_path, sizeof(cache_path)))
		return 0;

	f = fopen(cache_path, "rb");
	if (!f)
		return 0;
	if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr))
	{
		fclose(f);
		return 0;
	}
	tag_off = *(uint32_t const *)(hdr + 0x10);
	tag_sz = *(uint32_t const *)(hdr + 0x14);
	memset(map_internal_name, 0, sizeof(map_internal_name));
	memcpy(map_internal_name, hdr + 0x20, 31);

	if (tag_off < 2048u || tag_sz < 64u || tag_sz > 64u * 1024u * 1024u)
	{
		fclose(f);
		return 0;
	}
	tag_buf = (uint8_t *)malloc(tag_sz);
	if (!tag_buf || fseek(f, (long)tag_off, SEEK_SET) != 0 || fread(tag_buf, 1, tag_sz, f) != tag_sz)
	{
		free(tag_buf);
		fclose(f);
		return 0;
	}
	fclose(f);

	inst_addr = *(uint32_t const *)(tag_buf + 0x00);
	tag_count = *(int32_t const *)(tag_buf + 0x0C);
	tag_base = inst_addr - 0x24u;
	inst_off = 0x24u;
	if (tag_count <= 0 || tag_count > 16384 || inst_off + (uint32_t)tag_count * 32u > tag_sz)
	{
		free(tag_buf);
		return 0;
	}

	if (credits_fp)
	{
		fprintf(credits_fp, "## Xbox Map: `%s` (`%s`)\n",
			map_internal_name[0] ? map_internal_name : map_path, cache_path);
	}

	for (i = 0; i < tag_count; i++)
	{
		uint32_t io = inst_off + (uint32_t)i * 32u;
		uint32_t grp = *(uint32_t const *)(tag_buf + io + 0x00);
		uint32_t name_addr = *(uint32_t const *)(tag_buf + io + 0x10);
		uint32_t data_addr = *(uint32_t const *)(tag_buf + io + 0x14);
		char const *tname;
		int cat_id = -1;
		char grp_str[5];

		if (name_addr < tag_base || (name_addr - tag_base) >= tag_sz)
			continue;
		tname = (char const *)(tag_buf + (name_addr - tag_base));
		if (!tname[0] || is_excluded_scenery_or_effect_tag(tname))
			continue;

		if (grp == 'weap')
		{
			if (!is_excluded_weapon_or_equipment_tag(tname))
				cat_id = _forge_cat_weapons;
		}
		else if (grp == 'eqip')
		{
			if (!is_excluded_weapon_or_equipment_tag(tname))
				cat_id = _forge_cat_equipment;
		}
		else if (grp == 'vehi')
		{
			if (_stricmp(tname, "vehicles\\wraith\\wraith") != 0)
				cat_id = _forge_cat_vehicles;
		}
		else if (is_forge_door_tag(grp, tname))
			cat_id = _forge_cat_barriers_devices;
		else if (is_forge_light_beacon_emitter_tag(grp, tname) &&
				 (grp == 'scen' || is_allowed_forge_light_or_device_tag(grp, tname)))
			cat_id = _forge_cat_lights;
		else if (grp == 'scen')
			cat_id = _forge_cat_scenery;
		else if ((grp == 'mach' || grp == 'lifi') && is_allowed_forge_light_or_device_tag(grp, tname))
			cat_id = _forge_cat_scenery;

		if (cat_id < 0 || data_addr < tag_base || (data_addr - tag_base) + 0x38u > tag_sz)
			continue;

		/* Verify the object has a valid 'mode' model reference in the Xbox map (or is a point-light scenery tag) */
		if (!is_allowed_forge_light_or_device_tag(grp, tname))
		{
			uint32_t obj_off = data_addr - tag_base;
			int32_t mod_idx = *(int32_t const *)(tag_buf + obj_off + 0x34);
			if (mod_idx == -1 || (mod_idx & 0xFFFF) >= tag_count)
				continue;
		}

		grp_str[0] = (char)((grp >> 24) & 0xFF);
		grp_str[1] = (char)((grp >> 16) & 0xFF);
		grp_str[2] = (char)((grp >> 8) & 0xFF);
		grp_str[3] = (char)(grp & 0xFF);
		grp_str[4] = '\0';

		if (xbox_manifest_fp)
			fprintf(xbox_manifest_fp, "%s|%d|0|%s\n", grp_str, cat_id, tname);
		if (credits_fp)
			fprintf(credits_fp, "- `%s` (`%s`)\n", tname, grp_str);
		indexed_count++;

		if (grp == 'mach' && _stricmp(tname, "levels\\a30\\devices\\beam emitter\\beam emitter") == 0)
		{
			char const *red_tname = "levels\\a30\\devices\\beam emitter\\beam emitter red";
			if (xbox_manifest_fp)
				fprintf(xbox_manifest_fp, "mach|%d|0|%s\n", cat_id, red_tname);
			if (credits_fp)
				fprintf(credits_fp, "- `%s` (`mach`)\n", red_tname);
			indexed_count++;
		}
	}

	if (credits_fp)
		fprintf(credits_fp, "\n");
	free(tag_buf);
	return indexed_count;
}

static int forge_rip_maps_folder_to_cached_tags(char const *maps_dir)
{
#ifdef _WIN32
	char pattern[512];
	WIN32_FIND_DATAA fd;
	HANDLE h;
	FILE *xbox_manifest_fp;
	FILE *credits_fp;
	char map_files[64][512];
	int map_file_count = 0;
	int m;
	int ripped_maps = 0;
	int total_assets = 0;

	snprintf(pattern, sizeof(pattern), "%s\\*.map", maps_dir);
	h = FindFirstFileA(pattern, &fd);
	if (h == INVALID_HANDLE_VALUE)
		return 0;

	do
	{
		char full_path[512];
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		if (_stricmp(fd.cFileName, "bitmaps.map") == 0 ||
			_stricmp(fd.cFileName, "sounds.map") == 0 ||
			_stricmp(fd.cFileName, "loc.map") == 0 ||
			_stricmp(fd.cFileName, "ui.map") == 0 ||
			_stricmp(fd.cFileName, "forgeasset.map") == 0 ||
			_stricmp(fd.cFileName, "customforgeasset.map") == 0 ||
			_stricmp(fd.cFileName, "_temp_xbox_decomp.map") == 0)
		{
			continue;
		}
		snprintf(full_path, sizeof(full_path), "%s/%s", maps_dir, fd.cFileName);
		/* Strictly pull default tags from Xbox maps (version == 5) ONLY! */
		if (!is_xbox_version5_map_file(full_path))
			continue;
		if (map_file_count < 64)
			snprintf(map_files[map_file_count++], sizeof(map_files[0]), "%s", full_path);
	} while (FindNextFileA(h, &fd));
	CloseHandle(h);

	if (map_file_count <= 0)
		return 0;

	remove("Faber_data/cached_tags/catalog.txt");
	xbox_manifest_fp = fopen("Faber_data/cached_tags/xbox_catalog.txt", "wb");
	if (!xbox_manifest_fp)
		return 0;
	fprintf(xbox_manifest_fp, "# OpenCE : Faber - Native Xbox Maps Tag Catalog (Built from Xbox maps in %s)\n", maps_dir);

	credits_fp = fopen("Faber_data/credits.md", "wb");
	if (credits_fp)
	{
		fprintf(credits_fp, "# OpenCE : Faber - Native Xbox Map Tag Catalog\n\n");
		fprintf(credits_fp, "Indexed natively from Xbox maps in `%s/`.\n\n", maps_dir);
	}

	forge_xbox_loading_screen_begin();
	for (m = 0; m < map_file_count; m++)
	{
		int count_from_map;
		s_forge_loading_map_base = (float)m / (float)map_file_count;
		s_forge_loading_map_span = 1.0f / (float)map_file_count;
		forge_xbox_loading_screen_update(s_forge_loading_map_base + 0.05f * s_forge_loading_map_span);
		count_from_map = forge_index_xbox_map_file_to_catalog(
			map_files[m], xbox_manifest_fp, credits_fp);
		forge_xbox_loading_screen_update((float)(m + 1) / (float)map_file_count);
		if (count_from_map > 0)
		{
			ripped_maps++;
			total_assets += count_from_map;
		}
	}
	forge_xbox_loading_screen_end();

	{
		char donor_cache[512];
		platform_translate_path("z:\\cache_donor.map", donor_cache, sizeof(donor_cache));
		remove(donor_cache);
	}

	if (credits_fp)
		fclose(credits_fp);
	fclose(xbox_manifest_fp);
	return (ripped_maps > 0 && total_assets > 0) ? total_assets : 0;
#else
	(void)maps_dir;
	return 0;
#endif
}

static void forge_delete_files_matching(char const *dir_path, char const *pattern)
{
#ifdef _WIN32
	char search_pat[512];
	WIN32_FIND_DATAA fd;
	HANDLE h;
	snprintf(search_pat, sizeof(search_pat), "%s\\%s", dir_path, pattern);
	h = FindFirstFileA(search_pat, &fd);
	if (h == INVALID_HANDLE_VALUE)
		return;
	do
	{
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			char full[512];
			snprintf(full, sizeof(full), "%s/%s", dir_path, fd.cFileName);
			remove(full);
		}
	} while (FindNextFileA(h, &fd));
	CloseHandle(h);
#else
	(void)dir_path;
	(void)pattern;
#endif
}

static void forge_ensure_local_tag_cache_built(void)
{
	{
		FILE *f = fopen("Faber_data/cached_tags/xbox_catalog.txt", "r");
		if (f)
		{
			char line[512];
			int has_base_tags = 0;
			int has_lights_and_door = 0;
			while (fgets(line, sizeof(line), f))
			{
				if (strstr(line, "|0|") != NULL)
					has_base_tags = 1;
				if (strstr(line, "beam emitter red") != NULL)
					has_lights_and_door = 1;
			}
			fclose(f);
			if (has_base_tags && has_lights_and_door)
				return;
		}
	}

#ifdef _WIN32
	CreateDirectoryA("Faber_data", NULL);
	CreateDirectoryA("Faber_data/custom_tags", NULL);
	CreateDirectoryA("Faber_data/cached_tags", NULL);

	{
		int extracted_total = forge_rip_maps_folder_to_cached_tags("maps");
		if (extracted_total <= 0)
			extracted_total = forge_rip_maps_folder_to_cached_tags("FORGE CODE/GAME MAPS/XBOX MAPS");
		if (extracted_total <= 0)
			extracted_total = forge_rip_maps_folder_to_cached_tags("../FORGE CODE/GAME MAPS/XBOX MAPS");

		if (extracted_total <= 0)
		{
			int resp = MessageBoxA(
				NULL,
				"OpenCE : Faber - Startup Xbox Map Tag Cache\n\n"
				"No pre-existing Xbox map cache was found.\n\n"
				"Click Yes to select your Xbox 'maps/' folder and index the default Xbox Vehicle, Scenery, Weapon & Equipment tags.",
				"OpenCE : Faber - Xbox Map Tag Cache",
				0x00000004u | 0x00000040u /* MB_YESNO | MB_ICONINFORMATION */);
			if (resp == 6 /* IDYES */)
			{
				char maps_dir[260];
				snprintf(maps_dir, sizeof(maps_dir), "maps");
				forge_browse_for_folder(
					"Select your Halo Xbox 'maps' folder (containing Xbox .map files):",
					"maps",
					maps_dir,
					sizeof(maps_dir));
				extracted_total = forge_rip_maps_folder_to_cached_tags(maps_dir);
			}
		}
	}
#endif
}

void forge_runtime_init(void)
{
	if (g_forge.initialized)
		return;
	memset(&g_forge, 0, sizeof(g_forge));
	g_forge.initialized = 1;
	g_forge.mode = _forge_mode_play;
	g_forge.active_modal = _forge_modal_none;
	g_forge.current_category = _forge_cat_vehicles;
	g_forge.snap_mode = _forge_snap_15_deg;
	g_forge.ai_team_mode = _forge_ai_team_enemy_covenant;
	g_forge.cam_speed = 12.0f;
	g_forge.grabbed_object_index = NONE;
	g_forge.grabbed_live_index = -1;
	g_forge.hovered_object_index = NONE;
	g_forge.hovered_live_index = -1;
	g_forge.grab_distance = 5.0f;
	snprintf(g_forge.save_map_name, sizeof(g_forge.save_map_name), "forge_custom_arena");
	g_forge.save_map_name_len = (int)strlen(g_forge.save_map_name);

#ifdef _WIN32
	CreateDirectoryA("Faber_data", NULL);
	CreateDirectoryA("Faber_data/custom_tags", NULL);
#endif
	forge_ensure_local_tag_cache_built();
	forge_runtime_reload_catalogs();
	forge_snap_points_load_all();
}

/* ============================================================================
 * NATIVE XBOX MAP TAG LOADER & .OCETAGS CUSTOM TAG LOADER
 * Default tags are loaded natively from Xbox maps (using OpenCE's exact tag
 * schema & hardware vertex/triangle/texture registration).
 * Extra custom tags are loaded natively from .ocetags in custom_tags/.
 * ============================================================================ */

#define FORGE_MAX_HW_PARTS       48
#define FORGE_MAX_DYNAMIC_TAGS   256
#define FORGE_MAX_PHYS_ALLOCS    8192
#define FORGE_MAX_DYN_SOUND_PERMS 4096

struct forge_hw_part
{
	struct vertex_buffer vertex_buffer;
	struct triangle_buffer triangle_buffer;
	char shader_hint[128];
	float u_scale;
	float v_scale;
};

struct forge_dynamic_tag_entry
{
	uint32_t group_tag;
	char tag_path[256];
	long definition_index;
	float bounding_radius;
	int part_count;
	struct forge_hw_part parts[FORGE_MAX_HW_PARTS];
};

static struct
{
	void *saved_base_instances;
	long saved_base_count;
	void *allocated_instances;
	int count;
	struct forge_dynamic_tag_entry entries[FORGE_MAX_DYNAMIC_TAGS];
} s_dyn_tags;

static struct
{
	int count;
	void *ptrs[FORGE_MAX_PHYS_ALLOCS];
} s_forge_phys_allocs;

static struct
{
	int count;
	struct sound_permutation const *perms[FORGE_MAX_DYN_SOUND_PERMS];
} s_dyn_sound_perms;

static void *forge_phys_alloc(size_t bytes)
{
	void *p;
	size_t alloc_sz = (bytes > 64u) ? bytes : 64u;
	p = XPhysicalAlloc((unsigned long)alloc_sz, (unsigned long)-1, 0, PAGE_READWRITE);
	if (p)
	{
		memset(p, 0, alloc_sz);
		if (s_forge_phys_allocs.count < FORGE_MAX_PHYS_ALLOCS)
			s_forge_phys_allocs.ptrs[s_forge_phys_allocs.count++] = p;
		return p;
	}
	return calloc(1, alloc_sz);
}

static void forge_register_dyn_sound_perm(struct sound_permutation const *perm)
{
	int i;
	if (!perm || !perm->samples.address || perm->samples.size <= 0)
		return;
	for (i = 0; i < s_dyn_sound_perms.count; i++)
	{
		if (s_dyn_sound_perms.perms[i] == perm)
			return;
	}
	if (s_dyn_sound_perms.count < FORGE_MAX_DYN_SOUND_PERMS)
		s_dyn_sound_perms.perms[s_dyn_sound_perms.count++] = perm;
}

boolean mod_sounds_read(
	struct sound_permutation const *permutation,
	void *buffer)
{
	int i;
	if (permutation && buffer)
	{
		for (i = 0; i < s_dyn_sound_perms.count; i++)
		{
			if (s_dyn_sound_perms.perms[i] == permutation &&
				permutation->samples.address &&
				permutation->samples.size > 0)
			{
				memcpy(buffer, permutation->samples.address, (size_t)permutation->samples.size);
				return TRUE;
			}
		}
	}
	return orig_mod_sounds_read(permutation, buffer);
}

static int s_marker_meshes_loaded_flag = 0;
static long s_monitor_bipd_handle = NONE;
static int s_monitor_resolved = 0;
static int s_local_monitor_drawn_this_frame = 0;
static int s_mcc_hud_active = 0;

void forge_runtime_on_map_unload(void)
{
	int i;
	s_mcc_hud_active = 0;
	/* Dispose converted D3D8 vertex/index buffers for injected models BEFORE freeing tag buffers */
	custom_edition_injected_models_dispose();
	for (i = 0; i < s_forge_phys_allocs.count; i++)
	{
		if (s_forge_phys_allocs.ptrs[i])
			XPhysicalFree(s_forge_phys_allocs.ptrs[i]);
	}
	s_forge_phys_allocs.count = 0;
	s_dyn_sound_perms.count = 0;
	s_marker_meshes_loaded_flag = 0;
	s_monitor_bipd_handle = NONE;
	s_monitor_resolved = 0;
	s_local_monitor_drawn_this_frame = 0;

	if (s_dyn_tags.saved_base_instances)
	{
		cache_files_set_tag_instances(s_dyn_tags.saved_base_instances, s_dyn_tags.saved_base_count);
		s_dyn_tags.saved_base_instances = NULL;
		s_dyn_tags.saved_base_count = 0;
	}
	if (s_dyn_tags.allocated_instances)
	{
		free(s_dyn_tags.allocated_instances);
		s_dyn_tags.allocated_instances = NULL;
	}
	s_dyn_tags.count = 0;
	{
		char donor_cache[512];
		platform_translate_path("z:\\cache_donor.map", donor_cache, sizeof(donor_cache));
		remove(donor_cache);
	}
}

static void build_hw_part_from_arrays(
	struct forge_hw_part *dst_part,
	uint8_t *uncomp_verts,
	int32_t vtx_count,
	uint8_t const *idx_bytes,
	int32_t tri_count,
	char const *shader_hint,
	float scale_factor,
	float z_offset)
{
	uint32_t uncomp_sz;
	uint32_t comp_sz;
	uint32_t idx_sz;
	void *hw_verts;
	void *hw_tris;
	int v, c;

	/* Strip trailing 0xFFFF padding indices (from .gbxmodel 3-index model_triangle blocks) */
	if (idx_bytes && vtx_count >= 3)
	{
		uint16_t const *raw_idx = (uint16_t const *)idx_bytes;
		while (tri_count > 1 && raw_idx[tri_count + 1] >= (uint16_t)vtx_count)
			tri_count--;
	}
	if (vtx_count < 3 || tri_count < 1)
		return;

	uncomp_sz = (uint32_t)vtx_count * 68u;
	comp_sz = (uint32_t)vtx_count * 32u;
	idx_sz = ((uint32_t)tri_count + 2u) * 2u;

	for (v = 0; v < vtx_count; v++)
	{
		uint8_t *vp = uncomp_verts + (uint32_t)v * 68u;
		if (scale_factor != 1.0f || z_offset != 0.0f)
		{
			*(float *)(vp + 0) *= scale_factor;
			*(float *)(vp + 4) *= scale_factor;
			*(float *)(vp + 8) = (*(float *)(vp + 8) * scale_factor) + z_offset;
		}
		for (c = 0; c < 9; c++)
		{
			float val = *(float *)(vp + 12 + c * 4);
			if (!(val >= -1.0f && val <= 1.0f))
				*(float *)(vp + 12 + c * 4) = (c % 3 == 0) ? 1.0f : 0.0f;
		}
		*(int16_t *)(vp + 56) = 0;
		*(int16_t *)(vp + 58) = 0;
		*(float *)(vp + 60) = 1.0f;
		*(float *)(vp + 64) = 0.0f;
	}

	hw_verts = XPhysicalAlloc(comp_sz, (unsigned long)-1, 0, PAGE_READWRITE);
	if (!hw_verts)
		hw_verts = malloc(comp_sz);
	hw_tris = XPhysicalAlloc(idx_sz, (unsigned long)-1, 0, PAGE_READWRITE);
	if (!hw_tris)
		hw_tris = malloc(idx_sz);
	if (!hw_verts || !hw_tris)
		return;

	memset(dst_part, 0, sizeof(*dst_part));
	dst_part->u_scale = 1.0f;
	dst_part->v_scale = 1.0f;
	if (shader_hint && shader_hint[0])
	{
		char const *bar1 = strchr(shader_hint, '|');
		char const *bar2 = bar1 ? strchr(bar1 + 1, '|') : NULL;
		if (bar2)
		{
			float us = 1.0f, vs = 1.0f;
			size_t copy_len = (size_t)(bar2 - shader_hint);
			if (copy_len >= sizeof(dst_part->shader_hint))
				copy_len = sizeof(dst_part->shader_hint) - 1;
			memcpy(dst_part->shader_hint, shader_hint, copy_len);
			dst_part->shader_hint[copy_len] = '\0';
			if (sscanf(bar2 + 1, "%f,%f", &us, &vs) == 2)
			{
				if (us > 0.0001f && us < 10000.0f)
					dst_part->u_scale = us;
				if (vs > 0.0001f && vs < 10000.0f)
					dst_part->v_scale = vs;
			}
		}
		else
		{
			snprintf(dst_part->shader_hint, sizeof(dst_part->shader_hint), "%s", shader_hint);
		}
	}

	rasterizer_geometry_compress_vertices(
		_rasterizer_vertex_type_model_uncompressed,
		vtx_count,
		hw_verts,
		(long)comp_sz,
		uncomp_verts,
		(long)uncomp_sz);
	memcpy(hw_tris, idx_bytes, idx_sz);
	{
		uint16_t *tris_u16 = (uint16_t *)hw_tris;
		int32_t total_idx = tri_count + 2;
		int32_t k;
		for (k = 0; k < total_idx; k++)
		{
			if (tris_u16[k] >= (uint16_t)vtx_count)
				tris_u16[k] = (k > 0 && tris_u16[k - 1] < (uint16_t)vtx_count) ? tris_u16[k - 1] : 0u;
		}
	}

	rasterizer_vertex_buffer_new(
		&dst_part->vertex_buffer,
		_rasterizer_vertex_type_model_compressed,
		vtx_count,
		hw_verts,
		(long)comp_sz);

	rasterizer_triangle_buffer_new(
		&dst_part->triangle_buffer,
		_triangle_buffer_type_precompiled_strip,
		tri_count,
		hw_tris);
}

static int load_hw_parts_from_ftag_file(
	char const *ftag_path,
	struct forge_hw_part *out_parts,
	int max_parts,
	int *out_part_count,
	float *out_radius,
	float scale_factor,
	float z_offset)
{
	FILE *f = fopen(ftag_path, "rb");
	char magic[4];
	uint32_t version = 0, grp = 0, part_count = 0, p;
	float radius = 1.5f;
	char tag_name[256];

	if (!f)
		return 0;
	if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "FTAG", 4) != 0)
	{
		fclose(f);
		return 0;
	}
	if (fread(&version, 4, 1, f) != 1 || version != 1 ||
		fread(&grp, 4, 1, f) != 1 ||
		fread(&radius, 4, 1, f) != 1 ||
		fread(tag_name, 1, 256, f) != 256 ||
		fread(&part_count, 4, 1, f) != 1)
	{
		fclose(f);
		return 0;
	}

	*out_part_count = 0;
	if (out_radius)
		*out_radius = (radius > 0.1f && radius < 100.0f) ? radius : 1.5f;

	for (p = 0; p < part_count && *out_part_count < max_parts; p++)
	{
		int32_t vtx_count = 0, tri_count = 0;
		char shader_hint[128];
		uint32_t uncomp_sz, idx_sz;
		uint8_t *uncomp_verts;
		uint8_t *idx_bytes;

		if (fread(&vtx_count, 4, 1, f) != 1 ||
			fread(&tri_count, 4, 1, f) != 1 ||
			fread(shader_hint, 1, 128, f) != 128)
		{
			break;
		}
		if (vtx_count <= 0 || tri_count <= 0 || vtx_count > 32768 || tri_count > 32768)
			break;

		uncomp_sz = (uint32_t)vtx_count * 68u;
		idx_sz = ((uint32_t)tri_count + 2u) * 2u;
		uncomp_verts = (uint8_t *)malloc(uncomp_sz);
		idx_bytes = (uint8_t *)malloc(idx_sz);
		if (!uncomp_verts || !idx_bytes)
		{
			free(uncomp_verts);
			free(idx_bytes);
			break;
		}
		if (fread(uncomp_verts, 1, uncomp_sz, f) != uncomp_sz ||
			fread(idx_bytes, 1, idx_sz, f) != idx_sz)
		{
			free(uncomp_verts);
			free(idx_bytes);
			break;
		}

		build_hw_part_from_arrays(
			&out_parts[(*out_part_count)++],
			uncomp_verts,
			vtx_count,
			idx_bytes,
			tri_count,
			shader_hint,
			scale_factor,
			z_offset);

		free(uncomp_verts);
		free(idx_bytes);
	}

	fclose(f);
	return (*out_part_count > 0) ? 1 : 0;
}

static long append_runtime_tag_instance(
	long group_tag,
	long parent0,
	long parent1,
	char const *tag_path,
	void *base_address)
{
	long existing_count = 0;
	struct forge_cache_file_tag_instance *instances;
	struct forge_cache_file_tag_instance *grown;
	long max_salt = 0xE174;
	long i, new_abs_idx, new_tag_index;
	char *name_copy;

	instances = (struct forge_cache_file_tag_instance *)cache_files_tag_instances(&existing_count);
	if (!instances || existing_count <= 0 || existing_count >= 65000)
		return NONE;

	if (!s_dyn_tags.saved_base_instances)
	{
		s_dyn_tags.saved_base_instances = instances;
		s_dyn_tags.saved_base_count = existing_count;
	}

	grown = (struct forge_cache_file_tag_instance *)malloc((size_t)(existing_count + 1) * sizeof(*grown));
	name_copy = (char *)malloc(strlen(tag_path) + 1);
	if (!grown || !name_copy)
	{
		free(grown);
		free(name_copy);
		return NONE;
	}
	memcpy(grown, instances, (size_t)existing_count * sizeof(*grown));
	strcpy(name_copy, tag_path);

	for (i = 0; i < existing_count; i++)
	{
		if (instances[i].tag_index != NONE)
		{
			long s = (long)(((unsigned long)instances[i].tag_index >> 16) & 0x7FFFu);
			if (s > max_salt)
				max_salt = s;
		}
	}

	new_abs_idx = existing_count;
	new_tag_index = ((max_salt + 1) << 16) | (new_abs_idx & 0xFFFF);

	memset(&grown[new_abs_idx], 0, sizeof(grown[new_abs_idx]));
	grown[new_abs_idx].group_tag = group_tag;
	grown[new_abs_idx].parent_group_tags[0] = parent0;
	grown[new_abs_idx].parent_group_tags[1] = parent1;
	grown[new_abs_idx].tag_index = new_tag_index;
	grown[new_abs_idx].name = name_copy;
	grown[new_abs_idx].base_address = base_address;

	cache_files_set_tag_instances(grown, existing_count + 1);
	if (s_dyn_tags.allocated_instances)
		free(s_dyn_tags.allocated_instances);
	s_dyn_tags.allocated_instances = grown;

	return new_tag_index;
}

uint32_t forge_get_base_map_tag_count(void)
{
	long existing_count = 0;
	if (s_dyn_tags.saved_base_count > 0)
		return (uint32_t)s_dyn_tags.saved_base_count;
	if (cache_files_tag_instances(&existing_count) && existing_count > 0)
		return (uint32_t)existing_count;
	return 0;
}

static int is_valid_cyborg_driver_seat_label(char const *label)
{
	if (!label || !label[0])
		return 0;
	if (_stricmp(label, "W-driver") == 0 ||
		_stricmp(label, "G-driver") == 0 ||
		_stricmp(label, "B-driver") == 0 ||
		_stricmp(label, "S-driver") == 0 ||
		_stricmp(label, "R-driver") == 0 ||
		_stricmp(label, "scorpion-driver") == 0 ||
		_stricmp(label, "CT-driver") == 0)
		return 1;
	return 0;
}

static void patch_vehicle_tag_for_player_driving(long vehi_tag_idx, char const *tag_path)
{
	struct unit_definition *ud;

	if (vehi_tag_idx == NONE)
		return;

	ud = (struct unit_definition *)tag_get('vehi', vehi_tag_idx);
	if (ud)
	{
		ud->unit.flags |= FLAG(_unit_has_entrance_points_inside_bounding_sphere_bit);
		if (ud->unit.seats.count > 0 && ud->unit.seats.address)
		{
			struct unit_seat *seat0 = (struct unit_seat *)ud->unit.seats.address;
			seat0->flags &= ~FLAG(_unit_seat_invisible_bit);
			seat0->flags |= FLAG(_unit_seat_driver_bit) |
							FLAG(_unit_seat_third_person_camera_bit) |
							FLAG(_unit_seat_third_person_on_enter_bit);
			if (tag_path && strstr(tag_path, "wraith"))
				seat0->flags |= FLAG(_unit_seat_gunner_bit);
			if (!is_valid_cyborg_driver_seat_label(seat0->label))
				snprintf(seat0->label, sizeof(seat0->label), "W-driver");
		}
	}
}

static void patch_light_definition_buffer(uint8_t *lbuf)
{
	if (!lbuf)
		return;
	/* Bit 0 (_light_definition_dynamic_bit = 1) must be set and radius_modifier_upper_bound > 0
	 * for Halo's object_lights.c to illuminate surrounding world geometry and objects! */
	*(uint32_t *)(lbuf + 0x00) |= 1u;
	if (*(float const *)(lbuf + 0x04) < 3.5f)
		*(float *)(lbuf + 0x04) = 4.5f;
	if (*(float const *)(lbuf + 0x08) == 0.0f && *(float const *)(lbuf + 0x0C) == 0.0f)
	{
		*(float *)(lbuf + 0x08) = 1.0f;
		*(float *)(lbuf + 0x0C) = 1.0f;
	}
	if (*(float const *)(lbuf + 0x4C) == 0.0f &&
		*(float const *)(lbuf + 0x50) == 0.0f &&
		*(float const *)(lbuf + 0x54) == 0.0f)
	{
		*(float *)(lbuf + 0x38) = 1.0f;
		*(float *)(lbuf + 0x3C) = 1.0f;
		*(float *)(lbuf + 0x40) = 1.0f;
		*(float *)(lbuf + 0x44) = 1.0f;
		*(float *)(lbuf + 0x48) = 1.0f;
		*(float *)(lbuf + 0x4C) = 1.0f;
		*(float *)(lbuf + 0x50) = 1.0f;
		*(float *)(lbuf + 0x54) = 1.0f;
	}
}

static void patch_forge_device_and_light_tag_runtime(long tag_idx, uint32_t grp, char const *tag_path)
{
	uint8_t *obuf;
	int32_t att_cnt, a;
	uint8_t *att_base;

	(void)tag_path;
	if (tag_idx == NONE)
		return;
	if (grp != 'scen' && grp != 'mach' && grp != 'lifi' && grp != 'ctrl')
		return;

	obuf = (uint8_t *)tag_get((long)grp, tag_idx);
	if (!obuf)
		return;

	/* Ensure point lights and fixtures have a non-zero bounding radius so Forge raycasting and visibility work */
	if (*(float const *)(obuf + 0x04u) < 0.45f)
		*(float *)(obuf + 0x04u) = 0.45f;
	if (*(float const *)(obuf + 0x104u) < 1.5f)
		*(float *)(obuf + 0x104u) = 1.5f;

	if (grp == 'mach' && tag_path &&
		(_stricmp(tag_path, "levels\\a30\\devices\\beam emitter\\beam emitter") == 0 ||
		 _stricmp(tag_path, "levels\\a30\\devices\\beam emitter\\beam emitter red") == 0))
	{
		*(float *)(obuf + 0x04u) = 65.0f;  /* bounding_radius */
		*(float *)(obuf + 0x10u) = 0.0f;   /* bounding_offset.z */
		*(float *)(obuf + 0x104u) = 65.0f; /* render_bounding_radius */
	}

	/* Patch attached 'ligh' tags so placed lights, spotlights, and point lights cast dynamic illumination */
	att_cnt = *(int32_t const *)(obuf + 0x138u);
	att_base = *(uint8_t *const *)(obuf + 0x13Cu);
	for (a = 0; a < att_cnt && att_base; a++)
	{
		uint8_t *att = att_base + (size_t)a * 72u;
		uint32_t agrp = *(uint32_t const *)(att + 0x00u);
		int32_t aidx = *(int32_t const *)(att + 0x0Cu);
		if (agrp == 'ligh' && aidx != NONE)
		{
			uint8_t *lbuf = (uint8_t *)tag_get('ligh', aidx);
			if (lbuf)
				patch_light_definition_buffer(lbuf);
		}
	}

	/* Configure 'mach' doors to open/close automatically when players or vehicles approach */
	if (grp == 'mach')
	{
		int16_t mtype = *(int16_t const *)(obuf + 0x290u);
		if (mtype == 0 /* _machine_type_door */ ||
			(tag_path && (strstr(tag_path, "door") != NULL || strstr(tag_path, "Door") != NULL || strstr(tag_path, "DOOR") != NULL)))
		{
			*(int16_t *)(obuf + 0x290u) = 0;    /* _machine_type_door */
			*(float *)(obuf + 0x21Cu) = 2.5f;   /* device.automatic_activation_radius */
			*(uint16_t *)(obuf + 0x292u) |= 0x0003u; /* _machine_is_pathfinding_obstacle_bit | _machine_is_not_pathfinding_obstacle_when_open_bit */
			*(float *)(obuf + 0x294u) = 3.0f;   /* machine.door_open_time */
			*(int32_t *)(obuf + 0x320u) = 90;   /* machine.runtime_door_open_ticks (3.0f * 30) */
		}
	}
}

static long find_native_or_mp_variant_tag(uint32_t group_tag, char const *tag_path)
{
	long idx;
	long i;

	idx = tag_loaded((long)group_tag, tag_path);
	if (idx != NONE)
	{
		if (group_tag == 'vehi')
			patch_vehicle_tag_for_player_driving(idx, tag_path);
		else if (group_tag == 'scen' || group_tag == 'mach' || group_tag == 'lifi' || group_tag == 'ctrl')
			patch_forge_device_and_light_tag_runtime(idx, group_tag, tag_path);
		return idx;
	}

	/* Map between SP and MP vehicle/weapon names natively present in MP maps */
	if (group_tag == 'vehi')
	{
		static struct { char const *a; char const *b; } const vehi_pairs[] = {
			{ "vehicles\\warthog\\warthog", "vehicles\\warthog\\mp_warthog" },
			{ "vehicles\\ghost\\ghost", "vehicles\\ghost\\ghost_mp" },
			{ "vehicles\\scorpion\\scorpion", "vehicles\\scorpion\\scorpion_mp" },
			{ "vehicles\\banshee\\banshee", "vehicles\\banshee\\banshee_mp" },
			{ "vehicles\\c_gun_turret\\c_gun_turret", "vehicles\\c_gun_turret\\c_gun_turret_mp" },
			{ "vehicles\\c gun turret\\c gun turret", "vehicles\\c gun turret\\c gun turret_mp" },
			{ "vehicles\\c_gun_turret\\c_gun_turret", "vehicles\\c gun turret\\c gun turret_mp" },
			{ "vehicles\\c gun turret\\c gun turret", "vehicles\\c_gun_turret\\c_gun_turret_mp" }
		};
		for (i = 0; i < (long)(sizeof(vehi_pairs) / sizeof(vehi_pairs[0])); i++)
		{
			if (_stricmp(tag_path, vehi_pairs[i].a) == 0)
			{
				idx = tag_loaded('vehi', vehi_pairs[i].b);
				if (idx != NONE) return idx;
			}
			else if (_stricmp(tag_path, vehi_pairs[i].b) == 0)
			{
				idx = tag_loaded('vehi', vehi_pairs[i].a);
				if (idx != NONE) return idx;
			}
		}
	}
	return NONE;
}

static struct tag_schema_group const *forge_runtime_schema_group_get(uint32_t group_tag)
{
	struct tag_schema_group const *const *list;
	struct tag_schema_group const *group;

	if (group_tag == 'mod2')
	{
		for (group = tag_schema_custom_edition_groups; group->group_tag != 0; group++)
		{
			if (group->group_tag == 'mod2')
				return group;
		}
	}

	for (list = tag_schema_group_lists; *list != NULL; list++)
	{
		for (group = *list; group->group_tag != 0; group++)
		{
			if (group->group_tag == (unsigned long)group_tag)
				return group;
		}
	}
	return NULL;
}

static long load_native_ocetags_package_runtime(uint32_t req_group, char const *req_path, uint32_t *out_actual_group);

struct xbox_runtime_closure
{
	uint8_t const *tag_buf;
	uint32_t tag_sz;
	uint32_t tag_base;
	uint32_t inst_off;
	int32_t tag_count;
	uint8_t *visited;
	int32_t *order;
	int32_t order_count;
	long *runtime_handles;
	uint8_t **cloned_roots;
	int is_red_beam_variant;
};

static void xbox_collect_schema_dependencies(
	struct xbox_runtime_closure *cl,
	struct tag_schema_definition const *def,
	uint32_t base_off,
	int32_t elem_count,
	int depth);

static void xbox_visit_tag_index(struct xbox_runtime_closure *cl, int32_t abs_idx)
{
	uint32_t io, grp, name_addr, data_addr;
	char const *tname;
	struct tag_schema_group const *schema;

	if (abs_idx < 0 || abs_idx >= cl->tag_count)
		return;
	if (cl->visited[abs_idx])
		return;
	cl->visited[abs_idx] = 1;

	io = cl->inst_off + (uint32_t)abs_idx * 32u;
	if (io + 32u > cl->tag_sz)
		return;
	grp = *(uint32_t const *)(cl->tag_buf + io + 0x00);
	name_addr = *(uint32_t const *)(cl->tag_buf + io + 0x10);
	data_addr = *(uint32_t const *)(cl->tag_buf + io + 0x14);
	if (name_addr < cl->tag_base || (name_addr - cl->tag_base) >= cl->tag_sz)
		return;
	tname = (char const *)(cl->tag_buf + (name_addr - cl->tag_base));

	if (grp == 'udlg' || grp == 'sbsp' || grp == 'scnr')
	{
		cl->runtime_handles[abs_idx] = NONE;
		return;
	}

	/* Strictly keep Xbox tags using Xbox bitmaps, shaders, models, and sounds from their source Xbox .map!
	 * Only reuse non-visual/non-audio shared dependencies if we are on a stock Xbox map (never on a CE map). */
	if (cl->order_count > 0 && !custom_edition_cache_tags_loaded() && !cl->is_red_beam_variant &&
		grp != 'bitm' && grp != 'mode' && grp != 'snd!' && grp != 'lsnd' &&
		grp != 'shdr' && grp != 'senv' && grp != 'soso' && grp != 'sotr' &&
		grp != 'schi' && grp != 'scex' && grp != 'swat' && grp != 'sgla' &&
		grp != 'smet' && grp != 'spla')
	{
		long ex = tag_loaded((long)grp, tname);
		if (ex != NONE)
		{
			cl->runtime_handles[abs_idx] = ex;
			return;
		}
	}

	if (data_addr < cl->tag_base || (data_addr - cl->tag_base) >= cl->tag_sz)
		return;

	schema = forge_runtime_schema_group_get(grp);
	if (!schema || !schema->definition || schema->definition->size <= 0)
		return;
	if ((data_addr - cl->tag_base) + (uint32_t)schema->definition->size > cl->tag_sz)
		return;

	cl->order[cl->order_count++] = abs_idx;
	xbox_collect_schema_dependencies(cl, schema->definition, data_addr - cl->tag_base, 1, 1);
}

static void xbox_collect_schema_dependencies(
	struct xbox_runtime_closure *cl,
	struct tag_schema_definition const *def,
	uint32_t base_off,
	int32_t elem_count,
	int depth)
{
	int32_t elem_idx;
	struct tag_schema_field const *field;
	uint32_t elem_size;

	if (!def || !def->fields || elem_count <= 0 || def->size <= 0 || depth > 10)
		return;
	elem_size = (uint32_t)def->size;
	if ((uint64_t)base_off + (uint64_t)elem_count * elem_size > cl->tag_sz)
		return;

	for (elem_idx = 0; elem_idx < elem_count; elem_idx++)
	{
		uint32_t elem_off = base_off + (uint32_t)elem_idx * elem_size;
		for (field = def->fields; field->type != _tag_schema_terminator; field++)
		{
			int16_t k;
			int16_t fcount = field->count > 0 ? field->count : 1;
			uint32_t fstride = field->size > 0 ? (uint32_t)field->size : 0;

			for (k = 0; k < fcount; k++)
			{
				uint32_t f_off = elem_off + (uint32_t)field->offset + (uint32_t)k * fstride;
				if (field->type == _tag_schema_struct)
				{
					struct tag_schema_definition const *sub_def =
						(struct tag_schema_definition const *)field->definition;
					if (sub_def && f_off + (uint32_t)sub_def->size <= elem_off + elem_size)
						xbox_collect_schema_dependencies(cl, sub_def, f_off, 1, depth + 1);
				}
				else if (field->type == _tag_schema_block)
				{
					struct tag_schema_definition const *child_def =
						(struct tag_schema_definition const *)field->definition;
					if (child_def && child_def->size > 0 && f_off + 12u <= elem_off + elem_size)
					{
						int32_t cnt = *(int32_t const *)(cl->tag_buf + f_off + 0u);
						uint32_t addr = *(uint32_t const *)(cl->tag_buf + f_off + 4u);
						int32_t max_cnt = (field->maximum > 0) ? (int32_t)field->maximum : 32768;
						if (cnt > 0 && cnt <= max_cnt && addr >= cl->tag_base &&
							(uint64_t)(addr - cl->tag_base) + (uint64_t)cnt * (uint32_t)child_def->size <= cl->tag_sz)
						{
							xbox_collect_schema_dependencies(cl, child_def, addr - cl->tag_base, cnt, depth + 1);
						}
					}
				}
				else if (field->type == _tag_schema_reference)
				{
					if (f_off + 16u <= elem_off + elem_size)
					{
						int32_t ref_idx = *(int32_t const *)(cl->tag_buf + f_off + 12u);
						if (ref_idx != -1 && (ref_idx & 0xFFFF) < cl->tag_count)
							xbox_visit_tag_index(cl, ref_idx & 0xFFFF);
					}
				}
			}
		}
	}
}

static void xbox_relocate_schema_elements(
	struct xbox_runtime_closure *cl,
	struct tag_schema_definition const *def,
	uint8_t *dst_base,
	uint32_t src_base_off,
	int32_t elem_count,
	int depth)
{
	int32_t elem_idx;
	struct tag_schema_field const *field;
	uint32_t elem_size;

	if (!def || !def->fields || !dst_base || elem_count <= 0 || def->size <= 0 || depth > 10)
		return;
	elem_size = (uint32_t)def->size;

	for (elem_idx = 0; elem_idx < elem_count; elem_idx++)
	{
		uint8_t *elem_ptr = dst_base + (size_t)elem_idx * elem_size;
		uint32_t src_elem_off = src_base_off + (uint32_t)elem_idx * elem_size;

		for (field = def->fields; field->type != _tag_schema_terminator; field++)
		{
			int16_t k;
			int16_t fcount = field->count > 0 ? field->count : 1;
			uint32_t fstride = field->size > 0 ? (uint32_t)field->size : 0;

			for (k = 0; k < fcount; k++)
			{
				uint32_t f_rel = (uint32_t)field->offset + (uint32_t)k * fstride;
				uint32_t src_f_off = src_elem_off + f_rel;

				if (field->type == _tag_schema_struct)
				{
					struct tag_schema_definition const *sub_def =
						(struct tag_schema_definition const *)field->definition;
					if (sub_def && f_rel + (uint32_t)sub_def->size <= elem_size)
					{
						xbox_relocate_schema_elements(
							cl, sub_def, elem_ptr + f_rel, src_f_off, 1, depth + 1);
					}
				}
				else if (field->type == _tag_schema_block)
				{
					struct tag_schema_definition const *child_def =
						(struct tag_schema_definition const *)field->definition;
					if (child_def && child_def->size > 0 && f_rel + 12u <= elem_size)
					{
						struct tag_block *blk = (struct tag_block *)(elem_ptr + f_rel);
						int32_t cnt = *(int32_t const *)(cl->tag_buf + src_f_off + 0u);
						uint32_t addr = *(uint32_t const *)(cl->tag_buf + src_f_off + 4u);
						uint32_t sub_elem_sz = (uint32_t)child_def->size;
						int32_t max_cnt = (field->maximum > 0) ? (int32_t)field->maximum : 32768;

						blk->definition = NULL;
						if (cnt > 0 && cnt <= max_cnt && addr >= cl->tag_base &&
							(uint64_t)(addr - cl->tag_base) + (uint64_t)cnt * sub_elem_sz <= cl->tag_sz)
						{
							size_t copy_bytes = (size_t)cnt * sub_elem_sz;
							uint8_t *new_sub = (uint8_t *)calloc(1, copy_bytes);
							if (new_sub)
							{
								memcpy(new_sub, cl->tag_buf + (addr - cl->tag_base), copy_bytes);
								blk->count = cnt;
								blk->address = new_sub;
								xbox_relocate_schema_elements(
									cl, child_def, new_sub, addr - cl->tag_base, cnt, depth + 1);
							}
							else
							{
								blk->count = 0;
								blk->address = NULL;
							}
						}
						else
						{
							blk->count = 0;
							blk->address = NULL;
						}
					}
				}
				else if (field->type == _tag_schema_data)
				{
					if (f_rel + 20u <= elem_size)
					{
						struct tag_data *td = (struct tag_data *)(elem_ptr + f_rel);
						int32_t sz = *(int32_t const *)(cl->tag_buf + src_f_off + 0u);
						uint32_t addr = *(uint32_t const *)(cl->tag_buf + src_f_off + 12u);
						int32_t max_sz = (field->maximum > 0) ? (int32_t)field->maximum : (16 * 1024 * 1024);

						td->definition = NULL;
						if (sz > 0 && sz <= max_sz && addr >= cl->tag_base &&
							(uint64_t)(addr - cl->tag_base) + (uint32_t)sz <= cl->tag_sz)
						{
							void *new_td = malloc((size_t)sz);
							if (new_td)
							{
								memcpy(new_td, cl->tag_buf + (addr - cl->tag_base), (size_t)sz);
								td->size = sz;
								td->address = new_td;
							}
							else
							{
								td->size = 0;
								td->address = NULL;
							}
						}
						else
						{
							td->size = 0;
							td->address = NULL;
						}
					}
				}
				else if (field->type == _tag_schema_reference)
				{
					if (f_rel + 16u <= elem_size)
					{
						struct tag_reference *ref = (struct tag_reference *)(elem_ptr + f_rel);
						uint32_t name_addr = *(uint32_t const *)(cl->tag_buf + src_f_off + 4u);
						int32_t ref_idx = *(int32_t const *)(cl->tag_buf + src_f_off + 12u);

						ref->name = NULL;
						ref->name_length = 0;
						if (name_addr >= cl->tag_base && (name_addr - cl->tag_base) < cl->tag_sz)
						{
							char const *rname = (char const *)(cl->tag_buf + (name_addr - cl->tag_base));
							size_t rlen = strlen(rname);
							char *ncopy = (char *)malloc(rlen + 1u);
							if (ncopy)
							{
								memcpy(ncopy, rname, rlen + 1u);
								ref->name = ncopy;
							}
						}
						if (ref_idx != -1 && (ref_idx & 0xFFFF) < cl->tag_count &&
							cl->runtime_handles[ref_idx & 0xFFFF] != NONE)
						{
							long hdl = cl->runtime_handles[ref_idx & 0xFFFF];
							ref->index = hdl;
							ref->group_tag = tag_get_group_tag(hdl);
						}
						else
						{
							ref->index = NONE;
						}
					}
				}
				else if (field->type == _tag_schema_tag_index)
				{
					if (f_rel + 4u <= elem_size)
					{
						int32_t ref_idx = *(int32_t const *)(cl->tag_buf + src_f_off);
						if (ref_idx != -1 && (ref_idx & 0xFFFF) < cl->tag_count &&
							cl->runtime_handles[ref_idx & 0xFFFF] != NONE)
						{
							*(int32_t *)(elem_ptr + f_rel) = cl->runtime_handles[ref_idx & 0xFFFF];
						}
						else
						{
							*(int32_t *)(elem_ptr + f_rel) = NONE;
						}
					}
				}
			}
		}
	}
}

static void runtime_tint_dxt1_blue_to_red(uint8_t *pix, uint32_t byte_count)
{
	uint32_t off;
	if (!pix)
		return;
	for (off = 0; off + 8u <= byte_count; off += 8u)
	{
		uint16_t c0 = *(uint16_t const *)(pix + off + 0u);
		uint16_t c1 = *(uint16_t const *)(pix + off + 2u);
		uint32_t lut = *(uint32_t const *)(pix + off + 4u);
		uint16_t r0 = (uint16_t)((c0 >> 11) & 0x1Fu);
		uint16_t g0 = (uint16_t)((c0 >> 5) & 0x3Fu);
		uint16_t b0 = (uint16_t)(c0 & 0x1Fu);
		uint16_t r1 = (uint16_t)((c1 >> 11) & 0x1Fu);
		uint16_t g1 = (uint16_t)((c1 >> 5) & 0x3Fu);
		uint16_t b1 = (uint16_t)(c1 & 0x1Fu);
		uint16_t nr0 = (b0 > r0) ? b0 : r0;
		uint16_t nb0 = (r0 < b0) ? (uint16_t)(r0 >> 1) : (uint16_t)(b0 >> 1);
		uint16_t ng0 = (uint16_t)(g0 >> 2);
		uint16_t nr1 = (b1 > r1) ? b1 : r1;
		uint16_t nb1 = (r1 < b1) ? (uint16_t)(r1 >> 1) : (uint16_t)(b1 >> 1);
		uint16_t ng1 = (uint16_t)(g1 >> 2);
		uint16_t nc0 = (uint16_t)((nr0 << 11) | (ng0 << 5) | nb0);
		uint16_t nc1 = (uint16_t)((nr1 << 11) | (ng1 << 5) | nb1);

		if (c0 > c1)
		{
			if (nc0 < nc1)
			{
				uint16_t tmp = nc0;
				nc0 = nc1;
				nc1 = tmp;
				lut ^= 0x55555555u;
			}
			else if (nc0 == nc1)
			{
				if (nc0 < 0xFFFFu) nc0++;
				else nc1--;
			}
		}
		else
		{
			if (nc0 > nc1)
			{
				uint16_t tmp = nc0;
				uint32_t new_lut = 0;
				int p;
				nc0 = nc1;
				nc1 = tmp;
				for (p = 0; p < 16; p++)
				{
					uint32_t idx = (lut >> (p * 2)) & 3u;
					if (idx == 0u) idx = 1u;
					else if (idx == 1u) idx = 0u;
					new_lut |= (idx << (p * 2));
				}
				lut = new_lut;
			}
		}
		*(uint16_t *)(pix + off + 0u) = nc0;
		*(uint16_t *)(pix + off + 2u) = nc1;
		*(uint32_t *)(pix + off + 4u) = lut;
	}
}

static long load_native_xbox_tag_from_decompressed_map(
	char const *cache_map_path,
	uint32_t req_group,
	char const *req_path,
	uint32_t *out_actual_group)
{
	FILE *f;
	uint8_t hdr[2048];
	uint32_t tag_off, tag_sz, inst_addr;
	int32_t tag_count, i, root_abs_idx = -1;
	uint8_t *tag_buf;
	struct xbox_runtime_closure cl;
	long root_handle = NONE;
	int is_red_beam_variant = (req_group == 'mach' &&
		_stricmp(req_path, "levels\\a30\\devices\\beam emitter\\beam emitter red") == 0) ? 1 : 0;
	char const *lookup_path = is_red_beam_variant
		? "levels\\a30\\devices\\beam emitter\\beam emitter"
		: req_path;

	f = fopen(cache_map_path, "rb");
	if (!f)
		return NONE;
	if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr))
	{
		fclose(f);
		return NONE;
	}
	if (*(uint32_t const *)(hdr + 0x00) != 0x68656164u || *(uint32_t const *)(hdr + 0x04) != 5u)
	{
		fclose(f);
		return NONE;
	}
	tag_off = *(uint32_t const *)(hdr + 0x10);
	tag_sz = *(uint32_t const *)(hdr + 0x14);
	if (tag_off < 2048u || tag_sz < 64u || tag_sz > 64u * 1024u * 1024u)
	{
		fclose(f);
		return NONE;
	}

	tag_buf = (uint8_t *)malloc(tag_sz);
	if (!tag_buf || fseek(f, (long)tag_off, SEEK_SET) != 0 || fread(tag_buf, 1, tag_sz, f) != tag_sz)
	{
		free(tag_buf);
		fclose(f);
		return NONE;
	}

	inst_addr = *(uint32_t const *)(tag_buf + 0x00);
	tag_count = *(int32_t const *)(tag_buf + 0x0C);
	memset(&cl, 0, sizeof(cl));
	cl.tag_buf = tag_buf;
	cl.tag_sz = tag_sz;
	cl.tag_base = inst_addr - 0x24u;
	cl.inst_off = 0x24u;
	cl.tag_count = tag_count;
	cl.is_red_beam_variant = is_red_beam_variant;

	if (tag_count <= 0 || tag_count > 16384 || cl.inst_off + (uint32_t)tag_count * 32u > tag_sz)
	{
		free(tag_buf);
		fclose(f);
		return NONE;
	}

	/* Locate the requested tag in this Xbox map's tag table */
	for (i = 0; i < tag_count; i++)
	{
		uint32_t io = cl.inst_off + (uint32_t)i * 32u;
		uint32_t grp = *(uint32_t const *)(tag_buf + io + 0x00);
		uint32_t name_addr = *(uint32_t const *)(tag_buf + io + 0x10);
		char const *tname;
		if (name_addr < cl.tag_base || (name_addr - cl.tag_base) >= tag_sz)
			continue;
		tname = (char const *)(tag_buf + (name_addr - cl.tag_base));
		if ((grp == req_group || (req_group == 'mod2' && grp == 'mode')) &&
			_stricmp(tname, lookup_path) == 0)
		{
			root_abs_idx = i;
			break;
		}
	}

	if (root_abs_idx < 0)
	{
		free(tag_buf);
		fclose(f);
		return NONE;
	}

	cl.visited = (uint8_t *)calloc((size_t)tag_count, sizeof(uint8_t));
	cl.order = (int32_t *)calloc((size_t)tag_count, sizeof(int32_t));
	cl.runtime_handles = (long *)malloc((size_t)tag_count * sizeof(long));
	cl.cloned_roots = (uint8_t **)calloc((size_t)tag_count, sizeof(uint8_t *));
	if (!cl.visited || !cl.order || !cl.runtime_handles || !cl.cloned_roots)
	{
		free(cl.visited);
		free(cl.order);
		free(cl.runtime_handles);
		free(cl.cloned_roots);
		free(tag_buf);
		fclose(f);
		return NONE;
	}
	for (i = 0; i < tag_count; i++)
		cl.runtime_handles[i] = NONE;

	/* 1. Collect transitive tag dependency closure via OpenCE's tag_schema_group_lists */
	xbox_visit_tag_index(&cl, root_abs_idx);

	/* 2. Allocate root tag definitions and register them in global_tag_instances */
	for (i = 0; i < cl.order_count; i++)
	{
		int32_t abs_idx = cl.order[i];
		uint32_t io = cl.inst_off + (uint32_t)abs_idx * 32u;
		uint32_t grp = *(uint32_t const *)(tag_buf + io + 0x00);
		uint32_t p0 = *(uint32_t const *)(tag_buf + io + 0x04);
		uint32_t p1 = *(uint32_t const *)(tag_buf + io + 0x08);
		uint32_t name_addr = *(uint32_t const *)(tag_buf + io + 0x10);
		uint32_t data_addr = *(uint32_t const *)(tag_buf + io + 0x14);
		char const *tname = (char const *)(tag_buf + (name_addr - cl.tag_base));
		char reg_name[256];
		struct tag_schema_group const *schema = forge_runtime_schema_group_get(grp);
		size_t root_sz;
		uint8_t *root_buf;

		if (!schema || !schema->definition || schema->definition->size <= 0)
			continue;
		root_sz = (size_t)schema->definition->size;
		root_buf = (uint8_t *)calloc(1, root_sz < 64u ? 64u : root_sz);
		if (!root_buf)
			continue;
		memcpy(root_buf, tag_buf + (data_addr - cl.tag_base), root_sz);
		cl.cloned_roots[abs_idx] = root_buf;
		if (is_red_beam_variant)
		{
			if (abs_idx == root_abs_idx)
				snprintf(reg_name, sizeof(reg_name), "%s", req_path);
			else
				snprintf(reg_name, sizeof(reg_name), "%s__red", tname);
		}
		else if (abs_idx != root_abs_idx && tag_loaded((long)grp, tname) != NONE)
		{
			snprintf(reg_name, sizeof(reg_name), "%s__f%d", tname, s_dyn_tags.count);
		}
		else
		{
			snprintf(reg_name, sizeof(reg_name), "%s", tname);
		}
		cl.runtime_handles[abs_idx] = append_runtime_tag_instance(
			(long)grp,
			schema->parent_group_tags[0] ? (long)schema->parent_group_tags[0] : (long)p0,
			schema->parent_group_tags[1] ? (long)schema->parent_group_tags[1] : (long)p1,
			reg_name,
			root_buf);
	}

	/* 3. Relocate all nested tag_blocks, tag_data, and tag_references */
	for (i = 0; i < cl.order_count; i++)
	{
		int32_t abs_idx = cl.order[i];
		uint32_t io = cl.inst_off + (uint32_t)abs_idx * 32u;
		uint32_t grp = *(uint32_t const *)(tag_buf + io + 0x00);
		uint32_t data_addr = *(uint32_t const *)(tag_buf + io + 0x14);
		struct tag_schema_group const *schema = forge_runtime_schema_group_get(grp);
		if (cl.cloned_roots[abs_idx] && schema && schema->definition)
		{
			xbox_relocate_schema_elements(
				&cl,
				schema->definition,
				cl.cloned_roots[abs_idx],
				data_addr - cl.tag_base,
				1,
				1);
		}
	}

	/* 4. Initialize Xbox 'bitm' hardware textures and 'mode' hardware vertex/triangle buffers using OpenCE's native API */
	for (i = 0; i < cl.order_count; i++)
	{
		int32_t abs_idx = cl.order[i];
		uint32_t io = cl.inst_off + (uint32_t)abs_idx * 32u;
		uint32_t grp = *(uint32_t const *)(tag_buf + io + 0x00);
		uint32_t data_addr = *(uint32_t const *)(tag_buf + io + 0x14);
		uint8_t *root_buf = cl.cloned_roots[abs_idx];

		if (!root_buf || cl.runtime_handles[abs_idx] == NONE)
			continue;

		if (grp == 'bitm')
		{
			struct bitmap_group *bgrp = (struct bitmap_group *)root_buf;
			int b_idx;
			for (b_idx = 0; b_idx < bgrp->bitmaps.count && bgrp->bitmaps.address; b_idx++)
			{
				struct bitmap_data *bmp = (struct bitmap_data *)((uint8_t *)bgrp->bitmaps.address + (size_t)b_idx * 48u);
				long req_bytes;
				uint32_t read_bytes, alloc_bytes;
				uint8_t *pix_mem;
				D3DBaseTexture *tex;

				/* Clear _bitmap_cached_bit (bit 7 = 0x80) so _texture_cache_bitmap_get_hardware_format returns bmp->hardware_format directly! */
				bmp->flags &= (uint16_t)~(1u << 7);
				bmp->tag_index = cl.runtime_handles[abs_idx];
				bmp->cache_block_index = NONE;
				bmp->base_address = NULL;
				bmp->hardware_format = NULL;

				if (bmp->width <= 0 || bmp->height <= 0 || bmp->depth <= 0 || bmp->pixels_offset < 2048)
					continue;
				req_bytes = rasterizer_xbox_bitmap_get_pixel_data_size(bmp);
				if (req_bytes <= 0)
					continue;
				read_bytes = (bmp->pixels_size > 0 && bmp->pixels_size < req_bytes) ? (uint32_t)bmp->pixels_size : (uint32_t)req_bytes;
				alloc_bytes = (uint32_t)req_bytes;
				if ((uint32_t)bmp->pixels_size > alloc_bytes && bmp->pixels_size <= 16 * 1024 * 1024)
					alloc_bytes = (uint32_t)bmp->pixels_size;

				pix_mem = (uint8_t *)forge_phys_alloc(alloc_bytes);
				tex = (D3DBaseTexture *)calloc(1, sizeof(D3DBaseTexture));
				if (!pix_mem || !tex)
				{
					free(tex);
					continue;
				}
				if (fseek(f, (long)bmp->pixels_offset, SEEK_SET) != 0 ||
					fread(pix_mem, 1, read_bytes, f) != read_bytes)
				{
					free(tex);
					continue;
				}
				if (is_red_beam_variant && bmp->format == 14)
				{
					uint32_t name_addr = *(uint32_t const *)(tag_buf + io + 0x10);
					char const *btname = (name_addr >= cl.tag_base && (name_addr - cl.tag_base) < tag_sz)
						? (char const *)(tag_buf + (name_addr - cl.tag_base))
						: "";
					if (strstr(btname, "jackal shield") != NULL || strstr(btname, "static blue") != NULL)
						runtime_tint_dxt1_blue_to_red(pix_mem, read_bytes);
				}
				bmp->base_address = pix_mem;
				halo_custom_edition_texels_channels(bmp->base_address, 0 /* _custom_edition_channels_xbox */);

				/* Exact OpenCE texture_cache_initialize_hardware_format (source/cache/xbox_texture_cache.c:613-647) */
				tex->Data = 0;
				tex->Lock = 0;
				tex->Common = D3DCOMMON_TYPE_TEXTURE | 1;
				if (TEST_FLAG(bmp->flags, 4 /* _bitmap_linear_bit */))
				{
					tex->Format =
						(bitmap_format_to_d3d_linear_format(bmp->format, bmp->flags) << D3DFORMAT_FORMAT_SHIFT) |
						(1 << D3DFORMAT_MIPMAP_SHIFT) |
						(2 << D3DFORMAT_DIMENSION_SHIFT) |
						D3DFORMAT_BORDERSOURCE_COLOR |
						D3DFORMAT_DMACHANNEL_A;
					tex->Size =
						(((bitmap_mipmap_get_row_pitch(bmp, 0) + D3DTEXTURE_PITCH_ALIGNMENT - 1) / D3DTEXTURE_PITCH_ALIGNMENT - 1)
							<< D3DSIZE_PITCH_SHIFT) |
						((bmp->height - 1) << D3DSIZE_HEIGHT_SHIFT) |
						(bmp->width - 1);
				}
				else
				{
					tex->Format =
						(floor_log2(bmp->depth) << D3DFORMAT_PSIZE_SHIFT) |
						(floor_log2(bmp->height) << D3DFORMAT_VSIZE_SHIFT) |
						(floor_log2(bmp->width) << D3DFORMAT_USIZE_SHIFT) |
						(bitmap_format_to_d3d_format(bmp->format, bmp->flags) << D3DFORMAT_FORMAT_SHIFT) |
						((bmp->type == 1 /* _bitmap_type_3d */ ? 3 : 2) << D3DFORMAT_DIMENSION_SHIFT) |
						((rasterizer_xbox_bitmap_get_max_mipmap_count(bmp) + 1) << D3DFORMAT_MIPMAP_SHIFT) |
						(bmp->type == 2 /* _bitmap_type_cube_map */ ? D3DFORMAT_CUBEMAP : 0) |
						D3DFORMAT_BORDERSOURCE_COLOR |
						D3DFORMAT_DMACHANNEL_A;
					tex->Size = 0;
				}
				IDirect3DBaseTexture8_Register(tex, bmp->base_address);
				bmp->hardware_format = tex;
			}
		}
		else if (grp == 'senv' || grp == 'soso' || grp == 'sotr' || grp == 'schi' ||
				 grp == 'scex' || grp == 'swat' || grp == 'sgla' || grp == 'smet' || grp == 'spla')
		{
			int16_t stype = 4;
			if (grp == 'senv') stype = 3;
			else if (grp == 'soso') stype = 4;
			else if (grp == 'sotr') stype = 5;
			else if (grp == 'schi' || grp == 'scex') stype = 6;
			else if (grp == 'swat') stype = 7;
			else if (grp == 'sgla') stype = 8;
			else if (grp == 'smet') stype = 9;
			else if (grp == 'spla') stype = 10;
			*(int16_t *)(root_buf + 0x24u) = stype;
		}
		else if (grp == 'snd!')
		{
			struct sound_definition *sdef = (struct sound_definition *)root_buf;
			int pr_i, pm_i;
			sdef->promotion_sound.index = NONE;
			sdef->promotion_count = 0;
			for (pr_i = 0; pr_i < sdef->pitch_ranges.count && sdef->pitch_ranges.address; pr_i++)
			{
				struct sound_pitch_range *pr = (struct sound_pitch_range *)((uint8_t *)sdef->pitch_ranges.address + (size_t)pr_i * sizeof(struct sound_pitch_range));
				pr->played_permutation_mask = 0xFFFFFFFFu;
				pr->previous_permutation_index = NONE;
				pr->forced_permutation_index = NONE;
				for (pm_i = 0; pm_i < pr->permutations.count && pr->permutations.address; pm_i++)
				{
					struct sound_permutation *perm = (struct sound_permutation *)((uint8_t *)pr->permutations.address + (size_t)pm_i * sizeof(struct sound_permutation));
					perm->unknown0 = NONE;
					perm->unknown1 = 0;
					perm->unknown2 = (unsigned long)cl.runtime_handles[abs_idx];
					perm->unknown3 = (unsigned long)cl.runtime_handles[abs_idx];
					perm->samples.address = NULL;
					if (perm->samples.size > 0 && perm->samples.size <= MAXIMUM_SOUND_DATA_SIZE &&
						perm->samples.file_offset >= 2048)
					{
						void *sbuf = malloc((size_t)perm->samples.size);
						if (sbuf && fseek(f, (long)perm->samples.file_offset, SEEK_SET) == 0 &&
							fread(sbuf, 1, (size_t)perm->samples.size, f) == (size_t)perm->samples.size)
						{
							perm->samples.address = sbuf;
							perm->samples.file_offset = 0;
							forge_register_dyn_sound_perm(perm);
						}
						else
						{
							free(sbuf);
							perm->samples.size = 0;
						}
					}
					else
					{
						perm->samples.size = 0;
					}
				}
			}
		}
		else if (grp == 'mode')
		{
			uint32_t mod_off = data_addr - cl.tag_base;
			int32_t geom_cnt = *(int32_t const *)(root_buf + 0xD0u);
			uint8_t *geom_base = *(uint8_t **)(root_buf + 0xD4u);
			int32_t shdr_cnt = *(int32_t const *)(root_buf + 0xDCu);
			uint8_t *shdr_base = *(uint8_t **)(root_buf + 0xE0u);
			uint32_t src_geom_addr = *(uint32_t const *)(tag_buf + mod_off + 0xD4u);
			int g_idx, s_idx;

			if (!(*(float const *)(root_buf + 0x30u) > 0.0001f && *(float const *)(root_buf + 0x30u) < 10000.0f))
				*(float *)(root_buf + 0x30u) = 1.0f;
			if (!(*(float const *)(root_buf + 0x34u) > 0.0001f && *(float const *)(root_buf + 0x34u) < 10000.0f))
				*(float *)(root_buf + 0x34u) = 1.0f;

			/* Shift beam emitter root node down by 6.5 world units (~10m in Forge HUD) so its flare collar sits at the placement origin */
			if (is_red_beam_variant || _stricmp(req_path, "levels\\a30\\devices\\beam emitter\\beam emitter") == 0)
			{
				int32_t node_cnt = *(int32_t const *)(root_buf + 0xB8u);
				uint8_t *node_base = *(uint8_t **)(root_buf + 0xBCu);
				if (node_cnt > 0 && node_base)
					*(float *)(node_base + 48u) = -6.5f;
			}

			for (s_idx = 0; s_idx < shdr_cnt && shdr_base; s_idx++)
			{
				int16_t *perm_idx_ptr = (int16_t *)(shdr_base + (size_t)s_idx * 32u + 16u);
				if (*perm_idx_ptr < 0)
					*perm_idx_ptr = 0;
			}

			if (geom_cnt > 0 && geom_base && src_geom_addr >= cl.tag_base &&
				(uint64_t)(src_geom_addr - cl.tag_base) + (uint64_t)geom_cnt * 48u <= tag_sz)
			{
				for (g_idx = 0; g_idx < geom_cnt; g_idx++)
				{
					uint8_t *dst_geom = geom_base + (size_t)g_idx * 48u;
					uint32_t src_g_off = (src_geom_addr - cl.tag_base) + (uint32_t)g_idx * 48u;
					int32_t part_cnt = *(int32_t const *)(dst_geom + 36u);
					uint8_t *dst_parts = *(uint8_t **)(dst_geom + 40u);
					uint32_t src_part_addr = *(uint32_t const *)(tag_buf + src_g_off + 40u);
					int p_idx;

					if (part_cnt <= 0 || !dst_parts || src_part_addr < cl.tag_base ||
						(uint64_t)(src_part_addr - cl.tag_base) + (uint64_t)part_cnt * 104u > tag_sz)
						continue;

					for (p_idx = 0; p_idx < part_cnt; p_idx++)
					{
						uint8_t *dst_part = dst_parts + (size_t)p_idx * 104u;
						uint32_t p_off = (src_part_addr - cl.tag_base) + (uint32_t)p_idx * 104u;
						int16_t tri_type = *(int16_t const *)(tag_buf + p_off + 68u);
						int32_t tri_count = *(int32_t const *)(tag_buf + p_off + 72u);
						uint32_t tb_hw_addr = *(uint32_t const *)(tag_buf + p_off + 80u);
						int16_t vtx_type = *(int16_t const *)(tag_buf + p_off + 84u);
						int32_t vtx_count = *(int32_t const *)(tag_buf + p_off + 88u);
						uint32_t vb_hw_addr = *(uint32_t const *)(tag_buf + p_off + 100u);

						memset(dst_part + 68u, 0, 36u);
						((struct vertex_buffer *)(dst_part + 84u))->type = _rasterizer_vertex_type_model_compressed;
						if (tri_count > 0 && vtx_count > 0 &&
							tb_hw_addr >= cl.tag_base && (tb_hw_addr - cl.tag_base) + 12u <= tag_sz &&
							vb_hw_addr >= cl.tag_base && (vb_hw_addr - cl.tag_base) + 12u <= tag_sz)
						{
							uint32_t ib_data_addr = *(uint32_t const *)(tag_buf + (tb_hw_addr - cl.tag_base) + 4u);
							uint32_t vb_data_addr = *(uint32_t const *)(tag_buf + (vb_hw_addr - cl.tag_base) + 4u);
							uint32_t comp_sz = (uint32_t)vtx_count * 32u;
							uint32_t idx_sz = (tri_type == _triangle_buffer_type_precompiled_strip)
								? ((uint32_t)tri_count + 2u) * 2u
								: (uint32_t)tri_count * 6u;

							if (ib_data_addr >= cl.tag_base && (ib_data_addr - cl.tag_base) + idx_sz <= tag_sz &&
								vb_data_addr >= cl.tag_base && (vb_data_addr - cl.tag_base) + comp_sz <= tag_sz)
							{
								void *hw_verts = forge_phys_alloc(comp_sz);
								void *hw_tris = forge_phys_alloc(idx_sz);
								if (hw_verts && hw_tris)
								{
									memcpy(hw_verts, tag_buf + (vb_data_addr - cl.tag_base), comp_sz);
									memcpy(hw_tris, tag_buf + (ib_data_addr - cl.tag_base), idx_sz);
									rasterizer_vertex_buffer_new(
										(struct vertex_buffer *)(dst_part + 84u),
										vtx_type ? vtx_type : _rasterizer_vertex_type_model_compressed,
										vtx_count,
										hw_verts,
										(long)comp_sz);
									rasterizer_triangle_buffer_new(
										(struct triangle_buffer *)(dst_part + 68u),
										tri_type,
										tri_count,
										hw_tris);
								}
							}
						}
					}
				}
			}
			models_fix_transparent_part_links();
		}
		else if (grp == 'vehi')
		{
			struct unit_definition *ud = (struct unit_definition *)root_buf;
			ud->unit.flags |= FLAG(_unit_has_entrance_points_inside_bounding_sphere_bit);
		}
		else if (grp == 'antr')
		{
			/* Clear _animation_world_relative_bit (0x0002) on scenery/character animations (e.g. flood_captain in d20)
			 * so placed objects render at their placed world position rather than teleporting to campaign cutscene coordinates. */
			int32_t anim_cnt = *(int32_t const *)(root_buf + 0x74u);
			uint8_t *anim_base = *(uint8_t **)(root_buf + 0x78u);
			float n0_tx = 0.0f, n0_ty = 0.0f, n0_tz = 0.0f;
			int m_k, a_idx;

			for (m_k = 0; m_k < cl.order_count; m_k++)
			{
				int m_abs = cl.order[m_k];
				uint32_t m_io = cl.inst_off + (uint32_t)m_abs * 32u;
				if (*(uint32_t const *)(tag_buf + m_io + 0x00) == 'mode' && cl.cloned_roots[m_abs])
				{
					uint8_t const *mod_buf = cl.cloned_roots[m_abs];
					int32_t n_cnt = *(int32_t const *)(mod_buf + 0xB8u);
					uint8_t const *n_base = *(uint8_t *const *)(mod_buf + 0xBCu);
					if (n_cnt > 0 && n_base)
					{
						float tx = *(float const *)(n_base + 40u);
						float ty = *(float const *)(n_base + 44u);
						float tz = *(float const *)(n_base + 48u);
						if (tx > -10.0f && tx < 10.0f && ty > -10.0f && ty < 10.0f && tz > -10.0f && tz < 10.0f)
						{
							n0_tx = tx;
							n0_ty = ty;
							n0_tz = tz;
						}
					}
					break;
				}
			}

			for (a_idx = 0; a_idx < anim_cnt && anim_base; a_idx++)
			{
				uint8_t *anim = anim_base + (size_t)a_idx * 180u;
				uint16_t aflags = *(uint16_t const *)(anim + 0x3Au);
				if (aflags & 0x0002u)
				{
					*(uint16_t *)(anim + 0x3Au) = (uint16_t)(aflags & ~0x0002u);
					if (!(aflags & 0x0001u))
					{
						uint32_t trans_flags0 = *(uint32_t const *)(anim + 0x5Cu);
						uint32_t rot_flags0 = *(uint32_t const *)(anim + 0x64u);
						uint32_t n0_trans_off = (rot_flags0 & 1u) ? 0u : 8u;
						if (!(trans_flags0 & 1u))
						{
							int32_t def_sz = *(int32_t const *)(anim + 0x80u);
							uint8_t *def_ptr = *(uint8_t **)(anim + 0x88u);
							if (def_ptr && def_sz >= (int32_t)(n0_trans_off + 12u))
							{
								*(float *)(def_ptr + n0_trans_off + 0u) = n0_tx;
								*(float *)(def_ptr + n0_trans_off + 4u) = n0_ty;
								*(float *)(def_ptr + n0_trans_off + 8u) = n0_tz;
							}
						}
						else
						{
							int16_t frame_cnt = *(int16_t const *)(anim + 0x24u);
							int16_t frame_sz = *(int16_t const *)(anim + 0x26u);
							int32_t fdata_sz = *(int32_t const *)(anim + 0x94u);
							uint8_t *fdata_ptr = *(uint8_t **)(anim + 0x9Cu);
							int f_i;
							for (f_i = 0; f_i < frame_cnt && frame_sz >= (int16_t)(n0_trans_off + 12u) && fdata_ptr; f_i++)
							{
								uint32_t f_off = (uint32_t)f_i * (uint32_t)frame_sz + n0_trans_off;
								if (f_off + 12u <= (uint32_t)fdata_sz)
								{
									*(float *)(fdata_ptr + f_off + 0u) = n0_tx;
									*(float *)(fdata_ptr + f_off + 4u) = n0_ty;
									*(float *)(fdata_ptr + f_off + 8u) = n0_tz;
								}
							}
						}
					}
				}
			}
		}
		else if (grp == 'ligh')
		{
			patch_light_definition_buffer(root_buf);
			if (is_red_beam_variant)
			{
				*(float *)(root_buf + 0x38u) = 1.0f;
				*(float *)(root_buf + 0x3Cu) = 1.0f;
				*(float *)(root_buf + 0x40u) = 0.12f;
				*(float *)(root_buf + 0x44u) = 0.08f;
				*(float *)(root_buf + 0x48u) = 1.0f;
				*(float *)(root_buf + 0x4Cu) = 1.0f;
				*(float *)(root_buf + 0x50u) = 0.18f;
				*(float *)(root_buf + 0x54u) = 0.10f;
			}
		}
		else if (grp == 'lens' && is_red_beam_variant)
		{
			int32_t refl_cnt = *(int32_t const *)(root_buf + 0xC4u);
			uint8_t *refl_base = *(uint8_t *const *)(root_buf + 0xC8u);
			int r_idx;
			for (r_idx = 0; r_idx < refl_cnt && refl_base; r_idx++)
			{
				uint8_t *rf = refl_base + (size_t)r_idx * 128u;
				*(float *)(rf + 0x40u) = 1.0f;
				*(float *)(rf + 0x44u) = 1.0f;
				*(float *)(rf + 0x48u) = 0.12f;
				*(float *)(rf + 0x4Cu) = 0.08f;
				*(float *)(rf + 0x50u) = 1.0f;
				*(float *)(rf + 0x54u) = 1.0f;
				*(float *)(rf + 0x58u) = 0.18f;
				*(float *)(rf + 0x5Cu) = 0.10f;
				*(float *)(rf + 0x60u) = 1.0f;
				*(float *)(rf + 0x64u) = 1.0f;
				*(float *)(rf + 0x68u) = 0.12f;
				*(float *)(rf + 0x6Cu) = 0.08f;
			}
		}
	}

	root_handle = cl.runtime_handles[root_abs_idx];
	if (root_handle != NONE)
	{
		if (req_group == 'vehi')
			patch_vehicle_tag_for_player_driving(root_handle, req_path);
		else if (req_group == 'scen' || req_group == 'mach' || req_group == 'lifi' || req_group == 'ctrl')
			patch_forge_device_and_light_tag_runtime(root_handle, req_group, req_path);
		if (s_dyn_tags.count < FORGE_MAX_DYNAMIC_TAGS)
		{
			struct forge_dynamic_tag_entry *entry = &s_dyn_tags.entries[s_dyn_tags.count++];
			memset(entry, 0, sizeof(*entry));
			entry->group_tag = req_group;
			snprintf(entry->tag_path, sizeof(entry->tag_path), "%s", req_path);
			entry->definition_index = root_handle;
			entry->part_count = 0; /* 0 = rendered natively by Halo's render_model! */
			entry->bounding_radius = cl.cloned_roots[root_abs_idx]
				? *(float const *)(cl.cloned_roots[root_abs_idx] + 0x04u)
				: 1.5f;
			if (!(entry->bounding_radius > 0.1f && entry->bounding_radius < 100.0f))
				entry->bounding_radius = 1.5f;
		}
		if (out_actual_group)
			*out_actual_group = (uint32_t)tag_get_group_tag(root_handle);
	}

	free(cl.visited);
	free(cl.order);
	free(cl.runtime_handles);
	free(cl.cloned_roots);
	free(tag_buf);
	fclose(f);
	return root_handle;
}

static int find_donor_map_stems_from_credits(
	uint32_t req_group,
	char const *req_path,
	char out_stems[][64],
	int max_stems)
{
	FILE *fp;
	char line[512];
	char cur_stem[64];
	char grp_str[5];
	char const *lookup_path;
	int count = 0;

	if (!req_path || !req_path[0] || !out_stems || max_stems <= 0)
		return 0;

	lookup_path = (req_group == 'mach' &&
		_stricmp(req_path, "levels\\a30\\devices\\beam emitter\\beam emitter red") == 0)
		? "levels\\a30\\devices\\beam emitter\\beam emitter"
		: req_path;

	grp_str[0] = (char)((req_group >> 24) & 0xFF);
	grp_str[1] = (char)((req_group >> 16) & 0xFF);
	grp_str[2] = (char)((req_group >> 8) & 0xFF);
	grp_str[3] = (char)(req_group & 0xFF);
	grp_str[4] = '\0';

	fp = fopen("Faber_data/credits.md", "r");
	if (!fp)
		fp = fopen("OpenCE Forge/credits.md", "r");
	if (!fp)
		fp = fopen("port/forge/assets/credits.md", "r");
	if (!fp)
		return 0;

	cur_stem[0] = '\0';
	while (fgets(line, sizeof(line), fp))
	{
		if (strncmp(line, "## Xbox Map: `", 14) == 0)
		{
			char const *p = line + 14;
			char const *q = strchr(p, '`');
			if (q && q > p)
			{
				size_t len = (size_t)(q - p);
				if (len >= sizeof(cur_stem))
					len = sizeof(cur_stem) - 1;
				memcpy(cur_stem, p, len);
				cur_stem[len] = '\0';
			}
		}
		else if (cur_stem[0] && strncmp(line, "- `", 3) == 0)
		{
			char const *p1 = line + 3;
			char const *q1 = strchr(p1, '`');
			if (q1 && q1 > p1)
			{
				char tag_buf[256];
				size_t tlen = (size_t)(q1 - p1);
				char const *p2 = strchr(q1 + 1, '`');
				char const *q2 = p2 ? strchr(p2 + 1, '`') : NULL;
				if (tlen < sizeof(tag_buf) && p2 && q2 && (q2 - (p2 + 1)) == 4)
				{
					char gbuf[5];
					memcpy(tag_buf, p1, tlen);
					tag_buf[tlen] = '\0';
					memcpy(gbuf, p2 + 1, 4);
					gbuf[4] = '\0';
					if (_stricmp(tag_buf, lookup_path) == 0 &&
						(_stricmp(gbuf, grp_str) == 0 || (req_group == 'mod2' && _stricmp(gbuf, "mode") == 0)))
					{
						int i, dup = 0;
						for (i = 0; i < count; i++)
						{
							if (_stricmp(out_stems[i], cur_stem) == 0)
							{
								dup = 1;
								break;
							}
						}
						if (!dup && count < max_stems)
						{
							snprintf(out_stems[count++], 64, "%s", cur_stem);
							if (count >= max_stems)
								break;
						}
					}
				}
			}
		}
	}
	fclose(fp);
	return count;
}

static long load_native_xbox_tag_runtime(
	uint32_t req_group,
	char const *req_path,
	uint32_t *out_actual_group)
{
#ifdef _WIN32
	static char const *const s_xbox_map_dirs[] = {
		"maps",
		"FORGE CODE/GAME MAPS/XBOX MAPS",
		"../FORGE CODE/GAME MAPS/XBOX MAPS"
	};
	char candidate_stems[8][64];
	int stem_count, s, slot, d;

	if (!req_path || !req_path[0])
		return NONE;

	/* 1. Check OpenCE's Xbox hard-drive cache (z:\cache000.map .. z:\cache005.map and z:\cache_donor.map) */
	for (slot = 0; slot <= 6; slot++)
	{
		char z_xbox[64];
		char z_path[512];
		long hdl;
		if (slot < 6)
			snprintf(z_xbox, sizeof(z_xbox), "z:\\cache00%d.map", slot);
		else
			snprintf(z_xbox, sizeof(z_xbox), "z:\\cache_donor.map");
		platform_translate_path(z_xbox, z_path, sizeof(z_path));
		hdl = load_native_xbox_tag_from_decompressed_map(z_path, req_group, req_path, out_actual_group);
		if (hdl != NONE)
			return hdl;
	}

	/* 2. Check exact donor map(s) indexed in Faber_data/credits.md first so we never decompress unrelated maps */
	stem_count = find_donor_map_stems_from_credits(req_group, req_path, candidate_stems, 8);
	for (s = 0; s < stem_count; s++)
	{
		for (d = 0; d < (int)(sizeof(s_xbox_map_dirs) / sizeof(s_xbox_map_dirs[0])); d++)
		{
			char full_path[512];
			char cache_path[512];
			snprintf(full_path, sizeof(full_path), "%s/%s.map", s_xbox_map_dirs[d], candidate_stems[s]);
			if (is_xbox_version5_map_file(full_path) &&
				ensure_xbox_map_decompressed_cache(full_path, cache_path, sizeof(cache_path)))
			{
				long hdl = load_native_xbox_tag_from_decompressed_map(cache_path, req_group, req_path, out_actual_group);
				if (hdl != NONE)
					return hdl;
			}
		}
	}

	/* 3. Fallback: scan Xbox maps in maps/ */
	for (d = 0; d < (int)(sizeof(s_xbox_map_dirs) / sizeof(s_xbox_map_dirs[0])); d++)
	{
		char pattern[512];
		WIN32_FIND_DATAA fd;
		HANDLE h;
		snprintf(pattern, sizeof(pattern), "%s\\*.map", s_xbox_map_dirs[d]);
		h = FindFirstFileA(pattern, &fd);
		if (h != INVALID_HANDLE_VALUE)
		{
			do
			{
				char full_path[512];
				char cache_path[512];
				if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					continue;
				if (_stricmp(fd.cFileName, "bitmaps.map") == 0 ||
					_stricmp(fd.cFileName, "sounds.map") == 0 ||
					_stricmp(fd.cFileName, "loc.map") == 0 ||
					_stricmp(fd.cFileName, "ui.map") == 0)
					continue;
				snprintf(full_path, sizeof(full_path), "%s/%s", s_xbox_map_dirs[d], fd.cFileName);
				if (is_xbox_version5_map_file(full_path) &&
					ensure_xbox_map_decompressed_cache(full_path, cache_path, sizeof(cache_path)))
				{
					long hdl = load_native_xbox_tag_from_decompressed_map(cache_path, req_group, req_path, out_actual_group);
					if (hdl != NONE)
					{
						CloseHandle(h);
						return hdl;
					}
				}
			} while (FindNextFileA(h, &fd));
			CloseHandle(h);
		}
	}
#else
	(void)req_group;
	(void)req_path;
	(void)out_actual_group;
#endif
	return NONE;
}

static long register_dynamic_cached_tag(uint32_t group_tag, char const *tag_path, uint32_t *out_actual_group)
{
	int i;
	long native_idx;

	/* Check if already registered (or negative-cached) for the current map */
	for (i = 0; i < s_dyn_tags.count; i++)
	{
		if (s_dyn_tags.entries[i].group_tag == group_tag &&
			_stricmp(s_dyn_tags.entries[i].tag_path, tag_path) == 0)
		{
			if (s_dyn_tags.entries[i].definition_index == NONE)
				return NONE;
			if (out_actual_group)
				*out_actual_group = (uint32_t)tag_get_group_tag(s_dyn_tags.entries[i].definition_index);
			return s_dyn_tags.entries[i].definition_index;
		}
	}

	if (s_dyn_tags.count >= FORGE_MAX_DYNAMIC_TAGS)
		return NONE;

	/* 1. Check if an exact .ocetags package exists in custom_tags/ and load its complete tag tree natively! */
	native_idx = load_native_ocetags_package_runtime(group_tag, tag_path, out_actual_group);
	if (native_idx != NONE)
		return native_idx;

	/* 2. Load default Xbox tag natively from Xbox map cache using OpenCE's exact tag schema & hardware format! */
	native_idx = load_native_xbox_tag_runtime(group_tag, tag_path, out_actual_group);
	if (native_idx != NONE)
		return native_idx;

	/* 3. Negative-cache missing tag so subsequent frames do not re-scan disk */
	if (s_dyn_tags.count < FORGE_MAX_DYNAMIC_TAGS)
	{
		struct forge_dynamic_tag_entry *neg = &s_dyn_tags.entries[s_dyn_tags.count++];
		memset(neg, 0, sizeof(*neg));
		neg->group_tag = group_tag;
		snprintf(neg->tag_path, sizeof(neg->tag_path), "%s", tag_path);
		neg->definition_index = NONE;
	}

	return NONE;
}

long forge_resolve_engine_tag(uint32_t group_tag, char const *tag_path, uint32_t *out_actual_group)
{
	long idx;
	int i;

	if (out_actual_group)
		*out_actual_group = group_tag;
	if (!tag_path || !tag_path[0])
		return NONE;

	/* 0. If already dynamically registered (or negative-cached) for this map, return it directly */
	for (i = 0; i < s_dyn_tags.count; i++)
	{
		if (s_dyn_tags.entries[i].group_tag == group_tag &&
			_stricmp(s_dyn_tags.entries[i].tag_path, tag_path) == 0)
		{
			if (s_dyn_tags.entries[i].definition_index == NONE)
				return NONE;
			if (out_actual_group)
				*out_actual_group = (uint32_t)tag_get_group_tag(s_dyn_tags.entries[i].definition_index);
			return s_dyn_tags.entries[i].definition_index;
		}
	}

	/* 1. Check if the exact tag (or its native _mp / SP counterpart) is already in the map */
	idx = find_native_or_mp_variant_tag(group_tag, tag_path);
	if (idx != NONE)
	{
		if (group_tag == 'vehi')
			patch_vehicle_tag_for_player_driving(idx, tag_path);
		return idx;
	}

	/* 2. Dynamically load from custom_tags\*.ocetags or Xbox map cache into global_tag_instances! */
	idx = register_dynamic_cached_tag(group_tag, tag_path, out_actual_group);
	if (idx != NONE)
		return idx;

	return NONE;
}

static int ocetags_json_find_key_range(char const *buf, size_t start, size_t end, char const *key, size_t *out_val_pos)
{
	char pat[128];
	size_t pat_len;
	size_t p;

	snprintf(pat, sizeof(pat), "\"%s\"", key);
	pat_len = strlen(pat);
	if (end <= start || end - start < pat_len)
		return 0;
	for (p = start; p + pat_len <= end; p++)
	{
		if (memcmp(buf + p, pat, pat_len) == 0)
		{
			size_t q = p + pat_len;
			while (q < end && (buf[q] == ' ' || buf[q] == '\t' || buf[q] == '\r' || buf[q] == '\n'))
				q++;
			if (q < end && buf[q] == ':')
			{
				q++;
				while (q < end && (buf[q] == ' ' || buf[q] == '\t' || buf[q] == '\r' || buf[q] == '\n'))
					q++;
				*out_val_pos = q;
				return 1;
			}
		}
	}
	return 0;
}

static int ocetags_json_get_str(char const *buf, size_t start, size_t end, char const *key, char *out, size_t out_sz)
{
	size_t vpos = 0;
	size_t i = 0;

	if (!out || out_sz == 0)
		return 0;
	out[0] = 0;
	if (!ocetags_json_find_key_range(buf, start, end, key, &vpos))
		return 0;
	if (vpos >= end || buf[vpos] != '"')
		return 0;
	vpos++;
	while (vpos < end && buf[vpos] != '"' && i + 1 < out_sz)
	{
		if (buf[vpos] == '\\' && vpos + 1 < end)
		{
			vpos++;
			if (buf[vpos] == '\\' || buf[vpos] == '"' || buf[vpos] == '/')
				out[i++] = buf[vpos];
			else if (buf[vpos] == 'n')
				out[i++] = '\n';
			else
				out[i++] = buf[vpos];
		}
		else
		{
			out[i++] = buf[vpos];
		}
		vpos++;
	}
	out[i] = 0;
	return 1;
}

static int ocetags_json_get_u32(char const *buf, size_t start, size_t end, char const *key, uint32_t *out_val)
{
	size_t vpos = 0;
	if (!ocetags_json_find_key_range(buf, start, end, key, &vpos))
		return 0;
	if (vpos >= end)
		return 0;
	*out_val = (uint32_t)strtoul(buf + vpos, NULL, 10);
	return 1;
}

static int ocetags_find_matching_brace(char const *buf, size_t open_pos, size_t limit, char open_ch, char close_ch, size_t *out_close)
{
	int depth = 0;
	int in_str = 0;
	size_t p;

	for (p = open_pos; p < limit; p++)
	{
		char c = buf[p];
		if (in_str)
		{
			if (c == '\\' && p + 1 < limit)
				p++;
			else if (c == '"')
				in_str = 0;
		}
		else
		{
			if (c == '"')
				in_str = 1;
			else if (c == open_ch)
				depth++;
			else if (c == close_ch)
			{
				depth--;
				if (depth == 0)
				{
					*out_close = p;
					return 1;
				}
			}
		}
	}
	return 0;
}

static int ocetags_json_skip_value(char const *buf, size_t pos, size_t end, size_t *out_next)
{
	size_t close_pos = 0;
	while (pos < end && (buf[pos] == ' ' || buf[pos] == '\t' || buf[pos] == '\r' || buf[pos] == '\n'))
		pos++;
	if (pos >= end)
		return 0;
	if (buf[pos] == '{')
	{
		if (!ocetags_find_matching_brace(buf, pos, end, '{', '}', &close_pos))
			return 0;
		if (out_next)
			*out_next = close_pos + 1;
		return 1;
	}
	if (buf[pos] == '[')
	{
		if (!ocetags_find_matching_brace(buf, pos, end, '[', ']', &close_pos))
			return 0;
		if (out_next)
			*out_next = close_pos + 1;
		return 1;
	}
	if (buf[pos] == '"')
	{
		pos++;
		while (pos < end)
		{
			if (buf[pos] == '\\' && pos + 1 < end)
				pos += 2;
			else if (buf[pos] == '"')
			{
				pos++;
				break;
			}
			else
				pos++;
		}
		if (out_next)
			*out_next = pos;
		return 1;
	}
	while (pos < end && buf[pos] != ',' && buf[pos] != '}' && buf[pos] != ']' &&
		   buf[pos] != ' ' && buf[pos] != '\t' && buf[pos] != '\r' && buf[pos] != '\n')
		pos++;
	if (out_next)
		*out_next = pos;
	return 1;
}

struct ocetags_chunk_entry
{
	char id[64];
	char group[8];
	char path[256];
	char kind[48];
	uint32_t offset;
	uint32_t size;
	uint32_t vertex_count;
	uint32_t index_count;
	uint32_t width;
	uint32_t height;
	uint32_t format_code;
};

#define OCETAGS_MAX_CHUNKS 256

static uint32_t ocetags_group_str_to_fourcc(char const *grp)
{
	static struct { char const *name; uint32_t fourcc; } const s_map[] = {
		{ "actor", 'actr' },
		{ "actor_variant", 'actv' },
		{ "antenna", 'ant!' },
		{ "model_animations", 'antr' },
		{ "biped", 'bipd' },
		{ "bitmap", 'bitm' },
		{ "continuous_damage_effect", 'cdmg' },
		{ "model_collision_geometry", 'coll' },
		{ "color_table", 'colo' },
		{ "contrail", 'cont' },
		{ "device_control", 'ctrl' },
		{ "decal", 'deca' },
		{ "ui_widget_definition", 'DeLa' },
		{ "input_device_defaults", 'devc' },
		{ "device", 'devi' },
		{ "detail_object_collection", 'dobc' },
		{ "effect", 'effe' },
		{ "equipment", 'eqip' },
		{ "flag", 'flag' },
		{ "fog", 'fog ' },
		{ "font", 'font' },
		{ "material_effects", 'foot' },
		{ "garbage", 'garb' },
		{ "glow", 'glw!' },
		{ "grenade_hud_interface", 'grhi' },
		{ "hud_message_text", 'hmt ' },
		{ "hud_number", 'hud#' },
		{ "hud_globals", 'hudg' },
		{ "item", 'item' },
		{ "item_collection", 'itmc' },
		{ "damage_effect", 'jpt!' },
		{ "lens_flare", 'lens' },
		{ "lightning", 'elec' },
		{ "device_light_fixture", 'lifi' },
		{ "light", 'ligh' },
		{ "sound_looping", 'lsnd' },
		{ "device_machine", 'mach' },
		{ "globals", 'matg' },
		{ "meter", 'metr' },
		{ "light_volume", 'mgs2' },
		{ "gbxmodel", 'mod2' },
		{ "model", 'mode' },
		{ "multiplayer_scenario_description", 'mply' },
		{ "preferences_network_game", 'ngpr' },
		{ "object", 'obje' },
		{ "particle", 'part' },
		{ "particle_system", 'pctl' },
		{ "physics", 'phys' },
		{ "placeholder", 'plac' },
		{ "point_physics", 'pphy' },
		{ "projectile", 'proj' },
		{ "weather_particle_system", 'rain' },
		{ "scenario_structure_bsp", 'sbsp' },
		{ "scenery", 'scen' },
		{ "shader_transparent_chicago_extended", 'scex' },
		{ "shader_transparent_chicago", 'schi' },
		{ "scenario", 'scnr' },
		{ "shader_environment", 'senv' },
		{ "shader_transparent_glass", 'sgla' },
		{ "shader", 'shdr' },
		{ "sky", 'sky ' },
		{ "shader_transparent_meter", 'smet' },
		{ "sound", 'snd!' },
		{ "sound_environment", 'snde' },
		{ "shader_model", 'soso' },
		{ "shader_transparent_generic", 'sotr' },
		{ "ui_widget_collection", 'Soul' },
		{ "shader_transparent_plasma", 'spla' },
		{ "sound_scenery", 'ssce' },
		{ "string_list", 'str#' },
		{ "shader_transparent_water", 'swat' },
		{ "tag_collection", 'tagc' },
		{ "camera_track", 'trak' },
		{ "dialogue", 'udlg' },
		{ "unit_hud_interface", 'unhi' },
		{ "unit", 'unit' },
		{ "unicode_string_list", 'ustr' },
		{ "virtual_keyboard", 'vcky' },
		{ "vehicle", 'vehi' },
		{ "weapon", 'weap' },
		{ "wind", 'wind' },
		{ "weapon_hud_interface", 'wphi' }
	};
	size_t i;

	if (!grp || !grp[0])
		return 0;
	for (i = 0; i < sizeof(s_map) / sizeof(s_map[0]); i++)
	{
		if (_stricmp(grp, s_map[i].name) == 0)
			return s_map[i].fourcc;
	}
	if (strlen(grp) == 4)
	{
		return ((uint32_t)(uint8_t)grp[0] << 24) |
		       ((uint32_t)(uint8_t)grp[1] << 16) |
		       ((uint32_t)(uint8_t)grp[2] << 8) |
		       (uint32_t)(uint8_t)grp[3];
	}
	return 0;
}

static int ocetags_is_shader_fourcc(uint32_t fourcc)
{
	return (fourcc == 'soso' || fourcc == 'senv' || fourcc == 'schi' || fourcc == 'scex' ||
	        fourcc == 'sgla' || fourcc == 'smet' || fourcc == 'spla' || fourcc == 'swat' ||
	        fourcc == 'shdr');
}

static uint8_t *ocetags_json_decode_base64_key(
	char const *buf,
	size_t start,
	size_t end,
	char const *key,
	size_t *out_len)
{
	size_t vpos = 0;
	size_t b64_start, b64_len, cap;
	uint8_t *out;
	size_t o = 0;
	uint32_t val = 0;
	int valb = -8;
	size_t p;

	if (out_len)
		*out_len = 0;
	if (!ocetags_json_find_key_range(buf, start, end, key, &vpos))
		return NULL;
	if (vpos >= end || buf[vpos] != '"')
		return NULL;
	vpos++;
	b64_start = vpos;
	while (vpos < end && buf[vpos] != '"')
	{
		if (buf[vpos] == '\\' && vpos + 1 < end)
			vpos += 2;
		else
			vpos++;
	}
	b64_len = vpos - b64_start;
	cap = (b64_len * 3u) / 4u + 16u;
	out = (uint8_t *)malloc(cap);
	if (!out)
		return NULL;

	for (p = b64_start; p < vpos; p++)
	{
		unsigned char c = (unsigned char)buf[p];
		int d = -1;
		if (c >= 'A' && c <= 'Z') d = c - 'A';
		else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
		else if (c >= '0' && c <= '9') d = c - '0' + 52;
		else if (c == '+') d = 62;
		else if (c == '/') d = 63;
		else if (c == '=') break;
		else continue;

		val = (val << 6) | (uint32_t)d;
		valb += 6;
		if (valb >= 0)
		{
			if (o < cap)
				out[o++] = (uint8_t)((val >> valb) & 0xFFu);
			valb -= 8;
		}
	}
	if (out_len)
		*out_len = o;
	return out;
}

static int catalog_file_has_ocetags_entry(char const *gstr, char const *tag_path)
{
	static char const *const s_cat_files[2] = {
		"Faber_data/cached_tags/custom_catalog.txt",
		"Faber_data/cached_tags/xbox_catalog.txt"
	};
	char line[512];
	char needle[300];
	int f_idx;

	snprintf(needle, sizeof(needle), "|2|%s", tag_path);
	for (f_idx = 0; f_idx < 2; f_idx++)
	{
		FILE *f = fopen(s_cat_files[f_idx], "r");
		if (!f)
			continue;
		while (fgets(line, sizeof(line), f))
		{
			if (_strnicmp(line, gstr, 4) == 0 && strstr(line, needle) != NULL)
			{
				fclose(f);
				return 1;
			}
		}
		fclose(f);
	}
	return 0;
}

struct ocetags_v5_tag_span
{
	uint32_t fourcc;
	char group[32];
	char path[256];
	size_t obj_start;
	size_t obj_end;
	int is_2d_bitmap;
};

#define OCETAGS_V5_MAX_TAGS 512

static int forge_rip_ocetags_v5_json_to_cached_tags(
	char const *ocetags_path,
	char const *json,
	size_t json_len,
	FILE *manifest_fp,
	FILE *credits_fp)
{
	size_t tags_pos = 0, tags_end = 0;
	char custom_item_name[64];
	char root_group_str[32];
	uint32_t root_group = 0;
	char root_tag_path[256];
	size_t cur, obj_end = 0;
	char gstr[5];
	int cat_id;

	custom_item_name[0] = 0;
	root_group_str[0] = 0;
	root_tag_path[0] = 0;
	ocetags_json_get_str(json, 0, json_len, "itemName", custom_item_name, sizeof(custom_item_name));
	if (!custom_item_name[0])
		ocetags_json_get_str(json, 0, json_len, "displayName", custom_item_name, sizeof(custom_item_name));

	if (!ocetags_json_find_key_range(json, 0, json_len, "tags", &tags_pos) ||
	    tags_pos >= json_len || json[tags_pos] != '[' ||
	    !ocetags_find_matching_brace(json, tags_pos, json_len, '[', ']', &tags_end))
	{
		return 0;
	}

	cur = tags_pos + 1;
	while (cur < tags_end)
	{
		if (json[cur] == '{' && ocetags_find_matching_brace(json, cur, tags_end, '{', '}', &obj_end))
		{
			ocetags_json_get_str(json, cur, obj_end + 1, "group", root_group_str, sizeof(root_group_str));
			ocetags_json_get_str(json, cur, obj_end + 1, "path", root_tag_path, sizeof(root_tag_path));
			root_group = ocetags_group_str_to_fourcc(root_group_str);
			if (!custom_item_name[0])
			{
				ocetags_json_get_str(json, cur, obj_end + 1, "itemName", custom_item_name, sizeof(custom_item_name));
				if (!custom_item_name[0])
					ocetags_json_get_str(json, cur, obj_end + 1, "displayName", custom_item_name, sizeof(custom_item_name));
			}
			break;
		}
		cur++;
	}

	if (!root_group || !root_tag_path[0])
		return 0;

	/* Check optional <stem>.txt sidecar for custom Forge menu item name */
	if (ocetags_path && ocetags_path[0])
	{
		char txt_path[512];
		char const *dot = strrchr(ocetags_path, '.');
		FILE *tf;
		if (dot && _stricmp(dot, ".ocetags") == 0)
		{
			size_t prefix_len = (size_t)(dot - ocetags_path);
			if (prefix_len > sizeof(txt_path) - 6u)
				prefix_len = sizeof(txt_path) - 6u;
			memcpy(txt_path, ocetags_path, prefix_len);
			memcpy(txt_path + prefix_len, ".txt", 5);
			tf = fopen(txt_path, "rb");
			if (tf)
			{
				char tline[128];
				size_t tn = fread(tline, 1, sizeof(tline) - 1u, tf);
				char *p;
				fclose(tf);
				tline[tn] = 0;
				for (p = tline; *p; p++)
				{
					if (*p == '\r' || *p == '\n')
					{
						*p = 0;
						break;
					}
				}
				p = tline;
				while (*p == ' ' || *p == '\t') p++;
				if (_strnicmp(p, "[c] ", 4) == 0) p += 4;
				else if (_strnicmp(p, "[c]", 3) == 0) p += 3;
				while (*p == ' ' || *p == '\t') p++;
				if (*p)
					snprintf(custom_item_name, sizeof(custom_item_name), "%s", p);
			}
		}
	}

	cat_id = is_forge_light_beacon_emitter_tag(root_group, root_tag_path) ? _forge_cat_lights : _forge_cat_scenery;
	if (root_group == 'vehi') cat_id = _forge_cat_vehicles;
	else if (root_group == 'weap') cat_id = _forge_cat_weapons;
	else if (root_group == 'eqip' || root_group == 'garb') cat_id = _forge_cat_equipment;
	else if (root_group == 'bipd') cat_id = _forge_cat_bipeds;
	else if (root_group == 'mach' || root_group == 'ctrl')
		cat_id = is_forge_light_beacon_emitter_tag(root_group, root_tag_path) ? _forge_cat_lights : _forge_cat_barriers_devices;
	else if (root_group == 'lifi') cat_id = _forge_cat_lights;
	else if (root_group != 'scen') return 0;

	gstr[0] = (char)((root_group >> 24) & 0xFF);
	gstr[1] = (char)((root_group >> 16) & 0xFF);
	gstr[2] = (char)((root_group >> 8) & 0xFF);
	gstr[3] = (char)(root_group & 0xFF);
	gstr[4] = 0;

	if (catalog_file_has_ocetags_entry(gstr, root_tag_path))
		return 0;

	if (manifest_fp)
	{
		if (custom_item_name[0])
			fprintf(manifest_fp, "%s|%d|2|%s|%s\n", gstr, cat_id, root_tag_path, custom_item_name);
		else
			fprintf(manifest_fp, "%s|%d|2|%s\n", gstr, cat_id, root_tag_path);
		fflush(manifest_fp);
	}
	if (credits_fp)
	{
		fprintf(credits_fp, "- **`%s`** (`%s`) — native `.ocetags` package `%s`\r\n",
			root_tag_path, gstr, ocetags_path);
		fflush(credits_fp);
	}
	return 1;
}

struct ocetags_runtime_pkg_tag
{
	uint32_t fourcc;
	uint32_t actual_group;
	uint32_t orig_index;
	uint16_t orig_low;
	char path[256];
	size_t obj_start;
	size_t obj_end;
	uint8_t *tag_buf;
	size_t raw_len;
	long runtime_handle;
	int is_reused_existing;
};

static void ocetags_runtime_rebase_and_link_schema(
	struct tag_schema_definition const *def,
	uint8_t *elem_base,
	int32_t elem_count,
	uint8_t *tag_buf,
	size_t raw_len,
	uint32_t root_size,
	struct ocetags_runtime_pkg_tag const *pkg_tags,
	int pkg_tag_count,
	int depth)
{
	int32_t e;
	struct tag_schema_field const *field;
	uint32_t elem_size;

	if (!def || !def->fields || !elem_base || elem_count <= 0 || def->size <= 0 || depth > 10)
		return;

	elem_size = (uint32_t)def->size;
	for (e = 0; e < elem_count; e++)
	{
		uint8_t *elem_ptr = elem_base + (size_t)e * elem_size;
		for (field = def->fields; field->type != _tag_schema_terminator; field++)
		{
			int16_t k;
			int16_t fcount = (field->count > 0) ? field->count : 1;
			uint32_t fstride = (field->size > 0) ? (uint32_t)field->size : 0;

			for (k = 0; k < fcount; k++)
			{
				uint32_t f_off = (uint32_t)field->offset + (uint32_t)k * fstride;
				uint8_t *f_ptr = elem_ptr + f_off;

				if (field->type == _tag_schema_struct)
				{
					struct tag_schema_definition const *sub_def =
						(struct tag_schema_definition const *)field->definition;
					if (sub_def && f_off + (uint32_t)sub_def->size <= elem_size)
					{
						ocetags_runtime_rebase_and_link_schema(
							sub_def, f_ptr, 1, tag_buf, raw_len, root_size,
							pkg_tags, pkg_tag_count, depth + 1);
					}
				}
				else if (field->type == _tag_schema_block)
				{
					struct tag_schema_definition const *child_def =
						(struct tag_schema_definition const *)field->definition;
					if (child_def && child_def->size > 0 && f_off + 12u <= elem_size)
					{
						int32_t cnt = *(int32_t const *)(f_ptr + 0);
						uint32_t rel = *(uint32_t const *)(f_ptr + 4);
						uint32_t child_sz = (uint32_t)child_def->size;
						if (cnt > 0 && cnt <= 65536 &&
							rel >= root_size &&
							(uint64_t)rel + (uint64_t)cnt * child_sz <= (uint64_t)raw_len)
						{
							uint8_t *child_base = tag_buf + rel;
							*(int32_t *)(f_ptr + 0) = cnt;
							*(void **)(f_ptr + 4) = child_base;
							*(void **)(f_ptr + 8) = NULL;
							ocetags_runtime_rebase_and_link_schema(
								child_def, child_base, cnt, tag_buf, raw_len, root_size,
								pkg_tags, pkg_tag_count, depth + 1);
						}
						else
						{
							memset(f_ptr, 0, 12);
						}
					}
				}
				else if (field->type == _tag_schema_data)
				{
					if (f_off + 20u <= elem_size)
					{
						int32_t sz = *(int32_t const *)(f_ptr + 0);
						uint32_t rel = *(uint32_t const *)(f_ptr + 12);
						if (sz > 0 && sz <= 16 * 1024 * 1024 &&
							rel >= root_size &&
							(uint64_t)rel + (uint64_t)sz <= (uint64_t)raw_len)
						{
							*(int32_t *)(f_ptr + 0) = sz;
							*(uint32_t *)(f_ptr + 4) = 0;
							*(uint32_t *)(f_ptr + 8) = 0;
							*(void **)(f_ptr + 12) = tag_buf + rel;
							*(void **)(f_ptr + 16) = NULL;
						}
						else
						{
							memset(f_ptr, 0, 20);
						}
					}
				}
				else if (field->type == _tag_schema_reference)
				{
					if (f_off + 16u <= elem_size)
					{
						int32_t ref_idx = *(int32_t const *)(f_ptr + 12);
						long resolved_handle = NONE;
						uint32_t resolved_grp = *(uint32_t const *)(f_ptr + 0);
						if (ref_idx != -1 && ref_idx != 0)
						{
							uint16_t orig_low = (uint16_t)((uint32_t)ref_idx & 0xFFFFu);
							int m;
							for (m = 0; m < pkg_tag_count; m++)
							{
								if ((uint32_t)ref_idx == pkg_tags[m].orig_index || orig_low == pkg_tags[m].orig_low)
								{
									resolved_handle = pkg_tags[m].runtime_handle;
									resolved_grp = pkg_tags[m].actual_group;
									break;
								}
							}
						}
						if (resolved_grp == 'mod2') resolved_grp = 'mode';
						else if (resolved_grp == 'scex') resolved_grp = 'schi';
						*(uint32_t *)(f_ptr + 0) = (resolved_handle != NONE) ? resolved_grp : 0u;
						*(char const **)(f_ptr + 4) = NULL;
						*(int32_t *)(f_ptr + 8) = 0;
						*(int32_t *)(f_ptr + 12) = (int32_t)resolved_handle;
					}
				}
				else if (field->type == _tag_schema_tag_index)
				{
					if (f_off + 4u <= elem_size)
					{
						int32_t ref_idx = *(int32_t const *)f_ptr;
						long resolved_handle = NONE;
						if (ref_idx != -1 && ref_idx != 0)
						{
							uint16_t orig_low = (uint16_t)((uint32_t)ref_idx & 0xFFFFu);
							int m;
							for (m = 0; m < pkg_tag_count; m++)
							{
								if ((uint32_t)ref_idx == pkg_tags[m].orig_index || orig_low == pkg_tags[m].orig_low)
								{
									resolved_handle = pkg_tags[m].runtime_handle;
									break;
								}
							}
						}
						*(int32_t *)f_ptr = (int32_t)resolved_handle;
					}
				}
			}
		}
	}
}

static long load_native_ocetags_package_from_json(
	char const *json,
	size_t json_len,
	uint32_t req_group,
	char const *req_path,
	uint32_t *out_actual_group)
{
	size_t fmt_pos = 0, tags_pos = 0, tags_end = 0;
	struct ocetags_runtime_pkg_tag *pkg_tags = NULL;
	int pkg_tag_count = 0;
	int i;
	uint8_t **raw_assets = NULL;
	size_t *raw_asset_lens = NULL;
	int raw_asset_count = 0;
	long root_handle = NONE;

	if (!json || json_len < 32 || !req_path || !req_path[0])
		return NONE;
	if (!ocetags_json_find_key_range(json, 0, json_len, "formatVersion", &fmt_pos))
		return NONE;
	if (!ocetags_json_find_key_range(json, 0, json_len, "tags", &tags_pos) ||
		!ocetags_json_skip_value(json, tags_pos, json_len, &tags_end))
		return NONE;

	pkg_tags = (struct ocetags_runtime_pkg_tag *)calloc(OCETAGS_V5_MAX_TAGS, sizeof(*pkg_tags));
	if (!pkg_tags)
		return NONE;

	/* 1. Scan tags[] array and check if tags[0] matches (req_group, req_path) */
	{
		size_t p = tags_pos + 1;
		while (p < tags_end && pkg_tag_count < OCETAGS_V5_MAX_TAGS)
		{
			size_t obj_start, obj_end;
			while (p < tags_end && json[p] != '{' && json[p] != ']')
				p++;
			if (p >= tags_end || json[p] != '{')
				break;
			obj_start = p;
			if (!ocetags_json_skip_value(json, obj_start, tags_end, &obj_end))
				break;
			{
				struct ocetags_runtime_pkg_tag *pt = &pkg_tags[pkg_tag_count];
				char gstr[64];
				gstr[0] = 0;
				ocetags_json_get_str(json, obj_start, obj_end, "group", gstr, sizeof(gstr));
				ocetags_json_get_str(json, obj_start, obj_end, "path", pt->path, sizeof(pt->path));
				ocetags_json_get_u32(json, obj_start, obj_end, "originalIndex", &pt->orig_index);
				pt->orig_low = (uint16_t)(pt->orig_index & 0xFFFFu);
				pt->fourcc = ocetags_group_str_to_fourcc(gstr);
				pt->actual_group = (pt->fourcc == 'mod2') ? 'mode' : ((pt->fourcc == 'scex') ? 'schi' : pt->fourcc);
				pt->obj_start = obj_start;
				pt->obj_end = obj_end;
				pt->runtime_handle = NONE;
				if (pkg_tag_count == 0)
				{
					/* Match root tag! */
					if (pt->fourcc != req_group || _stricmp(pt->path, req_path) != 0)
					{
						free(pkg_tags);
						return NONE;
					}
				}
				pkg_tag_count++;
			}
			p = obj_end;
		}
	}

	if (pkg_tag_count <= 0)
	{
		free(pkg_tags);
		return NONE;
	}

	/* 2. Decode each tag's raw binary data and register in global_tag_instances.
	 * Strictly keep Custom Edition .ocetags packages using their own Custom Edition bitmaps,
	 * shaders, models, and sounds from the .ocetags package! */
	for (i = 0; i < pkg_tag_count; i++)
	{
		struct ocetags_runtime_pkg_tag *pt = &pkg_tags[i];
		struct tag_schema_group const *schema_grp = forge_runtime_schema_group_get(pt->fourcc);
		long p0 = schema_grp ? schema_grp->parent_group_tags[0] : NONE;
		long p1 = schema_grp ? schema_grp->parent_group_tags[1] : NONE;
		uint8_t *decoded = NULL;
		size_t decoded_len = 0;
		size_t alloc_sz;

		if (pt->fourcc == 'scex')
		{
			p0 = 'shdr';
			p1 = NONE;
		}

		decoded = ocetags_json_decode_base64_key(json, pt->obj_start, pt->obj_end, "data", &decoded_len);
		if (!decoded || decoded_len == 0)
		{
			free(decoded);
			/* Only reuse an existing map tag if the .ocetags entry has no embedded raw data AND we are on a CE map */
			if (i > 0 && custom_edition_cache_tags_loaded())
			{
				long ex = tag_loaded((long)pt->actual_group, pt->path);
				if (ex != NONE)
				{
					pt->runtime_handle = ex;
					pt->is_reused_existing = 1;
				}
			}
			continue;
		}

		alloc_sz = (decoded_len > 4096u) ? (decoded_len + 256u) : 4096u;
		pt->tag_buf = (uint8_t *)calloc(1, alloc_sz);
		if (!pt->tag_buf)
		{
			free(decoded);
			continue;
		}
		memcpy(pt->tag_buf, decoded, decoded_len);
		pt->raw_len = decoded_len;
		free(decoded);

		{
			char reg_name[256];
			if (i > 0 && tag_loaded((long)pt->actual_group, pt->path) != NONE)
				snprintf(reg_name, sizeof(reg_name), "%s__f%d", pt->path, s_dyn_tags.count);
			else
				snprintf(reg_name, sizeof(reg_name), "%s", pt->path);
			pt->runtime_handle = append_runtime_tag_instance(
				(long)pt->actual_group, p0, p1, reg_name, pt->tag_buf);
		}
	}

	/* 4. Rebase internal tag_blocks/tag_data and link all tag_references across the package */
	for (i = 0; i < pkg_tag_count; i++)
	{
		struct ocetags_runtime_pkg_tag *pt = &pkg_tags[i];
		struct tag_schema_group const *schema_grp;
		uint32_t root_sz;

		if (!pt->tag_buf || pt->is_reused_existing || pt->runtime_handle == NONE)
			continue;
		schema_grp = forge_runtime_schema_group_get(pt->fourcc);
		if (!schema_grp || !schema_grp->definition || schema_grp->definition->size <= 0)
			continue;
		root_sz = (uint32_t)schema_grp->definition->size;
		if (pt->raw_len < root_sz)
			continue;

		ocetags_runtime_rebase_and_link_schema(
			schema_grp->definition,
			pt->tag_buf,
			1,
			pt->tag_buf,
			pt->raw_len,
			root_sz,
			pkg_tags,
			pkg_tag_count,
			0);
	}

	/* 5. Decode per-tag embeddedRawDataAssets & embeddedRawDataPatches for Custom Edition 'bitm' and 'snd!' tags */
	for (i = 0; i < pkg_tag_count; i++)
	{
		struct ocetags_runtime_pkg_tag *pt = &pkg_tags[i];
		if (!pt->tag_buf || pt->is_reused_existing || pt->runtime_handle == NONE)
			continue;

		if (pt->fourcc == 'bitm' && pt->raw_len >= 0x6Cu)
		{
			struct bitmap_group *bgrp = (struct bitmap_group *)pt->tag_buf;
			uint8_t **tag_assets = NULL;
			size_t *tag_asset_lens = NULL;
			int tag_asset_count = 0;
			size_t assets_pos = 0, assets_end = 0;
			size_t patches_pos = 0, patches_end = 0;
			unsigned char chan_order = 0;
			int k, bi, ai;

			*(uint32_t *)(pt->tag_buf + 0x38u) = 0; /* clear processed_pixel_data.size */
			*(void **)(pt->tag_buf + 0x44u) = NULL;

			/* Check if any shader_model ('soso') uses this bitmap as multipurpose_map (+0xBC + 12 = +0xC8)
			 * without also using it as base_map (+0xB0), detail_map (+0xE8), or reflection_cube_map (+0x170) */
			{
				int used_mp = 0, used_rgba = 0;
				for (k = 0; k < pkg_tag_count; k++)
				{
					struct ocetags_runtime_pkg_tag *st = &pkg_tags[k];
					if (!st->tag_buf || st->fourcc != 'soso' || st->raw_len < 0x178u)
						continue;
					if (*(int32_t const *)(st->tag_buf + 0xC8u) == (int32_t)pt->runtime_handle)
						used_mp = 1;
					if (*(int32_t const *)(st->tag_buf + 0xB0u) == (int32_t)pt->runtime_handle ||
						*(int32_t const *)(st->tag_buf + 0xE8u) == (int32_t)pt->runtime_handle ||
						*(int32_t const *)(st->tag_buf + 0x170u) == (int32_t)pt->runtime_handle)
						used_rgba = 1;
				}
				if (used_mp && !used_rgba)
					chan_order = 1; /* _custom_edition_channels_multipurpose */
			}

			/* Decode per-tag embeddedRawDataAssets[] inside pt->obj_start .. pt->obj_end */
			if (ocetags_json_find_key_range(json, pt->obj_start, pt->obj_end, "embeddedRawDataAssets", &assets_pos) &&
				ocetags_json_skip_value(json, assets_pos, pt->obj_end, &assets_end))
			{
				size_t p = assets_pos + 1;
				tag_assets = (uint8_t **)calloc(64, sizeof(uint8_t *));
				tag_asset_lens = (size_t *)calloc(64, sizeof(size_t));
				while (tag_assets && tag_asset_lens && p < assets_end && tag_asset_count < 64)
				{
					size_t a_start, a_end;
					while (p < assets_end && json[p] != '{' && json[p] != ']')
						p++;
					if (p >= assets_end || json[p] != '{')
						break;
					a_start = p;
					if (!ocetags_json_skip_value(json, a_start, assets_end, &a_end))
						break;
					tag_assets[tag_asset_count] = ocetags_json_decode_base64_key(
						json, a_start, a_end, "data", &tag_asset_lens[tag_asset_count]);
					tag_asset_count++;
					p = a_end;
				}
			}

			for (bi = 0; bi < bgrp->bitmaps.count && bgrp->bitmaps.address; bi++)
			{
				struct bitmap_data *bmp = (struct bitmap_data *)((uint8_t *)bgrp->bitmaps.address + (size_t)bi * sizeof(struct bitmap_data));
				bmp->base_address = NULL;
				bmp->hardware_format = NULL;
				bmp->cache_block_index = NONE;
				bmp->tag_index = pt->runtime_handle;
			}

			/* Apply per-tag embeddedRawDataPatches[] (supports both patchOffset and offsetFieldOffset) */
			if (tag_asset_count > 0 &&
				ocetags_json_find_key_range(json, pt->obj_start, pt->obj_end, "embeddedRawDataPatches", &patches_pos) &&
				ocetags_json_skip_value(json, patches_pos, pt->obj_end, &patches_end))
			{
				size_t p = patches_pos + 1;
				while (p < patches_end)
				{
					size_t p_start, p_end;
					uint32_t a_idx = 0xFFFFFFFFu, patch_off = 0xFFFFFFFFu;
					while (p < patches_end && json[p] != '{' && json[p] != ']')
						p++;
					if (p >= patches_end || json[p] != '{')
						break;
					p_start = p;
					if (!ocetags_json_skip_value(json, p_start, patches_end, &p_end))
						break;
					if (ocetags_json_get_u32(json, p_start, p_end, "assetIndex", &a_idx) &&
						(ocetags_json_get_u32(json, p_start, p_end, "patchOffset", &patch_off) ||
						 ocetags_json_get_u32(json, p_start, p_end, "offsetFieldOffset", &patch_off)) &&
						a_idx < (uint32_t)tag_asset_count &&
						tag_assets[a_idx] && tag_asset_lens[a_idx] > 0 &&
						patch_off >= 24u && patch_off + 24u <= pt->raw_len)
					{
						struct bitmap_data *bmp = (struct bitmap_data *)(pt->tag_buf + (patch_off - 24u));
						bmp->base_address = tag_assets[a_idx];
						bmp->pixels_size = (long)tag_asset_lens[a_idx];
					}
					p = p_end;
				}
			}

			/* Finalize each bitmap_data entry: allocate in Xbox contiguous memory via forge_phys_alloc,
			 * convert Custom Edition texel layout via rasterizer_xbox_bitmap_rebuild_hardware_format,
			 * and initialize D3DBaseTexture hardware_format! */
			for (bi = 0; bi < bgrp->bitmaps.count && bgrp->bitmaps.address; bi++)
			{
				struct bitmap_data *bmp = (struct bitmap_data *)((uint8_t *)bgrp->bitmaps.address + (size_t)bi * sizeof(struct bitmap_data));
				uint8_t const *src_pixels = (uint8_t const *)bmp->base_address;
				size_t src_len = (bmp->pixels_size > 0) ? (size_t)bmp->pixels_size : 0u;
				short max_mips;
				long req_bytes;
				size_t alloc_bytes;
				void *pix_copy;
				D3DBaseTexture *tex;

				if ((!src_pixels || src_len == 0) && bi < tag_asset_count && tag_assets[bi] && tag_asset_lens[bi] > 0)
				{
					src_pixels = tag_assets[bi];
					src_len = tag_asset_lens[bi];
				}
				bmp->base_address = NULL;
				bmp->hardware_format = NULL;

				bmp->signature = 'bitm';
				bmp->flags &= (unsigned short)~(0x80u | 0x08u | 0x10u | 0x100u);
				if (bmp->width > 0 && (bmp->width & (bmp->width - 1)) == 0 &&
					bmp->height > 0 && (bmp->height & (bmp->height - 1)) == 0)
					bmp->flags |= 0x01u; /* _bitmap_power_of_two_dimensions_bit */
				if (bmp->format >= 14 && bmp->format <= 16)
					bmp->flags |= 0x02u; /* _bitmap_compressed_bit */
				else
					bmp->flags &= (unsigned short)~0x02u;
				if (bmp->format == 17)
					bmp->flags |= 0x04u; /* _bitmap_palettized_bit */

				if (!src_pixels || src_len == 0 || bmp->width <= 0 || bmp->height <= 0 || bmp->depth <= 0)
					continue;

				max_mips = rasterizer_xbox_bitmap_get_max_mipmap_count(bmp);
				if (bmp->mipmap_count < 0)
					bmp->mipmap_count = 0;
				if (bmp->mipmap_count > max_mips)
					bmp->mipmap_count = max_mips;
				while (bmp->mipmap_count > 0 &&
					   (size_t)bitmap_get_pixel_data_size(bmp) > src_len)
				{
					bmp->mipmap_count--;
				}
				req_bytes = rasterizer_xbox_bitmap_get_pixel_data_size(bmp);
				if (req_bytes <= 0)
					continue;
				alloc_bytes = ((size_t)req_bytes > src_len) ? (size_t)req_bytes : src_len;
				pix_copy = forge_phys_alloc(alloc_bytes);
				tex = (D3DBaseTexture *)calloc(1, sizeof(D3DBaseTexture));
				if (!pix_copy || !tex)
				{
					free(tex);
					continue;
				}
				memcpy(pix_copy, src_pixels, src_len);
				bmp->base_address = pix_copy;
				bmp->pixels_offset = 0;
				bmp->pixels_size = (long)alloc_bytes;
				bmp->tag_index = pt->runtime_handle;
				bmp->cache_block_index = NONE;

				halo_custom_edition_texels_channels(bmp->base_address, chan_order);
				rasterizer_xbox_bitmap_rebuild_hardware_format(bmp);

				tex->Data = 0;
				tex->Lock = 0;
				tex->Common = D3DCOMMON_TYPE_TEXTURE | 1;
				if (TEST_FLAG(bmp->flags, 4 /* _bitmap_linear_bit */))
				{
					tex->Format =
						(bitmap_format_to_d3d_linear_format(bmp->format, bmp->flags) << D3DFORMAT_FORMAT_SHIFT) |
						(1 << D3DFORMAT_MIPMAP_SHIFT) |
						(2 << D3DFORMAT_DIMENSION_SHIFT) |
						D3DFORMAT_BORDERSOURCE_COLOR |
						D3DFORMAT_DMACHANNEL_A;
					tex->Size =
						(((bitmap_mipmap_get_row_pitch(bmp, 0) + D3DTEXTURE_PITCH_ALIGNMENT - 1) / D3DTEXTURE_PITCH_ALIGNMENT - 1)
							<< D3DSIZE_PITCH_SHIFT) |
						((bmp->height - 1) << D3DSIZE_HEIGHT_SHIFT) |
						(bmp->width - 1);
				}
				else
				{
					tex->Format =
						(floor_log2(bmp->depth) << D3DFORMAT_PSIZE_SHIFT) |
						(floor_log2(bmp->height) << D3DFORMAT_VSIZE_SHIFT) |
						(floor_log2(bmp->width) << D3DFORMAT_USIZE_SHIFT) |
						(bitmap_format_to_d3d_format(bmp->format, bmp->flags) << D3DFORMAT_FORMAT_SHIFT) |
						((bmp->type == 1 /* _bitmap_type_3d */ ? 3 : 2) << D3DFORMAT_DIMENSION_SHIFT) |
						((rasterizer_xbox_bitmap_get_max_mipmap_count(bmp) + 1) << D3DFORMAT_MIPMAP_SHIFT) |
						(bmp->type == 2 /* _bitmap_type_cube_map */ ? D3DFORMAT_CUBEMAP : 0) |
						D3DFORMAT_BORDERSOURCE_COLOR |
						D3DFORMAT_DMACHANNEL_A;
					tex->Size = 0;
				}
				IDirect3DBaseTexture8_Register(tex, bmp->base_address);
				bmp->hardware_format = tex;
			}

			if (tag_assets)
			{
				for (ai = 0; ai < tag_asset_count; ai++)
					free(tag_assets[ai]);
				free(tag_assets);
			}
			free(tag_asset_lens);
		}
		else if (pt->fourcc == 'snd!' && pt->raw_len >= 0xA4u)
		{
			struct sound_definition *sdef = (struct sound_definition *)pt->tag_buf;
			uint8_t **tag_assets = NULL;
			size_t *tag_asset_lens = NULL;
			int tag_asset_count = 0;
			size_t assets_pos = 0, assets_end = 0;
			size_t patches_pos = 0, patches_end = 0;
			long channels = (sdef->encoding == 1) ? 2 : 1;
			long rate = (sdef->sample_rate == 1) ? 44100 : 22050;
			long encoded_rate = (channels == 1) ? 22050 : rate;
			int pr_i, pm_i, ai, flat_perm_idx = 0, valid_perms = 0;

			/* Decode per-tag embeddedRawDataAssets[] inside pt->obj_start .. pt->obj_end */
			if (ocetags_json_find_key_range(json, pt->obj_start, pt->obj_end, "embeddedRawDataAssets", &assets_pos) &&
				ocetags_json_skip_value(json, assets_pos, pt->obj_end, &assets_end))
			{
				size_t p = assets_pos + 1;
				tag_assets = (uint8_t **)calloc(128, sizeof(uint8_t *));
				tag_asset_lens = (size_t *)calloc(128, sizeof(size_t));
				while (tag_assets && tag_asset_lens && p < assets_end && tag_asset_count < 128)
				{
					size_t a_start, a_end;
					while (p < assets_end && json[p] != '{' && json[p] != ']')
						p++;
					if (p >= assets_end || json[p] != '{')
						break;
					a_start = p;
					if (!ocetags_json_skip_value(json, a_start, assets_end, &a_end))
						break;
					tag_assets[tag_asset_count] = ocetags_json_decode_base64_key(
						json, a_start, a_end, "data", &tag_asset_lens[tag_asset_count]);
					tag_asset_count++;
					p = a_end;
				}
			}

			sdef->promotion_sound.index = NONE;
			sdef->promotion_count = 0;
			if (channels == 1 && sdef->sample_rate == 1)
				sdef->sample_rate = 0; /* Xbox plays mono sounds at 22 kHz */
			if (sdef->minimum_distance <= 0.0f) sdef->minimum_distance = 1.0f;
			if (sdef->maximum_distance <= 0.0f) sdef->maximum_distance = 30.0f;
			if (sdef->random_pitch_bounds.lower <= 0.0f) sdef->random_pitch_bounds.lower = 1.0f;
			if (sdef->random_pitch_bounds.upper <= 0.0f) sdef->random_pitch_bounds.upper = 1.0f;
			if (sdef->zero_skip_fraction_modifier == 0.0f && sdef->one_skip_fraction_modifier == 0.0f)
			{
				sdef->zero_skip_fraction_modifier = 1.0f;
				sdef->one_skip_fraction_modifier = 1.0f;
			}
			if (sdef->zero_gain_modifier == 0.0f && sdef->one_gain_modifier == 0.0f)
			{
				sdef->zero_gain_modifier = 1.0f;
				sdef->one_gain_modifier = 1.0f;
			}
			if (sdef->zero_pitch_modifier == 0.0f && sdef->one_pitch_modifier == 0.0f)
			{
				sdef->zero_pitch_modifier = 1.0f;
				sdef->one_pitch_modifier = 1.0f;
			}

			/* Match embeddedRawDataPatches to permutations */
			if (tag_asset_count > 0 &&
				ocetags_json_find_key_range(json, pt->obj_start, pt->obj_end, "embeddedRawDataPatches", &patches_pos) &&
				ocetags_json_skip_value(json, patches_pos, pt->obj_end, &patches_end))
			{
				size_t p = patches_pos + 1;
				while (p < patches_end)
				{
					size_t p_start, p_end;
					uint32_t a_idx = 0xFFFFFFFFu, patch_off = 0xFFFFFFFFu;
					while (p < patches_end && json[p] != '{' && json[p] != ']')
						p++;
					if (p >= patches_end || json[p] != '{')
						break;
					p_start = p;
					if (!ocetags_json_skip_value(json, p_start, patches_end, &p_end))
						break;
					if (ocetags_json_get_u32(json, p_start, p_end, "assetIndex", &a_idx) &&
						(ocetags_json_get_u32(json, p_start, p_end, "patchOffset", &patch_off) ||
						 ocetags_json_get_u32(json, p_start, p_end, "offsetFieldOffset", &patch_off)) &&
						a_idx < (uint32_t)tag_asset_count &&
						tag_assets[a_idx] && tag_asset_lens[a_idx] > 0)
					{
						for (pr_i = 0; pr_i < sdef->pitch_ranges.count && sdef->pitch_ranges.address; pr_i++)
						{
							struct sound_pitch_range *pr = (struct sound_pitch_range *)((uint8_t *)sdef->pitch_ranges.address + (size_t)pr_i * sizeof(struct sound_pitch_range));
							for (pm_i = 0; pm_i < pr->permutations.count && pr->permutations.address; pm_i++)
							{
								struct sound_permutation *perm = (struct sound_permutation *)((uint8_t *)pr->permutations.address + (size_t)pm_i * sizeof(struct sound_permutation));
								uint32_t perm_off = (uint32_t)((uint8_t *)perm - pt->tag_buf);
								if (patch_off >= perm_off + 0x40u && patch_off < perm_off + 0x54u)
								{
									perm->samples.address = tag_assets[a_idx];
									perm->samples.size = (long)tag_asset_lens[a_idx];
								}
							}
						}
					}
					p = p_end;
				}
			}

			for (pr_i = 0; pr_i < sdef->pitch_ranges.count && sdef->pitch_ranges.address; pr_i++)
			{
				struct sound_pitch_range *pr = (struct sound_pitch_range *)((uint8_t *)sdef->pitch_ranges.address + (size_t)pr_i * sizeof(struct sound_pitch_range));
				if (pr->natural_pitch <= 0.0f) pr->natural_pitch = 1.0f;
				pr->playback_rate = 1.0f / pr->natural_pitch;
				pr->played_permutation_mask = 0xFFFFFFFFu;
				pr->previous_permutation_index = NONE;
				pr->forced_permutation_index = NONE;
				if (pr->actual_permutation_count <= 0 || pr->actual_permutation_count > pr->permutations.count)
					pr->actual_permutation_count = (short)pr->permutations.count;

				for (pm_i = 0; pm_i < pr->permutations.count && pr->permutations.address; pm_i++, flat_perm_idx++)
				{
					struct sound_permutation *perm = (struct sound_permutation *)((uint8_t *)pr->permutations.address + (size_t)pm_i * sizeof(struct sound_permutation));
					uint8_t const *src_snd = (uint8_t const *)perm->samples.address;
					size_t src_snd_len = (perm->samples.size > 0) ? (size_t)perm->samples.size : 0u;
					unsigned long encoded_bytes = 0;
					void *encoded_buf = NULL;

					if ((!src_snd || src_snd_len == 0) && flat_perm_idx < tag_asset_count &&
						tag_assets[flat_perm_idx] && tag_asset_lens[flat_perm_idx] > 0)
					{
						src_snd = tag_assets[flat_perm_idx];
						src_snd_len = tag_asset_lens[flat_perm_idx];
					}
					perm->samples.address = NULL;
					perm->samples.size = 0;
					perm->samples.file_offset = 0;
					perm->unknown0 = NONE;
					perm->unknown1 = 0;
					perm->unknown2 = (unsigned long)pt->runtime_handle;
					perm->unknown3 = (unsigned long)pt->runtime_handle;
					if (perm->gain <= 0.0f)
						perm->gain = 1.0f;

					if (src_snd && src_snd_len > 0)
					{
						if (perm->compression == 1 /* Xbox ADPCM */ && rate == encoded_rate)
						{
							encoded_buf = malloc(src_snd_len);
							if (encoded_buf)
							{
								memcpy(encoded_buf, src_snd, src_snd_len);
								encoded_bytes = (unsigned long)src_snd_len;
							}
						}
						else
						{
							encoded_buf = custom_edition_sounds_encode(
								src_snd,
								(long)src_snd_len,
								perm->compression,
								FALSE /* .ocetags raw data is little-endian */,
								channels,
								rate,
								encoded_rate,
								&encoded_bytes);
						}
						if (encoded_buf && encoded_bytes > 0 && encoded_bytes <= MAXIMUM_SOUND_DATA_SIZE)
						{
							perm->samples.address = encoded_buf;
							perm->samples.size = (long)encoded_bytes;
							perm->compression = 1; /* SOUND_COMPRESSION_XBOX_ADPCM */
							perm->sample_buffer_size = 0;
							forge_register_dyn_sound_perm(perm);
							valid_perms++;
						}
						else
						{
							free(encoded_buf);
						}
					}
				}
			}
			sdef->compression = 1;
			if (valid_perms == 0)
			{
				sdef->pitch_ranges.count = 0;
				sdef->pitch_ranges.address = NULL;
			}

			if (tag_assets)
			{
				for (ai = 0; ai < tag_asset_count; ai++)
					free(tag_assets[ai]);
				free(tag_assets);
			}
			free(tag_asset_lens);
		}
	}

	/* 6. Perform OpenCE runtime conversions on models ('mod2'->'mode'), shaders, animations, weapons, HUD, collision, and vehicles */
	for (i = 0; i < pkg_tag_count; i++)
	{
		struct ocetags_runtime_pkg_tag *pt = &pkg_tags[i];
		if (!pt->tag_buf || pt->is_reused_existing || pt->runtime_handle == NONE)
			continue;

		/* Shader runtime type and scex->schi conversion */
		if (ocetags_is_shader_fourcc(pt->fourcc) && pt->raw_len >= 0x28u)
		{
			int16_t stype = 4;
			if (pt->fourcc == 'senv') stype = 3;
			else if (pt->fourcc == 'soso') stype = 4;
			else if (pt->fourcc == 'sotr') stype = 5;
			else if (pt->fourcc == 'schi' || pt->fourcc == 'scex') stype = 6;
			else if (pt->fourcc == 'swat') stype = 7;
			else if (pt->fourcc == 'sgla') stype = 8;
			else if (pt->fourcc == 'smet') stype = 9;
			else if (pt->fourcc == 'spla') stype = 10;
			*(int16_t *)(pt->tag_buf + 0x24u) = stype;

			if (pt->fourcc == 'scex' && pt->raw_len >= 0x78u)
			{
				if (*(int32_t const *)(pt->tag_buf + 0x54u) == 0 && *(int32_t const *)(pt->tag_buf + 0x60u) > 0)
				{
					memcpy(pt->tag_buf + 0x54u, pt->tag_buf + 0x60u, 12);
					*(uint32_t *)(pt->tag_buf + 0x60u) = *(uint32_t const *)(pt->tag_buf + 0x6Cu);
					memset(pt->tag_buf + 0x64u, 0, 8);
				}
			}
		}
		else if (pt->fourcc == 'mod2' && pt->raw_len >= 0xE8u)
		{
			uint8_t *mbuf = pt->tag_buf;
			uint32_t mflags = *(uint32_t const *)(mbuf + 0x00u);
			int has_local_nodes = ((mflags >> 1) & 1u) != 0;
			int32_t node_cnt = *(int32_t const *)(mbuf + 0xB8u);
			uint8_t *node_base = *(uint8_t **)(mbuf + 0xBCu);
			int32_t geom_cnt = *(int32_t const *)(mbuf + 0xD0u);
			uint8_t *geom_base = *(uint8_t **)(mbuf + 0xD4u);
			int32_t shdr_cnt = *(int32_t const *)(mbuf + 0xDCu);
			uint8_t *shdr_base = *(uint8_t **)(mbuf + 0xE0u);
			int n, s_idx;

			if (!(*(float const *)(mbuf + 0x30u) > 0.0001f && *(float const *)(mbuf + 0x30u) < 10000.0f))
				*(float *)(mbuf + 0x30u) = 1.0f;
			if (!(*(float const *)(mbuf + 0x34u) > 0.0001f && *(float const *)(mbuf + 0x34u) < 10000.0f))
				*(float *)(mbuf + 0x34u) = 1.0f;

			for (s_idx = 0; s_idx < shdr_cnt && shdr_base; s_idx++)
			{
				int16_t *perm_idx_ptr = (int16_t *)(shdr_base + (size_t)s_idx * 32u + 16u);
				if (*perm_idx_ptr < 0)
					*perm_idx_ptr = 0;
			}

			/* Ensure model nodes have valid quaternions and runtime_default_inverse_matrix */
			if (node_cnt > 0 && node_base)
			{
				struct real_matrix4x3 default_node_mats[64];
				int need_inv_calc = 0;

				for (n = 0; n < node_cnt; n++)
				{
					uint8_t *mn = node_base + (size_t)n * 156u;
					float qi = *(float const *)(mn + 0x34u);
					float qj = *(float const *)(mn + 0x38u);
					float qk = *(float const *)(mn + 0x3Cu);
					float qw = *(float const *)(mn + 0x40u);
					float mscale = *(float const *)(mn + 0x68u);
					if (qi == 0.0f && qj == 0.0f && qk == 0.0f && qw == 0.0f)
						*(float *)(mn + 0x40u) = 1.0f;
					if (!(mscale > 0.0001f && mscale < 1000.0f))
						need_inv_calc = 1;
				}
				if (need_inv_calc)
				{
					memset(default_node_mats, 0, sizeof(default_node_mats));
					model_get_node_matrices((struct model const *)mbuf, default_node_mats, NULL, NULL, NULL);
					for (n = 0; n < node_cnt && n < 64; n++)
					{
						uint8_t *mn = node_base + (size_t)n * 156u;
						float mscale = *(float const *)(mn + 0x68u);
						if (!(mscale > 0.0001f && mscale < 1000.0f))
						{
							if (default_node_mats[n].scale > 0.0001f)
							{
								matrix4x3_inverse(
									&default_node_mats[n],
									(struct real_matrix4x3 *)(mn + 0x68u));
							}
							else
							{
								memset(mn + 0x68u, 0, 52);
								*(float *)(mn + 0x68u) = 1.0f;
								*(float *)(mn + 0x6Cu) = 1.0f;
								*(float *)(mn + 0x7Cu) = 1.0f;
								*(float *)(mn + 0x8Cu) = 1.0f;
							}
						}
					}
				}
				for (n = 0; n < 5; n++)
				{
					int16_t *cutoff = (int16_t *)(mbuf + 0x1Cu + (size_t)n * 2u);
					if (*cutoff <= 0 || *cutoff > node_cnt)
						*cutoff = (int16_t)(node_cnt - 1);
				}
			}

			/* Convert 132-byte custom_edition_model_part elements in-place to 104-byte model_geometry_part
			 * and build hardware vertex/triangle buffers from gbxmodelGeometryParts[] */
			{
				size_t gparts_arr = 0, gparts_end = 0;
				int has_gparts = (ocetags_json_find_key_range(json, pt->obj_start, pt->obj_end, "gbxmodelGeometryParts", &gparts_arr) &&
								  ocetags_json_skip_value(json, gparts_arr, pt->obj_end, &gparts_end));
				int g_idx;

				for (g_idx = 0; g_idx < geom_cnt && geom_base; g_idx++)
				{
					uint8_t *geom = geom_base + (size_t)g_idx * 48u;
					int32_t part_cnt = *(int32_t const *)(geom + 36u);
					uint8_t *parts_base = *(uint8_t **)(geom + 40u);
					int p_idx;

					if (!parts_base || part_cnt <= 0)
						continue;

					for (p_idx = 0; p_idx < part_cnt; p_idx++)
					{
						uint8_t ce_part[132];
						uint8_t xbox_part[104];
						uint8_t *verts_raw = NULL;
						size_t verts_bytes = 0;
						uint8_t *tris_raw = NULL;
						size_t tris_bytes = 0;

						memcpy(ce_part, parts_base + (size_t)p_idx * 132u, 132u);
						memset(xbox_part, 0, sizeof(xbox_part));
						memcpy(xbox_part, ce_part, 32u);
						*(uint32_t *)(xbox_part + 0u) &= ~0x02u; /* clear _model_geometry_part_local_nodes_bit */
						xbox_part[6] = 0xFFu; /* previous_part_index = -1 */
						xbox_part[7] = 0xFFu; /* next_part_index = -1 */
						if (has_local_nodes)
						{
							int16_t lnc = (int16_t)ce_part[107u];
							uint8_t const *ln = ce_part + 108u;
							int16_t c0 = *(int16_t const *)(xbox_part + 8u);
							int16_t c1 = *(int16_t const *)(xbox_part + 10u);
							if (lnc > 0 && lnc <= 22)
							{
								if (c0 >= 0 && c0 < lnc) *(int16_t *)(xbox_part + 8u) = (int16_t)ln[c0];
								if (c1 >= 0 && c1 < lnc) *(int16_t *)(xbox_part + 10u) = (int16_t)ln[c1];
							}
						}
						((struct vertex_buffer *)(xbox_part + 84u))->type = _rasterizer_vertex_type_model_compressed;

						/* Find matching entry in gbxmodelGeometryParts[] */
						if (has_gparts)
						{
							size_t gp = gparts_arr + 1;
							while (gp < gparts_end)
							{
								size_t gp_start, gp_end;
								uint32_t j_geom = 0xFFFFFFFFu, j_part = 0xFFFFFFFFu;
								while (gp < gparts_end && json[gp] != '{' && json[gp] != ']')
									gp++;
								if (gp >= gparts_end || json[gp] != '{')
									break;
								gp_start = gp;
								if (!ocetags_json_skip_value(json, gp_start, gparts_end, &gp_end))
									break;
								if (ocetags_json_get_u32(json, gp_start, gp_end, "geometryIndex", &j_geom) &&
									ocetags_json_get_u32(json, gp_start, gp_end, "partIndex", &j_part) &&
									j_geom == (uint32_t)g_idx && j_part == (uint32_t)p_idx)
								{
									verts_raw = ocetags_json_decode_base64_key(json, gp_start, gp_end, "vertexData", &verts_bytes);
									if (!verts_raw)
										verts_raw = ocetags_json_decode_base64_key(json, gp_start, gp_end, "vertices", &verts_bytes);
									tris_raw = ocetags_json_decode_base64_key(json, gp_start, gp_end, "triangleData", &tris_bytes);
									if (!tris_raw)
										tris_raw = ocetags_json_decode_base64_key(json, gp_start, gp_end, "triangles", &tris_bytes);
									break;
								}
								gp = gp_end;
							}
						}

						if (verts_raw && tris_raw && verts_bytes >= 3u * 68u && tris_bytes >= 6u)
						{
							int32_t vtx_count = (int32_t)(verts_bytes / 68u);
							int32_t idx_count = (int32_t)(tris_bytes / 2u);
							uint16_t const *raw_ibuf = (uint16_t const *)tris_raw;
							int16_t local_node_count = (int16_t)ce_part[107u];
							uint8_t const *local_nodes = ce_part + 108u;
							int v, c;

							while (idx_count > 3 && raw_ibuf[idx_count - 1] >= (uint16_t)vtx_count)
								idx_count--;

							for (v = 0; v < vtx_count; v++)
							{
								uint8_t *vp = verts_raw + (size_t)v * 68u;
								int16_t n0 = *(int16_t const *)(vp + 56u);
								int16_t n1 = *(int16_t const *)(vp + 58u);
								float w0 = *(float const *)(vp + 60u);
								float w1 = *(float const *)(vp + 64u);

								for (c = 0; c < 9; c++)
								{
									float val = *(float const *)(vp + 12u + (size_t)c * 4u);
									if (!(val >= -1.0f && val <= 1.0f))
										*(float *)(vp + 12u + (size_t)c * 4u) = (c % 3 == 0) ? 1.0f : 0.0f;
								}

								if (has_local_nodes && local_node_count > 0 && local_node_count <= 22)
								{
									n0 = (n0 >= 0 && n0 < local_node_count) ? (int16_t)local_nodes[n0] : 0;
									n1 = (n1 >= 0 && n1 < local_node_count) ? (int16_t)local_nodes[n1] : 0;
								}
								if (node_cnt > 0)
								{
									if (n0 < 0 || n0 >= node_cnt) n0 = 0;
									if (n1 < 0 || n1 >= node_cnt) n1 = 0;
								}
								else
								{
									n0 = 0;
									n1 = 0;
								}
								if (!(w0 >= 0.0f && w0 <= 1.0f) || !(w1 >= 0.0f && w1 <= 1.0f))
								{
									w0 = 1.0f;
									w1 = 0.0f;
								}
								*(int16_t *)(vp + 56u) = n0;
								*(int16_t *)(vp + 58u) = n1;
								*(float *)(vp + 60u) = w0;
								*(float *)(vp + 64u) = w1;
							}

							if (vtx_count >= 3 && idx_count >= 3)
							{
								uint32_t uncomp_sz = (uint32_t)vtx_count * 68u;
								uint32_t comp_sz = (uint32_t)vtx_count * 32u;
								uint32_t idx_sz = (uint32_t)idx_count * 2u;
								void *hw_verts = forge_phys_alloc(comp_sz);
								void *hw_tris = forge_phys_alloc(idx_sz);
								if (hw_verts && hw_tris)
								{
									uint16_t *tris_u16 = (uint16_t *)hw_tris;
									int32_t k;
									rasterizer_geometry_compress_vertices(
										_rasterizer_vertex_type_model_uncompressed,
										vtx_count,
										hw_verts,
										(long)comp_sz,
										verts_raw,
										(long)uncomp_sz);
									memcpy(hw_tris, tris_raw, idx_sz);
									for (k = 0; k < idx_count; k++)
									{
										if (tris_u16[k] >= (uint16_t)vtx_count)
											tris_u16[k] = (k > 0 && tris_u16[k - 1] < (uint16_t)vtx_count) ? tris_u16[k - 1] : 0u;
									}
									rasterizer_vertex_buffer_new(
										(struct vertex_buffer *)(xbox_part + 84u),
										_rasterizer_vertex_type_model_compressed,
										vtx_count,
										hw_verts,
										(long)comp_sz);
									rasterizer_triangle_buffer_new(
										(struct triangle_buffer *)(xbox_part + 68u),
										_triangle_buffer_type_precompiled_strip,
										idx_count - 2,
										hw_tris);
								}
							}
						}

						free(verts_raw);
						free(tris_raw);
						memcpy(parts_base + (size_t)p_idx * 104u, xbox_part, 104u);
					}
				}
			}

			/* Clear _gbxmodel_parts_have_local_nodes_bit (bit 1) now that local nodes are resolved */
			*(uint32_t *)(mbuf + 0x00u) = mflags & ~(1u << 1);
			models_fix_transparent_part_links();
		}
		else if (pt->fourcc == 'antr' && pt->raw_len >= 0x80u)
		{
			/* Sanitize animation_graph object_overlays (+0x00, stride 20) and clamp animations node_count */
			int32_t ov_cnt = *(int32_t const *)(pt->tag_buf + 0x00u);
			uint8_t *ov_base = *(uint8_t **)(pt->tag_buf + 0x04u);
			int32_t anim_cnt = *(int32_t const *)(pt->tag_buf + 0x74u);
			uint8_t *anim_base = *(uint8_t **)(pt->tag_buf + 0x78u);
			int32_t node_cnt = *(int32_t const *)(pt->tag_buf + 0x5Cu);
			int a;

			if (ov_cnt > 0 && ov_base)
			{
				for (a = 0; a < ov_cnt; a++)
				{
					uint8_t *ov = ov_base + (size_t)a * 20u;
					int16_t fn_ctrl = *(int16_t const *)(ov + 0x04u);
					int16_t fn_idx = *(int16_t const *)(ov + 0x06u);
					if (fn_ctrl < 0 || fn_ctrl > 3 || fn_idx < 0 || fn_idx >= 4)
					{
						*(int16_t *)(ov + 0x02u) = -1;
						*(int16_t *)(ov + 0x04u) = 0;
						*(int16_t *)(ov + 0x06u) = 0;
					}
				}
			}
			if (anim_cnt > 0 && anim_base)
			{
				for (a = 0; a < anim_cnt; a++)
				{
					uint8_t *an = anim_base + (size_t)a * 180u;
					int16_t nc = *(int16_t const *)(an + 0x3Au);
					if (nc < 0 || nc > 64 || (node_cnt > 0 && nc > node_cnt))
						*(int16_t *)(an + 0x3Au) = (int16_t)((node_cnt > 0 && node_cnt <= 64) ? node_cnt : 1);
				}
			}
		}
		else if (pt->fourcc == 'weap' && pt->raw_len >= 0x508u)
		{
			/* Convert Custom Edition weapon exported function modes at +0x330 (17->15, 18->16) */
			int f;
			for (f = 0; f < 4; f++)
			{
				int16_t *fn_mode = (int16_t *)(pt->tag_buf + 0x330u + (size_t)f * 2u);
				if (*fn_mode == 17) *fn_mode = 15;
				else if (*fn_mode == 18) *fn_mode = 16;
			}
		}
		else if (pt->fourcc == 'coll' && pt->raw_len >= 0x298u)
		{
			/* Sanitize collision pathfinding box and Mantle 0x0BDEA5CE vertex offsets */
			int pf_i, n;
			int32_t reg_cnt = *(int32_t const *)(pt->tag_buf + 0x240u);
			int32_t cn_cnt = *(int32_t const *)(pt->tag_buf + 0x28Cu);
			uint8_t *cn_base = *(uint8_t **)(pt->tag_buf + 0x290u);

			for (pf_i = 0; pf_i < 6; pf_i++)
			{
				float pfv = *(float const *)(pt->tag_buf + 0x268u + (size_t)pf_i * 4u);
				if (!(pfv >= -500.0f && pfv <= 500.0f))
					*(float *)(pt->tag_buf + 0x268u + (size_t)pf_i * 4u) = (pf_i & 1) ? 2.5f : -2.5f;
			}
			if (cn_cnt > 0 && cn_base)
			{
				for (n = 0; n < cn_cnt; n++)
				{
					uint8_t *cn = cn_base + (size_t)n * 64u;
					int16_t r_idx = *(int16_t const *)(cn + 0x20u);
					int32_t bsp_cnt = *(int32_t const *)(cn + 0x34u);
					uint8_t *bsp_base = *(uint8_t **)(cn + 0x38u);
					int b;

					if (r_idx != -1 && (r_idx < 0 || r_idx >= reg_cnt))
						*(int16_t *)(cn + 0x20u) = (reg_cnt > 0) ? 0 : -1;
					for (b = 0; b < bsp_cnt && bsp_base; b++)
					{
						uint8_t *bsp = bsp_base + (size_t)b * 96u;
						int32_t v_cnt = *(int32_t const *)(bsp + 7u * 12u + 0u);
						uint8_t *v_base = *(uint8_t **)(bsp + 7u * 12u + 4u);
						int v_i, c_i;
						for (v_i = 0; v_i < v_cnt && v_base; v_i++)
						{
							uint8_t *cv = v_base + (size_t)v_i * 16u;
							for (c_i = 0; c_i < 3; c_i++)
							{
								float val = *(float const *)(cv + (size_t)c_i * 4u);
								if (!(val >= -500.0f && val <= 500.0f))
								{
									uint32_t raw_bits = *(uint32_t const *)(cv + (size_t)c_i * 4u);
									uint32_t fixed_bits = raw_bits - 0x0BDEA5CEu;
									float fixed_val = 0.0f;
									if (fixed_bits >= 0x40440000u && fixed_bits <= 0x41B40000u)
										memcpy(&fixed_val, &fixed_bits, 4);
									*(float *)(cv + (size_t)c_i * 4u) = fixed_val;
								}
							}
						}
					}
				}
			}
		}
		else if (pt->fourcc == 'vehi' && pt->raw_len >= 0x3F0u)
		{
			patch_vehicle_tag_for_player_driving(pt->runtime_handle, pt->path);
		}
	}

	root_handle = pkg_tags[0].runtime_handle;
	if (root_handle != NONE && s_dyn_tags.count < FORGE_MAX_DYNAMIC_TAGS)
	{
		struct forge_dynamic_tag_entry *entry = &s_dyn_tags.entries[s_dyn_tags.count++];
		memset(entry, 0, sizeof(*entry));
		entry->group_tag = req_group;
		snprintf(entry->tag_path, sizeof(entry->tag_path), "%s", req_path);
		entry->definition_index = root_handle;
		entry->part_count = 0; /* 0 = let Halo's native render_model render the exact gbxmodel with all nodes/regions/shaders! */
		entry->bounding_radius = (pkg_tags[0].raw_len >= 0x08u) ? *(float const *)(pkg_tags[0].tag_buf + 0x04u) : 1.5f;
		if (!(entry->bounding_radius > 0.1f && entry->bounding_radius < 100.0f))
			entry->bounding_radius = 1.5f;
	}

	if (out_actual_group && root_handle != NONE)
		*out_actual_group = pkg_tags[0].actual_group;

	if (raw_assets)
	{
		for (i = 0; i < raw_asset_count; i++)
			free(raw_assets[i]);
		free(raw_assets);
	}
	free(raw_asset_lens);
	free(pkg_tags);
	return root_handle;
}

static void forge_make_safe_ocetags_stem(uint32_t grp, char const *tag_path, char *out_stem, size_t out_sz);
static uint8_t *forge_read_and_decompress_ocetags_file(char const *path, size_t *out_len, int *out_was_compressed);

static long load_native_ocetags_package_runtime(
	uint32_t req_group,
	char const *req_path,
	uint32_t *out_actual_group)
{
#ifdef _WIN32
	static char const *const s_ocetags_dirs[] = {
		"OpenCE Forge/custom_tags",
		"OpenCE Forge/cached_tags",
		"Faber_data/custom_tags",
		"custom_tags",
		"Faber_data/ocetags",
		"Faber_data/cached_tags",
		"Faber_data",
		"ocetags",
		"custom_maps",
		"maps",
		"mods"
	};
	char safe_stem[128];
	int d;

	if (!req_path || !req_path[0])
		return NONE;

	/* Fast O(1) direct lookup by <safe_stem>.ocetags first */
	forge_make_safe_ocetags_stem(req_group, req_path, safe_stem, sizeof(safe_stem));
	for (d = 0; d < (int)(sizeof(s_ocetags_dirs) / sizeof(s_ocetags_dirs[0])); d++)
	{
		char direct_path[512];
		size_t ulen = 0;
		int was_c = 0;
		uint8_t *ubuf;
		snprintf(direct_path, sizeof(direct_path), "%s/%s.ocetags", s_ocetags_dirs[d], safe_stem);
		ubuf = forge_read_and_decompress_ocetags_file(direct_path, &ulen, &was_c);
		if (ubuf && ulen > 32u)
		{
			long hdl = load_native_ocetags_package_from_json(
				(char const *)ubuf, ulen, req_group, req_path, out_actual_group);
			free(ubuf);
			if (hdl != NONE)
				return hdl;
		}
		else
		{
			free(ubuf);
		}
	}

	for (d = 0; d < (int)(sizeof(s_ocetags_dirs) / sizeof(s_ocetags_dirs[0])); d++)
	{
		char pattern[512];
		WIN32_FIND_DATAA fd;
		HANDLE h;
		snprintf(pattern, sizeof(pattern), "%s/*.ocetags", s_ocetags_dirs[d]);
		h = FindFirstFileA(pattern, &fd);
		if (h != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					char full_path[512];
					FILE *fp;
					snprintf(full_path, sizeof(full_path), "%s/%s", s_ocetags_dirs[d], fd.cFileName);
					fp = fopen(full_path, "rb");
					if (fp)
					{
						long fsz;
						fseek(fp, 0, SEEK_END);
						fsz = ftell(fp);
						fseek(fp, 0, SEEK_SET);
						if (fsz > 16 && fsz <= 128 * 1024 * 1024)
						{
							uint8_t magic_hdr[8];
							size_t mn = fread(magic_hdr, 1, 8, fp);
							fseek(fp, 0, SEEK_SET);
							if (mn == 8 && *(uint32_t const *)magic_hdr == 0x5A45434Fu)
							{
								uint32_t uncomp_sz = *(uint32_t const *)(magic_hdr + 4);
								if (uncomp_sz > 32u && uncomp_sz <= 256u * 1024u * 1024u)
								{
									uint8_t *comp_buf = (uint8_t *)malloc((size_t)fsz);
									char *json_buf = (char *)malloc((size_t)uncomp_sz + 1u);
									if (comp_buf && json_buf && fread(comp_buf, 1, (size_t)fsz, fp) == (size_t)fsz)
									{
										uLongf dst_len = (uLongf)uncomp_sz;
										if (uncompress((Bytef *)json_buf, &dst_len, (Bytef const *)(comp_buf + 8), (uLong)((size_t)fsz - 8u)) == Z_OK)
										{
											json_buf[dst_len] = 0;
											if (strstr(json_buf, "\"formatVersion\"") != NULL)
											{
												long hdl = load_native_ocetags_package_from_json(
													json_buf, (size_t)dst_len, req_group, req_path, out_actual_group);
												if (hdl != NONE)
												{
													free(comp_buf);
													free(json_buf);
													fclose(fp);
													CloseHandle(h);
													return hdl;
												}
											}
										}
									}
									free(comp_buf);
									free(json_buf);
								}
							}
							else if (fsz > 32)
							{
								/* Read a 2048-byte prefix first to quickly check if tags[0].path matches req_path */
								char hdr_peek[2049];
								size_t peek_n = fread(hdr_peek, 1, 2048, fp);
								hdr_peek[peek_n] = 0;
								if (strstr(hdr_peek, "\"formatVersion\"") != NULL)
								{
									char *json_buf;
									fseek(fp, 0, SEEK_SET);
									json_buf = (char *)malloc((size_t)fsz + 1u);
									if (json_buf)
									{
										if (fread(json_buf, 1, (size_t)fsz, fp) == (size_t)fsz)
										{
											long hdl;
											json_buf[fsz] = 0;
											hdl = load_native_ocetags_package_from_json(
												json_buf, (size_t)fsz, req_group, req_path, out_actual_group);
											if (hdl != NONE)
											{
												free(json_buf);
												fclose(fp);
												CloseHandle(h);
												return hdl;
											}
										}
										free(json_buf);
									}
								}
							}
						}
						fclose(fp);
					}
				}
			} while (FindNextFileA(h, &fd));
			CloseHandle(h);
		}
	}
#else
	(void)req_group;
	(void)req_path;
	(void)out_actual_group;
#endif
	return NONE;
}

#define OCETAGS_COMPRESSED_MAGIC 0x5A45434Fu /* 'OCEZ' */

#undef compress2
extern int compress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level);

static int forge_compress_ocetags_buffer_to_file(char const *out_path, uint8_t const *raw_buf, size_t raw_len)
{
	uLongf comp_cap;
	uint8_t *comp_buf;
	uint32_t hdr[2];
	FILE *fp;

	if (!out_path || !raw_buf || raw_len == 0)
		return 0;
	if (raw_len >= 8u && *(uint32_t const *)raw_buf == OCETAGS_COMPRESSED_MAGIC)
	{
		fp = fopen(out_path, "wb");
		if (!fp)
			return 0;
		fwrite(raw_buf, 1, raw_len, fp);
		fclose(fp);
		return 1;
	}
	comp_cap = (uLongf)(raw_len + (raw_len >> 8) + 64u);
	comp_buf = (uint8_t *)malloc((size_t)comp_cap);
	if (!comp_buf)
		return 0;
	if (compress2((Bytef *)comp_buf, &comp_cap, (Bytef const *)raw_buf, (uLong)raw_len, 9) != Z_OK)
	{
		free(comp_buf);
		return 0;
	}
	fp = fopen(out_path, "wb");
	if (!fp)
	{
		free(comp_buf);
		return 0;
	}
	hdr[0] = OCETAGS_COMPRESSED_MAGIC;
	hdr[1] = (uint32_t)raw_len;
	fwrite(hdr, sizeof(uint32_t), 2, fp);
	fwrite(comp_buf, 1, (size_t)comp_cap, fp);
	fclose(fp);
	free(comp_buf);
	return 1;
}

static uint8_t *forge_read_and_decompress_ocetags_file(char const *path, size_t *out_len, int *out_was_compressed)
{
	FILE *fp;
	size_t fsz;
	uint8_t *fbuf;

	if (out_len) *out_len = 0;
	if (out_was_compressed) *out_was_compressed = 0;
	if (!path || !path[0])
		return NULL;
	fp = fopen(path, "rb");
	if (!fp)
		return NULL;
	fseek(fp, 0, SEEK_END);
	fsz = (size_t)ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (fsz < 12u || fsz > 128u * 1024u * 1024u)
	{
		fclose(fp);
		return NULL;
	}
	fbuf = (uint8_t *)malloc(fsz + 1u);
	if (!fbuf)
	{
		fclose(fp);
		return NULL;
	}
	if (fread(fbuf, 1, fsz, fp) != fsz)
	{
		free(fbuf);
		fclose(fp);
		return NULL;
	}
	fclose(fp);
	fbuf[fsz] = 0;

	if (*(uint32_t const *)fbuf == OCETAGS_COMPRESSED_MAGIC)
	{
		uint32_t uncomp_sz = *(uint32_t const *)(fbuf + 4);
		uint8_t *ubuf;
		uLongf dst_len;
		if (out_was_compressed) *out_was_compressed = 1;
		if (uncomp_sz < 16u || uncomp_sz > 256u * 1024u * 1024u)
		{
			free(fbuf);
			return NULL;
		}
		ubuf = (uint8_t *)malloc((size_t)uncomp_sz + 1u);
		if (!ubuf)
		{
			free(fbuf);
			return NULL;
		}
		dst_len = (uLongf)uncomp_sz;
		if (uncompress((Bytef *)ubuf, &dst_len, (Bytef const *)(fbuf + 8), (uLong)(fsz - 8u)) != Z_OK)
		{
			free(ubuf);
			free(fbuf);
			return NULL;
		}
		free(fbuf);
		ubuf[dst_len] = 0;
		if (out_len) *out_len = (size_t)dst_len;
		return ubuf;
	}
	if (out_len) *out_len = fsz;
	return fbuf;
}

/* Index a standalone .ocetags package file (JSON formatVersion 5, OCETAG1\0 header, or OCEZ compressed)
 * in custom_tags/ into custom_catalog.txt without generating any .ftag/.fcol/.fbtm files */
static int forge_rip_ocetags_file_to_cached_tags(
	char const *ocetags_path,
	FILE *manifest_fp,
	FILE *credits_fp)
{
	uint8_t *file_buf = NULL;
	size_t file_size = 0;
	int was_compressed = 0;
	uint64_t manifest_off = 0, manifest_sz = 0, payload_off = 0, payload_sz = 0;
	char const *json;
	size_t json_len;
	char root_group_str[32];
	char root_tag_path[256];
	char custom_item_name[64];
	uint32_t root_group = 0;
	char gstr[5];
	int cat_id;

	if (!ocetags_path || !ocetags_path[0])
		return 0;
	file_buf = forge_read_and_decompress_ocetags_file(ocetags_path, &file_size, &was_compressed);
	if (!file_buf || file_size < 32u)
	{
		free(file_buf);
		return 0;
	}
	/* Automatically OCEZ-compress any uncompressed .ocetags file on disk to save space */
	if (!was_compressed)
		forge_compress_ocetags_buffer_to_file(ocetags_path, file_buf, file_size);

	/* Check for JSON formatVersion 5 (.ocetags starting with '{' or UTF-8 BOM + '{') */
	{
		size_t jstart = 0;
		if (file_size >= 3 && file_buf[0] == 0xEF && file_buf[1] == 0xBB && file_buf[2] == 0xBF)
			jstart = 3;
		while (jstart < file_size && (file_buf[jstart] == ' ' || file_buf[jstart] == '\t' || file_buf[jstart] == '\r' || file_buf[jstart] == '\n'))
			jstart++;
		if (jstart < file_size && file_buf[jstart] == '{')
		{
			int rc = forge_rip_ocetags_v5_json_to_cached_tags(
				ocetags_path,
				(char const *)(file_buf + jstart),
				file_size - jstart,
				manifest_fp,
				credits_fp);
			free(file_buf);
			return rc;
		}
	}

	if (memcmp(file_buf, "OCETAG1\0", 8) != 0)
	{
		free(file_buf);
		return 0;
	}
	memcpy(&manifest_off, file_buf + 16, sizeof(uint64_t));
	memcpy(&manifest_sz, file_buf + 24, sizeof(uint64_t));
	memcpy(&payload_off, file_buf + 32, sizeof(uint64_t));
	memcpy(&payload_sz, file_buf + 40, sizeof(uint64_t));
	if (manifest_off + manifest_sz > file_size || payload_off + payload_sz > file_size)
	{
		free(file_buf);
		return 0;
	}

	json = (char const *)(file_buf + (size_t)manifest_off);
	json_len = (size_t)manifest_sz;

	root_group_str[0] = 0;
	root_tag_path[0] = 0;
	custom_item_name[0] = 0;
	{
		size_t root_pos = 0, root_end = 0;
		if (ocetags_json_find_key_range(json, 0, json_len, "root", &root_pos) &&
			root_pos < json_len && json[root_pos] == '{' &&
			ocetags_find_matching_brace(json, root_pos, json_len, '{', '}', &root_end))
		{
			ocetags_json_get_str(json, root_pos, root_end + 1, "group", root_group_str, sizeof(root_group_str));
			ocetags_json_get_str(json, root_pos, root_end + 1, "path", root_tag_path, sizeof(root_tag_path));
			ocetags_json_get_str(json, root_pos, root_end + 1, "itemName", custom_item_name, sizeof(custom_item_name));
			if (!custom_item_name[0])
				ocetags_json_get_str(json, root_pos, root_end + 1, "displayName", custom_item_name, sizeof(custom_item_name));
			if (!custom_item_name[0])
				ocetags_json_get_str(json, root_pos, root_end + 1, "name", custom_item_name, sizeof(custom_item_name));
		}
		if (!custom_item_name[0])
			ocetags_json_get_str(json, 0, json_len, "itemName", custom_item_name, sizeof(custom_item_name));
		if (!custom_item_name[0])
			ocetags_json_get_str(json, 0, json_len, "displayName", custom_item_name, sizeof(custom_item_name));
	}

	root_group = ocetags_group_str_to_fourcc(root_group_str);
	free(file_buf);

	if (!root_group || !root_tag_path[0])
		return 0;

	/* Check optional <stem>.txt sidecar for custom Forge menu item name */
	if (ocetags_path && ocetags_path[0])
	{
		char txt_path[512];
		char const *dot = strrchr(ocetags_path, '.');
		FILE *tf;
		if (dot && _stricmp(dot, ".ocetags") == 0)
		{
			size_t prefix_len = (size_t)(dot - ocetags_path);
			if (prefix_len > sizeof(txt_path) - 6u)
				prefix_len = sizeof(txt_path) - 6u;
			memcpy(txt_path, ocetags_path, prefix_len);
			memcpy(txt_path + prefix_len, ".txt", 5);
			tf = fopen(txt_path, "rb");
			if (tf)
			{
				char tline[128];
				size_t tn = fread(tline, 1, sizeof(tline) - 1u, tf);
				char *p;
				fclose(tf);
				tline[tn] = 0;
				for (p = tline; *p; p++)
				{
					if (*p == '\r' || *p == '\n')
					{
						*p = 0;
						break;
					}
				}
				p = tline;
				while (*p == ' ' || *p == '\t') p++;
				if (_strnicmp(p, "[c] ", 4) == 0) p += 4;
				else if (_strnicmp(p, "[c]", 3) == 0) p += 3;
				while (*p == ' ' || *p == '\t') p++;
				if (*p)
					snprintf(custom_item_name, sizeof(custom_item_name), "%s", p);
			}
		}
	}

	cat_id = is_forge_light_beacon_emitter_tag(root_group, root_tag_path) ? _forge_cat_lights : _forge_cat_scenery;
	if (root_group == 'vehi') cat_id = _forge_cat_vehicles;
	else if (root_group == 'weap') cat_id = _forge_cat_weapons;
	else if (root_group == 'eqip' || root_group == 'garb') cat_id = _forge_cat_equipment;
	else if (root_group == 'bipd') cat_id = _forge_cat_bipeds;
	else if (root_group == 'mach' || root_group == 'ctrl')
		cat_id = is_forge_light_beacon_emitter_tag(root_group, root_tag_path) ? _forge_cat_lights : _forge_cat_barriers_devices;
	else if (root_group == 'lifi') cat_id = _forge_cat_lights;
	else if (root_group != 'scen') return 0;

	gstr[0] = (char)((root_group >> 24) & 0xFF);
	gstr[1] = (char)((root_group >> 16) & 0xFF);
	gstr[2] = (char)((root_group >> 8) & 0xFF);
	gstr[3] = (char)(root_group & 0xFF);
	gstr[4] = 0;

	if (catalog_file_has_ocetags_entry(gstr, root_tag_path))
		return 0;

	if (manifest_fp)
	{
		if (custom_item_name[0])
			fprintf(manifest_fp, "%s|%d|2|%s|%s\n", gstr, cat_id, root_tag_path, custom_item_name);
		else
			fprintf(manifest_fp, "%s|%d|2|%s\n", gstr, cat_id, root_tag_path);
		fflush(manifest_fp);
	}
	if (credits_fp)
	{
		fprintf(credits_fp, "- **`%s`** (`%s`) — native `.ocetags` package `%s`\r\n",
			root_tag_path, gstr, ocetags_path);
		fflush(credits_fp);
	}
	return 1;
}

static int forge_scan_and_rip_ocetags_folders(int force_reload)
{
	int extracted = 0;
#ifdef _WIN32
	static char const *const s_ocetags_dirs[] = {
		"OpenCE Forge/custom_tags",
		"Faber_data/custom_tags",
		"custom_tags"
	};
	char found_files[256][512];
	int found_count = 0;
	int d, f_idx;
	FILE *manifest_fp = NULL;
	FILE *credits_fp = NULL;

	CreateDirectoryA("OpenCE Forge", NULL);
	CreateDirectoryA("OpenCE Forge/custom_tags", NULL);
	CreateDirectoryA("OpenCE Forge/cached_tags", NULL);
	CreateDirectoryA("Faber_data", NULL);
	CreateDirectoryA("Faber_data/custom_tags", NULL);
	CreateDirectoryA("Faber_data/cached_tags", NULL);

	for (d = 0; d < (int)(sizeof(s_ocetags_dirs) / sizeof(s_ocetags_dirs[0])); d++)
	{
		char pattern[512];
		WIN32_FIND_DATAA fd;
		HANDLE h;
		snprintf(pattern, sizeof(pattern), "%s/*.ocetags", s_ocetags_dirs[d]);
		h = FindFirstFileA(pattern, &fd);
		if (h != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && found_count < 256)
				{
					snprintf(found_files[found_count++], sizeof(found_files[0]), "%s/%s", s_ocetags_dirs[d], fd.cFileName);
				}
			} while (FindNextFileA(h, &fd));
			CloseHandle(h);
		}
	}

	manifest_fp = fopen("Faber_data/cached_tags/custom_catalog.txt", "ab");
	if (!manifest_fp)
		manifest_fp = fopen("Faber_data/cached_tags/custom_catalog.txt", "wb");
	credits_fp = fopen("Faber_data/credits.md", "ab");

	if (force_reload)
		forge_xbox_loading_screen_begin();

	for (f_idx = 0; f_idx < found_count; f_idx++)
	{
		if (force_reload)
			forge_xbox_loading_screen_update((float)(f_idx + 1) / (float)(found_count > 0 ? found_count : 1));
		extracted += forge_rip_ocetags_file_to_cached_tags(found_files[f_idx], manifest_fp, credits_fp);
	}

	if (force_reload)
		forge_xbox_loading_screen_end();

	if (credits_fp)
		fclose(credits_fp);
	if (manifest_fp)
		fclose(manifest_fp);
#else
	(void)force_reload;
#endif
	return extracted;
}

static int is_ce_v609_map_file(char const *path)
{
	FILE *f = fopen(path, "rb");
	uint32_t hdr[2];
	if (!f)
		return 0;
	if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr))
	{
		fclose(f);
		return 0;
	}
	fclose(f);
	return (hdr[0] == 0x68656164u && hdr[1] == 609u) ? 1 : 0;
}

static int extract_embedded_forge_base_map_stem(char const *map_path, char *out_stem, size_t out_sz)
{
	FILE *f;
	uint8_t hdr[0x200];
	uint32_t state_off = 0, state_sz = 0, sig = 0;

	if (!map_path || !out_stem || out_sz == 0)
		return 0;
	out_stem[0] = 0;
	f = fopen(map_path, "rb");
	if (!f)
		return 0;
	if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr))
	{
		fclose(f);
		return 0;
	}
	memcpy(&state_off, hdr + 0x1E4u, 4);
	memcpy(&state_sz, hdr + 0x1E8u, 4);
	memcpy(&sig, hdr + 0x1FCu, 4);
	if (sig == 0x464C4731u && state_off >= 0x800u && state_sz > 0 && state_sz < 4u * 1024u * 1024u)
	{
		uint32_t read_sz = (state_sz < 1024u) ? state_sz : 1024u;
		char buf[1025];
		if (fseek(f, (long)state_off, SEEK_SET) == 0 && fread(buf, 1, read_sz, f) == read_sz)
		{
			char const *p;
			buf[read_sz] = 0;
			p = strstr(buf, "Base Map: ");
			if (p)
			{
				size_t i = 0;
				p += 10;
				while (p[i] && p[i] != '\r' && p[i] != '\n' && i + 1 < out_sz)
				{
					out_stem[i] = p[i];
					i++;
				}
				out_stem[i] = 0;
			}
		}
	}
	fclose(f);
	return out_stem[0] ? 1 : 0;
}

static int try_resolve_clean_ce_map_by_stem(char const *stem, char *out_path, size_t out_sz)
{
	char candidate[512];
	char h_xbox[128];
	FILE *f;
	if (!stem || !stem[0])
		return 0;

	snprintf(h_xbox, sizeof(h_xbox), "h:\\maps\\%s.map", stem);
	platform_translate_path(h_xbox, candidate, sizeof(candidate));
	if (is_ce_v609_map_file(candidate)) { snprintf(out_path, out_sz, "%s", candidate); return 1; }

	snprintf(candidate, sizeof(candidate), "C:/Program Files (x86)/Microsoft Games/Halo Custom Edition/maps/%s.map", stem);
	if (is_ce_v609_map_file(candidate)) { snprintf(out_path, out_sz, "%s", candidate); return 1; }

	snprintf(candidate, sizeof(candidate), "C:/Program Files/Microsoft Games/Halo Custom Edition/maps/%s.map", stem);
	if (is_ce_v609_map_file(candidate)) { snprintf(out_path, out_sz, "%s", candidate); return 1; }

	snprintf(candidate, sizeof(candidate), "custom_maps/%s.map", stem);
	if (is_ce_v609_map_file(candidate)) { snprintf(out_path, out_sz, "%s", candidate); return 1; }

	snprintf(candidate, sizeof(candidate), "maps/%s.map", stem);
	if ((f = fopen(candidate, "rb")) != NULL) { fclose(f); snprintf(out_path, out_sz, "%s", candidate); return 1; }

	return 0;
}

char const *forge_get_active_map_name(void)
{
	char const *loaded = cache_file_loaded_map_name();
	if (loaded && loaded[0] && _stricmp(loaded, "ui") != 0 && _stricmp(loaded, "levels\\ui\\ui") != 0)
		return loaded;
	return main_get_map_name();
}

static void resolve_current_base_map_path(char *out_path, size_t out_sz)
{
	char const *ce_loaded = custom_edition_cache_loaded_path();
	char const *map_leaf;
	char base_stem[64];
	FILE *f;

	if (ce_loaded && ce_loaded[0])
	{
		char const *rel_ce = ce_loaded;
		if ((rel_ce[0] == 'd' || rel_ce[0] == 'D') && rel_ce[1] == ':' && (rel_ce[2] == '\\' || rel_ce[2] == '/'))
			rel_ce += 3;
		if (is_ce_v609_map_file(rel_ce))
		{
			if (extract_embedded_forge_base_map_stem(rel_ce, base_stem, sizeof(base_stem)) &&
				_stricmp(base_stem, rel_ce) != 0 &&
				try_resolve_clean_ce_map_by_stem(base_stem, out_path, out_sz))
			{
				return;
			}
			snprintf(out_path, out_sz, "%s", rel_ce);
			return;
		}
	}

	map_leaf = forge_get_active_map_name();
	if (map_leaf && map_leaf[0])
	{
		char const *slash = strrchr(map_leaf, '\\');
		char candidate[512];
		if (!slash)
			slash = strrchr(map_leaf, '/');
		if (slash)
			map_leaf = slash + 1;

		snprintf(candidate, sizeof(candidate), "custom_maps/%s.map", map_leaf);
		if (extract_embedded_forge_base_map_stem(candidate, base_stem, sizeof(base_stem)) &&
			_stricmp(base_stem, map_leaf) != 0 &&
			try_resolve_clean_ce_map_by_stem(base_stem, out_path, out_sz))
		{
			return;
		}
		if (is_ce_v609_map_file(candidate))
		{
			snprintf(out_path, out_sz, "%s", candidate);
			return;
		}
		if (try_resolve_clean_ce_map_by_stem(map_leaf, out_path, out_sz))
			return;
	}

	if (try_resolve_clean_ce_map_by_stem("bloodgulch", out_path, out_sz))
		return;
	if ((f = fopen("maps/bloodgulch.map", "rb")) != NULL)
	{
		fclose(f);
		snprintf(out_path, out_sz, "maps/bloodgulch.map");
		return;
	}
	snprintf(out_path, out_sz, "maps/bloodgulch.map");
}

int forge_ui_save_copy_as(char const *new_map_name)
{
	char base_map_path[512];
	char clean_name[64];
	int was_already_saved = g_forge.has_saved_map_once;
	int ok;

	if (!new_map_name || !new_map_name[0])
	{
		char const *cur = forge_get_active_map_name();
		char const *slash = cur ? strrchr(cur, '\\') : NULL;
		if (!slash && cur)
			slash = strrchr(cur, '/');
		snprintf(clean_name, sizeof(clean_name), "%s_forge", slash ? (slash + 1) : ((cur && cur[0]) ? cur : "bloodgulch"));
	}
	else
	{
		snprintf(clean_name, sizeof(clean_name), "%s", new_map_name);
	}

	resolve_current_base_map_path(base_map_path, sizeof(base_map_path));

#ifdef _WIN32
	CreateDirectoryA("custom_maps", NULL);
#endif

	ok = forge_export_scenario_map(
		base_map_path,
		&g_forge.edit_state,
		clean_name);

	if (ok)
	{
		extern void custom_edition_maps_look_again(void);
		custom_edition_maps_look_again();
		snprintf(g_forge.save_map_name, sizeof(g_forge.save_map_name), "%s", clean_name);
		g_forge.save_map_name_len = (int)strlen(g_forge.save_map_name);
		g_forge.has_saved_map_once = 1;

		if (was_already_saved)
		{
			forge_set_status("Updated saved map 'custom_maps/%s.map' (%d placed, %d default edits)!",
				clean_name,
				(int)g_forge.edit_state.placed_object_count,
				(int)g_forge.edit_state.default_edit_count);
		}
		else
		{
			forge_set_status("Initial save complete: 'custom_maps/%s.map'! Press F5 anytime to quick-update.",
				clean_name);
		}
		return 1;
	}
	forge_set_status("ERROR: Failed to export custom_maps/%s.map", clean_name);
	return 0;
}

void forge_trigger_save_map_action(void)
{
	/* If an initial save has already been done in this session (or loaded from Edit Maps), quick-update immediately! */
	if (g_forge.has_saved_map_once && g_forge.save_map_name[0])
	{
		g_forge.active_modal = _forge_modal_none;
		forge_ui_save_copy_as(g_forge.save_map_name);
		return;
	}

	/* Otherwise, open the initial save prompt so the user can name their custom map */
	if (!g_forge.save_map_name[0])
	{
		char const *cur = forge_get_active_map_name();
		char const *slash = cur ? strrchr(cur, '\\') : NULL;
		if (!slash && cur)
			slash = strrchr(cur, '/');
		snprintf(g_forge.save_map_name, sizeof(g_forge.save_map_name), "%s_forge", slash ? (slash + 1) : ((cur && cur[0]) ? cur : "custom"));
		g_forge.save_map_name_len = (int)strlen(g_forge.save_map_name);
	}
	g_forge.active_modal = _forge_modal_save_copy_as;
}

static void parse_forge_state_text_buffer(char const *text_buf, char const *leaf)
{
	char const *cur = text_buf;
	char line[512];
	int in_forge_state = 0;

	if (!cur)
		return;

	while (*cur)
	{
		size_t len = 0;
		while (cur[len] && cur[len] != '\r' && cur[len] != '\n')
			len++;
		if (len >= sizeof(line))
			len = sizeof(line) - 1;
		memcpy(line, cur, len);
		line[len] = 0;
		cur += len;
		while (*cur == '\r' || *cur == '\n')
			cur++;

		if (strcmp(line, "[OpenCE Forge Map]") == 0)
		{
			/* This map is already a saved Forge map: enable quick-update on F5! */
			snprintf(g_forge.save_map_name, sizeof(g_forge.save_map_name), "%s", leaf);
			g_forge.save_map_name_len = (int)strlen(g_forge.save_map_name);
			g_forge.has_saved_map_once = 1;
		}
		else if (strcmp(line, "[ForgeState]") == 0)
		{
			in_forge_state = 1;
		}
		else if (in_forge_state && strncmp(line, "OBJECT|", 7) == 0)
		{
			unsigned int grp = 0;
			int src_cache = 0, baked = 0, team = 0;
			float x = 0, y = 0, z = 0, yaw = 0, pitch = 0, roll = 0;
			char tpath[256];
			tpath[0] = 0;
			if (sscanf(line + 7, "%u|%d|%d|%f|%f|%f|%f|%f|%f|%d|%255[^\r\n]",
					&grp, &src_cache, &baked, &x, &y, &z, &yaw, &pitch, &roll, &team, tpath) == 11 && tpath[0])
			{
				if (!baked)
				{
					/* Cached tag object not baked into base_map: spawn live now! */
					forge_3d_spawn_object_at((uint32_t)grp, tpath, (uint8_t)src_cache, x, y, z, yaw, pitch, roll, (int16_t)team, 0);
				}
				else
				{
					struct object_iterator iter;
					struct object_datum *obj;
					long found_oidx = NONE;
					float best_d2 = 36.0f;

					object_iterator_new(&iter, (unsigned long)-1, 0);
					while ((obj = object_iterator_next(&iter)) != NULL)
					{
						int li, already_claimed = 0;
						float dx, dy, dz, d2;
						char const *oname = NULL;
						if (obj->object.parent_object_index != NONE)
							continue;
						if (obj->object.type == _object_type_biped &&
							((struct unit_datum *)obj)->unit.player_index != NONE)
							continue;
						for (li = 0; li < g_forge.live_instance_count; li++)
						{
							if (g_forge.live_instances[li].active &&
								g_forge.live_instances[li].object_index == iter.index)
							{
								already_claimed = 1;
								break;
							}
						}
						if (already_claimed)
							continue;
						if (obj->definition_index != NONE)
							oname = tag_get_name(obj->definition_index);
						if (oname && oname[0])
						{
							size_t tlen = strlen(tpath);
							if (_stricmp(oname, tpath) != 0 &&
								_strnicmp(oname, tpath, tlen) != 0 &&
								!((uint32_t)grp == 'vehi' && _strnicmp(oname, "forge\\vehicles\\placed_unit_", 27) == 0))
								continue;
						}
						dx = obj->object.position.x - x;
						dy = obj->object.position.y - y;
						dz = obj->object.position.z - z;
						/* Physics objects (vehicles/weapons) spawn with +bounding_sphere_radius*0.5f on Z */
						if (dx * dx + dy * dy > 9.0f || (float)fabs((double)dz) > 6.0f)
							continue;
						d2 = dx * dx + dy * dy + dz * dz * 0.25f;
						if (d2 < best_d2)
						{
							best_d2 = d2;
							found_oidx = iter.index;
						}
					}

					if (found_oidx == NONE)
					{
						/* Not yet spawned by scenario: spawn live and record in placed_objects */
						forge_3d_spawn_object_at((uint32_t)grp, tpath, (uint8_t)src_cache, x, y, z, yaw, pitch, roll, (int16_t)team, 0);
					}
					else if (g_forge.edit_state.placed_object_count < FORGE_MAX_PLACED_OBJECTS)
					{
						/* Record baked object in edit_state and link to its live instance */
						int32_t po_idx = g_forge.edit_state.placed_object_count++;
						struct forge_placed_object_entry *po = &g_forge.edit_state.placed_objects[po_idx];
						struct object_datum *fobj = (struct object_datum *)object_try_and_get_and_verify_type(found_oidx, (unsigned long)-1);

						memset(po, 0, sizeof(*po));
						po->active = 1;
						po->group_tag = (uint32_t)grp;
						po->source_cache_id = (uint8_t)src_cache;
						po->category = (grp == 'vehi') ? _forge_object_vehicle :
							(grp == 'weap') ? _forge_object_weapon :
							(grp == 'bipd') ? _forge_object_biped :
							(grp == 'eqip') ? _forge_object_equipment : _forge_object_scenery;
						snprintf(po->tag_path, sizeof(po->tag_path), "%s", tpath);
						po->x = x; po->y = y; po->z = z;
						po->yaw = yaw; po->pitch = pitch; po->roll = roll;
						po->team_index = (int16_t)team;

						if (fobj && fobj->definition_index != NONE)
						{
							if ((uint32_t)grp == 'vehi')
								patch_vehicle_tag_for_player_driving(fobj->definition_index, tpath);
							else
								patch_forge_device_and_light_tag_runtime(fobj->definition_index, (uint32_t)grp, tpath);
						}

						if (g_forge.live_instance_count < FORGE_MAX_LIVE_INSTANCES)
						{
							int32_t li_idx = g_forge.live_instance_count++;
							struct forge_live_instance *inst = &g_forge.live_instances[li_idx];
							memset(inst, 0, sizeof(*inst));
							inst->active = 1;
							inst->object_index = found_oidx;
							inst->actor_index = NONE;
							inst->edit_entry_index = po_idx;
							inst->group_tag = (uint32_t)grp;
							inst->source_cache_id = (uint8_t)src_cache;
							inst->is_default_scenario_object = 0;
							inst->team_designator = (int16_t)team;
							inst->x = x; inst->y = y; inst->z = z;
							inst->yaw = yaw; inst->pitch = pitch; inst->roll = roll;
							snprintf(inst->tag_path, sizeof(inst->tag_path), "%s", tpath);
							forge_3d_move_instance(li_idx, x, y, z, yaw, pitch, roll, 0);
						}
					}
				}
			}
		}
		else if (in_forge_state && strncmp(line, "AI|", 3) == 0)
		{
			float x = 0, y = 0, z = 0, yaw = 0;
			int team = 0;
			char tpath[256];
			tpath[0] = 0;
			if (sscanf(line + 3, "%f|%f|%f|%f|%d|%255[^\r\n]", &x, &y, &z, &yaw, &team, tpath) == 6 && tpath[0])
			{
				forge_3d_spawn_object_at('actv', tpath, 0, x, y, z, yaw, 0.0f, 0.0f, (int16_t)team, 0);
			}
		}
		else if (in_forge_state && strncmp(line, "FF_SPAWN|", 9) == 0)
		{
			float x = 0, y = 0, z = 0, yaw = 0;
			int usage_id = 0;
			if (sscanf(line + 9, "%f|%f|%f|%f|%d", &x, &y, &z, &yaw, &usage_id) == 5)
			{
				int li, already_exists = 0;
				for (li = 0; li < g_forge.live_instance_count; li++)
				{
					if (g_forge.live_instances[li].active &&
						g_forge.live_instances[li].group_tag == 'ffsp')
					{
						float dx = g_forge.live_instances[li].x - x;
						float dy = g_forge.live_instances[li].y - y;
						float dz = g_forge.live_instances[li].z - z;
						if (dx * dx + dy * dy + dz * dz < 0.25f)
						{
							already_exists = 1;
							break;
						}
					}
				}
				if (!already_exists)
				{
					forge_3d_spawn_object_at('ffsp', "spawn\\firefight_spawn", 1, x, y, z, yaw, 0.0f, 0.0f, (int16_t)usage_id, 0);
				}
			}
		}
		else if (in_forge_state && strncmp(line, "DEFAULT_EDIT|", 13) == 0)
		{
			unsigned int grp = 0;
			int deleted = 0;
			float ox = 0, oy = 0, oz = 0, nx = 0, ny = 0, nz = 0, nyaw = 0, npitch = 0, nroll = 0;
			char tpath[256];
			tpath[0] = 0;
			if (sscanf(line + 13, "%u|%d|%f|%f|%f|%f|%f|%f|%f|%f|%f|%255[^\r\n]",
					&grp, &deleted, &ox, &oy, &oz, &nx, &ny, &nz, &nyaw, &npitch, &nroll, tpath) == 12 &&
				g_forge.edit_state.default_edit_count < FORGE_MAX_DEFAULT_OBJECT_EDITS)
			{
				struct forge_default_object_edit *de = &g_forge.edit_state.default_edits[g_forge.edit_state.default_edit_count++];
				struct object_iterator iter;
				struct object_datum *obj;
				long found_oidx = NONE;
				float best_d2 = 9.0f;

				memset(de, 0, sizeof(*de));
				de->active = 1;
				de->group_tag = (uint32_t)grp;
				de->deleted = (uint8_t)deleted;
				de->orig_x = ox; de->orig_y = oy; de->orig_z = oz;
				de->new_x = nx; de->new_y = ny; de->new_z = nz;
				de->new_yaw = nyaw; de->new_pitch = npitch; de->new_roll = nroll;
				snprintf(de->tag_path, sizeof(de->tag_path), "%s", tpath);

				/* Apply default edit live if the scenario object is still at its unedited origin (e.g. Xbox v5 FLG1 container) */
				object_iterator_new(&iter, (unsigned long)-1, 0);
				while ((obj = object_iterator_next(&iter)) != NULL)
				{
					float dx = obj->object.position.x - ox;
					float dy = obj->object.position.y - oy;
					float dz = obj->object.position.z - oz;
					float d2 = dx * dx + dy * dy + dz * dz * 0.25f;
					if (obj->object.parent_object_index != NONE)
						continue;
					if (obj->object.type == _object_type_biped &&
						((struct unit_datum *)obj)->unit.player_index != NONE)
						continue;
					if (d2 < best_d2)
					{
						best_d2 = d2;
						found_oidx = iter.index;
					}
				}
				if (found_oidx != NONE)
				{
					if (deleted)
					{
						object_delete(found_oidx);
					}
					else
					{
						struct object_datum *fobj = (struct object_datum *)object_try_and_get_and_verify_type(found_oidx, (unsigned long)-1);
						if (fobj)
						{
							fobj->object.position.x = nx;
							fobj->object.position.y = ny;
							fobj->object.position.z = nz;
						}
					}
				}
			}
		}
	}
}

static void forge_SyncScenarioTagsWithLoadedMap(void)
{
	int r, w = 0;
	long total = 0, i;
	struct forge_cache_file_tag_instance *instances;

	/* 1. Strip any transient map-local entries (source_cache_id == 0) from previous map loads */
	for (r = 0; r < g_forge.catalog_count; r++)
	{
		if (g_forge.catalog[r].source_cache_id != 0)
		{
			if (w != r)
				g_forge.catalog[w] = g_forge.catalog[r];
			w++;
		}
	}
	g_forge.catalog_count = w;

	/* 2. Scan the loaded map's tag instances and append uncached object tags with source_cache_id = 0 ("[MAP] ...") */
	instances = (struct forge_cache_file_tag_instance *)cache_files_tag_instances(&total);
	if (!instances || total <= 0)
		return;

	for (i = 0; i < total; i++)
	{
		uint32_t grp = instances[i].group_tag;
		char const *tpath = instances[i].name;
		uint8_t cat_id;
		int k, exists = 0;

		if (!tpath || !tpath[0] || !instances[i].base_address)
			continue;
		if (_strnicmp(tpath, "forge\\markers\\", 14) == 0 ||
		    _strnicmp(tpath, "forge/markers/", 14) == 0)
		{
			continue;
		}

		if (grp == 'weap')
		{
			if (is_excluded_weapon_or_equipment_tag(tpath))
				continue;
			cat_id = _forge_cat_weapons;
		}
		else if (grp == 'eqip' || grp == 'garb')
		{
			if (is_excluded_weapon_or_equipment_tag(tpath))
				continue;
			cat_id = _forge_cat_equipment;
		}
		else if (grp == 'vehi')
		{
			cat_id = _forge_cat_vehicles;
		}
		else if (grp == 'lifi')
		{
			cat_id = _forge_cat_lights;
		}
		else if (grp == 'mach' || grp == 'ctrl')
		{
			cat_id = is_forge_light_beacon_emitter_tag(grp, tpath)
				? _forge_cat_lights
				: _forge_cat_barriers_devices;
		}
		else if (grp == 'scen')
		{
			if (is_excluded_scenery_or_effect_tag(tpath))
				continue;
			cat_id = is_forge_light_beacon_emitter_tag(grp, tpath)
				? _forge_cat_lights
				: _forge_cat_scenery;
		}
		else
		{
			continue;
		}

		for (k = 0; k < g_forge.catalog_count; k++)
		{
			if (g_forge.catalog[k].group_tag == grp &&
			    _stricmp(g_forge.catalog[k].tag_path, tpath) == 0)
			{
				exists = 1;
				break;
			}
		}
		if (!exists && g_forge.catalog_count < FORGE_MAX_CATALOG_ENTRIES)
		{
			add_catalog_entry(grp, cat_id, 0, tpath, "[MAP]");
		}
	}
}

static void normalize_hud_placement_at(uint8_t *placement, int halve_offset, int halve_scale)
{
	if (!placement)
		return;
	if (halve_offset)
	{
		int16_t ox = *(int16_t *)(placement + 0x00);
		int16_t oy = *(int16_t *)(placement + 0x02);
		*(int16_t *)(placement + 0x00) = (int16_t)(ox / 2);
		*(int16_t *)(placement + 0x02) = (int16_t)(oy / 2);
	}
	if (halve_scale)
	{
		float sx = *(float *)(placement + 0x04);
		float sy = *(float *)(placement + 0x08);
		if (sx == 0.0f)
			sx = 0.5f;
		else if (sx > 0.6f)
			sx *= 0.5f;
		if (sy == 0.0f)
			sy = 0.5f;
		else if (sy > 0.6f)
			sy *= 0.5f;
		*(float *)(placement + 0x04) = sx;
		*(float *)(placement + 0x08) = sy;
		/* Clear _hud_use_high_resolution_scale_bit (bit 2) so placement is not halved twice */
		*(uint16_t *)(placement + 0x0C) &= (uint16_t)~(1u << 2);
	}
}

static int static_hud_element_follows_zoom(uint8_t const *static_elem)
{
	int32_t ov_count;
	uint8_t const *ov_elems;
	int32_t oi;

	if (!static_elem)
		return 0;
	/* multitexture_overlays tag_block is at +0x58 inside static_hud_element_definition (stride 0x1E0) */
	ov_count = *(int32_t const *)(static_elem + 0x58);
	ov_elems = (uint8_t const *)(uintptr_t)(*(uint32_t const *)(static_elem + 0x5C));
	if (ov_count <= 0 || ov_count > 32 || !ov_elems)
		return 0;
	for (oi = 0; oi < ov_count; oi++)
	{
		uint8_t const *ov = ov_elems + (uint32_t)oi * 0x1E0u;
		int32_t fn_count = *(int32_t const *)(ov + 0x154u);
		uint8_t const *fn_elems = (uint8_t const *)(uintptr_t)(*(uint32_t const *)(ov + 0x158u));
		int32_t fi;
		if (fn_count <= 0 || fn_count > 32 || !fn_elems)
			continue;
		for (fi = 0; fi < fn_count; fi++)
		{
			uint8_t const *eff = fn_elems + (uint32_t)fi * 0xDCu;
			int16_t source = *(int16_t const *)(eff + 0x44u);
			if (source == 7 /* _hud_multitexture_overlay_effector_source_zoom_level */)
				return 1;
		}
	}
	return 0;
}

static int hud_tag_ref_is_fullscreen_scope_mask(
	struct forge_cache_file_tag_instance const *instances,
	long total,
	uint8_t const *tag_ref_ptr)
{
	int32_t ref_idx;
	uint32_t abs_idx;
	uint8_t const *bitm_base;
	int32_t b_cnt;
	uint8_t const *b_elems;

	if (!instances || !tag_ref_ptr)
		return 0;
	ref_idx = *(int32_t const *)(tag_ref_ptr + 0x0C);
	if (ref_idx == NONE)
		return 0;
	abs_idx = (uint32_t)ref_idx & 0xFFFFu;
	if ((long)abs_idx >= total || instances[abs_idx].group_tag != 'bitm' || !instances[abs_idx].base_address)
		return 0;
	bitm_base = (uint8_t const *)instances[abs_idx].base_address;
	b_cnt = *(int32_t const *)(bitm_base + 0x60u);
	b_elems = (uint8_t const *)(uintptr_t)(*(uint32_t const *)(bitm_base + 0x64u));
	if (b_cnt > 0 && b_cnt <= 64 && b_elems)
	{
		uint16_t w = *(uint16_t const *)(b_elems + 0x04u);
		uint16_t h = *(uint16_t const *)(b_elems + 0x06u);
		if (w >= 512u && h >= 512u)
			return 1;
	}
	return 0;
}

int forge_is_mcc_hud_active(void)
{
	return s_mcc_hud_active;
}

void forge_normalize_mcc_hud_tags(void)
{
	long total = 0, i;
	struct forge_cache_file_tag_instance *instances;
	int mcc_detected = 0;

	s_mcc_hud_active = 0;
	instances = (struct forge_cache_file_tag_instance *)cache_files_tag_instances(&total);
	if (!instances || total <= 0)
		return;

	/* 1. Detect MCC 2x HUD on hud# tags EXCLUDING ui\hud\counter (which is 24/18 on all stock maps) */
	for (i = 0; i < total; i++)
	{
		if (instances[i].group_tag == 'hud#' && instances[i].base_address && instances[i].name)
		{
			uint8_t const *p;
			int8_t char_w, scr_w;
			if (_stricmp(instances[i].name, "ui\\hud\\counter") == 0 ||
			    _stricmp(instances[i].name, "ui/hud/counter") == 0)
			{
				continue;
			}
			p = (uint8_t const *)instances[i].base_address;
			char_w = *(int8_t const *)(p + 0x10);
			scr_w  = *(int8_t const *)(p + 0x11);
			if (char_w >= 20 || scr_w >= 16)
			{
				mcc_detected = 1;
				break;
			}
		}
	}

	if (!mcc_detected)
		return;

	s_mcc_hud_active = 1;

	/* 2. Normalize non-counter hud#, hudg, wphi, grhi, and unhi tags */
	for (i = 0; i < total; i++)
	{
		uint32_t grp = instances[i].group_tag;
		uint8_t *base = (uint8_t *)instances[i].base_address;
		if (!base)
			continue;

		if (grp == 'hud#')
		{
			int8_t char_w, scr_w, x_off, y_off, dec_w, col_w;
			if (instances[i].name &&
			    (_stricmp(instances[i].name, "ui\\hud\\counter") == 0 ||
			     _stricmp(instances[i].name, "ui/hud/counter") == 0))
			{
				continue;
			}
			char_w = *(int8_t *)(base + 0x10);
			scr_w  = *(int8_t *)(base + 0x11);
			if (char_w >= 20 || scr_w >= 16)
			{
				x_off = *(int8_t *)(base + 0x12);
				y_off = *(int8_t *)(base + 0x13);
				dec_w = *(int8_t *)(base + 0x14);
				col_w = *(int8_t *)(base + 0x15);
				*(int8_t *)(base + 0x10) = (int8_t)(((int)char_w + 1) / 2);
				*(int8_t *)(base + 0x11) = (int8_t)(((int)scr_w + 1) / 2);
				*(int8_t *)(base + 0x12) = (int8_t)((int)x_off / 2);
				*(int8_t *)(base + 0x13) = (int8_t)((int)y_off / 2);
				*(int8_t *)(base + 0x14) = (int8_t)(((int)dec_w + 1) / 2);
				*(int8_t *)(base + 0x15) = (int8_t)(((int)col_w + 1) / 2);
			}
		}
		else if (grp == 'hudg')
		{
			int32_t btn_cnt, bi;
			uint8_t *btn_elems;
			float ms_scale;

			/* messaging.placement (+0x024) */
			normalize_hud_placement_at(base + 0x024, 1, 1);
			/* messaging.button_icons (+0x0A8, stride 0x10) */
			btn_cnt = *(int32_t *)(base + 0x0A8);
			btn_elems = (uint8_t *)(uintptr_t)(*(uint32_t *)(base + 0x0AC));
			if (btn_cnt > 0 && btn_cnt <= 64 && btn_elems)
			{
				for (bi = 0; bi < btn_cnt; bi++)
				{
					uint8_t *ic = btn_elems + (uint32_t)bi * 0x10u;
					*(int16_t *)(ic + 0x02) = (int16_t)(*(int16_t *)(ic + 0x02) / 2);
					*(int16_t *)(ic + 0x04) = (int16_t)(*(int16_t *)(ic + 0x04) / 2);
					*(int16_t *)(ic + 0x06) = (int16_t)(*(int16_t *)(ic + 0x06) / 2);
				}
			}
			/* waypoint offsets (+0x120..+0x12C) */
			*(float *)(base + 0x120) *= 0.5f;
			*(float *)(base + 0x124) *= 0.5f;
			*(float *)(base + 0x128) *= 0.5f;
			*(float *)(base + 0x12C) *= 0.5f;
			/* defaults.motion_sensor_scale (+0x2D8) */
			ms_scale = *(float *)(base + 0x2D8);
			if (ms_scale > 0.6f)
				*(float *)(base + 0x2D8) = ms_scale * 0.5f;
			/* damage_indicators offsets (+0x310..+0x316) */
			*(int16_t *)(base + 0x310) = (int16_t)(*(int16_t *)(base + 0x310) / 2);
			*(int16_t *)(base + 0x312) = (int16_t)(*(int16_t *)(base + 0x312) / 2);
			*(int16_t *)(base + 0x314) = (int16_t)(*(int16_t *)(base + 0x314) / 2);
			*(int16_t *)(base + 0x316) = (int16_t)(*(int16_t *)(base + 0x316) / 2);
		}
		else if (grp == 'wphi')
		{
			int32_t cnt, k;
			uint8_t *elems;

			/* statics (+0x60, stride 0xB4, static_element at +0x24) */
			cnt = *(int32_t *)(base + 0x60);
			elems = (uint8_t *)(uintptr_t)(*(uint32_t *)(base + 0x64));
			if (cnt > 0 && cnt <= 64 && elems)
			{
				for (k = 0; k < cnt; k++)
				{
					uint8_t *st = elems + (uint32_t)k * 0xB4u + 0x24u;
					if (!static_hud_element_follows_zoom(st))
						normalize_hud_placement_at(st, 1, 1);
				}
			}
			/* meters (+0x6C, stride 0xB4, meter_element at +0x24) */
			cnt = *(int32_t *)(base + 0x6C);
			elems = (uint8_t *)(uintptr_t)(*(uint32_t *)(base + 0x70));
			if (cnt > 0 && cnt <= 64 && elems)
			{
				for (k = 0; k < cnt; k++)
					normalize_hud_placement_at(elems + (uint32_t)k * 0xB4u + 0x24u, 1, 1);
			}
			/* numbers (+0x78, stride 0xA0, number_element at +0x24) */
			cnt = *(int32_t *)(base + 0x78);
			elems = (uint8_t *)(uintptr_t)(*(uint32_t *)(base + 0x7C));
			if (cnt > 0 && cnt <= 64 && elems)
			{
				for (k = 0; k < cnt; k++)
				{
					uint8_t *num_el = elems + (uint32_t)k * 0xA0u + 0x24u;
					uint8_t num_flags = *(uint8_t *)(num_el + 0x45u);
					if (!(num_flags & (1u << 1))) /* skip _hud_number_show_only_when_zoomed_bit */
						normalize_hud_placement_at(num_el, 1, 0);
				}
			}
			/* crosshairs (+0x84, stride 0x68; bitmap at +0x24, items at +0x34 stride 0x6C) */
			cnt = *(int32_t *)(base + 0x84);
			elems = (uint8_t *)(uintptr_t)(*(uint32_t *)(base + 0x88));
			if (cnt > 0 && cnt <= 64 && elems)
			{
				for (k = 0; k < cnt; k++)
				{
					uint8_t *ch = elems + (uint32_t)k * 0x68u;
					int32_t icnt = *(int32_t *)(ch + 0x34u);
					uint8_t *ielems = (uint8_t *)(uintptr_t)(*(uint32_t *)(ch + 0x38u));
					int32_t ii;
					if (hud_tag_ref_is_fullscreen_scope_mask(instances, total, ch + 0x24u))
						continue;
					if (icnt > 0 && icnt <= 64 && ielems)
					{
						for (ii = 0; ii < icnt; ii++)
							normalize_hud_placement_at(ielems + (uint32_t)ii * 0x6Cu, 1, 1);
					}
				}
			}
			/* overlays (+0x90, stride 0x68; items at +0x34 stride 0x88) */
			cnt = *(int32_t *)(base + 0x90);
			elems = (uint8_t *)(uintptr_t)(*(uint32_t *)(base + 0x94));
			if (cnt > 0 && cnt <= 64 && elems)
			{
				for (k = 0; k < cnt; k++)
				{
					uint8_t *ov = elems + (uint32_t)k * 0x68u;
					int32_t icnt = *(int32_t *)(ov + 0x34u);
					uint8_t *ielems = (uint8_t *)(uintptr_t)(*(uint32_t *)(ov + 0x38u));
					int32_t ii;
					if (icnt > 0 && icnt <= 64 && ielems)
					{
						for (ii = 0; ii < icnt; ii++)
							normalize_hud_placement_at(ielems + (uint32_t)ii * 0x88u, 1, 1);
					}
				}
			}
			/* messaging_icon (+0x13C) */
			*(int16_t *)(base + 0x13C + 0x02) = (int16_t)(*(int16_t *)(base + 0x13C + 0x02) / 2);
			*(int16_t *)(base + 0x13C + 0x04) = (int16_t)(*(int16_t *)(base + 0x13C + 0x04) / 2);
			*(int16_t *)(base + 0x13C + 0x06) = (int16_t)(*(int16_t *)(base + 0x13C + 0x06) / 2);
		}
		else if (grp == 'grhi')
		{
			int32_t icnt, ii;
			uint8_t *ielems;

			/* background.placement (+0x024) */
			normalize_hud_placement_at(base + 0x024, 1, 1);
			/* grenade_count_panel.background.placement (+0x08C) */
			normalize_hud_placement_at(base + 0x08C, 1, 1);
			/* grenade_count_panel.numbers.placement (+0x0F4) */
			normalize_hud_placement_at(base + 0x0F4, 1, 0);
			/* overlays.items (+0x15C, stride 0x88) */
			icnt = *(int32_t *)(base + 0x15C);
			ielems = (uint8_t *)(uintptr_t)(*(uint32_t *)(base + 0x160));
			if (icnt > 0 && icnt <= 64 && ielems)
			{
				for (ii = 0; ii < icnt; ii++)
					normalize_hud_placement_at(ielems + (uint32_t)ii * 0x88u, 1, 1);
			}
		}
		else if (grp == 'unhi')
		{
			int32_t cnt, k;
			uint8_t *elems;

			/* Exact struct unit_hud_interface_definition offsets (from unit_hud_interface_definition.h): */
			normalize_hud_placement_at(base + 0x024, 1, 1); /* background.placement */
			normalize_hud_placement_at(base + 0x08C, 1, 1); /* shield_meter.background.placement */
			normalize_hud_placement_at(base + 0x0F4, 1, 1); /* shield_meter.meter.placement */
			normalize_hud_placement_at(base + 0x17C, 1, 1); /* health_meter.background.placement */
			normalize_hud_placement_at(base + 0x1E4, 1, 1); /* health_meter.meter.placement */
			normalize_hud_placement_at(base + 0x26C, 1, 1); /* motion_sensor.background.placement */
			normalize_hud_placement_at(base + 0x2D4, 1, 1); /* motion_sensor.foreground.placement */
			normalize_hud_placement_at(base + 0x35C, 1, 1); /* blip_placement */

			/* auxilary_panel.auxilary_overlays (tag_block at +0x3A4, stride 0x84, placement at +0x00) */
			cnt = *(int32_t *)(base + 0x3A4);
			elems = (uint8_t *)(uintptr_t)(*(uint32_t *)(base + 0x3A8));
			if (cnt > 0 && cnt <= 64 && elems)
			{
				for (k = 0; k < cnt; k++)
					normalize_hud_placement_at(elems + (uint32_t)k * 0x84u, 1, 1);
			}

			/* auxilary_meters (tag_block at +0x3CC, stride 0x144, panel.background at +0x14, panel.meter at +0x7C) */
			cnt = *(int32_t *)(base + 0x3CC);
			elems = (uint8_t *)(uintptr_t)(*(uint32_t *)(base + 0x3D0));
			if (cnt > 0 && cnt <= 64 && elems)
			{
				for (k = 0; k < cnt; k++)
				{
					uint8_t *am = elems + (uint32_t)k * 0x144u;
					normalize_hud_placement_at(am + 0x14u, 1, 1);
					normalize_hud_placement_at(am + 0x7Cu, 1, 1);
				}
			}
		}
	}
}

void forge_load_saved_map_state_if_present(char const *map_name)
{
	char const *slash;
	char leaf[64];
	char map_path[512];
	char txt_path[512];
	FILE *f;

	{
		char const *active_map = forge_get_active_map_name();
		if (active_map && active_map[0])
			map_name = active_map;
	}
	if (!map_name || !map_name[0])
		return;

	/* Auto-populate uncached local map tags ([MAP]) into the Forge palette and normalize MCC 2x HUD tags */
	forge_runtime_init();
	forge_SyncScenarioTagsWithLoadedMap();
	forge_normalize_mcc_hud_tags();

	/* Patch any baked vehicles in the loaded map so their driver seats and bounding-sphere entrance flags work in MP & Forge */
	{
		long tag_cnt = 0, ti;
		struct forge_cache_file_tag_instance *instances =
			(struct forge_cache_file_tag_instance *)cache_files_tag_instances(&tag_cnt);
		if (instances && tag_cnt > 0)
		{
			for (ti = 0; ti < tag_cnt; ti++)
			{
				if (instances[ti].group_tag == 'vehi' && instances[ti].base_address)
					patch_vehicle_tag_for_player_driving(instances[ti].tag_index, instances[ti].name);
			}
		}
	}

	slash = strrchr(map_name, '\\');
	if (!slash)
		slash = strrchr(map_name, '/');
	snprintf(leaf, sizeof(leaf), "%s", slash ? (slash + 1) : map_name);

	/* 1. Read embedded [OpenCE Forge Map] / [ForgeState] directly from .map header (0x1E4 / 0x1E8) */
	snprintf(map_path, sizeof(map_path), "custom_maps/%s.map", leaf);
	f = fopen(map_path, "rb");
	if (!f)
	{
		static char const *const state_map_cands[] = {
			"OpenCE Forge/forge_maps/%s.map",
			"OpenCE Forge/forge_maps/%s/%s.map",
			"Faber_data/forge_maps/%s/%s.map",
			"forge_maps/%s.map",
			"forge_maps/%s/%s.map"
		};
		int ci;
		for (ci = 0; ci < 5 && !f; ci++)
		{
			if (strstr(state_map_cands[ci], "%s/%s"))
				snprintf(map_path, sizeof(map_path), state_map_cands[ci], leaf, leaf);
			else
				snprintf(map_path, sizeof(map_path), state_map_cands[ci], leaf);
			f = fopen(map_path, "rb");
		}
	}
	if (f)
	{
		uint8_t hdr[0x200];
		if (fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr))
		{
			uint32_t state_off = 0, state_sz = 0, sig = 0;
			memcpy(&state_off, hdr + 0x1E4u, 4);
			memcpy(&state_sz, hdr + 0x1E8u, 4);
			memcpy(&sig, hdr + 0x1FCu, 4);
			if (sig == 0x464C4731u)
			{
				forge_runtime_init();
				snprintf(g_forge.save_map_name, sizeof(g_forge.save_map_name), "%s", leaf);
				g_forge.save_map_name_len = (int)strlen(g_forge.save_map_name);
				g_forge.has_saved_map_once = 1;
				if (state_off >= 0x800u && state_sz > 0 && state_sz < 4u * 1024u * 1024u)
				{
					char *buf = (char *)malloc(state_sz + 1u);
					if (buf)
					{
						if (fseek(f, (long)state_off, SEEK_SET) == 0 && fread(buf, 1, state_sz, f) == state_sz)
						{
							buf[state_sz] = 0;
							parse_forge_state_text_buffer(buf, leaf);
							free(buf);
							fclose(f);
							return;
						}
						free(buf);
					}
				}
			}
		}
		fclose(f);
	}

	/* 2. Legacy fallback for maps saved before embedded state */
	snprintf(txt_path, sizeof(txt_path), "OpenCE Forge/forge_maps/%s/%s.txt", leaf, leaf);
	f = fopen(txt_path, "rb");
	if (!f)
	{
		snprintf(txt_path, sizeof(txt_path), "Faber_data/forge_maps/%s/%s.txt", leaf, leaf);
		f = fopen(txt_path, "rb");
	}
	if (!f)
	{
		snprintf(txt_path, sizeof(txt_path), "forge_maps/%s/%s.txt", leaf, leaf);
		f = fopen(txt_path, "rb");
	}
	if (!f)
	{
		snprintf(txt_path, sizeof(txt_path), "custom_maps/%s.txt", leaf);
		f = fopen(txt_path, "rb");
	}
	if (f)
	{
		long sz;
		fseek(f, 0, SEEK_END);
		sz = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (sz > 0 && sz < 4 * 1024 * 1024)
		{
			char *buf = (char *)malloc((size_t)sz + 1u);
			if (buf)
			{
				size_t rd = fread(buf, 1, (size_t)sz, f);
				buf[rd] = 0;
				parse_forge_state_text_buffer(buf, leaf);
				free(buf);
			}
		}
		fclose(f);
	}
}

/* ============================================================================
 * MONITOR ("characters\monitor\monitor") & NATIVE MODEL RENDERER
 * - Renders 343 Guilty Spark ("characters\monitor\monitor") and 3D Spawn Menu
 *   previews directly through OpenCE's native orig_render_model!
 * ============================================================================ */

static int draw_native_object_model_at(
	long obj_def_idx,
	long unique_id,
	void const *lighting_ptr,
	union real_point3d const *pos,
	union real_vector3d const *forward,
	union real_vector3d const *up,
	int is_monitor,
	int is_preview,
	unsigned long flags)
{
	uint8_t const *obj_def;
	float radius;
	long model_idx;
	struct model const *mdef;
	struct real_matrix4x3 node_matrices[64];
	static struct render_lighting studio_lighting;
	struct render_lighting const *use_lighting;
	static union real_rgb_color change_colors_mon[4];
	static union real_rgb_color change_colors_std[4];
	static real function_values[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	union real_rgb_color *change_colors = is_monitor ? change_colors_mon : change_colors_std;
	union real_point3d draw_centroid = *pos;
	int node_count, n;

	if (obj_def_idx == NONE)
		return 0;
	obj_def = (uint8_t const *)tag_get(tag_get_group_tag(obj_def_idx), obj_def_idx);
	if (!obj_def)
		return 0;

	radius = *(float const *)(obj_def + 0x04u);
	if (!(radius > 0.1f && radius < 100.0f))
		radius = 1.5f;

	model_idx = *(int32_t const *)(obj_def + 0x34u);
	if (model_idx == NONE)
		return 0;
	mdef = (struct model const *)tag_get('mode', model_idx);
	if (!mdef)
		return 0;

	node_count = (int)mdef->nodes.count;
	if (node_count <= 0)
		return 0;
	if (node_count > 64)
		node_count = 64;

	memset(node_matrices, 0, sizeof(node_matrices));
	if (is_preview)
	{
		float preview_scale = 0.52f / ((radius > 0.15f) ? radius : 0.52f);
		struct real_matrix4x3 scale_pos_mat;
		if (preview_scale < 0.035f) preview_scale = 0.035f;
		if (preview_scale > 1.35f) preview_scale = 1.35f;

		model_get_node_matrices(mdef, node_matrices, global_origin3d, forward, up);
		matrix4x3_from_point_and_vectors(&scale_pos_mat, pos, global_forward3d, global_up3d);
		scale_pos_mat.scale = preview_scale;
		for (n = 0; n < node_count; n++)
		{
			struct real_matrix4x3 tmp = node_matrices[n];
			matrix4x3_multiply(&scale_pos_mat, &tmp, &node_matrices[n]);
		}
		radius *= preview_scale;
	}
	else if (is_monitor)
	{
		union real_point3d mpos = *pos;
		struct real_matrix4x3 scale_pos_mat;
		mpos.z += 0.55f;
		draw_centroid = mpos;
		model_get_node_matrices(mdef, node_matrices, global_origin3d, forward, up);
		matrix4x3_from_point_and_vectors(&scale_pos_mat, &mpos, global_forward3d, global_up3d);
		scale_pos_mat.scale = 1.35f;
		for (n = 0; n < node_count; n++)
		{
			struct real_matrix4x3 tmp = node_matrices[n];
			matrix4x3_multiply(&scale_pos_mat, &tmp, &node_matrices[n]);
		}
		radius *= 1.35f;
	}
	else
	{
		model_get_node_matrices(mdef, node_matrices, pos, forward, up);
	}

	if (is_monitor)
	{
		change_colors[0].red = 0.28f;
		change_colors[0].green = 0.72f;
		change_colors[0].blue = 1.0f;
		change_colors[1].red = 0.65f;
		change_colors[1].green = 0.90f;
		change_colors[1].blue = 1.0f;
	}
	else
	{
		change_colors[0].red = 0.85f;
		change_colors[0].green = 0.85f;
		change_colors[0].blue = 0.85f;
		change_colors[1].red = 0.75f;
		change_colors[1].green = 0.75f;
		change_colors[1].blue = 0.75f;
	}
	change_colors[2] = change_colors[0];
	change_colors[3] = change_colors[1];

	function_values[0] = 1.0f;
	function_values[1] = 1.0f;
	function_values[2] = 1.0f;
	function_values[3] = 1.0f;

	if (lighting_ptr)
	{
		use_lighting = (struct render_lighting const *)lighting_ptr;
	}
	else
	{
		memset(&studio_lighting, 0, sizeof(studio_lighting));
		studio_lighting.ambient_color.red = 0.72f;
		studio_lighting.ambient_color.green = 0.74f;
		studio_lighting.ambient_color.blue = 0.78f;
		studio_lighting.distant_light_count = 1;
		studio_lighting.distant_lights[0].color.red = 0.95f;
		studio_lighting.distant_lights[0].color.green = 0.95f;
		studio_lighting.distant_lights[0].color.blue = 0.92f;
		studio_lighting.distant_lights[0].direction.i = -0.577f;
		studio_lighting.distant_lights[0].direction.j = -0.577f;
		studio_lighting.distant_lights[0].direction.k = -0.577f;
		use_lighting = &studio_lighting;
	}

	orig_render_model(
		model_idx,
		10000.0f,
		node_matrices,
		NULL,
		change_colors,
		function_values,
		use_lighting,
		&draw_centroid,
		radius,
		NULL,
		(is_monitor || is_preview) ? NONE : unique_id,
		0,
		is_preview ? (flags | (1u << 3)) /* _render_model_first_person_bit */ : flags);
	return 1;
}

enum forge_marker_color_preset
{
	_marker_color_ffa_yellow = 0,
	_marker_color_red_team,
	_marker_color_blue_team,
	_marker_color_tele_green,
	_marker_color_race_orange,
	_marker_color_oddball_purple,
	_marker_color_koth_gold,
	NUMBER_OF_MARKER_COLOR_PRESETS
};

enum forge_marker_mesh_kind
{
	_marker_mesh_player_spawn = 0,
	_marker_mesh_tele_origin,
	_marker_mesh_tele_dest,
	_marker_mesh_race,
	_marker_mesh_oddball,
	_marker_mesh_ctf_flag,
	_marker_mesh_koth,
	_marker_mesh_koth_beam,
	_marker_mesh_firefight_spawn,
	NUMBER_OF_MARKER_MESH_KINDS
};

static struct
{
	int loaded;
	struct
	{
		int valid;
		int part_count;
		float radius;
		struct forge_hw_part parts[FORGE_MAX_HW_PARTS];
	} meshes[NUMBER_OF_MARKER_MESH_KINDS];
} s_marker_meshes = { 0 };

static long get_or_create_translucent_marker_shader(int color_preset, int is_highlighted, int is_z_prime)
{
	static uint32_t const s_preset_rgb[NUMBER_OF_MARKER_COLOR_PRESETS] = {
		0x00FFE618u, /* FFA Player Spawn     - Translucent Yellow */
		0x00FF2424u, /* Red Team Spawn/Flag  - Translucent Red    */
		0x002488FFu, /* Blue Team Spawn/Flag - Translucent Blue   */
		0x0024FF54u, /* Teleporter In/Out    - Translucent Green  */
		0x00FF8C14u, /* Race Marker          - Translucent Orange */
		0x00BC38FFu, /* Oddball Marker       - Translucent Purple */
		0x00FFC824u  /* KOTH Hill Marker     - Translucent Gold   */
	};
	char shdr_name[96];
	char bitm_name[96];
	long shdr_idx, tmpl_soso_idx, bitm_idx;
	struct tag_iterator iter;
	void *tmpl_ptr;
	struct bitmap_data *bmp;
	struct bitmap_group *bgrp;
	uint8_t *soso_def;
	uint32_t rgb, alpha_byte, argb;
	uint32_t *pix;
	float r_f, g_f, b_f;
	int i;

	if (color_preset < 0 || color_preset >= NUMBER_OF_MARKER_COLOR_PRESETS)
		color_preset = _marker_color_ffa_yellow;

	snprintf(shdr_name, sizeof(shdr_name), "forge\\markers\\shdr_%d_%d_%d",
		color_preset, is_highlighted ? 1 : 0, is_z_prime ? 1 : 0);
	shdr_idx = tag_loaded('soso', shdr_name);
	if (shdr_idx != NONE)
		return shdr_idx;

	rgb = s_preset_rgb[color_preset];
	alpha_byte = is_z_prime ? 0xFFu : (is_highlighted ? 0xB8u : 0x8Cu);
	argb = (alpha_byte << 24) | rgb;
	r_f = (float)((rgb >> 16) & 0xFFu) / 255.0f;
	g_f = (float)((rgb >> 8) & 0xFFu) / 255.0f;
	b_f = (float)(rgb & 0xFFu) / 255.0f;

	snprintf(bitm_name, sizeof(bitm_name), "forge\\markers\\bitm_%d_%d_%d",
		color_preset, is_highlighted ? 1 : 0, is_z_prime ? 1 : 0);
	bitm_idx = tag_loaded('bitm', bitm_name);
	if (bitm_idx == NONE)
	{
		/* Format 11 = _bitmap_format_a8r8g8b8 (32-bit ARGB) */
		bmp = bitmap_2d_new(16, 16, 0, 11);
		if (!bmp || !bmp->base_address)
			return NONE;
		pix = (uint32_t *)bmp->base_address;
		for (i = 0; i < 16 * 16; i++)
			pix[i] = argb;
		bitmap_rebuild(bmp);

		bgrp = (struct bitmap_group *)calloc(1, sizeof(struct bitmap_group));
		if (!bgrp)
			return NONE;
		bgrp->bitmaps.count = 1;
		bgrp->bitmaps.address = bmp;
		bitm_idx = append_runtime_tag_instance('bitm', NONE, NONE, bitm_name, bgrp);
		if (bitm_idx == NONE)
			return NONE;
	}

	tag_iterator_new(&iter, 'soso');
	tmpl_soso_idx = tag_iterator_next(&iter);
	if (tmpl_soso_idx == NONE)
		return NONE;
	tmpl_ptr = shader_definition_get(tmpl_soso_idx);
	if (!tmpl_ptr)
		return NONE;

	soso_def = (uint8_t *)calloc(1, 512);
	if (!soso_def)
		return NONE;
	memcpy(soso_def, tmpl_ptr, 440);

	*(int16_t *)(soso_def + 0x24) = 4; /* _shader_type_model */
	/* Bit 1 = _shader_model_two_sided_bit, Bit 2 = _shader_model_not_alpha_tested_bit, Bit 3 = _shader_model_alpha_blended_decal_bit */
	*(uint16_t *)(soso_def + 0x28) = is_z_prime ? ((1u << 1) | (1u << 2)) : ((1u << 1) | (1u << 2) | (1u << 3));
	*(float *)(soso_def + 0x38) = is_highlighted ? 0.72f : 0.55f;
	*(int16_t *)(soso_def + 0x4C) = 0;
	*(uint16_t *)(soso_def + 0x6C) = 1u; /* _shader_model_self_illumination_no_random_phase_bit */
	*(int16_t *)(soso_def + 0x70) = 0;
	*(int16_t *)(soso_def + 0x72) = 0;
	*(float *)(soso_def + 0x74) = 1.0f;
	*(float *)(soso_def + 0x78) = r_f * 0.34f;
	*(float *)(soso_def + 0x7C) = g_f * 0.34f;
	*(float *)(soso_def + 0x80) = b_f * 0.34f;
	*(float *)(soso_def + 0x84) = r_f * 0.34f;
	*(float *)(soso_def + 0x88) = g_f * 0.34f;
	*(float *)(soso_def + 0x8C) = b_f * 0.34f;
	*(float *)(soso_def + 0x9C) = 1.0f;
	*(float *)(soso_def + 0xA0) = 1.0f;
	*(int32_t *)(soso_def + 0xA4) = 'bitm';
	*(int32_t *)(soso_def + 0xA8) = 0;
	*(int32_t *)(soso_def + 0xAC) = 0;
	*(int32_t *)(soso_def + 0xB0) = bitm_idx;
	*(int32_t *)(soso_def + 0xC8) = NONE;
	*(int16_t *)(soso_def + 0xD4) = 0;
	*(int16_t *)(soso_def + 0xD6) = 0;
	*(float *)(soso_def + 0xD8) = 1.0f;
	*(int32_t *)(soso_def + 0xE8) = NONE;
	*(float *)(soso_def + 0xEC) = 1.0f;
	*(float *)(soso_def + 0x144) = 0.0f;
	*(float *)(soso_def + 0x154) = 0.0f;
	*(int32_t *)(soso_def + 0x170) = NONE;

	return append_runtime_tag_instance('soso', 'shdr', NONE, shdr_name, soso_def);
}

#include "forge_marker_ftags_embed.h"

static void ensure_objective_marker_meshes_loaded(void)
{
	static char const *const s_files[NUMBER_OF_MARKER_MESH_KINDS] = {
		"Faber_data/cached_tags/markers/player_spawn.ftag",
		"Faber_data/cached_tags/markers/teleporter_origin.ftag",
		"Faber_data/cached_tags/markers/teleporter_destination.ftag",
		"Faber_data/cached_tags/markers/race_marker.ftag",
		"Faber_data/cached_tags/markers/oddball_marker.ftag",
		"Faber_data/cached_tags/markers/ctf_flag_marker.ftag",
		"Faber_data/cached_tags/markers/koth_marker.ftag",
		"Faber_data/cached_tags/markers/koth_beam.ftag",
		"Faber_data/cached_tags/markers/firefight_spawn.ftag"
	};
	int k;

	if (s_marker_meshes.loaded && s_marker_meshes_loaded_flag)
		return;
	s_marker_meshes.loaded = 1;
	s_marker_meshes_loaded_flag = 1;

#ifdef _WIN32
	CreateDirectoryA("Faber_data", NULL);
	CreateDirectoryA("Faber_data/cached_tags", NULL);
	CreateDirectoryA("Faber_data/cached_tags/markers", NULL);
#endif
	for (k = 0; k < (int)(sizeof(g_embedded_marker_ftags) / sizeof(g_embedded_marker_ftags[0])); k++)
	{
		char mpath[256];
		FILE *chk;
		snprintf(mpath, sizeof(mpath), "Faber_data/cached_tags/markers/%s", g_embedded_marker_ftags[k].primary_name);
		chk = fopen(mpath, "rb");
		if (chk)
		{
			fclose(chk);
		}
		else
		{
			uLongf out_len = (uLongf)g_embedded_marker_ftags[k].uncompressed_size;
			uint8_t *raw = (uint8_t *)malloc(out_len);
			if (raw)
			{
				if (uncompress(raw, &out_len, g_embedded_marker_ftags[k].compressed_data, (uLong)g_embedded_marker_ftags[k].compressed_size) == Z_OK &&
					out_len == (uLongf)g_embedded_marker_ftags[k].uncompressed_size)
				{
					FILE *wf = fopen(mpath, "wb");
					if (wf)
					{
						fwrite(raw, 1, (size_t)out_len, wf);
						fclose(wf);
					}
				}
				free(raw);
			}
		}
	}

	for (k = 0; k < NUMBER_OF_MARKER_MESH_KINDS; k++)
	{
		s_marker_meshes.meshes[k].valid = 0;
		s_marker_meshes.meshes[k].part_count = 0;
		s_marker_meshes.meshes[k].radius = 0.45f;
		if (load_hw_parts_from_ftag_file(
				s_files[k],
				s_marker_meshes.meshes[k].parts,
				FORGE_MAX_HW_PARTS,
				&s_marker_meshes.meshes[k].part_count,
				&s_marker_meshes.meshes[k].radius,
				1.0f,
				0.0f))
		{
			s_marker_meshes.meshes[k].valid = (s_marker_meshes.meshes[k].part_count > 0) ? 1 : 0;
		}
		else if (k == _marker_mesh_firefight_spawn &&
			load_hw_parts_from_ftag_file(
				"Faber_data/cached_tags/markers/player_spawn.ftag",
				s_marker_meshes.meshes[k].parts,
				FORGE_MAX_HW_PARTS,
				&s_marker_meshes.meshes[k].part_count,
				&s_marker_meshes.meshes[k].radius,
				1.0f,
				0.0f))
		{
			s_marker_meshes.meshes[k].valid = (s_marker_meshes.meshes[k].part_count > 0) ? 1 : 0;
		}
	}
}

static void draw_translucent_marker_hw_parts(
	union real_point3d const *pos,
	union real_vector3d const *forward,
	union real_vector3d const *up,
	float radius,
	float scale,
	struct forge_hw_part *parts,
	int part_count,
	int color_preset,
	int is_highlighted,
	int is_preview)
{
	struct rasterizer_model_begin_parameters model_parameters;
	static union real_rgb_color change_colors[4];
	static real function_values[4];
	struct real_matrix4x3 node_matrix;
	long col_shdr_idx;
	struct shader *col_shdr_def;
	int p;

	if (!parts || part_count <= 0)
		return;

	(void)get_or_create_translucent_marker_shader(color_preset, is_highlighted, 1);
	col_shdr_idx = get_or_create_translucent_marker_shader(color_preset, is_highlighted, 0);
	if (col_shdr_idx == NONE)
		return;
	col_shdr_def = shader_definition_get(col_shdr_idx);
	if (!col_shdr_def)
		return;

	matrix4x3_from_point_and_vectors(&node_matrix, pos, forward, up);
	if (scale > 0.001f && scale != 1.0f)
		node_matrix.scale = scale;

	change_colors[0].red = 1.0f;
	change_colors[0].green = 1.0f;
	change_colors[0].blue = 1.0f;
	change_colors[1] = change_colors[0];
	change_colors[2] = change_colors[0];
	change_colors[3] = change_colors[0];

	function_values[0] = 1.0f;
	function_values[1] = 1.0f;
	function_values[2] = 1.0f;
	function_values[3] = 1.0f;

	memset(&model_parameters, 0, sizeof(model_parameters));
	model_parameters.unique_identifier = NONE;
	model_parameters.lighting.ambient_color.red = is_highlighted ? 0.88f : 0.72f;
	model_parameters.lighting.ambient_color.green = is_highlighted ? 0.88f : 0.72f;
	model_parameters.lighting.ambient_color.blue = is_highlighted ? 0.88f : 0.72f;
	model_parameters.lighting.distant_light_count = 1;
	model_parameters.lighting.distant_lights[0].color.red = 0.95f;
	model_parameters.lighting.distant_lights[0].color.green = 0.95f;
	model_parameters.lighting.distant_lights[0].color.blue = 0.95f;
	model_parameters.lighting.distant_lights[0].direction.i = -0.45f;
	model_parameters.lighting.distant_lights[0].direction.j = -0.45f;
	model_parameters.lighting.distant_lights[0].direction.k = -0.77f;
	model_parameters.centroid = *pos;
	model_parameters.radius = (radius > 0.1f) ? (radius * ((scale > 0.001f) ? scale : 1.0f)) : 0.6f;
	model_parameters.animation.colors = change_colors;
	model_parameters.animation.values = function_values;
	model_parameters.skinning.node_matrices = &node_matrix;
	model_parameters.skinning.node_matrix_count = 1;
	model_parameters.geometry_flags = (1u << 2) | (is_preview ? (1u << 4) : 0u); /* _rasterizer_geometry_no_fog_bit */
	model_parameters.base_map_scale.i = 1.0f;
	model_parameters.base_map_scale.j = 1.0f;

	rasterizer_model_begin(&model_parameters, FALSE);

	for (p = 0; p < part_count; p++)
	{
		rasterizer_model_draw(
			col_shdr_def,
			0,
			&parts[p].triangle_buffer,
			NONE,
			parts[p].triangle_buffer.count,
			&parts[p].vertex_buffer,
			NONE);
	}
	rasterizer_model_end();
	if (is_preview)
	{
		rasterizer_set_stencil_mode(0);
		rasterizer_set_frustum_z(0.0f, 0.0f);
	}
}

static int resolve_marker_kind_and_color(
	uint32_t group_tag,
	char const *tag_path,
	int16_t netgame_flag_type,
	int16_t team_designator,
	int *out_mesh_kind,
	int *out_color_preset)
{
	if (group_tag == 'ffsp' ||
		netgame_flag_type == FORGE_NETGAME_FLAG_FIREFIGHT_SPAWN ||
		(tag_path && _stricmp(tag_path, "spawn\\firefight_spawn") == 0))
	{
		int ff_type = (team_designator >= 0) ? FORGE_FF_UNPACK_TYPE(team_designator) : 0;
		*out_mesh_kind = _marker_mesh_firefight_spawn;
		if (ff_type == _forge_ff_spawn_player_vehicle)
			*out_color_preset = _marker_color_tele_green;
		else if (ff_type == _forge_ff_spawn_dropship_zone)
			*out_color_preset = _marker_color_koth_gold;
		else if (ff_type >= _forge_ff_spawn_ai_and_vehicles && ff_type <= _forge_ff_spawn_enemy_heavy_veh)
			*out_color_preset = _marker_color_race_orange;
		else
			*out_color_preset = _marker_color_red_team;
		return 1;
	}
	if (group_tag == 'spwn' || (tag_path && strncmp(tag_path, "spawn\\", 6) == 0))
	{
		*out_mesh_kind = _marker_mesh_player_spawn;
		if (team_designator == 1 || (tag_path && strstr(tag_path, "blue")))
			*out_color_preset = _marker_color_blue_team;
		else if (team_designator == 2 || (tag_path && strstr(tag_path, "ffa")))
			*out_color_preset = _marker_color_ffa_yellow;
		else
			*out_color_preset = _marker_color_red_team;
		return 1;
	}
	if (group_tag == 'tele' || (tag_path && strncmp(tag_path, "teleporter\\", 11) == 0))
	{
		int is_sender = (netgame_flag_type == 6 /* _netgame_flag_teleporter_source */ ||
			(tag_path && (strstr(tag_path, "origin") || strstr(tag_path, "sender"))));
		*out_mesh_kind = is_sender ? _marker_mesh_tele_origin : _marker_mesh_tele_dest;
		*out_color_preset = _marker_color_tele_green;
		return 1;
	}
	if (group_tag == 'flag' || (tag_path && strncmp(tag_path, "objective\\", 10) == 0))
	{
		if (netgame_flag_type == 2 /* _netgame_flag_oddball_ball_spawn */ ||
			(tag_path && strstr(tag_path, "oddball")))
		{
			*out_mesh_kind = _marker_mesh_oddball;
			*out_color_preset = _marker_color_oddball_purple;
			return 1;
		}
		if (netgame_flag_type == 3 /* _netgame_flag_race_track */ ||
			netgame_flag_type == 4 /* _netgame_flag_race_vehicle */ ||
			(tag_path && strstr(tag_path, "race")))
		{
			*out_mesh_kind = _marker_mesh_race;
			*out_color_preset = _marker_color_race_orange;
			return 1;
		}
		if (netgame_flag_type == 0 /* _netgame_flag_ctf_flag */ ||
			netgame_flag_type == 1 /* _netgame_flag_ctf_vehicle */ ||
			(tag_path && strstr(tag_path, "ctf")))
		{
			*out_mesh_kind = _marker_mesh_ctf_flag;
			*out_color_preset = (team_designator == 1 || (tag_path && strstr(tag_path, "blue")))
				? _marker_color_blue_team
				: _marker_color_red_team;
			return 1;
		}
		*out_mesh_kind = _marker_mesh_koth;
		*out_color_preset = _marker_color_koth_gold;
		return 1;
	}
	return 0;
}

static void draw_translucent_koth_beam_3d(
	union real_point3d const *p0,
	union real_point3d const *p1)
{
	struct rasterizer_model_begin_parameters model_parameters;
	union real_rgb_color change_colors[4];
	real function_values[4];
	struct real_matrix4x3 node_matrix;
	long col_shdr_idx;
	struct shader *col_shdr_def;
	float dx, dy, dz, len_xy, len_3d;
	int p;

	if (!s_marker_meshes.meshes[_marker_mesh_koth_beam].valid ||
		s_marker_meshes.meshes[_marker_mesh_koth_beam].part_count <= 0)
		return;

	dx = p1->x - p0->x;
	dy = p1->y - p0->y;
	dz = p1->z - p0->z;
	len_xy = square_root(dx * dx + dy * dy);
	len_3d = square_root(dx * dx + dy * dy + dz * dz);
	if (len_3d < 0.01f)
		return;

	col_shdr_idx = get_or_create_translucent_marker_shader(_marker_color_koth_gold, 1, 0);
	if (col_shdr_idx == NONE)
		return;
	col_shdr_def = shader_definition_get(col_shdr_idx);
	if (!col_shdr_def)
		return;

	/*
	 * Stretch the unit 3D beam (x in [0..1], y in [-0.015..+0.015], z in [0.025..0.055])
	 * along forward = (p1 - p0) while keeping left and up unit-length so cross-section
	 * thickness stays constant regardless of segment length!
	 */
	memset(&node_matrix, 0, sizeof(node_matrix));
	node_matrix.scale = 1.0f;
	node_matrix.position = *p0;
	node_matrix.forward.i = dx;
	node_matrix.forward.j = dy;
	node_matrix.forward.k = dz;
	if (len_xy > 0.001f)
	{
		node_matrix.left.i = -dy / len_xy;
		node_matrix.left.j = dx / len_xy;
		node_matrix.left.k = 0.0f;
	}
	else
	{
		node_matrix.left.i = 0.0f;
		node_matrix.left.j = 1.0f;
		node_matrix.left.k = 0.0f;
	}
	node_matrix.up.i = 0.0f;
	node_matrix.up.j = 0.0f;
	node_matrix.up.k = 1.0f;

	change_colors[0].red = 1.0f;
	change_colors[0].green = 1.0f;
	change_colors[0].blue = 1.0f;
	change_colors[1] = change_colors[0];
	change_colors[2] = change_colors[0];
	change_colors[3] = change_colors[0];

	function_values[0] = 1.0f;
	function_values[1] = 1.0f;
	function_values[2] = 1.0f;
	function_values[3] = 1.0f;

	memset(&model_parameters, 0, sizeof(model_parameters));
	model_parameters.unique_identifier = NONE;
	model_parameters.lighting.ambient_color.red = 0.90f;
	model_parameters.lighting.ambient_color.green = 0.90f;
	model_parameters.lighting.ambient_color.blue = 0.90f;
	model_parameters.lighting.distant_light_count = 1;
	model_parameters.lighting.distant_lights[0].color.red = 0.95f;
	model_parameters.lighting.distant_lights[0].color.green = 0.95f;
	model_parameters.lighting.distant_lights[0].color.blue = 0.95f;
	model_parameters.lighting.distant_lights[0].direction.i = -0.45f;
	model_parameters.lighting.distant_lights[0].direction.j = -0.45f;
	model_parameters.lighting.distant_lights[0].direction.k = -0.77f;
	model_parameters.centroid.x = 0.5f * (p0->x + p1->x);
	model_parameters.centroid.y = 0.5f * (p0->y + p1->y);
	model_parameters.centroid.z = 0.5f * (p0->z + p1->z);
	model_parameters.radius = 0.5f * len_3d + 0.5f;
	model_parameters.animation.colors = change_colors;
	model_parameters.animation.values = function_values;
	model_parameters.skinning.node_matrices = &node_matrix;
	model_parameters.skinning.node_matrix_count = 1;
	model_parameters.geometry_flags = (1u << 2); /* _rasterizer_geometry_no_fog_bit */
	model_parameters.base_map_scale.i = 1.0f;
	model_parameters.base_map_scale.j = 1.0f;

	rasterizer_model_begin(&model_parameters, FALSE);
	for (p = 0; p < s_marker_meshes.meshes[_marker_mesh_koth_beam].part_count; p++)
	{
		rasterizer_model_draw(
			col_shdr_def,
			0,
			&s_marker_meshes.meshes[_marker_mesh_koth_beam].parts[p].triangle_buffer,
			NONE,
			s_marker_meshes.meshes[_marker_mesh_koth_beam].parts[p].triangle_buffer.count,
			&s_marker_meshes.meshes[_marker_mesh_koth_beam].parts[p].vertex_buffer,
			NONE);
	}
	rasterizer_model_end();
}

static void render_world_objective_and_spawn_markers(void)
{
	int i, hill_id;

	ensure_objective_marker_meshes_loaded();

	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];
		int mesh_kind = 0, color_preset = 0;
		int is_hi;
		float dx, dy, dz, d2;
		union real_point3d pos;
		union real_vector3d fwd, up;

		if (!inst->active)
			continue;
		if (inst->group_tag != 'spwn' && inst->group_tag != 'tele' && inst->group_tag != 'flag' && inst->group_tag != 'ffsp')
			continue;
		if (!resolve_marker_kind_and_color(
				inst->group_tag,
				inst->tag_path,
				inst->netgame_flag_type,
				inst->team_designator,
				&mesh_kind,
				&color_preset))
			continue;
		if (mesh_kind < 0 || mesh_kind >= NUMBER_OF_MARKER_MESH_KINDS ||
			!s_marker_meshes.meshes[mesh_kind].valid)
			continue;

		/* Player spawns, Firefight spawns, KOTH, Oddball, and Race markers are only visible while Forge Mode is active */
		if (g_forge.mode != _forge_mode_3d_monitor &&
			(mesh_kind == _marker_mesh_player_spawn ||
			 mesh_kind == _marker_mesh_firefight_spawn ||
			 mesh_kind == _marker_mesh_koth ||
			 mesh_kind == _marker_mesh_oddball ||
			 mesh_kind == _marker_mesh_race))
		{
			continue;
		}

		if (mesh_kind == _marker_mesh_koth)
		{
			/* KOTH markers only highlight when actively clicked/grabbed (or during 10-node placement), never on hover/proximity */
			is_hi = (i == g_forge.grabbed_live_index) ||
					(g_forge.koth_group_placing && inst->team_designator == g_forge.koth_group_hill_id);
		}
		else
		{
			is_hi = (i == g_forge.grabbed_live_index || i == g_forge.hovered_live_index);
		}

		dx = inst->x - render.camera.position.x;
		dy = inst->y - render.camera.position.y;
		dz = inst->z - render.camera.position.z;
		d2 = dx * dx + dy * dy + dz * dz;
		if (d2 > 300.0f * 300.0f && !is_hi)
			continue;

		pos.x = inst->x;
		pos.y = inst->y;
		pos.z = inst->z;
		fwd.i = (float)cosine(inst->yaw);
		fwd.j = (float)sine(inst->yaw);
		fwd.k = 0.0f;
		up.i = 0.0f;
		up.j = 0.0f;
		up.k = 1.0f;

		draw_translucent_marker_hw_parts(
			&pos,
			&fwd,
			&up,
			s_marker_meshes.meshes[mesh_kind].radius,
			1.0f,
			s_marker_meshes.meshes[mesh_kind].parts,
			s_marker_meshes.meshes[mesh_kind].part_count,
			color_preset,
			is_hi,
			0);
	}

	/* Render 3D translucent KOTH boundary beams ONLY in Forge mode when a node of that hill is actually clicked/grabbed
	 * (or actively being placed as a 10-point hill), connecting the exact 3D marker positions. */
	if (g_forge.mode != _forge_mode_3d_monitor)
		return;
	for (hill_id = 0; hill_id < 16; hill_id++)
	{
		int node_indices[32];
		float node_angles[32];
		int ncount = 0;
		int hill_grabbed = (g_forge.koth_group_placing && g_forge.koth_group_hill_id == hill_id) ? 1 : 0;
		float cx_h = 0.0f, cy_h = 0.0f;
		int a, b;

		for (i = 0; i < g_forge.live_instance_count && ncount < 32; i++)
		{
			struct forge_live_instance const *inst = &g_forge.live_instances[i];
			if (!inst->active || inst->group_tag != 'flag')
				continue;
			if (inst->netgame_flag_type != 8 /* _netgame_flag_hill */ && strstr(inst->tag_path, "koth") == NULL)
				continue;
			if (inst->team_designator != hill_id)
				continue;
			node_indices[ncount++] = i;
			cx_h += inst->x;
			cy_h += inst->y;
			if (i == g_forge.grabbed_live_index)
				hill_grabbed = 1;
		}

		if (ncount >= 2 && hill_grabbed)
		{
			cx_h /= (float)ncount;
			cy_h /= (float)ncount;

			for (a = 0; a < ncount; a++)
			{
				struct forge_live_instance const *na = &g_forge.live_instances[node_indices[a]];
				node_angles[a] = (float)atan2((double)(na->y - cy_h), (double)(na->x - cx_h));
			}
			for (a = 0; a < ncount - 1; a++)
			{
				for (b = a + 1; b < ncount; b++)
				{
					if (node_angles[b] < node_angles[a])
					{
						float ta = node_angles[a];
						int ti = node_indices[a];
						node_angles[a] = node_angles[b];
						node_indices[a] = node_indices[b];
						node_angles[b] = ta;
						node_indices[b] = ti;
					}
				}
			}

			for (a = 0; a < ncount; a++)
			{
				int nb = (a + 1) % ncount;
				struct forge_live_instance const *inst0 = &g_forge.live_instances[node_indices[a]];
				struct forge_live_instance const *inst1 = &g_forge.live_instances[node_indices[nb]];
				union real_point3d p0, p1;
				p0.x = inst0->x;
				p0.y = inst0->y;
				p0.z = inst0->z;
				p1.x = inst1->x;
				p1.y = inst1->y;
				p1.z = inst1->z;
				draw_translucent_koth_beam_3d(&p0, &p1);
			}
		}
	}
}

static void render_spawn_palette_3d_preview(long object_index, void const *lighting_ptr)
{
	int i, match_count = 0, chosen_idx = -1;
	struct forge_catalog_entry const *entry;
	union real_point3d preview_pos;
	union real_vector3d preview_fwd, preview_up, cam_right;
	float dist, angle;
	int marker_kind = -1, marker_color = 0;
	uint32_t actual_grp = 0;
	long obj_def_idx;

	if (g_forge.active_modal != _forge_modal_spawn_palette || g_forge.h3_menu_stage != 1)
		return;

	for (i = 0; i < g_forge.catalog_count; i++)
	{
		if (g_forge.catalog[i].category == g_forge.current_category &&
			forge_catalog_entry_allowed(&g_forge.catalog[i]))
		{
			if (match_count == g_forge.palette_cursor)
			{
				chosen_idx = i;
				break;
			}
			match_count++;
		}
	}
	if (chosen_idx < 0)
		return;

	entry = &g_forge.catalog[chosen_idx];

	/* Check if this catalog entry is one of our 3D Translucent Spawn/Teleporter/Objective Markers! */
	if (resolve_marker_kind_and_color(entry->group_tag, entry->tag_path, -1, -1, &marker_kind, &marker_color))
	{
		ensure_objective_marker_meshes_loaded();
		if (marker_kind >= 0 && marker_kind < NUMBER_OF_MARKER_MESH_KINDS &&
			s_marker_meshes.meshes[marker_kind].valid)
		{
			dist = 1.45f;
			cross_product3d(&render.camera.forward, &render.camera.up, &cam_right);
			preview_pos.x = render.camera.position.x + render.camera.forward.i * dist + cam_right.i * 0.42f - render.camera.up.i * 0.18f;
			preview_pos.y = render.camera.position.y + render.camera.forward.j * dist + cam_right.j * 0.42f - render.camera.up.j * 0.18f;
			preview_pos.z = render.camera.position.z + render.camera.forward.k * dist + cam_right.k * 0.42f - render.camera.up.k * 0.18f;

			angle = (float)(system_milliseconds() % 6000u) * (6.2831853f / 6000.0f);
			preview_fwd.i = (float)cosine(angle);
			preview_fwd.j = (float)sine(angle);
			preview_fwd.k = 0.0f;
			preview_up.i = 0.0f;
			preview_up.j = 0.0f;
			preview_up.k = 1.0f;

			draw_translucent_marker_hw_parts(
				&preview_pos,
				&preview_fwd,
				&preview_up,
				s_marker_meshes.meshes[marker_kind].radius,
				1.15f,
				s_marker_meshes.meshes[marker_kind].parts,
				s_marker_meshes.meshes[marker_kind].part_count,
				marker_color,
				1,
				1);
			return;
		}
	}

	/* Debounce disk-backed runtime tag loading by 200 ms so rapid scrolling in the Spawn Palette never freezes */
	{
		static uint32_t s_preview_debounce_group = 0;
		static char s_preview_debounce_path[256] = "";
		static unsigned long s_preview_debounce_ms = 0;
		int already_in_ram = 0;
		int di;

		for (di = 0; di < s_dyn_tags.count; di++)
		{
			if (s_dyn_tags.entries[di].group_tag == entry->group_tag &&
				_stricmp(s_dyn_tags.entries[di].tag_path, entry->tag_path) == 0)
			{
				already_in_ram = 1;
				break;
			}
		}
		if (!already_in_ram && find_native_or_mp_variant_tag(entry->group_tag, entry->tag_path) != NONE)
			already_in_ram = 1;

		if (!already_in_ram)
		{
			unsigned long now_ms = system_milliseconds();
			if (s_preview_debounce_group != entry->group_tag ||
				_stricmp(s_preview_debounce_path, entry->tag_path) != 0)
			{
				s_preview_debounce_group = entry->group_tag;
				snprintf(s_preview_debounce_path, sizeof(s_preview_debounce_path), "%s", entry->tag_path);
				s_preview_debounce_ms = now_ms;
				return;
			}
			if (now_ms - s_preview_debounce_ms < 200u)
				return;
		}
	}

	obj_def_idx = forge_resolve_engine_tag(entry->group_tag, entry->tag_path, &actual_grp);
	if (obj_def_idx == NONE)
		return;

	dist = 1.55f;
	cross_product3d(&render.camera.forward, &render.camera.up, &cam_right);
	preview_pos.x = render.camera.position.x + render.camera.forward.i * dist + cam_right.i * 0.42f - render.camera.up.i * 0.14f;
	preview_pos.y = render.camera.position.y + render.camera.forward.j * dist + cam_right.j * 0.42f - render.camera.up.j * 0.14f;
	preview_pos.z = render.camera.position.z + render.camera.forward.k * dist + cam_right.k * 0.42f - render.camera.up.k * 0.14f;

	angle = (float)(system_milliseconds() % 6000u) * (6.2831853f / 6000.0f);
	preview_fwd.i = (float)cosine(angle);
	preview_fwd.j = (float)sine(angle);
	preview_fwd.k = 0.0f;
	preview_up.i = 0.0f;
	preview_up.j = 0.0f;
	preview_up.k = 1.0f;

	draw_native_object_model_at(
		obj_def_idx,
		object_index,
		lighting_ptr,
		&preview_pos,
		&preview_fwd,
		&preview_up,
		0,
		1,
		0);
}

void forge_render_world_3d_overlays(void const *lighting_ptr)
{
	if (!g_forge_session_enabled || !g_forge.initialized)
		return;
	if (g_forge.mode == _forge_mode_3d_monitor && g_forge.third_person_view && !s_local_monitor_drawn_this_frame)
	{
		long local_unit = player_control_get_unit_index(0);
		if (local_unit != NONE)
			forge_monitor_render_player_unit(local_unit, lighting_ptr, 0);
	}
	s_local_monitor_drawn_this_frame = 0;
	render_world_objective_and_spawn_markers();
	render_spawn_palette_3d_preview(NONE, lighting_ptr);
}

int forge_monitor_render_player_unit(long object_index, void const *lighting_ptr, unsigned long flags)
{
	struct unit_datum *unit;

	if (!g_forge_session_enabled || object_index == NONE)
		return 0;

	/* Check if this object is a player biped in Forge Monitor mode (per-player in MP!) */
	unit = (struct unit_datum *)object_try_and_get_and_verify_type(object_index, _object_mask_biped);
	if (!unit || unit->object.type != _object_type_biped || unit->unit.player_index == NONE)
		return 0;

	if (!forge_is_unit_in_monitor_mode(object_index))
		return 0;

	if (flags & (1u << 1)) /* Skip shadow pass for Monitor */
		return 1;

	if (!s_monitor_resolved || s_monitor_bipd_handle == NONE)
	{
		uint32_t actual_grp = 'bipd';
		s_monitor_resolved = 1;
		s_monitor_bipd_handle = forge_resolve_engine_tag('bipd', "characters\\monitor\\monitor", &actual_grp);
	}
	if (s_monitor_bipd_handle == NONE)
		return 0;

	if (object_index == player_control_get_unit_index(0))
	{
		union real_point3d pos;
		union real_vector3d fwd, up;
		float cy, sy, cp, sp;

		if (!g_forge.third_person_view)
			return 1;
		s_local_monitor_drawn_this_frame = 1;

		/* Place local Monitor at exact live camera anchor (draw_native_object_model_at adds +0.55f to Z) */
		pos.x = g_forge.cam_x;
		pos.y = g_forge.cam_y;
		pos.z = g_forge.cam_z - 0.55f;

		cy = (float)cosine(g_forge.cam_yaw);
		sy = (float)sine(g_forge.cam_yaw);
		cp = (float)cosine(g_forge.cam_pitch);
		sp = (float)sine(g_forge.cam_pitch);
		fwd.i = cy * cp;
		fwd.j = sy * cp;
		fwd.k = sp;
		up.i = -cy * sp;
		up.j = -sy * sp;
		up.k = cp;

		return draw_native_object_model_at(
			s_monitor_bipd_handle,
			NONE,
			lighting_ptr,
			&pos,
			&fwd,
			&up,
			1,
			0,
			flags);
	}

	{
		union real_point3d pos = unit->object.position;
		union real_vector3d fwd = unit->object.forward;
		union real_vector3d up = unit->object.up;
		int p_abs = (int)((uint32_t)unit->unit.player_index & 0xFFFFu);

		if (p_abs >= 0 && p_abs < 128 && g_forge.player_has_monitor_pos[p_abs])
		{
			float rx = g_forge.player_has_render_pos[p_abs]
				? g_forge.player_monitor_render_pos[p_abs][0]
				: g_forge.player_monitor_pos[p_abs][0];
			float ry = g_forge.player_has_render_pos[p_abs]
				? g_forge.player_monitor_render_pos[p_abs][1]
				: g_forge.player_monitor_pos[p_abs][1];
			float rz = g_forge.player_has_render_pos[p_abs]
				? g_forge.player_monitor_render_pos[p_abs][2]
				: g_forge.player_monitor_pos[p_abs][2];
			float ryaw = g_forge.player_has_render_pos[p_abs]
				? g_forge.player_monitor_render_yaw[p_abs]
				: g_forge.player_monitor_yaw[p_abs];
			float rpitch = g_forge.player_has_render_pos[p_abs]
				? g_forge.player_monitor_render_pitch[p_abs]
				: g_forge.player_monitor_pitch[p_abs];
			float cy = (float)cosine(ryaw);
			float sy = (float)sine(ryaw);
			float cp = (float)cosine(rpitch);
			float sp = (float)sine(rpitch);

			pos.x = rx;
			pos.y = ry;
			pos.z = rz - 0.55f;
			fwd.i = cy * cp;
			fwd.j = sy * cp;
			fwd.k = sp;
			up.i = -cy * sp;
			up.j = -sy * sp;
			up.k = cp;
		}

		return draw_native_object_model_at(
			s_monitor_bipd_handle,
			NONE,
			lighting_ptr,
			&pos,
			&fwd,
			&up,
			1,
			0,
			flags);
	}
}

/* =========================================================================
 * FORGE TAG OPTIONS: MANIFEST.BIN, DONOR MAP .OCETAGS EXTRACTOR,
 * STOCK/CUSTOM CACHE RESET, AND LIVE 140x114 3D TAG PREVIEWER
 * ========================================================================= */

extern void rasterizer_bitmap_changed(struct bitmap_data *bitmap);

#define FORGE_MANIFEST_MAGIC 0x464E4D4Fu /* 'OMNF' */

struct forge_manifest_bin_record
{
	uint32_t group_tag;
	uint32_t path_hash;
	uint32_t compressed_size;
	uint8_t is_custom;
	uint8_t category;
	uint16_t pad;
	char tag_path[256];
	char custom_name[64];
	char file_name[128];
};

static uint32_t forge_fnv1a_hash_ci(uint32_t grp, char const *s)
{
	uint32_t h = 2166136261u ^ grp;
	if (s)
	{
		while (*s)
		{
			unsigned char c = (unsigned char)*s++;
			if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
			if (c == '/') c = '\\';
			h ^= (uint32_t)c;
			h *= 16777619u;
		}
	}
	return h;
}

static void forge_make_safe_ocetags_stem(uint32_t grp, char const *tag_path, char *out_stem, size_t out_sz)
{
	char const *leaf = tag_path;
	char const *p;
	char clean[64];
	size_t ci = 0;
	uint32_t h;
	char gstr[5];

	if (!tag_path) tag_path = "tag";
	for (p = tag_path; *p; p++)
	{
		if (*p == '\\' || *p == '/')
			leaf = p + 1;
	}
	for (p = leaf; *p && ci + 1 < sizeof(clean); p++)
	{
		char c = *p;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
			clean[ci++] = c;
		else if (c == ' ')
			clean[ci++] = '_';
	}
	if (ci == 0)
	{
		clean[0] = 't'; clean[1] = 'a'; clean[2] = 'g'; ci = 3;
	}
	clean[ci] = '\0';
	gstr[0] = (char)((grp >> 24) & 0xFF);
	gstr[1] = (char)((grp >> 16) & 0xFF);
	gstr[2] = (char)((grp >> 8) & 0xFF);
	gstr[3] = (char)(grp & 0xFF);
	gstr[4] = '\0';
	for (ci = 0; ci < 4; ci++)
	{
		if (!((gstr[ci] >= 'a' && gstr[ci] <= 'z') || (gstr[ci] >= 'A' && gstr[ci] <= 'Z') || (gstr[ci] >= '0' && gstr[ci] <= '9')))
			gstr[ci] = '_';
	}
	h = forge_fnv1a_hash_ci(grp, tag_path);
	snprintf(out_stem, out_sz, "%s_%s_%08X", gstr, clean, (unsigned int)h);
}

static void forge_sync_manifest_bin(void)
{
#ifdef _WIN32
	static char const *const s_manifest_paths[] = {
		"OpenCE Forge/cached_tags/manifest.bin",
		"Faber_data/cached_tags/manifest.bin"
	};
	struct forge_manifest_bin_record *recs;
	int rec_count = 0;
	int i, m;

	CreateDirectoryA("OpenCE Forge", NULL);
	CreateDirectoryA("OpenCE Forge/custom_tags", NULL);
	CreateDirectoryA("OpenCE Forge/cached_tags", NULL);
	CreateDirectoryA("Faber_data", NULL);
	CreateDirectoryA("Faber_data/custom_tags", NULL);
	CreateDirectoryA("Faber_data/cached_tags", NULL);

	recs = (struct forge_manifest_bin_record *)calloc(FORGE_MAX_CATALOG_ENTRIES, sizeof(*recs));
	if (!recs)
		return;

	for (i = 0; i < g_forge.catalog_count && rec_count < FORGE_MAX_CATALOG_ENTRIES; i++)
	{
		struct forge_catalog_entry const *ce = &g_forge.catalog[i];
		struct forge_manifest_bin_record *r;
		char stem[128];
		char test_path[512];
		FILE *fp;

		if (ce->group_tag == 'spwn' || ce->group_tag == 'tele' || ce->group_tag == 'flag' || ce->group_tag == 'ffsp')
			continue;

		r = &recs[rec_count++];
		r->group_tag = ce->group_tag;
		r->path_hash = forge_fnv1a_hash_ci(ce->group_tag, ce->tag_path);
		r->compressed_size = 0;
		r->is_custom = (ce->source_cache_id == 2) ? 1u : 0u;
		r->category = ce->category;
		r->pad = 0;
		snprintf(r->tag_path, sizeof(r->tag_path), "%s", ce->tag_path);
		snprintf(r->custom_name, sizeof(r->custom_name), "%s", ce->display_name);

		forge_make_safe_ocetags_stem(ce->group_tag, ce->tag_path, stem, sizeof(stem));
		snprintf(r->file_name, sizeof(r->file_name), "%s.ocetags", stem);
		snprintf(test_path, sizeof(test_path), "OpenCE Forge/cached_tags/%s.ocetags", stem);
		fp = fopen(test_path, "rb");
		if (!fp)
		{
			snprintf(test_path, sizeof(test_path), "Faber_data/custom_tags/%s.ocetags", stem);
			fp = fopen(test_path, "rb");
		}
		if (fp)
		{
			fseek(fp, 0, SEEK_END);
			r->compressed_size = (uint32_t)ftell(fp);
			fclose(fp);
		}
	}

	for (m = 0; m < 2; m++)
	{
		FILE *bf = fopen(s_manifest_paths[m], "wb");
		if (bf)
		{
			uint32_t hdr[3];
			hdr[0] = FORGE_MANIFEST_MAGIC;
			hdr[1] = 1u;
			hdr[2] = (uint32_t)rec_count;
			fwrite(hdr, sizeof(uint32_t), 3, bf);
			if (rec_count > 0)
				fwrite(recs, sizeof(struct forge_manifest_bin_record), (size_t)rec_count, bf);
			fclose(bf);
		}
	}
	free(recs);
#endif
}

static char const s_b64_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *forge_base64_encode_alloc(uint8_t const *src, size_t len, size_t *out_b64_len)
{
	size_t out_cap = ((len + 2u) / 3u) * 4u + 1u;
	char *out = (char *)malloc(out_cap);
	size_t i = 0, o = 0;

	if (out_b64_len) *out_b64_len = 0;
	if (!out)
		return NULL;
	while (i + 2u < len)
	{
		uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1u] << 8) | (uint32_t)src[i + 2u];
		out[o++] = s_b64_alpha[(v >> 18) & 0x3Fu];
		out[o++] = s_b64_alpha[(v >> 12) & 0x3Fu];
		out[o++] = s_b64_alpha[(v >> 6) & 0x3Fu];
		out[o++] = s_b64_alpha[v & 0x3Fu];
		i += 3u;
	}
	if (i < len)
	{
		uint32_t v = (uint32_t)src[i] << 16;
		if (i + 1u < len)
			v |= (uint32_t)src[i + 1u] << 8;
		out[o++] = s_b64_alpha[(v >> 18) & 0x3Fu];
		out[o++] = s_b64_alpha[(v >> 12) & 0x3Fu];
		out[o++] = (i + 1u < len) ? s_b64_alpha[(v >> 6) & 0x3Fu] : '=';
		out[o++] = '=';
	}
	out[o] = '\0';
	if (out_b64_len) *out_b64_len = o;
	return out;
}

struct forge_str_builder
{
	char *buf;
	size_t len;
	size_t cap;
};

static int fsb_reserve(struct forge_str_builder *sb, size_t extra)
{
	size_t need = sb->len + extra + 1u;
	size_t ncap;
	char *nbuf;
	if (need <= sb->cap)
		return 1;
	ncap = (sb->cap > 0) ? sb->cap : 65536u;
	while (ncap < need)
		ncap *= 2u;
	nbuf = (char *)realloc(sb->buf, ncap);
	if (!nbuf)
		return 0;
	sb->buf = nbuf;
	sb->cap = ncap;
	return 1;
}

static int fsb_append_raw(struct forge_str_builder *sb, char const *s, size_t n)
{
	if (!fsb_reserve(sb, n))
		return 0;
	memcpy(sb->buf + sb->len, s, n);
	sb->len += n;
	sb->buf[sb->len] = '\0';
	return 1;
}

static int fsb_append_str(struct forge_str_builder *sb, char const *s)
{
	return fsb_append_raw(sb, s, strlen(s));
}

static int fsb_append_json_escaped(struct forge_str_builder *sb, char const *s)
{
	char const *p;
	if (!fsb_append_raw(sb, "\"", 1))
		return 0;
	for (p = s ? s : ""; *p; p++)
	{
		if (*p == '\\' || *p == '\"')
		{
			char esc[2];
			esc[0] = '\\';
			esc[1] = *p;
			if (!fsb_append_raw(sb, esc, 2))
				return 0;
		}
		else if ((unsigned char)*p >= 32u)
		{
			if (!fsb_append_raw(sb, p, 1))
				return 0;
		}
	}
	return fsb_append_raw(sb, "\"", 1);
}

struct forge_donor_map_ctx
{
	FILE *map_fp;
	FILE *bitmaps_fp;
	FILE *sounds_fp;
	int is_xbox_v5;
	uint32_t tag_off;
	uint32_t tag_sz;
	uint32_t tag_base;
	uint32_t inst_off;
	int32_t tag_count;
	uint32_t vtx_file_off;
	uint32_t idx_file_off;
	uint8_t *tag_buf;
};

static void forge_donor_map_close(struct forge_donor_map_ctx *ctx)
{
	if (!ctx) return;
	if (ctx->map_fp) fclose(ctx->map_fp);
	if (ctx->bitmaps_fp) fclose(ctx->bitmaps_fp);
	if (ctx->sounds_fp) fclose(ctx->sounds_fp);
	free(ctx->tag_buf);
	memset(ctx, 0, sizeof(*ctx));
}

static FILE *forge_open_companion_map(char const *donor_map_path, char const *companion_name)
{
	char path[512];
	char const *slash = strrchr(donor_map_path, '\\');
	FILE *fp;
	if (!slash) slash = strrchr(donor_map_path, '/');
	if (slash)
	{
		size_t dlen = (size_t)(slash - donor_map_path);
		if (dlen < sizeof(path) - 32u)
		{
			memcpy(path, donor_map_path, dlen);
			snprintf(path + dlen, sizeof(path) - dlen, "/%s", companion_name);
			fp = fopen(path, "rb");
			if (fp) return fp;
		}
	}
	snprintf(path, sizeof(path), "maps/%s", companion_name);
	fp = fopen(path, "rb");
	if (fp) return fp;
	snprintf(path, sizeof(path), "custom_maps/%s", companion_name);
	fp = fopen(path, "rb");
	if (fp) return fp;
	snprintf(path, sizeof(path), "C:/Program Files (x86)/Microsoft Games/Halo Custom Edition/maps/%s", companion_name);
	fp = fopen(path, "rb");
	if (fp) return fp;
	snprintf(path, sizeof(path), "C:/Program Files/Microsoft Games/Halo Custom Edition/maps/%s", companion_name);
	return fopen(path, "rb");
}

static int forge_donor_map_open(char const *donor_map_path, struct forge_donor_map_ctx *ctx)
{
	char open_path[512];
	uint8_t hdr[2048];
	uint32_t head_magic, map_ver, inst_addr;

	memset(ctx, 0, sizeof(*ctx));
	if (!donor_map_path || !donor_map_path[0])
		return 0;

	if (is_xbox_version5_map_file(donor_map_path))
	{
		if (!ensure_xbox_map_decompressed_cache(donor_map_path, open_path, sizeof(open_path)))
			return 0;
		ctx->is_xbox_v5 = 1;
	}
	else
	{
		snprintf(open_path, sizeof(open_path), "%s", donor_map_path);
		ctx->is_xbox_v5 = 0;
	}

	ctx->map_fp = fopen(open_path, "rb");
	if (!ctx->map_fp)
		return 0;
	if (fread(hdr, 1, sizeof(hdr), ctx->map_fp) != sizeof(hdr))
	{
		forge_donor_map_close(ctx);
		return 0;
	}
	head_magic = *(uint32_t const *)(hdr + 0x00);
	map_ver = *(uint32_t const *)(hdr + 0x04);
	if (head_magic != 0x68656164u)
	{
		forge_donor_map_close(ctx);
		return 0;
	}
	if (map_ver == 5u)
		ctx->is_xbox_v5 = 1;

	ctx->tag_off = *(uint32_t const *)(hdr + 0x10);
	ctx->tag_sz = *(uint32_t const *)(hdr + 0x14);
	if (ctx->tag_off < 2048u || ctx->tag_sz < 64u || ctx->tag_sz > 128u * 1024u * 1024u)
	{
		forge_donor_map_close(ctx);
		return 0;
	}
	ctx->tag_buf = (uint8_t *)malloc(ctx->tag_sz);
	if (!ctx->tag_buf ||
		fseek(ctx->map_fp, (long)ctx->tag_off, SEEK_SET) != 0 ||
		fread(ctx->tag_buf, 1, ctx->tag_sz, ctx->map_fp) != ctx->tag_sz)
	{
		forge_donor_map_close(ctx);
		return 0;
	}

	inst_addr = *(uint32_t const *)(ctx->tag_buf + 0x00);
	ctx->tag_count = *(int32_t const *)(ctx->tag_buf + 0x0C);
	ctx->inst_off = ctx->is_xbox_v5 ? 0x24u : 0x28u;
	ctx->tag_base = inst_addr - ctx->inst_off;
	if (ctx->tag_count <= 0 || ctx->tag_count > 32768 ||
		ctx->inst_off + (uint32_t)ctx->tag_count * 32u > ctx->tag_sz)
	{
		forge_donor_map_close(ctx);
		return 0;
	}

	if (!ctx->is_xbox_v5)
	{
		uint32_t vtx_off = *(uint32_t const *)(ctx->tag_buf + 0x14);
		uint32_t idx_delta = *(uint32_t const *)(ctx->tag_buf + 0x1C);
		ctx->vtx_file_off = vtx_off;
		ctx->idx_file_off = vtx_off + idx_delta;
		ctx->bitmaps_fp = forge_open_companion_map(donor_map_path, "bitmaps.map");
		ctx->sounds_fp = forge_open_companion_map(donor_map_path, "sounds.map");
	}
	return 1;
}

static int forge_donor_find_tag_index(struct forge_donor_map_ctx const *ctx, uint32_t grp, char const *tag_path)
{
	char const *lookup_path = tag_path;
	int32_t i;
	int pass;

	if (!ctx || !tag_path || !tag_path[0])
		return -1;
	if (_stricmp(tag_path, "levels\\a30\\devices\\beam emitter\\beam emitter red") == 0)
		lookup_path = "levels\\a30\\devices\\beam emitter\\beam emitter";

	/* Pass 0: exact group match (with mode/mod2 equivalence).
	 * Pass 1: fallback match across placeable object groups (scen/mach/lifi/ctrl/vehi/weap/eqip/bipd)
	 * in case a stock catalog entry's group_tag differs from the map's primary group_tag. */
	for (pass = 0; pass < 2; pass++)
	{
		for (i = 0; i < ctx->tag_count; i++)
		{
			uint32_t io = ctx->inst_off + (uint32_t)i * 32u;
			uint32_t t_grp = *(uint32_t const *)(ctx->tag_buf + io + 0x00);
			uint32_t name_addr = *(uint32_t const *)(ctx->tag_buf + io + 0x10);
			char const *tname;
			if (pass == 0)
			{
				if (t_grp != grp)
				{
					if (!((grp == 'mode' && t_grp == 'mod2') || (grp == 'mod2' && t_grp == 'mode')))
						continue;
				}
			}
			else
			{
				if (t_grp != 'scen' && t_grp != 'mach' && t_grp != 'lifi' && t_grp != 'ctrl' &&
					t_grp != 'vehi' && t_grp != 'weap' && t_grp != 'eqip' && t_grp != 'bipd' &&
					t_grp != 'actv' && t_grp != 'mod2' && t_grp != 'mode')
					continue;
			}
			if (name_addr < ctx->tag_base || (name_addr - ctx->tag_base) >= ctx->tag_sz)
				continue;
			tname = (char const *)(ctx->tag_buf + (name_addr - ctx->tag_base));
			if (_stricmp(tname, lookup_path) == 0 || (lookup_path != tag_path && _stricmp(tname, tag_path) == 0))
				return (int)i;
		}
	}
	return -1;
}

struct forge_ext_closure_list
{
	int32_t tag_indices[512];
	int count;
};

static uint8_t *forge_donor_read_ext_bitmap_tag(FILE *bitmaps_fp, uint32_t item_index, uint32_t *out_size)
{
	uint8_t hdr[16];
	uint8_t item[12];
	uint32_t index_offset, item_size, item_data_off;
	int32_t item_count;
	uint8_t *buf;

	if (out_size)
		*out_size = 0;
	if (!bitmaps_fp)
		return NULL;
	if (fseek(bitmaps_fp, 0, SEEK_SET) != 0 || fread(hdr, 1, 16, bitmaps_fp) != 16)
		return NULL;
	index_offset = *(uint32_t const *)(hdr + 8u);
	item_count = *(int32_t const *)(hdr + 12u);
	if (item_count <= 0 || item_count > 65536 || item_index >= (uint32_t)item_count || index_offset < 16u)
		return NULL;
	if (fseek(bitmaps_fp, (long)(index_offset + item_index * 12u), SEEK_SET) != 0 ||
		fread(item, 1, 12, bitmaps_fp) != 12)
		return NULL;
	item_size = *(uint32_t const *)(item + 4u);
	item_data_off = *(uint32_t const *)(item + 8u);
	if (item_size < 0x6Cu || item_size > 256u * 1024u)
		return NULL;
	buf = (uint8_t *)malloc(item_size);
	if (!buf)
		return NULL;
	if (fseek(bitmaps_fp, (long)item_data_off, SEEK_SET) != 0 ||
		fread(buf, 1, item_size, bitmaps_fp) != item_size)
	{
		free(buf);
		return NULL;
	}
	if (out_size)
		*out_size = item_size;
	return buf;
}

static void forge_donor_collect_schema_refs(
	struct forge_donor_map_ctx const *ctx,
	struct tag_schema_definition const *def,
	uint32_t elem_off,
	int32_t elem_count,
	struct forge_ext_closure_list *list,
	int depth);

static void forge_donor_add_tag_closure(
	struct forge_donor_map_ctx const *ctx,
	int32_t tag_idx,
	struct forge_ext_closure_list *list,
	int depth)
{
	uint32_t io, grp, data_addr, ext_flag;
	struct tag_schema_group const *schema_grp;
	int i;

	if (tag_idx < 0 || tag_idx >= ctx->tag_count || list->count >= 512 || depth > 24)
		return;
	for (i = 0; i < list->count; i++)
	{
		if (list->tag_indices[i] == tag_idx)
			return;
	}
	io = ctx->inst_off + (uint32_t)tag_idx * 32u;
	grp = *(uint32_t const *)(ctx->tag_buf + io + 0x00);
	data_addr = *(uint32_t const *)(ctx->tag_buf + io + 0x14);
	ext_flag = *(uint32_t const *)(ctx->tag_buf + io + 0x18);
	if (grp == 'scnr' || grp == 'sbsp' || grp == 'matg' || grp == 'ui  ')
		return;
	if (!ctx->is_xbox_v5 && ext_flag != 0)
	{
		/* Include external 'bitm' tags indexed in bitmaps.map so .ocetags bundles have diffuse textures */
		if (grp == 'bitm' && ctx->bitmaps_fp != NULL)
			list->tag_indices[list->count++] = tag_idx;
		return;
	}
	if (data_addr < ctx->tag_base || (data_addr - ctx->tag_base) >= ctx->tag_sz)
		return;

	schema_grp = forge_runtime_schema_group_get(grp);
	if (!schema_grp || !schema_grp->definition || schema_grp->definition->size <= 0)
		return;
	if ((data_addr - ctx->tag_base) + (uint32_t)schema_grp->definition->size > ctx->tag_sz)
		return;

	list->tag_indices[list->count++] = tag_idx;
	forge_donor_collect_schema_refs(
		ctx,
		schema_grp->definition,
		data_addr - ctx->tag_base,
		1,
		list,
		depth + 1);
}

static void forge_donor_collect_schema_refs(
	struct forge_donor_map_ctx const *ctx,
	struct tag_schema_definition const *def,
	uint32_t elem_off,
	int32_t elem_count,
	struct forge_ext_closure_list *list,
	int depth)
{
	int32_t e;
	uint32_t elem_size;
	struct tag_schema_field const *field;

	if (!def || !def->fields || elem_count <= 0 || def->size <= 0 || depth > 24)
		return;
	elem_size = (uint32_t)def->size;
	if ((uint64_t)elem_off + (uint64_t)elem_count * elem_size > (uint64_t)ctx->tag_sz)
		return;

	for (e = 0; e < elem_count; e++)
	{
		uint32_t base_off = elem_off + (uint32_t)e * elem_size;
		for (field = def->fields; field->type != _tag_schema_terminator; field++)
		{
			int16_t k;
			int16_t fcount = (field->count > 0) ? field->count : 1;
			uint32_t fstride = (field->size > 0) ? (uint32_t)field->size : 0;
			for (k = 0; k < fcount; k++)
			{
				uint32_t f_off = base_off + (uint32_t)field->offset + (uint32_t)k * fstride;
				uint8_t const *f_ptr = ctx->tag_buf + f_off;
				if (field->type == _tag_schema_struct)
				{
					struct tag_schema_definition const *sub_def =
						(struct tag_schema_definition const *)field->definition;
					if (sub_def && f_off + (uint32_t)sub_def->size <= base_off + elem_size)
						forge_donor_collect_schema_refs(ctx, sub_def, f_off, 1, list, depth + 1);
				}
				else if (field->type == _tag_schema_block)
				{
					struct tag_schema_definition const *child_def =
						(struct tag_schema_definition const *)field->definition;
					if (child_def && child_def->size > 0 && f_off + 12u <= base_off + elem_size)
					{
						int32_t cnt = *(int32_t const *)(f_ptr + 0);
						uint32_t addr = *(uint32_t const *)(f_ptr + 4);
						uint32_t child_sz = (uint32_t)child_def->size;
						if (ctx->is_xbox_v5 && child_def->size == 132)
							child_sz = 104u;
						if (cnt > 0 && cnt <= 65536 && addr >= ctx->tag_base &&
							(uint64_t)(addr - ctx->tag_base) + (uint64_t)cnt * child_sz <= (uint64_t)ctx->tag_sz)
						{
							if (!(ctx->is_xbox_v5 && child_def->size == 132))
								forge_donor_collect_schema_refs(ctx, child_def, addr - ctx->tag_base, cnt, list, depth + 1);
						}
					}
				}
				else if (field->type == _tag_schema_reference)
				{
					if (f_off + 16u <= base_off + elem_size)
					{
						int32_t ref_handle = *(int32_t const *)(f_ptr + 12);
						if (ref_handle != -1 && ref_handle != 0)
						{
							int32_t ref_low = (int32_t)((uint32_t)ref_handle & 0xFFFFu);
							if (ref_low >= 0 && ref_low < ctx->tag_count)
								forge_donor_add_tag_closure(ctx, ref_low, list, depth + 1);
						}
					}
				}
				else if (field->type == _tag_schema_tag_index)
				{
					if (f_off + 4u <= base_off + elem_size)
					{
						int32_t ref_handle = *(int32_t const *)f_ptr;
						if (ref_handle != -1 && ref_handle != 0)
						{
							int32_t ref_low = (int32_t)((uint32_t)ref_handle & 0xFFFFu);
							if (ref_low >= 0 && ref_low < ctx->tag_count)
								forge_donor_add_tag_closure(ctx, ref_low, list, depth + 1);
						}
					}
				}
			}
		}
	}
}

struct forge_flat_buf
{
	uint8_t *data;
	uint32_t size;
	uint32_t cap;
};

static uint32_t ffb_alloc(struct forge_flat_buf *fb, uint32_t bytes)
{
	uint32_t aligned_sz = (fb->size + 3u) & ~3u;
	uint32_t need = aligned_sz + bytes;
	if (need > fb->cap)
	{
		uint32_t ncap = (fb->cap > 0) ? fb->cap : 16384u;
		uint8_t *ndata;
		while (ncap < need)
			ncap *= 2u;
		ndata = (uint8_t *)realloc(fb->data, ncap);
		if (!ndata)
			return 0xFFFFFFFFu;
		memset(ndata + fb->cap, 0, ncap - fb->cap);
		fb->data = ndata;
		fb->cap = ncap;
	}
	if (aligned_sz > fb->size)
		memset(fb->data + fb->size, 0, aligned_sz - fb->size);
	fb->size = need;
	return aligned_sz;
}

static void forge_donor_flatten_schema(
	struct forge_donor_map_ctx const *ctx,
	struct tag_schema_definition const *def,
	uint32_t src_elem_off,
	uint32_t dst_elem_off,
	int32_t elem_count,
	struct forge_flat_buf *fb,
	int depth)
{
	int32_t e;
	uint32_t elem_size;
	struct tag_schema_field const *field;

	if (!def || !def->fields || elem_count <= 0 || def->size <= 0 || depth > 24)
		return;
	elem_size = (uint32_t)def->size;

	for (e = 0; e < elem_count; e++)
	{
		uint32_t s_base = src_elem_off + (uint32_t)e * elem_size;
		uint32_t d_base = dst_elem_off + (uint32_t)e * elem_size;
		for (field = def->fields; field->type != _tag_schema_terminator; field++)
		{
			int16_t k;
			int16_t fcount = (field->count > 0) ? field->count : 1;
			uint32_t fstride = (field->size > 0) ? (uint32_t)field->size : 0;
			for (k = 0; k < fcount; k++)
			{
				uint32_t f_rel = (uint32_t)field->offset + (uint32_t)k * fstride;
				uint32_t s_foff = s_base + f_rel;
				uint32_t d_foff = d_base + f_rel;
				if (field->type == _tag_schema_struct)
				{
					struct tag_schema_definition const *sub_def =
						(struct tag_schema_definition const *)field->definition;
					if (sub_def && f_rel + (uint32_t)sub_def->size <= elem_size)
						forge_donor_flatten_schema(ctx, sub_def, s_foff, d_foff, 1, fb, depth + 1);
				}
				else if (field->type == _tag_schema_block)
				{
					struct tag_schema_definition const *child_def =
						(struct tag_schema_definition const *)field->definition;
					if (child_def && child_def->size > 0 && f_rel + 12u <= elem_size)
					{
						int32_t cnt = *(int32_t const *)(ctx->tag_buf + s_foff + 0);
						uint32_t addr = *(uint32_t const *)(ctx->tag_buf + s_foff + 4);
						uint32_t child_sz = (uint32_t)child_def->size;
						if (ctx->is_xbox_v5 && child_sz == 132u)
						{
							/* Convert Xbox 104-byte model_geometry_part array to 132-byte custom_edition_model_part array */
							if (cnt > 0 && cnt <= 1024 && addr >= ctx->tag_base &&
								(uint64_t)(addr - ctx->tag_base) + (uint64_t)cnt * 104u <= (uint64_t)ctx->tag_sz)
							{
								uint32_t c_dst = ffb_alloc(fb, (uint32_t)cnt * 132u);
								if (c_dst != 0xFFFFFFFFu)
								{
									int32_t pi;
									for (pi = 0; pi < cnt; pi++)
									{
										uint8_t const *xp = ctx->tag_buf + (addr - ctx->tag_base) + (uint32_t)pi * 104u;
										uint8_t *cp = fb->data + c_dst + (uint32_t)pi * 132u;
										memset(cp, 0, 132u);
										memcpy(cp, xp, 32u);
									}
									*(int32_t *)(fb->data + d_foff + 0) = cnt;
									*(uint32_t *)(fb->data + d_foff + 4) = c_dst;
									*(uint32_t *)(fb->data + d_foff + 8) = 0;
								}
								else
								{
									memset(fb->data + d_foff, 0, 12);
								}
							}
							else
							{
								memset(fb->data + d_foff, 0, 12);
							}
						}
						else if (cnt > 0 && cnt <= 65536 && addr >= ctx->tag_base &&
							(uint64_t)(addr - ctx->tag_base) + (uint64_t)cnt * child_sz <= (uint64_t)ctx->tag_sz)
						{
							uint32_t c_src = addr - ctx->tag_base;
							uint32_t c_dst = ffb_alloc(fb, (uint32_t)cnt * child_sz);
							if (c_dst != 0xFFFFFFFFu)
							{
								memcpy(fb->data + c_dst, ctx->tag_buf + c_src, (size_t)cnt * child_sz);
								*(int32_t *)(fb->data + d_foff + 0) = cnt;
								*(uint32_t *)(fb->data + d_foff + 4) = c_dst;
								*(uint32_t *)(fb->data + d_foff + 8) = 0;
								forge_donor_flatten_schema(ctx, child_def, c_src, c_dst, cnt, fb, depth + 1);
							}
							else
							{
								memset(fb->data + d_foff, 0, 12);
							}
						}
						else
						{
							memset(fb->data + d_foff, 0, 12);
						}
					}
				}
				else if (field->type == _tag_schema_data)
				{
					if (f_rel + 20u <= elem_size)
					{
						int32_t sz = *(int32_t const *)(ctx->tag_buf + s_foff + 0);
						uint32_t ext_flags = *(uint32_t const *)(ctx->tag_buf + s_foff + 4);
						uint32_t addr = *(uint32_t const *)(ctx->tag_buf + s_foff + 12);
						if ((ext_flags & 1u) == 0 && sz > 0 && sz <= 16 * 1024 * 1024 &&
							addr >= ctx->tag_base &&
							(uint64_t)(addr - ctx->tag_base) + (uint64_t)sz <= (uint64_t)ctx->tag_sz)
						{
							uint32_t d_dst = ffb_alloc(fb, (uint32_t)sz);
							if (d_dst != 0xFFFFFFFFu)
							{
								memcpy(fb->data + d_dst, ctx->tag_buf + (addr - ctx->tag_base), (size_t)sz);
								*(int32_t *)(fb->data + d_foff + 0) = sz;
								*(uint32_t *)(fb->data + d_foff + 4) = 0;
								*(uint32_t *)(fb->data + d_foff + 8) = 0;
								*(uint32_t *)(fb->data + d_foff + 12) = d_dst;
								*(uint32_t *)(fb->data + d_foff + 16) = 0;
							}
							else
							{
								memset(fb->data + d_foff, 0, 20);
							}
						}
						else
						{
							memset(fb->data + d_foff, 0, 20);
						}
					}
				}
			}
		}
	}
}

static void forge_decompress_xbox_vertex_to_68b(uint8_t const *cv, uint8_t *uv)
{
	uint32_t c_norm, c_binorm, c_tan;
	int16_t u_s, v_s, n0_b, n1_b;
	uint16_t w0_u;
	float nx, ny, nz;

	memset(uv, 0, 68u);
	memcpy(uv + 0u, cv + 0u, 12u); /* x, y, z */

	/* Decode 11-11-10 normal */
	c_norm = *(uint32_t const *)(cv + 12u);
	nx = (float)((int32_t)(c_norm << 21) >> 21) / 1023.0f;
	ny = (float)((int32_t)(c_norm << 10) >> 21) / 1023.0f;
	nz = (float)((int32_t)c_norm >> 22) / 511.0f;
	*(float *)(uv + 12u) = nx;
	*(float *)(uv + 16u) = ny;
	*(float *)(uv + 20u) = nz;

	c_binorm = *(uint32_t const *)(cv + 16u);
	*(float *)(uv + 24u) = (float)((int32_t)(c_binorm << 21) >> 21) / 1023.0f;
	*(float *)(uv + 28u) = (float)((int32_t)(c_binorm << 10) >> 21) / 1023.0f;
	*(float *)(uv + 32u) = (float)((int32_t)c_binorm >> 22) / 511.0f;

	c_tan = *(uint32_t const *)(cv + 20u);
	*(float *)(uv + 36u) = (float)((int32_t)(c_tan << 21) >> 21) / 1023.0f;
	*(float *)(uv + 40u) = (float)((int32_t)(c_tan << 10) >> 21) / 1023.0f;
	*(float *)(uv + 44u) = (float)((int32_t)c_tan >> 22) / 511.0f;

	u_s = *(int16_t const *)(cv + 24u);
	v_s = *(int16_t const *)(cv + 26u);
	*(float *)(uv + 48u) = (float)u_s / 32767.0f;
	*(float *)(uv + 52u) = (float)v_s / 32767.0f;

	n0_b = (int16_t)((uint8_t)cv[28] / 3u);
	n1_b = (int16_t)((uint8_t)cv[29] / 3u);
	w0_u = *(uint16_t const *)(cv + 30u);
	*(int16_t *)(uv + 56u) = n0_b;
	*(int16_t *)(uv + 58u) = n1_b;
	{
		int16_t w0_s = (int16_t)w0_u;
		float w0_f = (float)w0_s / 32767.0f;
		if (w0_f < 0.0f) w0_f = 0.0f;
		if (w0_f > 1.0f) w0_f = 1.0f;
		*(float *)(uv + 60u) = w0_f;
		*(float *)(uv + 64u) = 1.0f - w0_f;
	}
}

int forge_extract_tag_from_donor_map_to_ocetags(
	char const *donor_map_path,
	uint32_t root_group,
	char const *root_tag_path,
	char const *user_custom_name)
{
	struct forge_donor_map_ctx ctx;
	struct forge_ext_closure_list *closure = NULL;
	struct forge_str_builder sb;
	int root_idx;
	int ci;
	char clean_custom_name[64];
	char safe_stem[128];
	char out_path[512];
	char gstr[5];
	int cat_id;

	if (!donor_map_path || !donor_map_path[0] || !root_tag_path || !root_tag_path[0])
		return 0;

	clean_custom_name[0] = '\0';
	if (user_custom_name && user_custom_name[0])
	{
		char const *p = user_custom_name;
		while (*p == ' ' || *p == '\t') p++;
		if (_strnicmp(p, "[c] ", 4) == 0) p += 4;
		else if (_strnicmp(p, "[c]", 3) == 0) p += 3;
		while (*p == ' ' || *p == '\t') p++;
		snprintf(clean_custom_name, sizeof(clean_custom_name), "%s", p);
	}
	if (!clean_custom_name[0])
	{
		char const *leaf = root_tag_path;
		char const *p;
		for (p = root_tag_path; *p; p++)
		{
			if (*p == '\\' || *p == '/')
				leaf = p + 1;
		}
		snprintf(clean_custom_name, sizeof(clean_custom_name), "%s", leaf);
	}

	if (!forge_donor_map_open(donor_map_path, &ctx))
		return 0;

	root_idx = forge_donor_find_tag_index(&ctx, root_group, root_tag_path);
	if (root_idx < 0)
	{
		forge_donor_map_close(&ctx);
		return 0;
	}

	closure = (struct forge_ext_closure_list *)calloc(1, sizeof(*closure));
	if (!closure)
	{
		forge_donor_map_close(&ctx);
		return 0;
	}
	forge_donor_add_tag_closure(&ctx, root_idx, closure, 0);
	if (closure->count <= 0)
	{
		free(closure);
		forge_donor_map_close(&ctx);
		return 0;
	}

	memset(&sb, 0, sizeof(sb));
	fsb_append_str(&sb, "{\n  \"formatVersion\": 5,\n  \"itemName\": ");
	fsb_append_json_escaped(&sb, clean_custom_name);
	fsb_append_str(&sb, ",\n  \"tags\": [\n");

	{
	int emitted_tags = 0;
	for (ci = 0; ci < closure->count; ci++)
	{
		int32_t tidx = closure->tag_indices[ci];
		uint32_t io = ctx.inst_off + (uint32_t)tidx * 32u;
		uint32_t grp = *(uint32_t const *)(ctx.tag_buf + io + 0x00);
		uint32_t orig_handle = *(uint32_t const *)(ctx.tag_buf + io + 0x0C);
		uint32_t name_addr = *(uint32_t const *)(ctx.tag_buf + io + 0x10);
		uint32_t data_addr = *(uint32_t const *)(ctx.tag_buf + io + 0x14);
		uint32_t ext_flag = *(uint32_t const *)(ctx.tag_buf + io + 0x18);
		char const *tname = (char const *)(ctx.tag_buf + (name_addr - ctx.tag_base));
		uint32_t out_grp = (grp == 'mode') ? 'mod2' : grp;
		struct tag_schema_group const *schema_grp = forge_runtime_schema_group_get(out_grp);
		struct forge_flat_buf fb;
		uint32_t root_sz;
		char *b64_data;
		char t_gstr[5];
		char num_buf[64];

		if (!schema_grp || !schema_grp->definition || schema_grp->definition->size <= 0)
			continue;
		root_sz = (uint32_t)schema_grp->definition->size;

		memset(&fb, 0, sizeof(fb));
		if (!ctx.is_xbox_v5 && ext_flag != 0)
		{
			uint32_t ext_sz = 0;
			uint8_t *ext_buf = (grp == 'bitm') ? forge_donor_read_ext_bitmap_tag(ctx.bitmaps_fp, data_addr, &ext_sz) : NULL;
			if (!ext_buf || ext_sz < root_sz)
			{
				free(ext_buf);
				continue;
			}
			if (ffb_alloc(&fb, ext_sz) == 0xFFFFFFFFu)
			{
				free(ext_buf);
				continue;
			}
			memcpy(fb.data, ext_buf, ext_sz);
			free(ext_buf);
		}
		else
		{
			if (ffb_alloc(&fb, root_sz) == 0xFFFFFFFFu)
				continue;
			memcpy(fb.data, ctx.tag_buf + (data_addr - ctx.tag_base), root_sz);
			forge_donor_flatten_schema(&ctx, schema_grp->definition, data_addr - ctx.tag_base, 0, 1, &fb, 0);
		}

		t_gstr[0] = (char)((out_grp >> 24) & 0xFF);
		t_gstr[1] = (char)((out_grp >> 16) & 0xFF);
		t_gstr[2] = (char)((out_grp >> 8) & 0xFF);
		t_gstr[3] = (char)(out_grp & 0xFF);
		t_gstr[4] = '\0';

		if (emitted_tags++ > 0)
			fsb_append_str(&sb, ",\n");
		fsb_append_str(&sb, "    {\n      \"group\": ");
		fsb_append_json_escaped(&sb, t_gstr);
		fsb_append_str(&sb, ",\n      \"path\": ");
		fsb_append_json_escaped(&sb, tname);
		snprintf(num_buf, sizeof(num_buf), ",\n      \"originalIndex\": %u,\n      \"data\": \"", (unsigned int)orig_handle);
		fsb_append_str(&sb, num_buf);

		b64_data = forge_base64_encode_alloc(fb.data, fb.size, NULL);
		if (b64_data)
		{
			fsb_append_str(&sb, b64_data);
			free(b64_data);
		}
		fsb_append_str(&sb, "\"");

		/* If 'mod2' / 'mode', serialize gbxmodelGeometryParts[] */
		if ((grp == 'mod2' || grp == 'mode') && fb.size >= 0xE8u)
		{
			uint32_t src_mod_off = data_addr - ctx.tag_base;
			int32_t geom_cnt = *(int32_t const *)(ctx.tag_buf + src_mod_off + 0xD0u);
			uint32_t geom_addr = *(uint32_t const *)(ctx.tag_buf + src_mod_off + 0xD4u);
			if (geom_cnt > 0 && geom_cnt <= 256 && geom_addr >= ctx.tag_base &&
				(uint64_t)(geom_addr - ctx.tag_base) + (uint64_t)geom_cnt * 48u <= (uint64_t)ctx.tag_sz)
			{
				int g_idx, first_gp = 1;
				fsb_append_str(&sb, ",\n      \"gbxmodelGeometryParts\": [\n");
				for (g_idx = 0; g_idx < geom_cnt; g_idx++)
				{
					uint32_t g_off = (geom_addr - ctx.tag_base) + (uint32_t)g_idx * 48u;
					int32_t part_cnt = *(int32_t const *)(ctx.tag_buf + g_off + 36u);
					uint32_t parts_addr = *(uint32_t const *)(ctx.tag_buf + g_off + 40u);
					uint32_t part_stride = ctx.is_xbox_v5 ? 104u : 132u;
					int p_idx;

					if (part_cnt <= 0 || part_cnt > 512 || parts_addr < ctx.tag_base ||
						(uint64_t)(parts_addr - ctx.tag_base) + (uint64_t)part_cnt * part_stride > (uint64_t)ctx.tag_sz)
						continue;

					for (p_idx = 0; p_idx < part_cnt; p_idx++)
					{
						uint8_t const *sp = ctx.tag_buf + (parts_addr - ctx.tag_base) + (uint32_t)p_idx * part_stride;
						uint8_t *vbuf_68 = NULL;
						size_t vbuf_bytes = 0;
						uint8_t *ibuf_16 = NULL;
						size_t ibuf_bytes = 0;

						if (!ctx.is_xbox_v5)
						{
							int32_t tri_cnt = *(int32_t const *)(sp + 0x48u);
							uint32_t idx_off = *(uint32_t const *)(sp + 0x4Cu);
							int32_t vtx_cnt = *(int32_t const *)(sp + 0x58u);
							uint32_t vtx_off = *(uint32_t const *)(sp + 0x64u);
							if (vtx_cnt > 0 && vtx_cnt <= 65536 && tri_cnt > 0 && tri_cnt <= 65536)
							{
								vbuf_bytes = (size_t)vtx_cnt * 68u;
								ibuf_bytes = (size_t)(tri_cnt + 2) * 2u;
								vbuf_68 = (uint8_t *)malloc(vbuf_bytes);
								ibuf_16 = (uint8_t *)malloc(ibuf_bytes);
								if (vbuf_68 && ibuf_16 &&
									fseek(ctx.map_fp, (long)(ctx.vtx_file_off + vtx_off), SEEK_SET) == 0 &&
									fread(vbuf_68, 1, vbuf_bytes, ctx.map_fp) == vbuf_bytes &&
									fseek(ctx.map_fp, (long)(ctx.idx_file_off + idx_off), SEEK_SET) == 0 &&
									fread(ibuf_16, 1, ibuf_bytes, ctx.map_fp) == ibuf_bytes)
								{
									/* Read ok */
								}
								else
								{
									free(vbuf_68); vbuf_68 = NULL;
									free(ibuf_16); ibuf_16 = NULL;
								}
							}
						}
						else
						{
							/* Xbox 104-byte part:
							 * triangle_buffer at +68 (count at +72, base_address at +76, hardware_format D3DIndexBuffer* at +80)
							 * vertex_buffer at +84 (type at +84, count at +88, offset at +92, base_address at +96, hardware_format D3DVertexBuffer* at +100)
							 * In Xbox v5 cache files, base_address (+76 / +96) is 0 on disk; hardware_format (+80 / +100) points
							 * to a 12-byte D3D header in the tag cache whose +4u field holds the actual data address. */
							int32_t tri_cnt = *(int32_t const *)(sp + 68u + 4u);
							uint32_t tri_addr = *(uint32_t const *)(sp + 68u + 8u);
							uint32_t tb_hw_addr = *(uint32_t const *)(sp + 68u + 12u);
							int16_t vtx_type = *(int16_t const *)(sp + 84u + 0u);
							int32_t vtx_cnt = *(int32_t const *)(sp + 84u + 4u);
							uint32_t vtx_addr = *(uint32_t const *)(sp + 84u + 12u);
							uint32_t vb_hw_addr = *(uint32_t const *)(sp + 84u + 16u);
							uint32_t v_stride = (vtx_type == _rasterizer_vertex_type_model_uncompressed) ? 68u : 32u;

							if ((tri_addr < ctx.tag_base || (tri_addr - ctx.tag_base) >= ctx.tag_sz) &&
								tb_hw_addr >= ctx.tag_base && (tb_hw_addr - ctx.tag_base) + 12u <= ctx.tag_sz)
							{
								tri_addr = *(uint32_t const *)(ctx.tag_buf + (tb_hw_addr - ctx.tag_base) + 4u);
							}
							if ((vtx_addr < ctx.tag_base || (vtx_addr - ctx.tag_base) >= ctx.tag_sz) &&
								vb_hw_addr >= ctx.tag_base && (vb_hw_addr - ctx.tag_base) + 12u <= ctx.tag_sz)
							{
								vtx_addr = *(uint32_t const *)(ctx.tag_buf + (vb_hw_addr - ctx.tag_base) + 4u);
							}

							if (vtx_cnt > 0 && vtx_cnt <= 65536 && tri_cnt > 0 && tri_cnt <= 65536 &&
								tri_addr >= ctx.tag_base && (tri_addr - ctx.tag_base) + (uint32_t)(tri_cnt + 2) * 2u <= ctx.tag_sz)
							{
								if (vtx_addr >= ctx.tag_base && (vtx_addr - ctx.tag_base) + (uint32_t)vtx_cnt * v_stride <= ctx.tag_sz)
								{
									vbuf_bytes = (size_t)vtx_cnt * 68u;
									ibuf_bytes = (size_t)(tri_cnt + 2) * 2u;
									vbuf_68 = (uint8_t *)malloc(vbuf_bytes);
									ibuf_16 = (uint8_t *)malloc(ibuf_bytes);
									if (vbuf_68 && ibuf_16)
									{
										int32_t vi;
										memcpy(ibuf_16, ctx.tag_buf + (tri_addr - ctx.tag_base), ibuf_bytes);
										if (v_stride == 68u)
										{
											memcpy(vbuf_68, ctx.tag_buf + (vtx_addr - ctx.tag_base), vbuf_bytes);
										}
										else
										{
											for (vi = 0; vi < vtx_cnt; vi++)
											{
												forge_decompress_xbox_vertex_to_68b(
													ctx.tag_buf + (vtx_addr - ctx.tag_base) + (uint32_t)vi * 32u,
													vbuf_68 + (size_t)vi * 68u);
											}
										}
									}
									else
									{
										free(vbuf_68); vbuf_68 = NULL;
										free(ibuf_16); ibuf_16 = NULL;
									}
								}
							}
						}

						if (vbuf_68 && ibuf_16)
						{
							char *vb64 = forge_base64_encode_alloc(vbuf_68, vbuf_bytes, NULL);
							char *ib64 = forge_base64_encode_alloc(ibuf_16, ibuf_bytes, NULL);
							if (vb64 && ib64)
							{
								if (!first_gp)
									fsb_append_str(&sb, ",\n");
								first_gp = 0;
								snprintf(num_buf, sizeof(num_buf),
									"        {\"geometryIndex\": %d, \"partIndex\": %d, \"vertexData\": \"",
									g_idx, p_idx);
								fsb_append_str(&sb, num_buf);
								fsb_append_str(&sb, vb64);
								fsb_append_str(&sb, "\", \"triangleData\": \"");
								fsb_append_str(&sb, ib64);
								fsb_append_str(&sb, "\"}");
							}
							free(vb64);
							free(ib64);
						}
						free(vbuf_68);
						free(ibuf_16);
					}
				}
				fsb_append_str(&sb, "\n      ]");
			}
		}
		/* If 'bitm', serialize embeddedRawDataAssets[] and embeddedRawDataPatches[] */
		else if (grp == 'bitm' && fb.size >= 0x6Cu)
		{
			int32_t b_cnt = *(int32_t const *)(fb.data + 0x60u);
			uint32_t b_rel = *(uint32_t const *)(fb.data + 0x64u);
			if (b_cnt > 0 && b_cnt <= 64 && b_rel >= 0x6Cu && b_rel + (uint32_t)b_cnt * 48u <= fb.size)
			{
				struct forge_str_builder patches_sb;
				int bi, asset_idx = 0;
				memset(&patches_sb, 0, sizeof(patches_sb));
				fsb_append_str(&sb, ",\n      \"embeddedRawDataAssets\": [\n");
				for (bi = 0; bi < b_cnt; bi++)
				{
					uint32_t bmp_rel = b_rel + (uint32_t)bi * 48u;
					uint16_t b_flags = *(uint16_t const *)(fb.data + bmp_rel + 0x0Eu);
					uint32_t pix_off = *(uint32_t const *)(fb.data + bmp_rel + 0x18u);
					int32_t pix_sz = *(int32_t const *)(fb.data + bmp_rel + 0x1Cu);
					FILE *src_fp = (((b_flags & 0x100u) != 0 || ext_flag != 0) && !ctx.is_xbox_v5) ? ctx.bitmaps_fp : ctx.map_fp;
					if (src_fp && pix_sz > 0 && pix_sz <= 16 * 1024 * 1024)
					{
						uint8_t *pbuf = (uint8_t *)malloc((size_t)pix_sz);
						if (pbuf && fseek(src_fp, (long)pix_off, SEEK_SET) == 0 &&
							fread(pbuf, 1, (size_t)pix_sz, src_fp) == (size_t)pix_sz)
						{
							char *pb64 = forge_base64_encode_alloc(pbuf, (size_t)pix_sz, NULL);
							if (pb64)
							{
								char p_entry[128];
								if (asset_idx > 0)
								{
									fsb_append_str(&sb, ",\n");
									fsb_append_str(&patches_sb, ",\n");
								}
								fsb_append_str(&sb, "        {\"data\": \"");
								fsb_append_str(&sb, pb64);
								fsb_append_str(&sb, "\"}");
								snprintf(p_entry, sizeof(p_entry),
									"        {\"assetIndex\": %d, \"patchOffset\": %u}",
									asset_idx, (unsigned int)(bmp_rel + 24u));
								fsb_append_str(&patches_sb, p_entry);
								asset_idx++;
								free(pb64);
							}
						}
						free(pbuf);
					}
				}
				fsb_append_str(&sb, "\n      ],\n      \"embeddedRawDataPatches\": [\n");
				if (patches_sb.buf)
					fsb_append_str(&sb, patches_sb.buf);
				fsb_append_str(&sb, "\n      ]");
				free(patches_sb.buf);
			}
		}

		fsb_append_str(&sb, "\n    }");
		free(fb.data);
	}
	}

	fsb_append_str(&sb, "\n  ]\n}\n");
	free(closure);
	forge_donor_map_close(&ctx);

	if (!sb.buf || sb.len < 32u)
	{
		free(sb.buf);
		return 0;
	}

	CreateDirectoryA("OpenCE Forge", NULL);
	CreateDirectoryA("OpenCE Forge/custom_tags", NULL);
	CreateDirectoryA("OpenCE Forge/cached_tags", NULL);
	CreateDirectoryA("Faber_data", NULL);
	CreateDirectoryA("Faber_data/custom_tags", NULL);
	CreateDirectoryA("Faber_data/cached_tags", NULL);

	forge_make_safe_ocetags_stem(root_group, root_tag_path, safe_stem, sizeof(safe_stem));
	{
		static char const *const s_target_dirs[] = {
			"OpenCE Forge/custom_tags",
			"OpenCE Forge/cached_tags",
			"Faber_data/custom_tags",
			"Faber_data/cached_tags"
		};
		int td;
		for (td = 0; td < 4; td++)
		{
			FILE *tf;
			snprintf(out_path, sizeof(out_path), "%s/%s.ocetags", s_target_dirs[td], safe_stem);
			forge_compress_ocetags_buffer_to_file(out_path, (uint8_t const *)sb.buf, sb.len);
			snprintf(out_path, sizeof(out_path), "%s/%s.txt", s_target_dirs[td], safe_stem);
			tf = fopen(out_path, "wb");
			if (tf)
			{
				fprintf(tf, "%s\n", clean_custom_name);
				fclose(tf);
			}
		}
	}
	free(sb.buf);

	/* Update custom_catalog.txt with the new/updated entry */
	gstr[0] = (char)((root_group >> 24) & 0xFF);
	gstr[1] = (char)((root_group >> 16) & 0xFF);
	gstr[2] = (char)((root_group >> 8) & 0xFF);
	gstr[3] = (char)(root_group & 0xFF);
	gstr[4] = '\0';

	cat_id = is_forge_light_beacon_emitter_tag(root_group, root_tag_path) ? _forge_cat_lights : _forge_cat_scenery;
	if (root_group == 'vehi') cat_id = _forge_cat_vehicles;
	else if (root_group == 'weap') cat_id = _forge_cat_weapons;
	else if (root_group == 'eqip' || root_group == 'garb') cat_id = _forge_cat_equipment;
	else if (root_group == 'bipd') cat_id = _forge_cat_bipeds;
	else if (root_group == 'mach' || root_group == 'ctrl')
		cat_id = is_forge_light_beacon_emitter_tag(root_group, root_tag_path) ? _forge_cat_lights : _forge_cat_barriers_devices;
	else if (root_group == 'lifi') cat_id = _forge_cat_lights;

	{
		static char const *const s_cat_paths[] = {
			"Faber_data/cached_tags/custom_catalog.txt",
			"OpenCE Forge/cached_tags/custom_catalog.txt"
		};
		int cp;
		for (cp = 0; cp < 2; cp++)
		{
			FILE *rf = fopen(s_cat_paths[cp], "rb");
			struct forge_str_builder cat_sb;
			char line[512];
			char needle[300];
			FILE *wf;

			memset(&cat_sb, 0, sizeof(cat_sb));
			snprintf(needle, sizeof(needle), "|2|%s", root_tag_path);
			if (rf)
			{
				while (fgets(line, sizeof(line), rf))
				{
					char const * p_match = strstr(line, needle);
					if (_strnicmp(line, gstr, 4) == 0 && p_match != NULL)
					{
						char after = p_match[strlen(needle)];
						if (after == '\0' || after == '\r' || after == '\n' || after == '|')
							continue;
					}
					fsb_append_str(&cat_sb, line);
				}
				fclose(rf);
			}
			snprintf(line, sizeof(line), "%s|%d|2|%s|%s\n", gstr, cat_id, root_tag_path, clean_custom_name);
			fsb_append_str(&cat_sb, line);
			wf = fopen(s_cat_paths[cp], "wb");
			if (wf)
			{
				if (cat_sb.buf && cat_sb.len > 0)
					fwrite(cat_sb.buf, 1, cat_sb.len, wf);
				fclose(wf);
			}
			free(cat_sb.buf);
		}
	}

	/* Upgrade matching runtime catalog entry in-place if present so active [MAP] palette selection stays aligned */
	{
		int ci, upgraded_in_place = 0;
		for (ci = 0; ci < g_forge.catalog_count; ci++)
		{
			if (g_forge.catalog[ci].group_tag == root_group &&
			    _stricmp(g_forge.catalog[ci].tag_path, root_tag_path) == 0)
			{
				g_forge.catalog[ci].source_cache_id = 2;
				snprintf(g_forge.catalog[ci].display_name, sizeof(g_forge.catalog[ci].display_name),
					"[C] %s", clean_custom_name);
				upgraded_in_place = 1;
			}
		}
		for (ci = 0; ci < g_forge.live_instance_count; ci++)
		{
			if (g_forge.live_instances[ci].active &&
			    g_forge.live_instances[ci].group_tag == root_group &&
			    _stricmp(g_forge.live_instances[ci].tag_path, root_tag_path) == 0)
			{
				g_forge.live_instances[ci].source_cache_id = 2;
			}
		}
		if (!upgraded_in_place)
		{
			forge_runtime_reload_catalogs();
			forge_SyncScenarioTagsWithLoadedMap();
		}
	}
	forge_sync_manifest_bin();
	return 1;
}

int forge_add_custom_map_tag_to_cache(
	char const *donor_map_path,
	char const *map_folder_unused,
	uint32_t root_group,
	char const *root_tag_path,
	char const *custom_display_name)
{
	(void)map_folder_unused;
	return forge_extract_tag_from_donor_map_to_ocetags(
		donor_map_path,
		root_group,
		root_tag_path,
		custom_display_name);
}

int forge_cache_active_map_tag(uint32_t group_tag, char const *tag_path)
{
	char const *cur_map = forge_get_active_map_name();
	char const *slash;
	char const *leaf_tag;
	char map_leaf[128];
	char candidate[512];
	char resolved_map[512];
	int i;
	static char const *const s_fmt_candidates[] = {
		"OpenCE Forge\\forge_maps\\%s.map",
		"OpenCE Forge\\forge_maps\\%s\\%s.map",
		"Faber_data\\forge_maps\\%s\\%s.map",
		"forge_maps\\%s.map",
		"forge_maps\\%s\\%s.map",
		"custom_maps\\%s.map",
		"maps\\%s.map"
	};

	if (!cur_map || !cur_map[0] || !tag_path || !tag_path[0])
		return 0;

	slash = strrchr(cur_map, '\\');
	if (!slash)
		slash = strrchr(cur_map, '/');
	snprintf(map_leaf, sizeof(map_leaf), "%s", slash ? (slash + 1) : cur_map);

	resolved_map[0] = '\0';
	for (i = 0; i < (int)(sizeof(s_fmt_candidates) / sizeof(s_fmt_candidates[0])); i++)
	{
		FILE *tf;
		if (strstr(s_fmt_candidates[i], "%s\\%s") != NULL)
			snprintf(candidate, sizeof(candidate), s_fmt_candidates[i], map_leaf, map_leaf);
		else
			snprintf(candidate, sizeof(candidate), s_fmt_candidates[i], map_leaf);
		tf = fopen(candidate, "rb");
		if (tf)
		{
			fclose(tf);
			snprintf(resolved_map, sizeof(resolved_map), "%s", candidate);
			break;
		}
	}
	if (!resolved_map[0])
		return 0;

	leaf_tag = strrchr(tag_path, '\\');
	if (!leaf_tag)
		leaf_tag = strrchr(tag_path, '/');
	leaf_tag = leaf_tag ? (leaf_tag + 1) : tag_path;

	return forge_add_custom_map_tag_to_cache(
		resolved_map,
		"custom_maps",
		group_tag,
		tag_path,
		leaf_tag);
}

static void forge_fabb_rebase_schema(
	struct tag_schema_definition const *def,
	uint8_t *slice,
	size_t slice_len,
	uint32_t root_off,
	uint32_t elem_rel,
	int32_t elem_count,
	int32_t bundle_tag_count,
	int depth)
{
	int32_t e;
	uint32_t elem_size;
	struct tag_schema_field const *field;

	if (!def || !def->fields || elem_count <= 0 || def->size <= 0 || depth > 24)
		return;
	elem_size = (uint32_t)def->size;
	if ((uint64_t)elem_rel + (uint64_t)elem_count * elem_size > (uint64_t)slice_len)
		return;

	for (e = 0; e < elem_count; e++)
	{
		uint32_t base_rel = elem_rel + (uint32_t)e * elem_size;
		for (field = def->fields; field->type != _tag_schema_terminator; field++)
		{
			int16_t k;
			int16_t fcount = (field->count > 0) ? field->count : 1;
			uint32_t fstride = (field->size > 0) ? (uint32_t)field->size : 0;
			for (k = 0; k < fcount; k++)
			{
				uint32_t f_rel = base_rel + (uint32_t)field->offset + (uint32_t)k * fstride;
				uint8_t *f_ptr = slice + f_rel;
				if (field->type == _tag_schema_struct)
				{
					struct tag_schema_definition const *sub_def =
						(struct tag_schema_definition const *)field->definition;
					if (sub_def && f_rel + (uint32_t)sub_def->size <= base_rel + elem_size)
					{
						forge_fabb_rebase_schema(
							sub_def, slice, slice_len, root_off, f_rel, 1, bundle_tag_count, depth + 1);
					}
				}
				else if (field->type == _tag_schema_block)
				{
					struct tag_schema_definition const *child_def =
						(struct tag_schema_definition const *)field->definition;
					if (child_def && child_def->size > 0 && f_rel + 12u <= base_rel + elem_size)
					{
						int32_t cnt = *(int32_t const *)(f_ptr + 0);
						uint32_t addr = *(uint32_t const *)(f_ptr + 4);
						uint32_t child_sz = (uint32_t)child_def->size;
						if (cnt > 0 && cnt <= 65536 && addr >= root_off &&
							(uint64_t)(addr - root_off) + (uint64_t)cnt * child_sz <= (uint64_t)slice_len)
						{
							uint32_t c_rel = addr - root_off;
							*(uint32_t *)(f_ptr + 4) = c_rel;
							*(uint32_t *)(f_ptr + 8) = 0;
							forge_fabb_rebase_schema(
								child_def, slice, slice_len, root_off, c_rel, cnt, bundle_tag_count, depth + 1);
						}
						else
						{
							memset(f_ptr, 0, 12);
						}
					}
				}
				else if (field->type == _tag_schema_data)
				{
					if (f_rel + 20u <= base_rel + elem_size)
					{
						int32_t sz = *(int32_t const *)(f_ptr + 0);
						uint32_t addr = *(uint32_t const *)(f_ptr + 12);
						if (sz > 0 && sz <= 16 * 1024 * 1024 && addr >= root_off &&
							(uint64_t)(addr - root_off) + (uint64_t)sz <= (uint64_t)slice_len)
						{
							*(uint32_t *)(f_ptr + 4) = 0;
							*(uint32_t *)(f_ptr + 8) = 0;
							*(uint32_t *)(f_ptr + 12) = addr - root_off;
							*(uint32_t *)(f_ptr + 16) = 0;
						}
						else
						{
							memset(f_ptr, 0, 20);
						}
					}
				}
				else if (field->type == _tag_schema_reference)
				{
					if (f_rel + 16u <= base_rel + elem_size)
					{
						int32_t ref_idx = *(int32_t const *)(f_ptr + 12);
						if (ref_idx >= 0 && ref_idx < bundle_tag_count)
							*(int32_t *)(f_ptr + 12) = ref_idx + 1;
						else
							*(int32_t *)(f_ptr + 12) = -1;
					}
				}
				else if (field->type == _tag_schema_tag_index)
				{
					if (f_rel + 4u <= base_rel + elem_size)
					{
						int32_t ref_idx = *(int32_t const *)f_ptr;
						if (ref_idx >= 0 && ref_idx < bundle_tag_count)
							*(int32_t *)f_ptr = ref_idx + 1;
						else
							*(int32_t *)f_ptr = -1;
					}
				}
			}
		}
	}
}

static void forge_ensure_vault_bundle_ocetags_cached(
	long profile,
	long tag_idx,
	uint32_t root_group,
	char const *root_tag_path,
	char const *custom_name)
{
	char safe_stem[128];
	char check_path[512];
	FILE *fp;
	unsigned long bundle_sz = 0;
	long handle;
	uint8_t *bundle = NULL;
	uint32_t sig, ver, tag_count, tags_off, names_off, names_sz, data_off, data_sz, fdata_off, fdata_sz;
	struct forge_str_builder sb;
	char clean_name[64];
	int ti;

	if (!root_tag_path || !root_tag_path[0])
		return;
	forge_make_safe_ocetags_stem(root_group, root_tag_path, safe_stem, sizeof(safe_stem));
	snprintf(check_path, sizeof(check_path), "Faber_data/custom_tags/%s.ocetags", safe_stem);
	fp = fopen(check_path, "rb");
	if (fp)
	{
		size_t ulen = 0;
		int was_c = 0;
		uint8_t *ubuf;
		fclose(fp);
		ubuf = forge_read_and_decompress_ocetags_file(check_path, &ulen, &was_c);
		if (ubuf && ulen > 32 && strstr((char const *)ubuf, "\"vaultFormat\": 2") != NULL)
		{
			free(ubuf);
			return;
		}
		free(ubuf);
	}
	snprintf(check_path, sizeof(check_path), "OpenCE Forge/custom_tags/%s.ocetags", safe_stem);
	fp = fopen(check_path, "rb");
	if (fp)
	{
		size_t ulen = 0;
		int was_c = 0;
		uint8_t *ubuf;
		fclose(fp);
		ubuf = forge_read_and_decompress_ocetags_file(check_path, &ulen, &was_c);
		if (ubuf && ulen > 32 && strstr((char const *)ubuf, "\"vaultFormat\": 2") != NULL)
		{
			free(ubuf);
			return;
		}
		free(ubuf);
	}

	handle = port_tag_profiles_bundle_open(profile, tag_idx, &bundle_sz);
	if (handle == NONE || bundle_sz < 0x40u || bundle_sz > 128u * 1024u * 1024u)
	{
		if (handle != NONE)
			port_tag_profiles_bundle_close(handle);
		return;
	}
	bundle = (uint8_t *)malloc(bundle_sz);
	if (!bundle || !port_tag_profiles_bundle_read_at(handle, 0, bundle, bundle_sz))
	{
		free(bundle);
		port_tag_profiles_bundle_close(handle);
		return;
	}
	port_tag_profiles_bundle_close(handle);

	sig = *(uint32_t const *)(bundle + 0x00u);
	ver = *(uint32_t const *)(bundle + 0x04u);
	tag_count = *(uint32_t const *)(bundle + 0x0Cu);
	tags_off = *(uint32_t const *)(bundle + 0x10u);
	names_off = *(uint32_t const *)(bundle + 0x14u);
	names_sz = *(uint32_t const *)(bundle + 0x18u);
	data_off = *(uint32_t const *)(bundle + 0x1Cu);
	data_sz = *(uint32_t const *)(bundle + 0x20u);
	fdata_off = *(uint32_t const *)(bundle + 0x24u);
	fdata_sz = *(uint32_t const *)(bundle + 0x28u);

	if (sig != 0x42424146u /* 'FABB' */ || ver != 2u || tag_count == 0 || tag_count > 4096u ||
		tags_off + tag_count * 32u > bundle_sz ||
		names_off + names_sz > bundle_sz ||
		data_off + data_sz > bundle_sz ||
		fdata_off + fdata_sz > bundle_sz)
	{
		free(bundle);
		return;
	}

	clean_name[0] = '\0';
	if (custom_name && custom_name[0])
		snprintf(clean_name, sizeof(clean_name), "%s", custom_name);
	if (!clean_name[0])
	{
		char const *leaf = root_tag_path;
		char const *p;
		for (p = root_tag_path; *p; p++)
		{
			if (*p == '\\' || *p == '/')
				leaf = p + 1;
		}
		snprintf(clean_name, sizeof(clean_name), "%s", leaf);
	}

	memset(&sb, 0, sizeof(sb));
	fsb_append_str(&sb, "{\n  \"formatVersion\": 5,\n  \"vaultFormat\": 2,\n  \"itemName\": ");
	fsb_append_json_escaped(&sb, clean_name);
	fsb_append_str(&sb, ",\n  \"tags\": [\n");

	for (ti = 0; ti < (int)tag_count; ti++)
	{
		uint8_t const *t_ent = bundle + tags_off + (uint32_t)ti * 32u;
		uint32_t grp = *(uint32_t const *)(t_ent + 0x00u);
		uint32_t n_off = *(uint32_t const *)(t_ent + 0x0Cu);
		uint32_t r_off = *(uint32_t const *)(t_ent + 0x10u);
		uint32_t next_r_off = (ti + 1 < (int)tag_count)
			? *(uint32_t const *)(bundle + tags_off + (uint32_t)(ti + 1) * 32u + 0x10u)
			: data_sz;
		uint32_t out_grp = (grp == 'mode') ? 'mod2' : grp;
		struct tag_schema_group const *schema_grp = forge_runtime_schema_group_get(out_grp);
		char const *tname = (n_off < names_sz) ? (char const *)(bundle + names_off + n_off) : "";
		uint8_t *slice;
		size_t slice_len;
		char *b64_data;
		char t_gstr[5];
		char num_buf[64];

		if (r_off >= next_r_off || next_r_off > data_sz || !schema_grp || !schema_grp->definition)
			continue;
		slice_len = (size_t)(next_r_off - r_off);
		slice = (uint8_t *)malloc(slice_len);
		if (!slice)
			continue;
		memcpy(slice, bundle + data_off + r_off, slice_len);
		forge_fabb_rebase_schema(
			schema_grp->definition, slice, slice_len, r_off, 0, 1, (int32_t)tag_count, 0);

		t_gstr[0] = (char)((out_grp >> 24) & 0xFF);
		t_gstr[1] = (char)((out_grp >> 16) & 0xFF);
		t_gstr[2] = (char)((out_grp >> 8) & 0xFF);
		t_gstr[3] = (char)(out_grp & 0xFF);
		t_gstr[4] = '\0';

		if (ti > 0)
			fsb_append_str(&sb, ",\n");
		fsb_append_str(&sb, "    {\n      \"group\": ");
		fsb_append_json_escaped(&sb, t_gstr);
		fsb_append_str(&sb, ",\n      \"path\": ");
		fsb_append_json_escaped(&sb, tname);
		snprintf(num_buf, sizeof(num_buf), ",\n      \"originalIndex\": %u,\n      \"data\": \"", (unsigned int)(ti + 1));
		fsb_append_str(&sb, num_buf);

		b64_data = forge_base64_encode_alloc(slice, slice_len, NULL);
		if (b64_data)
		{
			fsb_append_str(&sb, b64_data);
			free(b64_data);
		}
		fsb_append_str(&sb, "\"");

		if (out_grp == 'mod2' && slice_len >= 0xE8u)
		{
			int32_t geom_cnt = *(int32_t const *)(slice + 0xD0u);
			uint32_t geom_rel = *(uint32_t const *)(slice + 0xD4u);
			if (geom_cnt > 0 && geom_cnt <= 256 && geom_rel >= 0xE8u &&
				(uint64_t)geom_rel + (uint64_t)geom_cnt * 48u <= (uint64_t)slice_len)
			{
				int g_idx, first_gp = 1;
				fsb_append_str(&sb, ",\n      \"gbxmodelGeometryParts\": [\n");
				for (g_idx = 0; g_idx < geom_cnt; g_idx++)
				{
					uint32_t g_rel = geom_rel + (uint32_t)g_idx * 48u;
					int32_t part_cnt = *(int32_t const *)(slice + g_rel + 36u);
					uint32_t parts_rel = *(uint32_t const *)(slice + g_rel + 40u);
					int p_idx;
					if (part_cnt <= 0 || part_cnt > 512 || parts_rel < 0xE8u ||
						(uint64_t)parts_rel + (uint64_t)part_cnt * 132u > (uint64_t)slice_len)
						continue;
					for (p_idx = 0; p_idx < part_cnt; p_idx++)
					{
						uint8_t const *sp = slice + parts_rel + (uint32_t)p_idx * 132u;
						int32_t tri_cnt = *(int32_t const *)(sp + 0x48u);
						uint32_t idx_off = *(uint32_t const *)(sp + 0x4Cu);
						int32_t vtx_cnt = *(int32_t const *)(sp + 0x58u);
						uint32_t vtx_off = *(uint32_t const *)(sp + 0x64u);
						size_t v_bytes = (size_t)vtx_cnt * 68u;
						size_t i_bytes = (size_t)(tri_cnt + 2) * 2u;
						if (vtx_cnt > 0 && vtx_cnt <= 65536 && tri_cnt > 0 && tri_cnt <= 65536 &&
							(uint64_t)vtx_off + v_bytes <= fdata_sz &&
							(uint64_t)idx_off + i_bytes <= fdata_sz)
						{
							char *vb64 = forge_base64_encode_alloc(bundle + fdata_off + vtx_off, v_bytes, NULL);
							char *ib64 = forge_base64_encode_alloc(bundle + fdata_off + idx_off, i_bytes, NULL);
							if (vb64 && ib64)
							{
								if (!first_gp)
									fsb_append_str(&sb, ",\n");
								first_gp = 0;
								snprintf(num_buf, sizeof(num_buf),
									"        {\"geometryIndex\": %d, \"partIndex\": %d, \"vertexData\": \"",
									g_idx, p_idx);
								fsb_append_str(&sb, num_buf);
								fsb_append_str(&sb, vb64);
								fsb_append_str(&sb, "\", \"triangleData\": \"");
								fsb_append_str(&sb, ib64);
								fsb_append_str(&sb, "\"}");
							}
							free(vb64);
							free(ib64);
						}
					}
				}
				fsb_append_str(&sb, "\n      ]");
			}
		}
		else if (out_grp == 'bitm' && slice_len >= 0x6Cu)
		{
			int32_t b_cnt = *(int32_t const *)(slice + 0x60u);
			uint32_t b_rel = *(uint32_t const *)(slice + 0x64u);
			if (b_cnt > 0 && b_cnt <= 64 && b_rel >= 0x6Cu && b_rel + (uint32_t)b_cnt * 48u <= slice_len)
			{
				struct forge_str_builder patches_sb;
				int bi, asset_idx = 0;
				memset(&patches_sb, 0, sizeof(patches_sb));
				fsb_append_str(&sb, ",\n      \"embeddedRawDataAssets\": [\n");
				for (bi = 0; bi < b_cnt; bi++)
				{
					uint32_t bmp_rel = b_rel + (uint32_t)bi * 48u;
					uint32_t pix_off = *(uint32_t const *)(slice + bmp_rel + 0x18u);
					int32_t pix_sz = *(int32_t const *)(slice + bmp_rel + 0x1Cu);
					if (pix_sz > 0 && (uint64_t)pix_off + (uint64_t)pix_sz <= fdata_sz)
					{
						char *pb64 = forge_base64_encode_alloc(bundle + fdata_off + pix_off, (size_t)pix_sz, NULL);
						if (pb64)
						{
							char p_entry[128];
							if (asset_idx > 0)
							{
								fsb_append_str(&sb, ",\n");
								fsb_append_str(&patches_sb, ",\n");
							}
							fsb_append_str(&sb, "        {\"data\": \"");
							fsb_append_str(&sb, pb64);
							fsb_append_str(&sb, "\"}");
							snprintf(p_entry, sizeof(p_entry),
								"        {\"assetIndex\": %d, \"patchOffset\": %u}",
								asset_idx, (unsigned int)(bmp_rel + 24u));
							fsb_append_str(&patches_sb, p_entry);
							asset_idx++;
							free(pb64);
						}
					}
				}
				fsb_append_str(&sb, "\n      ],\n      \"embeddedRawDataPatches\": [\n");
				if (patches_sb.buf)
					fsb_append_str(&sb, patches_sb.buf);
				fsb_append_str(&sb, "\n      ]");
				free(patches_sb.buf);
			}
		}

		fsb_append_str(&sb, "\n    }");
		free(slice);
	}

	fsb_append_str(&sb, "\n  ]\n}\n");
	free(bundle);

	if (sb.buf && sb.len >= 32u)
	{
#ifdef _WIN32
		CreateDirectoryA("OpenCE Forge", NULL);
		CreateDirectoryA("OpenCE Forge/custom_tags", NULL);
		CreateDirectoryA("OpenCE Forge/cached_tags", NULL);
		CreateDirectoryA("Faber_data", NULL);
		CreateDirectoryA("Faber_data/custom_tags", NULL);
		CreateDirectoryA("Faber_data/cached_tags", NULL);
#endif
		snprintf(check_path, sizeof(check_path), "OpenCE Forge/custom_tags/%s.ocetags", safe_stem);
		forge_compress_ocetags_buffer_to_file(check_path, (uint8_t const *)sb.buf, sb.len);
		snprintf(check_path, sizeof(check_path), "Faber_data/custom_tags/%s.ocetags", safe_stem);
		forge_compress_ocetags_buffer_to_file(check_path, (uint8_t const *)sb.buf, sb.len);
	}
	free(sb.buf);
}

static void forge_remove_tag_from_catalog_file(char const *catalog_path, uint32_t group_tag, char const *tag_path)
{
	FILE *rf = fopen(catalog_path, "rb");
	struct forge_str_builder sb;
	char line[512];
	char gstr[5];
	FILE *wf;

	if (!rf)
		return;
	gstr[0] = (char)((group_tag >> 24) & 0xFF);
	gstr[1] = (char)((group_tag >> 16) & 0xFF);
	gstr[2] = (char)((group_tag >> 8) & 0xFF);
	gstr[3] = (char)(group_tag & 0xFF);
	gstr[4] = '\0';

	memset(&sb, 0, sizeof(sb));
	while (fgets(line, sizeof(line), rf))
	{
		char grp_s[16], tpath[256];
		int c_id = 0, s_id = 0;
		tpath[0] = '\0';
		if (sscanf(line, "%4s|%d|%d|%255[^\r\n|]", grp_s, &c_id, &s_id, tpath) == 4)
		{
			if (_stricmp(grp_s, gstr) == 0 && _stricmp(tpath, tag_path) == 0)
				continue;
		}
		fsb_append_str(&sb, line);
	}
	fclose(rf);

	wf = fopen(catalog_path, "wb");
	if (wf)
	{
		if (sb.buf && sb.len > 0)
			fwrite(sb.buf, 1, sb.len, wf);
		fclose(wf);
	}
	free(sb.buf);
}

int forge_delete_installed_tag_entry(uint32_t group_tag, char const *tag_path, int is_custom)
{
	char safe_stem[128];
	char fpath[512];
	static char const *const s_dirs[] = {
		"OpenCE Forge/custom_tags",
		"OpenCE Forge/cached_tags",
		"Faber_data/custom_tags",
		"Faber_data/cached_tags",
		"custom_tags"
	};
	int d;

	if (!tag_path || !tag_path[0])
		return 0;

	forge_make_safe_ocetags_stem(group_tag, tag_path, safe_stem, sizeof(safe_stem));
	for (d = 0; d < 5; d++)
	{
		if (!is_custom && (d == 0 || d == 2 || d == 4))
			continue;
		snprintf(fpath, sizeof(fpath), "%s/%s.ocetags", s_dirs[d], safe_stem);
		remove(fpath);
		snprintf(fpath, sizeof(fpath), "%s/%s.txt", s_dirs[d], safe_stem);
		remove(fpath);
	}

	/* Also scan custom_tags/cached_tags for any differently named .ocetags whose root matches (group_tag, tag_path) */
#ifdef _WIN32
	for (d = 0; d < 5; d++)
	{
		char pattern[512];
		WIN32_FIND_DATAA fd;
		HANDLE h;
		if (!is_custom && (d == 0 || d == 2 || d == 4))
			continue;
		snprintf(pattern, sizeof(pattern), "%s/*.ocetags", s_dirs[d]);
		h = FindFirstFileA(pattern, &fd);
		if (h != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					char full[512];
					size_t ulen = 0;
					int was_comp = 0;
					uint8_t *ubuf;
					snprintf(full, sizeof(full), "%s/%s", s_dirs[d], fd.cFileName);
					ubuf = forge_read_and_decompress_ocetags_file(full, &ulen, &was_comp);
					if (ubuf && ulen > 32)
					{
						size_t t_pos = 0;
						if (ocetags_json_find_key_range((char const *)ubuf, 0, ulen, "tags", &t_pos))
						{
							size_t p = t_pos + 1;
							while (p < ulen && ubuf[p] != '{') p++;
							if (p < ulen)
							{
								char gs[64], ps[256];
								gs[0] = 0; ps[0] = 0;
								ocetags_json_get_str((char const *)ubuf, p, ulen, "group", gs, sizeof(gs));
								ocetags_json_get_str((char const *)ubuf, p, ulen, "path", ps, sizeof(ps));
								if (ocetags_group_str_to_fourcc(gs) == group_tag && _stricmp(ps, tag_path) == 0)
								{
									char *dot;
									free(ubuf);
									ubuf = NULL;
									remove(full);
									dot = strrchr(full, '.');
									if (dot)
									{
										strcpy(dot, ".txt");
										remove(full);
									}
								}
							}
						}
					}
					free(ubuf);
				}
			} while (FindNextFileA(h, &fd));
			CloseHandle(h);
		}
	}
#endif

	if (is_custom)
		forge_remove_tag_from_catalog_file("Faber_data/cached_tags/custom_catalog.txt", group_tag, tag_path);
	else
		forge_remove_tag_from_catalog_file("Faber_data/cached_tags/xbox_catalog.txt", group_tag, tag_path);

	forge_runtime_reload_catalogs();
	forge_sync_manifest_bin();
	return 1;
}

int forge_reset_stock_tags_only(void)
{
	/* Clear stock cached tags and manifest.bin, then re-sync only custom tags from custom_tags/ */
	forge_delete_files_matching("OpenCE Forge/cached_tags", "*.ocetags");
	forge_delete_files_matching("OpenCE Forge/cached_tags", "*.txt");
	remove("OpenCE Forge/cached_tags/manifest.bin");
	remove("Faber_data/cached_tags/manifest.bin");
	remove("Faber_data/cached_tags/xbox_catalog.txt");
	remove("Faber_data/cached_tags/catalog.txt");
	remove("Faber_data/cached_tags/custom_catalog.txt");

	/* Re-sync custom tags from custom_tags/ into cached_tags/ with OCEZ compression */
#ifdef _WIN32
	{
		static char const *const s_cust_dirs[] = {
			"OpenCE Forge/custom_tags",
			"Faber_data/custom_tags",
			"custom_tags"
		};
		int d;
		for (d = 0; d < 3; d++)
		{
			char pattern[512];
			WIN32_FIND_DATAA fd;
			HANDLE h;
			snprintf(pattern, sizeof(pattern), "%s/*.ocetags", s_cust_dirs[d]);
			h = FindFirstFileA(pattern, &fd);
			if (h != INVALID_HANDLE_VALUE)
			{
				do
				{
					if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					{
						char src_path[512], dst_path[512];
						size_t ulen = 0;
						int was_comp = 0;
						uint8_t *ubuf;
						snprintf(src_path, sizeof(src_path), "%s/%s", s_cust_dirs[d], fd.cFileName);
						ubuf = forge_read_and_decompress_ocetags_file(src_path, &ulen, &was_comp);
						if (ubuf && ulen > 0)
						{
							if (!was_comp)
								forge_compress_ocetags_buffer_to_file(src_path, ubuf, ulen);
							snprintf(dst_path, sizeof(dst_path), "OpenCE Forge/cached_tags/%s", fd.cFileName);
							forge_compress_ocetags_buffer_to_file(dst_path, ubuf, ulen);
						}
						free(ubuf);
					}
				} while (FindNextFileA(h, &fd));
				CloseHandle(h);
			}
		}
	}
#endif

	forge_scan_and_rip_ocetags_folders(0);
	forge_runtime_reload_catalogs();
	forge_sync_manifest_bin();
	return 1;
}

int forge_reset_custom_tags_only(void)
{
	forge_delete_files_matching("OpenCE Forge/custom_tags", "*.ocetags");
	forge_delete_files_matching("OpenCE Forge/custom_tags", "*.txt");
	forge_delete_files_matching("OpenCE Forge/cached_tags", "*.ocetags");
	forge_delete_files_matching("OpenCE Forge/cached_tags", "*.txt");
	forge_delete_files_matching("Faber_data/custom_tags", "*.ocetags");
	forge_delete_files_matching("Faber_data/custom_tags", "*.txt");
	forge_delete_files_matching("Faber_data/cached_tags", "*.ocetags");
	forge_delete_files_matching("Faber_data/cached_tags", "*.txt");
	forge_delete_files_matching("custom_tags", "*.ocetags");
	forge_delete_files_matching("custom_tags", "*.txt");
	forge_delete_files_matching("Faber_data/ocetags", "*.ocetags");
	forge_delete_files_matching("Faber_data/cached_tags", "c_*.*");
	forge_delete_files_matching("Faber_data/cached_tags/markers", "c_*.*");
	remove("Faber_data/cached_tags/custom_catalog.txt");

	forge_runtime_reload_catalogs();
	forge_sync_manifest_bin();
	return 1;
}

/* =========================================================================
 * LIVE 140x114 32-BIT ROTATING 3D TAG PREVIEWER
 * ========================================================================= */

#define FORGE_PREVIEW_W 256
#define FORGE_PREVIEW_H 256
#define FORGE_PREVIEW_MAX_VERTS 16384
#define FORGE_PREVIEW_MAX_TRIS  24576
#define FORGE_PREVIEW_MAX_TEXTURES 32

struct forge_preview_tri
{
	uint16_t v0, v1, v2;
	int16_t tex_idx;
};

struct forge_preview_texture
{
	int32_t bitm_key;
	int16_t width;
	int16_t height;
	int alpha_test;
	uint32_t *pixels;
};

static struct
{
	int active;
	char key[512];
	uint32_t current_group_tag;
	char current_tag_path[FORGE_TAG_PATH_MAX];
	struct bitmap_data *bitmap;
	float zbuf[FORGE_PREVIEW_W * FORGE_PREVIEW_H];
	float (*verts)[3];
	float (*norms)[3];
	float (*uvs)[2];
	float (*proj)[3];
	float (*shade)[2];
	struct forge_preview_tri *tris;
	struct forge_preview_texture textures[FORGE_PREVIEW_MAX_TEXTURES];
	int tex_count;
	int use_textures;
	int last_rendered_use_textures;
	int vert_count;
	int tri_count;
	float bbox_min[3];
	float bbox_max[3];
	float center[3];
	float radius;
	uint32_t tag_size_bytes;
	unsigned long last_draw_ms;
	/* Interactive 3D Snap Point Editor Overlay State */
	int snap_editor_active;
	int snap_active_pt_idx;
	float snap_cam_yaw_deg;
	float snap_cam_pitch_deg;
	float snap_cam_zoom;
} s_forge_preview;

static struct
{
	int loaded;
	int count;
	struct forge_tag_snap_config configs[FORGE_MAX_SNAP_TAG_CONFIGS];
} s_snap_db;

void forge_snap_points_fill_normal_from_dir(struct forge_snap_point *pt, int normal_dir)
{
	if (!pt)
		return;
	if (normal_dir < 0 || normal_dir >= NUMBER_OF_FORGE_SNAP_DIRS)
		normal_dir = _forge_snap_dir_front;
	pt->normal_dir = (uint8_t)normal_dir;
	pt->nx = 0.0f;
	pt->ny = 0.0f;
	pt->nz = 0.0f;
	switch (normal_dir)
	{
	case _forge_snap_dir_front: pt->nx =  1.0f; break;
	case _forge_snap_dir_right: pt->ny = -1.0f; break;
	case _forge_snap_dir_back:  pt->nx = -1.0f; break;
	case _forge_snap_dir_left:  pt->ny =  1.0f; break;
	case _forge_snap_dir_up:    pt->nz =  1.0f; break;
	case _forge_snap_dir_down:  pt->nz = -1.0f; break;
	default:                    pt->nx =  1.0f; break;
	}
}

void forge_snap_points_load_all(void)
{
	static char const *const s_paths[] = {
		"Faber_data/cached_tags/snap_points.txt",
		"OpenCE Forge/cached_tags/snap_points.txt",
		"Faber_data/custom_tags/snap_points.txt"
	};
	FILE *fp = NULL;
	char line[512];
	int p;

	memset(&s_snap_db, 0, sizeof(s_snap_db));
	s_snap_db.loaded = 1;

	for (p = 0; p < 3; p++)
	{
		fp = fopen(s_paths[p], "r");
		if (fp)
			break;
	}
	if (!fp)
		return;

	while (fgets(line, sizeof(line), fp))
	{
		char *nl;
		unsigned int grp_u = 0;
		char tpath[256];
		float x = 0.0f, y = 0.0f, z = 0.0f;
		float nx = 1.0f, ny = 0.0f, nz = 0.0f;
		int ndir = 0, preset = 0;
		struct forge_tag_snap_config *cfg;
		struct forge_snap_point *pt;

		nl = strchr(line, '\r'); if (nl) *nl = '\0';
		nl = strchr(line, '\n'); if (nl) *nl = '\0';
		if (line[0] == '\0' || line[0] == '#')
			continue;

		tpath[0] = '\0';
		if (sscanf(line, "%x|%255[^|]|%f|%f|%f|%f|%f|%f|%d|%d",
				&grp_u, tpath, &x, &y, &z, &nx, &ny, &nz, &ndir, &preset) < 8)
		{
			continue;
		}
		if (!tpath[0])
			continue;

		cfg = forge_snap_points_get_for_tag((uint32_t)grp_u, tpath, 1);
		if (!cfg || cfg->point_count >= FORGE_MAX_SNAP_POINTS_PER_TAG)
			continue;

		pt = &cfg->points[cfg->point_count++];
		memset(pt, 0, sizeof(*pt));
		pt->x = x;
		pt->y = y;
		pt->z = z;
		pt->nx = nx;
		pt->ny = ny;
		pt->nz = nz;
		pt->normal_dir = (uint8_t)((ndir >= 0 && ndir < NUMBER_OF_FORGE_SNAP_DIRS) ? ndir : 0);
		pt->preset_id = (uint8_t)((preset >= 0 && preset < NUMBER_OF_FORGE_SNAP_PRESETS) ? preset : 0);
	}
	fclose(fp);
}

int forge_snap_points_save_all(void)
{
	FILE *fp;
	int i, p;

#ifdef _WIN32
	CreateDirectoryA("Faber_data", NULL);
	CreateDirectoryA("Faber_data/cached_tags", NULL);
#endif
	fp = fopen("Faber_data/cached_tags/snap_points.txt", "wb");
	if (!fp)
		return 0;

	fprintf(fp, "# OpenCE : Faber - 3D Tag Snap Points Configuration\n");
	fprintf(fp, "# Format: group_hex|tag_path|x|y|z|nx|ny|nz|normal_dir|preset_id\n");
	for (i = 0; i < s_snap_db.count; i++)
	{
		struct forge_tag_snap_config const *cfg = &s_snap_db.configs[i];
		if (!cfg->tag_path[0] || cfg->point_count <= 0)
			continue;
		for (p = 0; p < cfg->point_count && p < FORGE_MAX_SNAP_POINTS_PER_TAG; p++)
		{
			struct forge_snap_point const *pt = &cfg->points[p];
			fprintf(fp, "%08X|%s|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f|%d|%d\n",
				(unsigned int)cfg->group_tag,
				cfg->tag_path,
				(double)pt->x, (double)pt->y, (double)pt->z,
				(double)pt->nx, (double)pt->ny, (double)pt->nz,
				(int)pt->normal_dir,
				(int)pt->preset_id);
		}
	}
	fclose(fp);
	return 1;
}

struct forge_tag_snap_config *forge_snap_points_get_for_tag(uint32_t group_tag, char const *tag_path, int create_if_missing)
{
	int i;
	if (!tag_path || !tag_path[0])
		return NULL;
	if (!s_snap_db.loaded)
		forge_snap_points_load_all();

	for (i = 0; i < s_snap_db.count; i++)
	{
		if (s_snap_db.configs[i].group_tag == group_tag &&
			_stricmp(s_snap_db.configs[i].tag_path, tag_path) == 0)
		{
			return &s_snap_db.configs[i];
		}
	}
	/* Fallback match by tag_path if group_tag differs between scen/mach/lifi */
	for (i = 0; i < s_snap_db.count; i++)
	{
		if (_stricmp(s_snap_db.configs[i].tag_path, tag_path) == 0)
			return &s_snap_db.configs[i];
	}
	if (!create_if_missing || s_snap_db.count >= FORGE_MAX_SNAP_TAG_CONFIGS)
		return NULL;

	i = s_snap_db.count++;
	memset(&s_snap_db.configs[i], 0, sizeof(s_snap_db.configs[i]));
	s_snap_db.configs[i].group_tag = group_tag;
	snprintf(s_snap_db.configs[i].tag_path, sizeof(s_snap_db.configs[i].tag_path), "%s", tag_path);
	return &s_snap_db.configs[i];
}

int forge_snap_points_count_for_tag(uint32_t group_tag, char const *tag_path)
{
	struct forge_tag_snap_config const *cfg = forge_snap_points_get_for_tag(group_tag, tag_path, 0);
	return cfg ? cfg->point_count : 0;
}

void forge_snap_points_get_preview_bounds(float out_min[3], float out_max[3])
{
	int c;
	for (c = 0; c < 3; c++)
	{
		if (out_min) out_min[c] = s_forge_preview.bbox_min[c];
		if (out_max) out_max[c] = s_forge_preview.bbox_max[c];
	}
}

void forge_snap_points_set_editor_overlay(int active, uint32_t group_tag, char const *tag_path, int active_pt_idx, float cam_yaw_deg, float cam_pitch_deg, float cam_zoom)
{
	float zm = (cam_zoom > 0.2f) ? cam_zoom : 1.0f;
	int changed = 0;

	if (s_forge_preview.use_textures != 0 ||
		s_forge_preview.snap_editor_active != active ||
		s_forge_preview.snap_active_pt_idx != active_pt_idx ||
		s_forge_preview.snap_cam_yaw_deg != cam_yaw_deg ||
		s_forge_preview.snap_cam_pitch_deg != cam_pitch_deg ||
		s_forge_preview.snap_cam_zoom != zm)
	{
		changed = 1;
	}
	/* Keep the Snap Points 3D viewer & interactive editor untextured so snap points and normals are clearly visible */
	s_forge_preview.use_textures = 0;
	s_forge_preview.snap_editor_active = active;
	s_forge_preview.snap_active_pt_idx = active_pt_idx;
	s_forge_preview.snap_cam_yaw_deg = cam_yaw_deg;
	s_forge_preview.snap_cam_pitch_deg = cam_pitch_deg;
	s_forge_preview.snap_cam_zoom = zm;
	if (tag_path && tag_path[0])
	{
		if (s_forge_preview.current_group_tag != group_tag ||
			_stricmp(s_forge_preview.current_tag_path, tag_path) != 0)
		{
			changed = 1;
		}
		s_forge_preview.current_group_tag = group_tag;
		snprintf(s_forge_preview.current_tag_path, sizeof(s_forge_preview.current_tag_path), "%s", tag_path);
	}
	if (changed)
		s_forge_preview.last_draw_ms = 0;
}

void forge_snap_points_apply_preset_to_point(struct forge_snap_point *pt, int preset_id, int normal_dir)
{
	float min_x = s_forge_preview.bbox_min[0], max_x = s_forge_preview.bbox_max[0];
	float min_y = s_forge_preview.bbox_min[1], max_y = s_forge_preview.bbox_max[1];
	float min_z = s_forge_preview.bbox_min[2], max_z = s_forge_preview.bbox_max[2];
	float cx = 0.5f * (min_x + max_x);
	float cy = 0.5f * (min_y + max_y);
	float cz = 0.5f * (min_z + max_z);
	int auto_dir = _forge_snap_dir_front;

	if (!pt)
		return;
	if (preset_id < 0 || preset_id >= NUMBER_OF_FORGE_SNAP_PRESETS)
		preset_id = _forge_snap_preset_front_center;

	pt->preset_id = (uint8_t)preset_id;
	switch (preset_id)
	{
	case _forge_snap_preset_front_center:
		pt->x = max_x; pt->y = cy; pt->z = cz;
		auto_dir = _forge_snap_dir_front;
		break;
	case _forge_snap_preset_back_center:
		pt->x = min_x; pt->y = cy; pt->z = cz;
		auto_dir = _forge_snap_dir_back;
		break;
	case _forge_snap_preset_left_center:
		pt->x = cx; pt->y = max_y; pt->z = cz;
		auto_dir = _forge_snap_dir_left;
		break;
	case _forge_snap_preset_right_center:
		pt->x = cx; pt->y = min_y; pt->z = cz;
		auto_dir = _forge_snap_dir_right;
		break;
	case _forge_snap_preset_top_center:
		pt->x = cx; pt->y = cy; pt->z = max_z;
		auto_dir = _forge_snap_dir_up;
		break;
	case _forge_snap_preset_bot_center:
		pt->x = cx; pt->y = cy; pt->z = min_z;
		auto_dir = _forge_snap_dir_down;
		break;
	case _forge_snap_preset_front_base:
		pt->x = max_x; pt->y = cy; pt->z = min_z;
		auto_dir = _forge_snap_dir_front;
		break;
	case _forge_snap_preset_back_base:
		pt->x = min_x; pt->y = cy; pt->z = min_z;
		auto_dir = _forge_snap_dir_back;
		break;
	case _forge_snap_preset_left_base:
		pt->x = cx; pt->y = max_y; pt->z = min_z;
		auto_dir = _forge_snap_dir_left;
		break;
	case _forge_snap_preset_right_base:
		pt->x = cx; pt->y = min_y; pt->z = min_z;
		auto_dir = _forge_snap_dir_right;
		break;
	default:
		auto_dir = (int)pt->normal_dir;
		break;
	}
	forge_snap_points_fill_normal_from_dir(pt, (normal_dir >= 0) ? normal_dir : auto_dir);
}

void forge_snap_points_auto_generate(uint32_t group_tag, char const *tag_path, int mode)
{
	struct forge_tag_snap_config *cfg = forge_snap_points_get_for_tag(group_tag, tag_path, 1);
	if (!cfg)
		return;

	cfg->point_count = 0;
	if (mode == 0) /* 4-SIDE BOX (Front, Back, Left, Right face centers) */
	{
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_front_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_back_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_left_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_right_center, -1);
	}
	else if (mode == 1) /* 2-END BRIDGE / WALL (Opposite ends along longest horizontal axis) */
	{
		float dx = s_forge_preview.bbox_max[0] - s_forge_preview.bbox_min[0];
		float dy = s_forge_preview.bbox_max[1] - s_forge_preview.bbox_min[1];
		if (dy > 1.25f * dx)
		{
			forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_left_center, -1);
			forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_right_center, -1);
		}
		else
		{
			forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_front_center, -1);
			forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_back_center, -1);
		}
	}
	else if (mode == 2) /* 6-FACE CUBE (All 6 bounding box faces) */
	{
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_front_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_back_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_left_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_right_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_top_center, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_bot_center, -1);
	}
	else if (mode == 3) /* 4 BASE EDGES (Floor-aligned Front, Back, Left, Right base midpoints) */
	{
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_front_base, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_back_base, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_left_base, -1);
		forge_snap_points_apply_preset_to_point(&cfg->points[cfg->point_count++], _forge_snap_preset_right_base, -1);
	}
	forge_snap_points_save_all();
	s_forge_preview.last_draw_ms = 0;
}

static void forge_preview_reset_mesh(void)
{
	int ti;
	for (ti = 0; ti < FORGE_PREVIEW_MAX_TEXTURES; ti++)
	{
		free(s_forge_preview.textures[ti].pixels);
		s_forge_preview.textures[ti].pixels = NULL;
	}
	memset(s_forge_preview.textures, 0, sizeof(s_forge_preview.textures));
	s_forge_preview.tex_count = 0;
	s_forge_preview.vert_count = 0;
	s_forge_preview.tri_count = 0;
	s_forge_preview.bbox_min[0] = -0.5f;
	s_forge_preview.bbox_min[1] = -0.5f;
	s_forge_preview.bbox_min[2] = -0.5f;
	s_forge_preview.bbox_max[0] =  0.5f;
	s_forge_preview.bbox_max[1] =  0.5f;
	s_forge_preview.bbox_max[2] =  0.5f;
	s_forge_preview.center[0] = 0.0f;
	s_forge_preview.center[1] = 0.0f;
	s_forge_preview.center[2] = 0.0f;
	s_forge_preview.radius = 1.0f;
}

static int forge_preview_ensure_buffers(void)
{
	if (!s_forge_preview.verts)
		s_forge_preview.verts = (float (*)[3])malloc(FORGE_PREVIEW_MAX_VERTS * sizeof(float[3]));
	if (!s_forge_preview.norms)
		s_forge_preview.norms = (float (*)[3])malloc(FORGE_PREVIEW_MAX_VERTS * sizeof(float[3]));
	if (!s_forge_preview.uvs)
		s_forge_preview.uvs = (float (*)[2])malloc(FORGE_PREVIEW_MAX_VERTS * sizeof(float[2]));
	if (!s_forge_preview.proj)
		s_forge_preview.proj = (float (*)[3])malloc(FORGE_PREVIEW_MAX_VERTS * sizeof(float[3]));
	if (!s_forge_preview.shade)
		s_forge_preview.shade = (float (*)[2])malloc(FORGE_PREVIEW_MAX_VERTS * sizeof(float[2]));
	if (!s_forge_preview.tris)
		s_forge_preview.tris = (struct forge_preview_tri *)malloc(FORGE_PREVIEW_MAX_TRIS * sizeof(struct forge_preview_tri));
	if (!s_forge_preview.verts || !s_forge_preview.norms || !s_forge_preview.uvs ||
		!s_forge_preview.proj || !s_forge_preview.shade || !s_forge_preview.tris)
		return 0;
	if (!s_forge_preview.bitmap)
	{
		s_forge_preview.bitmap = bitmap_2d_new(FORGE_PREVIEW_W, FORGE_PREVIEW_H, 0, 11 /* _bitmap_format_a8r8g8b8 */);
		if (s_forge_preview.bitmap && s_forge_preview.bitmap->base_address)
		{
			memset(XBOX_POINTER(uint32_t, s_forge_preview.bitmap->base_address), 0, FORGE_PREVIEW_W * FORGE_PREVIEW_H * sizeof(uint32_t));
			bitmap_rebuild(s_forge_preview.bitmap);
		}
	}
	return (s_forge_preview.bitmap && s_forge_preview.bitmap->base_address) ? 1 : 0;
}

static void forge_preview_build_fallback_octahedron(void)
{
	static float const s_oct_v[6][3] = {
		{  0.0f,  0.0f,  1.0f },
		{  0.8f,  0.0f,  0.0f },
		{  0.0f,  0.8f,  0.0f },
		{ -0.8f,  0.0f,  0.0f },
		{  0.0f, -0.8f,  0.0f },
		{  0.0f,  0.0f, -1.0f }
	};
	static uint16_t const s_oct_t[8][3] = {
		{ 0, 1, 2 }, { 0, 2, 3 }, { 0, 3, 4 }, { 0, 4, 1 },
		{ 5, 2, 1 }, { 5, 3, 2 }, { 5, 4, 3 }, { 5, 1, 4 }
	};
	int i;
	forge_preview_reset_mesh();
	for (i = 0; i < 6; i++)
	{
		s_forge_preview.verts[i][0] = s_oct_v[i][0];
		s_forge_preview.verts[i][1] = s_oct_v[i][1];
		s_forge_preview.verts[i][2] = s_oct_v[i][2];
		s_forge_preview.norms[i][0] = s_oct_v[i][0];
		s_forge_preview.norms[i][1] = s_oct_v[i][1];
		s_forge_preview.norms[i][2] = s_oct_v[i][2];
		s_forge_preview.uvs[i][0] = 0.0f;
		s_forge_preview.uvs[i][1] = 0.0f;
	}
	for (i = 0; i < 8; i++)
	{
		s_forge_preview.tris[i].v0 = s_oct_t[i][0];
		s_forge_preview.tris[i].v1 = s_oct_t[i][1];
		s_forge_preview.tris[i].v2 = s_oct_t[i][2];
		s_forge_preview.tris[i].tex_idx = -1;
	}
	s_forge_preview.vert_count = 6;
	s_forge_preview.tri_count = 8;
	s_forge_preview.radius = 1.15f;
}

static void forge_preview_normalize_mesh(void)
{
	float min_v[3], max_v[3];
	float max_r2 = 0.0f;
	int i;

	if (s_forge_preview.vert_count < 3 || s_forge_preview.tri_count < 1)
	{
		forge_preview_build_fallback_octahedron();
		return;
	}
	min_v[0] = max_v[0] = s_forge_preview.verts[0][0];
	min_v[1] = max_v[1] = s_forge_preview.verts[0][1];
	min_v[2] = max_v[2] = s_forge_preview.verts[0][2];
	for (i = 1; i < s_forge_preview.vert_count; i++)
	{
		int c;
		for (c = 0; c < 3; c++)
		{
			if (s_forge_preview.verts[i][c] < min_v[c]) min_v[c] = s_forge_preview.verts[i][c];
			if (s_forge_preview.verts[i][c] > max_v[c]) max_v[c] = s_forge_preview.verts[i][c];
		}
	}
	s_forge_preview.bbox_min[0] = min_v[0];
	s_forge_preview.bbox_min[1] = min_v[1];
	s_forge_preview.bbox_min[2] = min_v[2];
	s_forge_preview.bbox_max[0] = max_v[0];
	s_forge_preview.bbox_max[1] = max_v[1];
	s_forge_preview.bbox_max[2] = max_v[2];
	s_forge_preview.center[0] = 0.5f * (min_v[0] + max_v[0]);
	s_forge_preview.center[1] = 0.5f * (min_v[1] + max_v[1]);
	s_forge_preview.center[2] = 0.5f * (min_v[2] + max_v[2]);
	for (i = 0; i < s_forge_preview.vert_count; i++)
	{
		float dx = s_forge_preview.verts[i][0] - s_forge_preview.center[0];
		float dy = s_forge_preview.verts[i][1] - s_forge_preview.center[1];
		float dz = s_forge_preview.verts[i][2] - s_forge_preview.center[2];
		float r2 = dx * dx + dy * dy + dz * dz;
		if (r2 > max_r2) max_r2 = r2;
	}
	s_forge_preview.radius = (max_r2 > 0.0001f) ? (float)sqrt((double)max_r2) : 1.0f;
}

static uint32_t forge_xbox_swizzle_xy(int x, int y, int w, int h)
{
	uint32_t idx = 0;
	int bit = 1;
	int shift = 0;
	while (bit < w || bit < h)
	{
		if (bit < w)
		{
			if (x & bit) idx |= (1u << shift);
			shift++;
		}
		if (bit < h)
		{
			if (y & bit) idx |= (1u << shift);
			shift++;
		}
		bit <<= 1;
	}
	return idx;
}

static size_t forge_preview_bitmap_level_bytes(int w, int h, int16_t format)
{
	if (w <= 0 || h <= 0)
		return 0;
	switch (format)
	{
	case 0:  /* A8 */
	case 1:  /* Y8 */
	case 2:  /* AY8 */
	case 17: /* P8 */
		return (size_t)w * (size_t)h;
	case 3:  /* A8Y8 */
	case 6:  /* R5G6B5 */
	case 8:  /* A1R5G5B5 */
	case 9:  /* A4R4G4B4 */
		return (size_t)w * (size_t)h * 2u;
	case 10: /* X8R8G8B8 */
	case 11: /* A8R8G8B8 */
		return (size_t)w * (size_t)h * 4u;
	case 14: /* DXT1 */
		return (size_t)((w + 3) / 4) * (size_t)((h + 3) / 4) * 8u;
	case 15: /* DXT3 */
	case 16: /* DXT5 */
		return (size_t)((w + 3) / 4) * (size_t)((h + 3) / 4) * 16u;
	default:
		return 0;
	}
}

static uint32_t *forge_preview_decode_bitmap_pixels(
	uint8_t const *bmp_entry_48,
	FILE *pix_fp,
	uint8_t const *mem_pix,
	size_t mem_pix_len,
	int16_t *out_w,
	int16_t *out_h)
{
	int orig_w, orig_h, w, h, mipmaps, mip;
	int16_t type, format;
	uint16_t b_flags;
	uint32_t pix_off;
	int32_t pix_sz;
	size_t mip_byte_off = 0, level_bytes = 0, avail_bytes = 0;
	int is_swizzled;
	uint8_t *alloc_raw = NULL;
	uint8_t const *src = NULL;
	uint32_t *dst = NULL;

	if (!bmp_entry_48 || !out_w || !out_h)
		return NULL;
	orig_w = (int)*(int16_t const *)(bmp_entry_48 + 0x04u);
	orig_h = (int)*(int16_t const *)(bmp_entry_48 + 0x06u);
	type = *(int16_t const *)(bmp_entry_48 + 0x0Au);
	format = *(int16_t const *)(bmp_entry_48 + 0x0Cu);
	b_flags = *(uint16_t const *)(bmp_entry_48 + 0x0Eu);
	mipmaps = (int)*(int16_t const *)(bmp_entry_48 + 0x14u);
	pix_off = *(uint32_t const *)(bmp_entry_48 + 0x18u);
	pix_sz = *(int32_t const *)(bmp_entry_48 + 0x1Cu);

	if (type != 0 /* _bitmap_type_2d */ || orig_w <= 0 || orig_w > 4096 || orig_h <= 0 || orig_h > 4096)
		return NULL;

	avail_bytes = mem_pix ? mem_pix_len : ((pix_sz > 0 && pix_sz <= 16 * 1024 * 1024) ? (size_t)pix_sz : 0u);
	if (avail_bytes == 0)
		return NULL;

	w = orig_w;
	h = orig_h;
	is_swizzled = ((b_flags & 0x08u) != 0 && format < 14);
	if (!is_swizzled && mipmaps > 0)
	{
		for (mip = 0; mip < mipmaps && (w > 128 || h > 128); mip++)
		{
			size_t cur_sz = forge_preview_bitmap_level_bytes(w, h, format);
			int nw = (w > 1) ? (w >> 1) : 1;
			int nh = (h > 1) ? (h >> 1) : 1;
			size_t next_sz = forge_preview_bitmap_level_bytes(nw, nh, format);
			if (cur_sz == 0 || next_sz == 0 || mip_byte_off + cur_sz + next_sz > avail_bytes)
				break;
			mip_byte_off += cur_sz;
			w = nw;
			h = nh;
		}
	}

	level_bytes = forge_preview_bitmap_level_bytes(w, h, format);
	if (level_bytes == 0 || mip_byte_off + level_bytes > avail_bytes)
	{
		w = orig_w;
		h = orig_h;
		mip_byte_off = 0;
		level_bytes = forge_preview_bitmap_level_bytes(w, h, format);
		if (level_bytes == 0 || level_bytes > avail_bytes)
			return NULL;
	}

	if (mem_pix)
	{
		src = mem_pix + mip_byte_off;
	}
	else if (pix_fp)
	{
		alloc_raw = (uint8_t *)malloc(level_bytes);
		if (!alloc_raw)
			return NULL;
		if (fseek(pix_fp, (long)(pix_off + (uint32_t)mip_byte_off), SEEK_SET) != 0 ||
			fread(alloc_raw, 1, level_bytes, pix_fp) != level_bytes)
		{
			free(alloc_raw);
			return NULL;
		}
		src = alloc_raw;
	}
	else
	{
		return NULL;
	}

	dst = (uint32_t *)malloc((size_t)w * (size_t)h * sizeof(uint32_t));
	if (!dst)
	{
		free(alloc_raw);
		return NULL;
	}

	if (format == 14 || format == 15 || format == 16) /* DXT1 / DXT3 / DXT5 */
	{
		int bw = (w + 3) / 4;
		int bh = (h + 3) / 4;
		size_t bstride = (format == 14) ? 8u : 16u;
		int by, bx;
		for (by = 0; by < bh; by++)
		{
			for (bx = 0; bx < bw; bx++)
			{
				uint8_t const *blk = src + ((size_t)by * (size_t)bw + (size_t)bx) * bstride;
				uint8_t const *cblk = (format == 14) ? blk : (blk + 8u);
				uint16_t c0 = *(uint16_t const *)(cblk + 0u);
				uint16_t c1 = *(uint16_t const *)(cblk + 2u);
				uint32_t bits = *(uint32_t const *)(cblk + 4u);
				uint32_t r[4], g[4], b[4], a_lut[4];
				uint8_t a_pal[8];
				uint64_t a_bits = 0;
				int py, px;

				r[0] = ((c0 >> 11) & 31u) * 255u / 31u;
				g[0] = ((c0 >> 5)  & 63u) * 255u / 63u;
				b[0] = ( c0        & 31u) * 255u / 31u;
				r[1] = ((c1 >> 11) & 31u) * 255u / 31u;
				g[1] = ((c1 >> 5)  & 63u) * 255u / 63u;
				b[1] = ( c1        & 31u) * 255u / 31u;
				a_lut[0] = 255u; a_lut[1] = 255u; a_lut[2] = 255u; a_lut[3] = 255u;

				if (format != 14 || c0 > c1)
				{
					r[2] = (2u * r[0] + r[1] + 1u) / 3u;
					g[2] = (2u * g[0] + g[1] + 1u) / 3u;
					b[2] = (2u * b[0] + b[1] + 1u) / 3u;
					r[3] = (r[0] + 2u * r[1] + 1u) / 3u;
					g[3] = (g[0] + 2u * g[1] + 1u) / 3u;
					b[3] = (b[0] + 2u * b[1] + 1u) / 3u;
				}
				else
				{
					r[2] = (r[0] + r[1]) >> 1;
					g[2] = (g[0] + g[1]) >> 1;
					b[2] = (b[0] + b[1]) >> 1;
					r[3] = 0u; g[3] = 0u; b[3] = 0u;
					a_lut[3] = 0u;
				}

				if (format == 16)
				{
					int ai;
					a_pal[0] = blk[0];
					a_pal[1] = blk[1];
					if (a_pal[0] > a_pal[1])
					{
						for (ai = 1; ai <= 6; ai++)
							a_pal[ai + 1] = (uint8_t)(((7 - ai) * (int)a_pal[0] + ai * (int)a_pal[1] + 3) / 7);
					}
					else
					{
						for (ai = 1; ai <= 4; ai++)
							a_pal[ai + 1] = (uint8_t)(((5 - ai) * (int)a_pal[0] + ai * (int)a_pal[1] + 2) / 5);
						a_pal[6] = 0u;
						a_pal[7] = 255u;
					}
					for (ai = 0; ai < 6; ai++)
						a_bits |= ((uint64_t)blk[2 + ai]) << (8 * ai);
				}

				for (py = 0; py < 4; py++)
				{
					int dy = by * 4 + py;
					if (dy >= h) continue;
					for (px = 0; px < 4; px++)
					{
						int dx = bx * 4 + px;
						int p_i = py * 4 + px;
						uint32_t ci = (bits >> (2 * p_i)) & 3u;
						uint32_t al = a_lut[ci];
						if (dx >= w) continue;
						if (format == 15)
						{
							uint16_t row_a = *(uint16_t const *)(blk + py * 2);
							al = ((row_a >> (px * 4)) & 0xFu) * 17u;
						}
						else if (format == 16)
						{
							al = a_pal[(a_bits >> (3 * p_i)) & 7u];
						}
						dst[dy * w + dx] = (al << 24) | (r[ci] << 16) | (g[ci] << 8) | b[ci];
					}
				}
			}
		}
	}
	else
	{
		int is_pot = ((w & (w - 1)) == 0) && ((h & (h - 1)) == 0);
		int y, x;
		for (y = 0; y < h; y++)
		{
			for (x = 0; x < w; x++)
			{
				size_t si = (is_swizzled && is_pot)
					? (size_t)forge_xbox_swizzle_xy(x, y, w, h)
					: ((size_t)y * (size_t)w + (size_t)x);
				uint32_t pixel = 0xFF808080u;
				switch (format)
				{
				case 0: /* A8 */
					pixel = ((uint32_t)src[si] << 24) | 0x00FFFFFFu;
					break;
				case 1:  /* Y8 */
				case 17: /* P8 */
				{
					uint32_t v = src[si];
					pixel = 0xFF000000u | (v << 16) | (v << 8) | v;
					break;
				}
				case 2: /* AY8 */
				{
					uint32_t v = src[si];
					pixel = (v << 24) | (v << 16) | (v << 8) | v;
					break;
				}
				case 3: /* A8Y8 */
				{
					uint16_t p16 = ((uint16_t const *)src)[si];
					uint32_t v = p16 & 0xFFu;
					uint32_t al = (p16 >> 8) & 0xFFu;
					pixel = (al << 24) | (v << 16) | (v << 8) | v;
					break;
				}
				case 6: /* R5G6B5 */
				{
					uint16_t p16 = ((uint16_t const *)src)[si];
					uint32_t r = ((p16 >> 11) & 31u) * 255u / 31u;
					uint32_t g = ((p16 >> 5)  & 63u) * 255u / 63u;
					uint32_t b = ( p16        & 31u) * 255u / 31u;
					pixel = 0xFF000000u | (r << 16) | (g << 8) | b;
					break;
				}
				case 8: /* A1R5G5B5 */
				{
					uint16_t p16 = ((uint16_t const *)src)[si];
					uint32_t al = (p16 & 0x8000u) ? 255u : 0u;
					uint32_t r = ((p16 >> 10) & 31u) * 255u / 31u;
					uint32_t g = ((p16 >> 5)  & 31u) * 255u / 31u;
					uint32_t b = ( p16        & 31u) * 255u / 31u;
					pixel = (al << 24) | (r << 16) | (g << 8) | b;
					break;
				}
				case 9: /* A4R4G4B4 */
				{
					uint16_t p16 = ((uint16_t const *)src)[si];
					uint32_t al = ((p16 >> 12) & 15u) * 17u;
					uint32_t r  = ((p16 >> 8)  & 15u) * 17u;
					uint32_t g  = ((p16 >> 4)  & 15u) * 17u;
					uint32_t b  = ( p16        & 15u) * 17u;
					pixel = (al << 24) | (r << 16) | (g << 8) | b;
					break;
				}
				case 10: /* X8R8G8B8 */
					pixel = ((uint32_t const *)src)[si] | 0xFF000000u;
					break;
				case 11: /* A8R8G8B8 */
					pixel = ((uint32_t const *)src)[si];
					break;
				default:
					break;
				}
				dst[y * w + x] = pixel;
			}
		}
	}

	free(alloc_raw);
	*out_w = (int16_t)w;
	*out_h = (int16_t)h;
	return dst;
}

static int forge_preview_extract_shader_info(
	uint32_t s_grp,
	uint8_t const *s_ptr,
	size_t s_len,
	uint8_t const *block_buf,
	size_t block_buf_len,
	uint32_t block_addr_bias,
	int32_t *out_bitm_handle,
	float *out_u_scale,
	float *out_v_scale,
	int *out_alpha_test)
{
	float u_s = 1.0f, v_s = 1.0f;
	int32_t b_handle = -1;
	int alpha_test = 0;

	if (!s_ptr || !out_bitm_handle || !out_u_scale || !out_v_scale || !out_alpha_test)
		return 0;

	if (s_grp == 'soso' && s_len >= 0xB4u)
	{
		u_s = *(float const *)(s_ptr + 0x9Cu);
		v_s = *(float const *)(s_ptr + 0xA0u);
		b_handle = *(int32_t const *)(s_ptr + 0xB0u);
		alpha_test = 0;
	}
	else if (s_grp == 'senv' && s_len >= 0x98u)
	{
		b_handle = *(int32_t const *)(s_ptr + 0x94u);
		alpha_test = 0;
	}
	else if ((s_grp == 'schi' || s_grp == 'scex') && s_len >= 0x60u)
	{
		int32_t m_cnt = *(int32_t const *)(s_ptr + 0x54u);
		uint32_t m_addr = *(uint32_t const *)(s_ptr + 0x58u);
		if (m_cnt <= 0 && s_grp == 'scex' && s_len >= 0x6Cu)
		{
			m_cnt = *(int32_t const *)(s_ptr + 0x60u);
			m_addr = *(uint32_t const *)(s_ptr + 0x64u);
		}
		if (m_cnt > 0 && m_addr >= block_addr_bias &&
			(uint64_t)(m_addr - block_addr_bias) + 220u <= (uint64_t)block_buf_len)
		{
			uint8_t const *m0 = block_buf + (m_addr - block_addr_bias);
			u_s = *(float const *)(m0 + 0x54u);
			v_s = *(float const *)(m0 + 0x58u);
			b_handle = *(int32_t const *)(m0 + 0x78u);
			alpha_test = 1;
		}
	}
	else if (s_grp == 'sotr' && s_len >= 0x60u)
	{
		int32_t m_cnt = *(int32_t const *)(s_ptr + 0x54u);
		uint32_t m_addr = *(uint32_t const *)(s_ptr + 0x58u);
		if (m_cnt > 0 && m_addr >= block_addr_bias &&
			(uint64_t)(m_addr - block_addr_bias) + 100u <= (uint64_t)block_buf_len)
		{
			uint8_t const *m0 = block_buf + (m_addr - block_addr_bias);
			u_s = *(float const *)(m0 + 0x04u);
			v_s = *(float const *)(m0 + 0x08u);
			b_handle = *(int32_t const *)(m0 + 0x28u);
			alpha_test = 1;
		}
	}
	else if (s_grp == 'sgla' && s_len >= 0x168u)
	{
		b_handle = *(int32_t const *)(s_ptr + 0x164u);
		u_s = *(float const *)(s_ptr + 0x154u);
		v_s = u_s;
		if (b_handle == -1 || b_handle == 0)
		{
			b_handle = *(int32_t const *)(s_ptr + 0x70u);
			u_s = *(float const *)(s_ptr + 0x60u);
			v_s = u_s;
		}
		alpha_test = 0;
	}
	else if (s_grp == 'smet' && s_len >= 0x5Cu)
	{
		b_handle = *(int32_t const *)(s_ptr + 0x58u);
		alpha_test = 0;
	}

	if (b_handle == -1 || b_handle == 0)
		return 0;
	if (!(u_s > 0.0001f && u_s < 1000.0f)) u_s = 1.0f;
	if (!(v_s > 0.0001f && v_s < 1000.0f)) v_s = 1.0f;
	*out_bitm_handle = b_handle;
	*out_u_scale = u_s;
	*out_v_scale = v_s;
	*out_alpha_test = alpha_test;
	return 1;
}

static int16_t forge_preview_load_donor_bitmap(
	struct forge_donor_map_ctx const *ctx,
	int32_t bitm_idx,
	int alpha_test)
{
	uint32_t b_io, b_grp, b_addr, b_ext;
	uint8_t const *bmp_entry = NULL;
	uint8_t *ext_buf = NULL;
	uint32_t ext_sz = 0;
	FILE *pix_fp = NULL;
	int16_t w = 0, h = 0;
	uint32_t *pixels = NULL;
	int i, slot;

	if (!ctx || bitm_idx < 0 || bitm_idx >= ctx->tag_count)
		return -1;
	for (i = 0; i < s_forge_preview.tex_count; i++)
	{
		if (s_forge_preview.textures[i].bitm_key == bitm_idx)
			return (s_forge_preview.textures[i].pixels != NULL) ? (int16_t)i : -1;
	}
	if (s_forge_preview.tex_count >= FORGE_PREVIEW_MAX_TEXTURES)
		return -1;

	slot = s_forge_preview.tex_count++;
	s_forge_preview.textures[slot].bitm_key = bitm_idx;
	s_forge_preview.textures[slot].alpha_test = alpha_test;
	s_forge_preview.textures[slot].pixels = NULL;

	b_io = ctx->inst_off + (uint32_t)bitm_idx * 32u;
	b_grp = *(uint32_t const *)(ctx->tag_buf + b_io + 0x00u);
	b_addr = *(uint32_t const *)(ctx->tag_buf + b_io + 0x14u);
	b_ext = *(uint32_t const *)(ctx->tag_buf + b_io + 0x18u);
	if (b_grp != 'bitm')
		return -1;

	if (!ctx->is_xbox_v5 && b_ext != 0)
	{
		ext_buf = forge_donor_read_ext_bitmap_tag(ctx->bitmaps_fp, b_addr, &ext_sz);
		if (ext_buf && ext_sz >= 0x6Cu)
		{
			int32_t b_cnt = *(int32_t const *)(ext_buf + 0x60u);
			uint32_t b_rel = *(uint32_t const *)(ext_buf + 0x64u);
			if (b_cnt > 0 && b_rel >= 0x6Cu && b_rel + 48u <= ext_sz)
			{
				bmp_entry = ext_buf + b_rel;
				pix_fp = ctx->bitmaps_fp;
			}
		}
	}
	else if (b_addr >= ctx->tag_base && (b_addr - ctx->tag_base) + 0x6Cu <= ctx->tag_sz)
	{
		uint32_t b_off = b_addr - ctx->tag_base;
		int32_t b_cnt = *(int32_t const *)(ctx->tag_buf + b_off + 0x60u);
		uint32_t b_block_addr = *(uint32_t const *)(ctx->tag_buf + b_off + 0x64u);
		if (b_cnt > 0 && b_block_addr >= ctx->tag_base &&
			(b_block_addr - ctx->tag_base) + 48u <= ctx->tag_sz)
		{
			uint16_t b_flags;
			bmp_entry = ctx->tag_buf + (b_block_addr - ctx->tag_base);
			b_flags = *(uint16_t const *)(bmp_entry + 0x0Eu);
			pix_fp = (((b_flags & 0x100u) != 0) && !ctx->is_xbox_v5) ? ctx->bitmaps_fp : ctx->map_fp;
		}
	}

	if (bmp_entry && pix_fp)
		pixels = forge_preview_decode_bitmap_pixels(bmp_entry, pix_fp, NULL, 0, &w, &h);
	free(ext_buf);

	if (!pixels)
		return -1;
	s_forge_preview.textures[slot].width = w;
	s_forge_preview.textures[slot].height = h;
	s_forge_preview.textures[slot].pixels = pixels;
	return (int16_t)slot;
}

static int16_t forge_preview_resolve_donor_part_shader(
	struct forge_donor_map_ctx const *ctx,
	int32_t shaders_cnt,
	uint32_t shaders_addr,
	int16_t shader_idx,
	float *out_u_scale,
	float *out_v_scale)
{
	uint32_t sh_off, s_io, s_grp, sh_data_addr, s_ext;
	int32_t sh_handle, s_idx, bitm_handle = -1;
	float u_s = 1.0f, v_s = 1.0f;
	int alpha_test = 0;

	if (out_u_scale) *out_u_scale = 1.0f;
	if (out_v_scale) *out_v_scale = 1.0f;
	if (!ctx || shader_idx < 0 || shader_idx >= shaders_cnt || shaders_cnt <= 0 || shaders_cnt > 256 ||
		shaders_addr < ctx->tag_base ||
		(uint64_t)(shaders_addr - ctx->tag_base) + (uint64_t)shaders_cnt * 32u > (uint64_t)ctx->tag_sz)
		return -1;

	sh_off = (shaders_addr - ctx->tag_base) + (uint32_t)shader_idx * 32u;
	sh_handle = *(int32_t const *)(ctx->tag_buf + sh_off + 0x0Cu);
	if (sh_handle == -1 || sh_handle == 0)
		return -1;
	s_idx = (int32_t)((uint32_t)sh_handle & 0xFFFFu);
	if (s_idx < 0 || s_idx >= ctx->tag_count)
		return -1;

	s_io = ctx->inst_off + (uint32_t)s_idx * 32u;
	s_grp = *(uint32_t const *)(ctx->tag_buf + s_io + 0x00u);
	sh_data_addr = *(uint32_t const *)(ctx->tag_buf + s_io + 0x14u);
	s_ext = *(uint32_t const *)(ctx->tag_buf + s_io + 0x18u);
	if ((!ctx->is_xbox_v5 && s_ext != 0) || sh_data_addr < ctx->tag_base || (sh_data_addr - ctx->tag_base) >= ctx->tag_sz)
		return -1;

	if (!forge_preview_extract_shader_info(
			s_grp,
			ctx->tag_buf + (sh_data_addr - ctx->tag_base),
			ctx->tag_sz - (sh_data_addr - ctx->tag_base),
			ctx->tag_buf,
			ctx->tag_sz,
			ctx->tag_base,
			&bitm_handle,
			&u_s,
			&v_s,
			&alpha_test))
		return -1;

	if (out_u_scale) *out_u_scale = u_s;
	if (out_v_scale) *out_v_scale = v_s;
	return forge_preview_load_donor_bitmap(ctx, (int32_t)((uint32_t)bitm_handle & 0xFFFFu), alpha_test);
}

static int16_t forge_preview_resolve_ocetags_part_shader(
	char const *json,
	size_t tags_pos,
	size_t tags_end,
	uint8_t const *mod_buf,
	size_t mod_len,
	int16_t shader_idx,
	float *out_u_scale,
	float *out_v_scale)
{
	int32_t sh_cnt, sh_handle, bitm_handle = -1;
	uint32_t sh_rel;
	uint16_t sh_low, bitm_low;
	float u_s = 1.0f, v_s = 1.0f;
	int alpha_test = 0, have_sh = 0, i, slot;
	size_t p;

	if (out_u_scale) *out_u_scale = 1.0f;
	if (out_v_scale) *out_v_scale = 1.0f;
	if (!json || !mod_buf || mod_len < 0xE8u || shader_idx < 0)
		return -1;

	sh_cnt = *(int32_t const *)(mod_buf + 0xDCu);
	sh_rel = *(uint32_t const *)(mod_buf + 0xE0u);
	if (shader_idx >= sh_cnt || sh_cnt <= 0 || sh_cnt > 256 || sh_rel < 0xE8u ||
		(uint64_t)sh_rel + (uint64_t)sh_cnt * 32u > (uint64_t)mod_len)
		return -1;

	sh_handle = *(int32_t const *)(mod_buf + sh_rel + (uint32_t)shader_idx * 32u + 0x0Cu);
	if (sh_handle == -1 || sh_handle == 0)
		return -1;
	sh_low = (uint16_t)((uint32_t)sh_handle & 0xFFFFu);

	/* Find shader tag in tags[] by matching originalIndex & 0xFFFF */
	p = tags_pos + 1;
	while (p < tags_end)
	{
		size_t o_start, o_end;
		uint32_t orig_idx = 0xFFFFFFFFu;
		while (p < tags_end && json[p] != '{' && json[p] != ']') p++;
		if (p >= tags_end || json[p] != '{') break;
		o_start = p;
		if (!ocetags_json_skip_value(json, o_start, tags_end, &o_end)) break;
		if (ocetags_json_get_u32(json, o_start, o_end, "originalIndex", &orig_idx) &&
			(uint16_t)(orig_idx & 0xFFFFu) == sh_low)
		{
			char sgstr[64];
			size_t sd_len = 0;
			uint8_t *s_buf = ocetags_json_decode_base64_key(json, o_start, o_end, "data", &sd_len);
			sgstr[0] = 0;
			ocetags_json_get_str(json, o_start, o_end, "group", sgstr, sizeof(sgstr));
			if (s_buf && sd_len >= 0x40u)
			{
				have_sh = forge_preview_extract_shader_info(
					ocetags_group_str_to_fourcc(sgstr),
					s_buf, sd_len,
					s_buf, sd_len, 0u,
					&bitm_handle, &u_s, &v_s, &alpha_test);
			}
			free(s_buf);
			break;
		}
		p = o_end;
	}
	if (!have_sh || bitm_handle == -1 || bitm_handle == 0)
		return -1;

	if (out_u_scale) *out_u_scale = u_s;
	if (out_v_scale) *out_v_scale = v_s;
	bitm_low = (uint16_t)((uint32_t)bitm_handle & 0xFFFFu);

	for (i = 0; i < s_forge_preview.tex_count; i++)
	{
		if (s_forge_preview.textures[i].bitm_key == (int32_t)bitm_low)
			return (s_forge_preview.textures[i].pixels != NULL) ? (int16_t)i : -1;
	}
	if (s_forge_preview.tex_count >= FORGE_PREVIEW_MAX_TEXTURES)
		return -1;

	slot = s_forge_preview.tex_count++;
	s_forge_preview.textures[slot].bitm_key = (int32_t)bitm_low;
	s_forge_preview.textures[slot].alpha_test = alpha_test;
	s_forge_preview.textures[slot].pixels = NULL;

	/* Find 'bitm' tag in tags[] by matching originalIndex & 0xFFFF */
	p = tags_pos + 1;
	while (p < tags_end)
	{
		size_t o_start, o_end;
		uint32_t orig_idx = 0xFFFFFFFFu;
		while (p < tags_end && json[p] != '{' && json[p] != ']') p++;
		if (p >= tags_end || json[p] != '{') break;
		o_start = p;
		if (!ocetags_json_skip_value(json, o_start, tags_end, &o_end)) break;
		if (ocetags_json_get_u32(json, o_start, o_end, "originalIndex", &orig_idx) &&
			(uint16_t)(orig_idx & 0xFFFFu) == bitm_low)
		{
			size_t bd_len = 0, raw_arr = 0, raw_end = 0;
			uint8_t *b_buf = ocetags_json_decode_base64_key(json, o_start, o_end, "data", &bd_len);
			if (b_buf && bd_len >= 0x6Cu)
			{
				int32_t b_cnt = *(int32_t const *)(b_buf + 0x60u);
				uint32_t b_rel = *(uint32_t const *)(b_buf + 0x64u);
				if (b_cnt > 0 && b_rel >= 0x6Cu && b_rel + 48u <= bd_len &&
					ocetags_json_find_key_range(json, o_start, o_end, "embeddedRawDataAssets", &raw_arr) &&
					ocetags_json_skip_value(json, raw_arr, o_end, &raw_end))
				{
					size_t rp = raw_arr + 1;
					while (rp < raw_end && json[rp] != '{' && json[rp] != ']') rp++;
					if (rp < raw_end && json[rp] == '{')
					{
						size_t r0_start = rp, r0_end = 0;
						if (ocetags_json_skip_value(json, r0_start, raw_end, &r0_end))
						{
							size_t pix_len = 0;
							uint8_t *raw_pix = ocetags_json_decode_base64_key(json, r0_start, r0_end, "data", &pix_len);
							if (raw_pix && pix_len > 0)
							{
								int16_t tw = 0, th = 0;
								uint32_t *dec = forge_preview_decode_bitmap_pixels(
									b_buf + b_rel, NULL, raw_pix, pix_len, &tw, &th);
								if (dec)
								{
									s_forge_preview.textures[slot].width = tw;
									s_forge_preview.textures[slot].height = th;
									s_forge_preview.textures[slot].pixels = dec;
								}
							}
							free(raw_pix);
						}
					}
				}
			}
			free(b_buf);
			break;
		}
		p = o_end;
	}

	return (s_forge_preview.textures[slot].pixels != NULL) ? (int16_t)slot : -1;
}

static void forge_preview_append_part_68b(
	uint8_t const *verts_68,
	int32_t vtx_cnt,
	uint16_t const *strip_indices,
	int32_t idx_cnt,
	struct real_matrix4x3 const *node_mats,
	int32_t node_cnt,
	uint8_t const *local_nodes,
	int32_t local_node_cnt,
	int16_t tex_idx,
	float u_scale,
	float v_scale)
{
	int32_t base_v = s_forge_preview.vert_count;
	int32_t vi, ii;

	if (vtx_cnt < 3 || idx_cnt < 3)
		return;
	if (base_v + vtx_cnt > FORGE_PREVIEW_MAX_VERTS)
		vtx_cnt = FORGE_PREVIEW_MAX_VERTS - base_v;
	if (vtx_cnt < 3)
		return;

	for (vi = 0; vi < vtx_cnt; vi++)
	{
		uint8_t const *vp = verts_68 + (size_t)vi * 68u;
		float px = *(float const *)(vp + 0u);
		float py = *(float const *)(vp + 4u);
		float pz = *(float const *)(vp + 8u);
		float nx = *(float const *)(vp + 12u);
		float ny = *(float const *)(vp + 16u);
		float nz = *(float const *)(vp + 20u);
		float vu = *(float const *)(vp + 48u);
		float vv = *(float const *)(vp + 52u);
		int16_t n0 = *(int16_t const *)(vp + 56u);
		int16_t n1 = *(int16_t const *)(vp + 58u);
		float w0 = *(float const *)(vp + 60u);
		float w1 = *(float const *)(vp + 64u);

		if (!(px > -10000.0f && px < 10000.0f)) px = 0.0f;
		if (!(py > -10000.0f && py < 10000.0f)) py = 0.0f;
		if (!(pz > -10000.0f && pz < 10000.0f)) pz = 0.0f;
		if (!(nx >= -2.0f && nx <= 2.0f)) nx = 0.0f;
		if (!(ny >= -2.0f && ny <= 2.0f)) ny = 0.0f;
		if (!(nz >= -2.0f && nz <= 2.0f)) nz = 1.0f;
		if (!(vu > -10000.0f && vu < 10000.0f)) vu = 0.0f;
		if (!(vv > -10000.0f && vv < 10000.0f)) vv = 0.0f;
		if (!(w0 >= 0.0f && w0 <= 1.0f)) w0 = 1.0f;
		if (!(w1 >= 0.0f && w1 <= 1.0f)) w1 = 0.0f;
		if (w0 + w1 <= 0.001f) { w0 = 1.0f; w1 = 0.0f; }

		if (local_nodes && local_node_cnt > 0)
		{
			if (n0 >= 0 && n0 < local_node_cnt && n0 < 22)
				n0 = (int16_t)local_nodes[n0];
			if (n1 >= 0 && n1 < local_node_cnt && n1 < 22)
				n1 = (int16_t)local_nodes[n1];
		}

		if (node_mats && node_cnt > 0)
		{
			if (n0 >= 0 && n0 < node_cnt && n1 >= 0 && n1 < node_cnt && n1 != n0 && w1 > 0.001f)
			{
				struct real_matrix4x3 const *m0 = &node_mats[n0];
				struct real_matrix4x3 const *m1 = &node_mats[n1];
				float tx0 = m0->scale * (m0->forward.i * px + m0->left.i * py + m0->up.i * pz) + m0->position.x;
				float ty0 = m0->scale * (m0->forward.j * px + m0->left.j * py + m0->up.j * pz) + m0->position.y;
				float tz0 = m0->scale * (m0->forward.k * px + m0->left.k * py + m0->up.k * pz) + m0->position.z;
				float tx1 = m1->scale * (m1->forward.i * px + m1->left.i * py + m1->up.i * pz) + m1->position.x;
				float ty1 = m1->scale * (m1->forward.j * px + m1->left.j * py + m1->up.j * pz) + m1->position.y;
				float tz1 = m1->scale * (m1->forward.k * px + m1->left.k * py + m1->up.k * pz) + m1->position.z;
				float tnx0 = m0->forward.i * nx + m0->left.i * ny + m0->up.i * nz;
				float tny0 = m0->forward.j * nx + m0->left.j * ny + m0->up.j * nz;
				float tnz0 = m0->forward.k * nx + m0->left.k * ny + m0->up.k * nz;
				float tnx1 = m1->forward.i * nx + m1->left.i * ny + m1->up.i * nz;
				float tny1 = m1->forward.j * nx + m1->left.j * ny + m1->up.j * nz;
				float tnz1 = m1->forward.k * nx + m1->left.k * ny + m1->up.k * nz;
				px = w0 * tx0 + w1 * tx1;
				py = w0 * ty0 + w1 * ty1;
				pz = w0 * tz0 + w1 * tz1;
				nx = w0 * tnx0 + w1 * tnx1;
				ny = w0 * tny0 + w1 * tny1;
				nz = w0 * tnz0 + w1 * tnz1;
			}
			else if (n0 >= 0 && n0 < node_cnt)
			{
				struct real_matrix4x3 const *m = &node_mats[n0];
				float tx = m->scale * (m->forward.i * px + m->left.i * py + m->up.i * pz) + m->position.x;
				float ty = m->scale * (m->forward.j * px + m->left.j * py + m->up.j * pz) + m->position.y;
				float tz = m->scale * (m->forward.k * px + m->left.k * py + m->up.k * pz) + m->position.z;
				float tnx = m->forward.i * nx + m->left.i * ny + m->up.i * nz;
				float tny = m->forward.j * nx + m->left.j * ny + m->up.j * nz;
				float tnz = m->forward.k * nx + m->left.k * ny + m->up.k * nz;
				px = tx; py = ty; pz = tz;
				nx = tnx; ny = tny; nz = tnz;
			}
		}
		s_forge_preview.verts[base_v + vi][0] = px;
		s_forge_preview.verts[base_v + vi][1] = py;
		s_forge_preview.verts[base_v + vi][2] = pz;
		s_forge_preview.norms[base_v + vi][0] = nx;
		s_forge_preview.norms[base_v + vi][1] = ny;
		s_forge_preview.norms[base_v + vi][2] = nz;
		s_forge_preview.uvs[base_v + vi][0] = vu * u_scale;
		s_forge_preview.uvs[base_v + vi][1] = vv * v_scale;
	}
	s_forge_preview.vert_count += vtx_cnt;

	for (ii = 0; ii + 2 < idx_cnt && s_forge_preview.tri_count < FORGE_PREVIEW_MAX_TRIS; ii++)
	{
		uint16_t i0 = strip_indices[ii];
		uint16_t i1 = strip_indices[ii + 1];
		uint16_t i2 = strip_indices[ii + 2];
		if (i0 == 0xFFFFu || i1 == 0xFFFFu || i2 == 0xFFFFu)
			continue;
		if (i0 == i1 || i1 == i2 || i0 == i2)
			continue;
		if (i0 >= (uint16_t)vtx_cnt || i1 >= (uint16_t)vtx_cnt || i2 >= (uint16_t)vtx_cnt)
			continue;
		if (ii & 1)
		{
			uint16_t tmp = i1;
			i1 = i2;
			i2 = tmp;
		}
		s_forge_preview.tris[s_forge_preview.tri_count].v0 = (uint16_t)(base_v + i0);
		s_forge_preview.tris[s_forge_preview.tri_count].v1 = (uint16_t)(base_v + i1);
		s_forge_preview.tris[s_forge_preview.tri_count].v2 = (uint16_t)(base_v + i2);
		s_forge_preview.tris[s_forge_preview.tri_count].tex_idx = tex_idx;
		s_forge_preview.tri_count++;
	}
}

static void forge_preview_compute_node_mats(
	uint8_t const *node_base,
	int32_t node_cnt,
	struct real_matrix4x3 *out_mats)
{
	int32_t n;
	int pass;
	int computed[64];

	memset(computed, 0, sizeof(computed));
	if (node_cnt > 64) node_cnt = 64;
	for (n = 0; n < node_cnt; n++)
	{
		out_mats[n].scale = 1.0f;
		out_mats[n].forward.i = 1.0f; out_mats[n].forward.j = 0.0f; out_mats[n].forward.k = 0.0f;
		out_mats[n].left.i = 0.0f;    out_mats[n].left.j = 1.0f;    out_mats[n].left.k = 0.0f;
		out_mats[n].up.i = 0.0f;      out_mats[n].up.j = 0.0f;      out_mats[n].up.k = 1.0f;
		out_mats[n].position.x = 0.0f; out_mats[n].position.y = 0.0f; out_mats[n].position.z = 0.0f;
	}
	for (pass = 0; pass < node_cnt; pass++)
	{
		for (n = 0; n < node_cnt; n++)
		{
			uint8_t const *mn = node_base + (size_t)n * 156u;
			int16_t parent_idx = -1;
			int32_t m;
			struct real_matrix4x3 loc;
			struct real_quaternion rot;
			union real_point3d trans;

			if (computed[n])
				continue;
			/* Find parent by scanning first_child_node_index (+36) and next_sibling_node_index (+34) */
			for (m = 0; m < node_cnt; m++)
			{
				uint8_t const *pmn = node_base + (size_t)m * 156u;
				int16_t fc = *(int16_t const *)(pmn + 36u);
				int guard = 0;
				while (fc >= 0 && fc < node_cnt && guard++ < 64)
				{
					if (fc == n) { parent_idx = (int16_t)m; break; }
					fc = *(int16_t const *)(node_base + (size_t)fc * 156u + 34u);
				}
				if (parent_idx >= 0) break;
			}
			if (parent_idx >= 0 && !computed[parent_idx])
				continue;

			trans.x = *(float const *)(mn + 0x28u);
			trans.y = *(float const *)(mn + 0x2Cu);
			trans.z = *(float const *)(mn + 0x30u);
			rot.v.i = *(float const *)(mn + 0x34u);
			rot.v.j = *(float const *)(mn + 0x38u);
			rot.v.k = *(float const *)(mn + 0x3Cu);
			rot.w   = *(float const *)(mn + 0x40u);
			if (rot.v.i == 0.0f && rot.v.j == 0.0f && rot.v.k == 0.0f && rot.w == 0.0f)
				rot.w = 1.0f;
			matrix4x3_from_point_and_quaternion(&loc, &trans, &rot);
			if (parent_idx >= 0)
				matrix4x3_multiply(&out_mats[parent_idx], &loc, &out_mats[n]);
			else
				out_mats[n] = loc;
			computed[n] = 1;
		}
	}
}

static int forge_preview_load_from_ocetags_json(char const *json, size_t json_len)
{
	size_t tags_pos = 0, tags_end = 0, p;
	uint16_t preferred_mod_low = 0xFFFFu;
	int have_preferred_mod = 0;
	int pass;

	if (!json || json_len < 32)
		return 0;
	if (!ocetags_json_find_key_range(json, 0, json_len, "tags", &tags_pos) ||
		!ocetags_json_skip_value(json, tags_pos, json_len, &tags_end))
		return 0;

	/* Inspect root tag (tags[0]) to identify the primary 'mod2'/'mode' tag's originalIndex */
	p = tags_pos + 1;
	while (p < tags_end && json[p] != '{' && json[p] != ']') p++;
	if (p < tags_end && json[p] == '{')
	{
		size_t root_start = p, root_end = 0;
		if (ocetags_json_skip_value(json, root_start, tags_end, &root_end))
		{
			char rgstr[64];
			uint32_t rfourcc;
			size_t rd_len = 0;
			uint8_t *r_buf;
			rgstr[0] = 0;
			ocetags_json_get_str(json, root_start, root_end, "group", rgstr, sizeof(rgstr));
			rfourcc = ocetags_group_str_to_fourcc(rgstr);
			r_buf = ocetags_json_decode_base64_key(json, root_start, root_end, "data", &rd_len);
			if (r_buf)
			{
				if (rfourcc == 'actv' && rd_len >= 0x24u)
				{
					int32_t unit_ref = *(int32_t const *)(r_buf + 0x20u);
					if (unit_ref != -1 && unit_ref != 0)
					{
						uint16_t unit_low = (uint16_t)((uint32_t)unit_ref & 0xFFFFu);
						size_t up = root_end;
						while (up < tags_end)
						{
							size_t u_start, u_end;
							uint32_t u_orig = 0xFFFFFFFFu;
							while (up < tags_end && json[up] != '{' && json[up] != ']') up++;
							if (up >= tags_end || json[up] != '{') break;
							u_start = up;
							if (!ocetags_json_skip_value(json, u_start, tags_end, &u_end)) break;
							if (ocetags_json_get_u32(json, u_start, u_end, "originalIndex", &u_orig) &&
								(uint16_t)(u_orig & 0xFFFFu) == unit_low)
							{
								size_t ud_len = 0;
								uint8_t *u_buf = ocetags_json_decode_base64_key(json, u_start, u_end, "data", &ud_len);
								if (u_buf && ud_len >= 0x38u)
								{
									int32_t m_ref = *(int32_t const *)(u_buf + 0x34u);
									if (m_ref != -1 && m_ref != 0)
									{
										preferred_mod_low = (uint16_t)((uint32_t)m_ref & 0xFFFFu);
										have_preferred_mod = 1;
									}
								}
								free(u_buf);
								break;
							}
							up = u_end;
						}
					}
				}
				else if (rd_len >= 0x38u)
				{
					int32_t m_ref = *(int32_t const *)(r_buf + 0x34u);
					if (m_ref != -1 && m_ref != 0)
					{
						preferred_mod_low = (uint16_t)((uint32_t)m_ref & 0xFFFFu);
						have_preferred_mod = 1;
					}
				}
				free(r_buf);
			}
		}
	}

	for (pass = 0; pass < 2; pass++)
	{
		p = tags_pos + 1;
		while (p < tags_end)
		{
			size_t obj_start, obj_end;
			char gstr[64];
			uint32_t fourcc;
			while (p < tags_end && json[p] != '{' && json[p] != ']') p++;
			if (p >= tags_end || json[p] != '{') break;
			obj_start = p;
			if (!ocetags_json_skip_value(json, obj_start, tags_end, &obj_end)) break;
			gstr[0] = 0;
			ocetags_json_get_str(json, obj_start, obj_end, "group", gstr, sizeof(gstr));
			fourcc = ocetags_group_str_to_fourcc(gstr);
			if (fourcc == 'mod2' || fourcc == 'mode')
			{
				if (pass == 0)
				{
					uint32_t orig_idx = 0xFFFFFFFFu;
					char mpath[256];
					mpath[0] = 0;
					ocetags_json_get_str(json, obj_start, obj_end, "path", mpath, sizeof(mpath));
					if (have_preferred_mod &&
						ocetags_json_get_u32(json, obj_start, obj_end, "originalIndex", &orig_idx))
					{
						if ((uint16_t)(orig_idx & 0xFFFFu) != preferred_mod_low)
						{
							p = obj_end;
							continue;
						}
					}
					else if (strstr(mpath, "\\fp\\") != NULL || strstr(mpath, "/fp/") != NULL)
					{
						p = obj_end;
						continue;
					}
				}
			{
				size_t d_len = 0;
				uint8_t *mod_buf = ocetags_json_decode_base64_key(json, obj_start, obj_end, "data", &d_len);
				struct real_matrix4x3 node_mats[64];
				int32_t node_cnt = 0;
				size_t gparts_arr = 0, gparts_end = 0;

			if (mod_buf && d_len >= 0xE8u)
			{
				int32_t nc = *(int32_t const *)(mod_buf + 0xB8u);
				uint32_t n_rel = *(uint32_t const *)(mod_buf + 0xBCu);
				if (nc > 0 && nc <= 64 && n_rel >= 0xE8u && n_rel + (uint32_t)nc * 156u <= d_len)
				{
					node_cnt = nc;
					forge_preview_compute_node_mats(mod_buf + n_rel, node_cnt, node_mats);
				}
			}

			if (ocetags_json_find_key_range(json, obj_start, obj_end, "gbxmodelGeometryParts", &gparts_arr) &&
				ocetags_json_skip_value(json, gparts_arr, obj_end, &gparts_end))
			{
				int has_local_nodes = (mod_buf && d_len >= 0xE8u && (*(uint32_t const *)(mod_buf + 0x00) & 2u) != 0);
				uint8_t active_geom[256];
				int has_region_filter = 0;
				size_t gp = gparts_arr + 1;

				memset(active_geom, 0, sizeof(active_geom));
				if (mod_buf && d_len >= 0xE8u)
				{
					int32_t reg_cnt = *(int32_t const *)(mod_buf + 0xC4u);
					uint32_t reg_rel = *(uint32_t const *)(mod_buf + 0xC8u);
					int32_t geom_cnt = *(int32_t const *)(mod_buf + 0xD0u);
					int32_t r_i;
					if (reg_cnt > 0 && reg_cnt <= 32 && reg_rel >= 0xE8u &&
						(uint64_t)reg_rel + (uint64_t)reg_cnt * 76u <= d_len)
					{
						for (r_i = 0; r_i < reg_cnt; r_i++)
						{
							uint32_t r_off = reg_rel + (uint32_t)r_i * 76u;
							int32_t perm_cnt = *(int32_t const *)(mod_buf + r_off + 64u);
							uint32_t perm_rel = *(uint32_t const *)(mod_buf + r_off + 68u);
							if (perm_cnt > 0 && perm_rel >= 0xE8u && (uint64_t)perm_rel + 88u <= d_len)
							{
								int lod_i;
								for (lod_i = 4; lod_i >= 0; lod_i--)
								{
									int16_t cand_g = *(int16_t const *)(mod_buf + perm_rel + 64u + (uint32_t)lod_i * 2u);
									if (cand_g >= 0 && cand_g < geom_cnt && cand_g < 256)
									{
										active_geom[cand_g] = 1;
										has_region_filter = 1;
										break;
									}
								}
							}
						}
					}
				}

				while (gp < gparts_end)
				{
					size_t gp_start, gp_end;
					size_t vb_len = 0, tb_len = 0;
					uint8_t *v_raw;
					uint8_t *t_raw;
					uint8_t const *local_nodes = NULL;
					int32_t local_node_cnt = 0;
					int16_t part_shader_idx = -1;
					int16_t tex_idx = -1;
					float sh_u_scale = 1.0f, sh_v_scale = 1.0f;
					float mod_u_scale = 1.0f, mod_v_scale = 1.0f;
					uint32_t g_i = 0, p_i = 0;
					int have_gp_idx;
					while (gp < gparts_end && json[gp] != '{' && json[gp] != ']') gp++;
					if (gp >= gparts_end || json[gp] != '{') break;
					gp_start = gp;
					if (!ocetags_json_skip_value(json, gp_start, gparts_end, &gp_end)) break;
					have_gp_idx = (ocetags_json_get_u32(json, gp_start, gp_end, "geometryIndex", &g_i) &&
								   ocetags_json_get_u32(json, gp_start, gp_end, "partIndex", &p_i));
					if (have_gp_idx && has_region_filter && g_i < 256u && !active_geom[g_i])
					{
						gp = gp_end;
						continue;
					}
					if (mod_buf && d_len >= 0xE8u)
					{
						mod_u_scale = *(float const *)(mod_buf + 0x30u);
						mod_v_scale = *(float const *)(mod_buf + 0x34u);
						if (!(mod_u_scale > 0.0001f && mod_u_scale < 1000.0f)) mod_u_scale = 1.0f;
						if (!(mod_v_scale > 0.0001f && mod_v_scale < 1000.0f)) mod_v_scale = 1.0f;
					}
					if (mod_buf && d_len >= 0xE8u && have_gp_idx)
					{
						int32_t geom_cnt = *(int32_t const *)(mod_buf + 0xD0u);
						uint32_t geom_rel = *(uint32_t const *)(mod_buf + 0xD4u);
						if ((int32_t)g_i < geom_cnt && geom_rel >= 0xE8u && (uint64_t)geom_rel + (uint64_t)geom_cnt * 48u <= d_len)
						{
							uint8_t const *g_ptr = mod_buf + geom_rel + (size_t)g_i * 48u;
							int32_t part_cnt = *(int32_t const *)(g_ptr + 36u);
							uint32_t parts_rel = *(uint32_t const *)(g_ptr + 40u);
							if ((int32_t)p_i < part_cnt && parts_rel >= 0xE8u && (uint64_t)parts_rel + (uint64_t)part_cnt * 132u <= d_len)
							{
								uint8_t const *ce_part = mod_buf + parts_rel + (size_t)p_i * 132u;
								part_shader_idx = *(int16_t const *)(ce_part + 0x04u);
								if (has_local_nodes)
								{
									local_node_cnt = (int32_t)ce_part[107];
									local_nodes = ce_part + 108;
								}
							}
						}
					}
					if (part_shader_idx >= 0)
					{
						tex_idx = forge_preview_resolve_ocetags_part_shader(
							json, tags_pos, tags_end, mod_buf, d_len,
							part_shader_idx, &sh_u_scale, &sh_v_scale);
					}
					v_raw = ocetags_json_decode_base64_key(json, gp_start, gp_end, "vertexData", &vb_len);
					if (!v_raw)
						v_raw = ocetags_json_decode_base64_key(json, gp_start, gp_end, "vertices", &vb_len);
					t_raw = ocetags_json_decode_base64_key(json, gp_start, gp_end, "triangleData", &tb_len);
					if (!t_raw)
						t_raw = ocetags_json_decode_base64_key(json, gp_start, gp_end, "triangles", &tb_len);
					if (v_raw && t_raw && vb_len >= 3u * 68u && tb_len >= 6u)
					{
						forge_preview_append_part_68b(
							v_raw,
							(int32_t)(vb_len / 68u),
							(uint16_t const *)t_raw,
							(int32_t)(tb_len / 2u),
							(node_cnt > 0) ? node_mats : NULL,
							node_cnt,
							local_nodes,
							local_node_cnt,
							tex_idx,
							mod_u_scale * sh_u_scale,
							mod_v_scale * sh_v_scale);
					}
					free(v_raw);
					free(t_raw);
					gp = gp_end;
				}
			}
			free(mod_buf);
			if (s_forge_preview.tri_count > 0)
				return 1;
			}
		}
		p = obj_end;
	}
	}
	return 0;
}

static int forge_preview_load_from_donor_map(char const *map_path, uint32_t group_tag, char const *tag_path)
{
	struct forge_donor_map_ctx ctx;
	struct forge_ext_closure_list *closure;
	int root_idx;
	uint32_t obj_io, obj_data_addr, obj_off;
	int32_t mod_handle, mod_idx;
	uint32_t mod_io, mod_data_addr, mod_off;
	int32_t node_cnt, reg_cnt, geom_cnt, r_i;
	uint32_t node_addr, reg_addr, geom_addr;
	struct real_matrix4x3 node_mats[64];

	if (!forge_donor_map_open(map_path, &ctx))
		return 0;

	root_idx = forge_donor_find_tag_index(&ctx, group_tag, tag_path);
	if (root_idx < 0)
	{
		forge_donor_map_close(&ctx);
		return 0;
	}

	/* Compute approximate closure size in bytes for the info panel, and keep closure for child mod2 fallback */
	closure = (struct forge_ext_closure_list *)calloc(1, sizeof(*closure));
	if (closure)
	{
		int ci;
		uint32_t est_bytes = 0;
		forge_donor_add_tag_closure(&ctx, root_idx, closure, 0);
		for (ci = 0; ci < closure->count; ci++)
		{
			int32_t ti = closure->tag_indices[ci];
			uint32_t tio = ctx.inst_off + (uint32_t)ti * 32u;
			uint32_t tgrp = *(uint32_t const *)(ctx.tag_buf + tio + 0x00);
			struct tag_schema_group const *sg = forge_runtime_schema_group_get((tgrp == 'mode') ? 'mod2' : tgrp);
			if (sg && sg->definition && sg->definition->size > 0)
				est_bytes += (uint32_t)sg->definition->size * 4u;
			else
				est_bytes += 2048u;
		}
		s_forge_preview.tag_size_bytes = est_bytes;
	}

	mod_idx = -1;
	obj_io = ctx.inst_off + (uint32_t)root_idx * 32u;
	obj_data_addr = *(uint32_t const *)(ctx.tag_buf + obj_io + 0x14);
	if (group_tag == 'actv' && obj_data_addr >= ctx.tag_base && (obj_data_addr - ctx.tag_base) + 0x24u <= ctx.tag_sz)
	{
		uint32_t actv_off = obj_data_addr - ctx.tag_base;
		uint32_t unit_grp = *(uint32_t const *)(ctx.tag_buf + actv_off + 0x14u);
		int32_t unit_handle = *(int32_t const *)(ctx.tag_buf + actv_off + 0x20u);
		if ((unit_grp == 'bipd' || unit_grp == 'unit' || unit_grp == 'vehi') && unit_handle != -1 && unit_handle != 0)
		{
			int32_t unit_idx = (int32_t)((uint32_t)unit_handle & 0xFFFFu);
			if (unit_idx >= 0 && unit_idx < ctx.tag_count)
			{
				uint32_t u_io = ctx.inst_off + (uint32_t)unit_idx * 32u;
				uint32_t u_addr = *(uint32_t const *)(ctx.tag_buf + u_io + 0x14u);
				if (u_addr >= ctx.tag_base && (u_addr - ctx.tag_base) + 0x38u <= ctx.tag_sz)
				{
					uint32_t u_off = u_addr - ctx.tag_base;
					uint32_t mod_ref_grp = *(uint32_t const *)(ctx.tag_buf + u_off + 0x28u);
					int32_t u_mod_handle = *(int32_t const *)(ctx.tag_buf + u_off + 0x34u);
					if ((mod_ref_grp == 'mod2' || mod_ref_grp == 'mode') && u_mod_handle != -1 && u_mod_handle != 0)
					{
						int32_t cand_idx = (int32_t)((uint32_t)u_mod_handle & 0xFFFFu);
						if (cand_idx >= 0 && cand_idx < ctx.tag_count)
							mod_idx = cand_idx;
					}
				}
			}
		}
	}
	else if (group_tag != 'actv' && obj_data_addr >= ctx.tag_base && (obj_data_addr - ctx.tag_base) + 0x38u <= ctx.tag_sz)
	{
		uint32_t mod_ref_grp;
		obj_off = obj_data_addr - ctx.tag_base;
		mod_ref_grp = *(uint32_t const *)(ctx.tag_buf + obj_off + 0x28u);
		mod_handle = *(int32_t const *)(ctx.tag_buf + obj_off + 0x34u);
		if ((mod_ref_grp == 'mod2' || mod_ref_grp == 'mode') && mod_handle != -1 && mod_handle != 0)
		{
			int32_t cand_idx = (int32_t)((uint32_t)mod_handle & 0xFFFFu);
			if (cand_idx >= 0 && cand_idx < ctx.tag_count)
				mod_idx = cand_idx;
		}
	}
	/* Fallback for 'actv' or composite objects whose model is in a child tag of the closure */
	if (mod_idx < 0 && closure)
	{
		int ci;
		for (ci = 0; ci < closure->count; ci++)
		{
			int32_t ti = closure->tag_indices[ci];
			uint32_t tio = ctx.inst_off + (uint32_t)ti * 32u;
			uint32_t tgrp = *(uint32_t const *)(ctx.tag_buf + tio + 0x00);
			if (tgrp == 'mod2' || tgrp == 'mode')
			{
				mod_idx = ti;
				break;
			}
		}
	}
	free(closure);
	if (mod_idx < 0 || mod_idx >= ctx.tag_count)
	{
		forge_donor_map_close(&ctx);
		return 0;
	}

	mod_io = ctx.inst_off + (uint32_t)mod_idx * 32u;
	mod_data_addr = *(uint32_t const *)(ctx.tag_buf + mod_io + 0x14);
	if (mod_data_addr < ctx.tag_base || (mod_data_addr - ctx.tag_base) + 0xE8u > ctx.tag_sz)
	{
		forge_donor_map_close(&ctx);
		return 0;
	}
	mod_off = mod_data_addr - ctx.tag_base;

	node_cnt = *(int32_t const *)(ctx.tag_buf + mod_off + 0xB8u);
	node_addr = *(uint32_t const *)(ctx.tag_buf + mod_off + 0xBCu);
	if (node_cnt > 0 && node_cnt <= 64 && node_addr >= ctx.tag_base &&
		(node_addr - ctx.tag_base) + (uint32_t)node_cnt * 156u <= ctx.tag_sz)
	{
		forge_preview_compute_node_mats(ctx.tag_buf + (node_addr - ctx.tag_base), node_cnt, node_mats);
	}
	else
	{
		node_cnt = 0;
	}

	reg_cnt = *(int32_t const *)(ctx.tag_buf + mod_off + 0xC4u);
	reg_addr = *(uint32_t const *)(ctx.tag_buf + mod_off + 0xC8u);
	geom_cnt = *(int32_t const *)(ctx.tag_buf + mod_off + 0xD0u);
	geom_addr = *(uint32_t const *)(ctx.tag_buf + mod_off + 0xD4u);

	if (geom_cnt <= 0 || geom_cnt > 256 || geom_addr < ctx.tag_base ||
		(uint64_t)(geom_addr - ctx.tag_base) + (uint64_t)geom_cnt * 48u > (uint64_t)ctx.tag_sz)
	{
		forge_donor_map_close(&ctx);
		return 0;
	}

	/* Load highest LOD geometry of each region (or geometry 0 if no regions) */
	{
	int32_t shaders_cnt = *(int32_t const *)(ctx.tag_buf + mod_off + 0xDCu);
	uint32_t shaders_addr = *(uint32_t const *)(ctx.tag_buf + mod_off + 0xE0u);
	float mod_u_scale = *(float const *)(ctx.tag_buf + mod_off + 0x30u);
	float mod_v_scale = *(float const *)(ctx.tag_buf + mod_off + 0x34u);
	if (!(mod_u_scale > 0.0001f && mod_u_scale < 1000.0f)) mod_u_scale = 1.0f;
	if (!(mod_v_scale > 0.0001f && mod_v_scale < 1000.0f)) mod_v_scale = 1.0f;

	for (r_i = 0; r_i < ((reg_cnt > 0 && reg_cnt <= 32) ? reg_cnt : 1); r_i++)
	{
		int32_t g_idx = 0;
		uint32_t g_off;
		int32_t part_cnt, p_idx;
		uint32_t parts_addr;
		uint32_t part_stride = ctx.is_xbox_v5 ? 104u : 132u;

		if (reg_cnt > 0 && reg_addr >= ctx.tag_base &&
			(uint64_t)(reg_addr - ctx.tag_base) + (uint64_t)reg_cnt * 76u <= (uint64_t)ctx.tag_sz)
		{
			uint32_t r_off = (reg_addr - ctx.tag_base) + (uint32_t)r_i * 76u;
			int32_t perm_cnt = *(int32_t const *)(ctx.tag_buf + r_off + 64u);
			uint32_t perm_addr = *(uint32_t const *)(ctx.tag_buf + r_off + 68u);
			if (perm_cnt > 0 && perm_addr >= ctx.tag_base &&
				(perm_addr - ctx.tag_base) + 88u <= ctx.tag_sz)
			{
				int lod_i;
				for (lod_i = 4; lod_i >= 0; lod_i--)
				{
					int16_t cand_g = *(int16_t const *)(ctx.tag_buf + (perm_addr - ctx.tag_base) + 64u + (uint32_t)lod_i * 2u);
					if (cand_g >= 0 && cand_g < geom_cnt)
					{
						g_idx = cand_g;
						break;
					}
				}
			}
		}

		g_off = (geom_addr - ctx.tag_base) + (uint32_t)g_idx * 48u;
		part_cnt = *(int32_t const *)(ctx.tag_buf + g_off + 36u);
		parts_addr = *(uint32_t const *)(ctx.tag_buf + g_off + 40u);
		if (part_cnt <= 0 || part_cnt > 256 || parts_addr < ctx.tag_base ||
			(uint64_t)(parts_addr - ctx.tag_base) + (uint64_t)part_cnt * part_stride > (uint64_t)ctx.tag_sz)
			continue;

		for (p_idx = 0; p_idx < part_cnt; p_idx++)
		{
			uint8_t const *sp = ctx.tag_buf + (parts_addr - ctx.tag_base) + (uint32_t)p_idx * part_stride;
			int16_t part_shader_idx = *(int16_t const *)(sp + 0x04u);
			float sh_u_scale = 1.0f, sh_v_scale = 1.0f;
			int16_t tex_idx = forge_preview_resolve_donor_part_shader(
				&ctx, shaders_cnt, shaders_addr, part_shader_idx, &sh_u_scale, &sh_v_scale);

			if (!ctx.is_xbox_v5)
			{
				int32_t tri_cnt = *(int32_t const *)(sp + 0x48u);
				uint32_t idx_off = *(uint32_t const *)(sp + 0x4Cu);
				int32_t vtx_cnt = *(int32_t const *)(sp + 0x58u);
				uint32_t vtx_off = *(uint32_t const *)(sp + 0x64u);
				if (vtx_cnt > 0 && vtx_cnt <= 32768 && tri_cnt > 0 && tri_cnt <= 32768)
				{
					size_t v_bytes = (size_t)vtx_cnt * 68u;
					size_t i_bytes = (size_t)(tri_cnt + 2) * 2u;
					uint8_t *v_buf = (uint8_t *)malloc(v_bytes);
					uint16_t *i_buf = (uint16_t *)malloc(i_bytes);
					if (v_buf && i_buf &&
						fseek(ctx.map_fp, (long)(ctx.vtx_file_off + vtx_off), SEEK_SET) == 0 &&
						fread(v_buf, 1, v_bytes, ctx.map_fp) == v_bytes &&
						fseek(ctx.map_fp, (long)(ctx.idx_file_off + idx_off), SEEK_SET) == 0 &&
						fread(i_buf, 1, i_bytes, ctx.map_fp) == i_bytes)
					{
						s_forge_preview.tag_size_bytes += (uint32_t)(v_bytes + i_bytes);
						{
							int has_local_nodes = ((*(uint32_t const *)(ctx.tag_buf + mod_off + 0x00) & 2u) != 0);
							forge_preview_append_part_68b(
								v_buf, vtx_cnt, i_buf, tri_cnt + 2,
								(node_cnt > 0) ? node_mats : NULL, node_cnt,
								has_local_nodes ? (sp + 108u) : NULL,
								has_local_nodes ? (int32_t)sp[107u] : 0,
								tex_idx,
								mod_u_scale * sh_u_scale,
								mod_v_scale * sh_v_scale);
						}
					}
					free(v_buf);
					free(i_buf);
				}
			}
			else
			{
				/* Xbox 104-byte model_geometry_part:
				 * triangle_buffer at +68 (count at +72, base_address at +76, hardware_format D3DIndexBuffer* at +80)
				 * vertex_buffer at +84 (type at +84, count at +88, offset at +92, base_address at +96, hardware_format D3DVertexBuffer* at +100)
				 * In Xbox v5 cache files, base_address (+76 / +96) is 0 on disk; hardware_format (+80 / +100) points
				 * to a 12-byte D3D header in the tag cache whose +4u field holds the actual data address. */
				int32_t tri_cnt = *(int32_t const *)(sp + 68u + 4u);
				uint32_t tri_addr = *(uint32_t const *)(sp + 68u + 8u);
				uint32_t tb_hw_addr = *(uint32_t const *)(sp + 68u + 12u);
				int16_t vtx_type = *(int16_t const *)(sp + 84u + 0u);
				int32_t vtx_cnt = *(int32_t const *)(sp + 84u + 4u);
				uint32_t vtx_addr = *(uint32_t const *)(sp + 84u + 12u);
				uint32_t vb_hw_addr = *(uint32_t const *)(sp + 84u + 16u);
				uint32_t v_stride = (vtx_type == _rasterizer_vertex_type_model_uncompressed) ? 68u : 32u;

				if ((tri_addr < ctx.tag_base || (tri_addr - ctx.tag_base) >= ctx.tag_sz) &&
					tb_hw_addr >= ctx.tag_base && (tb_hw_addr - ctx.tag_base) + 12u <= ctx.tag_sz)
				{
					tri_addr = *(uint32_t const *)(ctx.tag_buf + (tb_hw_addr - ctx.tag_base) + 4u);
				}
				if ((vtx_addr < ctx.tag_base || (vtx_addr - ctx.tag_base) >= ctx.tag_sz) &&
					vb_hw_addr >= ctx.tag_base && (vb_hw_addr - ctx.tag_base) + 12u <= ctx.tag_sz)
				{
					vtx_addr = *(uint32_t const *)(ctx.tag_buf + (vb_hw_addr - ctx.tag_base) + 4u);
				}

				if (vtx_cnt > 0 && vtx_cnt <= 32768 && tri_cnt > 0 && tri_cnt <= 32768 &&
					tri_addr >= ctx.tag_base && (tri_addr - ctx.tag_base) + (uint32_t)(tri_cnt + 2) * 2u <= ctx.tag_sz &&
					vtx_addr >= ctx.tag_base && (vtx_addr - ctx.tag_base) + (uint32_t)vtx_cnt * v_stride <= ctx.tag_sz)
				{
					uint8_t *v_buf = (uint8_t *)malloc((size_t)vtx_cnt * 68u);
					if (v_buf)
					{
						int32_t vi;
						if (v_stride == 68u)
						{
							memcpy(v_buf, ctx.tag_buf + (vtx_addr - ctx.tag_base), (size_t)vtx_cnt * 68u);
						}
						else
						{
							for (vi = 0; vi < vtx_cnt; vi++)
							{
								forge_decompress_xbox_vertex_to_68b(
									ctx.tag_buf + (vtx_addr - ctx.tag_base) + (uint32_t)vi * 32u,
									v_buf + (size_t)vi * 68u);
							}
						}
						s_forge_preview.tag_size_bytes += (uint32_t)vtx_cnt * v_stride + (uint32_t)(tri_cnt + 2) * 2u;
						forge_preview_append_part_68b(
							v_buf, vtx_cnt,
							(uint16_t const *)(ctx.tag_buf + (tri_addr - ctx.tag_base)),
							tri_cnt + 2,
							(node_cnt > 0) ? node_mats : NULL, node_cnt,
							NULL, 0,
							tex_idx,
							mod_u_scale * sh_u_scale,
							mod_v_scale * sh_v_scale);
						free(v_buf);
					}
				}
			}
		}
	}
	}

	forge_donor_map_close(&ctx);
	return (s_forge_preview.tri_count > 0) ? 1 : 0;
}

static void forge_preview_draw_line_2d(
	uint32_t *pix,
	float x0f, float y0f, float z0,
	float x1f, float y1f, float z1,
	uint32_t color,
	int depth_test)
{
	float dx = x1f - x0f;
	float dy = y1f - y0f;
	float adx = (dx >= 0.0f) ? dx : -dx;
	float ady = (dy >= 0.0f) ? dy : -dy;
	int steps = (int)((adx > ady) ? adx : ady);
	int s;

	if (steps <= 0) steps = 1;
	if (steps > 600) return;
	for (s = 0; s <= steps; s++)
	{
		float t = (float)s / (float)steps;
		int x = (int)(x0f + dx * t + 0.5f);
		int y = (int)(y0f + dy * t + 0.5f);
		float z = z0 + (z1 - z0) * t;
		if (x >= 0 && x < FORGE_PREVIEW_W && y >= 0 && y < FORGE_PREVIEW_H)
		{
			int idx = y * FORGE_PREVIEW_W + x;
			if (!depth_test || z >= s_forge_preview.zbuf[idx] - 0.02f)
				pix[idx] = color;
		}
	}
}

static void forge_preview_render_frame(unsigned long now_ms)
{
	uint32_t *pix;
	float angle, ca, sa, cp, sp, scale, scale_x;
	int i, g;

	if (!s_forge_preview.bitmap || !s_forge_preview.bitmap->base_address)
		return;
	pix = XBOX_POINTER(uint32_t, s_forge_preview.bitmap->base_address);

	/* Subtle dark two-tone gradient backdrop inside the 140x114 preview viewport */
	for (i = 0; i < FORGE_PREVIEW_W * FORGE_PREVIEW_H; i++)
	{
		int y = i / FORGE_PREVIEW_W;
		uint32_t r = (uint32_t)(5 + (y * 8) / FORGE_PREVIEW_H);
		uint32_t gr = (uint32_t)(12 + (y * 16) / FORGE_PREVIEW_H);
		uint32_t b = (uint32_t)(24 + (y * 28) / FORGE_PREVIEW_H);
		pix[i] = 0xFF000000u | (r << 16) | (gr << 8) | b;
		s_forge_preview.zbuf[i] = -1e9f;
	}

	if (s_forge_preview.snap_editor_active)
	{
		float pitch_rad = s_forge_preview.snap_cam_pitch_deg * (3.14159265f / 180.0f);
		float zm = (s_forge_preview.snap_cam_zoom > 0.2f) ? s_forge_preview.snap_cam_zoom : 1.0f;
		angle = s_forge_preview.snap_cam_yaw_deg * (3.14159265f / 180.0f);
		ca = (float)cos((double)angle);
		sa = (float)sin((double)angle);
		cp = (float)cos((double)pitch_rad);
		sp = (float)sin((double)pitch_rad);
		scale = (FORGE_PREVIEW_H * 0.41f * zm) / ((s_forge_preview.radius > 0.001f) ? s_forge_preview.radius : 1.0f);
	}
	else
	{
		angle = (float)(now_ms % 7000u) * (6.2831853f / 7000.0f) + 0.8f;
		ca = (float)cos((double)angle);
		sa = (float)sin((double)angle);
		cp = (float)cos(0.36);
		sp = (float)sin(0.36);
		scale = (FORGE_PREVIEW_H * 0.43f) / ((s_forge_preview.radius > 0.001f) ? s_forge_preview.radius : 1.0f);
	}
	/* Compensate for the 140x114 widget aspect ratio so the 256x256 square texture renders 1:1 unstretched */
	scale_x = scale * (114.0f / 140.0f);

	/* Render rotating ground grid under the normalized bounding sphere */
	for (g = -3; g <= 3; g++)
	{
		float d = (float)g * (s_forge_preview.radius * 0.32f);
		float ext = 3.0f * (s_forge_preview.radius * 0.32f);
		float gz = -s_forge_preview.radius * 0.78f;
		uint32_t gcol = (g == 0) ? 0xFF1E5C9Eu : 0xFF12365Eu;
		int axis;
		for (axis = 0; axis < 2; axis++)
		{
			float x0 = (axis == 0) ? -ext : d;
			float y0 = (axis == 0) ? d : -ext;
			float x1 = (axis == 0) ? ext : d;
			float y1 = (axis == 0) ? d : ext;
			float tw0 = x0 * ca - y0 * sa;
			float rt0 = x0 * sa + y0 * ca;
			float up0 = gz * cp - tw0 * sp;
			float dp0 = tw0 * cp + gz * sp;
			float tw1 = x1 * ca - y1 * sa;
			float rt1 = x1 * sa + y1 * ca;
			float up1 = gz * cp - tw1 * sp;
			float dp1 = tw1 * cp + gz * sp;
			forge_preview_draw_line_2d(
				pix,
				FORGE_PREVIEW_W * 0.5f + rt0 * scale_x,
				FORGE_PREVIEW_H * 0.54f - up0 * scale,
				dp0,
				FORGE_PREVIEW_W * 0.5f + rt1 * scale_x,
				FORGE_PREVIEW_H * 0.54f - up1 * scale,
				dp1,
				gcol,
				0);
		}
	}

	/* Project vertices and compute two-tone directional + rim lighting */
	for (i = 0; i < s_forge_preview.vert_count; i++)
	{
		float x = s_forge_preview.verts[i][0] - s_forge_preview.center[0];
		float y = s_forge_preview.verts[i][1] - s_forge_preview.center[1];
		float z = s_forge_preview.verts[i][2] - s_forge_preview.center[2];
		float nx = s_forge_preview.norms[i][0];
		float ny = s_forge_preview.norms[i][1];
		float nz = s_forge_preview.norms[i][2];
		float toward = x * ca - y * sa;
		float right = x * sa + y * ca;
		float up = z * cp - toward * sp;
		float n_tw = nx * ca - ny * sa;
		float n_rt = nx * sa + ny * ca;
		float n_up = nz * cp - n_tw * sp;
		float n_len;

		n_tw = n_tw * cp + nz * sp;
		s_forge_preview.proj[i][0] = FORGE_PREVIEW_W * 0.5f + right * scale_x;
		s_forge_preview.proj[i][1] = FORGE_PREVIEW_H * 0.52f - up * scale;
		s_forge_preview.proj[i][2] = toward * cp + z * sp;

		n_len = (float)sqrt((double)(n_rt * n_rt + n_up * n_up + n_tw * n_tw));
		if (n_len > 0.001f)
		{
			float dot_l, rim;
			if (n_tw < 0.0f) n_len = -n_len;
			n_rt /= n_len; n_up /= n_len; n_tw /= n_len;
			dot_l = n_rt * (-0.42f) + n_up * 0.56f + n_tw * 0.71f;
			if (dot_l < 0.0f) dot_l = 0.0f;
			rim = 1.0f - n_tw;
			if (rim < 0.0f) rim = 0.0f;
			s_forge_preview.shade[i][0] = 0.30f + 0.70f * dot_l;
			s_forge_preview.shade[i][1] = rim * rim * 0.65f;
		}
		else
		{
			s_forge_preview.shade[i][0] = 0.65f;
			s_forge_preview.shade[i][1] = 0.15f;
		}
	}

	/* Rasterize triangles (diffuse bitmap textured in ADD TAGS / INSTALLED TAGS, or two-tone shaded in SNAP POINTS) */
	for (i = 0; i < s_forge_preview.tri_count; i++)
	{
		uint16_t i0 = s_forge_preview.tris[i].v0;
		uint16_t i1 = s_forge_preview.tris[i].v1;
		uint16_t i2 = s_forge_preview.tris[i].v2;
		int16_t t_id = s_forge_preview.tris[i].tex_idx;
		struct forge_preview_texture const *tex =
			(s_forge_preview.use_textures && t_id >= 0 && t_id < s_forge_preview.tex_count &&
			 s_forge_preview.textures[t_id].pixels != NULL &&
			 s_forge_preview.textures[t_id].width > 0 && s_forge_preview.textures[t_id].height > 0)
			? &s_forge_preview.textures[t_id]
			: NULL;
		float const *a = s_forge_preview.proj[i0];
		float const *b = s_forge_preview.proj[i1];
		float const *c = s_forge_preview.proj[i2];
		float u0 = s_forge_preview.uvs[i0][0], v0 = s_forge_preview.uvs[i0][1];
		float u1 = s_forge_preview.uvs[i1][0], v1 = s_forge_preview.uvs[i1][1];
		float u2 = s_forge_preview.uvs[i2][0], v2 = s_forge_preview.uvs[i2][1];
		float area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
		int min_x, max_x, min_y, max_y, x, y;
		float inv_area;

		if (area > -0.05f && area < 0.05f)
			continue;
		inv_area = 1.0f / area;

		min_x = (int)floor(MIN(a[0], MIN(b[0], c[0])));
		max_x = (int)ceil(MAX(a[0], MAX(b[0], c[0])));
		min_y = (int)floor(MIN(a[1], MIN(b[1], c[1])));
		max_y = (int)ceil(MAX(a[1], MAX(b[1], c[1])));
		if (min_x < 0) min_x = 0;
		if (max_x >= FORGE_PREVIEW_W) max_x = FORGE_PREVIEW_W - 1;
		if (min_y < 0) min_y = 0;
		if (max_y >= FORGE_PREVIEW_H) max_y = FORGE_PREVIEW_H - 1;

		for (y = min_y; y <= max_y; y++)
		{
			float py = (float)y + 0.5f;
			for (x = min_x; x <= max_x; x++)
			{
				float px = (float)x + 0.5f;
				float w0 = ((b[0] - px) * (c[1] - py) - (b[1] - py) * (c[0] - px)) * inv_area;
				float w1 = ((c[0] - px) * (a[1] - py) - (c[1] - py) * (a[0] - px)) * inv_area;
				float w2 = 1.0f - w0 - w1;
				int tidx;
				float z, lit, rim;
				uint32_t cr, cg, cb;

				if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
					continue;
				tidx = y * FORGE_PREVIEW_W + x;
				z = w0 * a[2] + w1 * b[2] + w2 * c[2];
				if (z <= s_forge_preview.zbuf[tidx])
					continue;

				lit = w0 * s_forge_preview.shade[i0][0] + w1 * s_forge_preview.shade[i1][0] + w2 * s_forge_preview.shade[i2][0];
				rim = w0 * s_forge_preview.shade[i0][1] + w1 * s_forge_preview.shade[i1][1] + w2 * s_forge_preview.shade[i2][1];

				if (tex)
				{
					float u = w0 * u0 + w1 * u1 + w2 * u2;
					float v = w0 * v0 + w1 * v1 + w2 * v2;
					int tx, ty;
					uint32_t texel;
					float shade_gain;

					u -= (float)floor((double)u);
					v -= (float)floor((double)v);
					tx = (int)(u * (float)tex->width);
					ty = (int)(v * (float)tex->height);
					if (tx < 0) tx = 0;
					else if (tx >= (int)tex->width) tx = (int)tex->width - 1;
					if (ty < 0) ty = 0;
					else if (ty >= (int)tex->height) ty = (int)tex->height - 1;

					texel = tex->pixels[ty * (int)tex->width + tx];
					if (tex->alpha_test && ((texel >> 24) & 0xFFu) < 32u)
						continue;

					s_forge_preview.zbuf[tidx] = z;
					shade_gain = 0.28f + 1.15f * lit;
					cr = (uint32_t)PIN((float)((texel >> 16) & 0xFFu) * shade_gain + 16.0f * rim, 0.0f, 255.0f);
					cg = (uint32_t)PIN((float)((texel >> 8)  & 0xFFu) * shade_gain + 26.0f * rim, 0.0f, 255.0f);
					cb = (uint32_t)PIN((float)( texel        & 0xFFu) * shade_gain + 40.0f * rim, 0.0f, 255.0f);
					pix[tidx] = 0xFF000000u | (cr << 16) | (cg << 8) | cb;
				}
				else
				{
					float edge_dist = MIN(w0, MIN(w1, w2));
					s_forge_preview.zbuf[tidx] = z;
					if (edge_dist < 0.045f)
						rim += 0.28f;
					cr = (uint32_t)PIN(115.0f * lit + 40.0f * rim, 0.0f, 255.0f);
					cg = (uint32_t)PIN(175.0f * lit + 150.0f * rim, 0.0f, 255.0f);
					cb = (uint32_t)PIN(235.0f * lit + 255.0f * rim, 0.0f, 255.0f);
					pix[tidx] = 0xFF000000u | (cr << 16) | (cg << 8) | cb;
				}
			}
		}
	}

	if (s_forge_preview.vert_count <= 16)
	{
		for (i = 0; i < s_forge_preview.tri_count; i++)
		{
			uint16_t i0 = s_forge_preview.tris[i].v0;
			uint16_t i1 = s_forge_preview.tris[i].v1;
			uint16_t i2 = s_forge_preview.tris[i].v2;
			float const *a = s_forge_preview.proj[i0];
			float const *b = s_forge_preview.proj[i1];
			float const *c = s_forge_preview.proj[i2];
			forge_preview_draw_line_2d(pix, a[0], a[1], a[2], b[0], b[1], b[2], 0xFF2896FFu, 1);
			forge_preview_draw_line_2d(pix, b[0], b[1], b[2], c[0], c[1], c[2], 0xFF2896FFu, 1);
			forge_preview_draw_line_2d(pix, c[0], c[1], c[2], a[0], a[1], a[2], 0xFF2896FFu, 1);
		}
	}

	/* Render 3D Bounding Box Wireframe & Snap Point Markers + Outward Normal Vectors */
	{
		struct forge_tag_snap_config const *snap_cfg = NULL;
		if (s_forge_preview.current_tag_path[0])
		{
			snap_cfg = forge_snap_points_get_for_tag(
				s_forge_preview.current_group_tag,
				s_forge_preview.current_tag_path,
				0);
		}

		if (s_forge_preview.snap_editor_active)
		{
			static int const s_box_edges[12][2] = {
				{ 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 },
				{ 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
			};
			float bc[8][3];
			int ci, ei;
			for (ci = 0; ci < 8; ci++)
			{
				float bx = ((ci & 1) ? s_forge_preview.bbox_max[0] : s_forge_preview.bbox_min[0]) - s_forge_preview.center[0];
				float by = ((ci & 2) ? s_forge_preview.bbox_max[1] : s_forge_preview.bbox_min[1]) - s_forge_preview.center[1];
				float bz = ((ci & 4) ? s_forge_preview.bbox_max[2] : s_forge_preview.bbox_min[2]) - s_forge_preview.center[2];
				float tw = bx * ca - by * sa;
				float rt = bx * sa + by * ca;
				float up = bz * cp - tw * sp;
				bc[ci][0] = FORGE_PREVIEW_W * 0.5f + rt * scale_x;
				bc[ci][1] = FORGE_PREVIEW_H * 0.52f - up * scale;
				bc[ci][2] = tw * cp + bz * sp;
			}
			for (ei = 0; ei < 12; ei++)
			{
				int a = s_box_edges[ei][0];
				int b = s_box_edges[ei][1];
				forge_preview_draw_line_2d(
					pix,
					bc[a][0], bc[a][1], bc[a][2],
					bc[b][0], bc[b][1], bc[b][2],
					0xFF1E5688u,
					0);
			}
		}

		if (!s_forge_preview.use_textures && snap_cfg && snap_cfg->point_count > 0)
		{
			int p;
			float n_len = s_forge_preview.radius * 0.34f;
			if (n_len < 0.08f) n_len = 0.08f;
			for (p = 0; p < snap_cfg->point_count && p < FORGE_MAX_SNAP_POINTS_PER_TAG; p++)
			{
				struct forge_snap_point const *pt = &snap_cfg->points[p];
				int is_sel = (s_forge_preview.snap_editor_active && p == s_forge_preview.snap_active_pt_idx);
				float px = pt->x - s_forge_preview.center[0];
				float py = pt->y - s_forge_preview.center[1];
				float pz = pt->z - s_forge_preview.center[2];
				float ex = px + pt->nx * n_len;
				float ey = py + pt->ny * n_len;
				float ez = pz + pt->nz * n_len;
				float tw0 = px * ca - py * sa;
				float rt0 = px * sa + py * ca;
				float up0 = pz * cp - tw0 * sp;
				float dp0 = tw0 * cp + pz * sp;
				float tw1 = ex * ca - ey * sa;
				float rt1 = ex * sa + ey * ca;
				float up1 = ez * cp - tw1 * sp;
				float dp1 = tw1 * cp + ez * sp;
				float sx0 = FORGE_PREVIEW_W * 0.5f + rt0 * scale_x;
				float sy0 = FORGE_PREVIEW_H * 0.52f - up0 * scale;
				float sx1 = FORGE_PREVIEW_W * 0.5f + rt1 * scale_x;
				float sy1 = FORGE_PREVIEW_H * 0.52f - up1 * scale;
				uint32_t pt_col = is_sel ? 0xFFFFE838u : 0xFF2896FFu;
				uint32_t ln_col = is_sel ? 0xFF40FF60u : 0xFF60D8FFu;
				int rad = is_sel ? 6 : 4;
				int dy_i, dx_i;

				/* Draw outward normal vector line */
				forge_preview_draw_line_2d(pix, sx0, sy0, dp0, sx1, sy1, dp1, ln_col, 0);
				if (is_sel)
				{
					forge_preview_draw_line_2d(pix, sx0 + 1.0f, sy0, dp0, sx1 + 1.0f, sy1, dp1, ln_col, 0);
					forge_preview_draw_line_2d(pix, sx0, sy0 + 1.0f, dp0, sx1, sy1 + 1.0f, dp1, ln_col, 0);
				}

				/* Draw normal tip dot */
				for (dy_i = -2; dy_i <= 2; dy_i++)
				{
					for (dx_i = -2; dx_i <= 2; dx_i++)
					{
						int tx = (int)(sx1 + 0.5f) + dx_i;
						int ty = (int)(sy1 + 0.5f) + dy_i;
						if (tx >= 0 && tx < FORGE_PREVIEW_W && ty >= 0 && ty < FORGE_PREVIEW_H)
							pix[ty * FORGE_PREVIEW_W + tx] = ln_col;
					}
				}

				/* Draw snap point diamond marker */
				for (dy_i = -rad; dy_i <= rad; dy_i++)
				{
					int a_dy = (dy_i >= 0) ? dy_i : -dy_i;
					for (dx_i = -rad; dx_i <= rad; dx_i++)
					{
						int a_dx = (dx_i >= 0) ? dx_i : -dx_i;
						if (a_dx + a_dy <= rad)
						{
							int mx = (int)(sx0 + 0.5f) + dx_i;
							int my = (int)(sy0 + 0.5f) + dy_i;
							if (mx >= 0 && mx < FORGE_PREVIEW_W && my >= 0 && my < FORGE_PREVIEW_H)
							{
								pix[my * FORGE_PREVIEW_W + mx] = (a_dx + a_dy == rad) ? 0xFFFFFFFFu : pt_col;
							}
						}
					}
				}
			}
		}
	}

	bitmap_rebuild(s_forge_preview.bitmap);
	rasterizer_bitmap_changed(s_forge_preview.bitmap);
	s_forge_preview.last_rendered_use_textures = s_forge_preview.use_textures;
	s_forge_preview.last_draw_ms = now_ms;
}

void forge_tag_preview_clear(void)
{
	s_forge_preview.active = 0;
	s_forge_preview.key[0] = '\0';
	s_forge_preview.current_group_tag = 0;
	s_forge_preview.current_tag_path[0] = '\0';
	s_forge_preview.snap_editor_active = 0;
	s_forge_preview.tag_size_bytes = 0;
}

uint32_t forge_tag_preview_get_current_size_bytes(void)
{
	return s_forge_preview.tag_size_bytes;
}

void forge_tag_preview_set_map_tag(char const *map_path, uint32_t group_tag, char const *tag_path)
{
	char new_key[512];
	if (!map_path || !map_path[0] || !tag_path || !tag_path[0])
	{
		forge_tag_preview_clear();
		return;
	}
	if (!forge_preview_ensure_buffers())
		return;
	snprintf(new_key, sizeof(new_key), "MAP|%s|%08X|%s", map_path, (unsigned int)group_tag, tag_path);
	s_forge_preview.active = 1;
	s_forge_preview.use_textures = 1;
	s_forge_preview.current_group_tag = group_tag;
	snprintf(s_forge_preview.current_tag_path, sizeof(s_forge_preview.current_tag_path), "%s", tag_path);
	if (strcmp(s_forge_preview.key, new_key) == 0)
		return;
	snprintf(s_forge_preview.key, sizeof(s_forge_preview.key), "%s", new_key);
	s_forge_preview.tag_size_bytes = 0;
	forge_preview_reset_mesh();
	if (!forge_preview_load_from_donor_map(map_path, group_tag, tag_path))
		forge_preview_build_fallback_octahedron();
	forge_preview_normalize_mesh();
	s_forge_preview.last_draw_ms = 0;
}

void forge_tag_preview_set_installed_tag(uint32_t group_tag, char const *tag_path, int is_custom)
{
	char new_key[512];
	char safe_stem[128];
	int loaded = 0;

	if (!tag_path || !tag_path[0])
	{
		forge_tag_preview_clear();
		return;
	}
	if (!forge_preview_ensure_buffers())
		return;
	snprintf(new_key, sizeof(new_key), "INST|%d|%08X|%s", is_custom, (unsigned int)group_tag, tag_path);
	s_forge_preview.active = 1;
	s_forge_preview.use_textures = 1;
	s_forge_preview.current_group_tag = group_tag;
	snprintf(s_forge_preview.current_tag_path, sizeof(s_forge_preview.current_tag_path), "%s", tag_path);
	if (strcmp(s_forge_preview.key, new_key) == 0)
		return;
	snprintf(s_forge_preview.key, sizeof(s_forge_preview.key), "%s", new_key);
	s_forge_preview.tag_size_bytes = 0;
	forge_preview_reset_mesh();

	/* 1. Try direct <safe_stem>.ocetags or any matching .ocetags in custom_tags/ or cached_tags/ */
	forge_make_safe_ocetags_stem(group_tag, tag_path, safe_stem, sizeof(safe_stem));
	{
		static char const *const s_dirs[] = {
			"OpenCE Forge/custom_tags",
			"OpenCE Forge/cached_tags",
			"Faber_data/custom_tags",
			"Faber_data/cached_tags",
			"custom_tags"
		};
		int d;
		for (d = 0; d < 5 && !loaded; d++)
		{
			char cand[512];
			FILE *fp;
			snprintf(cand, sizeof(cand), "%s/%s.ocetags", s_dirs[d], safe_stem);
			fp = fopen(cand, "rb");
			if (fp)
			{
				size_t ulen = 0;
				int was_c = 0;
				uint8_t *ubuf;
				fseek(fp, 0, SEEK_END);
				s_forge_preview.tag_size_bytes = (uint32_t)ftell(fp);
				fclose(fp);
				ubuf = forge_read_and_decompress_ocetags_file(cand, &ulen, &was_c);
				if (ubuf && ulen > 32)
					loaded = forge_preview_load_from_ocetags_json((char const *)ubuf, ulen);
				free(ubuf);
			}
		}
#ifdef _WIN32
		for (d = 0; d < 5 && !loaded; d++)
		{
			char pattern[512];
			WIN32_FIND_DATAA fd;
			HANDLE h;
			snprintf(pattern, sizeof(pattern), "%s/*.ocetags", s_dirs[d]);
			h = FindFirstFileA(pattern, &fd);
			if (h != INVALID_HANDLE_VALUE)
			{
				do
				{
					if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					{
						char full[512];
						size_t ulen = 0;
						int was_c = 0;
						uint8_t *ubuf;
						snprintf(full, sizeof(full), "%s/%s", s_dirs[d], fd.cFileName);
						ubuf = forge_read_and_decompress_ocetags_file(full, &ulen, &was_c);
						if (ubuf && ulen > 32)
						{
							size_t t_pos = 0;
							if (ocetags_json_find_key_range((char const *)ubuf, 0, ulen, "tags", &t_pos))
							{
								size_t p = t_pos + 1;
								while (p < ulen && ubuf[p] != '{') p++;
								if (p < ulen)
								{
									char gs[64], ps[256];
									gs[0] = 0; ps[0] = 0;
									ocetags_json_get_str((char const *)ubuf, p, ulen, "group", gs, sizeof(gs));
									ocetags_json_get_str((char const *)ubuf, p, ulen, "path", ps, sizeof(ps));
									if (ocetags_group_str_to_fourcc(gs) == group_tag && _stricmp(ps, tag_path) == 0)
									{
										s_forge_preview.tag_size_bytes = (uint32_t)fd.nFileSizeLow;
										loaded = forge_preview_load_from_ocetags_json((char const *)ubuf, ulen);
									}
								}
							}
						}
						free(ubuf);
					}
				} while (!loaded && FindNextFileA(h, &fd));
				CloseHandle(h);
			}
		}
#endif
	}

	/* 2. If stock cached tag from Xbox donor maps, check credits.md donor stems & z:\cache000..005.map first,
	 *    then fall back to scanning maps/<name>.map */
	if (!loaded)
	{
		char candidate_stems[8][64];
		int num_stems = find_donor_map_stems_from_credits(group_tag, tag_path, candidate_stems, 8);
		int si;
		for (si = 0; si < num_stems && !loaded; si++)
		{
			char mpath[512];
			snprintf(mpath, sizeof(mpath), "maps/%s.map", candidate_stems[si]);
			if (forge_preview_load_from_donor_map(mpath, group_tag, tag_path))
				loaded = 1;
		}
		for (si = 0; si <= 5 && !loaded; si++)
		{
			char zpath[64];
			snprintf(zpath, sizeof(zpath), "z:\\cache00%d.map", si);
			if (forge_preview_load_from_donor_map(zpath, group_tag, tag_path))
				loaded = 1;
		}
	}
#ifdef _WIN32
	if (!loaded)
	{
		WIN32_FIND_DATAA fd;
		HANDLE h = FindFirstFileA("maps\\*.map", &fd);
		if (h != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					char mpath[512];
					if (_stricmp(fd.cFileName, "bitmaps.map") == 0 ||
						_stricmp(fd.cFileName, "sounds.map") == 0 ||
						_stricmp(fd.cFileName, "loc.map") == 0 ||
						_stricmp(fd.cFileName, "ui.map") == 0)
						continue;
					snprintf(mpath, sizeof(mpath), "maps/%s", fd.cFileName);
					if (forge_preview_load_from_donor_map(mpath, group_tag, tag_path))
					{
						loaded = 1;
						break;
					}
				}
			} while (FindNextFileA(h, &fd));
			CloseHandle(h);
		}
	}
#endif

	if (!loaded)
		forge_preview_build_fallback_octahedron();
	forge_preview_normalize_mesh();
	s_forge_preview.last_draw_ms = 0;
}

struct bitmap_data *forge_tag_preview_get_bitmap(void)
{
	unsigned long now_ms;
	if (!s_forge_preview.active || !s_forge_preview.bitmap)
		return NULL;
	now_ms = system_milliseconds();
	if (s_forge_preview.last_draw_ms == 0 ||
		s_forge_preview.last_rendered_use_textures != s_forge_preview.use_textures ||
		now_ms - s_forge_preview.last_draw_ms >= 30u)
		forge_preview_render_frame(now_ms);
	return s_forge_preview.bitmap;
}



