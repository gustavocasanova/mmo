import json
import os
import struct
import uuid
from pathlib import Path

import bpy


PROJECT_ROOT = Path(__file__).resolve().parent.parent
CHARACTER_PATH = (
    PROJECT_ROOT
    / "personagem"
    / "Universal Base Characters[Standard]"
    / "Universal Base Characters[Standard]"
    / "Base Characters"
    / "Godot - UE"
    / "Superhero_Male_FullBody.gltf"
)
ANIMATION_PATH = PROJECT_ROOT / "personagem" / "Slow Run.fbx"
OUTPUT_PATH = PROJECT_ROOT / "personagem" / "characterRIGGED.glb"

MIXAMO_TO_CHARACTER = {
    "mixamorig:Hips": "pelvis",
    "mixamorig:Spine": "spine_01",
    "mixamorig:Spine1": "spine_02",
    "mixamorig:Spine2": "spine_03",
    "mixamorig:Neck": "neck_01",
    "mixamorig:Head": "Head",
    "mixamorig:LeftShoulder": "clavicle_l",
    "mixamorig:LeftArm": "upperarm_l",
    "mixamorig:LeftForeArm": "lowerarm_l",
    "mixamorig:LeftHand": "hand_l",
    "mixamorig:RightShoulder": "clavicle_r",
    "mixamorig:RightArm": "upperarm_r",
    "mixamorig:RightForeArm": "lowerarm_r",
    "mixamorig:RightHand": "hand_r",
    "mixamorig:LeftUpLeg": "thigh_l",
    "mixamorig:LeftLeg": "calf_l",
    "mixamorig:LeftFoot": "foot_l",
    "mixamorig:LeftToeBase": "ball_l",
    "mixamorig:RightUpLeg": "thigh_r",
    "mixamorig:RightLeg": "calf_r",
    "mixamorig:RightFoot": "foot_r",
    "mixamorig:RightToeBase": "ball_r",
}

for side, suffix in (("Left", "l"), ("Right", "r")):
    for digit in ("Index", "Middle", "Ring", "Pinky", "Thumb"):
        target_digit = digit.casefold()
        for joint_number in (1, 2, 3):
            MIXAMO_TO_CHARACTER[
                f"mixamorig:{side}Hand{digit}{joint_number}"
            ] = f"{target_digit}_{joint_number:02d}_{suffix}"


def read_glb_json(path):
    with path.open("rb") as glb:
        header = glb.read(12)
        if len(header) != 12:
            raise RuntimeError("Blender exported an incomplete GLB")
        magic, version, total_length = struct.unpack("<4sII", header)
        if magic != b"glTF" or version != 2 or total_length != path.stat().st_size:
            raise RuntimeError("Blender exported an invalid GLB")

        offset = 12
        while offset < total_length:
            chunk_header = glb.read(8)
            if len(chunk_header) != 8:
                raise RuntimeError("GLB contains a truncated chunk header")
            chunk_length, chunk_type = struct.unpack("<II", chunk_header)
            chunk = glb.read(chunk_length)
            if len(chunk) != chunk_length:
                raise RuntimeError("GLB contains a truncated chunk")
            if chunk_type == 0x4E4F534A:
                return json.loads(chunk.decode("utf-8").rstrip(" \t\r\n\0"))
            offset += 8 + chunk_length

    raise RuntimeError("GLB has no JSON chunk")


def import_character():
    if not CHARACTER_PATH.is_file():
        raise FileNotFoundError(f"Character model not found: {CHARACTER_PATH}")
    bpy.ops.import_scene.gltf(filepath=str(CHARACTER_PATH))

    armatures = [obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"]
    if len(armatures) != 1:
        raise RuntimeError(f"Expected one character armature, found {len(armatures)}")
    armature = armatures[0]
    meshes = [
        obj for obj in bpy.context.scene.objects
        if obj.type == "MESH"
        and (
            obj.parent == armature
            or any(modifier.type == "ARMATURE" and modifier.object == armature
                   for modifier in obj.modifiers)
        )
    ]
    if not meshes:
        raise RuntimeError("Character model has no meshes bound to its armature")
    return armature, meshes


def import_animation():
    if not ANIMATION_PATH.is_file():
        raise FileNotFoundError(f"Run animation not found: {ANIMATION_PATH}")
    bpy.ops.import_scene.fbx(filepath=str(ANIMATION_PATH))

    armatures = [obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"]
    if len(armatures) != 2:
        raise RuntimeError(f"Expected source and target armatures, found {len(armatures)}")
    source = next(obj for obj in armatures if obj.animation_data and
                  obj.animation_data.action)
    action = source.animation_data.action
    source.animation_data.action_slot = action.slots[0]
    return source, action


def create_retargeted_action(source, source_action, target):
    missing_required = [
        bone for bone in (
            "mixamorig:Hips",
            "mixamorig:LeftUpLeg",
            "mixamorig:LeftLeg",
            "mixamorig:RightUpLeg",
            "mixamorig:RightLeg",
        )
        if bone not in source.pose.bones
        or MIXAMO_TO_CHARACTER[bone] not in target.pose.bones
    ]
    if missing_required:
        raise RuntimeError(
            "Run animation and character rigs do not match; missing bones: "
            + ", ".join(missing_required)
        )

    pairs = [
        (source_name, target_name)
        for source_name, target_name in MIXAMO_TO_CHARACTER.items()
        if source_name in source.pose.bones and target_name in target.pose.bones
    ]
    if len(pairs) < 20:
        raise RuntimeError(f"Only {len(pairs)} animation bones could be retargeted")

    scene = bpy.context.scene
    first_frame, last_frame = source_action.frame_range
    first_frame = int(first_frame)
    last_frame = int(last_frame)
    if last_frame <= first_frame:
        raise RuntimeError("Run animation has no usable frame range")

    ordered_pairs = sorted(
        pairs,
        key=lambda pair: len(target.data.bones[pair[1]].parent_recursive),
    )
    target_rest_world = {
        name: (target.matrix_world @ target.data.bones[name].matrix_local)
        .to_quaternion().normalized()
        for _, name in ordered_pairs
    }
    target_parent_rest_world = {
        bone.name: (target.matrix_world @ bone.matrix_local).to_quaternion().normalized()
        for bone in target.data.bones
    }

    scene.frame_set(first_frame)
    source_start_world = {
        source_name: (source.matrix_world @ source.pose.bones[source_name].matrix)
        .to_quaternion().normalized()
        for source_name, _ in ordered_pairs
    }

    action = bpy.data.actions.new("Slow Run")
    target.animation_data_create()
    target.animation_data.action = action
    frame_step = max(1, int(round(scene.render.fps / 30)))
    frames = list(range(first_frame, last_frame + 1, frame_step))
    if frames[-1] != last_frame:
        frames.append(last_frame)

    for frame in frames:
        scene.frame_set(frame)
        desired_world = {}
        for source_name, target_name in ordered_pairs:
            source_world = (
                source.matrix_world @ source.pose.bones[source_name].matrix
            ).to_quaternion().normalized()
            rotation_delta = source_world @ source_start_world[source_name].inverted()
            target_rest = target_rest_world[target_name]
            target_world = (rotation_delta @ target_rest).normalized()

            target_bone = target.data.bones[target_name]
            if target_bone.parent:
                parent_name = target_bone.parent.name
                parent_world = desired_world.get(
                    parent_name, target_parent_rest_world[parent_name]
                )
                parent_rest = target_parent_rest_world[parent_name]
            else:
                parent_world = target.matrix_world.to_quaternion().normalized()
                parent_rest = parent_world

            desired_local = parent_world.inverted() @ target_world
            rest_local = parent_rest.inverted() @ target_rest
            pose_rotation = (rest_local.inverted() @ desired_local).normalized()
            pose_bone = target.pose.bones[target_name]
            pose_bone.rotation_mode = "QUATERNION"
            pose_bone.rotation_quaternion = pose_rotation
            pose_bone.keyframe_insert(
                data_path="rotation_quaternion",
                frame=frame,
                group=target_name,
            )
            desired_world[target_name] = target_world

    scene.frame_start = first_frame
    scene.frame_end = last_frame
    return action, len(pairs)


def build_character():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)

    target, character_meshes = import_character()
    source, source_action = import_animation()
    action, mapped_bones = create_retargeted_action(source, source_action, target)

    source.animation_data.action = None
    bpy.data.actions.remove(source_action)
    for obj in list(bpy.data.objects):
        if obj != target and obj not in character_meshes:
            bpy.data.objects.remove(obj, do_unlink=True)

    bpy.ops.object.select_all(action="DESELECT")
    target.select_set(True)
    for mesh in character_meshes:
        mesh.select_set(True)
    bpy.context.view_layer.objects.active = target

    temporary_path = OUTPUT_PATH.with_name(f".characterRIGGED-{uuid.uuid4().hex}.glb")
    try:
        result = bpy.ops.export_scene.gltf(
            filepath=str(temporary_path),
            export_format="GLB",
            use_selection=True,
            export_animations=True,
            export_animation_mode="ACTIONS",
            export_nla_strips=False,
            export_skins=True,
        )
        if "FINISHED" not in result:
            raise RuntimeError(f"Blender GLB export did not finish: {result}")

        glb = read_glb_json(temporary_path)
        clips = glb.get("animations", [])
        if not glb.get("skins") or not any(
            "run" in clip.get("name", "").casefold() for clip in clips
        ):
            raise RuntimeError(
                "Exported GLB must contain the skinned character and Slow Run clip"
            )
        if len(glb["skins"][0].get("joints", [])) > 65:
            raise RuntimeError("Exported character exceeds the 65-joint runtime limit")

        os.replace(temporary_path, OUTPUT_PATH)
        print(
            f"[MMO] Exported {len(character_meshes)} character meshes, "
            f"{mapped_bones} retargeted bones and clip '{action.name}' to {OUTPUT_PATH}"
        )
    finally:
        if temporary_path.exists():
            temporary_path.unlink()


build_character()
