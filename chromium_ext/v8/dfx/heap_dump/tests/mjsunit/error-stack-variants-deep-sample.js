// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep Error stack-property variants alive for heap dump smoke. This follows
// V8's error-stack and stack-traces tests while avoiding recursion-heavy stack
// overflow cases.

(function () {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function keepError(name, callback) {
    try {
      callback();
    } catch (e) {
      keep(name, e);
      return e;
    }
    return null;
  }

  const originalPrepareStackTrace = Error.prepareStackTrace;
  const originalStackTraceLimit = Error.stackTraceLimit;
  const originalPrototypeStack =
      Object.getOwnPropertyDescriptor(Error.prototype, 'stack');

  function captureWithLimit(name, limit) {
    Error.stackTraceLimit = limit;
    function outer() {
      return middle();
    }
    function middle() {
      return inner();
    }
    function inner() {
      const error = new Error('heap-dump-stack-limit-' + name);
      const stack = error.stack;
      return {name, limit, error, stack};
    }
    return outer();
  }

  const limitCaptures = keep('limitCaptures', [
    captureWithLimit('zero', 0),
    captureWithLimit('one', 1),
    captureWithLimit('three', 3),
    captureWithLimit('nan', NaN),
    captureWithLimit('string', 'not-a-number'),
    captureWithLimit('infinity', Infinity),
  ]);

  const prepareVariants = keep('prepareVariants', []);

  Error.prepareStackTrace = function(error, frames) {
    return 42;
  };
  const numberStackError = keep(
      'numberStackError', new Error('heap-dump-number-stack'));
  prepareVariants.push({
    kind: 'number',
    error: numberStackError,
    stack: numberStackError.stack,
  });

  Error.prepareStackTrace = function(error, frames) {
    return {
      kind: 'object-stack',
      error,
      frames,
      first: frames[0],
    };
  };
  const objectStackError = keep(
      'objectStackError', new Error('heap-dump-object-stack'));
  prepareVariants.push({
    kind: 'object',
    error: objectStackError,
    stack: objectStackError.stack,
  });

  Error.prepareStackTrace = function(error, frames) {
    Object.defineProperty(error, 'stack', {
      configurable: true,
      enumerable: true,
      get() {
        return {kind: 'installed-stack-getter', frames};
      },
      set(value) {
        error.installedStackValue = value;
      },
    });
    return 'installed-stack-string';
  };
  const installedStackError = keep(
      'installedStackError', new Error('heap-dump-installed-stack'));
  const installedFirstStack = keep(
      'installedFirstStack', installedStackError.stack);
  const installedSecondStack = keep(
      'installedSecondStack', installedStackError.stack);
  installedStackError.stack = {kind: 'setter-value'};
  keep('installedSetterValue', installedStackError.installedStackValue);
  prepareVariants.push({
    kind: 'installed-accessor',
    error: installedStackError,
    first: installedFirstStack,
    second: installedSecondStack,
  });

  Error.prepareStackTrace = function(error, frames) {
    throw new RangeError('heap-dump-prepare-stack-throw');
  };
  keepError('prepareStackTraceThrowError', () => {
    const error = new Error('heap-dump-prepare-throw-trigger');
    return error.stack;
  });

  Error.prepareStackTrace = originalPrepareStackTrace;
  Error.stackTraceLimit = originalStackTraceLimit;

  const prototypeStackLog = keep('prototypeStackLog', []);
  Object.defineProperty(Error.prototype, 'stack', {
    configurable: true,
    get() {
      prototypeStackLog.push(['get', this.message]);
      return this.prototypeStackValue || 'prototype-stack';
    },
    set(value) {
      prototypeStackLog.push(['set', this.message, value]);
      this.prototypeStackValue = value;
    },
  });
  const prototypeStackObject = keep(
      'prototypeStackObject',
      Object.create(Error.prototype));
  prototypeStackObject.message = 'prototype-object';
  prototypeStackObject.stack = {kind: 'prototype-set'};
  keep('prototypeStackRead', prototypeStackObject.stack);
  if (originalPrototypeStack) {
    Object.defineProperty(Error.prototype, 'stack', originalPrototypeStack);
  } else {
    delete Error.prototype.stack;
  }

  function makeAggregateIterable(label, throwNextAt, throwReturn) {
    const log = [];
    return {
      log,
      [Symbol.iterator]() {
        let index = 0;
        return {
          next() {
            log.push(['next', index]);
            if (index === throwNextAt) {
              throw new SyntaxError(label + '-next-' + index);
            }
            if (index > 3) return {done: true};
            return {
              value: new Error(label + '-inner-' + index++),
              done: false,
            };
          },
          return() {
            log.push(['return', index]);
            if (throwReturn) {
              throw new TypeError(label + '-return');
            }
            return {done: true};
          },
        };
      },
    };
  }

  const aggregateIterable = keep(
      'aggregateIterable',
      makeAggregateIterable('aggregate-abrupt', 2, false));
  keepError('aggregateIterableNextError', () => {
    new AggregateError(aggregateIterable, 'heap-dump-aggregate-next');
  });
  keep('aggregateIterableLog', aggregateIterable.log);

  const aggregateCause = keep(
      'aggregateCause',
      new Error('heap-dump-aggregate-cause', {
        cause: new TypeError('heap-dump-nested-cause'),
      }));
  const nestedAggregate = keep(
      'nestedAggregate',
      new AggregateError([
        new Error('nested-one', {cause: aggregateCause}),
        new AggregateError([new RangeError('nested-range')],
                           'nested-aggregate'),
      ], 'heap-dump-nested-aggregate', {cause: aggregateCause}));
  keep('nestedAggregateStack', nestedAggregate.stack);

  keep('summaries', {
    limitNames: limitCaptures.map((capture) => capture.name),
    prepareKinds: prepareVariants.map((variant) => variant.kind),
    prototypeStackLog,
    aggregateErrors: [
      catalog.aggregateIterableNextError,
      nestedAggregate,
    ],
  });

  assertEquals(6, limitCaptures.length);
  assertEquals(42, prepareVariants[0].stack);
  assertEquals('object-stack', prepareVariants[1].stack.kind);
  assertEquals('installed-stack-getter', installedSecondStack.kind);
  assertEquals('setter-value', catalog.installedSetterValue.kind);
  assertTrue(catalog.prepareStackTraceThrowError instanceof RangeError);
  assertEquals('prototype-set', catalog.prototypeStackRead.kind);
  assertTrue(catalog.aggregateIterableNextError instanceof SyntaxError);
  assertTrue(nestedAggregate instanceof AggregateError);
  assertSame(aggregateCause, nestedAggregate.cause);

  globalThis.heapDumpErrorStackVariantsDeepSample = catalog;
})();
