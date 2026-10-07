# Third-Person Camera

`scene::CameraController` is an orbit camera independent of the player and renderer. Hold the right mouse button to orbit, release it to restore the cursor, and use the wheel to zoom. The camera follows a point above the character, supports unrestricted yaw and pitch clamped to -80°..80°, and smooths rotation, focus, and distance.

Default distance is 6 units, with player-selected zoom clamped to 2..15. Camera collision uses a swept sphere against the static boxes in `game::world::CollisionWorld`. When blocked, the actual camera distance contracts immediately without changing the desired zoom; it returns smoothly when the path clears. The world also keeps the camera above the ground plane. Collision radius, minimum collision distance, sensitivity, target offset, and smoothing times are configurable in `CameraSettings`.

The demo includes two visible static walls so both character blocking/sliding and camera obstruction can be tried in-game. Collision is intentionally limited to the flat ground, rectangular movement bounds, and axis-aligned static boxes; it is not a general terrain or rigid-body physics system.

The orbit and movement axes are independent: camera yaw determines the horizontal WASD basis, while `CharacterController` smoothly turns the character toward its travel direction. Camera input never rotates the character directly.

`tests/camera_tests.cpp` covers orbit, pitch and zoom limits, follow smoothing, projection validity, and camera collision/zoom recovery.
