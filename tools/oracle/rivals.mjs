// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The rivals' easings as an oracle (record mtwn-0050): GSAP and tween.js
// run under Node, each fetched by its pinned version and checked against
// its pinned SHA-512 before it runs, never kept in the repository. Each
// curve the rivals share with Maul Tween is sampled at 101 even
// progresses and written, exactly, to test/generated/rival_easing.h,
// which test/test_rival_easing.c checks. Run by hand, not on every
// commit:
//
//     node tools/oracle/rivals.mjs [--check]
//
// The packages are fetched into $MTWN_RIVALS_DIR, or a directory in the
// system's temporary one.

import { execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { createRequire } from "node:module";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const output = join(root, "test", "generated", "rival_easing.h");
const fetched = process.env.MTWN_RIVALS_DIR || join(tmpdir(), "mtwn-rivals");
const steps = 100;

const packages = {
  gsap: {
    spec: "gsap@3.15.0",
    file: "gsap-3.15.0.tgz",
    integrity:
      "sha512-dMW4CWBTUK1AEEDeZc1g4xpPGIrSf9fJF960qbTZmN/QwZIWY5wgliS6JWl9/25fpTGJrMRtSjGtOmPnfjZB+A==",
    main: "dist/gsap.js",
  },
  tween: {
    spec: "@tweenjs/tween.js@25.0.0",
    file: "tweenjs-tween.js-25.0.0.tgz",
    integrity:
      "sha512-XKLA6syeBUaPzx4j3qwMqzzq+V4uo72BnlbOjmuljLrRqdsd3qnzvZZoxvMHZ23ndsRS4aufU6JOZYpCbU6T1A==",
    main: "dist/tween.cjs",
  },
};

// Fetches a package once, refuses it unless its hash is the pinned one,
// unpacks it and loads its main file.
function load(name) {
  const p = packages[name];
  const dir = join(fetched, name);
  const tarball = join(dir, p.file);
  mkdirSync(dir, { recursive: true });
  if (!existsSync(tarball)) {
    execFileSync("npm", ["pack", p.spec, "--pack-destination", dir, "--silent"], {
      stdio: ["ignore", "ignore", "inherit"],
    });
  }
  const got = "sha512-" + createHash("sha512").update(readFileSync(tarball)).digest("base64");
  if (got !== p.integrity) {
    throw new Error(`${p.spec}: ${got} is not the pinned ${p.integrity}`);
  }
  execFileSync("tar", ["-xzf", tarball, "-C", dir]);
  return createRequire(join(dir, "package", "package.json"))(join(dir, "package", p.main));
}

// A double as a C hexadecimal literal, exactly.
function hex(value) {
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

const lines = [
  "// SPDX-License-Identifier: MIT",
  "// Copyright (c) 2026 Sirac Ozmen",
  "//",
  "// Generated by tools/oracle/rivals.mjs; do not edit. The easings GSAP",
  `// 3.15.0 and tween.js 25.0.0 compute, run under Node, at ${steps + 1} even`,
  "// progresses from 0 to 1.",
  "// clang-format off",
  "",
  "#ifndef MTWN_RIVAL_EASING_H",
  "#define MTWN_RIVAL_EASING_H",
  "",
  `#define RIVAL_STEPS ${steps}`,
  "",
  "static const RivalCurve s_rivalCurves[] = {",
];
for (const c of curves) {
  lines.push(
    `    {${c.source}, ${c.kind}, ${c.mode}, ${hex(c.a)}, ${hex(c.b)}, {${c.values.map(hex).join(", ")}}},`,
  );
}
lines.push("};", "", "#endif", "");
const text = lines.join("\n");

if (process.argv.includes("--check")) {
  if (!existsSync(output) || readFileSync(output, "utf8") !== text) {
    console.error(`${output} is not current; run node tools/oracle/rivals.mjs`);
    process.exit(1);
  }
  console.log(`rival easings: current, ${curves.length} curves`);
} else {
  writeFileSync(output, text);
  console.log(`wrote ${output}, ${curves.length} curves`);
}
