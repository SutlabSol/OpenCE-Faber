"""Modular Halo: Reach / ODST Firefight engine regression tests."""

import re
import pytest

from tools.harness import CHECK_FAILED, build, function, mutated, read, run

HEADER = re.sub(
    r"#include\s+.*|struct\s+game_variant\s*;|struct\s+game_variant_options\s*;|extern\s+struct\s+game_variant\s+global_variant\s*;.*",
    "",
    read("port/linux/include/halo_firefight.h"),
)
FF_CORE = read("port/linux/game/firefight.c")
FF_SPAWNER = read("port/linux/game/firefight_spawner.c")
FF_GEN = read("port/linux/game/firefight_generators.c")
FF_NET = read("port/linux/game/firefight_network.c")

CORE_FUNCTIONS = (
    "firefight_map_reach_skull_to_40_skull",
    "firefight_active",
    "firefight_globals_get",
    "firefight_globals_mutable",
    "firefight_active_skulls_get",
    "firefight_skull_active",
    "firefight_40_skull_active",
    "firefight_compute_set_vitality_scale",
    "firefight_recalculate_skulls_and_multiplier",
    "firefight_score_multiplier_get",
    "firefight_set_vitality_scale_get",
    "firefight_enemy_vitality_scale",
    "firefight_announce_banner",
    "firefight_elite_mode_active",
    "firefight_setup_faction_allegiances",
    "firefight_resolve_player_biped_tag",
    "firefight_add_shared_lives",
    "firefight_count_living_players",
    "firefight_player_out_of_lives",
    "firefight_spectating_active",
    "firefight_should_end_game",
    "firefight_prep_countdown_ticks",
    "firefight_wave_timer_limit_ticks",
    "firefight_update_round_and_set_skulls",
    "firefight_begin_pre_round",
    "firefight_begin_wave",
    "firefight_notify_actor_or_unit_killed",
    "firefight_variant_options_default",
    "firefight_player_killed_player",
    "firefight_apply_tick_player_traits",
    "firefight_update",
)

SPAWNER_FUNCTIONS = (
    "lowercase_copy",
    "firefight_unit_toggle_enabled",
    "faction_relevant_unit_mask",
    "firefight_validate_unit_toggles",
    "classify_actor_variant",
)

GEN_FUNCTIONS = (
    "firefight_is_generator_object",
    "firefight_apply_generator_damage",
    "firefight_try_trigger_generator_lockdown",
)

NET_FUNCTIONS = (
    "firefight_write_network_state",
    "firefight_read_network_state",
    "game_engine_firefight_write_network_state",
    "game_engine_firefight_read_network_state",
)


def extract_all():
    parts = []
    for name in CORE_FUNCTIONS:
        parts.append(function(FF_CORE, name))
    for name in SPAWNER_FUNCTIONS:
        parts.append(function(FF_SPAWNER, name))
    for name in GEN_FUNCTIONS:
        parts.append(function(FF_GEN, name))
    for name in NET_FUNCTIONS:
        parts.append(function(FF_NET, name))
    return "\n\n".join(parts)


def make(code=None):
    if code is None:
        code = extract_all()
    return build(
        "firefight",
        (
            ("firefight_header.inc", HEADER),
            ("under_test.inc", code),
        ),
    )


@pytest.mark.parametrize(
    "case",
    (
        "abi_and_variant_size",
        "skull_multipliers_and_progression",
        "wave_round_set_progression",
        "shared_lives_respawn_and_spectate",
        "generator_defense_and_lockdown",
        "scoring_sprees_multikills_and_network_sync",
        "unit_toggle_validation_and_faction_diplomacy",
    ),
)
def test_firefight_engine(case):
    status, output = run(make(), case)
    assert status == 0, output


@pytest.mark.parametrize(
    "case,fault",
    (
        (
            "skull_multipliers_and_progression",
            (
                "case _firefight_escalation_medium:  per_set_bonus = 0.25f; break;",
                "case _firefight_escalation_medium:  per_set_bonus = 0.00f; break;",
            ),
        ),
        (
            "wave_round_set_progression",
            (
                "ff_state.enemies_alive <= FIREFIGHT_REINFORCEMENT_THRESHOLD",
                "ff_state.enemies_alive == 0",
            ),
        ),
        (
            "generator_defense_and_lockdown",
            (
                "if (!gen->alive || gen->lockdown_ticks > 0)",
                "if (!gen->alive)",
            ),
        ),
        (
            "unit_toggle_validation_and_faction_diplomacy",
            (
                "return (candidate_unit_mask & relevant) != 0;",
                "return TRUE;",
            ),
        ),
    ),
)
def test_firefight_negative_controls(case, fault):
    code = extract_all()
    status, _ = run(make(mutated(code, *fault)), case)
    assert status == CHECK_FAILED
