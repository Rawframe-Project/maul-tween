// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Refuses to go on unless this process runs in the container that
// tools/oracle/run-in-container.sh starts, judged from what that
// container guarantees rather than from anything it could be told: no
// network interface but the loopback, a root that refuses a write as
// read-only (EROFS; a plain host user's write fails with EACCES
// instead), and a user other than root.

import { closeSync, openSync, unlinkSync } from "node:fs";
import { networkInterfaces } from "node:os";

export function refuseOutsideContainer(name) {
  const failed = [];
  const online = Object.entries(networkInterfaces())
    .filter(([face, addresses]) => face !== "lo" || addresses.some((a) => !a.internal))
    .map(([face]) => face);
  if (online.length > 0) {
    failed.push(`network interfaces other than lo: ${online.join(", ")}`);
  }
  const probe = `/.mtwn-contained-${process.pid}`;
  try {
    closeSync(openSync(probe, "wx"));
    failed.push("a root that takes a write");
    try {
      unlinkSync(probe);
    } catch {
      // Refused either way.
    }
  } catch (error) {
    if (error.code !== "EROFS") {
      failed.push(`a root that refuses a write with ${error.code}, not EROFS`);
    }
  }
  if (process.getuid() === 0) {
    failed.push("uid 0");
  }
  if (failed.length > 0) {
    console.error(
      `${name}: not in the container tools/oracle/run-in-container.sh starts (${failed.join("; ")})`,
    );
    process.exit(2);
  }
}
