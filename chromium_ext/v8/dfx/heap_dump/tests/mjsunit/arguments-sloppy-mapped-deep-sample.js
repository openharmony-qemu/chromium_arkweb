// Heap-dump shape sample for sloppy arguments objects and
// SloppyArgumentsElements fast/slow backing stores.

(function() {
  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function makeFastMappedArguments(first, second, third) {
    arguments.extra = {kind: 'fast-mapped-extra'};
    arguments[0].marker = 'first-mutated-through-arguments';
    second.marker = 'second-mutated-through-parameter';
    arguments[2] = {kind: 'third-overwritten-through-arguments'};
    return arguments;
  }

  function makeDeletedMappedArguments(first, second, third) {
    delete arguments[0];
    arguments[0] = {kind: 'first-unmapped-after-delete'};
    second.marker = 'second-still-mapped';
    arguments[3] = {kind: 'new-unmapped-index'};
    return arguments;
  }

  function makeSlowMappedArguments(first, second, third) {
    Object.defineProperties(arguments, {
      5: {
        get() {
          return {kind: 'accessor-index-five', first};
        },
        enumerable: true,
        configurable: true,
      },
      named: {
        value: {kind: 'named-arguments-property'},
        enumerable: true,
        configurable: true,
      },
    });
    arguments[10000] = {kind: 'far-dictionary-index'};
    delete arguments[1];
    arguments[1] = {kind: 'second-unmapped-after-delete'};
    return arguments;
  }

  function makeCalleeArguments(first) {
    arguments.callee.extra = {kind: 'callee-function-extra'};
    return arguments;
  }

  function makeApplyArguments(first, second) {
    return Array.prototype.slice.call(arguments).concat([{kind: 'slice-tail'}]);
  }

  const first = keep('first', {marker: 'first'});
  const second = keep('second', {marker: 'second'});
  const third = keep('third', {marker: 'third'});
  const fastMapped = keep(
      'fastMapped', makeFastMappedArguments(first, second, third));

  const deletedMapped = keep(
      'deletedMapped',
      makeDeletedMappedArguments(
          {marker: 'delete-first'}, {marker: 'delete-second'},
          {marker: 'delete-third'}));

  const slowMapped = keep(
      'slowMapped',
      makeSlowMappedArguments(
          {marker: 'slow-first'}, {marker: 'slow-second'},
          {marker: 'slow-third'}));

  const calleeArguments =
      keep('calleeArguments', makeCalleeArguments({marker: 'callee-first'}));
  const slicedArguments =
      keep('slicedArguments',
           makeApplyArguments({marker: 'slice-first'}, {marker: 'slice-second'}));

  const iteratedFastMapped = keep(
      'iteratedFastMapped', Array.from(fastMapped, (value) => value));
  const ownKeysSlowMapped = keep('ownKeysSlowMapped', Reflect.ownKeys(slowMapped));
  const valuesDeletedMapped =
      keep('valuesDeletedMapped', Object.values(deletedMapped));

  assertEquals('first-mutated-through-arguments', first.marker);
  assertEquals('second-mutated-through-parameter', fastMapped[1].marker);
  assertEquals('third-overwritten-through-arguments', fastMapped[2].kind);
  assertEquals('first-unmapped-after-delete', deletedMapped[0].kind);
  assertEquals('second-still-mapped', deletedMapped[1].marker);
  assertEquals('new-unmapped-index', deletedMapped[3].kind);
  assertEquals('second-unmapped-after-delete', slowMapped[1].kind);
  assertEquals('accessor-index-five', slowMapped[5].kind);
  assertEquals('far-dictionary-index', slowMapped[10000].kind);
  assertEquals('callee-function-extra', calleeArguments.callee.extra.kind);
  assertEquals('slice-tail', slicedArguments[2].kind);
  assertTrue(ownKeysSlowMapped.indexOf('10000') >= 0);
  assertTrue(valuesDeletedMapped.length >= 4);

  globalThis.__heapDumpArgumentsSloppyMappedDeepKeepalive = catalog;
})();
