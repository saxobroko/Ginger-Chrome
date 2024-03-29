// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/scanning_app.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {setScanServiceForTesting} from 'chrome://scanning/mojo_interface_provider.js';
import {MAX_NUM_SAVED_SCANNERS, ScannerArr, ScannerSetting, ScanSettings} from 'chrome://scanning/scanning_app_types.js';
import {tokenToString} from 'chrome://scanning/scanning_app_util.js';
import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';

import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible, waitAfterNextRender} from '../../test_util.m.js';

import {changeSelect, createScanner, createScannerSource} from './scanning_app_test_utils.js';
import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

const MY_FILES_PATH = '/home/chronos/user/MyFiles';

// An arbitrary date needed to replace |lastScanDate| in saved scan settings.
const LAST_SCAN_DATE = new Date('1/1/2021');

// Scanner sources names.
const ADF_DUPLEX = 'adf_duplex';
const ADF_SIMPLEX = 'adf_simplex';
const PLATEN = 'platen';

const ColorMode = {
  BLACK_AND_WHITE: ash.scanning.mojom.ColorMode.kBlackAndWhite,
  GRAYSCALE: ash.scanning.mojom.ColorMode.kGrayscale,
  COLOR: ash.scanning.mojom.ColorMode.kColor,
};

const FileType = {
  JPG: ash.scanning.mojom.FileType.kJpg,
  PDF: ash.scanning.mojom.FileType.kPdf,
  PNG: ash.scanning.mojom.FileType.kPng,
};

const PageSize = {
  A4: ash.scanning.mojom.PageSize.kIsoA4,
  Letter: ash.scanning.mojom.PageSize.kNaLetter,
  Max: ash.scanning.mojom.PageSize.kMax,
};

const SourceType = {
  FLATBED: ash.scanning.mojom.SourceType.kFlatbed,
  ADF_SIMPLEX: ash.scanning.mojom.SourceType.kAdfSimplex,
  ADF_DUPLEX: ash.scanning.mojom.SourceType.kAdfDuplex,
};

const firstPageSizes = [PageSize.A4, PageSize.Letter, PageSize.Max];

const secondPageSizes = [PageSize.A4, PageSize.Max];

const firstScannerId =
    /** @type {!mojoBase.mojom.UnguessableToken} */ ({high: 0, low: 1});
const firstScannerName = 'Scanner 1';

const secondScannerId =
    /** @type {!mojoBase.mojom.UnguessableToken} */ ({high: 0, low: 2});
const secondScannerName = 'Scanner 2';

const firstCapabilities = {
  sources: [
    createScannerSource(SourceType.ADF_DUPLEX, ADF_DUPLEX, firstPageSizes),
    createScannerSource(SourceType.FLATBED, PLATEN, firstPageSizes),
  ],
  colorModes: [ColorMode.BLACK_AND_WHITE, ColorMode.COLOR],
  resolutions: [75, 100, 300]
};

const secondCapabilities = {
  sources: [
    createScannerSource(SourceType.ADF_DUPLEX, ADF_DUPLEX, secondPageSizes),
    createScannerSource(SourceType.ADF_SIMPLEX, ADF_SIMPLEX, secondPageSizes),
  ],
  colorModes: [ColorMode.BLACK_AND_WHITE, ColorMode.GRAYSCALE],
  resolutions: [150, 600]
};

/** @implements {ash.scanning.mojom.ScanServiceInterface} */
class FakeScanService {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /** @private {!ScannerArr} */
    this.scanners_ = [];

    /**
     * @private {!Map<!mojoBase.mojom.UnguessableToken,
     *     !ash.scanning.mojom.ScannerCapabilities>}
     */
    this.capabilities_ = new Map();

    /** @private {?ash.scanning.mojom.ScanJobObserverRemote} */
    this.scanJobObserverRemote_ = null;

    /** @private {boolean} */
    this.failStartScan_ = false;

    this.resetForTest();
  }

  resetForTest() {
    this.scanners_ = [];
    this.capabilities_ = new Map();
    this.scanJobObserverRemote_ = null;
    this.failStartScan_ = false;
    this.resolverMap_.set('getScanners', new PromiseResolver());
    this.resolverMap_.set('getScannerCapabilities', new PromiseResolver());
    this.resolverMap_.set('startScan', new PromiseResolver());
    this.resolverMap_.set('cancelScan', new PromiseResolver());
  }

  /**
   * @param {string} methodName
   * @return {!PromiseResolver}
   * @private
   */
  getResolver_(methodName) {
    let method = this.resolverMap_.get(methodName);
    assertTrue(!!method, `Method '${methodName}' not found.`);
    return method;
  }

  /**
   * @param {string} methodName
   * @protected
   */
  methodCalled(methodName) {
    this.getResolver_(methodName).resolve();
  }

  /**
   * @param {string} methodName
   * @return {!Promise}
   */
  whenCalled(methodName) {
    return this.getResolver_(methodName).promise.then(() => {
      // Support sequential calls to whenCalled() by replacing the promise.
      this.resolverMap_.set(methodName, new PromiseResolver());
    });
  }

  /** @param {!ScannerArr} scanners */
  setScanners(scanners) {
    this.scanners_ = scanners;
  }

  /** @param {ash.scanning.mojom.Scanner} scanner */
  addScanner(scanner) {
    this.scanners_ = this.scanners_.concat(scanner);
  }

  /**
   * @param {!Map<!mojoBase.mojom.UnguessableToken,
   *     !ash.scanning.mojom.ScannerCapabilities>} capabilities
   */
  setCapabilities(capabilities) {
    this.capabilities_ = capabilities;
  }

  /** @param {boolean} failStartScan */
  setFailStartScan(failStartScan) {
    this.failStartScan_ = failStartScan;
  }

  /**
   * @param {number} pageNumber
   * @param {number} progressPercent
   * @return {!Promise}
   */
  simulateProgress(pageNumber, progressPercent) {
    this.scanJobObserverRemote_.onPageProgress(pageNumber, progressPercent);
    return flushTasks();
  }

  /**
   * @param {number} pageNumber
   * @return {!Promise}
   */
  simulatePageComplete(pageNumber) {
    this.scanJobObserverRemote_.onPageProgress(pageNumber, 100);
    const fakePageData = [2, 57, 13, 289];
    this.scanJobObserverRemote_.onPageComplete(fakePageData);
    return flushTasks();
  }

  /**
   * @param {!ash.scanning.mojom.ScanResult} result
   * @param {!Array<!mojoBase.mojom.FilePath>} scannedFilePaths
   * @return {!Promise}
   */
  simulateScanComplete(result, scannedFilePaths) {
    this.scanJobObserverRemote_.onScanComplete(result, scannedFilePaths);
    return flushTasks();
  }

  /**
   * @param {boolean} success
   * @return {!Promise}
   */
  simulateCancelComplete(success) {
    this.scanJobObserverRemote_.onCancelComplete(success);
    return flushTasks();
  }

  // scanService methods:

  /** @return {!Promise<{scanners: !ScannerArr}>} */
  getScanners() {
    return new Promise(resolve => {
      this.methodCalled('getScanners');
      resolve({scanners: this.scanners_ || []});
    });
  }

  /**
   * @param {!mojoBase.mojom.UnguessableToken} scanner_id
   * @return {!Promise<{capabilities:
   *    !ash.scanning.mojom.ScannerCapabilities}>}
   */
  getScannerCapabilities(scanner_id) {
    return new Promise(resolve => {
      this.methodCalled('getScannerCapabilities');
      resolve({capabilities: this.capabilities_.get(scanner_id)});
    });
  }

  /**
   * @param {!mojoBase.mojom.UnguessableToken} scanner_id
   * @param {!ash.scanning.mojom.ScanSettings} settings
   * @param {!ash.scanning.mojom.ScanJobObserverRemote} remote
   * @return {!Promise<{success: boolean}>}
   */
  startScan(scanner_id, settings, remote) {
    return new Promise(resolve => {
      this.scanJobObserverRemote_ = remote;
      this.methodCalled('startScan');
      resolve({success: !this.failStartScan_});
    });
  }

  cancelScan() {
    this.methodCalled('cancelScan');
  }
}

export function scanningAppTest() {
  /** @type {?ScanningAppElement} */
  let scanningApp = null;

  /** @type {?FakeScanService} */
  let fakeScanService_ = null;

  /** @type {?TestScanningBrowserProxy} */
  let testBrowserProxy = null;

  /** @type {?HTMLSelectElement} */
  let scannerSelect = null;

  /** @type {?HTMLSelectElement} */
  let sourceSelect = null;

  /** @type {?HTMLSelectElement} */
  let scanToSelect = null;

  /** @type {?HTMLSelectElement} */
  let fileTypeSelect = null;

  /** @type {?HTMLSelectElement} */
  let colorModeSelect = null;

  /** @type {?HTMLSelectElement} */
  let pageSizeSelect = null;

  /** @type {?HTMLSelectElement} */
  let resolutionSelect = null;

  /** @type {?CrButtonElement} */
  let scanButton = null;

  /** @type {?CrButtonElement} */
  let cancelButton = null;

  /** @type {?HTMLElement} */
  let helperText = null;

  /** @type {?HTMLElement} */
  let scanProgress = null;

  /** @type {?HTMLElement} */
  let progressText = null;

  /** @type {?HTMLElement} */
  let progressBar = null;

  /** @type {?HTMLElement} */
  let scannedImages = null;

  /**
   * @type {!Map<!mojoBase.mojom.UnguessableToken,
   *     !ash.scanning.mojom.ScannerCapabilities>}
   */
  const capabilities = new Map();
  capabilities.set(firstScannerId, firstCapabilities);
  capabilities.set(secondScannerId, secondCapabilities);

  /** @type {!ScannerArr} */
  const expectedScanners = [
    createScanner(firstScannerId, firstScannerName),
    createScanner(secondScannerId, secondScannerName)
  ];

  suiteSetup(() => {
    fakeScanService_ = new FakeScanService();
    setScanServiceForTesting(fakeScanService_);
    testBrowserProxy = new TestScanningBrowserProxy();
    ScanningBrowserProxyImpl.instance_ = testBrowserProxy;
    testBrowserProxy.setMyFilesPath(MY_FILES_PATH);
  });

  setup(function() {
    document.body.innerHTML = '';
  });

  teardown(function() {
    fakeScanService_.resetForTest();
    if (scanningApp) {
      scanningApp.remove();
    }
    scanningApp = null;
    scannerSelect = null;
    sourceSelect = null;
    scanToSelect = null;
    fileTypeSelect = null;
    colorModeSelect = null;
    pageSizeSelect = null;
    resolutionSelect = null;
    scanButton = null;
    cancelButton = null;
    helperText = null;
    scanProgress = null;
    progressText = null;
    progressBar = null;
    scannedImages = null;
    testBrowserProxy.reset();
  });

  /**
   * @param {!ScannerArr} scanners
   * @param {!Map<!mojoBase.mojom.UnguessableToken,
   *     !ash.scanning.mojom.ScannerCapabilities>} capabilities
   * @return {!Promise}
   */
  function initializeScanningApp(scanners, capabilities) {
    fakeScanService_.setScanners(scanners);
    fakeScanService_.setCapabilities(capabilities);
    scanningApp = /** @type {!ScanningAppElement} */ (
        document.createElement('scanning-app'));
    document.body.appendChild(scanningApp);
    assertTrue(!!scanningApp);
    assertTrue(isVisible(
        /** @type {!HTMLElement} */ (scanningApp.$$('loading-page'))));
    return fakeScanService_.whenCalled('getScanners');
  }

  /**
   * Returns the "More settings" button.
   * @return {!CrButtonElement}
   */
  function getMoreSettingsButton() {
    const button =
        /** @type {!CrButtonElement} */ (scanningApp.$$('#moreSettingsButton'));
    assertTrue(!!button);
    return button;
  }

  /**
   * Clicks the "More settings" button.
   * @return {!Promise}
   */
  function clickMoreSettingsButton() {
    getMoreSettingsButton().click();
    return flushTasks();
  }

  /**
   * Clicks the "Done" button.
   * @return {!Promise}
   */
  function clickDoneButton() {
    const button = scanningApp.$$('scan-done-section').$$('#doneButton');
    assertTrue(!!button);
    button.click();
    return flushTasks();
  }

  /**
   * Clicks the "Ok" button to close the scan failed dialog.
   * @return {!Promise}
   */
  function clickOkButton() {
    const button = scanningApp.$$('#okButton');
    assertTrue(!!button);
    button.click();
    return flushTasks();
  }

  /**
   * Returns whether the "More settings" section is expanded or not.
   * @return {boolean}
   */
  function isSettingsOpen() {
    return scanningApp.$$('#collapse').opened;
  }

  /**
   * Fetches capabilities then waits for app to change to READY state.
   * @return {!Promise}
   */
  function getScannerCapabilities() {
    return fakeScanService_.whenCalled('getScannerCapabilities').then(() => {
      return waitAfterNextRender(/** @type {!HTMLElement} */ (scanningApp));
    });
  }

  /**
   * Deep equals two ScanSettings objects.
   * @param {!ScanSettings} expectedScanSettings
   * @param {!ScanSettings} actualScanSettings
   */
  function compareSavedScanSettings(expectedScanSettings, actualScanSettings) {
    assertEquals(
        expectedScanSettings.lastUsedScannerName,
        actualScanSettings.lastUsedScannerName);
    assertEquals(
        expectedScanSettings.scanToPath, actualScanSettings.scanToPath);

    // Replace |lastScanDate|, which is a current date timestamp, with a fixed
    // date so assertArrayEquals() can be used.
    expectedScanSettings.scanners.forEach(
        scanner => scanner.lastScanDate = LAST_SCAN_DATE);
    actualScanSettings.scanners.forEach(
        scanner => scanner.lastScanDate = LAST_SCAN_DATE);
    assertArrayEquals(
        expectedScanSettings.scanners, actualScanSettings.scanners);
  }

  // Verify a full scan job can be completed.
  test('Scan', () => {
    /** @type {!Array<!mojoBase.mojom.FilePath>} */
    const scannedFilePaths =
        [{'path': '/test/path/scan1.jpg'}, {'path': '/test/path/scan2.jpg'}];

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('loading-page'))));

          scannerSelect = scanningApp.$$('#scannerSelect').$$('select');
          sourceSelect = scanningApp.$$('#sourceSelect').$$('select');
          scanToSelect = scanningApp.$$('#scanToSelect').$$('select');
          fileTypeSelect = scanningApp.$$('#fileTypeSelect').$$('select');
          colorModeSelect = scanningApp.$$('#colorModeSelect').$$('select');
          pageSizeSelect = scanningApp.$$('#pageSizeSelect').$$('select');
          resolutionSelect = scanningApp.$$('#resolutionSelect').$$('select');
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          cancelButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#cancelButton'));
          helperText = scanningApp.$$('#scanPreview').$$('#helperText');
          scanProgress = scanningApp.$$('#scanPreview').$$('#scanProgress');
          progressText = scanningApp.$$('#scanPreview').$$('#progressText');
          progressBar = scanningApp.$$('#scanPreview').$$('paper-progress');
          scannedImages = scanningApp.$$('#scanPreview').$$('#scannedImages');
          return getScannerCapabilities();
        })
        .then(() => {
          assertEquals(
              tokenToString(firstScannerId), scanningApp.selectedScannerId);
          // A scanner with type "FLATBED" will be used as the selectedSource
          // if it exists.
          assertEquals(
              firstCapabilities.sources[1].name, scanningApp.selectedSource);
          assertEquals(MY_FILES_PATH, scanningApp.selectedFilePath);
          assertEquals(FileType.PDF.toString(), scanningApp.selectedFileType);
          assertEquals(
              ColorMode.COLOR.toString(), scanningApp.selectedColorMode);
          assertEquals(
              firstCapabilities.sources[0].pageSizes[1].toString(),
              scanningApp.selectedPageSize);
          assertEquals(
              firstCapabilities.resolutions[0].toString(),
              scanningApp.selectedResolution);

          // Before the scan button is clicked, the settings and scan button
          // should be enabled, and the helper text should be displayed.
          assertFalse(scannerSelect.disabled);
          assertFalse(sourceSelect.disabled);
          assertFalse(scanToSelect.disabled);
          assertFalse(fileTypeSelect.disabled);
          assertFalse(colorModeSelect.disabled);
          assertFalse(pageSizeSelect.disabled);
          assertFalse(resolutionSelect.disabled);
          assertFalse(scanButton.disabled);
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
          assertFalse(
              isVisible(/** @type {!CrButtonElement} */ (cancelButton)));
          assertTrue(isVisible(/** @type {!HTMLElement} */ (helperText)));
          assertFalse(isVisible(/** @type {!HTMLElement} */ (scanProgress)));
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('scan-done-section'))));

          // Click the Scan button and wait till the scan is started.
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          // After the scan button is clicked and the scan has started, the
          // settings and scan button should be disabled, and the progress bar
          // and text should be visible and indicate that scanning is in
          // progress.
          assertTrue(scannerSelect.disabled);
          assertTrue(sourceSelect.disabled);
          assertTrue(scanToSelect.disabled);
          assertTrue(fileTypeSelect.disabled);
          assertTrue(colorModeSelect.disabled);
          assertTrue(pageSizeSelect.disabled);
          assertTrue(resolutionSelect.disabled);
          assertTrue(scanButton.disabled);
          assertFalse(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (cancelButton)));
          assertFalse(isVisible(/** @type {!HTMLElement} */ (helperText)));
          assertTrue(isVisible(/** @type {!HTMLElement} */ (scanProgress)));
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('scan-done-section'))));
          assertEquals('Scanning page 1', progressText.textContent.trim());
          assertEquals(0, progressBar.value);

          // Simulate a progress update and verify the progress bar and text are
          // updated correctly.
          return fakeScanService_.simulateProgress(1, 17);
        })
        .then(() => {
          assertEquals('Scanning page 1', progressText.textContent.trim());
          assertEquals(17, progressBar.value);

          // Simulate a page complete update and verify the progress bar and
          // text are updated correctly.
          return fakeScanService_.simulatePageComplete(1);
        })
        .then(() => {
          assertEquals('Scanning page 1', progressText.textContent.trim());
          assertEquals(100, progressBar.value);

          // Simulate a progress update for a second page and verify the
          // progress bar and text are updated correctly.
          return fakeScanService_.simulateProgress(2, 53);
        })
        .then(() => {
          assertEquals('Scanning page 2', progressText.textContent.trim());
          assertEquals(53, progressBar.value);

          // Complete the page.
          return fakeScanService_.simulatePageComplete(2);
        })
        .then(() => {
          // Complete the scan.
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kSuccess, scannedFilePaths);
        })
        .then(() => {
          assertTrue(isVisible(/** @type {!HTMLElement} */ (scannedImages)));
          assertEquals(2, scannedImages.querySelectorAll('img').length);
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('scan-done-section'))));
          assertArrayEquals(
              scannedFilePaths,
              scanningApp.$$('scan-done-section').scannedFilePaths);

          // Click the Done button to return to READY state.
          return clickDoneButton();
        })
        .then(() => {
          // After scanning is complete, the settings and scan button should be
          // enabled. The progress bar and text should no longer be visible.
          assertFalse(scannerSelect.disabled);
          assertFalse(sourceSelect.disabled);
          assertFalse(scanToSelect.disabled);
          assertFalse(fileTypeSelect.disabled);
          assertFalse(colorModeSelect.disabled);
          assertFalse(pageSizeSelect.disabled);
          assertFalse(resolutionSelect.disabled);
          assertFalse(scanButton.disabled);
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
          assertFalse(
              isVisible(/** @type {!CrButtonElement} */ (cancelButton)));
          assertTrue(isVisible(/** @type {!HTMLElement} */ (helperText)));
          assertFalse(isVisible(/** @type {!HTMLElement} */ (scanProgress)));
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('scan-done-section'))));
          assertFalse(isVisible(/** @type {!HTMLElement} */ (scannedImages)));
          assertEquals(0, scannedImages.querySelectorAll('img').length);
        });
  });

  // Verify the scan failed dialog shows when a scan job fails.
  test('ScanFailed', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          return getScannerCapabilities();
        })
        .then(() => {
          // Click the Scan button and wait till the scan is started.
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          // Simulate a progress update.
          return fakeScanService_.simulateProgress(1, 17);
        })
        .then(() => {
          // Simulate the scan failing.
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kIoError, []);
        })
        .then(() => {
          // The scan failed dialog should open.
          assertTrue(scanningApp.$$('#scanFailedDialog').open);
          // Click the dialog's Ok button to return to READY state.
          return clickOkButton();
        })
        .then(() => {
          // After the dialog closes, the scan button should be enabled and
          // ready to start a new scan.
          assertFalse(scanningApp.$$('#scanFailedDialog').open);
          assertFalse(scanButton.disabled);
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
        });
  });

  // Verify the correct message is shown in the scan failed dialog based on the
  // error type.
  test('ScanResults', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kUnknownError, []);
        })
        .then(() => {
          assertEquals(
              loadTimeData.getString('scanFailedDialogUnknownErrorText'),
              scanningApp.$$('#scanFailedDialogText').textContent.trim());
          return clickOkButton();
        })
        .then(() => {
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kDeviceBusy, []);
        })
        .then(() => {
          assertEquals(
              loadTimeData.getString('scanFailedDialogDeviceBusyText'),
              scanningApp.$$('#scanFailedDialogText').textContent.trim());
          return clickOkButton();
        })
        .then(() => {
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kAdfJammed, []);
        })
        .then(() => {
          assertEquals(
              loadTimeData.getString('scanFailedDialogAdfJammedText'),
              scanningApp.$$('#scanFailedDialogText').textContent.trim());
          return clickOkButton();
        })
        .then(() => {
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kAdfEmpty, []);
        })
        .then(() => {
          assertEquals(
              loadTimeData.getString('scanFailedDialogAdfEmptyText'),
              scanningApp.$$('#scanFailedDialogText').textContent.trim());
          return clickOkButton();
        })
        .then(() => {
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kFlatbedOpen, []);
        })
        .then(() => {
          assertEquals(
              loadTimeData.getString('scanFailedDialogFlatbedOpenText'),
              scanningApp.$$('#scanFailedDialogText').textContent.trim());
          return clickOkButton();
        })
        .then(() => {
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kIoError, []);
        })
        .then(() => {
          assertEquals(
              loadTimeData.getString('scanFailedDialogIoErrorText'),
              scanningApp.$$('#scanFailedDialogText').textContent.trim());
          return clickOkButton();
        });
  });

  // Verify a scan job can be canceled.
  test('CancelScan', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          cancelButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#cancelButton'));
          return getScannerCapabilities();
        })
        .then(() => {
          // Before the scan button is clicked, the scan button should be
          // visible and enabled, and the cancel button shouldn't be visible.
          assertFalse(scanButton.disabled);
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
          assertFalse(
              isVisible(/** @type {!CrButtonElement} */ (cancelButton)));

          // Click the Scan button and wait till the scan is started.
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          // After the scan button is clicked and the scan has started, the scan
          // button should be disabled and not visible, and the cancel button
          // should be visible.
          assertTrue(scanButton.disabled);
          assertFalse(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (cancelButton)));

          // Simulate a progress update and verify the progress bar and text are
          // updated correctly.
          return fakeScanService_.simulateProgress(1, 17);
        })
        .then(() => {
          // Click the cancel button to cancel the scan.
          cancelButton.click();
          return fakeScanService_.whenCalled('cancelScan');
        })
        .then(() => {
          // Cancel button should be disabled while canceling is in progress.
          assertTrue(cancelButton.disabled);
          // Simulate cancel completing successfully.
          return fakeScanService_.simulateCancelComplete(true);
        })
        .then(() => {
          // After canceling is complete, the scan button should be visible and
          // enabled, and the cancel button shouldn't be visible.
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
          assertFalse(
              isVisible(/** @type {!CrButtonElement} */ (cancelButton)));
          assertTrue(scanningApp.$$('#toast').open);
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#toastInfoIcon'))));
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#getHelpLink'))));
          assertEquals(
              scanningApp.i18n('scanCanceledToastText'),
              scanningApp.$$('#toastText').textContent.trim());
        });
  });

  // Verify the cancel scan failed dialog shows when a scan job fails to cancel.
  test('CancelScanFailed', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          cancelButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#cancelButton'));
          return getScannerCapabilities();
        })
        .then(() => {
          // Click the Scan button and wait till the scan is started.
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          // Simulate a progress update and verify the progress bar and text are
          // updated correctly.
          return fakeScanService_.simulateProgress(1, 17);
        })
        .then(() => {
          // Click the cancel button to cancel the scan.
          cancelButton.click();
          assertFalse(scanningApp.$$('#toast').open);
          return fakeScanService_.whenCalled('cancelScan');
        })
        .then(() => {
          // Cancel button should be disabled while canceling is in progress.
          assertTrue(cancelButton.disabled);
          // Simulate cancel failing.
          return fakeScanService_.simulateCancelComplete(false);
        })
        .then(() => {
          // After canceling fails, the error toast should pop up.
          assertTrue(scanningApp.$$('#toast').open);
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#toastInfoIcon'))));
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#getHelpLink'))));
          assertEquals(
              scanningApp.i18n('cancelFailedToastText'),
              scanningApp.$$('#toastText').textContent.trim());
          // The scan progress page should still be showing with the cancel
          // button visible.
          assertTrue(
              isVisible(scanningApp.$$('#scanPreview').$$('#scanProgress')));
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (cancelButton)));
          assertFalse(
              isVisible(scanningApp.$$('#scanPreview').$$('#helperText')));
          assertFalse(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
        });
  });

  // Verify the scan failed to start toast shows when a scan job fails to start.
  test('ScanFailedToStart', () => {
    fakeScanService_.setFailStartScan(true);

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          return getScannerCapabilities();
        })
        .then(() => {
          assertFalse(scanningApp.$$('#toast').open);
          // Click the Scan button and the scan will fail to start.
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          assertTrue(scanningApp.$$('#toast').open);
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#toastInfoIcon'))));
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#getHelpLink'))));
          assertEquals(
              scanningApp.i18n('startScanFailedToast'),
              scanningApp.$$('#toastText').textContent.trim());

          assertFalse(scanButton.disabled);
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
        });
  });

  // Verify the left and right panel exist on app initialization.
  test('PanelContainerContent', () => {
    return initializeScanningApp(expectedScanners, capabilities).then(() => {
      const panelContainer = scanningApp.$$('#panelContainer');
      assertTrue(!!panelContainer);

      const leftPanel = scanningApp.$$('#panelContainer > #leftPanel');
      const rightPanel = scanningApp.$$('#panelContainer > #rightPanel');

      assertTrue(!!leftPanel);
      assertTrue(!!rightPanel);
    });
  });

  // Verify the more settings toggle behavior.
  test('MoreSettingsToggle', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          // Verify that expandable section is closed by default.
          assertFalse(isSettingsOpen());
          // Expand more settings section.
          return clickMoreSettingsButton();
        })
        .then(() => {
          assertTrue(isSettingsOpen());
          // Close more settings section.
          return clickMoreSettingsButton();
        })
        .then(() => {
          assertFalse(isSettingsOpen());
        });
  });

  // Verify the loading page container is shown when no scanners are available.
  test('NoScanners', () => {
    return initializeScanningApp(/*scanners=*/[], /*capabilities=*/ new Map())
        .then(() => {
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('loading-page'))));
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#panelContainer'))));
        });
  });

  // Verify clicking the retry button loads available scanners.
  test('RetryClickLoadsScanners', () => {
    return initializeScanningApp(/*scanners=*/[], /*capabilities=*/ new Map())
        .then(() => {
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('loading-page'))));
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#panelContainer'))));

          fakeScanService_.setScanners(expectedScanners);
          fakeScanService_.setCapabilities(capabilities);
          scanningApp.$$('loading-page').$$('#retryButton').click();
          return fakeScanService_.whenCalled('getScanners');
        })
        .then(() => {
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('loading-page'))));
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#panelContainer'))));
        });
  });

  // Verify no changes are recorded when a scan job is initiated without any
  // settings changes.
  test('RecordNoSettingChanges', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();
          const numScanSettingChanges =
              testBrowserProxy.getArgs('recordNumScanSettingChanges')[0];
          assertEquals(0, numScanSettingChanges);
        });
  });

  // Verify the correct amount of settings changes are recorded when a scan job
  // is initiated.
  test('RecordSomeSettingChanges', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          return changeSelect(
              scanningApp.$$('#fileTypeSelect').$$('select'),
              FileType.JPG.toString(), /* selectedIndex */ null);
        })
        .then(() => {
          return changeSelect(
              scanningApp.$$('#resolutionSelect').$$('select'), '75',
              /* selectedIndex */ null);
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();
          const numScanSettingChanges =
              testBrowserProxy.getArgs('recordNumScanSettingChanges')[0];
          assertEquals(2, numScanSettingChanges);
        });
  });

  // Verify the correct amount of changes are recorded after the selected
  // scanner is changed then a scan job is initiated.
  test('RecordSettingsWithScannerChange', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          return changeSelect(
              scanningApp.$$('#colorModeSelect').$$('select'),
              ColorMode.BLACK_AND_WHITE.toString(), /* selectedIndex */ null);
        })
        .then(() => {
          return changeSelect(
              scanningApp.$$('#scannerSelect').$$('select'), /* value */ null,
              /* selectedIndex */ 1);
        })
        .then(() => {
          return changeSelect(
              scanningApp.$$('#fileTypeSelect').$$('select'),
              FileType.JPG.toString(), /* selectedIndex */ null);
        })
        .then(() => {
          return changeSelect(
              scanningApp.$$('#resolutionSelect').$$('select'), '150',
              /* selectedIndex */ null);
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();
          const numScanSettingChanges =
              testBrowserProxy.getArgs('recordNumScanSettingChanges')[0];
          assertEquals(3, numScanSettingChanges);
        });
  });

  // Verify the default scan settings are chosen on app load.
  test('DefaultScanSettings', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          assertEquals(
              tokenToString(firstScannerId),
              scanningApp.$$('#scannerSelect').$$('select').value);
          assertEquals(
              PLATEN, scanningApp.$$('#sourceSelect').$$('select').value);
          assertEquals(
              loadTimeData.getString('myFilesSelectOption'),
              scanningApp.$$('#scanToSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.FileType.kPdf.toString(),
              scanningApp.$$('#fileTypeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.ColorMode.kColor.toString(),
              scanningApp.$$('#colorModeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.PageSize.kNaLetter.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
          assertEquals(
              '300', scanningApp.$$('#resolutionSelect').$$('select').value);
        });
  });

  // Verify the first option in each settings dropdown is selected when the
  // default option is not available on the selected scanner.
  test('DefaultScanSettingsNotAvailable', () => {
    return initializeScanningApp(expectedScanners.slice(1), capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          assertEquals(
              tokenToString(secondScannerId),
              scanningApp.$$('#scannerSelect').$$('select').value);
          assertEquals(
              ADF_SIMPLEX, scanningApp.$$('#sourceSelect').$$('select').value);
          assertEquals(
              loadTimeData.getString('myFilesSelectOption'),
              scanningApp.$$('#scanToSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.FileType.kPdf.toString(),
              scanningApp.$$('#fileTypeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.ColorMode.kBlackAndWhite.toString(),
              scanningApp.$$('#colorModeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.PageSize.kIsoA4.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
          assertEquals(
              '600', scanningApp.$$('#resolutionSelect').$$('select').value);
        });
  });

  // Verify the default scan settings are used when saved settings are not
  // available for the selected scanner.
  test('SavedSettingsNotAvailable', () => {
    const savedScanSettings = {
      lastUsedScannerName: 'Wrong Scanner',
      scanToPath: 'scan/to/path',
      scanners: [{
        name: 'Wrong Scanner',
        lastScanDate: new Date(),
        sourceName: ADF_DUPLEX,
        fileType: ash.scanning.mojom.FileType.kPng,
        colorMode: ash.scanning.mojom.ColorMode.kGrayscale,
        pageSize: ash.scanning.mojom.PageSize.kMax,
        resolutionDpi: 100,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          assertEquals(
              tokenToString(firstScannerId),
              scanningApp.$$('#scannerSelect').$$('select').value);
          assertEquals(
              PLATEN, scanningApp.$$('#sourceSelect').$$('select').value);
          assertEquals(
              loadTimeData.getString('myFilesSelectOption'),
              scanningApp.$$('#scanToSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.FileType.kPdf.toString(),
              scanningApp.$$('#fileTypeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.ColorMode.kColor.toString(),
              scanningApp.$$('#colorModeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.PageSize.kNaLetter.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
          assertEquals(
              '300', scanningApp.$$('#resolutionSelect').$$('select').value);
        });
  });

  // Verify saved settings are applied when available for the selected scanner.
  test('ApplySavedSettings', () => {
    if (!loadTimeData.getBoolean('scanAppStickySettingsEnabled')) {
      return;
    }

    const selectedPath = {baseName: 'path', filePath: 'valid/scan/to/path'};
    testBrowserProxy.setSavedSettingsSelectedPath(selectedPath);

    const savedScanSettings = {
      lastUsedScannerName: firstScannerName,
      scanToPath: selectedPath.filePath,
      scanners: [{
        name: firstScannerName,
        lastScanDate: new Date(),
        sourceName: ADF_DUPLEX,
        fileType: ash.scanning.mojom.FileType.kPng,
        colorMode: ash.scanning.mojom.ColorMode.kBlackAndWhite,
        pageSize: ash.scanning.mojom.PageSize.kMax,
        resolutionDpi: 75,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          assertEquals(
              tokenToString(firstScannerId),
              scanningApp.$$('#scannerSelect').$$('select').value);
          assertEquals(
              ADF_DUPLEX, scanningApp.$$('#sourceSelect').$$('select').value);
          assertEquals(
              selectedPath.baseName,
              scanningApp.$$('#scanToSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.FileType.kPng.toString(),
              scanningApp.$$('#fileTypeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.ColorMode.kBlackAndWhite.toString(),
              scanningApp.$$('#colorModeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.PageSize.kMax.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
          assertEquals(
              '75', scanningApp.$$('#resolutionSelect').$$('select').value);
        });
  });

  // Verify if the setting value stored in saved settings is no longer
  // available on the selected scanner, the default setting is chosen.
  test('SettingNotFoundInCapabilities', () => {
    if (!loadTimeData.getBoolean('scanAppStickySettingsEnabled')) {
      return;
    }

    const selectedPath = {baseName: 'path', filePath: 'valid/scan/to/path'};
    testBrowserProxy.setSavedSettingsSelectedPath(selectedPath);

    const savedScanSettings = {
      lastUsedScannerName: firstScannerName,
      scanToPath: 'this/path/does/not/exist',
      scanners: [{
        name: firstScannerName,
        lastScanDate: new Date(),
        sourceName: ADF_SIMPLEX,
        fileType: -1,
        colorMode: ash.scanning.mojom.ColorMode.kGrayscale,
        pageSize: -1,
        resolutionDpi: 600,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          assertEquals(
              tokenToString(firstScannerId),
              scanningApp.$$('#scannerSelect').$$('select').value);
          assertEquals(
              PLATEN, scanningApp.$$('#sourceSelect').$$('select').value);
          assertEquals(
              loadTimeData.getString('myFilesSelectOption'),
              scanningApp.$$('#scanToSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.FileType.kPdf.toString(),
              scanningApp.$$('#fileTypeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.ColorMode.kColor.toString(),
              scanningApp.$$('#colorModeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.PageSize.kNaLetter.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
          assertEquals(
              '300', scanningApp.$$('#resolutionSelect').$$('select').value);
        });
  });

  // Verify the last used scanner is selected from saved settings.
  test('selectLastUsedScanner', () => {
    if (!loadTimeData.getBoolean('scanAppStickySettingsEnabled')) {
      return;
    }

    const savedScanSettings = {
      lastUsedScannerName: secondScannerName,
      scanToPath: 'scan/to/path',
      scanners: [{
        name: secondScannerName,
        lastScanDate: new Date(),
        sourceName: ADF_DUPLEX,
        fileType: ash.scanning.mojom.FileType.kPng,
        colorMode: ash.scanning.mojom.ColorMode.kBlackAndWhite,
        pageSize: ash.scanning.mojom.PageSize.kMax,
        resolutionDpi: 75,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          assertEquals(
              tokenToString(secondScannerId),
              scanningApp.$$('#scannerSelect').$$('select').value);
        });
  });

  // Verify the scan settings are sent to the Pref service to be saved.
  test('saveScanSettings', () => {
    if (!loadTimeData.getBoolean('scanAppStickySettingsEnabled')) {
      return;
    }

    const scannerSetting = {
      name: secondScannerName,
      lastScanDate: LAST_SCAN_DATE,
      sourceName: ADF_DUPLEX,
      fileType: ash.scanning.mojom.FileType.kPng,
      colorMode: ash.scanning.mojom.ColorMode.kBlackAndWhite,
      pageSize: ash.scanning.mojom.PageSize.kMax,
      resolutionDpi: 100,
    };

    const savedScanSettings = {
      lastUsedScannerName: secondScannerName,
      scanToPath: MY_FILES_PATH,
      scanners: [scannerSetting],
    };

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedScannerId = tokenToString(secondScannerId);
          scanningApp.selectedSource = scannerSetting.sourceName;
          scanningApp.selectedFileType = scannerSetting.fileType.toString();
          scanningApp.selectedColorMode = scannerSetting.colorMode.toString();
          scanningApp.selectedPageSize = scannerSetting.pageSize.toString();
          scanningApp.selectedResolution =
              scannerSetting.resolutionDpi.toString();

          scanningApp.$$('#scanButton').click();

          const actualSavedScanSettings = /** @type {!ScanSettings} */
              (JSON.parse(/** @type {string} */ (
                  testBrowserProxy.getArgs('saveScanSettings')[0])));
          compareSavedScanSettings(savedScanSettings, actualSavedScanSettings);
        });
  });

  // Verify that the correct scanner setting is replaced when saving scan
  // settings to the Pref service.
  test('replaceExistingScannerInScanSettings', () => {
    if (!loadTimeData.getBoolean('scanAppStickySettingsEnabled')) {
      return;
    }

    const firstScannerSetting = {
      name: firstScannerName,
      lastScanDate: LAST_SCAN_DATE,
      sourceName: ADF_DUPLEX,
      fileType: ash.scanning.mojom.FileType.kPng,
      colorMode: ash.scanning.mojom.ColorMode.kBlackAndWhite,
      pageSize: ash.scanning.mojom.PageSize.kMax,
      resolutionDpi: 100,
    };

    // The saved scan settings for the second scanner. This is loaded from the
    // Pref service when Scan app is initialized and sets the initial scan
    // settings.
    const initialSecondScannerSetting = {
      name: secondScannerName,
      lastScanDate: LAST_SCAN_DATE,
      sourceName: ADF_DUPLEX,
      fileType: ash.scanning.mojom.FileType.kPng,
      colorMode: ash.scanning.mojom.ColorMode.kBlackAndWhite,
      pageSize: ash.scanning.mojom.PageSize.kMax,
      resolutionDpi: 100,
    };

    const savedScanSettings = {
      lastUsedScannerName: secondScannerName,
      scanToPath: MY_FILES_PATH,
      scanners: [firstScannerSetting, initialSecondScannerSetting],
    };

    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    // The new scan settings for the second scanner that will replace the
    // initial scan settings.
    const newSecondScannerSetting = {
      name: secondScannerName,
      lastScanDate: LAST_SCAN_DATE,
      sourceName: ADF_SIMPLEX,
      fileType: ash.scanning.mojom.FileType.kJpg,
      colorMode: ash.scanning.mojom.ColorMode.kGrayscale,
      pageSize: ash.scanning.mojom.PageSize.kIsoA4,
      resolutionDpi: 600,
    };
    savedScanSettings.scanners[1] = newSecondScannerSetting;

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedScannerId = tokenToString(secondScannerId);
          scanningApp.selectedSource = newSecondScannerSetting.sourceName;
          scanningApp.selectedFileType =
              newSecondScannerSetting.fileType.toString();
          scanningApp.selectedColorMode =
              newSecondScannerSetting.colorMode.toString();
          scanningApp.selectedPageSize =
              newSecondScannerSetting.pageSize.toString();
          scanningApp.selectedResolution =
              newSecondScannerSetting.resolutionDpi.toString();

          scanningApp.$$('#scanButton').click();

          const actualSavedScanSettings = /** @type {!ScanSettings} */
              (JSON.parse(/** @type {string} */ (
                  testBrowserProxy.getArgs('saveScanSettings')[0])));
          compareSavedScanSettings(savedScanSettings, actualSavedScanSettings);
        });
  });

  // Verify that the correct scanner gets evicted when there are too many
  // scanners in saved scan settings.
  test('evictScannersOverTheMaxLimit', () => {
    if (!loadTimeData.getBoolean('scanAppStickySettingsEnabled')) {
      return;
    }

    const scannerToEvict = {
      name: secondScannerName,
      lastScanDate: '1/1/2021',
      sourceName: ADF_DUPLEX,
      fileType: ash.scanning.mojom.FileType.kPng,
      colorMode: ash.scanning.mojom.ColorMode.kBlackAndWhite,
      pageSize: ash.scanning.mojom.PageSize.kMax,
      resolutionDpi: 100,
    };

    // Create an identical scanner with `lastScanDate` set to infinity so it
    // will always have a later `lastScanDate` than |scannerToEvict|.
    const scannerToKeep = Object.assign({}, scannerToEvict);
    scannerToKeep.lastScanDate = '9999-12-31T08:00:00.000Z';
    assertTrue(
        new Date(scannerToKeep.lastScanDate) >
        new Date(scannerToEvict.lastScanDate));

    /** @type {!Array<!ScannerSetting>} */
    const scannersToKeep =
        new Array(MAX_NUM_SAVED_SCANNERS).fill(scannerToKeep);

    // Put |scannerToEvict| in the front of |scannersToKeep| to test that it
    // get correctly sorted to the back of the array when evicting scanners.
    const savedScanSettings = {
      lastUsedScannerName: secondScannerName,
      scanToPath: MY_FILES_PATH,
      scanners: [scannerToEvict].concat(scannersToKeep),
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();

          const actualSavedScanSettings = /** @type {!ScanSettings} */
              (JSON.parse(/** @type {string} */ (
                  testBrowserProxy.getArgs('saveScanSettings')[0])));
          assertEquals(
              MAX_NUM_SAVED_SCANNERS, actualSavedScanSettings.scanners.length);
          assertArrayEquals(scannersToKeep, actualSavedScanSettings.scanners);
        });
  });

  // Verify that no scanners get evicted when the number of scanners in saved
  // scan settings is equal to |MAX_NUM_SAVED_SCANNERS|.
  test('doNotEvictScannersAtMax', () => {
    if (!loadTimeData.getBoolean('scanAppStickySettingsEnabled')) {
      return;
    }

    /** @type {!Array<!ScannerSetting>} */
    const scanners = new Array(MAX_NUM_SAVED_SCANNERS);
    for (let i = 0; i < MAX_NUM_SAVED_SCANNERS; i++) {
      scanners[i] = {
        name: 'Scanner ' + (i + 1),
        lastScanDate: new Date(new Date().getTime() + i),
        sourceName: ADF_DUPLEX,
        fileType: ash.scanning.mojom.FileType.kPng,
        colorMode: ash.scanning.mojom.ColorMode.kBlackAndWhite,
        pageSize: ash.scanning.mojom.PageSize.kMax,
        resolutionDpi: 300,
      };
    }

    const savedScanSettings = {
      lastUsedScannerName: firstScannerName,
      scanToPath: MY_FILES_PATH,
      scanners: scanners,
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();

          const actualSavedScanSettings = /** @type {!ScanSettings} */
              (JSON.parse(/** @type {string} */ (
                  testBrowserProxy.getArgs('saveScanSettings')[0])));
          assertEquals(
              MAX_NUM_SAVED_SCANNERS, actualSavedScanSettings.scanners.length);
          compareSavedScanSettings(savedScanSettings, actualSavedScanSettings);
        });
  });
}
