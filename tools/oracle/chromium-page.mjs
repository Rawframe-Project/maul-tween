// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Loads one local HTML page in headless Chromium over the DevTools
// protocol and prints what the page found: the text of its element
// #out, once that starts with DONE. A page compares the library's claims
// about a browser with what the browser computes, such as color-mix()
// or getComputedStyle() of an addition.
//
//     tools/oracle/run-in-container.sh chromium PAGE.html
//
// It runs only through that wrapper, in a throwaway container without
// network, where $MTWN_RIVALS_CONTAINER is 1, and writes the text to
// $MTWN_RIVALS_OUT/PAGE.txt as well.

import { spawn } from "node:child_process";
import { writeFileSync } from "node:fs";
import { basename, join, resolve } from "node:path";
import { pathToFileURL } from "node:url";

if (process.env.MTWN_RIVALS_CONTAINER !== "1") {
  console.error("chromium-page: run it only through tools/oracle/run-in-container.sh");
  process.exit(2);
}
const page = process.argv[2];
if (!page) {
  console.error("usage: chromium-page.mjs PAGE.html");
  process.exit(2);
}
const chrome = process.env.MTWN_CHROME || "/chromium/chrome-linux64/chrome";
const out = process.env.MTWN_RIVALS_OUT || "/out";
const port = 9333;

// No sandbox: the container drops every capability, so Chromium's own
// cannot start, and the container is the boundary.
const browser = spawn(
  chrome,
  [
    "--headless=new",
    "--no-sandbox",
    "--disable-gpu",
    "--disable-dev-shm-usage",
    `--remote-debugging-port=${port}`,
    "--user-data-dir=/tmp/chromium-profile",
    "about:blank",
  ],
  { stdio: "ignore" },
);
const sleep = (ms) => new Promise((done) => setTimeout(done, ms));

let status = 1;
try {
  let targets;
  for (let i = 0; i < 100 && !targets; ++i) {
    await sleep(200);
    try {
      targets = await (await fetch(`http://127.0.0.1:${port}/json`)).json();
    } catch {
      // Not listening yet.
    }
  }
  const target = targets && targets.find((t) => t.type === "page");
  if (!target) {
    throw new Error("Chromium did not start");
  }
  const socket = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((done) => socket.addEventListener("open", done));
  let id = 0;
  const pending = new Map();
  socket.addEventListener("message", (event) => {
    const message = JSON.parse(event.data);
    if (pending.has(message.id)) {
      pending.get(message.id)(message);
      pending.delete(message.id);
    }
  });
  const send = (method, params) =>
    new Promise((done) => {
      pending.set(++id, done);
      socket.send(JSON.stringify({ id, method, params }));
    });
  await send("Page.navigate", { url: pathToFileURL(resolve(page)).href });
  let text = "";
  for (let i = 0; i < 150 && !text.startsWith("DONE"); ++i) {
    await sleep(200);
    const reply = await send("Runtime.evaluate", {
      expression: "document.getElementById('out')?.textContent ?? ''",
    });
    text = reply.result?.result?.value || "";
  }
  socket.close();
  if (!text.startsWith("DONE")) {
    throw new Error(`${page}: #out never started with DONE`);
  }
  console.log(text);
  writeFileSync(join(out, basename(page).replace(/\.html?$/, "") + ".txt"), text);
  status = 0;
} catch (error) {
  console.error(`chromium-page: ${error.message}`);
} finally {
  browser.kill();
}
process.exit(status);
