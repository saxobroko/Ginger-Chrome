// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigation, Page} from './navigation_helper.js';

/** @polymer */
class ExtensionsSidebarElement extends PolymerElement {
  static get is() {
    return 'extensions-sidebar';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  ready() {
    super.ready();
    this.setAttribute('role', 'navigation');
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.$.sectionMenu.select(
        navigation.getCurrentPage().page === Page.SHORTCUTS ? 1 : 0);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onLinkTap_(e) {
    e.preventDefault();
    navigation.navigateTo({page: e.target.dataset.path});
    this.dispatchEvent(
        new CustomEvent('close-drawer', {bubbles: true, composed: true}));
  }

  /** @private */
  onMoreExtensionsTap_() {
    chrome.metricsPrivate.recordUserAction('Options_GetMoreExtensions');
  }
}

customElements.define(ExtensionsSidebarElement.is, ExtensionsSidebarElement);
