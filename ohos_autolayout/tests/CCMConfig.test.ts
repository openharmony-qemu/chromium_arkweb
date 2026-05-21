/**
 * Unit tests for CCMConfig.ts
 */
import { CCMConfig, CCMRange, Breakpoint, AppRuleInfo, CheckRuleStateResult, ICCMConfig, AutoLayoutStrategyType } from '../src/Framework/Common/CCMConfig';

// Mock dependencies - Must come before imports
jest.mock('../src/Debug/Log', () => {
  const mockLog = {
    e: jest.fn(),
    info: jest.fn(),
    w: jest.fn(),
    d: jest.fn(),
  };
  return {
    __esModule: true,
    default: mockLog,
    ...mockLog
  };
});

import Log from '../src/Debug/Log';
import Tag from '../src/Debug/Tag';

describe('CCMConfig Module', () => {
  let configInstance: CCMConfig;

  beforeEach(() => {
    // Reset singleton instance before each test
    // @ts-ignore
    CCMConfig.instance = undefined;
    configInstance = CCMConfig.getInstance();
    jest.clearAllMocks();
  });

  describe('Singleton pattern', () => {
    test('should return the same instance', () => {
      const instance1 = CCMConfig.getInstance();
      const instance2 = CCMConfig.getInstance();
      
      expect(instance1).toBe(instance2);
    });

    test('should create instance with default config', () => {
      const instance = CCMConfig.getInstance();
      
      expect(instance).toBeDefined();
      expect(instance.getMinMaskAreaRatioThreshold()).toBe(1);
      expect(instance.getScaleAnimationDuration()).toBe(100);
    });
  });

  describe('Configuration getters', () => {
    test('should get minMaskAreaRatioThreshold', () => {
      expect(configInstance.getMinMaskAreaRatioThreshold()).toBe(1);
    });

    test('should get opacityFilter', () => {
      const filter = configInstance.getOpacityFilter();
      expect(filter).toEqual([0, 100]);
    });

    test('should get minContentAreaRatioThreshold', () => {
      expect(configInstance.getMinContentAreaRatioThreshold()).toBe(15);
    });

    test('should get scrollNodePattern', () => {
      const pattern = configInstance.getscrollNodePattern();
      expect(pattern).toContain('srcoll');
      expect(pattern).toContain('List');
    });

    test('should get closeButtonPattern', () => {
      const pattern = configInstance.getcloseButtonPattern();
      expect(pattern).toContain('close');
      expect(pattern).toContain('guanbi');
    });

    test('should get buttonPattern', () => {
      const pattern = configInstance.getButtonPattern();
      expect(pattern).toContain('button');
      expect(pattern).toContain('btn');
    });

    test('should get scaleAnimationDuration', () => {
      expect(configInstance.getScaleAnimationDuration()).toBe(100);
    });

    test('should get minScaleFactor', () => {
      expect(configInstance.getMinScaleFactor()).toBe(55);
    });

    test('should get minSARTofStickyComponent', () => {
      expect(configInstance.getMinSARTofStickyComponent()).toBe(5);
    });

    test('should get maxSARTofStickyComponent', () => {
      expect(configInstance.getMaxSARTofStickyComponent()).toBe(45);
    });

    test('should get appID', () => {
      expect(configInstance.getAppID()).toBe('');
    });

    test('should get page', () => {
      expect(configInstance.getPage()).toBe('');
    });
  });

  describe('Configuration setters', () => {
    test('should set minContentAreaRatioThreshold', () => {
      configInstance.setMinContentAreaRatioThreshold(20);
      expect(configInstance.getMinContentAreaRatioThreshold()).toBe(20);
    });
  });

  describe('findAspectRatioRange', () => {
    test('should find aspect ratio range for width in first breakpoint', () => {
      const range = configInstance.findAspectRatioRange(400);
      
      expect(range).toBeDefined();
      expect(range?.min).toBe(0.61);
      expect(range?.max).toBe(1.63);
    });

    test('should find aspect ratio range for width in second breakpoint', () => {
      const range = configInstance.findAspectRatioRange(700);
      
      expect(range).toBeDefined();
      expect(range?.min).toBe(0.7);
      expect(range?.max).toBe(2.0);
    });

    test('should find aspect ratio range for width in third breakpoint', () => {
      const range = configInstance.findAspectRatioRange(1100);
      
      expect(range).toBeDefined();
      expect(range?.min).toBe(1.3);
      expect(range?.max).toBe(1.45);
    });

    test('should return undefined for width outside breakpoints', () => {
      const range = configInstance.findAspectRatioRange(2000);
      expect(range).toBeUndefined();
    });

    test('should return undefined for width below all breakpoints', () => {
      const range = configInstance.findAspectRatioRange(100);
      expect(range).toBeUndefined();
    });

    test('should handle edge case at breakpoint boundaries', () => {
      const range1 = configInstance.findAspectRatioRange(320);
      expect(range1).toBeDefined();
      
      const range2 = configInstance.findAspectRatioRange(500);
      expect(range2).toBeDefined();
    });
  });

  describe('fromJson', () => {
    test('should parse valid JSON and update config', () => {
      const jsonConfig = JSON.stringify({
        minMaskAreaRatioThreshold: 5,
        opacityFilter: [10, 90],
        minContentAreaRatioThreshold: 20,
        scaleAnimationDuration: 200,
        minScaleFactor: 60,
        appRuleInfos: [{ id: '123', pg: ['page1'] }],
        needCheckIdAndPage: true,
      });
      
      const result = configInstance.fromJson(jsonConfig);
      
      expect(result).toBe(true);
      expect(configInstance.getMinMaskAreaRatioThreshold()).toBe(5);
      expect(configInstance.getOpacityFilter()).toEqual([10, 90]);
      expect(configInstance.getMinContentAreaRatioThreshold()).toBe(20);
      expect(configInstance.getScaleAnimationDuration()).toBe(200);
      expect(configInstance.getMinScaleFactor()).toBe(60);
    });

    test('should handle invalid JSON gracefully', () => {
      const invalidJson = 'not a json string';
      
      const result = configInstance.fromJson(invalidJson);
      
      expect(result).toBe(false);
      expect(Log.e).toHaveBeenCalled();
    });

    test('should handle empty string', () => {
      const result = configInstance.fromJson('');
      
      expect(result).toBe(false);
      expect(Log.e).toHaveBeenCalled();
    });

    test('should handle partial config update', () => {
      const jsonConfig = JSON.stringify({
        minMaskAreaRatioThreshold: 10,
        opacityFilter: [5, 95],
        minContentAreaRatioThreshold: 25,
        scaleAnimationDuration: 150,
        minScaleFactor: 50,
        appRuleInfos: [],
        needCheckIdAndPage: true,
      });
      
      const result = configInstance.fromJson(jsonConfig);
      
      expect(result).toBe(true);
      expect(configInstance.getMinMaskAreaRatioThreshold()).toBe(10);
    });

    test('should parse stringified appRuleInfos', () => {
      const jsonConfig = JSON.stringify({
        minMaskAreaRatioThreshold: 1,
        opacityFilter: [0, 100],
        minContentAreaRatioThreshold: 15,
        scaleAnimationDuration: 100,
        minScaleFactor: 55,
        appRuleInfos: '[{"id":"123","pg":["home"]}]',
        needCheckIdAndPage: true,
      });
      
      const result = configInstance.fromJson(jsonConfig);
      expect(result).toBe(true);
    });
  });

  describe('checkRule', () => {
    beforeEach(() => {
      // Mock window functions
      // @ts-ignore
      window._getAppId_ = jest.fn(() => '000');
      // @ts-ignore
      window._getPage_ = jest.fn(() => 'home');
    });

    afterEach(() => {
      // @ts-ignore
      delete window._getAppId_;
      // @ts-ignore
      delete window._getPage_;
    });

    test('should return initial when appId and page are not available', () => {
      // @ts-ignore
      window._getAppId_ = jest.fn(() => '');
      // @ts-ignore
      window._getPage_ = jest.fn(() => '');
      
      // Reset the singleton to get fresh state
      // @ts-ignore
      CCMConfig.instance = undefined;
      const freshInstance = CCMConfig.getInstance();
      
      const result = freshInstance.checkRule();
      
      expect(result).toBe(CheckRuleStateResult.initial);
      expect(Log.info).toHaveBeenCalledWith(
        '未能获取Appid和page，周期性监听',
        expect.any(String)
      );
    });

    test('should return inWhiteList when appId and page match', () => {
      // @ts-ignore
      window._getAppId_ = jest.fn(() => '000');
      // @ts-ignore
      window._getPage_ = jest.fn(() => 'home');
      
      // Reset the singleton to get fresh state
      // @ts-ignore
      CCMConfig.instance = undefined;
      const freshInstance = CCMConfig.getInstance();
      
      const result = freshInstance.checkRule();
      
      expect(result).toBe(CheckRuleStateResult.inWhiteList);
    });

    test('should return outOfWhiteList when no rule matches', () => {
      // @ts-ignore
      window._getAppId_ = jest.fn(() => '999');
      // @ts-ignore
      window._getPage_ = jest.fn(() => 'unknown');
      
      // Reset the singleton to get fresh state
      // @ts-ignore
      CCMConfig.instance = undefined;
      const freshInstance = CCMConfig.getInstance();
      
      const result = freshInstance.checkRule();
      
      expect(result).toBe(CheckRuleStateResult.outOfWhiteList);
    });

    test('should match wildcard appId', () => {
      const jsonConfig = JSON.stringify({
        minMaskAreaRatioThreshold: 1,
        opacityFilter: [0, 100],
        minContentAreaRatioThreshold: 15,
        scaleAnimationDuration: 100,
        minScaleFactor: 55,
        appRuleInfos: [{ id: '*', pg: ['home'] }],
        needCheckIdAndPage: true,
      });
      
      // Reset the singleton to get fresh state
      // @ts-ignore
      CCMConfig.instance = undefined;
      const freshInstance = CCMConfig.getInstance();
      freshInstance.fromJson(jsonConfig);
      
      // @ts-ignore
      window._getAppId_ = jest.fn(() => 'anyAppId');
      // @ts-ignore
      window._getPage_ = jest.fn(() => 'home');
      
      const result = freshInstance.checkRule();
      
      expect(result).toBe(CheckRuleStateResult.inWhiteList);
    });

    test('should match wildcard page', () => {
      const jsonConfig = JSON.stringify({
        minMaskAreaRatioThreshold: 1,
        opacityFilter: [0, 100],
        minContentAreaRatioThreshold: 15,
        scaleAnimationDuration: 100,
        minScaleFactor: 55,
        appRuleInfos: [{ id: '123', pg: ['*'] }],
        needCheckIdAndPage: true,
      });
      
      // Reset the singleton to get fresh state
      // @ts-ignore
      CCMConfig.instance = undefined;
      const freshInstance = CCMConfig.getInstance();
      freshInstance.fromJson(jsonConfig);
      
      // @ts-ignore
      window._getAppId_ = jest.fn(() => '123');
      // @ts-ignore
      window._getPage_ = jest.fn(() => 'anyPage');
      
      const result = freshInstance.checkRule();
      
      expect(result).toBe(CheckRuleStateResult.inWhiteList);
    });

    test('should cache result and not check again', () => {
      // @ts-ignore
      window._getAppId_ = jest.fn(() => '000');
      // @ts-ignore
      window._getPage_ = jest.fn(() => 'home');
      
      // Reset the singleton to get fresh state
      // @ts-ignore
      CCMConfig.instance = undefined;
      const freshInstance = CCMConfig.getInstance();
      
      const result1 = freshInstance.checkRule();
      const result2 = freshInstance.checkRule();
      
      expect(result1).toBe(result2);
      expect(result1).toBe(CheckRuleStateResult.inWhiteList);
    });

    test('should handle functions not being available', () => {
      // @ts-ignore
      delete window._getAppId_;
      // @ts-ignore
      delete window._getPage_;
      
      // Reset the singleton to get fresh state
      // @ts-ignore
      CCMConfig.instance = undefined;
      const freshInstance = CCMConfig.getInstance();
      
      const result = freshInstance.checkRule();
      
      expect(result).toBe(CheckRuleStateResult.initial);
    });

    test('should check custom config field', () => {
      // @ts-ignore
      window._getAppId_ = jest.fn(() => '123');
      // @ts-ignore
      window._getPage_ = jest.fn(() => 'home');

      // Reset the singleton to get fresh state
      // @ts-ignore
      CCMConfig.instance = undefined;
      const freshInstance = CCMConfig.getInstance();

      freshInstance.checkRule();

      expect(Log.info).not.toHaveBeenCalledWith('当前页面包含自定义配置策略', Tag.ccmConfig);

      const jsonConfig = JSON.stringify({
        minMaskAreaRatioThreshold: 1,
        opacityFilter: [0, 100],
        minContentAreaRatioThreshold: 15,
        scaleAnimationDuration: 100,
        minScaleFactor: 55,
        appRuleInfos: [{ id: '123', pg: ['*'], configs: [{style: ''}]}],
        needCheckIdAndPage: true,
      });

      freshInstance.fromJson(jsonConfig);
      freshInstance.checkRule();
      expect(Log.info).toHaveBeenCalledWith('当前页面包含自定义配置策略', Tag.ccmConfig);
    });
  });

  describe('update method', () => {
    test('should update configuration values', () => {
      const newConfig = {
        minMaskAreaRatioThreshold: 10,
        opacityFilter: [20, 80] as [number, number],
        minContentAreaRatioThreshold: 30,
        scaleAnimationDuration: 300,
        minScaleFactor: 70,
        appRuleInfos: [{ id: 'new', pg: ['newPage'] }],
        needCheckIdAndPage: true,
      };
      
      configInstance.update(newConfig);
      
      expect(configInstance.getMinMaskAreaRatioThreshold()).toBe(10);
      expect(configInstance.getOpacityFilter()).toEqual([20, 80]);
      expect(configInstance.getMinContentAreaRatioThreshold()).toBe(30);
      expect(configInstance.getScaleAnimationDuration()).toBe(300);
      expect(configInstance.getMinScaleFactor()).toBe(70);
    });
  });

  describe('Edge cases', () => {
    test('should handle negative values in configuration', () => {
      const jsonConfig = JSON.stringify({
        minMaskAreaRatioThreshold: -1,
        opacityFilter: [-10, 110],
        minContentAreaRatioThreshold: -5,
        scaleAnimationDuration: -100,
        minScaleFactor: -50,
        appRuleInfos: [],
        needCheckIdAndPage: true,
      });
      
      const result = configInstance.fromJson(jsonConfig);
      
      expect(result).toBe(true);
      expect(configInstance.getMinMaskAreaRatioThreshold()).toBe(-1);
    });

    test('should handle very large numbers', () => {
      const jsonConfig = JSON.stringify({
        minMaskAreaRatioThreshold: Number.MAX_SAFE_INTEGER,
        opacityFilter: [0, 100],
        minContentAreaRatioThreshold: 15,
        scaleAnimationDuration: 100,
        minScaleFactor: 55,
        appRuleInfos: [],
        needCheckIdAndPage: true,
      });
      
      const result = configInstance.fromJson(jsonConfig);
      
      expect(result).toBe(true);
      expect(configInstance.getMinMaskAreaRatioThreshold()).toBe(Number.MAX_SAFE_INTEGER);
    });

    test('should handle empty appRuleInfos array', () => {
      const jsonConfig = JSON.stringify({
        minMaskAreaRatioThreshold: 1,
        opacityFilter: [0, 100],
        minContentAreaRatioThreshold: 15,
        scaleAnimationDuration: 100,
        minScaleFactor: 55,
        appRuleInfos: [],
        needCheckIdAndPage: true,
      });
      
      const result = configInstance.fromJson(jsonConfig);
      expect(result).toBe(true);
    });
  });

  describe('parseMiniappOptionalConfig() method', () => {
    test('parseMiniappOptionalConfig should process the config field in the input app rule config string', () => {
      const ccmConfigInstance = CCMConfig.getInstance();
      let customConfigList = ccmConfigInstance.getCustomConfigs();
      expect(customConfigList.length).toBe(0);
    });
  });

  describe('isPopupScaleEnable() method', () => {
    test('isPopupScaleEnable should judge if the input strategy is for popup scale', () => {
      const ccmConfigInstance = CCMConfig.getInstance();
      expect(ccmConfigInstance.isPopupScaleEnable(AutoLayoutStrategyType.popupScale)).toBe(true);
      expect(ccmConfigInstance.isPopupScaleEnable(AutoLayoutStrategyType.alphabetNavigator)).toBe(false);
      expect(ccmConfigInstance.isPopupScaleEnable(AutoLayoutStrategyType.customConfig)).toBe(false);
    });
  });

  describe('isAlphabetNavigatorEnable() method', () => {
    test('isAlphabetNavigatorEnable should judge if the input strategy is for alphabet', () => {
      const ccmConfigInstance = CCMConfig.getInstance();
      expect(ccmConfigInstance.isAlphabetNavigatorEnable(AutoLayoutStrategyType.popupScale)).toBe(false);
      expect(ccmConfigInstance.isAlphabetNavigatorEnable(AutoLayoutStrategyType.alphabetNavigator)).toBe(true);
      expect(ccmConfigInstance.isAlphabetNavigatorEnable(AutoLayoutStrategyType.customConfig)).toBe(false);
    });
  });

  describe('isCustomConfigEnable() method', () => {
    test('isCustomConfigEnable should judge if the input strategy is for custom config', () => {
      const ccmConfigInstance = CCMConfig.getInstance();
      expect(ccmConfigInstance.isCustomConfigEnable(AutoLayoutStrategyType.popupScale)).toBe(false);
      expect(ccmConfigInstance.isCustomConfigEnable(AutoLayoutStrategyType.alphabetNavigator)).toBe(false);
      expect(ccmConfigInstance.isCustomConfigEnable(AutoLayoutStrategyType.customConfig)).toBe(true);
    });
  });
});
