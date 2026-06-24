// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Array.prototype.concat spreadability, prototype, proxy, and getter-order
// states alive for heap-dump smoke coverage. This follows stable portions of
// V8's array-concat.js and es6/array-concat-*.js mjsunit tests.

const heapDumpConcatState = {
  arrays: [],
  arrayLikes: [],
  proxies: [],
  results: [],
  logs: [],
  errors: [],
  descriptors: [],
};

function keepConcat(name, value) {
  heapDumpConcatState[name] = value;
  return value;
}

(function stringWrapperAndBasicHoles() {
  const concat = Array.prototype.concat;
  const stringWrapper = new String("abcd");
  const stringAsElement = [1, 2, 3].concat(stringWrapper);
  const wrapperReceiver = concat.call(stringWrapper);
  assertEquals([1, 2, 3, new String("abcd")], stringAsElement);
  assertEquals([new String("abcd")], wrapperReceiver);

  const protoArray = [1, 2, 3];
  protoArray.__proto__ = stringWrapper;
  protoArray.length = 4;
  const protoStringResult = concat.call(protoArray);
  assertEquals([1, 2, 3, "d"], protoStringResult);

  const holey = [void 0, "a", , "c"].concat(["d", , "f", [0, , 2], void 0]);
  assertEquals(9, holey.length);
  assertFalse(2 in holey);
  assertFalse(5 in holey);
  assertTrue(7 in holey);

  heapDumpConcatState.arrays.push(protoArray, holey);
  heapDumpConcatState.arrayLikes.push(stringWrapper);
  heapDumpConcatState.results.push(
      stringAsElement, wrapperReceiver, protoStringResult, holey);
})();

(function prototypeNumericPropertiesAndShadowing() {
  const arrayProto = [];
  const sparse = new Array(140);
  sparse.__proto__ = arrayProto;
  sparse.push("foo");
  sparse["124"] = "doo";
  arrayProto["123"] = "baz";
  const copiedFromPrototype = sparse.concat(["bar"]);
  assertEquals(142, copiedFromPrototype.length);
  assertEquals("doo", copiedFromPrototype[124]);
  assertEquals("baz", copiedFromPrototype[123]);
  assertEquals("foo", copiedFromPrototype[140]);
  assertEquals("bar", copiedFromPrototype[141]);

  arrayProto["123"] = undefined;
  assertEquals(undefined, sparse[123]);
  assertEquals("baz", copiedFromPrototype[123]);

  arrayProto[123] = "baz";
  sparse[123] = "xyz";
  const shadowed = sparse.concat(["tail"]);
  assertEquals("xyz", shadowed[123]);

  arrayProto.moe = "joe";
  sparse.ben = "jerry";
  const noNamedCopy = sparse.concat(["tail"]);
  assertEquals(undefined, noNamedCopy.ben);
  assertEquals("joe", sparse.moe);

  sparse[-1] = "minus1";
  const noNegativeIndex = sparse.concat(["tail"]);
  assertEquals(undefined, noNegativeIndex[-1]);
  assertEquals(undefined, noNegativeIndex[0xffffffff]);

  heapDumpConcatState.arrays.push(sparse, arrayProto);
  heapDumpConcatState.results.push(
      copiedFromPrototype, shadowed, noNamedCopy, noNegativeIndex);
})();

(function getterOrderAndLengthMutation() {
  const arr1 = [, 2];
  const arr2 = [1, 3];
  const baseline = [].concat(arr1, arr2);
  assertEquals([, 2, 1, 3], baseline);

  Object.defineProperty(arr1, 0, {
    get() {
      arr2.push("X");
      return undefined;
    },
    configurable: true,
  });
  const mutatedLength = [].concat(arr1, arr2);
  assertEquals([undefined, 2, 1, 3, "X"], mutatedLength);

  const trace = [];
  const arr3 = [];
  arr3.length = 128;
  for (let i = 0; i < 12; i++) {
    Object.defineProperty(arr3, i * i, {
      get() {
        trace.push(i);
        return {getter: i};
      },
      configurable: true,
    });
  }
  const getterResult = [0].concat(arr3, arr3);
  assertEquals(257, getterResult.length);
  assertEquals(24, trace.length);
  assertEquals(trace.slice(0, 12), trace.slice(12));

  heapDumpConcatState.arrays.push(arr1, arr2, arr3);
  heapDumpConcatState.results.push(baseline, mutatedLength, getterResult);
  heapDumpConcatState.logs.push(trace);
})();

(function spreadableAndUnspreadableObjects() {
  const spreadableSparse = {length: 5};
  spreadableSparse[Symbol.isConcatSpreadable] = true;
  const sparseSpreadResult = [].concat(spreadableSparse);
  assertEquals(5, sparseSpreadResult.length);
  assertFalse(0 in sparseSpreadResult);

  const spreadableObject = {
    0: {slot: 0},
    2: {slot: 2},
    length: 4,
    [Symbol.isConcatSpreadable]: true,
  };
  const objectSpreadResult = ["head"].concat(spreadableObject);
  assertEquals(5, objectSpreadResult.length);
  assertEquals("head", objectSpreadResult[0]);
  assertEquals(0, objectSpreadResult[1].slot);
  assertFalse(2 in objectSpreadResult);
  assertEquals(2, objectSpreadResult[3].slot);

  const unspreadableArray = [1, 2, 3];
  unspreadableArray[Symbol.isConcatSpreadable] = false;
  const unspreadResult = [].concat(unspreadableArray);
  assertEquals([[1, 2, 3]], unspreadResult);

  const regexpSpreadable = /heap/;
  regexpSpreadable[0] = "regexp-element";
  regexpSpreadable.length = 1;
  regexpSpreadable[Symbol.isConcatSpreadable] = true;
  const regexpResult = [].concat(regexpSpreadable);
  assertEquals(["regexp-element"], regexpResult);

  heapDumpConcatState.arrays.push(unspreadableArray);
  heapDumpConcatState.arrayLikes.push(
      spreadableSparse, spreadableObject, regexpSpreadable);
  heapDumpConcatState.results.push(
      sparseSpreadResult, objectSpreadResult, unspreadResult, regexpResult);
})();

(function proxyTrapOrderAndErrorCases() {
  const log = [];
  const logger = {};
  const handler = new Proxy({}, logger);
  logger.get = function(target, trap) {
    return function(...args) {
      log.push([trap, ...args]);
      return Reflect[trap](...args);
    };
  };
  const target = ["a", "b"];
  target[Symbol.isConcatSpreadable] = undefined;
  const proxy = new Proxy(target, handler);
  const proxyResult = [].concat(proxy);
  assertEquals(["a", "b"], proxyResult);
  assertEquals("get", log[0][0]);
  assertEquals(Symbol.isConcatSpreadable, log[0][2]);
  assertEquals("length", log[1][2]);
  assertEquals("has", log[2][0]);
  assertEquals("get", log[3][0]);

  function SpreadableGetterError() {}
  const spreadableGetterThrows = {};
  Object.defineProperty(spreadableGetterThrows, Symbol.isConcatSpreadable, {
    get() { throw new SpreadableGetterError(); },
  });
  const spreadableError = assertThrows(
      () => [].concat(spreadableGetterThrows), SpreadableGetterError);

  function LengthGetterError() {}
  const lengthGetterThrows = {
    [Symbol.isConcatSpreadable]: true,
  };
  Object.defineProperty(lengthGetterThrows, "length", {
    get() { throw new LengthGetterError(); },
  });
  const lengthError = assertThrows(
      () => [].concat(lengthGetterThrows), LengthGetterError);

  heapDumpConcatState.proxies.push(proxy, handler);
  heapDumpConcatState.arrayLikes.push(spreadableGetterThrows, lengthGetterThrows);
  heapDumpConcatState.results.push(proxyResult);
  heapDumpConcatState.logs.push(log);
  heapDumpConcatState.errors.push(spreadableError, lengthError);
  heapDumpConcatState.descriptors.push(
      Object.getOwnPropertyDescriptor(
          spreadableGetterThrows, Symbol.isConcatSpreadable),
      Object.getOwnPropertyDescriptor(lengthGetterThrows, "length"));
})();

keepConcat("summary", {
  arrayCount: heapDumpConcatState.arrays.length,
  arrayLikeCount: heapDumpConcatState.arrayLikes.length,
  proxyCount: heapDumpConcatState.proxies.length,
  resultCount: heapDumpConcatState.results.length,
  logCount: heapDumpConcatState.logs.length,
  errorCount: heapDumpConcatState.errors.length,
});
