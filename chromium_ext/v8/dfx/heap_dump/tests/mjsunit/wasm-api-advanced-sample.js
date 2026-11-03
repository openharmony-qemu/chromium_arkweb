// Flags: --allow-natives-syntax

// Heap-dump sample corpus:
// - WebAssembly.Memory/Table/Global/Module/Instance
// - WebAssembly.Tag and Exception when exposed by the current build
// - WebAssembly.Function and JSPI Suspender when exposed by the current build
// - Wasm GC struct/array refs through wasm-module-builder

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  function check(condition, message) {
    if (!condition) throw new Error(message);
  }

  const imports = {
    env: {
      importedFunction(x) {
        return x + 1;
      },
      importedGlobal: new WebAssembly.Global({ value: 'i32', mutable: true }, 3),
    },
  };

  const builder = new WasmModuleBuilder();
  const importedFunction =
      builder.addImport('env', 'importedFunction', kSig_i_i);
  builder.addImportedGlobal('env', 'importedGlobal', kWasmI32, true);
  const table = builder.addTable(kWasmFuncRef, 2, 4);
  builder.addExportOfKind('table', kExternalTable, table.index);
  builder.addMemory(1, 2, false);
  builder.exportMemoryAs('memory');

  const structType = builder.addStruct([
    makeField(kWasmI32, true),
    makeField(wasmRefNullType(kWasmAnyRef), true),
  ]);
  const arrayType = builder.addArray(wasmRefNullType(structType), true);

  builder.addFunction('callImport', kSig_i_i)
      .addBody([kExprLocalGet, 0, kExprCallFunction, importedFunction])
      .exportFunc();
  builder.addFunction(
             'makeStruct',
             makeSig([kWasmI32], [wasmRefType(structType)]))
      .addBody([
        kExprLocalGet, 0,
        kExprRefNull, kNullRefCode,
        kGCPrefix, kExprStructNew, structType,
      ])
      .exportFunc();
  builder.addFunction('makeArray', makeSig([kWasmI32], [wasmRefType(arrayType)]))
      .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayNewDefault, arrayType])
      .exportFunc();

  const module = builder.toModule();
  const instance = new WebAssembly.Instance(module, imports);
  const structObject = instance.exports.makeStruct(42);
  const arrayObject = instance.exports.makeArray(3);

  const optional = {};
  try {
    if (typeof WebAssembly.Tag === 'function' &&
        typeof WebAssembly.Exception === 'function') {
      optional.tag = new WebAssembly.Tag({ parameters: ['i32'] });
      optional.exception = new WebAssembly.Exception(optional.tag, [7]);
    }
  } catch (e) {
    optional.exceptionError = e.name;
  }

  try {
    if (typeof WebAssembly.Function === 'function') {
      optional.functionWrapper = new WebAssembly.Function(
          { parameters: ['i32'], results: ['i32'] }, (x) => x + 2);
    }
  } catch (e) {
    optional.functionWrapperError = e.name;
  }

  try {
    if (typeof WebAssembly.Suspender === 'function') {
      optional.suspender = new WebAssembly.Suspender();
    }
  } catch (e) {
    optional.suspenderError = e.name;
  }

  globalThis.heapDumpWasmApiAdvancedSample = {
    module,
    instance,
    imports,
    memory: instance.exports.memory,
    memoryBuffer: instance.exports.memory.buffer,
    table: instance.exports.table,
    exportedFunction: instance.exports.callImport,
    structObject,
    arrayObject,
    optional,
  };

  check(instance.exports.callImport(4) === 5, 'import call check failed');
  check(instance.exports.table instanceof WebAssembly.Table,
        'table check failed');
})();
