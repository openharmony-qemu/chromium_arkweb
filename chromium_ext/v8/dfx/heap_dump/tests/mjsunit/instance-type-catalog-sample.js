// Flags: --allow-natives-syntax

// Heap-dump sample corpus:
// Build a broad object catalog that is likely to touch many JS-side
// visitor-id / instance-type families in dfx-visitor-list.h.

(function() {
  function check(condition, message) {
    if (!condition) throw new Error(message);
  }

  const weakKey = { id: 'weak-key' };
  const weakTarget = { id: 'weak-target' };

  function* makeGenerator() {
    yield 1;
    yield 2;
  }

  async function makeAsyncResult() {
    return { ok: true };
  }

  class SampleClass {
    constructor(v) {
      this.v = v;
    }
    method() {
      return this.v;
    }
  }

  function addOne(x) {
    return x + 1;
  }

  const map = new Map([[1, 'one'], [2, { deep: true }]]);
  const set = new Set(['a', 'b', 'c']);
  const weakMap = new WeakMap([[weakKey, { held: true }]]);
  const weakSet = new WeakSet([weakKey]);
  const arrayBuffer = new ArrayBuffer(256);
  const dataView = new DataView(arrayBuffer, 8, 64);
  const sharedArrayBuffer =
      typeof SharedArrayBuffer === 'function' ? new SharedArrayBuffer(64) : null;
  const proxy = new Proxy(
      { x: 1, y: 2 },
      { get(target, key) { return Reflect.get(target, key); } });

  let resizableArrayBuffer = null;
  try {
    resizableArrayBuffer = new ArrayBuffer(64, { maxByteLength: 256 });
  } catch (e) {
    // Keep compatibility with runtimes that do not expose resizable AB yet.
  }

  let growableSharedArrayBuffer = null;
  if (typeof SharedArrayBuffer === 'function') {
    try {
      growableSharedArrayBuffer = new SharedArrayBuffer(64, { maxByteLength: 256 });
    } catch (e) {
      // Keep compatibility with runtimes that do not expose growable SAB yet.
    }
  }

  const finalizationRegistry =
      typeof FinalizationRegistry === 'function'
      ? new FinalizationRegistry((held) => held)
      : null;
  const weakRef = typeof WeakRef === 'function' ? new WeakRef(weakTarget) : null;
  if (finalizationRegistry !== null) {
    finalizationRegistry.register(weakTarget, 'weak-held-value');
  }

  const typedViews = {
    int8: new Int8Array(arrayBuffer, 0, 8),
    uint8: new Uint8Array(arrayBuffer, 8, 8),
    uint8Clamped: new Uint8ClampedArray(8),
    int16: new Int16Array(arrayBuffer, 16, 4),
    uint16: new Uint16Array(arrayBuffer, 24, 4),
    int32: new Int32Array(arrayBuffer, 32, 4),
    uint32: new Uint32Array(arrayBuffer, 48, 4),
    float32: new Float32Array([1.25, 2.25, 3.25]),
    float64: new Float64Array([1.5, 2.5, 3.5]),
    bigint64: new BigInt64Array([1n, 2n, 3n]),
    biguint64: new BigUint64Array([1n, 2n, 3n]),
  };

  const atomicsMutex =
      typeof Atomics === 'object' && typeof Atomics.Mutex === 'function'
      ? new Atomics.Mutex()
      : null;
  const atomicsCondition =
      typeof Atomics === 'object' && typeof Atomics.Condition === 'function'
      ? new Atomics.Condition()
      : null;

  globalThis.heapDumpInstanceTypeCatalog = {
    plain: { a: 1, nested: { b: 2 } },
    array: [1, 2, 3, { marker: 'array' }],
    map,
    set,
    weakMap,
    weakSet,
    arrayBuffer,
    dataView,
    sharedArrayBuffer,
    resizableArrayBuffer,
    growableSharedArrayBuffer,
    typedViews,
    date: new Date(1234),
    regexpLiteral: /heap-dump-instance-type/gi,
    regexpObject: new RegExp('heap-dump-instance-type', 'i'),
    error: new Error('heap-dump-error'),
    rangeError: new RangeError('heap-dump-range-error'),
    typeError: new TypeError('heap-dump-type-error'),
    promise: Promise.resolve({ settled: false }),
    generatorObject: makeGenerator(),
    asyncResult: makeAsyncResult(),
    classInstance: new SampleClass(42),
    functionObject: addOne,
    boundFunction: addOne.bind(null, 7),
    proxy,
    symbolValue: Symbol('heap-dump-symbol'),
    bigintValue: 20260401n,
    numberWrapper: Object(7),
    stringWrapper: Object('wrapped-string'),
    symbolWrapper: Object(Symbol('wrapped-symbol')),
    bigintWrapper: Object(7n),
    arrayIterator: [1, 2, 3].values(),
    mapIterator: map.entries(),
    setIterator: set.keys(),
    stringIterator: 'heap-dump'[Symbol.iterator](),
    weakRef,
    finalizationRegistry,
    atomicsMutex,
    atomicsCondition,
  };

  check(globalThis.heapDumpInstanceTypeCatalog.map instanceof Map,
        'map instance check failed');
  check(globalThis.heapDumpInstanceTypeCatalog.dataView instanceof DataView,
        'dataView instance check failed');
  check(globalThis.heapDumpInstanceTypeCatalog.classInstance.method() === 42,
        'class method check failed');
  check(typeof globalThis.heapDumpInstanceTypeCatalog.boundFunction === 'function',
        'boundFunction type check failed');
})();
