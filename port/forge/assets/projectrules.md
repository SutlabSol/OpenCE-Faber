# OpenCE : Faber Forge — Project Rules

The rules Forge code (`port/forge/`) follows, on top of the repository's
[AGENTS.md](../../../AGENTS.md). Where they disagree, AGENTS.md wins.

---

## 1. Where Forge code lives

1. **Forge code lives in `port/forge/`.** Sources in `src/`, headers in
   `include/`, the stand-alone tool in `tools/`, and what the Windows package
   copies into `Faber_data/` in `assets/`.
2. **Changes outside `port/forge/` are kept to the hooks Forge needs** and
   follow the repository's rules: a change in `source/` is marked with a
   `port:` comment; a name the shared code calls must also exist on the
   builds without Forge (`port/linux/game/forge_fallbacks.c`); symbol
   renames go in `FORGE_OBJCOPY_REDEFINES` (`tools/windows_build.py`).
3. **Platforms.** Forge is built into the Windows build (`OpenCE - Faber.exe`,
   identical to `halo.exe`). The Linux, Android and web builds must still
   build and link without it. Bringing Forge to another platform starts with
   replacing `GetAsyncKeyState` with the engine's input.
4. **Runtime files.** Faber-only files go in `Faber_data/` beside the
   executable (see [README.md](README.md)); the shared OpenCE files
   (`maps/`, `custom_maps/`, `saves/`, `config.toml`) are read in place and
   never moved.
5. **No retail data in the repository.** No `.map` files, and no tags
   extracted from them; the local tag cache is built from the user's own maps
   on first use. `maps/`, `custom_maps/` and `Faber_data/` stay in
   `.gitignore`.

---

## 2. Tag scope

1. **Root tag classes cached and cloned:**
   - Bipeds (`bipd`), with their actor variants (`actv`), actors (`actr`) and
     animations (`antr`).
   - Vehicles (`vehi`).
   - Weapons (`weap`), and `eqip` / `itmc` for grenades and collections.
   - Scenery (`scen`), and the devices `mach`, `ctrl`, `lifi`.
   - Their dependencies: `mod2`, `coll`, `phys`, `jpt!`, `proj`, `effe`,
     shaders (`shdr`, `soso`, `schi`, `scex`, `sgla`, `senv`, `spla`, `swat`,
     `smet`), `bitm`, `snd!`.
2. **Never extract structure BSPs** (`sbsp`), level geometry, terrain or
   lightmaps from donor maps.
3. **Resource maps.** Output is the Custom Edition cache format (version
   609). Tags whose data is in Custom Edition's `bitmaps.map`, `sounds.map`
   or `loc.map` keep their external links; tags from Halo PC maps (version
   7) whose pixels or samples are not there have them internalised into the
   `.map` and the external flag cleared (`bitmap_data.flags &= ~0x100`,
   `samples.flags &= ~1`).
4. **Map files are untrusted input**: read them through bounds-checked
   readers, as the rest of the port does.
5. **Attribution.** Every root tag extracted or injected is logged in
   `Faber_data/credits.md` with its class and path, its source map, and
   `*<map file name>* and its original creator` (from the map's `.txt` or
   readme when there is one).

---

## 3. "AI" in Forge

1. "AI" means the engine's own NPC actors (Marines, Crewmen, Elites, Grunts,
   Jackals, Hunters, Flood, Sentinels) run by `source/ai/`, not external AI
   agents or language models.
2. Placed AI is **Friendly** (`_game_team_player` / `_game_team_human`) or
   **Enemy** (`_game_team_covenant` / `_game_team_flood` /
   `_game_team_sentinel`).
3. In the 3D mode, placed AI starts at once (`actor_create_for_unit` +
   `actor_customize_unit`, `force_active = TRUE`). On export it is written to
   `scenario->ai_actor_palette` and `scenario->ai_encounters` (squads,
   starting locations, cluster-linked firing positions), so a stock build
   runs it.

---

## 4. Multiplayer tags and map sharing

1. **Live multiplayer / co-op Forge** offers only the shared base tags, so
   every client has the same tags; tags added through TAG OPTIONS are
   single-player only.
2. **Single-player Forge** offers every cached and added tag.
3. **Export** (`F5`) bakes every placed tag and its dependencies into
   `custom_maps/<name>.map`, so the map needs nothing else; OpenCE's map
   sharing (`map_share.c`) sends it to players who lack it.

---

## 5. Session isolation

1. Forge sessions use `FORGE_NETWORK_VERSION` (`forge_engine.h`) while
   `g_forge_session_enabled` is set; everything else uses OpenCE's
   `HALO_PORT_NETWORK_VERSION`. Forge and stock games therefore never join
   each other.
2. Raise `FORGE_NETWORK_VERSION` with any change to what Forge clients send
   each other or must agree on, and say why in the commit.
3. Anything another machine sends is untrusted: check sizes, indices and
   rates, as the port's network handlers do.
4. Exported maps hosted as normal games are plain Custom Edition maps and
   play with stock OpenCE.
