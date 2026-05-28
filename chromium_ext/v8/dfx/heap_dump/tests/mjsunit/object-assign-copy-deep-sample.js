// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Object.assign copy/ordering/proxy shapes alive for heap-dump smoke
// coverage. This follows stable portions of V8's object-assign.js and
// proxies-object-assign.js tests.

const heapDumpObjectAssignState = {
  symbols: [],
  sources: [],
  targets: [],
  descriptors: [],
  logs: [],
  errors: [],
  primitiveTargets: [],
};

function keepAssignRecord(name, record) {
  heapDumpObjectAssignState[name] = record;
  return record;
}

function checkDataProperty(object, propertyKey, value, writable,
                           enumerable, configurable) {
  const desc = Object.getOwnPropertyDescriptor(object, propertyKey);
  assertFalse(desc === undefined);
  assertTrue("value" in desc);
  assertEquals(value, desc.value);
  assertEquals(writable, desc.writable);
  assertEquals(enumerable, desc.enumerable);
  assertEquals(configurable, desc.configurable);
  heapDumpObjectAssignState.descriptors.push(desc);
}

(function basicMultipleSourcesAndSymbols() {
  const symA = Symbol("assign-a");
  const symB = Symbol("assign-b");
  const target = {existing: "target"};
  const sourceA = {alpha: 1};
  const sourceB = Object.defineProperties({beta: 2, [symA]: "symbol-a"}, {
    hidden: {value: "skip", enumerable: false},
    [symB]: {value: "symbol-b", enumerable: true, configurable: true},
  });

  const result = Object.assign(target, sourceA, sourceB);
  assertSame(target, result);
  assertEquals(1, target.alpha);
  assertEquals(2, target.beta);
  assertEquals("symbol-a", target[symA]);
  assertEquals("symbol-b", target[symB]);
  assertFalse("hidden" in target);

  heapDumpObjectAssignState.symbols.push(symA, symB);
  heapDumpObjectAssignState.sources.push(sourceA, sourceB);
  heapDumpObjectAssignState.targets.push(target);
})();

(function attributesAndAccessors() {
  const source = {a: 1, b: 2, c: 3, get d() { return 4; }};
  Object.defineProperty(source, "b", {writable: false});
  Object.defineProperty(source, "c", {configurable: false});
  const created = Object.assign({}, source);
  checkDataProperty(created, "a", 1, true, true, true);
  checkDataProperty(created, "b", 2, true, true, true);
  checkDataProperty(created, "c", 3, true, true, true);
  checkDataProperty(created, "d", 4, true, true, true);

  const setterLog = [];
  const setterTarget = {
    set value(v) { setterLog.push({setterValue: v}); },
  };
  Object.assign(setterTarget, {value: "via-setter"});
  assertEquals([{setterValue: "via-setter"}], setterLog);

  heapDumpObjectAssignState.sources.push(source);
  heapDumpObjectAssignState.targets.push(created, setterTarget);
  heapDumpObjectAssignState.logs.push(setterLog);
})();

(function traversalSnapshotAndReconfigure() {
  const addDuringTraversalSource = {
    get first() {
      this.late = "created-after-keys";
      return "first-value";
    },
  };
  const addTarget = Object.assign({}, addDuringTraversalSource);
  assertEquals("first-value", addTarget.first);
  assertFalse("late" in addTarget);

  const reconfigureLog = [];
  const reconfigureSource = {k1: 10};
  Object.defineProperty(reconfigureSource, "k2", {
    value: 20, enumerable: false, configurable: true,
  });
  const reconfigureTarget = {
    set k1(v) {
      reconfigureLog.push(["set-k1", v]);
      Object.defineProperty(reconfigureSource, "k2", {
        value: 20, enumerable: true, configurable: true,
      });
    },
  };
  Object.assign(reconfigureTarget, reconfigureSource);
  assertEquals(20, reconfigureTarget.k2);

  heapDumpObjectAssignState.sources.push(
      addDuringTraversalSource, reconfigureSource);
  heapDumpObjectAssignState.targets.push(addTarget, reconfigureTarget);
  heapDumpObjectAssignState.logs.push(reconfigureLog);
})();

(function stringSymbolOrderAndProxySource() {
  const orderLog = [];
  const sym1 = Symbol("order-1");
  const sym2 = Symbol("order-2");
  const orderedSource = {
    get [sym1]() { orderLog.push("get sym1"); return "s1"; },
    get a() { orderLog.push("get a"); return "a"; },
    get [sym2]() { orderLog.push("get sym2"); return "s2"; },
    get b() { orderLog.push("get b"); return "b"; },
    get c() { orderLog.push("get c"); return "c"; },
  };
  const orderedTarget = Object.assign({}, orderedSource);
  assertEquals(["get a", "get b", "get c", "get sym1", "get sym2"],
               orderLog);
  assertEquals("s1", orderedTarget[sym1]);
  assertEquals("s2", orderedTarget[sym2]);

  const proxyLog = [];
  const proxyTarget = {};
  const proxySource = new Proxy({backing: "source"}, {
    ownKeys(target) {
      proxyLog.push(["ownKeys", target.backing]);
      return ["x", "y"];
    },
    getOwnPropertyDescriptor(target, key) {
      proxyLog.push(["descriptor", key]);
      return {enumerable: true, configurable: true};
    },
    get(target, key) {
      proxyLog.push(["get", key]);
      return key + "-value";
    },
  });
  Object.assign(proxyTarget, proxySource);
  assertEquals({x: "x-value", y: "y-value"}, proxyTarget);

  heapDumpObjectAssignState.symbols.push(sym1, sym2);
  heapDumpObjectAssignState.sources.push(orderedSource, proxySource);
  heapDumpObjectAssignState.targets.push(orderedTarget, proxyTarget);
  heapDumpObjectAssignState.logs.push(orderLog, proxyLog);
})();

(function primitiveTargetsAndStringSource() {
  const boolTarget = Object.assign(true, {boxed: "bool"});
  const numberTarget = Object.assign(7, {boxed: "number"});
  const stringTarget = Object.assign("abc", {boxed: "string"});
  assertTrue(boolTarget instanceof Boolean);
  assertTrue(numberTarget instanceof Number);
  assertTrue(stringTarget instanceof String);
  assertEquals("bool", boolTarget.boxed);
  assertEquals("number", numberTarget.boxed);
  assertEquals("string", stringTarget.boxed);

  const fromString = Object.assign({}, "abc");
  assertEquals({0: "a", 1: "b", 2: "c"}, fromString);

  heapDumpObjectAssignState.primitiveTargets.push(
      boolTarget, numberTarget, stringTarget);
  heapDumpObjectAssignState.targets.push(fromString);
})();

(function failureCasesRemainReachable() {
  function AssignSetterError() {}
  function AssignGetterError() {}

  const setterLog = [];
  const setterTarget = {
    set first(v) {
      setterLog.push(["first", v]);
      throw new AssignSetterError();
    },
  };
  const setterError = assertThrows(
      () => Object.assign(setterTarget, {first: 1, second: 2}),
      AssignSetterError);
  assertFalse("second" in setterTarget);

  const getterSource = {
    get boom() { throw new AssignGetterError(); },
  };
  const getterError = assertThrows(
      () => Object.assign({}, getterSource), AssignGetterError);

  const nonWritableTarget = {};
  Object.defineProperty(nonWritableTarget, "locked", {
    value: "original", writable: false, enumerable: true,
  });
  const nonWritableError = assertThrows(
      () => Object.assign(nonWritableTarget, {locked: "new-value"}),
      TypeError);
  assertEquals("original", nonWritableTarget.locked);

  assertThrows(() => Object.assign(null, {}), TypeError);
  assertThrows(() => Object.assign(undefined, {}), TypeError);

  heapDumpObjectAssignState.sources.push(getterSource);
  heapDumpObjectAssignState.targets.push(setterTarget, nonWritableTarget);
  heapDumpObjectAssignState.errors.push(
      setterError, getterError, nonWritableError);
  heapDumpObjectAssignState.logs.push(setterLog);
})();

keepAssignRecord("summary", {
  sourceCount: heapDumpObjectAssignState.sources.length,
  targetCount: heapDumpObjectAssignState.targets.length,
  descriptorCount: heapDumpObjectAssignState.descriptors.length,
  logCount: heapDumpObjectAssignState.logs.length,
});
