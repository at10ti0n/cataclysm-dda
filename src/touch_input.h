#pragma once
#ifndef CATA_SRC_TOUCH_INPUT_H
#define CATA_SRC_TOUCH_INPUT_H

#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace touch_ui
{

enum class touch_device_class : std::uint8_t {
    phone,
    tablet
};

enum class touch_gesture_state : std::uint8_t {
    idle,
    pending,
    radial,
    movement,
    pan,
    pinch,
    cancelled
};

enum class semantic_input_kind : std::uint8_t {
    action,
    move,
    pan,
    zoom,
    cancel
};

struct semantic_input {
    semantic_input_kind kind = semantic_input_kind::cancel;
    std::string action_id;
    int dx = 0;
    int dy = 0;
    float amount = 0.0f;
};

struct normalized_point {
    float x = 0.0f;
    float y = 0.0f;
};

struct touch_layout {
    normalized_point movement_anchor;
    normalized_point radial_anchor;
    float control_scale = 1.0f;
    bool persistent_hud = false;
};

/** Device-class defaults. Coordinates are normalized so layouts survive rotation/Retina sizes. */
touch_layout default_touch_layout( touch_device_class device );

/** Thread-light semantic queue: touch/gamepad/platform adapters enqueue intents, gameplay consumes them. */
void queue_semantic_input( semantic_input input );
std::optional<semantic_input> pop_semantic_input();
void clear_semantic_input();

/** Deferred recognizer: no gameplay action is emitted while a gesture is ambiguous. */
class gesture_router
{
    public:
        void finger_down( std::int64_t finger, float x, float y, std::uint32_t ticks );
        void finger_motion( std::int64_t finger, float x, float y, std::uint32_t ticks );
        void finger_up( std::int64_t finger, float x, float y, std::uint32_t ticks );
        void cancel_all();

        touch_gesture_state state() const;
        bool radial_requested() const;
        normalized_point origin() const;
        normalized_point pointer() const;
        int selected_sector( int sector_count ) const;
        bool consume_radial_release();

    private:
        struct finger_state {
            std::int64_t id = 0;
            normalized_point start;
            normalized_point current;
            std::uint32_t down_ticks = 0;
            bool active = false;
        };

        finger_state primary_;
        finger_state secondary_;
        touch_gesture_state state_ = touch_gesture_state::idle;
        bool radial_release_ = false;
        float pinch_start_distance_ = 0.0f;

        void update_multitouch_state();
};

gesture_router &touch_gestures();

/** Last-input-device policy used to hide touch chrome when a controller/keyboard takes over. */
enum class input_device : std::uint8_t { touch, controller, keyboard_mouse };
void note_input_device( input_device device, std::uint32_t ticks );
input_device active_input_device();
bool touch_hud_visible();

/** Platform hooks. Apple implementations may provide CoreHaptics/UIKit behavior; others are no-ops. */
void haptic_sector_tick();
void haptic_action_commit();
void platform_touch_interruption();

/** Compact diagnostic ring for on-device bug reports. */
void log_touch_diagnostic( const std::string &event );
std::string touch_diagnostic_report();

} // namespace touch_ui

#endif // CATA_SRC_TOUCH_INPUT_H
