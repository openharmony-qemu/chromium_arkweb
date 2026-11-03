// Heap-dump shape sample for JSBoundFunction target/this/bound_arguments edges.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function targetFunction(first, second, third, fourth) {
    return {
      thisValue: this,
      argumentsLength: arguments.length,
      first,
      second,
      third,
      fourth,
    };
  }

  const boundThis = keep('boundThis', {kind: 'bound-this'});
  const boundArgObject = keep('boundArgObject', {kind: 'bound-arg-object'});
  const boundArgArray = keep('boundArgArray', [{kind: 'bound-array-value'}]);
  const boundArgFunction = keep('boundArgFunction', function boundArgFunction() {
    return 'bound-arg-function-result';
  });

  const noArgBound = keep('noArgBound', targetFunction.bind(boundThis));
  const oneArgBound =
      keep('oneArgBound', targetFunction.bind(boundThis, boundArgObject));
  const manyArgsBound = keep(
      'manyArgsBound',
      targetFunction.bind(
          boundThis, boundArgObject, boundArgArray, boundArgFunction,
          {kind: 'inline-bound-arg'}));
  const nestedBound = keep(
      'nestedBound',
      targetFunction
          .bind(boundThis, {kind: 'first-layer-bound'})
          .bind({kind: 'ignored-this'}, {kind: 'second-layer-bound'})
          .bind({kind: 'also-ignored-this'}, {kind: 'third-layer-bound'}));

  function ConstructTarget(first, second) {
    this.constructed = true;
    this.first = first;
    this.second = second;
    this.extra = {kind: 'constructed-extra'};
  }
  ConstructTarget.prototype.method = function method() {
    return this.first.kind + ':' + this.second.kind;
  };

  const boundConstructorThis =
      keep('boundConstructorThis', {kind: 'ignored-constructor-this'});
  const boundConstructorArg =
      keep('boundConstructorArg', {kind: 'bound-constructor-arg'});
  const boundConstructor = keep(
      'boundConstructor',
      ConstructTarget.bind(boundConstructorThis, boundConstructorArg));
  const constructedInstance = keep(
      'constructedInstance',
      new boundConstructor({kind: 'new-call-arg'}));

  class ClassTarget {
    constructor(first, second) {
      this.first = first;
      this.second = second;
      this.classExtra = {kind: 'class-extra'};
    }

    read() {
      return this.first.kind + ':' + this.second.kind;
    }
  }
  const boundClass = keep(
      'boundClass',
      ClassTarget.bind({kind: 'ignored-class-this'}, {kind: 'bound-class-arg'}));
  const classInstance =
      keep('classInstance', new boundClass({kind: 'class-new-arg'}));

  const proxyTarget = keep('proxyTarget', function proxyTarget(first, second) {
    return {kind: 'proxy-target-result', first, second, thisValue: this};
  });
  const proxyLog = keep('proxyLog', []);
  const callableProxy = keep(
      'callableProxy',
      new Proxy(proxyTarget, {
        apply(target, receiver, args) {
          proxyLog.push({receiver, args});
          return Reflect.apply(target, receiver, args);
        },
      }));
  const boundProxy = keep(
      'boundProxy',
      callableProxy.bind({kind: 'proxy-bound-this'}, {kind: 'proxy-bound-arg'}));

  keep('noArgResult', noArgBound({kind: 'call-first'}));
  keep('oneArgResult', oneArgBound({kind: 'call-second'}));
  keep('manyArgsResult', manyArgsBound());
  keep('nestedBoundResult', nestedBound({kind: 'call-tail'}));
  keep('boundProxyResult', boundProxy({kind: 'proxy-call-tail'}));

  assertEquals('bound-this', catalog.noArgResult.thisValue.kind);
  assertEquals('bound-arg-object', catalog.oneArgResult.first.kind);
  assertEquals(4, catalog.manyArgsResult.argumentsLength);
  assertEquals('first-layer-bound', catalog.nestedBoundResult.first.kind);
  assertEquals('second-layer-bound', catalog.nestedBoundResult.second.kind);
  assertEquals('third-layer-bound', catalog.nestedBoundResult.third.kind);
  assertEquals('bound-constructor-arg:new-call-arg',
               constructedInstance.method());
  assertEquals('bound-class-arg:class-new-arg', classInstance.read());
  assertEquals('proxy-bound-arg', catalog.boundProxyResult.first.kind);
  assertEquals(1, proxyLog.length);

  globalThis.__heapDumpBoundFunctionsDeepKeepalive = catalog;
})();
