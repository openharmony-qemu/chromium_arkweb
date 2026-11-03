// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Array.prototype.splice/unshift mutation, hole, prototype-copy, and
// argument-conversion states alive for heap-dump smoke coverage. This follows
// stable portions of V8's array-splice.js and array-unshift.js mjsunit tests.

const heapDumpSpliceUnshiftState = {
  arrays: [],
  prototypes: [],
  removed: [],
  logs: [],
  errors: [],
  descriptors: [],
};

function keepSpliceUnshift(name, value) {
  heapDumpSpliceUnshiftState[name] = value;
  return value;
}

(function spliceHolesAndInsertions() {
  const holes = new Array(10);
  const removedHole = holes.splice(1, 1, {kind: "one"}, {kind: "two"});
  assertEquals(1, removedHole.length);
  assertFalse(0 in removedHole);
  assertEquals(11, holes.length);
  assertFalse(0 in holes);
  assertEquals("one", holes[1].kind);
  assertEquals("two", holes[2].kind);
  assertFalse(3 in holes);

  const insertOnly = [1, 2, 3];
  const removedNone = insertOnly.splice(1, 0, "a", "b", "c");
  assertEquals([], removedNone);
  assertEquals([1, "a", "b", "c", 2, 3], insertOnly);

  const deleteTail = [1, 2, 3, 4, 5, 6, 7];
  const removedTail = deleteTail.splice(-3);
  assertEquals([1, 2, 3, 4], deleteTail);
  assertEquals([5, 6, 7], removedTail);

  const doubleArray = [1.1, 1.2, 1.3, 1.4];
  const removedDouble = doubleArray.splice(2, 1);
  assertEquals([1.1, 1.2, 1.4], doubleArray);
  assertEquals([1.3], removedDouble);

  heapDumpSpliceUnshiftState.arrays.push(
      holes, insertOnly, deleteTail, doubleArray);
  heapDumpSpliceUnshiftState.removed.push(
      removedHole, removedNone, removedTail, removedDouble);
})();

(function spliceArgumentConversionAndMutation() {
  const conversionLog = [];
  const array = [1, 2, 3];
  const start = {
    valueOf() {
      conversionLog.push("start-valueOf");
      array.push("during-start");
      return 1;
    },
  };
  const count = {
    toString() {
      conversionLog.push("count-toString");
      array.push("during-count");
      return "2";
    },
  };
  const removed = array.splice(start, count, "one", "two");
  assertEquals(["start-valueOf", "count-toString"], conversionLog);
  assertEquals([2, 3], removed);
  assertEquals([1, "one", "two"], array);

  const throwStart = {
    toString() { throw "splice-start-exception"; },
  };
  const startError = assertThrowsEquals(
      () => [1, 2, 3].splice(throwStart, 1), "splice-start-exception");
  const throwCount = {
    toString() { throw "splice-count-exception"; },
  };
  const countError = assertThrowsEquals(
      () => [1, 2, 3].splice(0, throwCount), "splice-count-exception");

  heapDumpSpliceUnshiftState.arrays.push(array);
  heapDumpSpliceUnshiftState.removed.push(removed);
  heapDumpSpliceUnshiftState.logs.push(conversionLog);
  heapDumpSpliceUnshiftState.errors.push(startError, countError);
})();

(function splicePrototypeElements() {
  const proto = [];
  proto[3] = "@3";
  proto[7] = "@7";
  const array = new Array(9);
  array.__proto__ = proto;
  const removed = array.splice(2, 2, "one", undefined, "two");
  assertEquals([, "@3"], removed);
  assertEquals("one", array[2]);
  assertTrue(array.hasOwnProperty(2));
  assertTrue(array.hasOwnProperty(3));
  assertTrue(array.hasOwnProperty(4));
  assertTrue(array.hasOwnProperty(8));
  assertFalse(array.hasOwnProperty(5));
  assertFalse(array.hasOwnProperty(6));
  assertFalse(array.hasOwnProperty(7));
  delete proto[3];
  delete proto[7];
  assertEquals(undefined, array[7]);

  heapDumpSpliceUnshiftState.arrays.push(array);
  heapDumpSpliceUnshiftState.prototypes.push(proto);
  heapDumpSpliceUnshiftState.removed.push(removed);
})();

(function unshiftHolesAndPrototypeElements() {
  const holes = new Array(10);
  const holesLength = holes.unshift("1st", "2nd", "3rd");
  assertEquals(13, holesLength);
  assertTrue(0 in holes);
  assertTrue(1 in holes);
  assertTrue(2 in holes);
  assertFalse(3 in holes);

  const proto = [];
  proto[3] = "@3";
  proto[7] = "@7";
  const array = new Array(9);
  array.__proto__ = proto;
  assertEquals(10, array.unshift("head"));
  assertEquals("head", array[0]);
  assertEquals("@3", array[4]);
  assertTrue(array.hasOwnProperty(4));
  assertEquals("@7", array[8]);
  assertTrue(array.hasOwnProperty(8));
  proto[5] = "@5";
  assertEquals("@5", array[5]);
  assertFalse(array.hasOwnProperty(5));

  const noArgs = new Array(3);
  const noArgsProto = [];
  noArgsProto[0] = "@0";
  noArgsProto[2] = "@2";
  noArgs.__proto__ = noArgsProto;
  assertEquals(3, noArgs.unshift());
  assertFalse(noArgs.hasOwnProperty(0));
  assertFalse(noArgs.hasOwnProperty(2));
  assertEquals("@0", noArgs[0]);
  assertEquals("@2", noArgs[2]);

  heapDumpSpliceUnshiftState.arrays.push(holes, array, noArgs);
  heapDumpSpliceUnshiftState.prototypes.push(proto, noArgsProto);
})();

(function unshiftNonEnumerableAndLengthError() {
  const array = [2, 3];
  Object.defineProperty(array, "1", {enumerable: false, configurable: true});
  array.unshift(1);
  assertEquals([1, 2, 3], array);
  const descriptor = Object.getOwnPropertyDescriptor(array, "2");
  assertEquals(3, descriptor.value);

  const largeLike = {length: 2 ** 53 - 3};
  const lengthError = assertThrows(
      () => Array.prototype.unshift.call(largeLike, 1, 2, 3, 4, 5),
      TypeError);

  heapDumpSpliceUnshiftState.arrays.push(array);
  heapDumpSpliceUnshiftState.descriptors.push(descriptor);
  heapDumpSpliceUnshiftState.errors.push(lengthError);
})();

keepSpliceUnshift("summary", {
  arrayCount: heapDumpSpliceUnshiftState.arrays.length,
  prototypeCount: heapDumpSpliceUnshiftState.prototypes.length,
  removedCount: heapDumpSpliceUnshiftState.removed.length,
  logCount: heapDumpSpliceUnshiftState.logs.length,
  errorCount: heapDumpSpliceUnshiftState.errors.length,
});
