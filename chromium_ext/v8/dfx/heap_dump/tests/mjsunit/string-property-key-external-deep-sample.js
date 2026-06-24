// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string --expose-gc

// Keep string property-key lookup shapes alive for heap dump smoke. This
// follows the stable property-key parts of V8's string-forwarding-table mjsunit
// while retaining in-object and dictionary property carriers,
// substring/cons/integer-index lookup keys, externalized keys, and lookup
// result objects. The current sandbox build rejects the forwarding-table flag
// as readonly, so this sample intentionally exercises the key shapes without
// relying on that flag.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const longKey = keep('longKey', 'key1234567890abcdefg');
  const substrKey = keep('substrKey', longKey.substring(3, 17));
  const consKey = keep('consKey', 'key' + 1234567890 + 'abcdefg');
  const integerIndex = keep('integerIndex', longKey.substring(3, 8));

  gc();
  gc();

  const inObjectCarrier = keep('inObjectCarrier', {
    key1234567890abcdefg: {kind: 'long-key-value'},
    '1234567890abcd': {kind: 'substr-value'},
    12345: {kind: 'integer-index-value'},
  });
  assertEquals('long-key-value', inObjectCarrier[longKey].kind);
  assertEquals('substr-value', inObjectCarrier[substrKey].kind);
  assertEquals('long-key-value', inObjectCarrier[consKey].kind);
  assertEquals('integer-index-value', inObjectCarrier[integerIndex].kind);

  const dictionaryCarrier = keep('dictionaryCarrier', []);
  for (let i = 0; i < 100; ++i) {
    dictionaryCarrier[i] = {index: i};
    dictionaryCarrier['XXX' + i] = {named: 'XXX' + i};
  }
  dictionaryCarrier.key1234567890abcdefg = {kind: 'dict-long-key-value'};
  dictionaryCarrier['1234567890abcd'] = {kind: 'dict-substr-value'};
  dictionaryCarrier[12345] = {kind: 'dict-integer-index-value'};

  const beforeExternalize = keep('beforeExternalize', {
    longKey: dictionaryCarrier[longKey],
    substrKey: dictionaryCarrier[substrKey],
    consKey: dictionaryCarrier[consKey],
    integerIndex: dictionaryCarrier[integerIndex],
  });

  const externalizeErrors = keep('externalizeErrors', []);
  for (const key of [longKey, substrKey, consKey, integerIndex]) {
    try {
      externalizeString(key);
    } catch (error) {
      externalizeErrors.push(error);
    }
  }

  const afterExternalize = keep('afterExternalize', {
    longKey: dictionaryCarrier[longKey],
    substrKey: dictionaryCarrier[substrKey],
    consKey: dictionaryCarrier[consKey],
    integerIndex: dictionaryCarrier[integerIndex],
  });

  const descriptors = keep('descriptors', {
    inObject: Object.getOwnPropertyDescriptors(inObjectCarrier),
    dictionary: Object.getOwnPropertyDescriptors(dictionaryCarrier),
  });
  const mapCarrier = keep('mapCarrier', new Map([
    [longKey, inObjectCarrier[longKey]],
    [substrKey, dictionaryCarrier[substrKey]],
    [consKey, dictionaryCarrier[consKey]],
    [integerIndex, dictionaryCarrier[integerIndex]],
  ]));

  assertEquals('dict-long-key-value', beforeExternalize.longKey.kind);
  assertEquals('dict-substr-value', beforeExternalize.substrKey.kind);
  assertEquals('dict-long-key-value', beforeExternalize.consKey.kind);
  assertEquals('dict-integer-index-value', beforeExternalize.integerIndex.kind);
  assertEquals('dict-long-key-value', afterExternalize.longKey.kind);
  assertEquals('dict-substr-value', afterExternalize.substrKey.kind);
  assertEquals('dict-long-key-value', afterExternalize.consKey.kind);
  assertEquals('dict-integer-index-value', afterExternalize.integerIndex.kind);
  assertTrue(mapCarrier.has(longKey));
  assertTrue('1234567890abcd' in descriptors.dictionary);

  globalThis.heapDumpStringPropertyKeyExternalDeepSample = catalog;
})();
