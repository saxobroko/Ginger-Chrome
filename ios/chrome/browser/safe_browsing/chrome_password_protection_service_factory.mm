// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/chrome_password_protection_service_factory.h"

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/safe_browsing/chrome_password_protection_service.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#import "ios/chrome/browser/sync/profile_sync_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
ChromePasswordProtectionService*
ChromePasswordProtectionServiceFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<ChromePasswordProtectionService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
ChromePasswordProtectionServiceFactory*
ChromePasswordProtectionServiceFactory::GetInstance() {
  static base::NoDestructor<ChromePasswordProtectionServiceFactory> instance;
  return instance.get();
}

ChromePasswordProtectionServiceFactory::ChromePasswordProtectionServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ChromePasswordProtectionService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(IOSChromePasswordStoreFactory::GetInstance());
  DependsOn(IOSUserEventServiceFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ChromePasswordProtectionServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  if (!safe_browsing_service) {
    return nullptr;
  }
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  return std::make_unique<ChromePasswordProtectionService>(
      safe_browsing_service, chrome_browser_state);
}

bool ChromePasswordProtectionServiceFactory::ServiceIsCreatedWithBrowserState()
    const {
  return false;
}

web::BrowserState* ChromePasswordProtectionServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}

bool ChromePasswordProtectionServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
