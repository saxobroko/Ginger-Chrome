// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/test/web_engine_browser_test.h"

#include <fuchsia/mediacodec/cpp/fidl_test_base.h>

#include "base/files/file_path.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/test/frame_test_util.h"
#include "fuchsia/base/test/test_navigation_listener.h"
#include "fuchsia/engine/features.h"
#include "fuchsia/engine/test/test_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// Currently, VP8 can only be decoded in software, and VP9 can be decoded in
// hardware and software. The tests rely on this.
constexpr char kLoadSoftwareOnlyCodecUrl[] = "/play_video.html?codecs=vp8";
constexpr char kLoadHardwareAndSoftwareCodecUrl[] =
    "/play_video.html?codecs=vp9";
constexpr char kCanPlaySoftwareOnlyCodecUrl[] = "/can_play_vp8.html";

}  // namespace

class MediaTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  MediaTest() {
    set_test_server_root(base::FilePath(cr_fuchsia::kTestServerRoot));
  }
  ~MediaTest() override = default;

  MediaTest(const MediaTest&) = delete;
  MediaTest& operator=(const MediaTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    CHECK(embedded_test_server()->Start());
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
  }

  // Creates a Frame with |navigation_listener_| attached.
  fuchsia::web::FramePtr CreateFrame() {
    return WebEngineBrowserTest::CreateFrame(&navigation_listener_);
  }

  cr_fuchsia::TestNavigationListener navigation_listener_;
};

using SoftwareOnlyDecodersEnabledTest = MediaTest;

// MediaTest with kEnableSoftwareOnlyVideoCodecs disabled.
class SoftwareOnlyDecodersDisabledTest : public MediaTest {
 public:
  SoftwareOnlyDecodersDisabledTest() = default;
  ~SoftwareOnlyDecodersDisabledTest() override = default;

 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        features::kEnableSoftwareOnlyVideoCodecs);
    MediaTest::SetUp();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// SoftwareOnlyDecodersDisabledTest with fuchsia.mediacodec.CodecFactory
// disconnected.
class SoftwareOnlyDecodersDisabledAndHardwareDecoderFailureTest
    : public SoftwareOnlyDecodersDisabledTest {
 public:
  SoftwareOnlyDecodersDisabledAndHardwareDecoderFailureTest() = default;
  ~SoftwareOnlyDecodersDisabledAndHardwareDecoderFailureTest() override =
      default;

 protected:
  // Removes the decoder service to cause calls to it to fail.
  void SetUpOnMainThread() override {
    component_context_.emplace(
        base::TestComponentContextForProcess::InitialState::kCloneAll);
    component_context_->additional_services()
        ->RemovePublicService<fuchsia::mediacodec::CodecFactory>();

    SoftwareOnlyDecodersDisabledTest::SetUpOnMainThread();
  }

  // Used to disconnect fuchsia.mediacodec.CodecFactory.
  absl::optional<base::TestComponentContextForProcess> component_context_;
};

// Verify that a codec only supported by a software decoder is reported as
// playable if kEnableSoftwareOnlyVideoCodecs is enabled.
IN_PROC_BROWSER_TEST_F(SoftwareOnlyDecodersEnabledTest,
                       CanPlayTypeSoftwareOnlyCodecIsTrue) {
  const GURL kUrl(embedded_test_server()->GetURL(kCanPlaySoftwareOnlyCodecUrl));

  // TODO(crbug.com/1200314): Refactor these tests to use FrameForTest and
  // possibly to simplify the calls below since some of the details are not
  // interesting to the individual tests. In particular, has_user_activation is
  // more relevant than speclific LoadUrlParams.
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "can play vp8: true");
}

// Verify that a codec only supported by a software decoder is reported as not
// playable if kEnableSoftwareOnlyVideoCodecs is disabled.
IN_PROC_BROWSER_TEST_F(SoftwareOnlyDecodersDisabledTest,
                       CanPlayTypeSoftwareOnlyCodecIsFalse) {
  const GURL kUrl(embedded_test_server()->GetURL(kCanPlaySoftwareOnlyCodecUrl));

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "can play vp8: false");
}

// Verify that a codec only supported by a software decoder is loaded if
// kEnableSoftwareOnlyVideoCodecs is enabled.
IN_PROC_BROWSER_TEST_F(SoftwareOnlyDecodersEnabledTest,
                       PlaySoftwareOnlyCodecSucceeds) {
  const GURL kUrl(embedded_test_server()->GetURL(kLoadSoftwareOnlyCodecUrl));

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "loaded");
}

// Verify that a codec only supported by a software decoder is not loaded if
// kEnableSoftwareOnlyVideoCodecs is disabled.
IN_PROC_BROWSER_TEST_F(SoftwareOnlyDecodersDisabledTest,
                       LoadSoftwareOnlyCodecFails) {
  const GURL kUrl(embedded_test_server()->GetURL(kLoadSoftwareOnlyCodecUrl));

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "media element error");
}

// Verify that a codec supported by hardware and software decoders plays if
// kEnableSoftwareOnlyVideoCodecs is disabled.
// Unlike the software-only codec, this codec loads because it is supposed to be
// supported. It then plays because a hardware decoder is used (when on real
// hardware) or it falls back to the software decoder (i.e., on emulator).
IN_PROC_BROWSER_TEST_F(SoftwareOnlyDecodersDisabledTest,
                       PlayHardwareAndSoftwareCodecSucceeds) {
  const GURL kUrl(
      embedded_test_server()->GetURL(kLoadHardwareAndSoftwareCodecUrl));

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), cr_fuchsia::CreateLoadUrlParamsWithUserActivation(),
      kUrl.spec()));

  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "loaded");
  cr_fuchsia::ExecuteJavaScript(frame.get(), "bear.play()");

  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "playing");
}

// Verify that a codec supported by hardware and software plays if
// kEnableSoftwareOnlyVideoCodecs is disabled and the hardware decoder fails.
// Unlike the software-only codec, this codec loads because it is supposed to be
// supported. It then plays because it falls back to the software decoder.
IN_PROC_BROWSER_TEST_F(
    SoftwareOnlyDecodersDisabledAndHardwareDecoderFailureTest,
    PlayHardwareAndSoftwareCodecFails) {
  const GURL kUrl(
      embedded_test_server()->GetURL(kLoadHardwareAndSoftwareCodecUrl));

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), cr_fuchsia::CreateLoadUrlParamsWithUserActivation(),
      kUrl.spec()));

  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "loaded");
  cr_fuchsia::ExecuteJavaScript(frame.get(), "bear.play()");

  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "playing");
}
