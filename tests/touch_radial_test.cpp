#include "cata_catch.h"
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
    CHECK( close_context->semantic_id == "interact" );
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
    CHECK( radial.front().action_id == "inventory" );
}

TEST_CASE( "touch_radial_has_icon_fallback_and_slot_limit", "[touch][radial]" )
{
    const std::vector<touch_ui::radial_action> radial = touch_ui::build_radial_actions(
    {
        "inventory", "apply", "fire", "reload_weapon", "open_movement", "wait", "craft", "map",
        "missions", "messages", "custom_context_action"
    }, 8 );

    REQUIRE( radial.size() == 8 );
    CHECK( std::all_of( radial.begin(), radial.end(), []( const touch_ui::radial_action &item ) {
        return !item.icon_id.empty();
    } ) );

    CHECK( touch_ui::fallback_icon_for_action( "custom_context_action" ) == "touch_action" );
    CHECK( touch_ui::fallback_icon_for_action( "special_vehicle_action" ) == "touch_vehicle" );
}
