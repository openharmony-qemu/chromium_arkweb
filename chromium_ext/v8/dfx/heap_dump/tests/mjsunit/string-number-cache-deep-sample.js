// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep string-family and numeric-cache-adjacent heap shapes alive for heap dump
// smoke. Exact ConsString/SlicedString/ThinString edges are pinned by cctests;
// this sample increases end-to-end rawheap coverage with natural JS inputs.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function makeDeepRope(prefix, count, suffixFactory) {
    let value = prefix;
    const snapshots = [];
    for (let i = 0; i < count; ++i) {
      value = value + ':' + suffixFactory(i);
      if ((i % 8) === 0) snapshots.push(value);
    }
    return {value, snapshots};
  }

  const oneByteRopes = keep('oneByteRopes', []);
  const twoByteRopes = keep('twoByteRopes', []);
  for (let i = 0; i < 24; ++i) {
    oneByteRopes.push(makeDeepRope(
        'one-byte-rope-' + i, 40, (j) => 'chunk-' + i + '-' + j));
    twoByteRopes.push(makeDeepRope(
        'two-byte-rope-\u0100-' + i, 32,
        (j) => '\u2603-' + i + '-' + j + '-\u{1f680}'));
  }

  const slicedStrings = keep('slicedStrings', []);
  for (let i = 0; i < oneByteRopes.length; ++i) {
    slicedStrings.push(oneByteRopes[i].value.slice(7, 120));
    slicedStrings.push(twoByteRopes[i].value.substring(5, 130));
  }

  const flattenedStrings = keep('flattenedStrings', slicedStrings.map(
      (value, index) => value.split(':').join('|') + '|flat-' + index));

  const internedKeyObjects = keep('internedKeyObjects', []);
  const internedKeys = keep('internedKeys', []);
  for (let i = 0; i < 64; ++i) {
    const key = ('heapDumpInternalizedKey_' + i).slice(0);
    const descriptorKey = ('heapDumpDescriptorKey_' + (i % 16)).substring(0);
    const object = {
      [key]: {i, key},
    };
    Object.defineProperty(object, descriptorKey, {
      configurable: true,
      enumerable: (i & 1) === 0,
      value: {i, descriptorKey},
    });
    internedKeys.push(key, descriptorKey);
    internedKeyObjects.push(object);
  }

  const symbolNames = keep('symbolNames', []);
  const symbols = keep('symbols', []);
  for (let i = 0; i < 32; ++i) {
    const local = Symbol('heap-dump-symbol-description-' + i);
    const registry = Symbol.for('heap-dump-symbol-registry-' + (i % 8));
    symbols.push(local, registry);
    symbolNames.push(String(local), Symbol.keyFor(registry));
  }

  const numericValues = keep('numericValues', [
    0,
    -0,
    1.5,
    -2.25,
    Math.PI,
    Math.E,
    Number.MIN_VALUE,
    Number.MAX_VALUE,
    Number.MAX_SAFE_INTEGER + 0.25,
    NaN,
    Infinity,
    -Infinity,
  ]);
  const numberStrings = keep('numberStrings', []);
  for (let i = 0; i < 128; ++i) {
    const value = numericValues[i % numericValues.length] + i / 10;
    numberStrings.push(String(value));
    numberStrings.push(value.toExponential(3));
    numberStrings.push(value.toPrecision(6));
  }

  const bigInts = keep('bigInts', []);
  const bigintStrings = keep('bigintStrings', []);
  for (let i = 0n; i < 32n; ++i) {
    const value = (1n << (64n + i)) + i + 20260525n;
    const negative = -value;
    bigInts.push(value, negative);
    bigintStrings.push(value.toString(10), negative.toString(16));
  }

  const stringWrappers = keep('stringWrappers', slicedStrings.slice(0, 16).map(
      (value, index) => {
        const wrapper = new String(value);
        wrapper.index = index;
        wrapper.extra = flattenedStrings[index];
        return wrapper;
      }));

  const templateSites = keep('templateSites', []);
  function tag(strings, ...values) {
    templateSites.push({
      strings,
      raw: strings.raw,
      values,
    });
    return strings[0] + values.join(':');
  }
  for (let i = 0; i < 16; ++i) {
    tag`template-\u0100-${i}-${oneByteRopes[i].value}`;
  }

  assertTrue(oneByteRopes[0].value.includes('chunk-0-39'));
  assertTrue(twoByteRopes[0].value.includes('\u2603-0-31'));
  assertTrue(slicedStrings[1].includes('\u0100'));
  assertEquals('heapDumpInternalizedKey_0', internedKeys[0]);
  assertEquals('heap-dump-symbol-registry-0', symbolNames[1]);
  assertEquals('0', numberStrings[0]);
  assertTrue(bigInts[0] > 0n);
  assertTrue(bigInts[1] < 0n);
  assertEquals(0, stringWrappers[0].index);
  assertEquals(16, templateSites.length);

  globalThis.heapDumpStringNumberCacheDeepSample = catalog;
})();
