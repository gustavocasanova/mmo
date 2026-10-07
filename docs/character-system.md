# Character System

## Responsibilities

```text
GLB/glTF --GltfModelLoader--> assets::Model (CPU data)
                                      |
                                      v
Character -> CharacterModel -> CharacterBody -> meshes + skeleton
                    |                 |
                    |                 +-> AnimationController -> pose/skin matrices
                    +-> EquipmentManager
                                      |
                                      v
                                  Renderer

InputSettings -> CharacterController -> Character transform / animation state
                       |                         |
                       +-> CollisionWorld <------+
CameraController -> horizontal WASD basis and camera pose
```

- `assets::Model` stores meshes, nodes, materials, textures, skeleton and clips without GPU dependencies.
- `CharacterBody` owns the base model and shared rig. `CharacterModel` composes the body, animation controller, appearance and equipment manager.
- `Character` stores a transform and basic stats. `CharacterController` handles movement, jumping, turning and locomotion animation without depending on OpenGL.
- `CollisionWorld` provides a flat ground plane, rectangular movement bounds, static AABB obstacles, character sliding and a swept camera-volume query. It is a small prototype collision layer, not general terrain or rigid-body physics.
- `Renderer` draws the character and the same static boxes used by collision queries.

## Animation

`Animator` samples `LINEAR`/`STEP` translation, rotation and scale channels and calculates `global * inverseBind` skinning matrices. `AnimationController` binds named clips to reusable states and can also play any loaded clip by name.

The client loads all 43 animations from `Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb`. Idle, walk, jog/sprint and jump clips drive movement. Jump uses `Jump_Start`, `Jump_Loop` while airborne, then `Jump_Land` on touchdown. A reversed-time `Walk_Backward_Loop` is derived from the included walk clip for reverse input; it is an additional runtime clip, not one of the pack's 43 originals.

Use `[` and `]` to preview every package animation, including clips not assigned to locomotion. Preview playback is one-shot and normal movement animation resumes when it ends. The demo does not trigger attack, hit, death, spell or interaction clips as gameplay actions.

## Movement and controls

`platform::InputSettings` centralizes the default bindings: WASD move, Space jumps, and Left Shift runs. WASD vectors are built from camera yaw only, flattened to the ground and normalized, so diagonals do not gain speed. `MovementSettings` configures walk/run/backward/strafe speeds, acceleration, deceleration, air control, gravity, jump impulse, turn speed and character collision dimensions.

The controller derives `Idle`, `Walking`, `Running`, `Jumping` or `Falling` from its velocity and grounded state. It smooths horizontal velocity and character yaw independently from the camera. Character translation is constrained by world bounds and slides along static box obstacles. The current floor is flat at Y=0.

Hold the right mouse button and move to orbit; release it to free the cursor. The wheel changes the chosen zoom. The demo's two visible walls can be used to test character and camera collision.

`CharacterControllerTest` covers camera-relative direction, diagonal normalization, movement acceleration, jumping/gravity/landing and wall blocking. `CharacterEquipmentTest` continues to validate the model, skeleton, equipment and animation-pack loading contracts.
