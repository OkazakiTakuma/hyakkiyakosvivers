const fs = require("fs");
const path = require("path");

const root = path.resolve(__dirname, "..");
const generatorName = "generate_player_entrance_animations.js";

function multiplyQuaternion(left, right) {
  return [
    left[3] * right[0] + left[0] * right[3] + left[1] * right[2] - left[2] * right[1],
    left[3] * right[1] - left[0] * right[2] + left[1] * right[3] + left[2] * right[0],
    left[3] * right[2] + left[0] * right[1] - left[1] * right[0] + left[2] * right[3],
    left[3] * right[3] - left[0] * right[0] - left[1] * right[1] - left[2] * right[2],
  ];
}

function normalizeQuaternion(value) {
  const length = Math.hypot(value[0], value[1], value[2], value[3]);
  return value.map((component) => component / length);
}

function quaternionFromEuler([x, y, z]) {
  const halfX = x * 0.5;
  const halfY = y * 0.5;
  const halfZ = z * 0.5;
  const qx = [Math.sin(halfX), 0, 0, Math.cos(halfX)];
  const qy = [0, Math.sin(halfY), 0, Math.cos(halfY)];
  const qz = [0, 0, Math.sin(halfZ), Math.cos(halfZ)];
  return normalizeQuaternion(multiplyQuaternion(qz, multiplyQuaternion(qy, qx)));
}

function findArrayBounds(source, key) {
  const keyIndex = source.indexOf(`"${key}"`);
  if (keyIndex < 0) throw new Error(`${key} array was not found`);
  const openIndex = source.indexOf("[", keyIndex);
  let depth = 0;
  let inString = false;
  let escaped = false;
  for (let index = openIndex; index < source.length; ++index) {
    const character = source[index];
    if (inString) {
      if (escaped) escaped = false;
      else if (character === "\\") escaped = true;
      else if (character === '"') inString = false;
      continue;
    }
    if (character === '"') inString = true;
    else if (character === "[") ++depth;
    else if (character === "]" && --depth === 0) return { openIndex, closeIndex: index };
  }
  throw new Error(`${key} array is not closed`);
}

function appendArrayItems(source, key, items, newline) {
  const { openIndex, closeIndex } = findArrayBounds(source, key);
  const lineStart = source.lastIndexOf(newline, closeIndex - 1) + newline.length;
  const closingIndent = source.slice(lineStart, closeIndex);
  const indentationUnit = closingIndent.includes("\t") ? "\t" : "  ";
  const childIndent = closingIndent + indentationUnit;
  const serialized = items
    .map((item) => JSON.stringify(item, null, 2).split("\n").map((line) => childIndent + line).join(newline))
    .join(`,${newline}`);
  const hasItems = source.slice(openIndex + 1, closeIndex).trim().length > 0;
  const insertionStart = /^\s*$/.test(closingIndent) ? lineStart : closeIndex;
  const insertion = `${hasItems ? "," : ""}${newline}${serialized}${newline}${closingIndent}`;
  return source.slice(0, insertionStart) + insertion + source.slice(closeIndex);
}

function rotationTrack(node, frames) {
  return { node, path: "rotation", frames };
}

function translationTrack(node, frames) {
  return { node, path: "translation", frames };
}

function replaceAccessorMaximum(source, accessorName, maximum) {
  const escapedName = accessorName.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const pattern = new RegExp(`("name"\\s*:\\s*"${escapedName}"[\\s\\S]*?"max"\\s*:\\s*\\[\\s*)[-+0-9.eE]+`);
  if (!pattern.test(source)) throw new Error(`${accessorName}: maximum was not found`);
  return source.replace(pattern, `$1${maximum}`);
}

const models = [
  {
    gltf: "project/Resources/human/miko.gltf",
    bin: "miko_entrance.bin",
    animation: "選択登場_巫女",
    times: [0.0, 0.32, 0.78, 1.20, 1.60],
    tracks: [
      rotationTrack("spine", [[-0.65, 0, 0], [-0.72, 0, 0], [0.22, 0, 0], [-0.12, 0, 0], [-0.16, 0, 0]]),
      rotationTrack("head", [[0.35, 0, 0], [0.30, 0, 0], [-0.16, 0, 0], [0.08, 0, 0], [0.10, 0, 0]]),
      rotationTrack("arm.L", [[0, 0, -0.35], [0, 0, -0.95], [0, 0, 0.75], [0, 0, 0.35], [0, 0, 0.48]]),
      rotationTrack("arm.R", [[0, 0, 0.35], [0, 0, 0.95], [0, 0, -0.75], [0, 0, -0.35], [0, 0, -0.48]]),
      rotationTrack("tail", [[0, -0.35, 0], [0, 0.48, 0], [0, -0.32, 0], [0, 0.16, 0], [0, 0.12, 0]]),
    ],
  },
  {
    gltf: "project/Resources/doppelganger/doppelganger.gltf",
    bin: "doppelganger_entrance.bin",
    animation: "選択登場_烏天狗",
    times: [0.0, 0.25, 0.65, 1.10, 1.45],
    tracks: [
      rotationTrack("spine", [[0, 1.00, -0.25], [0, 0.78, -0.18], [0, -0.36, 0.14], [0, 0.16, -0.07], [0, 0.22, -0.08]]),
      rotationTrack("head", [[0, -0.50, 0], [0, -0.38, 0], [0, 0.26, 0], [0, -0.10, 0], [0, -0.12, 0]]),
      rotationTrack("arm.L", [[-0.55, 0, 1.28], [-0.38, 0, 0.92], [0.34, 0, -0.88], [-0.12, 0, 0.30], [-0.16, 0, 0.48]]),
      rotationTrack("arm.R", [[0.58, 0, -1.22], [0.40, 0, -0.86], [-0.36, 0, 0.96], [0.14, 0, -0.32], [0.18, 0, -0.62]]),
      rotationTrack("pelvis", [[0, 0.58, 0], [0, 0.42, 0], [0, -0.20, 0], [0, 0.08, 0], [0, 0.12, 0]]),
    ],
  },
  {
    gltf: "project/Resources/cat/neko.gltf",
    bin: "neko_entrance.bin",
    animation: "選択登場_猫",
    times: [0.0, 0.30, 0.78, 1.30, 1.75],
    tracks: [
      rotationTrack("spine", [[0.90, 0, 0], [1.05, 0, 0], [-0.38, 0, 0], [0.20, 0, 0], [0.28, 0, 0]]),
      rotationTrack("head", [[-0.48, 0, 0], [-0.58, 0, 0], [0.34, 0, 0], [-0.14, 0, 0], [-0.14, 0, 0]]),
      rotationTrack("arm.L", [[-0.30, 0, 0.92], [-0.22, 0, 1.22], [0.18, 0, -0.52], [-0.08, 0, 0.20], [-0.10, 0, 0.34]]),
      rotationTrack("arm.R", [[-0.30, 0, -0.92], [-0.22, 0, -1.22], [0.18, 0, 0.52], [-0.08, 0, -0.20], [-0.10, 0, -0.34]]),
      rotationTrack("tail", [[0, -0.78, 0.30], [0, 0.92, -0.36], [0, -0.72, 0.28], [0, 0.38, -0.16], [0, 0.48, -0.14]]),
    ],
  },
  {
    gltf: "project/Resources/human/miko.gltf",
    bin: "miko_game_start.bin",
    animation: "ゲーム開始_巫女",
    times: [0.0, 0.28, 0.66, 1.02, 1.35],
    tracks: [
      rotationTrack("spine", [[-0.30, 0, 0], [-0.46, 0, 0], [0.16, 0, 0], [-0.08, 0, 0], [0, 0, 0]]),
      rotationTrack("head", [[0.20, 0, 0], [0.28, 0, 0], [-0.10, 0, 0], [0.04, 0, 0], [0, 0, 0]]),
      rotationTrack("arm.L", [[-0.18, 0.12, -0.82], [-0.34, 0.18, -1.08], [0.14, -0.08, 0.48], [-0.06, 0.04, -0.22], [0, 0, 0]]),
      rotationTrack("arm.R", [[-0.18, -0.12, 0.82], [-0.34, -0.18, 1.08], [0.14, 0.08, -0.48], [-0.06, -0.04, 0.22], [0, 0, 0]]),
      rotationTrack("tail", [[0, -0.32, 0], [0, 0.45, 0], [0, -0.38, 0], [0, 0.16, 0], [0, 0, 0]]),
    ],
  },
  {
    gltf: "project/Resources/doppelganger/doppelganger.gltf",
    bin: "doppelganger_game_start.bin",
    animation: "ゲーム開始_烏天狗",
    times: [0.0, 0.22, 0.58, 0.96, 1.35],
    tracks: [
      rotationTrack("spine", [[-0.38, 0.52, -0.24], [-0.50, 0.30, -0.18], [0.24, -0.24, 0.12], [-0.10, 0.12, -0.06], [0, 0, 0]]),
      rotationTrack("head", [[0.18, -0.34, 0], [0.26, -0.20, 0], [-0.12, 0.18, 0], [0.05, -0.08, 0], [0, 0, 0]]),
      rotationTrack("arm.L", [[-0.62, 0.10, 1.18], [-0.76, 0.16, 1.38], [0.30, -0.08, -0.58], [-0.14, 0.04, 0.24], [0, 0, 0]]),
      rotationTrack("arm.R", [[0.34, -0.18, -1.34], [0.48, -0.24, -1.58], [-0.28, 0.12, 0.72], [0.12, -0.05, -0.30], [0, 0, 0]]),
      rotationTrack("pelvis", [[0, 0.42, -0.12], [0, 0.28, -0.08], [0, -0.18, 0.06], [0, 0.08, -0.03], [0, 0, 0]]),
    ],
  },
  {
    gltf: "project/Resources/cat/neko.gltf",
    bin: "neko_game_start.bin",
    animation: "ゲーム開始_猫",
    times: [0.0, 0.24, 0.62, 1.00, 1.35],
    tracks: [
      rotationTrack("spine", [[0.72, 0, 0], [1.10, 0, 0], [-0.46, 0, 0], [0.18, 0, 0], [0, 0, 0]]),
      rotationTrack("head", [[-0.42, 0, 0], [-0.62, 0, 0], [0.30, 0, 0], [-0.12, 0, 0], [0, 0, 0]]),
      rotationTrack("arm.L", [[-0.42, 0, 0.86], [-0.64, 0, 1.18], [0.28, 0, -0.48], [-0.10, 0, 0.18], [0, 0, 0]]),
      rotationTrack("arm.R", [[-0.42, 0, -0.86], [-0.64, 0, -1.18], [0.28, 0, 0.48], [-0.10, 0, -0.18], [0, 0, 0]]),
      rotationTrack("tail", [[0, -0.66, 0.28], [0, 1.02, -0.40], [0, -0.82, 0.34], [0, 0.34, -0.14], [0, 0, 0]]),
    ],
  },
  {
    gltf: "project/Resources/human/miko.gltf",
    bin: "miko_defeat_baked.bin",
    animation: "力尽きる完全_巫女",
    times: [0.0, 0.18, 0.42, 0.78, 1.16, 1.52],
    tracks: [
      rotationTrack("root", [[0, 0, 0], [-0.08, 0, 0.04], [0.04, 0, 0.18], [0.08, 0, 0.62], [0.04, 0, 1.14], [0, 0, 1.36]]),
      translationTrack("root", [[0, 0, 0], [0, 0.03, 0], [-0.03, -0.02, 0], [-0.10, -0.06, 0], [-0.18, -0.10, 0], [-0.22, -0.12, 0]]),
      rotationTrack("spine", [[0, 0, 0], [-0.22, 0, 0], [0.10, 0, 0.05], [0.40, 0, 0.14], [0.66, 0, 0.24], [0.76, 0, 0.30]]),
      rotationTrack("head", [[0, 0, 0], [0.14, 0, 0], [-0.08, 0, 0], [-0.24, 0, -0.06], [-0.42, 0, -0.12], [-0.50, 0, -0.16]]),
      rotationTrack("arm.L", [[0, 0, 0], [-0.18, 0, -0.22], [-0.42, 0.06, -0.46], [-0.18, 0.10, -0.78], [0.12, 0.08, -1.02], [0.24, 0.04, -1.12]]),
      rotationTrack("arm.R", [[0, 0, 0], [-0.18, 0, 0.22], [-0.42, -0.06, 0.46], [-0.18, -0.10, 0.78], [0.12, -0.08, 1.02], [0.24, -0.04, 1.12]]),
      rotationTrack("leg.L", [[0, 0, 0], [0.06, 0, 0], [0.30, 0, 0.08], [0.54, 0, 0.16], [0.68, 0, 0.20], [0.72, 0, 0.22]]),
      rotationTrack("leg.R", [[0, 0, 0], [0.04, 0, 0], [0.20, 0, -0.06], [0.42, 0, -0.12], [0.54, 0, -0.16], [0.58, 0, -0.18]]),
      rotationTrack("tail", [[0, 0, 0], [0, 0.10, 0], [0, -0.18, 0.08], [0, -0.34, 0.15], [0, -0.48, 0.20], [0, -0.56, 0.22]]),
    ],
  },
  {
    gltf: "project/Resources/doppelganger/doppelganger.gltf",
    bin: "doppelganger_defeat_baked.bin",
    animation: "力尽きる完全_烏天狗",
    times: [0.0, 0.16, 0.38, 0.72, 1.10, 1.48],
    tracks: [
      rotationTrack("root", [[0, 0, 0], [-0.12, 0, -0.06], [0.06, 0, -0.24], [0.12, 0, -0.72], [0.06, 0, -1.22], [0, 0, -1.42]]),
      translationTrack("root", [[0, 0, 0], [0, 0.04, 0], [0.04, 0, 0], [0.13, -0.05, 0], [0.23, -0.10, 0], [0.28, -0.13, 0]]),
      rotationTrack("pelvis", [[0, 0, 0], [0, -0.18, 0.08], [0, 0.26, -0.14], [0.08, 0.44, -0.28], [0.14, 0.54, -0.38], [0.18, 0.58, -0.42]]),
      rotationTrack("spine", [[0, 0, 0], [-0.28, -0.16, 0.14], [-0.10, 0.30, -0.20], [0.34, 0.48, -0.38], [0.58, 0.56, -0.52], [0.68, 0.58, -0.58]]),
      rotationTrack("head", [[0, 0, 0], [0.20, 0.16, -0.08], [-0.12, -0.24, 0.12], [-0.30, -0.34, 0.20], [-0.44, -0.40, 0.26], [-0.50, -0.42, 0.28]]),
      rotationTrack("arm.L", [[0, 0, 0], [-0.44, 0.10, 0.62], [-0.72, 0.18, 0.98], [-0.34, 0.26, 1.24], [0.10, 0.30, 1.40], [0.28, 0.32, 1.48]]),
      rotationTrack("arm.R", [[0, 0, 0], [0.34, -0.12, -0.48], [0.58, -0.20, -0.76], [0.30, -0.28, -1.02], [-0.08, -0.32, -1.28], [-0.24, -0.34, -1.40]]),
      rotationTrack("leg.L", [[0, 0, 0], [0.10, 0, 0], [0.38, 0, 0.12], [0.64, 0, 0.22], [0.76, 0, 0.30], [0.80, 0, 0.34]]),
      rotationTrack("leg.R", [[0, 0, 0], [-0.04, 0, 0], [0.18, 0, -0.10], [0.46, 0, -0.22], [0.62, 0, -0.30], [0.68, 0, -0.34]]),
    ],
  },
  {
    gltf: "project/Resources/cat/neko.gltf",
    bin: "neko_defeat_baked.bin",
    animation: "力尽きる完全_猫",
    times: [0.0, 0.14, 0.34, 0.64, 1.02, 1.46],
    tracks: [
      rotationTrack("root", [[0, 0, 0], [0.14, 0, 0.03], [0.26, 0, 0.16], [0.18, 0, 0.54], [0.08, 0, 1.02], [0, 0, 1.24]]),
      translationTrack("root", [[0, 0, 0], [0, -0.01, 0], [-0.02, -0.05, 0], [-0.07, -0.10, 0], [-0.14, -0.15, 0], [-0.17, -0.17, 0]]),
      rotationTrack("pelvis", [[0, 0, 0], [-0.14, 0, 0], [0.20, 0, 0.08], [0.48, 0, 0.18], [0.62, 0, 0.26], [0.66, 0, 0.30]]),
      rotationTrack("spine", [[0, 0, 0], [0.42, 0, 0], [0.78, 0, 0.08], [0.32, 0, 0.18], [0.54, 0, 0.28], [0.62, 0, 0.34]]),
      rotationTrack("head", [[0, 0, 0], [-0.26, 0, 0], [-0.48, 0, -0.06], [-0.18, 0, -0.14], [-0.36, 0, -0.22], [-0.44, 0, -0.26]]),
      rotationTrack("arm.L", [[0, 0, 0], [-0.36, 0, 0.42], [-0.72, 0, 0.76], [-0.22, 0.06, 1.00], [0.14, 0.08, 1.18], [0.24, 0.08, 1.26]]),
      rotationTrack("arm.R", [[0, 0, 0], [-0.36, 0, -0.42], [-0.72, 0, -0.76], [-0.22, -0.06, -1.00], [0.14, -0.08, -1.18], [0.24, -0.08, -1.26]]),
      rotationTrack("leg.L", [[0, 0, 0], [0.24, 0, 0.12], [0.58, 0, 0.28], [0.76, 0, 0.42], [0.84, 0, 0.50], [0.86, 0, 0.54]]),
      rotationTrack("leg.R", [[0, 0, 0], [0.24, 0, -0.12], [0.58, 0, -0.28], [0.76, 0, -0.42], [0.84, 0, -0.50], [0.86, 0, -0.54]]),
      rotationTrack("tail", [[0, 0, 0], [0, 0.72, -0.24], [0, -0.88, 0.34], [0, 0.46, -0.30], [0, -0.24, 0.18], [0, -0.48, 0.26]]),
    ],
  },
];

for (const definition of models) {
  const gltfPath = path.join(root, definition.gltf);
  let source = fs.readFileSync(gltfPath, "utf8");
  const newline = source.includes("\r\n") ? "\r\n" : "\n";
  const gltf = JSON.parse(source);
  const animationAlreadyExists = gltf.animations.some((animation) => animation.name === definition.animation);

  const nodeIndices = new Map(gltf.nodes.map((node, index) => [node.name, index]));
  const chunks = [];
  let byteLength = 0;
  function appendFloats(values) {
    const padding = (4 - (byteLength % 4)) % 4;
    if (padding > 0) {
      chunks.push(Buffer.alloc(padding));
      byteLength += padding;
    }
    const offset = byteLength;
    const chunk = Buffer.alloc(values.length * 4);
    values.forEach((value, index) => chunk.writeFloatLE(value, index * 4));
    chunks.push(chunk);
    byteLength += chunk.length;
    return { offset, length: chunk.length };
  }

  const newBufferIndex = gltf.buffers.length;
  const newBufferViews = [];
  const newAccessors = [];
  const timeBytes = appendFloats(definition.times);
  newBufferViews.push({ buffer: newBufferIndex, byteOffset: timeBytes.offset, byteLength: timeBytes.length });
  const timeAccessorIndex = gltf.accessors.length;
  newAccessors.push({
    name: `${definition.animation}_時間`,
    bufferView: gltf.bufferViews.length,
    componentType: 5126,
    count: definition.times.length,
    type: "SCALAR",
    min: [definition.times[0]],
    max: [definition.times[definition.times.length - 1]],
  });

  const samplers = [];
  const channels = [];
  for (const track of definition.tracks) {
    const nodeIndex = nodeIndices.get(track.node);
    if (nodeIndex === undefined) throw new Error(`${definition.gltf}: node ${track.node} was not found`);
    if (track.frames.length !== definition.times.length) {
      throw new Error(`${definition.animation}/${track.node}/${track.path}: keyframe count mismatch`);
    }
    let values;
    let accessorType;
    let accessorSuffix;
    if (track.path === "rotation") {
      const baseRotation = gltf.nodes[nodeIndex].rotation || [0, 0, 0, 1];
      values = track.frames.flatMap((euler) =>
        normalizeQuaternion(multiplyQuaternion(quaternionFromEuler(euler), baseRotation)),
      );
      accessorType = "VEC4";
      accessorSuffix = "回転";
    } else if (track.path === "translation") {
      const baseTranslation = gltf.nodes[nodeIndex].translation || [0, 0, 0];
      values = track.frames.flatMap((offset) => [
        baseTranslation[0] + offset[0],
        baseTranslation[1] + offset[1],
        baseTranslation[2] + offset[2],
      ]);
      accessorType = "VEC3";
      accessorSuffix = "移動";
    } else {
      throw new Error(`${definition.animation}: unsupported track path ${track.path}`);
    }
    const valueBytes = appendFloats(values);
    const bufferViewIndex = gltf.bufferViews.length + newBufferViews.length;
    newBufferViews.push({ buffer: newBufferIndex, byteOffset: valueBytes.offset, byteLength: valueBytes.length });
    const accessorIndex = gltf.accessors.length + newAccessors.length;
    newAccessors.push({
      name: `${definition.animation}_${track.node}_${accessorSuffix}`,
      bufferView: bufferViewIndex,
      componentType: 5126,
      count: definition.times.length,
      type: accessorType,
    });
    samplers.push({ input: timeAccessorIndex, interpolation: "LINEAR", output: accessorIndex });
    channels.push({ sampler: samplers.length - 1, target: { node: nodeIndex, path: track.path } });
  }

  const binary = Buffer.concat(chunks);
  if (binary.length !== byteLength) throw new Error(`${definition.bin}: byte length mismatch`);
  fs.writeFileSync(path.join(path.dirname(gltfPath), definition.bin), binary);

  if (animationAlreadyExists) {
	source = replaceAccessorMaximum(source, `${definition.animation}_時間`, definition.times[definition.times.length - 1]);
	JSON.parse(source);
	fs.writeFileSync(gltfPath, source, "utf8");
    console.log(`updated ${definition.animation}: ${definition.bin} (${byteLength} bytes)`);
    continue;
  }

  source = appendArrayItems(source, "animations", [{
    name: definition.animation,
    samplers,
    channels,
    extras: { generatedBy: generatorName },
  }], newline);
  source = appendArrayItems(source, "accessors", newAccessors, newline);
  source = appendArrayItems(source, "bufferViews", newBufferViews, newline);
  source = appendArrayItems(source, "buffers", [{ byteLength, uri: definition.bin }], newline);
  JSON.parse(source);
  fs.writeFileSync(gltfPath, source, "utf8");
  console.log(`generated ${definition.animation}: ${definition.bin} (${byteLength} bytes)`);
}
