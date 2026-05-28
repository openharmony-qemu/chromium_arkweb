// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep WebAssembly.Memory resizable/fixed buffer transitions alive for heap dump
// smoke. This follows V8's memory-toresizable-tofixedlength and
// grow-memory-detaching-resizable-buffer tests with retained buffers/views.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  'use strict';

  const kPageSize = 65536;
  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const growModuleBuilder = new WasmModuleBuilder();
  growModuleBuilder.addMemory(1, 4);
  growModuleBuilder.addFunction('growMemory', kSig_i_i)
      .addBody([kExprLocalGet, 0, kExprMemoryGrow, kMemoryZero])
      .exportFunc();
  growModuleBuilder.exportMemoryAs('memory');
  const growModule = keep('growModule', growModuleBuilder.toModule());
  const growInstance =
      keep('growInstance', new WebAssembly.Instance(growModule));

  (function UnsharedMemoryBufferTransitions() {
    const memory = keep(
        'unsharedMemory', new WebAssembly.Memory({initial: 1, maximum: 4}));
    const fixedBuffer = keep('unsharedFixedBuffer', memory.buffer);
    const fixedView = keep('unsharedFixedView', new Uint8Array(fixedBuffer));
    fixedView[0] = 11;

    const resizableBuffer =
        keep('unsharedResizableBuffer', memory.toResizableBuffer());
    const resizableView =
        keep('unsharedResizableView', new Uint8Array(resizableBuffer));
    resizableView[1] = 22;
    const growZeroBuffer = keep('unsharedGrowZeroBuffer', memory.buffer);
    const growOldPages = keep('unsharedGrowOldPages', memory.grow(1));
    const grownBuffer = keep('unsharedGrownBuffer', memory.buffer);
    const grownTrackingView =
        keep('unsharedGrownTrackingView', new Uint8Array(grownBuffer));
    grownTrackingView[kPageSize] = 33;
    grownBuffer.resize(3 * kPageSize);
    grownTrackingView[2 * kPageSize] = 44;

    const fixedLengthBuffer =
        keep('unsharedFixedLengthBuffer', memory.toFixedLengthBuffer());
    const fixedLengthView =
        keep('unsharedFixedLengthView', new Uint8Array(fixedLengthBuffer));
    fixedLengthView[3] = 55;

    assertTrue(fixedBuffer.detached);
    assertEquals(0, fixedView.length);
    assertTrue(resizableBuffer.detached);
    assertEquals(1, growOldPages);
    assertSame(resizableBuffer, growZeroBuffer);
    assertSame(resizableBuffer, grownBuffer);
    assertEquals(0, resizableView.length);
    assertFalse(fixedLengthBuffer.detached);
    assertFalse(fixedLengthBuffer.resizable);
    assertEquals(3 * kPageSize, fixedLengthBuffer.byteLength);
  })();

  (function WasmBytecodeGrowKeepsResizableBuffer() {
    const memory = keep('bytecodeMemory', growInstance.exports.memory);
    const fixedBuffer = keep('bytecodeFixedBuffer', memory.buffer);
    const resizableBuffer =
        keep('bytecodeResizableBuffer', memory.toResizableBuffer());
    const trackingView =
        keep('bytecodeTrackingView', new Uint8Array(resizableBuffer));
    trackingView[4] = 66;
    const growZeroPages = keep(
        'bytecodeGrowZeroPages', growInstance.exports.growMemory(0));
    const growOnePages = keep(
        'bytecodeGrowOnePages', growInstance.exports.growMemory(1));
    trackingView[kPageSize + 4] = 77;

    assertTrue(fixedBuffer.detached);
    assertSame(resizableBuffer, memory.buffer);
    assertEquals(1, growZeroPages);
    assertEquals(1, growOnePages);
    assertEquals(2 * kPageSize, memory.buffer.byteLength);
    assertEquals(77, trackingView[kPageSize + 4]);
  })();

  keep('summary', {
    unsharedMemory: catalog.unsharedMemory instanceof WebAssembly.Memory,
    bytecodeMemory: catalog.bytecodeMemory instanceof WebAssembly.Memory,
  });

  globalThis.heapDumpWasmMemoryResizableBufferDeepSample = catalog;
})();
