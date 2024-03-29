// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test cases registered by GUEST_TEST.
 * @type {!Map<string, function(): Promise<undefined>>}
 */
const guestTestCases = new Map();

/**
 * Acts on TestMessageRunTestCase.
 * @param {!TestMessageRunTestCase} data
 * @return {!Promise<!TestMessageResponseData>}
 */
async function runTestCase(data) {
  const testCase = guestTestCases.get(data.testCase);
  if (!testCase) {
    throw new Error(`Unknown test case: '${data.testCase}'`);
  }
  await testCase();  // Propagate exceptions to the MessagePipe handler.
  return {testQueryResult: 'success'};
}

/**
 * Registers a test that runs in the guest context. To indicate failure, the
 * test logs a console error which fails these browser tests.
 * @param {!string} testName
 * @param {!function(): Promise<undefined>} testCase
 */
function GUEST_TEST(testName, testCase) {
  guestTestCases.set(testName, testCase);
}

/**
 * Tells the test driver the guest test message handlers are installed. This
 * requires the test handler that receives the signal to be set up. The order
 * that this occurs can not be guaranteed. So this function retries until the
 * signal is handled, which requires the 'test-handlers-ready' handler to be
 * registered in driver.js.
 */
async function signalTestHandlersReady() {
  const EXPECTED_ERROR =
      `No handler registered for message type 'test-handlers-ready'`;
  while (true) {
    try {
      await parentMessagePipe.sendMessage('test-handlers-ready', {});
      return;
    } catch (/** @type {!GenericErrorResponse} */ e) {
      if (e.message !== EXPECTED_ERROR) {
        console.error('Unexpected error in signalTestHandlersReady', e);
        return;
      }
    }
  }
}

/** Installs the MessagePipe handlers for receiving test queries. */
function installTestHandlers() {
  // Turn off error rethrowing for tests so the test runner doesn't mark
  // our error handling tests as failed.
  parentMessagePipe.rethrowErrors = false;

  parentMessagePipe.registerHandler('run-test-case', (data) => {
    return runTestCase(/** @type {!TestMessageRunTestCase} */ (data));
  });

  // Log errors, rather than send them to console.error. This allows the error
  // handling tests to work correctly, and is also required for
  // signalTestHandlersReady() to operate without failing tests.
  parentMessagePipe.logClientError = error =>
      console.log(JSON.stringify(error));

  signalTestHandlersReady();
}

// Ensure content and all scripts have loaded before installing test handlers.
if (document.readyState !== 'complete') {
  window.addEventListener('load', installTestHandlers);
} else {
  installTestHandlers();
}

//# sourceURL=guest_query_receiver.js
