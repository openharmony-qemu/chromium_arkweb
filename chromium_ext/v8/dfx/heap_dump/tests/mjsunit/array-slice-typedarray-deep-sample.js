// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Keep Array.prototype.slice and TypedArray.prototype.slice hole, prototype,
// arguments-object, species, and detach-error states alive for heap-dump smoke
// coverage. This follows stable portions of V8's array-slice.js,
// typed-array-slice.js, and es6/typedarray-slice.js mjsunit tests.

const heapDumpSliceState = {
  arrays: [],
  prototypes: [],
  typedArrays: [],
  buffers: [],
  results: [],
  logs: [],
  errors: [],
  descriptors: [],
};

function keepSlice(name, value) {
  heapDumpSliceState[name] = value;
  return value;
}

(function arrayHolesIndicesAndConversion() {
  const holes = new Array(10);
  const holesSlice = holes.slice();
  assertEquals(10, holesSlice.length);
  assertFalse(0 in holesSlice);

  const indexed = new Array(7);
  const negativeSlice = indexed.slice(-3);
  const positiveSlice = indexed.slice(4);
  const endSlice = indexed.slice(0, -3);
  assertEquals(3, negativeSlice.length);
  assertEquals(3, positiveSlice.length);
  assertEquals(4, endSlice.length);

  const conversionLog = [];
  const conversionArray = [0, 1, 2, 3];
  const start = {
    valueOf() {
      conversionLog.push("start-valueOf");
      conversionArray.push("during-start");
      return 1;
    },
  };
  const end = {
    toString() {
      conversionLog.push("end-toString");
      conversionArray.push("during-end");
      return "3";
    },
  };
  const conversionSlice = conversionArray.slice(start, end);
  assertEquals(["start-valueOf", "end-toString"], conversionLog);
  assertEquals([1, 2], conversionSlice);
  assertEquals([0, 1, 2, 3, "during-start", "during-end"],
               conversionArray);

  const throwingEnd = {
    toString() { throw "slice-end-exception"; },
  };
  const conversionError = assertThrowsEquals(
      () => [1, 2, 3].slice(0, throwingEnd), "slice-end-exception");

  heapDumpSliceState.arrays.push(holes, indexed, conversionArray);
  heapDumpSliceState.results.push(
      holesSlice, negativeSlice, positiveSlice, endSlice, conversionSlice);
  heapDumpSliceState.logs.push(conversionLog);
  heapDumpSliceState.errors.push(conversionError);
})();

(function arrayPrototypeIndexedPropertiesAndArguments() {
  const proto = [];
  proto[3] = "@3";
  proto[7] = "@7";
  const array = new Array(9);
  array.__proto__ = proto;
  const prototypeSlice = array.slice();
  assertEquals(9, prototypeSlice.length);
  assertEquals("@3", prototypeSlice[3]);
  assertTrue(prototypeSlice.hasOwnProperty(3));
  assertEquals("@7", prototypeSlice[7]);
  assertTrue(prototypeSlice.hasOwnProperty(7));
  proto[5] = "@5";
  assertEquals("@5", array[5]);
  assertFalse(array.hasOwnProperty(5));
  assertFalse(prototypeSlice.hasOwnProperty(5));

  function makeArgumentsSlice(expected, a0, a1, a2, a3) {
    const result = Array.prototype.slice.call(arguments, 2);
    %HeapObjectVerify(result);
    %HeapObjectVerify(arguments);
    assertEquals(expected, result);
    return {argumentsObject: arguments, result};
  }
  const oneArg = makeArgumentsSlice(["a"], ["a"], "a");
  const mixedArgs = makeArgumentsSlice(
      ["a", 1, undefined, void 0], ["a", 1, undefined, void 0],
      "a", 1, undefined, void 0);

  heapDumpSliceState.arrays.push(array);
  heapDumpSliceState.prototypes.push(proto);
  heapDumpSliceState.results.push(prototypeSlice, oneArg.result, mixedArgs.result);
  heapDumpSliceState.argumentsRecords = [oneArg, mixedArgs];
})();

(function typedArrayBasicSlicesAndSpecies() {
  const constructors = [
    Uint8Array,
    Int8Array,
    Uint16Array,
    Int16Array,
    Uint32Array,
    Int32Array,
    Uint8ClampedArray,
    Float32Array,
    Float64Array,
  ];
  const basicResults = [];
  const speciesResults = [];
  for (const Ctor of constructors) {
    const typed = new Ctor([1, 2, 3, 4]);
    const sliced = typed.slice(1, 3);
    assertEquals(2, sliced.length);
    assertEquals(2, sliced[0]);
    assertEquals(3, sliced[1]);
    assertTrue(sliced instanceof Ctor);
    basicResults.push(sliced);

    const speciesSource = new Ctor([1, 2, 3, 4, 5, 6]);
    speciesSource.constructor = {[Symbol.species]: Ctor};
    const speciesSlice = speciesSource.slice(3);
    assertArrayEquals([4, 5, 6], Array.from(speciesSlice));
    speciesResults.push(speciesSlice);

    heapDumpSliceState.typedArrays.push(typed, speciesSource);
    heapDumpSliceState.buffers.push(typed.buffer, speciesSource.buffer);
  }

  heapDumpSliceState.results.push(basicResults, speciesResults);
})();

(function typedArrayDetachedAndBadSpeciesErrors() {
  const errors = [];
  const detached = new Uint8Array([1, 2, 3, 4, 5, 6]);
  %ArrayBufferDetach(detached.buffer);
  const detachedError = assertThrows(() => {
    detached.slice({
      [Symbol.toPrimitive]() {
        throw new Error("parameter should not be processed after detach");
      },
    });
  }, TypeError);
  errors.push(detachedError);

  class BadSpeciesArray extends Uint8Array {
    static get [Symbol.species]() {
      return Array;
    }
  }
  const badSpecies = new BadSpeciesArray([1, 2, 3, 4]);
  const speciesError = assertThrows(() => badSpecies.slice(), TypeError);
  errors.push(speciesError);

  class Int16SpeciesArray extends Uint8Array {
    static get [Symbol.species]() {
      return Int16Array;
    }
  }
  const speciesTyped = new Int16SpeciesArray([1, 2, 3, 4, 5]);
  const int16Result = speciesTyped.slice(2);
  assertTrue(int16Result instanceof Int16Array);
  assertArrayEquals([3, 4, 5], Array.from(int16Result));

  heapDumpSliceState.typedArrays.push(detached, badSpecies, speciesTyped);
  heapDumpSliceState.buffers.push(
      detached.buffer, badSpecies.buffer, speciesTyped.buffer);
  heapDumpSliceState.results.push(int16Result);
  heapDumpSliceState.errors.push(...errors);
})();

keepSlice("summary", {
  arrayCount: heapDumpSliceState.arrays.length,
  prototypeCount: heapDumpSliceState.prototypes.length,
  typedArrayCount: heapDumpSliceState.typedArrays.length,
  bufferCount: heapDumpSliceState.buffers.length,
  resultCount: heapDumpSliceState.results.length,
  logCount: heapDumpSliceState.logs.length,
  errorCount: heapDumpSliceState.errors.length,
});
