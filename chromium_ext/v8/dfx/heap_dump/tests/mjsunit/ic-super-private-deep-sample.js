// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep super/private/optional-chain and dictionary-prototype IC shapes alive
// for heap dump smoke. This complements the generic feedback IC sample with
// access sites that V8's mjsunit suite exercises separately.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  class SuperBase {
    constructor(seed) {
      this.baseSeed = seed;
    }

    get baseGetter() {
      return {kind: 'base-getter', seed: this.baseSeed};
    }

    set baseSetter(value) {
      this.lastBaseSet = {value};
    }

    baseMethod(delta) {
      return {kind: 'base-method', value: this.baseSeed + delta};
    }
  }

  class SuperDerived extends SuperBase {
    #privateField;

    constructor(seed) {
      super(seed);
      this.#privateField = {kind: 'private-field', seed};
      this.publicField = {kind: 'public-field', seed};
    }

    readSuper(delta) {
      const getter = super.baseGetter;
      const method = super.baseMethod(delta);
      super.baseSetter = {seed: this.baseSeed, delta};
      return {getter, method, set: this.lastBaseSet};
    }

    readPrivate() {
      return this.#privateField;
    }

    writePrivate(value) {
      this.#privateField = {kind: 'private-field-updated', value};
      return this.#privateField;
    }

    optionalRead(key) {
      return this.publicField?.[key] ?? this.#privateField?.[key] ?? null;
    }
  }

  class StaticSuperBase {
    static get value() {
      return {kind: 'static-super-value'};
    }

    static method(delta) {
      return {kind: 'static-super-method', delta};
    }
  }

  class StaticSuperDerived extends StaticSuperBase {
    static #privateStatic = {kind: 'private-static'};

    static readStaticSuper(delta) {
      return {
        value: super.value,
        method: super.method(delta),
        privateStatic: this.#privateStatic,
      };
    }
  }

  const superObjects = keep('superObjects', []);
  const superResults = keep('superResults', []);
  for (let i = 0; i < 96; ++i) {
    const object = new SuperDerived(i);
    superObjects.push(object);
    superResults.push(object.readSuper(i % 7));
    superResults.push(object.readPrivate());
    superResults.push(object.writePrivate(i + 1000));
    superResults.push(object.optionalRead('seed'));
  }

  const dictionaryPrototype = keep('dictionaryPrototype', {});
  for (let i = 0; i < 72; ++i) {
    dictionaryPrototype['proto_' + i] = {i};
  }
  for (let i = 0; i < 36; i += 3) {
    delete dictionaryPrototype['proto_' + i];
  }
  Object.defineProperty(dictionaryPrototype, 'accessorProto', {
    configurable: true,
    enumerable: true,
    get() {
      return {kind: 'accessor-proto', receiver: this.receiverMarker};
    },
    set(value) {
      this.setterCapture = value;
    },
  });

  function loadFromDictionaryPrototype(object, key) {
    return object[key];
  }

  function storeThroughDictionaryPrototype(object, value) {
    object.accessorProto = value;
    return object.setterCapture;
  }

  const dictionaryReceivers = keep('dictionaryReceivers', []);
  const dictionaryResults = keep('dictionaryResults', []);
  for (let i = 0; i < 96; ++i) {
    const receiver = Object.create(dictionaryPrototype);
    receiver.receiverMarker = {i};
    receiver.own = {i, own: true};
    dictionaryReceivers.push(receiver);
    dictionaryResults.push(loadFromDictionaryPrototype(receiver, 'proto_' + (i % 72)));
    dictionaryResults.push(loadFromDictionaryPrototype(receiver, 'accessorProto'));
    dictionaryResults.push(storeThroughDictionaryPrototype(receiver, {stored: i}));
  }

  function optionalChainTarget(object, key) {
    return object?.nested?.[key]?.payload ?? object?.fallback?.payload ?? null;
  }

  const optionalTargets = keep('optionalTargets', []);
  const optionalResults = keep('optionalResults', []);
  for (let i = 0; i < 80; ++i) {
    const object = (i % 5) === 0 ? null : {
      nested: {
        ['slot_' + (i % 8)]: {payload: {i, path: 'nested'}},
      },
      fallback: {payload: {i, path: 'fallback'}},
    };
    optionalTargets.push(object);
    optionalResults.push(optionalChainTarget(object, 'slot_' + (i % 8)));
    optionalResults.push(optionalChainTarget(object, 'missing'));
  }

  const keyedAccessObjects = keep('keyedAccessObjects', []);
  const keyedResults = keep('keyedResults', []);
  for (let i = 0; i < 96; ++i) {
    const object = {
      ['field_' + (i % 16)]: {i, kind: 'field'},
      ['method_' + (i % 8)](delta) {
        return {i, delta, receiver: this};
      },
    };
    keyedAccessObjects.push(object);
    const fieldKey = 'field_' + (i % 16);
    const methodKey = 'method_' + (i % 8);
    keyedResults.push(object[fieldKey]);
    keyedResults.push(object[methodKey](i + 1));
  }

  keep('SuperBase', SuperBase);
  keep('SuperDerived', SuperDerived);
  keep('StaticSuperBase', StaticSuperBase);
  keep('StaticSuperDerived', StaticSuperDerived);
  keep('staticSuperResult', StaticSuperDerived.readStaticSuper(42));
  keep('loadFromDictionaryPrototype', loadFromDictionaryPrototype);
  keep('storeThroughDictionaryPrototype', storeThroughDictionaryPrototype);
  keep('optionalChainTarget', optionalChainTarget);

  assertEquals('base-getter', superResults[0].getter.kind);
  assertEquals('private-field', superResults[1].kind);
  assertEquals('private-field-updated', superResults[2].kind);
  assertEquals(0, superResults[3]);
  assertEquals('static-super-value', catalog.staticSuperResult.value.kind);
  assertEquals('accessor-proto', dictionaryResults[1].kind);
  assertEquals(0, dictionaryResults[2].stored);
  assertEquals('nested', optionalResults[2].path);
  assertEquals('fallback', optionalResults[3].path);
  assertEquals('field', keyedResults[0].kind);
  assertEquals(1, keyedResults[1].delta);

  globalThis.heapDumpIcSuperPrivateDeepSample = catalog;
})();
