// Heap-dump smoke sample for Error stack and RegExp related objects.
//
// This avoids native intrinsics so it can run under the generic heap-dump
// mjsunit runner. It keeps structured call-site arrays, Error subclasses,
// Error.cause/AggregateError, stack-property transitions, and RegExp match
// state alive for dump/translate smoke.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function captureStructuredStack(label) {
    const state = {label};
    const oldPrepareStackTrace = Error.prepareStackTrace;
    Error.prepareStackTrace = function(error, frames) {
      state.error = error;
      state.frames = frames;
      state.firstFrame = frames[0];
      state.frameMethods = frames.slice(0, 4).map((frame) => ({
        functionName: frame.getFunctionName(),
        methodName: frame.getMethodName(),
        fileName: frame.getFileName(),
        lineNumber: frame.getLineNumber(),
        columnNumber: frame.getColumnNumber(),
        isConstructor: frame.isConstructor(),
        isEval: frame.isEval(),
        isNative: frame.isNative(),
      }));
      return frames;
    };
    try {
      function outer() {
        return inner();
      }
      function inner() {
        const error = new Error('heap-dump structured stack ' + label);
        return error.stack;
      }
      state.stack = outer();
    } finally {
      Error.prepareStackTrace = oldPrepareStackTrace;
    }
    return state;
  }

  const structuredStacks = [
    captureStructuredStack('one'),
    captureStructuredStack('two'),
  ];

  const oldPrepareStackTrace = Error.prepareStackTrace;
  const stackMutationState = {};
  Error.prepareStackTrace = function(error, frames) {
    stackMutationState.error = error;
    stackMutationState.frames = frames;
    Object.defineProperty(error, 'stack', {
      configurable: true,
      enumerable: false,
      get() {
        return {customStackGetter: true, frames};
      },
    });
    return {formattedStack: true, frames};
  };
  const stackMutationError = new Error('heap-dump stack mutation');
  const formattedStack = stackMutationError.stack;
  const getterStack = stackMutationError.stack;
  Error.prepareStackTrace = oldPrepareStackTrace;

  const errorWithDictionaryElements = new Error('heap-dump dictionary stack');
  errorWithDictionaryElements[4096] = {farElement: true};
  errorWithDictionaryElements.extra = {extra: true};
  const dictionaryStack = errorWithDictionaryElements.stack;

  class HeapDumpCustomError extends Error {
    constructor(message, options) {
      super(message, options);
      this.customPayload = {custom: true};
    }
  }

  const cause = new TypeError('heap-dump cause');
  const errorObjects = {
    error: new Error('plain error', {cause}),
    evalError: new EvalError('eval error', {cause}),
    rangeError: new RangeError('range error', {cause}),
    referenceError: new ReferenceError('reference error', {cause}),
    syntaxError: new SyntaxError('syntax error', {cause}),
    typeError: new TypeError('type error', {cause}),
    uriError: new URIError('uri error', {cause}),
    customError: new HeapDumpCustomError('custom error', {cause}),
    aggregateError: new AggregateError(
        [new Error('aggregate-one'), new TypeError('aggregate-two')],
        'aggregate error',
        {cause}),
  };
  for (const error of Object.values(errorObjects)) {
    keep('stack:' + error.name + ':' + error.message, error.stack);
  }

  const regexpObjects = {
    global: /heap-(dump)-(\d+)/g,
    sticky: /dump/y,
    unicodeNamed: /(?<kind>heap)-(?<name>dump)/u,
    dotAll: /heap.*dump/s,
    multiline: /^heap$/m,
    ignoreCase: new RegExp('HEAP-DUMP', 'i'),
  };
  try {
    regexpObjects.indices = new RegExp('(?<word>heap)-(dump)', 'dg');
  } catch (e) {
    keep('regexpIndicesError', e);
  }

  const regexpSubject = 'heap-dump-1 heap-dump-22\nheap';
  const regexpResults = {
    globalExecs: [
      regexpObjects.global.exec(regexpSubject),
      regexpObjects.global.exec(regexpSubject),
    ],
    stickyFail: regexpObjects.sticky.exec(regexpSubject),
    named: regexpObjects.unicodeNamed.exec('heap-dump'),
    dotAll: regexpObjects.dotAll.exec('heap\n\n dump'),
    multiline: regexpObjects.multiline.exec(regexpSubject),
    ignoreCase: regexpObjects.ignoreCase.exec('heap-dump'),
    matchAll: Array.from(regexpSubject.matchAll(/heap-(dump)-(\d+)/g)),
    split: regexpSubject.split(/[-\s]+/),
    replace: regexpSubject.replace(/heap/g, 'HEAP'),
  };
  if (regexpObjects.indices) {
    regexpResults.indices = regexpObjects.indices.exec('heap-dump');
  }

  const regexpStringIterators = {
    matchAllGlobal: regexpSubject.matchAll(/heap/g),
    matchAllCapture: regexpSubject.matchAll(/heap-(dump)-(\d+)/g),
  };

  keep('structuredStacks', structuredStacks);
  keep('stackMutationState', stackMutationState);
  keep('stackMutationError', stackMutationError);
  keep('formattedStack', formattedStack);
  keep('getterStack', getterStack);
  keep('errorWithDictionaryElements', errorWithDictionaryElements);
  keep('dictionaryStack', dictionaryStack);
  keep('errorCause', cause);
  keep('errorObjects', errorObjects);
  keep('customErrorConstructor', HeapDumpCustomError);
  keep('regexpObjects', regexpObjects);
  keep('regexpResults', regexpResults);
  keep('regexpStringIterators', regexpStringIterators);

  assertTrue(Array.isArray(structuredStacks[0].frames));
  assertTrue(errorObjects.aggregateError instanceof AggregateError);
  assertTrue(regexpResults.matchAll.length >= 2);

  globalThis.heapDumpErrorsRegexpStackSample = catalog;
})();
