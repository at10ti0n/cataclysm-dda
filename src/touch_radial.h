#pragma once
#ifndef CATA_SRC_TOUCH_RADIAL_H
#define CATA_SRC_TOUCH_RADIAL_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

class input_context;

namespace touch_ui
{

/**
 * A semantic radial slot presented to a touch user.
 *
 * action_id is still a normal CDDA input action descriptor.  The semantic_id
 * and icon_id are presentation concepts only; gameplay never needs to know
 * about them.
 */
struct radial_action {
    std::string semantic_id;
    std::string label;
    std::string icon_id;
    std::string action_id;
};

/**
 * One stable touch intent can stand in for several legacy/key-oriented actions.
 * The first candidate registered by the active input_context wins.
 */
struct radial_action_family {
    std::string semantic_id;
    std::string label;
    std::string icon_id;
    std::vector<std::string> candidates;
};

/** Default ordered touch vocabulary.  Earlier families occupy radial slots first. */
const std::vector<radial_action_family> &default_radial_families();

/** Resolve one semantic family against action descriptors available in a context. */
std::optional<radial_action> resolve_radial_family(
    const radial_action_family &family,
    const std::vector<std::string> &available_actions );

/** Resolve one semantic family directly against an active CDDA input context. */
std::optional<radial_action> resolve_radial_family(
    const radial_action_family &family,
    const input_context &context );

/**
 * Build a compact radial from a list of registered actions.  Direction/navigation
 * actions are intentionally omitted because touch movement and list navigation
 * should be handled directly by gestures.
 */
std::vector<radial_action> build_radial_actions(
    const std::vector<std::string> &available_actions,
    std::size_t max_items = 8 );

/**
 * Build the semantic radial directly from the current input_context.  This is
 * the bridge the SDL touch layer uses; it does not require platform-specific
 * access to input_context internals.
 */
std::vector<radial_action> build_radial_actions(
    const input_context &context,
    std::size_t max_items = 8 );

/** Stable icon identifier for a raw action that did not match a semantic family. */
std::string fallback_icon_for_action( const std::string &action_id );

/**
 * SDL/ImGui integration hooks for the prototype radial overlay.
 *
 * Desktop testing uses press-and-hold of the right mouse button.  These hooks
 * are intentionally input-device agnostic at the semantic layer so iOS finger
 * gestures can drive the same overlay later.
 */
bool radial_overlay_wants_frame();
bool radial_overlay_should_capture_mouse();
void draw_radial_overlay();

} // namespace touch_ui

#endif // CATA_SRC_TOUCH_RADIAL_H
