// Heap-dump smoke sample for property-shape and dictionary objects.
//
// This is intentionally JS-only. White-box cctests own exact edge checks for
// DescriptorArray, TransitionArray, EnumCache, PropertyArray, and dictionaries;
// this sample keeps naturally-created shapes alive for dump/translate smoke.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function makeTransitionSeed(prefix, index) {
    const object = {};
    object[prefix + '_a'] = index;
    object[prefix + '_b'] = {index};
    object[prefix + '_c'] = prefix + index;
    return object;
  }

  const transitionFanout = [];
  for (let i = 0; i < 64; ++i) {
    const object = makeTransitionSeed('shape', i);
    object['branch_' + (i % 8)] = {branch: i};
    if ((i & 1) === 0) object.even = true;
    if ((i & 2) === 0) object.quarter = true;
    transitionFanout.push(object);
  }

  const fastPropertyObjects = [];
  for (let i = 0; i < 24; ++i) {
    const object = {};
    object.p0 = {i, slot: 0};
    object.p1 = {i, slot: 1};
    object.p2 = {i, slot: 2};
    object.p3 = {i, slot: 3};
    object.p4 = {i, slot: 4};
    fastPropertyObjects.push(object);
  }

  const dictionaryObjects = [];
  for (let i = 0; i < 6; ++i) {
    const object = {};
    for (let j = 0; j < 80; ++j) {
      object['dict_' + i + '_' + j] = {i, j};
    }
    for (let j = 0; j < 40; j += 3) {
      delete object['dict_' + i + '_' + j];
    }
    dictionaryObjects.push(object);
  }

  const accessorDictionary = {};
  for (let i = 0; i < 16; ++i) {
    Object.defineProperty(accessorDictionary, 'accessor_' + i, {
      configurable: true,
      enumerable: i % 2 === 0,
      get() {
        return {i};
      },
    });
  }
  delete accessorDictionary.accessor_3;
  delete accessorDictionary.accessor_7;

  const enumCacheObjects = [];
  for (let i = 0; i < 16; ++i) {
    const object = {
      first: i,
      second: {i},
      third: 'value-' + i,
    };
    enumCacheObjects.push({
      object,
      keys: Object.keys(object),
      values: Object.values(object),
      entries: Object.entries(object),
    });
    for (const key in object) {
      if (key === 'second') keep('enumSeenSecond' + i, object[key]);
    }
  }

  const sealed = Object.seal({
    sealedA: {a: 1},
    sealedB: {b: 2},
  });
  const frozen = Object.freeze({
    frozenA: {a: 1},
    frozenB: {b: 2},
  });
  const nonExtensible = {
    fixedA: {a: 1},
    fixedB: {b: 2},
  };
  Object.preventExtensions(nonExtensible);

  const nullPrototypeDictionary = Object.create(null);
  for (let i = 0; i < 48; ++i) {
    nullPrototypeDictionary['null_proto_' + i] = {i};
  }
  delete nullPrototypeDictionary.null_proto_5;
  delete nullPrototypeDictionary.null_proto_17;

  const symbolA = Symbol('shape-a');
  const symbolB = Symbol.for('heap-dump-shape-b');
  const symbolProperties = {
    visible: true,
    [symbolA]: {symbol: 'a'},
    [symbolB]: {symbol: 'b'},
  };
  Object.defineProperty(symbolProperties, 'hiddenShapeValue', {
    enumerable: false,
    configurable: true,
    value: {hidden: true},
  });

  const sparseArrays = [];
  for (let i = 0; i < 8; ++i) {
    const array = [];
    array[0] = {i, index: 0};
    array[10 + i] = {i, index: 10 + i};
    array[1000 + i * 17] = {i, sparse: true};
    array.extraProperty = {i, extra: true};
    sparseArrays.push(array);
  }

  const mixedElements = [
    [1, 2, 3],
    [1.25, 2.5, 3.75],
    [{object: 1}, {object: 2}],
  ];
  mixedElements[0].push({convertedToObjectElements: true});
  mixedElements[1].push({convertedFromDoubleElements: true});

  const proxiesOverShapeObjects = [];
  for (let i = 0; i < 4; ++i) {
    const target = dictionaryObjects[i];
    proxiesOverShapeObjects.push(new Proxy(target, {
      ownKeys(targetObject) {
        return Reflect.ownKeys(targetObject);
      },
      getOwnPropertyDescriptor(targetObject, key) {
        return Reflect.getOwnPropertyDescriptor(targetObject, key);
      },
      get(targetObject, key, receiver) {
        return Reflect.get(targetObject, key, receiver);
      },
    }));
  }

  keep('transitionFanout', transitionFanout);
  keep('fastPropertyObjects', fastPropertyObjects);
  keep('dictionaryObjects', dictionaryObjects);
  keep('accessorDictionary', accessorDictionary);
  keep('enumCacheObjects', enumCacheObjects);
  keep('sealed', sealed);
  keep('frozen', frozen);
  keep('nonExtensible', nonExtensible);
  keep('nullPrototypeDictionary', nullPrototypeDictionary);
  keep('symbolProperties', symbolProperties);
  keep('symbolKeys', [symbolA, symbolB]);
  keep('symbolReflectKeys', Reflect.ownKeys(symbolProperties));
  keep('sparseArrays', sparseArrays);
  keep('mixedElements', mixedElements);
  keep('proxiesOverShapeObjects', proxiesOverShapeObjects);
  keep('proxyKeys', proxiesOverShapeObjects.map((proxy) => Reflect.ownKeys(proxy)));

  assertTrue(Object.isSealed(sealed));
  assertTrue(Object.isFrozen(frozen));
  assertFalse(Object.isExtensible(nonExtensible));
  assertTrue(Reflect.ownKeys(symbolProperties).includes(symbolA));

  globalThis.heapDumpPropertyShapesSample = catalog;
})();
