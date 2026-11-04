// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Object.preventExtensions/seal/freeze integrity-level states alive for
// heap dump smoke coverage. This follows stable paths from V8's
// object-freeze.js, object-prevent-extensions.js, and integrity-level-map-update.js.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, callback) {
    try {
      callback();
    } catch (error) {
      catalog[name] = error;
      return error;
    }
    throw new Error(name + ' did not throw');
  }

  const extensible = keep('extensible', {
    data: {marker: 'extensible-data'},
    count: 1,
  });
  const prevented = keep('prevented', {
    data: {marker: 'prevented-data'},
    count: 2,
  });
  Object.preventExtensions(prevented);

  const sealed = keep('sealed', {
    data: {marker: 'sealed-data'},
    count: 3,
  });
  Object.seal(sealed);

  const frozen = keep('frozen', {
    data: {marker: 'frozen-data'},
    count: 4,
  });
  Object.freeze(frozen);

  assertTrue(Object.isExtensible(extensible));
  assertFalse(Object.isExtensible(prevented));
  assertFalse(Object.isSealed(prevented));
  assertTrue(Object.isSealed(sealed));
  assertTrue(Object.isFrozen(frozen));

  keepError('preventedAddError', () => {
    prevented.extra = {marker: 'should-not-attach'};
  });
  keepError('sealedDeleteError', () => {
    delete sealed.data;
  });
  keepError('frozenWriteError', () => {
    frozen.count = 44;
  });

  assertFalse(Object.prototype.hasOwnProperty.call(prevented, 'extra'));
  assertEquals(3, sealed.count);
  assertEquals(4, frozen.count);

  const accessorLog = keep('accessorLog', []);
  const accessorTarget = keep('accessorTarget', {});
  Object.defineProperty(accessorTarget, 'value', {
    configurable: true,
    enumerable: true,
    get() {
      accessorLog.push('get');
      return catalog.accessorValue;
    },
    set(value) {
      accessorLog.push('set');
      catalog.accessorValue = value;
    },
  });
  catalog.accessorValue = {marker: 'accessor-value'};
  Object.freeze(accessorTarget);
  assertTrue(Object.isFrozen(accessorTarget));
  assertEquals('accessor-value', accessorTarget.value.marker);
  accessorTarget.value = {marker: 'accessor-replaced'};
  assertEquals('accessor-replaced', accessorTarget.value.marker);
  assertArrayEquals(['get', 'set', 'get'], accessorLog);

  const array = keep('array', [1, {marker: 'array-object'}, 3]);
  const sealedArray = keep('sealedArray', Object.seal(array.slice()));
  const frozenArray = keep('frozenArray', Object.freeze(array.slice()));
  assertTrue(Object.isSealed(sealedArray));
  assertTrue(Object.isFrozen(frozenArray));
  keepError('sealedArrayPushError', () => sealedArray.push(4));
  keepError('frozenArraySetError', () => {
    frozenArray[1] = {marker: 'array-write'};
  });
  assertEquals('array-object', sealedArray[1].marker);
  assertEquals('array-object', frozenArray[1].marker);

  const dictionaryObject = keep('dictionaryObject', {});
  for (let i = 0; i < 40; ++i) {
    dictionaryObject['key' + i] = {index: i};
  }
  delete dictionaryObject.key1;
  delete dictionaryObject.key3;
  Object.preventExtensions(dictionaryObject);
  assertFalse(Object.isExtensible(dictionaryObject));
  assertEquals(39, dictionaryObject.key39.index);
  keepError('dictionaryAddError', () => {
    dictionaryObject.afterPreventExtensions = {marker: 'blocked'};
  });

  const proto = keep('proto', {protoValue: {marker: 'proto'}});
  const derived = keep('derived', Object.create(proto, {
    ownValue: {
      configurable: true,
      enumerable: true,
      writable: true,
      value: {marker: 'own'},
    },
  }));
  Object.freeze(derived);
  assertTrue(Object.isFrozen(derived));
  assertEquals('proto', derived.protoValue.marker);
  assertEquals('own', derived.ownValue.marker);

  const typed = keep('typed', new Uint8Array([1, 2, 3, 4]));
  Object.preventExtensions(typed);
  assertFalse(Object.isExtensible(typed));
  assertEquals(2, typed[1]);
  keepError('typedAddError', () => {
    typed.extra = {marker: 'typed-extra'};
  });

  const descriptors = keep('descriptors', {
    prevented: Object.getOwnPropertyDescriptors(prevented),
    sealed: Object.getOwnPropertyDescriptors(sealed),
    frozen: Object.getOwnPropertyDescriptors(frozen),
    accessor: Object.getOwnPropertyDescriptors(accessorTarget),
    dictionary: Object.getOwnPropertyDescriptors(dictionaryObject),
  });
  assertFalse(descriptors.sealed.data.configurable);
  assertFalse(descriptors.frozen.count.writable);
  assertFalse(descriptors.accessor.value.configurable);

  keep('summary', {
    names: Object.keys(catalog).sort(),
    dictionaryKeys: Object.keys(dictionaryObject).length,
    typedLength: typed.length,
  });

  globalThis.__heap_dump_object_integrity_levels_deep_sample = catalog;
})();
