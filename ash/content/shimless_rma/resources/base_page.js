// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'base-page' is the main page for the shimless rma process modal dialog.
 */
export class BasePageElement extends PolymerElement {
  static get is() {
    return 'base-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Orientation of the page, either row or column.
       * @type {string}
       */
      orientation: {
        type: String,
        value: "row",
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
  }

  /** @protected */
  headerClass_() {
    return `header-${this.orientation}`;
  }

  /** @protected */
  bodyClass_() {
    return `body-${this.orientation}`;
  }
};

customElements.define(BasePageElement.is, BasePageElement);
