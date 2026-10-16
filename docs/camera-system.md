# Third-Person Camera

`scene::CameraController` is an orbit camera independent of the player and renderer. In game mode, left-button drag rotates the camera without changing the character's facing. While movement continues, its heading stays stable; once movement stops during the drag, the WASD reference rebases to the current camera direction. Right-button drag rotates the camera and smoothly steers the character toward its view direction; A/D continue to strafe. Holding both buttons also moves forward until the chord ends, without consuming a world click. The camera keeps the reached orientation when manual look ends. A left click without a drag still selects a target, and a right click without a drag still selects/engages a target or exits combat. In the World Editor, hold the right mouse button to look around. Use the wheel to zoom.

Mouse look uses the standard vertical direction: moving the mouse upward raises the view, and moving it downward lowers the view. `CameraSettings::invert_vertical` can opt into the opposite direction; the default is non-inverted.

Default distance is 6 units, with player-selected zoom clamped to 2..15. Camera collision uses a swept sphere against the static boxes in `world::CollisionWorld`. When blocked, the actual camera distance contracts immediately without changing the desired zoom; it returns smoothly when the path clears. The world also keeps the camera above the ground plane. Collision radius, minimum collision distance, sensitivity, target offset, and smoothing times are configurable in `CameraSettings`.

The demo includes two visible static walls so both character blocking/sliding and camera obstruction can be tried in-game. Collision is intentionally limited to the flat ground, rectangular movement bounds, and axis-aligned static boxes; it is not a general terrain or rigid-body physics system.

Wheel zoom is proportional to the current camera distance, so each notch changes distance by the same percentage instead of the same number of world units. The default sensitivity avoids large jumps from a single notch.

The orbit and movement axes are independent: camera yaw determines the horizontal WASD basis, while `CharacterController` smoothly turns the character toward its travel direction. Left-button look never supplies a facing target; right-button look temporarily overrides automatic combat facing until released. The return yaw and its smoothing duration are configurable through `CameraController::set_follow_target_yaw` and `CameraSettings::follow_rotation_smoothing_seconds`.

In combat Focus Target, the camera keeps its player-relative orbit and selected zoom; only its aim point is smoothly moved to the target. Losing or switching a target blends the aim point instead of repositioning the camera around the player-target midpoint.

`tests/camera_tests.cpp` covers orbit, pitch and zoom limits, follow smoothing, projection validity, and camera collision/zoom recovery.
