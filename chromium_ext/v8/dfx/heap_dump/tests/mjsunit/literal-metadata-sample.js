// Heap-dump smoke sample for literal/compiler metadata objects.
//
// The exact boilerplate/feedback objects are covered by white-box cctests; this
// JS sample keeps source patterns alive as dump/translate input so visitor-list
// changes around literal and feedback object families are exercised end-to-end.

(function() {
  'use strict';

  const catalog = {};
  function keep(name, value) {
    catalog[name] = value;
    return value;
  }

  function tagForMetadata(strings, ...values) {
    return {
      cooked: strings,
      raw: strings.raw,
      values,
      joined: strings.join('|'),
    };
  }

  function makeObjectLiteral(seed) {
    const dynamicKey = 'dynamic_' + seed;
    return {
      seed,
      [dynamicKey]: {nested: seed + 1},
      get accessor() {
        return this.seed * 2;
      },
      set accessor(value) {
        this.seed = value / 2;
      },
      method(delta = 1) {
        return this.seed + delta;
      },
      __proto__: {
        inheritedLiteralMarker: true,
      },
    };
  }

  function makeArrayLiteral(seed) {
    return [
      seed,
      seed + 1,
      {objectElement: seed + 2},
      [seed + 3, seed + 4],
      ,
    ];
  }

  function exerciseFeedback(target, key, value) {
    target[key] = value;
    return target.method?.(value) + target[key];
  }

  class LiteralMetadataBase {
    constructor(baseValue) {
      this.baseValue = baseValue;
    }
  }

  class LiteralMetadataClass extends LiteralMetadataBase {
    static staticObject = {fromStaticField: true};
    static ['computedStatic' + 'Name'] = 'computed-static';
    instanceField = {fromInstanceField: true};

    constructor(value) {
      super(value);
      this.value = value;
    }

    ['computedMethod' + 'Name']() {
      return this.value;
    }
  }

  const objectLiterals = [];
  const arrayLiterals = [];
  const feedbackTargets = [];
  for (let i = 0; i < 32; i++) {
    const objectLiteral = makeObjectLiteral(i);
    const arrayLiteral = makeArrayLiteral(i);
    objectLiteral.accessor = i * 4;
    objectLiterals.push(objectLiteral);
    arrayLiterals.push(arrayLiteral);
    feedbackTargets.push(exerciseFeedback(objectLiteral, 'slot' + i, i));
  }

  const regexpLiteralOne = /heap-(dump)-literal-(\d+)/gy;
  const regexpLiteralTwo = /(?<kind>heap)-(?<name>dump)/du;
  const regexpMatches = [
    regexpLiteralOne.exec('heap-dump-literal-12'),
    regexpLiteralTwo.exec('heap-dump'),
    ...'heap dump heap dump'.matchAll(/heap\s+(dump)/g),
  ];

  const taggedTemplates = [
    tagForMetadata`heap-${'dump'}-${1}`,
    tagForMetadata`heap-${'dump'}-${2}`,
  ];

  const destructured = [];
  for (const [index, {objectElement} = {objectElement: -1}] of
       arrayLiterals.map((items, index) => [index, items[2]])) {
    destructured.push({index, objectElement});
  }

  const spreadObject = {
    ...objectLiterals[0],
    spreadMarker: true,
  };
  const spreadArray = [
    ...arrayLiterals[0],
    ...arrayLiterals[1],
  ];

  const classInstance = new LiteralMetadataClass(42);
  assertEquals(42, classInstance.computedMethodName());
  assertTrue(LiteralMetadataClass.staticObject.fromStaticField);

  try {
    makeObjectLiteral(100);
    makeArrayLiteral(100);
    exerciseFeedback(makeObjectLiteral(101), 'optSlot', 5);
    keep('warmedObjectLiteral', makeObjectLiteral(102));
    keep('warmedArrayLiteral', makeArrayLiteral(102));
    keep('warmedFeedbackResult',
         exerciseFeedback(makeObjectLiteral(103), 'optSlot', 7));
  } catch (e) {
    keep('warmupError', e);
  }

  keep('objectLiterals', objectLiterals);
  keep('arrayLiterals', arrayLiterals);
  keep('feedbackTargets', feedbackTargets);
  keep('regexpLiterals', [regexpLiteralOne, regexpLiteralTwo]);
  keep('regexpMatches', regexpMatches);
  keep('taggedTemplates', taggedTemplates);
  keep('destructured', destructured);
  keep('spreadObject', spreadObject);
  keep('spreadArray', spreadArray);
  keep('literalClass', LiteralMetadataClass);
  keep('literalClassInstance', classInstance);

  globalThis.heapDumpLiteralMetadataSample = catalog;
})();
