"""Merge a Mixamo character FBX and its animation FBX files into one glTF.

The engine imports one Model per file and takes every AnimationClip from that
same file (renderer/Render/Mesh/Model.cpp, ImportAnimations), so a character and
its clips have to live in one file before an Animator can be built out of them.
Mixamo hands out one FBX per clip and names every take "mixamo.com", which is
why the clips are renamed after their file here.

Usage (Blender 2.93+, headless):

    blender -b --python tools/mixamo_to_gltf.py -- <folder> <out.gltf> [model.fbx]

`folder` is scanned for *.fbx: the file given as `model.fbx` (default: the
largest one) is the skinned character, every other file becomes one clip named
after the file. The result is written as glTF with separate .bin and textures,
which is what the engine's texture loading expects (it resolves every texture
on disk relative to the model file, embedded ones cannot be read).
"""

import os
import sys

import bpy
import mathutils


def log(message):
    print("[mixamo] " + message)


def script_args():
    argv = sys.argv
    if "--" not in argv:
        raise SystemExit(__doc__)

    args = argv[argv.index("--") + 1:]
    if len(args) < 2:
        raise SystemExit(__doc__)

    folder, out = args[0], args[1]
    model = args[2] if len(args) > 2 else None

    sources = sorted(
        os.path.join(folder, name)
        for name in os.listdir(folder)
        if name.lower().endswith(".fbx")
    )
    if not sources:
        raise SystemExit("[mixamo] no .fbx in " + folder)

    if model is None:
        #the skinned character is the big file, the clips are skeleton only
        model = max(sources, key=os.path.getsize)
    else:
        model = os.path.join(folder, model)
        if model not in sources:
            raise SystemExit("[mixamo] " + model + " is not in " + folder)

    return model, out, [path for path in sources if path != model]


def supported(operator, kwargs):
    #the exporter gained and lost properties between Blender versions
    properties = {prop.identifier for prop in operator.get_rna_type().properties}
    return {key: value for key, value in kwargs.items() if key in properties}


def import_fbx(path):
    """Imports one FBX and returns what it added to the file."""
    objects_before = set(bpy.data.objects)
    actions_before = set(bpy.data.actions)

    bpy.ops.import_scene.fbx(filepath=path,
                             global_scale=1.0,
                             use_anim=True,
                             ignore_leaf_bones=False,
                             automatic_bone_orientation=False)

    objects = [obj for obj in bpy.data.objects if obj not in objects_before]
    actions = [action for action in bpy.data.actions if action not in actions_before]
    return objects, actions


def armature_of(objects):
    for obj in objects:
        if obj.type == "ARMATURE":
            return obj

    return None


def drop(objects):
    for obj in objects:
        bpy.data.objects.remove(obj, do_unlink=True)


def turn_to_engine_forward(armature, meshes):
    """Turns the rig a half turn about the up axis of the scene it is authored in.

    The engine imports with aiProcess_MakeLeftHanded, and assimp reflects the
    scene in its Z axis to do that: a character authored facing +Z - the glTF
    convention - arrives in the engine facing -Z, which is the opposite of the
    engine's own forward (Vec3f::forward is +Z, the way a camera at yaw 0
    looks). A character that arrives facing -Z walks away from a third person
    camera with its face turned to it. A half turn here lands the front of the
    rig on the engine's forward again, where every scene and controller expects
    a model to look, so nothing downstream has to correct the facing of one
    particular character.

    The clips are bone local, so they follow the turn; the meshes are children
    of the armature and follow it too, and they are turned with it here so the
    scene stays coherent even if a future export stops parenting them.
    """
    turn = mathutils.Matrix.Rotation(3.14159265358979323846, 4, "Z")

    for obj in [armature] + list(meshes):
        obj.matrix_world = turn @ obj.matrix_world

    log("turned {0} and {1} mesh(es) a half turn to face the engine's forward".format(
        armature.name, len(meshes)))


def main():
    model_path, out_path, clip_paths = script_args()

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.render.fps = 30
    bpy.context.scene.frame_start = 0

    log("character: " + model_path)
    objects, _ = import_fbx(model_path)

    armature = armature_of(objects)
    if armature is None:
        raise SystemExit("[mixamo] no armature in " + model_path)

    #the take of the character file is the T pose hold the rig already rests in
    if armature.animation_data is not None:
        armature.animation_data.action = None

    bones = [bone.name for bone in armature.pose.bones]
    mesh_objects = [obj for obj in objects if obj.type == "MESH"]
    meshes = [obj.name for obj in mesh_objects]
    log("bones: {0}, meshes: {1}".format(len(bones), ", ".join(meshes)))

    #the scene the engine loads has its forward on the engine's +Z
    turn_to_engine_forward(armature, mesh_objects)

    armature.animation_data_create()
    tracks = armature.animation_data.nla_tracks

    for clip_path in clip_paths:
        name = os.path.splitext(os.path.basename(clip_path))[0]
        clip_objects, clip_actions = import_fbx(clip_path)

        if not clip_actions:
            log("skip '{0}': no action".format(name))
            drop(clip_objects)
            continue

        first = float("inf")
        last = float("-inf")

        for action in clip_actions:
            #the take of a Mixamo FBX is always called "mixamo.com"
            action.name = name
            action.use_fake_user = True

            #Mixamo bakes its takes from frame 1, and the exporter keeps that
            #offset, so the clip would hold its first pose for a frame whenever
            #it loops; moving the keys to start at frame 0 makes t = 0 the first
            #key of every clip
            offset = action.frame_range[0]
            for fcurve in action.fcurves:
                for key in fcurve.keyframe_points:
                    key.co.x -= offset
                fcurve.update()

            #bones are addressed by name by the fcurves, and every Mixamo rig
            #uses the same names, so the action drives this armature as well
            track = tracks.new()
            track.name = name
            strip = track.strips.new(name, int(action.frame_range[0]), action)
            strip.blend_type = "REPLACE"
            #a strip holds its first frame before it starts, which would leave
            #the rig posed at the frame the export poses the rest of the scene
            #in; the exporter has to see the armature in its rest pose so the
            #joint nodes it writes match the inverse bind matrices of the skin
            strip.extrapolation = "NOTHING"

            first = min(first, action.frame_range[0])
            last = max(last, action.frame_range[1])
            log("clip '{0}': frames {1:.0f}-{2:.0f} ({3:.2f}s), "
                "{4} fcurves".format(name, action.frame_range[0], action.frame_range[1],
                                     (action.frame_range[1] - action.frame_range[0]) / 30.0,
                                     len(action.fcurves)))

        drop(clip_objects)

    #Mixamo brings no metallic map, and a material that never set it exports
    #with the glTF default of 0.5
    for material in bpy.data.materials:
        if material.use_nodes:
            for node in material.node_tree.nodes:
                if node.type == "BSDF_PRINCIPLED" and "Metallic" in node.inputs:
                    node.inputs["Metallic"].default_value = 0.0
        material.metallic = 0.0

    bpy.context.scene.frame_set(0)
    bpy.ops.object.select_all(action="SELECT")
    bpy.context.view_layer.objects.active = armature

    log("exporting {0} clips to {1}".format(len(tracks), out_path))
    bpy.ops.export_scene.gltf(**supported(bpy.ops.export_scene.gltf, {
        "filepath": os.path.abspath(out_path),
        "check_existing": False,
        "export_format": "GLTF_SEPARATE",
        "use_selection": True,
        "export_selected": True,
        "export_yup": True,
        "export_apply": False,
        "export_texcoords": True,
        "export_normals": True,
        "export_tangents": False,
        "export_colors": False,
        "export_materials": "EXPORT",
        "export_image_format": "AUTO",
        "export_skins": True,
        "export_all_influences": False,
        "export_morph": False,
        "export_animations": True,
        #every NLA track is one clip and keeps its own frame range
        "export_nla_strips": True,
        "export_frame_range": False,
        "export_frame_step": 1,
        "export_force_sampling": True,
        "export_current_frame": False,
        "export_cameras": False,
        "export_lights": False,
        "export_extras": False,
    }))
    log("done")


main()
