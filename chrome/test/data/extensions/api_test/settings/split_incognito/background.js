// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

['sync', 'local', 'session'].forEach(function(namespace) {
  chrome.storage[namespace].notifications = {};
  chrome.storage.onChanged.addListener(function(changes, event_namespace) {
    if (event_namespace == namespace) {
      var notifications = chrome.storage[namespace].notifications;
      Object.keys(changes).forEach(function(key) {
        notifications[key] = changes[key];
      });
    }
  });
});

// The test from C++ runs "actions", where each action is defined here.
// This allows the test to be tightly controlled between incognito and
// non-incognito modes.
// Each function accepts a callback which should be run when the settings
// operation fully completes.
var testActions = {
  noop: function(callback) {
    this.get("", callback);
  },
  assertEmpty: function(callback) {
    this.get(null, function(settings) {
      chrome.test.assertEq({}, settings);
      callback();
    });
  },
  assertFoo: function(callback) {
    this.get(null, function(settings) {
      chrome.test.assertEq({foo: "bar"}, settings);
      callback();
    });
  },
  setFoo: function(callback) {
    this.set({foo: "bar"}, callback);
  },
  removeFoo: function(callback) {
    this.remove("foo", callback);
  },
  clear: function(callback) {
    this.clear(callback);
  },
  assertNoNotifications: function(callback) {
    chrome.test.assertEq({}, this.notifications);
    callback();
  },
  clearNotifications: function(callback) {
    this.notifications = {};
    callback();
  },
  assertAddFooNotification: function(callback) {
    chrome.test.assertEq({ foo: { newValue: 'bar' } }, this.notifications);
    callback();
  },
  assertDeleteFooNotification: function(callback) {
    chrome.test.assertEq({ foo: { oldValue: 'bar' } }, this.notifications);
    callback();
  }
};

// The only test we run.  Runs "actions" (as defined above) until told
// to stop (when the message has isFinalAction set to true).
function testEverything() {
  function next() {
    var waiting =
        chrome.extension.inIncognitoContext ? "waiting_incognito" : "waiting";
    chrome.test.sendMessage(waiting, function(messageJson) {
      // We will get empty messages, which are considered a noop.
      var message = { action: 'noop', isFinalAction: false, namespace: 'sync' };
      if (messageJson.length != 0) {
        message = JSON.parse(messageJson);
      }
      var action = testActions[message.action];
      if (!action) {
        chrome.test.fail("Unknown action: " + message.action);
        return;
      }
      action.bind(chrome.storage[message.namespace])(
          message.isFinalAction ? chrome.test.succeed : next);
    });
  }
  next();
}

chrome.test.runTests([testEverything]);
