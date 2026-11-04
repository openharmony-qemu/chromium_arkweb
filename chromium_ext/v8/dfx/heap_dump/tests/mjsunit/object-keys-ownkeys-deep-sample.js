// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Keep Object.keys / Reflect.ownKeys property-enumeration and proxy trap states
// alive for heap-dump smoke coverage. This follows stable portions of V8's
// object-keys.js and es6/reflect-own-keys.js mjsunit tests.

const heapDumpKeysOwnKeysState = {
  objects: [],
  proxies: [],
  keysResults: [],
  ownKeysResults: [],
  descriptors: [],
  logs: [],
  errors: [],
};

function keepKeysOwnKeys(name, value) {
  heapDumpKeysOwnKeysState[name] = value;
  return value;
}

(function enumerableAndEnumerationCacheResults() {
  const empty = {};
  const emptyKeys = Object.keys(empty);
  %HeapObjectVerify(emptyKeys);
  assertEquals(0, emptyKeys.length);

  const nonEnumerable = {};
  Object.defineProperty(nonEnumerable, "x", {
    value: {hidden: true},
    enumerable: false,
    configurable: true,
  });
  let visibleKeys = Object.keys(nonEnumerable);
  %HeapObjectVerify(visibleKeys);
  assertEquals([], visibleKeys);
  nonEnumerable.y = {visible: true};
  visibleKeys = Object.keys(nonEnumerable);
  %HeapObjectVerify(visibleKeys);
  assertEquals(["y"], visibleKeys);

  const fastObject = {x: {rank: 1}, y: {rank: 2}};
  let cachedKeys = Object.keys(fastObject);
  assertEquals(["x", "y"], cachedKeys);
  cachedKeys[0] = "y";
  cachedKeys[1] = "x";
  const freshKeys = Object.keys(fastObject);
  assertEquals(["x", "y"], freshKeys);

  const symbol = Symbol("not-enumerated-by-Object.keys");
  const symbolOnly = {[symbol]: {symbolValue: true}};
  const symbolKeys = Object.keys(symbolOnly);
  assertEquals([], symbolKeys);

  heapDumpKeysOwnKeysState.objects.push(
      empty, nonEnumerable, fastObject, symbolOnly);
  heapDumpKeysOwnKeysState.keysResults.push(
      emptyKeys, visibleKeys, cachedKeys, freshKeys, symbolKeys);
  heapDumpKeysOwnKeysState.descriptors.push(
      Object.getOwnPropertyDescriptor(nonEnumerable, "x"));
})();

(function objectKeysProxyDescriptorTraps() {
  const proxyLog = [];
  const sparseProxy = new Proxy({}, {
    ownKeys(target) {
      proxyLog.push("ownKeys");
      return ["a", "b", "c"];
    },
    getOwnPropertyDescriptor(target, key) {
      proxyLog.push("descriptor-" + key);
      return {
        enumerable: key !== "b",
        configurable: true,
        value: {key},
      };
    },
  });
  const proxyKeys = Object.keys(sparseProxy);
  assertEquals(["a", "c"], proxyKeys);
  assertEquals(["ownKeys", "descriptor-a", "descriptor-b", "descriptor-c"],
               proxyLog);

  const targetLog = [];
  const target = {a: 1, b: 2, c: 3};
  const targetProxy = new Proxy(target, {
    getOwnPropertyDescriptor(target, key) {
      targetLog.push("descriptor-" + key);
      const desc = Reflect.getOwnPropertyDescriptor(target, key);
      desc.enumerable = key !== "b";
      return desc;
    },
  });
  const targetProxyKeys = Object.keys(targetProxy);
  assertEquals(["a", "c"], targetProxyKeys);
  assertEquals(["descriptor-a", "descriptor-b", "descriptor-c"], targetLog);

  const nonEnumerableTarget = Object.create(Object.prototype, {
    a: {value: 1, enumerable: true, configurable: true},
    b: {value: 2, enumerable: false, configurable: true},
    c: {value: 3, enumerable: true, configurable: true},
  });
  const nonEnumerableProxyKeys = Object.keys(new Proxy(
      nonEnumerableTarget, {
        getOwnPropertyDescriptor(target, key) {
          return Reflect.getOwnPropertyDescriptor(target, key);
        },
      }));
  assertEquals(["a", "c"], nonEnumerableProxyKeys);

  heapDumpKeysOwnKeysState.proxies.push(sparseProxy, targetProxy);
  heapDumpKeysOwnKeysState.objects.push(target, nonEnumerableTarget);
  heapDumpKeysOwnKeysState.keysResults.push(
      proxyKeys, targetProxyKeys, nonEnumerableProxyKeys);
  heapDumpKeysOwnKeysState.logs.push(proxyLog, targetLog);
})();

(function reflectOwnKeysOrdinaryObjects() {
  const simple = {a: 1, b: 2};
  const simpleKeys = Reflect.ownKeys(simple);
  assertEquals(["a", "b"], simpleKeys);

  const slow = {a: 1, b: 2, c: 3};
  delete slow.b;
  const slowKeys = Reflect.ownKeys(slow);
  assertEquals(["a", "c"], slowKeys);

  const arrayKeys = Reflect.ownKeys([1, 2]);
  assertEquals(["0", "1", "length"], arrayKeys);

  const protoObject = {foo: "foo"};
  protoObject.__proto__ = {bar: "bar"};
  const protoKeys = Reflect.ownKeys(protoObject);
  assertEquals(["foo"], protoKeys);

  const getterObject = {};
  getterObject.__defineGetter__("getter", function heapDumpGetter() {
    return "value";
  });
  const getterKeys = Reflect.ownKeys(getterObject);
  assertEquals(["getter"], getterKeys);

  const symbol = Symbol("own-symbol");
  const symbolObject = {};
  Object.defineProperty(symbolObject, "hidden", {
    value: "hidden",
    enumerable: false,
    configurable: true,
  });
  symbolObject.visible = "visible";
  symbolObject[symbol] = "symbol";
  const symbolOwnKeys = Reflect.ownKeys(symbolObject);
  assertEquals(["hidden", "visible", symbol], symbolOwnKeys);

  heapDumpKeysOwnKeysState.objects.push(
      simple, slow, protoObject, getterObject, symbolObject);
  heapDumpKeysOwnKeysState.ownKeysResults.push(
      simpleKeys, slowKeys, arrayKeys, protoKeys, getterKeys, symbolOwnKeys);
  heapDumpKeysOwnKeysState.descriptors.push(
      Object.getOwnPropertyDescriptor(getterObject, "getter"),
      Object.getOwnPropertyDescriptor(symbolObject, "hidden"));
})();

(function reflectOwnKeysArrayPrototypeIsolationAndPrimitives() {
  const savedConcat = Array.prototype.concat;
  const concatLog = [];
  Array.prototype.concat = function heapDumpUnexpectedConcat() {
    concatLog.push(arguments.length);
    return [];
  };
  let concatIndependentKeys;
  try {
    concatIndependentKeys = Reflect.ownKeys({0: "foo", bar: "baz"});
  } finally {
    Array.prototype.concat = savedConcat;
  }
  assertEquals(["0", "bar"], concatIndependentKeys);
  assertSame(Array.prototype, concatIndependentKeys.__proto__);
  assertEquals([], concatLog);

  const numberWrapperKeys = Reflect.ownKeys(Object(4));
  const stringWrapperKeys = Reflect.ownKeys(Object("foo"));
  const booleanWrapperKeys = Reflect.ownKeys(Object(true));
  assertEquals([], numberWrapperKeys);
  assertEquals(["0", "1", "2", "length"], stringWrapperKeys);
  assertEquals([], booleanWrapperKeys);

  const numberError = assertThrows(() => Reflect.ownKeys(4), TypeError);
  const stringError = assertThrows(() => Reflect.ownKeys("foo"), TypeError);
  const booleanError = assertThrows(() => Reflect.ownKeys(true), TypeError);

  heapDumpKeysOwnKeysState.ownKeysResults.push(
      concatIndependentKeys, numberWrapperKeys, stringWrapperKeys,
      booleanWrapperKeys);
  heapDumpKeysOwnKeysState.logs.push(concatLog);
  heapDumpKeysOwnKeysState.errors.push(numberError, stringError, booleanError);
})();

keepKeysOwnKeys("summary", {
  objectCount: heapDumpKeysOwnKeysState.objects.length,
  proxyCount: heapDumpKeysOwnKeysState.proxies.length,
  keysResultCount: heapDumpKeysOwnKeysState.keysResults.length,
  ownKeysResultCount: heapDumpKeysOwnKeysState.ownKeysResults.length,
  descriptorCount: heapDumpKeysOwnKeysState.descriptors.length,
  logCount: heapDumpKeysOwnKeysState.logs.length,
  errorCount: heapDumpKeysOwnKeysState.errors.length,
});
