// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Object.entries/values/getOwnPropertyDescriptors result shapes alive for
// heap dump smoke. This follows V8's ES8 object-entries and
// object-get-own-property-descriptors mjsunit paths while retaining descriptor
// records, proxy trap logs, filtered enumerable entries, arguments objects,
// string wrappers, and duplicate-key errors.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, factory) {
    try {
      factory();
    } catch (error) {
      keep(name, error);
      return error;
    }
    return null;
  }

  const symbolKey = keep('symbolKey', Symbol('descriptor-symbol'));
  const basicObject = keep('basicObject', {
    d: 1,
    c: 3,
    [Symbol.iterator]: void 0,
    0: 123,
    1000: 456,
    256: 'ducks',
    '0x100': 'quack',
  });
  basicObject.a = 2;
  basicObject.b = 4;
  basicObject[symbolKey] = {kind: 'symbol-value'};
  Object.defineProperty(basicObject, 'HIDDEN', {
    enumerable: false,
    value: {kind: 'hidden-value'},
  });
  Object.defineProperty(basicObject, 'getterValue', {
    enumerable: true,
    get() {
      return {kind: 'getter-result'};
    },
  });
  basicObject.c = 6;

  const basicEnumeration = keep('basicEnumeration', {
    keys: Object.keys(basicObject),
    values: Object.values(basicObject),
    entries: Object.entries(basicObject),
    descriptors: Object.getOwnPropertyDescriptors(basicObject),
    ownKeys: Reflect.ownKeys(basicObject),
  });

  const proxyLog = keep('proxyLog', []);
  const proxy = keep('proxy', new Proxy(basicObject, {
    ownKeys(target) {
      proxyLog.push(['ownKeys']);
      return Reflect.ownKeys(target);
    },
    get(target, name, receiver) {
      proxyLog.push(['get', String(name)]);
      return Reflect.get(target, name, receiver);
    },
    getOwnPropertyDescriptor(target, name) {
      proxyLog.push(['getOwnPropertyDescriptor', String(name)]);
      return Reflect.getOwnPropertyDescriptor(target, name);
    },
  }));
  const proxyEnumeration = keep('proxyEnumeration', {
    values: Object.values(proxy),
    entries: Object.entries(proxy),
    descriptors: Object.getOwnPropertyDescriptors(proxy),
  });

  const duplicateKeysProxy = keep('duplicateKeysProxy', new Proxy({}, {
    ownKeys() {
      return ['a', 'a'];
    },
    getOwnPropertyDescriptor() {
      return {configurable: true, enumerable: true, value: 1};
    },
  }));
  keepError('entriesDuplicateKeysError',
            () => Object.entries(duplicateKeysProxy));
  keepError('descriptorsDuplicateKeysError',
            () => Object.getOwnPropertyDescriptors(duplicateKeysProxy));

  const changingObject = keep('changingObject', {a: 1, b: 2});
  Object.defineProperty(changingObject, 'a', {
    enumerable: true,
    configurable: true,
    get() {
      delete changingObject.b;
      changingObject.c = 3;
      return 1;
    },
  });
  const changingEntries = keep('changingEntries',
                               Object.entries(changingObject));
  const changingDescriptors =
      keep('changingDescriptors',
           Object.getOwnPropertyDescriptors(changingObject));

  const argumentsResults = keep('argumentsResults', (function(a, b) {
    Object.defineProperties(arguments, {
      10000: {value: 'far-index', enumerable: true, configurable: true},
      hidden: {value: 'hidden', enumerable: false, configurable: true},
    });
    return {
      arguments,
      values: Object.values(arguments),
      entries: Object.entries(arguments),
      descriptors: Object.getOwnPropertyDescriptors(arguments),
      a,
      b,
    };
  })({kind: 'arg-a'}, {kind: 'arg-b'}));

  const stringWrapper = keep('stringWrapper', new String('heap'));
  Object.defineProperty(stringWrapper, 'extra', {
    value: {kind: 'string-wrapper-extra'},
    enumerable: true,
    configurable: true,
  });
  const stringWrapperResults = keep('stringWrapperResults', {
    values: Object.values(stringWrapper),
    entries: Object.entries(stringWrapper),
    descriptors: Object.getOwnPropertyDescriptors(stringWrapper),
  });

  keepError('entriesNullError', () => Object.entries(null));
  keepError('valuesUndefinedError', () => Object.values(undefined));
  keepError('descriptorsUndefinedError',
            () => Object.getOwnPropertyDescriptors(undefined));

  assertEquals('0', basicEnumeration.entries[0][0]);
  assertEquals('getter-result',
               basicEnumeration.descriptors.getterValue.get().kind);
  assertTrue(basicEnumeration.ownKeys.includes(symbolKey));
  assertTrue(proxyLog.some((entry) => entry[0] === 'ownKeys'));
  assertEquals('0', proxyEnumeration.entries[0][0]);
  assertTrue(catalog.entriesDuplicateKeysError instanceof TypeError);
  assertTrue(catalog.descriptorsDuplicateKeysError instanceof TypeError);
  assertEquals(1, changingEntries.length);
  assertEquals('a', changingEntries[0][0]);
  assertTrue(Object.prototype.hasOwnProperty.call(changingDescriptors, 'c'));
  assertTrue(argumentsResults.entries.some((entry) => entry[0] === '10000'));
  assertEquals('h', stringWrapperResults.entries[0][1]);
  assertTrue(catalog.entriesNullError instanceof TypeError);
  assertTrue(catalog.valuesUndefinedError instanceof TypeError);
  assertTrue(catalog.descriptorsUndefinedError instanceof TypeError);

  globalThis.heapDumpObjectEnumerationDescriptorsDeepSample = catalog;
})();
