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

const models = [
  {
    gltf: "project/Resources/human/miko.gltf",
    bin: "miko_entrance.bin",
    animation: "選択登場_巫女",
    times: [0.0, 0.18, 0.42, 0.68, 0.85],
    tracks: [
      rotationTrack("spine", [[-0.38, 0, 0], [-0.42, 0, 0], [0.10, 0, 0], [-0.04, 0, 0], [0, 0, 0]]),
      rotationTrack("head", [[0.22, 0, 0], [0.20, 0, 0], [-0.08, 0, 0], [0.03, 0, 0], [0, 0, 0]]),
      rotationTrack("arm.L", [[0, 0, -0.18], [0, 0, -0.46], [0, 0, 0.34], [0, 0, 0.10], [0, 0, 0]]),
      rotationTrack("arm.R", [[0, 0, 0.18], [0, 0, 0.46], [0, 0, -0.34], [0, 0, -0.10], [0, 0, 0]]),
      rotationTrack("tail", [[0, -0.18, 0], [0, 0.20, 0], [0, -0.12, 0], [0, 0.05, 0], [0, 0, 0]]),
    ],
  },
  {
    gltf: "project/Resources/doppelganger/doppelganger.gltf",
    bin: "doppelganger_entrance.bin",
    animation: "選択登場_烏天狗",
    times: [0.0, 0.16, 0.38, 0.64, 0.85],
    tracks: [
      rotationTrack("spine", [[0, 0.72, -0.14], [0, 0.52, -0.10], [0, -0.20, 0.07], [0, 0.08, -0.03], [0, 0, 0]]),
      rotationTrack("head", [[0, -0.30, 0], [0, -0.22, 0], [0, 0.14, 0], [0, -0.05, 0], [0, 0, 0]]),
      rotationTrack("arm.L", [[-0.35, 0, 0.92], [-0.20, 0, 0.58], [0.20, 0, -0.54], [-0.06, 0, 0.16], [0, 0, 0]]),
      rotationTrack("arm.R", [[0.38, 0, -0.88], [0.22, 0, -0.52], [-0.22, 0, 0.60], [0.07, 0, -0.18], [0, 0, 0]]),
      rotationTrack("pelvis", [[0, 0.34, 0], [0, 0.24, 0], [0, -0.10, 0], [0, 0.04, 0], [0, 0, 0]]),
    ],
  },
  {
    gltf: "project/Resources/cat/neko.gltf",
    bin: "neko_entrance.bin",
    animation: "選択登場_猫",
    times: [0.0, 0.20, 0.48, 0.78, 1.05],
    tracks: [
      rotationTrack("spine", [[0.56, 0, 0], [0.70, 0, 0], [-0.20, 0, 0], [0.10, 0, 0], [0, 0, 0]]),
      rotationTrack("head", [[-0.28, 0, 0], [-0.36, 0, 0], [0.20, 0, 0], [-0.08, 0, 0], [0, 0, 0]]),
      rotationTrack("arm.L", [[-0.18, 0, 0.58], [-0.12, 0, 0.82], [0.10, 0, -0.28], [-0.04, 0, 0.10], [0, 0, 0]]),
      rotationTrack("arm.R", [[-0.18, 0, -0.58], [-0.12, 0, -0.82], [0.10, 0, 0.28], [-0.04, 0, -0.10], [0, 0, 0]]),
      rotationTrack("tail", [[0, -0.48, 0.18], [0, 0.58, -0.22], [0, -0.42, 0.16], [0, 0.20, -0.08], [0, 0, 0]]),
    ],
  },
];

for (const definition of models) {
  const gltfPath = path.join(root, definition.gltf);
  let source = fs.readFileSync(gltfPath, "utf8");
  const newline = source.includes("\r\n") ? "\r\n" : "\n";
  const gltf = JSON.parse(source);
  if (gltf.animations.some((animation) => animation.name === definition.animation)) {
    console.log(`${definition.animation} already exists; skipped`);
    continue;
  }

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
    const baseRotation = gltf.nodes[nodeIndex].rotation || [0, 0, 0, 1];
    const rotations = track.frames.flatMap((euler) =>
      normalizeQuaternion(multiplyQuaternion(quaternionFromEuler(euler), baseRotation)),
    );
    const valueBytes = appendFloats(rotations);
    const bufferViewIndex = gltf.bufferViews.length + newBufferViews.length;
    newBufferViews.push({ buffer: newBufferIndex, byteOffset: valueBytes.offset, byteLength: valueBytes.length });
    const accessorIndex = gltf.accessors.length + newAccessors.length;
    newAccessors.push({
      name: `${definition.animation}_${track.node}_回転`,
      bufferView: bufferViewIndex,
      componentType: 5126,
      count: definition.times.length,
      type: "VEC4",
    });
    samplers.push({ input: timeAccessorIndex, interpolation: "LINEAR", output: accessorIndex });
    channels.push({ sampler: samplers.length - 1, target: { node: nodeIndex, path: track.path } });
  }

  const binary = Buffer.concat(chunks);
  if (binary.length !== byteLength) throw new Error(`${definition.bin}: byte length mismatch`);
  fs.writeFileSync(path.join(path.dirname(gltfPath), definition.bin), binary);

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
