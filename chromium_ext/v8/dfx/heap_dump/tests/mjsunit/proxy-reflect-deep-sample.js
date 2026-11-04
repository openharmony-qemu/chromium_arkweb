// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep JSProxy target/handler shapes alive for heap dump smoke. The exact
// JSProxy edge semantics are covered by cctest; this sample supplies varied
// proxy inputs for dump/translate regression detection.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const trapLog = keep('trapLog', []);

  const objectTarget = keep('objectTarget', {
    stable: {kind: 'stable'},
    value: 1,
    method(delta) {
      return this.value + delta;
    },
  });
  const objectHandler = keep('objectHandler', {
    get(target, key, receiver) {
      trapLog.push(['get', key]);
      if (key === 'virtual') return {kind: 'virtual'};
      return Reflect.get(target, key, receiver);
    },
    set(target, key, value, receiver) {
      trapLog.push(['set', key]);
      return Reflect.set(target, key, value, receiver);
    },
    has(target, key) {
      trapLog.push(['has', key]);
      return key === 'virtual' || Reflect.has(target, key);
    },
    ownKeys(target) {
      trapLog.push(['ownKeys']);
      return Reflect.ownKeys(target);
    },
    getOwnPropertyDescriptor(target, key) {
      trapLog.push(['getOwnPropertyDescriptor', key]);
      return Reflect.getOwnPropertyDescriptor(target, key);
    },
    defineProperty(target, key, descriptor) {
      trapLog.push(['defineProperty', key]);
      return Reflect.defineProperty(target, key, descriptor);
    },
    deleteProperty(target, key) {
      trapLog.push(['deleteProperty', key]);
      return Reflect.deleteProperty(target, key);
    },
    getPrototypeOf(target) {
      trapLog.push(['getPrototypeOf']);
      return Reflect.getPrototypeOf(target);
    },
    setPrototypeOf(target, prototype) {
      trapLog.push(['setPrototypeOf']);
      return Reflect.setPrototypeOf(target, prototype);
    },
  });
  const objectProxy = keep('objectProxy', new Proxy(objectTarget, objectHandler));
  objectProxy.extra = {kind: 'extra'};
  Object.defineProperty(objectProxy, 'defined', {
    value: {kind: 'defined'},
    configurable: true,
    enumerable: true,
  });
  delete objectProxy.extra;
  Object.setPrototypeOf(objectProxy, {protoMarker: true});

  function callableTarget(a, b) {
    return a + b;
  }
  callableTarget.meta = {kind: 'callable-target'};
  const callableHandler = keep('callableHandler', {
    apply(target, receiver, args) {
      trapLog.push(['apply', args.length]);
      return Reflect.apply(target, receiver, args) + 10;
    },
    get(target, key, receiver) {
      trapLog.push(['callableGet', key]);
      return Reflect.get(target, key, receiver);
    },
  });
  const callableProxy =
      keep('callableProxy', new Proxy(callableTarget, callableHandler));

  class ConstructTarget {
    constructor(value) {
      this.value = value;
    }

    read(delta) {
      return this.value + delta;
    }
  }
  const constructHandler = keep('constructHandler', {
    construct(target, args, newTarget) {
      trapLog.push(['construct', args.length]);
      const instance = Reflect.construct(target, args, newTarget);
      instance.constructedViaProxy = true;
      return instance;
    },
    get(target, key, receiver) {
      trapLog.push(['constructGet', key]);
      return Reflect.get(target, key, receiver);
    },
  });
  const constructProxy =
      keep('constructProxy', new Proxy(ConstructTarget, constructHandler));
  const constructedInstance = keep('constructedInstance', new constructProxy(20));

  const arrayTarget = keep('arrayTarget', [{id: 0}, {id: 1}, {id: 2}]);
  const arrayProxy = keep('arrayProxy', new Proxy(arrayTarget, {
    get(target, key, receiver) {
      trapLog.push(['arrayGet', key]);
      return Reflect.get(target, key, receiver);
    },
    set(target, key, value, receiver) {
      trapLog.push(['arraySet', key]);
      return Reflect.set(target, key, value, receiver);
    },
  }));
  arrayProxy[3] = {id: 3};

  const typedArrayTarget = keep('typedArrayTarget', new Uint16Array([1, 2, 3, 4]));
  const typedArrayProxy = keep('typedArrayProxy', new Proxy(typedArrayTarget, {
    get(target, key, receiver) {
      trapLog.push(['typedArrayGet', key]);
      if (key === 'length') return target.length;
      const value = Reflect.get(target, key, target);
      return typeof value === 'function' ? value.bind(target) : value;
    },
  }));

  const mapTarget = keep('mapTarget', new Map([
    ['alpha', {value: 1}],
    ['beta', {value: 2}],
  ]));
  const mapProxy = keep('mapProxy', new Proxy(mapTarget, {
    get(target, key, receiver) {
      trapLog.push(['mapGet', key]);
      if (key === 'size') return target.size;
      const value = Reflect.get(target, key, target);
      return typeof value === 'function' ? value.bind(target) : value;
    },
  }));

  const proxyAsPrototypeTarget = keep('proxyAsPrototypeTarget', {
    inherited: {kind: 'inherited'},
  });
  const proxyAsPrototype =
      keep('proxyAsPrototype', new Proxy(proxyAsPrototypeTarget, objectHandler));
  const objectWithProxyPrototype =
      keep('objectWithProxyPrototype', Object.create(proxyAsPrototype));
  objectWithProxyPrototype.own = {kind: 'own'};

  const revocableActive = keep(
      'revocableActive',
      Proxy.revocable({alive: true}, {
        get(target, key, receiver) {
          trapLog.push(['revocableGet', key]);
          return Reflect.get(target, key, receiver);
        },
      }));
  const activeRevocableValue = keep(
      'activeRevocableValue', revocableActive.proxy.alive);

  const revocableRevoked = keep(
      'revocableRevoked',
      Proxy.revocable({revoked: true}, {
        get(target, key, receiver) {
          return Reflect.get(target, key, receiver);
        },
      }));
  revocableRevoked.revoke();
  let revokedError = null;
  try {
    revocableRevoked.proxy.revoked;
  } catch (e) {
    revokedError = e;
  }
  keep('revokedError', revokedError);

  const weakMap = keep('weakMapWithProxyKey', new WeakMap([
    [objectProxy, {kind: 'object-proxy-key'}],
    [callableProxy, {kind: 'callable-proxy-key'}],
  ]));
  const weakSet = keep('weakSetWithProxyValues', new WeakSet([
    constructProxy,
    proxyAsPrototype,
  ]));

  assertEquals('virtual', objectProxy.virtual.kind);
  assertTrue('virtual' in objectProxy);
  assertTrue(Reflect.ownKeys(objectProxy).includes('stable'));
  assertEquals('defined', objectProxy.defined.kind);
  assertTrue(Object.getPrototypeOf(objectProxy).protoMarker);
  assertEquals(15, callableProxy(2, 3));
  assertEquals('callable-target', callableProxy.meta.kind);
  assertTrue(constructedInstance.constructedViaProxy);
  assertEquals(25, constructedInstance.read(5));
  assertEquals(4, arrayProxy.length);
  assertEquals(3, arrayProxy[3].id);
  assertEquals(4, typedArrayProxy.length);
  assertEquals(10, typedArrayProxy.reduce((sum, value) => sum + value, 0));
  assertEquals(2, mapProxy.size);
  assertEquals(1, mapProxy.get('alpha').value);
  assertEquals('inherited', objectWithProxyPrototype.inherited.kind);
  assertTrue(activeRevocableValue);
  assertTrue(revokedError instanceof TypeError);
  assertEquals('object-proxy-key', weakMap.get(objectProxy).kind);
  assertTrue(weakSet.has(constructProxy));

  globalThis.heapDumpProxyReflectDeepSample = catalog;
})();
