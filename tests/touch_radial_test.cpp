#include "cata_catch.h"
#include "touch_input.h"
#include "touch_radial.h"

#include <algorithm>
#include <string>
#include <vector>

TEST_CASE( "touch_radial_collapses_related_actions", "[touch][radial]" )
{
    const std::vector<std::string> actions = {
        "reload_item", "reload_weapon", "reload_wielded", "inventory", "UP", "DOWN"
    };
    const std::vector<touch_ui::radial_action> radial = touch_ui::build_radial_actions( actions );
    const auto reload = std::find_if( radial.begin(), radial.end(), []( const touch_ui::radial_action &item ) {
        return item.semantic_id == "reload";
    } );
    REQUIRE( reload != radial.end() );
    CHECK( reload->action_id == "reload_wielded" );
    CHECK( reload->icon_id == "touch_reload" );
    CHECK( std::count_if( radial.begin(), radial.end(), []( const touch_ui::radial_action &item ) {
        return item.semantic_id == "reload";
    } ) == 1 );
}

TEST_CASE( "touch_radial_semantic_action_changes_with_context", "[touch][radial]" )
{
    const touch_ui::radial_action_family interact = {
        "interact", "Interact", "touch_interact", { "examine", "open", "close" }
    };
    const auto open_context = touch_ui::resolve_radial_family( interact, { "open", "inventory" } );
    REQUIRE( open_context );
    CHECK( open_context->semantic_id == "interact" );
    CHECK( open_context->action_id == "open" );
    CHECK( open_context->icon_id == "touch_interact" );
    const auto close_context = touch_ui::resolve_radial_family( interact, { "close", "inventory" } );
    REQUIRE( close_context );
    CHECK( close_context->action_id == "close" );
}

TEST_CASE( "touch_radial_prefers_existing_interact_umbrella_action", "[touch][radial]" )
{
    const std::vector<touch_ui::radial_action> radial = touch_ui::build_radial_actions(
                { "interact", "open", "close", "examine" } );
    const auto interact = std::find_if( radial.begin(), radial.end(), []( const touch_ui::radial_action &item ) {
        return item.semantic_id == "interact";
    } );
    REQUIRE( interact != radial.end() );
    CHECK( interact->action_id == "interact" );
}

TEST_CASE( "touch_radial_omits_touch_native_navigation", "[touch][radial]" )
{
    const std::vector<touch_ui::radial_action> radial = touch_ui::build_radial_actions(
                { "UP", "DOWN", "LEFT", "RIGHT", "inventory" } );
    REQUIRE( radial.size() == 1 );
    CHECK( radial.front().semantic_id == "items" );
}

TEST_CASE( "touch_radial_has_icon_fallback_and_slot_limit", "[touch][radial]" )
{
    const std::vector<touch_ui::radial_action> radial = touch_ui::build_radial_actions(
    { "inventory", "apply", "fire", "reload_weapon", "open_movement", "wait", "craft", "map",
      "missions", "messages", "custom_context_action" }, 8 );
    REQUIRE( radial.size() == 8 );
    CHECK( std::all_of( radial.begin(), radial.end(), []( const touch_ui::radial_action &item ) {
        return !item.icon_id.empty();
    } ) );
    CHECK( touch_ui::fallback_icon_for_action( "custom_context_action" ) == "touch_action" );
}

TEST_CASE( "touch_gestures_defer_until_intent_is_known", "[touch][gesture]" )
{
    touch_ui::gesture_router router;
    router.finger_down( 1, 0.5f, 0.5f, 100 );
    CHECK( router.state() == touch_ui::touch_gesture_state::pending );
    CHECK_FALSE( touch_ui::pop_semantic_input().has_value() );

    router.finger_motion( 1, 0.505f, 0.505f, 250 );
    CHECK( router.state() == touch_ui::touch_gesture_state::pending );
    router.finger_motion( 1, 0.505f, 0.505f, 450 );
    CHECK( router.state() == touch_ui::touch_gesture_state::radial );
}

TEST_CASE( "second_finger_promotes_pending_touch_to_pan_or_pinch", "[touch][gesture]" )
{
    touch_ui::gesture_router router;
    router.finger_down( 1, 0.3f, 0.5f, 100 );
    router.finger_down( 2, 0.7f, 0.5f, 120 );
    CHECK( router.state() == touch_ui::touch_gesture_state::pan );
    router.finger_motion( 2, 0.8f, 0.5f, 140 );
    CHECK( router.state() == touch_ui::touch_gesture_state::pinch );
    const auto zoom = touch_ui::pop_semantic_input();
    REQUIRE( zoom );
    CHECK( zoom->kind == touch_ui::semantic_input_kind::zoom );
}

TEST_CASE( "touch_layouts_are_normalized_and_adaptive", "[touch][layout]" )
{
    const touch_ui::touch_layout phone = touch_ui::default_touch_layout( touch_ui::touch_device_class::phone );
    const touch_ui::touch_layout tablet = touch_ui::default_touch_layout( touch_ui::touch_device_class::tablet );
    CHECK( phone.movement_anchor.x >= 0.0f );
    CHECK( phone.radial_anchor.x <= 1.0f );
    CHECK_FALSE( phone.persistent_hud );
    CHECK( tablet.persistent_hud );
    CHECK( tablet.control_scale > phone.control_scale );
}

TEST_CASE( "controller_input_hides_touch_hud", "[touch][device]" )
{
    touch_ui::note_input_device( touch_ui::input_device::touch, 100 );
    CHECK( touch_ui::touch_hud_visible() );
    touch_ui::note_input_device( touch_ui::input_device::controller, 200 );
    CHECK_FALSE( touch_ui::touch_hud_visible() );
}
