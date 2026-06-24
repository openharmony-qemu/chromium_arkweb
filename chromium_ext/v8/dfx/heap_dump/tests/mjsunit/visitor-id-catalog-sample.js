// Heap-dump visitor-id coverage sample.

(function () {
  function outer(a, b, c) {
    var local = a + b + c;
    return function inner() { return local; };
  }

  function makeSloppyArguments() { return arguments; }
  async function asyncFn(x) { return await Promise.resolve(x + 1); }
  function* genFn() { yield 1; yield 2; }

  const weakKey = { key: true };
  const weakValue = { held: true };
  const proxyTarget = { a: 1 };
  const proxy = new Proxy(proxyTarget, {
    get(target, prop, receiver) {
      return Reflect.get(target, prop, receiver);
    }
  });

  const sliced = 'heap-dump-visitor-ids-sliced-string'.slice(5, 22);
  const cons = 'heap-dump-' + 'visitor-ids-cons-string';
  const maybeThin = ('thin-' + 'string').slice(0);

  const dict = {};
  for (let i = 0; i < 64; i++) dict['k' + i] = i;
  for (let i = 0; i < 16; i++) delete dict['k' + i];

  const weakMap = new WeakMap([[weakKey, weakValue]]);
  const weakSet = new WeakSet([weakKey]);
  const finalizationRegistry =
      typeof FinalizationRegistry === 'function'
          ? new FinalizationRegistry(() => 0)
          : null;
  const weakRef =
      typeof WeakRef === 'function' ? new WeakRef(weakValue) : null;
  if (finalizationRegistry) finalizationRegistry.register(weakValue, 1);

  const arrayBuffer = new ArrayBuffer(128);
  const dataView = new DataView(arrayBuffer);
  const typed = new Uint8Array(arrayBuffer);
  const shared =
      typeof SharedArrayBuffer === 'function' ? new SharedArrayBuffer(64) : null;

  const regexp = /visitor-id-(foo)?/gi;
  regexp.exec('visitor-id-foo');
  regexp.exec('visitor-id-bar');

  globalThis.__heap_dump_visitor_id_catalog = {
    proxy,
    proxyTarget,
    sliced,
    cons,
    maybeThin,
    dict,
    array: [1, 2, 3, { nested: true }],
    object: { deep: { ok: true } },
    weakMap,
    weakSet,
    weakRef,
    finalizationRegistry,
    arrayBuffer,
    dataView,
    typed,
    shared,
    moduleMap: new Map([[1, 'one'], [2, { deep: true }]]),
    moduleSet: new Set([1, 2, 3]),
    regexp,
    promise: Promise.resolve({ done: false }),
    closure: outer(1, 2, 3),
    gen: genFn(),
    asyncFn,
    args: makeSloppyArguments(1, 2, 3, 4),
    date: new Date(2026, 3, 3),
    bigint: 20260403n,
    symbol: Symbol('visitor-id-symbol'),
    evalClosure: (function () {
      let evalValue = 9;
      return function () { return eval('evalValue'); };
    })(),
    withClosure: (function () {
      var scopeObj = { p: 11 };
      with (scopeObj) {
        return function () { return p; };
      }
    })()
  };

  if (typeof Atomics.Mutex === 'function') {
    globalThis.__heap_dump_visitor_id_catalog.mutex = new Atomics.Mutex();
  }
  if (typeof Atomics.Condition === 'function') {
    globalThis.__heap_dump_visitor_id_catalog.condition =
        new Atomics.Condition();
  }

  if (typeof WebAssembly === 'object') {
    const bytes = new Uint8Array([
      0x00, 0x61, 0x73, 0x6d,
      0x01, 0x00, 0x00, 0x00,
      0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
      0x03, 0x02, 0x01, 0x00,
      0x07, 0x07, 0x01, 0x03, 0x6e, 0x6f, 0x70, 0x00, 0x00,
      0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b
    ]);
    const wasmModule = new WebAssembly.Module(bytes);
    const wasmInstance = new WebAssembly.Instance(wasmModule, {});
    globalThis.__heap_dump_visitor_id_catalog.wasm = {
      wasmModule,
      wasmInstance,
      memory: new WebAssembly.Memory({ initial: 1, maximum: 1 }),
      table: new WebAssembly.Table({ initial: 1, element: 'anyfunc' })
    };
  }
})();
