// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The rivals as an oracle: GSAP, tween.js and
// Motion's springs run under Node, and CSS Color 4's conversions.js, each
// fetched by its pinned version and checked against its pinned hash
// before it runs, never kept in the repository. What they compute for
// curves, springs, colors, staggers and timeline positions Maul Tween
// shares is written, exactly, to test/generated/rival_easing.h,
// rival_spring.h, rival_color.h, rival_stagger.h and rival_position.h,
// which test/test_rival_easing.c, test_rival_spring.c,
// test_rival_color.c, test_rival_stagger.c and test_rival_position.c
// check. Run by hand, not on every commit, and only through
// tools/oracle/run-in-container.sh:
//
//     tools/oracle/run-in-container.sh fetch
//     tools/oracle/run-in-container.sh rivals [--check]
//
// --fetch, on the host, only downloads: each package by npm pack and
// each sample file, into $MTWN_RIVALS_DIR, and refuses any whose hash is
// not the pinned one. It runs none of their code. A run never fetches:
// in a throwaway container without network, it unpacks the fetched
// packages into $MTWN_RIVALS_WORK, runs them and writes the headers to
// $MTWN_RIVALS_OUT; --check compares them with test/generated instead.
// Anywhere else it refuses to run (contained.mjs).

import { execFileSync } from "node:child_process";
import { runInNewContext } from "node:vm";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { createRequire } from "node:module";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { refuseOutsideContainer } from "./contained.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const generated = join(root, "test", "generated");
const fetched = process.env.MTWN_RIVALS_DIR || join(tmpdir(), "mtwn-rivals");
const work = process.env.MTWN_RIVALS_WORK || join(tmpdir(), "mtwn-rivals-work");
const out = process.env.MTWN_RIVALS_OUT || generated;
const fetching = process.argv.includes("--fetch");
const checking = process.argv.includes("--check");
if (!fetching) {
  refuseOutsideContainer("rivals");
}
const steps = 100;

const packages = {
  gsap: {
    spec: "gsap@3.15.0",
    file: "gsap-3.15.0.tgz",
    integrity:
      "sha512-dMW4CWBTUK1AEEDeZc1g4xpPGIrSf9fJF960qbTZmN/QwZIWY5wgliS6JWl9/25fpTGJrMRtSjGtOmPnfjZB+A==",
    name: "gsap",
    main: "dist/gsap.js",
  },
  tween: {
    spec: "@tweenjs/tween.js@25.0.0",
    file: "tweenjs-tween.js-25.0.0.tgz",
    integrity:
      "sha512-XKLA6syeBUaPzx4j3qwMqzzq+V4uo72BnlbOjmuljLrRqdsd3qnzvZZoxvMHZ23ndsRS4aufU6JOZYpCbU6T1A==",
    name: "@tweenjs/tween.js",
    main: "dist/tween.cjs",
  },
  motionUtils: {
    spec: "motion-utils@14.1.1",
    file: "motion-utils-14.1.1.tgz",
    integrity:
      "sha512-3VvN5Uvf8B6qG+OfOui6Nht5EWp+vLuU3IqcAj7ItZGKqtmTjCebl5sGZF7/08gEHRnEl1AP/fIwbcEyVrcQGQ==",
    name: "motion-utils",
    main: "dist/cjs/index.js",
  },
  motionDom: {
    spec: "motion-dom@14.2.0",
    file: "motion-dom-14.2.0.tgz",
    integrity:
      "sha512-TDdxnjAavh2Qm4lQCQI3uoTx9IR/Id2PI8JNEXW3AcwwV2yvhoimAX3/TnE+x82gDHfPq2yx/cJ58TTw5mMr0A==",
    name: "motion-dom",
    main: "dist/cjs/index.js",
    needs: ["motionUtils"],
  },
};

// CSS Color 4's sample code at w3c/csswg-drafts af8661b, the commit
// src/generated/color_matrices.h was made from.
const colorCommit = "af8661bfe903aac7dbb9c5d1e1eccb725b8c331d";
const colorFiles = {
  "multiply-matrices.js": "db7051e9992231b20a3b5164811a74a144bd18718ffed46fe7d3a59d8c770c7e",
  "conversions.js": "73551b73012580c774012279ffbdc7e150f8b269ee4c446a3c042aed4719c153",
};

// A package's tarball, refused unless its hash is the pinned one.
function checkedTarball(key) {
  const p = packages[key];
  const tarball = join(fetched, p.file);
  if (!existsSync(tarball)) {
    throw new Error(`${p.spec}: not fetched; run tools/oracle/run-in-container.sh fetch`);
  }
  const got = "sha512-" + createHash("sha512").update(readFileSync(tarball)).digest("base64");
  if (got !== p.integrity) {
    throw new Error(`${p.spec}: ${got} is not the pinned ${p.integrity}`);
  }
  return tarball;
}

// Downloads a package once by npm pack, which runs none of its scripts,
// and checks it.
function fetchPackage(key) {
  const p = packages[key];
  mkdirSync(fetched, { recursive: true });
  if (!existsSync(join(fetched, p.file))) {
    execFileSync("npm", ["pack", p.spec, "--pack-destination", fetched, "--silent"], {
      stdio: ["ignore", "ignore", "inherit"],
    });
  }
  checkedTarball(key);
}

// Unpacks a fetched package into a node_modules of its own, with what it
// needs.
function unpack(key, modules) {
  const p = packages[key];
  const tarball = checkedTarball(key);
  const into = join(modules, p.name);
  mkdirSync(into, { recursive: true });
  execFileSync("tar", ["-xzf", tarball, "-C", into, "--strip-components=1"]);
  for (const need of p.needs || []) {
    unpack(need, modules);
  }
}

// Loads a package's main file from its own node_modules.
function load(key) {
  const p = packages[key];
  const modules = join(work, key, "node_modules");
  unpack(key, modules);
  return createRequire(join(modules, "loader.js"))(join(modules, p.name, p.main));
}

// Downloads CSS Color 4's sample files once, with fetch, and checks
// them.
async function fetchColor() {
  const dir = join(fetched, "css-color-4");
  mkdirSync(dir, { recursive: true });
  for (const file of Object.keys(colorFiles)) {
    const path = join(dir, file);
    if (!existsSync(path)) {
      const url = `https://raw.githubusercontent.com/w3c/csswg-drafts/${colorCommit}/css-color-4/${file}`;
      const response = await fetch(url);
      if (!response.ok) {
        throw new Error(`${url}: ${response.status}`);
      }
      writeFileSync(path, Buffer.from(await response.arrayBuffer()));
    }
  }
  checkedColor();
}

// CSS Color 4's sample files, refused unless their SHA-256 is the pinned
// one.
function checkedColor() {
  let code = "";
  for (const [file, sha256] of Object.entries(colorFiles)) {
    const path = join(fetched, "css-color-4", file);
    if (!existsSync(path)) {
      throw new Error(`${file}: not fetched; run tools/oracle/run-in-container.sh fetch`);
    }
    const bytes = readFileSync(path);
    const got = createHash("sha256").update(bytes).digest("hex");
    if (got !== sha256) {
      throw new Error(`${file}: ${got} is not the pinned ${sha256}`);
    }
    code += bytes.toString("utf8") + "\n";
  }
  return code;
}

// Runs CSS Color 4's sample files in a context of their own.
function loadColor() {
  const code = checkedColor();
  const context = { Math, Array, Number, NaN };
  runInNewContext(code, context);
  return context;
}

if (fetching) {
  for (const key of Object.keys(packages)) {
    fetchPackage(key);
  }
  await fetchColor();
  console.log(`rivals: fetched and checked in ${fetched}`);
  process.exit(0);
}

// A double as a C hexadecimal literal, exactly; NaN as RIVAL_NAN.
function hex(value) {
  if (Number.isNaN(value)) {
    return "RIVAL_NAN";
  }
  if (value === 0) {
    return Object.is(value, -0) ? "-0x0.0p+0" : "0x0.0p+0";
  }
  const view = new DataView(new ArrayBuffer(8));
  view.setFloat64(0, value);
  const bits = view.getBigUint64(0);
  const sign = bits >> 63n ? "-" : "";
  const exponent = Number((bits >> 52n) & 0x7ffn);
  const fraction = (bits & 0xfffffffffffffn).toString(16).padStart(13, "0");
  if (exponent === 0) {
    return `${sign}0x0.${fraction}p-1022`;
  }
  return `${sign}0x1.${fraction}p${exponent - 1023 >= 0 ? "+" : ""}${exponent - 1023}`;
}

const kinds = { power: 0, sine: 1, expo: 2, circ: 3, bounce: 4, back: 5, elastic: 6 };
const modes = { in: 0, out: 1, inOut: 2 };
const curves = [];

function add(source, kind, mode, a, b, ease) {
  const values = [];
  for (let i = 0; i <= steps; ++i) {
    values.push(ease(i / steps));
  }
  curves.push({ source, kind, mode, a, b, values });
}

// GSAP's eases by name, mapped to Maul Tween's parameters: powerN is the
// power N + 1, back(s) the overshoot s, elastic(a, p) the amplitude and
// period, in-out elastic's period 0.45 when none is given. The elastics
// include amplitudes below 1, which GSAP takes as 1 with the period
// divided by them, and the largest amplitude, where GSAP's miss of 1 at
// the end is largest: (0.05, 0.001), (2, 0.03) and (512, 0.046) come
// nearest the elastic bound of test_rival_easing on these samples, of
// amplitudes from 0.05 to 512 and periods from 0.001 to 1.
const { gsap } = load("gsap");
for (const mode of ["in", "out", "inOut"]) {
  for (let n = 1; n <= 4; ++n) {
    add(0, kinds.power, modes[mode], n + 1, 0, gsap.parseEase(`power${n}.${mode}`));
  }
  for (const name of ["sine", "expo", "circ", "bounce"]) {
    add(0, kinds[name], modes[mode], 0, 0, gsap.parseEase(`${name}.${mode}`));
  }
  for (const s of [1.70158, 3]) {
    add(0, kinds.back, modes[mode], s, 0, gsap.parseEase(`back.${mode}(${s})`));
  }
  for (const [a, p] of [
    [1, 0.3],
    [1, 0.45],
    [1.5, 0.5],
    [0.05, 0.001],
    [0.5, 0.1],
    [2, 0.03],
    [512, 0.046],
  ]) {
    add(0, kinds.elastic, modes[mode], a, p, gsap.parseEase(`elastic.${mode}(${a}, ${p})`));
  }
}

// tween.js's easings: Quadratic to Quintic are the powers 2 to 5 and
// generatePow any power; Elastic is amplitude 1, period 0.4; Back's in
// and out are the overshoot 1.70158 and its in-out Penner's 1.70158 *
// 1.525.
const { Easing } = load("tween");
const tweenModes = { in: "In", out: "Out", inOut: "InOut" };
for (const mode of ["in", "out", "inOut"]) {
  const m = tweenModes[mode];
  ["Quadratic", "Cubic", "Quartic", "Quintic"].forEach((name, i) =>
    add(1, kinds.power, modes[mode], i + 2, 0, Easing[name][m]),
  );
  for (const power of [1.5, 2.5, 7]) {
    add(1, kinds.power, modes[mode], power, 0, Easing.generatePow(power)[m]);
  }
  add(1, kinds.sine, modes[mode], 0, 0, Easing.Sinusoidal[m]);
  add(1, kinds.expo, modes[mode], 0, 0, Easing.Exponential[m]);
  add(1, kinds.circ, modes[mode], 0, 0, Easing.Circular[m]);
  add(1, kinds.bounce, modes[mode], 0, 0, Easing.Bounce[m]);
  add(1, kinds.back, modes[mode], mode === "inOut" ? 1.70158 * 1.525 : 1.70158, 0, Easing.Back[m]);
  add(1, kinds.elastic, modes[mode], 1, 0.4, Easing.Elastic[m]);
}

// Motion's spring by stiffness, damping and mass, from an origin to a
// target with a velocity in units per second, sampled every 20 ms for 2
// s; its rest thresholds set so small it never snaps to the target.
const { spring } = load("motionDom");
const springs = [];
for (const [stiffness, damping, mass, origin, target, velocity] of [
  [100, 10, 1, 0, 1, 0],
  [300, 20, 1, 0, 100, -500],
  [170, 26, 1, -2, 3, 40],
  [50, 1, 0.5, 0, 10, 0],
  [100, 20, 1, 0, 1, 0],
  [100, 20, 1, 5, -5, 300],
  [100, 60, 2, 0, 1, 0],
  [400, 100, 1, 1, 0, -50],
]) {
  const generator = spring({
    keyframes: [origin, target],
    stiffness,
    damping,
    mass,
    velocity,
    restSpeed: 1e-300,
    restDelta: 1e-300,
  });
  const values = [];
  const velocities = [];
  for (let i = 0; i <= steps; ++i) {
    values.push(generator.next(i * 20).value);
    velocities.push(generator.velocity(i * 20));
  }
  springs.push({ params: [stiffness, damping, mass, origin, target, velocity], values, velocities });
}

// conversions.js from gamma sRGB, each channel a float as Maul Tween's
// colors hold them, to linear sRGB, Oklab and OkLCh (hue in degrees, NaN
// where conversions.js calls it powerless).
const css = loadColor();
const colors = [];
const levels = [0, 0.1, 0.25, 0.5, 0.75, 0.9, 1, -0.1, 1.2];
for (const r of levels) {
  for (const g of levels) {
    for (const b of levels) {
      const rgb = [Math.fround(r), Math.fround(g), Math.fround(b)];
      const linear = css.lin_sRGB(rgb);
      const lab = css.XYZ_to_OKLab(css.lin_sRGB_to_XYZ(linear));
      colors.push({ rgb, linear, lab, lch: css.OKLab_to_OKLCH(lab) });
    }
  }
}

// GSAP's distribute over rows and grids: a full one, one with a short last
// row, one of a single full row and one with fewer items than columns. From
// an index (5, or the last item), "end", "center", "edges" and ratio
// points, on each axis, by each and amount, positive and negative, linear
// and power1.in, from a base of 0 or 0.5. Each case is written as GSAP
// takes it, for the test to translate as the clips guide's porting note
// does.
const froms = { index: 0, end: 1, center: 2, edges: 3, ratio: 4 };
const axisCodes = { both: 0, x: 1, y: 2 };
const staggers = [];
const distribute = gsap.utils.distribute;
for (const [count, columns] of [
  [6, 0],
  [12, 4],
  [7, 3],
  [4, 4],
  [5, 8],
]) {
  const index = Math.min(5, count - 1);
  for (const from of [0, "end", "center", "edges", index, [0, 0], [1, 1], [0.25, 0.75]]) {
    for (const axis of columns === 0 ? ["both"] : ["both", "x", "y"]) {
      for (const [kind, spacing, base] of [
        ["each", 0.1, 0],
        ["amount", 1, 0],
        ["each", -0.1, 0],
        ["amount", -1, 0.5],
      ]) {
        for (const ease of ["none", "power1.in"]) {
          const vars = { from, ease, base, [kind]: spacing };
          if (columns > 0) {
            vars.grid = [Math.ceil(count / columns), columns];
          }
          if (axis !== "both") {
            vars.axis = axis;
          }
          const f = distribute(vars);
          const items = Array.from({ length: count }, (_, i) => i);
          staggers.push({
            count,
            columns,
            from: Array.isArray(from) ? froms.ratio : typeof from === "number" ? froms.index : froms[from],
            index: typeof from === "number" ? from : 0,
            ratio: Array.isArray(from) ? from : [0, 0],
            axis: axisCodes[axis],
            amount: kind === "amount" ? 1 : 0,
            spacing,
            base,
            eased: ease === "none" ? 0 : 1,
            offsets: items.map((i) => f(i, items[i], items)),
          });
        }
      }
    }
  }
}

// GSAP's timeline positions over scripts of up to six adds: children
// (tweens of a duration, delay, repeats, repeat delay and time scale),
// labels and callbacks, each at an absolute time, the end, the previous
// child's start or end, or a label (one not yet added among them), with
// offsets in seconds or percent. The scripts come from a fixed xorshift
// seed, and only those GSAP places without moving a child or a label
// before 0 are kept, since GSAP shifts its children there and nothing
// translates that. Each op is written as GSAP takes it, for the test to
// translate as the clips guide's porting note does, with the start GSAP
// gives it.
const opKinds = { child: 0, label: 1, call: 2 };
const opBases = { abs: 0, end: 1, prevStart: 2, prevEnd: 3, label: 4 };
const positions = [];
let state = 0x2545f491;
function random() {
  state ^= state << 13;
  state >>>= 0;
  state ^= state >>> 17;
  state ^= state << 5;
  state >>>= 0;
  return state / 4294967296;
}
const pick = (choices) => choices[Math.floor(random() * choices.length)];
// A percentage is of the previous child's total duration in "<N%" and
// ">N%", and of the inserted child's in "+=N%", "-=N%", "<+=N%", ">+=N%"
// and "label+=N%".
const ofPrevious = 1;
const ofInserted = 2;
function gsapPosition(op) {
  const size = Math.abs(op.offset) + (op.percent ? "%" : "");
  const sign = op.offset < 0 ? "-=" : "+=";
  const fromPrevious = op.percent === ofInserted ? sign + size : op.offset + (op.percent ? "%" : "");
  switch (op.base) {
    case "abs":
      return op.offset;
    case "end":
      return sign + size;
    case "prevStart":
      return "<" + fromPrevious;
    case "prevEnd":
      return ">" + fromPrevious;
    default:
      return op.offset === 0 ? `L${op.label}` : `L${op.label}${sign}${size}`;
  }
}
for (let attempt = 0; positions.length < 400 && attempt < 100000; ++attempt) {
  const timeline = gsap.timeline({ paused: true });
  const ops = [];
  const added = [];
  const names = [];
  let labels = 0;
  let moved = false;
  const count = 2 + Math.floor(random() * 5);
  for (let i = 0; i < count && !moved; ++i) {
    const kind = i === 0 ? "child" : pick(["child", "child", "child", "label", "call"]);
    const op = {
      kind,
      base: pick(["abs", "end", "end", "prevStart", "prevEnd", "label"]),
      offset: pick([0, 0, 0.5, 1, 2, -0.25, -0.5]),
      percent: 0,
      label: 0,
      duration: 0,
      delay: 0,
      repeat: 0,
      repeatDelay: 0,
      rate: 1,
    };
    if (kind === "child") {
      op.duration = pick([0.5, 1, 2]);
      op.delay = pick([0, 0, 0.5]);
      op.repeat = pick([0, 0, 0, 1, 2, -1]);
      op.repeatDelay = pick([0, 0.25]);
      op.rate = pick([1, 1, 2, 0.5]);
    }
    if (op.base === "abs") {
      op.offset = Math.abs(op.offset);
    } else if (op.base === "label") {
      // A child may name the label after the last, which GSAP adds at its
      // end; a label or callback names one that exists, or goes at the end.
      op.label = Math.floor(random() * (labels + (kind === "child" ? 1 : 0)));
      if (op.label === labels && kind !== "child") {
        op.base = "end";
      }
    }
    if (kind === "child" && op.base !== "abs" && random() < 0.2) {
      const fromPrevious = op.base === "prevStart" || op.base === "prevEnd";
      op.percent = fromPrevious && random() < 0.5 ? ofPrevious : ofInserted;
      op.offset = pick([25, 50, -50]);
    }
    const starts = added.map((a) => a.startTime());
    const placedLabels = JSON.stringify(timeline.labels);
    let made = null;
    if (kind === "label") {
      names.push(`L${labels}`);
      timeline.addLabel(`L${labels}`, gsapPosition(op));
    } else {
      made =
        kind === "child"
          ? gsap.to(
              { v: 0 },
              {
                v: 1,
                duration: op.duration,
                delay: op.delay,
                repeat: op.repeat,
                repeatDelay: op.repeatDelay,
                ease: "none",
              },
            )
          : gsap.delayedCall(0, () => {});
      made.timeScale(op.rate);
      timeline.add(made, gsapPosition(op));
      added.push(made);
    }
    const missing = op.base === "label" && op.label === labels;
    labels += kind === "label" || missing ? 1 : 0;
    // Below 0, GSAP shifts its children at its next string position or
    // render, so an op that lands there, or a shift of any before it,
    // leaves the script.
    moved =
      added.some((a, j) => j < starts.length && a.startTime() !== starts[j]) ||
      (made !== null && made.startTime() < 0) ||
      (kind === "label" && timeline.labels[names.at(-1)] < 0) ||
      (kind !== "label" && !missing && JSON.stringify(timeline.labels) !== placedLabels);
    ops.push(op);
  }
  const placed = added.map((a) => a.startTime());
  timeline.duration();
  moved = moved || added.some((a, j) => a.startTime() !== placed[j]);
  let child = 0;
  let label = 0;
  const starts = ops.map((op) =>
    op.kind === "label" ? timeline.labels[names[label++]] : added[child++].startTime(),
  );
  if (!moved && starts.every((t) => t >= 0 && t < 1e7)) {
    positions.push({ ops, starts });
  }
}

function header(guard, about, body) {
  return [
    "// SPDX-License-Identifier: MIT",
    "// Copyright (c) 2026 Sirac Ozmen",
    "//",
    "// Generated by tools/oracle/rivals.mjs; do not edit.",
    ...about.map((line) => `// ${line}`),
    "// clang-format off",
    "",
    `#ifndef ${guard}`,
    `#define ${guard}`,
    "",
    ...body,
    "",
    "#endif",
    "",
  ].join("\n");
}

const outputs = {
  "rival_easing.h": header(
    "MTWN_RIVAL_EASING_H",
    [
      "The easings GSAP 3.15.0 and tween.js 25.0.0 compute, run under Node,",
      `at ${steps + 1} even progresses from 0 to 1.`,
    ],
    [
      `#define RIVAL_STEPS ${steps}`,
      "",
      "static const RivalCurve s_rivalCurves[] = {",
      ...curves.map(
        (c) =>
          `    {${c.source}, ${c.kind}, ${c.mode}, ${hex(c.a)}, ${hex(c.b)}, {${c.values.map(hex).join(", ")}}},`,
      ),
      "};",
    ],
  ),
  "rival_spring.h": header(
    "MTWN_RIVAL_SPRING_H",
    [
      "Motion 14.2.0's springs (motion-dom), run under Node: stiffness,",
      "damping, mass, origin, target and velocity, then the value and the",
      `velocity every 20 ms for ${steps * 20} ms.`,
    ],
    [
      `#define RIVAL_SPRING_STEPS ${steps}`,
      "",
      "static const RivalSpring s_rivalSprings[] = {",
      ...springs.map(
        (c) =>
          `    {{${c.params.map(hex).join(", ")}}, {${c.values.map(hex).join(", ")}}, {${c.velocities.map(hex).join(", ")}}},`,
      ),
      "};",
    ],
  ),
  "rival_stagger.h": header(
    "MTWN_RIVAL_STAGGER_H",
    [
      "GSAP 3.15.0's distribute, run under Node: the count, the columns (0",
      "for a row), from (0 an index, 1 end, 2 center, 3 edges, 4 ratios),",
      "the index, the ratios, the axis, amount (1) or each (0), the spacing,",
      "the base, power1.in (1) or none (0), then the offsets.",
    ],
    [
      "static const RivalStagger s_rivalStaggers[] = {",
      ...staggers.map(
        (c) =>
          `    {${c.count}, ${c.columns}, ${c.from}, ${c.index}, {${c.ratio.map(hex).join(", ")}}, ${c.axis}, ${c.amount}, ${hex(c.spacing)}, ${hex(c.base)}, ${c.eased}, {${c.offsets.map(hex).join(", ")}}},`,
      ),
      "};",
    ],
  ),
  "rival_position.h": header(
    "MTWN_RIVAL_POSITION_H",
    [
      "GSAP 3.15.0's timeline positions, run under Node: each script's ops,",
      "each its kind (0 a child, 1 a label, 2 a callback), base (0 absolute,",
      "1 the end, 2 the previous start, 3 the previous end, 4 a label), offset,",
      "seconds (0) or percent of the previous (1) or the inserted child's (2)",
      "total duration, label, duration, delay, iterations, repeat delay and",
      "time scale, then the start GSAP gives each op.",
    ],
    [
      "#define RIVAL_INF ((double)INFINITY)",
      "",
      "static const RivalScript s_rivalScripts[] = {",
      ...positions.map(
        (p) =>
          `    {${p.ops.length}, {${p.ops
            .map(
              (o) =>
                `{${opKinds[o.kind]}, ${opBases[o.base]}, ${hex(o.offset)}, ${o.percent}, ${o.label}, ${hex(o.duration)}, ${hex(o.delay)}, ${o.repeat < 0 ? "RIVAL_INF" : hex(o.repeat + 1)}, ${hex(o.repeatDelay)}, ${hex(o.rate)}}`,
            )
            .join(", ")}}, {${p.starts.map(hex).join(", ")}}},`,
      ),
      "};",
    ],
  ),
  "rival_color.h": header(
    "MTWN_RIVAL_COLOR_H",
    [
      "CSS Color 4's conversions.js at w3c/csswg-drafts af8661b, run under",
      "Node: gamma sRGB, then linear sRGB, Oklab and OkLCh (hue in degrees).",
    ],
    [
      "#define RIVAL_NAN ((double)NAN)",
      "",
      "static const RivalColor s_rivalColors[] = {",
      ...colors.map(
        (c) =>
          `    {{${c.rgb.map(hex).join(", ")}}, {${c.linear.map(hex).join(", ")}}, {${c.lab.map(hex).join(", ")}}, {${c.lch.map(hex).join(", ")}}},`,
      ),
      "};",
    ],
  ),
};

let stale = 0;
for (const [name, text] of Object.entries(outputs)) {
  if (checking) {
    const path = join(generated, name);
    if (!existsSync(path) || readFileSync(path, "utf8") !== text) {
      console.error(`${name} is not current; run tools/oracle/run-in-container.sh rivals`);
      ++stale;
    }
  } else {
    const path = join(out, name);
    writeFileSync(path, text);
    console.log(`wrote ${path}`);
  }
}
if (stale > 0) {
  process.exit(1);
}
if (checking) {
  console.log(
    `rivals: current, ${curves.length} curves, ${springs.length} springs, ${colors.length} colors, ${staggers.length} staggers, ${positions.length} scripts`,
  );
}
