#include "touch_radial.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <unordered_set>
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
#else
    int x = 0;
    int y = 0;
#endif
    const auto buttons = SDL_GetMouseState( &x, &y );
    return ( buttons & SDL_BUTTON_RMASK ) != 0;
}

ImVec2 clamp_radial_center( ImVec2 center, const ImVec2 display_size )
{
    const float margin = radial_radius + radial_slot_radius + 10.0f;
    if( display_size.x > margin * 2.0f ) {
        center.x = std::clamp( center.x, margin, display_size.x - margin );
    }
    if( display_size.y > margin * 2.0f ) {
        center.y = std::clamp( center.y, margin, display_size.y - margin );
    }
    return center;
}

void line( ImDrawList *draw, const ImVec2 a, const ImVec2 b, const ImU32 color,
           const float thickness = 2.3f )
{
    draw->AddLine( a, b, color, thickness );
}

void draw_touch_icon( ImDrawList *draw, const std::string &icon_id, const ImVec2 center,
                      const float size, const ImU32 color )
{
    const float r = size * 0.5f;

    if( icon_id == "touch_interact" ) {
        draw->AddCircle( center, r * 0.52f, color, 20, 2.5f );
        line( draw, ImVec2( center.x + r * 0.36f, center.y + r * 0.36f ),
              ImVec2( center.x + r * 0.78f, center.y + r * 0.78f ), color, 2.5f );
        draw->AddCircleFilled( center, r * 0.11f, color );
    } else if( icon_id == "touch_items" || icon_id == "touch_inventory" ) {
        draw->AddRect( ImVec2( center.x - r * 0.67f, center.y - r * 0.42f ),
                       ImVec2( center.x + r * 0.67f, center.y + r * 0.7f ), color,
                       4.0f, ImDrawFlags_None, 2.3f );
        draw->PathLineTo( ImVec2( center.x - r * 0.34f, center.y - r * 0.42f ) );
        draw->PathBezierCubicCurveTo( ImVec2( center.x - r * 0.34f, center.y - r * 0.86f ),
                                      ImVec2( center.x + r * 0.34f, center.y - r * 0.86f ),
                                      ImVec2( center.x + r * 0.34f, center.y - r * 0.42f ) );
        draw->PathStroke( color, ImDrawFlags_None, 2.3f );
    } else if( icon_id == "touch_combat" ) {
        draw->AddCircle( center, r * 0.62f, color, 20, 2.2f );
        draw->AddCircle( center, r * 0.22f, color, 16, 2.2f );
        line( draw, ImVec2( center.x - r * 0.82f, center.y ), ImVec2( center.x - r * 0.38f, center.y ), color );
        line( draw, ImVec2( center.x + r * 0.38f, center.y ), ImVec2( center.x + r * 0.82f, center.y ), color );
        line( draw, ImVec2( center.x, center.y - r * 0.82f ), ImVec2( center.x, center.y - r * 0.38f ), color );
        line( draw, ImVec2( center.x, center.y + r * 0.38f ), ImVec2( center.x, center.y + r * 0.82f ), color );
    } else if( icon_id == "touch_reload" ) {
        draw->PathArcTo( center, r * 0.68f, -0.35f * pi, 1.25f * pi, 28 );
        draw->PathStroke( color, ImDrawFlags_None, 2.6f );
        draw->AddTriangleFilled( ImVec2( center.x - r * 0.72f, center.y - r * 0.15f ),
                                 ImVec2( center.x - r * 0.34f, center.y - r * 0.08f ),
                                 ImVec2( center.x - r * 0.58f, center.y + r * 0.2f ), color );
    } else if( icon_id == "touch_movement" ) {
        draw->AddTriangleFilled( ImVec2( center.x, center.y - r * 0.82f ),
                                 ImVec2( center.x - r * 0.25f, center.y - r * 0.34f ),
                                 ImVec2( center.x + r * 0.25f, center.y - r * 0.34f ), color );
        draw->AddTriangleFilled( ImVec2( center.x + r * 0.82f, center.y ),
                                 ImVec2( center.x + r * 0.34f, center.y - r * 0.25f ),
                                 ImVec2( center.x + r * 0.34f, center.y + r * 0.25f ), color );
        draw->AddTriangleFilled( ImVec2( center.x, center.y + r * 0.82f ),
                                 ImVec2( center.x - r * 0.25f, center.y + r * 0.34f ),
                                 ImVec2( center.x + r * 0.25f, center.y + r * 0.34f ), color );
        draw->AddTriangleFilled( ImVec2( center.x - r * 0.82f, center.y ),
                                 ImVec2( center.x - r * 0.34f, center.y - r * 0.25f ),
                                 ImVec2( center.x - r * 0.34f, center.y + r * 0.25f ), color );
    } else if( icon_id == "touch_wait" ) {
        draw->AddCircle( center, r * 0.68f, color, 24, 2.4f );
        line( draw, center, ImVec2( center.x, center.y - r * 0.43f ), color );
        line( draw, center, ImVec2( center.x + r * 0.34f, center.y + r * 0.2f ), color );
    } else if( icon_id == "touch_craft" ) {
        line( draw, ImVec2( center.x - r * 0.62f, center.y + r * 0.66f ),
              ImVec2( center.x + r * 0.48f, center.y - r * 0.44f ), color, 3.2f );
        draw->AddCircle( ImVec2( center.x + r * 0.5f, center.y - r * 0.46f ),
                         r * 0.25f, color, 16, 2.2f );
    } else if( icon_id == "touch_map" ) {
        line( draw, ImVec2( center.x - r * 0.7f, center.y - r * 0.58f ),
              ImVec2( center.x - r * 0.2f, center.y - r * 0.7f ), color );
        line( draw, ImVec2( center.x - r * 0.2f, center.y - r * 0.7f ),
              ImVec2( center.x + r * 0.25f, center.y - r * 0.5f ), color );
        line( draw, ImVec2( center.x + r * 0.25f, center.y - r * 0.5f ),
              ImVec2( center.x + r * 0.7f, center.y - r * 0.64f ), color );
        line( draw, ImVec2( center.x - r * 0.7f, center.y - r * 0.58f ),
              ImVec2( center.x - r * 0.7f, center.y + r * 0.62f ), color );
        line( draw, ImVec2( center.x - r * 0.2f, center.y - r * 0.7f ),
              ImVec2( center.x - r * 0.2f, center.y + r * 0.5f ), color );
        line( draw, ImVec2( center.x + r * 0.25f, center.y - r * 0.5f ),
              ImVec2( center.x + r * 0.25f, center.y + r * 0.68f ), color );
        line( draw, ImVec2( center.x + r * 0.7f, center.y - r * 0.64f ),
              ImVec2( center.x + r * 0.7f, center.y + r * 0.52f ), color );
    } else if( icon_id == "touch_more" ) {
        draw->AddCircleFilled( ImVec2( center.x - r * 0.48f, center.y ), r * 0.11f, color );
        draw->AddCircleFilled( center, r * 0.11f, color );
        draw->AddCircleFilled( ImVec2( center.x + r * 0.48f, center.y ), r * 0.11f, color );
    } else if( icon_id == "touch_vehicle" ) {
        draw->AddRect( ImVec2( center.x - r * 0.72f, center.y - r * 0.25f ),
                       ImVec2( center.x + r * 0.72f, center.y + r * 0.42f ), color,
                       4.0f, ImDrawFlags_None, 2.2f );
        draw->AddCircleFilled( ImVec2( center.x - r * 0.42f, center.y + r * 0.45f ), r * 0.16f, color );
        draw->AddCircleFilled( ImVec2( center.x + r * 0.42f, center.y + r * 0.45f ), r * 0.16f, color );
    } else if( icon_id == "touch_character" ) {
        draw->AddCircle( ImVec2( center.x, center.y - r * 0.36f ), r * 0.28f, color, 18, 2.3f );
        draw->PathArcTo( ImVec2( center.x, center.y + r * 0.7f ), r * 0.68f,
                         1.08f * pi, 1.92f * pi, 20 );
        draw->PathStroke( color, ImDrawFlags_None, 2.4f );
    } else {
        draw->AddCircle( center, r * 0.62f, color, 18, 2.3f );
        draw->AddCircleFilled( center, r * 0.12f, color );
    }
}

int selected_radial_slot( const ImVec2 center, const ImVec2 pointer, const std::size_t count )
{
    if( count == 0 ) {
        return -1;
    }
    const float dx = pointer.x - center.x;
    const float dy = pointer.y - center.y;
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

SDL_Keymod modifiers_for_binding( const input_event &binding )
{
    int modifiers = 0;
    if( binding.modifiers.count( keymod_t::ctrl ) != 0 ) {
        modifiers |= KMOD_CTRL;
    }
    if( binding.modifiers.count( keymod_t::shift ) != 0 ) {
        modifiers |= KMOD_SHIFT;
    }
    if( binding.modifiers.count( keymod_t::alt ) != 0 ) {
        modifiers |= KMOD_ALT;
    }
    return static_cast<SDL_Keymod>( modifiers );
}

bool enqueue_keyboard_binding( const input_event &binding )
{
    if( binding.sequence.size() != 1 ||
        ( binding.type != input_event_t::keyboard_code && binding.type != input_event_t::keyboard_char ) ) {
        return false;
    }

    SDL_Event event{};
    event.type = CATA_KEYDOWN;
#if SDL_MAJOR_VERSION >= 3
    event.key.type = CATA_KEYDOWN;
    event.key.key = static_cast<SDL_Keycode>( binding.get_first_input() );
    event.key.mod = modifiers_for_binding( binding );
    event.key.down = true;
    event.key.repeat = false;
#else
    event.key.type = CATA_KEYDOWN;
    event.key.state = SDL_PRESSED;
    event.key.repeat = 0;
    event.key.keysym.sym = static_cast<SDL_Keycode>( binding.get_first_input() );
    event.key.keysym.mod = modifiers_for_binding( binding );
#endif
    ( void )SDL_PushEvent( &event );
    return true;
}

void emit_radial_action( const radial_action &item )
{
    input_context *context = active_input_context();
    if( context == nullptr || !context->is_registered_action( item.action_id ) ) {
        return;
    }

    // The radial resolves a semantic CDDA action first.  For this desktop
    // prototype, bridge that action into the existing input pump by enqueuing
    // one of its keyboard bindings.  The touch model itself remains independent
    // of which physical key happens to be bound to the action.
    const std::vector<input_event> events = context->keys_bound_to( item.action_id, -1, false, true );
    const auto preferred = std::find_if( events.begin(), events.end(), []( const input_event &event ) {
        return event.type == input_event_t::keyboard_code && event.sequence.size() == 1;
    } );
    if( preferred != events.end() && enqueue_keyboard_binding( *preferred ) ) {
        return;
    }
    for( const input_event &event : events ) {
        if( enqueue_keyboard_binding( event ) ) {
            return;
        }
    }
}

#endif // TILES

} // namespace

const std::vector<radial_action_family> &default_radial_families()
{
    // The first eight entries are the stable default thumb vocabulary.  Each
    // slot resolves to the first underlying action actually registered by the
    // current CDDA input_context; the keyboard action taxonomy stays internal.
    static const std::vector<radial_action_family> families = {
        { "interact", "Interact", "touch_interact",
          { "interact", "examine_and_pickup", "examine", "open", "close", "grab", "chat", "peek" } },
        { "items", "Items", "touch_items",
          { "item_action_menu", "inventory", "pickup", "apply", "eat", "drop" } },
        { "combat", "Combat", "touch_combat",
          { "fire", "fire_burst", "autoattack", "throw_wielded", "throw" } },
        { "reload", "Reload", "touch_reload",
          { "reload_wielded", "reload_weapon", "reload_item", "unload" } },
        { "movement", "Movement", "touch_movement",
          { "open_movement", "cycle_move", "toggle_run", "toggle_crouch", "toggle_prone" } },
        { "wait", "Wait", "touch_wait", { "wait", "pause" } },
        { "craft", "Craft", "touch_craft", { "craft", "recraft", "long_craft", "construct" } },
        { "more", "More", "touch_more", { "action_menu", "main_menu" } },

        // Context fallbacks: these occupy a slot only when earlier families are
        // absent in a specialised screen.
        { "map", "Map", "touch_map", { "map" } },
        { "character", "Character", "touch_character",
          { "player_data", "bodystatus", "medical", "morale", "bionics", "mutations" } },
        { "vehicle", "Vehicle", "touch_vehicle", { "control_vehicle" } },
        { "zones", "Zones", "touch_zones", { "zones", "loot" } }
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
        return "touch_items";
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

    // Bespoke contexts still get access to actions not yet in the semantic
    // vocabulary, with a generic icon and readable fallback label.
    for( const std::string &action : available_actions ) {
        if( consumed_actions.count( action ) != 0 || is_touch_native_navigation_action( action ) ||
            action_is_family_candidate( action ) ) {
            continue;
        }
        result.push_back( radial_action{ action, prettify_action_id( action ),
                                        fallback_icon_for_action( action ), action } );
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

        // The radial owns this gesture.  Remove the queued button-down so the
        // underlying map does not also interpret it as a right-click action.
        SDL_FlushEvent( CATA_MOUSEBUTTONDOWN );
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
        // Consume the matching release too; otherwise the normal mouse path sees
        // half of a click after the radial has already handled the gesture.
        SDL_FlushEvent( CATA_MOUSEBUTTONUP );
        if( state.selected >= 0 && static_cast<std::size_t>( state.selected ) < state.actions.size() ) {
            emit_radial_action( state.actions[state.selected] );
        }
        state = radial_overlay_state();
    }
#endif
}

} // namespace touch_ui
