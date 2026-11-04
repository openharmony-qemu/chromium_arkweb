// Flags: --allow-natives-syntax

// Heap-dump sample corpus:
// - accessor/dictionary/transition-heavy ordinary objects
// - array, string, regexp, map, and set iterators
// - iterator helpers when exposed by the current build
// - promises, async generators, weak refs, finalization registries
// - disposable stacks and Temporal objects when exposed by the current build

(function() {
  function check(condition, message) {
    if (!condition) throw new Error(message);
  }

  const accessorTarget = { backing: { value: 7 } };
  Object.defineProperty(accessorTarget, 'computed', {
    configurable: true,
    enumerable: true,
    get() {
      return this.backing.value;
    },
    set(value) {
      this.backing.value = value;
    },
  });
  accessorTarget.computed = 11;

  const dictionaryObject = {};
  for (let i = 0; i < 96; ++i) dictionaryObject['dict_' + i] = { i };
  for (let i = 0; i < 48; i += 2) delete dictionaryObject['dict_' + i];

  const sparseArray = [];
  sparseArray[0] = { zero: true };
  sparseArray[128] = { far: true };
  sparseArray[4096] = { dictionary: true };

  const regexp = /(?<kind>heap)-(?<name>dump)/giu;
  const regexpMatches = Array.from('heap-dump heap-dump'.matchAll(regexp));
  const regexpStringIterator = 'heap-dump heap-dump'.matchAll(/heap/g);

  const map = new Map([
    ['alpha', { a: 1 }],
    ['beta', { b: 2 }],
  ]);
  const set = new Set([{ s: 1 }, { s: 2 }]);
  const array = [{ item: 1 }, { item: 2 }, { item: 3 }];

  const iteratorHelpers = {};
  try {
    if (typeof Iterator === 'function' && typeof Iterator.from === 'function') {
      const base = Iterator.from([1, 2, 3, 4]);
      iteratorHelpers.map = base.map((x) => x + 1);
      iteratorHelpers.filter = Iterator.from([1, 2, 3, 4]).filter((x) => x > 1);
      iteratorHelpers.take = Iterator.from([1, 2, 3, 4]).take(2);
      iteratorHelpers.drop = Iterator.from([1, 2, 3, 4]).drop(1);
      iteratorHelpers.flatMap =
          Iterator.from([1, 2]).flatMap((x) => [x, x + 10]);
    }
  } catch (e) {
    iteratorHelpers.error = e.name;
  }

  async function* asyncGenerator() {
    yield { async: 1 };
    yield { async: 2 };
  }

  let disposableStack = null;
  try {
    if (typeof DisposableStack === 'function') {
      disposableStack = new DisposableStack();
      disposableStack.defer(() => 0);
    }
  } catch (e) {
    disposableStack = { unavailable: e.name };
  }

  let asyncDisposableStack = null;
  try {
    if (typeof AsyncDisposableStack === 'function') {
      asyncDisposableStack = new AsyncDisposableStack();
      asyncDisposableStack.defer(async () => 0);
    }
  } catch (e) {
    asyncDisposableStack = { unavailable: e.name };
  }

  const weakTarget = { weak: true };
  const unregisterToken = { token: true };
  const finalizationRegistry =
      typeof FinalizationRegistry === 'function'
      ? new FinalizationRegistry((held) => held)
      : null;
  if (finalizationRegistry !== null) {
    finalizationRegistry.register(weakTarget, { held: true }, unregisterToken);
  }

  const temporalObjects = {};
  try {
    if (typeof Temporal === 'object') {
      temporalObjects.duration = new Temporal.Duration(1, 2, 3, 4, 5, 6, 7);
      temporalObjects.instant = Temporal.Instant.fromEpochMilliseconds(0);
      temporalObjects.plainDate = new Temporal.PlainDate(2026, 5, 25);
      temporalObjects.plainDateTime =
          new Temporal.PlainDateTime(2026, 5, 25, 12, 34, 56);
      temporalObjects.plainMonthDay = new Temporal.PlainMonthDay(5, 25);
      temporalObjects.plainTime = new Temporal.PlainTime(12, 34, 56);
      temporalObjects.plainYearMonth = new Temporal.PlainYearMonth(2026, 5);
      temporalObjects.zonedDateTime =
          new Temporal.ZonedDateTime(0n, Temporal.TimeZone.from('UTC'));
    }
  } catch (e) {
    temporalObjects.error = e.name;
  }

  globalThis.heapDumpLanguageBuiltinsDeepSample = {
    accessorTarget,
    dictionaryObject,
    sparseArray,
    regexp,
    regexpMatches,
    regexpStringIterator,
    mapKeys: map.keys(),
    mapValues: map.values(),
    mapEntries: map.entries(),
    setKeys: set.keys(),
    setValues: set.values(),
    setEntries: set.entries(),
    arrayKeys: array.keys(),
    arrayValues: array.values(),
    arrayEntries: array.entries(),
    stringIterator: 'heap-dump-language'[Symbol.iterator](),
    iteratorHelpers,
    pendingPromise: new Promise(() => {}),
    resolvedPromise: Promise.resolve({ resolved: true }),
    asyncGeneratorObject: asyncGenerator(),
    weakRef: typeof WeakRef === 'function' ? new WeakRef(weakTarget) : null,
    finalizationRegistry,
    unregisterToken,
    disposableStack,
    asyncDisposableStack,
    temporalObjects,
  };

  check(accessorTarget.computed === 11, 'accessor check failed');
  check(regexpMatches.length === 2, 'regexp matchAll check failed');
  check(map.size === 2 && set.size === 2, 'collection check failed');
})();
