// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {MockEntry} from '../../common/js/mock_entry.m.js';
// #import {FolderShortcutsDataModel} from './folder_shortcuts_data_model.m.js';
// #import {ArrayDataModel} from 'chrome://resources/js/cr/ui/array_data_model.m.js';
// clang-format on

/* #ignore */ 'use strict';

/**
 * Mock FolderShortcutDataModel.
 */
/* #export */ class MockFolderShortcutDataModel extends cr.ui.ArrayDataModel {
  /**
   * @param {!Array} array
   */
  constructor(array) {
    super(array);
  }

  /**
   * @return {!FolderShortcutsDataModel}
   * @public
   */
  asFolderShortcutsDataModel() {
    const instance = /** @type {!Object} */ (this);
    return /** @type {!FolderShortcutsDataModel} */ (instance);
  }

  /**
   * Mock function for FolderShortcutDataModel.compare().
   * @param {MockEntry} a First parameter to be compared.
   * @param {MockEntry} b Second parameter to be compared with.
   * @return {number} Negative if a < b, positive if a > b, or zero if a == b.
   */
  compare(a, b) {
    return a.fullPath.localeCompare(b.fullPath);
  }
}
