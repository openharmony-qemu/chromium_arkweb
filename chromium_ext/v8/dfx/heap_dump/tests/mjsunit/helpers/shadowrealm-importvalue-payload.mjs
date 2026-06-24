// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const marker = 'heap-dump-shadowrealm-importvalue';
export const objectExport = {realm: 'inner', nested: {value: 17}};
export function exportedFunction(delta = 0) {
  return `${globalThis.heapDumpShadowRealmScope}:${delta}`;
}

globalThis.heapDumpShadowRealmScope = 'inner-scope';
