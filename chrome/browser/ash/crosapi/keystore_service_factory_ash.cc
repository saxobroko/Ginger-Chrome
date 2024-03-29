// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/keystore_service_factory_ash.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/keystore_service_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace crosapi {

// static
KeystoreServiceAsh* KeystoreServiceFactoryAsh::GetForBrowserContext(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  if (ash::ProfileHelper::IsPrimaryProfile(profile)) {
    // This is the main KeystoreService that is expected to be used most of the
    // time.
    return crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->keystore_service_ash();
  }

  // These services are supposed to cover multi-sign-in scenario and other less
  // common use cases.
  return static_cast<KeystoreServiceAsh*>(
      KeystoreServiceFactoryAsh::GetInstance()->GetServiceForBrowserContext(
          context, /*create=*/true));
}

// static
KeystoreServiceFactoryAsh* KeystoreServiceFactoryAsh::GetInstance() {
  static base::NoDestructor<KeystoreServiceFactoryAsh> factory;
  return factory.get();
}

KeystoreServiceFactoryAsh::KeystoreServiceFactoryAsh()
    : BrowserContextKeyedServiceFactory(
          "KeystoreServiceFactoryAsh",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(chromeos::platform_keys::PlatformKeysServiceFactory::GetInstance());
}

KeyedService* KeystoreServiceFactoryAsh::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new KeystoreServiceAsh(context);
}

}  // namespace crosapi
