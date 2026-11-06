/*
FIREFIGHT_HUD.C

Halo 1 CE-styled Firefight HUD module:
- Firefight Status Panel (Total Session Game Time Counter MM:SS / H:MM:SS,
  Shared Lives Pool, Set / Round / Wave, Enemies Remaining, Next Wave Timer,
  Enemy Health/Shield Escalation %)
- Pre-Round Preparation Countdown Banner ("ROUND X STARTS IN XXs — RESUPPLY & BOARD VEHICLES")
- Animated Wave / Boss / Skull / Ordnance Announcement Banners
- Generator Defense Alpha / Beta / Gamma Shield & Health Bars + Lockdown status
- 3D->2D World-to-Screen Navpoints for Stragglers (<= 3 enemies) & Ordnance Drops
- Scoreboard & Post-Game Carnage Report header formatters
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries/cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "interface/hud_messaging.h"
#include "interface/interface.h"
#include "math/real_math.h"
#include "objects/objects.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "render/render_camera_projection.h"
#include "render/render_cameras.h"
#include "scenario/scenario.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/unicode.h"
#include "units/units.h"

#include "halo_firefight.h"
#include "halo_skulls.h"
#include "campaign_score.h"
#include "campaign_medals.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "interface/ui_widget.h"
#include "rasterizer/rasterizer_swizzle.h"
#include <xtl.h>
#include "rasterizer/xbox/rasterizer_xbox.h"
#include "mcc_rank_icons_data.h"

unsigned long system_milliseconds(void);
void draw_quad(rectangle2d const *bounds, pixel32 color);
struct bitmap_data *bitmap_group_try_and_get_bitmap(long bitmap_group_index, short bitmap_index);
short find_nav_point(char const *name);
short hud_get_nav_point_render_type(short local_player_index, real_point3d const *head, real_point3d const *position, long reference_object_index);
void custom_render_nav_point(short local_player_index, real_point3d const *position_pointer, short nav_index, short waypoint_type);
void render_interpolation_object_point(long object_index, real_point3d *point);

/* ---------- constants */

/* Stock Halo CE pause/objective box palette (matches pause_box_svg in tools/port_settings.py) */
#define FF_HUD_PANEL_FILL  0xBF0A2649
#define FF_HUD_PANEL_BOT   0xBF050D1C
#define FF_HUD_HEADER_FILL 0xBF000000
#define FF_HUD_PANEL_EDGE  0xFF2896FF
#define FF_HUD_WARN_FILL   0xBF481010
#define FF_HUD_WARN_BOT    0xBF220606
#define FF_HUD_WARN_EDGE   0xFFFF4444
#define FF_HUD_BAR_BG      0xBF050D1C
#define FF_HUD_SHIELD_BAR  0xFF2896FF
#define FF_HUD_HEALTH_BAR  0xFF40E070
#define FF_HUD_LOCK_BAR    0xFFFFCC33

enum
{
	_ff_text_left = 0,
	_ff_text_right = 1,
	_ff_text_center = 2
};

/* ---------- helpers */

static pixel32 ff_lerp_argb(pixel32 c0, pixel32 c1, real t)
{
	long a0 = (long)((c0 >> 24) & 0xFF);
	long r0 = (long)((c0 >> 16) & 0xFF);
	long g0 = (long)((c0 >> 8) & 0xFF);
	long b0 = (long)(c0 & 0xFF);
	long a1 = (long)((c1 >> 24) & 0xFF);
	long r1 = (long)((c1 >> 16) & 0xFF);
	long g1 = (long)((c1 >> 8) & 0xFF);
	long b1 = (long)(c1 & 0xFF);
	unsigned long a = (unsigned long)PIN((long)(a0 + (a1 - a0) * t + 0.5f), 0L, 255L);
	unsigned long r = (unsigned long)PIN((long)(r0 + (r1 - r0) * t + 0.5f), 0L, 255L);
	unsigned long g = (unsigned long)PIN((long)(g0 + (g1 - g0) * t + 0.5f), 0L, 255L);
	unsigned long b = (unsigned long)PIN((long)(b0 + (b1 - b0) * t + 0.5f), 0L, 255L);

	return (pixel32)((a << 24) | (r << 16) | (g << 8) | b);
}

static void ff_draw_bitmap_uv(
	struct bitmap_data *bitmap,
	short x0,
	short y0,
	short x1,
	short y1,
	real u0,
	real v0,
	real u1,
	real v1,
	pixel32 tint_argb)
{
	struct dynamic_screen_vertex vertices[4];
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	int v;

	if (!bitmap || !bitmap->hardware_format || x1 <= x0 || y1 <= y0 || ((tint_argb >> 24) & 0xFFUL) == 0)
		return;

	for (v = 0; v < 4; v++)
	{
		vertices[v].position.x = (v % 3) ? (real)x1 : (real)x0;
		vertices[v].position.y = (v > 1) ? (real)y1 : (real)y0;
		vertices[v].texture_coordinates.x = (v % 3) ? u1 : u0;
		vertices[v].texture_coordinates.y = (v > 1) ? v1 : v0;
		vertices[v].color = tint_argb;
	}

	csmemset(&parameters, 0, sizeof(parameters));
	parameters.meter_parameters = NULL;
	parameters.map_offset[0] = NULL;
	parameters.map_wrapped[0] = FALSE;
	parameters.map_wrapped[1] = FALSE;
	parameters.map_wrapped[2] = FALSE;
	parameters.map_scale[0].i = 1.0f;
	parameters.map_scale[0].j = 1.0f;
	parameters.map_texture_scale[0].i = 1.0f;
	parameters.map_texture_scale[0].j = 1.0f;
	parameters.point_sampled = FALSE;
	parameters.framebuffer_blend_function = 0;
	parameters.map[0] = bitmap;
	parameters.map[1] = NULL;
	parameters.map[2] = NULL;

	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
}

static boolean ff_draw_ce_menu_bitmap_uv(
	char const *tag_name,
	short frame_idx,
	short x0,
	short y0,
	short x1,
	short y1,
	real u0,
	real v0,
	real u1,
	real v1,
	pixel32 tint_argb)
{
	long tag_idx;
	struct bitmap_data *bmp;

	if (!tag_name || !tag_name[0] || x1 <= x0 || y1 <= y0)
		return FALSE;

	tag_idx = tag_loaded(BITMAP_GROUP_TAG, tag_name);
	if (tag_idx == NONE)
		return FALSE;

	bmp = bitmap_group_get_bitmap_from_sequence(tag_idx, 0, frame_idx);
	if (!bmp && frame_idx != 0)
		bmp = bitmap_group_get_bitmap_from_sequence(tag_idx, 0, 0);
	if (!bmp || !bmp->hardware_format || bmp->width <= 0 || bmp->height <= 0)
		return FALSE;

	ff_draw_bitmap_uv(bmp, x0, y0, x1, y1, u0, v0, u1, v1, tint_argb);
	return TRUE;
}

/* Draws a 3-slice horizontally rounded Halo CE option/row pill bar using pc\bitmaps\option_bkds_sm
 * (exact same rounded bar style used in the Forge menu). */
boolean firefight_draw_ce_rounded_pill(
	short px0,
	short py0,
	short px1,
	short py1,
	short frame_idx,
	real scale,
	pixel32 tint_argb)
{
	short w = (short)(px1 - px0);
	short h = (short)(py1 - py0);
	short cap = (short)MIN(w / 2, MAX(4, (int)(12.0f * scale + 0.5f)));
	real v1 = 28.0f / 32.0f;
	real u_cap = 16.0f / 256.0f;

	if (w <= 2 || h <= 2 || ((tint_argb >> 24) & 0xFFUL) == 0)
		return FALSE;

	if (w <= cap * 2)
	{
		return ff_draw_ce_menu_bitmap_uv(
			"pc\\bitmaps\\option_bkds_sm", frame_idx,
			px0, py0, px1, py1,
			0.0f, 0.0f, 1.0f, v1, tint_argb);
	}

	if (ff_draw_ce_menu_bitmap_uv("pc\\bitmaps\\option_bkds_sm", frame_idx, px0, py0, (short)(px0 + cap), py1, 0.0f, 0.0f, u_cap, v1, tint_argb) &&
		ff_draw_ce_menu_bitmap_uv("pc\\bitmaps\\option_bkds_sm", frame_idx, (short)(px0 + cap), py0, (short)(px1 - cap), py1, u_cap, 0.0f, 1.0f - u_cap, v1, tint_argb) &&
		ff_draw_ce_menu_bitmap_uv("pc\\bitmaps\\option_bkds_sm", frame_idx, (short)(px1 - cap), py0, px1, py1, 1.0f - u_cap, 0.0f, 1.0f, v1, tint_argb))
	{
		return TRUE;
	}
	return FALSE;
}

/* Draws the authentic 9-slice Halo CE pause/objectives container box with rounded blue corners
 * (exact same rounded panel style used in the Forge menu). */
void firefight_draw_ce_rounded_box(
	short px0,
	short py0,
	short px1,
	short py1,
	short header_px,
	real scale,
	pixel32 tint_argb,
	pixel32 fallback_fill,
	pixel32 fallback_edge)
{
	short w = (short)(px1 - px0);
	short h = (short)(py1 - py0);
	boolean with_header = (header_px > 0 && header_px < h - 4);
	char const *top_left   = with_header ? "pc\\pause\\objectives_left"   : "pc\\pause\\pausebox_left";
	char const *top_center = with_header ? "pc\\pause\\objectives_center" : "pc\\pause\\pausebox_center";
	char const *top_right  = with_header ? "pc\\pause\\objectives_right"  : "pc\\pause\\pausebox_right";
	char const *obj_left   = "pc\\pause\\objectives_left";
	char const *obj_center = "pc\\pause\\objectives_center";
	char const *obj_right  = "pc\\pause\\objectives_right";
	short frame_idx = 1;
	short cap_w = (short)MIN(w / 3, MAX(6, (int)(14.0f * scale + 0.5f)));
	short top_h = with_header
		? (short)PIN(header_px, (short)8, (short)(h - 6))
		: (short)MIN(h / 2, MAX(4, (int)(11.0f * scale + 0.5f)));
	short bot_h = (short)MIN(h - top_h, MAX(4, (int)(11.0f * scale + 0.5f)));
	real v_top_end = with_header ? (30.0f / 256.0f) : (14.0f / 256.0f);
	real v_body_start = 32.0f / 256.0f;
	real v_body_end = 211.0f / 256.0f;
	real v_bot_end = 225.0f / 256.0f;

	if (w <= 6 || h <= 6 || ((tint_argb >> 24) & 0xFFUL) == 0)
		return;

	/* Compact single-row bars without headers use the 3-slice Halo CE rounded pill (option_bkds_sm) */
	if (!with_header && h <= (short)(20.0f * scale + 0.5f))
	{
		if (firefight_draw_ce_rounded_pill(px0, py0, px1, py1, 0, scale, tint_argb))
			return;
	}

	if (w > cap_w * 2 && top_h > 0 && bot_h > 0)
	{
		boolean ok =
			ff_draw_ce_menu_bitmap_uv(top_left, frame_idx, px0, py0, (short)(px0 + cap_w), (short)(py0 + top_h), 0.0f, 0.0f, 1.0f, v_top_end, tint_argb) &&
			ff_draw_ce_menu_bitmap_uv(top_center, frame_idx, (short)(px0 + cap_w), py0, (short)(px1 - cap_w), (short)(py0 + top_h), 0.0f, 0.0f, 1.0f, v_top_end, tint_argb) &&
			ff_draw_ce_menu_bitmap_uv(top_right, frame_idx, (short)(px1 - cap_w), py0, px1, (short)(py0 + top_h), 0.0f, 0.0f, 1.0f, v_top_end, tint_argb);

		if (ok && (py1 - bot_h) > (py0 + top_h))
		{
			ok =
				ff_draw_ce_menu_bitmap_uv(obj_left, frame_idx, px0, (short)(py0 + top_h), (short)(px0 + cap_w), (short)(py1 - bot_h), 0.0f, v_body_start, 1.0f, v_body_end, tint_argb) &&
				ff_draw_ce_menu_bitmap_uv(obj_center, frame_idx, (short)(px0 + cap_w), (short)(py0 + top_h), (short)(px1 - cap_w), (short)(py1 - bot_h), 0.0f, v_body_start, 1.0f, v_body_end, tint_argb) &&
				ff_draw_ce_menu_bitmap_uv(obj_right, frame_idx, (short)(px1 - cap_w), (short)(py0 + top_h), px1, (short)(py1 - bot_h), 0.0f, v_body_start, 1.0f, v_body_end, tint_argb);
		}

		if (ok)
		{
			ok =
				ff_draw_ce_menu_bitmap_uv(obj_left, frame_idx, px0, (short)(py1 - bot_h), (short)(px0 + cap_w), py1, 0.0f, v_body_end, 1.0f, v_bot_end, tint_argb) &&
				ff_draw_ce_menu_bitmap_uv(obj_center, frame_idx, (short)(px0 + cap_w), (short)(py1 - bot_h), (short)(px1 - cap_w), py1, 0.0f, v_body_end, 1.0f, v_bot_end, tint_argb) &&
				ff_draw_ce_menu_bitmap_uv(obj_right, frame_idx, (short)(px1 - cap_w), (short)(py1 - bot_h), px1, py1, 0.0f, v_body_end, 1.0f, v_bot_end, tint_argb);
		}

		if (ok)
			return;
	}

	/* Fallback if menu bitmap tags are not present */
	{
		short border_px = (short)MAX(2, (int)(2.0f * scale + 0.5f));
		short corner_in = (short)MAX(2, (int)(3.0f * scale + 0.5f));
		short header_end_y = with_header ? (short)(py0 + top_h) : py0;
		short divider_end_y = (header_end_y > py0) ? (short)MIN(py1 - border_px, header_end_y + border_px) : py0;
		short body_y0 = (divider_end_y > py0) ? divider_end_y : (short)(py0 + border_px);
		short body_y1 = (short)(py1 - border_px);
		short inner_x0 = (short)(px0 + border_px);
		short inner_x1 = (short)(px1 - border_px);
		pixel32 bot_fill = (fallback_fill == FF_HUD_WARN_FILL) ? FF_HUD_WARN_BOT : FF_HUD_PANEL_BOT;
		rectangle2d r;
		int band;

		if (inner_x1 <= inner_x0 || body_y1 <= py0 + border_px)
			return;

		r.x0 = (short)(px0 + corner_in);
		r.x1 = (short)(px1 - corner_in);
		r.y0 = py0;
		r.y1 = (short)(py0 + border_px);
		if (r.x1 > r.x0)
			draw_quad(&r, fallback_edge);

		r.y0 = (short)(py1 - border_px);
		r.y1 = py1;
		if (r.x1 > r.x0)
			draw_quad(&r, fallback_edge);

		r.x0 = px0;
		r.x1 = inner_x0;
		r.y0 = (short)(py0 + border_px);
		r.y1 = body_y1;
		draw_quad(&r, fallback_edge);

		r.x0 = inner_x1;
		r.x1 = px1;
		draw_quad(&r, fallback_edge);

		if (header_end_y > py0 + border_px)
		{
			r.x0 = inner_x0;
			r.x1 = inner_x1;
			r.y0 = (short)(py0 + border_px);
			r.y1 = header_end_y;
			draw_quad(&r, FF_HUD_HEADER_FILL);

			r.y0 = header_end_y;
			r.y1 = divider_end_y;
			draw_quad(&r, fallback_edge);
		}

		if (body_y1 > body_y0)
		{
			short body_h = (short)(body_y1 - body_y0);
			for (band = 0; band < 3; band++)
			{
				short by0 = (short)(body_y0 + (body_h * band) / 3);
				short by1 = (band == 2) ? body_y1 : (short)(body_y0 + (body_h * (band + 1)) / 3);
				if (by1 > by0)
				{
					pixel32 fill_color = ff_lerp_argb(fallback_fill, bot_fill, (real)band * 0.5f);
					r.x0 = inner_x0;
					r.x1 = inner_x1;
					r.y0 = by0;
					r.y1 = by1;
					draw_quad(&r, fill_color);
				}
			}
		}
	}
}

static void draw_ce_hud_box(
	real vx0,
	real vy0,
	real scale,
	real x0,
	real y0,
	real x1,
	real y1,
	real header_h,
	pixel32 top_fill,
	pixel32 edge)
{
	short px0 = (short)(vx0 + x0 * scale + 0.5f);
	short py0 = (short)(vy0 + y0 * scale + 0.5f);
	short px1 = (short)MAX(vx0 + x1 * scale + 0.5f, px0 + 6);
	short py1 = (short)MAX(vy0 + y1 * scale + 0.5f, py0 + 6);
	short header_px = (header_h > 0.0f && y0 + header_h < y1)
		? (short)(header_h * scale + 0.5f)
		: 0;

	firefight_draw_ce_rounded_box(px0, py0, px1, py1, header_px, scale, 0xFFFFFFFFUL, top_fill, edge);
}

static void ff_draw_box(
	real vx0,
	real vy0,
	real scale,
	real x0,
	real y0,
	real x1,
	real y1,
	real header_h,
	pixel32 fill,
	pixel32 edge)
{
	draw_ce_hud_box(vx0, vy0, scale, x0, y0, x1, y1, header_h, fill, edge);
}

static void ff_draw_quad(
	real vx0,
	real vy0,
	real scale,
	real x0,
	real y0,
	real x1,
	real y1,
	pixel32 col)
{
	rectangle2d r;
	r.x0 = (short)(vx0 + x0 * scale + 0.5f);
	r.y0 = (short)(vy0 + y0 * scale + 0.5f);
	r.x1 = (short)(vx0 + x1 * scale + 0.5f);
	r.y1 = (short)(vy0 + y1 * scale + 0.5f);
	if (r.x1 > r.x0 && r.y1 > r.y0)
		draw_quad(&r, col);
}

static void ff_draw_text(
	long font_index,
	short line_height,
	real vx0,
	real vy0,
	real scale,
	char const *ascii_text,
	real x0,
	real x1,
	real y,
	real text_scale,
	short justification,
	real_argb_color const *color)
{
	real avail_w = MAX(1.0f, x1 - x0);
	real eff_scale = text_scale;
	real draw_x, ox, oy, final_scale;
	wchar_t wide[128];
	rectangle2d measure_bounds, ink, cursor, bounds;
	short ink_w;
	long i;

	if (!ascii_text || !ascii_text[0] || font_index == NONE)
		return;

	for (i = 0; i < (long)NUMBEROF(wide) - 1 && ascii_text[i]; i++)
		wide[i] = (wchar_t)(unsigned char)ascii_text[i];
	wide[i] = 0;

	draw_string_set_draw_mode(font_index, NONE, _ff_text_left, 0, color);
	measure_bounds.x0 = 0;
	measure_bounds.y0 = 0;
	measure_bounds.x1 = 4096;
	measure_bounds.y1 = (short)(line_height * 2);
	draw_unicode_string_compute_bounds(&measure_bounds, wide, &ink, &cursor);

	ink_w = (ink.x1 > ink.x0) ? (short)(ink.x1 - ink.x0) : 1;
	if ((real)ink_w * eff_scale > avail_w)
		eff_scale = avail_w / (real)ink_w;

	if (justification == _ff_text_center)
		draw_x = x0 + (avail_w - (real)ink_w * eff_scale) * 0.5f;
	else if (justification == _ff_text_right)
		draw_x = x1 - (real)ink_w * eff_scale;
	else
		draw_x = x0;

	ox = vx0 + draw_x * scale;
	oy = vy0 + y * scale;
	final_scale = scale * eff_scale;

	bounds.x0 = (short)(ox - (ink.x0 > 0 ? ink.x0 : 0));
	bounds.y0 = (short)oy;
	bounds.x1 = (short)(bounds.x0 + ink_w + 64);
	bounds.y1 = (short)(oy + line_height * 2);

	rasterizer_text_set_scale(final_scale, ox, oy);
	rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, wide);
	rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);
}

static void format_elapsed_time(long elapsed_ticks, char *out_buf, size_t buf_size)
{
	long total_seconds = MAX(0L, elapsed_ticks / TICKS_PER_SECOND);
	long hours = total_seconds / 3600L;
	long minutes = (total_seconds / 60L) % 60L;
	long seconds = total_seconds % 60L;

	if (hours > 0)
		snprintf(out_buf, buf_size, "%ld:%02ld:%02ld", hours, minutes, seconds);
	else
		snprintf(out_buf, buf_size, "%02ld:%02ld", minutes, seconds);
}

/* ---------- 3D -> 2D Navpoints (Stragglers & Resupply Drops) */

static void draw_world_navpoint(
	long font_index,
	short line_height,
	real scale,
	real_point3d const *world_pos,
	char const *label,
	pixel32 box_color,
	real_argb_color const *text_color,
	boolean is_friendly_marker)
{
	real_point3d view_pt;
	real_point2d screen_pt;
	real dist;
	char full_label[48];
	rectangle2d marker;
	short pill_x0, pill_y0, pill_x1, pill_y1;
	boolean drew_friendly_bitmap = FALSE;

	matrix4x3_transform_point(&render.frustum.world_to_view, world_pos, &view_pt);
	if (!render_camera_view_to_screen(&render.camera, &render.frustum, &view_pt, &screen_pt))
	{
		/* When ordnance is off-screen, render a green off-screen directional arrow */
		if (is_friendly_marker && render.local_player_index != NONE && hud_globals && hud_globals->waypoint.arrows.count > 0)
		{
			short arrow_index = find_nav_point("default");
			if (arrow_index == NONE)
				arrow_index = 0;
			if (arrow_index >= 0 && arrow_index < hud_globals->waypoint.arrows.count)
			{
				struct ff_hud_waypoint_arrow
				{
					char name[0x20];
					long unused0[2];
					unsigned long color;
					real opacity;
					real fade;
					short sequence_indices[3];
					short pad;
					long unused1[4];
					unsigned long flags;
					long unused2[6];
				};
				struct ff_hud_waypoint_arrow *arrow = TAG_BLOCK_GET_ELEMENT(
					&hud_globals->waypoint.arrows,
					arrow_index,
					struct ff_hud_waypoint_arrow);
				unsigned long saved_color = arrow->color;
				arrow->color = 0x0040E070;
				custom_render_nav_point(
					render.local_player_index,
					world_pos,
					arrow_index,
					1 /* _waypoint_off_screen */);
				arrow->color = saved_color;
			}
		}
		return;
	}

	dist = distance3d(&render.camera.position, world_pos);
	snprintf(full_label, sizeof(full_label), "%s [%dm]", label, (int)(dist * 3.0f + 0.5f));

	pill_x0 = (short)(screen_pt.x - 48.0f * scale + 0.5f);
	pill_y0 = (short)(screen_pt.y - 21.0f * scale + 0.5f);
	pill_x1 = (short)(screen_pt.x + 48.0f * scale + 0.5f);
	pill_y1 = (short)(screen_pt.y - 8.0f * scale + 0.5f);
	firefight_draw_ce_rounded_pill(pill_x0, pill_y0, pill_x1, pill_y1, 0, scale, 0xD8FFFFFFUL);

	if (is_friendly_marker)
	{
		long mp_hud_tag_index = interface_get_tag_index(_interface_bitmap_multiplayer_hud);
		if (mp_hud_tag_index != NONE)
		{
			struct bitmap_data *bitmap = bitmap_group_get_bitmap_from_sequence(
				mp_hud_tag_index,
				0,
				0);
			if (bitmap && _texture_cache_bitmap_get_hardware_format(bitmap, FALSE, TRUE))
			{
				point2d position;
				real_rectangle2d clip;
				real depth_factor;
				real fade;

				position.x = (short)fast_ftol(screen_pt.x) - render.camera.viewport_bounds.x0;
				position.y = (short)fast_ftol(screen_pt.y) - render.camera.viewport_bounds.y0;
				depth_factor = 1.0f -
					((-view_pt.z - render.frustum.z_near) * 100.0f) /
					(render.frustum.z_far - render.frustum.z_near);
				fade = PIN(depth_factor, 0.25f, 1.0f);
				clip.x0 = 0.0f;
				clip.x1 = 1.0f;
				clip.y0 = (1.0f - fade) * 0.5f;
				clip.y1 = 1.0f;
				interface_draw_bitmap(
					bitmap,
					&position,
					&clip,
					1.0f,
					0.0f,
					1.0f);
				drew_friendly_bitmap = TRUE;
			}
		}
	}

	if (!drew_friendly_bitmap)
	{
		/* Draw a downward chevron/diamond marker instead of a flat square */
		short cx = (short)(screen_pt.x + 0.5f);
		short cy = (short)(screen_pt.y + 1.0f * scale + 0.5f);
		short row;
		for (row = 0; row < 5; row++)
		{
			short half_w = (short)((4 - row) * scale + 0.5f);
			marker.x0 = cx - half_w;
			marker.x1 = cx + half_w + 1;
			marker.y0 = cy + (short)(row * scale);
			marker.y1 = marker.y0 + (short)(scale + 0.5f);
			if (marker.y1 <= marker.y0)
				marker.y1 = marker.y0 + 1;
			draw_quad(&marker, box_color);
		}
	}

	ff_draw_text(
		font_index,
		line_height,
		screen_pt.x - 46.0f * scale,
		screen_pt.y - 19.5f * scale,
		scale,
		full_label,
		0.0f,
		92.0f,
		0.0f,
		0.48f,
		_ff_text_center,
		text_color);
}

/* ---------- Generator Defense HUD Bars */

static void draw_generator_bars(
	long font_index,
	short line_height,
	real vx0,
	real vy0,
	real scale)
{
	static char const *const gen_names[3] = { "ALPHA GEN", "BETA GEN", "GAMMA GEN" };
	real base_x = 14.0f;
	real base_y = 106.0f;
	real card_w = 136.0f;
	real card_h = 24.0f;
	short g;

	if (ff_state.generator_count <= 0)
		return;

	for (g = 0; g < ff_state.generator_count && g < FIREFIGHT_MAX_GENERATORS; g++)
	{
		struct firefight_generator_state const *gen = &ff_state.generators[g];
		real y = base_y + g * (card_h + 4.0f);
		real_argb_color white = { 1.0f, 0.95f, 0.95f, 0.95f };
		real_argb_color red = { 1.0f, 1.0f, 0.35f, 0.35f };
		real_argb_color gold = { 1.0f, 1.0f, 0.85f, 0.25f };
		boolean destroyed = !gen->alive;
		real health_frac = (gen->max_health > 0) ? ((real)gen->health / (real)gen->max_health) : 0.0f;
		real shield_frac = (gen->max_shield > 0) ? ((real)gen->shield / (real)gen->max_shield) : 0.0f;
		char status_str[32];

		ff_draw_box(
			vx0, vy0, scale,
			base_x, y, base_x + card_w, y + card_h,
			0.0f,
			destroyed ? FF_HUD_WARN_FILL : FF_HUD_PANEL_FILL,
			destroyed ? FF_HUD_WARN_EDGE : FF_HUD_PANEL_EDGE);

		ff_draw_text(
			font_index, line_height, vx0, vy0, scale,
			gen_names[g],
			base_x + 6.0f, base_x + 76.0f, y + 3.0f,
			0.51f, _ff_text_left, &white);

		if (destroyed)
		{
			ff_draw_text(
				font_index, line_height, vx0, vy0, scale,
				"DESTROYED",
				base_x + 78.0f, base_x + card_w - 6.0f, y + 3.0f,
				0.51f, _ff_text_right, &red);
		}
		else if (gen->lockdown_ticks > 0)
		{
			snprintf(status_str, sizeof(status_str), "LOCK %ds",
				(int)((gen->lockdown_ticks + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND));
			ff_draw_text(
				font_index, line_height, vx0, vy0, scale,
				status_str,
				base_x + 78.0f, base_x + card_w - 6.0f, y + 3.0f,
				0.51f, _ff_text_right, &gold);
		}
		else
		{
			snprintf(status_str, sizeof(status_str), "%d%%",
				(int)(health_frac * 100.0f + 0.5f));
			ff_draw_text(
				font_index, line_height, vx0, vy0, scale,
				status_str,
				base_x + 78.0f, base_x + card_w - 6.0f, y + 3.0f,
				0.51f, _ff_text_right, &white);
		}

		/* Shield & Health bar strips */
		if (!destroyed)
		{
			real bar_x0 = base_x + 6.0f;
			real bar_w = card_w - 12.0f;
			rectangle2d bar_rect;

			/* Background */
			bar_rect.x0 = (short)(vx0 + bar_x0 * scale);
			bar_rect.y0 = (short)(vy0 + (y + 14.0f) * scale);
			bar_rect.x1 = (short)(vx0 + (bar_x0 + bar_w) * scale);
			bar_rect.y1 = (short)(vy0 + (y + 20.0f) * scale);
			draw_quad(&bar_rect, FF_HUD_BAR_BG);

			/* Health fill */
			bar_rect.x1 = (short)(vx0 + (bar_x0 + bar_w * PIN(health_frac, 0.0f, 1.0f)) * scale);
			bar_rect.y1 = (short)(vy0 + (y + 17.0f) * scale);
			draw_quad(&bar_rect, gen->lockdown_ticks > 0 ? FF_HUD_LOCK_BAR : FF_HUD_HEALTH_BAR);

			/* Shield fill */
			bar_rect.y0 = (short)(vy0 + (y + 17.0f) * scale);
			bar_rect.y1 = (short)(vy0 + (y + 20.0f) * scale);
			bar_rect.x1 = (short)(vx0 + (bar_x0 + bar_w * PIN(shield_frac, 0.0f, 1.0f)) * scale);
			draw_quad(&bar_rect, FF_HUD_SHIELD_BAR);

			/* 3D World-to-Screen Navpoint over active generator */
			{
				real_point3d gen_nav = gen->position;
				gen_nav.z += 1.25f;
				draw_world_navpoint(
					font_index,
					line_height,
					scale,
					&gen_nav,
					gen_names[g],
					gen->under_attack_ticks > 0 ? FF_HUD_WARN_EDGE : FF_HUD_PANEL_EDGE,
					gen->under_attack_ticks > 0 ? &red : (gen->lockdown_ticks > 0 ? &gold : &white),
					FALSE);
			}
		}
	}
}

struct ff_skull_hud_def
{
	char const *name;
	char const *code;
	char const *desc;
	pixel32 badge_color;
};

static struct ff_skull_hud_def const s_ff_skull_hud_defs[NUMBER_OF_FIREFIGHT_SKULLS] =
{
	{ "IRON",           "IRN", "TEAM WIPE ENDS THE SET",                 0xFFE83A3A },
	{ "BLACK EYE",      "BLK", "SHIELDS ONLY RECHARGE ON MELEE",         0xFFB668FF },
	{ "TOUGH LUCK",     "TLK", "ENEMIES DODGE GRENADES & ROCKETS",       0xFFF09828 },
	{ "CATCH",          "CAT", "ENEMIES THROW MORE GRENADES",            0xFF38C8F0 },
	{ "CLOUD",          "FOG", "MOTION TRACKER DISABLED",                0xFF90A8C0 },
	{ "FAMINE",         "FAM", "DROPPED WEAPONS HAVE LESS AMMO",         0xFFD4A838 },
	{ "TILT",           "TLT", "ENEMY SHIELD & ARMOR RESISTANCE UP",     0xFF4288F5 },
	{ "MYTHIC",         "MYT", "ENEMIES HAVE 2X HEALTH & SHIELDS",       0xFFFF5038 },
	{ "THUNDERSTORM",   "THN", "ENEMIES PROMOTED TO HIGHER RANKS",       0xFFFFD038 },
	{ "BLIND",          "BLD", "HUD & FIRST-PERSON WEAPON HIDDEN",       0xFF808898 },
	{ "BOOM",           "BOM", "2X EXPLOSION RADIUS & DAMAGE",           0xFFFF6E28 },
	{ "COWBELL",        "CWB", "ACCELERATED EXPLOSIVE KNOCKBACK",        0xFF50E088 },
	{ "GRUNT BIRTHDAY", "GBP", "HEADSHOTS DETONATE IN CONFETTI",         0xFFF570C0 }
};

static void draw_hud_mini_skull_icon(
	real vx0,
	real vy0,
	real scale,
	real cx,
	real cy,
	real radius,
	pixel32 bone_col,
	pixel32 eye_col)
{
	/* Cranium */
	ff_draw_quad(vx0, vy0, scale, cx - radius * 0.85f, cy - radius * 0.90f, cx + radius * 0.85f, cy + radius * 0.25f, bone_col);
	ff_draw_quad(vx0, vy0, scale, cx - radius * 0.65f, cy - radius * 1.05f, cx + radius * 0.65f, cy - radius * 0.85f, bone_col);
	/* Jaw & teeth */
	ff_draw_quad(vx0, vy0, scale, cx - radius * 0.52f, cy + radius * 0.25f, cx + radius * 0.52f, cy + radius * 0.85f, bone_col);
	/* Eye sockets */
	ff_draw_quad(vx0, vy0, scale, cx - radius * 0.58f, cy - radius * 0.42f, cx - radius * 0.16f, cy + radius * 0.02f, eye_col);
	ff_draw_quad(vx0, vy0, scale, cx + radius * 0.16f, cy - radius * 0.42f, cx + radius * 0.58f, cy + radius * 0.02f, eye_col);
	/* Nasal cavity */
	ff_draw_quad(vx0, vy0, scale, cx - radius * 0.10f, cy + radius * 0.02f, cx + radius * 0.10f, cy + radius * 0.28f, eye_col);
}

static void draw_wave_progress_pips_and_skulls(
	long font_index,
	short line_height,
	real vx0,
	real vy0,
	real scale,
	real panel_x,
	real panel_y,
	real panel_w)
{
	real strip_y = panel_y - 16.0f;
	real cur_x = panel_x;
	short r, w, bit;
	short cur_round = PIN(ff_state.current_round, (short)1, (short)FIREFIGHT_ROUNDS_PER_SET);
	short cur_wave = PIN(ff_state.current_wave, (short)1, (short)FIREFIGHT_WAVES_PER_ROUND);
	short shown_skulls = 0;
	real_argb_color skull_txt_col = { 1.0f, 0.85f, 0.94f, 1.0f };

	/* 1. MCC / Halo: Reach 5 Round Boxes + 3 Wave Pips inside active round (Halo CE rounded pill style) */
	for (r = 1; r <= FIREFIGHT_ROUNDS_PER_SET; r++)
	{
		boolean is_past = (!ff_state.in_bonus_round && r < cur_round) || ff_state.in_bonus_round;
		boolean is_current = (!ff_state.in_bonus_round && r == cur_round);
		real box_w = is_current ? 32.0f : 14.0f;
		real box_h = 13.0f;
		short rx0 = (short)(vx0 + cur_x * scale + 0.5f);
		short ry0 = (short)(vy0 + strip_y * scale + 0.5f);
		short rx1 = (short)(vx0 + (cur_x + box_w) * scale + 0.5f);
		short ry1 = (short)(vy0 + (strip_y + box_h) * scale + 0.5f);
		pixel32 border_c = is_current ? 0xFF68B8FF : (is_past ? 0xFF2896FF : 0xB01C4878);
		pixel32 fill_c = is_current ? 0xD8142848 : (is_past ? 0xC0124278 : 0x98061020);

		if (!firefight_draw_ce_rounded_pill(
				rx0, ry0, rx1, ry1,
				is_current ? 1 : 0,
				scale,
				is_current ? 0xFFFFFFFFUL : (is_past ? 0xF088D0FFUL : 0xC090B8E0UL)))
		{
			ff_draw_quad(vx0, vy0, scale, cur_x, strip_y, cur_x + box_w, strip_y + box_h, border_c);
			ff_draw_quad(vx0, vy0, scale, cur_x + 1.0f, strip_y + 1.0f, cur_x + box_w - 1.0f, strip_y + box_h - 1.0f, fill_c);
		}

		if (is_past)
		{
			/* Solid completed round bar in Halo CE blue */
			ff_draw_quad(vx0, vy0, scale, cur_x + 3.0f, strip_y + 3.0f, cur_x + box_w - 3.0f, strip_y + box_h - 3.0f, 0xFF2896FF);
		}
		else if (is_current)
		{
			/* 3 Wave Pips inside current round */
			for (w = 1; w <= FIREFIGHT_WAVES_PER_ROUND; w++)
			{
				real pip_x = cur_x + 3.5f + (real)(w - 1) * 8.8f;
				real pip_w = 7.2f;
				pixel32 pip_c;

				if (w < cur_wave)
					pip_c = 0xFF2896FF; /* Cleared wave (Halo CE blue) */
				else if (w == cur_wave)
					pip_c = (w == FIREFIGHT_WAVES_PER_ROUND) ? 0xFFFF6040 : 0xFFFFFFFF; /* Active wave (crisp white, red-orange on Boss wave) */
				else
					pip_c = 0x90102848; /* Upcoming wave */

				ff_draw_quad(vx0, vy0, scale, pip_x, strip_y + 3.0f, pip_x + pip_w, strip_y + box_h - 3.0f, pip_c);
			}
		}

		cur_x += box_w + 2.5f;
	}

	/* 2. Active Hazard Skull Badges on the right side of the pip strip (Halo CE rounded pill capsules) */
	{
		real badge_right = panel_x + panel_w;
		for (bit = NUMBER_OF_FIREFIGHT_SKULLS - 1; bit >= 0 && shown_skulls < 4; bit--)
		{
			if ((ff_state.active_skulls & (1UL << bit)) == 0)
				continue;
			{
				real bw = 25.0f;
				real bx0 = badge_right - bw;
				short sx0 = (short)(vx0 + bx0 * scale + 0.5f);
				short sy0 = (short)(vy0 + strip_y * scale + 0.5f);
				short sx1 = (short)(vx0 + (bx0 + bw) * scale + 0.5f);
				short sy1 = (short)(vy0 + (strip_y + 13.0f) * scale + 0.5f);
				pixel32 sc = s_ff_skull_hud_defs[bit].badge_color;

				if (bx0 < cur_x + 2.0f)
					break;
				if (!firefight_draw_ce_rounded_pill(sx0, sy0, sx1, sy1, 0, scale, 0xFFFFFFFFUL))
				{
					ff_draw_quad(vx0, vy0, scale, bx0, strip_y, bx0 + bw, strip_y + 13.0f, 0xFF2896FF);
					ff_draw_quad(vx0, vy0, scale, bx0 + 1.0f, strip_y + 1.0f, bx0 + bw - 1.0f, strip_y + 12.0f, 0xD80A1424);
				}
				draw_hud_mini_skull_icon(vx0, vy0, scale, bx0 + 5.5f, strip_y + 6.5f, 3.2f, sc, 0xFF0A1424);
				ff_draw_text(font_index, line_height, vx0, vy0, scale,
					s_ff_skull_hud_defs[bit].code, bx0 + 9.0f, bx0 + bw - 1.5f, strip_y + 1.0f,
					0.42f, _ff_text_center, &skull_txt_col);
				badge_right = bx0 - 2.0f;
				shown_skulls++;
			}
		}
	}
}

static void format_banner_strings(
	short banner_type,
	short banner_data,
	char *title_out,
	size_t title_size,
	char *sub_out,
	size_t sub_size)
{
	title_out[0] = '\0';
	sub_out[0] = '\0';

	switch (banner_type)
	{
	case _ff_banner_set_start:
		snprintf(title_out, title_size, "SET %d START", (int)banner_data);
		snprintf(sub_out, sub_size, "PREPARE FOR COMBAT");
		break;
	case _ff_banner_round_start:
		snprintf(title_out, title_size, "ROUND %d", (int)banner_data);
		snprintf(sub_out, sub_size, "HOLD YOUR GROUND");
		break;
	case _ff_banner_wave_start:
		snprintf(title_out, title_size, "WAVE %d", (int)banner_data);
		snprintf(sub_out, sub_size, "HOSTILES INBOUND");
		break;
	case _ff_banner_boss_wave:
		snprintf(title_out, title_size, "BOSS WAVE %d", (int)banner_data);
		snprintf(sub_out, sub_size, "ELIMINATE HIGH-VALUE TARGETS");
		break;
	case _ff_banner_reinforcements:
		snprintf(title_out, title_size, "REINFORCEMENTS");
		snprintf(sub_out, sub_size, "ENEMY SQUADS DEPLOYING");
		break;
	case _ff_banner_bonus_round:
		snprintf(title_out, title_size, "BONUS ROUND");
		snprintf(sub_out, sub_size, "60s GRUNT BIRTHDAY PARTY — EXTRA LIVES!");
		break;
	case _ff_banner_set_complete:
		snprintf(title_out, title_size, "SET %d COMPLETE", (int)banner_data);
		snprintf(sub_out, sub_size, "BONUS LIVES AWARDED");
		break;
	case _ff_banner_skull_activated:
		if (banner_data >= 0 && banner_data < NUMBER_OF_FIREFIGHT_SKULLS)
		{
			snprintf(title_out, title_size, "SKULL ACTIVE: %s", s_ff_skull_hud_defs[banner_data].name);
			snprintf(sub_out, sub_size, "%s", s_ff_skull_hud_defs[banner_data].desc);
		}
		else
		{
			snprintf(title_out, title_size, "SKULL ACTIVATED");
			snprintf(sub_out, sub_size, "ENEMY MODIFIER ENGAGED");
		}
		break;
	case _ff_banner_last_man_standing:
		snprintf(title_out, title_size, "LAST MAN STANDING");
		snprintf(sub_out, sub_size, "SURVIVE TO KEEP THE TEAM ALIVE");
		break;
	case _ff_banner_generator_under_attack:
		snprintf(title_out, title_size, "GENERATOR %c UNDER ATTACK", 'A' + PIN(banner_data, 0, 2));
		snprintf(sub_out, sub_size, "DEFEND THE GENERATOR");
		break;
	case _ff_banner_generator_locked_down:
		snprintf(title_out, title_size, "GENERATOR %c LOCKED DOWN", 'A' + PIN(banner_data, 0, 2));
		snprintf(sub_out, sub_size, "BLAST SHIELDS ACTIVE FOR 30s");
		break;
	case _ff_banner_generator_destroyed:
		snprintf(title_out, title_size, "GENERATOR %c DESTROYED", 'A' + PIN(banner_data, 0, 2));
		snprintf(sub_out, sub_size, "DEFENSIVE PERIMETER BREACHED");
		break;
	case _ff_banner_ordnance_drop:
		snprintf(title_out, title_size, "WEAPON & AMMO DROP INBOUND");
		snprintf(sub_out, sub_size, "RESUPPLY CACHE DEPLOYED AT BASE");
		break;
	case _ff_banner_heavy_ordnance_drop:
		snprintf(title_out, title_size, "HEAVY ANTI-ARMOR DROP");
		snprintf(sub_out, sub_size, "ROCKETS, SNIPER & OVERSHIELD DEPLOYED");
		break;
	case _ff_banner_dropship_inbound:
		snprintf(title_out, title_size, "DROPSHIP INBOUND");
		snprintf(sub_out, sub_size, "PHANTOM / PELICAN INSERTION DETECTED");
		break;
	case _ff_banner_vehicle_deployed:
		snprintf(title_out, title_size, "VEHICLE DEPLOYED");
		snprintf(sub_out, sub_size, "ARMORED SUPPORT AT PLAYER BASE");
		break;
	case _ff_banner_bonus_life:
		snprintf(title_out, title_size, "+%d BONUS LIFE", (int)MAX(1, banner_data));
		snprintf(sub_out, sub_size, "TEAM REINFORCEMENT EARNED");
		break;
	case _ff_banner_victory:
		snprintf(title_out, title_size, "FIREFIGHT VICTORY");
		snprintf(sub_out, sub_size, "ALL SETS CLEARED");
		break;
	case _ff_banner_defeat:
		snprintf(title_out, title_size, "FIREFIGHT OVER");
		snprintf(sub_out, sub_size, "TEAM ELIMINATED");
		break;
	default:
		break;
	}
}

/* ---------- Main Firefight HUD Draw */

void firefight_hud_draw(void)
{
	long font_index;
	struct font_header *font;
	short line_height;
	real vx0, vy0, vw, vh, scale;
	real panel_x = 14.0f;
	real panel_w = 196.0f;
	real panel_h = 56.0f;
	real panel_y;
	real_argb_color white = { 1.0f, 0.96f, 0.98f, 1.0f };
	real_argb_color cyan = { 1.0f, 0.42f, 0.78f, 1.0f };
	real_argb_color gold = { 1.0f, 0.96f, 0.98f, 1.0f };
	real_argb_color red = { 1.0f, 1.0f, 0.35f, 0.35f };
	real_argb_color green = { 1.0f, 0.35f, 1.0f, 0.45f };
	char line1_left[48], line1_right[32];
	char line2_left[48], line2_right[32];
	char line3_left[48], line3_right[32];
	char time_str[24];
	boolean prep_phase;

	if (!firefight_active())
		return;

	font_index = hud_get_font_index();
	if (font_index == NONE)
		return;
	font = font_definition_get(font_index);
	if (!font)
		return;
	line_height = (short)(font->ascending_height + font->descending_height);
	if (line_height <= 0)
		return;

	vx0 = (real)render.camera.viewport_bounds.x0;
	vy0 = (real)render.camera.viewport_bounds.y0;
	vw = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	vh = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	scale = vh / 480.0f;
	if (scale <= 0.0f)
		return;

	prep_phase = (ff_state.phase_state == _ff_phase_pre_set || ff_state.phase_state == _ff_phase_pre_round);

	/* 1. Bottom-Left Firefight Status Panel (placed above the motion sensor, Halo CE objective box style) */
	panel_y = 480.0f - 140.0f - panel_h;
	draw_wave_progress_pips_and_skulls(font_index, line_height, vx0, vy0, scale, panel_x, panel_y, panel_w);
	ff_draw_box(
		vx0, vy0, scale,
		panel_x, panel_y, panel_x + panel_w, panel_y + panel_h,
		18.0f,
		(ff_state.shared_lives_remaining >= 0 && ff_state.shared_lives_remaining <= 1) ? FF_HUD_WARN_FILL : FF_HUD_PANEL_FILL,
		(ff_state.shared_lives_remaining >= 0 && ff_state.shared_lives_remaining <= 1) ? FF_HUD_WARN_EDGE : FF_HUD_PANEL_EDGE);

	/* Row 1 (Header Strip): SET X RND Y WAVE Z  |  TIME MM:SS */
	if (ff_state.in_bonus_round)
	{
		snprintf(line1_left, sizeof(line1_left), "SET %d | BONUS ROUND", (int)ff_state.current_set);
	}
	else
	{
		snprintf(line1_left, sizeof(line1_left), "SET %d | RND %d | WAVE %d",
			(int)ff_state.current_set, (int)ff_state.current_round, (int)ff_state.current_wave);
	}
	format_elapsed_time(ff_state.session_elapsed_ticks, time_str, sizeof(time_str));
	snprintf(line1_right, sizeof(line1_right), "TIME %s", time_str);

	ff_draw_text(font_index, line_height, vx0, vy0, scale,
		line1_left, panel_x + 7.0f, panel_x + 128.0f, panel_y + 4.0f,
		0.54f, _ff_text_left, &white);
	ff_draw_text(font_index, line_height, vx0, vy0, scale,
		line1_right, panel_x + 130.0f, panel_x + panel_w - 7.0f, panel_y + 4.0f,
		0.54f, _ff_text_right, &cyan);

	/* Row 2: LIVES: XX  |  HOSTILES: XX */
	if (ff_state.shared_lives_remaining < 0)
		snprintf(line2_left, sizeof(line2_left), "LIVES: INFINITE");
	else
		snprintf(line2_left, sizeof(line2_left), "LIVES: %d", (int)ff_state.shared_lives_remaining);

	snprintf(line2_right, sizeof(line2_right), "HOSTILES: %d", (int)ff_state.enemies_alive);

	ff_draw_text(font_index, line_height, vx0, vy0, scale,
		line2_left, panel_x + 7.0f, panel_x + 104.0f, panel_y + 23.0f,
		0.58f, _ff_text_left, (ff_state.shared_lives_remaining >= 0 && ff_state.shared_lives_remaining <= 1) ? &red : &gold);
	ff_draw_text(font_index, line_height, vx0, vy0, scale,
		line2_right, panel_x + 106.0f, panel_x + panel_w - 7.0f, panel_y + 23.0f,
		0.58f, _ff_text_right, &white);

	/* Row 3: NEXT WAVE timer / Prep Countdown  |  Enemy Vitality Scale */
	if (prep_phase)
	{
		long prep_sec = (ff_state.phase_timer_ticks + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
		snprintf(line3_left, sizeof(line3_left), "PREP TIME: 0:%02ld", MAX(0L, prep_sec));
	}
	else if (ff_state.next_wave_timer_ticks > 0)
	{
		long wave_sec = MAX(0L, ff_state.next_wave_timer_ticks / TICKS_PER_SECOND);
		snprintf(line3_left, sizeof(line3_left), "NEXT WAVE: %ld:%02ld", wave_sec / 60L, wave_sec % 60L);
	}
	else
	{
		snprintf(line3_left, sizeof(line3_left), "NEXT WAVE: ON CLEAR");
	}

	snprintf(line3_right, sizeof(line3_right), "HP %d%%",
		(int)(firefight_set_vitality_scale_get() * 100.0f + 0.5f));

	ff_draw_text(font_index, line_height, vx0, vy0, scale,
		line3_left, panel_x + 7.0f, panel_x + 132.0f, panel_y + 39.0f,
		0.51f, _ff_text_left, &cyan);
	ff_draw_text(font_index, line_height, vx0, vy0, scale,
		line3_right, panel_x + 134.0f, panel_x + panel_w - 7.0f, panel_y + 39.0f,
		0.51f, _ff_text_right, &cyan);

	/* 2. Pre-Round Preparation Countdown Banner */
	if (prep_phase && ff_state.phase_timer_ticks > 0)
	{
		real bw = 280.0f;
		real bh = 38.0f;
		real bx = (vw / scale - bw) * 0.5f;
		real by = 52.0f;
		long secs = (ff_state.phase_timer_ticks + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
		char prep_title[64];
		short active_round_skull = NONE;
		short bit;

		for (bit = 0; bit < NUMBER_OF_FIREFIGHT_SKULLS; bit++)
		{
			if ((ff_state.round_skulls & (1UL << bit)) != 0)
			{
				active_round_skull = bit;
				break;
			}
		}

		ff_draw_box(vx0, vy0, scale, bx, by, bx + bw, by + bh, 19.0f, FF_HUD_PANEL_FILL, FF_HUD_LOCK_BAR);
		snprintf(prep_title, sizeof(prep_title), "ROUND %d STARTS IN %lds",
			(int)ff_state.current_round, MAX(1L, secs));
		ff_draw_text(font_index, line_height, vx0, vy0, scale,
			prep_title, bx + 8.0f, bx + bw - 8.0f, by + 4.0f,
			0.68f, _ff_text_center, &gold);
		ff_draw_text(font_index, line_height, vx0, vy0, scale,
			"RESUPPLY & BOARD VEHICLES", bx + 8.0f, bx + bw - 8.0f, by + 23.0f,
			0.51f, _ff_text_center, &cyan);

		if (active_round_skull != NONE)
		{
			char sk_title[64];
			real sby = by + bh + 4.0f;
			pixel32 sk_col = s_ff_skull_hud_defs[active_round_skull].badge_color;

			ff_draw_box(vx0, vy0, scale, bx, sby, bx + bw, sby + 34.0f, 17.0f, FF_HUD_WARN_FILL, sk_col);
			draw_hud_mini_skull_icon(vx0, vy0, scale, bx + 16.0f, sby + 9.0f, 5.2f, 0xFFF2ECE0, sk_col);
			draw_hud_mini_skull_icon(vx0, vy0, scale, bx + bw - 16.0f, sby + 9.0f, 5.2f, 0xFFF2ECE0, sk_col);
			snprintf(sk_title, sizeof(sk_title), "SKULL ACTIVE: %s", s_ff_skull_hud_defs[active_round_skull].name);
			ff_draw_text(font_index, line_height, vx0, vy0, scale,
				sk_title, bx + 26.0f, bx + bw - 26.0f, sby + 2.5f,
				0.58f, _ff_text_center, &gold);
			ff_draw_text(font_index, line_height, vx0, vy0, scale,
				s_ff_skull_hud_defs[active_round_skull].desc, bx + 10.0f, bx + bw - 10.0f, sby + 20.0f,
				0.48f, _ff_text_center, &white);
		}
	}
	/* 3. Wave / Boss / Skull / Ordnance Announcement Banner */
	else if (ff_state.banner_timer_ticks > 0 && ff_state.banner_type != _ff_banner_none)
	{
		char banner_title[64];
		char banner_sub[64];
		real bw = 280.0f;
		real bh = 38.0f;
		real bx = (vw / scale - bw) * 0.5f;
		real by = 52.0f;

		format_banner_strings(
			ff_state.banner_type,
			ff_state.banner_data,
			banner_title, sizeof(banner_title),
			banner_sub, sizeof(banner_sub));

		if (banner_title[0] != '\0')
		{
			boolean is_skull_b = (ff_state.banner_type == _ff_banner_skull_activated);
			pixel32 edge_c = is_skull_b && ff_state.banner_data >= 0 && ff_state.banner_data < NUMBER_OF_FIREFIGHT_SKULLS
				? s_ff_skull_hud_defs[ff_state.banner_data].badge_color
				: FF_HUD_PANEL_EDGE;

			ff_draw_box(vx0, vy0, scale, bx, by, bx + bw, by + bh, 19.0f, is_skull_b ? FF_HUD_WARN_FILL : FF_HUD_PANEL_FILL, edge_c);
			if (is_skull_b)
			{
				draw_hud_mini_skull_icon(vx0, vy0, scale, bx + 16.0f, by + 10.0f, 5.5f, 0xFFF2ECE0, edge_c);
				draw_hud_mini_skull_icon(vx0, vy0, scale, bx + bw - 16.0f, by + 10.0f, 5.5f, 0xFFF2ECE0, edge_c);
			}
			ff_draw_text(font_index, line_height, vx0, vy0, scale,
				banner_title, bx + 8.0f, bx + bw - 8.0f, by + 4.0f,
				0.68f, _ff_text_center, &gold);
			if (banner_sub[0] != '\0')
			{
				ff_draw_text(font_index, line_height, vx0, vy0, scale,
					banner_sub, bx + 8.0f, bx + bw - 8.0f, by + 23.0f,
					0.51f, _ff_text_center, &cyan);
			}
		}
	}

	/* 4. Generator Defense Status Bars */
	draw_generator_bars(font_index, line_height, vx0, vy0, scale);

	/* 5. 3D->2D World-to-Screen Navpoints for Stragglers (<= 3 enemies left) & Resupply Drops */
	if (ff_state.straggler_navpoints_active && ff_state.enemies_alive > 0 && ff_state.enemies_alive <= FIREFIGHT_STRAGGLER_THRESHOLD)
	{
		short local_player_index = render.local_player_index;
		short red_arrow_index = find_nav_point("target_red");
		short i;

		if (red_arrow_index == NONE)
			red_arrow_index = find_nav_point("default_red");
		if (red_arrow_index == NONE && hud_globals && hud_globals->waypoint.arrows.count > 0)
			red_arrow_index = (hud_globals->waypoint.arrows.count > 1) ? 1 : 0;

		for (i = 0; i < FIREFIGHT_MAX_TRACKED_ENEMIES; i++)
		{
			long unit_index = ff_state.tracked_enemy_units[i];
			struct unit_datum *unit;
			real_point3d nav_pos;

			if (unit_index == NONE)
				continue;
			unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
			if (!unit || unit->object.body_vitality <= 0.0f || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
				continue;

			nav_pos = unit->object.bounding_sphere_center;
			render_interpolation_object_point(unit_index, &nav_pos);
			nav_pos.z += unit->object.bounding_sphere_radius * 0.65f + 0.15f;

			if (local_player_index != NONE && red_arrow_index != NONE)
			{
				short waypoint_type = hud_get_nav_point_render_type(
					local_player_index,
					&render.camera.position,
					&nav_pos,
					unit_index);
				custom_render_nav_point(
					local_player_index,
					&nav_pos,
					red_arrow_index,
					waypoint_type);
			}
			else
			{
				draw_world_navpoint(
					font_index, line_height, scale,
					&nav_pos, "HOSTILE", FF_HUD_WARN_EDGE, &red, FALSE);
			}
		}
	}

	if (ff_state.ordnance_navpoint_ticks > 0)
	{
		real_point3d ord_pos = ff_state.last_ordnance_position;
		ord_pos.z += 0.4f;
		draw_world_navpoint(
			font_index, line_height, scale,
			&ord_pos,
			ff_state.last_ordnance_is_heavy ? "HEAVY ORDNANCE" : "ORDNANCE",
			0xE040E070, &green, TRUE);
	}
}

void firefight_hud_post_rasterize(void)
{
}

void firefight_format_scoreboard_title(
	wchar_t *title_string,
	long count,
	boolean postgame)
{
	char narrow[160];
	char time_str[24];
	long i;

	(void)postgame;
	if (!title_string || count <= 0)
		return;

	format_elapsed_time(ff_state.session_elapsed_ticks, time_str, sizeof(time_str));
	snprintf(
		narrow,
		sizeof(narrow),
		"FIREFIGHT | SET %d RND %d WAVE %d | LIVES: %d | TIME: %s | SCORE: %ld",
		(int)ff_state.current_set,
		(int)ff_state.current_round,
		(int)ff_state.current_wave,
		(int)ff_state.shared_lives_remaining,
		time_str,
		campaign_score_team_points());

	for (i = 0; i < count - 1 && narrow[i]; i++)
		title_string[i] = (wchar_t)(unsigned char)narrow[i];
	title_string[i] = 0;
}

boolean firefight_hud_format_scoreboard_title(
	wchar_t *title_string,
	long count)
{
	if (!firefight_active() || !title_string || count <= 0)
		return FALSE;

	firefight_format_scoreboard_title(title_string, count, FALSE);
	return TRUE;
}

void firefight_format_postgame_summary(
	wchar_t *line1,
	wchar_t *line2,
	long count)
{
	struct firefight_map_record best;
	char const *map_name = (global_scenario_index != NONE) ? tag_get_name(global_scenario_index) : "map";
	char narrow1[160];
	char narrow2[160];
	char time_str[24];
	long i;

	format_elapsed_time(ff_state.session_elapsed_ticks, time_str, sizeof(time_str));
	snprintf(
		narrow1,
		sizeof(narrow1),
		"REACHED SET %d RND %d WAVE %d | TIME: %s | TEAM SCORE: %ld",
		(int)ff_state.current_set,
		(int)ff_state.current_round,
		(int)ff_state.current_wave,
		time_str,
		campaign_score_team_points());

	if (firefight_stats_get_map_record(map_name, 0, &best) && best.best_score > 0)
	{
		snprintf(
			narrow2,
			sizeof(narrow2),
			"PERSONAL BEST: %ld PTS (SET %d RND %d WAVE %d)",
			best.best_score,
			(int)best.best_set,
			(int)best.best_round,
			(int)best.best_wave);
	}
	else
	{
		snprintf(
			narrow2,
			sizeof(narrow2),
			"SKULL MULTIPLIER: %.2fx | VITALITY SCALE: %d%%",
			(double)firefight_score_multiplier_get(),
			(int)(firefight_set_vitality_scale_get() * 100.0f + 0.5f));
	}

	if (line1 && count > 0)
	{
		for (i = 0; i < count - 1 && narrow1[i]; i++)
			line1[i] = (wchar_t)(unsigned char)narrow1[i];
		line1[i] = 0;
	}
	if (line2 && count > 0)
	{
		for (i = 0; i < count - 1 && narrow2[i]; i++)
			line2[i] = (wchar_t)(unsigned char)narrow2[i];
		line2[i] = 0;
	}
}

void firefight_hud_draw_postgame_summary(
	void (*drawline_fn)(wchar_t const *string, long row_index, short justification))
{
	wchar_t line1[160];
	wchar_t line2[160];

	if (!drawline_fn)
		return;

	firefight_format_postgame_summary(line1, line2, 160);
	drawline_fn(line1, 4, 0);
	drawline_fn(line2, 5, 0);
}

/* ---------- MCC Rank Icons & Customizable Emblem Renderer ---------- */

static pixel32 ff_apply_alpha(pixel32 argb, real alpha)
{
	unsigned long a = (unsigned long)((argb >> 24) & 0xFF);
	a = (unsigned long)PIN((long)((real)a * PIN(alpha, 0.0f, 1.0f) + 0.5f), 0L, 255L);
	return (pixel32)((argb & 0x00FFFFFFUL) | (a << 24));
}

static pixel32 const s_halo_emblem_colors[18] =
{
	0xFFECECEC, /* 0: White */
	0xFF26262A, /* 1: Black */
	0xFFD62B2B, /* 2: Red */
	0xFF245EE0, /* 3: Blue */
	0xFF7E848C, /* 4: Gray */
	0xFFF0C829, /* 5: Yellow */
	0xFF33A63C, /* 6: Green */
	0xFFF278B0, /* 7: Pink */
	0xFF853CC4, /* 8: Purple */
	0xFF29D4E6, /* 9: Cyan */
	0xFF1F42A6, /* 10: Cobalt */
	0xFFF0801F, /* 11: Orange */
	0xFF1F968C, /* 12: Teal */
	0xFF5E8049, /* 13: Sage */
	0xFF754E30, /* 14: Brown */
	0xFFC7B080, /* 15: Tan */
	0xFF821C26, /* 16: Maroon */
	0xFFF0736B  /* 17: Salmon */
};

static boolean mcc_point_in_chevron(real nx, real ny, real cy, real width, real thickness)
{
	real ax = (nx >= 0.0f) ? nx : -nx;
	real y_top;
	if (ax > width)
		return FALSE;
	y_top = cy + ax * 0.65f;
	return (ny >= y_top && ny <= y_top + thickness);
}

static boolean mcc_point_in_star(real nx, real ny, real cx, real cy, real radius)
{
	real dx = nx - cx;
	real dy = ny - cy;
	real ax = (dx >= 0.0f) ? dx : -dx;
	real ay = (dy >= 0.0f) ? dy : -dy;
	real dist = square_root(dx * dx + dy * dy);
	if (dist > radius)
		return FALSE;
	if (dist <= radius * 0.42f)
		return TRUE;
	if (ax < radius * 0.24f * (1.0f - ay / radius))
		return TRUE;
	if (ay < radius * 0.24f * (1.0f - ax / radius))
		return TRUE;
	if (dy > -radius * 0.15f &&
		fabsf(ax - ay * 0.85f) < radius * 0.22f * (1.0f - dist / radius))
		return TRUE;
	return FALSE;
}

#define FF_ICON_TEX_DIM 32
#define FF_ICON_HI_DIM 128
#define FF_ICON_HI_MIPS 4
#define FF_ICON_GRID_DIM 16
#define FF_ICON_MAX_RECTS 128

struct ff_hw_icon_entry
{
	boolean valid;
	unsigned long key;
	struct bitmap_data bitmap;
};

static struct ff_hw_icon_entry s_hw_icon_cache[64];
static int s_hw_icon_next = 0;

static void ff_downsample_2x(pixel32 const *src, int src_dim, pixel32 *dst)
{
	int dst_dim = src_dim >> 1;
	int y, x;

	for (y = 0; y < dst_dim; y++)
	{
		for (x = 0; x < dst_dim; x++)
		{
			pixel32 c00 = src[(y * 2) * src_dim + (x * 2)];
			pixel32 c01 = src[(y * 2) * src_dim + (x * 2 + 1)];
			pixel32 c10 = src[(y * 2 + 1) * src_dim + (x * 2)];
			pixel32 c11 = src[(y * 2 + 1) * src_dim + (x * 2 + 1)];
			unsigned long a00 = (c00 >> 24) & 0xFF;
			unsigned long a01 = (c01 >> 24) & 0xFF;
			unsigned long a10 = (c10 >> 24) & 0xFF;
			unsigned long a11 = (c11 >> 24) & 0xFF;
			unsigned long a_sum = a00 + a01 + a10 + a11;

			if (a_sum == 0)
			{
				dst[y * dst_dim + x] = 0;
			}
			else
			{
				unsigned long r = (((c00 >> 16) & 0xFF) * a00 + ((c01 >> 16) & 0xFF) * a01 +
					((c10 >> 16) & 0xFF) * a10 + ((c11 >> 16) & 0xFF) * a11 + (a_sum >> 1)) / a_sum;
				unsigned long g = (((c00 >> 8) & 0xFF) * a00 + ((c01 >> 8) & 0xFF) * a01 +
					((c10 >> 8) & 0xFF) * a10 + ((c11 >> 8) & 0xFF) * a11 + (a_sum >> 1)) / a_sum;
				unsigned long b = ((c00 & 0xFF) * a00 + (c01 & 0xFF) * a01 +
					(c10 & 0xFF) * a10 + (c11 & 0xFF) * a11 + (a_sum >> 1)) / a_sum;
				unsigned long a = (a_sum + 2) >> 2;
				if (r > 255) r = 255;
				if (g > 255) g = 255;
				if (b > 255) b = 255;
				if (a > 255) a = 255;
				dst[y * dst_dim + x] = (pixel32)((a << 24) | (r << 16) | (g << 8) | b);
			}
		}
	}
}

static long ff_color_dist_argb(pixel32 c1, pixel32 c2)
{
	long a1 = (long)((c1 >> 24) & 0xFF);
	long a2 = (long)((c2 >> 24) & 0xFF);
	long r1 = ((long)((c1 >> 16) & 0xFF) * a1) / 255;
	long g1 = ((long)((c1 >> 8) & 0xFF) * a1) / 255;
	long b1 = ((long)(c1 & 0xFF) * a1) / 255;
	long r2 = ((long)((c2 >> 16) & 0xFF) * a2) / 255;
	long g2 = ((long)((c2 >> 8) & 0xFF) * a2) / 255;
	long b2 = ((long)(c2 & 0xFF) * a2) / 255;
	long dr = r1 - r2, dg = g1 - g2, db = b1 - b2, da = a1 - a2;
	return dr * dr * 2 + dg * dg * 4 + db * db + da * da * 2;
}

static pixel32 ff_lerp_premul_argb(pixel32 c1, pixel32 c2, real t)
{
	real a1 = (real)((c1 >> 24) & 0xFF);
	real a2 = (real)((c2 >> 24) & 0xFF);
	real a = a1 + (a2 - a1) * t;
	long ai = (long)(a + 0.5f);
	real r1, g1, b1, r2, g2, b2;
	long r, g, b;

	if (ai <= 0)
		return 0;
	r1 = (real)((c1 >> 16) & 0xFF) * a1;
	g1 = (real)((c1 >> 8) & 0xFF) * a1;
	b1 = (real)(c1 & 0xFF) * a1;
	r2 = (real)((c2 >> 16) & 0xFF) * a2;
	g2 = (real)((c2 >> 8) & 0xFF) * a2;
	b2 = (real)(c2 & 0xFF) * a2;
	r = PIN((long)((r1 + (r2 - r1) * t) / a + 0.5f), 0L, 255L);
	g = PIN((long)((g1 + (g2 - g1) * t) / a + 0.5f), 0L, 255L);
	b = PIN((long)((b1 + (b2 - b1) * t) / a + 0.5f), 0L, 255L);
	ai = PIN(ai, 0L, 255L);
	return (pixel32)(((unsigned long)ai << 24) | ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b);
}

static pixel32 ff_grid_get_clamped(pixel32 const *grid, int w, int h, int x, int y)
{
	if (x < 0) x = 0;
	else if (x >= w) x = w - 1;
	if (y < 0) y = 0;
	else if (y >= h) y = h - 1;
	return grid[y * w + x];
}

static void ff_xbr_2x(pixel32 const *src, int w, int h, pixel32 *dst)
{
	int out_w = w * 2;
	int y, x;

	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			pixel32 E = ff_grid_get_clamped(src, w, h, x, y);
			pixel32 B = ff_grid_get_clamped(src, w, h, x, y - 1);
			pixel32 D = ff_grid_get_clamped(src, w, h, x - 1, y);
			pixel32 F = ff_grid_get_clamped(src, w, h, x + 1, y);
			pixel32 H = ff_grid_get_clamped(src, w, h, x, y + 1);
			pixel32 A = ff_grid_get_clamped(src, w, h, x - 1, y - 1);
			pixel32 C = ff_grid_get_clamped(src, w, h, x + 1, y - 1);
			pixel32 G = ff_grid_get_clamped(src, w, h, x - 1, y + 1);
			pixel32 I = ff_grid_get_clamped(src, w, h, x + 1, y + 1);
			pixel32 e0 = E, e1 = E, e2 = E, e3 = E;

			if (ff_color_dist_argb(B, H) > 400 && ff_color_dist_argb(D, F) > 400)
			{
				if (ff_color_dist_argb(D, B) < ff_color_dist_argb(E, A) &&
					ff_color_dist_argb(E, D) > 100 && ff_color_dist_argb(E, B) > 100)
					e0 = ff_lerp_premul_argb(E, ff_lerp_premul_argb(D, B, 0.5f), 0.75f);
				if (ff_color_dist_argb(B, F) < ff_color_dist_argb(E, C) &&
					ff_color_dist_argb(E, B) > 100 && ff_color_dist_argb(E, F) > 100)
					e1 = ff_lerp_premul_argb(E, ff_lerp_premul_argb(B, F, 0.5f), 0.75f);
				if (ff_color_dist_argb(D, H) < ff_color_dist_argb(E, G) &&
					ff_color_dist_argb(E, D) > 100 && ff_color_dist_argb(E, H) > 100)
					e2 = ff_lerp_premul_argb(E, ff_lerp_premul_argb(D, H, 0.5f), 0.75f);
				if (ff_color_dist_argb(F, H) < ff_color_dist_argb(E, I) &&
					ff_color_dist_argb(E, F) > 100 && ff_color_dist_argb(E, H) > 100)
					e3 = ff_lerp_premul_argb(E, ff_lerp_premul_argb(F, H, 0.5f), 0.75f);
			}
			dst[(y * 2) * out_w + (x * 2)] = e0;
			dst[(y * 2) * out_w + (x * 2 + 1)] = e1;
			dst[(y * 2 + 1) * out_w + (x * 2)] = e2;
			dst[(y * 2 + 1) * out_w + (x * 2 + 1)] = e3;
		}
	}
}

static void ff_smooth_icon_128(pixel32 const *src, pixel32 *dst)
{
	static int const k_dy[9] = { 0, -1, 1, 0, 0, -1, -1, 1, 1 };
	static int const k_dx[9] = { 0, 0, 0, -1, 1, -1, 1, -1, 1 };
	static real const k_w[9] = { 4.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f };
	int y, x, k;

	for (y = 0; y < FF_ICON_HI_DIM; y++)
	{
		for (x = 0; x < FF_ICON_HI_DIM; x++)
		{
			real r_sum = 0.0f, g_sum = 0.0f, b_sum = 0.0f, a_sum = 0.0f, w_sum = 0.0f;
			real a_avg;

			for (k = 0; k < 9; k++)
			{
				pixel32 nc = ff_grid_get_clamped(src, FF_ICON_HI_DIM, FF_ICON_HI_DIM, x + k_dx[k], y + k_dy[k]);
				real na = (real)((nc >> 24) & 0xFF) / 255.0f;
				real w = k_w[k];
				r_sum += (real)((nc >> 16) & 0xFF) * na * w;
				g_sum += (real)((nc >> 8) & 0xFF) * na * w;
				b_sum += (real)(nc & 0xFF) * na * w;
				a_sum += (real)((nc >> 24) & 0xFF) * w;
				w_sum += w;
			}
			a_avg = a_sum / w_sum;
			if (a_avg < 4.0f)
			{
				dst[y * FF_ICON_HI_DIM + x] = 0;
			}
			else
			{
				real a_norm = a_avg / 255.0f;
				long r = PIN((long)((r_sum / w_sum) / a_norm + 0.5f), 0L, 255L);
				long g = PIN((long)((g_sum / w_sum) / a_norm + 0.5f), 0L, 255L);
				long b = PIN((long)((b_sum / w_sum) / a_norm + 0.5f), 0L, 255L);
				long a = PIN((long)(a_avg + 0.5f), 0L, 255L);
				dst[y * FF_ICON_HI_DIM + x] = (pixel32)(((unsigned long)a << 24) |
					((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b);
			}
		}
	}
}

struct ff_metal_palette
{
	unsigned char light[3];
	unsigned char mid[3];
	unsigned char dark[3];
};

static struct ff_metal_palette const s_ff_metal_bronze = { { 248, 188, 128 }, { 198, 122, 58 }, { 108, 56, 22 } };
static struct ff_metal_palette const s_ff_metal_silver = { { 248, 252, 255 }, { 186, 198, 212 }, { 98, 110, 126 } };
static struct ff_metal_palette const s_ff_metal_gold   = { { 255, 242, 146 }, { 228, 178, 52 },  { 132, 88, 16 } };
static struct ff_metal_palette const s_ff_metal_cyan   = { { 195, 254, 255 }, { 68, 212, 238 },  { 22, 108, 138 } };

struct ff_emblem_sdf_layer
{
	real sdf;
	struct ff_metal_palette const *metal;
};

static real ff_sd_segment(real px, real py, real ax, real ay, real bx, real by)
{
	real vx = bx - ax, vy = by - ay;
	real wx = px - ax, wy = py - ay;
	real c1 = vx * wx + vy * wy;
	real c2 = vx * vx + vy * vy;
	real t = (c2 > 1e-8f) ? PIN(c1 / c2, 0.0f, 1.0f) : 0.0f;
	real dx = px - (ax + t * vx);
	real dy = py - (ay + t * vy);
	return square_root(dx * dx + dy * dy);
}

static real ff_eval_chevron_sdf(real ex, real ey, real cy, real arm_x, real drop_y, real thick, boolean upward)
{
	real ax = (ex >= 0.0f) ? ex : -ex;
	real d = upward
		? ff_sd_segment(ax, ey, 0.0f, cy - drop_y, arm_x, cy)
		: ff_sd_segment(ax, ey, 0.0f, cy + drop_y, arm_x, cy);
	return thick - d;
}

static real ff_eval_diamond_sdf(real ex, real ey, real cy, real rad, real hole_rad)
{
	real ax = (ex >= 0.0f) ? ex : -ex;
	real ay = (ey - cy >= 0.0f) ? (ey - cy) : -(ey - cy);
	real d_outer = (rad - (ax + ay)) * 0.7071f;
	if (hole_rad > 0.0f)
	{
		real d_inner = ((ax + ay) - hole_rad) * 0.7071f;
		return MIN(d_outer, d_inner);
	}
	return d_outer;
}

static int ff_eval_mcc_rank_emblem(short r, real ex, real ey, struct ff_emblem_sdf_layer out_layers[4])
{
	int n = 0;

	if (r == 0)
	{
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, 0.0f, 0.145f, 0.048f);
		out_layers[n++].metal = &s_ff_metal_silver;
	}
	else if (r == 1)
	{
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.01f, 0.165f, 0.135f, 0.046f, FALSE);
		out_layers[n++].metal = &s_ff_metal_bronze;
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.055f, 0.125f, 0.042f);
		out_layers[n++].metal = &s_ff_metal_silver;
	}
	else if (r >= 2 && r <= 4)
	{
		if (r == 2)
		{
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.04f, 0.185f, 0.145f, 0.054f, FALSE);
			out_layers[n++].metal = &s_ff_metal_bronze;
		}
		else if (r == 3)
		{
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.10f, 0.185f, 0.145f, 0.048f, FALSE);
			out_layers[n++].metal = &s_ff_metal_bronze;
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.03f, 0.185f, 0.145f, 0.048f, FALSE);
			out_layers[n++].metal = &s_ff_metal_bronze;
		}
		else
		{
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.075f, 0.185f, 0.145f, 0.046f, FALSE);
			out_layers[n++].metal = &s_ff_metal_bronze;
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.05f, 0.185f, 0.145f, 0.046f, FALSE);
			out_layers[n++].metal = &s_ff_metal_bronze;
			out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.135f, 0.075f, 0.0f);
			out_layers[n++].metal = &s_ff_metal_bronze;
		}
	}
	else if (r >= 5 && r <= 7)
	{
		if (r == 5)
		{
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.04f, 0.185f, 0.145f, 0.054f, FALSE);
			out_layers[n++].metal = &s_ff_metal_silver;
		}
		else if (r == 6)
		{
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.10f, 0.185f, 0.145f, 0.048f, FALSE);
			out_layers[n++].metal = &s_ff_metal_silver;
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.03f, 0.185f, 0.145f, 0.048f, FALSE);
			out_layers[n++].metal = &s_ff_metal_silver;
		}
		else
		{
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.075f, 0.185f, 0.145f, 0.046f, FALSE);
			out_layers[n++].metal = &s_ff_metal_silver;
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.05f, 0.185f, 0.145f, 0.046f, FALSE);
			out_layers[n++].metal = &s_ff_metal_silver;
			out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.135f, 0.075f, 0.0f);
			out_layers[n++].metal = &s_ff_metal_silver;
		}
	}
	else if (r == 8)
	{
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, 0.0f, 0.21f, 0.068f);
		out_layers[n++].metal = &s_ff_metal_silver;
	}
	else if (r == 9)
	{
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.05f, 0.175f, 0.14f, 0.046f, FALSE);
		out_layers[n++].metal = &s_ff_metal_silver;
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.045f, 0.185f, 0.058f);
		out_layers[n++].metal = &s_ff_metal_silver;
	}
	else if (r == 10)
	{
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, 0.065f, 0.125f, 0.0f);
		out_layers[n++].metal = &s_ff_metal_bronze;
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.07f, 0.185f, 0.145f, 0.050f, FALSE);
		out_layers[n++].metal = &s_ff_metal_silver;
	}
	else if (r == 11)
	{
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.01f, 0.175f, 0.135f, 0.043f, FALSE);
		out_layers[n++].metal = &s_ff_metal_silver;
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.115f, 0.175f, 0.135f, 0.043f, FALSE);
		out_layers[n++].metal = &s_ff_metal_silver;
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.075f, 0.175f, 0.055f);
		out_layers[n++].metal = &s_ff_metal_silver;
	}
	else if (r >= 12 && r <= 14)
	{
		if (r == 12)
		{
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.04f, 0.185f, 0.145f, 0.054f, FALSE);
			out_layers[n++].metal = &s_ff_metal_gold;
		}
		else if (r == 13)
		{
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.10f, 0.185f, 0.145f, 0.048f, FALSE);
			out_layers[n++].metal = &s_ff_metal_gold;
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.03f, 0.185f, 0.145f, 0.048f, FALSE);
			out_layers[n++].metal = &s_ff_metal_gold;
		}
		else
		{
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.075f, 0.185f, 0.145f, 0.046f, FALSE);
			out_layers[n++].metal = &s_ff_metal_gold;
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.05f, 0.185f, 0.145f, 0.046f, FALSE);
			out_layers[n++].metal = &s_ff_metal_gold;
			out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.135f, 0.075f, 0.0f);
			out_layers[n++].metal = &s_ff_metal_gold;
		}
	}
	else if (r == 15)
	{
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, 0.0f, 0.21f, 0.068f);
		out_layers[n++].metal = &s_ff_metal_gold;
	}
	else if (r == 16)
	{
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.05f, 0.175f, 0.14f, 0.046f, FALSE);
		out_layers[n++].metal = &s_ff_metal_gold;
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.045f, 0.185f, 0.058f);
		out_layers[n++].metal = &s_ff_metal_gold;
	}
	else if (r == 17)
	{
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.01f, 0.175f, 0.135f, 0.043f, FALSE);
		out_layers[n++].metal = &s_ff_metal_gold;
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.115f, 0.175f, 0.135f, 0.043f, FALSE);
		out_layers[n++].metal = &s_ff_metal_gold;
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.075f, 0.175f, 0.055f);
		out_layers[n++].metal = &s_ff_metal_gold;
	}
	else if (r == 18)
	{
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, 0.07f, 0.125f, 0.0f);
		out_layers[n++].metal = &s_ff_metal_gold;
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.06f, 0.185f, 0.14f, 0.048f, FALSE);
		out_layers[n++].metal = &s_ff_metal_gold;
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.125f, 0.068f, 0.0f);
		out_layers[n++].metal = &s_ff_metal_gold;
	}
	else if (r == 19)
	{
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.06f, 0.175f, 0.135f, 0.042f, TRUE);
		out_layers[n++].metal = &s_ff_metal_gold;
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.06f, 0.175f, 0.135f, 0.042f, FALSE);
		out_layers[n++].metal = &s_ff_metal_gold;
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, 0.0f, 0.185f, 0.058f);
		out_layers[n++].metal = &s_ff_metal_gold;
	}
	else if (r >= 20 && r <= 22)
	{
		real cy = (r == 21) ? -0.03f : 0.0f;
		if (r == 21)
		{
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.06f, 0.18f, 0.14f, 0.045f, FALSE);
			out_layers[n++].metal = &s_ff_metal_gold;
		}
		else if (r == 22)
		{
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.06f, 0.18f, 0.14f, 0.044f, TRUE);
			out_layers[n++].metal = &s_ff_metal_gold;
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.06f, 0.18f, 0.14f, 0.044f, FALSE);
			out_layers[n++].metal = &s_ff_metal_gold;
		}
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, cy, 0.21f, 0.0f);
		out_layers[n++].metal = &s_ff_metal_gold;
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, cy, 0.11f, 0.0f);
		out_layers[n++].metal = &s_ff_metal_cyan;
	}
	else if (r == 23)
	{
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.08f, 0.17f, 0.11f, 0.042f, TRUE);
		out_layers[n++].metal = &s_ff_metal_cyan;
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.06f, 0.18f, 0.135f, 0.048f, FALSE);
		out_layers[n++].metal = &s_ff_metal_gold;
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.125f, 0.068f, 0.0f);
		out_layers[n++].metal = &s_ff_metal_gold;
	}
	else if (r == 24)
	{
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.12f, 0.17f, 0.11f, 0.042f, TRUE);
		out_layers[n++].metal = &s_ff_metal_cyan;
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.03f, 0.195f, 0.058f);
		out_layers[n++].metal = &s_ff_metal_gold;
	}
	else if (r == 25)
	{
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, -0.06f, 0.20f, 0.14f, 0.044f, TRUE);
		out_layers[n++].metal = &s_ff_metal_silver;
		out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.12f, 0.17f, 0.11f, 0.042f, TRUE);
		out_layers[n++].metal = &s_ff_metal_cyan;
		out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, 0.0f, 0.195f, 0.058f);
		out_layers[n++].metal = &s_ff_metal_gold;
	}
	else
	{
		real dist = square_root(ex * ex + (ey - 0.01f) * (ey - 0.01f));
		real ad = (dist - 0.195f >= 0.0f) ? (dist - 0.195f) : -(dist - 0.195f);
		out_layers[n].sdf = (ey > -0.13f) ? (0.044f - ad) : -1.0f;
		out_layers[n++].metal = &s_ff_metal_gold;
		if (r == 26)
		{
			out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, 0.0f, 0.155f, 0.0f);
			out_layers[n++].metal = &s_ff_metal_cyan;
		}
		else if (r == 27)
		{
			out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, 0.03f, 0.135f, 0.0f);
			out_layers[n++].metal = &s_ff_metal_cyan;
			out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.15f, 0.095f, 0.0f);
			out_layers[n++].metal = &s_ff_metal_cyan;
		}
		else if (r == 28)
		{
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.02f, 0.145f, 0.105f, 0.044f, FALSE);
			out_layers[n++].metal = &s_ff_metal_cyan;
			out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.14f, 0.11f, 0.0f);
			out_layers[n++].metal = &s_ff_metal_cyan;
		}
		else
		{
			out_layers[n].sdf = ff_eval_chevron_sdf(ex, ey, 0.04f, 0.155f, 0.12f, 0.046f, FALSE);
			out_layers[n++].metal = &s_ff_metal_cyan;
			out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.04f, 0.12f, 0.0f);
			out_layers[n++].metal = &s_ff_metal_silver;
			out_layers[n].sdf = ff_eval_diamond_sdf(ex, ey, -0.16f, 0.11f, 0.0f);
			out_layers[n++].metal = &s_ff_metal_cyan;
		}
	}
	return n;
}

static boolean ff_draw_hw_icon_pixels(
	unsigned long key,
	pixel32 const *pixels_128x128,
	short x0,
	short y0,
	short size,
	real alpha)
{
	struct ff_hw_icon_entry *entry = NULL;
	int i;

	if (!global_d3d_device)
		return FALSE;

	for (i = 0; i < 64; i++)
	{
		if (s_hw_icon_cache[i].valid &&
			s_hw_icon_cache[i].key == key &&
			s_hw_icon_cache[i].bitmap.hardware_format != NULL)
		{
			entry = &s_hw_icon_cache[i];
			break;
		}
	}

	if (!entry && pixels_128x128)
	{
		static pixel32 s_mip64[64 * 64];
		static pixel32 s_mip32[32 * 32];
		static pixel32 s_mip16[16 * 16];
		pixel32 const *mip_ptrs[FF_ICON_HI_MIPS];
		short mip_dims[FF_ICON_HI_MIPS] = { 128, 64, 32, 16 };
		short mip;

		entry = &s_hw_icon_cache[s_hw_icon_next];
		s_hw_icon_next = (s_hw_icon_next + 1) & 63;

		if (!entry->bitmap.hardware_format)
		{
			csmemset(&entry->bitmap, 0, sizeof(entry->bitmap));
			entry->bitmap.signature = 'bitm';
			entry->bitmap.width = FF_ICON_HI_DIM;
			entry->bitmap.height = FF_ICON_HI_DIM;
			entry->bitmap.depth = 1;
			entry->bitmap.type = 0; /* _bitmap_type_2d */
			entry->bitmap.format = 11; /* _bitmap_format_a8r8g8b8 */
			entry->bitmap.flags = 1; /* _bitmap_has_power_of_two_dimensions_bit */
			entry->bitmap.mipmap_count = FF_ICON_HI_MIPS - 1;
			entry->bitmap.pixels_offset = 0;
			entry->bitmap.pixels_size = FF_ICON_HI_DIM * FF_ICON_HI_DIM * 4;
			entry->bitmap.tag_index = NONE;
			entry->bitmap.cache_block_index = NONE;
			if (IDirect3DDevice8_CreateTexture(
					global_d3d_device,
					FF_ICON_HI_DIM,
					FF_ICON_HI_DIM,
					FF_ICON_HI_MIPS,
					0,
					D3DFMT_A8R8G8B8,
					D3DPOOL_MANAGED,
					(IDirect3DTexture8 **)&entry->bitmap.hardware_format) < 0)
			{
				entry->bitmap.hardware_format = NULL;
				return FALSE;
			}
		}

		ff_downsample_2x(pixels_128x128, 128, s_mip64);
		ff_downsample_2x(s_mip64, 64, s_mip32);
		ff_downsample_2x(s_mip32, 32, s_mip16);
		mip_ptrs[0] = pixels_128x128;
		mip_ptrs[1] = s_mip64;
		mip_ptrs[2] = s_mip32;
		mip_ptrs[3] = s_mip16;

		for (mip = 0; mip < FF_ICON_HI_MIPS; mip++)
		{
			D3DLOCKED_RECT locked_rect;
			csmemset(&locked_rect, 0, sizeof(locked_rect));
			if (IDirect3DTexture8_LockRect(
					(IDirect3DTexture8 *)entry->bitmap.hardware_format,
					(UINT)mip,
					&locked_rect,
					NULL,
					D3DLOCK_NOOVERWRITE) >= 0 && locked_rect.pBits)
			{
				rasterizer_xbox_bitmap_swizzle2d_long(
					locked_rect.pBits,
					mip_ptrs[mip],
					mip_dims[mip],
					mip_dims[mip]);
				IDirect3DTexture8_UnlockRect(
					(IDirect3DTexture8 *)entry->bitmap.hardware_format,
					(UINT)mip);
			}
			else
			{
				return FALSE;
			}
		}

		entry->valid = TRUE;
		entry->key = key;
	}

	if (entry && entry->bitmap.hardware_format)
	{
		rectangle2d rect;
		short dim = MAX(8, size);
		pixel32 argb = ff_apply_alpha(0xFFFFFFFFUL, PIN(alpha, 0.0f, 1.0f));
		rect.x0 = x0;
		rect.y0 = y0;
		rect.x1 = (short)(x0 + dim);
		rect.y1 = (short)(y0 + dim);
		draw_bitmap_in_rect(&entry->bitmap, &rect, NULL, NULL, argb, NULL, TRUE);
		return TRUE;
	}

	return FALSE;
}

struct ff_cached_icon_rect
{
	unsigned char x0, y0, x1, y1;
	pixel32 col;
};

struct ff_cached_icon_mesh
{
	boolean valid;
	unsigned long key;
	short rect_count;
	struct ff_cached_icon_rect rects[FF_ICON_MAX_RECTS];
};

static struct ff_cached_icon_mesh s_rank_mesh_cache[8];
static int s_rank_mesh_next = 0;

static struct ff_cached_icon_mesh s_emblem_mesh_cache[8];
static int s_emblem_mesh_next = 0;

static void ff_build_merged_icon_mesh(
	pixel32 grid[FF_ICON_GRID_DIM][FF_ICON_GRID_DIM],
	struct ff_cached_icon_mesh *out_mesh,
	unsigned long key)
{
	short gy, gx;
	out_mesh->valid = TRUE;
	out_mesh->key = key;
	out_mesh->rect_count = 0;

	for (gy = 0; gy < FF_ICON_GRID_DIM; gy++)
	{
		for (gx = 0; gx < FF_ICON_GRID_DIM; gx++)
		{
			pixel32 c = grid[gy][gx];
			short gx1, gy1, tx, cy;
			if (c == 0)
				continue;

			gx1 = (short)(gx + 1);
			while (gx1 < FF_ICON_GRID_DIM && grid[gy][gx1] == c)
				gx1++;

			gy1 = (short)(gy + 1);
			while (gy1 < FF_ICON_GRID_DIM)
			{
				boolean row_matches = TRUE;
				for (tx = gx; tx < gx1; tx++)
				{
					if (grid[gy1][tx] != c)
					{
						row_matches = FALSE;
						break;
					}
				}
				if (!row_matches)
					break;
				gy1++;
			}

			for (cy = gy; cy < gy1; cy++)
			{
				for (tx = gx; tx < gx1; tx++)
					grid[cy][tx] = 0;
			}

			if (out_mesh->rect_count < FF_ICON_MAX_RECTS)
			{
				struct ff_cached_icon_rect *r = &out_mesh->rects[out_mesh->rect_count++];
				r->x0 = (unsigned char)gx;
				r->y0 = (unsigned char)gy;
				r->x1 = (unsigned char)gx1;
				r->y1 = (unsigned char)gy1;
				r->col = c;
			}
		}
	}
}

static void ff_draw_cached_icon_mesh(
	struct ff_cached_icon_mesh const *mesh,
	short x0,
	short y0,
	short size,
	real alpha)
{
	short w = MAX(8, size);
	short h = MAX(8, size);
	short i;

	for (i = 0; i < mesh->rect_count; i++)
	{
		struct ff_cached_icon_rect const *r = &mesh->rects[i];
		rectangle2d q;
		q.x0 = (short)(x0 + ((int)r->x0 * (int)w + (FF_ICON_GRID_DIM / 2)) / FF_ICON_GRID_DIM);
		q.x1 = (short)(x0 + ((int)r->x1 * (int)w + (FF_ICON_GRID_DIM / 2)) / FF_ICON_GRID_DIM);
		q.y0 = (short)(y0 + ((int)r->y0 * (int)h + (FF_ICON_GRID_DIM / 2)) / FF_ICON_GRID_DIM);
		q.y1 = (short)(y0 + ((int)r->y1 * (int)h + (FF_ICON_GRID_DIM / 2)) / FF_ICON_GRID_DIM);
		if (q.x1 > q.x0 && q.y1 > q.y0)
		{
			pixel32 col = (alpha < 0.999f) ? ff_apply_alpha(r->col, alpha) : r->col;
			draw_quad(&q, col);
		}
	}
}

void firefight_draw_mcc_rank_icon(
	short x0,
	short y0,
	short size,
	short tour_number,
	short rank_in_tour,
	real alpha)
{
	short t_idx = (short)PIN(tour_number - 1, 0, 10);
	short r_idx = (short)PIN(rank_in_tour - 1, 0, 29);
	unsigned long key = ((unsigned long)t_idx << 8) | (unsigned long)r_idx | 0x80000000UL;
	unsigned long const *pal = s_mcc_tour_palettes[t_idx];
	unsigned char const *idx_buf = s_mcc_rank_icon_indices[t_idx][r_idx];
	struct ff_cached_icon_mesh *mesh = NULL;
	int i;

	if (global_d3d_device)
	{
		if (ff_draw_hw_icon_pixels(key, NULL, x0, y0, size, alpha))
			return;
		{
			static pixel32 s_shield32[32 * 32];
			static pixel32 s_buf64[64 * 64];
			static pixel32 s_raw128[FF_ICON_HI_DIM * FF_ICON_HI_DIM];
			static pixel32 s_pixels128[FF_ICON_HI_DIM * FF_ICON_HI_DIM];
			unsigned char const *r0_buf = s_mcc_rank_icon_indices[t_idx][0];
			pixel32 cav_top = (pixel32)pal[r0_buf[8 * 32 + 12]];
			pixel32 cav_bot = (pixel32)pal[r0_buf[9 * 32 + 11]];
			real inv = 2.0f / (real)FF_ICON_HI_DIM;
			int py, px, li;

			for (py = 0; py < 32; py++)
			{
				for (px = 0; px < 32; px++)
				{
					int sx = (px < 16) ? px : (31 - px);
					if (py >= 8 && py <= 20 && sx >= 10 && (py + (15 - sx)) <= 22)
					{
						real gt = (real)(py - 8) / 12.0f;
						s_shield32[py * 32 + px] = ff_lerp_premul_argb(cav_top, cav_bot, gt);
					}
					else
					{
						s_shield32[py * 32 + px] = (pixel32)pal[r0_buf[py * 32 + sx]];
					}
				}
			}

			ff_xbr_2x(s_shield32, 32, 32, s_buf64);
			ff_xbr_2x(s_buf64, 64, 64, s_raw128);
			ff_smooth_icon_128(s_raw128, s_pixels128);

			for (py = 0; py < FF_ICON_HI_DIM; py++)
			{
				real ny = ((real)py + 0.5f) * inv - 1.0f;
				real ey = (ny + 0.125f) / 1.22f;

				for (px = 0; px < FF_ICON_HI_DIM; px++)
				{
					pixel32 base = s_pixels128[py * FF_ICON_HI_DIM + px];
					unsigned long ba = (base >> 24) & 0xFF;
					real nx, ex;
					real cr, cg, cb;
					struct ff_emblem_sdf_layer layers[4];
					int n_layers;

					if (ba == 0)
						continue;

					nx = ((real)px + 0.5f) * inv - 1.0f;
					ex = nx / 1.22f;
					cr = (real)((base >> 16) & 0xFF);
					cg = (real)((base >> 8) & 0xFF);
					cb = (real)(base & 0xFF);
					n_layers = ff_eval_mcc_rank_emblem(r_idx, ex, ey, layers);

					for (li = 0; li < n_layers; li++)
					{
						real esd = layers[li].sdf;
						if (esd > -0.028f)
						{
							if (esd < 0.0f)
							{
								real t_sh = (esd + 0.028f) / 0.028f;
								real mul = 1.0f - 0.72f * t_sh;
								cr *= mul;
								cg *= mul;
								cb *= mul;
							}
							else
							{
								real t_cov = MIN(1.0f, esd / 0.018f);
								struct ff_metal_palette const *mp = layers[li].metal;
								unsigned char const *ecol;
								real elight = -0.6f * ex - 0.8f * ey;

								if (esd < 0.032f && elight > 0.02f)
									ecol = mp->light;
								else if (esd < 0.032f && elight < -0.02f)
									ecol = mp->dark;
								else
									ecol = (elight < 0.12f) ? mp->mid : mp->light;

								cr = cr * (1.0f - t_cov) + (real)ecol[0] * t_cov;
								cg = cg * (1.0f - t_cov) + (real)ecol[1] * t_cov;
								cb = cb * (1.0f - t_cov) + (real)ecol[2] * t_cov;
							}
						}
					}

					s_pixels128[py * FF_ICON_HI_DIM + px] = (pixel32)(
						(ba << 24) |
						((unsigned long)PIN((long)(cr + 0.5f), 0L, 255L) << 16) |
						((unsigned long)PIN((long)(cg + 0.5f), 0L, 255L) << 8) |
						(unsigned long)PIN((long)(cb + 0.5f), 0L, 255L));
				}
			}

			if (ff_draw_hw_icon_pixels(key, s_pixels128, x0, y0, size, alpha))
				return;
		}
	}

	for (i = 0; i < 8; i++)
	{
		if (s_rank_mesh_cache[i].valid && s_rank_mesh_cache[i].key == key)
		{
			mesh = &s_rank_mesh_cache[i];
			break;
		}
	}

	if (!mesh)
	{
		pixel32 grid[FF_ICON_GRID_DIM][FF_ICON_GRID_DIM];
		short py, px;

		for (py = 0; py < FF_ICON_GRID_DIM; py++)
		{
			for (px = 0; px < FF_ICON_GRID_DIM; px++)
			{
				int sy = py * 2 + (py >= (FF_ICON_GRID_DIM / 2) ? 1 : 0);
				int sx = px * 2 + (px >= (FF_ICON_GRID_DIM / 2) ? 1 : 0);
				grid[py][px] = (pixel32)pal[idx_buf[sy * FF_ICON_TEX_DIM + sx]];
			}
		}

		mesh = &s_rank_mesh_cache[s_rank_mesh_next];
		s_rank_mesh_next = (s_rank_mesh_next + 1) & 7;
		ff_build_merged_icon_mesh(grid, mesh, key);
	}

	ff_draw_cached_icon_mesh(mesh, x0, y0, size, alpha);
}

void firefight_draw_h3_ranked_icon(
	short x0,
	short y0,
	short size,
	short ranked_index,
	real alpha)
{
	short idx = (short)PIN(ranked_index, 0, 41);
	unsigned long key = 0x20000000UL | (unsigned long)idx;
	unsigned long const *pal = s_h3_ranked_palette;
	unsigned char const *idx_buf = s_h3_ranked_icon_indices[idx];
	struct ff_cached_icon_mesh *mesh = NULL;
	int i;

	if (global_d3d_device)
	{
		if (ff_draw_hw_icon_pixels(key, NULL, x0, y0, size, alpha))
			return;
		{
			static pixel32 s_src32[32 * 32];
			static pixel32 s_buf64[64 * 64];
			static pixel32 s_raw128[FF_ICON_HI_DIM * FF_ICON_HI_DIM];
			static pixel32 s_pixels128[FF_ICON_HI_DIM * FF_ICON_HI_DIM];

			for (i = 0; i < 32 * 32; i++)
				s_src32[i] = (pixel32)pal[idx_buf[i]];
			ff_xbr_2x(s_src32, 32, 32, s_buf64);
			ff_xbr_2x(s_buf64, 64, 64, s_raw128);
			ff_smooth_icon_128(s_raw128, s_pixels128);
			if (ff_draw_hw_icon_pixels(key, s_pixels128, x0, y0, size, alpha))
				return;
		}
	}

	for (i = 0; i < 8; i++)
	{
		if (s_rank_mesh_cache[i].valid && s_rank_mesh_cache[i].key == key)
		{
			mesh = &s_rank_mesh_cache[i];
			break;
		}
	}

	if (!mesh)
	{
		pixel32 grid[FF_ICON_GRID_DIM][FF_ICON_GRID_DIM];
		short py, px;

		for (py = 0; py < FF_ICON_GRID_DIM; py++)
		{
			for (px = 0; px < FF_ICON_GRID_DIM; px++)
			{
				int sy = py * 2 + (py >= (FF_ICON_GRID_DIM / 2) ? 1 : 0);
				int sx = px * 2 + (px >= (FF_ICON_GRID_DIM / 2) ? 1 : 0);
				grid[py][px] = (pixel32)pal[idx_buf[sy * FF_ICON_TEX_DIM + sx]];
			}
		}

		mesh = &s_rank_mesh_cache[s_rank_mesh_next];
		s_rank_mesh_next = (s_rank_mesh_next + 1) & 7;
		ff_build_merged_icon_mesh(grid, mesh, key);
	}

	ff_draw_cached_icon_mesh(mesh, x0, y0, size, alpha);
}

static boolean mcc_eval_emblem_bg(short bg_index, real nx, real ny, boolean *out_is_border)
{
	real ax = (nx >= 0.0f) ? nx : -nx;
	real ay = (ny >= 0.0f) ? ny : -ny;
	real dist = square_root(nx * nx + ny * ny);
	real d = 1.0f;

	*out_is_border = FALSE;
	switch (bg_index % 12)
	{
	case 0: /* None */
		return FALSE;
	case 1: /* Shield */
	{
		real max_x = 0.84f;
		if (ny < -0.55f)
			max_x = 0.84f - (-0.55f - ny) * 0.6f;
		else if (ny > 0.15f)
			max_x = 0.84f - (ny - 0.15f) * 0.92f;
		if (ny < -0.88f || ny > 0.92f || ax > max_x)
			return FALSE;
		d = MIN(max_x - ax, MIN(ny + 0.88f, 0.92f - ny));
		*out_is_border = (d < 0.11f);
		return TRUE;
	}
	case 2: /* Circle */
		if (dist > 0.90f)
			return FALSE;
		*out_is_border = (dist > 0.78f);
		return TRUE;
	case 3: /* Diamond */
		d = 0.95f - (ax + ay);
		if (d < 0.0f)
			return FALSE;
		*out_is_border = (d < 0.12f);
		return TRUE;
	case 4: /* Octagon */
		d = MIN(0.86f - MAX(ax, ay), 1.20f - (ax + ay));
		if (d < 0.0f)
			return FALSE;
		*out_is_border = (d < 0.10f);
		return TRUE;
	case 5: /* Hexagon */
		d = MIN(0.85f - ax, 1.00f - (ax * 0.55f + ay));
		if (d < 0.0f)
			return FALSE;
		*out_is_border = (d < 0.10f);
		return TRUE;
	case 6: /* Maltese Cross */
		if ((ax <= 0.34f + ay * 0.25f && ay <= 0.88f) ||
			(ay <= 0.34f + ax * 0.25f && ax <= 0.88f))
		{
			*out_is_border = (MAX(ax, ay) > 0.78f);
			return TRUE;
		}
		return FALSE;
	case 7: /* Starburst (8-point star) */
		if ((ax <= 0.65f && ay <= 0.65f) || (ax + ay <= 0.92f))
		{
			*out_is_border = (MAX(ax, ay) > 0.56f && (ax + ay) > 0.82f);
			return TRUE;
		}
		return FALSE;
	case 8: /* Banner */
		if (ax <= 0.74f && ny >= -0.88f && ny <= 0.88f - (0.35f - ax * 0.45f))
		{
			*out_is_border = (ax > 0.64f || ny < -0.78f);
			return TRUE;
		}
		return FALSE;
	case 9: /* Crown Plate */
		if (ax <= 0.84f && ny >= -0.75f + (ax > 0.25f && ax < 0.55f ? 0.22f : 0.0f) && ny <= 0.84f)
		{
			*out_is_border = (ax > 0.74f || ny > 0.74f);
			return TRUE;
		}
		return FALSE;
	case 10: /* Wings */
		if ((ax <= 0.88f && ny >= -0.50f - ax * 0.25f && ny <= 0.45f - ax * 0.35f) || dist <= 0.55f)
		{
			*out_is_border = (dist > 0.46f && ax > 0.76f);
			return TRUE;
		}
		return FALSE;
	case 11: /* Cog / Gear */
	default:
		if (dist <= 0.74f || (dist <= 0.90f && (ax < 0.22f || ay < 0.22f || fabsf(ax - ay) < 0.22f)))
		{
			*out_is_border = (dist > 0.64f);
			return TRUE;
		}
		return FALSE;
	}
}

static int mcc_eval_emblem_fg(short fg_index, boolean toggle_sec, real nx, real ny)
{
	/* Returns: 0 = none, 1 = primary layer, 2 = secondary detail layer */
	real ax = (nx >= 0.0f) ? nx : -nx;
	real ay = (ny >= 0.0f) ? ny : -ny;
	real dist = square_root(nx * nx + ny * ny);

	switch (fg_index % 16)
	{
	case 0: /* Spartan Helmet */
		if (toggle_sec && ax <= 0.36f && ny >= -0.18f && ny <= 0.08f && (ny >= -0.06f || ax >= 0.10f))
			return 2; /* Gold Visor */
		if (toggle_sec && ax <= 0.12f && ny >= -0.58f && ny <= -0.24f)
			return 2; /* Helmet Crest */
		if (ax <= 0.46f && ny >= -0.48f && ny <= 0.46f && (ax * 0.9f + ay * 0.8f <= 0.62f))
		{
			if (ax <= 0.14f && ny >= 0.18f && ny <= 0.34f)
				return toggle_sec ? 2 : 0; /* Mouth vent */
			return 1;
		}
		break;

	case 1: /* Legendary Skull & Crossed Swords */
		if (toggle_sec && fabsf(ax - ay) <= 0.09f && ax <= 0.62f)
			return 2; /* Crossed swords behind skull */
		if (square_root(nx * nx + (ny + 0.08f) * (ny + 0.08f)) <= 0.36f ||
			(ax <= 0.20f && ny >= 0.14f && ny <= 0.44f))
		{
			if ((fabsf(ax - 0.13f) <= 0.07f && fabsf(ny + 0.05f) <= 0.07f) ||
				(ax <= 0.05f && fabsf(ny - 0.10f) <= 0.06f))
				return 0; /* Eyes & nose cavity */
			return 1;
		}
		break;

	case 2: /* Valkyrie (Winged Sword) */
		if (ax <= 0.08f && ny >= -0.62f && ny <= 0.62f)
			return 1; /* Sword blade */
		if (ax <= 0.28f && fabsf(ny - 0.22f) <= 0.07f)
			return 1; /* Crossguard */
		if (toggle_sec && ax >= 0.12f && ax <= 0.60f &&
			ny >= -0.35f + (ax - 0.12f) * 0.25f && ny <= 0.15f - (ax - 0.12f) * 0.35f)
			return 2; /* Wings */
		break;

	case 3: /* ODST Helljumper */
		if (toggle_sec && ax <= 0.16f && ny >= -0.64f && ny <= -0.28f)
			return 2; /* Drop trail */
		if (ax <= 0.44f && ny >= -0.32f && ny <= 0.48f && (ax + (ny > 0.1f ? (ny - 0.1f) * 0.6f : 0.0f) <= 0.50f))
		{
			if (ax <= 0.32f && ny >= -0.14f && ny <= 0.08f)
				return toggle_sec ? 2 : 0; /* ODST Visor */
			return 1;
		}
		break;

	case 4: /* Mark of Shame / Covenant Glyph */
		if (toggle_sec && dist >= 0.48f && dist <= 0.58f && ny <= 0.35f)
			return 2;
		if ((ax <= 0.42f && fabsf(ny + 0.10f + ax * 0.4f) <= 0.09f) ||
			(ax <= 0.10f && ny >= -0.45f && ny <= 0.48f))
			return 1;
		break;

	case 5: /* Marathon / Reclaimer */
		if (dist >= 0.42f && dist <= 0.56f)
			return 1;
		{
			real inner_d = square_root(nx * nx + (ny + 0.14f) * (ny + 0.14f));
			if (inner_d >= 0.14f && inner_d <= 0.26f)
				return toggle_sec ? 2 : 1;
			if (toggle_sec && ax <= 0.06f && ny >= 0.10f && ny <= 0.54f)
				return 2;
		}
		break;

	case 6: /* 343 Guilty Spark */
		if (dist <= 0.20f)
			return toggle_sec ? 2 : 1; /* Glowing blue eye */
		if (dist >= 0.28f && dist <= 0.52f)
			return 1;
		if (toggle_sec && ax <= 0.14f && fabsf(ay - 0.54f) <= 0.08f)
			return 2;
		break;

	case 7: /* Energy Sword */
		if (toggle_sec && ax <= 0.20f && fabsf(ny - 0.18f) <= 0.09f)
			return 2; /* Sword hilt */
		if (ax >= 0.04f && ax <= 0.26f - (ny + 0.55f) * 0.14f && ny >= -0.60f && ny <= 0.52f)
			return 1; /* Twin plasma blades */
		break;

	case 8: /* Bulltrue */
		if (toggle_sec && dist >= 0.14f && dist <= 0.24f && ny >= 0.15f)
			return 2; /* Nose ring */
		if (ax >= 0.20f && ax <= 0.58f && ny <= -0.05f && ny >= -0.55f &&
			fabsf((ax - 0.20f) - (-0.05f - ny) * 0.75f) <= 0.12f)
			return 2; /* Horns */
		if (ax <= 0.34f - ny * 0.25f && ny >= -0.22f && ny <= 0.32f)
			return 1;
		break;

	case 9: /* Phoenix / UNSC Eagle */
		if (toggle_sec && mcc_point_in_star(nx, ny, 0.0f, -0.32f, 0.20f))
			return 2;
		if (ax <= 0.60f && ny >= -0.18f - ax * 0.3f && ny <= 0.28f - ax * 0.4f)
			return 1;
		if (ax <= 0.16f && ny >= -0.10f && ny <= 0.48f)
			return 1;
		break;

	case 10: /* Biohazard / Flood */
		if (toggle_sec && dist >= 0.18f && dist <= 0.28f)
			return 2;
		if ((square_root(nx * nx + (ny + 0.26f) * (ny + 0.26f)) <= 0.24f &&
			 square_root(nx * nx + (ny + 0.34f) * (ny + 0.34f)) > 0.14f) ||
			(square_root((ax - 0.24f) * (ax - 0.24f) + (ny - 0.16f) * (ny - 0.16f)) <= 0.24f &&
			 square_root((ax - 0.31f) * (ax - 0.31f) + (ny - 0.22f) * (ny - 0.22f)) > 0.14f))
			return 1;
		break;

	case 11: /* Crosshairs */
		if (dist >= 0.36f && dist <= 0.48f)
			return 1;
		if (toggle_sec && ((ax <= 0.05f && ay <= 0.62f) || (ay <= 0.05f && ax <= 0.62f)))
			return 2;
		if (dist <= 0.10f)
			return 1;
		break;

	case 12: /* Atomic */
		if (toggle_sec && dist <= 0.16f)
			return 2;
		if (fabsf((nx * 0.7f + ny * 0.7f) * (nx * 0.7f + ny * 0.7f) * 3.5f +
			(nx * 0.7f - ny * 0.7f) * (nx * 0.7f - ny * 0.7f) - 0.32f) <= 0.09f)
			return 1;
		break;

	case 13: /* Jolly Roger */
		if (toggle_sec && fabsf(ax - ay * 1.1f) <= 0.08f && ax >= 0.22f && ax <= 0.58f)
			return 2;
		if (dist <= 0.32f || (ax <= 0.16f && ny >= 0.10f && ny <= 0.40f))
		{
			if (fabsf(ax - 0.11f) <= 0.06f && fabsf(ny + 0.04f) <= 0.06f)
				return 0;
			return 1;
		}
		break;

	case 14: /* Triad */
		if (toggle_sec && mcc_point_in_star(nx, ny, 0.0f, 0.0f, 0.22f))
			return 2;
		if (ny >= -0.52f && ny <= 0.42f && ax <= (ny + 0.52f) * 0.62f &&
			(ny > 0.28f || ax >= (ny + 0.52f) * 0.62f - 0.14f))
			return 1;
		break;

	case 15: /* Noble */
	default:
		if (toggle_sec && ax <= 0.09f && ny >= -0.58f && ny <= 0.54f)
			return 2;
		if (mcc_point_in_chevron(nx, ny, -0.35f, 0.54f, 0.15f) ||
			mcc_point_in_chevron(nx, ny, -0.02f, 0.54f, 0.15f))
			return 1;
		break;
	}
	return 0;
}

void firefight_draw_player_emblem(
	short x0,
	short y0,
	short size,
	struct firefight_emblem_info const *emblem,
	real alpha)
{
	struct firefight_emblem_info def_emb = { 0, 1, TRUE, 11, 0, 3, 0 };
	struct firefight_emblem_info const *e = emblem ? emblem : &def_emb;
	unsigned long fg = (unsigned long)(e->foreground & 0x1F);
	unsigned long bg = (unsigned long)(e->background & 0x1F);
	unsigned long sec = e->secondary_layer ? 1UL : 0UL;
	unsigned long c_pri = (unsigned long)PIN(e->primary_color, 0, 17);
	unsigned long c_sec = (unsigned long)PIN(e->secondary_color, 0, 17);
	unsigned long c_bg = (unsigned long)PIN(e->background_color, 0, 17);
	unsigned long key = 0x40000000UL | (fg << 24) | (bg << 19) | (sec << 18) | (c_pri << 12) | (c_sec << 6) | c_bg;
	struct ff_cached_icon_mesh *mesh = NULL;
	int i;

	if (global_d3d_device)
	{
		if (ff_draw_hw_icon_pixels(key, NULL, x0, y0, size, alpha))
			return;
		{
			static pixel32 s_pixels128[FF_ICON_HI_DIM * FF_ICON_HI_DIM];
			pixel32 col_pri = s_halo_emblem_colors[c_pri];
			pixel32 col_sec = s_halo_emblem_colors[c_sec];
			pixel32 col_bg = s_halo_emblem_colors[c_bg];
			pixel32 col_bg_dark = ff_lerp_argb(col_bg, 0xFF080C14, 0.45f);
			pixel32 col_bg_border = ff_lerp_argb(col_bg, 0xFFFFFFFF, 0.35f);
			real inv_half = 2.0f / (real)FF_ICON_HI_DIM;
			short py, px, sy, sx;

			for (py = 0; py < FF_ICON_HI_DIM; py++)
			{
				real grad_t = (real)py / (real)(FF_ICON_HI_DIM - 1);
				pixel32 bg_fill = ff_lerp_argb(col_bg, col_bg_dark, grad_t);

				for (px = 0; px < FF_ICON_HI_DIM; px++)
				{
					unsigned long r_sum = 0, g_sum = 0, b_sum = 0, a_sum = 0;

					for (sy = 0; sy < 2; sy++)
					{
						real ny = ((real)py + 0.25f + 0.5f * (real)sy) * inv_half - 1.0f;
						for (sx = 0; sx < 2; sx++)
						{
							real nx = ((real)px + 0.25f + 0.5f * (real)sx) * inv_half - 1.0f;
							pixel32 pcol = 0;
							boolean is_bg_border = FALSE;
							int fg_layer = mcc_eval_emblem_fg(e->foreground, e->secondary_layer, nx, ny);

							if (fg_layer == 1)
								pcol = col_pri;
							else if (fg_layer == 2)
								pcol = col_sec;
							else if (mcc_eval_emblem_bg(e->background, nx, ny, &is_bg_border))
								pcol = is_bg_border ? col_bg_border : bg_fill;

							if (pcol != 0)
							{
								unsigned long sa = (pcol >> 24) & 0xFF;
								r_sum += ((pcol >> 16) & 0xFF) * sa;
								g_sum += ((pcol >> 8) & 0xFF) * sa;
								b_sum += (pcol & 0xFF) * sa;
								a_sum += sa;
							}
						}
					}

					if (a_sum == 0)
					{
						s_pixels128[py * FF_ICON_HI_DIM + px] = 0;
					}
					else
					{
						unsigned long r = (r_sum + (a_sum >> 1)) / a_sum;
						unsigned long g = (g_sum + (a_sum >> 1)) / a_sum;
						unsigned long b = (b_sum + (a_sum >> 1)) / a_sum;
						unsigned long a = (a_sum + 2) >> 2;
						if (r > 255) r = 255;
						if (g > 255) g = 255;
						if (b > 255) b = 255;
						if (a > 255) a = 255;
						s_pixels128[py * FF_ICON_HI_DIM + px] = (pixel32)((a << 24) | (r << 16) | (g << 8) | b);
					}
				}
			}
			if (ff_draw_hw_icon_pixels(key, s_pixels128, x0, y0, size, alpha))
				return;
		}
	}

	for (i = 0; i < 8; i++)
	{
		if (s_emblem_mesh_cache[i].valid && s_emblem_mesh_cache[i].key == key)
		{
			mesh = &s_emblem_mesh_cache[i];
			break;
		}
	}

	if (!mesh)
	{
		pixel32 col_pri = s_halo_emblem_colors[c_pri];
		pixel32 col_sec = s_halo_emblem_colors[c_sec];
		pixel32 col_bg = s_halo_emblem_colors[c_bg];
		pixel32 col_bg_dark = ff_lerp_argb(col_bg, 0xFF080C14, 0.45f);
		pixel32 col_bg_border = ff_lerp_argb(col_bg, 0xFFFFFFFF, 0.35f);
		pixel32 grid[FF_ICON_GRID_DIM][FF_ICON_GRID_DIM];
		real inv_half = 2.0f / (real)FF_ICON_GRID_DIM;
		short py, px;

		for (py = 0; py < FF_ICON_GRID_DIM; py++)
		{
			real ny = ((real)py + 0.5f) * inv_half - 1.0f;
			real grad_t = (real)(py / 4) / 3.0f;
			pixel32 bg_fill = ff_lerp_argb(col_bg, col_bg_dark, grad_t);

			for (px = 0; px < FF_ICON_GRID_DIM; px++)
			{
				pixel32 pcol = 0;
				real nx = ((real)px + 0.5f) * inv_half - 1.0f;
				boolean is_bg_border = FALSE;
				int fg_layer = mcc_eval_emblem_fg(e->foreground, e->secondary_layer, nx, ny);

				if (fg_layer == 1)
				{
					pcol = col_pri;
				}
				else if (fg_layer == 2)
				{
					pcol = col_sec;
				}
				else if (mcc_eval_emblem_bg(e->background, nx, ny, &is_bg_border))
				{
					pcol = is_bg_border ? col_bg_border : bg_fill;
				}
				grid[py][px] = pcol;
			}
		}

		mesh = &s_emblem_mesh_cache[s_emblem_mesh_next];
		s_emblem_mesh_next = (s_emblem_mesh_next + 1) & 7;
		ff_build_merged_icon_mesh(grid, mesh, key);
	}

	ff_draw_cached_icon_mesh(mesh, x0, y0, size, alpha);
}

static void draw_screen_quad(short x0, short y0, short x1, short y1, pixel32 col)
{
	rectangle2d r;
	if (x1 <= x0 || y1 <= y0)
		return;
	r.x0 = x0;
	r.y0 = y0;
	r.x1 = x1;
	r.y1 = y1;
	draw_quad(&r, col);
}

struct mcc_nameplate_theme
{
	pixel32 left_bg;
	pixel32 right_bg;
	pixel32 border;
	pixel32 stripe_a;
	pixel32 stripe_b;
};

static struct mcc_nameplate_theme const s_mcc_nameplate_themes[FIREFIGHT_NAMEPLATE_COUNT] =
{
	/* 0: UNSC Standard (Navy & Tactical Cyan) */
	{ 0xE60B1E38, 0xE6050D1C, 0xFF2896FF, 0x702896FF, 0x4070C8FF },
	/* 1: Halo Ring (Deep Space & Forerunner Teal Arc) */
	{ 0xE6082832, 0xE604101A, 0xFF3CE8D8, 0x883CE8D8, 0x5098FFF5 },
	/* 2: Legendary Shield (Crimson & Gold Trim) */
	{ 0xE6360D12, 0xE6180508, 0xFFF2C038, 0x88F2C038, 0x58FF5048 },
	/* 3: ODST Drop Pod (Orbital Charcoal & Hazard Amber) */
	{ 0xE61A1E26, 0xE60B0D12, 0xFFF09824, 0x90F09824, 0x50FFD060 },
	/* 4: Covenant Empire (Royal Violet & Plasma Magenta) */
	{ 0xE6240F3D, 0xE60F061C, 0xFFC058FF, 0x88C058FF, 0x5050D8FF },
	/* 5: Forerunner Glyph (Obsidian & Hardlight Gold-Cyan) */
	{ 0xE6141C24, 0xE6080B10, 0xFFFFD868, 0x8848E8FF, 0x60FFD868 },
	/* 6: Firefight Veteran (Scorched Crimson & Ember Orange) */
	{ 0xE63B140C, 0xE61A0704, 0xFFFF682C, 0x90FF682C, 0x58FFCC40 },
	/* 7: Golden Warthog (UNSC Olive Drab & Gold Brass) */
	{ 0xE61B2916, 0xE60B1209, 0xFFE8C242, 0x88E8C242, 0x5078C860 },
	/* 8: Pillar of Autumn (Steel Cruiser & Reactor Blue) */
	{ 0xE61C2634, 0xE60C1118, 0xFF68B8FF, 0x8868B8FF, 0x50FFA038 },
	/* 9: Reclaimer (Mythic Teal & Solar Gold) */
	{ 0xE60D2B3E, 0xE61E0C2E, 0xFFFFDF60, 0x903CF0E0, 0x68FFDF60 }
};

void firefight_draw_mcc_nameplate(
	short x0,
	short y0,
	short x1,
	short y1,
	short nameplate_id,
	pixel32 armor_accent,
	real alpha)
{
	short id = (short)PIN(nameplate_id, 0, FIREFIGHT_NAMEPLATE_COUNT - 1);
	struct mcc_nameplate_theme const *th = &s_mcc_nameplate_themes[id];
	short w = (short)(x1 - x0);
	short h = (short)(y1 - y0);
	short b;
	pixel32 border_c = ff_apply_alpha((id == 0 && (armor_accent & 0x00FFFFFF) != 0) ? armor_accent : th->border, alpha);
	pixel32 sa = ff_apply_alpha(th->stripe_a, alpha);
	pixel32 sb = ff_apply_alpha(th->stripe_b, alpha);

	if (w <= 8 || h <= 6)
		return;

	/* 4-band horizontal gradient background */
	for (b = 0; b < 4; b++)
	{
		short bx0 = (short)(x0 + (w * b) / 4);
		short bx1 = (b == 3) ? x1 : (short)(x0 + (w * (b + 1)) / 4);
		pixel32 bg_c = ff_apply_alpha(ff_lerp_argb(th->left_bg, th->right_bg, (real)b / 3.0f), alpha);
		draw_screen_quad(bx0, y0, bx1, y1, bg_c);
	}

	/* Distinctive geometric motif per MCC Nameplate theme (fast quad primitives) */
	switch (id)
	{
	case 0: /* UNSC Standard: dual tactical horizon bars */
		draw_screen_quad((short)(x0 + 4), (short)(y1 - 5), (short)(x1 - 4), (short)(y1 - 3), sa);
		draw_screen_quad((short)(x1 - w / 3), (short)(y0 + 3), (short)(x1 - 4), (short)(y0 + 5), sb);
		break;
	case 1: /* Halo Ring: segmented diagonal installation arc */
		for (b = 0; b < 4; b++)
		{
			short rx = (short)(x1 - 54 + b * 10);
			short ry = (short)(y0 + 3 + b * (h - 8) / 4);
			draw_screen_quad(rx, ry, (short)(rx + 12), (short)(ry + 3), (b & 1) ? sb : sa);
		}
		break;
	case 2: /* Legendary Shield: twin crossed combat blades & gold crest */
		draw_screen_quad((short)(x0 + 4), (short)(y0 + h / 2 - 1), (short)(x1 - 4), (short)(y0 + h / 2 + 1), sa);
		draw_screen_quad((short)(x1 - 42), (short)(y0 + 4), (short)(x1 - 14), (short)(y0 + 7), sb);
		draw_screen_quad((short)(x1 - 42), (short)(y1 - 7), (short)(x1 - 14), (short)(y1 - 4), sb);
		break;
	case 3: /* ODST Drop Pod: hazard chevron stripes */
		for (b = 0; b < 4; b++)
		{
			short hx = (short)(x1 - 56 + b * 12);
			draw_screen_quad(hx, (short)(y0 + 3), (short)(hx + 6), (short)(y0 + h / 2), sa);
			draw_screen_quad((short)(hx + 2), (short)(y0 + h / 2), (short)(hx + 8), (short)(y1 - 3), sb);
		}
		break;
	case 4: /* Covenant Empire: energy lattice diamonds */
		draw_screen_quad((short)(x1 - 52), (short)(y0 + h / 2 - 2), (short)(x1 - 8), (short)(y0 + h / 2 + 2), sa);
		draw_screen_quad((short)(x1 - 36), (short)(y0 + 4), (short)(x1 - 24), (short)(y1 - 4), sb);
		break;
	case 5: /* Forerunner Glyph: Didact geometric pylon & rings */
		draw_screen_quad((short)(x1 - 46), (short)(y0 + 4), (short)(x1 - 12), (short)(y0 + 6), sa);
		draw_screen_quad((short)(x1 - 46), (short)(y1 - 6), (short)(x1 - 12), (short)(y1 - 4), sa);
		draw_screen_quad((short)(x1 - 32), (short)(y0 + 6), (short)(x1 - 26), (short)(y1 - 6), sb);
		break;
	case 6: /* Firefight Veteran: triple wave-tally bars & ember trim */
		draw_screen_quad((short)(x0 + 4), (short)(y1 - 5), (short)(x1 - 4), (short)(y1 - 3), sa);
		for (b = 0; b < 3; b++)
		{
			short vx = (short)(x1 - 38 + b * 9);
			draw_screen_quad(vx, (short)(y0 + 5), (short)(vx + 5), (short)(y1 - 6), sb);
		}
		break;
	case 7: /* Golden Warthog: twin racing stripes */
		draw_screen_quad((short)(x0 + 4), (short)(y1 - 8), (short)(x1 - 4), (short)(y1 - 6), sa);
		draw_screen_quad((short)(x0 + 4), (short)(y1 - 5), (short)(x1 - 4), (short)(y1 - 3), sb);
		break;
	case 8: /* Pillar of Autumn: MAC barrel rails & reactor core */
		draw_screen_quad((short)(x0 + w / 3), (short)(y0 + 4), (short)(x1 - 6), (short)(y0 + 6), sa);
		draw_screen_quad((short)(x0 + w / 2), (short)(y1 - 6), (short)(x1 - 6), (short)(y1 - 4), sa);
		draw_screen_quad((short)(x1 - 20), (short)(y0 + h / 2 - 3), (short)(x1 - 10), (short)(y0 + h / 2 + 3), sb);
		break;
	case 9: /* Reclaimer: full mythic double-border & twin energy conduits */
	default:
		draw_screen_quad((short)(x0 + 4), (short)(y0 + 3), (short)(x1 - 4), (short)(y0 + 5), sa);
		draw_screen_quad((short)(x0 + 4), (short)(y1 - 5), (short)(x1 - 4), (short)(y1 - 3), sb);
		draw_screen_quad((short)(x1 - 34), (short)(y0 + h / 2 - 2), (short)(x1 - 8), (short)(y0 + h / 2 + 2), sa);
		break;
	}

	/* Left accent bar & outer frame */
	draw_screen_quad(x0, y0, (short)(x0 + 4), y1, border_c);
	draw_screen_quad(x0, y0, x1, (short)(y0 + 1), border_c);
	draw_screen_quad(x0, (short)(y1 - 1), x1, y1, border_c);
	draw_screen_quad((short)(x1 - 1), y0, x1, y1, border_c);
}

void firefight_hud_draw_pgcr_extras(
	long mvp_player_index,
	short mvp_row)
{
	static unsigned long s_pgcr_anim_start_ms = 0;
	static long s_last_pgcr_mvp = -999;
	unsigned long now_ms = system_milliseconds();
	real anim_t;
	long font_index = hud_get_font_index();
	struct font_header *font;
	short line_height;
	real vx0 = (real)(-render.camera.viewport_bounds.x0);
	real vy0 = (real)(-render.camera.viewport_bounds.y0);
	real scale = 1.0f;
	long local_p = local_player_get_player_index(0);
	short top_medal_ids[3] = { NONE, NONE, NONE };
	short top_medal_counts[3] = { 0, 0, 0 };
	short top_medal_num = 0;
	struct firefight_postgame_xp_summary xp_sum;
	struct firefight_emblem_info emb;
	real_argb_color gold = { 1.0f, 1.0f, 0.85f, 0.28f };
	real_argb_color cyan = { 1.0f, 0.62f, 0.85f, 1.0f };
	real_argb_color white = { 1.0f, 0.95f, 0.95f, 0.95f };
	real_argb_color green = { 1.0f, 0.38f, 1.0f, 0.52f };

	if (font_index == NONE)
		return;
	font = font_definition_get(font_index);
	if (!font)
		return;
	line_height = (short)(font->ascending_height + font->descending_height);
	if (line_height <= 0)
		return;

	if (s_last_pgcr_mvp != mvp_player_index || (now_ms - s_pgcr_anim_start_ms) > 60000UL)
	{
		s_last_pgcr_mvp = mvp_player_index;
		s_pgcr_anim_start_ms = now_ms;
	}
	anim_t = PIN((real)(now_ms - s_pgcr_anim_start_ms) / 1800.0f, 0.0f, 1.0f);

	/* 1. Highlight Top Performer Row with Golden [MVP] Badge */
	if (mvp_player_index != NONE && mvp_row >= 8)
	{
		real ry = (real)(mvp_row * 18);
		ff_draw_quad(vx0, vy0, scale, 42.0f, ry - 1.0f, 594.0f, ry + 17.0f, 0x28FFD040);
		ff_draw_quad(vx0, vy0, scale, 6.0f, ry + 1.0f, 40.0f, ry + 15.0f, 0xFFE8B828);
		ff_draw_quad(vx0, vy0, scale, 7.0f, ry + 2.0f, 39.0f, ry + 14.0f, 0xE6382406);
		ff_draw_text(font_index, line_height, vx0, vy0, scale,
			"MVP", 7.0f, 39.0f, ry + 1.5f, 0.50f, _ff_text_center, &gold);
	}

	/* 2. Left PGCR Showcase Box: Local Player Nameplate + Top 3 Medals Earned */
	{
		real lx0 = 45.0f, ly0 = 324.0f, lx1 = 308.0f, ly1 = 404.0f;
		long target_p = (local_p != NONE) ? local_p : mvp_player_index;
		long medals_bmp = campaign_medals_bitmap();
		short m;

		firefight_stats_get_emblem(NONE, &emb);
		ff_draw_box(vx0, vy0, scale, lx0, ly0, lx1, ly1, 18.0f, FF_HUD_PANEL_FILL, FF_HUD_PANEL_EDGE);
		firefight_draw_mcc_nameplate(
			(short)(vx0 + lx0 + 2.0f),
			(short)(vy0 + ly0 + 2.0f),
			(short)(vx0 + lx1 - 2.0f),
			(short)(vy0 + ly0 + 19.0f),
			emb.nameplate,
			0,
			0.85f);
		ff_draw_text(font_index, line_height, vx0, vy0, scale,
			"TOP MEDALS & MVP SHOWCASE", lx0 + 8.0f, lx1 - 8.0f, ly0 + 3.0f,
			0.52f, _ff_text_left, &gold);

		top_medal_num = firefight_stats_get_player_top_medals(target_p, top_medal_ids, top_medal_counts, 3);
		if (top_medal_num <= 0)
		{
			ff_draw_text(font_index, line_height, vx0, vy0, scale,
				"NO MEDALS EARNED THIS MATCH", lx0 + 10.0f, lx1 - 10.0f, ly0 + 42.0f,
				0.54f, _ff_text_center, &cyan);
		}
		else
		{
			real slot_w = (lx1 - lx0 - 16.0f) / 3.0f;
			for (m = 0; m < top_medal_num; m++)
			{
				short mid = top_medal_ids[m];
				real sx0 = lx0 + 8.0f + (real)m * slot_w;
				real scx = sx0 + slot_w * 0.5f;
				struct bitmap_data *bmp = (medals_bmp != NONE && mid >= 0 && mid < NUMBER_OF_CAMPAIGN_MEDALS)
					? bitmap_group_try_and_get_bitmap(medals_bmp, mid)
					: NULL;
				char cnt_str[24];
				char lbl_str[32];

				ff_draw_quad(vx0, vy0, scale, sx0 + 2.0f, ly0 + 22.0f, sx0 + slot_w - 2.0f, ly1 - 4.0f, 0x6008182E);
				if (bmp)
				{
					rectangle2d mb;
					mb.x0 = (short)(vx0 + scx - 15.0f);
					mb.x1 = (short)(vx0 + scx + 15.0f);
					mb.y0 = (short)(vy0 + ly0 + 24.0f);
					mb.y1 = (short)(vy0 + ly0 + 54.0f);
					draw_bitmap_in_rect(bmp, &mb, NULL, NULL, 0xFFFFFFFF, NULL, TRUE);
				}
				else
				{
					ff_draw_quad(vx0, vy0, scale, scx - 10.0f, ly0 + 27.0f, scx + 10.0f, ly0 + 47.0f, 0xFFD4A838);
				}

				snprintf(cnt_str, sizeof(cnt_str), "x%d", (int)top_medal_counts[m]);
				ff_draw_text(font_index, line_height, vx0, vy0, scale,
					cnt_str, sx0 + 4.0f, sx0 + slot_w - 4.0f, ly0 + 24.0f,
					0.52f, _ff_text_right, &gold);

				if (mid >= 0 && mid < NUMBER_OF_CAMPAIGN_MEDALS && campaign_medal_descriptions[mid].label)
				{
					snprintf(lbl_str, sizeof(lbl_str), "%.13s", campaign_medal_descriptions[mid].label);
					ff_draw_text(font_index, line_height, vx0, vy0, scale,
						lbl_str, sx0 + 2.0f, sx0 + slot_w - 2.0f, ly0 + 58.0f,
						0.45f, _ff_text_center, &white);
				}
			}
		}
	}

	/* 3. Right PGCR Showcase Box: MCC Career XP Breakdown & Animated Rank Progression Bar */
	{
		real rx0 = 316.0f, ry0 = 324.0f, rx1 = 595.0f, ry1 = 404.0f;
		char hdr_right[48];
		char rk_line[64];
		char xp_line[80];
		char bar_label[48];
		long disp_xp_in_rank;
		long span_xp;
		real fill_frac;

		firefight_stats_get_postgame_xp_summary(&xp_sum);
		ff_draw_box(
			vx0, vy0, scale, rx0, ry0, rx1, ry1, 18.0f,
			FF_HUD_PANEL_FILL,
			xp_sum.promoted ? 0xFFFFD038 : FF_HUD_PANEL_EDGE);

		ff_draw_text(font_index, line_height, vx0, vy0, scale,
			xp_sum.promoted ? "RANK PROMOTED!" : "CAREER XP PROGRESSION",
			rx0 + 8.0f, rx0 + 175.0f, ry0 + 3.0f,
			0.52f, _ff_text_left, xp_sum.promoted ? &gold : &white);

		snprintf(hdr_right, sizeof(hdr_right), "+%ld XP EARNED", (long)(xp_sum.total_gained_xp * anim_t + 0.5f));
		ff_draw_text(font_index, line_height, vx0, vy0, scale,
			hdr_right, rx0 + 176.0f, rx1 - 8.0f, ry0 + 3.0f,
			0.52f, _ff_text_right, &green);

		/* Rank Icon + Rank Title & Tour */
		firefight_draw_mcc_rank_icon(
			(short)(vx0 + rx0 + 8.0f),
			(short)(vy0 + ry0 + 24.0f),
			32,
			xp_sum.after_rank.tour_number,
			xp_sum.after_rank.rank_in_tour,
			1.0f);

		snprintf(
			rk_line,
			sizeof(rk_line),
			"%s (%s - Rk %d)",
			xp_sum.after_rank.rank_name,
			xp_sum.after_rank.tour_name,
			(int)xp_sum.after_rank.rank_in_tour);
		ff_draw_text(font_index, line_height, vx0, vy0, scale,
			rk_line, rx0 + 46.0f, rx1 - 8.0f, ry0 + 23.0f,
			0.54f, _ff_text_left, &gold);

		snprintf(
			xp_line,
			sizeof(xp_line),
			"KILLS +%ld  |  MEDALS +%ld  |  BONUS +%ld",
			xp_sum.combat_xp,
			xp_sum.medal_xp,
			xp_sum.match_bonus_xp);
		ff_draw_text(font_index, line_height, vx0, vy0, scale,
			xp_line, rx0 + 46.0f, rx1 - 8.0f, ry0 + 39.0f,
			0.46f, _ff_text_left, &cyan);

		/* Animated XP Progress Bar */
		span_xp = MAX(1L, xp_sum.after_rank.xp_for_next_rank);
		disp_xp_in_rank = (long)((real)xp_sum.after_rank.xp_in_rank * anim_t + 0.5f);
		fill_frac = PIN((real)disp_xp_in_rank / (real)span_xp, 0.0f, 1.0f);

		{
			real bx0 = rx0 + 8.0f, by0 = ry0 + 60.0f, bx1 = rx1 - 8.0f, by1 = ry0 + 76.0f;
			real fill_w = (bx1 - bx0 - 2.0f) * fill_frac;

			ff_draw_quad(vx0, vy0, scale, bx0, by0, bx1, by1, 0xFF2896FF);
			ff_draw_quad(vx0, vy0, scale, bx0 + 1.0f, by0 + 1.0f, bx1 - 1.0f, by1 - 1.0f, 0xD8061020);
			if (fill_w > 1.0f)
			{
				ff_draw_quad(
					vx0, vy0, scale,
					bx0 + 1.0f, by0 + 1.0f, bx0 + 1.0f + fill_w, by1 - 1.0f,
					xp_sum.promoted ? 0xFFE8B828 : 0xFF2896FF);
			}
			snprintf(bar_label, sizeof(bar_label), "%ld / %ld XP", disp_xp_in_rank, span_xp);
			ff_draw_text(font_index, line_height, vx0, vy0, scale,
				bar_label, bx0 + 4.0f, bx1 - 4.0f, by0 + 1.5f,
				0.48f, _ff_text_center, &white);
		}
	}
}


