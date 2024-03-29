// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/browser/content_capture_receiver.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/content_capture/browser/content_capture_consumer.h"
#include "components/content_capture/browser/onscreen_content_provider.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_capture {
namespace {

static constexpr char16_t kMainFrameUrl[] = u"http://foo.com/main.html";
static constexpr char16_t kMainFrameUrl2[] = u"http://foo.com/2.html";
static constexpr char16_t kChildFrameUrl[] = u"http://foo.org/child.html";
static constexpr char16_t kMainFrameSameDocument[] =
    u"http://foo.com/main.html#1";

// Fake ContentCaptureSender to call ContentCaptureReceiver mojom interface.
class FakeContentCaptureSender {
 public:
  FakeContentCaptureSender() {}

  void DidCaptureContent(const ContentCaptureData& captured_content,
                         bool first_data) {
    content_capture_receiver_->DidCaptureContent(captured_content, first_data);
  }

  void DidUpdateContent(const ContentCaptureData& captured_content) {
    content_capture_receiver_->DidUpdateContent(captured_content);
  }

  void DidRemoveContent(const std::vector<int64_t>& data) {
    content_capture_receiver_->DidRemoveContent(data);
  }

  mojo::PendingAssociatedReceiver<mojom::ContentCaptureReceiver>
  GetPendingAssociatedReceiver() {
    return content_capture_receiver_.BindNewEndpointAndPassDedicatedReceiver();
  }

 private:
  mojo::AssociatedRemote<mojom::ContentCaptureReceiver>
      content_capture_receiver_;
};

class SessionRemovedTestHelper {
 public:
  void DidRemoveSession(const ContentCaptureSession& data) {
    removed_sessions_.push_back(data);
  }

  const std::vector<ContentCaptureSession>& removed_sessions() const {
    return removed_sessions_;
  }

  void Reset() { removed_sessions_.clear(); }

 private:
  std::vector<ContentCaptureSession> removed_sessions_;
};

// The helper class implements ContentCaptureConsumer and keeps the
// result for verification.
class ContentCaptureConsumerHelper : public ContentCaptureConsumer {
 public:
  explicit ContentCaptureConsumerHelper(
      SessionRemovedTestHelper* session_removed_test_helper)
      : session_removed_test_helper_(session_removed_test_helper) {}

  void DidCaptureContent(const ContentCaptureSession& parent_session,
                         const ContentCaptureFrame& data) override {
    parent_session_ = parent_session;
    captured_data_ = data;
  }

  void DidUpdateContent(const ContentCaptureSession& parent_session,
                        const ContentCaptureFrame& data) override {
    updated_parent_session_ = parent_session;
    updated_data_ = data;
  }

  void DidRemoveContent(const ContentCaptureSession& session,
                        const std::vector<int64_t>& ids) override {
    session_ = session;
    removed_ids_ = ids;
  }

  void DidRemoveSession(const ContentCaptureSession& data) override {
    if (session_removed_test_helper_)
      session_removed_test_helper_->DidRemoveSession(data);
    removed_sessions_.push_back(data);
  }

  void DidUpdateTitle(const ContentCaptureFrame& main_frame) override {
    updated_title_ = main_frame.title;
  }

  bool ShouldCapture(const GURL& url) override { return false; }

  const ContentCaptureSession& parent_session() const {
    return parent_session_;
  }

  const ContentCaptureSession& updated_parent_session() const {
    return updated_parent_session_;
  }

  const ContentCaptureSession& session() const { return session_; }

  const ContentCaptureFrame& captured_data() const { return captured_data_; }

  const ContentCaptureFrame& updated_data() const { return updated_data_; }

  const std::vector<ContentCaptureSession>& removed_sessions() const {
    return removed_sessions_;
  }

  const std::vector<int64_t>& removed_ids() const { return removed_ids_; }
  const std::u16string& updated_title() const { return updated_title_; }

  void Reset() { removed_sessions_.clear(); }

 private:
  ContentCaptureSession parent_session_;
  ContentCaptureSession updated_parent_session_;
  ContentCaptureSession session_;
  ContentCaptureFrame captured_data_;
  ContentCaptureFrame updated_data_;
  std::vector<int64_t> removed_ids_;
  std::vector<ContentCaptureSession> removed_sessions_;
  SessionRemovedTestHelper* session_removed_test_helper_;
  std::u16string updated_title_;
};

}  // namespace

class ContentCaptureReceiverTest : public content::RenderViewHostTestHarness,
                                   public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    // TODO (crbug.com/1115234): Remove the param when BFCache same site feature
    // launched.
    if (GetParam()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{{features::kBackForwardCache}, {{"enable_same_site", "true"}}}},
          // Allow BackForwardCache for all devices regardless of their memory.
          {features::kBackForwardCacheMemoryControls});
    }
    content::RenderViewHostTestHarness::SetUp();
    onscreen_content_provider_ =
        OnscreenContentProvider::Create(web_contents());

    content_capture_consumer_helper_ =
        std::make_unique<ContentCaptureConsumerHelper>(
            &session_removed_test_helper_);
    onscreen_content_provider_->AddConsumer(
        *(content_capture_consumer_helper_.get()));

    // This needed to keep the WebContentsObserverConsistencyChecker checks
    // happy for when AppendChild is called.
    NavigateAndCommit(GURL(kMainFrameUrl));
    content_capture_sender_ = std::make_unique<FakeContentCaptureSender>();
    main_frame_ = web_contents()->GetMainFrame();
    // Binds sender with receiver.
    OnscreenContentProvider::BindContentCaptureReceiver(
        content_capture_sender_->GetPendingAssociatedReceiver(), main_frame_);

    ContentCaptureData child;
    // Have the unique id for text content.
    child.id = 2;
    child.value = u"Hello";
    child.bounds = gfx::Rect(5, 5, 5, 5);
    // No need to set id in sender.
    test_data_.value = kMainFrameUrl;
    test_data_.bounds = gfx::Rect(10, 10);
    test_data_.children.push_back(child);
    test_data2_.value = kChildFrameUrl;
    test_data2_.bounds = gfx::Rect(10, 10);
    test_data2_.children.push_back(child);

    ContentCaptureData child_change;
    // Same ID with child.
    child_change.id = 2;
    child_change.value = u"Hello World";
    child_change.bounds = gfx::Rect(5, 5, 5, 5);
    test_data_change_.value = kMainFrameUrl;
    test_data_change_.bounds = gfx::Rect(10, 10);
    test_data_change_.children.push_back(child_change);

    // Update to test_data_.
    ContentCaptureData child2;
    // Have the unique id for text content.
    child2.id = 3;
    child2.value = u"World";
    child2.bounds = gfx::Rect(5, 10, 5, 5);
    test_data_update_.value = kMainFrameUrl;
    test_data_update_.bounds = gfx::Rect(10, 10);
    test_data_update_.children.push_back(child2);
  }

  void NavigateMainFrame(const GURL& url) {
    content_capture_consumer_helper()->Reset();
    NavigateAndCommit(url);
    main_frame_ = web_contents()->GetMainFrame();
  }

  void NavigateMainFrameSameDocument() {
    content_capture_consumer_helper()->Reset();
    NavigateAndCommit(GURL(kMainFrameSameDocument));
  }

  void SetupChildFrame() {
    child_content_capture_sender_ =
        std::make_unique<FakeContentCaptureSender>();
    child_frame_ =
        content::RenderFrameHostTester::For(main_frame_)->AppendChild("child");
    // Binds sender with receiver for child frame.
    OnscreenContentProvider::BindContentCaptureReceiver(
        child_content_capture_sender_->GetPendingAssociatedReceiver(),
        child_frame_);
  }

  FakeContentCaptureSender* content_capture_sender() {
    return content_capture_sender_.get();
  }

  FakeContentCaptureSender* child_content_capture_sender() {
    return child_content_capture_sender_.get();
  }

  const ContentCaptureData& test_data() const { return test_data_; }
  const ContentCaptureData& test_data_change() const {
    return test_data_change_;
  }
  const ContentCaptureData& test_data2() const { return test_data2_; }
  const ContentCaptureData& test_data_update() const {
    return test_data_update_;
  }
  const std::vector<int64_t>& expected_removed_ids() const {
    return expected_removed_ids_;
  }

  ContentCaptureFrame GetExpectedTestData(bool main_frame) const {
    ContentCaptureFrame expected(test_data_);
    // Replaces the id with expected id.
    expected.id = ContentCaptureReceiver::GetIdFrom(main_frame ? main_frame_
                                                               : child_frame_);
    return expected;
  }

  ContentCaptureFrame GetExpectedTestDataChange(int64_t expected_id) const {
    ContentCaptureFrame expected(test_data_change_);
    // Replaces the id with expected id.
    expected.id = expected_id;
    return expected;
  }

  ContentCaptureFrame GetExpectedTestData2(bool main_frame) const {
    ContentCaptureFrame expected(test_data2_);
    // Replaces the id with expected id.
    expected.id = ContentCaptureReceiver::GetIdFrom(main_frame ? main_frame_
                                                               : child_frame_);
    return expected;
  }

  ContentCaptureFrame GetExpectedTestDataUpdate(bool main_frame) const {
    ContentCaptureFrame expected(test_data_update_);
    // Replaces the id with expected id.
    expected.id = ContentCaptureReceiver::GetIdFrom(main_frame ? main_frame_
                                                               : child_frame_);
    return expected;
  }

  ContentCaptureConsumerHelper* content_capture_consumer_helper() const {
    return content_capture_consumer_helper_.get();
  }

  OnscreenContentProvider* onscreen_content_provider() const {
    return onscreen_content_provider_;
  }

  SessionRemovedTestHelper* session_removed_test_helper() {
    return &session_removed_test_helper_;
  }

  void VerifySession(const ContentCaptureSession& expected,
                     const ContentCaptureSession& result) const {
    EXPECT_EQ(expected.size(), result.size());
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(expected[i].id, result[i].id);
      EXPECT_EQ(expected[i].url, result[i].url);
      EXPECT_EQ(expected[i].bounds, result[i].bounds);
      EXPECT_TRUE(result[i].children.empty());
    }
  }

  void DidCaptureContent(const ContentCaptureData& captured_content,
                         bool first_data) {
    base::RunLoop run_loop;
    content_capture_sender()->DidCaptureContent(captured_content, first_data);
    run_loop.RunUntilIdle();
  }

  void DidCaptureContentForChildFrame(
      const ContentCaptureData& captured_content,
      bool first_data) {
    base::RunLoop run_loop;
    child_content_capture_sender()->DidCaptureContent(captured_content,
                                                      first_data);
    run_loop.RunUntilIdle();
  }

  void DidUpdateContent(const ContentCaptureData& updated_content) {
    base::RunLoop run_loop;
    content_capture_sender()->DidUpdateContent(updated_content);
    run_loop.RunUntilIdle();
  }

  void DidRemoveContent(const std::vector<int64_t>& data) {
    base::RunLoop run_loop;
    content_capture_sender()->DidRemoveContent(data);
    run_loop.RunUntilIdle();
  }

  void BuildChildSession(const ContentCaptureSession& parent,
                         const ContentCaptureFrame& data,
                         ContentCaptureSession* child) {
    ContentCaptureFrame child_frame = data;
    child_frame.children.clear();
    child->clear();
    child->push_back(child_frame);
    DCHECK(parent.size() == 1);
    child->push_back(parent.front());
  }

 protected:
  std::unique_ptr<ContentCaptureConsumerHelper>
      content_capture_consumer_helper_;
  OnscreenContentProvider* onscreen_content_provider_ = nullptr;

 private:
  // The sender for main frame.
  std::unique_ptr<FakeContentCaptureSender> content_capture_sender_;
  // The sender for child frame.
  std::unique_ptr<FakeContentCaptureSender> child_content_capture_sender_;
  content::RenderFrameHost* main_frame_ = nullptr;
  content::RenderFrameHost* child_frame_ = nullptr;
  ContentCaptureData test_data_;
  ContentCaptureData test_data_change_;
  ContentCaptureData test_data2_;
  ContentCaptureData test_data_update_;
  // Expected removed Ids.
  std::vector<int64_t> expected_removed_ids_{2};
  SessionRemovedTestHelper session_removed_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         ContentCaptureReceiverTest,
                         testing::Values(true, false));

TEST_P(ContentCaptureReceiverTest, DidCaptureContent) {
  DidCaptureContent(test_data(), true /* first_data */);
  EXPECT_TRUE(content_capture_consumer_helper()->parent_session().empty());
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(true /* main_frame */),
            content_capture_consumer_helper()->captured_data());
}

TEST_P(ContentCaptureReceiverTest, MultipleConsumers) {
  std::unique_ptr<ContentCaptureConsumerHelper> consumer2 =
      std::make_unique<ContentCaptureConsumerHelper>(nullptr);

  onscreen_content_provider()->AddConsumer(*(consumer2.get()));
  DidCaptureContent(test_data(), true /* first_data */);
  EXPECT_TRUE(content_capture_consumer_helper()->parent_session().empty());
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(true /* main_frame */),
            content_capture_consumer_helper()->captured_data());

  EXPECT_TRUE(consumer2->parent_session().empty());
  EXPECT_TRUE(consumer2->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(true /* main_frame */),
            consumer2->captured_data());

  // Verifies to get the remove session callback in RemoveConsumer.
  onscreen_content_provider()->RemoveConsumer(*(consumer2.get()));
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
  EXPECT_EQ(1u, consumer2->removed_sessions().size());
  std::vector<ContentCaptureFrame> expected{
      GetExpectedTestData(true /* main_frame */)};
  VerifySession(expected, consumer2->removed_sessions().front());
  EXPECT_EQ(1u, onscreen_content_provider()->GetConsumersForTesting().size());
  EXPECT_EQ(content_capture_consumer_helper(),
            onscreen_content_provider()->GetConsumersForTesting()[0]);
}

// TODO(https://crbug.com/1010179): Fix flakes on win10_chromium_x64_rel_ng and
// re-enable this test.
#if defined(OS_WIN)
#define MAYBE_DidCaptureContentWithUpdate DISABLED_DidCaptureContentWithUpdate
#else
#define MAYBE_DidCaptureContentWithUpdate DidCaptureContentWithUpdate
#endif
TEST_P(ContentCaptureReceiverTest, MAYBE_DidCaptureContentWithUpdate) {
  DidCaptureContent(test_data(), true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(content_capture_consumer_helper()->parent_session().empty());
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(true /* main_frame */),
            content_capture_consumer_helper()->captured_data());
  // Simulates to update the content within the same document.
  DidCaptureContent(test_data_update(), false /* first_data */);
  // Verifies to get test_data2() with correct frame content id.
  EXPECT_TRUE(content_capture_consumer_helper()->parent_session().empty());
  // Verifies that the session isn't removed.
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestDataUpdate(true /* main_frame */),
            content_capture_consumer_helper()->captured_data());
}

// TODO(https://crbug.com/1011204): Fix flakes on win10_chromium_x64_rel_ng and
// re-enable this test.
#if defined(OS_WIN)
#define MAYBE_DidUpdateContent DISABLED_DidUpdateContent
#else
#define MAYBE_DidUpdateContent DidUpdateContent
#endif
TEST_P(ContentCaptureReceiverTest, MAYBE_DidUpdateContent) {
  DidCaptureContent(test_data(), true /* first_data */);
  EXPECT_TRUE(content_capture_consumer_helper()->parent_session().empty());
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
  ContentCaptureFrame expected_data =
      GetExpectedTestData(true /* main_frame */);
  EXPECT_EQ(expected_data, content_capture_consumer_helper()->captured_data());

  // Simulate content change.
  DidUpdateContent(test_data_change());
  EXPECT_TRUE(
      content_capture_consumer_helper()->updated_parent_session().empty());
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestDataChange(expected_data.id),
            content_capture_consumer_helper()->updated_data());
}

TEST_P(ContentCaptureReceiverTest, DidRemoveSession) {
  DidCaptureContent(test_data(), true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(content_capture_consumer_helper()->parent_session().empty());
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(true /* main_frame */),
            content_capture_consumer_helper()->captured_data());
  // Simulates to navigate other document.
  DidCaptureContent(test_data2(), true /* first_data */);
  EXPECT_TRUE(content_capture_consumer_helper()->parent_session().empty());
  // Verifies that the previous session was removed.
  EXPECT_EQ(1u, content_capture_consumer_helper()->removed_sessions().size());
  std::vector<ContentCaptureFrame> expected{
      GetExpectedTestData(true /* main_frame */)};
  VerifySession(expected,
                content_capture_consumer_helper()->removed_sessions().front());
  // Verifies that we get the test_data2() from the new document.
  EXPECT_EQ(GetExpectedTestData2(true /* main_frame */),
            content_capture_consumer_helper()->captured_data());
}

TEST_P(ContentCaptureReceiverTest, DidRemoveContent) {
  DidCaptureContent(test_data(), true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(content_capture_consumer_helper()->parent_session().empty());
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(true /* main_frame */),
            content_capture_consumer_helper()->captured_data());
  // Simulates to remove the content.
  DidRemoveContent(expected_removed_ids());
  EXPECT_TRUE(content_capture_consumer_helper()->parent_session().empty());
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
  // Verifies that the removed_ids() was removed from the correct session.
  EXPECT_EQ(expected_removed_ids(),
            content_capture_consumer_helper()->removed_ids());
  std::vector<ContentCaptureFrame> expected{
      GetExpectedTestData(true /* main_frame */)};
  VerifySession(expected, content_capture_consumer_helper()->session());
}

TEST_P(ContentCaptureReceiverTest, ChildFrameDidCaptureContent) {
  // Simulate add child frame.
  SetupChildFrame();
  // Simulate to capture the content from main frame.
  DidCaptureContent(test_data(), true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(content_capture_consumer_helper()->parent_session().empty());
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(true /* main_frame */),
            content_capture_consumer_helper()->captured_data());
  // Simulate to capture the content from child frame.
  DidCaptureContentForChildFrame(test_data2(), true /* first_data */);
  // Verifies that the parent_session was set correctly.
  EXPECT_FALSE(content_capture_consumer_helper()->parent_session().empty());
  std::vector<ContentCaptureFrame> expected{
      GetExpectedTestData(true /* main_frame */)};
  VerifySession(expected, content_capture_consumer_helper()->parent_session());
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
  // Verifies that we receive the correct content from child frame.
  EXPECT_EQ(GetExpectedTestData2(false /* main_frame */),
            content_capture_consumer_helper()->captured_data());
}

// This test is for issue crbug.com/995121 .
TEST_P(ContentCaptureReceiverTest, RenderFrameHostGone) {
  auto* receiver =
      onscreen_content_provider()->ContentCaptureReceiverForFrameForTesting(
          web_contents()->GetMainFrame());
  // No good way to simulate crbug.com/995121, just set rfh_ to nullptr in
  // ContentCaptureReceiver, so content::WebContents::FromRenderFrameHost()
  // won't return WebContents.
  receiver->rfh_ = nullptr;
  // Ensure no crash.
  DidCaptureContent(test_data(), true /* first_data */);
  DidUpdateContent(test_data());
  DidRemoveContent(expected_removed_ids());
}

TEST_P(ContentCaptureReceiverTest, TitleUpdateTaskDelay) {
  auto* receiver =
      onscreen_content_provider()->ContentCaptureReceiverForFrameForTesting(
          web_contents()->GetMainFrame());
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  // Uses TestMockTimeTaskRunner to check the task state.
  receiver->title_update_task_runner_ = task_runner;

  receiver->SetTitle(u"title 1");
  // No task scheduled because no content has been captured.
  EXPECT_FALSE(receiver->notify_title_update_callback_);
  EXPECT_FALSE(task_runner->HasPendingTask());

  // Capture content, then update the title.
  DidCaptureContent(test_data(), /*first_data=*/true);
  std::u16string title2 = u"title 2";
  receiver->SetTitle(title2);
  // A task should be scheduled.
  EXPECT_TRUE(receiver->notify_title_update_callback_);
  EXPECT_TRUE(task_runner->HasPendingTask());
  EXPECT_EQ(2u, receiver->exponential_delay_);
  // Run the pending task.
  task_runner->FastForwardBy(
      base::TimeDelta::FromSeconds(receiver->exponential_delay_ / 2));
  task_runner->RunUntilIdle();
  // Verify the title is updated and the task is reset.
  EXPECT_EQ(title2, content_capture_consumer_helper()->updated_title());
  EXPECT_FALSE(receiver->notify_title_update_callback_);
  EXPECT_FALSE(task_runner->HasPendingTask());

  // Set the task_runner again since it is reset after task runs.
  receiver->title_update_task_runner_ = task_runner;

  // Change title again and verify the result.
  receiver->SetTitle(u"title 3");
  EXPECT_TRUE(receiver->notify_title_update_callback_);
  EXPECT_TRUE(task_runner->HasPendingTask());
  EXPECT_EQ(4u, receiver->exponential_delay_);

  // Remove the session to verify the pending task cancelled.
  receiver->RemoveSession();
  EXPECT_FALSE(receiver->notify_title_update_callback_);
  // The cancelled task isn't removed from the TaskRunner, prune it.
  task_runner->TakePendingTasks();
  EXPECT_FALSE(task_runner->HasPendingTask());
  // The delay time is reset after session is removed.
  EXPECT_EQ(1u, receiver->exponential_delay_);
  // Verify the latest task isn't run.
  EXPECT_EQ(title2, content_capture_consumer_helper()->updated_title());
}

// TODO(https://crbug.com/1010416): Fix flakes on win10_chromium_x64_rel_ng and
// re-enable this test.
#if defined(OS_WIN)
#define MAYBE_ChildFrameCaptureContentFirst \
  DISABLED_ChildFrameCaptureContentFirst
#else
#define MAYBE_ChildFrameCaptureContentFirst ChildFrameCaptureContentFirst
#endif
TEST_P(ContentCaptureReceiverTest, MAYBE_ChildFrameCaptureContentFirst) {
  // Simulate add child frame.
  SetupChildFrame();
  // Simulate to capture the content from child frame.
  DidCaptureContentForChildFrame(test_data2(), true /* first_data */);
  // Verifies that the parent_session was set correctly.
  EXPECT_FALSE(content_capture_consumer_helper()->parent_session().empty());

  ContentCaptureFrame data = GetExpectedTestData(true /* main_frame */);
  // Currently, there is no way to fake frame size, set it to 0.
  data.bounds = gfx::Rect();
  ContentCaptureSession expected{data};

  VerifySession(expected, content_capture_consumer_helper()->parent_session());
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
  // Verifies that we receive the correct content from child frame.
  EXPECT_EQ(GetExpectedTestData2(false /* main_frame */),
            content_capture_consumer_helper()->captured_data());

  // Get the child session, so we can verify that it has been removed in next
  // navigation
  ContentCaptureFrame child_frame = GetExpectedTestData2(false);
  // child_frame.children.clear();
  ContentCaptureSession removed_child_session;
  BuildChildSession(expected,
                    content_capture_consumer_helper()->captured_data(),
                    &removed_child_session);
  ContentCaptureSession removed_main_session = expected;
  // When main frame navigates to same url, the parent session will not change.
  NavigateMainFrame(GURL(kMainFrameUrl));
  SetupChildFrame();
  DidCaptureContentForChildFrame(test_data2(), true /* first_data */);
  VerifySession(expected, content_capture_consumer_helper()->parent_session());

  EXPECT_EQ(2u, content_capture_consumer_helper()->removed_sessions().size());
  VerifySession(removed_child_session,
                content_capture_consumer_helper()->removed_sessions().back());
  VerifySession(removed_main_session,
                content_capture_consumer_helper()->removed_sessions().front());

  // Get main and child session to verify that they are removed in next
  // navigateion.
  removed_main_session = expected;
  BuildChildSession(expected,
                    content_capture_consumer_helper()->captured_data(),
                    &removed_child_session);

  // When main frame navigates to same domain, the parent session will change.
  NavigateMainFrame(GURL(kMainFrameUrl2));
  SetupChildFrame();
  DidCaptureContentForChildFrame(test_data2(), true /* first_data */);

  // Intentionally reuse the data.id from previous result, so we know navigating
  // to same domain didn't create new ContentCaptureReceiver when call
  // VerifySession(), otherwise, we can't test the code to handle the navigation
  // in ContentCaptureReceiver.  - except when ProactivelySwapBrowsingInstance
  // or RenderDocument is enabled on same-site main frame navigation, where we
  // will get new RenderFrameHosts after the navigation to |kMainFrameUrl2|.
  if (content::CanSameSiteMainFrameNavigationsChangeRenderFrameHosts())
    data = GetExpectedTestData(/* main_frame =*/true);

  data.url = kMainFrameUrl2;
  // Currently, there is no way to fake frame size, set it to 0.
  data.bounds = gfx::Rect();
  expected.clear();
  expected.push_back(data);
  VerifySession(expected, content_capture_consumer_helper()->parent_session());
  // There are two sessions removed, one the main frame because we navigate to
  // different URL (though the domain is same), another one is child frame
  // because of the main frame change.
  EXPECT_EQ(2u, content_capture_consumer_helper()->removed_sessions().size());

  VerifySession(removed_child_session,
                content_capture_consumer_helper()->removed_sessions().back());
  VerifySession(removed_main_session,
                content_capture_consumer_helper()->removed_sessions().front());

  // Keep current sessions to verify removed sessions later.
  removed_main_session = expected;
  BuildChildSession(expected,
                    content_capture_consumer_helper()->captured_data(),
                    &removed_child_session);

  // When main frame navigates to different domain, the parent session will
  // change.
  NavigateMainFrame(GURL(kChildFrameUrl));
  SetupChildFrame();
  DidCaptureContentForChildFrame(test_data2(), true /* first_data */);

  data = GetExpectedTestData2(true /* main_frame */);
  // Currently, there is no way to fake frame size, set it to 0.
  data.bounds = gfx::Rect();
  expected.clear();
  expected.push_back(data);
  VerifySession(expected, content_capture_consumer_helper()->parent_session());
  EXPECT_EQ(2u, content_capture_consumer_helper()->removed_sessions().size());
  VerifySession(removed_child_session,
                content_capture_consumer_helper()->removed_sessions().back());
  VerifySession(removed_main_session,
                content_capture_consumer_helper()->removed_sessions().front());

  // Keep current sessions to verify removed sessions later.
  removed_main_session = expected;
  BuildChildSession(expected,
                    content_capture_consumer_helper()->captured_data(),
                    &removed_child_session);

  session_removed_test_helper()->Reset();
  DeleteContents();
  EXPECT_EQ(2u, session_removed_test_helper()->removed_sessions().size());
  VerifySession(removed_child_session,
                session_removed_test_helper()->removed_sessions().front());
  VerifySession(removed_main_session,
                session_removed_test_helper()->removed_sessions().back());
}

TEST_P(ContentCaptureReceiverTest, SameDocumentSameSession) {
  DidCaptureContent(test_data(), true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(content_capture_consumer_helper()->parent_session().empty());
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(true /* main_frame */),
            content_capture_consumer_helper()->captured_data());
  NavigateMainFrameSameDocument();
  // Verifies the session wasn't removed for the same document navigation.
  EXPECT_TRUE(content_capture_consumer_helper()->removed_sessions().empty());
}
class ContentCaptureReceiverMultipleFrameTest
    : public ContentCaptureReceiverTest {
 public:
  void SetUp() override {
    // Setup multiple frames before creates OnscreenContentProvider.
    content::RenderViewHostTestHarness::SetUp();
    // This needed to keep the WebContentsObserverConsistencyChecker checks
    // happy for when AppendChild is called.
    NavigateAndCommit(GURL("about:blank"));
    content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
        ->AppendChild("child");
    onscreen_content_provider_ =
        OnscreenContentProvider::Create(web_contents());

    content_capture_consumer_helper_ =
        std::make_unique<ContentCaptureConsumerHelper>(nullptr);
    onscreen_content_provider_->AddConsumer(
        *(content_capture_consumer_helper_.get()));
  }

  void TearDown() override { content::RenderViewHostTestHarness::TearDown(); }
};

// TODO(https://crbug.com/1010417): Fix flakes on win10_chromium_x64_rel_ng and
// re-enable this test.
#if defined(OS_WIN)
#define MAYBE_ReceiverCreatedForExistingFrame \
  DISABLED_ReceiverCreatedForExistingFrame
#else
#define MAYBE_ReceiverCreatedForExistingFrame ReceiverCreatedForExistingFrame
#endif
TEST_F(ContentCaptureReceiverMultipleFrameTest,
       MAYBE_ReceiverCreatedForExistingFrame) {
  EXPECT_EQ(2u, onscreen_content_provider()->GetFrameMapSizeForTesting());
}

}  // namespace content_capture
