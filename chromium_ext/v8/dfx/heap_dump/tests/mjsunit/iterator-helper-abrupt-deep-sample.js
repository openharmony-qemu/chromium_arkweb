// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep iterator-helper abrupt-completion and result shapes alive for heap dump
// smoke. The sample is feature-gated so it stays stable across V8 builds where
// individual iterator helper methods may be absent.

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

  function makeIterator(label, limit, options = {}) {
    const log = [];
    const iterator = {
      index: 0,
      log,
      next() {
        log.push(['next', this.index]);
        if (options.throwNextAt === this.index) {
          throw new Error(label + '-next-' + this.index);
        }
        if (this.index >= limit) return {done: true};
        return {
          value: {label, index: this.index++},
          done: false,
        };
      },
      return() {
        log.push(['return', this.index]);
        if (options.throwReturn) {
          throw new RangeError(label + '-return');
        }
        if (options.nonObjectReturn) return 17;
        return {done: true};
      },
      [Symbol.iterator]() {
        return this;
      },
    };
    if (typeof Iterator === 'function' && Iterator.prototype) {
      Object.setPrototypeOf(iterator, Iterator.prototype);
    }
    return iterator;
  }

  const ordinaryResults = keep('ordinaryResults', {});
  const abruptResults = keep('abruptResults', {});
  const helperIterators = keep('helperIterators', {});

  if (typeof Iterator === 'function' && typeof Iterator.from === 'function') {
    const helperPrototype = Iterator.prototype;

    if (typeof helperPrototype.map === 'function') {
      helperIterators.mapped = Iterator.from([1, 2, 3]).map((value, index) => ({
        value,
        index,
        mapped: value + index,
      }));
      ordinaryResults.mappedFirst = helperIterators.mapped.next();

      const throwingMapSource = makeIterator('throwing-map-source', 5);
      keep('throwingMapSource', throwingMapSource);
      keepError('throwingMapCallbackError', () => {
        Iterator.from(throwingMapSource).map((entry) => {
          if (entry.index === 1) throw new TypeError('heap-dump-map-callback');
          return entry.index;
        }).toArray();
      });
      abruptResults.throwingMapLog = throwingMapSource.log;
    }

    if (typeof helperPrototype.filter === 'function') {
      helperIterators.filtered = Iterator.from([1, 2, 3, 4, 5]).filter(
          (value) => value % 2 === 1);
      ordinaryResults.filteredFirst = helperIterators.filtered.next();

      const throwingFilterSource = makeIterator('throwing-filter-source', 4);
      keep('throwingFilterSource', throwingFilterSource);
      keepError('throwingFilterCallbackError', () => {
        Iterator.from(throwingFilterSource).filter((entry) => {
          if (entry.index === 2) {
            throw new SyntaxError('heap-dump-filter-callback');
          }
          return true;
        }).toArray();
      });
      abruptResults.throwingFilterLog = throwingFilterSource.log;
    }

    if (typeof helperPrototype.take === 'function') {
      const returnSource = makeIterator('take-return-source', 6);
      keep('takeReturnSource', returnSource);
      helperIterators.takeTwo = Iterator.from(returnSource).take(2);
      ordinaryResults.takeArray = helperIterators.takeTwo.toArray();
      abruptResults.takeReturnLog = returnSource.log;

      const throwingReturnSource =
          makeIterator('take-throw-return-source', 6, {throwReturn: true});
      keep('takeThrowReturnSource', throwingReturnSource);
      keepError('takeThrowReturnError', () => {
        Iterator.from(throwingReturnSource).take(1).toArray();
      });
      abruptResults.takeThrowReturnLog = throwingReturnSource.log;
    }

    if (typeof helperPrototype.drop === 'function') {
      helperIterators.dropTwo = Iterator.from([1, 2, 3, 4]).drop(2);
      ordinaryResults.dropFirst = helperIterators.dropTwo.next();
      keepError('dropNegativeError', () => Iterator.from([1]).drop(-1));
    }

    if (typeof helperPrototype.flatMap === 'function') {
      helperIterators.flatMapped = Iterator.from([1, 2]).flatMap((value) => [
        {kind: 'flat-a', value},
        {kind: 'flat-b', value: value + 10},
      ]);
      ordinaryResults.flatMappedFirst = helperIterators.flatMapped.next();
      keepError('flatMapNonIterableError', () => {
        Iterator.from([1]).flatMap(() => 7).toArray();
      });
    }

    if (typeof helperPrototype.reduce === 'function') {
      ordinaryResults.reduced = Iterator.from([1, 2, 3, 4]).reduce(
          (sum, value) => sum + value, 0);
      keepError('reduceEmptyError', () => Iterator.from([]).reduce(
          (sum, value) => sum + value));
    }

    if (typeof helperPrototype.some === 'function') {
      ordinaryResults.some = Iterator.from([1, 2, 3]).some(
          (value) => value === 2);
    }

    if (typeof helperPrototype.every === 'function') {
      ordinaryResults.every = Iterator.from([2, 4, 6]).every(
          (value) => value % 2 === 0);
    }

    if (typeof helperPrototype.find === 'function') {
      ordinaryResults.find = Iterator.from([
        {name: 'first'},
        {name: 'target'},
      ]).find((value) => value.name === 'target');
    }

    if (typeof helperPrototype.toArray === 'function') {
      ordinaryResults.toArray = Iterator.from([
        {kind: 'array-value-0'},
        {kind: 'array-value-1'},
      ]).toArray();

      const throwingNextSource =
          makeIterator('to-array-throw-next-source', 5, {throwNextAt: 2});
      keep('toArrayThrowNextSource', throwingNextSource);
      keepError('toArrayThrowNextError', () => {
        Iterator.from(throwingNextSource).toArray();
      });
      abruptResults.toArrayThrowNextLog = throwingNextSource.log;
    }

    keepError('badReceiverMapError', () => {
      Iterator.prototype.map.call({next: null}, (value) => value).next();
    });
    keepError('fromBadIteratorError', () => {
      Iterator.from({[Symbol.iterator]: () => ({next: 1})}).next();
    });
  }

  keep('summary', {
    helperNames: Object.keys(helperIterators),
    ordinaryNames: Object.keys(ordinaryResults),
    abruptNames: Object.keys(abruptResults),
    errorNames: Object.keys(catalog).filter((name) => /Error$/.test(name)),
  });

  if (typeof Iterator === 'function' && typeof Iterator.from === 'function') {
    assertTrue(catalog.summary.ordinaryNames.length > 0);
    assertTrue(catalog.summary.errorNames.length > 0);
  }

  globalThis.heapDumpIteratorHelperAbruptDeepSample = catalog;
})();
