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

export const guidelines = `
## General:
1) The user's instruction might be not exactly accurate, there might be typos, approximate descriptions, or abbreviations.
2) Base all conclusions strictly on the provided inputs. Do not assume or hallucinate elements, states, or outcomes.

## About Reason and ACTION:
1) If you want to search or input something, directly use SEARCH or FILL. DO NOT click on the input bar.
2) When there are multiple search bars, think carefully which one you should interact with.
3) Be careful to use GO_BACK. GO_BACK is NOT an undo operation. Use GO_BACK only when you confirm that the second webpage in the tab stack (from the top) is the target webpage that you want to go.
4) Be careful when the same action was performed continuously (check the history). You may have chosen the wrong action or control.
5) Observe the scroll bar on the right of the screen before trying to scroll. You must not scroll down if the scroll bar is at the bottom.
6) When you searched something but got no matching results, try relaxing the search term by searching only part of it.
7) To select an option from a SELECT element, you may use SELECT (e.g. for a SELECT element with options ["Views", "Price"], use SELECT [10] [Price]). Do not click on the SELECT element. However, if the dropdown menu is not a SELECT element (e.g. button), you must NOT use SELECT on it.
8) Using a search bar is normally more efficient than scrolling down.

## About take note:
1) Just because the page contains visible information required by the subtask does not mean it will be automatically recorded in the Notes. You need to take note (keeping the information in its original language) if the information has not been collected in the Notes.
2) Take note is NOT an Action. Do NOT output take_note in the Action section.

## About observation:
1) When observing the page, notice whether the information you need is fully or partially displayed. If it is partially displayed, observe how you may fully display it (e.g. controls like "see more" or pagination).
2) If something you expect is not shown on the screen, think if it is outside the screen. Pay attention to the location of the scroll bar on the right.
`;

export const actionNoTakeNote = vConfig.parallel
  ? '3) Take note is NOT an action. Do NOT output take_note in the output Action section.'
  : '';

export const guidelinesWithPlan = `
## About ACTION:
1) If you want to search or input something, directly use SEARCH or FILL. DO NOT click on the input bar.
2) When there are multiple search bars, think carefully which one you should interact with.
3) Be careful to use GO_BACK. GO_BACK is NOT an undo operation. Use GO_BACK only when you confirm that the second webpage in the tab stack (from the top) is the target webpage that you want to go.
4) Be careful when the same actions were performed continuously (check the history). You may have chosen the wrong action or control.
5) Observe the scroll bar on the right of the screen before trying to scroll. You must not scroll down if the scroll bar is at the bottom.
6) When you search something but get no matching results, try relaxing the search term by searching only part of it.
7) To select an option from a SELECT element, you may use SELECT (e.g. for a SELECT element with options ["Views", "Price"], use SELECT [10] [Price]). Do not click on the SELECT element.

## About take note:
1) Just because the page contains visible information required by the subtask does not mean it will be automatically recorded in the Notes. On the contrary, you need to take note (keeping the information in its original language) if the information has not been collected in the Notes.
${actionNoTakeNote}

## About observation:
1) When observing the page, notice whether the information you need is fully or partially displayed. If it is partially displayed, observe how you may fully display it (e.g. controls like "see more" or pagination).
2) If something you expect is not shown on the screen, think if it is outside the screen. Pay attention to the location of the scroll bar on the right.

## About subtask decompose (break down):
1) When breaking down the subtask, make sure the new tasks together can fully accomplish the current subtask. Do not miss any part of the subtask.
2) When planning future subtasks, DO NOT assume any specific controls of webpages that you have not observed in advance.
3) When breaking down the subtask, notice that you need to record information WHILE taking actions, so DO NOT divide searching for information and recording information into two subtasks. E.g. BAD decompose: ['Search for gloves', 'Read through the goods to find those under $10', 'Record the goods in the notes']. GOOD decompose: ['Search for gloves', 'Read through the goods and record those under $10 in the notes'].

`;
