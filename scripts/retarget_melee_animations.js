// Retargets selected "Human Melee Animations FREE" FBX clips onto the Universal Animation
// Library skeleton and writes them to a skeleton-only GLB the engine can merge at load time.
//
// Usage: node scripts/retarget_melee_animations.js <FBX2glTF.exe>
// FBX2glTF is available through `npm i fbx2gltf` (bin/Windows_NT/FBX2glTF.exe).
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const root = path.resolve(__dirname, '..');
const packRoot = path.join(root, 'assets', 'animations', 'Human Melee Animations FREE');
const male = path.join(packRoot, 'Animations', 'Male');
const ualPath = path.join(root, 'Universal Animation Library[Standard]', 'Unreal-Godot', 'UAL1_Standard.glb');
const outputPath = path.join(root, 'assets', 'animations', 'generated', 'melee_combat_ual.glb');

const fbx2gltf = process.argv[2];
if (!fbx2gltf) {
    console.error('usage: node scripts/retarget_melee_animations.js <FBX2glTF.exe>');
    process.exit(1);
}

const clips = [
    ['Melee_CombatIdle', 'Combat/HumanM@CombatIdle01.fbx'],
    ['Melee_Run_Forward', 'Movement/Run/HumanM@Run01_Forward.fbx'],
    ['Melee_Run_Backward', 'Movement/Run/HumanM@Run01_Backward.fbx'],
    ['Melee_StrafeRun_Left', 'Movement/Strafe/StrafeRun/HumanM@StrafeRun01_Left.fbx'],
    ['Melee_StrafeRun_Right', 'Movement/Strafe/StrafeRun/HumanM@StrafeRun01_Right.fbx'],
    ['Melee_StrafeRun_ForwardLeft', 'Movement/Strafe/StrafeRun/HumanM@StrafeRun01_ForwardLeft.fbx'],
    ['Melee_StrafeRun_ForwardRight', 'Movement/Strafe/StrafeRun/HumanM@StrafeRun01_ForwardRight.fbx'],
    ['Melee_StrafeRun_BackwardLeft', 'Movement/Strafe/StrafeRun/HumanM@StrafeRun01_BackwardLeft.fbx'],
    ['Melee_StrafeRun_BackwardRight', 'Movement/Strafe/StrafeRun/HumanM@StrafeRun01_BackwardRight.fbx'],
];

// source bone -> UAL bone
const boneMap = { 'B-hips': 'pelvis', 'B-spine': 'spine_01', 'B-chest': 'spine_03', 'B-neck': 'neck_01', 'B-head': 'Head' };
for (const [side, suffix] of [['L', 'l'], ['R', 'r']]) {
    Object.assign(boneMap, {
        [`B-shoulder.${side}`]: `clavicle_${suffix}`,
        [`B-upperArm.${side}`]: `upperarm_${suffix}`,
        [`B-forearm.${side}`]: `lowerarm_${suffix}`,
        [`B-hand.${side}`]: `hand_${suffix}`,
        [`B-thigh.${side}`]: `thigh_${suffix}`,
        [`B-shin.${side}`]: `calf_${suffix}`,
        [`B-foot.${side}`]: `foot_${suffix}`,
        // Toes are left at rest: the converted toe tracks flip erratically between frames.
    });
    for (const finger of ['index', 'middle', 'ring', 'pinky', 'thumb']) {
        for (const n of [1, 2, 3]) {
            boneMap[`B-${finger}Finger0${n}.${side}`.replace(/(thumb|pinky)Finger/, '$1')] = `${finger}_0${n}_${suffix}`;
        }
    }
}

// ---- quaternion / glTF helpers ([x, y, z, w]) ----
const qmul = (a, b) => [
    a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
    a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
    a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
    a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]];
const qinv = (q) => [-q[0], -q[1], -q[2], q[3]];
const qrot = (q, v) => {
    const r = qmul(qmul(q, [v[0], v[1], v[2], 0]), qinv(q));
    return [r[0], r[1], r[2]];
};
const qdot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
const qangle = (a, b) => 2 * Math.acos(Math.min(1, Math.abs(qdot(a, b)))) * 57.29578;

// The FBX conversion leaves isolated frames where a bone is flipped far away from both of
// its neighbours (visible as jitter). Make signs continuous, then replace those frames.
function despike(keys) {
    const out = keys.map((q) => q.slice());
    for (let i = 1; i < out.length; i++) {
        if (qdot(out[i - 1], out[i]) < 0) out[i] = out[i].map((x) => -x);
    }
    const count = out.length;
    let fixed = 0;
    for (let pass = 0; pass < 2; pass++) {
        for (let i = 0; i < count; i++) {
            const previous = out[(i + count - 1) % count];
            const next = out[(i + 1) % count];
            const jump = Math.max(qangle(previous, out[i]), qangle(out[i], next));
            if (jump > 25 && qangle(previous, next) < jump * 0.5) {
                const sign = qdot(previous, next) < 0 ? -1 : 1;
                out[i] = qnorm(previous.map((x, k) => x + next[k] * sign));
                fixed++;
            }
        }
    }
    return { keys: out, fixed };
}
const qnorm = (q) => { const l = Math.hypot(...q) || 1; return q.map((x) => x / l); };

function readGlb(file) {
    const b = fs.readFileSync(file);
    const jsonLength = b.readUInt32LE(12);
    const json = JSON.parse(b.slice(20, 20 + jsonLength).toString());
    const binOffset = 20 + jsonLength + 8;
    return { json, bin: b.slice(binOffset) };
}

function readAccessor(glb, index) {
    const a = glb.json.accessors[index];
    const view = glb.json.bufferViews[a.bufferView];
    const components = { SCALAR: 1, VEC3: 3, VEC4: 4, MAT4: 16 }[a.type];
    const start = (view.byteOffset || 0) + (a.byteOffset || 0);
    const out = [];
    for (let i = 0; i < a.count; i++) {
        const item = [];
        for (let c = 0; c < components; c++) {
            item.push(glb.bin.readFloatLE(start + (i * components + c) * 4));
        }
        out.push(components === 1 ? item[0] : item);
    }
    return out;
}

function nodeTables(json) {
    const parent = new Array(json.nodes.length).fill(-1);
    json.nodes.forEach((n, i) => (n.children || []).forEach((c) => { parent[c] = i; }));
    const byName = new Map(json.nodes.map((n, i) => [n.name, i]));
    return { parent, byName };
}

const restLocal = (n) => ({
    t: n.translation || [0, 0, 0],
    q: n.rotation || [0, 0, 0, 1],
    s: (n.scale || [1, 1, 1])[0],
});

function composeWorld(parentWorld, local) {
    if (!parentWorld) {
        return { p: local.t.slice(), q: local.q.slice(), s: local.s };
    }
    const scaled = local.t.map((x) => x * parentWorld.s);
    const rotated = qrot(parentWorld.q, scaled);
    return {
        p: parentWorld.p.map((x, i) => x + rotated[i]),
        q: qnorm(qmul(parentWorld.q, local.q)),
        s: parentWorld.s * local.s,
    };
}

function restWorlds(json) {
    const { parent } = nodeTables(json);
    const worlds = new Array(json.nodes.length);
    const resolve = (i) => {
        if (worlds[i]) return worlds[i];
        worlds[i] = composeWorld(parent[i] < 0 ? null : resolve(parent[i]), restLocal(json.nodes[i]));
        return worlds[i];
    };
    json.nodes.forEach((_, i) => resolve(i));
    return worlds;
}

// ---- convert FBX files ----
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'melee-'));
const convert = (fbx, name) => {
    const out = path.join(tmp, name);
    execFileSync(fbx2gltf, ['--binary', '--input', fbx, '--output', out], { stdio: 'ignore' });
    return readGlb(out + '.glb');
};

const sourceModel = convert(path.join(packRoot, 'Models', 'HumanM_Model.fbx'), 'model').json;
const sourceTables = nodeTables(sourceModel);
const sourceRest = restWorlds(sourceModel);

const ual = readGlb(ualPath);
const ualTables = nodeTables(ual.json);
const ualRest = restWorlds(ual.json);
const ualSkin = ual.json.skins[0];
const ualJoint = new Map(ualSkin.joints.map((node, i) => [ual.json.nodes[node].name, { node, index: i }]));

const sourceHip = sourceRest[sourceTables.byName.get('B-hips')];
const ualHip = ualRest[ualTables.byName.get('pelvis')];
const sourceFoot = sourceRest[sourceTables.byName.get('B-foot.L')];
const ualFoot = ualRest[ualTables.byName.get('foot_l')];
const sourceToe = sourceRest[sourceTables.byName.get('B-toe.L')];
const ualToe = ualRest[ualTables.byName.get('ball_l')];
console.log('rest hips   src', sourceHip.p.map((x) => x.toFixed(3)), 'ual', ualHip.p.map((x) => x.toFixed(3)));
console.log('rest foot   src', sourceFoot.p.map((x) => x.toFixed(3)), 'ual', ualFoot.p.map((x) => x.toFixed(3)));
console.log('rest toe-foot src', sourceToe.p.map((x, i) => (x - sourceFoot.p[i]).toFixed(3)),
    'ual', ualToe.p.map((x, i) => (x - ualFoot.p[i]).toFixed(3)));
const hipScale = (ualHip.p[1] - ualFoot.p[1]) / (sourceHip.p[1] - sourceFoot.p[1]);
console.log('hip scale', hipScale.toFixed(4));

// Order UAL bones so parents are processed before children.
const ualOrder = [];
{
    const seen = new Set();
    const visit = (node) => {
        if (seen.has(node)) return;
        if (ualTables.parent[node] >= 0) visit(ualTables.parent[node]);
        seen.add(node);
        ualOrder.push(node);
    };
    ualSkin.joints.forEach(visit);
}

const binaryChunks = [];
let binaryLength = 0;
const bufferViews = [];
const accessors = [];
function addAccessor(floats, type, count, extra = {}) {
    const buffer = Buffer.alloc(floats.length * 4);
    floats.forEach((v, i) => buffer.writeFloatLE(v, i * 4));
    const padding = (4 - (binaryLength % 4)) % 4;
    binaryLength += padding;
    binaryChunks.push(Buffer.alloc(padding));
    bufferViews.push({ buffer: 0, byteOffset: binaryLength, byteLength: buffer.length });
    binaryChunks.push(buffer);
    binaryLength += buffer.length;
    accessors.push({ bufferView: bufferViews.length - 1, componentType: 5126, count, type, ...extra });
    return accessors.length - 1;
}

const inverseBindIndex = addAccessor(
    readAccessor(ual, ualSkin.inverseBindMatrices).flat(), 'MAT4', ualSkin.joints.length);

const animations = [];
for (const [clipName, relative] of clips) {
    const glb = convert(path.join(male, relative), clipName);
    const animation = glb.json.animations[0];
    const { byName } = nodeTables(glb.json);
    const tracks = new Map();
    let times = null;
    for (const channel of animation.channels) {
        const sampler = animation.samplers[channel.sampler];
        const nodeName = glb.json.nodes[channel.target.node].name;
        const clipTimes = readAccessor(glb, sampler.input);
        times = times && times.length >= clipTimes.length ? times : clipTimes;
        if (!tracks.has(nodeName)) tracks.set(nodeName, {});
        tracks.get(nodeName)[channel.target.path] = { times: clipTimes, values: readAccessor(glb, sampler.output) };
    }
    const frameCount = times.length;
    for (const track of tracks.values()) {
        for (const channel of Object.values(track)) {
            if (channel.times.length !== frameCount) throw new Error(`${clipName}: mismatched key counts`);
        }
    }

    const localAt = (name, frame) => {
        const rest = restLocal(sourceModel.nodes[sourceTables.byName.get(name)]);
        const track = tracks.get(name) || {};
        return {
            t: track.translation ? track.translation.values[frame] : rest.t,
            q: track.rotation ? track.rotation.values[frame] : rest.q,
            s: track.scale ? track.scale.values[frame][0] : rest.s,
        };
    };
    const sourceWorldAt = (frame) => {
        const worlds = new Array(sourceModel.nodes.length);
        const resolve = (i) => {
            if (worlds[i]) return worlds[i];
            const name = sourceModel.nodes[i].name;
            const local = tracks.has(name) ? localAt(name, frame) : restLocal(sourceModel.nodes[i]);
            worlds[i] = composeWorld(sourceTables.parent[i] < 0 ? null : resolve(sourceTables.parent[i]), local);
            return worlds[i];
        };
        sourceModel.nodes.forEach((_, i) => resolve(i));
        return worlds;
    };

    const rotationKeys = new Map();
    const pelvisTranslations = [];
    for (let frame = 0; frame < frameCount; frame++) {
        const sourceWorld = sourceWorldAt(frame);
        const targetWorld = new Map();
        for (const node of ualOrder) {
            const name = ual.json.nodes[node].name;
            const parentNode = ualTables.parent[node];
            const parentWorld = parentNode >= 0 ? targetWorld.get(parentNode) : null;
            const rest = restLocal(ual.json.nodes[node]);
            let world;
            const sourceName = Object.keys(boneMap).find((k) => boneMap[k] === name);
            if (sourceName) {
                const sourceIndex = sourceTables.byName.get(sourceName);
                const delta = qmul(sourceWorld[sourceIndex].q, qinv(sourceRest[sourceIndex].q));
                const worldQ = qnorm(qmul(delta, ualRest[node].q));
                const parentQ = parentWorld ? parentWorld.q : [0, 0, 0, 1];
                const localQ = qnorm(qmul(qinv(parentQ), worldQ));
                if (!rotationKeys.has(node)) rotationKeys.set(node, []);
                rotationKeys.get(node).push(localQ);
                world = composeWorld(parentWorld, { t: rest.t, q: localQ, s: rest.s });
                if (name === 'pelvis') {
                    const offset = sourceWorld[sourceIndex].p.map((x, i) => (x - sourceRest[sourceIndex].p[i]) * hipScale);
                    const desired = ualRest[node].p.map((x, i) => x + offset[i]);
                    const local = qrot(qinv(parentWorld.q), desired.map((x, i) => x - parentWorld.p[i]))
                        .map((x) => x / parentWorld.s);
                    pelvisTranslations.push(local);
                    world.p = desired;
                }
            } else {
                world = composeWorld(parentWorld, rest);
            }
            targetWorld.set(node, world);
        }
    }

    const timeAccessor = addAccessor(times, 'SCALAR', frameCount, { min: [times[0]], max: [times[frameCount - 1]] });
    const samplers = [];
    const channels = [];
    let despiked = 0;
    for (const [node, rawKeys] of rotationKeys) {
        const { keys, fixed } = despike(rawKeys);
        despiked += fixed;
        const output = addAccessor(keys.flat(), 'VEC4', frameCount);
        samplers.push({ input: timeAccessor, output, interpolation: 'LINEAR' });
        channels.push({ sampler: samplers.length - 1, target: { node, path: 'rotation' } });
    }
    const pelvisNode = ualTables.byName.get('pelvis');
    const translationOutput = addAccessor(pelvisTranslations.flat(), 'VEC3', frameCount);
    samplers.push({ input: timeAccessor, output: translationOutput, interpolation: 'LINEAR' });
    channels.push({ sampler: samplers.length - 1, target: { node: pelvisNode, path: 'translation' } });
    animations.push({ name: clipName, samplers, channels });
    console.log(`${clipName}: ${frameCount} frames, ${channels.length} channels, ${despiked} spike frames fixed`);
}

const nodes = ual.json.nodes.map((n) => {
    const copy = { ...n };
    delete copy.mesh;
    delete copy.skin;
    return copy;
});
const json = {
    asset: { version: '2.0', generator: 'scripts/retarget_melee_animations.js' },
    scene: 0,
    scenes: ual.json.scenes,
    nodes,
    skins: [{ joints: ualSkin.joints, inverseBindMatrices: inverseBindIndex }],
    accessors,
    bufferViews,
    buffers: [{ byteLength: binaryLength }],
    animations,
};
const binary = Buffer.concat(binaryChunks);
const binaryPadded = Buffer.concat([binary, Buffer.alloc((4 - (binary.length % 4)) % 4)]);
json.buffers[0].byteLength = binaryPadded.length;
const finalJson = Buffer.from(JSON.stringify(json));
const finalJsonPadded = Buffer.concat([finalJson, Buffer.alloc((4 - (finalJson.length % 4)) % 4, 0x20)]);
const header = Buffer.alloc(12);
header.write('glTF', 0);
header.writeUInt32LE(2, 4);
header.writeUInt32LE(12 + 8 + finalJsonPadded.length + 8 + binaryPadded.length, 8);
const jsonHeader = Buffer.alloc(8);
jsonHeader.writeUInt32LE(finalJsonPadded.length, 0);
jsonHeader.write('JSON', 4);
const binHeader = Buffer.alloc(8);
binHeader.writeUInt32LE(binaryPadded.length, 0);
binHeader.write('BIN\0', 4);
fs.mkdirSync(path.dirname(outputPath), { recursive: true });
fs.writeFileSync(outputPath, Buffer.concat([header, jsonHeader, finalJsonPadded, binHeader, binaryPadded]));
console.log('wrote', outputPath);
fs.rmSync(tmp, { recursive: true, force: true });
