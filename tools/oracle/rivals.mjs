// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The rivals as an oracle (record mtwn-0050): GSAP, tween.js and
// Motion's springs run under Node, and CSS Color 4's conversions.js, each
// fetched by its pinned version and checked against its pinned hash
// before it runs, never kept in the repository. What they compute for
// curves, springs and colors Maul Tween shares is written, exactly, to
// test/generated/rival_easing.h, rival_spring.h and rival_color.h, which
// test/test_rival_easing.c, test_rival_spring.c and test_rival_color.c
// check. Run by hand, not on every commit:
//
//     node tools/oracle/rivals.mjs [--check]
//
// The packages are fetched into $MTWN_RIVALS_DIR, or a directory in the
// system's temporary one.

import { execFileSync } from "node:child_process";
import { runInNewContext } from "node:vm";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { createRequire } from "node:module";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const generated = join(root, "test", "generated");
const fetched = process.env.MTWN_RIVALS_DIR || join(tmpdir(), "mtwn-rivals");
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

// Fetches a package once, refuses it unless its hash is the pinned one,
// and unpacks it into a node_modules of its own, with what it needs.
function unpack(key, modules) {
  const p = packages[key];
  const tarball = join(fetched, p.file);
  mkdirSync(fetched, { recursive: true });
  if (!existsSync(tarball)) {
    execFileSync("npm", ["pack", p.spec, "--pack-destination", fetched, "--silent"], {
      stdio: ["ignore", "ignore", "inherit"],
    });
  }
  const got = "sha512-" + createHash("sha512").update(readFileSync(tarball)).digest("base64");
  if (got !== p.integrity) {
    throw new Error(`${p.spec}: ${got} is not the pinned ${p.integrity}`);
  }
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
  const modules = join(fetched, key, "node_modules");
  unpack(key, modules);
  return createRequire(join(modules, "loader.js"))(join(modules, p.name, p.main));
}

// Fetches CSS Color 4's sample files once, refuses them unless their
// SHA-256 is the pinned one, and runs them in a context of their own.
async function loadColor() {
  const dir = join(fetched, "css-color-4");
  mkdirSync(dir, { recursive: true });
  let code = "";
  for (const [file, sha256] of Object.entries(colorFiles)) {
    const path = join(dir, file);
    if (!existsSync(path)) {
      const url = `https://raw.githubusercontent.com/w3c/csswg-drafts/${colorCommit}/css-color-4/${file}`;
      const response = await fetch(url);
      if (!response.ok) {
        throw new Error(`${url}: ${response.status}`);
      }
      writeFileSync(path, Buffer.from(await response.arrayBuffer()));
    }
    const bytes = readFileSync(path);
    const got = createHash("sha256").update(bytes).digest("hex");
    if (got !== sha256) {
      throw new Error(`${file}: ${got} is not the pinned ${sha256}`);
    }
    code += bytes.toString("utf8") + "\n";
  }
  const context = { Math, Array, Number, NaN };
  runInNewContext(code, context);
  return context;
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
// period, in-out elastic's period 0.45 when none is given.
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
const css = await loadColor();
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
  const path = join(generated, name);
  if (process.argv.includes("--check")) {
    if (!existsSync(path) || readFileSync(path, "utf8") !== text) {
      console.error(`${path} is not current; run node tools/oracle/rivals.mjs`);
      ++stale;
    }
  } else {
    writeFileSync(path, text);
    console.log(`wrote ${path}`);
  }
}
if (stale > 0) {
  process.exit(1);
}
if (process.argv.includes("--check")) {
  console.log(`rivals: current, ${curves.length} curves, ${springs.length} springs, ${colors.length} colors`);
}
