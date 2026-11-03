// Flags: --allow-natives-syntax

// Heap-dump sample corpus:
// - plain objects / arrays
// - Map / Set / WeakMap / WeakSet
// - ArrayBuffer / DataView / typed arrays
// - Date / Error / RegExp / Symbol / BigInt

(function() {
  const weakKey = { key: 'weak' };
  const buffer = new ArrayBuffer(128);

  globalThis.heapDumpCoreBuiltinsSample = {
    plain: { alpha: 1, nested: { beta: 2 } },
    array: [1, 2, 3, { marker: 'array' }],
    map: new Map([[1, 'one'], [2, { deep: true }]]),
    set: new Set(['a', 'b', 'c']),
    weakMap: new WeakMap([[weakKey, { held: 'value' }]]),
    weakSet: new WeakSet([weakKey]),
    arrayBuffer: buffer,
    dataView: new DataView(buffer),
    int8: new Int8Array(buffer, 0, 8),
    uint16: new Uint16Array(buffer, 16, 4),
    float64: new Float64Array([1.5, 2.5, 3.5]),
    date: new Date(0),
    error: new Error('heap-dump-core-builtins-sample'),
    regexp: /heap-dump-core/gi,
    symbol: Symbol('heap-dump-core'),
    bigint: 20260327n,
  };

  assertTrue(globalThis.heapDumpCoreBuiltinsSample.map instanceof Map);
  assertTrue(globalThis.heapDumpCoreBuiltinsSample.dataView instanceof DataView);
})();
