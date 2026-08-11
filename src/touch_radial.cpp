#include "touch_radial.h"

#include <algorithm>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "input_context.h"

namespace touch_ui
{
namespace
{

bool contains_action( const std::vector<std::string> &actions, const std::string &action )
{
    return std::find( actions.begin(), actions.end(), action ) != actions.end();
}

bool is_touch_native_navigation_action( const std::string &action )
{
    static const std::unordered_set<std::string> navigation_actions = {
        "UP", "DOWN", "LEFT", "RIGHT",
        "LEFTUP", "RIGHTUP", "LEFTDOWN", "RIGHTDOWN",
        "SCROLL_UP", "SCROLL_DOWN", "PAGE_UP", "PAGE_DOWN", "HOME", "END",
        "CONFIRM", "QUIT", "ANY_INPUT", "COORDINATE", "TIMEOUT",
        "HELP_KEYBINDINGS", "toggle_language_to_en"
    };
    return navigation_actions.count( action ) != 0;
}

bool action_is_family_candidate( const std::string &action )
{
    for( const radial_action_family &family : default_radial_families() ) {
        if( contains_action( family.candidates, action ) ) {
            return true;
        }
    }
    return false;
}

std::string prettify_action_id( const std::string &action )
{
    std::string label = action;
    std::replace( label.begin(), label.end(), '_', ' ' );
    bool capitalize = true;
    for( char &c : label ) {
        if( capitalize && c >= 'a' && c <= 'z' ) {
            c = static_cast<char>( c - 'a' + 'A' );
        }
        capitalize = c == ' ';
    }
    return label;
}

} // namespace

const std::vector<radial_action_family> &default_radial_families()
{
    // Keep this list intentionally small and spatially stable.  It is a touch
    // vocabulary, not a second copy of the keybindings screen.
    static const std::vector<radial_action_family> families = {
        { "interact", "Interact", "touch_interact",
          { "examine_and_pickup", "examine", "open", "close", "grab", "chat", "peek" } },
        { "pickup", "Pick up", "touch_pickup", { "pickup", "pickup_all" } },
        { "inventory", "Inventory", "touch_inventory", { "inventory", "advinv" } },
        { "use", "Use", "touch_use", { "apply_wielded", "apply", "insert" } },
        { "combat", "Combat", "touch_combat",
          { "fire", "fire_burst", "autoattack", "throw_wielded", "throw" } },
        { "reload", "Reload", "touch_reload",
          { "reload_wielded", "reload_weapon", "reload_item", "unload" } },
        { "movement", "Movement", "touch_movement",
          { "open_movement", "cycle_move", "toggle_run", "toggle_crouch", "toggle_prone" } },
        { "wait", "Wait", "touch_wait", { "wait", "pause" } },
        { "consume", "Consume", "touch_consume", { "open_consume", "eat" } },
        { "craft", "Craft", "touch_craft", { "craft", "recraft", "long_craft", "construct" } },
        { "map", "Map", "touch_map", { "map" } },
        { "character", "Character", "touch_character",
          { "player_data", "bodystatus", "medical", "morale", "bionics", "mutations" } },
        { "vehicle", "Vehicle", "touch_vehicle", { "control_vehicle" } },
        { "zones", "Zones", "touch_zones", { "zones", "loot" } },
        { "drop", "Drop", "touch_drop", { "drop", "drop_adj" } },
        { "wear", "Wear", "touch_wear", { "wear", "take_off", "sort_armor" } }
    };
    return families;
}

std::optional<radial_action> resolve_radial_family(
    const radial_action_family &family,
    const std::vector<std::string> &available_actions )
{
    for( const std::string &candidate : family.candidates ) {
        if( contains_action( available_actions, candidate ) ) {
            return radial_action{ family.semantic_id, family.label, family.icon_id, candidate };
        }
    }
    return std::nullopt;
}

std::optional<radial_action> resolve_radial_family(
    const radial_action_family &family,
    const input_context &context )
{
    for( const std::string &candidate : family.candidates ) {
        if( context.is_registered_action( candidate ) ) {
            return radial_action{ family.semantic_id, family.label, family.icon_id, candidate };
        }
    }
    return std::nullopt;
}

std::string fallback_icon_for_action( const std::string &action_id )
{
    if( action_id.find( "fire" ) != std::string::npos || action_id.find( "attack" ) != std::string::npos ) {
        return "touch_combat";
    }
    if( action_id.find( "reload" ) != std::string::npos || action_id.find( "ammo" ) != std::string::npos ) {
        return "touch_reload";
    }
    if( action_id.find( "inventory" ) != std::string::npos || action_id.find( "item" ) != std::string::npos ) {
        return "touch_inventory";
    }
    if( action_id.find( "map" ) != std::string::npos ) {
        return "touch_map";
    }
    if( action_id.find( "vehicle" ) != std::string::npos ) {
        return "touch_vehicle";
    }
    if( action_id.find( "craft" ) != std::string::npos ) {
        return "touch_craft";
    }
    return "touch_action";
}

std::vector<radial_action> build_radial_actions(
    const std::vector<std::string> &available_actions,
    const std::size_t max_items )
{
    std::vector<radial_action> result;
    if( max_items == 0 ) {
        return result;
    }

    result.reserve( std::min( max_items, available_actions.size() ) );
    std::set<std::string> consumed_actions;

    for( const radial_action_family &family : default_radial_families() ) {
        const std::optional<radial_action> resolved = resolve_radial_family( family, available_actions );
        if( !resolved ) {
            continue;
        }
        result.push_back( *resolved );
        consumed_actions.insert( resolved->action_id );
        if( result.size() == max_items ) {
            return result;
        }
    }

    // Preserve uncommon/context-specific actions instead of making the radial
    // useless in bespoke screens.  They get a generic icon until explicitly
    // promoted into a semantic family.
    for( const std::string &action : available_actions ) {
        if( consumed_actions.count( action ) != 0 || is_touch_native_navigation_action( action ) ||
            action_is_family_candidate( action ) ) {
            continue;
        }
        result.push_back( radial_action{
            action,
            prettify_action_id( action ),
            fallback_icon_for_action( action ),
            action
        } );
        if( result.size() == max_items ) {
            break;
        }
    }

    return result;
}

std::vector<radial_action> build_radial_actions(
    const input_context &context,
    const std::size_t max_items )
{
    std::vector<radial_action> result;
    if( max_items == 0 ) {
        return result;
    }

    result.reserve( max_items );
    for( const radial_action_family &family : default_radial_families() ) {
        const std::optional<radial_action> resolved = resolve_radial_family( family, context );
        if( !resolved ) {
            continue;
        }
        result.push_back( *resolved );
        if( result.size() == max_items ) {
            break;
        }
    }
    return result;
}

} // namespace touch_ui
