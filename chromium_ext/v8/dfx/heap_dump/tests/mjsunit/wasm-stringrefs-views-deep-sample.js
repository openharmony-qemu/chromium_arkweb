// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

// Keep wasm stringref and string-view objects alive for heap dump smoke. String
// views cannot be returned directly to JS, so this stores a StringViewIter in a
// Wasm GC struct that remains reachable from globalThis.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const stringSig = makeSig([kWasmStringRef], [kWasmStringRef]);
  const getStringSig = makeSig([], [kWasmStringRef]);
  const setStringSig = makeSig([kWasmStringRef], []);
  const lengthSig = makeSig([kWasmStringRef], [kWasmI32]);

  const builder = new WasmModuleBuilder();
  const stringGlobal = builder.addGlobal(kWasmStringRef, true, false);
  stringGlobal.exportAs('stringGlobal');
  const stringTable = builder.addTable(kWasmStringRef, 4, 8);
  stringTable.exportAs('stringTable');
  const stringViewStructType = builder.addStruct([
    makeField(kWasmStringRef, true),
    makeField(kWasmStringViewIter, true),
  ]);
  const stringViewStructRef = wasmRefType(stringViewStructType);
  const makeViewStructSig =
      makeSig([kWasmStringRef], [stringViewStructRef]);

  builder.addFunction('passthrough', stringSig)
      .addBody([kExprLocalGet, 0])
      .exportFunc();
  builder.addFunction('getStringGlobal', getStringSig)
      .addBody([kExprGlobalGet, stringGlobal.index])
      .exportFunc();
  builder.addFunction('setStringGlobal', setStringSig)
      .addBody([kExprLocalGet, 0, kExprGlobalSet, stringGlobal.index])
      .exportFunc();
  builder.addFunction('stringLength', lengthSig)
      .addBody([kExprLocalGet, 0, ...GCInstr(kExprStringMeasureWtf16)])
      .exportFunc();
  builder.addFunction('makeStringViewStruct', makeViewStructSig)
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 0,
        ...GCInstr(kExprStringAsIter),
        kGCPrefix, kExprStructNew, stringViewStructType,
      ])
      .exportFunc();

  const module = keep('module', builder.toModule());
  const instance = keep('instance', new WebAssembly.Instance(module));
  const exports = keep('exports', instance.exports);

  const strings = keep('strings', [
    'heap-dump-stringref',
    'heap-dump-\u0100-\u2603',
    'emoji-\u{1f680}-stringref',
    ''.padStart(80, 'x') + '-tail',
  ]);
  const passthroughResults = keep(
      'passthroughResults',
      strings.map((value) => exports.passthrough(value)));
  exports.setStringGlobal(strings[1]);
  keep('stringGlobalRead', exports.getStringGlobal());
  for (let i = 0; i < strings.length; ++i) {
    exports.stringTable.set(i, strings[i]);
  }
  keep('stringTableValues', [
    exports.stringTable.get(0),
    exports.stringTable.get(1),
    exports.stringTable.get(2),
    exports.stringTable.get(3),
  ]);
  keep('stringTableGrowOldSize', exports.stringTable.grow(1, 'grown-stringref'));
  const viewStructs = keep(
      'viewStructs',
      strings.map((value) => exports.makeStringViewStruct(value)));
  keep('lengths', strings.map((value) => exports.stringLength(value)));

  assertEquals(strings[0], passthroughResults[0]);
  assertEquals(strings[1], catalog.stringGlobalRead);
  assertEquals(strings[2], catalog.stringTableValues[2]);
  assertEquals(4, catalog.stringTableGrowOldSize);
  assertEquals(strings[0].length, catalog.lengths[0]);
  assertEquals(strings.length, viewStructs.length);

  globalThis.heapDumpWasmStringRefsViewsDeepSample = catalog;
})();
