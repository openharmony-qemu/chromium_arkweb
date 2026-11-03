// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Keep Array callback builtins alive for heap-dump smoke coverage. This follows
// stable V8 mjsunit coverage for map/filter/every/some/forEach/reduce and
// index lookup paths with sparse arrays, prototype elements, callback closures,
// mid-iteration mutation, and error exits.

const heapDumpArrayIterationState = {
  arrays: [],
  callbacks: [],
  results: [],
  logs: [],
  errors: [],
  prototypes: [],
};

function keepIteration(name, value) {
  heapDumpArrayIterationState[name] = value;
  return value;
}

function keepResult(value) {
  heapDumpArrayIterationState.results.push(value);
  return value;
}

function makeCallbackRecorder(label, callback) {
  const log = [];
  const recorder = function(value, index, array) {
    log.push({label, value, index, length: array.length});
    return callback.call(this, value, index, array);
  };
  recorder.log = log;
  heapDumpArrayIterationState.callbacks.push(recorder);
  heapDumpArrayIterationState.logs.push(log);
  return recorder;
}

(function mapFilterEverySomeForEachWithPrototypeElements() {
  const proto = [];
  proto[0] = 5;
  proto[2] = 9;
  const holey = new Array(4);
  holey[1] = 7;
  holey.__proto__ = proto;

  const mapped = keepResult(holey.map(makeCallbackRecorder(
      'map-prototype', (value, index) => ({mapped: value + index}))));
  assertEquals({mapped: 5}, mapped[0]);
  assertEquals({mapped: 8}, mapped[1]);
  assertEquals({mapped: 11}, mapped[2]);
  assertFalse(3 in mapped);

  const filtered = keepResult(holey.filter(makeCallbackRecorder(
      'filter-prototype', (value) => value > 5)));
  assertEquals([7, 9], filtered);

  assertFalse(keepResult(holey.every(makeCallbackRecorder(
      'every-prototype', (value) => value < 9))));
  assertTrue(keepResult(holey.some(makeCallbackRecorder(
      'some-prototype', (value) => value === 9))));

  holey.forEach(makeCallbackRecorder('forEach-prototype', (value, index) => {
    holey[index] = value + 1;
  }));
  assertEquals(6, holey[0]);
  assertEquals(8, holey[1]);
  assertEquals(10, holey[2]);

  heapDumpArrayIterationState.arrays.push(holey, mapped, filtered);
  heapDumpArrayIterationState.prototypes.push(proto);
})();

(function reduceAndReduceRightRecordClosures() {
  const records = [];
  const source = [2, 4, , 6];
  const reducer = function(accumulator, value, index, array) {
    records.push({accumulator, value, index, length: array.length});
    return accumulator + value;
  };
  heapDumpArrayIterationState.callbacks.push(reducer);
  heapDumpArrayIterationState.logs.push(records);

  assertEquals(12, keepResult(source.reduce(reducer, 0)));
  assertEquals([0, 1, 3], records.map((record) => record.index));

  const reduceRightRecords = [];
  const reduceRight = function(accumulator, value, index) {
    reduceRightRecords.push({accumulator, value, index});
    return accumulator + String(value);
  };
  heapDumpArrayIterationState.callbacks.push(reduceRight);
  heapDumpArrayIterationState.logs.push(reduceRightRecords);
  assertEquals('642', keepResult(source.reduceRight(reduceRight, '')));
  assertEquals([3, 1, 0], reduceRightRecords.map((record) => record.index));

  heapDumpArrayIterationState.arrays.push(source);
})();

(function mutationDuringIterationUsesInitialLength() {
  const source = [1, 2, 3];
  const mutationLog = [];
  const mapped = keepResult(source.map(makeCallbackRecorder(
      'map-mutate', (value, index, array) => {
        mutationLog.push({index, length: array.length});
        if (index === 0) {
          array.push(4);
          array[2] = 30;
        }
        if (index === 1) {
          delete array[2];
        }
        return value * 10;
      })));
  assertEquals([10, 20, ,], mapped);
  assertEquals([1, 2, , 4], source);
  assertEquals([0, 1], mutationLog.map((entry) => entry.index));

  const filtered = keepResult(source.filter(makeCallbackRecorder(
      'filter-after-mutate', (value) => value > 1)));
  assertEquals([2, 4], filtered);

  heapDumpArrayIterationState.arrays.push(source, mapped, filtered);
  heapDumpArrayIterationState.logs.push(mutationLog);
})();

(function indexLookupAndGenericReceivers() {
  const values = [NaN, -0, 0, 'x', {id: 1}, 'x'];
  const target = values[4];
  assertTrue(keepResult(values.includes(NaN)));
  assertEquals(1, keepResult(values.indexOf(-0)));
  assertEquals(5, keepResult(values.lastIndexOf('x')));
  assertEquals(4, keepResult(values.indexOf(target)));

  const arrayLike = {length: 4, 0: 'a', 2: 'b', 3: 'a'};
  const genericMap = keepResult(Array.prototype.map.call(
      arrayLike, makeCallbackRecorder('generic-map', (value, index) =>
          value + index)));
  assertEquals(['a0', , 'b2', 'a3'], genericMap);
  assertEquals(3, keepResult(Array.prototype.lastIndexOf.call(arrayLike, 'a')));

  heapDumpArrayIterationState.arrays.push(values, genericMap);
  heapDumpArrayIterationState.objects = [target, arrayLike];
})();

(function callbackErrorPaths() {
  class ArrayIterationError extends Error {}
  const errorSource = [1, 2, 3];
  const error = assertThrows(() => errorSource.map(makeCallbackRecorder(
      'map-error', (value) => {
        if (value === 2) {
          throw new ArrayIterationError('heap dump array callback');
        }
        return value;
      })), ArrayIterationError);
  const reduceError = assertThrows(() => [].reduce((a, b) => a + b), TypeError);
  const callbackTypeError = assertThrows(() => [1].every({notCallable: true}),
                                        TypeError);

  heapDumpArrayIterationState.arrays.push(errorSource);
  heapDumpArrayIterationState.errors.push(
      error, reduceError, callbackTypeError);
})();

keepIteration('summary', {
  arrayCount: heapDumpArrayIterationState.arrays.length,
  callbackCount: heapDumpArrayIterationState.callbacks.length,
  resultCount: heapDumpArrayIterationState.results.length,
  logCount: heapDumpArrayIterationState.logs.length,
  errorCount: heapDumpArrayIterationState.errors.length,
  prototypeCount: heapDumpArrayIterationState.prototypes.length,
});
