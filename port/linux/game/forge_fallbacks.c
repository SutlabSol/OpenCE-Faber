/*
FORGE_FALLBACKS.C

What the game and the port's game units ask of Forge (port/forge) on the
builds that do not compile it. Forge is built into the Windows build only
(tools/windows_build.py); the Linux, Android and web builds still compile
the units that ask after it (bots.c, menu_functions.c, randomizer.c,
ui_widget.c), so this file gives them what Forge's own would: no Forge
session, and no Forge preview picture, so the Tag Injector's own is drawn.
The Windows build compiles this file too, to nothing.
*/

#ifndef HALO_WINDOWS

/* ---------- definitions */

struct bitmap_data;

/* ---------- globals */

/* port/forge/src/forge_runtime_tags.c's: nonzero while a Forge session is
being played, which these builds never play */
int g_forge_session_enabled = 0;

/* ---------- public code */

/* port/forge/src/forge_runtime_tags.c's: the picture of the tag chosen in
Forge's menus; with none, ui_widget.c draws tag_preview_picture() */
struct bitmap_data *forge_tag_preview_get_bitmap(void)
{
	return (struct bitmap_data *)0;
}

#endif /* HALO_WINDOWS */
