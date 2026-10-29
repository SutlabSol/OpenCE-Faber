"""The port's settings screens, which tools/ce_menus.py writes in place of the
PC version's (port/assets/menus/ce): laid out as its are (rows of a label and
a spinner over its row backgrounds, a line of help for the row chosen,
Defaults, OK and Cancel), with what this port has to set instead of what the
PC version had (no connection speed or texture quality: a window or the full
screen, the frame rate, the keyboard's own controls).

A row's spinner sets one of config.toml's settings (setting=, values=), or of
the profile being edited ("profile.<field>": the controller's settings), by
port/linux/game/menu_functions.c: "port setting load" shows its value,
"port settings save" (OK) writes those changed and applies them, "port
settings defaults" shows the defaults. Controls Setup binds the keyboard and
mouse's controls, by the functions the PC version names for it.
"""

import re
import sys
from pathlib import Path
from xml.sax.saxutils import quoteattr

PE = "main_menu/settings_select/player_setup/player_profile_edit"
# the mods' screen, and its rows (_mods); the main menu's items
# (ce/main_menu.xml): MODS in Credits' place (Credits is left out:
# tools/ce_menus.py's LEFT_OUT), Quit a row below it (its CHILD_OFFSETS)
MODS = "main_menu/mods"
MOD_ROWS = 11
MODS_ITEM_PLACE = (192, 391)
QUIT_ITEM_PLACE = (192, 427)
YES_NO = [("YES", "true"), ("NO", "false")]
ON_OFF = [("ON", "true"), ("OFF", "false")]
# the multiplayer armor colors, in the profile's order (player_profile.c)
SHIELD_COLORS = ["white", "black", "red", "blue", "gray", "yellow", "green", "pink", "purple", "cyan", "cobalt",
                 "orange", "teal", "sage", "brown", "tan", "maroon", "salmon"]
SENSITIVITIES = [(f"{value:g}", f"{value:g}") for value in (0.1, 0.15, 0.25, 0.4, 0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.5, 3, 4)]
VOLUMES = [(str(step), f"{step / 10:g}") for step in range(11)]

# each screen: its folder below PE, its screen's widget (the name the profile
# menu opens), its header (widget, bitmap), the row spacing, and its rows:
# (label, setting, [(shown, value)], help, platform[, key]): platform is None
# for every platform, else the platforms the row is for, separated by spaces
# ("desktop", "android", "web": the menus' platform attribute,
# port/linux/src/menu_files.c); key names the row's widgets where two rows set
# one setting (one for each platform), else the setting's name does
SCREENS = {
    "video_settings": {
        "screen": "video_settings_screen",
        "header": ("header_profile_video_settings", f"{PE}/video_settings/header_profile_video_settings"),
        # the other screens' 30px step. each platform packs its own rows, so
        # a row hidden on this machine does not leave a gap
        "spacing": 30,
        "help_top": 364,
        "platform_places": True,
        # (rows in the place of the row before them: Window Size in
        # Resolution's, port/linux/game/menu_functions.c showing the one the
        # display mode chosen uses; Android's anti-aliasing in the desktop's)
        "same_place": ["display.window_size", "anti_aliasing_android"],
        "rows": [
            ("DISPLAY MODE:", "display.mode",
             [("FULLSCREEN", "fullscreen"), ("BORDERLESS", "borderless"), ("WINDOWED", "windowed")],
             "Fullscreen takes the display; borderless covers it\nwith a window. F11 switches to the window.",
             "desktop"),
            # (port/linux/game/menu_tags.c adds the display's resolutions)
            ("RESOLUTION:", "display.resolution", [("NATIVE", "native")],
             "What fullscreen and borderless draw at. Fullscreen\nsets the display to it; borderless scales it.",
             "desktop"),
            # (port/linux/game/menu_tags.c puts the sizes that fit the desktop
            # in place of this one)
            ("WINDOW SIZE:", "display.window_size", [("1280 x 960", "1280x960")],
             "The window's size: 4:3, then 16:10, 16:9 and 21:9\n(its edges can also be dragged).", "desktop"),
            ("RESOLUTION SCALING:", "display.resolution_scaling", [("NATIVE", "native"), ("ORIGINAL", "original")],
             "Native draws at the resolution; Original draws\nthe Xbox's 640x480 and scales it up.", "desktop web"),
            ("V-SYNC:", "display.vsync", ON_OFF,
             "Wait for the display between frames, so that the\npicture never tears.", None),
            ("FRAME RATE LIMIT:", "display.max_fps",
             [("AUTO", "0"), ("30", "30"), ("60", "60"), ("120", "120"), ("144", "144"), ("165", "165"),
              ("240", "240"), ("NONE", "-1")],
             "With V-Sync off, the most frames a second. Auto:\ntwice the display's refresh rate.", "desktop web"),
            ("SMOOTH MOTION:", "display.interpolation", ON_OFF,
             "Draw a frame for every display refresh, blending\nbetween the game's 30 ticks a second.", None),
            ("INSTANT AIM:", "display.direct_camera", ON_OFF,
             "In first person, turn the view the moment the\nmouse moves, not up to two ticks later.", "desktop web"),
            ("HIGH-RES HUD:", "display.high_res_hud", ON_OFF,
             "Draw the HUD from the high-res redraws; off\ndraws the game's own pictures.", None),
            ("BUTTON STYLE:", "display.hud_button_style",
             [("CLASSIC", "classic"), ("GEMS", "gems"), ("DARK GEMS", "menu_gems")],
             "Classic: plain Xbox buttons. Gems: shaded Xbox\nbuttons. Dark Gems: the menu's button artwork.", None),
            ("HIGH-RES TEXT:", "display.high_res_text", ON_OFF,
             "Draw text and titles with high-res fonts; off\ndraws the game's own.", None),
            ("ANTI-ALIASING:", "display.anti_aliasing",
             [("OFF", "off"), ("FXAA", "fxaa"), ("SMAA", "smaa"), ("SSAA 2X", "ssaa2x"), ("MSAA 2X", "msaa2x"),
              ("MSAA 4X", "msaa4x"), ("MSAA 8X", "msaa8x")],
             "Smooth jagged edges, which the Xbox did not. FXAA\nand SMAA are cheap; SSAA and MSAA are sharper.",
             "desktop"),
            ("ANTI-ALIASING:", "display.anti_aliasing",
             [("OFF", "off"), ("FXAA", "fxaa"), ("MSAA 2X", "msaa2x"), ("MSAA 4X", "msaa4x")],
             "Smooth jagged edges, which the Xbox did not. FXAA\nis cheap; MSAA is sharper.",
             "android web", "anti_aliasing_android"),
            ("TEXTURE FILTERING:", "display.anisotropic_filtering",
             [("OFF", "0"), ("2X", "2"), ("4X", "4"), ("8X", "8"), ("16X", "16")],
             "Keep floors and walls sharp at a grazing angle,\nwhich the Xbox did not. Off: as the Xbox.", None),
            ("SHADOW RESOLUTION:", "display.shadow_resolution",
             [("128", "128"), ("256", "256"), ("512", "512"), ("1024", "1024")],
             "The size objects' shadows are drawn at: 128 as on\nthe Xbox; larger for smoother, as soft, edges.",
             None),
            ("PER-PIXEL LIGHTING:", "display.per_pixel_lighting", ON_OFF,
             "Light models for each pixel, without the facets\nof the Xbox's lighting for each vertex.", None),
            ("OVERSHIELD:", "display.overshield_effect", ON_OFF,
             "The overshield rises up the body and stays lit,\nin its own color. Online: the host's choice.", None),
            ("MODEL DETAIL:", "display.model_detail", [("STOCK", "stock"), ("NATIVE", "native"), ("HIGHEST", "highest")],
             "Stock picks models' detail by the Xbox's 480 lines;\nnative by this display's; highest, always the best.", None),
            ("SHIELD GLOW:", "display.shield_glow", ON_OFF,
             "Shields light their surroundings as they flare.\nOnline: the host's choice.", None),
            ("GLOW INTENSITY:", "display.shield_glow_intensity", [("DEFAULT", "default"), ("LIGHT SHOW", "light_show")],
             "Light Show: three times as far, at full from a\nthird of the flare. Online: the host's choice.", None),
            ("MATCH TIMER:", "display.match_timer", ON_OFF,
             "Draw a clock at the bottom right in multiplayer,\ncounting up from the start of the game.", None),
            ("HDR:", "display.hdr", ON_OFF,
             "Keep the light of explosions, plasma and the sun\npast white, and roll it off instead of clipping.", None),
            ("HDR DISPLAY:", "display.hdr_output", ON_OFF,
             "With HDR, on an HDR display, show that light\nbrighter than white. From the next start.", "desktop"),
        ],
    },
    "mouse_settings": {
        "screen": "mouse_settings_screen",
        "header": ("header_profile_mouse_settings", f"{PE}/mouse_settings/header_profile_mouse_settings"),
        "spacing": 30,
        "rows": [
            ("HORIZONTAL SENSITIVITY:", "input.mouse_sensitivity", SENSITIVITIES,
             "How fast the view turns side to side for the\nmouse's movement.", None),
            ("VERTICAL SENSITIVITY:", "input.mouse_vertical_sensitivity", [("SAME", "0")] + SENSITIVITIES,
             "How fast the view turns up and down; Same\nturns it as fast as side to side.", None),
            ("INVERT VERTICAL AXIS:", "input.invert_mouse", YES_NO,
             "Moving the mouse forward looks down.", None),
            ("AIM ASSIST:", "input.mouse_aim_assist", ON_OFF,
             "Slow and drag the view along with a target while\naiming with the mouse, as with a controller.", None),
        ],
    },
    "audio_settings": {
        "screen": "audio_settings_screen",
        "header": ("header_profile_audio_settings", f"{PE}/audio_settings/header_profile_audio_settings"),
        "spacing": 30,
        # (the devices, desktop only, first: Android's rows from the top)
        "platform_places": True,
        "rows": [
            # (port/linux/game/menu_tags.c adds the devices SDL finds)
            ("OUTPUT DEVICE:", "audio.output_device", [("SYSTEM DEFAULT", "default")],
             "Where the game's sound and the voices play.", "desktop"),
            ("INPUT DEVICE:", "audio.input_device", [("SYSTEM DEFAULT", "default")],
             "The microphone voice chat listens to.", "desktop"),
            ("MASTER VOLUME:", "audio.volume", VOLUMES, "The volume of everything.", None),
            ("MUSIC VOLUME:", "audio.music_volume", VOLUMES, "The music's volume.", None),
            ("EFFECTS VOLUME:", "audio.effects_volume", VOLUMES,
             "The volume of every other sound: effects and\nspeech.", None),
            ("REVERB:", "audio.reverb", ON_OFF,
             "Echo sounds as the place you are in does, and\nmuffle those behind walls, as the Xbox did.", None),
            ("SOUND:", "audio.enabled", ON_OFF,
             "Play sound at all; from the next time the game\nstarts.", None),
            # voice chat: this machine's own (port/linux/game/network_voice.c)
            ("VOICE CHAT:", "audio.voice_chat",
             [("PUSH TO TALK", "push_to_talk"), ("OPEN MIC", "open_mic"), ("OFF", "off")],
             "Talk in network games: while PUSH TO TALK is held\n(Controls), whenever you speak, or never.", None),
            ("VOICE VOLUME:", "audio.voice_volume", VOLUMES, "The other players' voices.", None),
        ],
    },
    "network_setup": {
        "screen": "network_settings_screen",
        "header": ("header_profile_network_settings", f"{PE}/network_setup/header_profile_network_settings"),
        "spacing": 30,
        # (the router, the clipboard and the updates, which the web build
        # has not: the other rows from the top there)
        "platform_places": True,
        "rows": [
            ("INTERNET PLAY:", "network.online", ON_OFF,
             "Host and join games over the internet by invite\nlinks; off keeps to the local network.",
             "desktop android web"),
            ("UPNP PORT FORWARDING:", "network.allow_upnp", ON_OFF,
             "Let internet play ask the router to forward its\nport, for networks that stop connections.",
             "desktop android"),
            ("JOIN FROM CLIPBOARD:", "network.join_from_clipboard", ON_OFF,
             "Join the game of an invite link copied before\nswitching to the game.", "desktop android"),
            ("CHECK FOR UPDATES:", "update.auto", ON_OFF,
             "Look for a new version when the game starts.", "desktop android"),
            ("PLAYER NAMES:", "display.player_names",
             [("ALL", "all"), ("ALLIES", "allies"), ("ENEMIES", "enemies"), ("NONE", "none")],
             "In multiplayer, whose names are drawn above their\nheads.", None),
            ("PLAYER NAME SIZE:", "display.player_name_scale",
             [(f"{value:g}x", f"{value:g}") for value in (0.5, 0.75, 1, 1.25, 1.5, 2)],
             "How large the players' names are drawn.", None),
            ("SCOREBOARD LAYOUT:", "display.scoreboard_team_layout", [("TEAMS", "teams"), ("BY SCORE", "score")],
             "A team game's scoreboard: a column for each team,\nor every player in order of score.", None),
            ("SCOREBOARD PANEL:", "display.scoreboard_background", ON_OFF,
             "Draw a panel behind the scoreboard, for clearer\ntext.", None),
        ],
    },
    "gamepad_setup": {
        "screen": "gamepad_setup_screen",
        "header": ("header_profile_controller_setting", f"{PE}/controller_edit/header_profile_controller_setting"),
        "spacing": 30,
        "rows": [
            ("LOOK SENSITIVITY:", "profile.look_sensitivity", [(str(step), str(step)) for step in range(1, 11)],
             "How fast the right stick turns the view.", None),
            ("INVERT LOOK:", "profile.invert_look", YES_NO, "Pushing the right stick up looks down.", None),
            ("INVERT FLIGHT:", "profile.flight_inversion", YES_NO,
             "Flying vehicles pitch as aircraft do: pushing up\nflies down.", None),
            ("AUTO-CENTER LOOK:", "profile.autocenter", YES_NO,
             "Level the view while walking.", None),
            ("BUTTON LAYOUT:", "profile.button_preset",
             [("DEFAULT", "0"), ("SWAP TRIGGERS", "1"), ("SWAP A, L TRIGGER", "2"), ("SWAP B, L TRIGGER", "3"),
              ("SWAP B, R STICK", "4"), ("BUMPER JUMPER", "5")],
             "Which of the controller's buttons does what (not\nthe keyboard's: Controls Setup sets those).", None),
            ("STICK LAYOUT:", "profile.joystick_preset",
             [("DEFAULT", "0"), ("SOUTHPAW", "1"), ("LEGACY", "2"), ("LEGACY SOUTHPAW", "3")],
             "Which stick moves and which looks.", None),
            ("VIBRATION:", "profile.vibration", ON_OFF, "Rumble the controller.", None),
            ("IN-GAME HELP:", "profile.ingame_help", ON_OFF, "Show the game's hints about its controls.", None),
        ],
    },
}

# Video Setup's categories open ordinary settings screens. Each has the same
# pending edits, Defaults, OK and Cancel as the other settings screens.
_video = SCREENS["video_settings"]
_graphics = {"display.high_res_hud", "display.hud_button_style", "display.high_res_text", "display.anti_aliasing",
             "display.anisotropic_filtering", "display.shadow_resolution", "display.per_pixel_lighting", "display.match_timer",
             "display.model_detail"}
# (the visual modifiers: a screen of their own, Extra Effects)
_extra_effects = {"display.shield_glow", "display.shield_glow_intensity",
                  "display.overshield_effect", "display.hdr", "display.hdr_output"}
SCREENS["video_settings/graphics"] = {
    "screen": "graphics_settings_screen", "header": _video["header"], "spacing": 30,
    "same_place": ["anti_aliasing_android"],
    "rows": [row for row in _video["rows"] if row[1] in _graphics],
}
SCREENS["video_settings/extra_effects"] = {
    "screen": "extra_effects_settings_screen", "header": _video["header"], "spacing": 30,
    "rows": [row for row in _video["rows"] if row[1] in _extra_effects],
}
_video["rows"] = [row for row in _video["rows"] if row[1] not in _graphics | _extra_effects]
_video["categories"] = [
    ("GRAPHICS:", "video_settings/graphics", "The HUD, text, anti-aliasing, shadows,\nlighting and HDR."),
    ("FOV AND VIEWMODELS:", "video_settings/fov_viewmodels",
     "The field of view and the first-person\nweapon. Their defaults keep the stock view."),
    ("EXTRA EFFECTS:", "video_settings/extra_effects",
     "Visual modifiers the stock game does not\nhave, such as the shield glow."),
]
SCREENS["video_settings/fov_viewmodels"] = {
    "screen": "fov_viewmodel_settings_screen", "header": _video["header"], "spacing": 30,
    "rows": [
        ("FOV:", "display.fov", [("DEFAULT", "0")] + [(str(n), str(n)) for n in range(80, 151, 5)],
         "On foot, horizontal at 16:9, in degrees.\nDefault keeps the stock view.", None),
        ("VIEWMODEL FOV:", "display.viewmodel_fov", [("DEFAULT", "0")] + [(str(n), str(n)) for n in range(80, 151, 5)],
         "The weapon and hands, horizontal at 16:9.\nDefault keeps the weapon's stock view.", None),
        ("VIEWMODELS:", "display.viewmodel_visible", ON_OFF,
         "Draw first-person weapons, hands and attached\nvisuals. Gameplay and sound continue when off.", None),
        ("PASSENGER THIRD PERSON:", "display.passenger_third_person", ON_OFF,
         "Third-person Warthog and Scorpion passengers in\nthe campaign and co-op. Weapon zoom is disabled.", None),
        ("ARM SHIELDS:", "display.viewmodel_shield", ON_OFF,
         "The energy shield's flare on your first-person\narms too, as on your body.", None),
        ("LEGS:", "display.first_person_legs", ON_OFF,
         "Your legs in first person, seen looking down,\nmoving as you run. Your body as others see it.", None),
        ("SHIELD COLOR:", "display.shield_color", [("DEFAULT", "default")] + [(c.upper(), c) for c in SHIELD_COLORS],
         "Your shield's flare in an armor color, whatever\nyour armor. Only you see it. Default: its own.", None),
    ],
}

# Audio Setup's sound processing (port/linux/src/dsound_sdl.c): the reverb
# and the resampler, a screen of their own (a category, as Video Setup's);
# the devices and the volumes stay on Audio Setup
_audio = SCREENS["audio_settings"]
_processing = {"audio.reverb"}
SCREENS["audio_settings/sound_processing"] = {
    "screen": "sound_processing_settings_screen", "header": _audio["header"], "spacing": 30,
    "rows": [row for row in _audio["rows"] if row[1] in _processing] + [
        ("RESAMPLER:", "audio.resampler", [("SINC", "sinc"), ("LINEAR", "linear")],
         "How sounds are converted to the output's rate:\nsinc is clean; linear is brighter, with aliasing.",
         None),
    ],
}
_audio["rows"] = [row for row in _audio["rows"] if row[1] not in _processing]

# (voice chat stays on Audio Setup; the text chat's rows are the multiplayer
# HUD's, on Network Setup)
_audio["categories"] = [
    ("SOUND PROCESSING:", "audio_settings/sound_processing",
     "The reverb, and how sounds are converted to the\noutput's rate."),
]

# Network Setup's map sharing (port/linux/game/map_share.c): this machine's
# part in another's game, a screen of its own (a category, as Video Setup's)
SEED_UPLOADS = (64, 128, 256, 512, 1024, 2048, 4096, 8192, 10240)
_network = SCREENS["network_setup"]
# (and the multiplayer HUD's rows, and streamer mode, screens of their own:
# Network Setup has room for nine places)
_hud_settings = ("display.player_names", "display.player_name_scale", "display.scoreboard_team_layout",
                 "display.scoreboard_background")
_hud_rows = [row for row in _network["rows"] if row[1] in _hud_settings]
_network["rows"] = [row for row in _network["rows"] if row[1] not in _hud_settings]
_network["categories"] = [
    ("MAP SHARING:", "network_setup/map_sharing",
     "Downloading Custom Edition maps you do not have,\nand helping send them to other players.",
     "desktop android"),
    ("MULTIPLAYER HUD:", "network_setup/multiplayer_hud",
     "Players' names above their heads, the\nscoreboard, and the text chat."),
    ("STREAMER MODE:", "network_setup/streamer_mode",
     "Hide offensive words in names and text from\nother players, maps and mods, as stars."),
]
SCREENS["network_setup/map_sharing"] = {
    "screen": "map_sharing_settings_screen", "header": _network["header"], "spacing": 30,
    "rows": [
        ("DOWNLOAD MAPS:", "network.map_download", ON_OFF,
         "Joining a game on a Custom Edition map you do not\nhave: download it from the players (if shared).",
         None),
        ("HELP SEND MAPS:", "network.map_seed", ON_OFF,
         "In another's game, send its map to players who\njoin without it, as a torrent's peers do.", None),
        ("UPLOAD LIMIT:", "network.map_seed_upload",
         [(f"{kb} KB/S" if kb < 1024 else f"{kb // 1024} MB/S", str(kb)) for kb in SEED_UPLOADS],
         "The most you send of maps to other players, so\nthat the game keeps the rest of your upload.", None),
    ],
}

# Network Setup's multiplayer HUD: its rows, moved from Network Setup, and the
# text chat's (port/linux/game/network_chat.c)
SCREENS["network_setup/multiplayer_hud"] = {
    "screen": "multiplayer_hud_settings_screen", "header": _network["header"], "spacing": 30,
    "rows": _hud_rows + [
        ("TEXT CHAT:", "display.text_chat", [("SHOW", "true"), ("HIDE", "false")],
         "SHOW the text chat, or HIDE it: no lines, and\nits keys and D-pad Up open nothing.", None),
        ("CHAT TEXT:", "display.chat_style", [("COLORS", "colors"), ("WHITE", "white")],
         "The text chat's colors: COLORS, names in their\nteam's color; WHITE, as Custom Edition.", None),
    ],
}

# Network Setup's streamer mode (port/linux/game/text_filter.c): words hidden
# in what other players, maps and mods show, each group from a severity of
# Surge AI's on (port/assets/text_filter)
STREAMER_LEVELS = [("OFF", "off"), ("SEVERE", "severe"), ("STRONG AND WORSE", "strong"), ("MILD AND WORSE", "mild")]
SCREENS["network_setup/streamer_mode"] = {
    "screen": "streamer_mode_settings_screen", "header": _network["header"], "spacing": 30,
    "rows": [
        ("STREAMER MODE:", "streamer.enabled", ON_OFF,
         "Hide offensive words in names and text from\nother players, maps and mods, as stars.", None),
        ("CENSOR SLURS:", "streamer.slurs", STREAMER_LEVELS,
         "Slurs about race, sexuality, gender and\ndisability.", None),
        ("CENSOR SWEARING:", "streamer.swearing", STREAMER_LEVELS,
         "Swearing and sexual words (STRONG AND WORSE\nleaves the mildest showing).", None),
        ("CENSOR INSULTS:", "streamer.insults", STREAMER_LEVELS,
         "Name-calling and insults that are not slurs.", None),
    ],
}

# Network Setup in a game (the pause menu's SETTINGS: port/linux/game/
# menu_tags.c's pause_network_patch): its pages alone, without the rows that
# would drop the game (internet play) or are for the main menu (the router,
# the clipboard, the updates)
SCREENS["network_setup/in_game"] = {
    "screen": "in_game_network_settings_screen", "header": _network["header"], "spacing": 30,
    "platform_places": True, "rows": [], "categories": _network["categories"],
}

# Controls Setup: the keyboard and mouse's actions, in groups (the order of
# port/linux/game/menu_functions.c's table of them)
CONTROL_GROUPS = ["MOVEMENT", "WEAPONS", "ACTIONS", "COMMUNICATION", "FORGE", "FORGE PAD"]
CONTROL_ROWS = 10

# the settings whose rows have the wider spinner (a device's name)
WIDE_SETTINGS = {"audio.output_device", "audio.input_device", "streamer.slurs", "streamer.swearing",
                 "streamer.insults"}

# the host's voice chat and vote kicks, on Teamplay Options
# (_teamplay_options_extras): each row's key, its label, its choices' words
# (menu_functions.c's gametype_options give their settings' values) and
# their helps
TEAMPLAY_EDIT = "main_menu/settings_select/multiplayer_setup/teamplay_options_edit"
VOICE_QUALITIES = (8, 12, 16, 24, 32, 48, 64)
TEAMPLAY_ROWS = [
    ("voice_mode", "VOICE CHAT:", ["OFF", "TEAM NEAR", "ANYONE NEAR", "TEAM", "TEAM, ENEMIES NEAR"], [
        "No voice chat during the game.",
        "Players hear their teammates who are near them.",
        "Players hear everyone who is near them.",
        "Players hear all their teammates, wherever they\\nare.",
        "Players hear all their teammates, and the enemies\\nwho are near them.",
    ]),
    ("voice_lobby", "LOBBY VOICE CHAT:", ["ON", "OFF"], [
        "Everyone hears everyone in the lobby, before and\\nafter the game.",
        "No voice chat in the lobby.",
    ]),
    ("voice_kbps", "VOICE QUALITY:", [f"{kbps} KBPS" for kbps in VOICE_QUALITIES],
     [f"Voices at {kbps} kilobits a second, in the lobby\\nand in the game." for kbps in VOICE_QUALITIES]),
    ("voice_proximity", "VOICE NEAR DISTANCE:", ["15 M", "30 M", "45 M", "60 M", "90 M", "150 M"],
     [f"Players {metres} metres apart or less are near\\nfor voice chat." for metres in (15, 30, 45, 60, 90, 150)]),
    # (network_votekick.c)
    ("votekick", "VOTE KICK:", ["ON", "OFF"], [
        "Players can vote to kick a player, from the\\nscoreboard.",
        "Players cannot vote to kick anyone.",
    ]),
    # (network_chat.c: one row for the chat and its links, as Teamplay
    # Options has room for one more)
    ("text_chat", "TEXT CHAT:", ["ON", "WITH LINKS", "OFF"], [
        "Players can type to each other; lines with links\\nare refused. Off in a public internet game.",
        "Players can type to each other, links too. Off\\nin a public internet game.",
        "No text chat in the game.",
    ]),
]

# the profile menu's words for what its items now open
STRING_OVERRIDES = {
    f"{PE}/profile_edit_descriptions": [
        "Rename this profile.\\n\\n\\nProfile:",
        "Choose the keys and mouse\\nbuttons for each action.\\n\\nProfile:",
        "Set this profile's controller\\nsensitivity and layouts.\\n\\nProfile:",
        "Adjust the mouse's sensitivity\\nand aiming.\\n\\nProfile:",
        "Adjust the volume of the music\\nand of everything else.\\n\\nProfile:",
        "Choose a window or the full\\nscreen, the frame rate and more.\\n\\nProfile:",
        "Internet play, updates and the\\nmultiplayer HUD.\\n\\nProfile:",
        "Change the current profile's\\nfree-for-all multiplayer color.\\n\\nProfile:",
        "Halo: Combat Evolved, the Xbox\\ngame, on this computer.\\n\\nProfile:",
    ],
}


def attributes(pairs: list) -> str:
    return "".join(f" {key}={quoteattr(str(value))}" for key, value in pairs if value is not None)


def _widget(name: str, pairs: list, inner: list) -> list:
    return [f"\t<widget{attributes([('name', name)] + pairs)}>", *[f"\t\t{line}" for line in inner], "\t</widget>"]


def _strings(name: str, texts: list) -> list:
    return [f"\t<strings{attributes([('name', name)])}>",
            *[f"\t\t<string{attributes([('text', text)])}/>" for text in texts], "\t</strings>"]


def _button(name: str, caption: int, handlers: list) -> list:
    pairs = [("type", "text"), ("width", 128), ("height", 24), ("bitmap", "bitmaps/text_button_background"),
             ("string_list", "strings/common_button_captions"), ("string_index", caption), ("font", "ui\\small_ui"),
             ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)]
    return _widget(name, pairs, handlers + ['<on event="left_mouse" run="mouse emit accept event"/>'])


def _screen(folder: str, spec: dict, rows: list, list_inputs: list, list_handlers: list, extra: list) -> list:
    """a screen: its header, the list of rows (with the help line as its
    description), and the buttons"""
    base = folder if folder.startswith("main_menu/") else f"{PE}/{folder}"
    header, header_bitmap = spec["header"]
    lines = []
    lines += _widget(f"{base}/{spec['screen']}",
                     [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child pause_game"),
                      ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                      f'<child{attributes([("widget", f"{base}/{header}")])}/>',
                      f'<child{attributes([("widget", f"{base}/options_menu")])}/>'])
    lines += _widget(f"{base}/{header}", [("controller", 1), ("left", 35), ("top", 11), ("width", 605), ("height", 59),
                                          ("bitmap", header_bitmap)], [])
    lines += _widget(f"{base}/help", [("type", "text"), ("controller", 1), ("left", 68),
                                      ("top", spec.get("help_top", 350)), ("width", 482),
                                      ("height", 60), ("string_list", f"{base}/help_strings"),
                                      ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    children = [f'<data input="{name}"/>' for name in list_inputs] + list_handlers
    # (each row: its widget, platform, and the place it is in, else the next)
    row_top = spec.get("row_top", 73)
    for index, (row, platform, *place) in enumerate(rows):
        y = row_top + (place[0] if place else index) * spec["spacing"]
        children.append(f'<child{attributes([("widget", row), ("x", 54), ("y", y), ("platform", platform)])}/>')
    children.append(f'<child{attributes([("widget", f"{base}/button_bar"), ("y", 414)])}/>')
    lines += _widget(f"{base}/options_menu",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", f"{base}/help")], children)
    lines += _widget(f"{base}/button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child{attributes([("widget", f"{base}/button_defaults"), ("y", 1)])}/>',
                      f'<child{attributes([("widget", f"{base}/button_ok"), ("x", 380), ("y", 1)])}/>',
                      '<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += extra
    return lines


def _placed(row: str, platform, members: list, places: dict) -> list:
    """a row's children (row, platform, place): one for its platforms where
    they have it in the same place, else one for each place, with the
    platforms whose place it is"""
    groups = {}
    for name in members:
        groups.setdefault(places[name], []).append(name)
    if len(groups) == 1:
        return [(row, platform, places[members[0]])]
    return [(row, " ".join(names) if len(names) < len(places) else None, place) for place, names in groups.items()]


def _setting_screen(folder: str, spec: dict) -> list:
    base = f"{PE}/{folder}"
    rows, extra = [], []
    place = -1
    # (platform_places: each platform's rows in places of their own, with
    # no gap where the others' are; a row in one place for all its
    # platforms, else a child for each place)
    places = {"desktop": -1, "android": -1, "web": -1}
    for index, (label, setting, choices, _, platform, *named) in enumerate(spec["rows"]):
        key = named[0] if named else setting.split(".", 1)[1]
        row = f"{base}/op_{key}"
        shares = setting in spec.get("same_place", ()) or key in spec.get("same_place", ())
        if spec.get("platform_places"):
            members = platform.split() if platform else list(places)
            for name in members:
                if not shares:
                    places[name] += 1
            rows += _placed(row, platform, members, places)
        else:
            if setting not in spec.get("same_place", ()) and key not in spec.get("same_place", ()):
                place += 1
            rows.append((row, platform, place))
        # (a device's name wants the wider spinner Server Setup's co-op rows
        # have: WIDE_SETTINGS)
        wide = setting in WIDE_SETTINGS
        extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF"), ("platform", platform)],
                         [f'<child{attributes([("widget", f"{base}/{key}_label")])}/>',
                          f'<child{attributes([("widget", f"{base}/{key}_spinner"), ("x", 286 if wide else 320), ("y", 1)])}/>'])
        extra += _widget(f"{base}/{key}_label",
                         [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                          ("string_list", f"{base}/labels"), ("string_index", index), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        extra += _widget(f"{base}/{key}_spinner",
                         [("type", "spinner"), ("left", None if wide else 3), ("top", 2), ("width", 206 if wide else 147),
                          ("height", 20), ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("strings", "|".join(shown for shown, _ in choices)), ("setting", setting),
                          ("values", "|".join(value for _, value in choices)), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4 if wide else 1),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -13 19 -7" if wide else "7 -6 19 0"),
                          ("footer_bounds", "7 208 19 214" if wide else "7 150 19 156")],
                         ['<on event="created" run="port setting load"/>'])
    # (each category's row: its label, the screen it opens, its help, and the
    # platforms it is for, as a row's: every one's when it names none)
    for index, (label, category_folder, _, *category_platform) in enumerate(spec.get("categories", ())):
        key = category_folder.rsplit("/", 1)[-1]
        row = f"{base}/op_{key}"
        target = f"{PE}/{category_folder}/{SCREENS[category_folder]['screen']}"
        platform = category_platform[0] if category_platform else None
        if spec.get("platform_places"):
            members = platform.split() if platform else list(places)
            for name in members:
                places[name] += 1
            rows += _placed(row, platform, members, places)
        else:
            rows.append((row, platform, place + 1 + index))
        extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF"), ("platform", platform)],
                         [f'<on event="a" open="{target}"/>', f'<on event="start" open="{target}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/{key}_label"/>'])
        extra += _widget(f"{base}/{key}_label",
                         [("type", "text"), ("controller", 1), ("width", 512), ("height", 22),
                          ("string_list", f"{base}/labels"), ("string_index", len(spec["rows"]) + index),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    extra += _button(f"{base}/button_defaults", 3, ['<on event="a" run="port settings defaults"/>',
                                                    '<on event="start" run="port settings defaults"/>'])
    extra += _button(f"{base}/button_ok", 1, ['<on event="a" run="port settings save" back="true"/>',
                                              '<on event="start" run="port settings save" back="true"/>'])
    extra += _strings(f"{base}/labels", [label for label, *_ in spec["rows"]] +
                      [label for label, *_ in spec.get("categories", ())])
    # (the help of the row whose label is string n is n + 1: the buttons' is 0)
    extra += _strings(f"{base}/help_strings",
                      [""] + [help_text.replace("\n", "\\n") for _, _, _, help_text, *_ in spec["rows"]] +
                      [help_text.replace("\n", "\\n") for _, _, help_text, *_ in spec.get("categories", ())])
    return _screen(folder, spec, rows, ["port settings help"], [], extra)


def _controls_screen() -> list:
    folder = "controls_setup"
    base = f"{PE}/{folder}"
    spec = {"screen": "controls_settings_screen", "spacing": 30,
            "header": ("header_profile_controls_settings", f"{base}/header_profile_controls_settings")}
    rows, extra = [(f"{base}/op_group", None)], []
    extra += _widget(f"{base}/op_group", [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                                          ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<child{attributes([("widget", f"{base}/group_label")])}/>',
                      f'<child{attributes([("widget", f"{base}/group_spinner"), ("x", 200), ("y", 1)])}/>'])
    extra += _widget(f"{base}/group_label",
                     [("type", "text"), ("controller", 1), ("width", 200), ("height", 22), ("text", "SHOW:"),
                      ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/group_spinner",
                     [("type", "spinner"), ("left", 3), ("top", 2), ("width", 297), ("height", 20),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                      ("strings", "|".join(CONTROL_GROUPS)), ("font", "ui\\large_ui"), ("color", "#FF2896FF"),
                      ("align", "center"), ("text_y", 1), ("header_bitmap", "bitmaps/arrow_sm_left"),
                      ("footer_bitmap", "bitmaps/arrow_sm_right"), ("header_bounds", "7 -6 19 0"),
                      ("footer_bounds", "7 300 19 306")], [])
    for index in range(CONTROL_ROWS):
        row = f"{base}/op_command_{index + 1}"
        rows.append((row, None))
        extra += _widget(row, [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         ['<on event="a" run="controls begin binding"/>',
                          '<on event="start" run="controls begin binding"/>',
                          '<on event="left right" run="controls binding slot"/>',
                          f'<child{attributes([("widget", f"{base}/command_label")])}/>',
                          f'<child{attributes([("widget", f"{base}/command_binding"), ("x", 200)])}/>',
                          f'<child{attributes([("widget", f"{base}/command_binding"), ("x", 356)])}/>'])
    extra += _widget(f"{base}/command_label",
                     [("type", "text"), ("controller", 1), ("width", 200), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/command_binding",
                     [("type", "text"), ("controller", 1), ("width", 152), ("height", 22), ("font", "ui\\small_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 5), ("text_flags", "no_focus_test")],
                     [])
    extra += _button(f"{base}/button_defaults", 3, ['<on event="a" run="controls screen defaults"/>',
                                                    '<on event="start" run="controls screen defaults"/>'])
    extra += _button(f"{base}/button_ok", 1, ['<on event="a" run="controls screen change set" back="true"/>',
                                              '<on event="start" run="controls screen change set" back="true"/>'])
    extra += _strings(f"{base}/help_strings", [
        "Choose what to show, with left and right.",
        "Enter sets the key chosen; left and right choose\\nwhich of two. Then press the new key or button.",
        "Press a key, a mouse button or turn the wheel...\\nEscape cancels, Delete clears it.",
    ])
    return _screen(folder, spec, rows, ["controls update menu"], ['<on event="created" run="controls screen init"/>'],
                   extra)


def settings_files() -> dict:
    """the files of the port's screens, by their names in ce/"""
    files = {}
    for folder, spec in SCREENS.items():
        files[f"{PE}/{folder}".replace("/", ".") + ".xml"] = _setting_screen(folder, spec)
    files[f"{PE}/controls_setup".replace("/", ".") + ".xml"] = _controls_screen()
    return {name: ['<?xml version="1.0" encoding="UTF-8"?>',
                   "<!-- The port's settings screen, in the PC version's style (tools/port_settings.py) -->",
                   "<menus>", *lines, "</menus>", ""] for name, lines in files.items()}


def replaced(folder: str) -> bool:
    """whether the PC version's folder of widgets is replaced by ours (and
    those below it: the controls' advanced screen, the video test)"""
    return any(folder == path or folder.startswith(f"{path}/")
               for path in [*(f"{PE}/{name}" for name in [*SCREENS, "controls_setup"]), *REPLACED_FOLDERS])


# ---------- multiplayer (PLAN.md section 11)

MT = "main_menu/multiplayer_type_select"

# Multiplayer's rows, in order: the PC version's, Create Game's LOCAL
# (_create_local) and CO-OP CAMPAIGN (_coop), 29 apart (not the PC version's
# 33) so that CO-OP still ends above the divider and Edit Gametypes
# (tools/ce_menus.py's CHILD_OFFSETS), as the profile settings' ten rows fit
# in the nine's room. JOIN GAME and CREATE GAME are labels, 32 to the left
MULTIPLAYER_ROWS = ["multiplayer_type_join_item", "multiplayer_type_join_internet_item",
                    "multiplayer_type_join_lan_item", "multiplayer_type_join_direct_item",
                    "multiplayer_type_create_item", "multiplayer_type_create_internet_item",
                    "multiplayer_type_create_lan_item", "multiplayer_type_create_local_item",
                    "multiplayer_type_coop_item"]
MULTIPLAYER_LABEL_ROWS = {"multiplayer_type_join_item", "multiplayer_type_create_item"}


def multiplayer_row_place(row: str) -> tuple:
    """a Multiplayer row's place in its list (x, y)"""
    return (-32 if row in MULTIPLAYER_LABEL_ROWS else 0, 78 + 29 * MULTIPLAYER_ROWS.index(row))

# strings added to the PC version's lists: (at, strings) of each list. Slayer's
# kills to win goes up to 500, for big games (the Xbox editor's are
# source/interface/ui_widget.c's kills_to_win_extra_strings): the values,
# and their helps after the five of its own (its helps are the rows' values'
# in turn: menu_functions.c's gametype_option_help)
SLAYER_EDIT = "main_menu/settings_select/multiplayer_setup/playlist_edit/slayer_edit"
PLAYER_EDIT = "main_menu/settings_select/multiplayer_setup/player_options_edit"
# (its rows, the PC version's, in order: ce_menus.py moves them 27 apart)
PLAYER_OPTIONS_ROWS = ['number_of_lives', 'maximum_health', 'shields', 'respawn_time', 'respawn_time_growth', 'odd_man_out', 'invisible_players', 'suicide_penalty']
# the vehicle sets' PC set (game_engine.h's VARIANT_VEHICLE_SET_PC), between
# GUN TURRETS and CUSTOM (menu_functions.c's VEHICLE_PRESET_PC), and its help
# among the sets' (its rows' values' in turn: gametype_option_help)
VEHICLE_EDIT = "main_menu/settings_select/multiplayer_setup/vehicle_options_edit"
AIM_ASSIST_HELP = [
    "Players keep their current mouse aim assist setting.",
    "Disable mouse aim assist for mouse/keyboard players.\\nController and touch aim assist are unchanged.",
]
# the host's shield effects, for every player of a network game
# (network_shield_effects.c): their words, then their helps
SHIELD_EFFECTS = ['STOCK', 'GLOW', 'LIGHT SHOW', 'OVERSHIELD', 'GLOW+OVERSHIELD', 'SHOW+OVERSHIELD',
                  "PLAYERS' OWN"]
SHIELD_EFFECTS_HELP = [
    "Everyone sees the stock game's shields and\\novershield, whatever their own settings.",
    "Everyone's shields light their surroundings as\\nthey flare.",
    "Everyone's shields light their surroundings as\\nthey flare, three times as far (Light Show).",
    "For everyone, the overshield rises up the body\\nand stays lit, in its own color.",
    "For everyone, shields light their surroundings,\\nand the overshield rises and stays lit.",
    "For everyone, Light Show's shield glow, and the\\novershield rising and staying lit.",
    "Not synced: each player sees the shield effects\\nof their own Video Setup settings.",
]

# OpenCE's gametypes' options (port/linux/game/game_modes.c), a screen of the
# gametype editor's (menu_functions.c's "mp profile init opence options"): the
# mode and Hardcore, then the mode's own rows, which menu_functions.c's
# gametype_option_help shows (op_inf_ for Infection, op_gg_ for Gun Game)
# below them; a row's values are menu_functions.c's gametype_options'
PLAYLIST_EDIT = "main_menu/settings_select/multiplayer_setup/playlist_edit"
OPENCE_EDIT = "main_menu/settings_select/multiplayer_setup/opence_options_edit"
OPENCE_ROWS = [
    ("oce_mode", "MODE:", ["NONE", "SWAT", "INFECTION", "GUN GAME"], [
        "The game type's own rules.",
        "No shields, and a shot to the head kills. Slayer only.",
        "Humans killed join the zombies, who win when\\nno human is left. Team Slayer only.",
        "Each kill gives the next weapon; a kill with the\\nlast wins. Free for all Slayer only."]),
    ("oce_hardcore", "HARDCORE:", ["OFF", "ON"], [
        "The motion sensor and aim assist as the game\\ntype has them.",
        "No motion sensor, no aim assist, and a kill\\nrestores the killer's health and shields."]),
    ("inf_zombies", "FIRST ZOMBIES:", ["1", "2", "3", "4", "8", "16"],
     ["The zombies chosen when the round starts\\n(a human is always left)."] * 6),
    ("inf_speed", "ZOMBIE SPEED:", ["100%", "110%", "125%", "150%", "175%", "200%"],
     ["How fast zombies move, of a human's speed."] * 6),
    ("inf_damage", "SKULL DAMAGE:", ["100%", "150%", "200%", "250%"],
     ["The damage of a zombie's skull's melee."] * 4),
    ("inf_respawn", "ZOMBIE RESPAWN:", ["1 SECOND", "3 SECONDS", "5 SECONDS", "10 SECONDS", "15 SECONDS",
                                        "30 SECONDS"],
     ["How long a dead zombie waits to respawn."] * 6),
    ("inf_countdown", "COUNTDOWN:", ["NONE", "5 SECONDS", "10 SECONDS", "15 SECONDS", "30 SECONDS"],
     ["How long the round waits, once two players are\\nalive, before the first zombies are chosen."] * 5),
    ("inf_shields", "ZOMBIE SHIELDS:", ["OFF", "ON"],
     ["Zombies have no shields.", "Zombies have shields, as humans do."]),
    ("gg_order", "WEAPON ORDER:", ["STRONGEST FIRST", "WEAKEST FIRST", "RANDOM"], [
        "From the rocket launcher to the pistol.", "From the pistol to the rocket launcher.",
        "In an order the host draws for each game."]),
    ("gg_melee", "MELEE DEMOTES:", ["OFF", "ON"], [
        "A melee kill takes nothing from the victim.",
        "A melee kill takes the victim back a weapon."]),
]
OPENCE_ITEM = f"{PLAYLIST_EDIT}/playlist_edit_opence_item"

STRING_INSERTS = {
    # (the gametype editor's OPENCE OPTIONS' words, after the PC version's:
    # menu_functions.c's playlist_edit_description)
    f"{PLAYLIST_EDIT}/playlist_edit_list_extended_dsc_text": [(8, [
        "Choose the rules of OpenCE's\\ngametypes: SWAT, Infection,\\nGun Game and Hardcore.\\nThis gametype:"])],
    f"{PLAYER_EDIT}/player_options_labels": [(8, ["AIM ASSIST:", "SHIELD EFFECTS:"])],
    f"{PLAYER_EDIT}/cap_player_options": [(28, AIM_ASSIST_HELP + SHIELD_EFFECTS_HELP)],
    # (Teamplay Options' voice chat and vote kick rows, after its own:
    # TEAMPLAY_ROWS)
    f"{TEAMPLAY_EDIT}/teamplay_options_labels": [(3, [label for _, label, *_ in TEAMPLAY_ROWS])],
    f"{TEAMPLAY_EDIT}/cap_teamplay_options": [(10, [text for *_, helps in TEAMPLAY_ROWS for text in helps])],
    f"{SLAYER_EDIT}/var_kills_to_win": [(5, ["75", "100", "150", "200", "250", "500"])],
    f"{VEHICLE_EDIT}/var_vehicle_set": [(8, ["PC"])],
    f"{VEHICLE_EDIT}/cap_vehicle_options": [(17, [
        "Every vehicle the map places will spawn, as in\\nHalo PC.",
    ])],
    f"{SLAYER_EDIT}/cap_slayer": [(11, [
        "Seventy-five kills to win. Settle in for a long\\nfight.",
        "A hundred kills to win. Made for big games.",
        "A hundred and fifty kills to win. Only a crowded\\nserver gets there.",
        "Two hundred kills to win. Bring friends. Lots of\\nthem.",
        "Two hundred and fifty kills to win.",
        "Five hundred kills to win. You'll be here a while.",
    ])],
}


# a custom loadout's weapons (game_engine.h's _loadout_weapon_*)
LOADOUT_WEAPONS = ["NONE", "RANDOM", "ASSAULT RIFLE", "PISTOL", "SHOTGUN", "SNIPER RIFLE", "ROCKET LAUNCHER",
                   "PLASMA PISTOL", "PLASMA RIFLE", "NEEDLER"]


def LOADOUT_HELPS(slot: str) -> list:
    """the helps of a custom loadout's weapon's values"""
    helps = [f"No {slot} weapon.", f"A random {slot} weapon, of those the map has,\\neach time you spawn."]
    for weapon in LOADOUT_WEAPONS[2:]:
        helps.append(f"Everyone spawns with a {weapon.lower()} as their {slot}\\nweapon (where the map has one).")
    return helps


STRING_OVERRIDES.update({
    f"{MT}/multiplayer_options": ["JOIN GAME", "CREATE GAME", "INTERNET", "LAN", "DIRECT LINK", "EDIT GAMETYPES",
                                  "SERVER BROWSER", "CO-OP CAMPAIGN", "LOCAL"],
    f"{MT}/multiplayer_option_descriptions": [
        "Browse the games on the\\nInternet.",
        "Join a multiplayer game on\\nyour LAN.",
        "Join a game by its invite link,\\nor one a Discord invite\\nreached.",
        "Host a game on the Internet:\\nplayers join by its invite\\nlink or Discord.",
        "Host a game on your LAN\\nonly.",
        "Play on this computer alone:\\nsplit screen, and bots in\\nSlayer games.",
        "Play the campaign with a\\nfriend in split screen:\\nplayer 2 on a gamepad.",
        "Set all the attributes for your \\nmultiplayer gametypes and\\nkeep them for future use.",
    ],
    # (and the port's OPENCE: OpenCE's gametypes, port/linux/game/game_modes.c)
    "main_menu/gametype_select/var_gametype_banks": ["STANDARD", "OPENCE", "CUSTOM"],
    # (the PC's weapon sets, then the Xbox's NO GRENADES: menu_functions.c's
    # gametype_options maps them to the engine's)
    "main_menu/settings_select/multiplayer_setup/item_options_edit/item_options_labels": ["INFINITE GRENADES:", "WEAPON SET:", "VEHICLE SET:", "STARTING EQUIPMENT:",
                                  "VEHICLE RESPAWN TIME:", "LOADOUT:", "PRIMARY WEAPON:", "SECONDARY WEAPON:",
                                  "MAP WEAPONS:"],
    # (the helps of each option's values in turn: menu_functions.c's
    # gametype_option_help)
    "main_menu/settings_select/multiplayer_setup/item_options_edit/cap_item_options": [
        "Everyone has a limitless supply of grenades, which\\nmakes for loud, explosive and occasionally messy fun.",
        "Each player can only carry up to 4 grenades of each\\ntype at any given time.",
        "The map will contain whatever weapons the designers\\nplaced on it.",
        "All the weapons on the map will be replaced by\\npistols.",
        "All the weapons on the map will be replaced by\\nassault rifles and plasma rifles.",
        "All the weapons on the map will be replaced by\\nplasma weapons.",
        "The guns on the map will be replaced by weapons\\nwith sniper scopes.",
        "Sniper rifles and pistols will not appear on the map.",
        "All the weapons on the map will be replaced by\\nrocket launchers.",
        "All the weapons on the map will be replaced by\\nshotguns.",
        "All the weapons on the map are only effective at\\nshort ranges.",
        "There are no weapons on the map that were made in\\nCovenant sweat shops.",
        "Only weapons made by the Covenant will appear\\nin the map.",
        "The flamethrower and fuel rod gun will not appear\\nin the map.",
        "Only the most devastating and dangerous of weapons\\nwill appear in the map.",
        "The map's weapons, without any grenades.",
        "You will start the game with the weapon set that the \\nmap designers custom tailored for it.",
        "Use the generic starting weapons across all maps.",
        "The map's weapons appear where its designers\\nplaced them.",
        "No weapons appear on the map: with a loadout of\\nNONE and NONE, melee and grenades only.",
        "The weapons follow the weapon set above.",
        "Everyone starts with the primary and secondary\\nweapons below; the map's weapons are its own.",
        *LOADOUT_HELPS("primary"),
        *LOADOUT_HELPS("secondary"),
    ],
    "main_menu/settings_select/multiplayer_setup/item_options_edit/var_weapon_set": [
        "NORMAL", "PISTOLS", "RIFLES", "PLASMA WEAPONS", "SNIPER", "NO SNIPING", "ROCKET LAUNCHERS", "SHOTGUNS",
        "SHORT RANGE", "HUMAN", "COVENANT", "CLASSIC", "HEAVY WEAPONS", "NO GRENADES"],
})

# which of STRING_OVERRIDES' strings say what one of the PC version's own
# says: its index in the PC version's list, or None (the port's own). A map
# in another language than English has those in its language (map=,
# menu_tags.c's map_strings); the inserted strings (STRING_INSERTS) are the
# port's own, the rest of their lists the PC version's
OVERRIDE_ORIGINALS = {
    f"{PE}/profile_edit_descriptions": [0, None, None, None, None, None, None, 7, None],
    f"{MT}/multiplayer_options": [0, 1, 2, 3, None, 5, None, None, None],
    f"{MT}/multiplayer_option_descriptions": [0, 1, None, None, 4, None, None, 5],
    "main_menu/gametype_select/var_gametype_banks": [0, None, 2],
    "main_menu/settings_select/multiplayer_setup/item_options_edit/item_options_labels": [0, 1, 2, 3, 4] + [None] * 4,
    # (the first's note that infinite grenades work only with 4 players or
    # fewer is left out: not so here)
    "main_menu/settings_select/multiplayer_setup/item_options_edit/cap_item_options":
        [None] + list(range(1, 15)) + [None, 15, 16] + [None] * 24,
    "main_menu/settings_select/multiplayer_setup/item_options_edit/var_weapon_set": list(range(13)) + [None],
}

# the map lists' first row: SINGLEPLAYER, MULTIPLAYER, CUSTOM SINGLEPLAYER or
# CUSTOM MULTIPLAYER maps (_map_kind)
MAP_KIND_CHOOSER = "main_menu/new_select/list_item_0_map_kind"

# changes to the PC version's widgets (by our names): attributes set, all
# their handlers replaced, children added, game data inputs added
WIDGET_PATCHES = {
    # The host returns here after a round with no menu history. Back must
    # reopen the main menu when it closes the last screen, as the lobby does.
    f"{MT}/connected/connected_map_select_wrapper": {"set": {
        "flags": "pass_unhandled_to_focused_child pause_game main_menu_if_no_history tag_controller_index force_handle_mouse",
    }},
    # (the main menu's MODS, before Quit: _mods)
    "main_menu/main_menu_select_list": {"insert_before": {"main_menu/main_menu_item_quit_game": [
        f'<child widget="main_menu/main_menu_item_mods" x="{MODS_ITEM_PLACE[0]}" y="{MODS_ITEM_PLACE[1]}"/>']}},
    f"{PLAYER_EDIT}/player_options_menu": {"insert_before": {
        f"{PLAYER_EDIT}/player_button_bar": [
            f'<child widget="{PLAYER_EDIT}/op_aim_assist" x="54" y="289"/>',
            f'<child widget="{PLAYER_EDIT}/op_shield_effects" x="54" y="316"/>']}},
    # (its rows 27 apart, not 30, so that ten fit above the help, which keeps
    # the three lines some have: ce_menus.py's CHILD_OFFSETS)
    f"{PLAYER_EDIT}/player_options_help": {"set": {"top": 345, "height": 60}},
    # (the gametype editor's OPENCE OPTIONS, after its own rows: _opence_options)
    f"{PLAYLIST_EDIT}/edit_playlist_select_list": {
        "insert_before": {f"{PLAYLIST_EDIT}/playlist_edit_button_bar": [f'<child widget="{OPENCE_ITEM}"/>']},
        "inputs": ["port playlist edit desc"]},
    # (the profile settings' picture: on Gamepad Setup's row, the profile's
    # button settings, BITMAP_FRAMES; menu_functions.c's
    # profile_gamepad_layout)
    f"{PE}/profile_edit_extended_desc_pic": {"inputs": ["port gamepad layout preview"]},
    # (straight to their screens: no "checking for updates" dialog, which
    # asked the PC version's servers)
    f"{MT}/multiplayer_type_join_internet_item": {"set": {"string_index": 6}, "handlers": [
        f'<on event="a" run="mp type set mode" open="{MT}/join_game/join_game_screen"/>',
        f'<on event="start" run="mp type set mode" open="{MT}/join_game/join_game_screen"/>',
        '<on event="left_mouse" run="mouse emit accept event"/>',
    ]},
    f"{MT}/multiplayer_type_create_internet_item": {"handlers": [
        '<on event="a" run="join controller to mp game"/>',
        f'<on event="a" run="mp type set mode" open="{MT}/connected/connected_map_select_wrapper"/>',
        '<on event="left_mouse" run="mouse emit accept event"/>',
    ]},
    f"{MT}/multiplayer_type_join_direct_item": {"handlers": [
        f'<on event="a" run="mp type set mode" open="{MT}/join_game/join_game_screen"/>',
        f'<on event="start" run="mp type set mode" open="{MT}/join_game/join_game_screen"/>',
        '<on event="left_mouse" run="mouse emit accept event"/>',
    ]},
    # (Create Game's LOCAL, then Co-op, after Create Game's: _create_local,
    # _coop)
    f"{MT}/multiplayer_type_select_list": {"insert_before": {
        f"{MT}/multiplayer_type_gametypes_item": [
            f'<child widget="{MT}/{row}" y="{multiplayer_row_place(row)[1]}"/>'
            for row in ["multiplayer_type_create_local_item", "multiplayer_type_coop_item"]],
    }},
    # (the Map screen's and New Game's lists: SINGLEPLAYER or MULTIPLAYER
    # maps, the first row's chooser's (_map_kind); B on the Map screen goes
    # from a co-op level's difficulties back to the level, else out of the
    # screen: menu_functions.c's map list)
    f"{MT}/mp_map_select/mp_map_select_list_2": {"swap": {"main_menu/new_select/list_item_0": MAP_KIND_CHOOSER},
                                                 "handlers": [
        '<on event="created" run="mp level list initialize"/>',
        '<on event="deleted" run="mp level list dispose"/>',
        f'<on event="custom_activation" run="mp level select" open="{MT}/connected/gametype_select_screen_wrapper"/>',
        '<on event="b" run="port map list back"/>',
        '<on event="back" run="port map list back"/>',
    ]},
    "main_menu/solo_level_select/solo_level_select_list": {"swap": {"main_menu/new_select/list_item_0": MAP_KIND_CHOOSER}},
    # (each description shows the other kind's maps too: a campaign level's
    # picture, name and words on the Map screen, a multiplayer map's on New
    # Game's)
    f"{MT}/mp_map_select/mp_map_right_item": {"children": [
        '<child widget="main_menu/solo_level_select/replay_level_right_name"/>',
        '<child widget="main_menu/solo_level_select/replay_level_right_pic"/>',
        '<child widget="main_menu/solo_level_select/replay_level_right_data"/>',
    ]},
    "main_menu/solo_level_select/replay_level_right_item": {"children": [
        f'<child widget="{MT}/mp_map_select/mp_map_right_name"/>',
        f'<child widget="{MT}/mp_map_select/mp_map_right_pic"/>',
        f'<child widget="{MT}/mp_map_select/mp_map_right_data"/>',
    ]},
    f"{MT}/join_game/header_join_game": {"children": [
        f'<child widget="{MT}/join_game/header_server_browser"/>',
        f'<child widget="{MT}/join_game/header_direct_link"/>',
    ]},
    # (the small font's line does not fit below their 4 units of offset)
    f"{MT}/join_game/ticker_player_info": {"set": {"text_y": 1}},
    f"{MT}/join_game/ticker_rules_info": {"set": {"text_y": 1}},
    # (Item Options' loadout rows, over its buttons)
    "main_menu/settings_select/multiplayer_setup/item_options_edit/item_options_menu": {"insert_before": {
        "main_menu/settings_select/multiplayer_setup/item_options_edit/item_button_bar": [
            f'<child widget="main_menu/settings_select/multiplayer_setup/item_options_edit/op_map_weapons" x="54" y="163"/>',
            f'<child widget="main_menu/settings_select/multiplayer_setup/item_options_edit/op_loadout" x="54" y="193"/>',
            f'<child widget="main_menu/settings_select/multiplayer_setup/item_options_edit/op_primary_weapon" x="54" y="223"/>',
            f'<child widget="main_menu/settings_select/multiplayer_setup/item_options_edit/op_secondary_weapon" x="54" y="253"/>',
        ]}},
    # (Teamplay Options' voice chat and vote kick rows, over its buttons:
    # Server Setup's only, _teamplay_options_extras)
    f"{TEAMPLAY_EDIT}/teamplay_options_menu": {"insert_before": {
        f"{TEAMPLAY_EDIT}/teamplay_button_bar": [
            f'<child widget="{TEAMPLAY_EDIT}/op_{key}" x="54" y="{163 + 30 * index}"/>'
            for index, (key, *_) in enumerate(TEAMPLAY_ROWS)]}},
    # (the PC's Vehicles row's Start opened Item Options)
    "main_menu/settings_select/multiplayer_setup/playlist_edit/playlist_edit_vehicles_list_item": {"handlers": [
        '<on event="a" open="main_menu/settings_select/multiplayer_setup/vehicle_options_edit/vehicle_options_screen"/>',
        '<on event="start" open="main_menu/settings_select/multiplayer_setup/vehicle_options_edit/vehicle_options_screen"/>',
        '<on event="left_mouse" run="mouse emit accept event"/>',
    ]},
    # (Direct Link's clipboard button in the place of the PC version's
    # Refresh, and Refresh in Get List's: the Server Browser's only, which
    # asks the public games' hosts again; "swap" puts another widget in a
    # child's place)
    f"{MT}/join_game/join_game_button_bar": {"swap": {
        f"{MT}/join_game/join_game_button_refresh": f"{MT}/join_game/button_clipboard",
        f"{MT}/join_game/join_game_button_update": f"{MT}/join_game/join_game_button_refresh",
    }},
}

# frames added after a PC bitmap's (by our names): (the Xbox map's bitmap,
# its frame, the size drawn at, where in the widget). The profile settings'
# picture gets the Xbox's Controller Setup's pictures of its five button
# settings (its first four frames are the thumbstick settings'), which show
# 512 by 235 of their 512 by 512: drawn at 279 wide, in the middle of the
# picture's 279 by 202
XBOX_CONTROLLER_PICTURES = f"ui\\shell\\{PE.replace('/', chr(92))}\\controller_edit\\config_controller"
BITMAP_FRAMES = {
    f"{PE}/profile_options": [(XBOX_CONTROLLER_PICTURES, 4 + preset, 279, 279, 0, 34) for preset in range(5)],
}

# the titles this port has that the PC version has not, set as its headers
# are (port/assets/menus/port_svg): bitmap name, text
FF_SETTINGS_PAGE_COUNT = 5

TITLES = {
    "main_menu/mods/header_mods": "MODS",
    f"{MT}/gamemode/header_gamemode": "GAMEMODE",
    f"{MT}/header_standard": "STANDARD",
    f"{MT}/header_ranked": "RANKED",
    f"{MT}/header_firefight": "FIREFIGHT",
    f"{MT}/header_forge": "FORGE",
    f"{MT}/service_record/header_service_record": "SERVICE RECORD",
    f"{MT}/join_game/header_server_browser": "SERVER BROWSER",
    f"{MT}/join_game/header_direct_link": "DIRECT LINK",
    f"{MT}/join_game/header_password": "PASSWORD",
    f"{MT}/lobby/header_lobby": "GAME LOBBY",
    f"{MT}/coop/header_player_2": "PLAYER 2 PROFILE",
    f"{MT}/lobby/header_add_player": "ADD PLAYER",
}


def title_backdrop(width: float) -> str:
    """a header's dark backdrop, as the PC headers' redraws have it, for text
    width units wide (the text is set over it in OpenCE: ce_menus.py)"""
    return "\n".join([
        '<svg xmlns="http://www.w3.org/2000/svg" width="512" height="64" viewBox="0 0 512 64">',
        "  <defs>",
        '    <filter id="soft" filterUnits="userSpaceOnUse" x="-60" y="-60" width="632" height="184">',
        '      <feGaussianBlur stdDeviation="14.23 13.06"/>', "    </filter>",
        '    <mask id="cut" maskUnits="userSpaceOnUse" x="0" y="0" width="512" height="64">',
        '      <rect width="512" height="59" fill="#fff"/>', "    </mask>", "  </defs>",
        '  <g mask="url(#cut)">',
        f'    <rect x="29.08" y="28.05" width="{width + 28:.2f}" height="40.95" fill="#021931" fill-opacity="0.893" '
        'filter="url(#soft)"/>',
        "  </g>", "</svg>", ""])


# the in-game pause menu's box, taller for the buttons this port adds
# (menu_tags.c's pause_patch: SETTINGS, and the host's END GAME): the
# pausebox5 redraw's rounded box (ui-svg-handmade/shell/bitmaps), drawn to
# hold each number of buttons, one frame each (the Xbox's 2-button box is
# 159 units high, and each button 35 more)
PAUSE_BOX_BUTTONS = [3, 4]
# the campaign pause box, tall enough for five rows (SETTINGS added) or six
# (network co-op's FRIENDLY FIRE too) at the PC version's pause menu's pitch
# (30) with the prompts still under the last row: the stock box is 159, and
# those rows need 36 and 66 more (menu_tags.c's pause_campaign_patch); the
# mission objectives' box beside it grows with it
PAUSE_BOX_EXTRA_HEIGHTS = [195, 225]


def pause_box_height(buttons: int) -> int:
    return 159 + (buttons - 2) * 35


def pause_box_svg(piece: str, height: int, header: bool = False) -> str:
    """a piece of the pause box (left cap, centre strip, right cap; 9-slice:
    the strip tiles across), height units high in its 256-high texture"""
    width = 4 if piece == "center" else 16
    lines = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="256" viewBox="0 0 {width} 256">']
    fill = "#0a2649"
    body_clip = ""
    if not header:
        # Keep the translucent footer clear of the body: blending two
        # 75%-opaque fills would make it nearly opaque and tint it blue.
        lines += [f'  <defs><clipPath id="body-area"><rect x="-16" y="0" width="48" height="{height - 28}"/></clipPath></defs>']
        body_clip = ' clip-path="url(#body-area)"'
    if height in PAUSE_BOX_EXTRA_HEIGHTS or height in map(pause_box_height, PAUSE_BOX_BUTTONS):
        # The stock campaign body fades from blue to dark navy at 75% opacity.
        # Scale it within the body, excluding the objective header or key footer.
        top, bottom = (29, height - 2) if header else (2, height - 28)
        lines += [f'  <defs><linearGradient id="body" gradientUnits="userSpaceOnUse" x1="0" y1="{top}" x2="0" y2="{bottom}">',
                  '    <stop offset="0" stop-color="#0a2649"/>',
                  '    <stop offset="1" stop-color="#050d1c"/>',
                  '  </linearGradient></defs>']
        fill = "url(#body)"
    if piece == "center":
        lines += [f'  <rect x="0" y="2" width="4" height="{height - 4}" fill="{fill}" fill-opacity="0.75"{body_clip}/>',
                  '  <rect x="0" y="0" width="4" height="2" fill="#2896ff"/>',
                  f'  <rect x="0" y="{height - 2}" width="4" height="2" fill="#2896ff"/>']
    else:
        bottom, corner = height - 1, height - 9.5
        cap = [f'    <path d="M9.5,1 H17 V{bottom} H9.5 A8.5,8.5 0 0 1 1,{corner} V9.5 A8.5,8.5 0 0 1 9.5,1 Z" '
               f'fill="{fill}" fill-opacity="0.75" fill-rule="evenodd"{body_clip}/>',
               f'    <path d="M17,1 H9.5 A8.5,8.5 0 0 0 1,9.5 V{corner} A8.5,8.5 0 0 0 9.5,{bottom} H17" fill="none" '
               'stroke="#2896ff" stroke-width="2"/>']
        lines += (['  <g transform="matrix(-1 0 0 1 16 0)">'] + cap + ['  </g>']) if piece == "right" else \
            [line[2:] for line in cap]
    # The stock objectives have a 28-unit header; the button key has a
    # 28-unit footer. Clip the black fill to the rounded panel interior.
    top = 2 if header else height - 28
    divider = 28 if header else height - 28
    if piece == "left":
        lines += [f'  <defs><clipPath id="strip"><rect x="2" y="2" width="{width + 16}" height="{height - 4}" rx="7.5"/></clipPath></defs>']
    if piece == "right":
        lines += [f'  <defs><clipPath id="strip"><rect x="-16" y="2" width="{width + 14}" height="{height - 4}" rx="7.5"/></clipPath></defs>']
    clip = ' clip-path="url(#strip)"' if piece != "center" else ""
    opacity = ' fill-opacity="0.75"' if not header else ""
    lines += [f'  <g{clip}>',
              f'    <rect x="0" y="{top}" width="{width}" height="26" fill="black"{opacity}/>',
              f'    <rect x="0" y="{divider}" width="{width}" height="0.75" fill="#2896ff"/>',
              '  </g>']
    lines += ["</svg>", ""]
    return "\n".join(lines)


# where the PC headers' text is (units of their 512x64), and its colour
TITLE_LEFT, TITLE_TOP, TITLE_CAP = 30.68, 33.5, 21.5
TITLE_COLOR = (0x29, 0x95, 0xFD, 255)


def _header(name: str, bitmap: str, platform=None) -> list:
    return _widget(name, [("controller", 1), ("left", 35), ("top", 11), ("width", 363), ("height", 59),
                          ("bitmap", bitmap), ("platform", platform)], [])


# a text field's B and Back do nothing of their own (the engine would go
# back from the field): its screen's (SCREEN_BACK) cancel the edit, else
# leave
FIELD_BACK = ['<on event="b"/>', '<on event="back"/>']
SCREEN_BACK = ['<on event="b" run="gamespy back handler"/>', '<on event="back" run="gamespy back handler"/>']


def _value_row(base: str, key: str, label_index: int, run: str) -> list:
    """a row of a label and a value the player sets (A edits it)"""
    lines = _widget(f"{base}/op_{key}", [("width", 512), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                                         ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                    [f'<on event="a" run="{run}"/>', f'<on event="start" run="{run}"/>', *FIELD_BACK,
                     '<on event="left_mouse" run="mouse emit accept event"/>',
                     f'<child widget="{base}/{key}_label"/>', f'<child widget="{base}/{key}_value" x="230" y="1"/>'])
    lines += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 230), ("height", 22),
                                             ("string_list", f"{base}/labels"), ("string_index", label_index or None),
                                             ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                             ("text_y", 4)], [])
    lines += _widget(f"{base}/{key}_value", [("type", "text"), ("controller", 1), ("width", 280), ("height", 22),
                                             ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("align", "center"),
                                             ("text_y", 5), ("text_flags", "no_focus_test")], [])
    return lines


def _join_game_extras() -> list:
    """the browser's added titles, Direct Link's clipboard button, and the
    password screen"""
    base = f"{MT}/join_game"
    lines = _password_screen()
    lines += _header(f"{base}/header_server_browser", f"{base}/header_server_browser")
    lines += _header(f"{base}/header_direct_link", f"{base}/header_direct_link")
    lines += _widget(f"{base}/button_clipboard", [("type", "text"), ("width", 128), ("height", 24),
                                                  ("bitmap", "bitmaps/text_button_background"),
                                                  ("text", "PASTE LINK"), ("font", "ui\\small_ui"),
                                                  ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="direct ip connect go"/>', '<on event="start" run="direct ip connect go"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    return lines


def _password_screen() -> list:
    """the server browser's password screen: a game with a password chosen
    (menu_functions.c's lobby_browser_select), its password typed (shown as
    stars), JOIN GAME joins it if it is the game's; else its help says so"""
    base = f"{MT}/join_game/password"
    spec = {"screen": "password_screen", "spacing": 28, "help_top": 120,
            "header": ("header_password", f"{MT}/join_game/header_password")}
    extra = _value_row(base, "password", 0, "port password edit")
    extra += _button(f"{base}/button_defaults", 3, [])
    extra += _widget(f"{base}/button_ok", [("type", "text"), ("width", 128), ("height", 24),
                                           ("bitmap", "bitmaps/text_button_background"), ("text", "JOIN GAME"),
                                           ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"),
                                           ("text_y", 2)],
                     ['<on event="a" run="port password join"/>', '<on event="start" run="port password join"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    extra += _strings(f"{base}/labels", ["PASSWORD:"])
    # (by its state: menu_functions.c's password_screen_update)
    extra += _strings(f"{base}/help_strings", [
        "",
        "This game has a password. Type it, then press\\nEnter to join.",
        "That is not the game's password. Try again.",
        "Could not join the game.",
    ])
    lines = _screen(base, spec, [(f"{base}/op_password", None)], ["port password update"],
                    ['<on event="created" run="port password init"/>'], extra)
    # (no Defaults: the bar is JOIN GAME and CANCEL; B goes back at once,
    # even while typing, which the screen starts with)
    lines = [line.replace(f'<child widget="{base}/button_defaults" y="1"/>', "") for line in lines]
    lines = [line.replace('<on event="b" back="true"/>', '<on event="b" run="port password back"/>')
             .replace('<on event="back" back="true"/>', '<on event="back" run="port password back"/>')
             for line in lines]
    return lines


def _server_settings() -> list:
    """Create Game's server settings (in the PC version's place): the game's
    name, the most players (up to the port's 128), its invite link, and
    whether the server browser lists it (PUBLIC or PRIVATE: an internet
    game's)"""
    base = f"{MT}/server_settings"
    # (thirteen rows: shifted up with 22px step so BOT OPTIONS sits cleanly above help_top=366)
    spec = {"screen": "server_settings_screen", "row_top": 66, "spacing": 22, "help_top": 366,
            "header": ("header_server_settings", f"{base}/header_server_settings")}
    rows, extra = [], []
    extra += _value_row(base, "server_name", 0, "ss edit server name")
    rows.append((f"{base}/op_server_name", None))
    extra += _widget(f"{base}/op_max_players", [("width", 512), ("height", 28),
                                                ("flags", "pass_unhandled_to_focused_child"),
                                                ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<child widget="{base}/max_players_label"/>',
                      f'<child widget="{base}/max_players_spinner" x="320" y="1"/>'])
    extra += _widget(f"{base}/max_players_label", [("type", "text"), ("controller", 1), ("width", 300),
                                                   ("height", 22), ("string_list", f"{base}/labels"),
                                                   ("string_index", 1), ("font", "ui\\large_ui"),
                                                   ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/max_players_spinner",
                     [("type", "spinner"), ("left", 3), ("top", 2), ("width", 147), ("height", 20),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                      ("strings", "|".join(str(count) for count in MAXIMUM_PLAYERS)), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1),
                      ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                      ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 150 19 156")], [])
    rows.append((f"{base}/op_max_players", None))
    extra += _value_row(base, "invite", 2, "ss copy invite")
    rows.append((f"{base}/op_invite", None))
    # (PUBLIC or PRIVATE: menu_functions.c's server_settings_update)
    extra += _widget(f"{base}/op_listing", [("width", 512), ("height", 28),
                                            ("flags", "pass_unhandled_to_focused_child"),
                                            ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<child widget="{base}/listing_label"/>',
                      f'<child widget="{base}/listing_spinner" x="320" y="1"/>'])
    extra += _widget(f"{base}/listing_label", [("type", "text"), ("controller", 1), ("width", 300),
                                               ("height", 22), ("string_list", f"{base}/labels"),
                                               ("string_index", 9), ("font", "ui\\large_ui"),
                                               ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/listing_spinner",
                     [("type", "spinner"), ("left", 3), ("top", 2), ("width", 147), ("height", 20),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                      ("strings", "PUBLIC|PRIVATE"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1),
                      ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                      ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 150 19 156")],
                     ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    rows.append((f"{base}/op_listing", None))
    # PASSWORD (a public internet game's: menu_functions.c's server_settings_update),
    # which players from the server browser must type
    extra += _value_row(base, "password", 10 + len(COOP_SETUP_SCREENS), "ss edit server password")
    rows.append((f"{base}/op_password", None))
    # MAP SHARING (map_share.c), a screen of its own as co-op's options are,
    # for any game: SETUP_OPTIONS_PLACE below it
    key, _, _, _ = MAP_SHARE_SETUP
    target = f"{base}/{key}/{key}_screen"
    extra += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                         ("flags", "pass_unhandled_to_focused_child"),
                                         ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<on event="a" open="{target}"/>', f'<on event="start" open="{target}"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>',
                      f'<child widget="{base}/{key}_label"/>'])
    extra += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                                             ("string_list", f"{base}/labels"),
                                             ("string_index", 11 + len(COOP_SETUP_SCREENS)),
                                             ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                             ("text_y", 4)], [])
    rows.append((f"{base}/op_{key}", None, SETUP_OPTIONS_PLACE - 1))
    # co-op's options, in the place of the gametype's rows, which co-op hides
    # as multiplayer hides co-op's (menu_functions.c's server_settings_update):
    # rows opening their screens, as the gametype's options' do
    # (_setup_option_screen; COOP_SETUP_SCREENS)
    for index, (key, title, _, _) in enumerate(COOP_SETUP_SCREENS):
        target = f"{base}/{key}/{key}_screen"
        extra += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                             ("flags", "pass_unhandled_to_focused_child"),
                                             ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<on event="a" open="{target}"/>', f'<on event="start" open="{target}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/{key}_label"/>'])
        extra += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                                                 ("string_list", f"{base}/labels"), ("string_index", 10 + index),
                                                 ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                                 ("text_y", 4)], [])
        rows.append((f"{base}/op_{key}", None, SETUP_OPTIONS_PLACE + index))
    # the gametype's options for this game (the gametype editor's screens,
    # editing a copy of the gametype chosen: "port setup edit")
    for index, (key, screen) in enumerate(SETUP_OPTION_SCREENS):
        target = f"main_menu/settings_select/multiplayer_setup/{screen}"
        extra += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                             ("flags", "pass_unhandled_to_focused_child"),
                                             ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<on event="a" run="port setup edit" open="{target}"/>',
                          f'<on event="start" run="port setup edit" open="{target}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/{key}_label"/>', f'<child widget="{base}/{key}_value" x="230" y="1"/>'])
        extra += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 230), ("height", 22),
                                                 ("string_list", f"{base}/labels"), ("string_index", 3 + index),
                                                 ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                                 ("text_y", 4)], [])
        extra += _widget(f"{base}/{key}_value", [("type", "text"), ("controller", 1), ("width", 280), ("height", 22),
                                                 ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("align", "center"),
                                                 ("text_y", 5), ("text_flags", "no_focus_test")], [])
        rows.append((f"{base}/op_{key}", None, SETUP_OPTIONS_PLACE + index))
    extra += _widget(f"{base}/op_ff_player_scale", [("width", 512), ("height", 28),
                                                    ("flags", "pass_unhandled_to_focused_child"),
                                                    ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<child widget="{base}/ff_player_scale_label"/>',
                      f'<child widget="{base}/ff_player_scale_spinner" x="320" y="1"/>'])
    extra += _widget(f"{base}/ff_player_scale_label", [("type", "text"), ("controller", 1), ("width", 300),
                                                       ("height", 22), ("string_list", f"{base}/labels"),
                                                       ("string_index", 13 + len(COOP_SETUP_SCREENS)),
                                                       ("font", "ui\\large_ui"), ("color", "#FF2896FF"),
                                                       ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/ff_player_scale_spinner",
                     [("type", "spinner"), ("left", 3), ("top", 2), ("width", 147), ("height", 20),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                      ("strings", "0%|5%|10%|20%|50%"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1),
                      ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                      ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 150 19 156")],
                     ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    rows.append((f"{base}/op_ff_player_scale", None, SETUP_OPTIONS_PLACE + 3))
    # BOT OPTIONS below TEAMPLAY OPTIONS in Standard Multiplayer, and at the
    # bottom of the menus in Firefight / Co-op Campaign
    key = BOTS_SETUP[0]
    target = f"{base}/{key}/{key}_screen"
    extra += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                         ("flags", "pass_unhandled_to_focused_child"),
                                         ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<on event="a" open="{target}"/>', f'<on event="start" open="{target}"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>',
                      f'<child widget="{base}/{key}_label"/>', f'<child widget="{base}/{key}_value" x="230" y="1"/>'])
    extra += _widget(f"{base}/{key}_value", [("type", "text"), ("controller", 1), ("width", 280), ("height", 22),
                                             ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("align", "center"),
                                             ("text_y", 5), ("text_flags", "no_focus_test")], [])
    extra += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 230), ("height", 22),
                                             ("string_list", f"{base}/labels"),
                                             ("string_index", 12 + len(COOP_SETUP_SCREENS)),
                                             ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13),
                                             ("text_y", 4)], [])
    rows.append((f"{base}/op_{key}", None, SETUP_OPTIONS_PLACE + 6))
    # SCORING toggle at the bottom of the menus for Firefight / Co-op Campaign
    extra += _widget(f"{base}/op_scoring", [("width", 512), ("height", 28),
                                            ("flags", "pass_unhandled_to_focused_child"),
                                            ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                     [f'<child widget="{base}/scoring_label"/>',
                      f'<child widget="{base}/scoring_spinner" x="320" y="1"/>'])
    extra += _widget(f"{base}/scoring_label", [("type", "text"), ("controller", 1), ("width", 300),
                                               ("height", 22), ("string_list", f"{base}/labels"),
                                               ("string_index", 14 + len(COOP_SETUP_SCREENS)),
                                               ("font", "ui\\large_ui"), ("color", "#FF2896FF"),
                                               ("text_x", 13), ("text_y", 4)], [])
    extra += _widget(f"{base}/scoring_spinner",
                     [("type", "spinner"), ("left", 3), ("top", 2), ("width", 147), ("height", 20),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                      ("strings", "ON|OFF"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1),
                      ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                      ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 150 19 156")],
                     ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    rows.append((f"{base}/op_scoring", None, SETUP_OPTIONS_PLACE + 5))
    extra += _button(f"{base}/button_defaults", 3, [])
    extra += _widget(f"{base}/button_ok", [("type", "text"), ("width", 128), ("height", 24),
                                           ("bitmap", "bitmaps/text_button_background"), ("text", "START GAME"),
                                           ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"),
                                           ("text_y", 2)],
                     [f'<on event="a" run="ss start game" open="{MT}/lobby/lobby_screen"/>',
                      f'<on event="start" run="ss start game" open="{MT}/lobby/lobby_screen"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    extra += _strings(f"{base}/labels", ["GAME NAME:", "MAXIMUM PLAYERS:", "INVITE LINK:", "GAME TYPE:",
                                         "PLAYER OPTIONS:", "ITEM OPTIONS:", "VEHICLE OPTIONS:", "INDICATOR OPTIONS:",
                                         "TEAMPLAY OPTIONS:", "LISTING:",
                                         *(f"{title}:" for _, title, _, _ in COOP_SETUP_SCREENS), "PASSWORD:",
                                         f"{MAP_SHARE_SETUP[1]}:",
                                         f"{BOTS_SETUP[1]}:",
                                         "SCALE WITH PLAYERS:",
                                         "SCORING:"])
    extra += _strings(f"{base}/help_strings", [
        "",
        "The name the game shows in the lists of games.\\nEnter changes it.",
        "The most players the game takes (up to 128).",
        "Players join this game by its invite link, or your\\nDiscord invite. Enter copies the link again.",
        "The gametype and its rules, for this game.",
        "Lives, health, shields and respawning, for this game.",
        "Weapons, grenades and starting equipment, for\\nthis game.",
        "Each team's vehicles and their respawn time, for\\nthis game.",
        "The motion tracker and nav points, for this game.",
        "Friendly fire and team balance, for this game.",
        # (LISTING's, by its choice)
        "Anyone can see and join your game: it is listed\\nin everyone's Server Browser.",
        "Only players with your invite link can join.",
        # (co-op's options' rows: COOP_SETUP_SCREENS)
        *(help_text for _, _, help_text, _ in COOP_SETUP_SCREENS),
        # (PASSWORD's)
        "Players from the Server Browser must type it to\\njoin (an invite link needs none). Enter sets it.",
        # (MAP SHARING's)
        MAP_SHARE_SETUP[2],
        # (BOTS': BOTS_SETUP)
        BOTS_SETUP[2],
        # (FIREFIGHT SCALE WITH PLAYERS)
        "Extra enemies added per player from the 2nd player\\nonwards (non-compounding from the base count).",
        # (SCORING)
        "Turn Campaign and Firefight scoring on or off for\\nthis game.",
    ])
    lines = _screen(base, spec, rows, ["server settings update"],
                    ['<on event="created" run="server settings init"/>'], extra)
    # (co-op's options' screens, map sharing's and the bots')
    for key, title, _, screen_rows in [*COOP_SETUP_SCREENS, MAP_SHARE_SETUP, BOTS_SETUP]:
        lines += _setup_option_screen(f"{base}/{key}", key, screen_rows)
    # (no Defaults: the bar is START and CANCEL; B cancels the name's edit
    # first)
    lines = [line.replace(f'<child widget="{base}/button_defaults" y="1"/>', "") for line in lines]
    lines = [line.replace('<on event="b" back="true"/>', SCREEN_BACK[0])
             .replace('<on event="back" back="true"/>', SCREEN_BACK[1]) for line in lines]
    return lines


MAXIMUM_PLAYERS = [2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128]
# CO-OP OPTIONS' amounts of PER PLAYER (network.coop_enemies, percentages)
# and STATIC MULTIPLIER (network.coop_enemies_multiplier), as menu_functions.c's
# gametype_options have their values
COOP_ENEMIES_PERCENTAGES = [25, 50, 100, 150, 200]
COOP_ENEMIES_MULTIPLIERS = [2, 4, 8, 16, 32]
# co-op's Server Setup options, in screens of their own as the gametype's
# are (its rows: _server_settings), each its key, its title, its row's help
# and its rows (as TEAMPLAY_ROWS: each spinner's key, label, words and the
# help of each of them; menu_functions.c's gametype_options give their
# settings' values)
COOP_SETUP_SCREENS = [
    ("coop_options", "CO-OP OPTIONS", "Friendly fire, the enemies and player collisions,\\nfor this game.", [
        ("coop_friendly_fire", "FRIENDLY FIRE:", ["OFF", "ON", "SHIELD ONLY", "EXPLOSIVES ONLY"], [
            "Players can not be hurt by weapons and explosives\\nfired by the other players.",
            "Players can be hurt by weapons or explosives\\nfired by the other players.",
            "Damage from the other players will only reduce\\nshields. Health will be unaffected.",
            "Players can be hurt by damage from explosives\\nfired by the other players.",
        ]),
        ("coop_extra_enemies", "EXTRA ENEMIES:", ["PER PLAYER", "STATIC MULTIPLIER", "NONE"], [
            "Enemy squads grow with the players: by the value of\\n'Per Player' for each player past the first.",
            "Enemy squads use 'Multiplier', however many players\\nthere are.",
            "Enemy squads are as the campaign has them.",
        ]),
        ("coop_extra_pickups", "EXTRA PICKUPS:", ["PER PLAYER", "STATIC MULTIPLIER", "NONE"], [
            "Pickups grow with the players: by the value of\\n'Per Player' for each player past the first.",
            "Pickups use 'Multiplier', up to eight times as many,\\nhowever many players there are.",
            "Pickups are as the campaign has them.",
        ]),
        ("coop_extra_vehicles", "EXTRA VEHICLES:", ["PER PLAYER", "STATIC MULTIPLIER", "NONE"], [
            "Vehicles respawn after being driven a short distance\\nbased on the number of players: by two thirds the value of\\n'Per Player' for each player past the first.",
            "Vehicles respawn after being driven a short distance,\\nwith extra copies equal to ('Multiplier' - 1) * 2/3,\\nrounded down, however many players there are.",
            "Vehicles are as the campaign has them.",
        ]),
        ("coop_enemies_multiplier", "MULTIPLIER:", [f"{value}X" for value in COOP_ENEMIES_MULTIPLIERS],
         ["Static amounts for the options set to this mode.\\nPickups are capped at eight times as many."] * len(COOP_ENEMIES_MULTIPLIERS)),
        ("coop_enemies_per_player", "PER PLAYER:", [f"{value}%" for value in COOP_ENEMIES_PERCENTAGES],
         ["For each player past the first, options set to Per\\nPlayer get this much more (100%: as many again)."] *
         len(COOP_ENEMIES_PERCENTAGES)),
        ("coop_player_collisions", "PLAYER COLLISIONS:", ["ON", "OFF"], [
            "Players bump into each other, as in the campaign.",
            "Players walk through each other, so that no one\\nblocks a doorway. The AI's characters still block.",
        ]),
        # (network.coop_ally_friendly_fire: game_engine.c's
        # game_engine_friendly_damage)
        ("coop_ally_friendly_fire", "ALLY FRIENDLY FIRE:", ["ON", "OFF"], [
            "Players can hurt and kill their AI allies, as in\\nthe campaign.",
            "Players can not hurt their AI allies, so that no\\none can kill Keyes or the Marines a level needs.",
        ]),
    ]),
    ("voice_options", "VOICE AND VOTING", "Voice chat, text chat and votes to kick a player,\\nfor this game.",
     TEAMPLAY_ROWS),
]
# Server Setup's map sharing (port/linux/game/map_share.c), for any game,
# laid out as co-op's options are (its key, its title, its row's help and
# its rows: menu_functions.c's gametype_options give their settings' values)
MAP_SHARE_UPLOADS = (256, 512, 1024, 2048, 4096, 8192, 16384, 20480)
MAP_SHARE_SETUP = ("map_share_options", "MAP SHARING",
                   "Sending players who join without the game's\\nCustom Edition map the map, and how fast.", [
    ("map_share", "SHARE MAPS:", ["ON", "OFF"], [
        "Players who join without the map download it\\nfrom you and the other players.",
        "Players who join without the map cannot play.",
    ]),
    ("map_share_upload", "UPLOAD LIMIT:", [f"{kb} KB/S" if kb < 1024 else f"{kb // 1024} MB/S"
                                           for kb in MAP_SHARE_UPLOADS],
     ["The most you send of maps, so that the game\\nkeeps the rest of your upload."] * len(MAP_SHARE_UPLOADS)),
])
# Server Setup's bots (port/linux/game/bots.c, the settings' bots.count,
# bots.skill and bots.teams), a LOCAL game's only (no other machine joins
# it), in MAP SHARING's place, which a LOCAL game hides (menu_functions.c's
# server_settings_update); laid out as co-op's options are. The bots join
# the lobby as the screen's OK sets them, in a Slayer game (and SWAT,
# Infection and Gun Game, Slayer's)
BOTS_COUNTS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15]
BOTS_SKILLS = ["easy", "normal", "heroic", "legendary"]
BOTS_SKILL_CHOICES = BOTS_SKILLS
BOTS_SETUP = ("bots_options", "BOT OPTIONS",
              "Configure AI bots: count, difficulty, replacement\\non join, and Campaign/Firefight scoring.", [
    ("bots_count", "BOTS:", [str(count) if count else "NONE" for count in BOTS_COUNTS],
     ["No AI bots in this game."] +
     ["AI bots in this game (1-3 in Campaign & Firefight,\\nup to Max Players - 1 in Standard Multiplayer)."] *
     (len(BOTS_COUNTS) - 1)),
    ("bots_skill", "BOT DIFFICULTY:", [skill.upper() for skill in BOTS_SKILL_CHOICES], [
        "Easy bots. In Campaign & Firefight, active bots\\napply a -0.5x score multiplier modifier.",
        "Normal bots. In Campaign & Firefight, active bots\\napply a -1.0x score multiplier modifier.",
        "Heroic bots. In Campaign & Firefight, active bots\\napply a -1.5x score multiplier modifier.",
        "Legendary bots. In Campaign & Firefight, active bots\\napply a -2.0x score multiplier modifier.",
    ]),
    ("bots_replace", "REPLACE ON JOIN:", ["ON", "OFF"], [
        "Joining players replace one bot at a time when\\nthe server reaches maximum players.",
        "Bots occupy permanent player slots. The server\\nreads as full when players + bots reach max players.",
    ]),
    ("bots_teams", "TEAMS:", ["EVEN", "AGAINST YOU"], [
        "In a team game, the bots join both teams, evening\\nthem with the players.",
        "In a team game, every bot is on the team the\\nplayers are not on.",
    ]),
    ("bots_scoring", "SCORING:", ["ON", "OFF"], [
        "Enable Campaign & Firefight scoring for kills,\\nmedals and time bonuses.",
        "Turn Campaign & Firefight scoring off.",
    ]),
])
# (Server Setup's place of its options' rows, co-op's or the gametype's: after
# MAP SHARING's)
SETUP_OPTIONS_PLACE = 6
# (their screens' titles)
TITLES.update({f"{MT}/server_settings/{key}/header_{key}": title
               for key, title, _, _ in [*COOP_SETUP_SCREENS, MAP_SHARE_SETUP, BOTS_SETUP]})
# (the rows of a co-op options screen in one place, the one shown: PER
# PLAYER's and MULTIPLIER's, by EXTRA ENEMIES')
SETUP_OPTION_SAME_PLACE = set()


def _setup_option_screen(base: str, key: str, rows: list) -> list:
    """a screen of options laid out as the gametype's are (Teamplay
    Options'), its spinners setting config.toml's settings (menu_functions.c's
    "port setup options init" and "port setup options save", OK), each
    choice's help shown as the gametype's are ("game settings lists text
    update": the help strings, each spinner's choices' in turn)"""
    lines = _widget(f"{base}/{key}_screen", [("width", 640), ("height", 480),
                                            ("flags", "pass_unhandled_to_focused_child"),
                                            ("bitmap", "bitmaps/gradient")],
                    [f'<child widget="{base}/header_{key}"/>', f'<child widget="{base}/{key}_menu"/>'])
    lines += _header(f"{base}/header_{key}", f"{base}/header_{key}")
    lines += _widget(f"{base}/{key}_help", [("type", "text"), ("controller", 1), ("left", 68), ("top", 341),
                                           ("width", 482), ("height", 79), ("string_list", f"{base}/cap_{key}"),
                                           ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    children = ['<data input="game settings lists text update"/>',
                '<on event="created" run="port setup options init"/>']
    place = -1
    for row_key, *_ in rows:
        if row_key not in SETUP_OPTION_SAME_PLACE:
            place += 1
        children.append(f'<child widget="{base}/op_{row_key}" x="54" y="{73 + 30 * place}"/>')
    children.append(f'<child widget="{base}/{key}_button_bar" y="414"/>')
    lines += _widget(f"{base}/{key}_menu", [("type", "column_list"), ("width", 640), ("height", 480),
                                           ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                           ("description", f"{base}/{key}_help")], children)
    lines += _widget(f"{base}/{key}_button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                                 ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<data input="common button bar update"/>', f'<child widget="{base}/{key}_button_ok" x="380" y="1"/>',
                      '<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += _button(f"{base}/{key}_button_ok", 1, ['<on event="a" run="port setup options save"/>',
                                                     '<on event="start" run="port setup options save"/>'])
    for index, (row_key, label, strings, _) in enumerate(rows):
        lines += _widget(f"{base}/op_{row_key}", [("width", 512), ("height", 28),
                                                 ("flags", "pass_unhandled_to_focused_child"),
                                                 ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<child widget="{base}/{row_key}_label"/>',
                          f'<child widget="{base}/{row_key}_spinner" x="286" y="1"/>'])
        lines += _widget(f"{base}/{row_key}_label", [("type", "text"), ("controller", 1), ("width", 300),
                                                     ("height", 22), ("string_list", f"{base}/{key}_labels"),
                                                     ("string_index", index), ("font", "ui\\large_ui"),
                                                     ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{base}/{row_key}_spinner",
                         [("type", "spinner"), ("top", 2), ("width", 206), ("height", 20),
                          ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("string_list", f"{base}/var_{row_key}"), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4),
                          ("list_flags", "items_from_strings"),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -13 19 -7"), ("footer_bounds", "7 208 19 214")],
                         ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
        lines += _strings(f"{base}/var_{row_key}", strings)
    lines += _strings(f"{base}/{key}_labels", [label for _, label, *_ in rows])
    lines += _strings(f"{base}/cap_{key}", [text for *_, helps in rows for text in helps])
    return lines
# Server Setup's rows of the gametype's options: the gametype editor's
# screens
SETUP_OPTION_SCREENS = [
    ("game_type", "playlist_edit/game_type_select/gametype_select_screen"),
    ("player_options", "player_options_edit/player_options_screen"),
    ("item_options", "item_options_edit/item_options_screen"),
    ("vehicle_options", "vehicle_options_edit/vehicle_options_screen"),
    ("indicator_options", "indicator_options_edit/indicator_options_screen"),
    ("team_options", "teamplay_options_edit/teamplay_options_screen"),
]


def _lobby() -> list:
    """the lobby the host's and the joining players wait in (the Xbox's
    pregame's functions, which need no widget of theirs): up to the port's
    128 players, scrolling; the game's map and gametype; the countdown"""
    base = f"{MT}/lobby"
    # (split screen: another controller's START joins it, its B leaves alone:
    # menu_functions.c's lobby_join. After a game it is the only screen,
    # network_game_reset_to_pregame_ui opening it in place of the Xbox's
    # pregame: leaving it goes to the main menu, as the PC version's postgame
    # screens do)
    lines = _widget(f"{base}/lobby_screen", [("width", 640), ("height", 480),
                                             ("flags", "pass_unhandled_to_focused_child main_menu_if_no_history"),
                                             ("bitmap", "bitmaps/gradient")],
                    ['<on event="created" run="port lobby open"/>',
                     '<on event="created" run="net server accept conx"/>',
                     '<on event="created" run="net server allow start"/>',
                     '<on event="b" run="port lobby leave" back="true"/>',
                     '<on event="back" run="port lobby leave" back="true"/>',
                     f'<on event="start" run="port lobby join" open="{base}/player_profile_screen" branch="true"/>',
                     '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                     f'<child widget="{base}/lobby_list"/>',
                     f'<child widget="{base}/header_lobby"/>',
                     f'<child widget="{base}/lobby_join_help"/>'])
    lines += _header(f"{base}/header_lobby", f"{base}/header_lobby")
    lines += _widget(f"{base}/lobby_join_help", [("type", "text"), ("controller", 1), ("left", 355), ("top", 446),
                                                 ("width", 275), ("height", 24), ("font", "ui\\small_ui"),
                                                 ("color", "#FF2896FF"), ("align", "right"), ("text_y", 5),
                                                 ("text_flags", "no_focus_test")], [])
    # (its rows: any controller's left and right switch its own player's team,
    # on the rows only, so that they move along the buttons)
    rows = [f'<child widget="{base}/list_item_{index}" x="20" y="{73 + 30 * index}"/>' for index in range(11)]
    lines += _widget(f"{base}/lobby_list", [("type", "column_list"), ("width", 640), ("height", 480),
                                            ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                            ("description", f"{base}/lobby_desc")],
                     ['<data input="net splitscreen prejoin players"/>', '<data input="port lobby update"/>',
                      *rows,
                      f'<child widget="{base}/lobby_button_bar" y="414"/>'])
    for index in range(11):
        lines += _widget(f"{base}/list_item_{index}",
                         [("width", 390), ("height", 28),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if index == 0 else "bitmaps/sel_list_item_bkd"),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("align", "center"), ("text_y", 3)],
                         ['<on event="left right" run="swap player team"/>',
                          '<child widget="main_menu/new_select/list_item_text" x="25"/>',
                          '<child widget="main_menu/new_select/list_item_arrows"/>'])
    lines += _widget(f"{base}/lobby_desc", [("width", 640), ("height", 480)],
                     ['<child widget="main_menu/current_profile_name"/>',
                      f'<child widget="{base}/lobby_right_item" x="22" y="2"/>'])
    lines += _widget(f"{base}/lobby_right_item", [("controller", 1), ("left", 406), ("top", 75), ("width", 162),
                                                  ("height", 326),
                                                  ("bitmap", "bitmaps/spinner_list_3_wide_item_background")],
                     # (a campaign level's picture and name in place of the
                     # map's, for network co-op: menu_functions.c's
                     # lobby_map_show)
                     [f'<child widget="{base}/lobby_map_pic"/>', f'<child widget="{base}/lobby_map_name"/>',
                      '<child widget="main_menu/solo_level_select/replay_level_right_pic"/>',
                      '<child widget="main_menu/solo_level_select/replay_level_right_name"/>',
                      f'<child widget="{base}/lobby_game_data"/>'])
    lines += _widget(f"{base}/lobby_map_pic", [("controller", 1), ("left", 419), ("top", 87), ("width", 140),
                                               ("height", 114), ("bitmap", "ui\\shell\\bitmaps\\mp_map_grafix")], [])
    lines += _widget(f"{base}/lobby_map_name", [("type", "text"), ("controller", 1), ("left", 417), ("top", 204),
                                                ("width", 146), ("height", 43), ("string_list", "main_menu/mp_map_list"),
                                                ("font", "ui\\large_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/lobby_game_data", [("type", "text"), ("controller", 1), ("left", 417), ("top", 250),
                                                 ("width", 146), ("height", 144), ("font", "ui\\small_ui"),
                                                 ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/lobby_button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                                  ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     # (SWITCH TEAM first: hidden in a game without teams, which
                     # moves the focus to the first shown, START NOW; at the end
                     # it traps none)
                     [f'<child widget="{base}/lobby_button_team" x="120" y="1"/>',
                      f'<child widget="{base}/lobby_button_start" x="250" y="1"/>',
                      f'<child widget="{base}/lobby_button_add" x="380" y="1"/>',
                      f'<child widget="{base}/lobby_button_leave" x="510" y="1"/>'])
    for key, caption, handlers in (
        ("add", "ADD PLAYER", ['<on event="a" run="port lobby add player"/>',
                               '<on event="start" run="port lobby add player"/>']),
        ("team", "SWITCH TEAM", ['<on event="a" run="swap player team"/>', '<on event="start" run="swap player team"/>']),
        ("start", "START NOW", ['<on event="a" run="net game speed start"/>',
                                '<on event="start" run="net game speed start"/>']),
        ("leave", "LEAVE", ['<on event="a" run="mouse emit back event"/>',
                            '<on event="start" run="mouse emit back event"/>'])):
        lines += _widget(f"{base}/lobby_button_{key}", [("type", "text"), ("width", 128), ("height", 24),
                                                       ("bitmap", "bitmaps/text_button_background"),
                                                       ("text", caption), ("font", "ui\\small_ui"),
                                                       ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                         handlers + ['<on event="left_mouse" run="mouse emit accept event"/>'])
    # a split screen player's profile, chosen as they join the lobby (any
    # controller's presses: Co-op's rows; the host's countdown waits)
    lines += _widget(f"{base}/player_profile_screen", [("width", 640), ("height", 480),
                                                       ("flags", "pass_unhandled_to_focused_child"),
                                                       ("bitmap", "bitmaps/gradient")],
                     ['<on event="created" run="net server defer start"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{base}/player_profile_list"/>',
                      f'<child widget="{base}/header_add_player"/>',
                      f'<child widget="{base}/player_profile_help" x="20" y="416"/>'])
    lines += _header(f"{base}/header_add_player", f"{base}/header_add_player")
    lines += _widget(f"{base}/player_profile_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", "main_menu/profile_manager/player_profile_extended_desc")],
                     ['<data input="3wide player profile list update"/>',
                      '<on event="created" run="port lobby player list initialize"/>',
                      '<on event="deleted" run="player profile list dispose"/>',
                      '<on event="custom_activation" run="port lobby player choose" back="true" branch="true"/>',
                      *[f'<child widget="{MT}/coop/list_item_{index}" x="20" y="{73 + 30 * index}"/>'
                        for index in range(11)],
                      f'<child widget="{MT}/coop/player_2_button_bar" y="414"/>'])
    lines += _widget(f"{base}/player_profile_help", [("type", "text"), ("width", 350), ("height", 24),
                                                     ("text", "The new player's profile."),
                                                     ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("text_y", 5),
                                                     ("text_flags", "no_focus_test")], [])
    # a game under way's lobby, before joining it (the browser's rows of
    # games in progress): what its advertisement tells, JOIN GAME. Split
    # screen players join here, as in the lobby, before JOIN GAME: the game
    # starts at once for the machine, with the players it brings
    lines += _widget(f"{base}/preview_screen", [("width", 640), ("height", 480),
                                                ("flags", "pass_unhandled_to_focused_child"),
                                                ("bitmap", "bitmaps/gradient")],
                     ['<on event="created" run="port lobby open"/>',
                      '<on event="b" run="port lobby preview leave" back="true"/>',
                      '<on event="back" run="port lobby preview leave" back="true"/>',
                      f'<on event="start" run="port lobby preview add" open="{base}/player_profile_screen" branch="true"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{base}/preview_list"/>',
                      f'<child widget="{base}/header_lobby"/>',
                      f'<child widget="{base}/lobby_join_help"/>'])
    lines += _widget(f"{base}/preview_list", [("type", "column_list"), ("width", 640), ("height", 480),
                                              ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                              ("description", f"{base}/lobby_desc")],
                     ['<data input="port lobby preview update"/>',
                      f'<child widget="{base}/preview_status" x="30" y="75"/>',
                      f'<child widget="{base}/preview_button_bar" y="414"/>'])
    lines += _widget(f"{base}/preview_status", [("type", "text"), ("controller", 1), ("width", 360), ("height", 300),
                                                ("font", "ui\\large_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/preview_button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                                    ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child widget="{base}/preview_button_join" x="250" y="1"/>',
                      f'<child widget="{base}/preview_button_add" x="380" y="1"/>',
                      f'<child widget="{base}/preview_button_back" x="510" y="1"/>'])
    for key, caption, run in (("join", "JOIN GAME", "port lobby preview join"),
                              ("add", "ADD PLAYER", "port lobby add player"),
                              ("back", "BACK", "mouse emit back event")):
        lines += _widget(f"{base}/preview_button_{key}", [("type", "text"), ("width", 128), ("height", 24),
                                                         ("bitmap", "bitmaps/text_button_background"),
                                                         ("text", caption), ("font", "ui\\small_ui"),
                                                         ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                         [f'<on event="a" run="{run}"/>', f'<on event="start" run="{run}"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>'])
    return lines


def _coop() -> list:
    """Co-op, the campaign for two players on this machine in split screen
    (the Xbox's Cooperative Play): Multiplayer's CO-OP CAMPAIGN, then player
    2's profile, chosen with player 2's controller (its rows take any
    controller's presses, as the shared rows do only controller 1's), then
    New Game's levels and difficulty, which either player's controller uses
    (ui_widget.c's widget_takes_events_of_controller; menu_functions.c's
    coop_begin)"""
    base = f"{MT}/coop"
    lines = _widget(f"{MT}/multiplayer_type_coop_item",
                    [("type", "text"), ("left", 51), ("width", 232), ("height", 32), ("bitmap", "bitmaps/list_item_bkd"),
                     ("string_list", f"{MT}/multiplayer_options"), ("string_index", 7), ("font", "ui\\large_ui"),
                     ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                    [f'<on event="a" run="port coop begin" open="{base}/player_2_profile_screen" branch="true"/>',
                     f'<on event="start" run="port coop begin" open="{base}/player_2_profile_screen" branch="true"/>',
                     '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/player_2_profile_screen", [("width", 640), ("height", 480),
                                                         ("flags", "pass_unhandled_to_focused_child"),
                                                         ("bitmap", "bitmaps/gradient")],
                     ['<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{base}/player_2_profile_list"/>',
                      f'<child widget="{base}/header_player_2"/>',
                      f'<child widget="{base}/player_2_help" x="20" y="416"/>'])
    lines += _header(f"{base}/header_player_2", f"{base}/header_player_2")
    rows = [f'<child widget="{base}/list_item_{index}" x="20" y="{73 + 30 * index}"/>' for index in range(11)]
    lines += _widget(f"{base}/player_2_profile_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", "main_menu/profile_manager/player_profile_extended_desc")],
                     ['<data input="3wide player profile list update"/>',
                      '<on event="created" run="port coop player 2 list initialize"/>',
                      '<on event="deleted" run="player profile list dispose"/>',
                      '<on event="custom_activation" run="port coop player 2" '
                      'open="main_menu/solo_level_select/solo_level_select_screen" branch="true"/>',
                      *rows, f'<child widget="{base}/player_2_button_bar" y="414"/>'])
    for index in range(11):
        lines += _widget(f"{base}/list_item_{index}",
                         [("width", 390), ("height", 28),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if index == 0 else "bitmaps/sel_list_item_bkd"),
                          ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("align", "center"), ("text_y", 3)],
                         ['<on event="a" run="single prev cl item activated"/>',
                          '<on event="start" run="single prev cl item activated"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          '<child widget="main_menu/new_select/list_item_text" x="25"/>',
                          '<child widget="main_menu/new_select/list_item_arrows"/>'])
    lines += _widget(f"{base}/player_2_help", [("type", "text"), ("width", 350), ("height", 24),
                                               ("text", "Player 2: choose with your controller."),
                                               ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("text_y", 5),
                                               ("text_flags", "no_focus_test")], [])
    lines += _widget(f"{base}/player_2_button_bar", [("type", "column_list"), ("width", 640), ("height", 28),
                                                     ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<data input="common button bar update"/>',
                      '<child widget="main_menu/profile_manager/profile_manager_button_ok" x="380" y="1"/>',
                      '<child widget="common_button_back" x="510" y="1"/>'])
    return lines


def _create_local() -> list:
    """Create Game's LOCAL row: plays on this machine alone (split screen and
    bots), opening the map and gametype lists as LAN does (menu_functions.c's
    ui_widget_port_host_local)"""
    return _widget(f"{MT}/multiplayer_type_create_local_item",
                   [("type", "text"), ("left", 51), ("width", 232), ("height", 32), ("bitmap", "bitmaps/list_item_bkd"),
                    ("string_list", f"{MT}/multiplayer_options"), ("string_index", 8), ("font", "ui\\large_ui"),
                    ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                   ['<on event="a" run="join controller to mp game"/>',
                    f'<on event="a" run="mp type set mode" open="{MT}/connected/connected_map_select_wrapper"/>',
                    '<on event="left_mouse" run="mouse emit accept event"/>'])


def _item_options_extras() -> list:
    """Item Options' rows of the port's: the map's weapons (YES or NO), and
    the loadout, CATEGORY (the weapon set) or CUSTOM (each player's primary
    and secondary weapons)"""
    base = "main_menu/settings_select/multiplayer_setup/item_options_edit"
    lines = []
    for key, strings, label in (("map_weapons", "var_map_weapons", 8), ("loadout", "var_loadout", 5),
                                ("primary_weapon", "var_loadout_weapon", 6),
                                ("secondary_weapon", "var_loadout_weapon", 7)):
        lines += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
                                               ("flags", "pass_unhandled_to_focused_child"),
                                               ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<child widget="{base}/{key}_label"/>',
                          f'<child widget="{base}/{key}_spinner" x="286" y="1"/>'])
        lines += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1), ("width", 300), ("height", 22),
                                                   ("string_list", f"{base}/item_options_labels"),
                                                   ("string_index", label), ("font", "ui\\large_ui"),
                                                   ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{base}/{key}_spinner",
                         [("type", "spinner"), ("top", 2), ("width", 206), ("height", 20),
                          ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("string_list", f"{base}/{strings}"), ("font", "ui\\large_ui"), ("color", "#FF2896FF"),
                          ("align", "center"), ("text_y", 4), ("list_flags", "items_from_strings"),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -13 19 -7"), ("footer_bounds", "7 208 19 214")],
                         ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    lines += _strings(f"{base}/var_map_weapons", ["YES", "NO"])
    lines += _strings(f"{base}/var_loadout", ["CATEGORY", "CUSTOM"])
    lines += _strings(f"{base}/var_loadout_weapon", LOADOUT_WEAPONS)
    return lines


def _player_options_extras() -> list:
    """The host's aim-assist policy and shield effects, hidden while editing
    saved gametypes."""
    base = PLAYER_EDIT
    lines = []
    for key, label_index, words in (("aim_assist", 8, ["ALLOWED", "BLOCK M+KB"]),
                                    ("shield_effects", 9, SHIELD_EFFECTS)):
        lines += _widget(f"{base}/op_{key}", [("width", 512), ("height", 28),
            ("flags", "pass_unhandled_to_focused_child"), ("bitmap", "bitmaps/option_bkds"),
            ("color", "#FF2896FF")], [f'<child widget="{base}/{key}_label"/>',
            f'<child widget="{base}/{key}_spinner" x="286" y="1"/>'])
        lines += _widget(f"{base}/{key}_label", [("type", "text"), ("controller", 1),
            ("width", 280), ("height", 22), ("string_list", f"{base}/player_options_labels"),
            ("string_index", label_index), ("font", "ui\\large_ui"), ("color", "#FF2896FF"),
            ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{base}/{key}_spinner", [("type", "spinner"), ("top", 2),
            ("width", 206), ("height", 20), ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
            ("string_list", f"{base}/var_{key}"), ("font", "ui\\large_ui"),
            ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4),
            ("list_flags", "items_from_strings"), ("header_bitmap", "bitmaps/arrow_sm_left"),
            ("footer_bitmap", "bitmaps/arrow_sm_right"), ("header_bounds", "7 -13 19 -7"),
            ("footer_bounds", "7 208 19 214")], ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    return lines + _strings(f"{base}/var_aim_assist", ["ALLOWED", "BLOCK M+KB"]) + \
        _strings(f"{base}/var_shield_effects", SHIELD_EFFECTS)


def _opence_options() -> list:
    """the gametype editor's OPENCE OPTIONS: its row, after the editor's own
    (as they are laid out), and its screen, as the gametype's options' are
    (_setup_option_screen), its spinners the gametype's (menu_functions.c's
    gametype_options), its header its words (no picture of its own)"""
    lines = _setup_option_screen(OPENCE_EDIT, "opence", OPENCE_ROWS)
    lines = [line.replace("port setup options init", "mp profile init opence options")
             .replace("port setup options save", "mp profile set opence options")
             .replace(f'bitmap="{OPENCE_EDIT}/header_opence"',
                      'type="text" text="OPENCE OPTIONS" font="ui\\large_ui" color="#FFFFFFFF" text_x="13" text_y="16"')
             for line in lines]
    lines += _widget(OPENCE_ITEM, [("type", "text"), ("left", 51), ("top", 309), ("width", 232), ("height", 32),
                                   ("bitmap", "bitmaps/list_item_bkd"), ("text", "OPENCE OPTIONS"),
                                   ("font", "ui\\large_ui"), ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                     [f'<on event="a" open="{OPENCE_EDIT}/opence_screen"/>',
                      f'<on event="start" open="{OPENCE_EDIT}/opence_screen"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    return lines


def _teamplay_options_extras() -> list:
    """Teamplay Options' rows of the port's: the host's voice chat
    (port/linux/game/network_voice.c) and vote kicks (network_votekick.c),
    in Server Setup's copy only (menu_functions.c's gametype_options_init),
    kept in config.toml"""
    lines = []
    for index, (key, label, strings, _) in enumerate(TEAMPLAY_ROWS):
        lines += _widget(f"{TEAMPLAY_EDIT}/op_{key}", [("width", 512), ("height", 28),
                                                       ("flags", "pass_unhandled_to_focused_child"),
                                                       ("bitmap", "bitmaps/option_bkds"), ("color", "#FF2896FF")],
                         [f'<child widget="{TEAMPLAY_EDIT}/{key}_label"/>',
                          f'<child widget="{TEAMPLAY_EDIT}/{key}_spinner" x="286" y="1"/>'])
        lines += _widget(f"{TEAMPLAY_EDIT}/{key}_label", [("type", "text"), ("controller", 1), ("width", 300),
                                                          ("height", 22),
                                                          ("string_list", f"{TEAMPLAY_EDIT}/teamplay_options_labels"),
                                                          ("string_index", 3 + index), ("font", "ui\\large_ui"),
                                                          ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 4)], [])
        lines += _widget(f"{TEAMPLAY_EDIT}/{key}_spinner",
                         [("type", "spinner"), ("top", 2), ("width", 206), ("height", 20),
                          ("flags", "pass_unhandled_to_focused_child left_right_tabs_items"),
                          ("string_list", f"{TEAMPLAY_EDIT}/var_{key}"), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4),
                          ("list_flags", "items_from_strings"),
                          ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                          ("header_bounds", "7 -13 19 -7"), ("footer_bounds", "7 208 19 214")],
                         ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
        lines += _strings(f"{TEAMPLAY_EDIT}/var_{key}", strings)
    return lines


def _map_kind() -> list:
    """the map lists' first row (New Game's and the Map screen's), as the
    gametype list's chooser: a spinner of SINGLEPLAYER or MULTIPLAYER maps,
    or the Custom Edition maps of the custom_maps folder of either kind
    (menu_functions.c's map lists), for either controller (split screen
    co-op's New Game takes both)"""
    lines = _widget(MAP_KIND_CHOOSER, [("width", 256), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                                       ("bitmap", "bitmaps/option_bkds_sm"), ("font", "ui\\large_ui"),
                                       ("color", "#FF2896FF"), ("align", "center"), ("text_y", 3)],
                    [f'<child widget="{MAP_KIND_CHOOSER}_spinner" x="9" y="2"/>'])
    # (wider than the gametype chooser's, for CUSTOM SINGLEPLAYER, its
    # arrows at the row's ends)
    lines += _widget(f"{MAP_KIND_CHOOSER}_spinner",
                     [("type", "spinner"), ("width", 238), ("height", 22), ("flags", "left_right_tabs_items"),
                      ("string_list", "main_menu/new_select/var_map_kinds"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1), ("list_flags", "items_from_strings"),
                      ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                      ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 238 19 244")],
                     ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    lines += _strings("main_menu/new_select/var_map_kinds",
                      ["SINGLEPLAYER", "MULTIPLAYER", "CUSTOM SINGLEPLAYER", "CUSTOM MULTIPLAYER", "FORGE MAPS"])
    return lines


def multiplayer_files() -> dict:
    """the port's multiplayer widgets: the browser's additions, the server
    settings, the lobby"""
    head = ['<?xml version="1.0" encoding="UTF-8"?>',
            "<!-- The port's multiplayer screens, in the PC version's style (tools/port_settings.py) -->", "<menus>"]
    return {
        f"{MT}/join_game".replace("/", ".") + ".port.xml": head + _join_game_extras() + ["</menus>", ""],
        f"{MT}/server_settings".replace("/", ".") + ".xml": head + _server_settings() + ["</menus>", ""],
        f"{MT}/lobby".replace("/", ".") + ".xml": head + _lobby() + ["</menus>", ""],
        f"{MT}/coop".replace("/", ".") + ".xml": head + _coop() + ["</menus>", ""],
        f"{MT}/local".replace("/", ".") + ".xml": head + _create_local() + ["</menus>", ""],
        "main_menu/new_select".replace("/", ".") + ".port.xml": head + _map_kind() + ["</menus>", ""],
        "main_menu/settings_select/multiplayer_setup/item_options_edit".replace("/", ".") + ".port.xml": head + _item_options_extras() + ["</menus>", ""],
        PLAYER_EDIT.replace("/", ".") + ".port.xml": head + _player_options_extras() + ["</menus>", ""],
        TEAMPLAY_EDIT.replace("/", ".") + ".port.xml": head + _teamplay_options_extras() + ["</menus>", ""],
        OPENCE_EDIT.replace("/", ".") + ".port.xml": head + _opence_options() + ["</menus>", ""],
        MODS.replace("/", ".") + ".xml": [head[0], "<!-- The port's Mods screen, in the PC version's style "
                                          "(tools/port_settings.py) -->", head[2]] + _mods() + ["</menus>", ""],
    }


REPLACED_FOLDERS = [f"{MT}/server_settings"]


# ---------- the mods (port/linux/game/mods.c, docs/mods.md): the main menu's
# MODS, which opens the list of the mods folder's mods, in their load order,
# each turned on or off and moved up or down (menu_functions.c's "port mods"
# functions)

# the redraws' main menu items (svg/shell/main_menu/menu_*.svg): their
# letters' top and scale, and where their middle is
MENU_ITEM_TOP, MENU_ITEM_SCALE, MENU_ITEM_MIDDLE = 6.57, 0.9037, 130.0
# the space between two of the redraws' letters, in their units (their
# capitals are 21.5 high), for the pairs the port's items have: no redraw has
# these pairs, so each is the sum of a space after the first letter and one
# before the second, fitted (least squares) to the 53 pairs the redraws' items
# and headers have (D-I, of the main menu's Credits, gives D's right side)
MENU_ITEM_SPACES = {"MO": 3.17, "OD": 2.8, "DS": 2.3}
# the mods' status icons (port/assets/icons/lucide): a mod out of the order it
# asks for, and one that cannot be played (another mod says it cannot be
# played with it, or this build cannot play it): menu_functions.c's
# mods_update shows frame 1 or 2
MOD_STATUS_ICONS = [("triangle-alert", "#FFB020"), ("circle-x", "#FF4040")]


def redraw_letters(redraws: Path) -> dict:
    """the redraws' letters (their <path id="gA">s, one drawing of each
    letter in every item and header), by letter"""
    letters = {}
    for svg in sorted(redraws.rglob("*.svg")):
        if any(part.startswith("_") for part in svg.relative_to(redraws).parts):
            continue
        for letter, path in re.findall(r'<path id="g([A-Z])" d="([^"]+)"', svg.read_text()):
            if letters.setdefault(letter, path) != path:
                sys.exit(f"{svg}: the redraws draw {letter} two ways")
    return letters


def menu_item_frames(text: str, redraws: Path) -> list:
    """the frames (svg, width, height) of a main menu item of the port's: its
    text in the redraws' own letters (redraw_letters), spaced, placed and
    coloured as their items are, dim, then white with their blue glow"""
    from fontTools.pens.boundsPen import BoundsPen
    from fontTools.svgLib.path import parse_path

    letters = redraw_letters(redraws)
    widths = []
    for letter in text:
        pen = BoundsPen(None)
        parse_path(letters[letter], pen)
        widths.append(pen.bounds[2])
    spaces = [MENU_ITEM_SPACES[first + second] for first, second in zip(text, text[1:])]
    x = MENU_ITEM_MIDDLE - (sum(widths) + sum(spaces)) * MENU_ITEM_SCALE / 2
    uses = []
    for index, letter in enumerate(text):
        uses.append(f'<use href="#g{letter}" transform="translate({x:.2f},{MENU_ITEM_TOP}) scale({MENU_ITEM_SCALE})"/>')
        x += (widths[index] + (spaces[index] if index < len(spaces) else 0)) * MENU_ITEM_SCALE
    paths = [f'    <path id="g{letter}" d="{letters[letter]}"/>' for letter in sorted(set(text))]
    head = ['<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="256" '
            'height="64" viewBox="0 0 256 64">', "  <defs>", *paths]
    dim = [*head, "  </defs>", f'  <!-- "{text}" (normal state: dim blue) -->',
           '  <g id="text" fill="#2895ff" fill-opacity="0.467">', *[f"    {use}" for use in uses], "  </g>", "</svg>", ""]
    glow = [*head, '    <filter id="glow" filterUnits="userSpaceOnUse" x="0" y="0" width="256" height="64">',
            '      <feGaussianBlur stdDeviation="2.19"/>', "    </filter>", "  </defs>",
            f'  <!-- "{text}" (highlighted: white with blue glow) -->',
            '  <g fill="#2895ff" stroke="#2895ff" stroke-width="6.3" stroke-linejoin="round" opacity="0.650" '
            'filter="url(#glow)">', *[f"    {use}" for use in uses], "  </g>",
            '  <g id="text" fill="#fff">', *[f"    {use}" for use in uses], "  </g>", "</svg>", ""]
    return [("\n".join(dim), 256, 64), ("\n".join(glow), 256, 64)]


def mod_status_frames(icons: Path, size: int = 24) -> list:
    """the frames of the mods' status icon, `size` pixels square (the menus
    draw a bitmap at its size): none, out of order, incompatible"""
    blank = f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 24 24"/>\n'
    return [(blank, size, size)] + [
        ((icons / f"{icon}.svg").read_text().replace('stroke="currentColor"', f'stroke="{color}"')
         .replace('width="24"', f'width="{size}"').replace('height="24"', f'height="{size}"'), size, size)
        for icon, color in MOD_STATUS_ICONS]


def _text_button(name: str, text: str, handlers: list) -> list:
    """a button of the button bar with its own words"""
    pairs = [("type", "text"), ("width", 128), ("height", 24), ("bitmap", "bitmaps/text_button_background"),
             ("text", text), ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)]
    return _widget(name, pairs, handlers + ['<on event="left_mouse" run="mouse emit accept event"/>'])


def _mods() -> list:
    """the main menu's MODS and its screen, laid out as the lobby is: on the
    left two thirds the mods as a resource pack list has them (the one at the
    top playing over those below it: the load order upside down), a row each
    with its status icon, name and ON or OFF; on the right third, in the
    lobby's frame, the mod chosen: its name, what is wrong with it, whether it
    is on and its priority, and what its mod.toml says. A turns the mod chosen
    on or off, left and right (and MOVE UP and MOVE DOWN, for the mouse) move
    it; OK keeps it all (load_order.toml) and plays them, Cancel forgets it
    (menu_functions.c)"""
    base = MODS
    lines = _widget("main_menu/main_menu_item_mods",
                    [("type", "text"), ("width", 256), ("height", 33), ("bitmap", "main_menu/menu_mods")],
                    [f'<on event="a" open="{base}/mods_screen"/>', f'<on event="start" open="{base}/mods_screen"/>',
                     '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/mods_screen", [("width", 640), ("height", 480),
                                             ("flags", "pass_unhandled_to_focused_child pause_game"),
                                             ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>', '<on event="back" back="true"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{base}/mods_list"/>',
                      f'<child widget="{base}/header_mods"/>'])
    lines += _header(f"{base}/header_mods", f"{base}/header_mods")
    # (the lobby's rows, MOD_ROWS of them, scrolling over more mods)
    rows = [f'<child widget="{base}/op_mod_{index + 1}" x="20" y="{73 + 30 * index}"/>' for index in range(MOD_ROWS)]
    lines += _widget(f"{base}/mods_list", [("type", "column_list"), ("width", 640), ("height", 480),
                                           ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                                           ("description", f"{base}/mod_details")],
                     ['<data input="port mods update"/>', '<on event="created" run="port mods init"/>',
                      '<on event="deleted" run="port mods close"/>', *rows,
                      f'<child widget="{base}/button_bar" y="414"/>'])
    for index in range(MOD_ROWS):
        lines += _widget(f"{base}/op_mod_{index + 1}",
                         [("width", 390), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if index == 0 else "bitmaps/sel_list_item_bkd"),
                          ("color", "#FF2896FF")],
                         ['<on event="a" run="port mods toggle"/>', '<on event="start" run="port mods toggle"/>',
                          '<on event="left" run="port mods move up"/>', '<on event="right" run="port mods move down"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/mod_status" x="8" y="2"/>',
                          f'<child widget="{base}/mod_name" x="34"/>',
                          f'<child widget="{base}/mod_state" x="318"/>'])
    lines += _widget(f"{base}/mod_status", [("controller", 1), ("width", 24), ("height", 24),
                                            ("bitmap", "mods/status")], [])
    lines += _widget(f"{base}/mod_name",
                     [("type", "text"), ("controller", 1), ("width", 284), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_y", 4)], [])
    lines += _widget(f"{base}/mod_state",
                     [("type", "text"), ("controller", 1), ("width", 66), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 4), ("text_flags", "no_focus_test")], [])
    # (in the lobby's frame, sel_list_desc_bkd's: the mod chosen's name, the
    # icon of what is wrong with it, and the rest)
    lines += _widget(f"{base}/mod_details", [("width", 640), ("height", 480)],
                     [f'<child widget="{base}/mod_details_name"/>', f'<child widget="{base}/mod_details_status"/>',
                      f'<child widget="{base}/mod_details_text"/>'])
    lines += _widget(f"{base}/mod_details_name",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 84), ("width", 200), ("height", 44),
                      ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    lines += _widget(f"{base}/mod_details_status", [("controller", 1), ("left", 411), ("top", 132), ("width", 16),
                                                    ("height", 16), ("bitmap", "mods/status_small")], [])
    lines += _widget(f"{base}/mod_details_text",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 130), ("width", 200),
                      ("height", 272), ("font", "ui\\small_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     [f'<child widget="{base}/button_up" y="1"/>', f'<child widget="{base}/button_down" x="130" y="1"/>',
                      f'<child widget="{base}/button_ok" x="380" y="1"/>',
                      '<child widget="common_button_cancel" x="510" y="1"/>'])
    lines += _text_button(f"{base}/button_up", "MOVE UP", ['<on event="a" run="port mods move up"/>',
                                                          '<on event="start" run="port mods move up"/>'])
    lines += _text_button(f"{base}/button_down", "MOVE DOWN", ['<on event="a" run="port mods move down"/>',
                                                              '<on event="start" run="port mods move down"/>'])
    lines += _button(f"{base}/button_ok", 1, ['<on event="a" run="port mods save" back="true"/>',
                                              '<on event="start" run="port mods save" back="true"/>'])
    return lines


def firefight_emblem_svg(for_card: bool = False) -> str:
    """Halo CE-styled Firefight emblem SVG (based on the Halo 3: ODST / Halo: Reach
    Firefight crest: crossed assault rifles behind a geometric shield crest &
    stylized visor/skull with 5 wave stars above), rendered in the exact same
    glowing cyan/blue wireframe & scanline style (#2896ff stroke, #0a2649 fill
    glow, soft outer Gaussian blur, and black silhouette core) as the original
    Halo CE gametype icons (rules_options and game_type_grafix)."""
    if for_card:
        width, height = 256, 128
        transform = "translate(70, 58) scale(0.56)"
    else:
        width, height = 256, 256
        transform = "translate(128, 96) scale(1.0)"
    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        "  <defs>",
        '    <filter id="ff_glow" filterUnits="userSpaceOnUse" x="-64" y="-64" width="384" height="384">',
        '      <feGaussianBlur stdDeviation="5.5"/>',
        "    </filter>",
        '    <linearGradient id="ff_card_bg" gradientUnits="userSpaceOnUse" x1="0" y1="0" x2="0" y2="114">',
        '      <stop offset="0" stop-color="#061728"/>',
        '      <stop offset="0.55" stop-color="#0a2843"/>',
        '      <stop offset="1" stop-color="#154d85"/>',
        "    </linearGradient>",
        '    <linearGradient id="ff_shield_fill" gradientUnits="userSpaceOnUse" x1="0" y1="-68" x2="0" y2="72">',
        '      <stop offset="0" stop-color="#0a2649"/>',
        '      <stop offset="1" stop-color="#050d1c"/>',
        "    </linearGradient>",
        "  </defs>",
    ]
    if for_card:
        lines += [
            '  <rect x="0" y="0" width="140" height="114" fill="url(#ff_card_bg)"/>',
            '  <rect x="0" y="0" width="140" height="1.5" fill="#2896ff" fill-opacity="0.85"/>',
            '  <rect x="0" y="112.5" width="140" height="1.5" fill="#2896ff" fill-opacity="0.85"/>',
        ]
    emblem_body = [
        f'  <g transform="{transform}">',
        "    <!-- Outer cyan glow aura (#2896ff) -->",
        '    <g filter="url(#ff_glow)" opacity="0.72" fill="#2896ff" stroke="#2896ff" stroke-width="4" stroke-linejoin="round">',
        '      <g transform="rotate(-42)"><rect x="-82" y="-5.5" width="164" height="11" rx="2"/></g>',
        '      <g transform="rotate(42)"><rect x="-82" y="-5.5" width="164" height="11" rx="2"/></g>',
        '      <polygon points="0,-60 46,-36 38,24 0,68 -38,24 -46,-36"/>',
        "    </g>",
        "    <!-- Crossed Rifles (black core + #0a2649 glow + #2896ff wireframe) -->",
        '    <g transform="rotate(-42)">',
        '      <path d="M-84,-6 L-54,-9 L-48,-5 L46,-5 L64,-8 L82,-3 L82,4 L56,5 L46,9 L18,9 L12,5 L-46,5 L-56,11 L-84,8 Z" fill="#000000" stroke="#2896ff" stroke-width="2" stroke-linejoin="round"/>',
        '      <line x1="-44" y1="-1" x2="54" y2="-1" stroke="#2896ff" stroke-width="1.2" stroke-opacity="0.8"/>',
        "    </g>",
        '    <g transform="rotate(42)">',
        '      <path d="M-84,-6 L-54,-9 L-48,-5 L46,-5 L64,-8 L82,-3 L82,4 L56,5 L46,9 L18,9 L12,5 L-46,5 L-56,11 L-84,8 Z" fill="#000000" stroke="#2896ff" stroke-width="2" stroke-linejoin="round"/>',
        '      <line x1="-44" y1="-1" x2="54" y2="-1" stroke="#2896ff" stroke-width="1.2" stroke-opacity="0.8"/>',
        "    </g>",
        "    <!-- Outer Shield Crest -->",
        '    <polygon points="0,-60 46,-36 38,24 0,68 -38,24 -46,-36" fill="#000000" stroke="#2896ff" stroke-width="2.4" stroke-linejoin="round"/>',
        '    <polygon points="0,-53 39,-32 32,20 0,58 -32,20 -39,-32" fill="url(#ff_shield_fill)" stroke="#2896ff" stroke-width="1.4" stroke-linejoin="round"/>',
        "    <!-- Horizontal Scanlines inside Shield -->",
        '    <g stroke="#2896ff" stroke-width="0.9" stroke-opacity="0.32">',
        '      <line x1="-34" y1="-28" x2="34" y2="-28"/>',
        '      <line x1="-35" y1="-20" x2="35" y2="-20"/>',
        '      <line x1="-34" y1="-12" x2="34" y2="-12"/>',
        '      <line x1="-33" y1="-4" x2="33" y2="-4"/>',
        '      <line x1="-31" y1="4" x2="31" y2="4"/>',
        '      <line x1="-29" y1="12" x2="29" y2="12"/>',
        '      <line x1="-25" y1="20" x2="25" y2="20"/>',
        "    </g>",
        "    <!-- Inner ODST / Spartan Helmet & Skull Visor -->",
        '    <polygon points="0,-42 26,-27 28,2 17,22 13,38 -13,38 -17,22 -28,2 -26,-27" fill="#000000" stroke="#2896ff" stroke-width="2" stroke-linejoin="round"/>',
        '    <polygon points="0,-16 22,-19 18,-3 4,-2 0,-7 -4,-2 -18,-3 -22,-19" fill="#0a2649" stroke="#2896ff" stroke-width="1.8" stroke-linejoin="round"/>',
        '    <polygon points="0,0 5,11 -5,11" fill="#0a2649" stroke="#2896ff" stroke-width="1.4" stroke-linejoin="round"/>',
        "    <!-- Skull Jaw / Visor Grill -->",
        '    <line x1="-8" y1="18" x2="-8" y2="35" stroke="#2896ff" stroke-width="1.6"/>',
        '    <line x1="-2.7" y1="18" x2="-2.7" y2="36" stroke="#2896ff" stroke-width="1.6"/>',
        '    <line x1="2.7" y1="18" x2="2.7" y2="36" stroke="#2896ff" stroke-width="1.6"/>',
        '    <line x1="8" y1="18" x2="8" y2="35" stroke="#2896ff" stroke-width="1.6"/>',
        "    <!-- 5 Wave Stars Arc Above Crest -->",
        '    <g fill="#0a2649" stroke="#2896ff" stroke-width="1.3" stroke-linejoin="round">',
        '      <polygon transform="translate(-38,-62) scale(0.62)" points="0,-9 2.6,-2.8 9.2,-2.2 4.1,2.2 5.7,8.8 0,5.2 -5.7,8.8 -4.1,2.2 -9.2,-2.2 -2.6,-2.8"/>',
        '      <polygon transform="translate(-19,-70) scale(0.68)" points="0,-9 2.6,-2.8 9.2,-2.2 4.1,2.2 5.7,8.8 0,5.2 -5.7,8.8 -4.1,2.2 -9.2,-2.2 -2.6,-2.8"/>',
        '      <polygon transform="translate(0,-74) scale(0.78)" points="0,-9 2.6,-2.8 9.2,-2.2 4.1,2.2 5.7,8.8 0,5.2 -5.7,8.8 -4.1,2.2 -9.2,-2.2 -2.6,-2.8"/>',
        '      <polygon transform="translate(19,-70) scale(0.68)" points="0,-9 2.6,-2.8 9.2,-2.2 4.1,2.2 5.7,8.8 0,5.2 -5.7,8.8 -4.1,2.2 -9.2,-2.2 -2.6,-2.8"/>',
        '      <polygon transform="translate(38,-62) scale(0.62)" points="0,-9 2.6,-2.8 9.2,-2.2 4.1,2.2 5.7,8.8 0,5.2 -5.7,8.8 -4.1,2.2 -9.2,-2.2 -2.6,-2.8"/>',
        "    </g>",
        "  </g>",
        "</svg>",
        "",
    ]
    return "\n".join(lines + emblem_body)


def _gamemode_screen() -> list:
    """GAMEMODE selection screen opened when clicking Multiplayer on the Main Menu:
    Standard, Ranked, Firefight, Forge, Theater, Creation Hub, Dedicated Server."""
    base = f"{MT}/gamemode"
    lines = _header(f"{MT}/header_standard", f"{MT}/header_standard")
    lines += _header(f"{MT}/header_ranked", f"{MT}/header_ranked")
    lines += _header(f"{MT}/header_firefight", f"{MT}/header_firefight")
    lines += _header(f"{MT}/header_forge", f"{MT}/header_forge")
    lines += _header(f"{MT}/header_theater", f"{MT}/header_theater")
    lines += _header(f"{MT}/header_creation_hub", f"{MT}/header_creation_hub")
    lines += _header(f"{MT}/header_dedi_server", f"{MT}/header_dedi_server")
    lines += _header(f"{base}/header_gamemode", f"{base}/header_gamemode")
    lines += _widget(f"{base}/gamemode_options_pic",
                     [("controller", 1), ("left", 307), ("top", 90), ("width", 256), ("height", 250),
                      ("bitmap", f"{base}/gamemode_options")], [])
    lines += _widget(f"{base}/gamemode_options_txt",
                     [("type", "text"), ("controller", 1), ("left", 290), ("top", 285), ("width", 290), ("height", 105),
                      ("font", "ui\\large_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/gamemode_ext_desc",
                     [("controller", 1), ("left", 275), ("top", 78), ("width", 320), ("height", 322),
                      ("bitmap", "bitmaps/list_field"), ("color", "#FF0080FF")],
                     [f'<child widget="{base}/gamemode_options_pic"/>',
                      f'<child widget="{base}/gamemode_options_txt"/>',
                      '<child widget="main_menu/current_profile_name"/>'])
    lines += _widget(f"{base}/gm_standard_item",
                     [("type", "text"), ("left", 51), ("width", 232), ("height", 32),
                      ("bitmap", "bitmaps/list_item_bkd_top"), ("text", "STANDARD"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                     [f'<on event="a" run="gamemode select standard" open="{MT}/multiplayer_type_select_screen"/>',
                      f'<on event="start" run="gamemode select standard" open="{MT}/multiplayer_type_select_screen"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/gm_ranked_item",
                     [("type", "text"), ("left", 51), ("width", 232), ("height", 32),
                      ("bitmap", "bitmaps/list_item_bkd"), ("text", "RANKED"), ("font", "ui\\large_ui"),
                      ("color", "#FF556677"), ("text_x", 13), ("text_y", 5)],
                     [f'<on event="a" run="gamemode select ranked" open="{MT}/multiplayer_type_select_screen"/>',
                      f'<on event="start" run="gamemode select ranked" open="{MT}/multiplayer_type_select_screen"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/gm_firefight_item",
                     [("type", "text"), ("left", 51), ("width", 232), ("height", 32),
                      ("bitmap", "bitmaps/list_item_bkd"), ("text", "FIREFIGHT"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                     [f'<on event="a" run="gamemode select firefight" open="{MT}/multiplayer_type_select_screen"/>',
                      f'<on event="start" run="gamemode select firefight" open="{MT}/multiplayer_type_select_screen"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/gm_forge_item",
                     [("type", "text"), ("left", 51), ("width", 232), ("height", 32),
                      ("bitmap", "bitmaps/list_item_bkd"), ("text", "FORGE"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                     ['<on event="a" run="gamemode select forge" open="main_menu/forge_select/forge_menu"/>',
                      '<on event="start" run="gamemode select forge" open="main_menu/forge_select/forge_menu"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/gm_theater_item",
                     [("type", "text"), ("left", 51), ("width", 232), ("height", 32),
                      ("bitmap", "bitmaps/list_item_bkd"), ("text", "THEATER"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                     [f'<on event="a" run="gamemode select theater" open="{MT}/theater/theater_screen"/>',
                      f'<on event="start" run="gamemode select theater" open="{MT}/theater/theater_screen"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/gm_creation_hub_item",
                     [("type", "text"), ("left", 51), ("width", 232), ("height", 32),
                      ("bitmap", "bitmaps/list_item_bkd"), ("text", "CREATION HUB"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                     [f'<on event="a" run="gamemode select creation hub" open="{MT}/creation_hub/creation_hub_screen"/>',
                      f'<on event="start" run="gamemode select creation hub" open="{MT}/creation_hub/creation_hub_screen"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/gm_dedi_server_item",
                     [("type", "text"), ("left", 51), ("width", 232), ("height", 32),
                      ("bitmap", "bitmaps/list_item_bkd"), ("text", "DEDICATED SERVER"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                     [f'<on event="a" run="gamemode select dedi server" open="{MT}/dedi_server/dedi_server_screen"/>',
                      f'<on event="start" run="gamemode select dedi server" open="{MT}/dedi_server/dedi_server_screen"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/gamemode_button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<data input="common button bar update"/>',
                      f'<child widget="{MT}/multiplayer_button_service_record" x="2" y="1"/>',
                      '<child widget="common_button_back" x="510" y="1"/>'])
    lines += _widget(f"{base}/gamemode_select_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_items"),
                      ("description", f"{base}/gamemode_ext_desc")],
                     ['<data input="gamemode menu update"/>',
                      '<on event="created" run="gamemode menu init"/>',
                      f'<child widget="{base}/gm_standard_item" y="78"/>',
                      f'<child widget="{base}/gm_ranked_item" y="111"/>',
                      f'<child widget="{base}/gm_firefight_item" y="144"/>',
                      f'<child widget="{base}/gm_forge_item" y="177"/>',
                      f'<child widget="{base}/gm_theater_item" y="210"/>',
                      f'<child widget="{base}/gm_creation_hub_item" y="243"/>',
                      f'<child widget="{base}/gm_dedi_server_item" y="276"/>',
                      f'<child widget="{base}/gamemode_button_bar" y="414"/>'])
    lines += _widget(f"{base}/gamemode_select_screen",
                     [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                      ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>',
                      '<on event="back" back="true"/>',
                      f'<child widget="{base}/header_gamemode"/>',
                      f'<child widget="{base}/gamemode_select_list"/>',
                      '<child widget="main_menu/blueline" x="64" y="349"/>'])
    return lines


def _theater_screen() -> list:
    """Halo 1 CE Theater Mode screen (main_menu/multiplayer_type_select/theater/theater_screen)."""
    base = f"{MT}/theater"
    lines = []
    for row in range(1, 12):
        lines += _widget(f"{base}/op_theater_{row}",
                         [("width", 390), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if row == 1 else "bitmaps/sel_list_item_bkd"),
                          ("color", "#FF2896FF")],
                         ['<on event="a" run="port theater watch"/>',
                          '<on event="start" run="port theater watch"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/theater_name" x="10"/>',
                          f'<child widget="{base}/theater_date" x="280"/>'])
    lines += _widget(f"{base}/theater_name",
                     [("type", "text"), ("controller", 1), ("width", 266), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_y", 4)], [])
    lines += _widget(f"{base}/theater_date",
                     [("type", "text"), ("controller", 1), ("width", 100), ("height", 22), ("font", "ui\\small_ui"),
                      ("color", "#FF70B7FF"), ("align", "right"), ("text_y", 6)], [])
    lines += _widget(f"{base}/theater_details_name",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 84), ("width", 200), ("height", 44),
                      ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    lines += _widget(f"{base}/theater_details_text",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 130), ("width", 200), ("height", 272),
                      ("font", "ui\\small_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/theater_details",
                     [("width", 640), ("height", 480)],
                     [f'<child widget="{base}/theater_details_name"/>',
                      f'<child widget="{base}/theater_details_text"/>'])
    lines += _widget(f"{base}/theater_button_watch",
                     [("type", "text"), ("width", 128), ("height", 24),
                      ("bitmap", "bitmaps/text_button_background"), ("text", "WATCH FILM"),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="port theater watch"/>',
                      '<on event="start" run="port theater watch"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/theater_button_record",
                     [("type", "text"), ("width", 128), ("height", 24),
                      ("bitmap", "bitmaps/text_button_background"), ("text", "REC: ON"),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="port theater record"/>',
                      '<on event="start" run="port theater record"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/theater_button_delete",
                     [("type", "text"), ("width", 128), ("height", 24),
                      ("bitmap", "bitmaps/text_button_background"), ("text", "DELETE"),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="port theater delete"/>',
                      '<on event="start" run="port theater delete"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/theater_button_export",
                     [("type", "text"), ("width", 128), ("height", 24),
                      ("bitmap", "bitmaps/text_button_background"), ("text", "COPY PATH"),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="port theater export"/>',
                      '<on event="start" run="port theater export"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/theater_button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<data input="common button bar update"/>',
                      f'<child widget="{base}/theater_button_watch" x="2" y="1"/>',
                      f'<child widget="{base}/theater_button_record" x="129" y="1"/>',
                      f'<child widget="{base}/theater_button_delete" x="256" y="1"/>',
                      f'<child widget="{base}/theater_button_export" x="383" y="1"/>',
                      '<child widget="common_button_back" x="510" y="1"/>'])
    lines += _widget(f"{base}/theater_select_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", f"{base}/theater_details")],
                     ['<data input="port theater update"/>',
                      '<on event="created" run="port theater init"/>',
                      *[f'<child widget="{base}/op_theater_{row}" x="20" y="{73 + 30 * (row - 1)}"/>'
                        for row in range(1, 12)],
                      f'<child widget="{base}/theater_button_bar" y="414"/>'])
    lines += _widget(f"{base}/theater_screen",
                     [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                      ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>',
                      '<on event="back" back="true"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{MT}/header_theater"/>',
                      f'<child widget="{base}/theater_select_list"/>'])
    return lines


def _creation_hub_screen() -> list:
    """Creation Hub screen (main_menu/multiplayer_type_select/creation_hub/creation_hub_screen)."""
    base = f"{MT}/creation_hub"
    lines = []
    for row in range(1, 12):
        lines += _widget(f"{base}/op_tag_{row}",
                         [("width", 390), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if row == 1 else "bitmaps/sel_list_item_bkd"),
                          ("color", "#FF2896FF")],
                         ['<on event="a" run="port tags choose"/>',
                          '<on event="start" run="port tags choose"/>',
                          '<on event="left" run="port tags left"/>',
                          '<on event="right" run="port tags right"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/tag_name" x="10"/>',
                          f'<child widget="{base}/tag_state" x="280"/>'])
    lines += _widget(f"{base}/tag_name",
                     [("type", "text"), ("controller", 1), ("width", 266), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_y", 4)], [])
    lines += _widget(f"{base}/tag_state",
                     [("type", "text"), ("controller", 1), ("width", 100), ("height", 22), ("font", "ui\\small_ui"),
                      ("color", "#FF70B7FF"), ("align", "right"), ("text_y", 6)], [])
    lines += _widget(f"{base}/tag_details_name",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 84), ("width", 200), ("height", 44),
                      ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    lines += _widget(f"{base}/tag_preview",
                     [("controller", 1), ("left", 440), ("top", 280), ("width", 144), ("height", 102)], [])
    lines += _widget(f"{base}/tag_details_text",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 130), ("width", 200), ("height", 272),
                      ("font", "ui\\small_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/tag_details",
                     [("width", 640), ("height", 480)],
                     [f'<child widget="{base}/tag_details_name"/>',
                      f'<child widget="{base}/tag_preview"/>',
                      f'<child widget="{base}/tag_details_text"/>'])
    lines += _widget(f"{base}/tags_button_new",
                     [("type", "text"), ("width", 128), ("height", 24),
                      ("bitmap", "bitmaps/text_button_background"), ("text", "FIRING RANGE"),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="port tags new profile"/>',
                      '<on event="start" run="port tags new profile"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/tags_button_profile",
                     [("type", "text"), ("width", 128), ("height", 24),
                      ("bitmap", "bitmaps/text_button_background"), ("text", "CREATE MAP"),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="port tags profile button"/>',
                      '<on event="start" run="port tags profile button"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/tags_button_back",
                     [("type", "text"), ("width", 128), ("height", 24),
                      ("bitmap", "bitmaps/text_button_background"), ("text", "BACK"),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="port tags back"/>',
                      '<on event="start" run="port tags back"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/tags_button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<data input="common button bar update"/>',
                      f'<child widget="{base}/tags_button_new" x="2" y="1"/>',
                      f'<child widget="{base}/tags_button_profile" x="129" y="1"/>',
                      f'<child widget="{base}/tags_button_back" x="510" y="1"/>'])
    lines += _widget(f"{base}/tags_select_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", f"{base}/tag_details")],
                     ['<data input="port tags update"/>',
                      '<on event="created" run="port tags init"/>',
                      '<on event="deleted" run="port tags close"/>',
                      *[f'<child widget="{base}/op_tag_{row}" x="20" y="{73 + 30 * (row - 1)}"/>'
                        for row in range(1, 12)],
                      f'<child widget="{base}/tags_button_bar" y="414"/>'])
    lines += _widget(f"{base}/creation_hub_screen",
                     [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                      ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" run="port tags back"/>',
                      '<on event="back" run="port tags back"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{MT}/header_creation_hub"/>',
                      f'<child widget="{base}/tags_select_list"/>'])
    return lines


def _dedi_server_screen() -> list:
    """Dedicated Server Director screen (main_menu/multiplayer_type_select/dedi_server/dedi_server_screen)."""
    base = f"{MT}/dedi_server"
    lines = []
    for row in range(1, 9):
        lines += _widget(f"{base}/op_dedi_{row}",
                         [("width", 390), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                          ("bitmap", "bitmaps/sel_list_item_bkd_top" if row == 1 else "bitmaps/sel_list_item_bkd"),
                          ("color", "#FF2896FF")],
                         ['<on event="a" run="dedi server choose"/>',
                          '<on event="start" run="dedi server choose"/>',
                          '<on event="left" run="dedi server left"/>',
                          '<on event="right" run="dedi server right"/>',
                          '<on event="left_mouse" run="mouse emit accept event"/>',
                          f'<child widget="{base}/dedi_name" x="10"/>',
                          f'<child widget="{base}/dedi_state" x="230"/>'])
    lines += _widget(f"{base}/dedi_name",
                     [("type", "text"), ("controller", 1), ("width", 216), ("height", 22), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_y", 4)], [])
    lines += _widget(f"{base}/dedi_state",
                     [("type", "text"), ("controller", 1), ("width", 150), ("height", 22), ("font", "ui\\small_ui"),
                      ("color", "#FF70B7FF"), ("align", "right"), ("text_y", 6)], [])
    lines += _widget(f"{base}/dedi_details_name",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 84), ("width", 200), ("height", 44),
                      ("font", "ui\\large_ui"), ("color", "#FFFFFFFF")], [])
    lines += _widget(f"{base}/dedi_details_text",
                     [("type", "text"), ("controller", 1), ("left", 412), ("top", 130), ("width", 200), ("height", 272),
                      ("font", "ui\\small_ui"), ("color", "#FF2896FF")], [])
    lines += _widget(f"{base}/dedi_details",
                     [("width", 640), ("height", 480)],
                     [f'<child widget="{base}/dedi_details_name"/>',
                      f'<child widget="{base}/dedi_details_text"/>'])
    lines += _widget(f"{base}/dedi_button_save",
                     [("type", "text"), ("width", 128), ("height", 24),
                      ("bitmap", "bitmaps/text_button_background"), ("text", "SAVE CONFIG"),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="dedi server save"/>',
                      '<on event="start" run="dedi server save"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/dedi_button_copy",
                     [("type", "text"), ("width", 128), ("height", 24),
                      ("bitmap", "bitmaps/text_button_background"), ("text", "COPY CLI CMD"),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="dedi server copy"/>',
                      '<on event="start" run="dedi server copy"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/dedi_button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<data input="common button bar update"/>',
                      f'<child widget="{base}/dedi_button_save" x="2" y="1"/>',
                      f'<child widget="{base}/dedi_button_copy" x="129" y="1"/>',
                      '<child widget="common_button_back" x="510" y="1"/>'])
    lines += _widget(f"{base}/dedi_select_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", f"{base}/dedi_details")],
                     ['<data input="dedi server update"/>',
                      '<on event="created" run="dedi server init"/>',
                      *[f'<child widget="{base}/op_dedi_{row}" x="20" y="{73 + 30 * (row - 1)}"/>'
                        for row in range(1, 9)],
                      f'<child widget="{base}/dedi_button_bar" y="414"/>'])
    lines += _widget(f"{base}/dedi_server_screen",
                     [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                      ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>',
                      '<on event="back" back="true"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{MT}/header_dedi_server"/>',
                      f'<child widget="{base}/dedi_select_list"/>'])
    return lines


def _forge_screens() -> list:
    """Forge mode screens (opened from Multiplayer -> Forge):
    1. main_menu/forge_select/forge_menu (Forge Hub styled identically to Gamemode Select)
    2. main_menu/forge_select/forge_mp_type_select_screen (Join/Create/Edit Maps for Multiplayer Forge)
    3. main_menu/forge_select/forge_edit_maps_screen (User Created & Downloaded Forge maps browser)."""
    fbase = "main_menu/forge_select"
    gm_base = f"{MT}/gamemode"
    lines = []

    # 1. Forge Hub menu items
    hub_items = [
        ("forge_item_single_player", "SINGLE PLAYER", "bitmaps/list_item_bkd_top", [
            '<on event="a" open="main_menu/solo_level_select/solo_level_select_screen"/>',
            '<on event="start" open="main_menu/solo_level_select/solo_level_select_screen"/>',
            '<on event="left_mouse" run="mouse emit accept event"/>',
        ]),
        ("forge_item_multiplayer", "MULTIPLAYER", "bitmaps/list_item_bkd", [
            f'<on event="a" open="{fbase}/forge_mp_type_select_screen"/>',
            f'<on event="start" open="{fbase}/forge_mp_type_select_screen"/>',
            '<on event="left_mouse" run="mouse emit accept event"/>',
        ]),
        ("forge_item_edit_maps", "EDIT MAPS", "bitmaps/list_item_bkd", [
            f'<on event="a" open="{fbase}/forge_edit_maps_screen"/>',
            f'<on event="start" open="{fbase}/forge_edit_maps_screen"/>',
            '<on event="left_mouse" run="mouse emit accept event"/>',
        ]),
        ("forge_item_tag_options", "TAG OPTIONS", "bitmaps/list_item_bkd", [
            f'<on event="a" open="{fbase}/forge_tag_options_menu"/>',
            f'<on event="start" open="{fbase}/forge_tag_options_menu"/>',
            '<on event="left_mouse" run="mouse emit accept event"/>',
        ]),
    ]
    for item_name, label, bkd, handlers in hub_items:
        lines += _widget(f"{fbase}/{item_name}",
                         [("type", "text"), ("left", 51), ("width", 232), ("height", 32),
                          ("bitmap", bkd), ("text", label), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                         handlers)

    lines += _widget(f"{fbase}/forge_menu_select_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_items"),
                      ("description", f"{gm_base}/gamemode_ext_desc")],
                     ['<data input="gamemode menu update"/>',
                      '<on event="created" run="gamemode menu init"/>',
                      *[f'<child widget="{fbase}/{item_name}" y="{78 + 33 * idx}"/>'
                        for idx, (item_name, *_) in enumerate(hub_items)],
                      f'<child widget="{gm_base}/gamemode_button_bar" y="414"/>'])
    lines += _widget(f"{fbase}/forge_menu",
                     [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                      ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>',
                      '<on event="back" back="true"/>',
                      f'<child widget="{MT}/header_forge"/>',
                      f'<child widget="{fbase}/forge_menu_select_list"/>',
                      '<child widget="main_menu/blueline" x="64" y="349"/>'])

    # 1b. Forge Tag Options menu (opened from Forge -> TAG OPTIONS, styled identically to Forge Hub)
    tag_options_items = [
        ("forge_item_inject_tags", "SYNC CUSTOM TAGS", "bitmaps/list_item_bkd_top", [
            '<on event="a" run="port mods save"/>',
            '<on event="start" run="port mods save"/>',
            '<on event="left_mouse" run="mouse emit accept event"/>',
        ]),
        ("clear_custom_cache_profile_item", "CLEAR CUSTOM TAGS", "bitmaps/list_item_bkd", [
            '<on event="a" run="port mods close"/>',
            '<on event="start" run="port mods close"/>',
            '<on event="left_mouse" run="mouse emit accept event"/>',
        ]),
        ("reset_tag_cache_profile_item", "RESET TAG CACHE", "bitmaps/list_item_bkd", [
            '<on event="a" run="port mods close"/>',
            '<on event="start" run="port mods close"/>',
            '<on event="left_mouse" run="mouse emit accept event"/>',
        ]),
    ]
    for item_name, label, bkd, handlers in tag_options_items:
        lines += _widget(f"{fbase}/{item_name}",
                         [("type", "text"), ("left", 51), ("width", 232), ("height", 32),
                          ("bitmap", bkd), ("text", label), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                         handlers)

    lines += _widget(f"{fbase}/forge_tag_options_select_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_items"),
                      ("description", f"{gm_base}/gamemode_ext_desc")],
                     ['<data input="gamemode menu update"/>',
                      '<on event="created" run="gamemode menu init"/>',
                      *[f'<child widget="{fbase}/{item_name}" y="{78 + 33 * idx}"/>'
                        for idx, (item_name, *_) in enumerate(tag_options_items)],
                      f'<child widget="{gm_base}/gamemode_button_bar" y="414"/>'])
    lines += _widget(f"{fbase}/forge_tag_options_menu",
                     [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                      ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>',
                      '<on event="back" back="true"/>',
                      f'<child widget="{MT}/header_forge"/>',
                      f'<child widget="{fbase}/forge_tag_options_select_list"/>',
                      '<child widget="main_menu/blueline" x="64" y="349"/>'])

    # 2. Forge Multiplayer Type Select Screen
    lines += _widget(f"{fbase}/forge_type_edit_maps_item",
                     [("type", "text"), ("left", 51), ("width", 232), ("height", 32),
                      ("bitmap", "bitmaps/list_item_bkd"), ("text", "EDIT MAPS"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                     [f'<on event="a" open="{fbase}/forge_edit_maps_screen"/>',
                      f'<on event="start" open="{fbase}/forge_edit_maps_screen"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{fbase}/forge_mp_type_select_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_items"),
                      ("description", f"{MT}/multiplayer_type_list_ext_desc")],
                     ['<data input="multiplayer type menu update desc"/>',
                      '<data input="mp type screen update"/>',
                      f'<child widget="{MT}/multiplayer_type_join_item" x="-32" y="78"/>',
                      f'<child widget="{MT}/multiplayer_type_join_internet_item" y="107"/>',
                      f'<child widget="{MT}/multiplayer_type_join_lan_item" y="136"/>',
                      f'<child widget="{MT}/multiplayer_type_join_direct_item" y="165"/>',
                      f'<child widget="{MT}/multiplayer_type_create_item" x="-32" y="194"/>',
                      f'<child widget="{MT}/multiplayer_type_create_internet_item" y="223"/>',
                      f'<child widget="{MT}/multiplayer_type_create_lan_item" y="252"/>',
                      f'<child widget="{MT}/multiplayer_type_create_local_item" y="281"/>',
                      f'<child widget="{fbase}/forge_type_edit_maps_item" y="358"/>',
                      f'<child widget="{MT}/multiplayer_type_button_bar" y="414"/>'])
    lines += _widget(f"{fbase}/forge_mp_type_select_screen",
                     [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                      ("bitmap", "bitmaps/gradient")],
                     ['<on event="created" run="mp type menu initialize"/>',
                      '<on event="b" back="true"/>',
                      '<on event="back" back="true"/>',
                      f'<child widget="{MT}/header_forge"/>',
                      f'<child widget="{fbase}/forge_mp_type_select_list"/>',
                      '<child widget="main_menu/blueline" x="64" y="349"/>'])

    # 3. Forge Edit Maps Screen
    lines += _widget(f"{fbase}/edit_maps_kind_spinner",
                     [("type", "spinner"), ("width", 238), ("height", 22), ("flags", "left_right_tabs_items"),
                      ("strings", "USER CREATED|DOWNLOADED"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 1),
                      ("header_bitmap", "bitmaps/arrow_sm_left"), ("footer_bitmap", "bitmaps/arrow_sm_right"),
                      ("header_bounds", "7 -6 19 0"), ("footer_bounds", "7 238 19 244")],
                     ['<on event="left_mouse" run="mouse spinner 1wide click"/>'])
    lines += _widget(f"{fbase}/edit_maps_kind_row",
                     [("width", 256), ("height", 28), ("flags", "pass_unhandled_to_focused_child"),
                      ("bitmap", "bitmaps/option_bkds_sm"), ("font", "ui\\large_ui"),
                      ("color", "#FF2896FF"), ("align", "center"), ("text_y", 3)],
                     [f'<child widget="{fbase}/edit_maps_kind_spinner" x="9" y="2"/>'])
    lines += _widget(f"{fbase}/forge_edit_maps_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_children"),
                      ("description", "main_menu/solo_level_select/solo_level_extended_desc")],
                     ['<data input="load game list update"/>',
                      '<on event="created" run="load game menu init"/>',
                      '<on event="custom_activation" run="load game menu activated"/>',
                      '<on event="b" back="true"/>',
                      '<on event="back" back="true"/>',
                      f'<child widget="{fbase}/edit_maps_kind_row" x="82" y="73"/>',
                      *[f'<child widget="main_menu/new_select/list_item_{idx}" x="20" y="{103 + 30 * (idx - 1)}"/>'
                        for idx in range(1, 11)],
                      '<child widget="main_menu/solo_level_select/solo_level_select_button_bar" y="414"/>'])
    lines += _widget(f"{fbase}/forge_edit_maps_screen",
                     [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                      ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>',
                      '<on event="back" back="true"/>',
                      '<child widget="main_menu/new_select/sel_list_desc_bkd"/>',
                      f'<child widget="{fbase}/forge_edit_maps_list"/>',
                      f'<child widget="{MT}/header_forge"/>'])
    return lines


def _service_record_screen() -> list:
    """Dedicated SERVICE RECORD menu screen with 7 vertical tabs on the left
    (OVERVIEW, FIREFIGHT STATS, CAMPAIGN STATS, MP STANDARD, MP RANKED, MEDALS CHEST, ACHIEVEMENTS)
    plus CHECK LEADERBOARDS at the bottom, and the stats/medals/achievements display box on the right."""
    base = f"{MT}/service_record"
    lines = _header(f"{base}/header_service_record", f"{base}/header_service_record")
    tabs = [
        ("sr_tab_0", "OVERVIEW", "bitmaps/list_item_bkd_top"),
        ("sr_tab_1", "FIREFIGHT STATS", "bitmaps/list_item_bkd"),
        ("sr_tab_2", "CAMPAIGN STATS", "bitmaps/list_item_bkd"),
        ("sr_tab_3", "MP STANDARD", "bitmaps/list_item_bkd"),
        ("sr_tab_4", "MP RANKED", "bitmaps/list_item_bkd"),
        ("sr_tab_5", "MEDALS CHEST", "bitmaps/list_item_bkd"),
        ("sr_tab_6", "ACHIEVEMENTS", "bitmaps/list_item_bkd"),
    ]
    for name, text, bkd in tabs:
        handlers = [
            '<on event="a" run="service record select tab"/>',
            '<on event="start" run="service record select tab"/>',
            '<on event="left_mouse" run="mouse emit accept event"/>',
        ]
        if name in ("sr_tab_5", "sr_tab_6"):
            handlers += [
                '<on event="left" run="service record medals prev"/>',
                '<on event="right" run="service record medals next"/>',
            ]
        lines += _widget(f"{base}/{name}",
                         [("type", "text"), ("left", 51), ("width", 232), ("height", 32),
                          ("bitmap", bkd), ("text", text), ("font", "ui\\large_ui"),
                          ("color", "#FF2896FF"), ("text_x", 13), ("text_y", 5)],
                         handlers)
    lines += _widget(f"{base}/sr_check_leaderboards",
                     [("type", "text"), ("left", 51), ("width", 232), ("height", 32),
                      ("bitmap", "bitmaps/list_item_bkd"), ("text", "CHECK LEADERBOARDS"),
                      ("font", "ui\\large_ui"), ("color", "#FFFFD040"), ("text_x", 13), ("text_y", 5)],
                     ['<on event="a" run="service record check leaderboards"/>',
                      '<on event="start" run="service record check leaderboards"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/sr_button_export",
                     [("type", "text"), ("width", 128), ("height", 24),
                      ("bitmap", "bitmaps/text_button_background"), ("text", "EXPORT JSON"),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="service record export json"/>',
                      '<on event="start" run="service record export json"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/sr_button_copy_id",
                     [("type", "text"), ("width", 128), ("height", 24),
                      ("bitmap", "bitmaps/text_button_background"), ("text", "COPY PLAYER ID"),
                      ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="service record copy id"/>',
                      '<on event="start" run="service record copy id"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/sr_button_link_account",
                     [("type", "text"), ("width", 128), ("height", 24),
                      ("bitmap", "bitmaps/text_button_background"), ("text", "LINK ACCOUNT"),
                      ("font", "ui\\small_ui"), ("color", "#FFFFD040"), ("align", "center"), ("text_y", 2)],
                     ['<on event="a" run="service record link account"/>',
                      '<on event="start" run="service record link account"/>',
                      '<on event="left_mouse" run="mouse emit accept event"/>'])
    lines += _widget(f"{base}/service_record_button_bar",
                     [("type", "column_list"), ("width", 640), ("height", 28),
                      ("flags", "pass_unhandled_to_focused_child left_right_tabs_items")],
                     ['<data input="common button bar update"/>',
                      f'<child widget="{base}/sr_button_export" x="2" y="1"/>',
                      f'<child widget="{base}/sr_button_copy_id" x="134" y="1"/>',
                      f'<child widget="{base}/sr_button_link_account" x="266" y="1"/>',
                      '<child widget="common_button_back" x="510" y="1"/>'])
    lines += _widget(f"{base}/sr_box_title",
                     [("type", "text"), ("controller", 1), ("left", 290), ("top", 88), ("width", 285), ("height", 22),
                      ("font", "ui\\small_ui"), ("color", "#FFFFD040")], [])
    desc_children = [f'<child widget="{base}/sr_box_title"/>']
    for row in range(1, 8):
        top = 120 + (row - 1) * 28
        lines += _widget(f"{base}/sr_r{row}_label",
                         [("type", "text"), ("controller", 1), ("left", 290), ("top", top), ("width", 200),
                          ("height", 22), ("font", "ui\\small_ui"), ("color", "#FF2896FF")], [])
        lines += _widget(f"{base}/sr_r{row}_val",
                         [("type", "text"), ("controller", 1), ("left", 375), ("top", top), ("width", 200),
                          ("height", 22), ("font", "ui\\small_ui"), ("color", "#FFFFFFFF"), ("align", "right")], [])
        desc_children.append(f'<child widget="{base}/sr_r{row}_label"/>')
        desc_children.append(f'<child widget="{base}/sr_r{row}_val"/>')
    lines += _widget(f"{base}/sr_arrow_left",
                     [("controller", 1), ("left", 290), ("top", 324), ("width", 16), ("height", 16),
                      ("bitmap", "bitmaps/arrow_sm_left")], [])
    lines += _widget(f"{base}/sr_box_footer",
                     [("type", "text"), ("controller", 1), ("left", 310), ("top", 324), ("width", 246), ("height", 22),
                      ("font", "ui\\small_ui"), ("color", "#FF2896FF"), ("align", "center")], [])
    lines += _widget(f"{base}/sr_arrow_right",
                     [("controller", 1), ("left", 560), ("top", 324), ("width", 16), ("height", 16),
                      ("bitmap", "bitmaps/arrow_sm_right")], [])
    desc_children.append(f'<child widget="{base}/sr_arrow_left"/>')
    desc_children.append(f'<child widget="{base}/sr_box_footer"/>')
    desc_children.append(f'<child widget="{base}/sr_arrow_right"/>')
    desc_children.append('<child widget="main_menu/current_profile_name"/>')
    lines += _widget(f"{base}/service_record_ext_desc",
                     [("controller", 1), ("left", 275), ("top", 78), ("width", 320), ("height", 322),
                      ("bitmap", "bitmaps/list_field"), ("color", "#FF0080FF")],
                     desc_children)
    lines += _widget(f"{base}/service_record_list",
                     [("type", "column_list"), ("width", 640), ("height", 480),
                      ("flags", "pass_unhandled_to_focused_child up_down_tabs_items"),
                      ("description", f"{base}/service_record_ext_desc")],
                     ['<data input="service record update"/>',
                      '<on event="created" run="service record init"/>',
                      f'<child widget="{base}/sr_tab_0" y="78"/>',
                      f'<child widget="{base}/sr_tab_1" y="112"/>',
                      f'<child widget="{base}/sr_tab_2" y="146"/>',
                      f'<child widget="{base}/sr_tab_3" y="180"/>',
                      f'<child widget="{base}/sr_tab_4" y="214"/>',
                      f'<child widget="{base}/sr_tab_5" y="248"/>',
                      f'<child widget="{base}/sr_tab_6" y="282"/>',
                      f'<child widget="{base}/sr_check_leaderboards" y="358"/>',
                      f'<child widget="{base}/service_record_button_bar" y="414"/>'])
    lines += _widget(f"{base}/service_record_screen",
                     [("width", 640), ("height", 480), ("flags", "pass_unhandled_to_focused_child"),
                      ("bitmap", "bitmaps/gradient")],
                     ['<on event="b" back="true"/>',
                      '<on event="back" back="true"/>',
                      f'<child widget="{base}/header_service_record"/>',
                      f'<child widget="{base}/service_record_list"/>',
                      '<child widget="main_menu/blueline" x="64" y="349"/>'])
    return lines


def _render_firefight_emblem_pil(for_card: bool = False, scale: int = 4):
    """High-resolution anti-aliased renderer for the Halo CE-styled Firefight emblem
    (#2896ff cyan wireframe & scanline glow, #0a2649 dark blue interior glow,
    black silhouette core), matching the original Halo CE gametype icons."""
    import math
    from PIL import Image, ImageChops, ImageDraw, ImageFilter

    ss = scale * 2  # 2x supersampling above target 4x output for crisp anti-aliased vectors
    w_log, h_log = (256, 128) if for_card else (256, 256)
    W, H = w_log * ss, h_log * ss
    cx, cy = (70.0 * ss, 58.0 * ss) if for_card else (128.0 * ss, 96.0 * ss)
    sc = (0.56 if for_card else 1.0) * ss

    def pt(x: float, y: float) -> tuple:
        return (cx + x * sc, cy + y * sc)

    def rot_pts(pts: list, deg: float) -> list:
        rad = math.radians(deg)
        c, s = math.cos(rad), math.sin(rad)
        return [pt(x * c - y * s, x * s + y * c) for x, y in pts]

    def star_pts(sx: float, sy: float, st_scale: float) -> list:
        raw = [
            (0, -9), (2.6, -2.8), (9.2, -2.2), (4.1, 2.2), (5.7, 8.8),
            (0, 5.2), (-5.7, 8.8), (-4.1, 2.2), (-9.2, -2.2), (-2.6, -2.8),
        ]
        return [pt(sx + x * st_scale, sy + y * st_scale) for x, y in raw]

    rifle_poly = [
        (-84, -6), (-54, -9), (-48, -5), (46, -5), (64, -8), (82, -3),
        (82, 4), (56, 5), (46, 9), (18, 9), (12, 5), (-46, 5), (-56, 11), (-84, 8),
    ]
    rifle_1 = rot_pts(rifle_poly, -42.0)
    rifle_2 = rot_pts(rifle_poly, 42.0)
    outer_shield = [pt(x, y) for x, y in [(0, -60), (46, -36), (38, 24), (0, 68), (-38, 24), (-46, -36)]]
    inner_shield = [pt(x, y) for x, y in [(0, -53), (39, -32), (32, 20), (0, 58), (-32, 20), (-39, -32)]]
    helmet_poly = [pt(x, y) for x, y in [(0, -42), (26, -27), (28, 2), (17, 22), (13, 38), (-13, 38), (-17, 22), (-28, 2), (-26, -27)]]
    visor_poly = [pt(x, y) for x, y in [(0, -16), (22, -19), (18, -3), (4, -2), (0, -7), (-4, -2), (-18, -3), (-22, -19)]]
    nose_poly = [pt(x, y) for x, y in [(0, 0), (5, 11), (-5, 11)]]

    canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    if for_card:
        card_w, card_h = int(140 * ss), int(114 * ss)
        card = Image.new("RGBA", (card_w, card_h), (0, 0, 0, 0))
        cdraw = ImageDraw.Draw(card)
        for y in range(card_h):
            t = y / max(1, card_h - 1)
            if t < 0.55:
                u = t / 0.55
                r = int(6 + (10 - 6) * u)
                g = int(23 + (40 - 23) * u)
                b = int(40 + (67 - 40) * u)
            else:
                u = (t - 0.55) / 0.45
                r = int(10 + (21 - 10) * u)
                g = int(40 + (77 - 40) * u)
                b = int(67 + (133 - 67) * u)
            cdraw.line([(0, y), (card_w - 1, y)], fill=(r, g, b, 255))
        cdraw.rectangle([0, 0, card_w - 1, max(1, int(1.5 * ss))], fill=(40, 150, 255, 220))
        cdraw.rectangle([0, card_h - max(1, int(1.5 * ss)), card_w - 1, card_h - 1], fill=(40, 150, 255, 220))
        canvas.alpha_composite(card, (0, 0))

    # Outer cyan Gaussian glow layer (#2896ff)
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gdraw = ImageDraw.Draw(glow)
    gdraw.polygon(rifle_1, fill=(40, 150, 255, 190))
    gdraw.polygon(rifle_2, fill=(40, 150, 255, 190))
    gdraw.polygon(outer_shield, fill=(40, 150, 255, 210))
    for sx, sy, st_s in [(-38, -62, 0.62), (-19, -70, 0.68), (0, -74, 0.78), (19, -70, 0.68), (38, -62, 0.62)]:
        gdraw.polygon(star_pts(sx, sy, st_s), fill=(40, 150, 255, 180))
    glow = glow.filter(ImageFilter.GaussianBlur(radius=5.2 * sc))
    canvas.alpha_composite(glow)

    # Core layer
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    w_main = max(2, int(round(2.2 * sc)))
    w_mid = max(2, int(round(1.6 * sc)))
    w_thin = max(1, int(round(1.1 * sc)))

    for r_pts, deg in [(rifle_1, -42.0), (rifle_2, 42.0)]:
        draw.polygon(r_pts, fill=(0, 0, 0, 255))
        draw.line(r_pts + [r_pts[0]], fill=(40, 150, 255, 255), width=w_main, joint="curve")
        line_pts = rot_pts([(-44, -1), (54, -1)], deg)
        draw.line(line_pts, fill=(40, 150, 255, 200), width=w_thin)

    draw.polygon(outer_shield, fill=(0, 0, 0, 255))
    draw.line(outer_shield + [outer_shield[0]], fill=(40, 150, 255, 255), width=w_main, joint="curve")
    draw.polygon(inner_shield, fill=(10, 38, 73, 235))

    # Scanlines clipped to inner_shield
    scan_mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(scan_mask).polygon(inner_shield, fill=95)
    scan_layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sdraw = ImageDraw.Draw(scan_layer)
    for sy_off in range(-48, 56, 6):
        p0, p1 = pt(-42, sy_off), pt(42, sy_off)
        sdraw.line([p0, p1], fill=(40, 150, 255, 95), width=max(1, int(round(0.9 * sc))))
    scan_layer.putalpha(ImageChops.multiply(scan_layer.getchannel("A"), scan_mask))
    layer.alpha_composite(scan_layer)
    draw = ImageDraw.Draw(layer)
    draw.line(inner_shield + [inner_shield[0]], fill=(40, 150, 255, 235), width=w_mid, joint="curve")

    # Helmet & Skull Visor
    draw.polygon(helmet_poly, fill=(0, 0, 0, 255))
    draw.line(helmet_poly + [helmet_poly[0]], fill=(40, 150, 255, 255), width=w_main, joint="curve")
    draw.polygon(visor_poly, fill=(14, 54, 102, 255))
    draw.line(visor_poly + [visor_poly[0]], fill=(40, 150, 255, 255), width=w_mid, joint="curve")
    draw.polygon(nose_poly, fill=(10, 38, 73, 255))
    draw.line(nose_poly + [nose_poly[0]], fill=(40, 150, 255, 240), width=w_thin, joint="curve")

    # Jaw teeth lines
    for tx, ty1 in [(-8.0, 35.0), (-2.7, 36.0), (2.7, 36.0), (8.0, 35.0)]:
        draw.line([pt(tx, 18.0), pt(tx, ty1)], fill=(40, 150, 255, 245), width=w_mid)

    # 5 Wave Stars above crest
    for sx, sy, st_s in [(-38, -62, 0.62), (-19, -70, 0.68), (0, -74, 0.78), (19, -70, 0.68), (38, -62, 0.62)]:
        spts = star_pts(sx, sy, st_s)
        draw.polygon(spts, fill=(10, 38, 73, 255))
        draw.line(spts + [spts[0]], fill=(40, 150, 255, 255), width=w_thin, joint="curve")

    if for_card:
        # Clip to the 140x114 card area
        clip = Image.new("L", (W, H), 0)
        ImageDraw.Draw(clip).rectangle([0, 0, int(140 * ss) - 1, int(114 * ss) - 1], fill=255)
        canvas.alpha_composite(layer)
        canvas.putalpha(ImageChops.multiply(canvas.getchannel("A"), clip))
    else:
        canvas.alpha_composite(layer)

    return canvas.resize((w_log * scale, h_log * scale), Image.LANCZOS)


def _render_locked_mode_emblem_pil(kind: str, scale: int = 4):
    """Renders a Halo CE wireframe emblem for RANKED (dimmed) or FORGE (active bright cyan) (256x256 logical)."""
    from PIL import Image, ImageChops, ImageDraw, ImageFilter

    ss = scale * 2
    W, H = 256 * ss, 256 * ss
    cx, cy = 128.0 * ss, 96.0 * ss
    sc = float(ss)
    active = (kind != "ranked")

    def pt(x: float, y: float) -> tuple:
        return (cx + x * sc, cy + y * sc)

    canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gdraw = ImageDraw.Draw(glow)
    outer = [pt(x, y) for x, y in [(0, -58), (48, -30), (48, 26), (0, 64), (-48, 26), (-48, -30)]]
    gdraw.polygon(outer, fill=(40, 150, 255, 205 if active else 110))
    glow = glow.filter(ImageFilter.GaussianBlur(radius=(5.2 if active else 4.5) * sc))
    canvas.alpha_composite(glow)

    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    w_main = max(2, int(round((2.2 if active else 2.0) * sc)))
    w_mid = max(2, int(round((1.6 if active else 1.5) * sc)))

    draw.polygon(outer, fill=(0, 0, 0, 255 if active else 220))
    draw.line(outer + [outer[0]], fill=(40, 150, 255, 255 if active else 150), width=w_main, joint="curve")
    inner = [pt(x, y) for x, y in [(0, -50), (40, -25), (40, 21), (0, 55), (-40, 21), (-40, -25)]]
    draw.polygon(inner, fill=(10, 38, 73, 235 if active else 160))
    if active:
        scan_mask = Image.new("L", (W, H), 0)
        ImageDraw.Draw(scan_mask).polygon(inner, fill=95)
        scan_layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        sdraw = ImageDraw.Draw(scan_layer)
        for sy_off in range(-46, 52, 6):
            sdraw.line([pt(-42, sy_off), pt(42, sy_off)], fill=(40, 150, 255, 95), width=max(1, int(round(0.9 * sc))))
        scan_layer.putalpha(ImageChops.multiply(scan_layer.getchannel("A"), scan_mask))
        layer.alpha_composite(scan_layer)
        draw = ImageDraw.Draw(layer)
    draw.line(inner + [inner[0]], fill=(40, 150, 255, 235 if active else 130), width=w_mid, joint="curve")

    if kind == "ranked":
        # Ranked chevrons
        for dy in (-18, 0, 18):
            chev = [pt(-24, dy + 8), pt(0, dy - 10), pt(24, dy + 8), pt(24, dy + 16), pt(0, dy - 2), pt(-24, dy + 16)]
            draw.polygon(chev, fill=(0, 0, 0, 240))
            draw.line(chev + [chev[0]], fill=(40, 150, 255, 165), width=w_mid, joint="curve")
    elif kind == "theater":
        # Theater film frame + play triangle
        frame_box = [pt(-26, -20), pt(26, -20), pt(26, 20), pt(-26, 20)]
        draw.polygon(frame_box, fill=(0, 0, 0, 255))
        draw.line(frame_box + [frame_box[0]], fill=(40, 150, 255, 255), width=w_mid, joint="curve")
        play = [pt(-9, -11), pt(13, 0), pt(-9, 11)]
        draw.polygon(play, fill=(40, 150, 255, 235))
        for sx in (-20, -10, 0, 10, 20):
            for sy in (-16, 16):
                sprocket = [pt(sx - 2, sy - 2), pt(sx + 2, sy - 2), pt(sx + 2, sy + 2), pt(sx - 2, sy + 2)]
                draw.polygon(sprocket, fill=(40, 150, 255, 220))
    elif kind == "creation_hub":
        # Forerunner Creation Hub prism / cube + inner core
        top_face = [pt(0, -28), pt(24, -14), pt(0, 0), pt(-24, -14)]
        left_face = [pt(-24, -14), pt(0, 0), pt(0, 28), pt(-24, 14)]
        right_face = [pt(0, 0), pt(24, -14), pt(24, 14), pt(0, 28)]
        for face in (top_face, left_face, right_face):
            draw.polygon(face, fill=(0, 0, 0, 245))
            draw.line(face + [face[0]], fill=(40, 150, 255, 255), width=w_mid, joint="curve")
        core = [pt(0, -10), pt(9, 0), pt(0, 10), pt(-9, 0)]
        draw.polygon(core, fill=(40, 150, 255, 235))
    elif kind == "dedi_server":
        # Dedicated Server 3-blade rack with status LEDs
        for ry in (-20, -2, 16):
            blade = [pt(-26, ry - 6), pt(26, ry - 6), pt(26, ry + 6), pt(-26, ry + 6)]
            draw.polygon(blade, fill=(0, 0, 0, 255))
            draw.line(blade + [blade[0]], fill=(40, 150, 255, 255), width=w_mid, joint="curve")
            led = [pt(-20, ry - 2), pt(-14, ry - 2), pt(-14, ry + 2), pt(-20, ry + 2)]
            draw.polygon(led, fill=(40, 150, 255, 255))
            slot = [pt(4, ry), pt(20, ry)]
            draw.line(slot, fill=(40, 150, 255, 200), width=w_mid)
    else:
        # Forge anvil + hammer silhouette
        anvil = [pt(-28, 12), pt(28, 12), pt(20, 22), pt(12, 22), pt(16, 34), pt(-16, 34), pt(-12, 22), pt(-20, 22)]
        hammer = [pt(-6, 10), pt(-6, -24), pt(-22, -24), pt(-22, -34), pt(18, -34), pt(24, -26), pt(6, -24), pt(6, 10)]
        draw.polygon(hammer, fill=(0, 0, 0, 255))
        draw.line(hammer + [hammer[0]], fill=(40, 150, 255, 255), width=w_mid, joint="curve")
        draw.polygon(anvil, fill=(0, 0, 0, 255))
        draw.line(anvil + [anvil[0]], fill=(40, 150, 255, 255), width=w_mid, joint="curve")

    canvas.alpha_composite(layer)
    return canvas.resize((256 * scale, 256 * scale), Image.LANCZOS)


def _render_title_png(name: str, text: str, menus_dir: Path, font_path: Path, scale: int = 4) -> None:
    """Renders a Halo CE menu header title PNG (512x64 logical, 2048x256 4x PNG)
    using OpenCE-Regular.ttf over the soft dark blue header backdrop."""
    from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

    probe = ImageFont.truetype(str(font_path), 1000)
    left, top, right, bottom = probe.getbbox("H")
    font = ImageFont.truetype(str(font_path), round(TITLE_CAP * scale * 1000 / (bottom - top)))
    width = font.getlength(text) / scale

    svg_path = menus_dir / "port_svg" / f"{name}.svg"
    svg_path.parent.mkdir(parents=True, exist_ok=True)
    svg_path.write_text(title_backdrop(width), encoding="utf-8")

    W, H = 512 * scale, 64 * scale
    backdrop = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    bdraw = ImageDraw.Draw(backdrop)
    rx0 = int(round(29.08 * scale))
    ry0 = int(round(28.05 * scale))
    rx1 = int(round((29.08 + width + 28.0) * scale))
    ry1 = int(round((28.05 + 40.95) * scale))
    bdraw.rectangle([rx0, ry0, rx1, ry1], fill=(2, 25, 49, int(round(0.893 * 255))))
    backdrop = backdrop.filter(ImageFilter.GaussianBlur(radius=13.5 * scale))
    # Mask to y < 59 (as title_backdrop's <rect width="512" height="59"/> does)
    cut = Image.new("L", (W, H), 0)
    ImageDraw.Draw(cut).rectangle([0, 0, W - 1, int(59 * scale) - 1], fill=255)
    backdrop.putalpha(ImageChops.multiply(backdrop.getchannel("A"), cut))

    draw = ImageDraw.Draw(backdrop)
    cap_top = font.getbbox("H")[1]
    draw.text((TITLE_LEFT * scale, TITLE_TOP * scale - cap_top), text, font=font, fill=TITLE_COLOR)

    png_path = menus_dir / f"ce/port/{name}.png"
    png_path.parent.mkdir(parents=True, exist_ok=True)
    backdrop.save(png_path, optimize=True)


def generate_firefight_menu_assets() -> None:
    """Generates the Firefight & Gamemode SVG/PNG assets and updates the menu XMLs
    in port/assets/menus/ce."""
    root = Path(__file__).resolve().parent.parent
    menus_dir = root / "port/assets/menus"
    ce_dir = menus_dir / "ce"
    font_path = root / "port/assets/fonts/OpenCE-Regular.ttf"

    # 1. Render header_gamemode.png, header_service_record.png, header_bots_options.png, and the 7 gamemode headers
    _render_title_png(f"{MT}/server_settings/bots_options/header_bots_options", "BOTS", menus_dir, font_path)
    _render_title_png(f"{MT}/gamemode/header_gamemode", "GAMEMODE", menus_dir, font_path)
    _render_title_png(f"{MT}/service_record/header_service_record", "SERVICE RECORD", menus_dir, font_path)
    _render_title_png(f"{MT}/header_standard", "STANDARD", menus_dir, font_path)
    _render_title_png(f"{MT}/header_ranked", "RANKED", menus_dir, font_path)
    _render_title_png(f"{MT}/header_firefight", "FIREFIGHT", menus_dir, font_path)
    _render_title_png(f"{MT}/header_forge", "FORGE", menus_dir, font_path)
    _render_title_png(f"{MT}/header_theater", "THEATER", menus_dir, font_path)
    _render_title_png(f"{MT}/header_creation_hub", "CREATION HUB", menus_dir, font_path)
    _render_title_png(f"{MT}/header_dedi_server", "DEDICATED SERVER", menus_dir, font_path)

    # 2. Write Firefight emblem SVGs and render 4x PNGs
    ff_svg_256 = menus_dir / "port_svg/gamemode/firefight_logo__0.svg"
    ff_svg_256.parent.mkdir(parents=True, exist_ok=True)
    ff_svg_256.write_text(firefight_emblem_svg(for_card=False), encoding="utf-8")

    ff_svg_card = menus_dir / "port_svg/bitmaps/game_type_grafix__6.svg"
    ff_svg_card.parent.mkdir(parents=True, exist_ok=True)
    ff_svg_card.write_text(firefight_emblem_svg(for_card=True), encoding="utf-8")

    ff_png_256 = ce_dir / "port/gamemode/firefight_logo__0.png"
    ff_png_256.parent.mkdir(parents=True, exist_ok=True)
    _render_firefight_emblem_pil(for_card=False, scale=4).save(ff_png_256, optimize=True)

    ff_png_card = ce_dir / "port/bitmaps/game_type_grafix__6.png"
    ff_png_card.parent.mkdir(parents=True, exist_ok=True)
    _render_firefight_emblem_pil(for_card=True, scale=4).save(ff_png_card, optimize=True)

    ranked_png = ce_dir / "port/gamemode/ranked_logo__0.png"
    _render_locked_mode_emblem_pil("ranked", scale=4).save(ranked_png, optimize=True)

    forge_png = ce_dir / "port/gamemode/forge_logo__0.png"
    _render_locked_mode_emblem_pil("forge", scale=4).save(forge_png, optimize=True)

    theater_png = ce_dir / "port/gamemode/theater_logo__0.png"
    _render_locked_mode_emblem_pil("theater", scale=4).save(theater_png, optimize=True)

    creation_hub_png = ce_dir / "port/gamemode/creation_hub_logo__0.png"
    _render_locked_mode_emblem_pil("creation_hub", scale=4).save(creation_hub_png, optimize=True)

    dedi_server_png = ce_dir / "port/gamemode/dedi_server_logo__0.png"
    _render_locked_mode_emblem_pil("dedi_server", scale=4).save(dedi_server_png, optimize=True)

    # 3. Update bitmaps.xml
    bitmaps_xml_path = ce_dir / "bitmaps.xml"
    bitmaps_text = bitmaps_xml_path.read_text(encoding="utf-8")
    # Replace frame 6 of bitmaps/game_type_grafix with our Halo CE Firefight card graphic
    bitmaps_text = re.sub(
        r'(<bitmap name="bitmaps/game_type_grafix">.*?\n\t\t<frame png="ce/shell/bitmaps/game_type_grafix__5\.png" width="256" height="128"/>\n\t\t)<frame png="[^"]+" width="256" height="128"/>',
        r'\1<frame png="ce/port/bitmaps/game_type_grafix__6.png" width="256" height="128"/>',
        bitmaps_text,
        flags=re.DOTALL,
    )
    # Replace frame 5 of rules_options with our Halo CE Firefight 256x256 logo
    bitmaps_text = re.sub(
        r'(<bitmap name="main_menu/settings_select/multiplayer_setup/playlist_edit/game_type_select/rules_options">.*?\n\t\t<frame png="ce/shell/main_menu/settings_select/multiplayer_setup/playlist_edit/game_type_select/rules_options__4\.png" width="256" height="256"/>\n\t\t)<frame png="[^"]+" width="256" height="256"/>',
        r'\1<frame png="ce/port/gamemode/firefight_logo__0.png" width="256" height="256"/>',
        bitmaps_text,
        flags=re.DOTALL,
    )
    # Add gamemode_options, header_gamemode, header_service_record, header_bots_options, and gamemode headers
    extra_bitmaps = "\n".join([
        f'\t<bitmap name="{MT}/server_settings/bots_options/header_bots_options">',
        f'\t\t<frame png="ce/port/{MT}/server_settings/bots_options/header_bots_options.png" width="512" height="64"/>',
        "\t</bitmap>",
        f'\t<bitmap name="{MT}/header_standard">',
        f'\t\t<frame png="ce/port/{MT}/header_standard.png" width="512" height="64"/>',
        "\t</bitmap>",
        f'\t<bitmap name="{MT}/header_ranked">',
        f'\t\t<frame png="ce/port/{MT}/header_ranked.png" width="512" height="64"/>',
        "\t</bitmap>",
        f'\t<bitmap name="{MT}/header_firefight">',
        f'\t\t<frame png="ce/port/{MT}/header_firefight.png" width="512" height="64"/>',
        "\t</bitmap>",
        f'\t<bitmap name="{MT}/header_forge">',
        f'\t\t<frame png="ce/port/{MT}/header_forge.png" width="512" height="64"/>',
        "\t</bitmap>",
        f'\t<bitmap name="{MT}/header_theater">',
        f'\t\t<frame png="ce/port/{MT}/header_theater.png" width="512" height="64"/>',
        "\t</bitmap>",
        f'\t<bitmap name="{MT}/header_creation_hub">',
        f'\t\t<frame png="ce/port/{MT}/header_creation_hub.png" width="512" height="64"/>',
        "\t</bitmap>",
        f'\t<bitmap name="{MT}/header_dedi_server">',
        f'\t\t<frame png="ce/port/{MT}/header_dedi_server.png" width="512" height="64"/>',
        "\t</bitmap>",
        f'\t<bitmap name="{MT}/gamemode/header_gamemode">',
        f'\t\t<frame png="ce/port/{MT}/gamemode/header_gamemode.png" width="512" height="64"/>',
        "\t</bitmap>",
        f'\t<bitmap name="{MT}/gamemode/gamemode_options">',
        '\t\t<frame png="ce/shell/main_menu/settings_select/multiplayer_setup/playlist_edit/game_type_select/rules_options__2.png" width="256" height="256"/>',
        '\t\t<frame png="ce/port/gamemode/ranked_logo__0.png" width="256" height="256"/>',
        '\t\t<frame png="ce/port/gamemode/firefight_logo__0.png" width="256" height="256"/>',
        '\t\t<frame png="ce/port/gamemode/forge_logo__0.png" width="256" height="256"/>',
        '\t\t<frame png="ce/port/gamemode/theater_logo__0.png" width="256" height="256"/>',
        '\t\t<frame png="ce/port/gamemode/creation_hub_logo__0.png" width="256" height="256"/>',
        '\t\t<frame png="ce/port/gamemode/dedi_server_logo__0.png" width="256" height="256"/>',
        "\t</bitmap>",
        f'\t<bitmap name="{MT}/service_record/header_service_record">',
        f'\t\t<frame png="ce/port/{MT}/service_record/header_service_record.png" width="512" height="64"/>',
        "\t</bitmap>",
    ])
    # Remove any previous copies of those blocks before re-inserting cleanly
    for bm_name in (
        f"{MT}/server_settings/bots_options/header_bots_options",
        f"{MT}/header_standard",
        f"{MT}/header_ranked",
        f"{MT}/header_firefight",
        f"{MT}/header_forge",
        f"{MT}/header_theater",
        f"{MT}/header_creation_hub",
        f"{MT}/header_dedi_server",
        f"{MT}/gamemode/header_gamemode",
        f"{MT}/gamemode/gamemode_options",
        f"{MT}/service_record/header_service_record",
    ):
        bitmaps_text = re.sub(
            rf'\t<bitmap name="{re.escape(bm_name)}">.*?\t</bitmap>\n?',
            "",
            bitmaps_text,
            flags=re.DOTALL,
        )
    bitmaps_text = bitmaps_text.replace("</menus>", f"{extra_bitmaps}\n</menus>")
    bitmaps_xml_path.write_text(bitmaps_text, encoding="utf-8")

    # 3b. Write server_settings.xml, local.xml, new_select.port.xml, and controls_setup.xml
    mp_generated = multiplayer_files()
    for xml_name in (
        f"{MT}/server_settings".replace("/", ".") + ".xml",
        f"{MT}/local".replace("/", ".") + ".xml",
        "main_menu/new_select".replace("/", ".") + ".port.xml",
    ):
        if xml_name in mp_generated:
            (ce_dir / xml_name).write_text("\n".join(mp_generated[xml_name]), encoding="utf-8")
    st_generated = settings_files()
    controls_xml_name = f"{PE}/controls_setup".replace("/", ".") + ".xml"
    if controls_xml_name in st_generated:
        (ce_dir / controls_xml_name).write_text("\n".join(st_generated[controls_xml_name]), encoding="utf-8")

    # 4. Update main_menu.xml and main_menu.profile_manager.xml so Multiplayer opens GAMEMODE
    for menu_file in ("main_menu.xml", "main_menu.profile_manager.xml"):
        menu_path = ce_dir / menu_file
        text = menu_path.read_text(encoding="utf-8")
        text = text.replace(
            f'open="{MT}/multiplayer_type_select_screen"',
            f'open="{MT}/gamemode/gamemode_select_screen"',
        )
        menu_path.write_text(text, encoding="utf-8")

    # 5. Update main_menu.multiplayer_type_select.xml with GAMEMODE, SERVICE RECORD, LOCAL item, and mode headers
    mp_select_path = ce_dir / "main_menu.multiplayer_type_select.xml"
    mp_text = mp_select_path.read_text(encoding="utf-8")
    cut_marker = "\t<!-- GAMEMODE & SERVICE RECORD SCREENS -->"
    if cut_marker in mp_text:
        mp_text = mp_text.split(cut_marker)[0]
    elif "\t<!-- SERVICE RECORD & LEADERBOARDS SCREEN -->" in mp_text:
        mp_text = mp_text.split("\t<!-- SERVICE RECORD & LEADERBOARDS SCREEN -->")[0]
    else:
        mp_text = mp_text.replace("</menus>", "")
    mp_list_replacement = "\n".join([
        '\t<widget name="main_menu/multiplayer_type_select/multiplayer_type_select_list" type="column_list" width="640" height="480" flags="pass_unhandled_to_focused_child up_down_tabs_items" description="main_menu/multiplayer_type_select/multiplayer_type_list_ext_desc">',
        '\t\t<data input="multiplayer type menu update desc"/>',
        '\t\t<data input="mp type screen update"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/multiplayer_type_join_item" x="-32" y="78"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/multiplayer_type_join_internet_item" y="107"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/multiplayer_type_join_lan_item" y="136"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/multiplayer_type_join_direct_item" y="165"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/multiplayer_type_create_item" x="-32" y="194"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/multiplayer_type_create_internet_item" y="223"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/multiplayer_type_create_lan_item" y="252"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/multiplayer_type_create_local_item" y="281"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/multiplayer_type_coop_item" y="310"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/multiplayer_type_gametypes_item" y="358"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/multiplayer_type_button_bar" y="414"/>',
        "\t</widget>",
        '\t<widget name="main_menu/multiplayer_type_select/multiplayer_type_select_screen" width="640" height="480" flags="pass_unhandled_to_focused_child" bitmap="bitmaps/gradient">',
        '\t\t<on event="created" run="mp type menu initialize"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/header_multiplayer"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/header_standard"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/header_ranked"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/header_firefight"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/header_forge"/>',
        '\t\t<child widget="main_menu/multiplayer_type_select/multiplayer_type_select_list"/>',
        '\t\t<child widget="main_menu/blueline" x="64" y="349"/>',
        "\t</widget>\n",
    ])
    mp_text = re.sub(
        r'\t<widget name="main_menu/multiplayer_type_select/multiplayer_type_select_list".*?\t<widget name="main_menu/multiplayer_type_select/multiplayer_type_select_screen".*?\t</widget>\n?',
        mp_list_replacement,
        mp_text,
        flags=re.DOTALL,
    )
    addition = (
        [cut_marker]
        + _gamemode_screen()
        + _forge_screens()
        + _theater_screen()
        + _creation_hub_screen()
        + _dedi_server_screen()
        + _service_record_screen()
        + ["</menus>", ""]
    )
    mp_select_path.write_text(mp_text + "\n".join(addition), encoding="utf-8")

    # 6. Clean up dead var_ff_* string lists and ensure LOCAL is present in strings.xml
    strings_path = ce_dir / "strings.xml"
    if strings_path.is_file():
        strings_text = strings_path.read_text(encoding="utf-8")
        for dead_str in (
            "main_menu/settings_select/multiplayer_setup/playlist_edit/slayer_edit/var_ff_species",
            "main_menu/settings_select/multiplayer_setup/playlist_edit/slayer_edit/var_ff_stats_view",
            "main_menu/settings_select/multiplayer_setup/playlist_edit/slayer_edit/var_ff_stats_action",
        ):
            strings_text = re.sub(
                rf'\t<strings name="{re.escape(dead_str)}"[^>]*>.*?\t</strings>\n?',
                "",
                strings_text,
                flags=re.DOTALL,
            )
        if '<string text="LOCAL"/>' not in strings_text:
            strings_text = strings_text.replace(
                '\t\t<string text="CO-OP CAMPAIGN"/>\n\t</strings>',
                '\t\t<string text="CO-OP CAMPAIGN"/>\n\t\t<string text="LOCAL"/>\n\t</strings>',
            )
        if 'Play on this computer alone:' not in strings_text:
            strings_text = strings_text.replace(
                '\t\t<string text="Host a game on your LAN\\nonly." map="4"/>\n',
                '\t\t<string text="Host a game on your LAN\\nonly." map="4"/>\n'
                '\t\t<string text="Play on this computer alone:\\nsplit screen, and bots in\\nSlayer games."/>\n',
            )
        strings_path.write_text(strings_text, encoding="utf-8")

    # 7. Ensure all generated XMLs and PNGs are registered in port/assets/menus/menus.json["files"]
    import json
    menus_json_path = menus_dir / "menus.json"
    if menus_json_path.is_file():
        manifest = json.loads(menus_json_path.read_text(encoding="utf-8"))
        manifest.pop("pngs", None)
        files_set = set(manifest.get("files", []))
        for new_entry in (
            "ce/main_menu.multiplayer_type_select.local.xml",
            "ce/main_menu.settings_select.multiplayer_setup.opence_options_edit.port.xml",
            "ce/port/bitmaps/game_type_grafix__6.png",
            "ce/port/gamemode/firefight_logo__0.png",
            "ce/port/gamemode/ranked_logo__0.png",
            "ce/port/gamemode/forge_logo__0.png",
            "ce/port/gamemode/theater_logo__0.png",
            "ce/port/gamemode/creation_hub_logo__0.png",
            "ce/port/gamemode/dedi_server_logo__0.png",
            f"ce/port/{MT}/server_settings/bots_options/header_bots_options.png",
            f"ce/port/{MT}/header_standard.png",
            f"ce/port/{MT}/header_ranked.png",
            f"ce/port/{MT}/header_firefight.png",
            f"ce/port/{MT}/header_forge.png",
            f"ce/port/{MT}/header_theater.png",
            f"ce/port/{MT}/header_creation_hub.png",
            f"ce/port/{MT}/header_dedi_server.png",
            f"ce/port/{MT}/gamemode/header_gamemode.png",
            f"ce/port/{MT}/service_record/header_service_record.png",
        ):
            files_set.add(new_entry)
        xml_entries = sorted(f for f in files_set if f.endswith(".xml"))
        other_entries = sorted(f for f in files_set if not f.endswith(".xml"))
        manifest["files"] = xml_entries + other_entries
        menus_json_path.write_text(json.dumps(manifest, indent=1) + "\n", encoding="utf-8")


if __name__ == "__main__":
    generate_firefight_menu_assets()

