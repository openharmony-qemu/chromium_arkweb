// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-externalize-string

// Keep String.prototype.isWellFormed/toWellFormed external-string slow paths
// alive for heap dump smoke. This follows V8's external-uncached
// well-formedness mjsunit pattern while retaining the externalized strings and
// normalized replacement-string results.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function makeExternalizableTwoByte(unit) {
    return createExternalizableString(
        unit.padEnd(kExternalStringMinTwoByteLength, unit));
  }

  const wellFormed = keep('wellFormed',
                         makeExternalizableTwoByte('\u1234'));
  const illFormedHigh = keep('illFormedHigh',
                            makeExternalizableTwoByte('\uD83D'));
  const illFormedLow = keep('illFormedLow',
                           makeExternalizableTwoByte('\uDC00'));

  const consWellFormed = keep('consWellFormed', '\u1234' + '\u1234');
  const consIllFormed = keep('consIllFormed', '\uD83D' + '\uD83D');
  consWellFormed.endsWith('\u1234');
  consIllFormed.endsWith('\uD83D');

  const thinSource = keep('thinSource', '\u1234' + '\u1234');
  const thinCarrier = keep('thinCarrier', {});
  thinCarrier[thinSource] = {kind: 'thin-source-key'};

  const beforeExternalize = keep('beforeExternalize', {
    wellFormed: wellFormed.isWellFormed(),
    illFormedHigh: illFormedHigh.isWellFormed(),
    illFormedLow: illFormedLow.isWellFormed(),
    consWellFormed: consWellFormed.isWellFormed(),
    consIllFormed: consIllFormed.isWellFormed(),
    thinSource: thinSource.isWellFormed(),
  });

  const externalizeErrors = keep('externalizeErrors', []);
  for (const value of [wellFormed, illFormedHigh, illFormedLow]) {
    try {
      externalizeString(value);
    } catch (error) {
      externalizeErrors.push(error);
    }
  }

  const afterExternalize = keep('afterExternalize', {
    wellFormed: wellFormed.isWellFormed(),
    illFormedHigh: illFormedHigh.isWellFormed(),
    illFormedLow: illFormedLow.isWellFormed(),
  });
  const toWellFormedResults = keep('toWellFormedResults', {
    wellFormed: wellFormed.toWellFormed(),
    illFormedHigh: illFormedHigh.toWellFormed(),
    illFormedLow: illFormedLow.toWellFormed(),
    consWellFormed: consWellFormed.toWellFormed(),
    consIllFormed: consIllFormed.toWellFormed(),
    thinSource: thinSource.toWellFormed(),
  });

  const wrapper = keep('wrapper', new String(illFormedHigh));
  wrapper.normalized = toWellFormedResults.illFormedHigh;
  wrapper.flags = {beforeExternalize, afterExternalize};

  assertTrue(beforeExternalize.wellFormed);
  assertFalse(beforeExternalize.illFormedHigh);
  assertFalse(beforeExternalize.illFormedLow);
  assertTrue(beforeExternalize.consWellFormed);
  assertFalse(beforeExternalize.consIllFormed);
  assertTrue(beforeExternalize.thinSource);
  assertTrue(afterExternalize.wellFormed);
  assertFalse(afterExternalize.illFormedHigh);
  assertFalse(afterExternalize.illFormedLow);
  assertEquals('\u1234'.padEnd(kExternalStringMinTwoByteLength, '\u1234'),
               toWellFormedResults.wellFormed);
  assertEquals('\uFFFD'.padEnd(kExternalStringMinTwoByteLength, '\uFFFD'),
               toWellFormedResults.illFormedHigh);
  assertEquals('\uFFFD'.padEnd(kExternalStringMinTwoByteLength, '\uFFFD'),
               toWellFormedResults.illFormedLow);
  assertEquals('\uFFFD\uFFFD', toWellFormedResults.consIllFormed);
  assertEquals(toWellFormedResults.illFormedHigh, wrapper.normalized);

  globalThis.heapDumpStringWellFormedExternalDeepSample = catalog;
})();
