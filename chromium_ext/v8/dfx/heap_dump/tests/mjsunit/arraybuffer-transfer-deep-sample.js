// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Keep ArrayBuffer transfer result states alive for heap dump smoke. This
// follows V8's harmony/arraybuffer-transfer.js while retaining transferred
// buffers, detached sources, TypedArray/DataView views, resizable/fixed-length
// state records, and representative error objects.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, factory) {
    try {
      factory();
    } catch (error) {
      keep(name, error);
      return error;
    }
    return null;
  }

  function writeData(buffer) {
    const view = new Uint8Array(buffer);
    const pairs = [[1, 4], [4, 18], [17, 255], [62, 48]];
    for (const [index, value] of pairs) {
      if (index < view.length) view[index] = value;
    }
    return pairs;
  }

  function snapshot(method, label, byteLength, options, newLength) {
    const source = new ArrayBuffer(byteLength, options);
    const pairs = writeData(source);
    const sourceU8 = new Uint8Array(source);
    const sourceDataView = new DataView(source);
    const transferred = newLength === undefined ? source[method]() :
                                             source[method](newLength);
    const transferredU8 = new Uint8Array(transferred);
    const transferredDataView = new DataView(transferred);
    const state = {
      label,
      method,
      source,
      sourceU8,
      sourceDataView,
      transferred,
      transferredU8,
      transferredDataView,
      sourceDetached: source.detached,
      sourceByteLength: source.byteLength,
      sourceViewLength: sourceU8.length,
      transferredByteLength: transferred.byteLength,
      transferredResizable: transferred.resizable,
      transferredMaxByteLength: transferred.maxByteLength,
      sampleValues: pairs.map(([index]) => transferredU8[index] || 0),
    };
    keep(label, state);
    return state;
  }

  if (typeof ArrayBuffer.prototype.transfer !== 'function' ||
      typeof ArrayBuffer.prototype.transferToFixedLength !== 'function') {
    keep('unavailable', true);
    globalThis.heapDumpArrayBufferTransferDeepSample = catalog;
    return;
  }

  const transferStates = keep('transferStates', [
    snapshot('transfer', 'transferSameFixed', 128, undefined, undefined),
    snapshot('transfer', 'transferGrowFixed', 64, undefined, 192),
    snapshot('transfer', 'transferShrinkFixed', 128, undefined, 32),
    snapshot('transfer', 'transferSameResizable', 128, {maxByteLength: 256},
             undefined),
    snapshot('transfer', 'transferShrinkResizable', 128, {maxByteLength: 256},
             64),
  ]);

  const fixedLengthStates = keep('fixedLengthStates', [
    snapshot('transferToFixedLength', 'fixedSameFixed', 128, undefined,
             undefined),
    snapshot('transferToFixedLength', 'fixedGrowFixed', 64, undefined, 192),
    snapshot('transferToFixedLength', 'fixedShrinkResizable', 128,
             {maxByteLength: 256}, 64),
    snapshot('transferToFixedLength', 'fixedGrowResizable', 64,
             {maxByteLength: 256}, 192),
  ]);

  keepError('transferBeyondMaxError',
            () => new ArrayBuffer(64, {maxByteLength: 96}).transfer(128));
  keepError('transferNegativeLengthError',
            () => new ArrayBuffer(64).transfer(-1));
  keepError('fixedHugeLengthError',
            () => new ArrayBuffer(64).transferToFixedLength(
                Number.MAX_SAFE_INTEGER + 1));
  keepError('transferSharedBufferError',
            () => ArrayBuffer.prototype.transfer.call(
                new SharedArrayBuffer(64)));
  keepError('fixedSharedBufferError',
            () => ArrayBuffer.prototype.transferToFixedLength.call(
                new SharedArrayBuffer(64)));

  const parameterConversion = keep('parameterConversion', {});
  {
    const source = new ArrayBuffer(64);
    const lengthObject = {
      valueOf() {
        %ArrayBufferDetach(source);
        return 64;
      },
    };
    parameterConversion.detachedSource = source;
    parameterConversion.detachError =
        keepError('transferDetachDuringLengthError',
                  () => source.transfer(lengthObject));
  }
  {
    const source = new ArrayBuffer(64, {maxByteLength: 256});
    const lengthObject = {
      valueOf() {
        source.resize(32);
        return 96;
      },
    };
    const transferred = source.transferToFixedLength(lengthObject);
    parameterConversion.shrinkDuringLength = {
      source,
      transferred,
      sourceDetached: source.detached,
      transferredByteLength: transferred.byteLength,
      transferredResizable: transferred.resizable,
    };
  }

  const emptyChainSource = keep('emptyChainSource', new ArrayBuffer());
  const emptyChainFirst = keep('emptyChainFirst', emptyChainSource.transfer());
  const emptyChainSecond = keep('emptyChainSecond',
                                emptyChainFirst.transferToFixedLength(32));

  assertTrue(transferStates[0].sourceDetached);
  assertEquals(0, transferStates[0].sourceByteLength);
  assertEquals(128, transferStates[0].transferredByteLength);
  assertEquals(192, transferStates[1].transferredByteLength);
  assertEquals(32, transferStates[2].transferredByteLength);
  assertTrue(transferStates[3].transferredResizable);
  assertFalse(fixedLengthStates[2].transferredResizable);
  assertEquals(192, fixedLengthStates[3].transferredByteLength);
  assertTrue(catalog.transferBeyondMaxError instanceof RangeError);
  assertTrue(catalog.transferNegativeLengthError instanceof RangeError);
  assertTrue(catalog.fixedHugeLengthError instanceof RangeError);
  assertTrue(catalog.transferSharedBufferError instanceof TypeError);
  assertTrue(catalog.fixedSharedBufferError instanceof TypeError);
  assertTrue(catalog.transferDetachDuringLengthError instanceof TypeError);
  assertTrue(parameterConversion.shrinkDuringLength.sourceDetached);
  assertEquals(32, emptyChainSecond.byteLength);

  globalThis.heapDumpArrayBufferTransferDeepSample = catalog;
})();
