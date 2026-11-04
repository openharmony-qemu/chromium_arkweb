// Heap-dump sample corpus:
// - closures
// - compiled functions
// - regexp literals and constructed regexp
// - prototype chains and custom instances

(function() {
  function outer(seed) {
    const captured = { seed };
    return function inner(delta) {
      return captured.seed + delta;
    };
  }

  function HeapDumpCtor(name) {
    this.name = name;
  }

  HeapDumpCtor.prototype.describe = function describe() {
    return this.name;
  };

  const closure = outer(5);
  const instance = new HeapDumpCtor('catalog');

  globalThis.heapDumpCompilerSample = {
    closure,
    boundClosure: closure.bind(null, 9),
    regexpLiteral: /catalog-(\d+)/g,
    regexpObject: new RegExp('catalog', 'i'),
    instance,
    protoObject: Object.create(instance),
  };

  for (let i = 0; i < 16; i++) {
    closure(i);
  }

  assertEquals(14, globalThis.heapDumpCompilerSample.boundClosure());
  assertEquals('catalog', globalThis.heapDumpCompilerSample.instance.describe());
})();
