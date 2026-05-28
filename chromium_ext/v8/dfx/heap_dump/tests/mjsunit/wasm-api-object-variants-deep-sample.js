// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep WebAssembly JS API object variants alive for heap dump smoke: memory,
// table, global, tag, exception, module, instance, and exported wrappers.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  const jsGlobals = keep('jsGlobals', {
    i32: new WebAssembly.Global({value: 'i32', mutable: true}, 11),
    i64: new WebAssembly.Global({value: 'i64', mutable: true}, 12n),
    f64: new WebAssembly.Global({value: 'f64', mutable: true}, 13.5),
    externref: new WebAssembly.Global(
        {value: 'externref', mutable: true}, {kind: 'extern-global'}),
  });
  jsGlobals.i32.value = 21;
  jsGlobals.i64.value = 22n;
  jsGlobals.f64.value = 23.5;
  jsGlobals.externref.value = {kind: 'extern-global-updated'};

  const jsMemories = keep('jsMemories', {
    fixed: new WebAssembly.Memory({initial: 1, maximum: 2}),
    shared: new WebAssembly.Memory({initial: 1, maximum: 2, shared: true}),
  });
  const fixedBytes = keep('fixedBytes', new Uint8Array(jsMemories.fixed.buffer));
  const sharedBytes =
      keep('sharedBytes', new Uint8Array(jsMemories.shared.buffer));
  fixedBytes[0] = 17;
  sharedBytes[0] = 19;
  keep('fixedByteBeforeGrow', fixedBytes[0]);
  keep('fixedGrowOldPages', jsMemories.fixed.grow(1));
  keep('fixedBytesAfterGrow', new Uint8Array(jsMemories.fixed.buffer));

  const jsTables = keep('jsTables', {
    externref: new WebAssembly.Table(
        {element: 'externref', initial: 2, maximum: 6}),
    funcref: new WebAssembly.Table(
        {element: 'anyfunc', initial: 2, maximum: 6}),
  });
  jsTables.externref.set(0, {kind: 'extern-table-zero'});
  jsTables.externref.set(1, {kind: 'extern-table-one'});
  keep('externTableGrowOldSize',
       jsTables.externref.grow(1, {kind: 'extern-table-grown'}));

  const jsTags = keep('jsTags', {});
  if (typeof WebAssembly.Tag === 'function' &&
      typeof WebAssembly.Exception === 'function') {
    jsTags.i32 = new WebAssembly.Tag({parameters: ['i32']});
    jsTags.externref = new WebAssembly.Tag({parameters: ['externref']});
    jsTags.mixed = new WebAssembly.Tag({
      parameters: ['i32', 'i64', 'f64', 'externref'],
    });
    keep('jsExceptions', {
      i32: new WebAssembly.Exception(jsTags.i32, [31], {traceStack: true}),
      externref: new WebAssembly.Exception(
          jsTags.externref, [{kind: 'externref-exception'}]),
      mixed: new WebAssembly.Exception(
          jsTags.mixed, [32, 33n, 34.5, {kind: 'mixed-exception'}]),
    });
  }

  const builder = new WasmModuleBuilder();
  const importedGlobal =
      builder.addImportedGlobal('m', 'importedGlobal', kWasmI32, true);
  builder.addMemory(1, 3, false);
  builder.exportMemoryAs('moduleMemory');
  const moduleTable = builder.addTable(kWasmExternRef, 3, 6);
  moduleTable.exportAs('moduleExternTable');
  const moduleI32Global =
      builder.addGlobal(kWasmI32, true, false, wasmI32Const(41));
  moduleI32Global.exportAs('moduleI32Global');
  const moduleTag = builder.addTag(kSig_v_i);
  builder.addExportOfKind('moduleTag', kExternalTag, moduleTag);
  builder.addFunction('readImportedGlobal', kSig_i_v)
      .addBody([kExprGlobalGet, importedGlobal])
      .exportFunc();
  builder.addFunction('writeImportedGlobal', kSig_v_i)
      .addBody([kExprLocalGet, 0, kExprGlobalSet, importedGlobal])
      .exportFunc();
  builder.addFunction('echoExternRef',
                      makeSig([kWasmExternRef], [kWasmExternRef]))
      .addBody([kExprLocalGet, 0])
      .exportFunc();

  const imports = keep('imports', {
    m: {
      importedGlobal: jsGlobals.i32,
    },
  });
  const module = keep('module', builder.toModule());
  const instance = keep('instance', new WebAssembly.Instance(module, imports));
  const exports = keep('exports', instance.exports);
  exports.moduleExternTable.set(0, {kind: 'module-table-zero'});
  keep('moduleTableGrowOldSize',
       exports.moduleExternTable.grow(1, {kind: 'module-table-grown'}));
  const moduleMemoryBytes =
      keep('moduleMemoryBytes', new Uint8Array(exports.moduleMemory.buffer));
  moduleMemoryBytes[0] = 23;
  keep('moduleMemoryByteBeforeGrow', moduleMemoryBytes[0]);
  keep('moduleMemoryGrowOldPages', exports.moduleMemory.grow(1));
  keep('moduleMemoryBytesAfterGrow',
       new Uint8Array(exports.moduleMemory.buffer));

  let moduleException = null;
  if (typeof WebAssembly.Exception === 'function') {
    moduleException = keep(
        'moduleException',
        new WebAssembly.Exception(exports.moduleTag, [45]));
  }

  assertEquals(21, jsGlobals.i32.value);
  assertEquals(22n, jsGlobals.i64.value);
  assertEquals(23.5, jsGlobals.f64.value);
  assertEquals('extern-global-updated', jsGlobals.externref.value.kind);
  assertEquals(17, catalog.fixedByteBeforeGrow);
  assertEquals(19, sharedBytes[0]);
  assertEquals(1, catalog.fixedGrowOldPages);
  assertEquals('extern-table-zero', jsTables.externref.get(0).kind);
  assertEquals(2, catalog.externTableGrowOldSize);
  assertEquals(21, exports.readImportedGlobal());
  exports.writeImportedGlobal(55);
  assertEquals(55, jsGlobals.i32.value);
  assertEquals('module-table-zero', exports.moduleExternTable.get(0).kind);
  assertEquals(3, catalog.moduleTableGrowOldSize);
  assertEquals(23, catalog.moduleMemoryByteBeforeGrow);
  assertSame(catalog.exports.echoExternRef({kind: 'echo'}).kind, 'echo');
  if (moduleException !== null) {
    assertTrue(moduleException.is(exports.moduleTag));
    assertEquals(45, moduleException.getArg(exports.moduleTag, 0));
  }

  globalThis.heapDumpWasmApiObjectVariantsDeepSample = catalog;
})();
