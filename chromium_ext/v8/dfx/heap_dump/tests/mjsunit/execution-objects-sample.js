// Heap-dump sample corpus:
// - Promise / async result
// - array/map/set iterators
// - generator object
// - bound function
// - class instance
// - primitive wrappers

(function() {
  function* makeGenerator() {
    yield 1;
    yield 2;
  }

  async function makeAsyncValue() {
    return { ok: true };
  }

  class HeapDumpSampleClass {
    constructor() {
      this.answer = 42;
    }
  }

  function sampleTarget(x) {
    return x + 1;
  }

  globalThis.heapDumpExecutionSample = {
    promise: Promise.resolve({ settled: false }),
    arrayIterator: [1, 2, 3].values(),
    mapIterator: new Map([[1, 'one']]).entries(),
    setIterator: new Set([1, 2, 3]).keys(),
    generatorObject: makeGenerator(),
    asyncResult: makeAsyncValue(),
    boundFunction: sampleTarget.bind(null, 7),
    classInstance: new HeapDumpSampleClass(),
    numberWrapper: Object(7),
    stringWrapper: Object('wrapped'),
    symbolWrapper: Object(Symbol('wrapped-symbol')),
    bigintWrapper: Object(7n),
  };

  assertEquals(42, globalThis.heapDumpExecutionSample.classInstance.answer);
  assertEquals('function', typeof globalThis.heapDumpExecutionSample.boundFunction);
})();
