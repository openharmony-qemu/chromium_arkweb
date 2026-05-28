// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --validate-asm --allow-natives-syntax

// Keep asm.js -> Wasm translation artifacts alive for heap dump smoke. This
// follows V8's asm mjsunit patterns and retains valid compiled asm modules,
// asm heap buffers/views, foreign imports, and a non-asm fallback module.

(function() {
  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function MemoryModule(stdlib, foreign, heap) {
    "use asm";
    var i32 = new stdlib.Int32Array(heap);
    var imul = stdlib.Math.imul;
    var callback = foreign.callback;

    function store(index, value) {
      index = index | 0;
      value = value | 0;
      i32[index >> 2] = value;
    }

    function load(index) {
      index = index | 0;
      return i32[index >> 2] | 0;
    }

    function call(value) {
      value = value | 0;
      return callback((imul(value, 3) + 7) | 0) | 0;
    }

    return {store: store, load: load, call: call};
  }

  function MathModule(stdlib, foreign, heap) {
    "use asm";
    var sqrt = stdlib.Math.sqrt;
    var imported = +foreign.imported;

    function mix(intValue, doubleValue) {
      intValue = intValue | 0;
      doubleValue = +doubleValue;
      return +(sqrt(+(intValue + 9 | 0)) + doubleValue + imported);
    }

    return {mix: mix};
  }

  function FallbackModule(stdlib, foreign, heap) {
    "use asm";
    var value = +foreign.value;

    function read() {
      return +value;
    }

    return {read: read};
  }

  const heap = keep('heap', new ArrayBuffer(64 * 1024));
  const heapView = keep('heapView', new Int32Array(heap));
  const imports = keep('imports', {
    callback(value) {
      return (value + 11) | 0;
    },
    imported: 1.25,
    get value() {
      return 23.5;
    },
  });

  const memoryInstance = keep('memoryInstance',
                              MemoryModule(globalThis, imports, heap));
  assertTrue(%IsAsmWasmCode(MemoryModule));
  memoryInstance.store(0, 42);
  memoryInstance.store(4, -17);
  assertEquals(42, memoryInstance.load(0));
  assertEquals(-17, memoryInstance.load(4));
  assertEquals(30, memoryInstance.call(4));

  const mathInstance = keep('mathInstance', MathModule(globalThis, imports, heap));
  assertTrue(%IsAsmWasmCode(MathModule));
  assertEqualsDelta(8.25, mathInstance.mix(7, 3.0), 0.001);

  const fallbackInstance = keep('fallbackInstance',
                                FallbackModule(globalThis, imports, heap));
  assertFalse(%IsAsmWasmCode(FallbackModule));
  assertEquals(23.5, fallbackInstance.read());

  keep('catalog', catalog);
  keep('heapView', heapView);
  keep('MemoryModule', MemoryModule);
  keep('MathModule', MathModule);
  keep('FallbackModule', FallbackModule);
})();
