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

## Layered animation (movement + combat)

Movement and combat are independent. `CharacterController` owns `MovementState` (Idle, Walk, Run, StrafeLeft, StrafeRight, Backward) and drives the lower-body layer (the `Animator`). `CombatController` owns `CombatState` (None, Attack, Attack2, Block, Cast) and drives the upper-body layer through `AnimationController::play_action`. `AnimationMixer` (`src/animation/layered_animation.*`) blends the one-shot action clip over the locomotion pose using a `BoneMask`: `FinalPose = Blend(LowerPose, UpperPose, mask * layerWeight)`. No combined clips exist; attacking never restarts or replaces the locomotion clip, and movement input is never blocked.

- The mask is built from the real skeleton: the first existing candidate of `spine_01`/`Spine`/`spine` plus all its descendants (spine, chest, clavicles, arms, hands, fingers, neck, head). `root`, `pelvis` and legs stay in the lower layer, so action clips cannot displace the character; the controller remains the only owner of position.
- The layer weight blends in over `blend_in`, holds, and blends out over `blend_out` before the action ends (smoothstep).
- `ActionSettings`/`CombatActionDefinition` expose duration, speed, `hit_time`, `blend_in` and `blend_out`. A running action is not restarted; a new one is accepted during blend-out.
- The mixer emits `ActionStart`/`ActionHit`/`ActionEnd` events; `CombatController` translates them to `AttackStart`/`AttackHit`/`AttackEnd` for the `CombatSystem`, which decides on damage/effects. The animation never applies damage.
- Client input: left mouse or `1` = Attack, `2` = Attack 2, `3` = Cast. `F3` toggles a console debug line with movement/combat state, lower/upper clips and weights, attack time, mask bone count and active layers. `Block` has no clip in the animation pack and is not bound by default. The pack has no strafe clips, so strafing reuses the walk clip.
- Tests: `AnimationLayersTest` (mask, blend, events, weights) and `CharacterControllerTest` (walk + attack with the real skeleton).
