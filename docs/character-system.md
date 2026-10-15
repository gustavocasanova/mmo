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

The client loads all 43 animations from `Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb`. Idle, jog, walk (backward only) and jump clips drive movement. Jump uses `Jump_Start`, `Jump_Loop` while airborne, then `Jump_Land` on touchdown. A reversed-time `Walk_Backward_Loop` is derived from the included walk clip at startup. If the locally generated melee animation file is available, the client also merges its retargeted combat locomotion clips into the model.

Use `[` and `]` to preview every package animation, including clips not assigned to locomotion. Preview playback is one-shot and normal movement animation resumes when it ends. Combat attacks are triggered by the target-based auto-attack system and apply damage only on the animation's hit event.

## Movement and controls

`platform::InputSettings` centralizes the default bindings: WASD move, Space jumps, and there is no walk toggle: the character always runs (backward uses the slower `Walk_Backward_Loop`). The pack has no lateral walk/run clips, so A/D reuse the forward jog clip while the character turns toward travel. WASD vectors are built from camera yaw only, flattened to the ground and normalized, so diagonals do not gain speed. `MovementSettings` configures walk/run/backward/strafe speeds, acceleration, deceleration, air control, gravity, jump impulse, turn speed and character collision dimensions.

The controller derives `Idle`, `Walking`, `Running`, `Jumping` or `Falling` from its velocity and grounded state. It smooths horizontal velocity and character yaw independently from the camera. Character translation is constrained by world bounds and slides along static box obstacles. The current floor is flat at Y=0.

Hold either mouse button and move to orbit; release it to free the cursor. The wheel changes the chosen zoom. The demo's two visible walls can be used to test character and camera collision.

`CharacterControllerTest` covers camera-relative direction, diagonal normalization, movement acceleration, jumping/gravity/landing and wall blocking. `CharacterEquipmentTest` continues to validate the model, skeleton, equipment and animation-pack loading contracts.

## Layered animation (movement + combat)

Movement and combat are independent. `CharacterController` owns `MovementState` (Idle, Walk, Run, StrafeLeft, StrafeRight, Backward) and drives the lower-body layer (the `Animator`). `CombatController` owns `CombatState` (None, Attack, Attack2, Block, Cast) and drives the upper-body layer through `AnimationController::play_action`. `AnimationMixer` (`src/animation/layered_animation.*`) blends the one-shot action clip over the locomotion pose using a `BoneMask`: `FinalPose = Blend(LowerPose, UpperPose, mask * layerWeight)`. No combined clips exist; attacking never restarts or replaces the locomotion clip, and movement input is never blocked.

- The mask is built from the real skeleton: the first existing candidate of `spine_01`/`Spine`/`spine` plus all its descendants (spine, chest, clavicles, arms, hands, fingers, neck, head). `root`, `pelvis` and legs stay in the lower layer, so action clips cannot displace the character; the controller remains the only owner of position.
- The layer weight blends in over `blend_in`, holds, and blends out over `blend_out` before the action ends (smoothstep).
- `ActionSettings`/`CombatActionDefinition` expose duration, speed, `hit_time`, `blend_in` and `blend_out`. A running action is not restarted; a new one is accepted during blend-out.
- The mixer emits `ActionStart`/`ActionHit`/`ActionEnd` events; `CombatController` translates them to `AttackStart`/`AttackHit`/`AttackEnd` for the `CombatSystem`, which decides on damage/effects. The animation never applies damage.
- Client input: left-click to select a target (click empty space to clear); `TAB` / `Shift+TAB` cycle living hostile targets; `0` clears the target; `1` toggles auto attack. A right-click on an entity also selects it and engages auto attack; a right-click on empty space exits combat. `F3` toggles a console debug line with movement/combat state, lower/upper clips and weights, attack time, mask bone count and active layers. `Block` has no clip in the animation pack and is not bound by default.
- Tests: `AnimationLayersTest` (mask, blend, events, weights) and `CharacterControllerTest` (walk + attack with the real skeleton).

## MMORPG target combat

The client composes the existing independent gameplay systems in `game::combat`: `EntityRegistry` owns player/NPC combat entities, `TargetSystem` owns current/previous target IDs and tab selection, `CombatSystem` runs auto attacks, and `DamageSystem` applies and broadcasts damage events. `TargetFrame` and the renderer's ground ring consume the selected entity as presentation only; they do not own target state. Clicking can select friendly NPCs or corpses for inspection, while TAB cycles only living hostiles that are in the configured range and camera-facing field of view.

Auto attack is independent from WASD. It evaluates a live hostile target, melee range (default 2.5 m), cooldown (default 1.5 s), facing cone, and line of sight before requesting a `CombatController` attack. The animation layer emits the impact event; at that moment combat rechecks that auto attack is still active, the same target remains selected and alive, and range and line of sight still permit the hit before `DamageSystem` applies damage. Clearing/changing target, moving out of reach, losing line of sight, or killing the target prevents a stale impact from dealing damage. Nothing teleports or locks player movement.

The target frame shows name, level, health and selected/dead status; the ground ring is red on a living target and gray on a corpse. Removing a dead NPC invalidates and clears the target on the next target-system update; the client also stops auto attack when its target dies or is removed. `CombatSettings` and `TargetSettings` configure attack reach/speed/damage/facing and TAB range/FOV. Current target ownership is separate from UI/rendering and animation so focus, mouseover, target-of-target, abilities, GCD and server authority can be added without putting those responsibilities in the controller.

## Melee combat locomotion

While a facing target is set (combat engaged), `CharacterController` switches from the single forward run to the 8-direction strafe set of the "Human Melee Animations FREE" pack (male clips): `Melee_CombatIdle`, `Melee_Run_Forward`, `Melee_Run_Backward` and `Melee_StrafeRun_{Left,Right,ForwardLeft,ForwardRight,BackwardLeft,BackwardRight}`. The clip is chosen from the movement direction relative to the target (45-degree sectors). Backward-leaning movement eases toward `combat_backward_speed`. Leaving combat restores the normal idle/run clips. Movement stays on the lower-body layer while an attack runs on the masked upper-body layer, so changing strafe direction does not interrupt the attack.

The pack uses a different skeleton, so `scripts/retarget_melee_animations.js` converts the FBX clips with FBX2glTF (`npm i fbx2gltf`) and retargets them onto the Universal Animation Library skeleton using world-space rotation deltas, writing `assets/animations/generated/melee_combat_ual.glb`. The client merges those clips into the character model with `assets::append_animations`. Regenerate with `node scripts/retarget_melee_animations.js <path to FBX2glTF.exe>`; add clips by extending the `clips` list in the script. The pack should be placed at the repository root in `Human Melee Animations FREE/`; both it and the generated GLB are ignored by Git.

When the generated pack is present, the client composes normal jog/sprint, forward/backward combat runs, and diagonal combat runs from the Human Melee lower-body tracks (pelvis and descendants below `spine_01`) and the matching UAL run's remaining tracks. The pure left/right combat strafes used by A/D retain their original full-body Human Melee clips. Source keys are retimed to the base clip duration, so the new gait changes only the lower body on the composed clips.

The source animation pack and generated output are not included in this repository because the pack's license does not allow public redistribution of the source assets. Obtain the pack directly from its creator, place it at the repository root as `Human Melee Animations FREE/`, and generate the GLB locally. Without the generated file, the client logs a notice and falls back to the built-in UAL locomotion and combat animations.
The upper-body layer corrects the masked branch root (spine_01) to keep the action clip's model-space orientation, so lower-body clips that twist the hips (melee strafe) do not drag the torso and arms of an attack.

Combat clips cross-fade (0.15 s) with matched phase between same-gait clips, and the 8-way direction choice has 12� of hysteresis so a direction near a sector edge does not flip between clips.

The retarget script removes isolated spike frames from the converted rotation tracks (the FBX conversion flips some bones, notably the clavicles, for single frames) and leaves toe bones at rest.
