// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {getFaviconForPageURL} from 'chrome://resources/js/icon.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadLaterApiProxy, ReadLaterApiProxyImpl} from '../read_later_api_proxy.js';

/**
 * Event name for open state of a folder being changed.
 * @const {string}
 */
export const FOLDER_OPEN_CHANGED_EVENT = 'bookmark-folder-open-changed';

export class BookmarkFolderElement extends PolymerElement {
  static get is() {
    return 'bookmark-folder';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      childDepth_: {
        type: Number,
        value: 1,
      },

      depth: {
        type: Number,
        observer: 'onDepthChanged_',
        value: 0,
      },

      /** @type {!chrome.bookmarks.BookmarkTreeNode} */
      folder: Object,

      /** @private */
      open_: {
        type: Boolean,
        value: false,
      },

      /** @type {!Array<string>} */
      openFolders: {
        type: Array,
        observer: 'onOpenFoldersChanged_',
      },
    };
  }

  constructor() {
    super();

    /** @private @const {!ReadLaterApiProxy} */
    this.readLaterApi_ = ReadLaterApiProxyImpl.getInstance();
  }

  /**
   * @param {!Event} event
   * @private
   */
  onBookmarkClick_(event) {
    event.preventDefault();
    this.readLaterApi_.openURL({url: event.model.item.url}, false);
  }

  /**
   * @param {string} url
   * @return {string}
   * @private
   */
  getBookmarkIcon_(url) {
    return getFaviconForPageURL(url, false);
  }

  /** @private */
  onDepthChanged_() {
    this.childDepth_ = this.depth + 1;
    this.style.setProperty('--node-depth', `${this.depth}`);
    this.$.children.style.setProperty('--node-depth', `${this.childDepth_}`);
  }

  /** @private */
  onFolderClick_() {
    if (!this.folder.children.length) {
      // No reason to open if there are no children to show.
      return;
    }

    this.open_ = !this.open_;
    this.dispatchEvent(new CustomEvent(FOLDER_OPEN_CHANGED_EVENT, {
      bubbles: true,
      composed: true,
      detail: {
        id: this.folder.id,
        open: this.open_,
      }
    }));
  }

  /** @private */
  onOpenFoldersChanged_() {
    this.open_ =
        Boolean(this.openFolders) && this.openFolders.includes(this.folder.id);
  }
}

customElements.define(BookmarkFolderElement.is, BookmarkFolderElement);