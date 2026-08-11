#include "touch_input.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <sstream>
#include <utility>
#include <vector>

namespace touch_ui
{
namespace
{
constexpr float gesture_deadzone = 0.025f;
constexpr float pinch_threshold = 0.018f;
constexpr std::uint32_t long_press_ms = 320;
constexpr std::size_t diagnostic_limit = 100;

std::mutex queue_mutex;
std::deque<semantic_input> semantic_queue;
std::mutex diagnostic_mutex;
std::deque<std::string> diagnostic_events;
input_device last_device = input_device::touch;
std::uint32_t last_device_ticks = 0;

float distance( normalized_point a, normalized_point b )
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt( dx * dx + dy * dy );
}

} // namespace

touch_layout default_touch_layout( const touch_device_class device )
{
    if( device == touch_device_class::tablet ) {
        return { { 0.14f, 0.82f }, { 0.86f, 0.80f }, 1.12f, true };
    }
    return { { 0.16f, 0.82f }, { 0.84f, 0.80f }, 1.0f, false };
}

void queue_semantic_input( semantic_input input )
{
    std::lock_guard<std::mutex> lock( queue_mutex );
    semantic_queue.push_back( std::move( input ) );
}

std::optional<semantic_input> pop_semantic_input()
{
    std::lock_guard<std::mutex> lock( queue_mutex );
    if( semantic_queue.empty() ) {
        return std::nullopt;
    }
    semantic_input result = std::move( semantic_queue.front() );
    semantic_queue.pop_front();
    return result;
}

void clear_semantic_input()
{
    std::lock_guard<std::mutex> lock( queue_mutex );
    semantic_queue.clear();
}

void gesture_router::finger_down( const std::int64_t finger, const float x, const float y,
                                  const std::uint32_t ticks )
{
    finger_state incoming{ finger, { x, y }, { x, y }, ticks, true };
    if( !primary_.active ) {
        primary_ = incoming;
        state_ = touch_gesture_state::pending;
        radial_release_ = false;
        return;
    }
    if( !secondary_.active && finger != primary_.id ) {
        secondary_ = incoming;
        pinch_start_distance_ = distance( primary_.current, secondary_.current );
        state_ = touch_gesture_state::pan;
    }
}

void gesture_router::finger_motion( const std::int64_t finger, const float x, const float y,
                                    const std::uint32_t ticks )
{
    finger_state *moved = nullptr;
    if( primary_.active && primary_.id == finger ) {
        moved = &primary_;
    } else if( secondary_.active && secondary_.id == finger ) {
        moved = &secondary_;
    }
    if( moved == nullptr ) {
        return;
    }
    const normalized_point before = moved->current;
    moved->current = { x, y };

    if( secondary_.active ) {
        const float current_distance = distance( primary_.current, secondary_.current );
        if( std::fabs( current_distance - pinch_start_distance_ ) >= pinch_threshold ) {
            state_ = touch_gesture_state::pinch;
            queue_semantic_input( { semantic_input_kind::zoom, "", 0, 0,
                                    current_distance - pinch_start_distance_ } );
            pinch_start_distance_ = current_distance;
        } else {
            state_ = touch_gesture_state::pan;
            queue_semantic_input( { semantic_input_kind::pan, "", 0, 0,
                                    distance( before, moved->current ) } );
        }
        return;
    }

    const float travelled = distance( primary_.start, primary_.current );
    if( state_ == touch_gesture_state::pending && ticks - primary_.down_ticks >= long_press_ms ) {
        state_ = touch_gesture_state::radial;
        log_touch_diagnostic( "gesture: radial long-press" );
    } else if( state_ == touch_gesture_state::pending && travelled >= gesture_deadzone ) {
        state_ = touch_gesture_state::movement;
    }
}

void gesture_router::finger_up( const std::int64_t finger, const float x, const float y,
                                const std::uint32_t ticks )
{
    if( primary_.active && primary_.id == finger ) {
        primary_.current = { x, y };
        if( state_ == touch_gesture_state::pending && ticks - primary_.down_ticks >= long_press_ms ) {
            state_ = touch_gesture_state::radial;
        }
        radial_release_ = state_ == touch_gesture_state::radial;
        primary_.active = false;
    } else if( secondary_.active && secondary_.id == finger ) {
        secondary_.current = { x, y };
        secondary_.active = false;
    }

    if( !primary_.active && !secondary_.active && !radial_release_ ) {
        state_ = touch_gesture_state::idle;
    } else if( primary_.active && !secondary_.active ) {
        state_ = touch_gesture_state::pending;
    }
}

void gesture_router::cancel_all()
{
    primary_ = finger_state();
    secondary_ = finger_state();
    state_ = touch_gesture_state::cancelled;
    radial_release_ = false;
    clear_semantic_input();
    log_touch_diagnostic( "gesture: cancelled by lifecycle/interruption" );
}

touch_gesture_state gesture_router::state() const
{
    return state_;
}

bool gesture_router::radial_requested() const
{
    return state_ == touch_gesture_state::radial || radial_release_;
}

normalized_point gesture_router::origin() const
{
    return primary_.start;
}

normalized_point gesture_router::pointer() const
{
    return primary_.current;
}

int gesture_router::selected_sector( const int sector_count ) const
{
    if( sector_count <= 0 ) {
        return -1;
    }
    const float dx = primary_.current.x - primary_.start.x;
    const float dy = primary_.current.y - primary_.start.y;
    if( std::sqrt( dx * dx + dy * dy ) < gesture_deadzone ) {
        return -1;
    }
    constexpr float pi = 3.14159265358979323846f;
    float angle = std::atan2( dy, dx ) + pi * 0.5f;
    if( angle < 0.0f ) {
        angle += 2.0f * pi;
    }
    const float sector = 2.0f * pi / static_cast<float>( sector_count );
    return static_cast<int>( std::floor( ( angle + sector * 0.5f ) / sector ) ) % sector_count;
}

bool gesture_router::consume_radial_release()
{
    if( !radial_release_ ) {
        return false;
    }
    radial_release_ = false;
    state_ = touch_gesture_state::idle;
    return true;
}

gesture_router &touch_gestures()
{
    static gesture_router router;
    return router;
}

void note_input_device( const input_device device, const std::uint32_t ticks )
{
    last_device = device;
    last_device_ticks = ticks;
    ( void )last_device_ticks;
}

input_device active_input_device()
{
    return last_device;
}

bool touch_hud_visible()
{
    return last_device == input_device::touch;
}

void haptic_sector_tick()
{
    // Platform-neutral seam. Apple adapter can map this to a light impact/selection haptic.
}

void haptic_action_commit()
{
    // Platform-neutral seam. Apple adapter can map this to a medium confirmation haptic.
}

void platform_touch_interruption()
{
    touch_gestures().cancel_all();
}

void log_touch_diagnostic( const std::string &event )
{
    std::lock_guard<std::mutex> lock( diagnostic_mutex );
    diagnostic_events.push_back( event );
    while( diagnostic_events.size() > diagnostic_limit ) {
        diagnostic_events.pop_front();
    }
}

std::string touch_diagnostic_report()
{
    std::lock_guard<std::mutex> lock( diagnostic_mutex );
    std::ostringstream out;
    out << "CDDA touch input diagnostics\n";
    out << "active_device=" << static_cast<int>( last_device ) << "\n";
    out << "gesture_state=" << static_cast<int>( touch_gestures().state() ) << "\n";
    out << "events=" << diagnostic_events.size() << "\n";
    for( const std::string &event : diagnostic_events ) {
        out << "- " << event << "\n";
    }
    return out.str();
}

} // namespace touch_ui
