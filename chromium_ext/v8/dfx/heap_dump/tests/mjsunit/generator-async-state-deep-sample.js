// Heap-dump shape sample for generator, async function, and async generator
// suspended/queued states.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  let releaseAsyncFunction;
  const asyncFunctionBlocker = keep(
      'asyncFunctionBlocker',
      new Promise((resolve) => {
        releaseAsyncFunction = resolve;
      }));
  keep('releaseAsyncFunction', releaseAsyncFunction);

  function* nestedGenerator(seed) {
    const localObject = {kind: 'generator-local', seed};
    try {
      yield {kind: 'first-yield', localObject};
      yield* [
        {kind: 'delegated-yield', seed: seed + 1},
        {kind: 'delegated-yield', seed: seed + 2},
      ];
      return {kind: 'generator-return', localObject};
    } finally {
      localObject.finallyVisited = true;
    }
  }

  function* catchGenerator(seed) {
    try {
      yield {kind: 'catch-generator-yield', seed};
      throw new Error('generator-inner-error');
    } catch (error) {
      yield {kind: 'catch-generator-caught', error};
    }
  }

  async function pendingAsyncFunction(seed) {
    const asyncLocal = {kind: 'async-function-local', seed};
    await asyncFunctionBlocker;
    return {kind: 'async-function-result', asyncLocal};
  }

  async function* queuedAsyncGenerator(seed) {
    const asyncGeneratorLocal = {kind: 'async-generator-local', seed};
    yield {kind: 'async-generator-first', asyncGeneratorLocal};
    await new Promise(() => {});
    yield {kind: 'async-generator-unreachable'};
  }

  const suspendedGenerator = keep('suspendedGenerator', nestedGenerator(10));
  const firstGeneratorResult =
      keep('firstGeneratorResult', suspendedGenerator.next());
  const delegatedGenerator = keep('delegatedGenerator', nestedGenerator(20));
  keep('delegatedFirstResult', delegatedGenerator.next());
  keep('delegatedSecondResult', delegatedGenerator.next());
  const closedGenerator = keep('closedGenerator', nestedGenerator(30));
  keep('closedFirstResult', closedGenerator.next());
  keep('closedReturnResult', closedGenerator.return({kind: 'manual-return'}));

  const generatorWithCatch = keep('generatorWithCatch', catchGenerator(40));
  keep('catchFirstResult', generatorWithCatch.next());
  keep('catchSecondResult', generatorWithCatch.next());

  const pendingAsyncPromise =
      keep('pendingAsyncPromise', pendingAsyncFunction(50));

  const asyncGenerator = keep('asyncGenerator', queuedAsyncGenerator(60));
  const asyncGeneratorFirstRequest =
      keep('asyncGeneratorFirstRequest', asyncGenerator.next());
  const asyncGeneratorSecondRequest =
      keep('asyncGeneratorSecondRequest', asyncGenerator.next());
  const asyncGeneratorReturnRequest =
      keep('asyncGeneratorReturnRequest',
           asyncGenerator.return({kind: 'async-generator-manual-return'}));

  const customAsyncIterable = keep('customAsyncIterable', {
    state: {index: 0},
    [Symbol.asyncIterator]() {
      return this;
    },
    next() {
      return Promise.resolve({
        value: {kind: 'custom-async-iterator-value', index: this.state.index++},
        done: false,
      });
    },
    return(value) {
      this.state.returned = value;
      return Promise.resolve({value, done: true});
    },
  });
  const customAsyncIterator =
      keep('customAsyncIterator', customAsyncIterable[Symbol.asyncIterator]());
  keep('customAsyncIteratorNext', customAsyncIterator.next());
  keep('customAsyncIteratorReturn',
       customAsyncIterator.return({kind: 'custom-return'}));

  assertEquals('first-yield', firstGeneratorResult.value.kind);
  assertEquals('delegated-yield', catalog.delegatedSecondResult.value.kind);
  assertTrue(catalog.closedReturnResult.done);
  assertEquals('catch-generator-caught', catalog.catchSecondResult.value.kind);
  assertTrue(pendingAsyncPromise instanceof Promise);
  assertTrue(asyncGeneratorFirstRequest instanceof Promise);
  assertTrue(asyncGeneratorSecondRequest instanceof Promise);
  assertTrue(asyncGeneratorReturnRequest instanceof Promise);

  globalThis.__heapDumpGeneratorAsyncStateKeepalive = catalog;
})();
