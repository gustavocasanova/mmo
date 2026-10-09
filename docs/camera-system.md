# Third-Person Camera

`scene::CameraController` is an orbit camera independent of the player and renderer. In game mode, hold and drag either mouse button to orbit independently of the character; on left-button press, the movement camera heading is held at its starting yaw until release, so orbiting cannot turn or redirect the character. The camera smoothly returns behind the character when released. A left click without a drag still selects a target. A right click without a drag still selects/engages a target or exits combat, while a right-button drag is reserved for free camera look. In the World Editor, hold the right mouse button to look around. Use the wheel to zoom.

Mouse look uses the standard vertical direction: moving the mouse upward raises the view, and moving it downward lowers the view. `CameraSettings::invert_vertical` can opt into the opposite direction; the default is non-inverted.

Default distance is 6 units, with player-selected zoom clamped to 2..15. Camera collision uses a swept sphere against the static boxes in `game::world::CollisionWorld`. When blocked, the actual camera distance contracts immediately without changing the desired zoom; it returns smoothly when the path clears. The world also keeps the camera above the ground plane. Collision radius, minimum collision distance, sensitivity, target offset, and smoothing times are configurable in `CameraSettings`.

The demo includes two visible static walls so both character blocking/sliding and camera obstruction can be tried in-game. Collision is intentionally limited to the flat ground, rectangular movement bounds, and axis-aligned static boxes; it is not a general terrain or rigid-body physics system.

The orbit and movement axes are independent: camera yaw determines the horizontal WASD basis, while `CharacterController` smoothly turns the character toward its travel direction. Camera input never rotates the character directly. The return yaw and its smoothing duration are configurable through `CameraController::set_follow_target_yaw` and `CameraSettings::follow_rotation_smoothing_seconds`.

`tests/camera_tests.cpp` covers orbit, pitch and zoom limits, follow smoothing, projection validity, and camera collision/zoom recovery.
