// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper module for heap-dump module live-binding smoke samples. It is also
// registered as a standalone sample so its SourceTextModule/module context
// remains visible even when no importer runs.

export let liveCounter = 0;
export let liveObject = {kind: 'initial-live-object', value: 0};
export const stableObject = {
  kind: 'stable-object',
  nested: {value: 7},
};
export const stableArray = [
  {kind: 'stable-array-0'},
  {kind: 'stable-array-1'},
];

export function bumpLiveCounter(delta = 1) {
  liveCounter += delta;
  liveObject = {kind: 'updated-live-object', value: liveCounter};
  return liveCounter;
}

export function readLiveObject() {
  return liveObject;
}

export class LiveBindingClass {
  static staticPayload = {kind: 'live-binding-static'};

  constructor(seed) {
    this.seed = seed;
  }

  read() {
    return liveCounter + this.seed;
  }
}

export default {
  label: 'module-live-binding-helper',
  stableObject,
  stableArray,
};

globalThis.__heapDumpModuleLiveBindingHelper = {
  stableObject,
  stableArray,
  liveObject,
  liveCounter,
  bumpLiveCounter,
  LiveBindingClass,
  metaUrl: import.meta.url,
};

