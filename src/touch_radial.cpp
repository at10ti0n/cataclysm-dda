#include "touch_radial.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "input_context.h"

#if defined(TILES)
#include <imgui/imgui.h>

#include "input.h"
#include "sdl_wrappers.h"
#endif

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

#if defined(TILES)

extern input_event last_input;

constexpr float pi = 3.14159265358979323846f;
constexpr float radial_deadzone = 38.0f;
constexpr float radial_radius = 116.0f;
constexpr float radial_slot_radius = 31.0f;

struct radial_overlay_state {
    bool open = false;
    ImVec2 center = ImVec2( 0.0f, 0.0f );
    int selected = -1;
    std::vector<radial_action> actions;
};

radial_overlay_state &overlay_state()
{
    static radial_overlay_state state;
    return state;
}

input_context *active_input_context()
{
    return input_context::input_context_stack.back();
}

bool right_mouse_is_down()
{
#if SDL_MAJOR_VERSION >= 3
    float x = 0.0f;
    float y = 0.0f;
    const SDL_MouseButtonFlags buttons = SDL_GetMouseState( &x, &y );
#else
    int x = 0;
    int y = 0;
    const Uint32 buttons = SDL_GetMouseState( &x, &y );
#endif
    return ( buttons & SDL_BUTTON_RMASK ) != 0;
}

ImVec2 clamp_radial_center( ImVec2 center, const ImVec2 display_size )
{
    const float margin = radial_radius + radial_slot_radius + 8.0f;
    if( display_size.x > margin * 2.0f ) {
        center.x = std::clamp( center.x, margin, display_size.x - margin );
    }
    if( display_size.y > margin * 2.0f ) {
        center.y = std::clamp( center.y, margin, display_size.y - margin );
    }
    return center;
}

void line_icon( ImDrawList *draw, const ImVec2 &a, const ImVec2 &b, ImU32 color, float thickness = 2.3f )
{
    draw->AddLine( a, b, color, thickness );
}

void draw_touch_icon( ImDrawList *draw, const std::string &icon_id, const ImVec2 center,
                      const float size, const ImU32 color )
{
    const float r = size * 0.5f;
    if( icon_id == "touch_interact" ) {
        draw->AddCircle( center, r * 0.54f, color, 20, 2.5f );
        line_icon( draw, ImVec2( center.x + r * 0.38f, center.y + r * 0.38f ),
                   ImVec2( center.x + r * 0.82f, center.y + r * 0.82f ), color, 2.5f );
        draw->AddCircleFilled( center, r * 0.12f, color );
    } else if( icon_id == "touch_pickup" ) {
        line_icon( draw, ImVec2( center.x, center.y - r * 0.72f ),
                   ImVec2( center.x, center.y + r * 0.42f ), color );
        line_icon( draw, ImVec2( center.x, center.y + r * 0.42f ),
                   ImVec2( center.x - r * 0.42f, center.y ), color );
        line_icon( draw, ImVec2( center.x, center.y + r * 0.42f ),
                   ImVec2( center.x + r * 0.42f, center.y ), color );
        draw->AddRect( ImVec2( center.x - r * 0.58f, center.y + r * 0.5f ),
                       ImVec2( center.x + r * 0.58f, center.y + r * 0.78f ), color, 2.0f, 0, 2.0f );
    } else if( icon_id == "touch_inventory" ) {
        draw->AddRect( ImVec2( center.x - r * 0.66f, center.y - r * 0.45f ),
                       ImVec2( center.x + r * 0.66f, center.y + r * 0.7f ), color, 4.0f, 0, 2.3f );
        draw->AddBezierCubic( ImVec2( center.x - r * 0.35f, center.y - r * 0.45f ),
                              ImVec2( center.x - r * 0.35f, center.y - r * 0.92f ),
                              ImVec2( center.x + r * 0.35f, center.y - r * 0.92f ),
                              ImVec2( center.x + r * 0.35f, center.y - r * 0.45f ), color, 2.3f );
    } else if( icon_id == "touch_use" ) {
        draw->AddCircle( ImVec2( center.x - r * 0.26f, center.y + r * 0.2f ), r * 0.3f, color, 16, 2.2f );
        line_icon( draw, ImVec2( center.x - r * 0.04f, center.y - r * 0.02f ),
                   ImVec2( center.x + r * 0.66f, center.y - r * 0.72f ), color, 3.0f );
        line_icon( draw, ImVec2( center.x + r * 0.36f, center.y - r * 0.42f ),
                   ImVec2( center.x + r * 0.7f, center.y - r * 0.08f ), color, 2.2f );
    } else if( icon_id == "touch_combat" ) {
        line_icon( draw, ImVec2( center.x - r * 0.58f, center.y + r * 0.58f ),
                   ImVec2( center.x + r * 0.48f, center.y - r * 0.48f ), color, 3.2f );
        draw->AddTriangleFilled( ImVec2( center.x + r * 0.72f, center.y - r * 0.72f ),
                                 ImVec2( center.x + r * 0.24f, center.y - r * 0.5f ),
                                 ImVec2( center.x + r * 0.5f, center.y - r * 0.24f ), color );
        line_icon( draw, ImVec2( center.x - r * 0.72f, center.y + r * 0.34f ),
                   ImVec2( center.x - r * 0.34f, center.y + r * 0.72f ), color, 3.0f );
    } else if( icon_id == "touch_reload" ) {
        draw->PathArcTo( center, r * 0.68f, -0.35f * pi, 1.25f * pi, 28 );
        draw->PathStroke( color, 0, 2.6f );
        draw->AddTriangleFilled( ImVec2( center.x - r * 0.72f, center.y - r * 0.16f ),
                                 ImVec2( center.x - r * 0.34f, center.y - r * 0.1f ),
                                 ImVec2( center.x - r * 0.58f, center.y + r * 0.2f ), color );
    } else if( icon_id == "touch_movement" ) {
        draw->AddTriangleFilled( ImVec2( center.x, center.y - r * 0.78f ),
                                 ImVec2( center.x - r * 0.24f, center.y - r * 0.34f ),
                                 ImVec2( center.x + r * 0.24f, center.y - r * 0.34f ), color );
        draw->AddTriangleFilled( ImVec2( center.x + r * 0.78f, center.y ),
                                 ImVec2( center.x + r * 0.34f, center.y - r * 0.24f ),
                                 ImVec2( center.x + r * 0.34f, center.y + r * 0.24f ), color );
        draw->AddTriangleFilled( ImVec2( center.x, center.y + r * 0.78f ),
                                 ImVec2( center.x - r * 0.24f, center.y + r * 0.34f ),
                                 ImVec2( center.x + r * 0.24f, center.y + r * 0.34f ), color );
        draw->AddTriangleFilled( ImVec2( center.x - r * 0.78f, center.y ),
                                 ImVec2( center.x - r * 0.34f, center.y - r * 0.24f ),
                                 ImVec2( center.x - r * 0.34f, center.y + r * 0.24f ), color );
    } else if( icon_id == "touch_wait" ) {
        draw->AddCircle( center, r * 0.68f, color, 24, 2.4f );
        line_icon( draw, center, ImVec2( center.x, center.y - r * 0.42f ), color );
        line_icon( draw, center, ImVec2( center.x + r * 0.34f, center.y + r * 0.2f ), color );
    } else if( icon_id == "touch_consume" ) {
        line_icon( draw, ImVec2( center.x - r * 0.45f, center.y - r * 0.7f ),
                   ImVec2( center.x - r * 0.45f, center.y + r * 0.7f ), color, 2.5f );
        line_icon( draw, ImVec2( center.x - r * 0.68f, center.y - r * 0.7f ),
                   ImVec2( center.x - r * 0.68f, center.y - r * 0.12f ), color, 1.8f );
        line_icon( draw, ImVec2( center.x - r * 0.22f, center.y - r * 0.7f ),
                   ImVec2( center.x - r * 0.22f, center.y - r * 0.12f ), color, 1.8f );
        draw->AddCircle( ImVec2( center.x + r * 0.34f, center.y - r * 0.14f ), r * 0.28f, color, 16, 2.2f );
        line_icon( draw, ImVec2( center.x + r * 0.34f, center.y + r * 0.14f ),
                   ImVec2( center.x + r * 0.34f, center.y + r * 0.7f ), color, 2.5f );
    } else if( icon_id == "touch_craft" ) {
        line_icon( draw, ImVec2( center.x - r * 0.58f, center.y + r * 0.62f ),
                   ImVec2( center.x + r * 0.5f, center.y - r * 0.46f ), color, 3.0f );
        draw->AddCircle( ImVec2( center.x + r * 0.52f, center.y - r * 0.48f ), r * 0.24f, color, 14, 2.2f );
        line_icon( draw, ImVec2( center.x - r * 0.7f, center.y - r * 0.18f ),
                   ImVec2( center.x - r * 0.12f, center.y + r * 0.4f ), color, 2.5f );
    } else if( icon_id == "touch_map" ) {
        const ImVec2 a( center.x - r * 0.72f, center.y - r * 0.58f );
        const ImVec2 b( center.x - r * 0.22f, center.y - r * 0.72f );
        const ImVec2 c( center.x + r * 0.24f, center.y - r * 0.52f );
        const ImVec2 d( center.x + r * 0.72f, center.y - r * 0.68f );
        line_icon( draw, a, ImVec2( a.x, center.y + r * 0.62f ), color );
        line_icon( draw, b, ImVec2( b.x, center.y + r * 0.48f ), color );
        line_icon( draw, c, ImVec2( c.x, center.y + r * 0.68f ), color );
        line_icon( draw, d, ImVec2( d.x, center.y + r * 0.5f ), color );
        line_icon( draw, a, b, color );
        line_icon( draw, b, c, color );
        line_icon( draw, c, d, color );
        line_icon( draw, ImVec2( a.x, center.y + r * 0.62f ), ImVec2( b.x, center.y + r * 0.48f ), color );
        line_icon( draw, ImVec2( b.x, center.y + r * 0.48f ), ImVec2( c.x, center.y + r * 0.68f ), color );
        line_icon( draw, ImVec2( c.x, center.y + r * 0.68f ), ImVec2( d.x, center.y + r * 0.5f ), color );
    } else if( icon_id == "touch_character" ) {
        draw->AddCircle( ImVec2( center.x, center.y - r * 0.38f ), r * 0.28f, color, 18, 2.4f );
        draw->PathArcTo( ImVec2( center.x, center.y + r * 0.72f ), r * 0.7f, 1.08f * pi, 1.92f * pi, 20 );
        draw->PathStroke( color, 0, 2.5f );
    } else if( icon_id == "touch_vehicle" ) {
        draw->AddRect( ImVec2( center.x - r * 0.72f, center.y - r * 0.26f ),
                       ImVec2( center.x + r * 0.72f, center.y + r * 0.42f ), color, 4.0f, 0, 2.3f );
        line_icon( draw, ImVec2( center.x - r * 0.42f, center.y - r * 0.26f ),
                   ImVec2( center.x - r * 0.14f, center.y - r * 0.62f ), color );
        line_icon( draw, ImVec2( center.x - r * 0.14f, center.y - r * 0.62f ),
                   ImVec2( center.x + r * 0.42f, center.y - r * 0.62f ), color );
        line_icon( draw, ImVec2( center.x + r * 0.42f, center.y - r * 0.62f ),
                   ImVec2( center.x + r * 0.58f, center.y - r * 0.26f ), color );
        draw->AddCircleFilled( ImVec2( center.x - r * 0.42f, center.y + r * 0.46f ), r * 0.17f, color );
        draw->AddCircleFilled( ImVec2( center.x + r * 0.42f, center.y + r * 0.46f ), r * 0.17f, color );
    } else if( icon_id == "touch_zones" ) {
        draw->AddRect( ImVec2( center.x - r * 0.7f, center.y - r * 0.7f ),
                       ImVec2( center.x + r * 0.7f, center.y + r * 0.7f ), color, 2.0f, 0, 2.2f );
        line_icon( draw, ImVec2( center.x, center.y - r * 0.7f ), ImVec2( center.x, center.y + r * 0.7f ), color );
        line_icon( draw, ImVec2( center.x - r * 0.7f, center.y ), ImVec2( center.x + r * 0.7f, center.y ), color );
    } else if( icon_id == "touch_drop" ) {
        line_icon( draw, ImVec2( center.x, center.y - r * 0.72f ),
                   ImVec2( center.x, center.y + r * 0.34f ), color, 2.7f );
        draw->AddTriangleFilled( ImVec2( center.x, center.y + r * 0.7f ),
                                 ImVec2( center.x - r * 0.3f, center.y + r * 0.24f ),
                                 ImVec2( center.x + r * 0.3f, center.y + r * 0.24f ), color );
        line_icon( draw, ImVec2( center.x - r * 0.62f, center.y + r * 0.72f ),
                   ImVec2( center.x + r * 0.62f, center.y + r * 0.72f ), color );
    } else if( icon_id == "touch_wear" ) {
        draw->AddPolyline( std::vector<ImVec2>{
            ImVec2( center.x - r * 0.68f, center.y - r * 0.46f ),
            ImVec2( center.x - r * 0.28f, center.y - r * 0.7f ),
            ImVec2( center.x, center.y - r * 0.42f ),
            ImVec2( center.x + r * 0.28f, center.y - r * 0.7f ),
            ImVec2( center.x + r * 0.68f, center.y - r * 0.46f ),
            ImVec2( center.x + r * 0.45f, center.y + r * 0.72f ),
            ImVec2( center.x - r * 0.45f, center.y + r * 0.72f ),
            ImVec2( center.x - r * 0.68f, center.y - r * 0.46f )
        }.data(), 8, color, 0, 2.2f );
    } else {
        draw->AddCircle( center, r * 0.62f, color, 18, 2.4f );
        draw->AddCircleFilled( center, r * 0.13f, color );
        draw->AddCircleFilled( ImVec2( center.x, center.y - r * 0.38f ), r * 0.09f, color );
        draw->AddCircleFilled( ImVec2( center.x + r * 0.33f, center.y + r * 0.18f ), r * 0.09f, color );
        draw->AddCircleFilled( ImVec2( center.x - r * 0.33f, center.y + r * 0.18f ), r * 0.09f, color );
    }
}

int selected_radial_slot( const ImVec2 center, const ImVec2 mouse, const std::size_t count )
{
    if( count == 0 ) {
        return -1;
    }
    const float dx = mouse.x - center.x;
    const float dy = mouse.y - center.y;
    if( std::sqrt( dx * dx + dy * dy ) < radial_deadzone ) {
        return -1;
    }
    float angle = std::atan2( dy, dx ) + pi * 0.5f;
    if( angle < 0.0f ) {
        angle += 2.0f * pi;
    }
    const float sector = 2.0f * pi / static_cast<float>( count );
    return static_cast<int>( std::floor( ( angle + sector * 0.5f ) / sector ) ) %
           static_cast<int>( count );
}

void emit_radial_action( const radial_action &item )
{
    input_context *context = active_input_context();
    if( context == nullptr || !context->is_registered_action( item.action_id ) ) {
        return;
    }

    // Prototype bridge only: selection is semantic/action based, but the current
    // input manager accepts physical events.  Replay one existing binding until
    // input_context grows a direct action queue for touch/gamepad UI surfaces.
    std::vector<input_event> events = context->keys_bound_to( item.action_id, -1, false, true );
    if( events.empty() ) {
        events = context->keys_bound_to( item.action_id, -1, false, false );
    }
    if( !events.empty() ) {
        last_input = events.front();
    }
}

#endif // TILES

} // namespace

const std::vector<radial_action_family> &default_radial_families()
{
    // Keep this list intentionally small and spatially stable.  It is a touch
    // vocabulary, not a second copy of the keybindings screen.
    static const std::vector<radial_action_family> families = {
        { "interact", "Interact", "touch_interact",
          { "interact", "examine_and_pickup", "examine", "open", "close", "grab", "chat", "peek" } },
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

bool radial_overlay_wants_frame()
{
#if defined(TILES)
    radial_overlay_state &state = overlay_state();
    if( state.open ) {
        return true;
    }
    input_context *context = active_input_context();
    return context != nullptr && right_mouse_is_down() && !build_radial_actions( *context ).empty();
#else
    return false;
#endif
}

bool radial_overlay_should_capture_mouse()
{
#if defined(TILES)
    return radial_overlay_wants_frame();
#else
    return false;
#endif
}

void draw_radial_overlay()
{
#if defined(TILES)
    if( ImGui::GetCurrentContext() == nullptr ) {
        return;
    }

    radial_overlay_state &state = overlay_state();
    input_context *context = active_input_context();
    const bool right_down = right_mouse_is_down();
    ImGuiIO &io = ImGui::GetIO();

    if( !state.open ) {
        if( !right_down || context == nullptr ) {
            return;
        }
        state.actions = build_radial_actions( *context );
        if( state.actions.empty() ) {
            return;
        }
        state.open = true;
        state.center = clamp_radial_center( io.MousePos, io.DisplaySize );
        state.selected = -1;
    }

    if( right_down ) {
        state.selected = selected_radial_slot( state.center, io.MousePos, state.actions.size() );
    }

    ImDrawList *draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled( ImVec2( 0.0f, 0.0f ), io.DisplaySize, IM_COL32( 0, 0, 0, 70 ) );
    draw->AddCircleFilled( state.center, radial_deadzone, IM_COL32( 18, 20, 24, 225 ), 36 );
    draw->AddCircle( state.center, radial_deadzone, IM_COL32( 180, 188, 200, 170 ), 36, 1.5f );

    const float count = static_cast<float>( state.actions.size() );
    for( std::size_t i = 0; i < state.actions.size(); ++i ) {
        const float angle = -pi * 0.5f + 2.0f * pi * static_cast<float>( i ) / count;
        const ImVec2 pos( state.center.x + std::cos( angle ) * radial_radius,
                          state.center.y + std::sin( angle ) * radial_radius );
        const bool selected = static_cast<int>( i ) == state.selected;
        const float radius = radial_slot_radius + ( selected ? 5.0f : 0.0f );
        const ImU32 fill = selected ? IM_COL32( 68, 116, 180, 245 ) : IM_COL32( 27, 31, 38, 232 );
        const ImU32 edge = selected ? IM_COL32( 232, 240, 255, 255 ) : IM_COL32( 132, 142, 156, 210 );
        draw->AddCircleFilled( pos, radius, fill, 30 );
        draw->AddCircle( pos, radius, edge, 30, selected ? 2.7f : 1.4f );
        draw_touch_icon( draw, state.actions[i].icon_id, pos, selected ? 31.0f : 27.0f,
                         IM_COL32( 245, 247, 250, 255 ) );

        const ImVec2 text_size = ImGui::CalcTextSize( state.actions[i].label.c_str() );
        draw->AddText( ImVec2( pos.x - text_size.x * 0.5f, pos.y + radius + 5.0f ),
                       IM_COL32( 238, 241, 246, 245 ), state.actions[i].label.c_str() );
    }

    const char *center_text = "Cancel";
    if( state.selected >= 0 && static_cast<std::size_t>( state.selected ) < state.actions.size() ) {
        center_text = state.actions[state.selected].label.c_str();
    }
    const ImVec2 center_text_size = ImGui::CalcTextSize( center_text );
    draw->AddText( ImVec2( state.center.x - center_text_size.x * 0.5f,
                           state.center.y - center_text_size.y * 0.5f ),
                   IM_COL32( 250, 250, 252, 255 ), center_text );

    if( !right_down ) {
        if( state.selected >= 0 && static_cast<std::size_t>( state.selected ) < state.actions.size() ) {
            emit_radial_action( state.actions[state.selected] );
        }
        state = radial_overlay_state();
    }
#endif
}

} // namespace touch_ui
