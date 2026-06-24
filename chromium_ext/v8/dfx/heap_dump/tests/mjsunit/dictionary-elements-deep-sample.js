// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Keep slow/dictionary-elements arrays alive for heap dump smoke. The
// `%HasDictionaryElements` checks make the heap shape intentional instead of
// relying on a sparse-array heuristic.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const sparseObjectElements = keep('sparseObjectElements', []);
  sparseObjectElements[0] = {slot: 0};
  sparseObjectElements[1 << 20] = {slot: 'high-object'};
  sparseObjectElements.extraProperty = {kind: 'array-property'};
  assertTrue(%HasDictionaryElements(sparseObjectElements));

  const sparseAfterDeletes = keep('sparseAfterDeletes', []);
  for (let i = 0; i < 32; ++i) {
    sparseAfterDeletes[i * 4096] = {i, live: true};
  }
  for (let i = 0; i < 32; i += 3) {
    delete sparseAfterDeletes[i * 4096];
  }
  sparseAfterDeletes[1 << 21] = {slot: 'after-delete-high'};
  assertTrue(%HasDictionaryElements(sparseAfterDeletes));

  const accessorElements = keep('accessorElements', []);
  const getterTarget = keep('getterTarget', {value: 'accessor-target'});
  accessorElements[1 << 20] = {slot: 'force-dictionary'};
  Object.defineProperty(accessorElements, 17, {
    configurable: true,
    enumerable: true,
    get() {
      return getterTarget;
    },
    set(value) {
      getterTarget.value = value.value;
    },
  });
  assertTrue(%HasDictionaryElements(accessorElements));

  const mixedKeyElements = keep('mixedKeyElements', []);
  mixedKeyElements[2 ** 32 - 2] = {slot: 'max-array-index'};
  mixedKeyElements[9007199254740991] = {slot: 'named-property-index-string'};
  mixedKeyElements.named = {slot: 'named-property'};
  assertTrue(%HasDictionaryElements(mixedKeyElements));

  keep('refChecks', [
    sparseObjectElements[0],
    sparseObjectElements[1 << 20],
    sparseAfterDeletes[1 << 21],
    accessorElements[17],
    mixedKeyElements[2 ** 32 - 2],
    mixedKeyElements.named,
  ]);

  assertEquals('high-object', sparseObjectElements[1 << 20].slot);
  assertEquals('after-delete-high', sparseAfterDeletes[1 << 21].slot);
  assertEquals('accessor-target', accessorElements[17].value);
  assertEquals('max-array-index', mixedKeyElements[2 ** 32 - 2].slot);
  assertEquals(
      'named-property-index-string',
      mixedKeyElements[9007199254740991].slot);

  globalThis.heapDumpDictionaryElementsDeepSample = catalog;
})();
