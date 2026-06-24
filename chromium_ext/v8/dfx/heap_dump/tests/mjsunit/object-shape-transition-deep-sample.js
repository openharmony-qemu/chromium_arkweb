// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep object map-transition, descriptor-array, enum-cache, property-array,
// and dictionary-property shapes alive for heap dump smoke. Exact edge parity
// is handled by cctests; this sample supplies naturally-created JS heap input.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function ShapeCtor(seed) {
    this.a = {seed, slot: 'a'};
    this.b = seed + 1;
    this.c = 'shape-' + seed;
  }

  ShapeCtor.prototype.read = function read() {
    return this.a.seed + this.b;
  };

  const transitionChain = keep('transitionChain', []);
  for (let i = 0; i < 80; ++i) {
    const object = new ShapeCtor(i);
    object['fanout_' + (i % 10)] = {i, branch: i % 10};
    if ((i & 1) === 0) object.even = {i};
    if ((i & 2) === 0) object.twoBit = {i};
    if ((i & 4) === 0) object.fourBit = {i};
    transitionChain.push(object);
  }

  const prototypeBase = keep('prototypeBase', {
    baseValue: {kind: 'base-value'},
    baseMethod() {
      return this.localValue.kind;
    },
  });
  const prototypeMid = keep('prototypeMid', Object.create(prototypeBase));
  prototypeMid.midValue = {kind: 'mid-value'};

  const prototypeTransitions = keep('prototypeTransitions', []);
  for (let i = 0; i < 24; ++i) {
    const object = {
      localValue: {kind: 'local-' + i},
      stable: i,
    };
    if ((i % 3) === 0) {
      Object.setPrototypeOf(object, prototypeBase);
    } else if ((i % 3) === 1) {
      Object.setPrototypeOf(object, prototypeMid);
    } else {
      object.__proto__ = {
        protoValue: {kind: 'inline-proto-' + i},
        baseMethod() {
          return this.localValue.kind + ':inline';
        },
      };
    }
    prototypeTransitions.push(object);
  }

  const dictionaryPressure = keep('dictionaryPressure', []);
  for (let i = 0; i < 10; ++i) {
    const object = {anchor: {i}};
    for (let j = 0; j < 96; ++j) {
      object['dict_prop_' + i + '_' + j] = {i, j};
    }
    for (let j = 0; j < 96; j += 4) {
      delete object['dict_prop_' + i + '_' + j];
    }
    Object.defineProperty(object, 'accessor_' + i, {
      configurable: true,
      enumerable: i % 2 === 0,
      get() {
        return object.anchor;
      },
      set(value) {
        object.anchor = value;
      },
    });
    dictionaryPressure.push(object);
  }

  const computedKeyObjects = keep('computedKeyObjects', []);
  const privateSymbol = keep('privateSymbol', Symbol('heap-dump-shape-private'));
  const sharedSymbol = keep('sharedSymbol', Symbol.for('heap-dump-shape-shared'));
  for (let i = 0; i < 32; ++i) {
    const dynamic = 'computed_' + i;
    const object = {
      fixed: {i},
      [dynamic]: {dynamic},
      [privateSymbol]: {privateSymbol: i},
      [sharedSymbol]: {sharedSymbol: i},
    };
    Object.defineProperty(object, 'nonEnumerable_' + i, {
      configurable: true,
      enumerable: false,
      value: {hidden: i},
    });
    computedKeyObjects.push(object);
  }

  const enumCacheStress = keep('enumCacheStress', []);
  for (let i = 0; i < 32; ++i) {
    const object = {
      alpha: {i, name: 'alpha'},
      beta: {i, name: 'beta'},
      gamma: {i, name: 'gamma'},
      delta: {i, name: 'delta'},
    };
    const keys = Object.keys(object);
    const entries = Object.entries(object);
    const forInSeen = [];
    for (const key in object) {
      forInSeen.push(key);
    }
    enumCacheStress.push({object, keys, entries, forInSeen});
  }

  const frozenTransitionTargets = keep('frozenTransitionTargets', []);
  for (let i = 0; i < 12; ++i) {
    const object = {
      first: {i},
      second: {i},
    };
    if ((i & 1) === 0) {
      Object.seal(object);
    } else {
      Object.freeze(object);
    }
    frozenTransitionTargets.push(object);
  }

  assertEquals(1, transitionChain[0].read());
  assertEquals('local-0', prototypeTransitions[0].baseMethod());
  assertEquals('local-2:inline', prototypeTransitions[2].baseMethod());
  assertEquals(dictionaryPressure[3].anchor, dictionaryPressure[3].accessor_3);
  assertTrue(Reflect.ownKeys(computedKeyObjects[0]).includes(privateSymbol));
  assertEquals('alpha', enumCacheStress[0].keys[0]);
  assertTrue(Object.isSealed(frozenTransitionTargets[0]));
  assertTrue(Object.isFrozen(frozenTransitionTargets[1]));

  globalThis.heapDumpObjectShapeTransitionDeepSample = catalog;
})();
