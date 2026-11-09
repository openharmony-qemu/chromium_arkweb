/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import { vConfig } from '../VersionConfig';

export interface Action {
  name: string;
  description: string;
  format: string;
  argsSchema: string;
  example?: string;
  needWait: boolean;
}

const enableTakeNote = !(
  vConfig.conventional || vConfig.take_note_in_observe || vConfig.parallel
);
const elementOrCoordinate = vConfig.support_coordinate_action ? 'elementid/coordinate' : 'elementid';
const pointPrompt = vConfig.support_coordinate_action ? ' or a point' : '';
const clickArgsSchema = vConfig.support_coordinate_action
  ? '{"elementId": <number>} or {"coordinate": [<number>, ...]}'
  : '{"elementId": <number>}';

const navigation: Action = {
  name: 'NAVIGATION',
  description:
    'Open a new tab in the browser and enter the url to navigate to the page. Scenario example: Use this when the current page is irrelevant to the subtask or task.',
  format: 'NAVIGATION [url]',
  argsSchema: '{"url": <string>}',
  needWait: true,
};

const navigateTo: Action = {
  name: 'NAVIGATE_TO',
  description:
    'Navigate the current tab to the url without opening a new tab. Use this when the task must leave or replace ' +
    'the current page, including flows where the current page may show an unsaved-changes dialog. Use NAVIGATION ' +
    'instead when the current page should remain open in its own tab.',
  format: 'NAVIGATE_TO [url]',
  argsSchema: '{"url": <string>}',
  needWait: true,
};

const goBack: Action = {
  name: 'GO_BACK',
  description:
    'Return to the previous page in the tab stack (stack pop). Scenario example: Use this when the current page does not meet expectations and you need to revert, or when there is no more action required on this page and you need to continue elsewhere. This is not a normal go back action in browser which operates only on the current tab. It follows the logic of the tab stack!',
  format: 'GO_BACK',
  argsSchema: '{}',
  needWait: true,
};


const click: Action = {
  name: 'CLICK',
  description:
    `Click on a web element${pointPrompt}. Use this to interact with buttons, links, or checkboxes with an elementId. Do NOT use CLICK for input controls such as search boxes while SEARCH or FILL is more appropriate. Do not close login windows.`,
  format: `CLICK [${elementOrCoordinate}]`,
  argsSchema: clickArgsSchema,
  needWait: true,
};

const dialog: Action = {
  name: 'DIALOG',
  description:
    'Handle the open browser dialog. This browser-level action needs no elementId and remains valid when LiteDomTree is empty. Use accept to confirm or leave, dismiss to cancel or stay, and provide prompt text only when needed.',
  format: 'DIALOG [accept/dismiss] [prompt text optional]',
  argsSchema: '{"mode": <string accept|dismiss>, "text"?: <string>}',
  needWait: false,
};

const search: Action = {
  name: 'SEARCH',
  description:
    'Click and enter text into the search box corresponding to elementid and press Enter. Use this to search relevant information or jump to new pages.',
  format: 'SEARCH [elementid] [text]',
  argsSchema: '{"elementId": <number>, "text": <string>}',
  needWait: true,
};

const searchByQuery: Action = {
  name: 'SEARCH_BY_QUERY',
  description:
    'Click and enter text with 3 or more keywords into the search box or a form/table cell. Do not include time-related words in the query.',
  format: 'SEARCH_BY_QUERY [elementid] [text1, text2, text3, ... ]',
  argsSchema: '{"elementId": <number>, "text": <string>}',
  needWait: true,
};

const fill: Action = {
  name: 'FILL',
  description:
    'Click and enter text into an input box (inputtable=true). Specify either to directly input when the box is empty, or replace / prepend / append to the already existing content. To fill multiple lines of text, separate the lines with \\n instead of real line breaks.',
  format: 'FILL [INPUT/REPLACE/PREPEND/APPEND] [elementid] [text]',
  argsSchema: '{"mode": <string INPUT|REPLACE|PREPEND|APPEND>, "elementId": <number>, "text": <string>}',
  needWait: false,
};

const richText: Action = {
  name: 'RICH_TEXT',
  description:
  'Fill rich text formatted by **HTML code** into an input box (inputtable=True). Specify either to directly input when the box is empty, or replace / prepend / append to the already existing content. You may use this to fill text with specific format. No need to click on the input element first, directly use this.',
  format: 'RICH_TEXT [INPUT/REPLACE/PREPEND/APPEND] [elementid] [code]',
  argsSchema: '{"mode": <string INPUT|REPLACE|PREPEND|APPEND>, "elementId": <number>, "code": <string>}',
  needWait: false,
};

const selectText: Action = {
  name: 'SELECT_TEXT',
  description:
  'Select a specified substring from the texts already filled in an input box (inputtable=true). Specify the index of the substring (starting from 1 for the first appearance) in case there are multiple matches. ',
  format: 'SELECT_TEXT [elementid] [substring] [index]',
  argsSchema: '{"elementId": <number>, "substring": <string>, "index": <number>}',
  needWait: false,
};

const hover: Action = {
  name: 'HOVER',
  description:
    'Hover the mouse over an element. Use this when there are dropdown items or hidden controls triggered by hovering.',
  format: 'HOVER [elementid]',
  argsSchema: '{"elementId": <number>}',
  needWait: true,
};

const scroll: Action = {
  name: 'SCROLL',
  description:
  'Scroll the page / an element in the specified direction. You can specify the scroll target (window / elementID) and viewport ratio (between 0 and 1, where 1 is one full viewport height/width. You are encouraged to use a larger ratio below 1 such as 0.7 to improve efficiency).  Examples: SCROLL [DOWN] [elementID] [1]; SCROLL [DOWN] [window] [0.7]. You can scroll elements with scrollable=true, or scroll the whole window when there is a page scrollbar.',
  format: 'SCROLL [UP/DOWN/LEFT/RIGHT] [window/elementID] [viewportRatio optional, 0-1]',
  argsSchema: '{"direction": <string UP|DOWN|LEFT|RIGHT>, "target": <"window"|elementId number>, "amount"?: <number 0-1>}',
  needWait: true,
};

const select: Action = {
  name: 'SELECT',
  description:
    'Select an option from an element with a "SELECT" tag. The option must strictly follow the text options in the LiteDOMTree. ',
  format: 'SELECT [elementid] [option]',
  argsSchema: '{"elementId": <number>, "option": <string>}',
  needWait: false,
};

const upload: Action = {
  name: 'UPLOAD',
  description:
  'Upload local file(s) with exact absolute file system path(s) explicitly provided by the user. The element MUST have tag <input> and uploadable=true. If there is no such element, you cannot use UPLOAD. **Do NOT click upload buttons** as it will trigger a OS file selector which you cannot see. Note that the uploadable element might not be visible on the screen and thus its semantics might not be clear, and you need to try to guess which one to use if multiple is provided.',
  format: 'UPLOAD [elementid] [local_path1] [local_path2] ...',
  argsSchema: '{"elementId": <number>, "paths": [<string>, ...]}',
  needWait: true,
};

const takeNote: Action = {
  name: 'TAKE_NOTE',
  description:
    'Record certain information from the current webpage in the Notes. Use this when the instruction requires collecting information, or when you need to memorize information to support later decisions such as comparison. Keep the information faithful.',
  format: 'TAKE_NOTE [text]',
  argsSchema: '{"text": <string>}',
  needWait: false,
};

const calculate: Action = {
  name: 'CALCULATE',
  description:
    'Calculate the value of a mathematical expression. Example scenario: the task needs arithmetic before the next action.',
  format: 'CALCULATE [expression]',
  argsSchema: '{"expression": <string>}',
  needWait: false,
};

const getHref: Action = {
  name: 'GET_HREF',
  description:
    'Get the href of DOM <a> elements with specified ids.',
  format: 'GET_HREF [elementID ...]',
  argsSchema: '{"elementIds": [<number>, ...]}',
  needWait: false,
};


const dispatch: Action = {
  name: 'DISPATCH',
  description:
    'Dispatch sub-agents to perform a repetitive task on multiple parallel arranged anchor elements (tag=A). This action will not change the current webpage because each sub-agent performs its subtask in a new tab. The subtask should be concrete and the targets must be anchor elements. Just say what to do **after** opening the target links, do not include \'Open this links\' in the subtask.',
  format: 'DISPATCH [task] [elementID ...]',
  argsSchema: '{"task": <string>, "elementIds": [<number>, ...]}',
  needWait: false,
};

const userControl: Action = {
  name: 'USER_CONTROL',
  description:
  'Request user to take over the browser. Use this when: 1. login / CAPTCHA: requires user login / CAPTCHA to continue task (a login window popped up).  2. dangerous operations: if you are about to perform operations including: open, click or navigate to suspicious hyperlinks or links (e.g., the website is claimed to be a shopping website, a transaction platform, or a link for filling in forms), sending private data (e.g., name, ID card number, mobile number, and address) to external systems, transaction (payment), or delete-related actions (e.g., delete video). In reason and explanation, specify what the user needs to do.',
  format: 'USER_CONTROL [reason]',
  argsSchema: '{"reason": <string>}',
  needWait: false,
};


const askUser: Action = {
  name: 'ASK_USER',
  description:
  `Ask the user to provide information / options missing in the instruction that is essential to continuing the task. For example, filling required fields in a form which you don't know / choosing from multiple passengers when booking tickets that is unspecified in the instruction. ` +
    `The user may answer the question, or manually take over the browser. You can combine multiple questions. Use this action carefully. **DO NOT** ask for methods to complete the task.`,
  format: 'ASK_USER [question]',
  argsSchema: '{"question": <string>}',
  needWait: false,
};

const wait: Action = {
  name: 'WAIT',
  description:
    'Do nothing and wait for the webpage to load. Use this when the webpage is still loading, or a lot of litedom elements are missing.',
  format: 'WAIT',
  argsSchema: '{}',
  needWait: false,
};

const skip: Action = {
  name: 'SKIP',
  description:
    'Do nothing and skip the current subtask if it is already completed.',
  format: 'SKIP',
  argsSchema: '{}',
  needWait: false,
};

const finish: Action = {
  name: 'FINISH',
  description:
    'Perform no action and conclude the task as completed.',
  format: 'FINISH',
  argsSchema: '{}',
  needWait: false,
};

const arrow: Action = {
  name: 'ARROW',
  description:
    'Press one of the arrow keys.',
  format: 'ARROW [UP/DOWN/LEFT/RIGHT]',
  argsSchema: '{"direction": <string UP|DOWN|LEFT|RIGHT>}',
  needWait: true,
};

const press: Action = {
  name: 'PRESS',
  description:
    'Press a key. Example: PRESS Enter, PRESS Tab.',
  format: 'PRESS [key]',
  argsSchema: '{"key": <string>}',
  needWait: false,
};

const allActions: Action[] = [
  navigation,
  navigateTo,
  goBack,
  dialog,
  click,
  search,
  searchByQuery,
  fill,
  hover,
  scroll,
  select,
  upload,
  takeNote,
  calculate,
  richText,
  getHref,
  dispatch,
  userControl,
  wait,
  skip,
  finish,
  arrow,
  press,
];

export const ACTION_BY_NAME: Map<string, Action> = new Map(
  allActions.map(action => [action.name.toUpperCase(), action]),
);

export function getActionByName(name: string): Action | undefined {
  return ACTION_BY_NAME.get(name.trim().toUpperCase());
}

const basicActions: Action[] = [
  navigation,
  navigateTo,
  goBack,
  dialog,
  click,
  search,
  fill,
  scroll,
  userControl,
  finish,
];

const planActions: Action[] = [
  navigation,
  navigateTo,
  dialog,
  click,
  hover,
  search,
  fill,
  selectText,
  richText,
  scroll,
  select,
  upload,
  ...(enableTakeNote ? [takeNote] : []),
  goBack,
  dispatch,
  userControl,
  // askUser,
  wait,
  finish,
];

const judgeActions: Action[] = [
  navigation,
  navigateTo,
  goBack,
  dialog,
  click,
  search,
  fill,
  selectText,
  hover,
  scroll,
  select,
  upload,
  arrow,
  press,
  ...(enableTakeNote ? [takeNote] : []),
  userControl,
  wait,
  skip,
  finish,
];

let formatReminder = vConfig.json_action_format
  ? '** Important Notice **: Each action must be a JSON object with "name" and "args" keys. Use the exact key names specified in each action\'s Args below. elementId values must be numbers, not strings.'
  : '** Important Notice **: ' +
  'In all the actions, you must not miss the square brackets! Good example: FILL [REPLACE] [10] [John]. Bad example: FILL REPLACE 10 John';
if (vConfig.support_coordinate_action) {
  if (vConfig.json_action_format) {
    formatReminder += `\n ** About Coordinate targets **:
  Some actions support using coordinate rather than elements as targets. You may use this when the element you want to operate does not have an elementid (note that the litedom may not include ALL interactive elements due to rule defects), or to click on an empty place to disable dropdown menus.
  To specify coordinates, output the coordinates as 4 or 2 numbers. 4 number denotes the bounding box of the target element. 2 number denotes the point to perform the action on.
  `;
  } else {
    formatReminder += `\n ** About Coordinate targets **:
  Actions whose target format is ${elementOrCoordinate} supports specifying coordinate rather than elements as targets. You may use this when the element you want to operate does not have an elementid, or to click on an empty place to disable dropdown menus.
  To specify coordinates, output COORDINATE[x1, y1, x2, y2] denoting the bounding box of the target element, or just COORDINATE[x, y] denoting the pixel to click.
  Example of using element as target: CLICK [25]
  Example of using coordinate as target: CLICK [COORDINATE[290, 532, 315, 550]]
  `;
  }
}

function makeActionPrompt(
  actions: Action[],
  options?: {
    includeExamples?: boolean;
    closingText?: string;
  },
): string {
  let promptParts: string[] = [];
  actions.forEach((action) => {
    promptParts.push(`- ${action.name}: ${action.description}\n`);
    promptParts.push(vConfig.json_action_format ? `  Args: ${action.argsSchema}\n` : `  Format: ${action.format}\n`);
    if (options?.includeExamples && action.example) {
      promptParts.push(`  Example: ${action.example}\n`);
    }
    promptParts.push('\n');
  });

  if (options?.closingText) {
    promptParts.push(options.closingText);
  }
  return promptParts.join('');
}
export const actions = makeActionPrompt(basicActions, {closingText: formatReminder});

export const actionsWithPlan = makeActionPrompt(planActions, {closingText: formatReminder});

export const actionsForJudge = makeActionPrompt(judgeActions, {closingText: formatReminder});

export const ACTION_DEFINITIONS = planActions;
export const ACTION_PROMPT = actionsWithPlan;

const actionMap = new Map<string, boolean>(
  ACTION_DEFINITIONS.map(action => [action.name.toUpperCase(), action.needWait])
);

export function resolveActionNeedWait(actionName: string): boolean {
  const normalized = actionName.trim().toUpperCase();
  return actionMap.get(normalized) || true;
}

type ActionSynthesizer = (
  args: Record<string, Object>,
  getStr: (key: string) => string,
  has: (key: string) => boolean,
) => string;

const actionSynthesizers: Record<string, ActionSynthesizer> = {
  NAVIGATION: (_args, getStr) => `NAVIGATION [${getStr('url')}]`,
  NAVIGATE_TO: (_args, getStr) => `NAVIGATE_TO [${getStr('url')}]`,
  GO_BACK: () => 'GO_BACK',
  DIALOG: (_args, getStr) => {
    const mode = getStr('mode') || 'accept';
    const text = getStr('text');
    return text.length > 0 ? `DIALOG [${mode}] [${text}]` : `DIALOG [${mode}]`;
  },
  CLICK: (args, getStr, has) => {
    if (has('coordinate')) {
      const coord = args.coordinate;
      const coordArr: Object[] = Array.isArray(coord) ? coord as Object[] : [coord];
      return `CLICK [COORDINATE[${coordArr.map(item => String(item)).join(', ')}]]`;
    }
    return `CLICK [${getStr('elementId')}]`;
  },
  SEARCH: (_args, getStr) => `SEARCH [${getStr('elementId')}] [${getStr('text')}]`,
  SEARCH_BY_QUERY: (_args, getStr) => `SEARCH_BY_QUERY [${getStr('elementId')}] [${getStr('text')}]`,
  FILL: (_args, getStr) =>
    `FILL [${getStr('mode') || 'REPLACE'}] [${getStr('elementId')}] [${getStr('text')}]`,
  SELECT_TEXT: (_args, getStr) =>
    `SELECT_TEXT [${getStr('elementId')}] [${getStr('substring')}] [${getStr('index') || '1'}]`,
  HOVER: (_args, getStr) => `HOVER [${getStr('elementId')}]`,
  SCROLL: (_args, getStr, has) => {
    const direction = getStr('direction') || 'DOWN';
    const target = has('target') ? getStr('target') : 'window';
    let result = `SCROLL [${direction}] [${target}]`;
    if (has('amount')) {
      result += ` [${getStr('amount')}]`;
    }
    return result;
  },
  SELECT: (_args, getStr) => `SELECT [${getStr('elementId')}] [${getStr('option')}]`,
  UPLOAD: (args, getStr) => {
    const pathsRaw = args.paths;
    const paths: Object[] = Array.isArray(pathsRaw) ? pathsRaw as Object[] : [];
    const pathParts = paths.map(path => `[${String(path)}]`).join(' ');
    return `UPLOAD [${getStr('elementId')}]${pathParts.length > 0 ? ' ' + pathParts : ''}`;
  },
  TAKE_NOTE: (_args, getStr) => `TAKE_NOTE [${getStr('text')}]`,
  CALCULATE: (_args, getStr) => `CALCULATE [${getStr('expression')}]`,
  RICH_TEXT: (_args, getStr) =>
    `RICH_TEXT [${getStr('mode') || 'REPLACE'}] [${getStr('elementId')}] [${getStr('code')}]`,
  GET_HREF: (args) => {
    const idsRaw = args.elementIds;
    const ids: Object[] = Array.isArray(idsRaw) ? idsRaw as Object[] : [];
    return `GET_HREF ${ids.map(id => `[${String(id)}]`).join(' ')}`.trim();
  },
  DISPATCH: (args, getStr) => {
    const idsRaw = args.elementIds;
    const ids: Object[] = Array.isArray(idsRaw) ? idsRaw as Object[] : [];
    return `DISPATCH [${getStr('task')}] [${ids.map(id => String(id)).join(' ')}]`;
  },
  USER_CONTROL: (_args, getStr) => `USER_CONTROL [${getStr('reason')}]`,
  ASK_USER: (_args, getStr) => `ASK_USER [${getStr('question')}]`,
  WAIT: () => 'WAIT',
  SKIP: () => 'SKIP',
  FINISH: () => 'FINISH',
  ARROW: (_args, getStr) => `ARROW [${getStr('direction') || 'DOWN'}]`,
  PRESS: (_args, getStr) => `PRESS [${getStr('key')}]`,
};

export function synthesizeActionString(name: string, args?: Record<string, Object>): string {
  const normalizedName = name.trim().toUpperCase();
  const synthesizer = actionSynthesizers[normalizedName];
  if (!synthesizer) {
    return '';
  }
  const actionArgs: Record<string, Object> = args || {};
  const getStr = (key: string): string => {
    const value = actionArgs[key];
    return value !== undefined && value !== null ? String(value) : '';
  };
  const has = (key: string): boolean => {
    return actionArgs[key] !== undefined && actionArgs[key] !== null;
  };
  return synthesizer(actionArgs, getStr, has);
}
