// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Object.fromEntries iterator-close, key-conversion, and property-creation
// states alive for heap-dump smoke coverage. This follows stable portions of
// V8's harmony/object-fromentries.js mjsunit test.

const heapDumpFromEntriesState = {
  entries: [],
  iterables: [],
  iterators: [],
  results: [],
  symbols: [],
  logs: [],
  errors: [],
  descriptors: [],
};

function keepFromEntriesRecord(name, value) {
  heapDumpFromEntriesState[name] = value;
  return value;
}

(function fastAndSlowKeyOrder() {
  const fastEntries = [
    ["z", {name: "first-z"}],
    ["y", {name: "first-y"}],
    ["x", {name: "x"}],
    ["y", {name: "second-y"}],
    [100, {name: "hundred"}],
  ];
  const fastResult = Object.fromEntries(fastEntries);
  assertEquals(["100", "z", "y", "x"], Object.keys(fastResult));
  assertEquals("second-y", fastResult.y.name);

  const slowEntries = fastEntries.map((entry) => entry.slice());
  let index = 0;
  const slowIterable = {
    [Symbol.iterator]() { return this; },
    next() {
      return {
        done: index >= slowEntries.length,
        value: slowEntries[index++],
      };
    },
    return() { throw new Error("unreachable return"); },
  };
  const slowResult = Object.fromEntries(slowIterable);
  assertEquals(["100", "z", "y", "x"], Object.keys(slowResult));
  assertEquals("second-y", slowResult.y.name);

  heapDumpFromEntriesState.entries.push(fastEntries, slowEntries);
  heapDumpFromEntriesState.iterables.push(slowIterable);
  heapDumpFromEntriesState.results.push(fastResult, slowResult);
})();

(function entriesDoNotUseEntryIterator() {
  class Entry {
    constructor(key, value) {
      this[0] = key;
      this[1] = value;
    }
    get [Symbol.iterator]() {
      throw new Error("entry iterator must not be loaded");
    }
  }
  const entries = [
    new Entry("alpha", {value: 1}),
    new Entry("beta", {value: 2}),
  ];
  const setEntries = new Set(entries);
  const arrayResult = Object.fromEntries(entries);
  const setResult = Object.fromEntries(setEntries);
  assertEquals(1, arrayResult.alpha.value);
  assertEquals(2, setResult.beta.value);

  heapDumpFromEntriesState.entries.push(entries);
  heapDumpFromEntriesState.iterables.push(setEntries);
  heapDumpFromEntriesState.results.push(arrayResult, setResult);
})();

(function createDataPropertyOverPrototypeAccessor() {
  Object.defineProperty(Object.prototype, "heapDumpFromEntriesProperty", {
    configurable: true,
    get() { throw new Error("prototype getter should not run"); },
    set() { throw new Error("prototype setter should not run"); },
  });
  try {
    const result = Object.fromEntries([
      ["heapDumpFromEntriesProperty", "own-value"],
    ]);
    const descriptor = Object.getOwnPropertyDescriptor(
        result, "heapDumpFromEntriesProperty");
    assertEquals("own-value", result.heapDumpFromEntriesProperty);
    assertTrue(descriptor.enumerable);
    assertTrue(descriptor.configurable);
    assertTrue(descriptor.writable);

    heapDumpFromEntriesState.results.push(result);
    heapDumpFromEntriesState.descriptors.push(descriptor);
  } finally {
    delete Object.prototype.heapDumpFromEntriesProperty;
  }
})();

(function keyConversionMutatesLaterEntries() {
  const symToPrimitive = Symbol("fromEntries-toPrimitive");
  const toPrimitiveEntries = [
    [0, "zero"],
    ["a", "letter"],
    [{
      [Symbol.toPrimitive]() {
        delete toPrimitiveEntries[3][0];
        toPrimitiveEntries[3].__proto__ = {0: "mutated-key"};
        return symToPrimitive;
      },
    }, "symbol-value"],
    [3, "mutated-value"],
  ];
  const toPrimitiveResult = Object.fromEntries(toPrimitiveEntries);
  assertEquals(["0", "a", "mutated-key", symToPrimitive],
               Reflect.ownKeys(toPrimitiveResult));
  assertEquals("symbol-value", toPrimitiveResult[symToPrimitive]);

  const symExisting = Symbol("fromEntries-existing");
  const toStringEntries = [
    [symExisting, "symbol-existing"],
    [0, "zero"],
    [{
      toString() {
        delete toStringEntries[3][0];
        toStringEntries[3].__proto__ = {0: "proto-key"};
        return "z";
      },
      valueOf() { throw new Error("valueOf should not run"); },
    }, "z-value"],
    [3, "proto-value"],
  ];
  const toStringResult = Object.fromEntries(toStringEntries);
  assertEquals(["0", "z", "proto-key", symExisting],
               Reflect.ownKeys(toStringResult));
  assertEquals("symbol-existing", toStringResult[symExisting]);

  heapDumpFromEntriesState.entries.push(toPrimitiveEntries, toStringEntries);
  heapDumpFromEntriesState.results.push(toPrimitiveResult, toStringResult);
  heapDumpFromEntriesState.symbols.push(symToPrimitive, symExisting);
})();

(function iteratorCloseOnAbruptEntryAccess() {
  class KeyAccessorError {}
  class ValueAccessorError {}
  class KeyToStringError {}

  const keyLog = [];
  const keyEntries = [{
    get 0() {
      keyLog.push("get-key");
      throw new KeyAccessorError();
    },
    get 1() {
      keyLog.push("unreachable-value");
      return "value";
    },
  }];
  const keyIterator = keyEntries[Symbol.iterator]();
  keyIterator.return = function() {
    keyLog.push("return");
    return {};
  };
  const keyError = assertThrows(
      () => Object.fromEntries(keyIterator), KeyAccessorError);
  assertEquals(["get-key", "return"], keyLog);

  const valueLog = [];
  const valueEntries = [{
    0: "value-key",
    get 1() {
      valueLog.push("get-value");
      throw new ValueAccessorError();
    },
  }];
  const valueIterator = valueEntries[Symbol.iterator]();
  valueIterator.return = function() {
    valueLog.push("return");
    return {};
  };
  const valueError = assertThrows(
      () => Object.fromEntries(valueIterator), ValueAccessorError);
  assertEquals(["get-value", "return"], valueLog);

  const conversionLog = [];
  const conversionEntries = [{
    get 0() {
      conversionLog.push("get-key");
      return {
        toString() {
          conversionLog.push("toString");
          throw new KeyToStringError();
        },
      };
    },
    get 1() {
      conversionLog.push("get-value");
      return "value";
    },
  }];
  const conversionIterator = conversionEntries[Symbol.iterator]();
  conversionIterator.return = function() {
    conversionLog.push("return");
    return {};
  };
  const conversionError = assertThrows(
      () => Object.fromEntries(conversionIterator), KeyToStringError);
  assertEquals(["get-key", "get-value", "toString", "return"],
               conversionLog);

  heapDumpFromEntriesState.entries.push(
      keyEntries, valueEntries, conversionEntries);
  heapDumpFromEntriesState.iterators.push(
      keyIterator, valueIterator, conversionIterator);
  heapDumpFromEntriesState.logs.push(keyLog, valueLog, conversionLog);
  heapDumpFromEntriesState.errors.push(
      keyError, valueError, conversionError);
})();

(function iteratorAcquisitionAndEntryErrors() {
  class GetIteratorError {}
  class CallIteratorError {}
  class NextError {}

  const getIteratorIterable = {};
  Object.defineProperty(getIteratorIterable, Symbol.iterator, {
    get() { throw new GetIteratorError(); },
  });
  const getIteratorError = assertThrows(
      () => Object.fromEntries(getIteratorIterable), GetIteratorError);

  const callIteratorIterable = {
    [Symbol.iterator]() { throw new CallIteratorError(); },
  };
  const callIteratorError = assertThrows(
      () => Object.fromEntries(callIteratorIterable), CallIteratorError);

  const nextIterable = {
    [Symbol.iterator]() { return this; },
    next() { throw new NextError(); },
    return() { throw new Error("return should not run after next throws"); },
  };
  const nextError = assertThrows(
      () => Object.fromEntries(nextIterable), NextError);

  const primitiveEntryError = assertThrows(
      () => Object.fromEntries([null]), TypeError);
  assertThrows(() => Object.fromEntries(null), TypeError);
  assertThrows(() => Object.fromEntries(undefined), TypeError);

  heapDumpFromEntriesState.iterables.push(
      getIteratorIterable, callIteratorIterable, nextIterable);
  heapDumpFromEntriesState.errors.push(
      getIteratorError, callIteratorError, nextError, primitiveEntryError);
})();

keepFromEntriesRecord("summary", {
  entrySets: heapDumpFromEntriesState.entries.length,
  iterableCount: heapDumpFromEntriesState.iterables.length,
  iteratorCount: heapDumpFromEntriesState.iterators.length,
  resultCount: heapDumpFromEntriesState.results.length,
  errorCount: heapDumpFromEntriesState.errors.length,
});
