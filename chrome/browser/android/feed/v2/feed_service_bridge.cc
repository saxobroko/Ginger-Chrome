// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/v2/feed_service_bridge.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/time/time.h"
#include "chrome/browser/android/feed/v2/feed_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feed/android/jni_headers/FeedServiceBridge_jni.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/feed_feature_list.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"

namespace feed {
namespace {

FeedService* GetFeedService() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return nullptr;
  return FeedServiceFactory::GetForBrowserContext(profile);
}

FeedApi* GetFeedApi() {
  FeedService* service = GetFeedService();
  if (!service)
    return nullptr;
  return service->GetStream();
}

}  // namespace

static jboolean JNI_FeedServiceBridge_IsEnabled(JNIEnv* env) {
  return FeedServiceBridge::IsEnabled();
}

static void JNI_FeedServiceBridge_Startup(JNIEnv* env) {
  // Trigger creation FeedService, since we need to handle certain browser
  // events, like sign-in/sign-out, even if the Feed isn't visible.
  GetFeedService();
}

static int JNI_FeedServiceBridge_GetLoadMoreTriggerLookahead(JNIEnv* env) {
  return GetFeedConfig().load_more_trigger_lookahead;
}

static int JNI_FeedServiceBridge_GetLoadMoreTriggerScrollDistanceDp(
    JNIEnv* env) {
  return GetFeedConfig().load_more_trigger_scroll_distance_dp;
}

static void JNI_FeedServiceBridge_ReportOpenVisitComplete(JNIEnv* env,
                                                          jlong visitTimeMs) {
  FeedApi* api = GetFeedApi();
  if (!api)
    return;
  api->ReportOpenVisitComplete(base::TimeDelta::FromMilliseconds(visitTimeMs));
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_FeedServiceBridge_GetClientInstanceId(JNIEnv* env) {
  std::string instance_id;
  FeedApi* api = GetFeedApi();
  if (api) {
    instance_id = api->GetClientInstanceId();
  }
  return base::android::ConvertUTF8ToJavaString(env, instance_id);
}

static int JNI_FeedServiceBridge_GetVideoPreviewsTypePreference(JNIEnv* env) {
  PrefService* pref_service = ProfileManager::GetLastUsedProfile()->GetPrefs();
  return pref_service->GetInteger(feed::prefs::kVideoPreviewsType);
}

static void JNI_FeedServiceBridge_SetVideoPreviewsTypePreference(JNIEnv* env,
                                                                 jint setting) {
  PrefService* pref_service = ProfileManager::GetLastUsedProfile()->GetPrefs();
  pref_service->SetInteger(feed::prefs::kVideoPreviewsType, setting);
}

static jlong JNI_FeedServiceBridge_GetReliabilityLoggingId(JNIEnv* env) {
  return FeedServiceBridge::GetReliabilityLoggingId();
}

static jboolean JNI_FeedServiceBridge_IsAutoplayEnabled(JNIEnv* env) {
  return FeedServiceBridge::IsAutoplayEnabled();
}

static jlong JNI_FeedServiceBridge_AddUnreadContentObserver(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_observer,
    jboolean is_web_feed) {
  FeedApi* api = GetFeedApi();
  if (!api)
    return 0;
  JavaUnreadContentObserver* observer = new JavaUnreadContentObserver(
      base::android::ScopedJavaGlobalRef<jobject>(j_observer));
  api->AddUnreadContentObserver(is_web_feed ? kWebFeedStream : kForYouStream,
                                observer);
  return reinterpret_cast<jlong>(observer);
}

std::string FeedServiceBridge::GetLanguageTag() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return ConvertJavaStringToUTF8(env,
                                 Java_FeedServiceBridge_getLanguageTag(env));
}

DisplayMetrics FeedServiceBridge::GetDisplayMetrics() {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<double> numbers;
  base::android::JavaDoubleArrayToDoubleVector(
      env, Java_FeedServiceBridge_getDisplayMetrics(env), &numbers);
  DCHECK_EQ(3UL, numbers.size());
  DisplayMetrics result;
  result.density = numbers[0];
  result.width_pixels = numbers[1];
  result.height_pixels = numbers[2];
  return result;
}

bool FeedServiceBridge::IsAutoplayEnabled() {
  // For now, disable autoplay if metrics are disabled until we can ensure that
  // the autoplay feature does not report metrics.
  return base::FeatureList::IsEnabled(kInterestFeedV2Autoplay) &&
         ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}

void FeedServiceBridge::ClearAll() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedServiceBridge_clearAll(env);
}

bool FeedServiceBridge::IsEnabled() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  return FeedService::IsEnabled(*profile->GetPrefs());
}

void FeedServiceBridge::PrefetchImage(const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedServiceBridge_prefetchImage(
      env, base::android::ConvertUTF8ToJavaString(env, url.spec()));
}

uint64_t FeedServiceBridge::GetReliabilityLoggingId() {
  PrefService* profile_prefs = ProfileManager::GetLastUsedProfile()->GetPrefs();
  if (!g_browser_process->metrics_service()) {
    // If for some reason we don't have the metrics client ID, an ID based only
    // on the random "salt" will be generated.
    return FeedService::GetReliabilityLoggingId(/*metrics_id=*/std::string(),
                                                profile_prefs);
  }
  return FeedService::GetReliabilityLoggingId(
      g_browser_process->metrics_service()->GetClientId(), profile_prefs);
}

JavaUnreadContentObserver::JavaUnreadContentObserver(
    base::android::ScopedJavaGlobalRef<jobject> j_observer)
    : obj_(j_observer) {}

feed::JavaUnreadContentObserver::~JavaUnreadContentObserver() = default;

void JavaUnreadContentObserver::HasUnreadContentChanged(
    bool has_unread_content) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UnreadContentObserver_hasUnreadContentChanged(env, obj_,
                                                     has_unread_content);
}

void JavaUnreadContentObserver::Destroy(JNIEnv*) {
  delete this;
}

}  // namespace feed
