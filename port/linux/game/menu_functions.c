/*
MENU_FUNCTIONS.C

The event handler functions the menus written in XML can run besides the
game's (menu_tags.c names them; ui_widget_event_handler_function_invoke
calls them from PC_MENU_FUNCTION_BASE on):

- "port quit game" (and the PC version's "main menu quit game") quits, as
  closing the window does;
- the PC version's "profile set edit begin" begins editing the first player
  profile, as its settings screens need, and fails with none: the main
  menu's then asks for a new one's name, as the menu's other items do, and
  its handlers open the screens that make the profile ("new campaign
  decision", after which Settings' is the one being edited);
- "gamespy screen init" hides the server browser's error and filter panels,
  and the title of the mode "mp type set mode" did not choose (Internet or
  LAN);
- its buttons' events: "mouse emit back event" and "mouse emit x event" push
  B and X (as the mouse does), "emit custom activation event" (an OK
  button) runs the screen's custom activation handler, "single prev cl item
  activated" (an item chosen) its list's, and its back handlers go back;
- "port setting load", on a spinner's created event, shows the value its
  setting has (or the nearest of its values): config.toml's, or for
  "profile.<field>" the controller's in the profile being edited;
- "port setting save", on its deleted event, writes the value shown back,
  if the player changed it;
- the settings screens' (tools/port_settings.py): "port settings save"
  (OK) writes those changed and applies the window's (the rest follow
  config.toml themselves), "port settings defaults" shows the defaults,
  "port settings help" the help of the row chosen (and Video Setup's
  Resolution or Window Size, for the display mode shown);
- Controls Setup's: the keyboard and mouse's controls, two bindings each,
  shown a group at a time ("controls update menu"); "controls begin
  binding" takes the next key or button pressed for the binding left and
  right choose ("controls binding slot"); OK ("controls screen change set")
  writes them, "controls screen defaults" shows the defaults.

A spinner's values are its strings' (setting= and values= in its <widget>,
menu_tags.c's pc_menu_setting).

The PC version's campaign menus, on player 1's profile (the active one, else
the one last used, else the first) and the game's saved game in it:
- "campaign menu continue" goes on with the saved game;
- New Game's list of levels ("initialize sp level list solo", "solo map
  list update", "solo level set map") has those the profile has reached, and
  marks those it has finished on Normal, Heroic and Legendary, as the
  Xbox's list does; a level with the saved game in it goes on with it, at
  its difficulty. Its first row's chooser shows MULTIPLAYER's maps
  instead, one chosen played alone at once;
- the difficulty menu: "difficulty item select" (a difficulty chosen) and
  "set difficulty" (its OK button: the difficulty shown) start the game;
- Load Game's list ("load game menu init", "load game list update", "load
  game menu activated") has each profile's saved game, and "load game menu
  delete request" and "delete finish" delete one.
The Xbox's functions of those names take the Xbox's widgets (a spinner of
levels, the difficulty list itself), so ours run instead (menu_tags.c).

Co-op (the Xbox's Cooperative Play, Multiplayer's CO-OP CAMPAIGN): "port coop
begin" makes two players, player 1 on its profile; "port coop player 2"
gives player 2 the profile chosen and the controller that chose it; then the
campaign's New Game and difficulty start the game for both.

The rest are the PC version's, which its menus (port/assets/menus/ce)
name and the Xbox's has not: they do nothing yet, and succeed, so that what
their handlers open opens.
*/

#include "cseries.h"
#include "input/input.h"
#include "interface/event_manager.h"
#include "interface/player_ui.h"
#include "interface/ui_widget.h"
#include "interface/ui_widget_event_handler_functions.h"
#include "main/main.h"
#include "networking/network_game_manager.h"
#include "rasterizer/rasterizer.h"
#include "saved games/player_profile.h"
#include "tag_files/tag_groups.h"
#include "text/text_group.h"
#include "text/unicode.h"

#include "halo_menus.h"
#include "custom_edition_cache.h"
#include "custom_edition_maps.h"
#include "game_modes.h"
#include "network_voice.h"
#include "text/draw_string.h"
/* (internet play's server browser: the platform layer's) */
#include "../src/p2p.h"
#include "text_filter.h"
#include "halo_firefight.h"
#include "halo_skulls.h"
#include "campaign_score.h"
#include "bots.h"
#include "tag_injector.h"
#include "tag_preview.h"
#include "tag_editor.h"
#include "tag_multiplayer.h"
#include "tag_campaign.h"
#include "../src/port_tag_profiles.h"
#include "firing_range.h"
#include "network_replay.h"
#include "community.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <xtl.h>

/* the platform layer's (port/linux/src) */
void platform_log(char const *format, ...);
void platform_request_quit(void);
char const *pc_menu_function_name(long function_index);
char const *pc_menu_game_data_input_name(long function_index);
void event_manager_post_button(short controller_index, short button_index);
int config_text(char const *name, char *text, unsigned int size);
int config_write(char const *name, char const *value);
int config_boolean(char const *name);
long config_integer(char const *name);
int config_default(char const *name, char *text, unsigned int size);
char const *config_string(char const *name);
void platform_display_apply(void);
void platform_binding_capture_begin(void);
int platform_binding_capture_poll(int *input);
void halo_input_name(int input, char *name, unsigned int size);
void halo_input_shown_name(char const *binding, char *shown, unsigned int size);
short pc_menu_string_index(long definition_index);
/* cseries_windows.c's */
unsigned long system_milliseconds(void);

/* the game's (port) */
boolean ui_widget_port_dispatch_event(struct widget_instance *widget, short event_type, short controller_index,
	boolean *deleted);
void ui_widget_port_go_back(struct widget_instance *widget);
short ui_widget_port_list_index(struct widget_instance *list_widget);
boolean ui_widget_port_saved_game(char const **map_name, short *level, short *difficulty);
char const *ui_widget_event_handler_function_name(long function_index);
short main_get_solo_level_from_name(char const *name);
boolean player_name_clean(wchar_t *name, long count);
short players_port_local_player_count(void);

boolean pc_menu_event_function_invoke(struct widget_instance *widget, struct event_record *event,
	long function_index, boolean *widget_deleted);
void pc_menu_game_data_function_invoke(struct widget_instance *widget, long function);

/* ---------- constants */

#define MAXIMUM_STRINGS 64
/* the PC version's custom activation event (this engine never sends it) */
#define EVENT_CUSTOM_ACTIVATION 32
#define BUTTON_A 0
#define BUTTON_B 1
#define BUTTON_X 2
#define BUTTON_START 12

enum
{
	_pc_menu_function_quit_game,
	_pc_menu_function_setting_load,
	_pc_menu_function_setting_save,
	NUMBER_OF_PC_MENU_FUNCTIONS
};

/* ---------- structures */

/* a widget, as ui_widget.c has it */
struct widget_instance
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
	struct widget_instance *previous;
	struct widget_instance *next;
	struct widget_instance *parent;
	struct widget_instance *child;
	struct widget_instance *focused_child;
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
			struct widget_instance *extended_description;
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

typedef char verify_widget_instance_size[
	sizeof(struct widget_instance) == 0x58 ? 1 : -1];
typedef char verify_widget_instance_child_offset[
	offsetof(struct widget_instance, child) == 0x34 ? 1 : -1];
typedef char verify_widget_instance_parent_offset[
	offsetof(struct widget_instance, parent) == 0x30 ? 1 : -1];
typedef char verify_widget_instance_selected_index_offset[
	offsetof(struct widget_instance, parameters.list.selected_index) == 0x3C ? 1 : -1];
typedef char verify_widget_instance_number_of_items_offset[
	offsetof(struct widget_instance, parameters.list.number_of_items) == 0x44 ? 1 : -1];
typedef char verify_widget_instance_animation_offset[
	offsetof(struct widget_instance, animation) == 0x50 ? 1 : -1];

/* menu_tags.c's */
struct pc_menu_setting
{
	long definition_index;
	char const *setting;
	long value_count;
	char const *values[MAXIMUM_STRINGS];
	short loaded_index;
};

struct pc_menu_setting *pc_menu_setting_get(long definition_index);

/* ---------- private code */

static boolean campaign_profile(short controller, struct player_profile *profile);

static boolean text_is_number(char const *text, double *number)
{
	char *end;

	*number = strtod(text, &end);
	return *text && !*end;
}

/* whether the widget is one of a spinner's items, which the game makes of
its strings from its own definition, handlers and all (ui_widget.c,
ui_widget_load_children_recursive) */
static boolean spinner_item(struct widget_instance const *widget)
{
	return widget->parent && widget->parent->definition_tag_index == widget->definition_tag_index;
}

/* the value shown for the setting's: the same one, else the nearest number */
static short setting_value_index(struct pc_menu_setting const *setting, char const *current)
{
	double current_number, value_number, distance = 0.0;
	short index, nearest = 0;

	for (index = 0; index < setting->value_count; index++)
	{
		if (!_stricmp(setting->values[index], current))
			return index;
	}
	if (!text_is_number(current, &current_number))
		return 0;
	for (index = 0; index < setting->value_count; index++)
	{
		if (text_is_number(setting->values[index], &value_number))
		{
			double gap = value_number > current_number ? value_number - current_number : current_number - value_number;

			if (index == 0 || gap < distance)
			{
				distance = gap;
				nearest = index;
			}
		}
	}
	return nearest;
}

/* the profile's settings ("profile.<field>"): those of the controller in
the profile being edited */
static struct
{
	char const *name;
	short field;
	/* its default (player_profile.c's new profile's) */
	byte default_value;
} const profile_settings[] =
{
	{ "profile.look_sensitivity", 0, 3 },
	{ "profile.invert_look", 1, FALSE },
	{ "profile.flight_inversion", 2, FALSE },
	{ "profile.autocenter", 3, FALSE },
	{ "profile.button_preset", 4, 0 },
	{ "profile.joystick_preset", 5, 0 },
	{ "profile.vibration", 6, TRUE },
	{ "profile.ingame_help", 7, TRUE },
};

static byte *profile_setting_field(struct player_profile *profile, short field)
{
	struct player_profile_controller_settings *controls = &profile->controller_settings;

	switch (field)
	{
	case 0: return &controls->look_sensitivity;
	case 1: return (byte *)&controls->invert_look;
	case 2: return (byte *)&controls->flight_stick_aircraft_controls;
	case 3: return (byte *)&controls->autocenter;
	case 4: return &controls->button_preset;
	case 5: return &controls->joystick_preset;
	case 6: return (byte *)&controls->vibration_disabled;
	case 7: return (byte *)&controls->ingame_help_disabled;
	}
	return NULL;
}

static boolean profile_setting_boolean(short field)
{
	return field != 0 && field != 4 && field != 5;
}

/* a setting's value as text, or (default_value) its default: config.toml's,
or the profile's */
static boolean setting_text(char const *name, char *text, unsigned int size, boolean default_value)
{
	short index;

	if (strncmp(name, "profile.", 8))
	{
		if (!(default_value ? config_default(name, text, size) : config_text(name, text, size)))
			return FALSE;
		/* (display.mode empty: display.fullscreen's, as the window has it:
		sdl_platform.c) */
		if (!strcmp(name, "display.mode") && !text[0])
			snprintf(text, size, "%s", default_value || config_boolean("display.fullscreen") ? "borderless" : "windowed");
		/* (display.window_size empty: 640x480 times display.window_scale, as
		older versions set it) */
		if (!strcmp(name, "display.window_size") && !text[0])
		{
			char scale_text[32] = "";
			long scale;

			if (default_value)
				config_default("display.window_scale", scale_text, sizeof(scale_text));
			else
				config_text("display.window_scale", scale_text, sizeof(scale_text));
			scale = atol(scale_text);
			if (scale < 1)
				scale = 1;
			snprintf(text, size, "%ldx%ld", 640 * scale, 480 * scale);
		}
		return TRUE;
	}
	for (index = 0; index < NUMBEROF(profile_settings); index++)
	{
		struct player_profile *profile = player_ui_get_edit_player_profile();
		short field = profile_settings[index].field;
		long value;

		if (strcmp(name, profile_settings[index].name))
			continue;
		if (default_value)
			value = profile_settings[index].default_value;
		else if (profile && profile_setting_field(profile, field))
			value = *profile_setting_field(profile, field);
		else
			return FALSE;
		/* (vibration and help are kept as their opposites) */
		if (!default_value && (field == 6 || field == 7))
			value = !value;
		if (profile_setting_boolean(field))
			snprintf(text, size, "%s", value ? "true" : "false");
		else
			snprintf(text, size, "%ld", value);
		return TRUE;
	}
	return FALSE;
}

static boolean setting_write(char const *name, char const *value)
{
	short index;

	if (strncmp(name, "profile.", 8))
		return config_write(name, value);
	for (index = 0; index < NUMBEROF(profile_settings); index++)
	{
		struct player_profile *profile = player_ui_get_edit_player_profile();
		short field = profile_settings[index].field;
		byte *place;
		long number;

		if (strcmp(name, profile_settings[index].name))
			continue;
		if (!profile || !(place = profile_setting_field(profile, field)))
			return FALSE;
		number = profile_setting_boolean(field) ? !strcmp(value, "true") : atol(value);
		if (field == 6 || field == 7)
			number = !number;
		*place = (byte)number;
		return TRUE;
	}
	return FALSE;
}

static boolean setting_load(struct widget_instance *widget)
{
	struct pc_menu_setting *setting = pc_menu_setting_get(widget->definition_tag_index);
	char current[300];

	if (spinner_item(widget))
		return TRUE;
	if (!setting || !setting_text(setting->setting, current, sizeof(current), FALSE))
		return FALSE;
	setting->loaded_index = setting_value_index(setting, current);
	if (setting->loaded_index < (short)widget->parameters.list.number_of_items)
		widget->parameters.list.selected_index = setting->loaded_index;
	return TRUE;
}

static boolean setting_save(struct widget_instance *widget)
{
	struct pc_menu_setting *setting = pc_menu_setting_get(widget->definition_tag_index);

	if (spinner_item(widget))
		return TRUE;
	if (!setting || setting->loaded_index == NONE || widget->parameters.list.selected_index < 0 ||
		widget->parameters.list.selected_index >= setting->value_count)
		return FALSE;
	if (widget->parameters.list.selected_index == setting->loaded_index)
		return TRUE;
	if (!config_write(setting->setting, setting->values[widget->parameters.list.selected_index]))
	{
		platform_log("menus: could not write %s to config.toml", setting->setting);
		return FALSE;
	}
	platform_log("menus: %s = %s (from the next start)", setting->setting, setting->values[widget->parameters.list.selected_index]);
	setting->loaded_index = widget->parameters.list.selected_index;
	return TRUE;
}

static short controller_of(struct widget_instance const *widget)
{
	return widget->local_player_index >= 0 && widget->local_player_index < 4 ? widget->local_player_index : 0;
}

/* the controller the event came from, else the widget's */
static short event_controller(struct widget_instance const *widget, struct event_record const *event)
{
	return event && event->controller_index >= 0 && event->controller_index < 4 ?
		event->controller_index : controller_of(widget);
}

/* runs the custom activation handler of the widget's list, or the nearest
of its parents' that has one (widget_deleted: the widget went with its
screen) */
static boolean item_activated(struct widget_instance *widget, short controller, boolean *widget_deleted)
{
	struct widget_instance *parent;

	for (parent = widget->parent; parent; parent = parent->parent)
	{
		boolean deleted;

		if (ui_widget_port_dispatch_event(parent, EVENT_CUSTOM_ACTIVATION, controller, &deleted))
		{
			if (deleted)
				*widget_deleted = TRUE;
			return TRUE;
		}
	}
	return FALSE;
}

/* whether the multiplayer menu's choice was LAN, not Internet (mp type set
mode) */
static boolean lan_mode;

/* hides the descendants of the widget with the name */
static void hide_named(struct widget_instance *widget, char const *name)
{
	struct widget_instance *child;

	for (child = widget->child; child; child = child->next)
	{
		if (!strcmp(child->name, name))
			child->visible = FALSE;
		hide_named(child, name);
	}
}

/* the server browser's panels for errors and filters are hidden until they
are wanted, and its title is Internet's or LAN's (gamespy screen init) */
static void server_browser_initialize(struct widget_instance *screen)
{
	struct widget_instance *child;

	hide_named(screen, lan_mode ? "header_internet" : "header_lan");
	/* (a widget's name is its definition's: the last part of ours) */
	for (child = screen->child; child; child = child->next)
	{
		if (!strcmp(child->name, "gamespy_error_fullscreen") || !strcmp(child->name, "filters_screen"))
		{
			child->visible = FALSE;
			if (screen->focused_child == child)
				screen->focused_child = NULL;
		}
	}
	for (child = screen->child; child && !screen->focused_child; child = child->next)
	{
		if (child->visible && child->type == 3)
			screen->focused_child = child;
	}
}

/* begins editing player 1's profile (as the campaign has it); FALSE if
there is none */
boolean pc_menu_profile_edit_begin(void)
{
	struct player_profile profile;

	if (!campaign_profile(0, &profile))
		return FALSE;
	player_ui_begin_editing_profile(player_ui_get_active_player_profile_index(0));
	return TRUE;
}

/* the game's own function of the name (as its tags name it), run for the
widget: for ours of a name of the game's, which do more than it */
static boolean game_function(char const *name, struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted)
{
	char const *function;
	long index;

	for (index = 0; (function = ui_widget_event_handler_function_name(index)) != NULL; index++)
	{
		if (!strcmp(function, name))
			return ui_widget_event_handler_function_invoke(widget, event, (word)index, widget_deleted);
	}
	return FALSE;
}

/* "profile set edit begin": Settings, the main menu's and a game's pause
menu's. With no profile, the main menu's asks for a new one's name, as the
menu's other items do ("new game if no plyr profiles"), and fails: the
screens its handler then opens make the profile and edit it
(new_profile_decision). Without the name asked, they made none, and showed
"new campaign decision" as failed. The pause menu's has no such screens,
and only fails. */
static boolean profile_edit_begin(struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted)
{
	if (pc_menu_profile_edit_begin())
		return TRUE;
	/* (a press: the profile is made on its controller) */
	if (main_menu_is_active() && event && event->controller_index >= 0 && event->controller_index < 4)
		game_function("new campaign chosen", widget, event, widget_deleted);
	return FALSE;
}

/* "new campaign decision" (the screen after a new profile's name, each of
the main menu's items having its own): the game's makes the profile, if the
name was entered. Settings' then begins editing it: the PC version's screens
open Settings with nothing more run, and its screen closes at once with no
profile being edited ("close if not editing profile"). */
static boolean new_profile_decision(struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted)
{
	if (!game_function("new campaign decision", widget, event, widget_deleted))
		return FALSE;
	return strcmp(widget->name, "settings_creating_profile") || pc_menu_profile_edit_begin();
}

/* ---------- the campaign */

/* the rows of its lists (main_menu/new_select/list_item_0 to 10) */
#define MAXIMUM_ROWS 11
/* its strings' and sp_levels' entries past the levels': a level not
reached, and (map_data) a saved game on the level */
#define LEVEL_UNAVAILABLE 10
#define LEVEL_IN_PROGRESS 11
#define ROW_TEXT_LENGTH 64
#define MAXIMUM_PROFILES 32
#define SOUND_ERROR 4
#define SOUND_CURSOR 1
#define SOUND_FORWARD 2
#define SOUND_BACK 3

/* (an index whose profile reads: saved_game_files.c's) */
#define PROFILE_VALID_BIT 0x80000000UL

struct campaign_level
{
	boolean available;
	/* finished on Normal, Heroic, Legendary (difficulty_options_small's
	frames 1 to 3) */
	boolean finished[3];
};

struct campaign_saved_game
{
	long profile_index;
	wchar_t profile_name[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH + 1];
	short level;
	short difficulty;
};

static struct
{
	struct campaign_level levels[NUMBER_OF_SINGLE_PLAYER_LEVELS];
	/* player 1's saved game's level, else NONE */
	short saved_level;
	/* the level and saved game the lists' descriptions last showed */
	short shown_level;
	short shown_saved_game;
	/* Load Game's */
	struct campaign_saved_game saved_games[MAXIMUM_ROWS];
	short saved_game_count;
	short saved_game_to_delete;
} campaign;

/* the descendant of the widget with the name (a widget's name is its
definition's: the last part of ours), the nth of them */
static struct widget_instance *descendant(struct widget_instance *widget, char const *name, long *nth)
{
	struct widget_instance *child;

	for (child = widget->child; child; child = child->next)
	{
		struct widget_instance *found;

		if (!strcmp(child->name, name) && (*nth)-- == 0)
			return child;
		found = descendant(child, name, nth);
		if (found)
			return found;
	}
	return NULL;
}

static struct widget_instance *named(struct widget_instance *widget, char const *name, long nth)
{
	return widget ? descendant(widget, name, &nth) : NULL;
}

/* the text box's own text (as the server list sets its items'), in a
buffer of length characters (made the first time: a text box is always
given the same length) */
static void text_set_length(struct widget_instance *text_box, wchar_t const *text, short length)
{
	if (!text_box)
		return;
	if (!text_box->parameters.text_box.text)
	{
		text_box->parameters.text_box.text = ui_widget_realloc(NULL, length * sizeof(wchar_t),
			__FILE__, __LINE__);
	}
	if (text_box->parameters.text_box.text)
	{
		ustrncpy(text_box->parameters.text_box.text, text, length - 1);
		text_box->parameters.text_box.text[length - 1] = 0;
	}
}

static void text_set(struct widget_instance *text_box, wchar_t const *text)
{
	text_set_length(text_box, text, ROW_TEXT_LENGTH);
}

/* one of our string lists' strings, on one line (its line breaks spaces) */
static void string_get(char const *list, short index, wchar_t *text)
{
	long tag_index = tag_loaded('ustr', list);
	wchar_t const *string = tag_index != NONE ? unicode_string_list_get_string(tag_index, index) : NULL;
	long length = 0;

	for (; string && *string && length < ROW_TEXT_LENGTH - 1; string++)
	{
		wchar_t character = *string == '\r' || *string == '\n' ? ' ' : *string;

		if (character != ' ' || (length && text[length - 1] != ' '))
			text[length++] = character;
	}
	while (length && text[length - 1] == ' ')
		length--;
	text[length] = 0;
}

/* the list's row that has the focus, else NONE (its buttons) */
static short focused_row(struct widget_instance *list)
{
	struct widget_instance *child;
	short index = 0;

	for (child = list->child; child && index < MAXIMUM_ROWS; child = child->next, index++)
	{
		if (child == list->focused_child)
			return strncmp(child->name, "list_item_", 10) ? NONE : index;
	}
	return NONE;
}

static void focus_row(struct widget_instance *list, short row)
{
	struct widget_instance *child = list->child;
	short index;

	for (index = 0; child && child->next && index < row; index++)
		child = child->next;
	if (row == NONE)
	{
		/* (its buttons: the last child) */
		while (child && child->next)
			child = child->next;
	}
	if (child)
	{
		list->focused_child = child;
		list->parameters.list.selected_index = row == NONE ? 0 : row;
	}
}

/* shows the first count rows, with their texts, the arrows on the one that
has the focus, and no scroll buttons (the rows are enough) */
static void rows_update(struct widget_instance *list, short count, void (*row_text)(short row, wchar_t *text))
{
	struct widget_instance *row;
	short index = 0;

	for (row = list->child; row && index < MAXIMUM_ROWS; row = row->next, index++)
	{
		wchar_t text[ROW_TEXT_LENGTH];
		struct widget_instance *arrows = named(row, "list_item_arrows", 0);

		if (strncmp(row->name, "list_item_", 10))
			break;
		row->visible = index < count;
		if (index < count)
		{
			row_text(index, text);
			text_set(named(row, "list_item_text", 0), text);
		}
		if (arrows)
			arrows->visible = row == list->focused_child;
		if (named(row, "scroll_up_button", 0))
			named(row, "scroll_up_button", 0)->visible = FALSE;
		if (named(row, "scroll_down_button", 0))
			named(row, "scroll_down_button", 0)->visible = FALSE;
	}
}

/* player 1's profile, read again (the active one, else the one last used,
else the first), on the controller (in co-op, on the one that chose co-op:
coop_begin); FALSE if there is none */
static boolean campaign_profile(short controller, struct player_profile *profile)
{
	long profile_index = player_ui_get_active_player_profile_index(0);

	if (profile_index == NONE || !player_profile_get(profile_index, profile))
	{
		profile_index = player_ui_get_player1_last_used_profile_index();
		if (profile_index == NONE || !player_profile_get(profile_index, profile))
		{
			word count = 1;

			profile_index = NONE;
			player_profiles_enumerate_available_to_local_player_index(NONE, &count, &profile_index, FALSE);
			if ((short)count <= 0 || profile_index == NONE || !player_profile_get(profile_index, profile))
				return FALSE;
		}
	}
	player_ui_set_active_player_profile(0, profile_index, profile);
	if (player_spawn_count < 2)
		player_ui_set_single_player_local_player_controller(0, controller);
	return TRUE;
}

/* the levels the profile has reached (as the Xbox's list has them: those
it has played, the one after the last it finished, the first) and finished,
and its saved game's; in co-op, those either player's has reached and
finished, and no saved game (a game of one player's does not go on with
two: game_state.c's game_state_header_valid) */
static void campaign_levels_read(struct player_profile const *profile)
{
	char const *map_name;
	struct player_profile player2;
	short highest_level, highest_difficulty, difficulty, level, player;

	for (level = 0; level < NUMBER_OF_SINGLE_PLAYER_LEVELS; level++)
		memset(&campaign.levels[level], 0, sizeof(campaign.levels[level]));
	if (player_spawn_count >= 2)
		player_ui_get_active_player_profile(1, &player2);
	for (player = 0; player < (player_spawn_count >= 2 ? 2 : 1); player++)
	{
		struct player_profile const *reader = player == 0 ? profile : &player2;

		player_profile_get_highest_completed_solo_level((struct player_profile *)reader, &highest_level,
			&highest_difficulty);
		for (level = 0; level < NUMBER_OF_SINGLE_PLAYER_LEVELS; level++)
		{
			byte flags = reader->single_player_map_flags[level];
			short marker;

			campaign.levels[level].available |= flags || level == highest_level + 1 || level == 0;
			for (marker = 0; marker < 3; marker++)
				campaign.levels[level].finished[marker] |= (flags >> (marker + 1)) & 1;
		}
	}
	if (player_spawn_count >= 2 || !ui_widget_port_saved_game(&map_name, &campaign.saved_level, &difficulty))
		campaign.saved_level = NONE;
}

/* plays the map, at the difficulty (a saved game in it goes on: main.c's
main_new_map, if its difficulty is this one) */
static void campaign_start(char const *map_name, short difficulty, short controller)
{
	if (player_spawn_count < 2)
		player_ui_set_single_player_local_player_controller(0, controller);
	main_set_map_name(map_name);
	main_set_difficulty(difficulty);
	game_connection_set(0);
	main_menu_switch_to_single_player();
	player_ui_remember_player1_profile(TRUE);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
}

static boolean campaign_fail(void)
{
	ui_play_audio_feedback_sound(SOUND_ERROR);
	return FALSE;
}

/* the description's level: its name, picture and words, and the
difficulties it has been finished on (finished: New Game's, NULL for a level
not reached), or (saved_difficulty, not NONE: Load Game's) the saved game's
difficulty */
static void level_description(struct widget_instance *description, char const *prefix, short level, boolean in_progress,
	boolean const *finished, short saved_difficulty)
{
	char name[64];
	struct widget_instance *widget;
	short marker;

	snprintf(name, sizeof(name), "%s_right_name", prefix);
	if ((widget = named(description, name, 0)) != NULL)
		widget->parameters.text_box.string_list_index = level;
	snprintf(name, sizeof(name), "%s_right_pic", prefix);
	if ((widget = named(description, name, 0)) != NULL)
		widget->animation.current_frame_index = level;
	snprintf(name, sizeof(name), "%s_right_data", prefix);
	if ((widget = named(description, name, 0)) != NULL && saved_difficulty != NONE)
	{
		wchar_t text[ROW_TEXT_LENGTH];

		string_get("pc\\main_menu\\player_profiles_select\\difficulty_names", saved_difficulty, text);
		text_set(widget, text);
	}
	else if (widget)
		widget->parameters.text_box.string_list_index = in_progress ? LEVEL_IN_PROGRESS : level;
	for (marker = 0; marker < 3; marker++)
	{
		if ((widget = named(description, "difficulty_indicator", marker)) != NULL)
		{
			widget->visible = saved_difficulty != NONE ? marker == 0 : finished && finished[marker];
			widget->animation.current_frame_index = saved_difficulty != NONE ? saved_difficulty : marker + 1;
		}
	}
}

/* player 1's profile's name (the active one, else the one used last: the
main menu clears the active ones), on the descriptions'
current_profile_name */
static void profile_name_show(struct widget_instance *description)
{
	/* (the profile read from its file at most once a second) */
	static struct player_profile profile;
	static long read_index = NONE;
	static unsigned long read_time;
	static boolean read_good;
	long index = player_ui_get_active_player_profile_index(0);

	if (!description)
		return;
	if (index != NONE)
	{
		player_ui_get_active_player_profile(0, &profile);
		read_index = NONE;
	}
	else if ((index = player_ui_get_player1_last_used_profile_index()) == NONE)
		return;
	else
	{
		/* (one that cannot be read too: tried again a second later) */
		if (index != read_index || system_milliseconds() - read_time > 1000)
		{
			read_index = index;
			read_good = player_profile_get(index, &profile);
			read_time = system_milliseconds();
		}
		if (!read_good)
			return;
	}
	text_set(named(description, "current_profile_name", 0), profile.player_name);
}

/* (Campaign's menu) "campaign menu init": player 1's profile and saved
game, for its items */
static boolean campaign_menu_initialize(short controller)
{
	struct player_profile profile;

	if (campaign_profile(controller, &profile))
		campaign_levels_read(&profile);
	return TRUE;
}

/* "campaign menu continue": the saved game goes on */
static boolean campaign_continue(short controller)
{
	struct player_profile profile;
	char const *map_name;
	short level, difficulty;

	if (!campaign_profile(controller, &profile) || !ui_widget_port_saved_game(&map_name, &level, &difficulty))
		return campaign_fail();
	campaign_start(map_name, difficulty, controller);
	return TRUE;
}

/* ---- the map lists (New Game's and the Map screen's): their first row's
chooser of SINGLEPLAYER (the campaign's levels), MULTIPLAYER maps (the
Xbox's), CUSTOM SINGLEPLAYER or CUSTOM MULTIPLAYER maps (the Custom Edition
maps of the custom_maps folder, by the scenario type their files give:
custom_edition_maps.c) (port_settings.MAP_KIND_CHOOSER), then the kind's
rows, scrolling as list_scroll scrolls. A singleplayer kind's map is played
as the campaign is (alone, or hosted as network co-op), a multiplayer kind's
as multiplayer maps are. */

enum
{
	MAP_KIND_SINGLEPLAYER,
	MAP_KIND_MULTIPLAYER,
	MAP_KIND_CUSTOM_SINGLEPLAYER,
	MAP_KIND_CUSTOM_MULTIPLAYER,
	NUMBER_OF_MAP_KINDS
};

/* the rows after the chooser */
#define MAP_KIND_ROWS 10

static void visible_set(struct widget_instance *widget, boolean visible);
short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);

static boolean map_kind_singleplayer(short kind)
{
	return kind == MAP_KIND_SINGLEPLAYER || kind == MAP_KIND_CUSTOM_SINGLEPLAYER;
}

/* The kind the chooser shows, kept to the multiplayer kinds unless
`singleplayer`: a singleplayer kind it was turned to becomes the next
multiplayer one the way it turned from `previous`. */
static short map_kind_shown(struct widget_instance *list, boolean singleplayer, short previous)
{
	struct widget_instance *spinner = named(list, "list_item_0_map_kind_spinner", 0);
	short kind;

	if (!spinner)
		return singleplayer ? MAP_KIND_SINGLEPLAYER : MAP_KIND_MULTIPLAYER;
	kind = (short)PIN(spinner->parameters.list.selected_index, 0, NUMBER_OF_MAP_KINDS - 1);
	if (!singleplayer && map_kind_singleplayer(kind))
	{
		short step = kind == (previous + NUMBER_OF_MAP_KINDS - 1) % NUMBER_OF_MAP_KINDS ? -1 : 1;

		do
			kind = (short)((kind + step + NUMBER_OF_MAP_KINDS) % NUMBER_OF_MAP_KINDS);
		while (map_kind_singleplayer(kind));
		spinner->parameters.list.selected_index = kind;
	}
	return kind;
}

static void map_kind_set(struct widget_instance *list, short kind)
{
	struct widget_instance *spinner = named(list, "list_item_0_map_kind_spinner", 0);

	if (spinner)
		spinner->parameters.list.selected_index = kind;
}

/* the first of count entries shown, for the entry chosen to be in the middle */
static short map_kind_first(short chosen, short count)
{
	return (short)PIN(chosen - MAP_KIND_ROWS / 2, 0, MAX(count - MAP_KIND_ROWS, 0));
}

static boolean list_scrolls_at_end(void);

static void map_kind_focus(struct widget_instance *list, short first, short chosen)
{
	focus_row(list, (short)(1 + chosen - first));
}

/* the rows after the chooser: count entries from *first's, scrolled on at
their ends; the entry focused, NONE when the chooser or the buttons have the
focus */
static short map_kind_rows_update(struct widget_instance *list, short *first, short count,
	void (*entry_text)(short entry, wchar_t *text))
{
	short shown = (short)MIN(count, MAP_KIND_ROWS);
	short row = focused_row(list), index = 0;
	struct widget_instance *child;

	if (row != NONE && row >= 1 && row == shown && *first + shown < count && list_scrolls_at_end())
	{
		(*first)++;
		focus_row(list, --row);
	}
	else if (row == 1 && *first > 0 && list_scrolls_at_end())
	{
		(*first)--;
		focus_row(list, ++row);
	}
	for (child = list->child; child; child = child->next, index++)
	{
		wchar_t text[ROW_TEXT_LENGTH];

		if (index < 1)
			continue;
		if (strncmp(child->name, "list_item_", 10))
			break;
		child->visible = index - 1 < shown;
		if (index - 1 < shown)
		{
			entry_text((short)(*first + index - 1), text);
			text_set(named(child, "list_item_text", 0), text);
		}
		visible_set(named(child, "list_item_arrows", 0), child == list->focused_child);
		visible_set(named(child, "scroll_up_button", 0), FALSE);
		visible_set(named(child, "scroll_down_button", 0), FALSE);
	}
	return row == NONE || row < 1 || row > shown ? NONE : (short)(*first + row - 1);
}

/* the Xbox's multiplayer maps: the first of the multiplayer maps
(ui_widget_port_multiplayer_maps), which go on with the Custom Edition ones */
static short xbox_multiplayer_map_count(short multiplayer_map_count)
{
	return (short)MAX(multiplayer_map_count - custom_edition_maps_count(FALSE), 0);
}

static void multiplayer_map_text(short map, wchar_t *text)
{
	string_get("pc\\main_menu\\mp_map_list", map, text);
}

/* a Custom Edition map's name (the menus find its name, picture and
description by its display index: custom_edition_maps.c) */
static void custom_map_text(boolean campaign, short map, wchar_t *text)
{
	wchar_t const *name = custom_edition_maps_name(custom_edition_maps_display_index_of(campaign, map));

	ustrncpy(text, name ? name : L"", ROW_TEXT_LENGTH - 1);
	text[ROW_TEXT_LENGTH - 1] = 0;
}

static void custom_campaign_map_text(short map, wchar_t *text)
{
	custom_map_text(TRUE, map, text);
}

static void custom_multiplayer_map_text(short map, wchar_t *text)
{
	custom_map_text(FALSE, map, text);
}

/* the description's map (each list's has both kinds' widgets): a campaign
level's picture, name and words (level_description sets them), or a map's
by its display index (a multiplayer map's, or a Custom Edition map's), or
(both NONE) neither */
static void map_description_show(struct widget_instance *description, short level, short map)
{
	struct widget_instance *widget;
	short marker;

	visible_set(named(description, "replay_level_right_name", 0), level != NONE);
	visible_set(named(description, "replay_level_right_pic", 0), level != NONE);
	visible_set(named(description, "replay_level_right_data", 0), level != NONE);
	visible_set(named(description, "mp_map_right_name", 0), map != NONE);
	visible_set(named(description, "mp_map_right_pic", 0), map != NONE);
	visible_set(named(description, "mp_map_right_data", 0), map != NONE);
	if (level == NONE)
	{
		for (marker = 0; marker < 3; marker++)
			visible_set(named(description, "difficulty_indicator", marker), FALSE);
	}
	if (map == NONE)
		return;
	if ((widget = named(description, "mp_map_right_name", 0)) != NULL)
		widget->parameters.text_box.string_list_index = map;
	if ((widget = named(description, "mp_map_right_pic", 0)) != NULL)
		widget->animation.current_frame_index = map;
	if ((widget = named(description, "mp_map_right_data", 0)) != NULL)
		widget->parameters.text_box.string_list_index = map;
}

/* New Game's list: SINGLEPLAYER's levels, the profile's reached, or
MULTIPLAYER's maps, played alone to walk around (no game engine: a campaign
game on the map); CUSTOM SINGLEPLAYER's maps played as the campaign's levels
are, at the difficulty chosen next, and CUSTOM MULTIPLAYER's walked around
as MULTIPLAYER's are */
static struct
{
	short kind;
	short first, chosen;
	/* the multiplayer maps (ui_widget_port_multiplayer_maps) */
	char const *const *map_names;
	short map_count;
} level_list;

/* a map list's entries of a kind (the Map screen's difficulties aside) */
static short map_kind_count(short kind, short multiplayer_map_count)
{
	switch (kind)
	{
	case MAP_KIND_SINGLEPLAYER:
		return NUMBER_OF_SINGLE_PLAYER_LEVELS;
	case MAP_KIND_MULTIPLAYER:
		return xbox_multiplayer_map_count(multiplayer_map_count);
	case MAP_KIND_CUSTOM_SINGLEPLAYER:
		return custom_edition_maps_count(TRUE);
	default:
		return custom_edition_maps_count(FALSE);
	}
}

/* the display index of a multiplayer or Custom Edition kind's entry, for its
description (map_description_show) */
static short map_kind_display_index(short kind, short entry)
{
	switch (kind)
	{
	case MAP_KIND_CUSTOM_SINGLEPLAYER:
		return custom_edition_maps_display_index_of(TRUE, entry);
	case MAP_KIND_CUSTOM_MULTIPLAYER:
		return custom_edition_maps_display_index_of(FALSE, entry);
	default:
		return entry;
	}
}

/* the level name of a Custom Edition kind's entry, or NULL */
static char const *custom_map_level_name(short kind, short entry)
{
	return custom_edition_maps_level_name(map_kind_display_index(kind, entry));
}

/* "initialize sp level list solo" starts on the level last played */
static boolean level_list_initialize(struct widget_instance *list, short controller)
{
	struct player_profile profile;
	short level, last_used;

	if (!campaign_profile(controller, &profile))
		return FALSE;
	campaign_levels_read(&profile);
	level = PIN(profile.last_single_player_map_played, 0, NUMBER_OF_SINGLE_PLAYER_LEVELS - 1);
	if (campaign.saved_level != NONE)
		level = campaign.saved_level;
	if (!campaign.levels[level].available)
		level = 0;
	campaign.shown_level = level;
	level_list.map_count = ui_widget_port_multiplayer_maps(&level_list.map_names, &last_used);
	level_list.kind = MAP_KIND_SINGLEPLAYER;
	level_list.chosen = level;
	level_list.first = map_kind_first(level, NUMBER_OF_SINGLE_PLAYER_LEVELS);
	map_kind_set(list, level_list.kind);
	map_kind_focus(list, level_list.first, level_list.chosen);
	return TRUE;
}

static void level_row_text(short level, wchar_t *text)
{
	string_get("pc\\main_menu\\map_list", campaign.levels[level].available ? level : LEVEL_UNAVAILABLE, text);
}

/* "solo map list update" */
static void level_list_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	short kind = map_kind_shown(list, TRUE, level_list.kind), count, entry, level;

	if (kind != level_list.kind)
	{
		level_list.kind = kind;
		level_list.first = level_list.chosen = 0;
	}
	count = map_kind_count(kind, level_list.map_count);
	entry = map_kind_rows_update(list, &level_list.first, count,
		kind == MAP_KIND_SINGLEPLAYER ? level_row_text :
		kind == MAP_KIND_MULTIPLAYER ? multiplayer_map_text :
		kind == MAP_KIND_CUSTOM_SINGLEPLAYER ? custom_campaign_map_text : custom_multiplayer_map_text);
	if (entry != NONE)
		level_list.chosen = entry;
	if (kind != MAP_KIND_SINGLEPLAYER)
	{
		map_description_show(description, NONE,
			level_list.chosen < count ? map_kind_display_index(kind, level_list.chosen) : NONE);
		profile_name_show(description);
		return;
	}
	map_description_show(description, 0, NONE);
	campaign.shown_level = level_list.chosen;
	level = campaign.shown_level;
	if (!campaign.levels[level].available)
		level = LEVEL_UNAVAILABLE;
	level_description(description, "replay_level", level, level == campaign.saved_level,
		level == LEVEL_UNAVAILABLE ? NULL : campaign.levels[level].finished, NONE);
	profile_name_show(description);
}

/* "solo level set map": the level shown, if reached, or the Custom Edition
campaign map, for the difficulty menu; or the multiplayer map, played at
once (FALSE: no difficulty menu). The list is read again first: a tap on
the touchscreen focuses a row and activates it at once, before the list's
update has read which row it is (it started the level shown before, Pillar
of Autumn on a new profile). The map and gametype lists do the same. */
static boolean level_choose(struct widget_instance *list, short controller)
{
	short level, count;

	level_list_update(list);
	level = campaign.shown_level;
	count = map_kind_count(level_list.kind, level_list.map_count);

	if (level_list.kind != MAP_KIND_SINGLEPLAYER && (level_list.chosen < 0 || level_list.chosen >= count))
		return campaign_fail();
	if (level_list.kind == MAP_KIND_MULTIPLAYER || level_list.kind == MAP_KIND_CUSTOM_MULTIPLAYER)
	{
		struct player_profile profile;
		char const *map_name = level_list.kind == MAP_KIND_MULTIPLAYER ? level_list.map_names[level_list.chosen] :
			custom_map_level_name(level_list.kind, level_list.chosen);

		if (!map_name || !campaign_profile(controller, &profile))
			return campaign_fail();
		campaign_start(map_name, main_get_difficulty(), controller);
		return FALSE;
	}
	if (level_list.kind == MAP_KIND_CUSTOM_SINGLEPLAYER)
	{
		char const *map_name = custom_map_level_name(level_list.kind, level_list.chosen);

		if (!map_name)
			return campaign_fail();
		main_set_map_name(map_name);
		main_defer_map_map_change();
		return TRUE;
	}
	if (level < 0 || level >= NUMBER_OF_SINGLE_PLAYER_LEVELS || !campaign.levels[level].available)
		return campaign_fail();
	/* (not yet: setting it at the main menu changes map, as the Xbox's has it
	not) */
	main_set_map_name(main_get_solo_level_name(level));
	main_defer_map_map_change();
	return TRUE;
}

static short difficulty_shown(struct widget_instance *widget)
{
	for (; widget; widget = widget->parent)
	{
		if (!strcmp(widget->name, "difficulty_select_list") && widget->parameters.list.extended_description)
		{
			struct widget_instance *picture = named(widget->parameters.list.extended_description,
				"difficulty_options", 0);

			return picture ? picture->animation.current_frame_index : main_get_difficulty();
		}
	}
	return main_get_difficulty();
}

static short sibling_index(struct widget_instance *widget)
{
	struct widget_instance *child;
	short index = 0;

	for (child = widget->parent ? widget->parent->child : widget; child && child != widget; child = child->next)
		index++;
	return index;
}

/* Load Game's list: each profile's saved game (read with the profile as
player 1's, which then is again) */
static void saved_games_read(short controller)
{
	struct player_profile profile, active;
	long profiles[MAXIMUM_PROFILES];
	long active_index;
	word count = MAXIMUM_PROFILES;
	short index;

	campaign.saved_game_count = 0;
	if (!campaign_profile(controller, &active))
		return;
	active_index = player_ui_get_active_player_profile_index(0);
	player_profiles_enumerate_available_to_local_player_index(NONE, &count, profiles, FALSE);
	for (index = 0; index < (short)count && campaign.saved_game_count < MAXIMUM_ROWS; index++)
	{
		struct campaign_saved_game *saved_game = &campaign.saved_games[campaign.saved_game_count];
		char const *map_name;

		if (!((unsigned long)profiles[index] & PROFILE_VALID_BIT) || !player_profile_get(profiles[index], &profile))
			continue;
		player_ui_set_active_player_profile(0, profiles[index], &profile);
		if (ui_widget_port_saved_game(&map_name, &saved_game->level, &saved_game->difficulty))
		{
			saved_game->profile_index = profiles[index];
			ustrncpy(saved_game->profile_name, profile.player_name, MAXIMUM_PLAYER_PROFILE_NAME_LENGTH);
			saved_game->profile_name[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH] = 0;
			campaign.saved_game_count++;
		}
	}
	player_ui_set_active_player_profile(0, active_index, &active);
	campaign_levels_read(&active);
}

/* "load game menu init" */
static boolean saved_game_list_initialize(struct widget_instance *list, short controller)
{
	short index;

	saved_games_read(controller);
	campaign.shown_saved_game = 0;
	for (index = 0; index < campaign.saved_game_count; index++)
	{
		if (campaign.saved_games[index].profile_index == player_ui_get_active_player_profile_index(0))
			campaign.shown_saved_game = index;
	}
	focus_row(list, campaign.saved_game_count ? campaign.shown_saved_game : NONE);
	return TRUE;
}

static void saved_game_row_text(short row, wchar_t *text)
{
	ustrncpy(text, campaign.saved_games[row].profile_name, ROW_TEXT_LENGTH - 1);
	text[ROW_TEXT_LENGTH - 1] = 0;
}

/* "load game list update" */
static void saved_game_list_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	struct widget_instance *item = named(description, "load_level_right_item", 0);
	short row = focused_row(list);

	rows_update(list, campaign.saved_game_count, saved_game_row_text);
	if (row != NONE && row < campaign.saved_game_count)
		campaign.shown_saved_game = row;
	if (campaign.shown_saved_game >= campaign.saved_game_count)
		campaign.shown_saved_game = campaign.saved_game_count - 1;
	if (item)
		item->visible = campaign.saved_game_count > 0;
	if (campaign.saved_game_count > 0)
	{
		struct campaign_saved_game const *saved_game = &campaign.saved_games[campaign.shown_saved_game];

		level_description(description, "load_level", saved_game->level, FALSE, NULL, saved_game->difficulty);
	}
	else if (row != NONE)
	{
		focus_row(list, NONE);
	}
	profile_name_show(description);
}

/* "load game menu activated": the saved game goes on, with its profile as
player 1's */
static boolean saved_game_continue(short controller)
{
	struct campaign_saved_game const *saved_game;
	struct player_profile profile;

	if (campaign.shown_saved_game < 0 || campaign.shown_saved_game >= campaign.saved_game_count)
		return campaign_fail();
	saved_game = &campaign.saved_games[campaign.shown_saved_game];
	if (!player_profile_get(saved_game->profile_index, &profile))
		return campaign_fail();
	player_ui_set_active_player_profile(0, saved_game->profile_index, &profile);
	campaign_start(main_get_solo_level_name(saved_game->level), saved_game->difficulty, controller);
	return TRUE;
}

/* "load game menu delete request" (before its question) and "delete
finish" (its OK): the saved game shown goes */
static boolean saved_game_delete_request(void)
{
	if (campaign.shown_saved_game < 0 || campaign.shown_saved_game >= campaign.saved_game_count)
		return campaign_fail();
	campaign.saved_game_to_delete = campaign.shown_saved_game;
	return TRUE;
}

static boolean saved_game_delete(short controller)
{
	char path[256];

	if (campaign.saved_game_to_delete < 0 || campaign.saved_game_to_delete >= campaign.saved_game_count ||
		!player_profile_get_enclosing_directory_path(campaign.saved_games[campaign.saved_game_to_delete].profile_index,
			path) || strlen(path) + strlen("savegame.bin") >= sizeof(path))
	{
		return campaign_fail();
	}
	strcat(path, "savegame.bin");
	if (!DeleteFileA(path))
		platform_log("menus: could not delete %s", path);
	campaign.saved_game_to_delete = NONE;
	saved_games_read(controller);
	return TRUE;
}

/* the settings' screens (tools/port_settings.py): every spinner of a
setting in the screen */
static void settings_each(struct widget_instance *widget, boolean (*visit)(struct widget_instance *spinner,
	struct pc_menu_setting *setting))
{
	struct widget_instance *child;

	for (child = widget->child; child; child = child->next)
	{
		struct pc_menu_setting *setting = child->type == 2 && !spinner_item(child) ?
			pc_menu_setting_get(child->definition_tag_index) : NULL;

		if (setting)
			visit(child, setting);
		else
			settings_each(child, visit);
	}
}

static struct widget_instance *screen_of(struct widget_instance *widget)
{
	while (widget->parent)
		widget = widget->parent;
	return widget;
}

/* "port settings save" (OK): those changed written, and applied */
static boolean setting_changed_save(struct widget_instance *spinner, struct pc_menu_setting *setting)
{
	short index = spinner->parameters.list.selected_index;

	if (index < 0 || index >= setting->value_count || index == setting->loaded_index)
		return TRUE;
	if (!setting_write(setting->setting, setting->values[index]))
	{
		platform_log("menus: could not set %s", setting->setting);
		return FALSE;
	}
	platform_log("menus: %s = %s", setting->setting, setting->values[index]);
	setting->loaded_index = index;
	return TRUE;
}

/* "port settings defaults" */
static boolean setting_default_show(struct widget_instance *spinner, struct pc_menu_setting *setting)
{
	char text[300];

	if (setting_text(setting->setting, text, sizeof(text), TRUE))
		spinner->parameters.list.selected_index = setting_value_index(setting, text);
	return TRUE;
}

/* "port settings help": the line of the row chosen (by its label's string:
the buttons' is the first) */
static void settings_help(struct widget_instance *list)
{
	struct widget_instance *help = list->parameters.list.extended_description;
	struct widget_instance *row = list->focused_child;
	short index = 0;

	if (!help)
		return;
	if (row && row->child && row->child->type == 1 && strncmp(row->name, "button", 6))
	{
		short label = pc_menu_string_index(row->child->definition_tag_index);

		if (label != NONE)
			index = label + 1;
	}
	help->parameters.text_box.string_list_index = index;
}

/* Video Setup's Resolution and Window Size, in the one row's place
(tools/port_settings.py): the one the display mode shown uses (Window Size
the window's, Resolution fullscreen's and borderless's) is shown, and the
list passes over the other, which takes no focus while it is hidden */
static void video_rows_show(struct widget_instance *list)
{
	struct widget_instance *mode = named(list, "mode_spinner", 0);
	struct widget_instance *resolution = named(list, "op_resolution", 0);
	struct widget_instance *window_size = named(list, "op_window_size", 0);
	struct pc_menu_setting *setting = mode ? pc_menu_setting_get(mode->definition_tag_index) : NULL;
	struct widget_instance *shown, *child;
	short index;

	if (!setting || !resolution || !window_size)
		return;
	index = mode->parameters.list.selected_index;
	shown = index >= 0 && index < setting->value_count && !strcmp(setting->values[index], "windowed") ?
		window_size : resolution;
	resolution->visible = shown == resolution;
	window_size->visible = shown == window_size;
	/* (the focus on the one hidden goes to the one shown: Defaults can
	change the mode while it has it) */
	if (list->focused_child == (shown == resolution ? window_size : resolution))
	{
		for (index = 0, child = list->child; child && child != shown; child = child->next)
			index++;
		list->focused_child = shown;
		list->parameters.list.selected_index = index;
	}
}

/* Graphics' button style follows the pending HUD choice, including Defaults.
The remaining options close the gap; the help and bottom buttons stay put. */
static void hud_button_style_row_show(struct widget_instance *list)
{
	struct widget_instance *hud = named(list, "high_res_hud_spinner", 0);
	struct widget_instance *row = named(list, "op_hud_button_style", 0);
	struct pc_menu_setting *setting = hud ? pc_menu_setting_get(hud->definition_tag_index) : NULL;
	struct widget_instance *child;
	short index;
	boolean visible;

	if (!setting || !row) return;
	index = hud->parameters.list.selected_index;
	visible = index >= 0 && index < setting->value_count && !strcmp(setting->values[index], "true");
	if (row->visible != visible)
	{
		for (child = row->next; child; child = child->next)
			if (!strncmp(child->name, "op_", 3))
				child->vertical_offset += visible ? 30 : -30;
		row->visible = visible;
	}
	if (!visible && list->focused_child == row)
	{
		for (index = 0, child = list->child; child && child != hud->parent; child = child->next)
			index++;
		list->focused_child = hud->parent;
		list->parameters.list.selected_index = index;
	}
}

/* ---------- Change Color: the profile's colour, from a list of the
game's colours (more than its rows: it scrolls), by the Xbox's names for
its spinner's functions */

#define COLOR_COUNT 18
#define COLOR_ROWS 11

static struct
{
	/* the colour on the first row, and the colour chosen */
	short first;
	short color;
} color_list;

static void color_row_text(short row, wchar_t *text)
{
	string_get("pc\\main_menu\\settings_select\\player_setup\\player_profile_edit\\color_edit\\colors_list",
		(short)(color_list.first + row), text);
}

/* "color picker menu initialize": on the profile's colour */
static boolean color_list_initialize(struct widget_instance *list)
{
	struct player_profile *profile = player_ui_get_edit_player_profile();

	if (!profile)
		return FALSE;
	color_list.color = (short)PIN(profile->primary_color_index, 0, COLOR_COUNT - 1);
	color_list.first = (short)PIN(color_list.color - COLOR_ROWS / 2, 0, COLOR_COUNT - COLOR_ROWS);
	focus_row(list, (short)(color_list.color - color_list.first));
	return TRUE;
}

/* (ui_widget.c) whether the pointer moved the menus' focus last */
boolean ui_widget_port_pointer_focused(void);
boolean ui_widget_port_hover_focused(void);

/* Whether a list whose focus is on its first or last row moves on: when the
d-pad put it there. The pointer resting there would make it move every
frame; the wheel scrolls it instead. */
static boolean list_scrolls_at_end(void)
{
	return !ui_widget_port_pointer_focused();
}

/* a list of more items than its rows: the row chosen kept off its ends
while there are more past them, the list moving instead; the item chosen,
or NONE (its buttons) */
static short list_scroll(struct widget_instance *list, short *first, short count, short rows)
{
	short row = focused_row(list);

	if (row == rows - 1 && *first + rows < count && list_scrolls_at_end())
	{
		(*first)++;
		focus_row(list, --row);
	}
	else if (row == 0 && *first > 0 && list_scrolls_at_end())
	{
		(*first)--;
		focus_row(list, ++row);
	}
	return row == NONE || row >= rows ? NONE : (short)(*first + row);
}

/* "color picker update": the list scrolled on at its ends, the colour's
name and picture */
static void color_list_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	struct widget_instance *widget;
	short color = list_scroll(list, &color_list.first, COLOR_COUNT, COLOR_ROWS);

	if (color != NONE)
		color_list.color = color;
	rows_update(list, COLOR_ROWS, color_row_text);
	if ((widget = named(description, "color_right_name", 0)) != NULL)
		widget->parameters.text_box.string_list_index = color_list.color;
	if ((widget = named(description, "color_right_pic", 0)) != NULL)
		widget->animation.current_frame_index = color_list.color;
	profile_name_show(description);
}

/* "color picker select color" */
static boolean color_choose(void)
{
	struct player_profile *profile = player_ui_get_edit_player_profile();

	if (!profile)
		return FALSE;
	profile->primary_color_index = color_list.color;
	return TRUE;
}

/* ---------- Edit Profile Settings: its picture of Gamepad Setup, the
profile's button settings as the Xbox's Controller Setup shows them */

/* the picture's frames (the bitmap's: tools/port_settings.py's
BITMAP_FRAMES): Gamepad Setup's row's, as the list numbers its rows
(player_profile_edit_select_menu_update_extended_description), and after
the PC version's nine the Xbox's five of the button settings */
#define PROFILE_GAMEPAD_FRAME 2
#define PROFILE_FIRST_LAYOUT_FRAME 9
#define PROFILE_LAYOUT_FRAMES (_button_preset_swap_b_and_right_thumb + 1)

/* "port gamepad layout preview" (the picture's own): on Gamepad Setup's
row, the edited profile's button settings; the port's own (Bumper Jumper),
of which the Xbox has no picture, keeps the row's */
static void profile_gamepad_layout(struct widget_instance *picture)
{
	struct player_profile *profile = player_ui_get_edit_player_profile();
	short frame = picture->animation.current_frame_index;

	if (frame == PROFILE_GAMEPAD_FRAME ||
		(frame >= PROFILE_FIRST_LAYOUT_FRAME && frame < PROFILE_FIRST_LAYOUT_FRAME + PROFILE_LAYOUT_FRAMES))
	{
		byte preset = profile ? profile->controller_settings.button_preset : (byte)_button_preset_standard;

		picture->animation.current_frame_index = preset < PROFILE_LAYOUT_FRAMES ?
			(short)(PROFILE_FIRST_LAYOUT_FRAME + preset) : PROFILE_GAMEPAD_FRAME;
	}
}

/* ---------- Profiles: the player profiles, and a row to make one (as the
PC version's list has them, but by the Xbox's names for its spinner's
functions); choosing one makes it player 1's, the profile the campaign and
Settings use; deleting one asks first */

#define PROFILE_ROWS 11
#define PROFILE_DEFAULT_BIT 0x40000000UL

enum
{
	PROFILE_STEP_SELECT = 0,
	PROFILE_STEP_EDIT_MENU = 1,
	PROFILE_STEP_EMBLEM = 2,
	PROFILE_STEP_COLOR = 3
};

static boolean text_field_editing(struct widget_instance *row);
static void text_field_begin(struct widget_instance *row, char const *text, short maximum,
	void (*done)(char const *text));
static void text_field_end(boolean keep);
static char const *profile_text_field_poll(void);
static void wide_to_text(wchar_t const *wide, char *text, short size);

static wchar_t const *const g_emblem_fg_names[FIREFIGHT_EMBLEM_FOREGROUND_COUNT] =
{
	L"Spartan Helmet",
	L"ODST Drop Pod",
	L"Valkyrie Wings",
	L"Crossed Swords",
	L"Legendary Shield",
	L"UNSC Eagle",
	L"Flaming Skull",
	L"Covenant Mark",
	L"Marathon Symbol",
	L"Reclaimer Glyph",
	L"Nova Star",
	L"Wolf Fang",
	L"Spartan Spear",
	L"Phoenix Crest",
	L"Atomic Core",
	L"Noble Crown"
};

static wchar_t const *const g_emblem_bg_names[FIREFIGHT_EMBLEM_BACKGROUND_COUNT] =
{
	L"None",
	L"Circle",
	L"Shield",
	L"Diamond",
	L"Hexagon",
	L"Octagon",
	L"Cross",
	L"Star",
	L"Banner",
	L"Chevron",
	L"Oval",
	L"Square"
};

static wchar_t const *const g_halo_color_names[FIREFIGHT_EMBLEM_COLOR_COUNT] =
{
	L"White",
	L"Steel",
	L"Red",
	L"Orange",
	L"Gold",
	L"Olive",
	L"Green",
	L"Sage",
	L"Cyan",
	L"Teal",
	L"Cobalt",
	L"Blue",
	L"Violet",
	L"Purple",
	L"Pink",
	L"Crimson",
	L"Brown",
	L"Tan"
};

static wchar_t const *const g_mcc_nameplate_names[FIREFIGHT_NAMEPLATE_COUNT] =
{
	L"UNSC Standard",
	L"Halo Ring",
	L"Legendary Shield",
	L"ODST Drop Pod",
	L"Covenant Empire",
	L"Forerunner Glyph",
	L"Firefight Veteran",
	L"Golden Warthog",
	L"Pillar of Autumn",
	L"Reclaimer"
};

static struct
{
	long indices[MAXIMUM_PROFILES];
	struct player_profile profiles[MAXIMUM_PROFILES];
	/* the profiles; the row after them makes a new one */
	short count;
	short first;
	short chosen;
	long to_delete;
	short step;
	short edit_row;
	short emblem_row;
	struct firefight_emblem_info emblem;
	boolean prev_left;
	boolean prev_right;
} profile_list;

static void profile_name_edit_done(char const *text)
{
	wchar_t cleaned[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH + 1];
	short i;

	if (profile_list.chosen < 0 || profile_list.chosen >= profile_list.count)
		return;
	memset(cleaned, 0, sizeof(cleaned));
	for (i = 0; text && text[i] && i < MAXIMUM_PLAYER_PROFILE_NAME_LENGTH - 1; i++)
		cleaned[i] = (wchar_t)(unsigned char)text[i];
	cleaned[i] = 0;
	if (!player_name_clean(cleaned, MAXIMUM_PLAYER_PROFILE_NAME_LENGTH) || !cleaned[0])
	{
		ui_play_audio_feedback_sound(SOUND_ERROR);
		return;
	}
	memset(profile_list.profiles[profile_list.chosen].player_name, 0,
		sizeof(profile_list.profiles[profile_list.chosen].player_name));
	ustrncpy(profile_list.profiles[profile_list.chosen].player_name, cleaned,
		MAXIMUM_PLAYER_PROFILE_NAME_LENGTH - 1);
	player_profile_save(profile_list.indices[profile_list.chosen],
		&profile_list.profiles[profile_list.chosen]);
	player_ui_set_active_player_profile(0, profile_list.indices[profile_list.chosen],
		&profile_list.profiles[profile_list.chosen]);
	player_ui_remember_player1_profile(TRUE);
	firefight_stats_set_emblem(profile_list.indices[profile_list.chosen], &profile_list.emblem);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
}

static void profile_emblem_cycle(short row, short delta)
{
	switch (row)
	{
	case 0:
		profile_list.emblem.foreground = (short)((profile_list.emblem.foreground + delta +
			FIREFIGHT_EMBLEM_FOREGROUND_COUNT) % FIREFIGHT_EMBLEM_FOREGROUND_COUNT);
		break;
	case 1:
		profile_list.emblem.secondary_layer = profile_list.emblem.secondary_layer ? 0 : 1;
		break;
	case 2:
		profile_list.emblem.background = (short)((profile_list.emblem.background + delta +
			FIREFIGHT_EMBLEM_BACKGROUND_COUNT) % FIREFIGHT_EMBLEM_BACKGROUND_COUNT);
		break;
	case 3:
		profile_list.emblem.primary_color = (short)((profile_list.emblem.primary_color + delta +
			FIREFIGHT_EMBLEM_COLOR_COUNT) % FIREFIGHT_EMBLEM_COLOR_COUNT);
		break;
	case 4:
		profile_list.emblem.secondary_color = (short)((profile_list.emblem.secondary_color + delta +
			FIREFIGHT_EMBLEM_COLOR_COUNT) % FIREFIGHT_EMBLEM_COLOR_COUNT);
		break;
	case 5:
		profile_list.emblem.background_color = (short)((profile_list.emblem.background_color + delta +
			FIREFIGHT_EMBLEM_COLOR_COUNT) % FIREFIGHT_EMBLEM_COLOR_COUNT);
		break;
	case 6:
		profile_list.emblem.nameplate = (short)((profile_list.emblem.nameplate + delta +
			FIREFIGHT_NAMEPLATE_COUNT) % FIREFIGHT_NAMEPLATE_COUNT);
		break;
	default:
		return;
	}
	if (profile_list.chosen >= 0 && profile_list.chosen < profile_list.count)
		firefight_stats_set_emblem(profile_list.indices[profile_list.chosen], &profile_list.emblem);
	ui_play_audio_feedback_sound(SOUND_CURSOR);
}

/* the profiles, read again when they are not those read last */
static void profile_list_read(boolean always)
{
	long indices[MAXIMUM_PROFILES];
	word count = MAXIMUM_PROFILES;
	short index, valid = 0;

	player_profiles_enumerate_available_to_local_player_index(NONE, &count, indices, FALSE);
	for (index = 0; index < (short)count; index++)
	{
		if ((unsigned long)indices[index] & PROFILE_VALID_BIT)
			indices[valid++] = indices[index];
	}
	if (!always && valid == profile_list.count &&
		!memcmp(indices, profile_list.indices, valid * sizeof(indices[0])))
	{
		return;
	}
	profile_list.count = 0;
	for (index = 0; index < valid; index++)
	{
		if (player_profile_get(indices[index], &profile_list.profiles[profile_list.count]))
			profile_list.indices[profile_list.count++] = indices[index];
	}
	profile_list.chosen = (short)PIN(profile_list.chosen, 0, profile_list.count);
}

static short profile_list_rows(void)
{
	if (profile_list.step == PROFILE_STEP_EDIT_MENU)
		return 3;
	if (profile_list.step == PROFILE_STEP_EMBLEM)
		return 8;
	if (profile_list.step == PROFILE_STEP_COLOR)
		return COLOR_ROWS;
	return (short)MIN(profile_list.count + 1, PROFILE_ROWS);
}

/* "player profile list initialize": on player 1's profile */
static boolean profile_list_initialize(struct widget_instance *list)
{
	long active = player_ui_get_active_player_profile_index(0);
	short index;

	if (text_field_editing(NULL))
		text_field_end(FALSE);
	profile_list.step = PROFILE_STEP_SELECT;
	profile_list.edit_row = 0;
	profile_list.emblem_row = 0;
	profile_list_read(TRUE);
	if (active == NONE)
		active = player_ui_get_player1_last_used_profile_index();
	profile_list.chosen = 0;
	for (index = 0; index < profile_list.count; index++)
	{
		if (profile_list.indices[index] == active)
			profile_list.chosen = index;
	}
	profile_list.first = (short)PIN(profile_list.chosen - PROFILE_ROWS / 2, 0,
		MAX(0, profile_list.count + 1 - PROFILE_ROWS));
	focus_row(list, (short)(profile_list.chosen - profile_list.first));
	return TRUE;
}

static void profile_row_text(short row, wchar_t *text)
{
	if (profile_list.step == PROFILE_STEP_EDIT_MENU)
	{
		if (row == 0)
		{
			if (text_field_editing(NULL))
			{
				char const *typed = profile_text_field_poll();
				wchar_t wide[ROW_TEXT_LENGTH];
				short i;

				for (i = 0; typed && typed[i] && i < MAXIMUM_PLAYER_PROFILE_NAME_LENGTH; i++)
					wide[i] = (wchar_t)(unsigned char)typed[i];
				if ((system_milliseconds() / 500) % 2)
					wide[i++] = L'_';
				wide[i] = 0;
				usnprintf(text, ROW_TEXT_LENGTH - 1, L"Name: %s", wide);
				text[ROW_TEXT_LENGTH - 1] = 0;
			}
			else
			{
				ustrncpy(text, L"Change Name", ROW_TEXT_LENGTH - 1);
				text[ROW_TEXT_LENGTH - 1] = 0;
			}
		}
		else if (row == 1)
		{
			ustrncpy(text, L"Customise Emblem", ROW_TEXT_LENGTH - 1);
			text[ROW_TEXT_LENGTH - 1] = 0;
		}
		else
		{
			ustrncpy(text, L"Change Colour", ROW_TEXT_LENGTH - 1);
			text[ROW_TEXT_LENGTH - 1] = 0;
		}
		return;
	}
	if (profile_list.step == PROFILE_STEP_EMBLEM)
	{
		short fg = (short)PIN(profile_list.emblem.foreground, 0, FIREFIGHT_EMBLEM_FOREGROUND_COUNT - 1);
		short bg = (short)PIN(profile_list.emblem.background, 0, FIREFIGHT_EMBLEM_BACKGROUND_COUNT - 1);
		short c1 = (short)PIN(profile_list.emblem.primary_color, 0, FIREFIGHT_EMBLEM_COLOR_COUNT - 1);
		short c2 = (short)PIN(profile_list.emblem.secondary_color, 0, FIREFIGHT_EMBLEM_COLOR_COUNT - 1);
		short c3 = (short)PIN(profile_list.emblem.background_color, 0, FIREFIGHT_EMBLEM_COLOR_COUNT - 1);
		short np = (short)PIN(profile_list.emblem.nameplate, 0, FIREFIGHT_NAMEPLATE_COUNT - 1);

		switch (row)
		{
		case 0:
			usnprintf(text, ROW_TEXT_LENGTH - 1, L"Emblem: %s", g_emblem_fg_names[fg]);
			break;
		case 1:
			usnprintf(text, ROW_TEXT_LENGTH - 1, L"Toggle Detail: %s",
				profile_list.emblem.secondary_layer ? L"ON" : L"OFF");
			break;
		case 2:
			usnprintf(text, ROW_TEXT_LENGTH - 1, L"Background: %s", g_emblem_bg_names[bg]);
			break;
		case 3:
			usnprintf(text, ROW_TEXT_LENGTH - 1, L"Primary: %s", g_halo_color_names[c1]);
			break;
		case 4:
			usnprintf(text, ROW_TEXT_LENGTH - 1, L"Secondary: %s", g_halo_color_names[c2]);
			break;
		case 5:
			usnprintf(text, ROW_TEXT_LENGTH - 1, L"BG Colour: %s", g_halo_color_names[c3]);
			break;
		case 6:
			usnprintf(text, ROW_TEXT_LENGTH - 1, L"Nameplate: %s", g_mcc_nameplate_names[np]);
			break;
		default:
			usnprintf(text, ROW_TEXT_LENGTH - 1, L"Save & Return");
			break;
		}
		text[ROW_TEXT_LENGTH - 1] = 0;
		return;
	}
	if (profile_list.step == PROFILE_STEP_COLOR)
	{
		color_row_text(row, text);
		return;
	}
	{
		short item = (short)(profile_list.first + row);

		if (item < profile_list.count)
		{
			ustrncpy(text, profile_list.profiles[item].player_name, MAXIMUM_PLAYER_PROFILE_NAME_LENGTH);
			text[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH] = 0;
		}
		else
		{
			string_get("pc\\main_menu\\player_profiles_select\\profile_description_labels", 4, text);
		}
	}
}

static void visible_set(struct widget_instance *widget, boolean visible)
{
	if (widget)
		widget->visible = visible;
}

/* "3wide player profile list update": the rows, and the profile chosen:
its name, colour, current level, best difficulty and controls */
static void profile_list_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	struct widget_instance *screen = screen_of(list);
	struct widget_instance *widget;
	short item;
	boolean profile;

	profile_list_read(FALSE);
	visible_set(named(screen, "header_select_profile", 0), profile_list.step == PROFILE_STEP_SELECT);
	visible_set(named(list, "profile_manager_button_new", 0), profile_list.step == PROFILE_STEP_SELECT);
	visible_set(named(list, "profile_manager_button_delete", 0), profile_list.step == PROFILE_STEP_SELECT);

	if (profile_list.step == PROFILE_STEP_SELECT)
	{
		item = list_scroll(list, &profile_list.first, (short)(profile_list.count + 1), profile_list_rows());
		if (item != NONE)
			profile_list.chosen = item;
	}
	else if (profile_list.step == PROFILE_STEP_EDIT_MENU)
	{
		short r = focused_row(list);
		if (r != NONE && r < 3)
			profile_list.edit_row = r;
		if (text_field_editing(NULL) && profile_list.edit_row != 0)
			text_field_end(TRUE);
	}
	else if (profile_list.step == PROFILE_STEP_EMBLEM)
	{
		short r = focused_row(list);
		boolean left_down = input_key_is_down(_key_left_arrow);
		boolean right_down = input_key_is_down(_key_right_arrow);

		if (input_has_gamepad(0))
		{
			struct gamepad_state const *gp = input_get_gamepad_state(0);
			if (gp)
			{
				left_down = left_down || gp->buttons[_gamepad_binary_button_dpad_left] > 0;
				right_down = right_down || gp->buttons[_gamepad_binary_button_dpad_right] > 0;
			}
		}
		if (r != NONE && r < 8)
			profile_list.emblem_row = r;
		if (r != NONE && r >= 0 && r <= 6)
		{
			if (left_down && !profile_list.prev_left)
				profile_emblem_cycle(r, -1);
			else if (right_down && !profile_list.prev_right)
				profile_emblem_cycle(r, 1);
		}
		profile_list.prev_left = left_down;
		profile_list.prev_right = right_down;
	}
	else if (profile_list.step == PROFILE_STEP_COLOR)
	{
		short c = list_scroll(list, &color_list.first, COLOR_COUNT, COLOR_ROWS);
		if (c != NONE)
			color_list.color = (short)PIN(c, 0, COLOR_COUNT - 1);
	}

	rows_update(list, profile_list_rows(), profile_row_text);
	profile = profile_list.chosen < profile_list.count;
	text_set(named(description, "player_profile_right_name", 0),
		profile ? profile_list.profiles[profile_list.chosen].player_name : L"");
	visible_set(named(description, "empty_profile_slot_text", 0), !profile);
	visible_set(named(description, "qtr_screen_profile_color_pic", 0),
		profile && profile_list.step != PROFILE_STEP_EMBLEM);
	visible_set(named(description, "current_level_label", 0),
		profile && profile_list.step != PROFILE_STEP_EMBLEM);
	visible_set(named(description, "current_level", 0),
		profile && profile_list.step != PROFILE_STEP_EMBLEM);
	visible_set(named(description, "joystick_controls_label", 0),
		profile && profile_list.step != PROFILE_STEP_EMBLEM);
	visible_set(named(description, "joystick_controls", 0),
		profile && profile_list.step != PROFILE_STEP_EMBLEM);
	visible_set(named(description, "skill_level_label", 0), FALSE);
	visible_set(named(description, "skill_level", 0), FALSE);
	if (profile)
	{
		struct player_profile *chosen = &profile_list.profiles[profile_list.chosen];
		short level, difficulty;
		short shown_color = (profile_list.step == PROFILE_STEP_COLOR) ?
			color_list.color : chosen->primary_color_index;

		if ((widget = named(description, "qtr_screen_profile_color_pic", 0)) != NULL)
			widget->animation.current_frame_index = (short)PIN(shown_color, 0, COLOR_COUNT - 1);
		if ((widget = named(description, "current_level", 0)) != NULL)
		{
			widget->parameters.text_box.string_list_index =
				(short)PIN(chosen->last_single_player_map_played, 0, NUMBER_OF_SINGLE_PLAYER_LEVELS - 1);
		}
		if ((widget = named(description, "joystick_controls", 0)) != NULL)
			widget->parameters.text_box.string_list_index = chosen->controller_settings.invert_look ? 1 : 0;
		player_profile_get_highest_completed_solo_level(chosen, &level, &difficulty);
		if (level != NONE && profile_list.step != PROFILE_STEP_EMBLEM &&
			(widget = named(description, "skill_level", 0)) != NULL)
		{
			widget->parameters.text_box.string_list_index = difficulty;
			widget->visible = TRUE;
			visible_set(named(description, "skill_level_label", 0), TRUE);
		}
	}
	profile_name_show(description);
}

/* "profile manager select": selecting a profile sets it as Player 1's active
profile and opens the 'EDIT PROFILE' menu (Change Name, Customise Emblem,
Change Colour). Returns FALSE only for the '[New Profile]' slot so the XML's
if_failed conditional opens pm_empty_selected. */
static boolean profile_choose(struct widget_instance *widget, short controller)
{
	struct widget_instance *list = widget ? named(screen_of(widget), "profile_manager_select_list_2", 0) : NULL;
	struct player_profile profile;

	if (list)
		profile_list_update(list);

	if (profile_list.step == PROFILE_STEP_SELECT)
	{
		if (profile_list.chosen >= profile_list.count ||
			!player_profile_get(profile_list.indices[profile_list.chosen], &profile))
		{
			return FALSE;
		}
		player_ui_set_active_player_profile(0, profile_list.indices[profile_list.chosen], &profile);
		player_ui_set_single_player_local_player_controller(0, controller);
		player_ui_remember_player1_profile(TRUE);
		firefight_stats_get_emblem(profile_list.indices[profile_list.chosen], &profile_list.emblem);
		profile_list.step = PROFILE_STEP_EDIT_MENU;
		profile_list.edit_row = 0;
		if (list)
			focus_row(list, 0);
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return TRUE;
	}

	if (profile_list.step == PROFILE_STEP_EDIT_MENU)
	{
		short row = list ? focused_row(list) : profile_list.edit_row;
		if (row == NONE)
			row = profile_list.edit_row;

		if (text_field_editing(NULL))
		{
			text_field_end(TRUE);
			return TRUE;
		}
		if (row == 0)
		{
			/* Change Name */
			char current_name[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH + 1];
			wide_to_text(profile_list.profiles[profile_list.chosen].player_name,
				current_name, sizeof(current_name));
			text_field_begin(list ? list->focused_child : widget, current_name,
				MAXIMUM_PLAYER_PROFILE_NAME_LENGTH - 1, profile_name_edit_done);
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			return TRUE;
		}
		if (row == 1)
		{
			/* Customise Emblem */
			firefight_stats_get_emblem(profile_list.indices[profile_list.chosen], &profile_list.emblem);
			profile_list.step = PROFILE_STEP_EMBLEM;
			profile_list.emblem_row = 0;
			profile_list.prev_left = TRUE;
			profile_list.prev_right = TRUE;
			if (list)
				focus_row(list, 0);
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			return TRUE;
		}
		if (row == 2)
		{
			/* Change Colour */
			struct player_profile *chosen = &profile_list.profiles[profile_list.chosen];
			color_list.color = (short)PIN(chosen->primary_color_index, 0, COLOR_COUNT - 1);
			color_list.first = (short)PIN(color_list.color - COLOR_ROWS / 2, 0, COLOR_COUNT - COLOR_ROWS);
			profile_list.step = PROFILE_STEP_COLOR;
			if (list)
				focus_row(list, (short)(color_list.color - color_list.first));
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			return TRUE;
		}
		return TRUE;
	}

	if (profile_list.step == PROFILE_STEP_EMBLEM)
	{
		short row = list ? focused_row(list) : NONE;
		if (row != NONE && row >= 0 && row <= 6)
		{
			profile_emblem_cycle(row, 1);
			return TRUE;
		}
		/* Row 7 ('Save & Return') or bottom OK button */
		firefight_stats_set_emblem(profile_list.indices[profile_list.chosen], &profile_list.emblem);
		profile_list.step = PROFILE_STEP_EDIT_MENU;
		profile_list.edit_row = 1;
		if (list)
			focus_row(list, 1);
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return TRUE;
	}

	if (profile_list.step == PROFILE_STEP_COLOR)
	{
		if (profile_list.chosen >= 0 && profile_list.chosen < profile_list.count)
		{
			struct player_profile *chosen = &profile_list.profiles[profile_list.chosen];
			chosen->primary_color_index = (short)PIN(color_list.color, 0, COLOR_COUNT - 1);
			player_profile_save(profile_list.indices[profile_list.chosen], chosen);
			player_ui_set_active_player_profile(0, profile_list.indices[profile_list.chosen], chosen);
			player_ui_remember_player1_profile(TRUE);
			firefight_stats_set_emblem(profile_list.indices[profile_list.chosen], &profile_list.emblem);
		}
		profile_list.step = PROFILE_STEP_EDIT_MENU;
		profile_list.edit_row = 2;
		if (list)
			focus_row(list, 2);
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return TRUE;
	}

	return TRUE;
}

/* "request del player profile" (before its question) */
static boolean profile_delete_request(void)
{
	if (profile_list.step != PROFILE_STEP_SELECT || profile_list.chosen >= profile_list.count)
		return campaign_fail();
	profile_list.to_delete = profile_list.indices[profile_list.chosen];
	return TRUE;
}

/* "final del player profile" (its OK): deleted, and if it was player 1's,
the first left is */
static boolean profile_delete(void)
{
	long index = profile_list.to_delete;
	boolean active = index == player_ui_get_active_player_profile_index(0);

	profile_list.to_delete = NONE;
	if (index == NONE || ((unsigned long)index & PROFILE_DEFAULT_BIT))
		return campaign_fail();
	player_profile_delete(index);
	profile_list_read(TRUE);
	if (active)
	{
		struct player_profile profile;

		if (profile_list.count && player_profile_get(profile_list.indices[0], &profile))
		{
			player_ui_set_active_player_profile(0, profile_list.indices[0], &profile);
			player_ui_remember_player1_profile(TRUE);
		}
		else
		{
			memset(&profile, 0, sizeof(profile));
			player_ui_set_active_player_profile(0, NONE, &profile);
		}
	}
	return TRUE;
}

/* ---------- Co-op: the campaign for two players on this machine, in split
screen (the Xbox's Cooperative Play, which the PC version has not):
Multiplayer's CO-OP CAMPAIGN ("port coop begin"), player 2's profile, chosen
with player 2's controller ("port coop player 2"), then New Game's levels
(those either has reached) and difficulty, with either player's controller
(ui_widget.c, widget_takes_events_of_controller). The main menu and
Multiplayer go back to one player (main_menu_initialize,
multiplayer_type_menu_initialize).
With one gamepad, it is player 2's (pc_menu_split_players) */

/* "port coop begin": two players, player 1 on its profile (campaign_profile)
and the controller that chose co-op */
static boolean coop_begin(short controller)
{
	struct player_profile profile;

	player_spawn_count = 1;
	player_ui_reset_single_player_local_player_controllers();
	if (!campaign_profile(controller, &profile))
		return campaign_fail();
	player_spawn_count = 2;
	return TRUE;
}

/* "port coop player 2 list initialize": on player 2's profile, else the first
that is not player 1's */
static boolean coop_player2_list_initialize(struct widget_instance *list)
{
	long player1 = player_ui_get_active_player_profile_index(0);
	long player2 = player_ui_get_active_player_profile_index(1);
	short index;

	profile_list.step = PROFILE_STEP_SELECT;
	profile_list_read(TRUE);
	profile_list.chosen = 0;
	for (index = profile_list.count - 1; index >= 0; index--)
	{
		if (profile_list.indices[index] != player1)
			profile_list.chosen = index;
	}
	for (index = 0; index < profile_list.count && player2 != NONE; index++)
	{
		if (profile_list.indices[index] == player2)
			profile_list.chosen = index;
	}
	profile_list.first = (short)PIN(profile_list.chosen - PROFILE_ROWS / 2, 0,
		MAX(0, profile_list.count + 1 - PROFILE_ROWS));
	focus_row(list, (short)(profile_list.chosen - profile_list.first));
	return TRUE;
}

/* "port coop player 2": the profile chosen made player 2's, on the controller
that chose it, which must not be player 1's */
static boolean coop_player2_choose(short controller)
{
	struct player_profile profile;

	if (player_spawn_count < 2 || profile_list.chosen >= profile_list.count ||
		!player_profile_get(profile_list.indices[profile_list.chosen], &profile))
	{
		return campaign_fail();
	}
	if (controller == player_ui_get_single_player_local_player_controller(0))
	{
		display_error_text_deferred(L"Player 2 chooses their\r\nprofile with their own\r\ncontroller.",
			NONE);
		return campaign_fail();
	}
	player_ui_set_single_player_local_player_controller(1, controller);
	player_ui_set_active_player_profile(1, profile_list.indices[profile_list.chosen], &profile);
	return TRUE;
}

/* ---------- Controls Setup: the keyboard and mouse's controls
(port/linux/include/halo_keyboard.h), shown a group at a time, two
bindings each */

#define CONTROL_BINDINGS 2
#define CONTROL_ROWS 7
#define CONTROL_NAME_LENGTH 32

static struct
{
	char const *setting;
	wchar_t const *label;
	short group;
} const controls[] =
{
	{ "controls.move_forward", L"MOVE FORWARD", 0 },
	{ "controls.move_backward", L"MOVE BACKWARD", 0 },
	{ "controls.strafe_left", L"STRAFE LEFT", 0 },
	{ "controls.strafe_right", L"STRAFE RIGHT", 0 },
	{ "controls.jump", L"JUMP", 0 },
	{ "controls.crouch", L"CROUCH", 0 },
	{ "controls.fire", L"FIRE", 1 },
	{ "controls.throw_grenade", L"THROW GRENADE", 1 },
	{ "controls.melee", L"MELEE", 1 },
	{ "controls.reload", L"RELOAD", 1 },
	{ "controls.zoom", L"ZOOM", 1 },
	{ "controls.switch_weapon", L"SWITCH WEAPON", 1 },
	{ "controls.switch_grenade", L"SWITCH GRENADE", 1 },
	{ "controls.action", L"ACTION", 2 },
	{ "controls.flashlight", L"FLASHLIGHT", 2 },
	{ "controls.scoreboard", L"SHOW SCORES", 2 },
	{ "controls.pause", L"PAUSE MENU", 2 },
	{ "controls.screenshot", L"SCREENSHOT", 2 },
	{ "controls.push_to_talk", L"PUSH TO TALK", 3 },
	{ "controls.chat", L"CHAT", 3 },
	{ "controls.team_chat", L"TEAM CHAT", 3 },
};

static struct
{
	/* the bindings shown, until OK writes them */
	char bindings[NUMBEROF(controls)][CONTROL_BINDINGS][CONTROL_NAME_LENGTH];
	/* the binding left and right choose, and the one being set */
	short slot;
	short capturing_control;
	short capturing_slot;
} controls_screen = { { { { 0 } } }, 0, NONE, 0 };

/* a setting's bindings ("Left Ctrl, C") into the control's */
static void control_bindings_read(short control, char const *text)
{
	short slot;

	for (slot = 0; slot < CONTROL_BINDINGS; slot++)
	{
		char *binding = controls_screen.bindings[control][slot];
		unsigned int length;

		while (*text == ' ' || *text == ',')
			text++;
		length = (unsigned int)strcspn(text, ",");
		if (length >= CONTROL_NAME_LENGTH)
			length = CONTROL_NAME_LENGTH - 1;
		memcpy(binding, text, length);
		binding[length] = 0;
		while (length && binding[length - 1] == ' ')
			binding[--length] = 0;
		text += strcspn(text, ",");
	}
}

static void control_bindings_text(short control, char *text, unsigned int size)
{
	char const *first = controls_screen.bindings[control][0];
	char const *second = controls_screen.bindings[control][1];

	snprintf(text, size, "%s%s%s", *first ? first : second, *first && *second ? ", " : "", *first ? second : "");
}

/* "controls screen init" (and "controls screen defaults": the defaults) */
static boolean controls_load(boolean defaults)
{
	short control;

	for (control = 0; control < NUMBEROF(controls); control++)
	{
		char text[128];

		if (defaults)
			config_default(controls[control].setting, text, sizeof(text));
		else
			snprintf(text, sizeof(text), "%s", config_string(controls[control].setting));
		control_bindings_read(control, text);
	}
	controls_screen.capturing_control = NONE;
	return TRUE;
}

/* "controls screen change set" (OK) */
static boolean controls_save(void)
{
	short control;
	boolean result = TRUE;

	for (control = 0; control < NUMBEROF(controls); control++)
	{
		char text[128];

		control_bindings_text(control, text, sizeof(text));
		if (strcmp(text, config_string(controls[control].setting)))
		{
			if (config_write(controls[control].setting, text))
				platform_log("menus: %s = %s", controls[control].setting, text);
			else
				result = FALSE;
		}
	}
	return result;
}

static struct widget_instance *control_row(struct widget_instance *widget)
{
	for (; widget; widget = widget->parent)
	{
		if (!strncmp(widget->name, "op_command_", 11))
			return widget;
	}
	return NULL;
}

/* the control on the row (op_command_<n>) in the group shown, else NONE */
static short control_of_row(struct widget_instance *row, short group)
{
	short wanted = row ? (short)(atoi(row->name + 11) - 1) : NONE;
	short control;

	for (control = 0; control < NUMBEROF(controls) && wanted >= 0; control++)
	{
		if (controls[control].group == group && wanted-- == 0)
			return control;
	}
	return NONE;
}

static short controls_group(struct widget_instance *screen)
{
	struct widget_instance *spinner = named(screen, "group_spinner", 0);

	return spinner ? spinner->parameters.list.selected_index : 0;
}

/* "controls begin binding" (A on a row): the next key or button pressed */
static boolean control_capture_begin(struct widget_instance *widget)
{
	short control = control_of_row(control_row(widget), controls_group(screen_of(widget)));

	if (control == NONE)
		return FALSE;
	controls_screen.capturing_control = control;
	controls_screen.capturing_slot = controls_screen.slot;
	platform_binding_capture_begin();
	return TRUE;
}

/* the key or button taken (then bound to no other control), or cleared */
static void control_capture_poll(void)
{
	short control = controls_screen.capturing_control;
	short slot = controls_screen.capturing_slot;
	int input = -1;
	int result;

	if (control == NONE || !(result = platform_binding_capture_poll(&input)))
		return;
	controls_screen.capturing_control = NONE;
	if (result == 1)
	{
		char name[CONTROL_NAME_LENGTH];
		short other, other_slot;

		halo_input_name(input, name, sizeof(name));
		if (!*name)
			return;
		for (other = 0; other < NUMBEROF(controls); other++)
		{
			for (other_slot = 0; other_slot < CONTROL_BINDINGS; other_slot++)
			{
				if (!_stricmp(controls_screen.bindings[other][other_slot], name))
					controls_screen.bindings[other][other_slot][0] = 0;
			}
		}
		snprintf(controls_screen.bindings[control][slot], CONTROL_NAME_LENGTH, "%s", name);
	}
	else if (result == 2)
	{
		controls_screen.bindings[control][slot][0] = 0;
	}
}

/* "controls update menu": the group's rows, their bindings (the one left
and right choose marked on the row chosen), and the help */
static void controls_update(struct widget_instance *list)
{
	short group = controls_group(list);
	struct widget_instance *row;
	struct widget_instance *focused_row = control_row(list->focused_child);

	control_capture_poll();
	for (row = list->child; row; row = row->next)
	{
		short control = control_of_row(row, group);
		short slot;

		if (strncmp(row->name, "op_command_", 11))
			continue;
		row->visible = control != NONE;
		if (control == NONE)
		{
			if (row == focused_row)
				focus_row(list, 0);
			continue;
		}
		text_set(named(row, "command_label", 0), controls[control].label);
		for (slot = 0; slot < CONTROL_BINDINGS; slot++)
		{
			char const *binding = controls_screen.bindings[control][slot];
			wchar_t text[ROW_TEXT_LENGTH];
			char name[CONTROL_NAME_LENGTH];
			char shown[64];
			short index;

			/* (the key as the keyboard's layout labels it: xinput_sdl.c) */
			if (*binding)
				halo_input_shown_name(binding, name, sizeof(name));
			else
				snprintf(name, sizeof(name), "%s", "-");
			if (controls_screen.capturing_control == control && controls_screen.capturing_slot == slot)
				snprintf(shown, sizeof(shown), "%s", "PRESS A KEY");
			else if (row == focused_row && controls_screen.slot == slot)
				snprintf(shown, sizeof(shown), "> %s <", name);
			else
				snprintf(shown, sizeof(shown), "%s", name);
			for (index = 0; shown[index] && index < ROW_TEXT_LENGTH - 1; index++)
				text[index] = (wchar_t)(unsigned char)shown[index];
			text[index] = 0;
			text_set(named(row, "command_binding", slot), text);
		}
	}
	if (list->parameters.list.extended_description)
	{
		list->parameters.list.extended_description->parameters.text_box.string_list_index =
			controls_screen.capturing_control != NONE ? 2 : focused_row ? 1 : 0;
	}
}

/* ---------- Mods (the main menu's: tools/port_settings.py's _mods), laid
out as the lobby is: the mods folder's mods, a row each (op_mod_<n>), as a
resource pack list is, the one that plays over the others at the top (the
load order upside down: the mod loaded last first), turned on or off and
moved up or down there, and in the frame beside them the mod chosen's
details; OK keeps it (port/linux/src/port_mods.c's load_order.toml) and
plays them on the map now (mods.c), Cancel forgets it */

#define MOD_ROWS 11
#define MOD_DETAILS_LENGTH 1024
/* the details' lines: as many as fit down the frame */
#define MOD_DETAILS_LINES 13
/* (from the shadow of each box's last character) */
#define MOD_TEXT_MARGIN 8

/* the text boxes whose lines are measured: the details' mod name and the
rest, and the rows' names */
enum
{
	_mod_details_name,
	_mod_details_text,
	_mod_row_name,
	NUMBER_OF_MOD_TEXT_BOXES
};

/* the text boxes' names, and as many characters as fit across each until
it has been drawn, and its font is known */
static char const *const mod_text_box_names[NUMBER_OF_MOD_TEXT_BOXES] = { "mod_details_name", "mod_details_text",
	"mod_name" };
static short const mod_text_box_characters[NUMBER_OF_MOD_TEXT_BOXES] = { 17, 30, 24 };

/* (port/linux/src/port_mods.c, port_mods.h's) */
enum
{
	_port_mod_fine,
	_port_mod_out_of_order,
	_port_mod_incompatible,
	_port_mod_unplayable,
};

long port_mods_count(void);
long port_mods_at(long position);
char const *port_mods_name(long index);
int port_mods_on(long index);
int port_mods_status(long index, char *text, unsigned long size);
int port_mods_details(long index, char *text, unsigned long size);
long port_mods_move(long position, long places);
void port_mods_turn(long index, int on);
int port_mods_save(void);
void port_mods_revert(void);
/* (mods.c) */
void mods_apply(void);

static struct
{
	/* the place on the list (0 the top: the mod loaded last) of the first
	row, and of the mod chosen last (MOVE UP and MOVE DOWN's, which take the
	focus from the rows) */
	short first;
	short chosen;
	/* the text boxes' fonts and widths as they were last drawn
	(menu_functions_text_box_drawn), which their lines are measured in;
	NONE before then */
	long fonts[NUMBER_OF_MOD_TEXT_BOXES];
	short widths[NUMBER_OF_MOD_TEXT_BOXES];
	/* what the details show (they are wrapped again when it changes), and
	the mod each row's name is of, in which font (NONE: none yet) */
	char shown[128];
	long row_mods[MOD_ROWS];
	long row_fonts[MOD_ROWS];
} mods_screen;

/* the row (op_mod_<n>) a widget is or is in, else NULL */
static struct widget_instance *mod_row(struct widget_instance *widget)
{
	for (; widget; widget = widget->parent)
	{
		if (!strncmp(widget->name, "op_mod_", 7))
			return widget;
	}
	return NULL;
}

/* the place on the list of the mod on a row, else NONE */
static short mod_row_place(struct widget_instance *row)
{
	short place = row ? (short)(mods_screen.first + atoi(row->name + 7) - 1) : NONE;

	return place >= 0 && place < port_mods_count() ? place : NONE;
}

/* the mod at a place on the list (its place in the load order counted from
the end), else NONE */
static long mod_at_place(short place)
{
	long count = port_mods_count();

	return place >= 0 && place < count ? port_mods_at(count - 1 - place) : NONE;
}

/* UTF-8 text (mod.toml's) as the menus' (a line break the game's \r\n; a
character the menus' UTF-16 cannot hold, or a broken one, a ?) */
static void mods_wide_text(char const *text, wchar_t *wide, short length)
{
	short index = 0;

	while (*text && index < length - 2)
	{
		unsigned char first = (unsigned char)*text++;
		unsigned long character = first;
		short following = first >= 0xF0 ? 3 : first >= 0xE0 ? 2 : first >= 0xC0 ? 1 : 0;

		if (first >= 0x80)
		{
			character = first & (0x3F >> following);
			for (; following > 0 && ((unsigned char)*text & 0xC0) == 0x80; following--)
				character = (character << 6) | ((unsigned char)*text++ & 0x3F);
			if (following || first < 0xC0 || character > 0xFFFF)
				character = '?';
		}
		if (character == '\n')
			wide[index++] = '\r';
		wide[index++] = (wchar_t)character;
	}
	wide[index] = 0;
}

/* the start of the UTF-8 character before `at` in text */
static size_t mods_character_before(char const *text, size_t at)
{
	if (at > 0)
		at--;
	while (at > 0 && ((unsigned char)text[at] & 0xC0) == 0x80)
		at--;
	return at;
}

/* whether `count` bytes of text fit across a text box (box): measured in
its font, from its left as it is drawn (leading spaces and all), else
counted */
static boolean mods_text_fits(short box, char const *text, size_t count)
{
	static real_argb_color const white = { 1.0f, 1.0f, 1.0f, 1.0f };
	char part[MOD_DETAILS_LENGTH];
	wchar_t wide[MOD_DETAILS_LENGTH];
	rectangle2d bounds;
	rectangle2d text_bounds;
	rectangle2d cursor;

	if (!count)
		return TRUE;
	if (mods_screen.fonts[box] == NONE)
		return count <= (size_t)mod_text_box_characters[box];
	if (count >= sizeof(part))
		return FALSE;
	memcpy(part, text, count);
	part[count] = 0;
	mods_wide_text(part, wide, NUMBEROF(wide));
	bounds.x0 = 0;
	bounds.y0 = 0;
	bounds.x1 = 0x3FFF;
	bounds.y1 = 480;
	draw_string_set_draw_mode(mods_screen.fonts[box], NONE, 0, 0, &white);
	draw_unicode_string_compute_bounds(&bounds, wide, &text_bounds, &cursor);
	return MAX(text_bounds.x1, cursor.x0) - bounds.x0 <= mods_screen.widths[box] - MOD_TEXT_MARGIN;
}

/* `count` bytes of text, as much of them as fits across a text box (box)
with "..." after it, into `shortened` */
static void mods_text_shorten(short box, char const *text, size_t count, char *shortened, size_t size)
{
	size_t kept = count;

	for (;;)
	{
		snprintf(shortened, size, "%.*s...", (int)kept, text);
		if (!kept || mods_text_fits(box, shortened, strlen(shortened)))
			return;
		kept = mods_character_before(text, kept);
	}
}

/* text, after what `wrapped` holds, in lines that fit across a text box
(box), broken at spaces and at its own line breaks (word by word: a word
too long for a line broken where it fits); `lines` lines in all at most,
the last cut short with "..." when there is more: how many it holds now */
static short mods_wrap(char const *text, char *wrapped, size_t size, short box, short lines, short line)
{
	size_t length = strlen(wrapped);

	while (line < lines && *text)
	{
		char const *end = strchr(text, '\n');
		size_t count = end ? (size_t)(end - text) : strlen(text);
		size_t cut = 0;
		boolean more;

		for (;;)
		{
			size_t next = cut;

			while (next < count && text[next] == ' ')
				next++;
			while (next < count && text[next] != ' ')
				next++;
			if (next == cut || !mods_text_fits(box, text, next))
				break;
			cut = next;
		}
		if (!cut && count)
		{
			for (cut = 0; cut < count && text[cut] == ' '; cut++)
			{
			}
			while (cut < count && text[cut] != ' ')
				cut++;
			while (cut > 1 && !mods_text_fits(box, text, cut))
				cut = mods_character_before(text, cut);
			if (!cut)
				cut = 1;
		}
		more = cut < count || (text[count] == '\n' && text[count + 1]);
		if (line == lines - 1 && more)
		{
			char shortened[MOD_DETAILS_LENGTH];

			mods_text_shorten(box, text, cut, shortened, sizeof(shortened));
			snprintf(wrapped + length, size - length, "%s%s", length ? "\n" : "", shortened);
			return lines;
		}
		snprintf(wrapped + length, size - length, "%s%.*s", length ? "\n" : "", (int)cut, text);
		length = strlen(wrapped);
		line++;
		text += cut;
		while (*text == ' ')
			text++;
		if (*text == '\n')
			text++;
	}
	return line;
}

/* "port mods init" (the list's created), and "port mods close" (its
deleted: what OK has not kept is forgotten) */
static boolean mods_screen_reset(void)
{
	short index;

	port_mods_revert();
	mods_screen.first = 0;
	mods_screen.chosen = 0;
	mods_screen.shown[0] = 0;
	for (index = 0; index < NUMBER_OF_MOD_TEXT_BOXES; index++)
	{
		mods_screen.fonts[index] = NONE;
		mods_screen.widths[index] = 0;
	}
	for (index = 0; index < MOD_ROWS; index++)
		mods_screen.row_mods[index] = NONE;
	return TRUE;
}

/* "port mods toggle" (A on a row; a mod that cannot be played stays as it
is, so that it is still off when it can be) */
static boolean mods_toggle(struct widget_instance *widget)
{
	short place = mod_row_place(mod_row(widget));
	long mod = mod_at_place(place);

	if (place == NONE)
		return FALSE;
	if (port_mods_status(mod, NULL, 0) != _port_mod_unplayable)
		port_mods_turn(mod, !port_mods_on(mod));
	return TRUE;
}

/* "port mods move up" and "port mods move down" (left and right on a row,
and the buttons): the mod of the row, else the one chosen last, moved one
place on the list (up: later in the load order, over one more mod); the
focus follows it on its row */
static boolean mods_move(struct widget_instance *widget, short places)
{
	struct widget_instance *row = mod_row(widget);
	short count = (short)port_mods_count();
	short place = row ? mod_row_place(row) : mods_screen.chosen;
	short moved;

	/* (no mods: nothing to move) */
	if (!count)
		return TRUE;
	if (place == NONE || place >= count)
		return FALSE;
	moved = (short)(count - 1 - port_mods_move(count - 1 - place, -places));
	mods_screen.chosen = moved;
	if (moved < mods_screen.first)
		mods_screen.first = moved;
	else if (moved >= mods_screen.first + MOD_ROWS)
		mods_screen.first = (short)(moved - MOD_ROWS + 1);
	if (row && row->parent)
		focus_row(row->parent, (short)(moved - mods_screen.first));
	return TRUE;
}

/* the details of the mod at `place` on the list, in the frame (the list's
description): its name; what is wrong with it, beside its icon; whether it
is on and its priority (1 the top, the mod that wins); and what its
mod.toml says */
static void mods_details_show(struct widget_instance *details, short place, short count)
{
	long mod = mod_at_place(place);
	struct widget_instance *icon = named(details, "mod_details_status", 0);
	char problem[MOD_DETAILS_LENGTH];
	char about[MOD_DETAILS_LENGTH];
	char text[MOD_DETAILS_LENGTH];
	wchar_t wide[MOD_DETAILS_LENGTH];
	int status;
	short line;
	char shown[sizeof(mods_screen.shown)];

	/* (wrapped again only when what they show, or its fonts, change) */
	snprintf(shown, sizeof(shown), "%ld %d %d %d %d %ld %d %ld %d", mod, port_mods_status(mod, NULL, 0),
		port_mods_on(mod), place, count, mods_screen.fonts[_mod_details_name],
		mods_screen.widths[_mod_details_name], mods_screen.fonts[_mod_details_text],
		mods_screen.widths[_mod_details_text]);
	if (!strcmp(shown, mods_screen.shown))
		return;
	strcpy(mods_screen.shown, shown);
	if (mod < 0)
	{
		/* (no mods: where they go) */
		mods_wide_text(count ? "" : "No mods", wide, NUMBEROF(wide));
		text_set_length(named(details, "mod_details_name", 0), wide, NUMBEROF(wide));
		text[0] = 0;
		if (!count)
		{
			mods_wrap("Each mod is a folder of its own in the mods folder, beside the maps folder.", text,
				sizeof(text), _mod_details_text, MOD_DETAILS_LINES, 0);
		}
		mods_wide_text(text, wide, NUMBEROF(wide));
		text_set_length(named(details, "mod_details_text", 0), wide, NUMBEROF(wide));
		visible_set(icon, FALSE);
		return;
	}
	status = port_mods_status(mod, problem, sizeof(problem));
	text[0] = 0;
	mods_wrap(port_mods_name(mod), text, sizeof(text), _mod_details_name, 2, 0);
	mods_wide_text(text, wide, NUMBEROF(wide));
	text_set_length(named(details, "mod_details_name", 0), wide, NUMBEROF(wide));
	visible_set(icon, status != _port_mod_fine);
	if (icon)
		icon->animation.current_frame_index = status == _port_mod_out_of_order ? 1 : 2;
	text[0] = 0;
	line = 0;
	if (status != _port_mod_fine)
	{
		/* (its first line indented, for the icon) */
		snprintf(text, sizeof(text), "     %s", problem);
		strcpy(problem, text);
		text[0] = 0;
		line = mods_wrap(problem, text, sizeof(text), _mod_details_text, MOD_DETAILS_LINES, line);
		line = mods_wrap(" ", text, sizeof(text), _mod_details_text, MOD_DETAILS_LINES, line);
	}
	snprintf(about, sizeof(about), "%s\nPriority: %d of %d", status == _port_mod_unplayable ? "Cannot be played" :
		port_mods_on(mod) ? "On" : "Off", place + 1, count);
	line = mods_wrap(about, text, sizeof(text), _mod_details_text, MOD_DETAILS_LINES, line);
	line = mods_wrap(" ", text, sizeof(text), _mod_details_text, MOD_DETAILS_LINES, line);
	port_mods_details(mod, about, sizeof(about));
	mods_wrap(about, text, sizeof(text), _mod_details_text, MOD_DETAILS_LINES, line);
	mods_wide_text(text, wide, NUMBEROF(wide));
	text_set_length(named(details, "mod_details_text", 0), wide, NUMBEROF(wide));
}

/* "port mods update": the rows (the list moving on at its ends while there
are more mods past them), each mod's name (cut short where it is too long),
ON or OFF and its icon (out of the order it asks for; another says it cannot
be played with it, or it cannot be played), and the details of the mod
chosen */
static void mods_update(struct widget_instance *list)
{
	struct widget_instance *row;
	struct widget_instance *focused = mod_row(list->focused_child);
	short count = (short)port_mods_count();
	short focused_index = focused ? (short)(atoi(focused->name + 7) - 1) : NONE;

	if (focused_index == MOD_ROWS - 1 && mods_screen.first + MOD_ROWS < count && list_scrolls_at_end())
	{
		mods_screen.first++;
		focus_row(list, --focused_index);
	}
	else if (focused_index == 0 && mods_screen.first > 0 && list_scrolls_at_end())
	{
		mods_screen.first--;
		focus_row(list, ++focused_index);
	}
	if (mods_screen.first > 0 && mods_screen.first + MOD_ROWS > count)
		mods_screen.first = (short)MAX(0, count - MOD_ROWS);
	if (focused_index != NONE && mods_screen.first + focused_index < count)
		mods_screen.chosen = (short)(mods_screen.first + focused_index);
	for (row = list->child; row; row = row->next)
	{
		short place;
		short row_index;
		long mod;
		struct widget_instance *icon;
		int status;

		if (strncmp(row->name, "op_mod_", 7))
			continue;
		/* (its background: the focused one's outlined frame, which the
		engine shows only on a bitmap of two frames, as the browser's) */
		row->animation.current_frame_index = row == focused ? 1 : 0;
		place = mod_row_place(row);
		row->visible = place != NONE;
		if (place == NONE)
		{
			if (row == focused)
				focus_row(list, count ? (short)(count - 1 - mods_screen.first) : NONE);
			continue;
		}
		mod = mod_at_place(place);
		status = port_mods_status(mod, NULL, 0);
		row_index = (short)PIN(atoi(row->name + 7) - 1, 0, MOD_ROWS - 1);
		if (mods_screen.row_mods[row_index] != mod ||
			mods_screen.row_fonts[row_index] != mods_screen.fonts[_mod_row_name])
		{
			char const *name = port_mods_name(mod);
			char shortened[MOD_DETAILS_LENGTH];
			wchar_t wide[ROW_TEXT_LENGTH];

			if (mods_text_fits(_mod_row_name, name, strlen(name)))
				snprintf(shortened, sizeof(shortened), "%s", name);
			else
				mods_text_shorten(_mod_row_name, name, strlen(name), shortened, sizeof(shortened));
			mods_wide_text(shortened, wide, ROW_TEXT_LENGTH);
			text_set(named(row, "mod_name", 0), wide);
			mods_screen.row_mods[row_index] = mod;
			mods_screen.row_fonts[row_index] = mods_screen.fonts[_mod_row_name];
		}
		text_set(named(row, "mod_state", 0), port_mods_on(mod) ? L"ON" : L"OFF");
		icon = named(row, "mod_status", 0);
		visible_set(icon, status != _port_mod_fine);
		if (icon)
			icon->animation.current_frame_index = status == _port_mod_out_of_order ? 1 : 2;
	}
	mods_details_show(list->parameters.list.extended_description, count ? mods_screen.chosen : NONE, count);
}

/* "port mods save" (OK): kept, and played at once (port_mods_save keeps
what it cannot write until the game quits) */
static boolean mods_save(void)
{
	port_mods_save();
	mods_apply();
	return TRUE;
}

/* ---------- Multiplayer (port/assets/menus' PLAN: Create Game > Internet
or LAN; Join Game > Server Browser (blank, for now), LAN, Direct Link): the
Xbox's networking, run by the engine's port entry points
(ui_widget_event_handler_functions.c) on our lists */

#define MAXIMUM_ADVERTISED_GAMES 9
#define GAMETYPE_ROWS 10
#define MAXIMUM_GAMETYPES 256
#define BROWSER_ROWS 15
#define LOBBY_ROWS 11
#define TEXT_FIELD_LENGTH 128
#define PLAYLIST_READ_ONLY_BIT 0x40000000UL
/* (a key stroke's modifier, as input_xbox.c has them: shift, control) */
#define KEY_MODIFIER_CONTROL_BIT 1
#define LOBBY_NAME "pc\\main_menu\\multiplayer_type_select\\lobby\\lobby_screen"
#define PREVIEW_NAME "pc\\main_menu\\multiplayer_type_select\\lobby\\preview_screen"
/* the server browser's password screen (tools/port_settings.py), and the
longest password */
#define PASSWORD_NAME "pc\\main_menu\\multiplayer_type_select\\join_game\\password\\password_screen"
#define PASSWORD_LENGTH 32
/* the lobby's panel's lines (the gametype, the players, the countdown, the
invite) */
#define LOBBY_TEXT_LENGTH (ROW_TEXT_LENGTH * 4)

enum
{
	_multiplayer_mode_server_browser,
	_multiplayer_mode_lan,
	_multiplayer_mode_direct_link,
	_multiplayer_mode_host_internet,
	_multiplayer_mode_host_lan,
	/* (Create Game's LOCAL: this machine alone, the bots' games) */
	_multiplayer_mode_host_local,
};

enum
{
	_gamemode_standard = 0,
	_gamemode_ranked = 1,
	_gamemode_firefight = 2,
	_gamemode_forge = 3,
	_gamemode_theater = 4,
	_gamemode_creation_hub = 5,
	_gamemode_dedi_server = 6,
};

enum
{
	_client_state_searching,
	_client_state_joining,
	_client_state_pregame,
	_client_state_ingame,
	_client_state_postgame,
};

/* a game the client found, as network_client_manager.c has it */
struct advertised_game
{
	byte key_id[8];
	byte key[16];
	/* XNADDR: size, flags, abEnet (the port's machine identifier), ina */
	byte xnaddr[12];
	byte nonce[8];
	unsigned long update_time;
	wchar_t game_name[16];
	long map_version;
	char map_name[0x80];
	short engine_type;
	word machine_count;
	word player_count;
	short maximum_player_count;
	short unknown100;
	short platform;
	boolean open;
	boolean valid;
	boolean has_teams;
	boolean oddball_variant;
};

typedef char verify_advertised_game_platform_offset[offsetof(struct advertised_game, platform) == 0xDE ? 1 : -1];
typedef char verify_advertised_game_open_offset[offsetof(struct advertised_game, open) == 0xE0 ? 1 : -1];

/* the engine's (port) */
short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);
boolean ui_widget_port_multiplayer_map_choose(short level_index);
boolean ui_widget_port_cooperative_level_choose(char const *map_name, short difficulty);
boolean ui_widget_port_campaign_map_for_gametype_choose(char const *map_name);
short ui_widget_port_gametypes(long *indices, short maximum, short *last_used);
boolean ui_widget_port_gametype_choose(long profile_index);
boolean ui_widget_port_host(struct widget_instance *widget, struct event_record *event, boolean *widget_deleted);
boolean ui_widget_port_host_local(struct widget_instance *widget, struct event_record *event, boolean *widget_deleted);
boolean ui_widget_port_browse(struct widget_instance *widget, struct event_record *event, boolean *widget_deleted);
boolean ui_widget_port_open(struct widget_instance *widget, char const *name, boolean *widget_deleted);
boolean network_game_client_advertised_game_in_progress(void *client, struct advertised_game *game);
byte network_game_client_advertised_game_kind(void *client, struct advertised_game *game);
boolean ui_widget_port_join(struct widget_instance *widget, void *advertised_game, char const *lobby_name,
	boolean *widget_deleted);
boolean ui_widget_port_multiplayer_player(short controller_index, long profile_index);
boolean ui_widget_port_unjoin_player(struct widget_instance *widget, struct event_record *event,
	boolean *widget_deleted);
void network_game_server_port_set_settings(wchar_t const *name, long maximum_players);
void network_game_server_port_set_cooperative_friendly_fire(short friendly_fire);
void network_game_server_port_set_cooperative_player_collisions(boolean player_collisions);
void network_game_server_port_set_cooperative_ally_friendly_fire(boolean ally_friendly_fire);
void *global_network_game_client_get(void);
void *global_network_game_server_get(void);
boolean network_game_is_splitscreen_local(void);
struct advertised_game *network_game_client_get_available_games(void *client);
boolean network_game_client_advertised_game_is_valid(struct advertised_game *game);
short network_game_client_get_state(void *client, short *state_data);
struct network_game *network_game_client_get_game(void *client);
struct network_game *network_game_server_get_game(void *server);
short network_game_client_get_local_machine_index(void);
short network_game_client_get_seconds_to_game_start(void *client);
boolean network_player_is_valid(struct network_player *player);
boolean playlist_profile_get(long index, struct game_variant *variant);
boolean playlist_profile_get_display_name(long index, wchar_t *name);
boolean playlist_profile_get_options(long index, struct game_variant_options *options);
boolean input_get_key(struct key_stroke *key);
boolean game_engine_running(void);
void game_engine_end_game(void);
/* the platform layer's */
int p2p_join_invite(char const *text);
int p2p_join_listing(struct p2p_listing const *listing);
int p2p_invite_link(char *link, int size);
void p2p_set_hosting_allowed(int allowed);
int p2p_peer_address(unsigned char const *identifier, unsigned long *address);
int platform_clipboard_get(char *text, int size);
void platform_clipboard_set(char const *text);
void platform_text_field(int typing, int password);
int config_boolean(char const *name);
void ui_widget_port_post_button(short controller_index, short button_index);

static wchar_t const *const engine_names[] = { L"", L"CTF", L"SLAYER", L"ODDBALL", L"KING OF THE HILL", L"RACE", L"FIREFIGHT" };
static short const maximum_players[] = { 2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128 };

static struct
{
	short mode;
	/* Create Game's gametype list */
	long gametypes[MAXIMUM_GAMETYPES];
	short gametype_count;
	/* (those of the bank the list's spinner shows: STANDARD, OPENCE, CUSTOM) */
	short bank[MAXIMUM_GAMETYPES], bank_count, bank_shown;
	short gametype_first, gametype_chosen;
	/* the server settings */
	wchar_t game_name[16];
	short maximum_players_index;
	/* (co-op's own, so it leaves multiplayer's as it was: each game hosted
	starts with COOPERATIVE_DEFAULT_PLAYERS players at most) */
	short cooperative_maximum_players_index;
	boolean cooperative_maximum_players_set;
	/* the co-op game's FRIENDLY FIRE, and its PLAYER COLLISIONS OFF, as it
	starts (CO-OP OPTIONS': network.coop_friendly_fire's and
	network.coop_player_collisions': server_start) */
	short cooperative_friendly_fire;
	boolean cooperative_no_player_collisions;
	/* the browser's games */
	struct advertised_game *games[MAXIMUM_ADVERTISED_GAMES];
	short game_count, game_chosen;
	/* the lobby's */
	short lobby_first;
	/* the game under way whose lobby is shown before joining it: its key
	(the client's slot it is in may come to hold another game) */
	byte preview_key_id[8];
	byte preview_xnaddr[12];
	/* Server Setup's LISTING: PRIVATE (each new game starts with
	network.host_public's choice: PUBLIC unless set otherwise) */
	boolean game_private;
	/* Server Setup's PASSWORD (a public internet game's; empty: none), kept
	while the game runs, never written down */
	char game_password[PASSWORD_LENGTH + 1];
	/* GAMEMODE selection: FALSE = Standard PvP, TRUE = Firefight-Exclusive Mode */
	boolean firefight_mode;
	short gamemode;
} multiplayer = { .maximum_players_index = NUMBEROF(maximum_players) - 1, .gamemode = _gamemode_standard };

/* ---- a text field (Direct Link's link, the game's name): the keyboard
types into it (Ctrl+V pastes), its row's A (enter) is done, B (escape)
cancels */
static struct
{
	struct widget_instance *row;
	char text[TEXT_FIELD_LENGTH];
	char before[TEXT_FIELD_LENGTH];
	short maximum;
	void (*done)(char const *text);
	/* a password's: shown as stars (text_field_begin_masked) */
	boolean masked;
} text_field;

/* when the field was last shown (a field not shown for a while is let go
of: its screen has gone) */
static unsigned long text_field_shown_time;

static boolean text_field_editing(struct widget_instance *row)
{
	return text_field.row && (!row || text_field.row == row);
}

static void text_field_open(struct widget_instance *row, char const *text, short maximum,
	void (*done)(char const *text), boolean masked)
{
	struct key_stroke key;

	text_field.row = row;
	snprintf(text_field.text, sizeof(text_field.text), "%s", text);
	snprintf(text_field.before, sizeof(text_field.before), "%s", text);
	text_field.maximum = (short)MIN(maximum, TEXT_FIELD_LENGTH - 1);
	text_field.done = done;
	text_field.masked = masked;
	text_field_shown_time = system_milliseconds();
	while (input_get_key(&key))
		;
	platform_text_field(TRUE, masked);
}

static void text_field_begin(struct widget_instance *row, char const *text, short maximum,
	void (*done)(char const *text))
{
	text_field_open(row, text, maximum, done, FALSE);
}

/* a password's field: as text_field_begin, its text shown as stars */
static void text_field_begin_masked(struct widget_instance *row, char const *text, short maximum,
	void (*done)(char const *text))
{
	text_field_open(row, text, maximum, done, TRUE);
}

static void text_field_end(boolean keep)
{
	void (*done)(char const *text) = text_field.done;

	platform_text_field(FALSE, FALSE);
	text_field.row = NULL;
	text_field.done = NULL;
	if (!keep)
		snprintf(text_field.text, sizeof(text_field.text), "%s", text_field.before);
	else if (done)
		done(text_field.text);
}

static void text_field_insert(char const *text)
{
	size_t length = strlen(text_field.text);

	for (; *text && length < (size_t)text_field.maximum; text++)
	{
		if (*text >= ' ' && *text <= '~')
			text_field.text[length++] = *text;
	}
	text_field.text[length] = 0;
}

/* the keys typed since the last frame, and the field shown (a caret
blinking while it is edited) */

static void text_field_show(struct widget_instance *value, char const *text, boolean editing)
{
	wchar_t shown[TEXT_FIELD_LENGTH + 2];
	short index;

	if (editing)
	{
		text_field_shown_time = system_milliseconds();
		struct key_stroke key;

		while (input_get_key(&key))
		{
			size_t length = strlen(text_field.text);

			if (key.key_code == _key_backspace)
			{
				if (length)
					text_field.text[length - 1] = 0;
			}
			else if (TEST_FLAG(key.modifier_flags, KEY_MODIFIER_CONTROL_BIT) && key.key_code == _key_v)
			{
				char clipboard[TEXT_FIELD_LENGTH];

				if (platform_clipboard_get(clipboard, sizeof(clipboard)))
					text_field_insert(clipboard);
			}
			else if ((unsigned char)key.ascii_code >= ' ' && (unsigned char)key.ascii_code <= '~')
			{
				char character[2] = { key.ascii_code, 0 };

				text_field_insert(character);
			}
		}
		text = text_field.text;
	}
	for (index = 0; text[index] && index < TEXT_FIELD_LENGTH; index++)
		shown[index] = editing && text_field.masked ? L'*' : (wchar_t)(unsigned char)text[index];
	if (editing && (system_milliseconds() / 500) % 2)
		shown[index++] = L'_';
	shown[index] = 0;
	text_set(value, shown);
	/* (what is being typed shown as it is: streamer mode hides nothing of it,
	text_filter.c) */
	if (editing && value)
		text_filter_exempt(value->parameters.text_box.text);
	else if (!text_field_editing(NULL))
		text_filter_exempt(NULL);
}

static char const *profile_text_field_poll(void)
{
	if (text_field_editing(NULL))
	{
		struct key_stroke key;

		text_field_shown_time = system_milliseconds();
		while (input_get_key(&key))
		{
			size_t length = strlen(text_field.text);

			if (key.key_code == _key_backspace)
			{
				if (length)
					text_field.text[length - 1] = 0;
			}
			else if (TEST_FLAG(key.modifier_flags, KEY_MODIFIER_CONTROL_BIT) && key.key_code == _key_v)
			{
				char clipboard[TEXT_FIELD_LENGTH];

				if (platform_clipboard_get(clipboard, sizeof(clipboard)))
					text_field_insert(clipboard);
			}
			else if ((unsigned char)key.ascii_code >= ' ' && (unsigned char)key.ascii_code <= '~')
			{
				char character[2] = { key.ascii_code, 0 };

				text_field_insert(character);
			}
		}
	}
	return text_field.text;
}

/* ---------- Creation Hub (Select GAMEMODE -> CREATION HUB):
 * Central tag cache ("creation_hub", no user-facing tag profiles),
 * Direct Map Creation (CREATE / EDIT MAP -> asks for New Map Name, Base Map to
 * Edit, and has a Test Firing Range shortcut),
 * 4-Zone Forerunner Firing Range (TEST FIRING RANGE),
 * Tag Extractor with Live 3D Rotating Preview & Usability Diagnostics,
 * Cached Tags & Settings, and Integrated Mods Manager. */

#define CREATION_HUB_CACHE_NAME "creation_hub"
#define TAG_ROWS 11
#define TAG_DETAILS_LENGTH 1024
#define TAG_DETAILS_LINES 13

enum
{
	_tag_details_name,
	_tag_details_text,
	_tag_row_name,
	NUMBER_OF_TAG_TEXT_BOXES
};

static char const *const tag_text_box_names[NUMBER_OF_TAG_TEXT_BOXES] = {
	"tag_details_name",
	"tag_details_text",
	"tag_name"
};

enum
{
	_tags_list_hub = 0,
	_tags_list_create_map,
	_tags_list_kinds,
	_tags_list_maps,
	_tags_list_categories,
	_tags_list_tags,
	_tags_list_profile_categories,
	_tags_list_profile,
	_tags_list_tag,
};

static struct
{
	char const *code;
	char const *label;
} const stock_base_maps[] = {
	{ "bloodgulch", "Blood Gulch (MP)" },
	{ "sidewinder", "Sidewinder (MP)" },
	{ "hangemhigh", "Hang 'Em High (MP)" },
	{ "chillout", "Chill Out (MP)" },
	{ "damnation", "Damnation (MP)" },
	{ "battlecreek", "Battle Creek (MP)" },
	{ "prisoner", "Prisoner (MP)" },
	{ "ratrace", "Rat Race (MP)" },
	{ "boardingaction", "Boarding Action (MP)" },
	{ "carousel", "Derelict (MP)" },
	{ "longest", "Longest (MP)" },
	{ "putput", "Chiron TL-34 (MP)" },
	{ "wizard", "Wizard (MP)" },
	{ "a10", "Pillar of Autumn (SP)" },
	{ "a30", "Halo (SP)" },
	{ "a50", "Truth & Reconciliation (SP)" },
	{ "b30", "Silent Cartographer (SP)" },
	{ "b40", "Assault on Control Room (SP)" },
	{ "c10", "343 Guilty Spark (SP)" },
	{ "c20", "The Library (SP)" },
	{ "c40", "Two Betrayals (SP)" },
	{ "d20", "Keyes (SP)" },
	{ "d40", "The Maw (SP)" },
};
#define STOCK_BASE_MAP_COUNT ((short)NUMBEROF(stock_base_maps))

static struct
{
	short list;
	short first;
	short chosen;
	short hub_chosen;
	short create_chosen;
	boolean campaign_maps;
	short map_first;
	short map_chosen;
	short map;
	short category_first;
	short category_chosen;
	short category;
	short tag_first;
	short tag_chosen;
	long profile;
	short profile_category_first;
	short profile_category_chosen;
	short profile_category;
	short profile_tag_first;
	short profile_tag_chosen;
	char new_map_name[48];
	short base_map_index;
	boolean inject_into_created_map;
	char create_status[256];
	char action_note[256];
	long fonts[NUMBER_OF_TAG_TEXT_BOXES];
	short widths[NUMBER_OF_TAG_TEXT_BOXES];
	char shown[256];
} tags_screen;

static long creation_hub_ensure_cache(void)
{
	long prof;

	port_tag_profiles_scan();
	prof = port_tag_profiles_find(CREATION_HUB_CACHE_NAME);
	if (prof < 0)
		prof = port_tag_profiles_create(CREATION_HUB_CACHE_NAME);
	tags_screen.profile = prof;
	return prof;
}

static boolean creation_hub_live_injection_on(void)
{
	char const *active = config_string("randomizer.tag_profile");

	return active && active[0] != 0;
}

static void creation_hub_set_live_injection(boolean on)
{
	long prof = creation_hub_ensure_cache();
	char const *folder = prof >= 0 ? port_tag_profiles_folder(prof) : CREATION_HUB_CACHE_NAME;

	config_write("randomizer.tag_profile", on && folder ? folder : "");
}

void tag_injector_profiles_poll(void)
{
	static boolean initialized = FALSE;
	unsigned long before = 0, after = 0;

	if (!initialized)
	{
		initialized = TRUE;
		port_tag_profiles_shared_import_all();
		creation_hub_ensure_cache();
	}
	if (tag_injector_profiles_unstored() > 0)
		tag_injector_profile_store_next(&before, &after);
}

void randomizer_profiles_poll(void)
{
}

void daily_menu_poll(void)
{
}

static struct widget_instance *tag_row(struct widget_instance *widget)
{
	for (; widget; widget = widget->parent)
	{
		if (!strncmp(widget->name, "op_tag_", 7))
			return widget;
	}
	return NULL;
}

static char const *tag_short_name(char const *path)
{
	char const *slash = strrchr(path, '\\');
	char const *forward = strrchr(path, '/');

	if (forward && (!slash || forward > slash))
		slash = forward;
	return slash ? slash + 1 : path;
}

static void tags_format_bytes(unsigned long bytes, char *out, size_t size)
{
	if (bytes >= 1024UL * 1024UL)
		snprintf(out, size, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
	else if (bytes >= 1024UL)
		snprintf(out, size, "%lu KB", (bytes + 512UL) / 1024UL);
	else
		snprintf(out, size, "%lu B", bytes);
}

static short tags_custom_maps_of_kind_count(boolean campaign)
{
	short i, total = tag_injector_map_count(), count = 0;

	for (i = 0; i < total; i++)
	{
		if ((tag_injector_map_campaign(i) != FALSE) == (campaign != FALSE))
			count++;
	}
	return count;
}

static short tags_profile_category_count(long profile, short category)
{
	long count = port_tag_profiles_tag_count(profile);
	long tag;
	short n = 0;

	for (tag = 0; tag < count; tag++)
	{
		if (tag_injector_group_in_category(port_tag_profiles_tag_group(profile, tag), category))
			n++;
	}
	return n;
}

static long tags_profile_category_tag(long profile, short category, short which)
{
	long count = port_tag_profiles_tag_count(profile);
	long tag;

	for (tag = 0; tag < count; tag++)
	{
		if (tag_injector_group_in_category(port_tag_profiles_tag_group(profile, tag), category) && which-- == 0)
			return tag;
	}
	return NONE;
}

static short creation_hub_base_map_total(void)
{
	short custom_cnt = tag_injector_map_count();
	if (custom_cnt < 0)
		custom_cnt = 0;
	return (short)(STOCK_BASE_MAP_COUNT + custom_cnt);
}

static void creation_hub_base_map_info(
	short index,
	char *out_label,
	size_t label_size,
	char *out_path,
	size_t path_size,
	char *out_code,
	size_t code_size)
{
	short total = creation_hub_base_map_total();

	if (total <= 0)
		index = 0;
	else
		index = (short)((index % total + total) % total);

	if (index < STOCK_BASE_MAP_COUNT)
	{
		char const *code = stock_base_maps[index].code;
		snprintf(out_label, label_size, "%s", stock_base_maps[index].label);
		snprintf(out_code, code_size, "%s", code);
		snprintf(out_path, path_size, "maps\\%s.map", code);
	}
	else
	{
		short c_idx = (short)(index - STOCK_BASE_MAP_COUNT);
		wchar_t const *wname = tag_injector_map_name(c_idx);
		char const *folder = tag_injector_map_folder(c_idx);
		char cname[64];

		if (wname)
			wide_to_text(wname, cname, sizeof(cname));
		else
			snprintf(cname, sizeof(cname), "bloodgulch");
		snprintf(out_label, label_size, "%s (Custom)", cname);
		if (folder && folder[0])
		{
			snprintf(out_code, code_size, "custom_maps\\%s\\%s", folder, cname);
			snprintf(out_path, path_size, "custom_maps\\%s\\%s.map", folder, cname);
		}
		else
		{
			snprintf(out_code, code_size, "custom_maps\\%s", cname);
			snprintf(out_path, path_size, "custom_maps\\%s.map", cname);
		}
	}
}

static void creation_hub_new_map_name_done(char const *text)
{
	short i, out = 0;

	for (i = 0; text && text[i] && out < (short)sizeof(tags_screen.new_map_name) - 1; i++)
	{
		char c = text[i];
		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
		else if (c == ' ' || c == '-')
			c = '_';
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
			tags_screen.new_map_name[out++] = c;
	}
	tags_screen.new_map_name[out] = 0;
	if (!tags_screen.new_map_name[0])
		snprintf(tags_screen.new_map_name, sizeof(tags_screen.new_map_name), "custom_forge_map");
	tags_screen.shown[0] = 0;
}

static boolean creation_hub_launch_firing_range_now(struct widget_instance *widget)
{
	long prof = creation_hub_ensure_cache();
	char const *folder = prof >= 0 ? port_tag_profiles_folder(prof) : CREATION_HUB_CACHE_NAME;

	firing_range_request(folder ? folder : CREATION_HUB_CACHE_NAME);
	campaign_start(firing_range_map_name(), 1, controller_of(widget));
	return TRUE;
}

static boolean creation_hub_build_and_open_map(struct widget_instance *widget)
{
	char base_label[96], base_path[1024], base_code[128];
	char dest_map_path[1024], dest_txt_path[1024], level_name[128];
	long prof = creation_hub_ensure_cache();
	long tag_cnt = prof >= 0 ? port_tag_profiles_tag_count(prof) : 0;
	FILE *txt_fp;

	if (text_field_editing(NULL))
		text_field_end(TRUE);
	if (!tags_screen.new_map_name[0])
		snprintf(tags_screen.new_map_name, sizeof(tags_screen.new_map_name), "custom_forge_map");

	creation_hub_base_map_info(
		tags_screen.base_map_index,
		base_label, sizeof(base_label),
		base_path, sizeof(base_path),
		base_code, sizeof(base_code));

	CreateDirectoryA("custom_maps", NULL);
	snprintf(dest_map_path, sizeof(dest_map_path), "custom_maps\\%s.map", tags_screen.new_map_name);
	snprintf(dest_txt_path, sizeof(dest_txt_path), "custom_maps\\%s.txt", tags_screen.new_map_name);

	if (_stricmp(base_path, dest_map_path) != 0)
	{
		if (!CopyFileA(base_path, dest_map_path, FALSE))
		{
			snprintf(tags_screen.create_status, sizeof(tags_screen.create_status),
				"Launching base map '%s' directly.", base_label);
			snprintf(level_name, sizeof(level_name), "%s", base_code);
		}
		else
		{
			txt_fp = fopen(dest_txt_path, "wb");
			if (txt_fp)
			{
				fprintf(txt_fp, "Created in Creation Hub\r\nBase: %s\r\nCached Tags: %ld\r\n",
					base_label, tag_cnt);
				fclose(txt_fp);
			}
			custom_edition_maps_look_again();
			tag_injector_maps_find();
			snprintf(level_name, sizeof(level_name), "custom_maps\\%s", tags_screen.new_map_name);
			snprintf(tags_screen.create_status, sizeof(tags_screen.create_status),
				"Created '%s' with %ld cached tags!", dest_map_path, tag_cnt);
		}
	}
	else
	{
		snprintf(level_name, sizeof(level_name), "custom_maps\\%s", tags_screen.new_map_name);
	}

	{
		extern int g_forge_session_enabled;

		g_forge_session_enabled = 1;
	}
	creation_hub_set_live_injection(tags_screen.inject_into_created_map);
	campaign_start(level_name, 1, controller_of(widget));
	return TRUE;
}

static short tags_list_count(void)
{
	switch (tags_screen.list)
	{
	case _tags_list_hub:
		return 6;
	case _tags_list_create_map:
		return 6;
	case _tags_list_kinds:
		return 2;
	case _tags_list_maps:
		return tags_custom_maps_of_kind_count(tags_screen.campaign_maps);
	case _tags_list_categories:
		return NUMBER_OF_TAG_INJECTOR_CATEGORIES + 1;
	case _tags_list_tags:
		return (short)(tag_injector_category_count(tags_screen.category) + 1);
	case _tags_list_profile_categories:
		return NUMBER_OF_TAG_INJECTOR_CATEGORIES + 2;
	case _tags_list_profile:
		return (short)(tags_profile_category_count(tags_screen.profile, tags_screen.profile_category) + 1);
	case _tags_list_tag:
		return 3;
	}
	return 0;
}

static short tag_row_place(struct widget_instance *row)
{
	short place = row ? (short)(tags_screen.first + atoi(row->name + 7) - 1) : NONE;

	return place >= 0 && place < tags_list_count() ? place : NONE;
}

static void tags_row_show(struct widget_instance *row, char const *name, char const *state)
{
	long saved_font = mods_screen.fonts[_mod_row_name];
	short saved_width = mods_screen.widths[_mod_row_name];
	char shortened[TAG_DETAILS_LENGTH];
	wchar_t wide[ROW_TEXT_LENGTH];

	mods_screen.fonts[_mod_row_name] = tags_screen.fonts[_tag_row_name];
	mods_screen.widths[_mod_row_name] = tags_screen.widths[_tag_row_name];
	if (mods_text_fits(_mod_row_name, name, strlen(name)))
		snprintf(shortened, sizeof(shortened), "%s", name);
	else
		mods_text_shorten(_mod_row_name, name, strlen(name), shortened, sizeof(shortened));
	mods_screen.fonts[_mod_row_name] = saved_font;
	mods_screen.widths[_mod_row_name] = saved_width;
	mods_wide_text(shortened, wide, ROW_TEXT_LENGTH);
	text_set(named(row, "tag_name", 0), wide);
	mods_wide_text(state, wide, ROW_TEXT_LENGTH);
	text_set(named(row, "tag_state", 0), wide);
}

static void tags_preview_update(struct widget_instance *details)
{
	struct widget_instance *preview = named(details, "tag_preview", 0);

	if (tags_screen.list == _tags_list_tags && tags_screen.chosen > 0 &&
		tags_screen.chosen - 1 < tag_injector_category_count(tags_screen.category))
	{
		tag_preview_map_tag(tags_screen.category, tags_screen.chosen - 1);
		visible_set(preview, TRUE);
		return;
	}
	if ((tags_screen.list == _tags_list_profile && tags_screen.chosen > 0) ||
		tags_screen.list == _tags_list_tag)
	{
		short place = (tags_screen.list == _tags_list_tag) ?
			(short)(tags_screen.profile_tag_chosen - 1) : (short)(tags_screen.chosen - 1);
		long tag = tags_profile_category_tag(tags_screen.profile, tags_screen.profile_category, place);
		if (tag != NONE)
		{
			tag_preview_profile_tag(tags_screen.profile, tag);
			visible_set(preview, TRUE);
			return;
		}
	}
	tag_preview_none();
	visible_set(preview, FALSE);
}

static void tags_details_show(struct widget_instance *details)
{
	long saved_name_font = mods_screen.fonts[_mod_details_name];
	short saved_name_width = mods_screen.widths[_mod_details_name];
	long saved_text_font = mods_screen.fonts[_mod_details_text];
	short saved_text_width = mods_screen.widths[_mod_details_text];
	char title[TAG_DETAILS_LENGTH];
	char body[TAG_DETAILS_LENGTH];
	char text[TAG_DETAILS_LENGTH];
	wchar_t wide[TAG_DETAILS_LENGTH];
	struct tag_injector_needs needs;
	short needs_state = _tag_preview_needs_waiting;
	long tag_cnt = tags_screen.profile >= 0 ? port_tag_profiles_tag_count(tags_screen.profile) : 0;
	unsigned long cache_bytes = tags_screen.profile >= 0 ? port_tag_profiles_size(tags_screen.profile) : 0;
	char size_str[32];
	char shown[sizeof(tags_screen.shown)];

	if (!details)
		return;

	tags_preview_update(details);
	memset(&needs, 0, sizeof(needs));
	needs_state = tag_preview_needs(&needs);

	snprintf(shown, sizeof(shown), "%d %d %d %d %d %d %ld %lu %d %s %s %s %d %d %ld %d %ld %d",
		tags_screen.list, tags_screen.chosen, (int)tags_screen.campaign_maps,
		tags_screen.map, tags_screen.category, tags_screen.profile_category,
		tag_cnt, cache_bytes, (int)needs_state,
		tags_screen.new_map_name, tags_screen.create_status, tags_screen.action_note,
		tags_screen.base_map_index, (int)tags_screen.inject_into_created_map,
		tags_screen.fonts[_tag_details_name], tags_screen.widths[_tag_details_name],
		tags_screen.fonts[_tag_details_text], tags_screen.widths[_tag_details_text]);
	if (!text_field_editing(NULL) && !strcmp(shown, tags_screen.shown))
		return;
	strcpy(tags_screen.shown, shown);

	title[0] = 0;
	body[0] = 0;
	tags_format_bytes(cache_bytes, size_str, sizeof(size_str));

	if (tags_screen.list == _tags_list_hub)
	{
		snprintf(title, sizeof(title), "CREATION HUB");
		switch (tags_screen.chosen)
		{
		case 0:
			snprintf(body, sizeof(body),
				"Create or edit a custom map file.\n\nChoose your new map name, pick any stock or custom base map to edit, inject your %ld cached tags (%s) from the Creation Hub cache, or jump straight into the Forerunner Firing Range.",
				tag_cnt, size_str);
			break;
		case 1:
			snprintf(body, sizeof(body),
				"Launch the 4-zone Forerunner Firing Range.\n\nIncludes the 35m Gun Range Hall with live DPS & spread tracking, the 40x25x50m Vehicle Track & Flight Zone, the Animation Stage, and the AI Fight Room with C-Uplink computer terminals.\n\nPress ESC -> TAG EDITOR inside the range to edit weapon & HUD values live!\nCached tags ready: %ld (%s)",
				tag_cnt, size_str);
			break;
		case 2:
			snprintf(body, sizeof(body),
				"Extract weapons, vehicles, bipeds, actors, sounds, and effects from Custom Edition maps into the Creation Hub's central deduplicated Tag Cache.\n\nIncludes a live 3D rotating preview and tag usability diagnostics.");
			break;
		case 3:
			snprintf(body, sizeof(body),
				"Browse all %ld tags (%s) stored in the Creation Hub Tag Cache.\n\nInspect 3D previews, configure weapon/biped options, test & edit live tag values in the Firing Range, or remove tags from the cache.",
				tag_cnt, size_str);
			break;
		case 4:
			snprintf(body, sizeof(body),
				"Open the Integrated Mods Manager (%ld mods detected).\n\nEnable, disable, and reorder mod packages from the mods folder.",
				port_mods_count());
			break;
		default:
			snprintf(body, sizeof(body),
				"Live Tag Injection: %s\n\nWhen ON, all %ld enabled tags (%s) in the Creation Hub Tag Cache are automatically injected into levels as they load.",
				creation_hub_live_injection_on() ? "ENABLED" : "DISABLED",
				tag_cnt, size_str);
			break;
		}
	}
	else if (tags_screen.list == _tags_list_create_map)
	{
		char base_label[96], base_path[1024], base_code[128];
		creation_hub_base_map_info(
			tags_screen.base_map_index,
			base_label, sizeof(base_label),
			base_path, sizeof(base_path),
			base_code, sizeof(base_code));
		snprintf(title, sizeof(title), "CREATE / EDIT MAP");
		snprintf(body, sizeof(body),
			"New Map File: custom_maps\\%s.map\nBase Map: %s\nInject Cached Tags: %s (%ld tags, %s)\n\n%s",
			tags_screen.new_map_name[0] ? tags_screen.new_map_name : "custom_forge_map",
			base_label,
			tags_screen.inject_into_created_map ? "YES" : "NO",
			tag_cnt, size_str,
			tags_screen.create_status[0] ? tags_screen.create_status :
			"Press A on NEW MAP NAME to type a map name, Left/Right on BASE MAP to select the template map, then select CREATE & OPEN MAP or TEST FIRING RANGE.");
	}
	else if (tags_screen.list == _tags_list_kinds)
	{
		char reason[256] = "";
		boolean avail = tag_injector_maps_available(reason, sizeof(reason));
		boolean camp = tags_screen.chosen == 0;
		snprintf(title, sizeof(title), camp ? "CAMPAIGN MAPS" : "MULTIPLAYER MAPS");
		if (!avail)
			snprintf(body, sizeof(body), "%s", reason[0] ? reason : "Place Custom Edition .map files in custom_maps to extract tags.");
		else
			snprintf(body, sizeof(body), "%d %s maps found in custom_maps.\n\nSelect a map to inspect and extract its weapons, vehicles, bipeds, and projectiles into the Creation Hub cache.",
				tags_custom_maps_of_kind_count(camp), camp ? "campaign" : "multiplayer");
	}
	else if (tags_screen.list == _tags_list_maps)
	{
		short map = tag_injector_map_of(tags_screen.campaign_maps, tags_screen.chosen);
		wchar_t const *wname = map != NONE ? tag_injector_map_name(map) : NULL;
		char mname[64] = "No Maps";
		if (wname)
			wide_to_text(wname, mname, sizeof(mname));
		snprintf(title, sizeof(title), "%s", mname);
		snprintf(body, sizeof(body), "Press A to open %s and browse its extractable tag categories.", mname);
	}
	else if (tags_screen.list == _tags_list_categories)
	{
		wchar_t const *wname = tag_injector_map_name(tags_screen.map);
		char mname[64] = "Map";
		if (wname)
			wide_to_text(wname, mname, sizeof(mname));
		snprintf(title, sizeof(title), "%s", mname);
		if (tags_screen.chosen == 0)
		{
			snprintf(body, sizeof(body),
				"Extract all compatible weapons, vehicles, and bipeds from %s into the Creation Hub Tag Cache.\n\n%s",
				mname, tags_screen.action_note);
		}
		else
		{
			short cat = (short)(tags_screen.chosen - 1);
			snprintf(body, sizeof(body), "%s in %s: %ld\nCached in Creation Hub: %ld (%s)\n\n%s",
				tag_injector_category_name(cat), mname,
				tag_injector_category_count(cat),
				tag_cnt, size_str, tags_screen.action_note);
		}
	}
	else if (tags_screen.list == _tags_list_tags)
	{
		short cat = tags_screen.category;
		if (tags_screen.chosen == 0)
		{
			snprintf(title, sizeof(title), "%s", tag_injector_category_name(cat));
			snprintf(body, sizeof(body), "Extract all %ld %s from this map into the Creation Hub Tag Cache.\n\n%s",
				tag_injector_category_count(cat), tag_injector_category_name(cat),
				tags_screen.action_note);
		}
		else
		{
			long idx = tags_screen.chosen - 1;
			char const *path = tag_injector_category_tag(cat, idx);
			boolean in_cache = tag_injector_tag_in_profile(tags_screen.profile, cat, idx) != NONE;
			char reason[128] = "";
			long tsize = tag_injector_tag_size(cat, idx, reason, sizeof(reason));
			char tsize_str[32] = "";

			if (tsize > 0)
				tags_format_bytes((unsigned long)tsize, tsize_str, sizeof(tsize_str));
			snprintf(title, sizeof(title), "%s", path ? tag_short_name(path) : "Tag");
			snprintf(body, sizeof(body), "%s  %s\n%s\n%s\n%s",
				in_cache ? "CACHED IN HUB" : "Press A to Extract to Cache",
				tsize_str,
				needs_state == _tag_preview_needs_read ?
					(needs.count > 0 ? "Tag dependencies checked (see 3D preview)" : "Ready in cache") :
					(reason[0] ? reason : "Inspecting tag..."),
				path ? path : "",
				tags_screen.action_note);
		}
	}
	else if (tags_screen.list == _tags_list_profile_categories)
	{
		snprintf(title, sizeof(title), "TAG CACHE (%ld)", tag_cnt);
		if (tags_screen.chosen < NUMBER_OF_TAG_INJECTOR_CATEGORIES)
		{
			short cat = tags_screen.chosen;
			snprintf(body, sizeof(body), "%s in Creation Hub Cache: %d\nTotal Cache Size: %s\n\nSelect to preview in 3D, configure options, test in the Firing Range, or remove.",
				tag_injector_category_name(cat),
				tags_profile_category_count(tags_screen.profile, cat),
				size_str);
		}
		else if (tags_screen.chosen == NUMBER_OF_TAG_INJECTOR_CATEGORIES)
		{
			snprintf(body, sizeof(body), "Launch the 4-zone Forerunner Firing Range to test and live-edit all %ld cached tags (%s).",
				tag_cnt, size_str);
		}
		else
		{
			snprintf(body, sizeof(body), "Clear all %ld extracted tags from the Creation Hub Tag Cache.\n\n%s",
				tag_cnt, tags_screen.action_note);
		}
	}
	else if (tags_screen.list == _tags_list_profile || tags_screen.list == _tags_list_tag)
	{
		short place = (tags_screen.list == _tags_list_tag) ?
			(short)(tags_screen.profile_tag_chosen - 1) : (short)(tags_screen.chosen - 1);
		long tag = place >= 0 ? tags_profile_category_tag(tags_screen.profile, tags_screen.profile_category, place) : NONE;

		if (tag == NONE)
		{
			snprintf(title, sizeof(title), "TEST FIRING RANGE");
			snprintf(body, sizeof(body),
				"Launch the Forerunner Firing Range with your %ld cached tags.\n\nInside the range, equip any weapon and press ESC -> TAG EDITOR to edit damage, fire rate, projectiles, and HUD layout live!",
				tag_cnt);
		}
		else
		{
			char disp[96], ctype[64], bsize[32];
			char const *tpath = port_tag_profiles_tag_name(tags_screen.profile, tag);
			char const *tmap = port_tag_profiles_tag_map(tags_screen.profile, tag);
			int enabled = port_tag_profiles_tag_enabled(tags_screen.profile, tag);

			tag_injector_tag_display_name(tags_screen.profile, tag, disp, sizeof(disp));
			tag_injector_tag_custom_type(tags_screen.profile, tag, ctype, sizeof(ctype));
			tags_format_bytes(port_tag_profiles_tag_size(tags_screen.profile, tag), bsize, sizeof(bsize));
			snprintf(title, sizeof(title), "%s", disp[0] ? disp : (tpath ? tag_short_name(tpath) : "Tag"));
			snprintf(body, sizeof(body), "%s%s%s (%s)\nFrom map: %s | %s\n%s\n%s",
				tag_injector_category_name(tags_screen.profile_category),
				ctype[0] ? " - " : "", ctype,
				bsize,
				tmap ? tmap : "cache",
				enabled ? "ENABLED" : "DISABLED",
				tpath ? tpath : "",
				tags_screen.action_note);
		}
	}

	mods_screen.fonts[_mod_details_name] = tags_screen.fonts[_tag_details_name];
	mods_screen.widths[_mod_details_name] = tags_screen.widths[_tag_details_name];
	mods_screen.fonts[_mod_details_text] = tags_screen.fonts[_tag_details_text];
	mods_screen.widths[_mod_details_text] = tags_screen.widths[_tag_details_text];
	text[0] = 0;
	mods_wrap(title, text, sizeof(text), _mod_details_name, 2, 0);
	mods_wide_text(text, wide, NUMBEROF(wide));
	text_set_length(named(details, "tag_details_name", 0), wide, NUMBEROF(wide));
	text[0] = 0;
	mods_wrap(body, text, sizeof(text), _mod_details_text,
		named(details, "tag_preview", 0) && named(details, "tag_preview", 0)->visible ?
			7 : TAG_DETAILS_LINES, 0);
	mods_wide_text(text, wide, NUMBEROF(wide));
	text_set_length(named(details, "tag_details_text", 0), wide, NUMBEROF(wide));
	mods_screen.fonts[_mod_details_name] = saved_name_font;
	mods_screen.widths[_mod_details_name] = saved_name_width;
	mods_screen.fonts[_mod_details_text] = saved_text_font;
	mods_screen.widths[_mod_details_text] = saved_text_width;
}

static boolean tags_screen_reset(struct widget_instance *list)
{
	short index;

	if (text_field_editing(NULL))
		text_field_end(FALSE);
	creation_hub_ensure_cache();
	tag_injector_maps_find();
	tags_screen.list = _tags_list_hub;
	tags_screen.first = 0;
	tags_screen.chosen = 0;
	tags_screen.shown[0] = 0;
	tags_screen.action_note[0] = 0;
	if (!tags_screen.new_map_name[0])
		snprintf(tags_screen.new_map_name, sizeof(tags_screen.new_map_name), "custom_forge_map");
	tags_screen.inject_into_created_map = TRUE;
	tags_screen.create_status[0] = 0;
	for (index = 0; index < NUMBER_OF_TAG_TEXT_BOXES; index++)
	{
		tags_screen.fonts[index] = NONE;
		tags_screen.widths[index] = 0;
	}
	if (list && !strcmp(list->name, "tags_select_list"))
		focus_row(list, 0);
	return TRUE;
}

static boolean tags_screen_close(void)
{
	if (text_field_editing(NULL))
		text_field_end(FALSE);
	tag_preview_dispose();
	tag_injector_map_close();
	return TRUE;
}

static void tags_open_list(struct widget_instance *list, short which, short chosen)
{
	short count;

	if (text_field_editing(NULL))
		text_field_end(TRUE);
	tags_screen.list = which;
	tags_screen.action_note[0] = 0;
	count = tags_list_count();
	tags_screen.chosen = count ? (short)PIN(chosen, 0, count - 1) : 0;
	tags_screen.first = (short)PIN(tags_screen.chosen - TAG_ROWS / 2, 0, MAX(0, count - TAG_ROWS));
	tags_screen.shown[0] = 0;
	if (list)
		focus_row(list, count ? (short)(tags_screen.chosen - tags_screen.first) : NONE);
}

static void tags_update(struct widget_instance *list)
{
	struct widget_instance *row;
	struct widget_instance *focused = tag_row(list->focused_child);
	struct widget_instance *new_button = named(list, "tags_button_new", 0);
	struct widget_instance *profile_button = named(list, "tags_button_profile", 0);
	short count = tags_list_count();
	short focused_index = focused ? (short)(atoi(focused->name + 7) - 1) : NONE;
	long tag_cnt = tags_screen.profile >= 0 ? port_tag_profiles_tag_count(tags_screen.profile) : 0;

	tag_injector_profiles_poll();

	if (focused_index == TAG_ROWS - 1 && tags_screen.first + TAG_ROWS < count && list_scrolls_at_end())
	{
		tags_screen.first++;
		focus_row(list, --focused_index);
	}
	else if (focused_index == 0 && tags_screen.first > 0 && list_scrolls_at_end())
	{
		tags_screen.first--;
		focus_row(list, ++focused_index);
	}
	if (tags_screen.first > 0 && tags_screen.first + TAG_ROWS > count)
		tags_screen.first = (short)MAX(0, count - TAG_ROWS);
	if (focused_index != NONE && tags_screen.first + focused_index < count)
		tags_screen.chosen = (short)(tags_screen.first + focused_index);

	for (row = list->child; row; row = row->next)
	{
		short place;

		if (strncmp(row->name, "op_tag_", 7))
			continue;
		row->animation.current_frame_index = row == focused ? 1 : 0;
		place = tag_row_place(row);
		row->visible = place != NONE;
		if (place == NONE)
		{
			if (row == focused)
				focus_row(list, count ? (short)(count - 1 - tags_screen.first) : NONE);
			continue;
		}
		if (tags_screen.list == _tags_list_hub)
		{
			char state[32];
			switch (place)
			{
			case 0:
				tags_row_show(row, "CREATE / EDIT MAP", "BUILDER");
				break;
			case 1:
				tags_row_show(row, "TEST FIRING RANGE", "LAUNCH");
				break;
			case 2:
				tags_row_show(row, "EXTRACT TAGS FROM MAPS", "IMPORT");
				break;
			case 3:
				snprintf(state, sizeof(state), "%ld TAGS", tag_cnt);
				tags_row_show(row, "CACHED TAGS & TAG EDITOR", state);
				break;
			case 4:
				snprintf(state, sizeof(state), "%ld MODS", port_mods_count());
				tags_row_show(row, "MODS MANAGER", state);
				break;
			default:
				tags_row_show(row, "LIVE TAG INJECTION", creation_hub_live_injection_on() ? "ON" : "OFF");
				break;
			}
		}
		else if (tags_screen.list == _tags_list_create_map)
		{
			char line[128], state[48];
			char base_label[96], base_path[1024], base_code[128];

			creation_hub_base_map_info(
				tags_screen.base_map_index,
				base_label, sizeof(base_label),
				base_path, sizeof(base_path),
				base_code, sizeof(base_code));
			switch (place)
			{
			case 0:
				if (text_field_editing(row))
				{
					char const *typed = profile_text_field_poll();
					snprintf(line, sizeof(line), "NEW MAP NAME: %s_", typed ? typed : "");
					tags_row_show(row, line, "TYPING");
				}
				else
				{
					snprintf(line, sizeof(line), "NEW MAP NAME: %s",
						tags_screen.new_map_name[0] ? tags_screen.new_map_name : "custom_forge_map");
					tags_row_show(row, line, "EDIT");
				}
				break;
			case 1:
				snprintf(line, sizeof(line), "BASE MAP: %s", base_label);
				tags_row_show(row, line, "< >");
				break;
			case 2:
				snprintf(state, sizeof(state), "%s (%ld)",
					tags_screen.inject_into_created_map ? "YES" : "NO",
					tag_cnt);
				tags_row_show(row, "INJECT CACHED WEAPONS/TAGS", state);
				break;
			case 3:
				tags_row_show(row, "CREATE & OPEN MAP", "GO");
				break;
			case 4:
				tags_row_show(row, "TEST FIRING RANGE", "LAUNCH");
				break;
			default:
				tags_row_show(row, "BACK TO CREATION HUB", "");
				break;
			}
		}
		else if (tags_screen.list == _tags_list_kinds)
		{
			char state[32];
			boolean camp = place == 0;
			snprintf(state, sizeof(state), "%d", tags_custom_maps_of_kind_count(camp));
			tags_row_show(row, camp ? "CAMPAIGN MAPS" : "MULTIPLAYER MAPS", state);
		}
		else if (tags_screen.list == _tags_list_maps)
		{
			short map = tag_injector_map_of(tags_screen.campaign_maps, place);
			wchar_t const *wname = map != NONE ? tag_injector_map_name(map) : NULL;
			char mname[64] = "";
			if (wname)
				wide_to_text(wname, mname, sizeof(mname));
			tags_row_show(row, mname, "");
		}
		else if (tags_screen.list == _tags_list_categories)
		{
			char state[32];
			if (place == 0)
			{
				tags_row_show(row, "EXTRACT ALL ADDABLE TAGS", "ALL");
			}
			else
			{
				short cat = (short)(place - 1);
				snprintf(state, sizeof(state), "%ld", tag_injector_category_count(cat));
				tags_row_show(row, tag_injector_category_name(cat), state);
			}
		}
		else if (tags_screen.list == _tags_list_tags)
		{
			short cat = tags_screen.category;
			if (place == 0)
			{
				char state[32];
				snprintf(state, sizeof(state), "%ld", tag_injector_category_count(cat));
				tags_row_show(row, "EXTRACT ALL IN CATEGORY", state);
			}
			else
			{
				long idx = place - 1;
				char const *path = tag_injector_category_tag(cat, idx);
				boolean in_cache = tag_injector_tag_in_profile(tags_screen.profile, cat, idx) != NONE;
				tags_row_show(row, path ? tag_short_name(path) : "", in_cache ? "CACHED" : "");
			}
		}
		else if (tags_screen.list == _tags_list_profile_categories)
		{
			char state[32];
			if (place < NUMBER_OF_TAG_INJECTOR_CATEGORIES)
			{
				snprintf(state, sizeof(state), "%d", tags_profile_category_count(tags_screen.profile, place));
				tags_row_show(row, tag_injector_category_name(place), state);
			}
			else if (place == NUMBER_OF_TAG_INJECTOR_CATEGORIES)
			{
				tags_row_show(row, "TEST FIRING RANGE", "LAUNCH");
			}
			else
			{
				tags_row_show(row, "CLEAR TAG CACHE", "");
			}
		}
		else if (tags_screen.list == _tags_list_profile)
		{
			if (place == 0)
			{
				tags_row_show(row, "TEST & EDIT IN FIRING RANGE", "LAUNCH");
			}
			else
			{
				long tag = tags_profile_category_tag(tags_screen.profile, tags_screen.profile_category, (short)(place - 1));
				char disp[96] = "";
				if (tag != NONE)
					tag_injector_tag_display_name(tags_screen.profile, tag, disp, sizeof(disp));
				tags_row_show(row, disp,
					tag != NONE && port_tag_profiles_tag_enabled(tags_screen.profile, tag) ? "ON" : "OFF");
			}
		}
		else if (tags_screen.list == _tags_list_tag)
		{
			long tag = tags_profile_category_tag(tags_screen.profile, tags_screen.profile_category,
				(short)(tags_screen.profile_tag_chosen - 1));
			int en = tag != NONE ? port_tag_profiles_tag_enabled(tags_screen.profile, tag) : 1;
			if (place == 0)
				tags_row_show(row, "ENABLED IN CACHE", en ? "ON" : "OFF");
			else if (place == 1)
				tags_row_show(row, "TEST & EDIT IN FIRING RANGE", "LAUNCH");
			else
				tags_row_show(row, "REMOVE FROM CACHE", "");
		}
	}

	if (new_button)
		text_set(new_button, L"FIRING RANGE");
	if (profile_button)
		text_set(profile_button, L"CREATE MAP");
	tags_details_show(list->parameters.list.extended_description);
}

static boolean tags_step(struct widget_instance *widget, short delta)
{
	struct widget_instance *list = named(screen_of(widget), "tags_select_list", 0);

	if (tags_screen.list == _tags_list_create_map)
	{
		if (tags_screen.chosen == 1)
		{
			short total = creation_hub_base_map_total();
			if (total > 0)
				tags_screen.base_map_index = (short)((tags_screen.base_map_index + delta + total) % total);
			tags_screen.shown[0] = 0;
			ui_play_audio_feedback_sound(SOUND_CURSOR);
			return TRUE;
		}
		if (tags_screen.chosen == 2)
		{
			tags_screen.inject_into_created_map = !tags_screen.inject_into_created_map;
			tags_screen.shown[0] = 0;
			ui_play_audio_feedback_sound(SOUND_CURSOR);
			return TRUE;
		}
	}
	else if (tags_screen.list == _tags_list_hub && tags_screen.chosen == 5)
	{
		creation_hub_set_live_injection(!creation_hub_live_injection_on());
		tags_screen.shown[0] = 0;
		ui_play_audio_feedback_sound(SOUND_CURSOR);
		if (list)
			tags_update(list);
		return TRUE;
	}
	return TRUE;
}

static boolean tags_choose(struct widget_instance *widget, boolean *widget_deleted)
{
	struct widget_instance *list = named(screen_of(widget), "tags_select_list", 0);
	struct widget_instance *row = tag_row(widget);
	short place = row ? tag_row_place(row) : tags_screen.chosen;

	if (place == NONE)
		return TRUE;
	tags_screen.chosen = place;

	if (tags_screen.list == _tags_list_hub)
	{
		tags_screen.hub_chosen = place;
		if (place == 0)
		{
			tags_open_list(list, _tags_list_create_map, 0);
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			return TRUE;
		}
		if (place == 1)
			return creation_hub_launch_firing_range_now(widget);
		if (place == 2)
		{
			tag_injector_maps_find();
			tags_open_list(list, _tags_list_kinds, 0);
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			return TRUE;
		}
		if (place == 3)
		{
			creation_hub_ensure_cache();
			tags_open_list(list, _tags_list_profile_categories, 0);
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			return TRUE;
		}
		if (place == 4)
			return ui_widget_port_open(widget, "pc\\main_menu\\mods\\mods_screen", widget_deleted);
		if (place == 5)
			return tags_step(widget, 1);
		return TRUE;
	}

	if (tags_screen.list == _tags_list_create_map)
	{
		tags_screen.create_chosen = place;
		if (place == 0)
		{
			if (text_field_editing(row))
				text_field_end(TRUE);
			else
				text_field_begin(row ? row : widget, tags_screen.new_map_name,
					sizeof(tags_screen.new_map_name) - 1, creation_hub_new_map_name_done);
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			return TRUE;
		}
		if (place == 1 || place == 2)
			return tags_step(widget, 1);
		if (place == 3)
			return creation_hub_build_and_open_map(widget);
		if (place == 4)
			return creation_hub_launch_firing_range_now(widget);
		tags_open_list(list, _tags_list_hub, 0);
		ui_play_audio_feedback_sound(SOUND_BACK);
		return TRUE;
	}

	if (tags_screen.list == _tags_list_kinds)
	{
		tags_screen.campaign_maps = (place == 0);
		if (tags_custom_maps_of_kind_count(tags_screen.campaign_maps) <= 0)
			return campaign_fail();
		tags_open_list(list, _tags_list_maps, 0);
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return TRUE;
	}

	if (tags_screen.list == _tags_list_maps)
	{
		short map = tag_injector_map_of(tags_screen.campaign_maps, place);
		if (map == NONE || !tag_injector_map_open(map))
			return campaign_fail();
		tags_screen.map_first = tags_screen.first;
		tags_screen.map_chosen = place;
		tags_screen.map = map;
		tags_open_list(list, _tags_list_categories, 0);
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return TRUE;
	}

	if (tags_screen.list == _tags_list_categories)
	{
		if (place == 0)
		{
			short cat;
			long idx, added = 0, carried = 0;
			char reason[128] = "";

			creation_hub_ensure_cache();
			for (cat = 0; cat < NUMBER_OF_TAG_INJECTOR_CATEGORIES; cat++)
			{
				long cnt;
				if (!tag_injector_category_addable(cat))
					continue;
				cnt = tag_injector_category_count(cat);
				for (idx = 0; idx < cnt; idx++)
				{
					if (tag_injector_tag_in_profile(tags_screen.profile, cat, idx) == NONE &&
						tag_injector_tag_add(tags_screen.profile, cat, idx, &carried, reason, sizeof(reason)))
					{
						added++;
					}
				}
			}
			snprintf(tags_screen.action_note, sizeof(tags_screen.action_note),
				"Extracted %ld new tags into Creation Hub Cache.", added);
			tags_screen.shown[0] = 0;
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			return TRUE;
		}
		tags_screen.category_first = tags_screen.first;
		tags_screen.category_chosen = place;
		tags_screen.category = (short)(place - 1);
		if (tag_injector_category_count(tags_screen.category) <= 0)
			return campaign_fail();
		tags_open_list(list, _tags_list_tags, 0);
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return TRUE;
	}

	if (tags_screen.list == _tags_list_tags)
	{
		short cat = tags_screen.category;
		long carried = 0;
		char reason[128] = "";

		creation_hub_ensure_cache();
		if (place == 0)
		{
			long idx, cnt = tag_injector_category_count(cat), added = 0;
			for (idx = 0; idx < cnt; idx++)
			{
				if (tag_injector_tag_in_profile(tags_screen.profile, cat, idx) == NONE &&
					tag_injector_tag_add(tags_screen.profile, cat, idx, &carried, reason, sizeof(reason)))
				{
					added++;
				}
			}
			snprintf(tags_screen.action_note, sizeof(tags_screen.action_note),
				"Extracted %ld tags from %s.", added, tag_injector_category_name(cat));
		}
		else
		{
			long idx = place - 1;
			long existing = tag_injector_tag_in_profile(tags_screen.profile, cat, idx);
			if (existing != NONE)
			{
				if (!tag_injector_tag_stock(tags_screen.profile, existing))
					port_tag_profiles_tag_remove(tags_screen.profile, existing);
				snprintf(tags_screen.action_note, sizeof(tags_screen.action_note), "Removed tag from Cache.");
			}
			else if (tag_injector_tag_add(tags_screen.profile, cat, idx, &carried, reason, sizeof(reason)))
			{
				snprintf(tags_screen.action_note, sizeof(tags_screen.action_note),
					"Added to Cache (+%ld sub-tags).", carried);
			}
			else
			{
				snprintf(tags_screen.action_note, sizeof(tags_screen.action_note),
					"%s", reason[0] ? reason : "Could not extract tag.");
				tags_screen.shown[0] = 0;
				return campaign_fail();
			}
		}
		tags_screen.shown[0] = 0;
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return TRUE;
	}

	if (tags_screen.list == _tags_list_profile_categories)
	{
		if (place == NUMBER_OF_TAG_INJECTOR_CATEGORIES)
			return creation_hub_launch_firing_range_now(widget);
		if (place == NUMBER_OF_TAG_INJECTOR_CATEGORIES + 1)
		{
			long count = port_tag_profiles_tag_count(tags_screen.profile);
			long t;
			for (t = count - 1; t >= 0; t--)
			{
				if (!tag_injector_tag_stock(tags_screen.profile, t))
					port_tag_profiles_tag_remove(tags_screen.profile, t);
			}
			snprintf(tags_screen.action_note, sizeof(tags_screen.action_note), "Cleared extracted tags from Cache.");
			tags_screen.shown[0] = 0;
			ui_play_audio_feedback_sound(SOUND_BACK);
			return TRUE;
		}
		tags_screen.profile_category_first = tags_screen.first;
		tags_screen.profile_category_chosen = place;
		tags_screen.profile_category = place;
		tags_open_list(list, _tags_list_profile, 0);
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return TRUE;
	}

	if (tags_screen.list == _tags_list_profile)
	{
		if (place == 0)
			return creation_hub_launch_firing_range_now(widget);
		tags_screen.profile_tag_first = tags_screen.first;
		tags_screen.profile_tag_chosen = place;
		tags_open_list(list, _tags_list_tag, 0);
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return TRUE;
	}

	if (tags_screen.list == _tags_list_tag)
	{
		long tag = tags_profile_category_tag(tags_screen.profile, tags_screen.profile_category,
			(short)(tags_screen.profile_tag_chosen - 1));
		if (tag == NONE)
			return campaign_fail();
		if (place == 0)
		{
			int en = port_tag_profiles_tag_enabled(tags_screen.profile, tag);
			port_tag_profiles_tag_set_enabled(tags_screen.profile, tag, !en);
			tags_screen.shown[0] = 0;
			ui_play_audio_feedback_sound(SOUND_CURSOR);
			return TRUE;
		}
		if (place == 1)
			return creation_hub_launch_firing_range_now(widget);
		if (!tag_injector_tag_stock(tags_screen.profile, tag) &&
			port_tag_profiles_tag_remove(tags_screen.profile, tag))
		{
			tags_open_list(list, _tags_list_profile,
				(short)MIN(tags_screen.profile_tag_chosen,
					tags_profile_category_count(tags_screen.profile, tags_screen.profile_category)));
			ui_play_audio_feedback_sound(SOUND_BACK);
			return TRUE;
		}
		return campaign_fail();
	}

	return TRUE;
}

static boolean tags_new_profile(struct widget_instance *widget)
{
	return creation_hub_launch_firing_range_now(widget);
}

static boolean tags_profile_button(struct widget_instance *widget)
{
	struct widget_instance *list = named(screen_of(widget), "tags_select_list", 0);

	tags_open_list(list, _tags_list_create_map, 0);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

static boolean tags_back(struct widget_instance *widget, boolean *widget_deleted)
{
	struct widget_instance *list = named(screen_of(widget), "tags_select_list", 0);

	if (text_field_editing(NULL))
	{
		text_field_end(FALSE);
		ui_play_audio_feedback_sound(SOUND_BACK);
		return TRUE;
	}
	tag_preview_none();
	if (tags_screen.list == _tags_list_tag)
	{
		tags_open_list(list, _tags_list_profile, tags_screen.profile_tag_chosen);
		ui_play_audio_feedback_sound(SOUND_BACK);
		return TRUE;
	}
	if (tags_screen.list == _tags_list_profile)
	{
		tags_open_list(list, _tags_list_profile_categories, tags_screen.profile_category_chosen);
		ui_play_audio_feedback_sound(SOUND_BACK);
		return TRUE;
	}
	if (tags_screen.list == _tags_list_tags)
	{
		tags_open_list(list, _tags_list_categories, tags_screen.category_chosen);
		ui_play_audio_feedback_sound(SOUND_BACK);
		return TRUE;
	}
	if (tags_screen.list == _tags_list_categories)
	{
		tag_injector_map_close();
		tags_open_list(list, _tags_list_maps, tags_screen.map_chosen);
		ui_play_audio_feedback_sound(SOUND_BACK);
		return TRUE;
	}
	if (tags_screen.list == _tags_list_maps)
	{
		tags_open_list(list, _tags_list_kinds, tags_screen.campaign_maps ? 0 : 1);
		ui_play_audio_feedback_sound(SOUND_BACK);
		return TRUE;
	}
	if (tags_screen.list == _tags_list_kinds ||
		tags_screen.list == _tags_list_create_map ||
		tags_screen.list == _tags_list_profile_categories)
	{
		tags_open_list(list, _tags_list_hub, tags_screen.hub_chosen);
		ui_play_audio_feedback_sound(SOUND_BACK);
		return TRUE;
	}
	ui_play_audio_feedback_sound(SOUND_BACK);
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;
	return TRUE;
}

/* ---------- Theater Mode (Select GAMEMODE -> THEATER):
 * Browse saved films in replays/, inspect map/gametype/duration/size/server,
 * watch in Halo 1 CE Theater HUD with timeline transport & free-cam,
 * copy replay folder path, and toggle automatic recording. */

#define THEATER_ROWS 11
#define THEATER_MAXIMUM 64
#define THEATER_DETAILS_LENGTH 1024
#define THEATER_DETAILS_LINES 13

enum
{
	_theater_details_name,
	_theater_details_text,
	_theater_row_name,
	NUMBER_OF_THEATER_TEXT_BOXES
};

static char const *const theater_text_box_names[NUMBER_OF_THEATER_TEXT_BOXES] = {
	"theater_details_name",
	"theater_details_text",
	"theater_name"
};

static struct
{
	char names[THEATER_MAXIMUM][NETWORK_REPLAY_NAME_SIZE];
	struct network_replay_info infos[THEATER_MAXIMUM];
	short count;
	short first;
	short chosen;
	short confirm_delete;
	char status_note[256];
	long fonts[NUMBER_OF_THEATER_TEXT_BOXES];
	short widths[NUMBER_OF_THEATER_TEXT_BOXES];
	char shown[256];
} theater;

static struct widget_instance *theater_row(struct widget_instance *widget)
{
	for (; widget; widget = widget->parent)
	{
		if (!strncmp(widget->name, "op_theater_", 11))
			return widget;
	}
	return NULL;
}

static short theater_row_place(struct widget_instance *row)
{
	short place = row ? (short)(theater.first + atoi(row->name + 11) - 1) : NONE;

	return place >= 0 && place < theater.count ? place : NONE;
}

static void theater_details_show(struct widget_instance *details)
{
	long saved_name_font = mods_screen.fonts[_mod_details_name];
	short saved_name_width = mods_screen.widths[_mod_details_name];
	long saved_text_font = mods_screen.fonts[_mod_details_text];
	short saved_text_width = mods_screen.widths[_mod_details_text];
	char title[THEATER_DETAILS_LENGTH];
	char body[THEATER_DETAILS_LENGTH];
	char text[THEATER_DETAILS_LENGTH];
	wchar_t wide[THEATER_DETAILS_LENGTH];
	boolean rec_on = config_boolean("network.record_replays") != 0;
	char shown[sizeof(theater.shown)];

	if (!details)
		return;
	snprintf(shown, sizeof(shown), "%d %d %d %d %s %ld %d %ld %d",
		theater.count, theater.chosen, theater.confirm_delete,
		(int)rec_on, theater.status_note,
		theater.fonts[_theater_details_name], theater.widths[_theater_details_name],
		theater.fonts[_theater_details_text], theater.widths[_theater_details_text]);
	if (!strcmp(shown, theater.shown))
		return;
	strcpy(theater.shown, shown);

	if (theater.chosen < 0 || theater.chosen >= theater.count)
	{
		char folder[256] = "";
		network_replay_folder(folder, sizeof(folder));
		snprintf(title, sizeof(title), "NO SAVED FILMS");
		snprintf(body, sizeof(body),
			"Recording is %s.\nFolder: %s\n\nPlayed Campaign, Firefight, and Multiplayer matches are saved automatically while Recording is ON.\n\nHalo CE Theater Controls:\nSPACE: Pause / Resume\nLEFT / RIGHT: Seek -/+ 10s\nUP / DOWN: Speed (0.25x..4x)\nTAB: Cycle Player | V: 1P / 3P / Free Cam\nH: Toggle CE HUD",
			rec_on ? "ON" : "OFF",
			folder[0] ? folder : "replays");
	}
	else
	{
		struct network_replay_info const *info = &theater.infos[theater.chosen];
		char info_lines[512] = "";

		network_replay_info_text(info, info_lines, sizeof(info_lines));
		snprintf(title, sizeof(title), "%s", info->map[0] ? info->map : theater.names[theater.chosen]);
		snprintf(body, sizeof(body),
			"%s\nRecording: %s\n\n%s",
			info_lines,
			rec_on ? "ON" : "OFF",
			theater.confirm_delete == theater.chosen ?
				"PRESS DELETE AGAIN TO CONFIRM" :
			theater.status_note[0] ?
				theater.status_note :
				"WATCH: Play Film in CE Theater\nCOPY PATH: Copy Replays Folder Path\nREC: Toggle Auto-Save");
	}

	mods_screen.fonts[_mod_details_name] = theater.fonts[_theater_details_name];
	mods_screen.widths[_mod_details_name] = theater.widths[_theater_details_name];
	mods_screen.fonts[_mod_details_text] = theater.fonts[_theater_details_text];
	mods_screen.widths[_mod_details_text] = theater.widths[_theater_details_text];
	text[0] = 0;
	mods_wrap(title, text, sizeof(text), _mod_details_name, 2, 0);
	mods_wide_text(text, wide, NUMBEROF(wide));
	text_set_length(named(details, "theater_details_name", 0), wide, NUMBEROF(wide));
	text[0] = 0;
	mods_wrap(body, text, sizeof(text), _mod_details_text, THEATER_DETAILS_LINES, 0);
	mods_wide_text(text, wide, NUMBEROF(wide));
	text_set_length(named(details, "theater_details_text", 0), wide, NUMBEROF(wide));
	mods_screen.fonts[_mod_details_name] = saved_name_font;
	mods_screen.widths[_mod_details_name] = saved_name_width;
	mods_screen.fonts[_mod_details_text] = saved_text_font;
	mods_screen.widths[_mod_details_text] = saved_text_width;
}

static boolean theater_refresh(struct widget_instance *list)
{
	short index;

	theater.count = (short)network_replay_list(theater.names, THEATER_MAXIMUM);
	for (index = 0; index < theater.count; index++)
		network_replay_info(theater.names[index], &theater.infos[index]);
	theater.first = 0;
	theater.chosen = 0;
	theater.confirm_delete = NONE;
	theater.status_note[0] = 0;
	theater.shown[0] = 0;
	for (index = 0; index < NUMBER_OF_THEATER_TEXT_BOXES; index++)
	{
		theater.fonts[index] = NONE;
		theater.widths[index] = 0;
	}
	if (list && !strcmp(list->name, "theater_select_list"))
		focus_row(list, theater.count ? 0 : NONE);
	return TRUE;
}

static void theater_update(struct widget_instance *list)
{
	struct widget_instance *row;
	struct widget_instance *focused = theater_row(list->focused_child);
	struct widget_instance *record_button = named(list, "theater_button_record", 0);
	short count = theater.count;
	short focused_index = focused ? (short)(atoi(focused->name + 11) - 1) : NONE;

	if (focused_index == THEATER_ROWS - 1 && theater.first + THEATER_ROWS < count && list_scrolls_at_end())
	{
		theater.first++;
		focus_row(list, --focused_index);
	}
	else if (focused_index == 0 && theater.first > 0 && list_scrolls_at_end())
	{
		theater.first--;
		focus_row(list, ++focused_index);
	}
	if (theater.first > 0 && theater.first + THEATER_ROWS > count)
		theater.first = (short)MAX(0, count - THEATER_ROWS);
	if (focused_index != NONE && theater.first + focused_index < count &&
		theater.chosen != (short)(theater.first + focused_index))
	{
		theater.chosen = (short)(theater.first + focused_index);
		theater.confirm_delete = NONE;
		theater.status_note[0] = 0;
	}
	for (row = list->child; row; row = row->next)
	{
		short place;
		struct network_replay_info const *info;
		long saved_font;
		short saved_width;
		char label[128], shortened[THEATER_DETAILS_LENGTH];
		wchar_t wide[ROW_TEXT_LENGTH];

		if (strncmp(row->name, "op_theater_", 11))
			continue;
		row->animation.current_frame_index = row == focused ? 1 : 0;
		place = theater_row_place(row);
		row->visible = place != NONE;
		if (place == NONE)
		{
			if (row == focused)
				focus_row(list, count ? (short)(count - 1 - theater.first) : NONE);
			continue;
		}
		info = &theater.infos[place];
		snprintf(label, sizeof(label), "%s - %s",
			info->map[0] ? info->map : theater.names[place],
			info->variant[0] ? info->variant : "Film");
		saved_font = mods_screen.fonts[_mod_row_name];
		saved_width = mods_screen.widths[_mod_row_name];
		mods_screen.fonts[_mod_row_name] = theater.fonts[_theater_row_name];
		mods_screen.widths[_mod_row_name] = theater.widths[_theater_row_name];
		if (mods_text_fits(_mod_row_name, label, strlen(label)))
			snprintf(shortened, sizeof(shortened), "%s", label);
		else
			mods_text_shorten(_mod_row_name, label, strlen(label), shortened, sizeof(shortened));
		mods_screen.fonts[_mod_row_name] = saved_font;
		mods_screen.widths[_mod_row_name] = saved_width;
		mods_wide_text(shortened, wide, ROW_TEXT_LENGTH);
		text_set(named(row, "theater_name", 0), wide);
		mods_wide_text(info->short_date, wide, ROW_TEXT_LENGTH);
		text_set(named(row, "theater_date", 0), wide);
	}
	if (record_button)
		text_set(record_button, config_boolean("network.record_replays") ? L"REC: ON" : L"REC: OFF");
	theater_details_show(list->parameters.list.extended_description);
}

static boolean theater_delete(struct widget_instance *widget)
{
	struct widget_instance *list = named(screen_of(widget), "theater_select_list", 0);
	short i;

	if (theater.chosen < 0 || theater.chosen >= theater.count)
		return campaign_fail();
	if (theater.confirm_delete != theater.chosen)
	{
		theater.confirm_delete = theater.chosen;
		theater.status_note[0] = 0;
		ui_play_audio_feedback_sound(SOUND_CURSOR);
		return TRUE;
	}
	if (!network_replay_delete(theater.names[theater.chosen]))
		return campaign_fail();
	theater.count = (short)network_replay_list(theater.names, THEATER_MAXIMUM);
	for (i = 0; i < theater.count; i++)
		network_replay_info(theater.names[i], &theater.infos[i]);
	theater.confirm_delete = NONE;
	theater.status_note[0] = 0;
	if (theater.chosen >= theater.count)
		theater.chosen = (short)MAX(0, theater.count - 1);
	if (list)
		focus_row(list, theater.count ? (short)(theater.chosen - theater.first) : NONE);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

static boolean theater_watch(struct widget_instance *widget)
{
	struct widget_instance *row = theater_row(widget);
	short place = row ? theater_row_place(row) : theater.chosen;
	char why[128] = "";

	if (place < 0 || place >= theater.count)
		return campaign_fail();
	theater.chosen = place;
	if (!network_replay_watch(theater.names[place], why, sizeof(why)))
	{
		snprintf(theater.status_note, sizeof(theater.status_note), "%s",
			why[0] ? why : "Cannot play this film.");
		theater.shown[0] = 0;
		return campaign_fail();
	}
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

static boolean theater_record(void)
{
	boolean next = !config_boolean("network.record_replays");

	config_write("network.record_replays", next ? "true" : "false");
	theater.shown[0] = 0;
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

static boolean theater_export(void)
{
	char folder[512] = "";

	network_replay_folder(folder, sizeof(folder));
	if (!folder[0])
		return campaign_fail();
	platform_clipboard_set(folder);
	theater.confirm_delete = NONE;
	snprintf(theater.status_note, sizeof(theater.status_note), "Copied Replays Folder to Clipboard:\n%s", folder);
	theater.shown[0] = 0;
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

/* ---------- Dedicated Server Director Screen (Select GAMEMODE -> DEDICATED SERVER):
 * Configure Dedicated / Listen Server name, rotation preset (including Firefight),
 * max players, public listing, AI bots, map sharing, copy CLI command or launch
 * the Web Admin Panel / Listen Server directly from the Halo CE UI. */

#define DEDI_ROWS 8

static char const *const dedi_presets[] = {
	"FIREFIGHT ROTATION",
	"SLAYER & CTF MIX",
	"CLASSIC ALL MODES",
	"CUSTOM EDITION MIX"
};
#define DEDI_PRESET_COUNT ((short)NUMBEROF(dedi_presets))

static char const *const dedi_skills[] = {
	"easy", "normal", "heroic", "legendary"
};

static struct
{
	short chosen;
	char server_name[32];
	short preset_idx;
	short max_players_idx;
	boolean public_lobby;
	short bots_count;
	short bots_skill_idx;
	boolean map_share;
	char status_msg[256];
} dedi_screen = {
	0, "OpenCE Dedicated", 0, 4, TRUE, 4, 1, TRUE, ""
};

static struct widget_instance *dedi_row(struct widget_instance *widget)
{
	for (; widget; widget = widget->parent)
	{
		if (!strncmp(widget->name, "op_dedi_", 8))
			return widget;
	}
	return NULL;
}

static boolean multiplayer_player(short controller);
static void game_name_done(char const *text);

static void dedi_name_done(char const *text)
{
	snprintf(dedi_screen.server_name, sizeof(dedi_screen.server_name), "%s",
		(text && text[0]) ? text : "OpenCE Dedicated");
	game_name_done(dedi_screen.server_name);
}

static boolean dedi_server_init(struct widget_instance *list)
{
	char const *sk = config_string("bots.skill");
	short i;

	if (text_field_editing(NULL))
		text_field_end(FALSE);
	if (multiplayer.game_name[0])
		wide_to_text(multiplayer.game_name, dedi_screen.server_name, sizeof(dedi_screen.server_name));
	if (!dedi_screen.server_name[0])
		snprintf(dedi_screen.server_name, sizeof(dedi_screen.server_name), "OpenCE Dedicated");
	dedi_screen.public_lobby = config_boolean("network.host_public") != 0;
	dedi_screen.map_share = config_boolean("network.map_share") != 0;
	dedi_screen.bots_count = (short)PIN(config_integer("bots.count"), 0, 15);
	dedi_screen.bots_skill_idx = 1;
	for (i = 0; i < 4; i++)
	{
		if (sk && !_stricmp(sk, dedi_skills[i]))
			dedi_screen.bots_skill_idx = i;
	}
	dedi_screen.chosen = 0;
	dedi_screen.status_msg[0] = 0;
	if (list && !strcmp(list->name, "dedi_select_list"))
		focus_row(list, 0);
	return TRUE;
}

static boolean dedi_server_save(struct widget_instance *widget)
{
	char num_buf[16];

	(void)widget;
	if (text_field_editing(NULL))
		text_field_end(TRUE);
	game_name_done(dedi_screen.server_name);
	multiplayer.maximum_players_index = (short)PIN(dedi_screen.max_players_idx, 0, NUMBEROF(maximum_players) - 1);
	config_write("network.host_public", dedi_screen.public_lobby ? "true" : "false");
	config_write("network.map_share", dedi_screen.map_share ? "true" : "false");
	snprintf(num_buf, sizeof(num_buf), "%d", (int)dedi_screen.bots_count);
	config_write("bots.count", num_buf);
	config_write("bots.skill", dedi_skills[PIN(dedi_screen.bots_skill_idx, 0, 3)]);
	snprintf(dedi_screen.status_msg, sizeof(dedi_screen.status_msg),
		"Saved Dedicated Server settings to config.toml!");
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

static boolean dedi_server_open_web(struct widget_instance *widget)
{
	char cmd[256];

	(void)widget;
	snprintf(cmd, sizeof(cmd),
		"opence_server --name \"%s\" --max-players %d %s",
		dedi_screen.server_name,
		(int)maximum_players[PIN(dedi_screen.max_players_idx, 0, NUMBEROF(maximum_players) - 1)],
		dedi_screen.preset_idx == 0 ? "--gametype firefight" : "");
	platform_clipboard_set(cmd);
	snprintf(dedi_screen.status_msg, sizeof(dedi_screen.status_msg),
		"Copied CLI launch command to clipboard:\n%s\nWeb Admin Panel: http://127.0.0.1:8080",
		cmd);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

static boolean dedi_server_step(struct widget_instance *widget, short delta)
{
	short place = dedi_screen.chosen;

	(void)widget;
	if (place == 1)
	{
		dedi_screen.preset_idx = (short)((dedi_screen.preset_idx + delta + DEDI_PRESET_COUNT) % DEDI_PRESET_COUNT);
		ui_play_audio_feedback_sound(SOUND_CURSOR);
	}
	else if (place == 2)
	{
		short cnt = NUMBEROF(maximum_players);
		dedi_screen.max_players_idx = (short)((dedi_screen.max_players_idx + delta + cnt) % cnt);
		ui_play_audio_feedback_sound(SOUND_CURSOR);
	}
	else if (place == 3)
	{
		dedi_screen.public_lobby = !dedi_screen.public_lobby;
		ui_play_audio_feedback_sound(SOUND_CURSOR);
	}
	else if (place == 4)
	{
		dedi_screen.bots_count = (short)((dedi_screen.bots_count + delta + 16) % 16);
		ui_play_audio_feedback_sound(SOUND_CURSOR);
	}
	else if (place == 5)
	{
		dedi_screen.bots_skill_idx = (short)((dedi_screen.bots_skill_idx + delta + 4) % 4);
		ui_play_audio_feedback_sound(SOUND_CURSOR);
	}
	else if (place == 6)
	{
		dedi_screen.map_share = !dedi_screen.map_share;
		ui_play_audio_feedback_sound(SOUND_CURSOR);
	}
	return TRUE;
}

static boolean dedi_server_choose(struct widget_instance *widget, struct event_record *event, boolean *widget_deleted)
{
	struct widget_instance *row = dedi_row(widget);
	short place = row ? (short)(atoi(row->name + 8) - 1) : dedi_screen.chosen;

	if (place < 0 || place >= DEDI_ROWS)
		return TRUE;
	dedi_screen.chosen = place;
	if (place == 0)
	{
		if (text_field_editing(row))
			text_field_end(TRUE);
		else
			text_field_begin(row ? row : widget, dedi_screen.server_name,
				sizeof(dedi_screen.server_name) - 1, dedi_name_done);
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return TRUE;
	}
	if (place >= 1 && place <= 6)
		return dedi_server_step(widget, 1);
	if (place == 7)
	{
		short controller = event_controller(widget, event);
		dedi_server_save(widget);
		multiplayer.gamemode = (dedi_screen.preset_idx == 0) ? _gamemode_firefight : _gamemode_standard;
		multiplayer.firefight_mode = (dedi_screen.preset_idx == 0);
		multiplayer.mode = _multiplayer_mode_host_internet;
		if (!multiplayer_player(controller))
			return FALSE;
		p2p_set_hosting_allowed(TRUE);
		multiplayer.game_private = !dedi_screen.public_lobby;
		p2p_set_hosting_public(dedi_screen.public_lobby);
		if (!ui_widget_port_host(widget, event, widget_deleted))
			return campaign_fail();
		return ui_widget_port_open(widget,
			"pc\\main_menu\\multiplayer_type_select\\mp_map_select\\mp_map_select_screen",
			widget_deleted);
	}
	return TRUE;
}

static void dedi_server_update(struct widget_instance *list)
{
	struct widget_instance *row;
	struct widget_instance *focused = dedi_row(list->focused_child);
	struct widget_instance *details = list->parameters.list.extended_description;
	short focused_index = focused ? (short)(atoi(focused->name + 8) - 1) : NONE;

	if (focused_index != NONE && focused_index >= 0 && focused_index < DEDI_ROWS)
		dedi_screen.chosen = focused_index;

	for (row = list->child; row; row = row->next)
	{
		short place;
		char label[64], val[64];
		wchar_t wlabel[ROW_TEXT_LENGTH], wval[ROW_TEXT_LENGTH];

		if (strncmp(row->name, "op_dedi_", 8))
			continue;
		row->animation.current_frame_index = row == focused ? 1 : 0;
		place = (short)(atoi(row->name + 8) - 1);
		row->visible = place >= 0 && place < DEDI_ROWS;
		if (!row->visible)
			continue;
		label[0] = 0;
		val[0] = 0;
		switch (place)
		{
		case 0:
			snprintf(label, sizeof(label), "SERVER NAME");
			if (text_field_editing(row))
			{
				char const *typed = profile_text_field_poll();
				snprintf(val, sizeof(val), "%s_", typed ? typed : "");
			}
			else
			{
				snprintf(val, sizeof(val), "%s", dedi_screen.server_name);
			}
			break;
		case 1:
			snprintf(label, sizeof(label), "ROTATION PRESET");
			snprintf(val, sizeof(val), "%s", dedi_presets[PIN(dedi_screen.preset_idx, 0, DEDI_PRESET_COUNT - 1)]);
			break;
		case 2:
			snprintf(label, sizeof(label), "MAX PLAYERS");
			snprintf(val, sizeof(val), "%d",
				(int)maximum_players[PIN(dedi_screen.max_players_idx, 0, NUMBEROF(maximum_players) - 1)]);
			break;
		case 3:
			snprintf(label, sizeof(label), "SERVER LISTING");
			snprintf(val, sizeof(val), "%s", dedi_screen.public_lobby ? "PUBLIC" : "PRIVATE");
			break;
		case 4:
			snprintf(label, sizeof(label), "AI BOTS");
			snprintf(val, sizeof(val), "%d", (int)dedi_screen.bots_count);
			break;
		case 5:
			snprintf(label, sizeof(label), "BOT SKILL");
			snprintf(val, sizeof(val), "%s",
				dedi_screen.bots_skill_idx == 0 ? "EASY" :
				dedi_screen.bots_skill_idx == 1 ? "NORMAL" :
				dedi_screen.bots_skill_idx == 2 ? "HEROIC" : "LEGENDARY");
			break;
		case 6:
			snprintf(label, sizeof(label), "MAP SHARING");
			snprintf(val, sizeof(val), "%s", dedi_screen.map_share ? "ON" : "OFF");
			break;
		default:
			snprintf(label, sizeof(label), "START LISTEN SERVER");
			snprintf(val, sizeof(val), "HOST NOW");
			break;
		}
		mods_wide_text(label, wlabel, ROW_TEXT_LENGTH);
		mods_wide_text(val, wval, ROW_TEXT_LENGTH);
		text_set(named(row, "dedi_name", 0), wlabel);
		text_set(named(row, "dedi_state", 0), wval);
	}

	if (details)
	{
		long saved_name_font = mods_screen.fonts[_mod_details_name];
		short saved_name_width = mods_screen.widths[_mod_details_name];
		long saved_text_font = mods_screen.fonts[_mod_details_text];
		short saved_text_width = mods_screen.widths[_mod_details_text];
		char body[512], wrapped[512];
		wchar_t wtitle[64], wbody[512];

		mods_screen.fonts[_mod_details_name] = NONE;
		mods_screen.widths[_mod_details_name] = 0;
		mods_screen.fonts[_mod_details_text] = NONE;
		mods_screen.widths[_mod_details_text] = 0;
		wrapped[0] = 0;
		mods_wrap("DEDICATED SERVER", wrapped, sizeof(wrapped), _mod_details_name, 2, 0);
		mods_wide_text(wrapped, wtitle, NUMBEROF(wtitle));
		text_set_length(named(details, "dedi_details_name", 0), wtitle, NUMBEROF(wtitle));
		snprintf(body, sizeof(body),
			"Server: %s\nPreset: %s\nCapacity: %d Players (%d Bots)\nListing: %s | Map Share: %s\n\n%s",
			dedi_screen.server_name,
			dedi_presets[PIN(dedi_screen.preset_idx, 0, DEDI_PRESET_COUNT - 1)],
			(int)maximum_players[PIN(dedi_screen.max_players_idx, 0, NUMBEROF(maximum_players) - 1)],
			(int)dedi_screen.bots_count,
			dedi_screen.public_lobby ? "Public" : "Private",
			dedi_screen.map_share ? "On" : "Off",
			dedi_screen.status_msg[0] ? dedi_screen.status_msg :
			"Configure your Dedicated or Listen Server.\n\nSelect START LISTEN SERVER to host immediately in-game, or COPY CLI CMD to copy the headless opence_server launch command (with Web Admin Dashboard on port 8080).");
		wrapped[0] = 0;
		mods_wrap(body, wrapped, sizeof(wrapped), _mod_details_text, 13, 0);
		mods_wide_text(wrapped, wbody, NUMBEROF(wbody));
		text_set_length(named(details, "dedi_details_text", 0), wbody, NUMBEROF(wbody));
		mods_screen.fonts[_mod_details_name] = saved_name_font;
		mods_screen.widths[_mod_details_name] = saved_name_width;
		mods_screen.fonts[_mod_details_text] = saved_text_font;
		mods_screen.widths[_mod_details_text] = saved_text_width;
	}
}

/* ---- the menu */

/* "mp type set mode": the item's (by its name) */
static void multiplayer_mode_set(struct widget_instance *widget)
{
	char const *name = widget->name;

	/* (an instance's name is cut to 31 characters:
	"multiplayer_type_create_interne") */
	multiplayer.mode =
		strstr(name, "create_inter") ? _multiplayer_mode_host_internet :
		strstr(name, "create_lan") ? _multiplayer_mode_host_lan :
		strstr(name, "create_local") ? _multiplayer_mode_host_local :
		strstr(name, "join_lan") ? _multiplayer_mode_lan :
		strstr(name, "join_direct") ? _multiplayer_mode_direct_link : _multiplayer_mode_server_browser;
}

/* player 1's profile for the controller's player in a network game */
static boolean multiplayer_player(short controller)
{
	struct player_profile profile;

	if (!campaign_profile(controller, &profile))
		return campaign_fail();
	return ui_widget_port_multiplayer_player(controller, player_ui_get_active_player_profile_index(0));
}

/* Create Game's ("join controller to mp game", first): player 1 in, and
the server made: an internet one (an invite, Discord), a LAN one, or a
LOCAL one, which no other machine joins (the bots': bots.c) */
static boolean multiplayer_host(struct widget_instance *widget, struct event_record *event, short controller,
	boolean *widget_deleted)
{
	multiplayer_mode_set(widget);
	if (!multiplayer_player(controller))
		return FALSE;
	p2p_set_hosting_allowed(multiplayer.mode == _multiplayer_mode_host_internet);
	/* (in the server browser, if PUBLIC: Server Setup's LISTING, which a
	new game starts with as network.host_public says) */
	multiplayer.game_private = !config_boolean("network.host_public");
	p2p_set_hosting_public(multiplayer.mode == _multiplayer_mode_host_internet && !multiplayer.game_private);
	multiplayer.cooperative_maximum_players_set = FALSE;
	if (multiplayer.mode == _multiplayer_mode_host_local)
		return ui_widget_port_host_local(widget, event, widget_deleted);
	return ui_widget_port_host(widget, event, widget_deleted);
}

/* ---- the map list (the Map screen's): its chooser's SINGLEPLAYER levels,
MULTIPLAYER maps, CUSTOM SINGLEPLAYER or CUSTOM MULTIPLAYER maps (the map
lists', above). A level or Custom Edition campaign map chosen lists the
difficulties (B goes back to the map) and is hosted as a network co-op game,
which goes to Server Setup; a multiplayer map goes on to the gametypes.
Split screen hosts only multiplayer maps, its chooser kept to the
multiplayer kinds. */

enum
{
	MAP_STEP_MAPS,
	MAP_STEP_DIFFICULTIES,
};

static struct
{
	short kind, step;
	/* hosting over the network, which co-op needs */
	boolean hosting;
	/* the multiplayer maps (ui_widget_port_multiplayer_maps): the Xbox's,
	then the Custom Edition ones */
	short map_count;
	short first, chosen;
	/* the level or Custom Edition campaign map (the kind's entry) whose
	difficulties are listed */
	short level;
} map_list;

#define SERVER_SETUP_NAME "pc\\main_menu\\multiplayer_type_select\\server_settings\\server_settings_screen"

static short map_step_count(void)
{
	if (map_list.step == MAP_STEP_DIFFICULTIES)
		return NUMBER_OF_GAME_DIFFICULTY_LEVELS + 1;
	return map_kind_count(map_list.kind, map_list.map_count);
}

/* opens a step at the entry given */
static void map_step_open(struct widget_instance *list, short step, short entry)
{
	short count;

	map_list.step = step;
	count = map_step_count();
	map_list.chosen = (short)PIN(entry, 0, MAX(count - 1, 0));
	map_list.first = map_kind_first(map_list.chosen, count);
	map_kind_focus(list, map_list.first, map_list.chosen);
}

static void map_level_text(short level, wchar_t *text)
{
	string_get("pc\\main_menu\\map_list", level, text);
}

static void map_difficulty_text(short difficulty, wchar_t *text)
{
	if (difficulty == NUMBER_OF_GAME_DIFFICULTY_LEVELS)
	{
		ustrncpy(text, L"FIREFIGHT (GAMETYPES)", ROW_TEXT_LENGTH - 1);
		text[ROW_TEXT_LENGTH - 1] = 0;
		return;
	}
	string_get("pc\\main_menu\\player_profiles_select\\difficulty_names", difficulty, text);
}

/* "mp level list initialize": on the multiplayer map used last */
static boolean map_list_initialize(struct widget_instance *list)
{
	char const *const *names;
	short last_used = 0;

	map_list.hosting = global_network_game_server_get() != NULL && !network_game_is_splitscreen_local();
	/* (the maps folders looked for again as the list opens) */
	custom_edition_maps_look_again();
	map_list.map_count = ui_widget_port_multiplayer_maps(&names, &last_used);
	/* (a Custom Edition map's kind, when it was one) */
	map_list.kind = MAP_KIND_MULTIPLAYER;
	if (last_used >= xbox_multiplayer_map_count(map_list.map_count))
	{
		map_list.kind = MAP_KIND_CUSTOM_MULTIPLAYER;
		last_used -= xbox_multiplayer_map_count(map_list.map_count);
	}
	map_kind_set(list, map_list.kind);
	map_step_open(list, MAP_STEP_MAPS, last_used);
	return TRUE;
}

/* "mp map list update": the chooser's kind, the step's rows, and beside
them the level or map chosen: its picture, name and words */
static void map_list_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	short kind = map_kind_shown(list, TRUE, map_list.kind), count, entry;
	short shown;

	if (kind != map_list.kind)
	{
		map_list.kind = kind;
		map_list.step = MAP_STEP_MAPS;
		map_list.first = map_list.chosen = 0;
	}
	count = map_step_count();
	entry = map_kind_rows_update(list, &map_list.first, count, map_list.step == MAP_STEP_DIFFICULTIES ?
		map_difficulty_text : kind == MAP_KIND_SINGLEPLAYER ? map_level_text :
		kind == MAP_KIND_MULTIPLAYER ? multiplayer_map_text :
		kind == MAP_KIND_CUSTOM_SINGLEPLAYER ? custom_campaign_map_text : custom_multiplayer_map_text);
	if (entry != NONE)
		map_list.chosen = entry;
	visible_set(named(description, "mp_map_right_item", 0), map_list.chosen < count);
	/* (the map whose difficulties are listed, else the one chosen) */
	shown = map_list.step == MAP_STEP_DIFFICULTIES ? map_list.level : map_list.chosen;
	if (kind == MAP_KIND_SINGLEPLAYER)
	{
		map_description_show(description, shown, NONE);
		level_description(description, "replay_level", shown, FALSE, NULL, NONE);
	}
	else
	{
		map_description_show(description, NONE,
			map_list.step == MAP_STEP_DIFFICULTIES || map_list.chosen < count ? map_kind_display_index(kind, shown) :
			NONE);
	}
	profile_name_show(description);
}

/* "mp level select" (the list's OK): the map chosen, or a level's
difficulties, or the level at the difficulty chosen. FALSE stays on the Map
screen (the gametypes open on TRUE). */
static boolean map_list_choose(struct widget_instance *list, boolean *widget_deleted)
{
	short chosen;

	map_list_update(list);
	chosen = map_list.chosen;

	if (chosen >= map_step_count())
		return campaign_fail();
	if (map_list.step == MAP_STEP_DIFFICULTIES)
	{
		char const *map_name = map_list.kind == MAP_KIND_SINGLEPLAYER ? main_get_solo_level_name(map_list.level) :
			custom_map_level_name(map_list.kind, map_list.level);

		if (chosen == NUMBER_OF_GAME_DIFFICULTY_LEVELS)
		{
			if (!ui_widget_port_campaign_map_for_gametype_choose(map_name))
				return campaign_fail();
			return TRUE;
		}
		/* the co-op game set up, then Server Setup in the gametypes' place */
		if (!ui_widget_port_cooperative_level_choose(map_name, chosen))
			return campaign_fail();
		return ui_widget_port_open(list, SERVER_SETUP_NAME, widget_deleted);
	}
	/* (a multiplayer map: the Xbox's, then the Custom Edition ones, in the
	multiplayer maps) */
	if (map_list.kind == MAP_KIND_MULTIPLAYER)
		return ui_widget_port_multiplayer_map_choose(chosen);
	if (map_list.kind == MAP_KIND_CUSTOM_MULTIPLAYER)
		return ui_widget_port_multiplayer_map_choose((short)(xbox_multiplayer_map_count(map_list.map_count) + chosen));
	if (!map_list.hosting || multiplayer.firefight_mode)
	{
		char const *map_name = map_list.kind == MAP_KIND_SINGLEPLAYER ? main_get_solo_level_name(chosen) :
			custom_map_level_name(map_list.kind, chosen);

		if (!ui_widget_port_campaign_map_for_gametype_choose(map_name))
			return campaign_fail();
		return TRUE;
	}
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	map_list.level = chosen;
	map_step_open(list, MAP_STEP_DIFFICULTIES, (short)PIN(main_get_difficulty(), 0, NUMBER_OF_GAME_DIFFICULTY_LEVELS - 1));
	return FALSE;
}

/* "port map list back" (B on the list): from the difficulties back to the
level, else out of the Map screen */
static boolean map_list_back(struct widget_instance *list, boolean *widget_deleted)
{
	ui_play_audio_feedback_sound(SOUND_BACK);
	if (map_list.step == MAP_STEP_DIFFICULTIES)
	{
		map_step_open(list, MAP_STEP_MAPS, map_list.level);
		return TRUE;
	}
	ui_widget_port_go_back(list);
	*widget_deleted = TRUE;
	return TRUE;
}

unsigned char menu_port_back(void *screen)
{
	struct widget_instance *widget = screen;
	struct widget_instance *list = named(widget, "mp_map_select_list_2", 0);
	struct widget_instance *prof_list = named(widget, "profile_manager_select_list_2", 0);

	if (text_field_editing(NULL))
	{
		text_field_end(FALSE);
		ui_play_audio_feedback_sound(SOUND_BACK);
		return 1;
	}
	if (list && map_list.step == MAP_STEP_DIFFICULTIES)
	{
		ui_play_audio_feedback_sound(SOUND_BACK);
		map_step_open(list, MAP_STEP_MAPS, map_list.level);
		return 1;
	}
	if (prof_list && profile_list.step != PROFILE_STEP_SELECT)
	{
		if (text_field_editing(NULL))
		{
			text_field_end(FALSE);
			ui_play_audio_feedback_sound(SOUND_BACK);
			return 1;
		}
		if (profile_list.step == PROFILE_STEP_EMBLEM)
		{
			if (profile_list.chosen >= 0 && profile_list.chosen < profile_list.count)
				firefight_stats_set_emblem(profile_list.indices[profile_list.chosen], &profile_list.emblem);
			profile_list.step = PROFILE_STEP_EDIT_MENU;
			profile_list.edit_row = 1;
			focus_row(prof_list, 1);
			ui_play_audio_feedback_sound(SOUND_BACK);
			return 1;
		}
		if (profile_list.step == PROFILE_STEP_COLOR)
		{
			profile_list.step = PROFILE_STEP_EDIT_MENU;
			profile_list.edit_row = 2;
			focus_row(prof_list, 2);
			ui_play_audio_feedback_sound(SOUND_BACK);
			return 1;
		}
		if (profile_list.step == PROFILE_STEP_EDIT_MENU)
		{
			profile_list.step = PROFILE_STEP_SELECT;
			focus_row(prof_list, (short)(profile_list.chosen - profile_list.first));
			ui_play_audio_feedback_sound(SOUND_BACK);
			return 1;
		}
	}
	return 0;
}

void pc_menus_shell_loaded(void)
{
}

static struct
{
	char map_name[256];
	short controller;
	short difficulty;
} campaign_pending;

static struct skull_set skulls_menu_set;
static long skulls_menu_back_tag = NONE;
static struct
{
	boolean asked;
	boolean open;
} skulls_setup;
static boolean campaign_screens_for_server;
static boolean campaign_bots_screen_active = FALSE;

static boolean skulls_open(struct widget_instance *widget, boolean *widget_deleted);
static boolean campaign_bots_open(struct widget_instance *widget, boolean *widget_deleted);
static short campaign_level_of(char const *map_name);

extern boolean ui_widget_port_error_showing(void);
extern struct widget_instance *ui_widget_port_active_screen(void);
extern char *tag_get_name(long tag_index);

static boolean setting_show_saved(struct widget_instance *spinner, struct pc_menu_setting *setting)
{
	(void)setting;
	setting_load(spinner);
	return TRUE;
}

static boolean randomizer_spinner_is(struct widget_instance *list, char const *spinner, char const *value)
{
	struct widget_instance *widget = named(list, spinner, 0);
	struct pc_menu_setting *setting = widget ? pc_menu_setting_get(widget->definition_tag_index) : NULL;
	short index = widget ? widget->parameters.list.selected_index : NONE;

	return setting && index >= 0 && index < setting->value_count && !strcmp(setting->values[index], value);
}

static boolean skulls_open_over(long back_tag, boolean *widget_deleted)
{
	char const *screen = "pc\\main_menu\\difficulty_select\\skulls\\skulls_screen";

	skulls_menu_back_tag = back_tag;
	skulls_setup.open = FALSE;
	skulls_menu_set = skull_set_intersection(skulls_set_from_text(config_string("game.skulls_active")),
		skulls_unlocked_set());
	if (!ui_widget_load_by_name_or_tag(screen, NONE, NULL, NONE, back_tag, NONE, NONE))
		return FALSE;
	*widget_deleted = TRUE;
	return TRUE;
}

static boolean skulls_open(struct widget_instance *widget, boolean *widget_deleted)
{
	return skulls_open_over(screen_of(widget)->definition_tag_index, widget_deleted);
}

static boolean campaign_bots_open(struct widget_instance *widget, boolean *widget_deleted)
{
	char const *screen = "pc\\main_menu\\multiplayer_type_select\\server_settings\\bots_options\\bots_options_screen";
	long back_tag = screen_of(widget)->definition_tag_index;

	campaign_bots_screen_active = TRUE;
	if (!ui_widget_load_by_name_or_tag(screen, NONE, NULL, NONE, back_tag, NONE, NONE))
	{
		campaign_bots_screen_active = FALSE;
		return FALSE;
	}
	*widget_deleted = TRUE;
	return TRUE;
}

static boolean skulls_unlock_choose(struct widget_instance *widget, boolean play, boolean instant,
	boolean lose_on_death, boolean *widget_deleted)
{
	if (!config_write("game.skulls_unlock_mode", "play") ||
		!config_write("game.skulls_all_unlocked", play ? "false" : "true") ||
		!config_write("game.skulls_instant_unlock", instant ? "true" : "false") ||
		!config_write("game.skulls_lose_on_death", lose_on_death ? "true" : "false"))
	{
		platform_log("skulls: could not write game.skulls_unlock_mode");
	}
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	if (skulls_setup.open)
	{
		skulls_setup.open = FALSE;
		ui_widget_port_go_back(widget);
		*widget_deleted = TRUE;
		return TRUE;
	}
	return skulls_open_over(skulls_menu_back_tag, widget_deleted);
}

static boolean skulls_question_open(char const *screen, boolean forward, boolean *widget_deleted)
{
	(void)screen;
	(void)forward;
	return skulls_open_over(skulls_menu_back_tag, widget_deleted);
}

static int skull_of(struct widget_instance *widget)
{
	int skull;

	if (!widget || strncmp(widget->name, "skull_", 6))
		return NONE;
	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
	{
		if (!strcmp(widget->name + 6, skull_descriptions[skull].key))
			return skull;
	}
	return NONE;
}

static boolean skull_disables_achievements(int skull)
{
	return skull == _skull_bandanna || skull == _skull_scarab || skull == _skull_envy;
}

static boolean skull_toggle(struct widget_instance *widget)
{
	int skull = skull_of(widget);
	struct skull_set unlocked = skulls_unlocked_set();

	if (skull == NONE)
		return FALSE;
	if (!skull_set_has(&unlocked, skull))
	{
		ui_play_audio_feedback_sound(SOUND_ERROR);
		return TRUE;
	}
	if (skull_set_has(&skulls_menu_set, skull))
		skulls_menu_set = skull_set_difference(skulls_menu_set, skull_set_of(skull));
	else
	{
		skull_set_add(&skulls_menu_set, skull);
		if (skull_disables_achievements(skull))
		{
			display_error_text_deferred(
				L"\rActivating this skull will\rdisable achievements.",
				0);
		}
	}
	ui_play_audio_feedback_sound(skull_set_has(&skulls_menu_set, skull) ? SOUND_FORWARD : SOUND_BACK);
	return TRUE;
}

#define SKULL_COLUMNS 4
#define SKULL_BUTTONS 3

void ui_widget_port_give_focus(struct widget_instance *widget);

static boolean skulls_move(struct widget_instance *widget, short across, short down)
{
	struct widget_instance *list = widget ? widget->parent : NULL;
	struct widget_instance *child;
	short rows = (NUMBER_OF_SKULLS + SKULL_COLUMNS - 1) / SKULL_COLUMNS;
	short index = 0;
	short row, column, target;

	if (!list)
		return FALSE;
	for (child = list->child; child && child != widget; child = child->next)
		index++;
	if (!child)
		return FALSE;
	if (index < NUMBER_OF_SKULLS)
		row = index / SKULL_COLUMNS, column = index % SKULL_COLUMNS;
	else
		row = rows, column = index - NUMBER_OF_SKULLS;
	if (across)
	{
		column += across;
		if (column < 0 || column >= (row == rows ? SKULL_BUTTONS : SKULL_COLUMNS))
			return TRUE;
	}
	else
	{
		boolean buttons_row = row == rows;

		row = (short)((row + down + rows + 1) % (rows + 1));
		if (buttons_row != (row == rows))
			column = (short)MIN(column, (row == rows ? SKULL_BUTTONS : SKULL_COLUMNS) - 1);
	}
	if (row == rows)
		target = (short)(NUMBER_OF_SKULLS + column);
	else
	{
		target = (short)(row * SKULL_COLUMNS + column);
		if (target >= NUMBER_OF_SKULLS)
			target = (short)(row * SKULL_COLUMNS);
	}
	for (child = list->child; child && target > 0; child = child->next)
		target--;
	if (child && child != widget)
		ui_widget_port_give_focus(child);
	return TRUE;
}

static boolean skulls_start(struct widget_instance *widget, boolean *widget_deleted)
{
	char text[SKULLS_TEXT_LENGTH];

	skulls_text_from_set(skull_set_intersection(skulls_menu_set, skulls_unlocked_set()), text, sizeof(text));
	if (!config_write("game.skulls_active", text))
		platform_log("skulls: could not write game.skulls_active");
	if (campaign_screens_for_server)
	{
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		ui_widget_port_go_back(widget);
		*widget_deleted = TRUE;
		return TRUE;
	}
	if (campaign_bots_open(widget, widget_deleted))
	{
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return TRUE;
	}
	campaign_start(campaign_pending.map_name, campaign_pending.difficulty, campaign_pending.controller);
	return TRUE;
}

static void text_widen(wchar_t *text, unsigned int length, char const *ascii)
{
	unsigned int index;

	for (index = 0; ascii[index] && index + 1 < length; index++)
		text[index] = (wchar_t)(unsigned char)ascii[index];
	text[index] = 0;
}

static void skulls_screen_update(struct widget_instance *list)
{
	struct widget_instance *focused = list->focused_child;
	int focused_skull = skull_of(focused);
	struct widget_instance *help = list->parameters.list.extended_description;
	struct skull_set unlocked = skulls_unlocked_set();
	char line[256];
	wchar_t text[ROW_TEXT_LENGTH];
	int skull;

	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
	{
		struct widget_instance *item;
		boolean locked = !skull_set_has(&unlocked, skull);

		snprintf(line, sizeof(line), "skull_%s", skull_descriptions[skull].key);
		item = named(list, line, 0);
		if (!item)
			continue;
		snprintf(line, sizeof(line), "%s %s", locked ? "[-]" : skull_set_has(&skulls_menu_set, skull) ? "[X]" : "[ ]",
			skull_descriptions[skull].label);
		text_widen(text, NUMBEROF(text), line);
		text_set(item, text);
		item->alpha_modifier = locked ? 0.4f : 1.0f;
	}
	if (!help)
		return;
	if (focused_skull != NONE && !skull_set_has(&unlocked, focused_skull))
		snprintf(line, sizeof(line), "Locked. Complete Campaign missions, finish Firefight sets,\nor kill enemies to unlock skulls.");
	else if (focused_skull != NONE && skull_disables_achievements(focused_skull))
		snprintf(line, sizeof(line), "%s Activating this skull will disable achievements.",
			skull_descriptions[focused_skull].description);
	else if (focused_skull != NONE)
		snprintf(line, sizeof(line), "%s", skull_descriptions[focused_skull].description);
	else
		line[0] = 0;
	{
		wchar_t wide_help[256];
		text_widen(wide_help, NUMBEROF(wide_help), line);
		text_set_length(help, wide_help, NUMBEROF(wide_help));
	}
}

static enum
{
	_skulls_reset_none,
	_skulls_reset_asked,
	_skulls_reset_done
} skulls_reset_state;

static void skulls_reset_progress(struct widget_instance *widget)
{
	if (skulls_reset_state != _skulls_reset_asked)
	{
		skulls_reset_state = _skulls_reset_asked;
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return;
	}
	if (!config_write("game.skulls_unlocked", "") || !config_write("game.skulls_active", "") ||
		!config_write("game.skulls_all_unlocked", "false") || !config_write("game.skulls_unlock_mode", "play") ||
		!config_write("game.skulls_lose_on_death", "true"))
	{
		platform_log("skulls: could not reset the skulls' progress");
		skulls_reset_state = _skulls_reset_none;
		ui_play_audio_feedback_sound(SOUND_ERROR);
		return;
	}
	platform_log("menus: skull progress reset");
	skulls_reset_state = _skulls_reset_done;
	skulls_setup.asked = TRUE;
	settings_each(screen_of(widget), setting_show_saved);
	ui_play_audio_feedback_sound(SOUND_BACK);
}

static void gameplay_screen_update(struct widget_instance *list)
{
	struct widget_instance *all_row = named(list, "op_skulls_all_unlocked", 0);
	struct widget_instance *instant_row = named(list, "op_skulls_instant_unlock", 0);
	struct widget_instance *death_row = named(list, "op_skulls_lose_on_death", 0);
	struct widget_instance *count_row = named(list, "op_skulls_drop_count", 0);
	struct widget_instance *chance_row = named(list, "op_skulls_drop_chance", 0);
	struct widget_instance *reset_row = named(list, "op_reset_progress", 0);
	struct widget_instance *help = list->parameters.list.extended_description;
	boolean answered = randomizer_spinner_is(list, "skulls_spinner", "true") &&
		skulls_unlock_mode() != _skulls_unlock_unset;

	if (all_row)
		all_row->visible = answered;
	if (instant_row)
		instant_row->visible = answered && !randomizer_spinner_is(list, "skulls_all_unlocked_spinner", "true");
	if (death_row)
	{
		death_row->visible = answered && !randomizer_spinner_is(list, "skulls_all_unlocked_spinner", "true") &&
			!randomizer_spinner_is(list, "skulls_instant_unlock_spinner", "true");
	}
	if (count_row)
		count_row->visible = answered && !randomizer_spinner_is(list, "skulls_all_unlocked_spinner", "true");
	if (chance_row)
		chance_row->visible = answered && !randomizer_spinner_is(list, "skulls_all_unlocked_spinner", "true");
	if (!reset_row || list->focused_child != reset_row)
	{
		skulls_reset_state = _skulls_reset_none;
		return;
	}
	if (help && skulls_reset_state != _skulls_reset_none && reset_row->child)
	{
		short label = pc_menu_string_index(reset_row->child->definition_tag_index);

		if (label != NONE)
			help->parameters.text_box.string_list_index = (short)(label + 1 + skulls_reset_state);
	}
}

static boolean difficulty_start_with_screens(struct widget_instance *widget, short difficulty, short controller,
	boolean *widget_deleted)
{
	struct player_profile profile;
	char const *map_name = main_get_map_name();

	if (!campaign_profile(controller, &profile))
		return campaign_fail();
	if (!custom_edition_maps_level_campaign(map_name))
		map_name = main_get_solo_level_name(0);
	difficulty = PIN(difficulty, 0, 3);
	snprintf(campaign_pending.map_name, sizeof(campaign_pending.map_name), "%s", map_name);
	campaign_pending.controller = controller;
	campaign_pending.difficulty = difficulty;
	campaign_screens_for_server = FALSE;
	main_set_difficulty(difficulty);
	if (config_boolean("game.skulls"))
	{
		if (skulls_open(widget, widget_deleted))
			return TRUE;
	}
	if (campaign_bots_open(widget, widget_deleted))
		return TRUE;
	campaign_start(map_name, difficulty, controller);
	return TRUE;
}

static boolean server_campaign_screen(struct widget_instance *widget, boolean *widget_deleted)
{
	campaign_screens_for_server = TRUE;
	return skulls_open(widget, widget_deleted);
}

#define MAIN_MENU_NAME "ui\\shell\\main_menu\\main_menu"
#define SKULLS_UNLOCK_SCREEN_NAME "pc\\main_menu\\difficulty_select\\skulls\\unlock_screen"
#define SETTINGS_SCREEN_PREFIX "pc\\main_menu\\settings_select\\"

void skulls_setup_poll(boolean main_menu)
{
	(void)main_menu;
}

/* (the gametype editor's: Server Setup's copy of the game's gametype) */
static void gametype_setup_begin(void);
static void gametype_setup_end(void);
static boolean gametype_setup_apply(void);
static void gametype_setup_type(wchar_t *text);

/* ---- the gametype list: its spinner's bank (STANDARD: the Xbox's built-in
ones, OPENCE: OpenCE's built-in ones, game_modes.c, CUSTOM: those saved,
FIREFIGHT: built-in and custom Firefight variants) in its first row, then
the bank's gametypes */

enum
{
	GAMETYPE_BANK_STANDARD = 0,
	GAMETYPE_BANK_OPENCE,
	GAMETYPE_BANK_CUSTOM,
	NUMBER_OF_GAMETYPE_BANKS,
	GAMETYPE_BANK_FIREFIGHT = NUMBER_OF_GAMETYPE_BANKS,
	GAMETYPE_BANK_RANKED,
	GAMETYPE_BANK_FORGE,
};

/* a gametype's bank */
static short gametype_bank(long profile_index)
{
	struct game_variant variant;
	struct game_variant_options options;

	if (playlist_profile_get(profile_index, &variant))
	{
		if (variant.game_engine_index == game_engine_firefight)
			return GAMETYPE_BANK_FIREFIGHT;
	}
	if (!((unsigned long)profile_index & PLAYLIST_READ_ONLY_BIT))
		return GAMETYPE_BANK_CUSTOM;
	if (playlist_profile_get(profile_index, &variant) && playlist_profile_get_options(profile_index, &options))
	{
		if (game_modes_variant_is_ranked(&variant, &options))
			return GAMETYPE_BANK_RANKED;
		if (game_modes_variant_is_opence(&variant, &options))
			return GAMETYPE_BANK_OPENCE;
	}
	return GAMETYPE_BANK_STANDARD;
}

static boolean gametype_profile_matches_gamemode(long profile_index)
{
	short bank = gametype_bank(profile_index);

	if (multiplayer.gamemode == _gamemode_firefight || multiplayer.firefight_mode)
		return bank == GAMETYPE_BANK_FIREFIGHT;
	if (multiplayer.gamemode == _gamemode_ranked)
		return bank == GAMETYPE_BANK_RANKED;
	if (multiplayer.gamemode == _gamemode_forge)
		return bank == GAMETYPE_BANK_FORGE;
	return bank == GAMETYPE_BANK_STANDARD || bank == GAMETYPE_BANK_OPENCE || bank == GAMETYPE_BANK_CUSTOM || bank == GAMETYPE_BANK_FIREFIGHT;
}

static void gametype_bank_read(short bank)
{
	short index;

	multiplayer.bank_shown = bank;
	multiplayer.bank_count = 0;
	for (index = 0; index < multiplayer.gametype_count; index++)
	{
		if (gametype_bank(multiplayer.gametypes[index]) == bank)
			multiplayer.bank[multiplayer.bank_count++] = index;
	}
	multiplayer.gametype_first = 0;
	multiplayer.gametype_chosen = 0;
}

/* a gametype's kind and rules, for the description beside a list: its game
engine's name (or its OpenCE gametype's), teams, and what wins */
static void gametype_rules_text(long profile_index, wchar_t *text, short size)
{
	struct game_variant variant;
	struct game_variant_options options;
	wchar_t const *kind;
	long engine;

	text[0] = 0;
	if (!playlist_profile_get(profile_index, &variant))
		return;
	if (!playlist_profile_get_options(profile_index, &options))
		game_variant_options_default(&variant, &options);
	engine = PIN(variant.game_engine_index, 0, 6);
	if (engine == game_engine_firefight)
	{
		short sets = variant.game_engine_variant.firefight.set_limit;
		short lives = variant.game_engine_variant.firefight.starting_lives;
		wchar_t set_buf[16];
		wchar_t life_buf[16];

		if (sets <= 0)
			usnprintf(set_buf, NUMBEROF(set_buf) - 1, L"Unlimited");
		else
			usnprintf(set_buf, NUMBEROF(set_buf) - 1, L"%d", (int)sets);
		set_buf[NUMBEROF(set_buf) - 1] = 0;

		if (lives < 0)
			usnprintf(life_buf, NUMBEROF(life_buf) - 1, L"Unlimited");
		else
			usnprintf(life_buf, NUMBEROF(life_buf) - 1, L"%d", (int)lives);
		life_buf[NUMBEROF(life_buf) - 1] = 0;

		usnprintf(text, size - 1, L"%s\r\nSets: %s\r\nLives: %s | Gen: %d",
			engine_names[engine], set_buf, life_buf,
			(int)variant.game_engine_variant.firefight.generator_count);
		text[size - 1] = 0;
		return;
	}
	kind = game_modes_variant_kind_name(&variant, &options);
	if (game_modes_variant_is_ranked(&variant, &options))
	{
		wchar_t const *playlist_name = game_modes_ranked_playlist_name(options.ranked_playlist);
		wchar_t const *playlist_format = game_modes_ranked_playlist_format(options.ranked_playlist);
		wchar_t const *score_unit = (engine == game_engine_ctf) ? L"captures" :
			((engine == game_engine_oddball || engine == game_engine_king) ? L"min" : L"kills");

		usnprintf(text, size - 1, L"%s - %s\r\n%s\r\nScore to win: %ld %s",
			playlist_name ? playlist_name : L"Ranked",
			kind ? kind : engine_names[engine],
			playlist_format ? playlist_format : (variant.universal_variant.teams ? L"Teams" : L"Free for all"),
			variant.universal_variant.score_to_win,
			score_unit);
		text[size - 1] = 0;
		return;
	}
	switch (game_modes_variant_mode(&variant, &options))
	{
	case _game_mode_infection:
		if (options.time_limit > 0)
			usnprintf(text, size - 1, L"%s\r\nTeams\r\nRound time: %d min", kind, options.time_limit);
		else
			usnprintf(text, size - 1, L"%s\r\nTeams\r\nUntil no human is left", kind);
		break;
	case _game_mode_gun_game:
		usnprintf(text, size - 1, L"%s\r\nFree for all\r\nA kill with each weapon", kind);
		break;
	default:
		usnprintf(text, size - 1, L"%s\r\n%s\r\nScore to win: %ld", kind ? kind : engine_names[engine],
			variant.universal_variant.teams ? L"Teams" : L"Free for all", variant.universal_variant.score_to_win);
		break;
	}
	text[size - 1] = 0;
}

/* "mp profiles list initialize": on the gametype used last */
static boolean gametype_list_initialize(struct widget_instance *list)
{
	struct widget_instance *chooser = named(list, "list_item_0_chooser", 0);
	struct widget_instance *spinner = named(list, "list_item_0_chooser_spinner", 0);
	boolean exclusive_mode = multiplayer.gamemode != _gamemode_standard || multiplayer.firefight_mode ||
		(!map_list.hosting && map_kind_singleplayer(map_list.kind));
	short last, index, initial_bank;

	multiplayer.gametype_count = ui_widget_port_gametypes(multiplayer.gametypes, MAXIMUM_GAMETYPES, &last);
	initial_bank = multiplayer.gametype_count ? gametype_bank(multiplayer.gametypes[last]) : GAMETYPE_BANK_STANDARD;
	if (multiplayer.gamemode == _gamemode_firefight || multiplayer.firefight_mode ||
		(!map_list.hosting && map_kind_singleplayer(map_list.kind)))
	{
		initial_bank = GAMETYPE_BANK_FIREFIGHT;
	}
	else if (multiplayer.gamemode == _gamemode_ranked)
	{
		initial_bank = GAMETYPE_BANK_RANKED;
	}
	else if (multiplayer.gamemode == _gamemode_forge)
	{
		initial_bank = GAMETYPE_BANK_FORGE;
	}
	else if (initial_bank >= NUMBER_OF_GAMETYPE_BANKS)
	{
		initial_bank = GAMETYPE_BANK_STANDARD;
	}
	if (chooser)
	{
		chooser->visible = !exclusive_mode;
		chooser->disabled = exclusive_mode;
	}
	gametype_bank_read(initial_bank);
	for (index = 0; index < multiplayer.bank_count; index++)
	{
		if (multiplayer.bank[index] == last)
			multiplayer.gametype_chosen = index;
	}
	multiplayer.gametype_first = (short)PIN(multiplayer.gametype_chosen - GAMETYPE_ROWS / 2, 0,
		MAX(0, multiplayer.bank_count - GAMETYPE_ROWS));
	if (spinner)
		spinner->parameters.list.selected_index = (short)PIN(multiplayer.bank_shown, 0, NUMBER_OF_GAMETYPE_BANKS - 1);
	focus_row(list, (short)(1 + multiplayer.gametype_chosen - multiplayer.gametype_first));
	return TRUE;
}

/* a gametype's name, in a row's text (the name's own buffer is longer:
playlist_profile_get_display_name fills MAX_GAMENAME characters) */
static void gametype_display_name(long profile_index, wchar_t *text)
{
	wchar_t name[MAX_GAMENAME];

	text[0] = 0;
	if (playlist_profile_get_display_name(profile_index, name))
	{
		ustrncpy(text, name, ROW_TEXT_LENGTH - 1);
		text[ROW_TEXT_LENGTH - 1] = 0;
	}
}

static void gametype_name(short item, wchar_t *text)
{
	text[0] = 0;
	if (item >= 0 && item < multiplayer.bank_count)
		gametype_display_name(multiplayer.gametypes[multiplayer.bank[item]], text);
}

/* "gt select list update": the bank shown, its rows (from the second), and
the gametype chosen: its name, picture and rules */
static void gametype_list_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	struct widget_instance *chooser = named(list, "list_item_0_chooser", 0);
	struct widget_instance *spinner = named(list, "list_item_0_chooser_spinner", 0);
	struct widget_instance *row;
	struct widget_instance *widget;
	boolean exclusive_mode = multiplayer.gamemode != _gamemode_standard || multiplayer.firefight_mode ||
		(!map_list.hosting && map_kind_singleplayer(map_list.kind));
	short row_index = 0, focused = NONE, count;

	if (chooser)
	{
		chooser->visible = !exclusive_mode;
		chooser->disabled = exclusive_mode;
	}
	if (multiplayer.gamemode == _gamemode_firefight || multiplayer.firefight_mode ||
		(!map_list.hosting && map_kind_singleplayer(map_list.kind)))
	{
		if (multiplayer.bank_shown != GAMETYPE_BANK_FIREFIGHT)
			gametype_bank_read(GAMETYPE_BANK_FIREFIGHT);
	}
	else if (multiplayer.gamemode == _gamemode_ranked)
	{
		if (multiplayer.bank_shown != GAMETYPE_BANK_RANKED)
			gametype_bank_read(GAMETYPE_BANK_RANKED);
	}
	else if (multiplayer.gamemode == _gamemode_forge)
	{
		if (multiplayer.bank_shown != GAMETYPE_BANK_FORGE)
			gametype_bank_read(GAMETYPE_BANK_FORGE);
	}
	else if (spinner)
	{
		if (spinner->parameters.list.selected_index >= NUMBER_OF_GAMETYPE_BANKS)
			spinner->parameters.list.selected_index = (short)PIN(multiplayer.bank_shown, 0, NUMBER_OF_GAMETYPE_BANKS - 1);
		if (spinner->parameters.list.selected_index != multiplayer.bank_shown)
			gametype_bank_read((short)PIN(spinner->parameters.list.selected_index, 0, NUMBER_OF_GAMETYPE_BANKS - 1));
	}
	count = (short)MIN(multiplayer.bank_count, GAMETYPE_ROWS);
	/* (the rows after the spinner's: scrolled as list_scroll does) */
	for (row = list->child; row; row = row->next, row_index++)
	{
		if (row == list->focused_child && row_index >= 1 && row_index <= GAMETYPE_ROWS)
			focused = (short)(row_index - 1);
	}
	if (focused == count - 1 && multiplayer.gametype_first + count < multiplayer.bank_count && list_scrolls_at_end())
	{
		multiplayer.gametype_first++;
		focus_row(list, (short)focused);
		focused--;
	}
	else if (focused == 0 && multiplayer.gametype_first > 0 && list_scrolls_at_end())
	{
		multiplayer.gametype_first--;
		focus_row(list, 2);
		focused++;
	}
	if (focused != NONE)
		multiplayer.gametype_chosen = (short)(multiplayer.gametype_first + focused);
	for (row = list->child, row_index = 0; row; row = row->next, row_index++)
	{
		wchar_t text[ROW_TEXT_LENGTH];

		if (row_index < 1 || row_index > GAMETYPE_ROWS)
			continue;
		row->visible = row_index - 1 < count;
		gametype_name((short)(multiplayer.gametype_first + row_index - 1), text);
		text_set(named(row, "list_item_text", 0), text);
		if (named(row, "list_item_arrows", 0))
			named(row, "list_item_arrows", 0)->visible = row == list->focused_child;
		if (named(row, "scroll_up_button", 0))
			named(row, "scroll_up_button", 0)->visible = FALSE;
		if (named(row, "scroll_down_button", 0))
			named(row, "scroll_down_button", 0)->visible = FALSE;
	}
	visible_set(named(description, "locked_gametype_icon", 0), FALSE);
	visible_set(named(description, "gametype_right_item", 0), multiplayer.bank_count > 0);
	if (multiplayer.gametype_chosen < multiplayer.bank_count)
	{
		struct game_variant variant;
		wchar_t text[ROW_TEXT_LENGTH * 2];

		gametype_name(multiplayer.gametype_chosen, text);
		text_set(named(description, "gametype_right_name", 0), text);
		if (playlist_profile_get(multiplayer.gametypes[multiplayer.bank[multiplayer.gametype_chosen]], &variant))
		{
			long engine = PIN(variant.game_engine_index, 0, 6);

			if ((widget = named(description, "gametype_right_pic", 0)) != NULL)
				widget->animation.current_frame_index = (short)engine;
			gametype_rules_text(multiplayer.gametypes[multiplayer.bank[multiplayer.gametype_chosen]], text,
				NUMBEROF(text));
			text_set(named(description, "gametype_right_data", 0), text);
		}
	}
	profile_name_show(description);
}

/* "mp profile set for game" */
static boolean gametype_choose(struct widget_instance *list)
{
	long profile_index;
	struct game_variant_options options;

	gametype_list_update(list);
	gametype_setup_end();
	if (multiplayer.gametype_chosen >= multiplayer.bank_count)
		return campaign_fail();
	profile_index = multiplayer.gametypes[multiplayer.bank[multiplayer.gametype_chosen]];
	if (playlist_profile_get_options(profile_index, &options) && options.ranked_max_players > 0)
	{
		short idx;
		for (idx = 0; idx < NUMBEROF(maximum_players); idx++)
		{
			if (maximum_players[idx] == (short)options.ranked_max_players)
			{
				multiplayer.maximum_players_index = idx;
				break;
			}
		}
	}
	return ui_widget_port_gametype_choose(profile_index);
}

/* ---- the server settings */

static void game_name_done(char const *text)
{
	short index;

	for (index = 0; text[index] && index < NUMBEROF(multiplayer.game_name) - 1; index++)
		multiplayer.game_name[index] = (wchar_t)(unsigned char)text[index];
	multiplayer.game_name[index] = 0;
}

/* Whether a network game is co-op: a campaign level or a Custom Edition
campaign map, with no game engine (set up by
ui_widget_port_cooperative_level_choose). A Custom Edition map this machine
has not is taken to be one: co-op is the only game with no game engine. */
static boolean game_cooperative(struct network_game const *game)
{
	return game && !game->variant.game_engine_index &&
		(custom_edition_maps_level_campaign(game->map.name) ||
		(custom_edition_level_name(game->map.name) && custom_edition_maps_display_index(game->map.name) == NONE));
}

/* the same, for the game this machine is hosting */
static boolean hosting_cooperative(void)
{
	void *server = global_network_game_server_get();

	return server && game_cooperative(network_game_server_get_game(server));
}

boolean multiplayer_gamemode_is_ranked(void)
{
	if (game_engine_running() &&
		game_modes_variant_is_ranked(game_engine_get_variant(), game_variant_options_get()))
	{
		return TRUE;
	}
	return !hosting_cooperative() && multiplayer.gamemode == _gamemode_ranked;
}

/* Server Setup's gametype rows, which co-op hides */
static char const *const server_settings_gametype_rows[] =
{
	"op_game_type", "op_player_options", "op_item_options", "op_vehicle_options", "op_indicator_options",
	"op_team_options",
};

/* the most players a co-op game hosted starts with (maximum_players') */
#define COOPERATIVE_DEFAULT_PLAYERS 16
/* Server Setup's help for co-op's options' rows (CO-OP OPTIONS, VOICE AND
VOTING), PASSWORD's, MAP SHARING's and BOTS' (its help_strings, tools/port_settings.py) */
#define COOPERATIVE_OPTIONS_HELP 12
#define COOPERATIVE_VOICE_OPTIONS_HELP 13
#define SERVER_PASSWORD_HELP 14
#define MAP_SHARE_OPTIONS_HELP 15
#define BOTS_SETUP_HELP 16
#define FF_PLAYER_SCALE_HELP 17
#define SCORING_SETUP_HELP 18

static short const ff_player_scale_percents[] =
{
	0, 5, 10, 20, 50
};

/* co-op's EXTRA ENEMIES' choices (network.coop_enemies_mode's values, as
gametype_options' coop_extra_enemies_spinner has them, in this order): its
amount's row is the choice's */
enum
{
	_cooperative_enemies_per_player,
	_cooperative_enemies_multiplier,
	_cooperative_enemies_none,
	NUMBER_OF_COOPERATIVE_ENEMIES_MODES
};

/* co-op's FRIENDLY FIRE's choices (network.coop_friendly_fire's values, as
gametype_options' coop_friendly_fire_spinner and server_start have them, in
this order) */
static short const cooperative_friendly_fire_modes[] =
{
	_friendly_fire_off, _friendly_fire_on, _friendly_fire_shields_only, _friendly_fire_explosives_only
};

/* the most players Server Setup shows and sets: the multiplayer game's, or
the co-op game's (COOPERATIVE_DEFAULT_PLAYERS the first time in each game
hosted: multiplayer_host) */
static short *server_settings_maximum_players_index(void)
{
	short index;

	if (!hosting_cooperative())
		return &multiplayer.maximum_players_index;
	if (!multiplayer.cooperative_maximum_players_set)
	{
		multiplayer.cooperative_maximum_players_index = NUMBEROF(maximum_players) - 1;
		for (index = 0; index < NUMBEROF(maximum_players); index++)
		{
			if (maximum_players[index] == COOPERATIVE_DEFAULT_PLAYERS)
				multiplayer.cooperative_maximum_players_index = index;
		}
		multiplayer.cooperative_maximum_players_set = TRUE;
	}
	return &multiplayer.cooperative_maximum_players_index;
}

/* Server Setup's LISTING, PRIVATE: the multiplayer game's (as
network.host_public started it), or co-op's, saved in network.coop_public */
static boolean server_settings_private(void)
{
	return hosting_cooperative() ? !config_boolean("network.coop_public") : multiplayer.game_private;
}

static void server_settings_private_set(boolean private_game)
{
	if (!hosting_cooperative())
		multiplayer.game_private = private_game;
	else if (private_game != server_settings_private() &&
		!config_write("network.coop_public", private_game ? "false" : "true"))
	{
		platform_log("menus: could not write network.coop_public to config.toml");
	}
}

/* "server settings init": the game's name (player 1's, else the one given
last), the most players, the gametype's copy (once: the screen is made
again on coming back from an option's screen) */
static boolean server_settings_initialize(struct widget_instance *list)
{
	struct widget_instance *spinner = named(list, "max_players_spinner", 0);

	campaign_bots_screen_active = FALSE;
	/* (co-op has no gametype to edit) */
	if (!hosting_cooperative())
		gametype_setup_begin();
	if (!multiplayer.game_name[0] && player_ui_get_active_player_profile_index(0) != NONE)
	{
		struct player_profile profile;

		player_ui_get_active_player_profile(0, &profile);
		ustrncpy(multiplayer.game_name, profile.player_name, NUMBEROF(multiplayer.game_name) - 1);
	}
	if (spinner)
		spinner->parameters.list.selected_index = *server_settings_maximum_players_index();
	/* (PUBLIC or PRIVATE: this game's; the screen is made again on coming
	back from an option's screen) */
	if ((spinner = named(list, "listing_spinner", 0)) != NULL)
		spinner->parameters.list.selected_index = server_settings_private() ? 1 : 0;
	if ((spinner = named(list, "ff_player_scale_spinner", 0)) != NULL)
	{
		short pct = firefight_server_player_scale_percent_get();
		short idx, chosen = 0;

		for (idx = 0; idx < NUMBEROF(ff_player_scale_percents); idx++)
		{
			if (ff_player_scale_percents[idx] == pct)
				chosen = idx;
		}
		spinner->parameters.list.selected_index = chosen;
	}
	if ((spinner = named(list, "scoring_spinner", 0)) != NULL)
		spinner->parameters.list.selected_index = config_boolean("game.campaign_score") ? 0 : 1;
	p2p_set_hosting_public(multiplayer.mode == _multiplayer_mode_host_internet && !server_settings_private());
	return TRUE;
}

static void wide_to_text(wchar_t const *wide, char *text, short size)
{
	short index;

	for (index = 0; wide[index] && index < size - 1; index++)
		text[index] = wide[index] < 0x80 ? (char)wide[index] : '?';
	text[index] = 0;
}

/* "server settings update" */
static void server_settings_update(struct widget_instance *list)
{
	struct widget_instance *spinner = named(list, "max_players_spinner", 0);
	struct widget_instance *row = named(list, "op_server_name", 0);
	char text[TEXT_FIELD_LENGTH];

	if (spinner)
		*server_settings_maximum_players_index() = (short)PIN(spinner->parameters.list.selected_index, 0,
			NUMBEROF(maximum_players) - 1);
	if ((spinner = named(list, "ff_player_scale_spinner", 0)) != NULL)
	{
		short idx = (short)PIN(spinner->parameters.list.selected_index, 0,
			NUMBEROF(ff_player_scale_percents) - 1);
		firefight_server_player_scale_percent_set(ff_player_scale_percents[idx]);
	}
	if ((spinner = named(list, "scoring_spinner", 0)) != NULL &&
		named(list, "op_scoring", 0) && named(list, "op_scoring", 0)->visible)
	{
		boolean on = spinner->parameters.list.selected_index == 0;
		if ((config_boolean("game.campaign_score") != 0) != (on != 0))
			config_write("game.campaign_score", on ? "true" : "false");
	}
	wide_to_text(multiplayer.game_name, text, sizeof(text));
	text_field_show(named(list, "server_name_value", 0), text, text_field_editing(row));
	if (multiplayer.mode == _multiplayer_mode_host_local)
		snprintf(text, sizeof(text), "NONE: A LOCAL GAME");
	else if (multiplayer.mode != _multiplayer_mode_host_internet)
		snprintf(text, sizeof(text), "NONE: A LAN GAME");
	else if (!config_boolean("network.online"))
		snprintf(text, sizeof(text), "INTERNET PLAY IS OFF (SETTINGS)");
	else if (!p2p_invite_link(text, sizeof(text)))
		snprintf(text, sizeof(text), "MADE WHEN THE GAME STARTS");
	text_field_show(named(list, "invite_value", 0), text, FALSE);
	{
		wchar_t type[ROW_TEXT_LENGTH];

		gametype_setup_type(type);
		text_set(named(list, "game_type_value", 0), type);
	}
	settings_help(list);
	{
		struct game_variant spec_variant;
		boolean cooperative = hosting_cooperative();
		boolean is_firefight = !cooperative && player_ui_game_variant_specified(&spec_variant) &&
			spec_variant.game_engine_index == game_engine_firefight;
		boolean is_ranked = !cooperative && multiplayer.gamemode == _gamemode_ranked;
		short row;

		for (row = 0; row < NUMBEROF(server_settings_gametype_rows); row++)
		{
			boolean show_row = !cooperative && !(is_ranked && row > 0);
			visible_set(named(list, server_settings_gametype_rows[row], 0), show_row);
		}
		visible_set(named(list, "op_ff_player_scale", 0), is_firefight);
		if (is_firefight)
		{
			visible_set(named(list, "op_player_options", 0), FALSE);
			visible_set(named(list, "op_item_options", 0), FALSE);
			visible_set(named(list, "op_vehicle_options", 0), FALSE);
		}
		/* co-op's options in their place, rows opening their screens
		(tools/port_settings.py's COOP_SETUP_SCREENS), each its help */
		{
			struct widget_instance *help = list->parameters.list.extended_description;

			visible_set(named(list, "op_coop_options", 0), cooperative);
			visible_set(named(list, "op_voice_options", 0), cooperative);
			if (help && list->focused_child == named(list, "op_coop_options", 0))
				help->parameters.text_box.string_list_index = COOPERATIVE_OPTIONS_HELP;
			else if (help && list->focused_child == named(list, "op_voice_options", 0))
				help->parameters.text_box.string_list_index = COOPERATIVE_VOICE_OPTIONS_HELP;
			/* (MAP SHARING's, any game's: map_share.c) */
			else if (help && list->focused_child == named(list, "op_map_share_options", 0))
				help->parameters.text_box.string_list_index = MAP_SHARE_OPTIONS_HELP;
			else if (help && list->focused_child == named(list, "op_ff_player_scale", 0))
				help->parameters.text_box.string_list_index = FF_PLAYER_SCALE_HELP;
			else if (help && list->focused_child == named(list, "op_scoring", 0))
				help->parameters.text_box.string_list_index = SCORING_SETUP_HELP;
		}
		/* BOTS and SCORING on Server Setup (Standard Multiplayer, Firefight, and Co-op Campaign) */
		{
			boolean local = multiplayer.mode == _multiplayer_mode_host_local;
			boolean show_bots = !is_ranked;
			boolean pve_bots = cooperative || is_firefight;
			struct widget_instance *help = list->parameters.list.extended_description;

			visible_set(named(list, "op_map_share_options", 0), !local);
			visible_set(named(list, "op_bots_options", 0), show_bots);
			visible_set(named(list, "op_scoring", 0), pve_bots);
			if (help && show_bots && list->focused_child == named(list, "op_bots_options", 0))
				help->parameters.text_box.string_list_index = BOTS_SETUP_HELP;
			if (show_bots)
			{
				short max_p = maximum_players[PIN(*server_settings_maximum_players_index(), 0, NUMBEROF(maximum_players) - 1)];
				long max_bots = pve_bots ? 3L : (long)MAX(0, max_p - 1);
				long count = PIN(config_integer("bots.count"), 0, MIN((long)BOTS_MAXIMUM, max_bots));
				char const *skill = config_string("bots.skill");
				char value[64];
				short index;

				if (!count)
					snprintf(value, sizeof(value), "NONE");
				else if (pve_bots)
				{
					float pen = bots_difficulty_multiplier_penalty(bots_difficulty_from_string(skill));
					snprintf(value, sizeof(value), "%ld, %s (-%.1fX)", count, skill ? skill : "NORMAL", pen);
				}
				else
				{
					snprintf(value, sizeof(value), "%ld, %s, %s", count, skill ? skill : "NORMAL",
						config_boolean("bots.replace_on_join") ? "REPLACE" : "KEEP");
				}
				for (index = 0; value[index]; index++)
					value[index] = (char)toupper((unsigned char)value[index]);
				text_field_show(named(list, "bots_options_value", 0), value, FALSE);
			}
		}
	}
	/* LISTING (an internet game's): PUBLIC, listed in everyone's server
	browser, or PRIVATE, for this game. Its help is its choice's */
	visible_set(named(list, "op_listing", 0), multiplayer.mode == _multiplayer_mode_host_internet &&
		config_boolean("network.online") && config_boolean("network.public_lobby"));
	if ((spinner = named(list, "listing_spinner", 0)) != NULL && named(list, "op_listing", 0) &&
		named(list, "op_listing", 0)->visible)
	{
		boolean public = spinner->parameters.list.selected_index == 0;
		struct widget_instance *help = list->parameters.list.extended_description;

		server_settings_private_set(!public);
		p2p_set_hosting_public(public);
		if (help && list->focused_child == named(list, "op_listing", 0))
			help->parameters.text_box.string_list_index = (short)(10 + spinner->parameters.list.selected_index);
	}
	/* PASSWORD (a PUBLIC internet game's): its stars, NONE if it has none */
	row = named(list, "op_password", 0);
	visible_set(row, named(list, "op_listing", 0) && named(list, "op_listing", 0)->visible &&
		!server_settings_private());
	if (row && !row->visible && text_field_editing(row))
		text_field_end(FALSE);
	if (row && row->visible)
	{
		struct widget_instance *help = list->parameters.list.extended_description;
		short index;

		for (index = 0; multiplayer.game_password[index] && index < (short)sizeof(text) - 1; index++)
			text[index] = '*';
		text[index] = 0;
		text_field_show(named(list, "password_value", 0), index ? text : "NONE", text_field_editing(row));
		if (help && list->focused_child == row)
			help->parameters.text_box.string_list_index = SERVER_PASSWORD_HELP;
	}
	/* Space visible option rows evenly at standard non-overlapping height */
	{
		static char const * const all_rows[] = {
			"op_server_name",
			"op_max_players",
			"op_invite",
			"op_listing",
			"op_password",
			"op_map_share_options",
			"op_coop_options",
			"op_voice_options",
			"op_game_type",
			"op_player_options",
			"op_item_options",
			"op_vehicle_options",
			"op_indicator_options",
			"op_team_options",
			"op_ff_player_scale",
			"op_bots_options",
			"op_scoring",
		};
		short index, visible_count = 0;
		short top, step;

		for (index = 0; index < NUMBEROF(all_rows); index++)
		{
			struct widget_instance *item = named(list, all_rows[index], 0);
			if (item && item->visible)
				visible_count++;
		}
		top = visible_count >= 13 ? 72 : 73;
		step = visible_count <= 10 ? 30 : visible_count == 11 ? 28 : visible_count == 12 ? 26 : 25;
		for (index = 0; index < NUMBEROF(all_rows); index++)
		{
			struct widget_instance *item = named(list, all_rows[index], 0);
			if (item && item->visible)
			{
				item->vertical_offset = top;
				top += step;
			}
		}
		if (list->parameters.list.extended_description)
		{
			short min_help_top = (short)(top - step + 26);
			list->parameters.list.extended_description->vertical_offset =
				min_help_top > 374 ? min_help_top : 374;
		}
	}
}

static void server_password_done(char const *text)
{
	snprintf(multiplayer.game_password, sizeof(multiplayer.game_password), "%s", text);
}

/* "ss edit server password" (its row's A: begins, or ends) */
static boolean server_password_edit(struct widget_instance *row)
{
	if (text_field_editing(row))
	{
		text_field_end(TRUE);
		return TRUE;
	}
	text_field_begin_masked(row, multiplayer.game_password, PASSWORD_LENGTH, server_password_done);
	return TRUE;
}

/* "ss edit server name" (its row's A: begins, or ends) */
static boolean server_name_edit(struct widget_instance *row)
{
	char text[TEXT_FIELD_LENGTH];

	if (text_field_editing(row))
	{
		text_field_end(TRUE);
		return TRUE;
	}
	wide_to_text(multiplayer.game_name, text, sizeof(text));
	text_field_begin(row, text, NUMBEROF(multiplayer.game_name) - 1, game_name_done);
	return TRUE;
}

/* "ss copy invite" */
static boolean invite_copy(void)
{
	char link[TEXT_FIELD_LENGTH];

	if (!p2p_invite_link(link, sizeof(link)))
		return campaign_fail();
	platform_clipboard_set(link);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

/* "ss start game": the settings given the server, then the lobby */
static boolean server_start(void)
{
	if (text_field_editing(NULL))
		text_field_end(TRUE);
	network_game_server_port_set_settings(multiplayer.game_name,
		maximum_players[PIN(*server_settings_maximum_players_index(), 0, NUMBEROF(maximum_players) - 1)]);
	/* (its listing's password: a PUBLIC internet game's, else none) */
	p2p_set_hosting_password(multiplayer.mode == _multiplayer_mode_host_internet && !server_settings_private() ?
		multiplayer.game_password : NULL);
	/* the gametype as Server Setup's options left it (co-op keeps its own,
	and its FRIENDLY FIRE) */
	if (hosting_cooperative())
	{
		/* (as CO-OP OPTIONS left them: network.coop_friendly_fire, in
		cooperative_friendly_fire_modes' order, network.coop_player_collisions
		and network.coop_ally_friendly_fire) */
		static char const *const friendly_fire_names[] = { "off", "on", "shields_only", "explosives_only" };
		char const *friendly_fire = config_string("network.coop_friendly_fire");
		short index;

		gametype_setup_end();
		multiplayer.cooperative_friendly_fire = _friendly_fire_on;
		for (index = 0; friendly_fire && index < (short)NUMBEROF(friendly_fire_names); index++)
		{
			if (!strcmp(friendly_fire, friendly_fire_names[index]))
				multiplayer.cooperative_friendly_fire = cooperative_friendly_fire_modes[index];
		}
		multiplayer.cooperative_no_player_collisions = !config_boolean("network.coop_player_collisions");
		network_game_server_port_set_cooperative_friendly_fire(multiplayer.cooperative_friendly_fire);
		network_game_server_port_set_cooperative_player_collisions(!multiplayer.cooperative_no_player_collisions);
		network_game_server_port_set_cooperative_ally_friendly_fire(config_boolean("network.coop_ally_friendly_fire"));
	}
	else if (!gametype_setup_apply())
		return campaign_fail();
	return global_network_game_server_get() != NULL;
}

/* ---- the browser (Join Game > Server Browser, LAN, Direct Link): the
games found on the LAN, or (Direct Link) from internet play's peers
(reached by an invite link, the clipboard, or Discord) */

#define JOIN_GAME_LABELS "pc\\main_menu\\multiplayer_type_select\\join_game\\join_game_ticker_labels"
enum { _join_game_label_servers = 6, _join_game_label_players, _join_game_label_page };

static boolean game_from_peer(struct advertised_game const *game)
{
	unsigned long address;

	return p2p_peer_address(game->xnaddr + 2, &address) != 0;
}

int campaign_levels_all_unlocked(void)
{
	char const *progression = config_string("game.campaign_progression");

	return progression && !_stricmp(progression, "all_unlocked");
}

boolean menu_advertised_game_from_peer(long index)
{
	void *client = global_network_game_client_get();
	struct advertised_game *games = client ? network_game_client_get_available_games(client) : NULL;

	return games && index >= 0 && index < MAXIMUM_ADVERTISED_GAMES && game_from_peer(&games[index]);
}

static boolean advertised_in_progress(struct advertised_game *game)
{
	return network_game_client_advertised_game_in_progress(global_network_game_client_get(), game);
}

static void browser_games_read(void)
{
	void *client = global_network_game_client_get();
	struct advertised_game *games = client ? network_game_client_get_available_games(client) : NULL;
	short pass, index;

	multiplayer.game_count = 0;
	if (!games || multiplayer.mode == _multiplayer_mode_server_browser)
		return;
	/* (the open games, then those under way) */
	for (pass = 0; pass < 2; pass++)
	{
		for (index = 0; index < MAXIMUM_ADVERTISED_GAMES; index++)
		{
			struct advertised_game *game = &games[index];

			if (network_game_client_advertised_game_is_valid(game) && !advertised_in_progress(game) == (pass == 0) &&
				game_from_peer(game) == (multiplayer.mode == _multiplayer_mode_direct_link))
			{
				if (multiplayer.mode == _multiplayer_mode_lan)
				{
					if (multiplayer.gamemode == _gamemode_ranked)
						continue;
					if ((game->engine_type == game_engine_firefight) !=
						(multiplayer.gamemode == _gamemode_firefight || multiplayer.firefight_mode != FALSE))
					{
						continue;
					}
				}
				multiplayer.games[multiplayer.game_count++] = game;
			}
		}
	}
	if (multiplayer.game_chosen >= multiplayer.game_count)
		multiplayer.game_chosen = (short)MAX(0, multiplayer.game_count - 1);
}

static short browser_row_index(struct widget_instance *row)
{
	return row && !strncmp(row->name, "server_item_", 12) ? (short)(atoi(row->name + 12) - 1) : NONE;
}

/* the ping each row shows (-1: none), coloured by pc_menu_text_color: green
under PING_GOOD milliseconds, yellow under PING_FAIR, red above */
static short browser_row_pings[BROWSER_ROWS];
enum
{
	PING_GOOD = 80,
	PING_FAIR = 160,
};
#define PING_COLOR_GOOD 0x5ED38CFFUL
#define PING_COLOR_FAIR 0xE8C547FFUL
#define PING_COLOR_POOR 0xE86A5AFFUL

/* ui_widget.c: a text's colour in place of its own (FALSE: its own): a
server browser row's ping by how good it is */
boolean pc_menu_text_color(struct widget_instance *widget, real_argb_color *color)
{
	short row = widget->parent ? browser_row_index(widget->parent) : NONE;
	short ping;
	unsigned long rgba;

	if (row < 0 || row >= BROWSER_ROWS || strcmp(widget->name, "server_item_ping"))
		return FALSE;
	ping = browser_row_pings[row];
	if (ping < 0)
		return FALSE;
	rgba = ping < PING_GOOD ? PING_COLOR_GOOD : ping < PING_FAIR ? PING_COLOR_FAIR : PING_COLOR_POOR;
	color->red = (real)((rgba >> 24) & 0xFF) / 255.0f;
	color->green = (real)((rgba >> 16) & 0xFF) / 255.0f;
	color->blue = (real)((rgba >> 8) & 0xFF) / 255.0f;
	return TRUE;
}

/* a bar of buttons' focus, off one hidden: on its first one shown */
static void focus_off_hidden(struct widget_instance *bar)
{
	struct widget_instance *button;
	short button_index = 0;

	for (button = bar && bar->focused_child && !bar->focused_child->visible ? bar->child : NULL; button;
		button = button->next, button_index++)
	{
		if (button->visible)
		{
			bar->focused_child = button;
			bar->parameters.list.selected_index = button_index;
			break;
		}
	}
}

/* whether the browser's list's child can take focus (once visible) */
static boolean browser_item_usable(struct widget_instance *child)
{
	return browser_row_index(child) != NONE || !strcmp(child->name, "join_game_button_bar");
}

/* the browser's focus, off what cannot have it (at the start, or a row whose
game went): the first game, else the buttons (off those hidden); the row of
the game chosen is `chosen_row` (NONE: none shown) */
static void browser_focus(struct widget_instance *list, short chosen_row)
{
	char const *const choices[] = { "server_item_1", "join_game_button_bar" };
	struct widget_instance *focused = list->focused_child;
	struct widget_instance *row;
	short index;

	/* (the rows' backgrounds, as Custom Edition's: the chosen game's
	outlined in yellow, the focused one's in blue. The engine shows a list
	item's focus only on a bitmap of two frames, and theirs has three:
	normal, focused, selected) */
	for (row = list->child; row; row = row->next)
	{
		short row_index = browser_row_index(row);

		if (row_index != NONE)
			row->animation.current_frame_index = row_index == chosen_row ? 2 : row == focused ? 1 : 0;
	}
	focus_off_hidden(named(list, "join_game_button_bar", 0));
	if (focused && focused->visible && !focused->disabled)
		return;
	for (index = 0; index < NUMBEROF(choices); index++)
	{
		struct widget_instance *child;
		short child_index = 0;

		for (child = list->child; child; child = child->next, child_index++)
		{
			if (!strcmp(child->name, choices[index]) && child->visible)
			{
				list->focused_child = child;
				list->parameters.list.selected_index = child_index;
				return;
			}
		}
	}
}

/* (the server browser's: below) */
static void lobby_browser_begin(void);
static void lobby_browser_update(struct widget_instance *list);
static boolean lobby_browser_select(struct widget_instance *widget, short controller, short row,
	boolean *widget_deleted);

/* "gamespy screen init": the mode's title and parts; the games' client */
static boolean browser_initialize(struct widget_instance *screen, struct event_record *event,
	boolean *widget_deleted)
{
	char const *title = multiplayer.mode == _multiplayer_mode_lan ? "header_lan" :
		multiplayer.mode == _multiplayer_mode_direct_link ? "header_direct_link" : "header_server_browser";
	char const *const titles[] = { "header_internet", "header_lan", "header_direct_link", "header_server_browser" };
	char const *const unused[] = { "op_browser_mode", "join_game_button_update", "join_game_button_filters" };
	short index;

	server_browser_initialize(screen);
	for (index = 0; index < NUMBEROF(titles); index++)
		visible_set(named(screen, titles[index], 0), !strcmp(titles[index], title));
	for (index = 0; index < NUMBEROF(unused); index++)
		visible_set(named(screen, unused[index], 0), FALSE);
	visible_set(named(screen, "button_clipboard", 0), multiplayer.mode == _multiplayer_mode_direct_link);
	visible_set(named(screen, "join_game_button_refresh", 0), multiplayer.mode == _multiplayer_mode_server_browser);
	{
		struct widget_instance *list = named(screen, "join_game_items_list", 0);
		struct widget_instance *child;

		/* (only the games' rows and the buttons take focus: not the column titles, which sort nothing, nor the ticker) */
		for (child = list ? list->child : NULL; child; child = child->next)
			child->disabled = !browser_item_usable(child);
		if (list)
			browser_focus(list, NONE);
	}
	visible_set(named(screen, "scroll_up_button", 0), FALSE);
	visible_set(named(screen, "scroll_down_button", 0), FALSE);
	/* (the columns' sort arrows, both of each over its title: the list is
	not sorted) */
	for (index = 0; named(screen, "header_sort_arrows", index); index++)
		visible_set(named(screen, "header_sort_arrows", index), FALSE);
	multiplayer.game_chosen = 0;
	/* (the server browser's games are internet play's listings; joining
	one reaches its host, whose game the client then finds as Direct Link's) */
	if (multiplayer.mode == _multiplayer_mode_server_browser)
		lobby_browser_begin();
	return ui_widget_port_browse(screen, event, widget_deleted);
}

/* a scenario's name without its path */
static char const *scenario_name(char const *path)
{
	char const *name = strrchr(path, '\\');

	return name ? name + 1 : path;
}

/* the campaign level whose scenario a path or name is (a network co-op
game's map), else NONE */
static short campaign_level_of(char const *map_name)
{
	short level;

	/* (a Custom Edition map, custom_maps\a30, is never one) */
	if (map_name &&
		(_strnicmp(map_name, CUSTOM_EDITION_LEVEL_NAME_PREFIX, csstrlen(CUSTOM_EDITION_LEVEL_NAME_PREFIX)) == 0 ||
		 custom_edition_level_name(map_name)))
		return NONE;
	for (level = 0; level < NUMBER_OF_SINGLE_PLAYER_LEVELS; level++)
	{
		if (!_stricmp(scenario_name(main_get_solo_level_name(level)), scenario_name(map_name)))
			return level;
	}
	return NONE;
}

/* a game's type as the browsers and lobby show it: its engine's name, or
CO-OP for one with none (network co-op: ui_widget_port_cooperative_level_choose) */
static wchar_t const *game_type_name(long engine_type)
{
	return engine_type == 0 ? L"CO-OP" : engine_names[PIN(engine_type, 0, 6)];
}

/* ... or its OpenCE gametype's kind (its advertisement's or listing's:
game_modes.h), "HC" for Hardcore in the browsers' narrow column */
static wchar_t const *game_kind_name(long engine_type, byte kind, boolean abbreviated)
{
	wchar_t const *name = game_modes_kind_name(engine_type, kind, abbreviated);

	return name ? name : game_type_name(engine_type);
}

/* an advertised (LAN, Direct Link) game's kind */
static byte advertised_game_kind(struct advertised_game *game)
{
	return network_game_client_advertised_game_kind(global_network_game_client_get(), game);
}

/* a map's name as the menus show it (a campaign level's too, hosted as
network co-op; its scenario's name, if not one of theirs), from its
scenario's path or name */
static void map_display_name(char const *map_name, wchar_t *text)
{
	char const *const *names;
	short index;
	short count = xbox_multiplayer_map_count(ui_widget_port_multiplayer_maps(&names, NULL));

	/* (a Custom Edition map's, if this machine has it: custom_edition_maps.c) */
	if (custom_edition_level_name(map_name) || custom_edition_maps_display_index(map_name) != NONE)
	{
		short display_index = custom_edition_maps_display_index(map_name);
		wchar_t const *name = display_index != NONE ? custom_edition_maps_name(display_index) : NULL;

		if (name)
		{
			ustrncpy(text, name, ROW_TEXT_LENGTH - 1);
			text[ROW_TEXT_LENGTH - 1] = 0;
			return;
		}
		map_name = scenario_name(map_name);
		count = 0;
	}
	for (index = 0; index < count; index++)
	{
		if (!_stricmp(scenario_name(names[index]), scenario_name(map_name)))
		{
			string_get("pc\\main_menu\\mp_map_list", index, text);
			return;
		}
	}
	if ((index = campaign_level_of(map_name)) != NONE)
	{
		string_get("pc\\main_menu\\map_list", index, text);
		return;
	}
	for (index = 0; map_name[index] && index < ROW_TEXT_LENGTH - 1; index++)
		text[index] = (wchar_t)(unsigned char)map_name[index];
	text[index] = 0;
}

static void game_map_name(struct advertised_game const *game, wchar_t *text)
{
	map_display_name(game->map_name, text);
}

/* ---- Join Game > Server Browser: internet play's public games (their
hosts' listings, p2p_lobby.c). Joining one joins its invite (as Direct
Link's PASTE LINK); once its host is reached its game is among the client's,
and an A press (posted) joins it, or shows its lobby if under way */

#define LOBBY_BROWSER_GAMES 256
/* milliseconds: a host not reached in this long is given up (p2p's own
wait is longer) */
#define LOBBY_BROWSER_JOIN_TIMEOUT 30000
/* how long the browser looks before it says it found none, and shows a
join that failed */
#define LOBBY_BROWSER_LOOK_TIME 6000
#define LOBBY_BROWSER_FAILED_TIME 8000

static struct
{
	struct p2p_listing games[LOBBY_BROWSER_GAMES];
	/* the games found, the first on the rows, the one last focused (Join's) */
	short count, first, chosen;
	/* whether it has shown a game (the first has the focus) */
	boolean shown;
	unsigned long begin_time;
	/* a game being joined: its host, its name, since when, the controller
	that asked; ready: its game reached (the A press posted) */
	boolean joining, ready;
	unsigned char identifier[6];
	char name[P2P_LISTING_NAME_SIZE + 1];
	unsigned long join_time;
	short controller;
	/* when the press was posted (again if it was lost) */
	unsigned long ready_time;
	/* the last that failed, and when */
	char failed_name[P2P_LISTING_NAME_SIZE + 1];
	unsigned long failed_time;
	boolean failed_nat_strict;
	/* a game with a password chosen: the password screen's (its own copy:
	the rows' are read again each frame), and a join it began, which the
	browser takes up again as it comes back */
	struct p2p_listing password_game;
	boolean password_joined;
} lobby_browser;

/* the row of the browser's chosen game, NONE if it is not on the rows */
static short browser_chosen_row(void)
{
	boolean server_browser = multiplayer.mode == _multiplayer_mode_server_browser;
	short chosen = server_browser ? lobby_browser.chosen : multiplayer.game_chosen;
	short row = server_browser ? (short)(chosen - lobby_browser.first) : chosen;

	return chosen < (server_browser ? lobby_browser.count : multiplayer.game_count) && row >= 0 &&
		row < BROWSER_ROWS ? row : NONE;
}

static void lobby_browser_begin(void)
{
	lobby_browser.count = 0;
	lobby_browser.first = 0;
	lobby_browser.chosen = 0;
	lobby_browser.shown = FALSE;
	/* (going on with the join the password screen began) */
	lobby_browser.joining = lobby_browser.password_joined;
	lobby_browser.ready = FALSE;
	lobby_browser.password_joined = FALSE;
	lobby_browser.begin_time = system_milliseconds();
	p2p_lobby_browse(TRUE);
}

/* "gamespy screen dispose" */
static void lobby_browser_end(void)
{
	lobby_browser.joining = lobby_browser.ready = FALSE;
	p2p_lobby_browse(FALSE);
}

static void text_to_wide(char const *text, wchar_t *wide, short size)
{
	short index;

	for (index = 0; text[index] && index < size - 1; index++)
		wide[index] = (wchar_t)(unsigned char)text[index];
	wide[index] = 0;
}

/* the games whose names are valid, as a host keeps the names of the
machines and players that join it (network_game_server_clean_name): each
name cleaned (player_name_clean), and a game whose name has nothing left
that names it left out; returns how many are left */
static short lobby_browser_valid_games(struct p2p_listing *games, short count)
{
	short read;
	short written = 0;

	for (read = 0; read < count; read++)
	{
		wchar_t name[P2P_LISTING_NAME_SIZE + 1];
		short index;

		if (multiplayer.gamemode == _gamemode_ranked)
			continue;
		if ((games[read].engine_type == game_engine_firefight) !=
			(multiplayer.gamemode == _gamemode_firefight || multiplayer.firefight_mode != FALSE))
		{
			continue;
		}
		text_to_wide(games[read].name, name, NUMBEROF(name));
		if (!player_name_clean(name, NUMBEROF(name)))
			continue;
		/* (ASCII still: the listing's names are) */
		for (index = 0; name[index]; index++)
			games[read].name[index] = (char)name[index];
		games[read].name[index] = 0;
		if (written != read)
			games[written] = games[read];
		written++;
	}
	return written;
}

/* the game being joined, once its host is reached (the client's game from
it), else NULL */
static struct advertised_game *lobby_browser_joined_game(void)
{
	void *client = global_network_game_client_get();
	struct advertised_game *games = client ? network_game_client_get_available_games(client) : NULL;
	short index;

	for (index = 0; games && index < MAXIMUM_ADVERTISED_GAMES; index++)
	{
		if (network_game_client_advertised_game_is_valid(&games[index]) &&
			!memcmp(games[index].xnaddr + 2, lobby_browser.identifier, sizeof(lobby_browser.identifier)))
		{
			return &games[index];
		}
	}
	return NULL;
}

/* the focus on a game's row (0 to BROWSER_ROWS - 1) */
static void lobby_browser_focus_row(struct widget_instance *list, short row)
{
	struct widget_instance *child;
	short child_index = 0;
	char name[16];

	sprintf(name, "server_item_%d", row + 1);
	for (child = list->child; child; child = child->next, child_index++)
	{
		if (!strcmp(child->name, name))
		{
			list->focused_child = child;
			list->parameters.list.selected_index = child_index;
			return;
		}
	}
}

/* "gamespy screen update" for the server browser: the rows (scrolled on
at their ends), the line below them, the counts over the titles */
static void lobby_browser_update(struct widget_instance *list)
{
	struct widget_instance *stats = list->parameters.list.extended_description;
	struct widget_instance *row;
	short focused = browser_row_index(list->focused_child);
	wchar_t text[ROW_TEXT_LENGTH * 2];
	unsigned long now = system_milliseconds();
	short chosen;

	lobby_browser.count = lobby_browser_valid_games(lobby_browser.games,
		(short)p2p_lobby_games(lobby_browser.games, LOBBY_BROWSER_GAMES));
	if (focused == BROWSER_ROWS - 1 && lobby_browser.first + BROWSER_ROWS < lobby_browser.count && list_scrolls_at_end())
	{
		lobby_browser.first++;
		lobby_browser_focus_row(list, --focused);
	}
	else if (focused == 0 && lobby_browser.first > 0 && list_scrolls_at_end())
	{
		lobby_browser.first--;
		lobby_browser_focus_row(list, ++focused);
	}
	lobby_browser.first = (short)PIN(lobby_browser.first, 0, MAX(0, lobby_browser.count - BROWSER_ROWS));
	/* (the first game found takes the focus from the buttons, which had it
	while there were none) */
	if (lobby_browser.count && !lobby_browser.shown)
	{
		struct widget_instance *first_row = named(list, "server_item_1", 0);

		lobby_browser.shown = TRUE;
		if (first_row)
		{
			first_row->visible = TRUE;
			lobby_browser_focus_row(list, 0);
			focused = 0;
			lobby_browser.chosen = lobby_browser.first;
		}
	}
	if (focused != NONE && !ui_widget_port_hover_focused())
		lobby_browser.chosen = (short)(lobby_browser.first + focused);
	chosen = lobby_browser.chosen;
	for (row = list->child; row; row = row->next)
	{
		short index = browser_row_index(row);
		struct p2p_listing const *game;

		if (index == NONE)
			continue;
		index = (short)(index + lobby_browser.first);
		row->visible = index < lobby_browser.count;
		if (index >= lobby_browser.count)
			continue;
		game = &lobby_browser.games[index];
		text_to_wide(game->name, text, ROW_TEXT_LENGTH);
		text_set(named(row, "server_item_server_name", 0), text);
		map_display_name(game->map, text);
		text_set(named(row, "server_item_map", 0), text);
		text_set(named(row, "server_item_type", 0), game_kind_name(game->engine_type, game->kind, TRUE));
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%d/%d", game->player_count, game->maximum_player_count);
		text_set(named(row, "server_item_players", 0), text);
		/* (its host's ping, once probed: p2p_lobby.c) */
		browser_row_pings[index - lobby_browser.first] = game->failed ? -1 : game->ping;
		if (game->ping >= 0)
			usnprintf(text, ROW_TEXT_LENGTH - 1, L"%d", game->ping);
		else if (game->master && game->region[0])
		{
			short r_idx;
			text_to_wide(game->region, text, ROW_TEXT_LENGTH);
			for (r_idx = 0; text[r_idx]; r_idx++)
			{
				if (text[r_idx] >= 'a' && text[r_idx] <= 'z')
					text[r_idx] = (wchar_t)(text[r_idx] - 'a' + 'A');
			}
		}
		text_set(named(row, "server_item_ping", 0), game->failed ? L"FAILED" :
			(game->ping >= 0 || (game->master && game->region[0])) ? text :
			!game->open ? L"CLOSED" : game->in_progress ? L"LIVE" : L"-");
		/* (the lock: a game with a password; the server: a dedicated
		server's. Their bitmaps' first frame is empty, the PC version's for
		none; the next is the icon, in the menus' blue) */
		{
			struct widget_instance *lock = named(row, "server_item_locked", 0);
			struct widget_instance *dedicated = named(row, "server_item_dedicated", 0);

			visible_set(lock, game->locked);
			if (lock)
				lock->animation.current_frame_index = 1;
			visible_set(dedicated, game->dedicated);
			if (dedicated)
				dedicated->animation.current_frame_index = 1;
		}
		visible_set(named(row, "server_item_classic", 0), FALSE);
	}
	/* joining: once the host is reached, its game joined (by an A press, on
	its row); given up after a while */
	if (lobby_browser.ready && now - lobby_browser.ready_time > 3000 &&
		now - lobby_browser.join_time <= LOBBY_BROWSER_JOIN_TIMEOUT)
	{
		lobby_browser.ready_time = now;
		ui_widget_port_post_button(lobby_browser.controller, BUTTON_A);
	}
	if (lobby_browser.joining)
	{
		if (lobby_browser.ready)
		{
			/* (its press posted: waiting for it, or for the timeout) */
			if (now - lobby_browser.join_time > LOBBY_BROWSER_JOIN_TIMEOUT)
				lobby_browser.joining = lobby_browser.ready = FALSE;
		}
		else if (lobby_browser_joined_game())
		{
			short index;

			lobby_browser.ready = TRUE;
			for (index = 0; index < lobby_browser.count; index++)
			{
				if (!memcmp(lobby_browser.games[index].identifier, lobby_browser.identifier, 6) &&
					index >= lobby_browser.first && index < lobby_browser.first + BROWSER_ROWS)
				{
					lobby_browser_focus_row(list, (short)(index - lobby_browser.first));
				}
			}
			lobby_browser.ready_time = now;
			ui_widget_port_post_button(lobby_browser.controller, BUTTON_A);
		}
		else if (now - lobby_browser.join_time > LOBBY_BROWSER_JOIN_TIMEOUT)
		{
			p2p_lobby_mark_failed(lobby_browser.identifier);
			csmemcpy(lobby_browser.failed_name, lobby_browser.name, sizeof(lobby_browser.failed_name));
			lobby_browser.failed_time = now ? now : 1;
			lobby_browser.failed_nat_strict = p2p_nat_strict();
			lobby_browser.joining = FALSE;
			platform_log("menus: could not reach the server browser's game %s", lobby_browser.name);
			ui_play_audio_feedback_sound(SOUND_ERROR);
		}
	}
	/* the line below the rows */
	{
		wchar_t name[P2P_LISTING_NAME_SIZE + 1];

		if (!config_boolean("network.online"))
			usnprintf(text, NUMBEROF(text) - 1, L"Internet play is off (Settings)");
		else if (!config_boolean("network.public_lobby"))
			usnprintf(text, NUMBEROF(text) - 1, L"The server browser is off (network.public_lobby)");
		else if (lobby_browser.joining)
		{
			text_to_wide(lobby_browser.name, name, NUMBEROF(name));
			usnprintf(text, NUMBEROF(text) - 1, L"Connecting to %s...", name);
		}
		else if (lobby_browser.failed_time && now - lobby_browser.failed_time < LOBBY_BROWSER_FAILED_TIME)
		{
			text_to_wide(lobby_browser.failed_name, name, NUMBEROF(name));
			/* (and why, when this network's NAT is what stops it: the router's
			UPnP, or a forwarded network.tunnel_port, lets it through) */
			usnprintf(text, NUMBEROF(text) - 1, lobby_browser.failed_nat_strict ?
				L"Strict NAT, turn on UPnP: could not reach %s" :
				L"Could not reach %s", name);
		}
		else if (!lobby_browser.count)
		{
			/* (an old build sees none of the games: it says why) */
			int newer = 0, wrong_clock = 0;

			p2p_lobby_hidden_games(&newer, &wrong_clock);
			if (now - lobby_browser.begin_time < LOBBY_BROWSER_LOOK_TIME)
				usnprintf(text, NUMBEROF(text) - 1, L"Looking for public games...");
			else if (newer)
			{
				usnprintf(text, NUMBEROF(text) - 1, L"%d %s on a newer version: update the game to join", newer,
					newer == 1 ? L"game is" : L"games are");
			}
			else
				usnprintf(text, NUMBEROF(text) - 1, L"No public games found");
		}
		else if (chosen < lobby_browser.count)
		{
			struct p2p_listing const *game = &lobby_browser.games[chosen];
			wchar_t gametype[P2P_LISTING_GAMETYPE_SIZE + 1];

			text_to_wide(game->gametype, gametype, NUMBEROF(gametype));
			usnprintf(text, NUMBEROF(text) - 1, L"%s: %d %s of %d%s%s", gametype, game->player_count,
				game->player_count == 1 ? L"player" : L"players", game->maximum_player_count,
				!game->open ? L", full or starting" : game->in_progress ? L", under way" : L"",
				game->locked ? L", password" : L"");
		}
		else
			text[0] = 0;
		text[NUMBEROF(text) - 1] = 0;
		text_set(named(list, "ticker_player_info", 0), text);
		text_set(named(list, "ticker_rules_info", 0), L"");
	}
	if (stats)
	{
		wchar_t label[ROW_TEXT_LENGTH];
		long players = 0;
		short index;

		for (index = 0; index < lobby_browser.count; index++)
			players += lobby_browser.games[index].player_count;
		string_get(JOIN_GAME_LABELS, _join_game_label_players, label);
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s %ld", label, players);
		text_set(named(stats, "player_count_label", 0), text);
		string_get(JOIN_GAME_LABELS, _join_game_label_page, label);
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s %d/%d", label, chosen / BROWSER_ROWS + 1,
			MAX(1, (lobby_browser.count + BROWSER_ROWS - 1) / BROWSER_ROWS));
		text_set(named(stats, "page_count_label", 0), text);
		string_get(JOIN_GAME_LABELS, _join_game_label_servers, label);
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s %d", label, lobby_browser.count);
		text_set(named(stats, "server_count_label", 0), text);
	}
	browser_focus(list, browser_chosen_row());
}

/* the game's host reached by its invite (joined, once it is reached:
lobby_browser_update) */
static boolean lobby_browser_join(struct p2p_listing const *game, short controller)
{
	/* (a public game: the text chat is off in it, whatever its host says:
	network_chat.c) */
	if (!p2p_join_listing(game))
		return FALSE;
	csmemcpy(lobby_browser.identifier, game->identifier, sizeof(lobby_browser.identifier));
	csmemcpy(lobby_browser.name, game->name, sizeof(lobby_browser.name));
	lobby_browser.joining = TRUE;
	lobby_browser.ready = FALSE;
	lobby_browser.join_time = system_milliseconds();
	lobby_browser.controller = controller;
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

/* the server browser's rows and buttons: Refresh asks the hosts again; a
row (or Join) joins its game's invite, a game with a password once its
screen has it; the A press posted once its host is reached joins its game
(or shows its lobby, if under way) */
static boolean lobby_browser_select(struct widget_instance *widget, short controller, short row,
	boolean *widget_deleted)
{
	struct p2p_listing const *game;
	short index;

	/* (the press posted: whatever has the focus) */
	if (lobby_browser.ready)
	{
		struct advertised_game *found = lobby_browser_joined_game();

		lobby_browser.joining = lobby_browser.ready = FALSE;
		if (!found)
			return campaign_fail();
		if (advertised_in_progress(found))
		{
			/* (player 1 joining it: others join them there) */
			if (!multiplayer_player(controller))
				return FALSE;
			csmemcpy(multiplayer.preview_key_id, found->key_id, sizeof(multiplayer.preview_key_id));
			csmemcpy(multiplayer.preview_xnaddr, found->xnaddr, sizeof(multiplayer.preview_xnaddr));
			return ui_widget_port_open(widget, PREVIEW_NAME, widget_deleted);
		}
		if (!found->open)
			return campaign_fail();
		if (!multiplayer_player(controller))
			return FALSE;
		return ui_widget_port_join(widget, found, LOBBY_NAME, widget_deleted);
	}
	if (strstr(widget->name, "button_refresh"))
	{
		p2p_lobby_refresh();
		lobby_browser.begin_time = system_milliseconds();
		ui_play_audio_feedback_sound(SOUND_FORWARD);
		return TRUE;
	}
	if (row == NONE && !strstr(widget->name, "button_join"))
		return TRUE;
	if (lobby_browser.joining)
		return TRUE;
	index = row != NONE ? (short)(lobby_browser.first + row) : lobby_browser.chosen;
	if (index >= lobby_browser.count)
		return campaign_fail();
	game = &lobby_browser.games[index];
	if (!game->open || !config_boolean("network.online"))
		return campaign_fail();
	/* (a game with a password: its screen asks for it, then joins) */
	if (game->locked)
	{
		lobby_browser.password_game = *game;
		lobby_browser.controller = controller;
		return ui_widget_port_open(widget, PASSWORD_NAME, widget_deleted);
	}
	return lobby_browser_join(game, controller) || campaign_fail();
}

/* ---- the server browser's password screen (a game with a password
chosen: lobby_browser_select, tools/port_settings.py): its password typed at
once, shown as stars; JOIN GAME, or Enter, joins the game with it if it is
the game's (the browser, back again, goes on with the join), else its help
says it is not and the typing begins again */

/* its help's lines (tools/port_settings.py's) */
enum
{
	_password_help_ask = 1,
	_password_help_wrong,
	_password_help_failed,
};

static struct
{
	char text[PASSWORD_LENGTH + 1];
	short help;
} password_screen;

static void password_screen_done(char const *text)
{
	snprintf(password_screen.text, sizeof(password_screen.text), "%s", text);
}

/* "port password init" */
static boolean password_screen_initialize(struct widget_instance *list)
{
	struct widget_instance *row = named(list, "op_password", 0);

	password_screen.text[0] = 0;
	password_screen.help = _password_help_ask;
	if (row)
		text_field_begin_masked(row, "", PASSWORD_LENGTH, password_screen_done);
	return TRUE;
}

/* "port password update" */
static void password_screen_update(struct widget_instance *list)
{
	struct widget_instance *help = list->parameters.list.extended_description;
	char stars[PASSWORD_LENGTH + 1];
	short index;

	for (index = 0; password_screen.text[index] && index < PASSWORD_LENGTH; index++)
		stars[index] = '*';
	stars[index] = 0;
	text_field_show(named(list, "password_value", 0), stars, text_field_editing(named(list, "op_password", 0)));
	if (help)
		help->parameters.text_box.string_list_index = password_screen.help;
}

/* "port password join" (JOIN GAME, and Enter: password_screen_edit) */
static boolean password_screen_join(struct widget_instance *widget, boolean *widget_deleted)
{
	struct p2p_listing game = lobby_browser.password_game;
	struct widget_instance *row = named(screen_of(widget), "op_password", 0);

	if (text_field_editing(NULL))
		text_field_end(TRUE);
	if (!p2p_listing_unlock(&game, password_screen.text))
	{
		password_screen.help = _password_help_wrong;
		password_screen.text[0] = 0;
		if (row)
			text_field_begin_masked(row, "", PASSWORD_LENGTH, password_screen_done);
		return campaign_fail();
	}
	if (!lobby_browser_join(&game, lobby_browser.controller))
	{
		password_screen.help = _password_help_failed;
		return campaign_fail();
	}
	password_screen.text[0] = 0;
	lobby_browser.password_joined = TRUE;
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;
	return TRUE;
}

/* "port password back" (B, Escape): back to the browser at once, though the
password is being typed */
static boolean password_screen_back(struct widget_instance *widget, boolean *widget_deleted)
{
	if (text_field_editing(NULL))
		text_field_end(FALSE);
	password_screen.text[0] = 0;
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;
	return TRUE;
}

/* "port password edit" (the field's row's A: the typing begins; Enter, its
end, joins) */
static boolean password_screen_edit(struct widget_instance *row, boolean *widget_deleted)
{
	if (text_field_editing(row))
		return password_screen_join(row, widget_deleted);
	text_field_begin_masked(row, password_screen.text, PASSWORD_LENGTH, password_screen_done);
	return TRUE;
}

/* "gamespy screen update": the games' rows (name, map, gametype, players),
the one chosen's line, the counts over the titles (the PC version's: its
players, page, servers) */
static void browser_update(struct widget_instance *list)
{
	struct widget_instance *row;
	struct widget_instance *stats = list->parameters.list.extended_description;
	short focused = browser_row_index(list->focused_child);

	if (multiplayer.mode == _multiplayer_mode_server_browser)
	{
		lobby_browser_update(list);
		return;
	}
	browser_games_read();
	if (focused != NONE && focused < multiplayer.game_count && !ui_widget_port_hover_focused())
		multiplayer.game_chosen = focused;
	for (row = list->child; row; row = row->next)
	{
		short index = browser_row_index(row);
		struct advertised_game *game;
		wchar_t text[ROW_TEXT_LENGTH];
		int ping;

		if (index == NONE)
			continue;
		row->visible = index < multiplayer.game_count;
		if (index >= multiplayer.game_count)
			continue;
		game = multiplayer.games[index];
		ustrncpy(text, game->game_name, NUMBEROF(game->game_name) - 1);
		text[NUMBEROF(game->game_name) - 1] = 0;
		text_set(named(row, "server_item_server_name", 0), text);
		game_map_name(game, text);
		text_set(named(row, "server_item_map", 0), text);
		text_set(named(row, "server_item_type", 0),
			game_kind_name(game->engine_type, advertised_game_kind(game), TRUE));
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%d/%d", game->player_count, game->maximum_player_count);
		text_set(named(row, "server_item_players", 0), text);
		/* (a Direct Link game's host: the tunnel's ping) */
		ping = multiplayer.mode == _multiplayer_mode_direct_link ? p2p_peer_ping(game->xnaddr + 2) : -1;
		browser_row_pings[index] = (short)ping;
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%d", ping);
		text_set(named(row, "server_item_ping", 0), ping >= 0 ? text : advertised_in_progress(game) ? L"LIVE" : L"");
		visible_set(named(row, "server_item_locked", 0), FALSE);
		visible_set(named(row, "server_item_dedicated", 0), FALSE);
		visible_set(named(row, "server_item_classic", 0), FALSE);
	}
	{
		wchar_t text[ROW_TEXT_LENGTH * 2];

		if (multiplayer.mode == _multiplayer_mode_server_browser)
			text[0] = 0;
		else if (!multiplayer.game_count)
		{
			usnprintf(text, NUMBEROF(text) - 1, L"%s", multiplayer.mode == _multiplayer_mode_direct_link ?
				L"Copy an invite link and PASTE LINK, or accept a Discord invite" :
				L"Looking for games on your LAN...");
		}
		else
		{
			struct advertised_game *game = multiplayer.games[multiplayer.game_chosen];

			usnprintf(text, NUMBEROF(text) - 1, L"%d players of %d, on %d machines%s", game->player_count,
				game->maximum_player_count, game->machine_count, advertised_in_progress(game) ? L": under way" : L"");
		}
		text[NUMBEROF(text) - 1] = 0;
		text_set(named(list, "ticker_player_info", 0), text);
		text_set(named(list, "ticker_rules_info", 0), L"");
	}
	if (stats)
	{
		wchar_t label[ROW_TEXT_LENGTH], text[ROW_TEXT_LENGTH];
		long players = 0;
		short index;

		for (index = 0; index < multiplayer.game_count; index++)
			players += multiplayer.games[index]->player_count;
		string_get(JOIN_GAME_LABELS, _join_game_label_players, label);
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s %ld", label, players);
		text_set(named(stats, "player_count_label", 0), text);
		/* (one page: the list holds every game found) */
		string_get(JOIN_GAME_LABELS, _join_game_label_page, label);
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s 1/1", label);
		text_set(named(stats, "page_count_label", 0), text);
		string_get(JOIN_GAME_LABELS, _join_game_label_servers, label);
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s %d", label, multiplayer.game_count);
		text_set(named(stats, "server_count_label", 0), text);
	}
	browser_focus(list, browser_chosen_row());
}

/* "direct ip connect go" (Direct Link's PASTE LINK): the invite
link on the clipboard reached, its game then in the list */
static boolean direct_link_from_clipboard(void)
{
	char text[TEXT_FIELD_LENGTH];
	char *link = text;
	size_t length;

	if (!platform_clipboard_get(text, sizeof(text)))
		text[0] = 0;
	while (*link == ' ' || *link == '\t' || *link == '\r' || *link == '\n')
		link++;
	for (length = strlen(link); length && (link[length - 1] == ' ' || link[length - 1] == '\t' ||
		link[length - 1] == '\r' || link[length - 1] == '\n'); length--)
		link[length - 1] = 0;
	if (!*link || !p2p_join_invite(link))
	{
		platform_log("menus: the clipboard has no invite link");
		return campaign_fail();
	}
	return TRUE;
}

/* joining the game chosen: player 1 in, then the lobby */
static boolean browser_join(struct widget_instance *widget, short controller, boolean *widget_deleted)
{
	if (multiplayer.game_chosen >= multiplayer.game_count)
		return campaign_fail();
	if (!multiplayer_player(controller))
		return FALSE;
	return ui_widget_port_join(widget, multiplayer.games[multiplayer.game_chosen], LOBBY_NAME, widget_deleted);
}

/* "gamespy select item" (a row: chooses its game, or joins it once chosen)
and "gamespy select button" (Join) */
static boolean browser_select(struct widget_instance *widget, struct event_record *event, short controller,
	boolean *widget_deleted)
{
	short row = browser_row_index(widget);

	/* (a row not chosen, clicked after the pointer passed over it: chosen,
	as Custom Edition's; a click on it chosen, a double click, A or JOIN
	joins its game. Not the A press a join posts: lobby_browser_select) */
	if (row != NONE && row != browser_chosen_row() && !lobby_browser.ready)
	{
		if (multiplayer.mode == _multiplayer_mode_server_browser)
			lobby_browser.chosen = (short)(lobby_browser.first + row);
		else
			multiplayer.game_chosen = row;
		ui_play_audio_feedback_sound(SOUND_CURSOR);
		return TRUE;
	}
	if (multiplayer.mode == _multiplayer_mode_server_browser)
		return lobby_browser_select(widget, controller, row, widget_deleted);
	if (row != NONE)
		multiplayer.game_chosen = row;
	if (row != NONE || strstr(widget->name, "button_join"))
	{
		/* (a game under way: its lobby first, then JOIN GAME) */
		if (multiplayer.game_chosen < multiplayer.game_count &&
			advertised_in_progress(multiplayer.games[multiplayer.game_chosen]))
		{
			/* (player 1 joining it: others join them there) */
			if (!multiplayer_player(controller))
				return FALSE;
			csmemcpy(multiplayer.preview_key_id, multiplayer.games[multiplayer.game_chosen]->key_id,
				sizeof(multiplayer.preview_key_id));
			csmemcpy(multiplayer.preview_xnaddr, multiplayer.games[multiplayer.game_chosen]->xnaddr,
				sizeof(multiplayer.preview_xnaddr));
			return ui_widget_port_open(widget, PREVIEW_NAME, widget_deleted);
		}
		return browser_join(widget, controller, widget_deleted);
	}
	return TRUE;
}

/* "player profile save changes" (Settings' OK, the profile being edited):
saved if it has changes, as the Xbox's (the saving screen follows); if it
has none (the settings' own screens write theirs to config.toml as their OK
is chosen), editing ends and the previous screen comes back, as CANCEL
(the Xbox's called that a failure, and closed every screen) */
static boolean profile_save_changes(struct widget_instance *widget, boolean *widget_deleted)
{
	if (player_ui_edit_profile_is_dirty())
	{
		if (player_ui_save_profile())
			return TRUE;
		platform_log("menus: could not save the profile's changes");
		return campaign_fail();
	}
	player_ui_end_editing_profile();
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	ui_widget_port_go_back(widget);
	*widget_deleted = TRUE;
	return TRUE;
}

/* "port profile settings save" (Gamepads' OK in a single-player campaign:
menu_tags.c's pause_settings_patch): the profile saved at once, not on
Settings' OK, so that the campaign's next save of the player's profile
keeps it; saving makes it the player's own (player_ui_save_profile), and it
is edited again from what was saved, for Settings to go on with */
static boolean profile_settings_save(struct widget_instance *widget)
{
	long index = player_ui_get_edit_profile_index();

	settings_each(screen_of(widget), setting_changed_save);
	if (!player_ui_get_edit_player_profile() || !player_ui_edit_profile_is_dirty())
		return TRUE;
	if (!player_ui_save_profile())
	{
		platform_log("menus: could not save the profile's changes");
		return campaign_fail();
	}
	player_ui_begin_editing_profile(index);
	return TRUE;
}

/* "port pause end game" (the in-game pause menu's END GAME, the host's:
menu_tags.c's pause_patch): the game ends as its time limit would, its
players staying for the next (the carnage report, then the host's PICK GAME) */
static boolean pause_end_game(void)
{
	if (!global_network_game_server_get() || !game_engine_running())
		return campaign_fail();
	game_engine_end_game();
	return TRUE;
}

/* "port coop friendly fire" (network co-op's pause menu's FRIENDLY FIRE,
menu_tags.c): switches this machine's own friendly fire, from now on */
static boolean coop_friendly_fire_change(void)
{
	char const *choice = config_boolean("network.coop_own_friendly_fire") ? "false" : "true";

	if (!config_write("network.coop_own_friendly_fire", choice))
		platform_log("menus: could not write network.coop_own_friendly_fire to config.toml");
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

/* "port coop friendly fire text": the button's text, ON or OFF */
static void coop_friendly_fire_text(struct widget_instance *button)
{
	button->parameters.text_box.string_list_index = config_boolean("network.coop_own_friendly_fire") ? 0 : 1;
}

/* ---- the lobby: the game's players (up to the port's 128), its map and
gametype, the countdown */

static struct network_player *lobby_players[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static short lobby_player_count;

/* split screen: up to 4 players on this machine. In the lobby, a controller
not playing presses START to join ("port lobby join"), then chooses a
profile (its screen, "port lobby player choose"); ADD PLAYER ("port lobby
add player") gives one gamepad its own controller first (as it shares
player 1's: pc_menu_split_players). A player's B leaves the game alone ("port
lobby leave"; the machine's last leaves it). In game, a player's pause menu's
QUIT is theirs (the Xbox's "mp game player quit") */
static struct
{
	/* ADD PLAYER chosen: one gamepad is the new player's */
	boolean adding;
	/* the controller choosing its profile, NONE if none */
	short controller;
} lobby_join = { FALSE, NONE };

/* the client's player of the controller on this machine, NULL if none */
static struct network_player *lobby_local_player(short controller)
{
	void *client = global_network_game_client_get();
	struct network_game *game = client ? network_game_client_get_game(client) : NULL;
	short machine_index = network_game_client_get_local_machine_index();
	short index;

	for (index = 0; game && machine_index != NONE && index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; index++)
	{
		struct network_player *player = &game->players[index];

		if (network_player_is_valid(player) && (short)player->machine_index == machine_index &&
			(short)player->controller_index == controller)
		{
			return player;
		}
	}
	return NULL;
}

/* whether the controller plays (or is to: joined, not yet added) */
static boolean lobby_controller_playing(short controller)
{
	return player_ui_local_player_wants_to_play_multiplayer(controller) || lobby_local_player(controller);
}

/* this machine's players in the network game (or to be) */
static short lobby_local_player_count(void)
{
	short controller, count = 0;

	if (!global_network_game_client_get())
		return 0;
	for (controller = 0; controller < MAXIMUM_LOCAL_PLAYERS; controller++)
		count += lobby_controller_playing(controller) ? 1 : 0;
	return count;
}

/* (xinput_sdl.c) whether this machine has, or is adding, a second player: co-op,
or split screen in a network game (a player who quit keeps their part of the
screen until the game ends). Then one gamepad is its own controller, not
sharing player 1's with the keyboard */
unsigned char pc_menu_split_players(void)
{
	return player_spawn_count >= 2 || lobby_join.adding || lobby_join.controller != NONE ||
		lobby_local_player_count() >= 2 || players_port_local_player_count() >= 2;
}

/* the lobby's widget that has the focus (the one a press goes to) */
static struct widget_instance *focused_leaf(struct widget_instance *widget)
{
	while (widget->parent)
		widget = widget->parent;
	while (widget->focused_child)
		widget = widget->focused_child;
	return widget;
}

/* "port lobby open" (the lobby made, or come back to): no player being added */
static boolean lobby_join_reset(void)
{
	lobby_join.adding = FALSE;
	lobby_join.controller = NONE;
	return TRUE;
}

/* "port lobby add player" (ADD PLAYER): the next START of another controller
joins (one gamepad leaves the keyboard's controller for its own) */
static boolean lobby_add_player(void)
{
	if (lobby_local_player_count() >= MAXIMUM_LOCAL_PLAYERS)
		return campaign_fail();
	lobby_join.adding = TRUE;
	return TRUE;
}

/* "port lobby join" (START): a controller not playing joins, choosing its
profile next (FALSE: no profile screen); a player's START is the focused
button's */
static boolean lobby_join_start(struct widget_instance *widget, short controller, boolean *widget_deleted)
{
	void *client = global_network_game_client_get();
	struct network_game *game = client ? network_game_client_get_game(client) : NULL;
	short state_data;

	if (lobby_controller_playing(controller))
	{
		ui_widget_port_dispatch_event(focused_leaf(widget), BUTTON_START, controller, widget_deleted);
		return FALSE;
	}
	if (!game || network_game_client_get_state(client, &state_data) != _client_state_pregame)
		return campaign_fail();
	if (lobby_player_count >= game->maximum_players)
	{
		display_error_text_deferred(L"The game is full.", NONE);
		return campaign_fail();
	}
	lobby_join.controller = controller;
	return TRUE;
}

/* "port lobby leave" (B): a player leaves the game, the machine's last
leaving it (TRUE: back from the lobby); a controller not playing cancels
ADD PLAYER */
static boolean lobby_leave(struct widget_instance *widget, struct event_record *event, short controller,
	boolean *widget_deleted)
{
	if (!lobby_controller_playing(controller))
	{
		lobby_join_reset();
		return FALSE;
	}
	return ui_widget_port_unjoin_player(widget, event, widget_deleted);
}

/* "port lobby player list initialize": on the joining controller's profile,
else the first no other player of this machine has */
static boolean lobby_player_list_initialize(struct widget_instance *list)
{
	long mine = lobby_join.controller != NONE ? player_ui_get_active_player_profile_index(lobby_join.controller) : NONE;
	short index, controller;

	profile_list.step = PROFILE_STEP_SELECT;
	profile_list_read(TRUE);
	profile_list.chosen = 0;
	for (index = profile_list.count - 1; index >= 0; index--)
	{
		boolean taken = FALSE;

		for (controller = 0; controller < MAXIMUM_LOCAL_PLAYERS; controller++)
		{
			taken |= controller != lobby_join.controller && lobby_controller_playing(controller) &&
				player_ui_get_active_player_profile_index(controller) == profile_list.indices[index];
		}
		if (!taken)
			profile_list.chosen = index;
	}
	for (index = 0; index < profile_list.count && mine != NONE; index++)
	{
		if (profile_list.indices[index] == mine)
			profile_list.chosen = index;
	}
	profile_list.first = (short)PIN(profile_list.chosen - PROFILE_ROWS / 2, 0,
		MAX(0, profile_list.count + 1 - PROFILE_ROWS));
	focus_row(list, (short)(profile_list.chosen - profile_list.first));
	return TRUE;
}

/* "port lobby player choose": the joining controller's player on the profile
chosen (the lobby's "net splitscreen prejoin players" adds them) */
static boolean lobby_player_choose(void)
{
	short controller = lobby_join.controller;

	if (controller == NONE || profile_list.chosen >= profile_list.count ||
		!ui_widget_port_multiplayer_player(controller, profile_list.indices[profile_list.chosen]))
	{
		return campaign_fail();
	}
	return TRUE;
}

/* the lobby's line under its players: how another player joins */
static void lobby_join_help(struct widget_instance *list)
{
	struct widget_instance *help = named(screen_of(list), "lobby_join_help", 0);
	wchar_t const *text = L"";
	short controller;

	if (lobby_join.adding)
	{
		text = L"New player: press START.";
	}
	else if (lobby_local_player_count() < MAXIMUM_LOCAL_PLAYERS)
	{
		for (controller = 0; controller < MAXIMUM_LOCAL_PLAYERS; controller++)
		{
			if (input_has_gamepad(controller) && !lobby_controller_playing(controller))
				text = L"Another controller: START joins.";
		}
	}
	text_set(help, text);
}

static void lobby_row_text(short row, wchar_t *text)
{
	struct network_player *player = lobby_players[multiplayer.lobby_first + row];
	struct network_game *game = network_game_client_get_game(global_network_game_client_get());
	wchar_t name[NUMBEROF(player->name) + 1];

	ustrncpy(name, player->name, NUMBEROF(player->name));
	name[NUMBEROF(player->name)] = 0;
	if (game && game->variant.universal_variant.teams)
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s  (%s)", name, player->team_index ? L"BLUE" : L"RED");
	else
		usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s", name);
	/* (this machine's players, when it has more than one: their controllers) */
	if (lobby_local_player_count() >= 2 && (short)player->machine_index == network_game_client_get_local_machine_index())
	{
		size_t length = ustrlen(text);

		usnprintf(text + length, ROW_TEXT_LENGTH - 1 - length, L"  [P%d]", player->controller_index + 1);
	}
	text[ROW_TEXT_LENGTH - 1] = 0;
}

/* the lobby's panel's map: its picture and name */
static void lobby_map_show(struct widget_instance *description, char const *map_name)
{
	char const *const *names;
	short count = xbox_multiplayer_map_count(ui_widget_port_multiplayer_maps(&names, NULL)), map = 19, index;
	short level = campaign_level_of(map_name);
	struct widget_instance *widget;

	for (index = 0; index < count; index++)
	{
		if (!_stricmp(names[index], map_name))
			map = index;
	}
	/* (a Custom Edition map's picture and name, multiplayer or campaign, by
	its display index, if this machine has it: custom_edition_maps.c) */
	if (custom_edition_maps_display_index(map_name) != NONE &&
		(custom_edition_level_name(map_name) ||
		 (map_name && _strnicmp(map_name, CUSTOM_EDITION_LEVEL_NAME_PREFIX, csstrlen(CUSTOM_EDITION_LEVEL_NAME_PREFIX)) == 0)))
		map = custom_edition_maps_display_index(map_name);
	/* (a network co-op game's level: its picture and name, sp_levels' and
	map_list's, in the place of the map's) */
	visible_set(named(description, "lobby_map_pic", 0), level == NONE);
	visible_set(named(description, "lobby_map_name", 0), level == NONE);
	visible_set(named(description, "replay_level_right_pic", 0), level != NONE);
	visible_set(named(description, "replay_level_right_name", 0), level != NONE);
	if ((widget = named(description, "lobby_map_pic", 0)) != NULL)
		widget->animation.current_frame_index = map;
	if ((widget = named(description, "lobby_map_name", 0)) != NULL)
		widget->parameters.text_box.string_list_index = map;
	if (level == NONE)
		return;
	if ((widget = named(description, "replay_level_right_pic", 0)) != NULL)
		widget->animation.current_frame_index = level;
	if ((widget = named(description, "replay_level_right_name", 0)) != NULL)
		widget->parameters.text_box.string_list_index = level;
}

/* the lobby's rows' texts as last updated, and their players' machines,
for their speaker icons (menu_functions_text_box_drawn); and when */
static struct
{
	struct widget_instance *texts[LOBBY_ROWS];
	long machines[LOBBY_ROWS];
	short count;
	unsigned long updated_at;
} lobby_icons;

/* ui_widget.c's: a text box drawn, at the bounds its text was drawn in
(its font and justification still set): a lobby row's player talking in
voice chat (or muted) has a speaker just left of their name
(port/linux/game/network_voice.c) */
void menu_functions_text_box_drawn(struct widget_instance *widget, rectangle2d const *bounds)
{
	short row;

	/* (the Mods screen's texts: the fonts and widths their lines are
	measured in, mods_text_fits) */
	for (row = 0; widget->name && row < NUMBER_OF_MOD_TEXT_BOXES; row++)
	{
		if (!strcmp(widget->name, mod_text_box_names[row]))
		{
			mods_screen.fonts[row] = draw_string_get_font();
			mods_screen.widths[row] = (short)(bounds->x1 - bounds->x0);
			return;
		}
	}
	for (row = 0; widget->name && row < NUMBER_OF_TAG_TEXT_BOXES; row++)
	{
		if (!strcmp(widget->name, tag_text_box_names[row]))
		{
			tags_screen.fonts[row] = draw_string_get_font();
			tags_screen.widths[row] = (short)(bounds->x1 - bounds->x0);
			return;
		}
	}
	for (row = 0; widget->name && row < NUMBER_OF_THEATER_TEXT_BOXES; row++)
	{
		if (!strcmp(widget->name, theater_text_box_names[row]))
		{
			theater.fonts[row] = draw_string_get_font();
			theater.widths[row] = (short)(bounds->x1 - bounds->x0);
			return;
		}
	}

	/* (as the lobby last updated them: the rows' widgets go with it) */
	if (!lobby_icons.count || system_milliseconds() - lobby_icons.updated_at > 250)
		return;
	for (row = 0; row < lobby_icons.count; row++)
	{
		long machine_index = lobby_icons.machines[row];
		rectangle2d icon;
		short size;

		if (lobby_icons.texts[row] != widget)
			continue;
		if (!network_voice_machine_speaking(machine_index) && !network_voice_machine_muted(machine_index))
			return;
		size = (short)MIN(14, bounds->y1 - bounds->y0);
		icon.x1 = (short)(bounds->x1 - 4);
		icon.y0 = (short)((bounds->y0 + bounds->y1 - size) / 2);
		/* (the name's ink: the icon just left of it, centred on its
		capitals) */
		if (widget->parameters.text_box.text && widget->parameters.text_box.text[0])
		{
			rectangle2d text;
			rectangle2d cursor;

			draw_unicode_string_compute_bounds(bounds, widget->parameters.text_box.text, &text, &cursor);
			if (text.x0 - size - 6 >= bounds->x0)
				icon.x1 = (short)(text.x0 - 6);
			icon.y0 = (short)(draw_unicode_string_capital_middle(bounds, widget->parameters.text_box.text) - size / 2);
		}
		icon.x0 = (short)(icon.x1 - size);
		icon.y1 = (short)(icon.y0 + size);
		network_voice_draw_icon(&icon, network_voice_machine_muted(machine_index), 1.0f);
		return;
	}
}

/* "port lobby update" */
static void lobby_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	void *client = global_network_game_client_get();
	struct network_game *game = client ? network_game_client_get_game(client) : NULL;
	short state_data, state = client ? network_game_client_get_state(client, &state_data) : NONE;
	wchar_t text[LOBBY_TEXT_LENGTH];
	short index;

	lobby_player_count = 0;
	for (index = 0; game && state >= _client_state_pregame && index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; index++)
	{
		if (network_player_is_valid(&game->players[index]))
			lobby_players[lobby_player_count++] = &game->players[index];
	}
	list_scroll(list, &multiplayer.lobby_first, lobby_player_count, LOBBY_ROWS);
	if (multiplayer.lobby_first > MAX(0, lobby_player_count - LOBBY_ROWS))
		multiplayer.lobby_first = (short)MAX(0, lobby_player_count - LOBBY_ROWS);
	rows_update(list, (short)MIN(lobby_player_count, LOBBY_ROWS), lobby_row_text);
	/* (the rows' players, for their speaker icons) */
	{
		struct widget_instance *row = list->child;

		lobby_icons.count = 0;
		for (index = 0; row && index < MIN(lobby_player_count, LOBBY_ROWS); row = row->next, index++)
		{
			if (strncmp(row->name, "list_item_", 10))
				break;
			lobby_icons.texts[index] = named(row, "list_item_text", 0);
			lobby_icons.machines[index] = lobby_players[multiplayer.lobby_first + index]->machine_index;
			lobby_icons.count = (short)(index + 1);
		}
		lobby_icons.updated_at = system_milliseconds();
	}
	lobby_join_help(list);
	visible_set(named(list, "lobby_button_team", 0), game && game->variant.universal_variant.teams);
	/* (the buttons' focus, off Switch Team when it is hidden) */
	focus_off_hidden(named(list, "lobby_button_bar", 0));
	visible_set(named(description, "lobby_right_item", 0), game != NULL && state >= _client_state_pregame);
	if (!game || state < _client_state_pregame)
	{
		profile_name_show(description);
		return;
	}
	lobby_map_show(description, game->map.name);
	{
		short seconds = network_game_client_get_seconds_to_game_start(client);
		char link[TEXT_FIELD_LENGTH];
		wchar_t gametype[NUMBEROF(game->variant.human_readable_game_description) + 1];
		/* (an OpenCE gametype's kind: game_modes.c) */
		wchar_t const *kind = game_modes_variant_kind_name(&game->variant, &game->variant_options);

		ustrncpy(gametype, game->variant.human_readable_game_description, NUMBEROF(gametype) - 1);
		gametype[NUMBEROF(gametype) - 1] = 0;
		usnprintf(text, NUMBEROF(text) - 1, L"%s\r\n%s\r\n%d of %d players\r\n\r\n%s", gametype,
			kind ? kind : engine_names[PIN(game->variant.game_engine_index, 0, 6)], lobby_player_count,
			game->maximum_players,
			seconds > 0 ? L"Starting in:" : game->machine_count < 2 ? L"Waiting for players" : L"");
		text[NUMBEROF(text) - 1] = 0;
		if (seconds > 0)
		{
			size_t length = ustrlen(text);

			usnprintf(text + length, NUMBEROF(text) - 1 - length, L" %d", seconds);
		}
		if (game->variant.game_engine_index == game_engine_firefight)
		{
			struct firefight_map_record rec;
			if (firefight_stats_get_map_record(game->map.name, NONE, &rec) &&
				(rec.best_score > 0 || rec.best_set > 0))
			{
				size_t length = ustrlen(text);
				usnprintf(text + length, NUMBEROF(text) - 1 - length,
					L"\r\nBest: %ld pts (S%d R%d W%d)",
					rec.best_score, (int)rec.best_set, (int)rec.best_round, (int)rec.best_wave);
			}
		}
		if (global_network_game_server_get() && p2p_invite_link(link, sizeof(link)))
		{
			size_t length = ustrlen(text);

			usnprintf(text + length, NUMBEROF(text) - 1 - length, L"\r\n\r\nInvite link copied:\r\npaste it to friends");
		}
		text_set_length(named(description, "lobby_game_data", 0), text, LOBBY_TEXT_LENGTH);
	}
	profile_name_show(description);
}

/* ---- an in-progress game's lobby, before joining it (Direct Link and
LAN's rows of games under way): what its advertisement tells (no players'
names: they come with joining), JOIN GAME. Split screen players join here
as in the lobby (lobby_join), for the game starts at once for a machine
that joins it: player 1 as it opens, the others with START */

/* the previewed game, found again among the client's (NULL: gone) */
static struct advertised_game *preview_game(void)
{
	void *client = global_network_game_client_get();
	struct advertised_game *games = client ? network_game_client_get_available_games(client) : NULL;
	short index;

	for (index = 0; games && index < MAXIMUM_ADVERTISED_GAMES; index++)
	{
		if (network_game_client_advertised_game_is_valid(&games[index]) &&
			!csmemcmp(games[index].key_id, multiplayer.preview_key_id, sizeof(multiplayer.preview_key_id)) &&
			!csmemcmp(games[index].xnaddr, multiplayer.preview_xnaddr, sizeof(multiplayer.preview_xnaddr)))
		{
			return &games[index];
		}
	}
	return NULL;
}

/* the preview's status's end: the players joining from here, when there
are more than one (split screen) */
static void preview_players_text(wchar_t *text, short size)
{
	short controller;
	size_t length;

	if (lobby_local_player_count() < 2)
		return;
	length = ustrlen(text);
	usnprintf(text + length, size - 1 - length, L"\r\n\r\nJoining from here:");
	for (controller = 0; controller < MAXIMUM_LOCAL_PLAYERS; controller++)
	{
		struct player_profile profile;
		wchar_t name[NUMBEROF(profile.player_name) + 1];

		if (!player_ui_local_player_wants_to_play_multiplayer(controller))
			continue;
		player_ui_get_active_player_profile(controller, &profile);
		ustrncpy(name, profile.player_name, NUMBEROF(profile.player_name));
		name[NUMBEROF(profile.player_name)] = 0;
		text[size - 1] = 0;
		length = ustrlen(text);
		usnprintf(text + length, size - 1 - length, L"\r\n%s  [P%d]", name, controller + 1);
	}
	text[size - 1] = 0;
}

/* "port lobby preview update" */
static void preview_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	struct advertised_game *game = preview_game();
	boolean valid = game != NULL;
	wchar_t text[LOBBY_TEXT_LENGTH], name[NUMBEROF(game->game_name) + 1];

	visible_set(named(description, "lobby_right_item", 0), valid);
	if (valid)
	{
		lobby_map_show(description, game->map_name);
		usnprintf(text, NUMBEROF(text) - 1, L"%s\r\n%d of %d players\r\non %d machines",
			game_kind_name(game->engine_type, advertised_game_kind(game), FALSE), game->player_count,
			game->maximum_player_count, game->machine_count);
		text[NUMBEROF(text) - 1] = 0;
		text_set_length(named(description, "lobby_game_data", 0), text, LOBBY_TEXT_LENGTH);
		ustrncpy(name, game->game_name, NUMBEROF(game->game_name));
		name[NUMBEROF(game->game_name)] = 0;
		/* (the text box does not wrap: lines of up to 28 characters) */
		usnprintf(text, NUMBEROF(text) - 1, L"%s\r\n\r\nThis game is under way.\r\n\r\n%s", name,
			game->open ? L"JOIN GAME joins it now;\r\nits players show then." :
			L"It cannot be joined now:\r\nit is loading, over or full.");
		text[NUMBEROF(text) - 1] = 0;
		preview_players_text(text, NUMBEROF(text));
	}
	else
		usnprintf(text, NUMBEROF(text) - 1, L"The game is gone.");
	text[NUMBEROF(text) - 1] = 0;
	text_set_length(named(list, "preview_status", 0), text, LOBBY_TEXT_LENGTH);
	lobby_join_help(list);
	profile_name_show(description);
}

/* "port lobby preview add" (START): as the lobby's ("port lobby join"), a
controller not joining chooses its profile, to join with the others; a
joining player's START is the focused button's */
static boolean preview_add(struct widget_instance *widget, short controller, boolean *widget_deleted)
{
	struct advertised_game *game = preview_game();
	short count = lobby_local_player_count();

	if (lobby_controller_playing(controller))
	{
		ui_widget_port_dispatch_event(focused_leaf(widget), BUTTON_START, controller, widget_deleted);
		return FALSE;
	}
	if (!game || !game->open || count >= MAXIMUM_LOCAL_PLAYERS)
		return campaign_fail();
	if (game->player_count + count >= game->maximum_player_count)
	{
		display_error_text_deferred(L"The game is full.", NONE);
		return campaign_fail();
	}
	lobby_join.controller = controller;
	return TRUE;
}

/* "port lobby preview leave" (B): a joining player stays out, the last of
them backing out (TRUE); a controller not joining cancels ADD PLAYER, else
backs out for them all (the game is not joined yet: none is kept here) */
static boolean preview_leave(short controller)
{
	short index;

	if (!lobby_controller_playing(controller))
	{
		if (lobby_join.adding)
		{
			lobby_join_reset();
			return FALSE;
		}
		for (index = 0; index < MAXIMUM_LOCAL_PLAYERS; index++)
			player_ui_local_player_left_multiplayer_game(index);
		return TRUE;
	}
	player_ui_local_player_left_multiplayer_game(controller);
	return lobby_local_player_count() == 0;
}

/* "port lobby preview join": with the players joining from here (player 1,
as the preview opened, and those added), whom the lobby asks the host for
all at once ("net splitscreen prejoin players"): it starts the machine in
the game once it has them all */
static boolean preview_join(struct widget_instance *widget, short controller, boolean *widget_deleted)
{
	struct advertised_game *game = preview_game();

	if (!game || !game->open)
		return campaign_fail();
	if (!lobby_local_player_count() && !multiplayer_player(controller))
		return FALSE;
	return ui_widget_port_join(widget, game, LOBBY_NAME, widget_deleted);
}

/* ---- the gametype editor: Edit Gametypes' list (the built-in gametypes
and those saved), the gametype being edited (player_ui's) or Server Setup's
(a copy of the game's gametype), its options' screens: each option a
spinner, found by its name (the PC version's screens put them in another
order than the Xbox's, whose functions of the same names walk them by
place) */

boolean ui_widget_port_gametype_edit_begin(long profile_index);
boolean ui_widget_port_gametype_save(struct widget_instance *widget, boolean *widget_deleted);
boolean ui_widget_port_game_variant_set(struct game_variant *variant, struct game_variant_options const *options);
boolean ui_widget_port_gametype_delete(long profile_index);

#define GAMETYPE_EDIT_ROWS 11

static struct
{
	/* the list's */
	long gametypes[MAXIMUM_GAMETYPES];
	short count, first, chosen;
	boolean stale;
	long deleting;
	/* Server Setup's copy of the game's gametype, edited in place of
	player_ui's (setup) */
	boolean setup;
	struct game_variant setup_variant;
	struct game_variant_options setup_options;
	/* the vehicle screen's side shown */
	short vehicle_side;
} gametype_edit = { { 0 }, 0, 0, 0, TRUE, NONE, FALSE };

static struct game_variant *edit_variant(void)
{
	return gametype_edit.setup ? &gametype_edit.setup_variant : player_ui_get_edit_playlist_profile();
}

static struct game_variant_options *edit_options(void)
{
	return gametype_edit.setup ? &gametype_edit.setup_options : player_ui_get_edit_playlist_options();
}

enum
{
	_option_long,		/* a long of the variant */
	_option_byte,		/* a byte of the variant */
	_option_flag,		/* a bit of the variant's flags (argument), set by value 1 */
	_option_health,		/* the variant's health, tenths */
	_option_short,		/* a short of the options */
	_option_option_byte,	/* a byte of the options */
	_option_radar,		/* the options' radar players, and the variant's flag */
	_option_setting,	/* the host's own (config.toml): Server Setup's copy only */
	_option_variant_short,	/* a short of the variant */
	_option_variant_u8,	/* an unsigned byte value of the variant */
	_option_unit_mask,	/* bit(s) in options->firefight_unit_toggles (1 = enabled, 0 = disabled) */
	_option_option_ulong	/* an unsigned long of the options */
};

struct gametype_option
{
	char const *spinner;
	short kind;
	short offset;
	unsigned long argument;
	short count;
	long values[16];
	/* (_option_setting: the setting, and its values' text) */
	char const *setting;
	char const *setting_values[16];
};

#define VARIANT_FIELD(field) (short)offsetof(struct game_variant, field)
#define OPTIONS_FIELD(field) (short)offsetof(struct game_variant_options, field)
#define TIME_LIMITS { 0, 10, 15, 20, 25, 30, 45 }

static struct gametype_option const gametype_options[] =
{
	/* OPENCE OPTIONS (OpenCE's gametypes: game_modes.c) */
	{ "oce_mode_spinner", _option_option_byte, OPTIONS_FIELD(mode), 0, 4,
		{ _game_mode_none, _game_mode_swat, _game_mode_infection, _game_mode_gun_game } },
	{ "oce_hardcore_spinner", _option_option_byte, OPTIONS_FIELD(hardcore), 0, 2, { FALSE, TRUE } },
	{ "inf_zombies_spinner", _option_option_byte, OPTIONS_FIELD(infection_initial_zombies), 0, 6, { 1, 2, 3, 4, 8, 16 } },
	{ "inf_speed_spinner", _option_option_byte, OPTIONS_FIELD(infection_zombie_speed), 0, 6,
		{ 100, 110, 125, 150, 175, 200 } },
	{ "inf_damage_spinner", _option_option_byte, OPTIONS_FIELD(infection_skull_damage), 0, 4, { 100, 150, 200, 250 } },
	{ "inf_respawn_spinner", _option_option_byte, OPTIONS_FIELD(infection_respawn_time), 0, 6, { 1, 3, 5, 10, 15, 30 } },
	{ "inf_countdown_spinner", _option_option_byte, OPTIONS_FIELD(infection_countdown), 0, 5, { 0, 5, 10, 15, 30 } },
	{ "inf_shields_spinner", _option_option_byte, OPTIONS_FIELD(infection_zombie_shields), 0, 2, { FALSE, TRUE } },
	{ "gg_order_spinner", _option_option_byte, OPTIONS_FIELD(gun_game_order), 0, 3,
		{ _gun_game_order_strongest_first, _gun_game_order_weakest_first, _gun_game_order_random } },
	{ "gg_melee_spinner", _option_option_byte, OPTIONS_FIELD(gun_game_melee_demotes), 0, 2, { FALSE, TRUE } },
	/* player options */
	{ "number_of_lives_spinner", _option_long, VARIANT_FIELD(universal_variant.lives), 0, 4, { 0, 1, 3, 5 } },
	{ "maximum_health_spinner", _option_health, 0, 0, 6, { 5, 10, 15, 20, 30, 40 } },
	{ "shields_spinner", _option_flag, 0, FLAG(_game_variant_no_shields_bit), 2, { 0, 1 } },
	{ "respawn_time_spinner", _option_long, VARIANT_FIELD(universal_variant.respawn_time), 0, 4, { 0, 150, 300, 450 } },
	{ "respawn_time_growth_spinner", _option_long, VARIANT_FIELD(universal_variant.respawn_time_growth), 0, 4,
		{ 0, 150, 300, 450 } },
	{ "odd_man_out_spinner", _option_byte, VARIANT_FIELD(universal_variant.odd_man_out), 0, 2, { 1, 0 } },
	{ "invisible_players_spinner", _option_flag, 0, FLAG(_game_variant_always_invisible_bit), 2, { 1, 0 } },
	{ "suicide_penalty_spinner", _option_long, VARIANT_FIELD(universal_variant.suicide_penalty), 0, 4,
		{ 0, 150, 300, 450 } },
	/* item options (weapon sets: the PC's list, then the Xbox's NO GRENADES) */
	{ "item_options_infinite_grenades_spinner", _option_flag, 0, FLAG(_game_variant_infinite_grenades_bit), 2,
		{ 1, 0 } },
	{ "item_options_weapon_set_spinner", _option_long, VARIANT_FIELD(universal_variant.weapon_set), 0, 14,
		{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 10 } },
	{ "item_options_starting_equipment_spinner", _option_flag, 0, FLAG(_game_variant_generic_starting_equipment_bit),
		2, { 0, 1 } },
	{ "map_weapons_spinner", _option_option_byte, OPTIONS_FIELD(no_map_weapons), 0, 2, { 0, 1 } },
	/* (the loadout: the weapon set's, or each player's two weapons) */
	{ "loadout_spinner", _option_option_byte, OPTIONS_FIELD(loadout), 0, 2, { _loadout_category, _loadout_custom } },
	{ "primary_weapon_spinner", _option_option_byte, OPTIONS_FIELD(primary_weapon), 0, NUMBER_OF_LOADOUT_WEAPONS,
		{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 } },
	{ "secondary_weapon_spinner", _option_option_byte, OPTIONS_FIELD(secondary_weapon), 0, NUMBER_OF_LOADOUT_WEAPONS,
		{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 } },
	/* indicator options */
	{ "indicator_options_radar display_spinner", _option_long, VARIANT_FIELD(universal_variant.goal_radar), 0, 3,
		{ 0, 1, 2 } },
	{ "indicator_options_players_on_radar_spinner", _option_radar, OPTIONS_FIELD(radar_players), 0, 3,
		{ _radar_players_all, _radar_players_friends, _radar_players_none } },
	{ "indicator_options_friends_on_screen_spinner", _option_flag, 0, FLAG(_game_variant_allow_friendly_navpoints_bit),
		2, { 1, 0 } },
	/* capture the flag */
	{ "assault_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.ctf.assault), 0, 2, { 1, 0 } },
	{ "single_flag_spinner", _option_long, VARIANT_FIELD(game_engine_variant.ctf.single_flag_time), 0, 6,
		{ 0, 1800, 3600, 5400, 9000, 18000 } },
	{ "flag_must_reset_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.ctf.flag_must_reset), 0, 2,
		{ 1, 0 } },
	{ "flag_at_home_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.ctf.flag_at_home_to_score), 0, 2,
		{ 1, 0 } },
	{ "captures_to_win_spinner", _option_long, VARIANT_FIELD(universal_variant.score_to_win), 0, 5,
		{ 1, 3, 5, 10, 15 } },
	{ "time_limit_spinner", _option_short, OPTIONS_FIELD(time_limit), 0, 7, TIME_LIMITS },
	/* king of the hill */
	{ "koth_moving_hill_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.king.moving_hill), 0, 2, { 1, 0 } },
	{ "koth_score_to_win_spinner", _option_long, VARIANT_FIELD(universal_variant.score_to_win), 0, 5,
		{ 1, 2, 5, 10, 15 } },
	{ "koth_team_play_spinner", _option_byte, VARIANT_FIELD(universal_variant.teams), 0, 2, { 1, 0 } },
	/* oddball */
	{ "trait_with_ball_spinner", _option_long, VARIANT_FIELD(game_engine_variant.oddball.trait_with_ball), 0, 4,
		{ 0, 1, 2, 3 } },
	{ "trait_without_ball_spinner", _option_long, VARIANT_FIELD(game_engine_variant.oddball.trait_without_ball), 0, 4,
		{ 0, 1, 2, 3 } },
	{ "speed_with_ball_spinner", _option_long, VARIANT_FIELD(game_engine_variant.oddball.speed_with_ball), 0, 3,
		{ 1, 0, 2 } },
	{ "ball_type_spinner", _option_long, VARIANT_FIELD(game_engine_variant.oddball.oddball_ball_type), 0, 3,
		{ 0, 1, 2 } },
	{ "random_start_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.oddball.random_start), 0, 2, { 1, 0 } },
	{ "ball_spawn_count_spinner", _option_long, VARIANT_FIELD(game_engine_variant.oddball.ball_spawn_count), 0, 16,
		{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 } },
	{ "score_to_win_spinner", _option_long, VARIANT_FIELD(universal_variant.score_to_win), 0, 5, { 1, 2, 5, 10, 15 } },
	/* (Slayer's, Oddball's and Race's) */
	{ "team_play_spinner", _option_byte, VARIANT_FIELD(universal_variant.teams), 0, 2, { 1, 0 } },
	/* race */
	{ "team_scoring_spinner", _option_long, VARIANT_FIELD(game_engine_variant.race.team_scoring), 0, 3, { 0, 1, 2 } },
	{ "race_type_spinner", _option_long, VARIANT_FIELD(game_engine_variant.race.race_type), 0, 3, { 0, 1, 2 } },
	{ "laps_to_win_spinner", _option_long, VARIANT_FIELD(universal_variant.score_to_win), 0, 6,
		{ 1, 3, 5, 10, 15, 25 } },
	/* slayer (the PC's rows' names and labels are crossed: kill_penalty's
	row is labelled KILL IN ORDER, kill_in_order's KILL PENALTY; each sets
	what its label says) */
	{ "death_bonus_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.slayer.no_death_bonus), 0, 2, { 0, 1 } },
	{ "kill_penalty_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.slayer.kill_in_order), 0, 2, { 1, 0 } },
	{ "kill_in_order_spinner", _option_byte, VARIANT_FIELD(game_engine_variant.slayer.no_kill_penalty), 0, 2,
		{ 0, 1 } },
	/* (up to 500, for big games: tools/port_settings.py's STRING_INSERTS) */
	{ "kills_to_win_spinner", _option_long, VARIANT_FIELD(universal_variant.score_to_win), 0, 11,
		{ 5, 10, 15, 25, 50, 75, 100, 150, 200, 250, 500 } },
	/* team options */
	{ "friendly_fire_spinner", _option_short, OPTIONS_FIELD(friendly_fire), 0, 4,
		{ _friendly_fire_off, _friendly_fire_on, _friendly_fire_shields_only, _friendly_fire_explosives_only } },
	{ "friendly_fire_penalty_spinner", _option_short, OPTIONS_FIELD(friendly_fire_penalty), 0, 4, { 0, 5, 10, 15 } },
	{ "autobalance_spinner", _option_option_byte, OPTIONS_FIELD(auto_team_balance), 0, 2, { 0, 1 } },
	/* (the host's voice chat, below them: port/linux/game/network_voice.c;
	tools/port_settings.py's TEAMPLAY_ROWS) */
	{ "voice_mode_spinner", _option_setting, 0, 0, 5, { 0 }, "network.voice_mode",
		{ "off", "team_proximity", "team_enemy_proximity", "team_global", "team_global_enemy_proximity" } },
	{ "voice_lobby_spinner", _option_setting, 0, 0, 2, { 0 }, "network.voice_lobby", { "true", "false" } },
	{ "voice_kbps_spinner", _option_setting, 0, 0, 7, { 0 }, "network.voice_kbps",
		{ "8", "12", "16", "24", "32", "48", "64" } },
	{ "voice_proximity_spinner", _option_setting, 0, 0, 6, { 0 }, "network.voice_proximity",
		{ "5", "10", "15", "20", "30", "50" } },
	/* (and the players' votes to kick: port/linux/game/network_votekick.c) */
	{ "votekick_spinner", _option_setting, 0, 0, 2, { 0 }, "network.votekick", { "true", "false" } },
	/* (and the text chat: port/linux/game/network_chat.c) */
	{ "text_chat_spinner", _option_setting, 0, 0, 3, { 0 }, "network.text_chat", { "on", "links", "off" } },
	{ "aim_assist_spinner", _option_setting, 0, 0, 2, { 0 }, "network.block_mouse_aim_assist", { "false", "true" } },
	/* (the shield effects every machine draws: network_shield_effects.c) */
	{ "shield_effects_spinner", _option_setting, 0, 0, 7, { 0 }, "network.shield_effects",
		{ "stock", "glow", "light_show", "overshield", "glow_overshield", "light_show_overshield", "players" } },
	/* co-op's options (Server Setup's CO-OP OPTIONS: tools/port_settings.py's
	COOP_SETUP_SCREENS; coop_enemies.c) */
	{ "coop_friendly_fire_spinner", _option_setting, 0, 0, 4, { 0 }, "network.coop_friendly_fire",
		{ "off", "on", "shields_only", "explosives_only" } },
	{ "coop_extra_enemies_spinner", _option_setting, 0, 0, 3, { 0 }, "network.coop_enemies_mode",
		{ "per_player", "multiplier", "none" } },
	{ "coop_extra_pickups_spinner", _option_setting, 0, 0, 3, { 0 }, "network.coop_pickups",
		{ "per_player", "multiplier", "none" } },
	{ "coop_extra_vehicles_spinner", _option_setting, 0, 0, 3, { 0 }, "network.coop_vehicles",
		{ "per_player", "multiplier", "none" } },
	{ "coop_enemies_per_player_spinner", _option_setting, 0, 0, 5, { 0 }, "network.coop_enemies",
		{ "25", "50", "100", "150", "200" } },
	{ "coop_enemies_multiplier_spinner", _option_setting, 0, 0, 5, { 0 }, "network.coop_enemies_multiplier",
		{ "2", "4", "8", "16", "32" } },
	{ "coop_player_collisions_spinner", _option_setting, 0, 0, 2, { 0 }, "network.coop_player_collisions",
		{ "true", "false" } },
	{ "coop_ally_friendly_fire_spinner", _option_setting, 0, 0, 2, { 0 }, "network.coop_ally_friendly_fire",
		{ "true", "false" } },
	/* map sharing's (Server Setup's MAP SHARING: tools/port_settings.py's
	MAP_SHARE_SETUP; map_share.c) */
	{ "map_share_spinner", _option_setting, 0, 0, 2, { 0 }, "network.map_share", { "true", "false" } },
	{ "map_share_upload_spinner", _option_setting, 0, 0, 8, { 0 }, "network.map_share_upload",
		{ "256", "512", "1024", "2048", "4096", "8192", "16384", "20480" } },
	/* the bots' (Server Setup's BOTS & Campaign Bot Options: tools/port_settings.py's
	BOTS_SETUP; bots.c) */
	{ "bots_count_spinner", _option_setting, 0, 0, 14, { 0 }, "bots.count",
		{ "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "15" } },
	{ "bots_skill_spinner", _option_setting, 0, 0, 4, { 0 }, "bots.skill",
		{ "easy", "normal", "heroic", "legendary" } },
	{ "bots_replace_spinner", _option_setting, 0, 0, 2, { 0 }, "bots.replace_on_join", { "true", "false" } },
	{ "bots_teams_spinner", _option_setting, 0, 0, 2, { 0 }, "bots.teams", { "even", "against" } },
	{ "bots_scoring_spinner", _option_setting, 0, 0, 2, { 0 }, "game.campaign_score", { "true", "false" } },
	/* vehicle options (the side's set and counts: vehicles_update) */
	{ "vehicles_respawn_spinner", _option_short, OPTIONS_FIELD(vehicle_respawn_time), 0, 7,
		{ 0, 30, 60, 90, 120, 180, 300 } },
	/* firefight options: Page 0 (Wave & Lives) */
	{ "ff_set_limit_spinner", _option_variant_short, VARIANT_FIELD(game_engine_variant.firefight.set_limit), 0, 6,
		{ 0, 1, 2, 3, 5, 10 } },
	{ "ff_starting_lives_spinner", _option_variant_short, VARIANT_FIELD(game_engine_variant.firefight.starting_lives), 0, 8,
		{ -1, 0, 3, 5, 7, 10, 15, 20 } },
	{ "ff_round_lives_spinner", _option_variant_short, VARIANT_FIELD(game_engine_variant.firefight.bonus_lives_per_round), 0, 5,
		{ 0, 1, 2, 3, 5 } },
	{ "ff_bonus_lives_spinner", _option_variant_short, VARIANT_FIELD(game_engine_variant.firefight.bonus_lives_per_set), 0, 5,
		{ 0, 1, 3, 5, 10 } },
	{ "ff_max_lives_spinner", _option_variant_short, VARIANT_FIELD(game_engine_variant.firefight.max_lives_cap), 0, 7,
		{ 0, 5, 10, 15, 20, 30, 50 } },
	{ "ff_prep_time_spinner", _option_option_byte, OPTIONS_FIELD(firefight_prep_countdown), 0, 5,
		{ 0, 1, 2, 3, 4 } },
	{ "ff_wave_limit_spinner", _option_option_byte, OPTIONS_FIELD(firefight_next_wave_mode), 0, 5,
		{ _firefight_next_wave_reach_default, _firefight_next_wave_timer_60s,
		  _firefight_next_wave_timer_90s, _firefight_next_wave_timer_120s, _firefight_next_wave_clear_all } },
	{ "ff_bonus_round_spinner", _option_variant_u8, VARIANT_FIELD(game_engine_variant.firefight.bonus_round_enabled), 0, 2,
		{ 1, 0 } },
	{ "ff_bonus_dur_spinner", _option_option_byte, OPTIONS_FIELD(firefight_round_3_skull), 0, 5,
		{ 30, 45, 60, 90, 120 } },
	/* firefight options: Page 1 (Factions & AI) */
	{ "ff_faction_spinner", _option_variant_u8, VARIANT_FIELD(game_engine_variant.firefight.wave_faction), 0, 8,
		{ _firefight_faction_covenant, _firefight_faction_mixed, _firefight_faction_flood,
		  _firefight_faction_sentinels, _firefight_faction_covenant_flood,
		  _firefight_faction_covenant_sentinels, _firefight_faction_flood_sentinels,
		  _firefight_faction_unsc_humans } },
	{ "ff_allegiance_spinner", _option_option_byte, OPTIONS_FIELD(firefight_faction_diplomacy), 0, 2,
		{ _firefight_diplomacy_allied_against_players, _firefight_diplomacy_classic_infighting } },
	{ "ff_difficulty_spinner", _option_variant_u8, VARIANT_FIELD(game_engine_variant.firefight.difficulty), 0, 4,
		{ 0, 1, 2, 3 } },
	{ "ff_escalation_spinner", _option_option_byte, OPTIONS_FIELD(firefight_set_escalation), 0, 5,
		{ 0, 1, 2, 3, 4 } },
	{ "ff_enemy_hp_spinner", _option_option_byte, OPTIONS_FIELD(firefight_round_1_skull), 0, 5,
		{ 0, 1, 2, 3, 4 } },
	{ "ff_enemy_shd_spinner", _option_option_byte, OPTIONS_FIELD(firefight_round_2_skull), 0, 5,
		{ 0, 1, 2, 3, 4 } },
	{ "ff_wave_size_spinner", _option_option_byte, OPTIONS_FIELD(firefight_wave_size_scale), 0, 4,
		{ 0, 1, 2, 3 } },
	{ "ff_boss_freq_spinner", _option_option_byte, OPTIONS_FIELD(firefight_boss_wave_mode), 0, 3,
		{ 0, 1, 2 } },
	{ "ff_ai_weapons_spinner", _option_option_byte, OPTIONS_FIELD(firefight_ai_weapon_sandbox), 0, 3,
		{ 0, 1, 2 } },
	/* firefight options: Page 2 (Enemy Units) */
	{ "ff_u_grunts_spinner", _option_unit_mask, 0, (1u << _ff_unit_covenant_grunts_bit), 2, { 1, 0 } },
	{ "ff_u_jackals_spinner", _option_unit_mask, 0, (1u << _ff_unit_covenant_jackals_bit), 2, { 1, 0 } },
	{ "ff_u_elites_spinner", _option_unit_mask, 0, (1u << _ff_unit_covenant_elites_bit), 2, { 1, 0 } },
	{ "ff_u_specops_spinner", _option_unit_mask, 0, (1u << _ff_unit_covenant_specops_stealth_bit), 2, { 1, 0 } },
	{ "ff_u_zealots_spinner", _option_unit_mask, 0, (1u << _ff_unit_covenant_zealots_bit), 2, { 1, 0 } },
	{ "ff_u_hunters_spinner", _option_unit_mask, 0, (1u << _ff_unit_covenant_hunters_bit), 2, { 1, 0 } },
	{ "ff_u_flood_c_spinner", _option_unit_mask, 0, (1u << _ff_unit_flood_combat_bit), 2, { 1, 0 } },
	{ "ff_u_flood_k_spinner", _option_unit_mask, 0,
		(1u << _ff_unit_flood_infection_bit) | (1u << _ff_unit_flood_carrier_bit), 2, { 1, 0 } },
	{ "ff_u_sents_spinner", _option_unit_mask, 0,
		(1u << _ff_unit_sentinels_bit) | (1u << _ff_unit_human_marines_bit) |
		(1u << _ff_unit_human_odst_heavies_bit), 2, { 1, 0 } },
	/* firefight options: Page 3 (Vehicles & Drops) */
	{ "ff_dropships_spinner", _option_option_byte, OPTIONS_FIELD(firefight_dropship_mode), 0, 4,
		{ 0, 1, 2, 3 } },
	{ "ff_boss_veh_spinner", _option_variant_u8, VARIANT_FIELD(game_engine_variant.firefight.vehicle_drops_enabled), 0, 5,
		{ 1, 0, 2, 3, 4 } },
	{ "ff_ordnance_spinner", _option_variant_u8, VARIANT_FIELD(game_engine_variant.firefight.ordnance_drops), 0, 4,
		{ 0, 1, 2, 3 } },
	{ "ff_r1_veh_spinner", _option_option_byte, OPTIONS_FIELD(firefight_early_vehicle), 0, 7,
		{ 0, 1, 2, 3, 1, 2, 3 } },
	{ "ff_r2_veh_spinner", _option_option_byte, OPTIONS_FIELD(firefight_early_vehicle), 0, 7,
		{ 0, 1, 2, 3, 1, 2, 3 } },
	{ "ff_r3_veh_spinner", _option_option_byte, OPTIONS_FIELD(firefight_late_vehicle), 0, 7,
		{ 0, 4, 2, 1, 1, 3, 2 } },
	{ "ff_allies_spinner", _option_option_byte, OPTIONS_FIELD(firefight_friendly_ai), 0, 5,
		{ 0, 1, 1, 2, 2 } },
	{ "ff_generators_spinner", _option_variant_short, VARIANT_FIELD(game_engine_variant.firefight.generator_count), 0, 4,
		{ 0, 1, 2, 3 } },
	{ "ff_gen_fail_spinner", _option_variant_u8, VARIANT_FIELD(game_engine_variant.firefight.generator_fail_mode), 0, 2,
		{ 0, 1 } },
	/* firefight options: Page 4 (Skulls & Traits) */
	{ "ff_prog_skulls_spinner", _option_option_byte, OPTIONS_FIELD(firefight_skull_progression_mode), 0, 4,
		{ 0, 1, 2, 3 } },
	{ "ff_start_skulls_spinner", _option_option_ulong, OPTIONS_FIELD(firefight_custom_skulls), 0, 6,
		{ 0, (1L << _firefight_skull_grunt_birthday_party_bit),
		  (1L << _firefight_skull_tough_luck_bit) | (1L << _firefight_skull_catch_bit),
		  (1L << _firefight_skull_mythic_bit) | (1L << _firefight_skull_tilt_bit),
		  (1L << _firefight_skull_famine_bit) | (1L << _firefight_skull_black_eye_bit),
		  0x1FFFL } },
	{ "ff_shields_spinner", _option_option_byte, OPTIONS_FIELD(firefight_player_shields), 0, 6,
		{ 0, 1, 0, 2, 2, 3 } },
	{ "ff_speed_spinner", _option_option_byte, OPTIONS_FIELD(firefight_player_speed), 0, 4,
		{ 0, 1, 2, 3 } },
	{ "ff_gravity_spinner", _option_option_byte, OPTIONS_FIELD(firefight_player_gravity), 0, 4,
		{ 0, 1, 2, 3 } },
	{ "ff_ammo_spinner", _option_option_byte, OPTIONS_FIELD(firefight_infinite_ammo), 0, 3,
		{ 0, 1, 2 } },
	{ "ff_gen_hp_spinner", _option_short, OPTIONS_FIELD(firefight_generator_health), 0, 4,
		{ 2000, 1000, 4000, 8000 } },
	{ "ff_custom_skulls_spinner", _option_option_ulong, OPTIONS_FIELD(firefight_starting_skulls[0]), 0, 5,
		{ 0,
		  (1L << _skull_bandanna) | (1L << _skull_boom),
		  (1L << 1) | (1L << 2), /* Scarab + Sputnik (mapped in firefight_40_skull_active) */
		  (1L << _skull_outbreak) | (1L << 9), /* Outbreak + Swarm */
		  (1L << _skull_bandanna) | (1L << _skull_boom) | (1L << 2) | (1L << _skull_pinata) |
		  (1L << _skull_grunt_funeral) | (1L << _skull_recession) | (1L << _skull_malfunction) | (1L << 8) } },
};

/* TEXT CHAT's (network.text_chat) OFF, which it shows while the game is
listed in the server browser, and its host cannot change: a listed game has
no text chat whatever the setting (network_chat.c), which is kept for a
private game or a LAN one */
#define TEXT_CHAT_SPINNER "text_chat_spinner"
#define TEXT_CHAT_OFF 2

static boolean text_chat_locked(struct gametype_option const *option)
{
	return !strcmp(option->spinner, TEXT_CHAT_SPINNER) && p2p_hosting_listed();
}

/* an option of the host's own setting: the value shown for it (the one
set, else the nearest) */
static short gametype_option_setting_index(struct gametype_option const *option)
{
	struct pc_menu_setting setting;
	char text[64];
	short index;

	csmemset(&setting, 0, sizeof(setting));
	setting.setting = option->setting;
	setting.value_count = option->count;
	for (index = 0; index < option->count; index++)
		setting.values[index] = option->setting_values[index];
	if (!setting_text(option->setting, text, sizeof(text), FALSE))
		return 0;
	return setting_value_index(&setting, text);
}

/* (an instance's name is its definition's cut to 31 characters:
"item_options_infinite_grenades_") */
#define WIDGET_NAME_LENGTH 31

static struct gametype_option const *gametype_option_named(char const *spinner)
{
	short index;

	for (index = 0; index < NUMBEROF(gametype_options); index++)
	{
		if (!strncmp(gametype_options[index].spinner, spinner, WIDGET_NAME_LENGTH))
			return &gametype_options[index];
	}
	return NULL;
}

static long gametype_option_value(struct gametype_option const *option, struct game_variant *variant,
	struct game_variant_options *options)
{
	byte *v = (byte *)variant;
	byte *o = (byte *)options;

	switch (option->kind)
	{
	case _option_long: return *(long *)(v + option->offset);
	case _option_byte: return v[option->offset] != 0;
	case _option_flag: return (variant->universal_variant.flags & option->argument) != 0;
	case _option_health: return (long)(variant->universal_variant.health * 10.0f + 0.5f);
	case _option_short: return *(short *)(o + option->offset);
	case _option_option_byte: return o[option->offset];
	case _option_radar: return options->radar_players;
	case _option_variant_short: return *(short *)(v + option->offset);
	case _option_variant_u8: return v[option->offset];
	case _option_unit_mask:
	{
		unsigned short mask = options->firefight_unit_toggles != 0 ?
			options->firefight_unit_toggles : FIREFIGHT_UNIT_MASK_ALL;
		return (mask & (unsigned short)option->argument) != 0 ? 1 : 0;
	}
	case _option_option_ulong: return (long)(*(unsigned long *)(o + option->offset));
	}
	return 0;
}

static void gametype_option_value_set(struct gametype_option const *option, long value, struct game_variant *variant,
	struct game_variant_options *options)
{
	byte *v = (byte *)variant;
	byte *o = (byte *)options;

	switch (option->kind)
	{
	case _option_long: *(long *)(v + option->offset) = value; break;
	case _option_byte: v[option->offset] = (byte)value; break;
	case _option_flag:
		if (value)
			variant->universal_variant.flags |= option->argument;
		else
			variant->universal_variant.flags &= ~option->argument;
		break;
	case _option_health: variant->universal_variant.health = (real)value / 10.0f; break;
	case _option_short: *(short *)(o + option->offset) = (short)value; break;
	case _option_option_byte: o[option->offset] = (byte)value; break;
	case _option_variant_short: *(short *)(v + option->offset) = (short)value; break;
	case _option_variant_u8: v[option->offset] = (byte)value; break;
	case _option_unit_mask:
		if (options->firefight_unit_toggles == 0)
			options->firefight_unit_toggles = FIREFIGHT_UNIT_MASK_ALL;
		if (value)
			options->firefight_unit_toggles |= (unsigned short)option->argument;
		else
			options->firefight_unit_toggles &= ~(unsigned short)option->argument;
		break;
	case _option_option_ulong:
		*(unsigned long *)(o + option->offset) = (unsigned long)value;
		break;
	case _option_radar:
		/* (and the Xbox's flag: other players on the tracker or not) */
		options->radar_players = (byte)value;
		if (value == _radar_players_none)
			variant->universal_variant.flags &= ~FLAG(_game_variant_draw_object_in_motion_sensor_bit);
		else
			variant->universal_variant.flags |= FLAG(_game_variant_draw_object_in_motion_sensor_bit);
		break;
	}
}

/* the value's place in the option's list: its own, else the nearest */
static short gametype_option_index(struct gametype_option const *option, long value)
{
	short index, best = 0;
	long best_distance = 0x7FFFFFFF;

	for (index = 0; index < option->count; index++)
	{
		long distance = option->values[index] > value ? option->values[index] - value : value - option->values[index];

		if (distance < best_distance)
		{
			best_distance = distance;
			best = index;
		}
	}
	return best;
}

/* each of the screen's option rows' spinner, with the option */
static void gametype_options_each(struct widget_instance *list, boolean save)
{
	struct game_variant *variant = edit_variant();
	struct game_variant_options *options = edit_options();
	struct widget_instance *row;

	if (!variant || !options)
		return;
	for (row = list->child; row; row = row->next)
	{
		struct widget_instance *spinner;

		for (spinner = row->child; spinner; spinner = spinner->next)
		{
			/* (a spinner: its label's name, cut to 31 characters, can be
			the same as its own) */
			struct gametype_option const *option = spinner->type == 2 ? gametype_option_named(spinner->name) : NULL;

			if (!option)
				continue;
			/* (the host's own settings: Server Setup's alone, their rows
			hidden in Edit Gametypes', which edits a gametype for any game;
			written on OK, as the gametype's are kept) */
			if (option->kind == _option_setting)
			{
				short index = (short)PIN(spinner->parameters.list.selected_index, 0, option->count - 1);

				visible_set(row, gametype_edit.setup);
				if (!gametype_edit.setup)
					continue;
				if (!save)
				{
					spinner->parameters.list.selected_index = text_chat_locked(option) ? TEXT_CHAT_OFF :
						gametype_option_setting_index(option);
				}
				else if (!text_chat_locked(option) && index != gametype_option_setting_index(option) &&
					!setting_write(option->setting, option->setting_values[index]))
				{
					platform_log("menus: could not set %s", option->setting);
				}
				continue;
			}
			if (save)
			{
				short index = (short)PIN(spinner->parameters.list.selected_index, 0, option->count - 1);

				gametype_option_value_set(option, option->values[index], variant, options);
			}
			else
				spinner->parameters.list.selected_index =
					gametype_option_index(option, gametype_option_value(option, variant, options));
		}
	}
}

/* each of a screen's rows of the host's own settings (_option_setting:
co-op's options' screens, tools/port_settings.py's _setup_option_screen),
shown, or (save) set */
static void setting_options_each(struct widget_instance *list, boolean save)
{
	struct widget_instance *row;

	for (row = list->child; row; row = row->next)
	{
		struct widget_instance *spinner;

		for (spinner = row->child; spinner; spinner = spinner->next)
		{
			struct gametype_option const *option = spinner->type == 2 ? gametype_option_named(spinner->name) : NULL;
			short index;

			if (!option || option->kind != _option_setting)
				continue;
			index = (short)PIN(spinner->parameters.list.selected_index, 0, option->count - 1);
			if (!save)
			{
				spinner->parameters.list.selected_index = text_chat_locked(option) ? TEXT_CHAT_OFF :
					gametype_option_setting_index(option);
			}
			else if (!text_chat_locked(option) && index != gametype_option_setting_index(option) &&
				!setting_write(option->setting, option->setting_values[index]))
			{
				platform_log("menus: could not set %s", option->setting);
			}
		}
	}
}

/* the options' list of a button of its bar (OK's): the one above it whose
rows are its options */
static struct widget_instance *gametype_options_list(struct widget_instance *widget)
{
	struct widget_instance *list;

	for (list = widget; list; list = list->parent)
	{
		struct widget_instance *child;

		for (child = list->child; child; child = child->next)
		{
			if (!strncmp(child->name, "op_", 3))
				return list;
		}
	}
	return NULL;
}

/* ---- the vehicle options: a side's (red's, blue's) set and counts, on
the spinners while its side is shown */

static char const *const vehicle_spinners[NUMBER_OF_VARIANT_VEHICLES] =
{
	"warthog_spinner", "ghost_spinner", "scorpion_spinner", "rwarthog_spinner", "banshee_spinner", "cgturret_spinner"
};
/* (the presets' spinner: the vehicle sets 0 to 7, then PC and CUSTOM) */
#define VEHICLE_PRESET_PC 8
#define VEHICLE_PRESET_CUSTOM 9

static struct widget_instance *vehicle_spinner(struct widget_instance *list, char const *name)
{
	struct widget_instance *row;

	for (row = list->child; row; row = row->next)
	{
		struct widget_instance *spinner = named(row, name, 0);

		if (spinner)
			return spinner;
	}
	return NULL;
}

static void vehicles_show(struct widget_instance *list)
{
	struct game_variant_options *options = edit_options();
	struct widget_instance *spinner;
	short side = gametype_edit.vehicle_side, index;

	if (!options)
		return;
	if ((spinner = vehicle_spinner(list, "team_spinner")) != NULL)
		spinner->parameters.list.selected_index = side;
	if ((spinner = vehicle_spinner(list, "vehicle_presets_spinner")) != NULL)
		spinner->parameters.list.selected_index = options->vehicle_set[side] == VARIANT_VEHICLE_SET_CUSTOM ?
			VEHICLE_PRESET_CUSTOM : options->vehicle_set[side] == VARIANT_VEHICLE_SET_PC ?
			VEHICLE_PRESET_PC : (short)MIN(options->vehicle_set[side], VEHICLE_PRESET_PC - 1);
	for (index = 0; index < NUMBER_OF_VARIANT_VEHICLES; index++)
	{
		if ((spinner = vehicle_spinner(list, vehicle_spinners[index])) != NULL)
			spinner->parameters.list.selected_index =
				(short)MIN(options->vehicle_counts[side][index], MAXIMUM_VARIANT_VEHICLE_COUNT);
	}
}

/* the side shown's spinners kept (and the Xbox's vehicle set: red's, if it
is one of the Xbox's) */
static void vehicles_keep(struct widget_instance *list)
{
	struct game_variant *variant = edit_variant();
	struct game_variant_options *options = edit_options();
	struct widget_instance *spinner;
	short side = gametype_edit.vehicle_side, index;

	if (!options || !variant)
		return;
	if ((spinner = vehicle_spinner(list, "vehicle_presets_spinner")) != NULL)
		options->vehicle_set[side] = spinner->parameters.list.selected_index >= VEHICLE_PRESET_CUSTOM ?
			VARIANT_VEHICLE_SET_CUSTOM : spinner->parameters.list.selected_index == VEHICLE_PRESET_PC ?
			VARIANT_VEHICLE_SET_PC : (byte)spinner->parameters.list.selected_index;
	for (index = 0; index < NUMBER_OF_VARIANT_VEHICLES; index++)
	{
		if ((spinner = vehicle_spinner(list, vehicle_spinners[index])) != NULL)
			options->vehicle_counts[side][index] =
				(byte)PIN(spinner->parameters.list.selected_index, 0, MAXIMUM_VARIANT_VEHICLE_COUNT);
	}
	variant->universal_variant.vehicle_set = options->vehicle_set[0] <= 4 ? options->vehicle_set[0] : 0;
}

/* "mp prof vehicles update": the side changed (the one left kept, the other
shown); a count changed makes the side's set CUSTOM */
static void vehicles_update(struct widget_instance *list)
{
	struct game_variant_options *options = edit_options();
	struct widget_instance *team = vehicle_spinner(list, "team_spinner");
	struct widget_instance *preset = vehicle_spinner(list, "vehicle_presets_spinner");
	short index;

	if (!options)
		return;
	if (team && team->parameters.list.selected_index != gametype_edit.vehicle_side)
	{
		vehicles_keep(list);
		gametype_edit.vehicle_side = (short)PIN(team->parameters.list.selected_index, 0, 1);
		vehicles_show(list);
		return;
	}
	for (index = 0; preset && index < NUMBER_OF_VARIANT_VEHICLES; index++)
	{
		struct widget_instance *spinner = vehicle_spinner(list, vehicle_spinners[index]);

		if (spinner && spinner->parameters.list.selected_index !=
			options->vehicle_counts[gametype_edit.vehicle_side][index])
		{
			options->vehicle_counts[gametype_edit.vehicle_side][index] = (byte)spinner->parameters.list.selected_index;
			preset->parameters.list.selected_index = VEHICLE_PRESET_CUSTOM;
		}
	}
}

/* "mp profile init X" (the list's creation): its spinners from the gametype */
static boolean gametype_options_init(struct widget_instance *list)
{
	if (!edit_variant())
		return campaign_fail();
	if (named(list, "ff_page_spinner", 0))
		named(list, "ff_page_spinner", 0)->parameters.list.selected_index = 0;
	gametype_options_each(list, FALSE);
	if (named(list, "op_team", 0))
		vehicles_show(list);
	return TRUE;
}

/* "mp profile set X" (OK): the gametype from its spinners */
static boolean gametype_options_save(struct widget_instance *widget)
{
	struct widget_instance *list = gametype_options_list(widget);

	if (!list || !edit_variant())
		return campaign_fail();
	if (named(list, "op_team", 0))
		vehicles_keep(list);
	gametype_options_each(list, TRUE);
	/* (OPENCE OPTIONS: Infection is played on teams, Gun Game free for all:
	game_modes_variant_mode) */
	if (named(list, "oce_mode_spinner", 0) && edit_variant()->game_engine_index == game_engine_slayer)
	{
		if (edit_options()->mode == _game_mode_infection)
			edit_variant()->universal_variant.teams = TRUE;
		else if (edit_options()->mode == _game_mode_gun_game)
			edit_variant()->universal_variant.teams = FALSE;
	}
	return TRUE;
}

/* ---- Server Setup's options: a copy of the game's gametype (the one chosen
before), edited by the editor's screens, the game's on START GAME */

static void gametype_setup_begin(void)
{
	if (gametype_edit.setup)
		return;
	if (!player_ui_game_variant_specified(&gametype_edit.setup_variant))
		return;
	gametype_edit.setup_options = *player_ui_get_game_variant_options();
	gametype_edit.setup = TRUE;
	gametype_edit.vehicle_side = 0;
}

static void gametype_setup_end(void)
{
	gametype_edit.setup = FALSE;
}

static boolean gametype_setup_apply(void)
{
	boolean applied = !gametype_edit.setup ||
		ui_widget_port_game_variant_set(&gametype_edit.setup_variant, &gametype_edit.setup_options);

	gametype_edit.setup = FALSE;
	return applied;
}

/* the game type row's: the gametype's name and type */
static void gametype_setup_type(wchar_t *text)
{
	wchar_t name[NUMBEROF(gametype_edit.setup_variant.human_readable_game_description) + 1];
	wchar_t const *kind;

	text[0] = 0;
	if (!gametype_edit.setup)
		return;
	ustrncpy(name, gametype_edit.setup_variant.human_readable_game_description, NUMBEROF(name) - 1);
	name[NUMBEROF(name) - 1] = 0;
	kind = game_modes_variant_kind_name(&gametype_edit.setup_variant, &gametype_edit.setup_options);
	usnprintf(text, ROW_TEXT_LENGTH - 1, L"%s (%s%s)", name,
		kind ? kind : engine_names[PIN(gametype_edit.setup_variant.game_engine_index, 0, 6)],
		gametype_edit.setup_variant.universal_variant.teams ? L", TEAMS" : L"");
	text[ROW_TEXT_LENGTH - 1] = 0;
}

/* "port setup edit" (a Server Setup option's row): its screen edits the copy */
static boolean gametype_setup_edit(void)
{
	gametype_setup_begin();
	return gametype_edit.setup || campaign_fail();
}

/* ---- Edit Gametypes' list */

static void gametype_edit_read(void)
{
	long all[MAXIMUM_GAMETYPES];
	short last;
	short count;
	short out = 0;
	short i;

	count = ui_widget_port_gametypes(all, MAXIMUM_GAMETYPES, &last);
	for (i = 0; i < count; i++)
	{
		if (gametype_profile_matches_gamemode(all[i]))
			gametype_edit.gametypes[out++] = all[i];
	}
	gametype_edit.count = out;
	gametype_edit.chosen = (short)MIN(gametype_edit.chosen, MAX(0, gametype_edit.count - 1));
	gametype_edit.stale = FALSE;
}

/* "mp profiles list initialize" (Edit Gametypes'): on the gametype used last */
static boolean gametype_edit_list_initialize(struct widget_instance *list)
{
	long all[MAXIMUM_GAMETYPES];
	short last;
	short count;
	short out = 0;
	short chosen = 0;
	short i;

	gametype_edit.setup = FALSE;
	count = ui_widget_port_gametypes(all, MAXIMUM_GAMETYPES, &last);
	for (i = 0; i < count; i++)
	{
		if (gametype_profile_matches_gamemode(all[i]))
		{
			if (i == last)
				chosen = out;
			gametype_edit.gametypes[out++] = all[i];
		}
	}
	gametype_edit.count = out;
	gametype_edit.chosen = chosen;
	gametype_edit.first = (short)MAX(0, MIN(chosen - GAMETYPE_EDIT_ROWS / 2, gametype_edit.count - GAMETYPE_EDIT_ROWS));
	gametype_edit.stale = FALSE;
	focus_row(list, (short)(gametype_edit.chosen - gametype_edit.first));
	return TRUE;
}

static void gametype_edit_row_text(short row, wchar_t *text)
{
	gametype_display_name(gametype_edit.gametypes[gametype_edit.first + row], text);
}

/* "gt edit list update": the rows (scrolled), the gametype chosen's
picture, name and rules */
static void gametype_edit_list_update(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	struct widget_instance *widget;
	short focused;

	if (gametype_edit.stale)
		gametype_edit_read();
	focused = list_scroll(list, &gametype_edit.first, gametype_edit.count, GAMETYPE_EDIT_ROWS);
	if (focused != NONE)
		gametype_edit.chosen = (short)MAX(0, MIN(focused, gametype_edit.count - 1));
	rows_update(list, (short)MIN(gametype_edit.count, GAMETYPE_EDIT_ROWS), gametype_edit_row_text);
	visible_set(named(description, "gametype_right_item", 0), gametype_edit.count > 0);
	if (gametype_edit.chosen < gametype_edit.count)
	{
		long profile_index = gametype_edit.gametypes[gametype_edit.chosen];
		struct game_variant variant;
		wchar_t text[ROW_TEXT_LENGTH * 2];

		visible_set(named(description, "locked_gametype_icon", 0),
			((unsigned long)profile_index & PLAYLIST_READ_ONLY_BIT) != 0);
		gametype_display_name(profile_index, text);
		text_set(named(description, "gametype_right_name", 0), text);
		if (playlist_profile_get(profile_index, &variant))
		{
			long engine = PIN(variant.game_engine_index, 0, 6);

			if ((widget = named(description, "gametype_right_pic", 0)) != NULL)
				widget->animation.current_frame_index = (short)engine;
			gametype_rules_text(profile_index, text, NUMBEROF(text));
			text_set(named(description, "gametype_right_data", 0), text);
		}
	}
	profile_name_show(description);
}

/* "port playlist edit desc" (after the PC version's "playlist settings menu
update desc"): the gametype editor's OPENCE OPTIONS, its eighth row, has its
own words (the list's ninth) over GAME OPTIONS' picture, and the buttons
after it SAVE CHANGES' */
#define PLAYLIST_EDIT_OPENCE_ROW 7
#define PLAYLIST_EDIT_OPENCE_WORDS 8
#define PLAYLIST_EDIT_GAME_OPTIONS_PICTURE 1

static void playlist_edit_description(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	short index = list->focused_child ? ui_widget_port_list_index(list) : NONE;

	if (!description || !description->child || !description->child->next || index < PLAYLIST_EDIT_OPENCE_ROW)
		return;
	description->child->parameters.text_box.string_list_index =
		index == PLAYLIST_EDIT_OPENCE_ROW ? PLAYLIST_EDIT_OPENCE_WORDS : (short)(index - 1);
	description->child->next->animation.current_frame_index =
		index == PLAYLIST_EDIT_OPENCE_ROW ? PLAYLIST_EDIT_GAME_OPTIONS_PICTURE : (short)(index - 1);
}

/* "mp profile begin editing" (the list's custom activation: OK, a row's A) */
static boolean gametype_edit_begin(void)
{
	if (gametype_edit.chosen >= gametype_edit.count)
		return campaign_fail();
	gametype_edit.setup = FALSE;
	gametype_edit.vehicle_side = 0;
	return ui_widget_port_gametype_edit_begin(gametype_edit.gametypes[gametype_edit.chosen]);
}

/* "request del playlist profile" (X, DELETE): not a built-in one */
static boolean gametype_delete_request(void)
{
	long profile_index = gametype_edit.chosen < gametype_edit.count ? gametype_edit.gametypes[gametype_edit.chosen] :
		NONE;

	if (profile_index == NONE || ((unsigned long)profile_index & PLAYLIST_READ_ONLY_BIT))
		return campaign_fail();
	gametype_edit.deleting = profile_index;
	return TRUE;
}

/* "final del playlist profile" */
static boolean gametype_delete_final(void)
{
	boolean deleted = gametype_edit.deleting != NONE && ui_widget_port_gametype_delete(gametype_edit.deleting);

	gametype_edit.deleting = NONE;
	gametype_edit.stale = TRUE;
	return deleted;
}

/* ---- the gametype's screen and the gametype type's */

/* "get edit game settings name": the gametype's name */
static void gametype_edit_name(struct widget_instance *text_box)
{
	struct game_variant *variant = edit_variant();
	wchar_t name[NUMBEROF(variant->human_readable_game_description) + 1];

	if (!variant)
		return;
	ustrncpy(name, variant->human_readable_game_description, NUMBEROF(name) - 1);
	name[NUMBEROF(name) - 1] = 0;
	text_set(text_box, name);
	if (text_box && text_box->parent)
	{
		boolean is_ff = variant->game_engine_index == game_engine_firefight;
		struct widget_instance *veh_row = named(text_box->parent, "playlist_edit_vehicles_list_ite", 0);

		if (!veh_row)
			veh_row = named(text_box->parent, "playlist_edit_vehicles_list_item", 0);
		visible_set(veh_row, !is_ff);
	}
}

#define FF_SETTINGS_PAGE_COUNT 5
#define FF_UNIT_GUARD_HELP_STRING_INDEX 191
static unsigned long ff_unit_guard_warning_time = 0;
static unsigned long ff_stats_sync_time = 0;
static wchar_t ff_stats_sync_status[64] = L"";
static short sr_selected_tab = 0;
static short sr_medal_page = 0;

/* ---- Gamemode Select Screen ("Standard", "Ranked", "Firefight", "Forge") */

static void gamemode_apply_disabled_rows(struct widget_instance *list)
{
	struct widget_instance *ranked = named(list, "gm_ranked_item", 0);

	if (ranked)
	{
		ranked->disabled = TRUE;
		ranked->alpha_modifier = 0.45f;
	}
}

static boolean gamemode_menu_init(struct widget_instance *list)
{
	if (!list)
		return FALSE;

	if (list->name && !strcmp(list->name, "forge_menu_select_list"))
	{
		if (named(list, "forge_item_single_player", 0))
			list->focused_child = named(list, "forge_item_single_player", 0);
		return TRUE;
	}
	if (list->name && !strcmp(list->name, "forge_tag_options_select_list"))
	{
		if (named(list, "forge_item_add_tags", 0))
			list->focused_child = named(list, "forge_item_add_tags", 0);
		else if (named(list, "forge_item_inject_tags", 0))
			list->focused_child = named(list, "forge_item_inject_tags", 0);
		return TRUE;
	}
	if (list->name && !strcmp(list->name, "creation_hub_select_list"))
	{
		if (named(list, "chub_item_create_map", 0))
			list->focused_child = named(list, "chub_item_create_map", 0);
		return TRUE;
	}

	gamemode_apply_disabled_rows(list);
	if (multiplayer.gamemode == _gamemode_firefight && named(list, "gm_firefight_item", 0))
		list->focused_child = named(list, "gm_firefight_item", 0);
	else if (multiplayer.gamemode == _gamemode_forge && named(list, "gm_forge_item", 0))
		list->focused_child = named(list, "gm_forge_item", 0);
	else if (multiplayer.gamemode == _gamemode_theater && named(list, "gm_theater_item", 0))
		list->focused_child = named(list, "gm_theater_item", 0);
	else if (multiplayer.gamemode == _gamemode_creation_hub && named(list, "gm_creation_hub_item", 0))
		list->focused_child = named(list, "gm_creation_hub_item", 0);
	else if (multiplayer.gamemode == _gamemode_dedi_server && named(list, "gm_dedi_server_item", 0))
		list->focused_child = named(list, "gm_dedi_server_item", 0);
	else if (named(list, "gm_standard_item", 0))
		list->focused_child = named(list, "gm_standard_item", 0);
	return TRUE;
}

static boolean gamemode_select_standard(void)
{
	multiplayer.gamemode = _gamemode_standard;
	multiplayer.firefight_mode = FALSE;
	gametype_edit.stale = TRUE;
	return TRUE;
}

static boolean gamemode_select_ranked(void)
{
	multiplayer.gamemode = _gamemode_ranked;
	multiplayer.firefight_mode = FALSE;
	gametype_edit.stale = TRUE;
	return TRUE;
}

static boolean gamemode_select_firefight(void)
{
	multiplayer.gamemode = _gamemode_firefight;
	multiplayer.firefight_mode = TRUE;
	gametype_edit.stale = TRUE;
	return TRUE;
}

static boolean gamemode_select_forge(void)
{
	multiplayer.gamemode = _gamemode_forge;
	multiplayer.firefight_mode = FALSE;
	gametype_edit.stale = TRUE;
	return TRUE;
}

static boolean gamemode_select_theater(void)
{
	multiplayer.gamemode = _gamemode_theater;
	multiplayer.firefight_mode = FALSE;
	gametype_edit.stale = TRUE;
	return TRUE;
}

static boolean gamemode_select_creation_hub(void)
{
	multiplayer.gamemode = _gamemode_creation_hub;
	multiplayer.firefight_mode = FALSE;
	gametype_edit.stale = TRUE;
	return TRUE;
}

static boolean gamemode_select_dedi_server(void)
{
	multiplayer.gamemode = _gamemode_dedi_server;
	multiplayer.firefight_mode = FALSE;
	gametype_edit.stale = TRUE;
	return TRUE;
}

static void gamemode_menu_update(struct widget_instance *list)
{
	struct widget_instance *desc = list ? list->parameters.list.extended_description : NULL;
	short focused_mode = _gamemode_standard;

	if (!list)
		return;

	if (list->name && !strcmp(list->name, "creation_hub_select_list"))
	{
		if (desc)
		{
			struct widget_instance *pic = named(desc, "gamemode_options_pic", 0);
			struct widget_instance *txt = named(desc, "gamemode_options_txt", 0);
			char const *fc = (list->focused_child && list->focused_child->name) ? list->focused_child->name : "";

			if (pic)
				pic->animation.current_frame_index = _gamemode_creation_hub;
			if (txt)
			{
				text_set_length(
					txt,
					!strcmp(fc, "chub_item_create_map") ?
						L"CREATE / EDIT MAP\r\nCHOOSE MAP NAME &\r\nBASE MAP, INJECT TAGS" :
					!strcmp(fc, "chub_item_tag_editor") ?
						L"TAG CACHE & EDITOR\r\nEXTRACT CUSTOM TAGS,\r\n3D PREVIEW & EDIT" :
					!strcmp(fc, "chub_item_firing_range") ?
						L"FORERUNNER TEST RANGE\r\nGUN RANGE, VEHICLES,\r\nANIMATIONS & AI ARENA" :
					!strcmp(fc, "chub_item_mods_manager") ?
						L"MODS MANAGER\r\nENABLE, DISABLE &\r\nREORDER MOD PACKAGES" :
						L"CREATION HUB\r\nMODDING, TAG INJECTION,\r\nMAP CREATION & RANGE",
					160);
			}
			profile_name_show(desc);
		}
		return;
	}

	if (list->name &&
		(!strcmp(list->name, "forge_menu_select_list") ||
		 !strcmp(list->name, "forge_tag_options_select_list")))
	{
		if (desc)
		{
			struct widget_instance *pic = named(desc, "gamemode_options_pic", 0);
			struct widget_instance *txt = named(desc, "gamemode_options_txt", 0);
			char const *fc = (list->focused_child && list->focused_child->name) ? list->focused_child->name : "";

			if (pic)
				pic->animation.current_frame_index = _gamemode_forge;
			if (txt)
			{
				text_set_length(
					txt,
					!strcmp(fc, "forge_item_multiplayer") ?
						L"MULTIPLAYER FORGE\r\nHOST OR JOIN LIVE\r\nCO-OP MAP EDITING\r\nSESSIONS" :
					!strcmp(fc, "forge_item_edit_maps") ?
						L"SAVED FORGE MAPS\r\nBROWSE & LOAD SAVED\r\nFORGE & CUSTOM MAPS" :
					!strcmp(fc, "forge_item_tag_options") ?
						L"TAG OPTIONS\r\nADD TAGS FROM MAPS,\r\nMANAGE & RESET\r\nTAG CACHES" :
					!strcmp(fc, "forge_item_add_tags") ?
						L"ADD TAGS\r\nBROWSE CUSTOM MAPS\r\nWITH 3D PREVIEW &\r\nEXTRACT TAGS" :
					!strcmp(fc, "forge_item_installed_tags") ?
						L"INSTALLED TAGS\r\nVIEW 3D PREVIEWS &\r\nMANAGE INSTALLED\r\nCACHE TAGS" :
					!strcmp(fc, "forge_item_reset_stock_tags") ?
						L"RESET STOCK TAGS\r\nCLEAR STOCK TAG CACHE\r\nWHILE KEEPING\r\nCUSTOM TAGS" :
					!strcmp(fc, "forge_item_reset_custom_tags") ?
						L"RESET CUSTOM TAGS\r\nREMOVE ALL EXTRACTED\r\nCUSTOM [C] TAG\r\nPACKAGES" :
					!strcmp(fc, "forge_item_inject_tags") ?
						L"SYNC CUSTOM TAGS\r\nIMPORT .OCETAGS\r\nPACKAGES FROM\r\nFABER_DATA" :
					!strcmp(fc, "clear_custom_cache_profile_item") ?
						L"CLEAR CUSTOM TAGS\r\nREMOVE IMPORTED\r\nCUSTOM .OCETAGS\r\nPACKAGES" :
					!strcmp(fc, "reset_tag_cache_profile_item") ?
						L"RESET TAG CACHE\r\nREBUILD ALL EXTRACTED\r\nFORGE TAG CACHES" :
					!strcmp(fc, "forge_item_snap_points") ?
						L"SNAP POINTS\r\nASSIGN 3D SNAP FACES\r\n& ALIGNMENT EDGES\r\nTO TAGS" :
						L"SINGLE PLAYER SANDBOX\r\nEDIT ANY STOCK,\r\nCUSTOM OR FORGE\r\nMAP SOLO",
					160);
			}
			profile_name_show(desc);
		}
		return;
	}

	gamemode_apply_disabled_rows(list);
	if (list->focused_child && list->focused_child->disabled)
	{
		list->focused_child = (multiplayer.gamemode == _gamemode_firefight && named(list, "gm_firefight_item", 0)) ?
			named(list, "gm_firefight_item", 0) :
			(multiplayer.gamemode == _gamemode_forge && named(list, "gm_forge_item", 0)) ?
			named(list, "gm_forge_item", 0) :
			named(list, "gm_standard_item", 0);
	}

	if (list->focused_child && list->focused_child->name)
	{
		if (!strcmp(list->focused_child->name, "gm_ranked_item"))
			focused_mode = _gamemode_ranked;
		else if (!strcmp(list->focused_child->name, "gm_firefight_item"))
			focused_mode = _gamemode_firefight;
		else if (!strcmp(list->focused_child->name, "gm_forge_item"))
			focused_mode = _gamemode_forge;
		else if (!strcmp(list->focused_child->name, "gm_theater_item"))
			focused_mode = _gamemode_theater;
		else if (!strcmp(list->focused_child->name, "gm_creation_hub_item"))
			focused_mode = _gamemode_creation_hub;
		else if (!strcmp(list->focused_child->name, "gm_dedi_server_item"))
			focused_mode = _gamemode_dedi_server;
	}

	if (desc)
	{
		struct widget_instance *pic = named(desc, "gamemode_options_pic", 0);
		struct widget_instance *txt = named(desc, "gamemode_options_txt", 0);

		if (pic)
			pic->animation.current_frame_index = focused_mode;
		if (txt)
		{
			text_set_length(
				txt,
				focused_mode == _gamemode_firefight ?
					L"CO-OP WAVE SURVIVAL\r\nHOLD OUT AGAINST\r\nCOVENANT DROPSHIPS" :
				focused_mode == _gamemode_ranked ?
					L"COMPETITIVE RANKED\r\nPLAY COMING SOON" :
				focused_mode == _gamemode_forge ?
					L"MAP & SANDBOX EDITOR\r\nBUILD, SPAWN TAGS &\r\nEDIT MAPS SOLO OR\r\nTOGETHER" :
				focused_mode == _gamemode_theater ?
					L"THEATER REPLAY STUDIO\r\nWATCH SAVED FILMS\r\nWITH FREE-CAM &\r\nSPEED CONTROLS" :
				focused_mode == _gamemode_creation_hub ?
					L"CREATION HUB\r\nTAG CACHE, TAG EDITOR,\r\nMAP CREATOR & RANGE" :
				focused_mode == _gamemode_dedi_server ?
					L"DEDICATED SERVER\r\nCONFIGURE & LAUNCH\r\nSERVER ROTATION &\r\nWEB PANEL" :
					L"STANDARD MULTIPLAYER\r\nSLAYER, CTF, KING,\r\nODDBALL & RACE",
				160);
		}
		profile_name_show(desc);
	}
}

static void mp_type_screen_update(struct widget_instance *list)
{
	struct widget_instance *screen = list ? screen_of(list) : NULL;
	struct widget_instance *coop_item = list ? named(list, "multiplayer_type_coop_item", 0) : NULL;
	struct widget_instance *gametypes_item = list ? named(list, "multiplayer_type_gametypes_item", 0) : NULL;
	struct widget_instance *desc = list ? list->parameters.list.extended_description : NULL;
	boolean is_standard = multiplayer.gamemode == _gamemode_standard;
	boolean can_edit_gametypes = multiplayer.gamemode == _gamemode_standard ||
		multiplayer.gamemode == _gamemode_firefight;

	if (screen)
	{
		visible_set(named(screen, "header_multiplayer", 0), FALSE);
		visible_set(named(screen, "header_standard", 0), multiplayer.gamemode == _gamemode_standard);
		visible_set(named(screen, "header_ranked", 0), multiplayer.gamemode == _gamemode_ranked);
		visible_set(named(screen, "header_firefight", 0), multiplayer.gamemode == _gamemode_firefight);
		visible_set(named(screen, "header_forge", 0), multiplayer.gamemode == _gamemode_forge);
	}
	if (coop_item)
	{
		coop_item->visible = is_standard;
		coop_item->disabled = !is_standard;
	}
	if (gametypes_item)
	{
		gametypes_item->visible = can_edit_gametypes;
		gametypes_item->disabled = !can_edit_gametypes;
	}
	if (list && list->focused_child && (!list->focused_child->visible || list->focused_child->disabled))
	{
		if (named(list, "multiplayer_type_join_internet_item", 0))
			list->focused_child = named(list, "multiplayer_type_join_internet_item", 0);
	}
	if (desc && list && list->focused_child && list->focused_child->name &&
		!strcmp(list->focused_child->name, "forge_type_edit_maps_item"))
	{
		struct widget_instance *txt = desc->child;
		if (txt)
			txt->parameters.text_box.string_list_index = 7;
	}
}

/* ---- Dedicated Service Record Menu Screen */

static void service_record_set_row(
	struct widget_instance *desc,
	short row_num,
	wchar_t const *label_text,
	wchar_t const *val_text)
{
	char label_name[32];
	char val_name[32];

	if (!desc)
		return;
	snprintf(label_name, sizeof(label_name), "sr_r%d_label", (int)row_num);
	snprintf(val_name, sizeof(val_name), "sr_r%d_val", (int)row_num);
	text_set(named(desc, label_name, 0), label_text ? label_text : L"");
	text_set(named(desc, val_name, 0), val_text ? val_text : L"");
}

static void service_record_set_status(char const *status_ascii)
{
	short i;

	for (i = 0; status_ascii && status_ascii[i] && i + 1 < (short)NUMBEROF(ff_stats_sync_status); i++)
		ff_stats_sync_status[i] = (wchar_t)(unsigned char)status_ascii[i];
	ff_stats_sync_status[i] = 0;
	ff_stats_sync_time = system_milliseconds();
}

static boolean service_record_init(struct widget_instance *list)
{
	sr_selected_tab = 0;
	sr_medal_page = 0;
	ff_stats_sync_time = 0;
	ff_stats_sync_status[0] = 0;
	if (list && named(list, "sr_tab_0", 0))
		list->focused_child = named(list, "sr_tab_0", 0);
	return TRUE;
}

static boolean service_record_select_tab(struct widget_instance *widget)
{
	if (widget && widget->name && !strncmp(widget->name, "sr_tab_", 7) &&
		widget->name[7] >= '0' && widget->name[7] <= '7')
	{
		short tab = (short)(widget->name[7] - '0');
		if (tab == 5 && sr_selected_tab == 5)
			sr_medal_page = (short)((sr_medal_page < 5) ? (sr_medal_page + 5) : (sr_medal_page - 5));
		else if ((tab == 6 && sr_selected_tab == 6) || (tab == 7 && sr_selected_tab == 7))
			sr_medal_page = (short)((sr_medal_page + 1) % 6);
		else if (tab != sr_selected_tab)
			sr_medal_page = 0;
		sr_selected_tab = tab;
		ui_play_audio_feedback_sound(SOUND_FORWARD);
	}
	return TRUE;
}

static boolean service_record_medals_prev(struct widget_instance *widget)
{
	(void)widget;
	if (sr_selected_tab == 5)
	{
		sr_medal_page = (short)((sr_medal_page + 9) % 10);
		ui_play_audio_feedback_sound(SOUND_CURSOR);
	}
	else if (sr_selected_tab == 6 || sr_selected_tab == 7)
	{
		sr_medal_page = (short)((sr_medal_page + 5) % 6);
		ui_play_audio_feedback_sound(SOUND_CURSOR);
	}
	return TRUE;
}

static boolean service_record_medals_next(struct widget_instance *widget)
{
	(void)widget;
	if (sr_selected_tab == 5)
	{
		sr_medal_page = (short)((sr_medal_page + 1) % 10);
		ui_play_audio_feedback_sound(SOUND_CURSOR);
	}
	else if (sr_selected_tab == 6 || sr_selected_tab == 7)
	{
		sr_medal_page = (short)((sr_medal_page + 1) % 6);
		ui_play_audio_feedback_sound(SOUND_CURSOR);
	}
	return TRUE;
}

static boolean service_record_check_leaderboards(struct widget_instance *widget)
{
	char status_ascii[64];

	(void)widget;
	status_ascii[0] = '\0';
	firefight_stats_open_web_profile(NONE, status_ascii, sizeof(status_ascii));
	service_record_set_status(status_ascii);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

static boolean service_record_export_json(struct widget_instance *widget)
{
	char status_ascii[64];
	char json_path[300];

	(void)widget;
	if (firefight_stats_export_json(NONE, json_path, sizeof(json_path)))
		snprintf(status_ascii, sizeof(status_ascii), "EXPORTED JSON TO SAVE DIR");
	else
		snprintf(status_ascii, sizeof(status_ascii), "JSON EXPORT FAILED");
	service_record_set_status(status_ascii);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

static boolean service_record_copy_id(struct widget_instance *widget)
{
	char status_ascii[64];

	(void)widget;
	status_ascii[0] = '\0';
	firefight_stats_copy_player_id(NONE, status_ascii, sizeof(status_ascii));
	service_record_set_status(status_ascii);
	ui_play_audio_feedback_sound(SOUND_FORWARD);
	return TRUE;
}

static boolean service_record_link_account(struct widget_instance *widget)
{
	char code_ascii[16];
	char status_ascii[64];
	char player_id[24];
	char text[256];
	static wchar_t msg[256];
	size_t c;

	(void)widget;
	code_ascii[0] = '\0';
	status_ascii[0] = '\0';
	player_id[0] = '\0';
	firefight_stats_get_player_id(NONE, player_id, sizeof(player_id));
	firefight_stats_generate_link_code(NONE, code_ascii, sizeof(code_ascii), status_ascii, sizeof(status_ascii));
	service_record_set_status(status_ascii);
	ui_play_audio_feedback_sound(SOUND_FORWARD);

	snprintf(
		text,
		sizeof(text),
		"\rEnter Code on Halostats\r%s",
		code_ascii);
	for (c = 0; text[c] != '\0' && c + 1 < NUMBEROF(msg); c++)
		msg[c] = (wchar_t)(unsigned char)text[c];
	msg[c] = 0;
	display_error_text_deferred(msg, 0);
	return TRUE;
}

static boolean is_campaign_map_key(char const *map_key)
{
	static char const *const camp_keys[10] =
	{
		"a10", "a30", "a50", "b30", "b40",
		"c10", "c20", "c40", "d20", "d40"
	};
	short i;

	if (!map_key || !map_key[0])
		return FALSE;
	for (i = 0; i < 10; i++)
	{
		if (strcmp(map_key, camp_keys[i]) == 0)
			return TRUE;
	}
	return FALSE;
}

static void service_record_update(struct widget_instance *list)
{
	struct widget_instance *desc = list ? list->parameters.list.extended_description : NULL;
	static wchar_t const *const tab_titles[8] =
	{
		L"CAREER SERVICE RECORD",
		L"FIREFIGHT COMBAT RECORD",
		L"CAMPAIGN SERVICE RECORD",
		L"MP STANDARD STATS",
		L"MP RANKED STATS",
		L"MEDALS CHEST",
		L"CAREER ACHIEVEMENTS",
		L"MCC CHALLENGES"
	};
	static wchar_t const *const medal_page_titles[10] =
	{
		L"CAMP/FF: MULTIKILLS (1/5)",
		L"CAMP/FF: SPREES (2/5)",
		L"CAMP/FF: WEAPON SPREES (3/5)",
		L"CAMP/FF: STYLE I (4/5)",
		L"CAMP/FF: STYLE II (5/5)",
		L"MP MEDALS: MULTIKILLS (1/5)",
		L"MP MEDALS: SPREES (2/5)",
		L"MP MEDALS: WEAPON SPREES (3/5)",
		L"MP MEDALS: STYLE I (4/5)",
		L"MP MEDALS: STYLE II (5/5)"
	};
	static wchar_t const *const ach_page_titles[6] =
	{
		L"ACHIEVEMENTS: CAMPAIGN I (1/6)",
		L"ACHIEVEMENTS: CAMPAIGN II (2/6)",
		L"ACHIEVEMENTS: DIFFICULTY (3/6)",
		L"ACHIEVEMENTS: FIREFIGHT I (4/6)",
		L"ACHIEVEMENTS: FIREFIGHT II (5/6)",
		L"ACHIEVEMENTS: SKULLS (6/6)"
	};
	static wchar_t const *const chal_page_titles[6] =
	{
		L"DAILY: CAMPAIGN (1/6)",
		L"DAILY: FIREFIGHT (2/6)",
		L"DAILY: MULTIPLAYER (3/6)",
		L"WEEKLY: CAMPAIGN (4/6)",
		L"WEEKLY: FIREFIGHT (5/6)",
		L"WEEKLY: MULTIPLAYER (6/6)"
	};
	struct firefight_career_stats career;
	struct widget_instance *arrow_left;
	struct widget_instance *arrow_right;
	char player_id[24];
	wchar_t val[64];
	wchar_t lbl[64];
	long total_medals = 0;
	long mp_total_medals = 0;
	short unlocked_ach = 0;
	short ach_score = 0;
	short ach_max_score = 1000;
	short view;
	short i;

	if (!list || !desc)
		return;

	if (list->focused_child && list->focused_child->name &&
		!strncmp(list->focused_child->name, "sr_tab_", 7) &&
		list->focused_child->name[7] >= '0' && list->focused_child->name[7] <= '7')
	{
		short focused_tab = (short)(list->focused_child->name[7] - '0');
		if (focused_tab != sr_selected_tab)
		{
			sr_selected_tab = focused_tab;
			sr_medal_page = 0;
		}
	}
	view = (short)PIN(sr_selected_tab, 0, 7);
	if (view == 5)
		text_set(named(desc, "sr_box_title", 0), medal_page_titles[PIN(sr_medal_page, 0, 9)]);
	else if (view == 6)
		text_set(named(desc, "sr_box_title", 0), ach_page_titles[PIN(sr_medal_page, 0, 5)]);
	else if (view == 7)
		text_set(named(desc, "sr_box_title", 0), chal_page_titles[PIN(sr_medal_page, 0, 5)]);
	else
		text_set(named(desc, "sr_box_title", 0), tab_titles[view]);

	firefight_stats_get_career(NONE, &career);
	firefight_stats_get_player_id(NONE, player_id, sizeof(player_id));
	firefight_stats_get_achievement_count(NONE, &unlocked_ach, NULL);
	firefight_stats_get_achievement_score(NONE, &ach_score, &ach_max_score);
	for (i = 0; i < 38; i++)
	{
		total_medals += career.medal_counts[i];
		mp_total_medals += career.mp_medal_counts[i];
	}

	if (view == 0)
	{
		/* Tab 0: OVERVIEW */
		struct firefight_mcc_rank_info rk;
		char rank_buf[64];

		firefight_stats_get_mcc_rank(NONE, &rk);
		snprintf(rank_buf, sizeof(rank_buf), "R%d: %s (T%d)",
			(int)rk.rank_in_tour, rk.rank_name ? rk.rank_name : "Recruit", (int)rk.tour_number);
		text_widen(val, NUMBEROF(val), rank_buf);
		service_record_set_row(desc, 1, L"MCC RANK:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld XP (%ld/%ld)",
			rk.total_xp, rk.xp_in_rank, rk.xp_for_next_rank);
		service_record_set_row(desc, 2, L"CAREER XP:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", career.total_matches);
		service_record_set_row(desc, 3, L"MATCHES PLAYED:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", career.total_score);
		service_record_set_row(desc, 4, L"CAREER SCORE:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", career.total_kills);
		service_record_set_row(desc, 5, L"TOTAL KILLS:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", career.total_sets_completed);
		service_record_set_row(desc, 6, L"SETS COMPLETED:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%d/%dG (%d/%d)",
			(int)ach_score, (int)ach_max_score, (int)unlocked_ach, (int)HALO_ACHIEVEMENT_COUNT);
		service_record_set_row(desc, 7, L"ACHIEVEMENT SCORE:", val);
	}
	else if (view == 1)
	{
		/* Tab 1: FIREFIGHT STATS */
		long kd_whole = career.total_deaths > 0 ? (career.total_kills / career.total_deaths) : career.total_kills;
		long kd_frac = career.total_deaths > 0 ? (((career.total_kills % career.total_deaths) * 100) / career.total_deaths) : 0;
		short ff_map_shown = 0;
		short r;

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", career.total_sets_completed);
		service_record_set_row(desc, 1, L"SETS COMPLETED:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", career.total_waves_cleared);
		service_record_set_row(desc, 2, L"WAVES CLEARED:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", career.total_boss_kills);
		service_record_set_row(desc, 3, L"BOSSES SLAIN:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", career.best_kill_spree);
		service_record_set_row(desc, 4, L"BEST KILL SPREE:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld.%02ld", kd_whole, kd_frac);
		service_record_set_row(desc, 5, L"FIREFIGHT K/D:", val);

		for (r = 0; r < 32 && ff_map_shown < 2; r++)
		{
			char map_key[32];
			struct firefight_map_record rec;

			if (!firefight_stats_get_indexed_map_record(r, NONE, map_key, sizeof(map_key), &rec))
				break;
			if (rec.best_set <= 0 && is_campaign_map_key(map_key))
				continue;

			for (i = 0; map_key[i] && i < 16; i++)
			{
				char c = map_key[i];
				lbl[i] = (wchar_t)((c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c);
			}
			lbl[i++] = L':';
			lbl[i] = 0;
			usnprintf(val, NUMBEROF(val) - 1, L"%ld (S%d R%d W%d)",
				rec.best_score, (int)rec.best_set, (int)rec.best_round, (int)rec.best_wave);
			service_record_set_row(desc, 6 + ff_map_shown, lbl, val);
			ff_map_shown++;
		}
		while (ff_map_shown < 2)
		{
			service_record_set_row(desc, 6 + ff_map_shown,
				(ff_map_shown == 0) ? L"TOP MAP RECORD:" : L"2ND MAP RECORD:",
				L"PLAY FIREFIGHT");
			ff_map_shown++;
		}
	}
	else if (view == 2)
	{
		/* Tab 2: CAMPAIGN STATS */
		static char const *const camp_levels[10] =
		{
			"levels\\a10\\a10", "levels\\a30\\a30", "levels\\a50\\a50",
			"levels\\b30\\b30", "levels\\b40\\b40", "levels\\c10\\c10",
			"levels\\c20\\c20", "levels\\c40\\c40", "levels\\d20\\d20",
			"levels\\d40\\d40"
		};
		static char const *const camp_short[10] =
		{
			"a10", "a30", "a50", "b30", "b40",
			"c10", "c20", "c40", "d20", "d40"
		};
		static wchar_t const *const diff_names[4] =
		{
			L"EASY", L"NORMAL", L"HEROIC", L"LEGENDARY"
		};
		long missions_cleared = 0;
		long total_camp_score = 0;
		long best_camp_score = 0;
		long fastest_secs = 0;
		long total_camp_medals = 0;
		long highest_diff = -1;
		short unlocked_skulls = 0;
		struct skull_set u_set = skulls_unlocked_set();

		for (i = 0; i < NUMBER_OF_SKULLS; i++)
		{
			if (skull_set_has(&u_set, i))
				unlocked_skulls++;
		}

		for (i = 0; i < 10; i++)
		{
			struct campaign_score_record crec;
			struct firefight_map_record frec;
			boolean has_c = campaign_score_level_record(camp_levels[i], NONE, &crec);
			boolean has_f = firefight_stats_get_map_record(camp_short[i], NONE, &frec);
			long lvl_score = 0;
			long lvl_secs = 0;
			long lvl_medals = 0;
			long lvl_diff = -1;

			if (has_c)
			{
				if (crec.score > lvl_score)
					lvl_score = crec.score;
				if (crec.seconds > 0)
					lvl_secs = crec.seconds;
				if (crec.medals > lvl_medals)
					lvl_medals = crec.medals;
				if (crec.difficulty >= 0 && crec.difficulty <= 3)
					lvl_diff = crec.difficulty;
			}
			if (has_f && frec.best_set == 0)
			{
				if (frec.best_score > lvl_score)
					lvl_score = frec.best_score;
				if (frec.best_seconds > 0 && (lvl_secs == 0 || frec.best_seconds < lvl_secs))
					lvl_secs = frec.best_seconds;
				if (frec.best_round > lvl_medals)
					lvl_medals = frec.best_round;
				if (frec.best_difficulty > lvl_diff && frec.best_difficulty <= 3)
					lvl_diff = frec.best_difficulty;
			}

			if (has_c || (has_f && frec.best_set == 0 && frec.best_score > 0))
			{
				missions_cleared++;
				total_camp_score += MAX(0L, lvl_score);
				if (lvl_score > best_camp_score)
					best_camp_score = lvl_score;
				if (lvl_secs > 0 && (fastest_secs == 0 || lvl_secs < fastest_secs))
					fastest_secs = lvl_secs;
				total_camp_medals += MAX(0L, lvl_medals);
				if (lvl_diff > highest_diff)
					highest_diff = lvl_diff;
			}
		}

		usnprintf(val, NUMBEROF(val) - 1, L"%ld / 10", missions_cleared);
		service_record_set_row(desc, 1, L"MISSIONS CLEARED:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", total_camp_score);
		service_record_set_row(desc, 2, L"CAMPAIGN SCORE:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", best_camp_score);
		service_record_set_row(desc, 3, L"BEST MISSION SCORE:", val);

		if (fastest_secs > 0)
			usnprintf(val, NUMBEROF(val) - 1, L"%02ld:%02ld", fastest_secs / 60, fastest_secs % 60);
		else
			usnprintf(val, NUMBEROF(val) - 1, L"--:--");
		service_record_set_row(desc, 4, L"FASTEST MISSION:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", total_camp_medals);
		service_record_set_row(desc, 5, L"CAMPAIGN MEDALS:", val);

		service_record_set_row(desc, 6, L"HIGHEST DIFFICULTY:",
			(highest_diff >= 0 && highest_diff <= 3) ? diff_names[highest_diff] : L"NONE");

		usnprintf(val, NUMBEROF(val) - 1, L"%d / %d", (int)unlocked_skulls, (int)NUMBER_OF_SKULLS);
		service_record_set_row(desc, 7, L"SKULLS UNLOCKED:", val);
	}
	else if (view == 3)
	{
		/* Tab 3: MP STANDARD */
		long mp_k = career.mp_kills > 0 ? career.mp_kills : career.total_kills;
		long mp_d = career.mp_deaths > 0 ? career.mp_deaths : career.total_deaths;
		long mp_a = career.mp_assists > 0 ? career.mp_assists : career.total_assists;
		long mp_m = career.mp_matches > 0 ? career.mp_matches : career.total_matches;
		long mp_s = career.mp_score > 0 ? career.mp_score : career.total_score;
		long kd_whole = mp_d > 0 ? (mp_k / mp_d) : mp_k;
		long kd_frac = mp_d > 0 ? (((mp_k % mp_d) * 100) / mp_d) : 0;
		long avg_score = mp_m > 0 ? (mp_s / mp_m) : 0;

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", mp_m);
		service_record_set_row(desc, 1, L"STANDARD MATCHES:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", mp_k);
		service_record_set_row(desc, 2, L"STANDARD KILLS:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", mp_d);
		service_record_set_row(desc, 3, L"STANDARD DEATHS:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld.%02ld", kd_whole, kd_frac);
		service_record_set_row(desc, 4, L"STANDARD K/D:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", mp_a);
		service_record_set_row(desc, 5, L"STANDARD ASSISTS:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", career.best_kill_spree);
		service_record_set_row(desc, 6, L"BEST KILL SPREE:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", avg_score);
		service_record_set_row(desc, 7, L"AVG SCORE / MATCH:", val);
	}
	else if (view == 4)
	{
		/* Tab 4: MP RANKED (Halo 3 Ranked Mode: 0..100 pts, +2 Win, -2 Loss, 0 Draw, Recruit .. General Grade 4) */
		struct firefight_h3_ranked_info h3_rk;
		wchar_t rk_title_wide[40];
		long rk_k = career.ranked_kills;
		long rk_d = career.ranked_deaths;
		long rk_m = career.ranked_matches;
		long kd_whole = rk_d > 0 ? (rk_k / rk_d) : rk_k;
		long kd_frac = rk_d > 0 ? (((rk_k % rk_d) * 100) / rk_d) : 0;

		firefight_stats_get_h3_ranked(NONE, &h3_rk);
		text_widen(rk_title_wide, NUMBEROF(rk_title_wide), h3_rk.rank_name ? h3_rk.rank_name : "Recruit");

		service_record_set_row(desc, 1, L"RANKED RANK:", rk_title_wide);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld / 100 PTS", h3_rk.points);
		service_record_set_row(desc, 2, L"RANKED POINTS:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ldW - %ldL - %ldD", h3_rk.wins, h3_rk.losses, h3_rk.draws);
		service_record_set_row(desc, 3, L"RANKED W/L/D:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", rk_m);
		service_record_set_row(desc, 4, L"RANKED MATCHES:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", rk_k);
		service_record_set_row(desc, 5, L"RANKED KILLS:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld", rk_d);
		service_record_set_row(desc, 6, L"RANKED DEATHS:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%ld.%02ld", kd_whole, kd_frac);
		service_record_set_row(desc, 7, L"RANKED K/D:", val);
	}
	else if (view == 5)
	{
		/* Tab 5: MEDALS CHEST (5 pages of 7 rows = all 38 Halo medals) */
		static struct
		{
			wchar_t const *label;
			short medal_idx;
			short extra_count;
		} const medal_rows[5][7] =
		{
			/* page 0: Multikills */
			{
				{ L"DOUBLE KILL:", 0, 1 },
				{ L"TRIPLE KILL:", 1, 1 },
				{ L"OVERKILL:", 2, 1 },
				{ L"KILLTACULAR:", 3, 1 },
				{ L"KILLTROCITY:", 4, 1 },
				{ L"KILLIMANJARO:", 5, 1 },
				{ L"KILLTASTROPHE+:", 6, 3 },
			},
			/* page 1: Sprees */
			{
				{ L"KILLING SPREE:", 9, 1 },
				{ L"KILLING FRENZY:", 10, 1 },
				{ L"RUNNING RIOT:", 11, 1 },
				{ L"RAMPAGE:", 12, 1 },
				{ L"UNTOUCHABLE:", 13, 1 },
				{ L"INVINCIBLE:", 14, 1 },
				{ L"INCONCEIVABLE+:", 15, 2 },
			},
			/* page 2: Weapon Sprees */
			{
				{ L"SHOTGUN SPREE:", 17, 1 },
				{ L"OPEN SEASON:", 18, 1 },
				{ L"SNIPER SPREE:", 19, 1 },
				{ L"SHARPSHOOTER:", 20, 1 },
				{ L"SPLATTER SPREE:", 21, 1 },
				{ L"VEH. MANSLAUGHTER:", 22, 1 },
				{ L"STICK SPREE:", 23, 1 },
			},
			/* page 3: Style Medals 1 */
			{
				{ L"BEAT DOWN:", 24, 1 },
				{ L"ASSASSIN:", 25, 1 },
				{ L"HEADSHOT:", 26, 1 },
				{ L"SNIPER KILL:", 27, 1 },
				{ L"GRENADE STICK:", 28, 1 },
				{ L"SPLATTER:", 29, 1 },
				{ L"INCINERATION:", 30, 1 },
			},
			/* page 4: Style Medals 2 */
			{
				{ L"FROM THE GRAVE:", 31, 1 },
				{ L"CLOSE CALL:", 32, 1 },
				{ L"FIRST STRIKE:", 33, 1 },
				{ L"REVENGE:", 34, 1 },
				{ L"AVENGER:", 35, 1 },
				{ L"WHEELMAN:", 36, 1 },
				{ L"PERFECTION:", 37, 1 },
			}
		};
		short page_idx = (short)PIN(sr_medal_page, 0, 9);
		boolean showing_mp = (page_idx >= 5);
		short group = (short)(page_idx % 5);
		long const *source_medals = showing_mp ? career.mp_medal_counts : career.medal_counts;
		short r;

		for (r = 0; r < 7; r++)
		{
			long count = 0;
			short k;
			for (k = 0; k < medal_rows[group][r].extra_count; k++)
				count += source_medals[medal_rows[group][r].medal_idx + k];
			usnprintf(val, NUMBEROF(val) - 1, L"%ld", count);
			service_record_set_row(desc, r + 1, medal_rows[group][r].label, val);
		}
	}
	else if (view == 6)
	{
		/* Tab 6: ACHIEVEMENTS (6 pages of 7 rows = 39 achievements) */
		short page = (short)PIN(sr_medal_page, 0, 5);
		short r;

		for (r = 0; r < 7; r++)
		{
			short ach_idx = (short)(page * 7 + r);
			if (ach_idx < HALO_ACHIEVEMENT_COUNT)
			{
				wchar_t const *title = NULL;
				boolean is_unlocked = firefight_stats_is_achievement_unlocked(NONE, ach_idx);
				short pts = firefight_stats_get_achievement_points(ach_idx);

				firefight_stats_get_achievement_info(ach_idx, NULL, &title, NULL);
				for (i = 0; title && title[i] && i < 24; i++)
					lbl[i] = title[i];
				lbl[i++] = L':';
				lbl[i] = 0;
				usnprintf(
					val,
					NUMBEROF(val) - 1,
					is_unlocked ? L"+%dG UNLOCKED" : L"%dG LOCKED",
					(int)pts);
				service_record_set_row(desc, r + 1, lbl, val);
			}
			else
			{
				service_record_set_row(desc, r + 1, L"", L"");
			}
		}
	}
	else
	{
		/* Tab 7: MCC CHALLENGES (6 pages: Daily/Weekly x Campaign/Firefight/Multiplayer) */
		short page = (short)PIN(sr_medal_page, 0, 5);
		short period = (short)(page / 3);
		short cat = (short)(page % 3);
		struct firefight_challenge_view chals[4];
		short count = firefight_stats_get_challenges(NONE, cat, period, chals, 4);
		short cat_done = 0;
		short d_done = 0, d_total = 0;
		short w_done = 0, w_total = 0;
		short r;

		firefight_stats_get_challenge_summary(NONE, &d_done, &d_total, &w_done, &w_total);
		for (r = 0; r < 4; r++)
		{
			if (r < count)
			{
				char val_ascii[64];
				char lbl_ascii[48];

				if (chals[r].completed)
					cat_done++;
				snprintf(lbl_ascii, sizeof(lbl_ascii), "%s:", chals[r].title);
				if (chals[r].completed)
					snprintf(val_ascii, sizeof(val_ascii), "DONE (+%ld XP)", chals[r].xp_reward);
				else
					snprintf(val_ascii, sizeof(val_ascii), "%ld/%ld (+%ld XP)",
						chals[r].progress, chals[r].target, chals[r].xp_reward);
				text_widen(lbl, NUMBEROF(lbl), lbl_ascii);
				text_widen(val, NUMBEROF(val), val_ascii);
				service_record_set_row(desc, r + 1, lbl, val);
			}
			else
			{
				service_record_set_row(desc, r + 1, L"", L"");
			}
		}
		usnprintf(val, NUMBEROF(val) - 1, L"%d / %d COMPLETED", (int)cat_done, (int)count);
		service_record_set_row(desc, 5, L"CATEGORY STATUS:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%d / %d COMPLETED", (int)d_done, (int)d_total);
		service_record_set_row(desc, 6, L"DAILY CHALLENGES:", val);

		usnprintf(val, NUMBEROF(val) - 1, L"%d / %d COMPLETED", (int)w_done, (int)w_total);
		service_record_set_row(desc, 7, L"WEEKLY CHALLENGES:", val);
	}

	arrow_left = named(desc, "sr_arrow_left", 0);
	arrow_right = named(desc, "sr_arrow_right", 0);
	if (arrow_left)
		arrow_left->visible = (view == 5 || view == 6 || view == 7);
	if (arrow_right)
		arrow_right->visible = (view == 5 || view == 6 || view == 7);

	if (ff_stats_sync_time != 0 && system_milliseconds() - ff_stats_sync_time < 5000 && ff_stats_sync_status[0])
	{
		text_set(named(desc, "sr_box_footer", 0), ff_stats_sync_status);
	}
	else if (list->focused_child && list->focused_child->name &&
		!strcmp(list->focused_child->name, "sr_check_leaderboards"))
	{
		text_set(named(desc, "sr_box_footer", 0), L"OPEN HALOSTATS.CC IN OVERLAY");
	}
	else if (view == 5)
	{
		boolean showing_mp = (PIN(sr_medal_page, 0, 9) >= 5);
		if (showing_mp)
			usnprintf(val, NUMBEROF(val) - 1, L"MP MEDALS: %ld (A: CAMP/FF)", mp_total_medals);
		else
			usnprintf(val, NUMBEROF(val) - 1, L"CAMP/FF MEDALS: %ld (A: MP)", total_medals);
		text_set(named(desc, "sr_box_footer", 0), val);
	}
	else if (view == 6)
	{
		usnprintf(val, NUMBEROF(val) - 1, L"PAGE %d / 6  (%d / %dG)",
			(int)(PIN(sr_medal_page, 0, 5) + 1), (int)ach_score, (int)ach_max_score);
		text_set(named(desc, "sr_box_footer", 0), val);
	}
	else if (view == 7)
	{
		usnprintf(val, NUMBEROF(val) - 1, L"PAGE %d / 6  (LEFT/RIGHT CATEGORY)",
			(int)(PIN(sr_medal_page, 0, 5) + 1));
		text_set(named(desc, "sr_box_footer", 0), val);
	}
	else
	{
		text_set(named(desc, "sr_box_footer", 0), L"WEB LEADERBOARDS: HALOSTATS.CC");
	}

	profile_name_show(desc);
}

/* "game settings lists text update": the help of the option with the
focus, for its value (the description's strings are each option's values'
helps in turn), as the Xbox's, which stops the game on the buttons' row */
static void gametype_option_help(struct widget_instance *list)
{
	struct widget_instance *description = list->parameters.list.extended_description;
	struct widget_instance *row;
	short index = 0;

	/* Firefight 5-page Custom Gametype Editor & Unit Toggle Validation Guard */
	if (named(list, "ff_page_spinner", 0))
	{
		short page = (short)PIN(named(list, "ff_page_spinner", 0)->parameters.list.selected_index, 0, FF_SETTINGS_PAGE_COUNT - 1);
		short y = 100;
		unsigned short candidate_unit_mask = FIREFIGHT_UNIT_MASK_ALL;
		byte wave_faction = _firefight_faction_covenant;

		if (named(list, "ff_faction_spinner", 0))
		{
			struct gametype_option const *f_opt = gametype_option_named("ff_faction_spinner");
			short f_idx = named(list, "ff_faction_spinner", 0)->parameters.list.selected_index;
			if (f_opt)
				wave_faction = (byte)f_opt->values[PIN(f_idx, 0, f_opt->count - 1)];
		}

		for (row = list->child; row; row = row->next)
		{
			if (!strncmp(row->name, "op_ff", 5) && row->name[5] >= '0' && row->name[5] <= '4')
			{
				boolean show = (row->name[5] - '0') == page;
				visible_set(row, show);
				if (show)
				{
					row->vertical_offset = y;
					y += 27;
				}
			}
			if (!strncmp(row->name, "op_ff2_u_", 9))
			{
				struct widget_instance *sp;
				for (sp = row->child; sp && sp->type != 2; sp = sp->next)
					;
				if (sp)
				{
					struct gametype_option const *opt = gametype_option_named(sp->name);
					if (opt && opt->kind == _option_unit_mask && sp->parameters.list.selected_index == 1)
						candidate_unit_mask &= ~(unsigned short)opt->argument;
				}
			}
		}

		if (list->focused_child && !list->focused_child->visible && named(list, "op_ff_page", 0))
			list->focused_child = named(list, "op_ff_page", 0);

		/* Enforce: at least 1 enemy unit type must remain enabled for the active faction(s) */
		if (!firefight_validate_unit_toggles(wave_faction, candidate_unit_mask))
		{
			if (list->focused_child && !strncmp(list->focused_child->name, "op_ff2_u_", 9))
			{
				struct widget_instance *sp;
				for (sp = list->focused_child->child; sp && sp->type != 2; sp = sp->next)
					;
				if (sp && sp->parameters.list.selected_index == 1)
				{
					sp->parameters.list.selected_index = 0;
					ui_play_audio_feedback_sound(SOUND_ERROR);
					ff_unit_guard_warning_time = system_milliseconds();
				}
			}
			else
			{
				char const *fallback_spinner =
					(wave_faction == _firefight_faction_flood) ? "ff_u_flood_c_spinner" :
					(wave_faction == _firefight_faction_sentinels || wave_faction == _firefight_faction_unsc_humans) ? "ff_u_sents_spinner" :
					"ff_u_grunts_spinner";
				if (named(list, fallback_spinner, 0))
					named(list, fallback_spinner, 0)->parameters.list.selected_index = 0;
			}
		}

		if (page == 2 && ff_unit_guard_warning_time != 0 &&
			system_milliseconds() - ff_unit_guard_warning_time < 3000 && description)
		{
			description->parameters.text_box.string_list_index = FF_UNIT_GUARD_HELP_STRING_INDEX;
			return;
		}
	}

	/* Hide conflicting vanilla MP settings when editing a Firefight variant */
	if (edit_variant() && edit_variant()->game_engine_index == game_engine_firefight)
	{
		if (named(list, "op_number_of_lives", 0))
		{
			short y = 73;

			visible_set(named(list, "op_number_of_lives", 0), FALSE);
			visible_set(named(list, "op_shields", 0), FALSE);
			visible_set(named(list, "op_odd_man_out", 0), FALSE);
			for (row = list->child; row; row = row->next)
			{
				if (row->visible && !strncmp(row->name, "op_", 3))
				{
					row->vertical_offset = y;
					y += 30;
				}
			}
			if (list->focused_child && !list->focused_child->visible && named(list, "op_maximum_health", 0))
				list->focused_child = named(list, "op_maximum_health", 0);
		}
		if (named(list, "op_autobalance", 0))
		{
			short y = 73;

			visible_set(named(list, "op_autobalance", 0), FALSE);
			for (row = list->child; row; row = row->next)
			{
				if (row->visible && !strncmp(row->name, "op_", 3))
				{
					row->vertical_offset = y;
					y += 30;
				}
			}
			if (list->focused_child && !list->focused_child->visible && named(list, "op_friendly_fire", 0))
				list->focused_child = named(list, "op_friendly_fire", 0);
		}
	}

	/* (Item Options: the weapon set's row for a category loadout, the two
	weapons' for a custom one) */
	if (named(list, "loadout_spinner", 0))
	{
		boolean custom = named(list, "loadout_spinner", 0)->parameters.list.selected_index == _loadout_custom;

		visible_set(named(list, "op_weapon_set", 0), !custom);
		visible_set(named(list, "op_primary_weapon", 0), custom);
		visible_set(named(list, "op_secondary_weapon", 0), custom);
	}
	/* (OPENCE OPTIONS: the mode's own rows, Infection's or Gun Game's, below
	MODE and HARDCORE) */
	if (named(list, "oce_mode_spinner", 0))
	{
		short mode = named(list, "oce_mode_spinner", 0)->parameters.list.selected_index;
		short place = 73 + 2 * 30;

		for (row = list->child; row; row = row->next)
		{
			boolean infection = !strncmp(row->name, "op_inf_", 7);

			if (!infection && strncmp(row->name, "op_gg_", 6))
				continue;
			row->visible = infection ? mode == _game_mode_infection : mode == _game_mode_gun_game;
			if (row->visible)
			{
				row->vertical_offset = place;
				place += 30;
			}
		}
	}
	/* (TEXT CHAT held at OFF while the game is listed: text_chat_locked) */
	if (named(list, TEXT_CHAT_SPINNER, 0) && p2p_hosting_listed())
		named(list, TEXT_CHAT_SPINNER, 0)->parameters.list.selected_index = TEXT_CHAT_OFF;
	/* CO-OP OPTIONS: enemies, pickups and vehicles share the amount rows.
	Mixed modes may need both rows at once. */
	if (named(list, "coop_extra_enemies_spinner", 0))
	{
		short mode = named(list, "coop_extra_enemies_spinner", 0)->parameters.list.selected_index;

		struct widget_instance *pickups = named(list, "coop_extra_pickups_spinner", 0);
		struct widget_instance *vehicles = named(list, "coop_extra_vehicles_spinner", 0);
		boolean per_player = mode == _cooperative_enemies_per_player ||
			(pickups && pickups->parameters.list.selected_index == _cooperative_enemies_per_player) ||
			(vehicles && vehicles->parameters.list.selected_index == _cooperative_enemies_per_player);
		boolean multiplier = mode == _cooperative_enemies_multiplier ||
			(pickups && pickups->parameters.list.selected_index == _cooperative_enemies_multiplier) ||
			(vehicles && vehicles->parameters.list.selected_index == _cooperative_enemies_multiplier);

		visible_set(named(list, "op_coop_enemies_per_player", 0), per_player);
		visible_set(named(list, "op_coop_enemies_multiplier", 0), multiplier);

		/* Hidden amounts must not reserve a blank row. Offsets are relative
		to the parent: moving only the row carries its labels and spinners
		with it when widget_instance_render_recursive draws and notes targets. */
		{
			short y = 73;

			for (row = list->child; row; row = row->next)
				if (row->visible && !strncmp(row->name, "op_coop_", 8))
				{
					row->vertical_offset = y;
					y += 30;
				}
		}
	}
	/* BOT OPTIONS: Campaign / Co-op / Firefight vs Standard Multiplayer */
	if (named(list, "bots_count_spinner", 0))
	{
		static short const bot_counts[14] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15 };
		struct widget_instance *count_sp = named(list, "bots_count_spinner", 0);
		boolean pve_bots = campaign_bots_screen_active || hosting_cooperative() ||
			(gametype_edit.setup && gametype_edit.setup_variant.game_engine_index == game_engine_firefight);
		short max_idx = 3;
		short y = 73;

		if (!pve_bots)
		{
			short max_p = maximum_players[PIN(*server_settings_maximum_players_index(), 0, NUMBEROF(maximum_players) - 1)];
			short max_bots = (short)MAX(0, max_p - 1);
			short k;

			max_idx = 0;
			for (k = 0; k < 14; k++)
			{
				if (bot_counts[k] <= max_bots)
					max_idx = k;
			}
		}
		if (count_sp->parameters.list.selected_index > max_idx)
		{
			if (count_sp->parameters.list.selected_index == 13 && max_idx < 13)
				count_sp->parameters.list.selected_index = max_idx;
			else
				count_sp->parameters.list.selected_index = 0;
		}
		visible_set(named(list, "op_bots_replace", 0), !pve_bots);
		visible_set(named(list, "op_bots_teams", 0), !pve_bots);
		visible_set(named(list, "op_bots_scoring", 0), pve_bots);
		for (row = list->child; row; row = row->next)
		{
			if (row->visible && !strncmp(row->name, "op_bots_", 8))
			{
				row->vertical_offset = y;
				y += 30;
			}
		}
	}
	/* (the server browser's filters, hidden: their helps are fewer than
	their values) */
	if (!description || !list->focused_child || !strncmp(list->name, "filters", 7))
		return;
	for (row = list->child; row; row = row->next)
	{
		struct widget_instance *spinner;

		for (spinner = row->child; spinner && spinner->type != 2 /* spinner */; spinner = spinner->next)
			;
		if (row == list->focused_child)
		{
			if (spinner)
				description->parameters.text_box.string_list_index = (short)(index + spinner->parameters.list.selected_index);
			return;
		}
		if (spinner)
			index += spinner->parameters.list.number_of_items;
	}
}

static char const *const engine_items[] =
{
	/* (the type list's rows, in order: game_engine_ctf and so on) */
	"gametype_select_ctf_item", "gametype_select_koth_item", "gametype_select_slayer_item",
	"gametype_select_oddball_item", "gametype_select_race_item", "gametype_select_firefight_item"
};
static long const engine_of_item[] = { 1, 4, 2, 3, 5, 6 };

/* "mp profile init game engine": the gametype's type focused */
static boolean gametype_engine_init(struct widget_instance *list)
{
	struct game_variant *variant = edit_variant();
	short index;
	short y = 100;

	if (!variant)
		return campaign_fail();

	if (multiplayer.firefight_mode && variant->game_engine_index != game_engine_firefight)
	{
		csmemset(&variant->game_engine_variant, 0, sizeof(variant->game_engine_variant));
		variant->game_engine_index = game_engine_firefight;
		variant->universal_variant.teams = TRUE;
		variant->game_engine_variant.firefight.starting_lives = 7;
		variant->game_engine_variant.firefight.bonus_lives_per_round = 1;
		variant->game_engine_variant.firefight.bonus_lives_per_set = 3;
		variant->game_engine_variant.firefight.max_lives_cap = 20;
		variant->game_engine_variant.firefight.difficulty = 2;
		variant->game_engine_variant.firefight.ordnance_drops = 2;
		variant->game_engine_variant.firefight.bonus_round_enabled = 1;
		variant->game_engine_variant.firefight.progression_skulls = 1;
		variant->game_engine_variant.firefight.vehicle_drops_enabled = 1;
		if (edit_options())
			firefight_variant_options_default(edit_options());
	}
	else if (!multiplayer.firefight_mode && variant->game_engine_index == game_engine_firefight)
	{
		csmemset(&variant->game_engine_variant, 0, sizeof(variant->game_engine_variant));
		variant->game_engine_index = game_engine_slayer;
	}

	for (index = 0; index < NUMBEROF(engine_items); index++)
	{
		struct widget_instance *row = named(list, engine_items[index], 0);
		boolean is_ff = (engine_of_item[index] == game_engine_firefight);
		boolean show = (multiplayer.gamemode == _gamemode_firefight || multiplayer.firefight_mode) ? is_ff :
			(multiplayer.gamemode == _gamemode_standard) ? !is_ff : FALSE;

		if (row)
		{
			visible_set(row, show);
			if (show)
			{
				row->vertical_offset = y;
				y += 30;
			}
		}
		if (show && engine_of_item[index] == variant->game_engine_index)
			focus_row(list, index);
	}
	return TRUE;
}

/* "mp profile set game engine" (a type's A): another type clears the type's
own rules (as the Xbox's) */
static boolean gametype_engine_set(struct widget_instance *item)
{
	struct game_variant *variant = edit_variant();
	short index;

	if (!variant)
		return campaign_fail();
	for (index = 0; index < NUMBEROF(engine_items); index++)
	{
		if (!strcmp(item->name, engine_items[index]))
		{
			if (variant->game_engine_index != engine_of_item[index])
			{
				csmemset(&variant->game_engine_variant, 0, sizeof(variant->game_engine_variant));
				if (engine_of_item[index] == game_engine_firefight)
				{
					variant->universal_variant.teams = TRUE;
					variant->game_engine_variant.firefight.starting_lives = 7;
					variant->game_engine_variant.firefight.bonus_lives_per_round = 1;
					variant->game_engine_variant.firefight.bonus_lives_per_set = 3;
					variant->game_engine_variant.firefight.max_lives_cap = 20;
					variant->game_engine_variant.firefight.difficulty = 2;
					variant->game_engine_variant.firefight.ordnance_drops = 2;
					variant->game_engine_variant.firefight.bonus_round_enabled = 1;
					variant->game_engine_variant.firefight.progression_skulls = 1;
					variant->game_engine_variant.firefight.vehicle_drops_enabled = 1;
					if (edit_options())
						firefight_variant_options_default(edit_options());
				}
			}
			variant->game_engine_index = engine_of_item[index];
			return TRUE;
		}
	}
	return campaign_fail();
}

/* "mp edit profile set rule text": the gametype's type (its string in
ui\multiplayer_game_text, as the Xbox's: capture the flag 3, slayer 4,
oddball 5, king of the hill 6, race 7, unknown 8, firefight 191) */
static void gametype_engine_name(struct widget_instance *text_box)
{
	static short const strings[] = { 8, 3, 4, 5, 6, 7, 191 };
	struct game_variant *variant = edit_variant();

	if (variant)
		text_box->parameters.text_box.string_list_index = strings[PIN(variant->game_engine_index, 0, 6)];
}

/* ---------- public code */

boolean pc_menu_event_function_invoke(
	struct widget_instance *widget,
	struct event_record *event,
	long function_index,
	boolean *widget_deleted)
{
	short controller = event_controller(widget, event);

	switch (function_index)
	{
	case _pc_menu_function_quit_game:
		platform_request_quit();
		return TRUE;
	case _pc_menu_function_setting_load:
		return setting_load(widget);
	case _pc_menu_function_setting_save:
		return setting_save(widget);
	}
	{
		char const *name = pc_menu_function_name(function_index);

		if (!name)
			return FALSE;
		if (!strcmp(name, "main menu quit game"))
		{
			platform_request_quit();
		}
		else if (!strcmp(name, "profile set edit begin"))
		{
			return profile_edit_begin(widget, event, widget_deleted);
		}
		else if (!strcmp(name, "new campaign decision"))
		{
			return new_profile_decision(widget, event, widget_deleted);
		}
		/* (the press posted is the controller's that chose the button: a
		split screen player's LEAVE is theirs) */
		else if (!strcmp(name, "mouse emit accept event"))
		{
			event_manager_post_button(controller, BUTTON_A);
		}
		else if (!strcmp(name, "mouse emit back event"))
		{
			event_manager_post_button(controller, BUTTON_B);
		}
		else if (!strcmp(name, "mouse emit x event"))
		{
			event_manager_post_button(controller, BUTTON_X);
		}
		else if (!strcmp(name, "emit custom activation event"))
		{
			struct widget_instance *top = widget;

			while (top->parent)
				top = top->parent;
			boolean deleted;

			ui_widget_port_dispatch_event(top, EVENT_CUSTOM_ACTIVATION, event_controller(widget, event), &deleted);
			if (deleted)
				*widget_deleted = TRUE;
		}
		else if (!strcmp(name, "mp type set mode"))
		{
			lan_mode = strstr(widget->name, "_lan_") != NULL;
			multiplayer_mode_set(widget);
			/* (Create's: the map list only for a game made, "join controller
			to mp game" before; it fails with the game's port in use, by
			another copy of the game) */
			if ((multiplayer.mode == _multiplayer_mode_host_internet ||
				multiplayer.mode == _multiplayer_mode_host_lan ||
				multiplayer.mode == _multiplayer_mode_host_local) && !global_network_game_server_get())
			{
				platform_log("menus: the game could not be made (is its port in use?)");
				return campaign_fail();
			}
		}
		else if (!strcmp(name, "gamespy screen init"))
		{
			return browser_initialize(widget, event, widget_deleted);
		}
		else if (!strcmp(name, "gamespy select item") || !strcmp(name, "gamespy select button"))
		{
			return browser_select(widget, event, controller, widget_deleted);
		}
		else if (!strcmp(name, "gamespy back handler") && text_field_editing(NULL))
		{
			text_field_end(FALSE);
		}
		else if (!strcmp(name, "port setup edit"))
		{
			return gametype_setup_edit();
		}
		/* (co-op's options' screens: their settings shown, and OK's) */
		else if (!strcmp(name, "port setup options init"))
		{
			setting_options_each(widget, FALSE);
		}
		else if (!strcmp(name, "port setup options save"))
		{
			struct widget_instance *list = gametype_options_list(widget);

			if (list)
				setting_options_each(list, TRUE);
			if (campaign_bots_screen_active && list && named(list, "bots_count_spinner", 0))
			{
				campaign_bots_screen_active = FALSE;
				campaign_start(campaign_pending.map_name, campaign_pending.difficulty, campaign_pending.controller);
				return TRUE;
			}
			ui_play_audio_feedback_sound(SOUND_FORWARD);
			ui_widget_port_go_back(widget);
			*widget_deleted = TRUE;
			return TRUE;
		}
		else if (!strcmp(name, "mp profile save changes"))
		{
			return ui_widget_port_gametype_save(widget, widget_deleted);
		}
		else if (!strcmp(name, "mp profile begin editing"))
		{
			return gametype_edit_begin();
		}
		else if (!strcmp(name, "request del playlist profile"))
		{
			return gametype_delete_request();
		}
		else if (!strcmp(name, "final del playlist profile"))
		{
			return gametype_delete_final();
		}
		else if (!strcmp(name, "mp profile init game engine"))
		{
			return gametype_engine_init(widget);
		}
		else if (!strcmp(name, "mp profile set game engine"))
		{
			return gametype_engine_set(widget);
		}
		else if (!strncmp(name, "mp profile init ", 16) || !strcmp(name, "mp prof init teamplay options") ||
			!strcmp(name, "mp prof init vehicle options"))
		{
			return gametype_options_init(widget);
		}
		else if ((!strncmp(name, "mp profile set ", 15) && strcmp(name, "mp profile set for game")) ||
			!strcmp(name, "mp prof save teamplay options") || !strcmp(name, "mp prof save vehicle options"))
		{
			return gametype_options_save(widget);
		}
		else if (!strcmp(name, "port lobby preview join"))
		{
			return preview_join(widget, controller, widget_deleted);
		}
		else if (!strcmp(name, "gamespy screen dispose"))
		{
			lobby_browser_end();
		}
		else if (!strcmp(name, "port pause end game"))
		{
			return pause_end_game();
		}
		else if (!strcmp(name, "port coop friendly fire"))
		{
			return coop_friendly_fire_change();
		}
		else if (!strcmp(name, "player profile save changes"))
		{
			return profile_save_changes(widget, widget_deleted);
		}
		else if (!strcmp(name, "port profile settings save"))
		{
			return profile_settings_save(widget);
		}
		else if (!strcmp(name, "direct ip connect go"))
		{
			return direct_link_from_clipboard();
		}
		else if (!strcmp(name, "join controller to mp game"))
		{
			return multiplayer_host(widget, event, controller, widget_deleted);
		}
		else if (!strcmp(name, "mp level list initialize"))
		{
			return map_list_initialize(widget);
		}
		else if (!strcmp(name, "mp level select"))
		{
			return map_list_choose(widget, widget_deleted);
		}
		else if (!strcmp(name, "port map list back"))
		{
			return map_list_back(widget, widget_deleted);
		}
		else if (!strcmp(name, "mp profiles list initialize"))
		{
			if (!strcmp(widget->name, "playlist_select_list"))
				return gametype_edit_list_initialize(widget);
			return gametype_list_initialize(widget);
		}
		else if (!strcmp(name, "mp profile set for game"))
		{
			return gametype_choose(widget);
		}
		else if (!strcmp(name, "server settings init"))
		{
			return server_settings_initialize(widget);
		}
		else if (!strcmp(name, "ss edit server name"))
		{
			return server_name_edit(widget);
		}
		else if (!strcmp(name, "ss edit server password"))
		{
			return server_password_edit(widget);
		}
		else if (!strcmp(name, "port password init"))
		{
			return password_screen_initialize(widget);
		}
		else if (!strcmp(name, "port password edit"))
		{
			return password_screen_edit(widget, widget_deleted);
		}
		else if (!strcmp(name, "port password join"))
		{
			return password_screen_join(widget, widget_deleted);
		}
		else if (!strcmp(name, "port password back"))
		{
			return password_screen_back(widget, widget_deleted);
		}
		else if (!strcmp(name, "ss copy invite"))
		{
			return invite_copy();
		}
		else if (!strcmp(name, "ss start game"))
		{
			return server_start();
		}
		else if (!strcmp(name, "single prev cl item activated"))
		{
			item_activated(widget, event_controller(widget, event), widget_deleted);
		}
		else if (!strcmp(name, "port settings save"))
		{
			settings_each(screen_of(widget), setting_changed_save);
			platform_display_apply();
		}
		else if (!strcmp(name, "port settings defaults"))
		{
			settings_each(screen_of(widget), setting_default_show);
		}
		else if (!strcmp(name, "port mods init") || !strcmp(name, "port mods close"))
		{
			return mods_screen_reset();
		}
		else if (!strcmp(name, "port mods toggle"))
		{
			return mods_toggle(widget);
		}
		else if (!strcmp(name, "port mods move up") || !strcmp(name, "port mods move down"))
		{
			return mods_move(widget, (short)(!strcmp(name, "port mods move up") ? -1 : 1));
		}
		else if (!strcmp(name, "port mods save"))
		{
			return mods_save();
		}
		else if (!strcmp(name, "controls screen init") || !strcmp(name, "controls screen defaults"))
		{
			return controls_load(!strcmp(name, "controls screen defaults"));
		}
		else if (!strcmp(name, "controls screen change set"))
		{
			return controls_save();
		}
		else if (!strcmp(name, "controls begin binding"))
		{
			return control_capture_begin(widget);
		}
		else if (!strcmp(name, "player profile list initialize"))
		{
			return profile_list_initialize(widget);
		}
		else if (!strcmp(name, "profile manager select"))
		{
			return profile_choose(widget, controller);
		}
		else if (!strcmp(name, "port lobby open"))
		{
			return lobby_join_reset();
		}
		else if (!strcmp(name, "port lobby add player"))
		{
			return lobby_add_player();
		}
		else if (!strcmp(name, "port lobby join"))
		{
			return lobby_join_start(widget, controller, widget_deleted);
		}
		else if (!strcmp(name, "port lobby leave"))
		{
			return lobby_leave(widget, event, controller, widget_deleted);
		}
		else if (!strcmp(name, "port lobby player list initialize"))
		{
			return lobby_player_list_initialize(widget);
		}
		else if (!strcmp(name, "port lobby player choose"))
		{
			return lobby_player_choose();
		}
		else if (!strcmp(name, "port lobby preview add"))
		{
			return preview_add(widget, controller, widget_deleted);
		}
		else if (!strcmp(name, "port lobby preview leave"))
		{
			return preview_leave(controller);
		}
		else if (!strcmp(name, "port coop begin"))
		{
			return coop_begin(controller);
		}
		else if (!strcmp(name, "port coop player 2 list initialize"))
		{
			return coop_player2_list_initialize(widget);
		}
		else if (!strcmp(name, "port coop player 2"))
		{
			return coop_player2_choose(controller);
		}
		else if (!strcmp(name, "request del player profile"))
		{
			return profile_delete_request();
		}
		else if (!strcmp(name, "final del player profile"))
		{
			return profile_delete();
		}
		else if (!strcmp(name, "color picker menu initialize"))
		{
			return color_list_initialize(widget);
		}
		else if (!strcmp(name, "color picker select color"))
		{
			return color_choose();
		}
		else if (!strcmp(name, "controls binding slot"))
		{
			controls_screen.slot = (short)!controls_screen.slot;
		}
		else if (!strcmp(name, "campaign menu init"))
		{
			return campaign_menu_initialize(controller);
		}
		else if (!strcmp(name, "campaign menu continue"))
		{
			return campaign_continue(controller);
		}
		else if (!strcmp(name, "initialize sp level list solo"))
		{
			return level_list_initialize(widget, controller);
		}
		else if (!strcmp(name, "solo level set map"))
		{
			return level_choose(widget, controller);
		}
		else if (!strcmp(name, "difficulty item select"))
		{
			return difficulty_start_with_screens(widget, sibling_index(widget), controller, widget_deleted);
		}
		else if (!strcmp(name, "set difficulty"))
		{
			short difficulty = PIN(difficulty_shown(widget), 0, 3);
			boolean result = difficulty_start_with_screens(widget, difficulty, controller, widget_deleted);

			*widget_deleted = TRUE;
			return result;
		}
		else if (!strcmp(name, "ss skulls"))
		{
			return server_campaign_screen(widget, widget_deleted);
		}
		else if (!strcmp(name, "skull toggle"))
		{
			return skull_toggle(widget);
		}
		else if (!strcmp(name, "skulls toggle all"))
		{
			struct skull_set unlocked = skulls_unlocked_set();
			struct skull_set on = skull_set_intersection(skulls_menu_set, unlocked);

			skulls_menu_set = skull_set_is_empty(&on) ? unlocked : skull_set_none();
			ui_play_audio_feedback_sound(skull_set_is_empty(&on) ? SOUND_FORWARD : SOUND_BACK);
		}
		else if (!strcmp(name, "skulls start"))
		{
			return skulls_start(widget, widget_deleted);
		}
		else if (!strcmp(name, "skulls reset progress"))
		{
			skulls_reset_progress(widget);
		}
		else if (!strcmp(name, "skulls unlock all"))
		{
			return skulls_unlock_choose(widget, FALSE, FALSE, TRUE, widget_deleted);
		}
		else if (!strcmp(name, "skulls unlock play"))
		{
			return skulls_question_open("pc\\main_menu\\difficulty_select\\skulls\\death_screen", TRUE,
				widget_deleted);
		}
		else if (!strcmp(name, "skulls unlock pickup"))
		{
			return skulls_unlock_choose(widget, TRUE, TRUE, TRUE, widget_deleted);
		}
		else if (!strcmp(name, "skulls death lose"))
		{
			return skulls_unlock_choose(widget, TRUE, FALSE, TRUE, widget_deleted);
		}
		else if (!strcmp(name, "skulls death keep"))
		{
			return skulls_unlock_choose(widget, TRUE, FALSE, FALSE, widget_deleted);
		}
		else if (!strcmp(name, "skulls death back"))
		{
			return skulls_question_open("pc\\main_menu\\difficulty_select\\skulls\\unlock_screen", FALSE,
				widget_deleted);
		}
		else if (!strcmp(name, "skulls up"))
		{
			return skulls_move(widget, 0, -1);
		}
		else if (!strcmp(name, "skulls down"))
		{
			return skulls_move(widget, 0, 1);
		}
		else if (!strcmp(name, "skulls left"))
		{
			return skulls_move(widget, -1, 0);
		}
		else if (!strcmp(name, "skulls right"))
		{
			return skulls_move(widget, 1, 0);
		}
		else if (!strcmp(name, "load game menu init"))
		{
			return saved_game_list_initialize(widget, controller);
		}
		else if (!strcmp(name, "load game menu activated"))
		{
			return saved_game_continue(controller);
		}
		else if (!strcmp(name, "load game menu delete request"))
		{
			return saved_game_delete_request();
		}
		else if (!strcmp(name, "load game menu delete finish"))
		{
			return saved_game_delete(controller);
		}
		else if (!strcmp(name, "gamemode menu init"))
		{
			return gamemode_menu_init(widget);
		}
		else if (!strcmp(name, "gamemode select standard"))
		{
			return gamemode_select_standard();
		}
		else if (!strcmp(name, "gamemode select ranked"))
		{
			return gamemode_select_ranked();
		}
		else if (!strcmp(name, "gamemode select firefight"))
		{
			return gamemode_select_firefight();
		}
		else if (!strcmp(name, "gamemode select forge"))
		{
			return gamemode_select_forge();
		}
		else if (!strcmp(name, "gamemode select theater"))
		{
			return gamemode_select_theater();
		}
		else if (!strcmp(name, "gamemode select creation hub"))
		{
			return gamemode_select_creation_hub();
		}
		else if (!strcmp(name, "gamemode select dedi server"))
		{
			return gamemode_select_dedi_server();
		}
		else if (!strcmp(name, "port tags init"))
		{
			return tags_screen_reset(widget);
		}
		else if (!strcmp(name, "port tags close"))
		{
			return tags_screen_close();
		}
		else if (!strcmp(name, "port tags choose") || !strcmp(name, "port tags select"))
		{
			return tags_choose(widget, widget_deleted);
		}
		else if (!strcmp(name, "port tags left"))
		{
			return tags_step(widget, -1);
		}
		else if (!strcmp(name, "port tags right"))
		{
			return tags_step(widget, 1);
		}
		else if (!strcmp(name, "port tags back"))
		{
			return tags_back(widget, widget_deleted);
		}
		else if (!strcmp(name, "port tags new profile") || !strcmp(name, "port tags test"))
		{
			return tags_new_profile(widget);
		}
		else if (!strcmp(name, "port tags profile button"))
		{
			return tags_profile_button(widget);
		}
		else if (!strcmp(name, "port theater init") || !strcmp(name, "theater screen init"))
		{
			return theater_refresh(widget);
		}
		else if (!strcmp(name, "port theater watch") || !strcmp(name, "theater select"))
		{
			return theater_watch(widget);
		}
		else if (!strcmp(name, "port theater record"))
		{
			return theater_record();
		}
		else if (!strcmp(name, "port theater delete"))
		{
			return theater_delete(widget);
		}
		else if (!strcmp(name, "port theater export"))
		{
			return theater_export();
		}
		else if (!strcmp(name, "dedi server init"))
		{
			return dedi_server_init(widget);
		}
		else if (!strcmp(name, "dedi server choose") || !strcmp(name, "dedi server select"))
		{
			return dedi_server_choose(widget, event, widget_deleted);
		}
		else if (!strcmp(name, "dedi server left"))
		{
			return dedi_server_step(widget, -1);
		}
		else if (!strcmp(name, "dedi server right"))
		{
			return dedi_server_step(widget, 1);
		}
		else if (!strcmp(name, "dedi server save"))
		{
			return dedi_server_save(widget);
		}
		else if (!strcmp(name, "dedi server copy"))
		{
			return dedi_server_open_web(widget);
		}
		else if (!strcmp(name, "service record init"))
		{
			return service_record_init(widget);
		}
		else if (!strcmp(name, "service record select tab"))
		{
			return service_record_select_tab(widget);
		}
		else if (!strcmp(name, "service record medals prev"))
		{
			return service_record_medals_prev(widget);
		}
		else if (!strcmp(name, "service record medals next"))
		{
			return service_record_medals_next(widget);
		}
		else if (!strcmp(name, "service record check leaderboards"))
		{
			return service_record_check_leaderboards(widget);
		}
		else if (!strcmp(name, "service record export json"))
		{
			return service_record_export_json(widget);
		}
		else if (!strcmp(name, "service record copy id"))
		{
			return service_record_copy_id(widget);
		}
		else if (!strcmp(name, "service record link account"))
		{
			return service_record_link_account(widget);
		}
		else if (!strcmp(name, "controls back handler") || !strcmp(name, "gamespy back handler") ||
			!strcmp(name, "gamespy dismiss error") || !strcmp(name, "gamespy dismiss filters"))
		{
			ui_widget_port_go_back(widget);
			*widget_deleted = TRUE;
		}
		return TRUE;
	}
}

void pc_menu_game_data_function_invoke(
	struct widget_instance *widget,
	long function)
{
	char const *name = pc_menu_game_data_input_name(function);

	if (!name)
		return;
	/* (a text field whose screen has gone: let go of) */
	if (text_field.row && system_milliseconds() - text_field_shown_time > 500)
		text_field_end(FALSE);
	if (!strcmp(name, "solo map list update"))
		level_list_update(widget);
	else if (!strcmp(name, "mp map list update"))
		map_list_update(widget);
	else if (!strcmp(name, "gt select list update"))
		gametype_list_update(widget);
	else if (!strcmp(name, "server settings update"))
		server_settings_update(widget);
	else if (!strcmp(name, "port password update"))
		password_screen_update(widget);
	else if (!strcmp(name, "gamespy screen update"))
		browser_update(widget);
	else if (!strcmp(name, "gt edit list update"))
		gametype_edit_list_update(widget);
	else if (!strcmp(name, "get edit game settings name"))
		gametype_edit_name(widget);
	else if (!strcmp(name, "game settings lists text update"))
		gametype_option_help(widget);
	else if (!strcmp(name, "mp edit profile set rule text"))
		gametype_engine_name(widget);
	else if (!strcmp(name, "mp prof vehicles update"))
		vehicles_update(widget);
	else if (!strcmp(name, "port lobby preview update"))
		preview_update(widget);
	else if (!strcmp(name, "port lobby update"))
		lobby_update(widget);
	else if (!strcmp(name, "port settings help"))
	{
		video_rows_show(widget);
		hud_button_style_row_show(widget);
		settings_help(widget);
	}
	else if (!strcmp(name, "controls update menu"))
		controls_update(widget);
	else if (!strcmp(name, "port mods update"))
		mods_update(widget);
	else if (!strcmp(name, "color picker update"))
		color_list_update(widget);
	else if (!strcmp(name, "3wide player profile list update"))
		profile_list_update(widget);
	else if (!strcmp(name, "load game list update"))
		saved_game_list_update(widget);
	else if (!strcmp(name, "port gamepad layout preview"))
		profile_gamepad_layout(widget);
	else if (!strcmp(name, "skulls screen update"))
		skulls_screen_update(widget);
	else if (!strcmp(name, "gameplay screen update"))
		gameplay_screen_update(widget);
	else if (!strcmp(name, "port coop friendly fire text"))
		coop_friendly_fire_text(widget);
	else if (!strcmp(name, "port playlist edit desc"))
		playlist_edit_description(widget);
	else if (!strcmp(name, "gamemode menu update"))
		gamemode_menu_update(widget);
	else if (!strcmp(name, "mp type screen update"))
		mp_type_screen_update(widget);
	else if (!strcmp(name, "service record update"))
		service_record_update(widget);
	else if (!strcmp(name, "port tags update"))
		tags_update(widget);
	else if (!strcmp(name, "port theater update") || !strcmp(name, "theater screen update"))
		theater_update(widget);
	else if (!strcmp(name, "dedi server update"))
		dedi_server_update(widget);
}

extern void draw_quad(rectangle2d const *rect, pixel32 color);

static void menu_draw_quad_rect(short x0, short y0, short x1, short y1, pixel32 color)
{
	rectangle2d r;

	if (x1 <= x0 || y1 <= y0)
		return;
	r.x0 = x0;
	r.y0 = y0;
	r.x1 = x1;
	r.y1 = y1;
	draw_quad(&r, color);
}

static void menu_draw_unicode_text(
	short x0,
	short y0,
	short x1,
	short y1,
	long font_tag,
	short justification,
	real_argb_color const *color,
	wchar_t const *text)
{
	rectangle2d bounds;

	if (font_tag == NONE || !text || !text[0])
		return;
	bounds.x0 = x0;
	bounds.y0 = y0;
	bounds.x1 = x1;
	bounds.y1 = y1;
	draw_string_set_draw_mode(font_tag, NONE, justification, 0, color);
	rasterizer_draw_unicode_string(&bounds, &bounds, NULL, 0, text);
}

static void menu_draw_unicode_text_scaled_vcentered(
	short x0,
	short x1,
	real mid_y,
	long font_tag,
	real base_scale,
	real_argb_color const *color,
	wchar_t const *text)
{
	rectangle2d measure_bounds, text_bounds, cursor_bounds, draw_bounds;
	short cap_mid;
	real scale = base_scale;
	real avail_w = (real)(x1 - x0);
	real raw_w;
	short draw_y;

	if (font_tag == NONE || !text || !text[0] || x1 <= x0)
		return;

	measure_bounds.x0 = 0;
	measure_bounds.y0 = 0;
	measure_bounds.x1 = 2000;
	measure_bounds.y1 = 200;
	draw_string_set_draw_mode(font_tag, NONE, 0, 0, color);
	draw_unicode_string_compute_bounds(&measure_bounds, text, &text_bounds, &cursor_bounds);
	raw_w = (real)(text_bounds.x1 - text_bounds.x0);
	if (raw_w * scale > avail_w && raw_w > 1.0f)
	{
		scale = avail_w / raw_w;
		if (scale < 0.50f)
			scale = 0.50f;
	}

	cap_mid = draw_unicode_string_capital_middle(&measure_bounds, text);
	if (cap_mid <= 0)
		cap_mid = 9;
	draw_y = (short)(mid_y - (real)cap_mid * scale + 0.5f);

	draw_bounds.x0 = x0;
	draw_bounds.y0 = draw_y;
	draw_bounds.x1 = (short)(x0 + (short)(avail_w / scale) + 32);
	draw_bounds.y1 = (short)(draw_y + 64);

	rasterizer_text_set_scale(scale, (real)x0, (real)draw_y);
	draw_string_set_draw_mode(font_tag, NONE, 0, 0, color);
	rasterizer_draw_unicode_string(&draw_bounds, &draw_bounds, NULL, 0, text);
	rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);
}

void menu_functions_post_render(void *active_widget, boolean at_main_menu)
{
	struct widget_instance *screen = (struct widget_instance *)active_widget;
	static struct player_profile cached_profile;
	static long cached_index = NONE;
	static unsigned long cached_time = 0;
	static boolean cached_valid = FALSE;
	static long cached_stats_index = NONE;
	static unsigned long cached_stats_time = 0;
	static struct firefight_mcc_rank_info cached_rank_info;
	static struct firefight_h3_ranked_info cached_h3_ranked;
	static struct firefight_emblem_info cached_emblem;
	static long sel_cached_index = NONE;
	static struct firefight_mcc_rank_info sel_cached_rank;
	static struct firefight_emblem_info sel_cached_emblem;
	long active_index;
	long large_font;
	long small_font;
	unsigned long now_ms;

	if (!screen)
		return;

	now_ms = system_milliseconds();
	large_font = tag_loaded('font', "ui\\large_ui");
	small_font = tag_loaded('font', "ui\\small_ui");
	if (small_font == NONE)
		small_font = large_font;

	/* Resolve Player 1's active or last-used profile */
	active_index = player_ui_get_active_player_profile_index(0);
	if (active_index != NONE)
	{
		player_ui_get_active_player_profile(0, &cached_profile);
		cached_index = active_index;
		cached_valid = TRUE;
	}
	else
	{
		active_index = player_ui_get_player1_last_used_profile_index();
		if (active_index == NONE && profile_list.count > 0)
			active_index = profile_list.indices[0];
		if (active_index != NONE &&
			(active_index != cached_index || now_ms - cached_time > 1000))
		{
			cached_index = active_index;
			cached_valid = player_profile_get(active_index, &cached_profile);
			cached_time = now_ms;
		}
	}

	/* 1. Top-Right Menu UI Player Card (Rank Icon + Custom Emblem + Player Name) */
	if (at_main_menu && cached_valid)
	{
		struct firefight_mcc_rank_info rank_info;
		struct firefight_emblem_info emblem;
		real_rgb_color armor_rgb = player_profile_get_rgb_color(
			PIN(cached_profile.primary_color_index, 0, COLOR_COUNT - 1));
		pixel32 accent_col = (0xFFU << 24) |
			((unsigned long)(PIN((long)(armor_rgb.red * 255.0f + 0.5f), 0L, 255L)) << 16) |
			((unsigned long)(PIN((long)(armor_rgb.green * 255.0f + 0.5f), 0L, 255L)) << 8) |
			(unsigned long)(PIN((long)(armor_rgb.blue * 255.0f + 0.5f), 0L, 255L));
		real_argb_color name_col = { 1.0f, 0.95f, 0.98f, 1.0f };
		short screen_right = (short)(640 + (halo_screen_width() - 640) / 2);
		short card_x1 = (short)(screen_right - 6);
		short card_x0 = (short)(card_x1 - 236);
		short card_y0 = 6, card_y1 = 40;

		if (cached_stats_index != cached_index || now_ms - cached_stats_time > 1500)
		{
			memset(&cached_rank_info, 0, sizeof(cached_rank_info));
			memset(&cached_h3_ranked, 0, sizeof(cached_h3_ranked));
			firefight_stats_get_mcc_rank(cached_index, &cached_rank_info);
			firefight_stats_get_h3_ranked(cached_index, &cached_h3_ranked);
			firefight_stats_get_emblem(cached_index, &cached_emblem);
			cached_stats_index = cached_index;
			cached_stats_time = now_ms;
		}
		rank_info = cached_rank_info;
		emblem = cached_emblem;
		if (profile_list.step == PROFILE_STEP_EMBLEM &&
			profile_list.chosen >= 0 && profile_list.chosen < profile_list.count &&
			profile_list.indices[profile_list.chosen] == cached_index)
		{
			emblem = profile_list.emblem;
			cached_emblem = profile_list.emblem;
		}

		{
			short text_x0 = (short)(card_x0 + 100);
			short text_x1 = (short)(card_x1 - 8);
			real card_mid_y = 0.5f * (real)(card_y0 + card_y1);

			/* MCC Nameplate Card frame & armor color stripe */
			firefight_draw_mcc_nameplate(card_x0, card_y0, card_x1, card_y1, emblem.nameplate, accent_col, 1.0f);

			/* Custom Emblem (left) + MCC Shield & Level Icon (middle) + Halo 3 Ranked Emblem (right of shield) */
			firefight_draw_player_emblem((short)(card_x0 + 8), (short)(card_y0 + 4), 26, &emblem, 1.0f);
			firefight_draw_mcc_rank_icon((short)(card_x0 + 38), (short)(card_y0 + 4), 26,
				rank_info.tour_number, rank_info.rank_in_tour, 1.0f);
			firefight_draw_h3_ranked_icon((short)(card_x0 + 68), (short)(card_y0 + 4), 26,
				cached_h3_ranked.rank_index, 1.0f);

			/* Player Name slightly smaller (0.80x) and vertically centered on the nameplate backdrop */
			menu_draw_unicode_text_scaled_vcentered(
				text_x0,
				text_x1,
				card_mid_y,
				large_font,
				0.80f,
				&name_col,
				cached_profile.player_name);
		}
	}

	/* 2. Profiles -> EDIT PROFILE / CUSTOMISE EMBLEM / CHANGE COLOUR overlays */
	if (named(screen, "profile_manager_select_list_2", 0))
	{
		if (profile_list.step != PROFILE_STEP_SELECT)
		{
			real_argb_color hdr_col = { 1.0f, 0.16f, 0.59f, 1.0f };
			wchar_t const *hdr_text =
				(profile_list.step == PROFILE_STEP_EMBLEM) ? L"CUSTOMISE EMBLEM" :
				(profile_list.step == PROFILE_STEP_COLOR) ? L"CHANGE COLOUR" :
				L"EDIT PROFILE";

			menu_draw_unicode_text(35, 14, 420, 48, large_font, 0, &hdr_col, hdr_text);
			menu_draw_quad_rect(35, 38, 360, 40, 0xCC2896FF);
		}

		if (profile_list.chosen >= 0 && profile_list.chosen < profile_list.count)
		{
			long sel_index = profile_list.indices[profile_list.chosen];
			struct firefight_mcc_rank_info sel_rank;
			struct firefight_emblem_info sel_emblem;
			short t_num, r_in_tour;

			if (sel_cached_index != sel_index)
			{
				memset(&sel_cached_rank, 0, sizeof(sel_cached_rank));
				firefight_stats_get_mcc_rank(sel_index, &sel_cached_rank);
				firefight_stats_get_emblem(sel_index, &sel_cached_emblem);
				sel_cached_index = sel_index;
				if (sel_index == cached_index)
				{
					cached_rank_info = sel_cached_rank;
					cached_emblem = sel_cached_emblem;
					cached_stats_index = cached_index;
					cached_stats_time = now_ms;
				}
			}
			sel_rank = sel_cached_rank;
			if (profile_list.step == PROFILE_STEP_EMBLEM)
			{
				sel_emblem = profile_list.emblem;
				sel_cached_emblem = profile_list.emblem;
			}
			else
			{
				sel_emblem = sel_cached_emblem;
			}
			t_num = sel_rank.tour_number > 0 ? sel_rank.tour_number : 1;
			r_in_tour = sel_rank.rank_in_tour > 0 ? sel_rank.rank_in_tour : 1;

			if (profile_list.step == PROFILE_STEP_EMBLEM)
			{
				real_argb_color lbl_col = { 1.0f, 0.16f, 0.59f, 1.0f };
				real_argb_color white_col = { 1.0f, 0.95f, 0.98f, 1.0f };
				wchar_t r_wide[32];
				wchar_t t_wide[48];

				/* Large Live Emblem Preview Box */
				menu_draw_quad_rect(456, 114, 562, 220, 0xCC050E1A);
				menu_draw_quad_rect(456, 114, 562, 116, 0xFF2896FF);
				menu_draw_quad_rect(456, 218, 562, 220, 0xFF2896FF);
				menu_draw_quad_rect(456, 114, 458, 220, 0xFF2896FF);
				menu_draw_quad_rect(560, 114, 562, 220, 0xFF2896FF);
				firefight_draw_player_emblem(463, 121, 92, &sel_emblem, 1.0f);

				menu_draw_unicode_text(432, 228, 586, 248, small_font, 2, &lbl_col, L"LIVE EMBLEM PREVIEW");

				/* MCC Nameplate + Rank & Tour Card below preview */
				firefight_draw_mcc_nameplate(438, 256, 580, 314, sel_emblem.nameplate, 0, 1.0f);
				firefight_draw_mcc_rank_icon(444, 265, 36, t_num, r_in_tour, 1.0f);
				text_widen(r_wide, NUMBEROF(r_wide), sel_rank.rank_name ? sel_rank.rank_name : "Recruit");
				text_widen(t_wide, NUMBEROF(t_wide), sel_rank.tour_name ? sel_rank.tour_name : "Tour 1 (Bronze)");
				menu_draw_unicode_text(486, 266, 576, 286, large_font, 0, &white_col, r_wide);
				menu_draw_unicode_text(486, 286, 576, 306, small_font, 0, &lbl_col, t_wide);

				menu_draw_unicode_text(432, 332, 586, 370, small_font, 2, &lbl_col,
					L"LEFT/RIGHT OR CLICK\r\nTO CUSTOMISE");
			}
			else
			{
				/* Draw Custom Emblem & MCC Rank Icon in the top corners of the Spartan preview box */
				firefight_draw_player_emblem(435, 108, 28, &sel_emblem, 1.0f);
				firefight_draw_mcc_rank_icon(553, 108, 28, t_num, r_in_tour, 1.0f);
			}
		}
	}

	/* 3. Bot & Scoring Options Screen: Live Campaign / Firefight Score Multiplier Display */
	if (named(screen, "bots_options_menu", 0))
	{
		struct widget_instance *list = named(screen, "bots_options_menu", 0);
		boolean pve_bots = campaign_bots_screen_active || hosting_cooperative() ||
			(gametype_edit.setup && gametype_edit.setup_variant.game_engine_index == game_engine_firefight);

		if (pve_bots)
		{
			struct widget_instance *count_sp = named(list, "bots_count_spinner", 0);
			struct widget_instance *skill_sp = named(list, "bots_skill_spinner", 0);
			struct widget_instance *score_sp = named(list, "bots_scoring_spinner", 0);
			short bot_cnt = count_sp ? PIN(count_sp->parameters.list.selected_index, 0, 3) : 0;
			short bot_sk = skill_sp ? PIN(skill_sp->parameters.list.selected_index, 0, 3) : 1;
			boolean score_on = score_sp ? (score_sp->parameters.list.selected_index == 0) :
				(config_boolean("game.campaign_score") != 0);
			short diff = campaign_bots_screen_active ? PIN(campaign_pending.difficulty, 0, 3) :
				(!hosting_cooperative() && gametype_edit.setup &&
				 gametype_edit.setup_variant.game_engine_index == game_engine_firefight) ?
				PIN(gametype_edit.setup_variant.game_engine_variant.firefight.difficulty, 0, 3) :
				PIN(main_get_difficulty(), 0, 3);
			float diff_mult = campaign_score_difficulty_multiplier_for(diff);
			float skull_mult = (!hosting_cooperative() && gametype_edit.setup &&
				gametype_edit.setup_variant.game_engine_index == game_engine_firefight) ? 1.0f :
				campaign_score_skulls_multiplier();
			float base_mult = diff_mult * skull_mult;
			float bot_pen = (bot_cnt > 0) ? bots_difficulty_multiplier_penalty(bot_sk) : 0.0f;
			float final_mult = (base_mult <= 0.0f) ? 0.0f : MAX(0.0f, base_mult - bot_pen);
			real_argb_color cyan_col = { 1.0f, 0.16f, 0.59f, 1.0f };
			real_argb_color white_col = { 1.0f, 0.95f, 0.98f, 1.0f };
			real_argb_color warn_col = { 1.0f, 1.0f, 0.55f, 0.25f };
			real_argb_color green_col = { 1.0f, 0.45f, 0.95f, 0.65f };
			char buf_a[96], buf_b[96], buf_c[96];
			wchar_t wbuf[96];

			firefight_draw_ce_rounded_box(54, 174, 566, 260, 0, 1.0f, 0xFFFFFFFFUL, 0xCC061326UL, 0xFF2896FFUL);

			snprintf(buf_a, sizeof(buf_a), "BASE MULTIPLIER (DIFFICULTY %.2fX * SKULLS %.2fX): %.2fX",
				diff_mult, skull_mult, base_mult);
			text_widen(wbuf, NUMBEROF(wbuf), buf_a);
			menu_draw_unicode_text(68, 182, 554, 202, small_font, 0, &cyan_col, wbuf);

			if (bot_cnt > 0)
			{
				static char const *const sk_names[4] = { "EASY", "NORMAL", "HEROIC", "LEGENDARY" };
				snprintf(buf_b, sizeof(buf_b), "BOT MODIFIER (%d %s %s): -%.1fX",
					(int)bot_cnt, sk_names[bot_sk], bot_cnt == 1 ? "BOT" : "BOTS", bot_pen);
				text_widen(wbuf, NUMBEROF(wbuf), buf_b);
				menu_draw_unicode_text(68, 205, 554, 225, small_font, 0, &warn_col, wbuf);
			}
			else
			{
				text_widen(wbuf, NUMBEROF(wbuf), "BOT MODIFIER (NO BOTS): -0.0X");
				menu_draw_unicode_text(68, 205, 554, 225, small_font, 0, &green_col, wbuf);
			}

			if (!score_on)
			{
				text_widen(wbuf, NUMBEROF(wbuf), "CAMPAIGN SCORE MULTIPLIER: OFF (0.00X)");
				menu_draw_unicode_text(68, 230, 554, 252, large_font, 0, &warn_col, wbuf);
			}
			else
			{
				snprintf(buf_c, sizeof(buf_c), "CAMPAIGN SCORE MULTIPLIER: %.2fX", final_mult);
				text_widen(wbuf, NUMBEROF(wbuf), buf_c);
				menu_draw_unicode_text(68, 230, 554, 252, large_font, 0, &white_col, wbuf);
			}
		}
	}
}
