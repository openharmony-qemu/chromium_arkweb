// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep accessor/property descriptor shapes alive for heap dump smoke. Exact
// AccessorInfo/AccessorPair edges are pinned by cctests; this JS sample keeps
// natural getter/setter, descriptor, prototype, and proxy inputs reachable.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const symbolAccessorKey = keep(
      'symbolAccessorKey', Symbol('heap-dump-accessor-symbol'));
  const computedAccessorKey = 'computed_' + 'accessor';

  const accessorTargets = keep('accessorTargets', []);
  for (let i = 0; i < 32; ++i) {
    const state = {i, value: {kind: 'initial', i}};
    const object = {state};
    Object.defineProperty(object, 'value', {
      configurable: true,
      enumerable: (i & 1) === 0,
      get() {
        return state.value;
      },
      set(next) {
        state.value = {kind: 'set', i, next};
      },
    });
    Object.defineProperty(object, computedAccessorKey + '_' + i, {
      configurable: true,
      enumerable: true,
      get() {
        return {kind: 'computed-getter', i};
      },
    });
    Object.defineProperty(object, symbolAccessorKey, {
      configurable: true,
      enumerable: i % 3 === 0,
      get() {
        return {kind: 'symbol-getter', i};
      },
      set(next) {
        state.symbolValue = next;
      },
    });
    object.value = {updated: i};
    object[symbolAccessorKey] = {symbolUpdated: i};
    accessorTargets.push(object);
  }

  const prototypeState = keep('prototypeState', {value: {kind: 'proto'}});
  const prototypeWithAccessor = keep('prototypeWithAccessor', {});
  Object.defineProperty(prototypeWithAccessor, 'inheritedAccessor', {
    configurable: true,
    enumerable: true,
    get() {
      return prototypeState.value;
    },
    set(next) {
      prototypeState.value = next;
    },
  });
  const inheritedAccessorObjects = keep('inheritedAccessorObjects', []);
  for (let i = 0; i < 24; ++i) {
    const object = Object.create(prototypeWithAccessor);
    object.own = {i};
    object.inheritedAccessor = {kind: 'inherited-set', i};
    inheritedAccessorObjects.push(object);
  }

  class AccessorClass {
    #privateState = {kind: 'private-initial'};

    constructor(seed) {
      this.seed = seed;
    }

    get publicAccessor() {
      return {seed: this.seed, privateState: this.#privateState};
    }

    set publicAccessor(value) {
      this.#privateState = {kind: 'private-set', value};
    }

    static get staticAccessor() {
      return {kind: 'static-accessor'};
    }
  }
  const classInstances = keep('classInstances', []);
  for (let i = 0; i < 16; ++i) {
    const instance = new AccessorClass(i);
    instance.publicAccessor = {i};
    classInstances.push(instance);
  }
  keep('AccessorClass', AccessorClass);
  keep('AccessorClassPrototypeDescriptor',
       Object.getOwnPropertyDescriptor(AccessorClass.prototype, 'publicAccessor'));
  keep('AccessorClassStaticDescriptor',
       Object.getOwnPropertyDescriptor(AccessorClass, 'staticAccessor'));

  const descriptorMutations = keep('descriptorMutations', []);
  for (let i = 0; i < 16; ++i) {
    const object = {};
    Object.defineProperty(object, 'slot', {
      configurable: true,
      enumerable: true,
      value: {kind: 'data', i},
      writable: true,
    });
    Object.defineProperty(object, 'slot', {
      configurable: true,
      enumerable: i % 2 === 0,
      get() {
        return {kind: 'converted-accessor', i};
      },
      set(value) {
        object.lastSet = value;
      },
    });
    object.slot = {kind: 'mutation-set', i};
    descriptorMutations.push({
      object,
      descriptor: Object.getOwnPropertyDescriptor(object, 'slot'),
      keys: Reflect.ownKeys(object),
    });
  }

  const descriptorProxyLog = keep('descriptorProxyLog', []);
  const descriptorProxies = keep('descriptorProxies', accessorTargets.slice(0, 8)
      .map((target, index) => new Proxy(target, {
        get(targetObject, key, receiver) {
          descriptorProxyLog.push(['get', index, key]);
          return Reflect.get(targetObject, key, receiver);
        },
        set(targetObject, key, value, receiver) {
          descriptorProxyLog.push(['set', index, key]);
          return Reflect.set(targetObject, key, value, receiver);
        },
        getOwnPropertyDescriptor(targetObject, key) {
          descriptorProxyLog.push(['descriptor', index, key]);
          return Reflect.getOwnPropertyDescriptor(targetObject, key);
        },
        ownKeys(targetObject) {
          descriptorProxyLog.push(['ownKeys', index]);
          return Reflect.ownKeys(targetObject);
        },
      })));
  const proxyDescriptors = keep('proxyDescriptors',
      descriptorProxies.map((proxy) => ({
        valueDescriptor: Object.getOwnPropertyDescriptor(proxy, 'value'),
        keys: Reflect.ownKeys(proxy),
        value: proxy.value,
      })));

  assertEquals('set', accessorTargets[0].value.kind);
  assertEquals('computed-getter', accessorTargets[1][computedAccessorKey + '_1'].kind);
  assertEquals('symbol-getter', accessorTargets[2][symbolAccessorKey].kind);
  assertEquals('inherited-set', prototypeState.value.kind);
  assertEquals('private-set', classInstances[0].publicAccessor.privateState.kind);
  assertEquals('static-accessor', AccessorClass.staticAccessor.kind);
  assertEquals('converted-accessor', descriptorMutations[0].object.slot.kind);
  assertEquals('function', typeof descriptorMutations[0].descriptor.get);
  assertTrue(proxyDescriptors[0].keys.includes('value'));

  globalThis.heapDumpAccessorPropertyDeepSample = catalog;
})();
