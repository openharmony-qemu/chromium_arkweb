// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Array.prototype.flat/flatMap depth, callback, species, and generic
// receiver states alive for heap-dump smoke coverage. This follows stable
// portions of V8's harmony/array-flat*.js and array-flatMap*.js tests.

const heapDumpArrayFlatState = {
  inputs: [],
  results: [],
  speciesResults: [],
  logs: [],
  descriptors: [],
  errors: [],
};

function keepArrayFlat(name, value) {
  heapDumpArrayFlatState[name] = value;
  return value;
}

(function flatDepthAndCoercion() {
  const nested = [
    {kind: "root"},
    [{kind: "depth-one"}],
    [[{kind: "depth-two"}]],
  ];
  const depthZero = nested.flat(0);
  const depthOne = nested.flat();
  const depthTwo = nested.flat(2);
  const depthInfinity = nested.flat(Infinity);
  const depthTrue = nested.flat(true);
  const depthString = nested.flat("2");
  assertEquals(3, depthZero.length);
  assertEquals("depth-one", depthOne[1].kind);
  assertEquals("depth-two", depthTwo[2].kind);
  assertEquals("depth-two", depthInfinity[2].kind);
  assertEquals("depth-one", depthTrue[1].kind);
  assertEquals("depth-two", depthString[2].kind);

  const depthProxy = nested.flat(new Proxy({valueOf() { return 1; }}, {}));
  assertEquals("depth-one", depthProxy[1].kind);

  const symbolDepthError =
      assertThrows(() => nested.flat(Symbol("bad-depth")), TypeError);
  const nullPrototypeDepthError =
      assertThrows(() => nested.flat(Object.create(null)), TypeError);

  heapDumpArrayFlatState.inputs.push(nested);
  heapDumpArrayFlatState.results.push(
      depthZero, depthOne, depthTwo, depthInfinity, depthTrue, depthString,
      depthProxy);
  heapDumpArrayFlatState.errors.push(
      symbolDepthError, nullPrototypeDepthError);
})();

(function flatSkipsHolesAndReadsPrototype() {
  const holey = [
    {slot: 0},
    ,
    [{slot: 2}],
    ,
    [[{slot: 4}]],
  ];
  const skipped = holey.flat(Infinity);
  assertEquals([{slot: 0}, {slot: 2}, {slot: 4}], skipped);

  Array.prototype[1] = {slot: "prototype-one"};
  try {
    const prototypeFilled = holey.flat(1);
    assertEquals("prototype-one", prototypeFilled[1].slot);
    heapDumpArrayFlatState.results.push(prototypeFilled);
  } finally {
    delete Array.prototype[1];
  }

  heapDumpArrayFlatState.inputs.push(holey);
  heapDumpArrayFlatState.results.push(skipped);
})();

(function genericReceiversAndLengthGetters() {
  const generic = {
    0: [{kind: "array-element"}],
    1: {kind: "plain-object"},
    2: [{kind: "tail"}],
    length: 3,
  };
  const genericFlat = Array.prototype.flat.call(generic, 1);
  assertEquals("array-element", genericFlat[0].kind);
  assertEquals("plain-object", genericFlat[1].kind);
  assertEquals("tail", genericFlat[2].kind);

  const lengthLog = [];
  const lengthGetter = {
    get length() {
      lengthLog.push("length");
      return 0;
    },
  };
  const emptyFlat = Array.prototype.flat.call(lengthGetter, Infinity);
  const emptyFlatMap = Array.prototype.flatMap.call(lengthGetter, (value) => [
    value,
  ]);
  assertEquals(["length", "length"], lengthLog);
  assertEquals([], emptyFlat);
  assertEquals([], emptyFlatMap);

  heapDumpArrayFlatState.inputs.push(generic, lengthGetter);
  heapDumpArrayFlatState.results.push(genericFlat, emptyFlat, emptyFlatMap);
  heapDumpArrayFlatState.logs.push(lengthLog);
})();

(function flatMapCallbackAndThisArg() {
  const callbackLog = [];
  const thisArg = {kind: "this-arg"};
  const input = [{rank: 1}, {rank: 2}, {rank: 3}];
  const mapped = input.flatMap(function(element, index, receiver) {
    callbackLog.push({
      seen: element.rank,
      index,
      thisKind: this.kind,
      sameReceiver: receiver === input,
    });
    return [{rank: element.rank}, [element.rank * element.rank]];
  }, thisArg);
  assertEquals(6, mapped.length);
  assertEquals(1, mapped[0].rank);
  assertEquals([1], mapped[1]);
  assertEquals("this-arg", callbackLog[0].thisKind);
  assertTrue(callbackLog[2].sameReceiver);

  const genericFlatMap = Array.prototype.flatMap.call({
    0: "a",
    1: "b",
    length: 2,
  }, (value, index) => [{value, index}]);
  assertEquals([{value: "a", index: 0}, {value: "b", index: 1}],
               genericFlatMap);

  const nonCallableError =
      assertThrows(() => [].flatMap({notCallable: true}), TypeError);
  const nullReceiverError = assertThrows(
      () => Array.prototype.flatMap.call(null, (value) => value), TypeError);

  heapDumpArrayFlatState.inputs.push(input);
  heapDumpArrayFlatState.results.push(mapped, genericFlatMap);
  heapDumpArrayFlatState.logs.push(callbackLog);
  heapDumpArrayFlatState.errors.push(nonCallableError, nullReceiverError);
})();

(function speciesResults() {
  class PlainSpeciesArray extends Array {
    static get [Symbol.species]() {
      return Array;
    }
  }
  const plainSpecies = new PlainSpeciesArray(
      [{kind: "plain-a"}], [{kind: "plain-b"}]);
  const plainFlat = plainSpecies.flat();
  const plainFlatMap = plainSpecies.flatMap((value) => [value, value]);
  assertFalse(plainFlat instanceof PlainSpeciesArray);
  assertFalse(plainFlatMap instanceof PlainSpeciesArray);
  assertTrue(plainFlat instanceof Array);

  class SelfSpeciesArray extends Array {
    static get [Symbol.species]() {
      return this;
    }
  }
  const selfSpecies = new SelfSpeciesArray(
      [{kind: "self-a"}], [{kind: "self-b"}]);
  const selfFlat = selfSpecies.flat();
  const selfFlatMap = selfSpecies.flatMap((value) => [value]);
  assertTrue(selfFlat instanceof SelfSpeciesArray);
  assertTrue(selfFlatMap instanceof SelfSpeciesArray);

  heapDumpArrayFlatState.inputs.push(plainSpecies, selfSpecies);
  heapDumpArrayFlatState.speciesResults.push(
      plainFlat, plainFlatMap, selfFlat, selfFlatMap);
})();

(function descriptorsAndReceiverErrors() {
  const flatDescriptor =
      Object.getOwnPropertyDescriptor(Array.prototype, "flat");
  const flatMapDescriptor =
      Object.getOwnPropertyDescriptor(Array.prototype, "flatMap");
  assertEquals(Array.prototype.flat, flatDescriptor.value);
  assertEquals(Array.prototype.flatMap, flatMapDescriptor.value);
  assertTrue(flatDescriptor.writable);
  assertFalse(flatDescriptor.enumerable);
  assertTrue(flatDescriptor.configurable);
  assertTrue(flatMapDescriptor.writable);
  assertFalse(flatMapDescriptor.enumerable);
  assertTrue(flatMapDescriptor.configurable);

  const nullFlatError =
      assertThrows(() => Array.prototype.flat.call(null), TypeError);
  const undefinedFlatError =
      assertThrows(() => Array.prototype.flat.call(undefined), TypeError);

  heapDumpArrayFlatState.descriptors.push(flatDescriptor, flatMapDescriptor);
  heapDumpArrayFlatState.errors.push(nullFlatError, undefinedFlatError);
})();

keepArrayFlat("summary", {
  inputCount: heapDumpArrayFlatState.inputs.length,
  resultCount: heapDumpArrayFlatState.results.length,
  speciesResultCount: heapDumpArrayFlatState.speciesResults.length,
  logCount: heapDumpArrayFlatState.logs.length,
  errorCount: heapDumpArrayFlatState.errors.length,
});
