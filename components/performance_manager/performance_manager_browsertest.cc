// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/performance_manager.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "components/performance_manager/test_support/performance_manager_browsertest_harness.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

using PerformanceManagerBrowserTest = PerformanceManagerBrowserTestHarness;

// A full browser test is required for this because we need RenderFrameHosts
// to be created.
IN_PROC_BROWSER_TEST_F(PerformanceManagerBrowserTest,
                       GetFrameNodeForRenderFrameHost) {
  // NavigateToURL blocks until the load has completed. We assert that the
  // contents has been reused as we don't have general WebContents creation
  // hooks in our BrowserTest fixture, and if a new contents was created it
  // would be missing the PM tab helper.
  auto* old_contents = shell()->web_contents();
  static const char kUrl[] = "about:blank";
  ASSERT_TRUE(NavigateToURL(shell(), GURL(kUrl)));
  content::WebContents* contents = shell()->web_contents();
  ASSERT_EQ(contents, old_contents);
  ASSERT_EQ(contents->GetLastCommittedURL().possibly_invalid_spec(), kUrl);
  content::RenderFrameHost* rfh = contents->GetMainFrame();
  ASSERT_TRUE(rfh->IsRenderFrameCreated());

  base::WeakPtr<FrameNode> frame_node =
      PerformanceManager::GetFrameNodeForRenderFrameHost(rfh);

  // Post a task to the Graph and make it call a function on the UI thread that
  // will ensure that |frame_node| is really associated with |rfh|.

  base::RunLoop run_loop;
  auto check_rfh_on_main_thread =
      base::BindLambdaForTesting([&](const RenderFrameHostProxy& rfh_proxy) {
        EXPECT_EQ(rfh, rfh_proxy.Get());
        run_loop.Quit();
      });

  auto call_on_graph_cb = base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(frame_node.get());
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(check_rfh_on_main_thread),
                                  frame_node->GetRenderFrameHostProxy()));
  });

  PerformanceManager::CallOnGraph(FROM_HERE, call_on_graph_cb);

  // Wait for |check_rfh_on_main_thread| to be called.
  run_loop.Run();

  // This closes the window, and destroys the underlying WebContents.
  shell()->Close();
  contents = nullptr;

  // After deleting |contents| the corresponding FrameNode WeakPtr should be
  // invalid.
  base::RunLoop run_loop_after_contents_reset;
  auto quit_closure = run_loop_after_contents_reset.QuitClosure();
  auto call_on_graph_cb_2 = base::BindLambdaForTesting([&]() {
    EXPECT_FALSE(frame_node.get());
    std::move(quit_closure).Run();
  });

  PerformanceManager::CallOnGraph(FROM_HERE, call_on_graph_cb_2);
  run_loop_after_contents_reset.Run();
}

IN_PROC_BROWSER_TEST_F(PerformanceManagerBrowserTest, OpenerTrackingWorks) {
  // Load a page that will load a popup.
  GURL url(embedded_test_server()->GetURL("a.com", "/a_popup_a.html"));
  content::ShellAddedObserver shell_added_observer;
  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Wait for the popup window to appear, and then wait for it to load.
  auto* popup = shell_added_observer.GetShell();
  ASSERT_TRUE(popup);
  WaitForLoad(popup->web_contents());

  auto* contents = shell()->web_contents();
  auto page = PerformanceManager::GetPageNodeForWebContents(contents);

  // Jump into the graph and make sure everything is connected as expected.
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([&page, &run_loop]() {
        EXPECT_TRUE(page);
        auto* frame = page->GetMainFrameNode();
        EXPECT_EQ(1u, frame->GetOpenedPageNodes().size());
        auto* embedded_page = *(frame->GetOpenedPageNodes().begin());
        EXPECT_EQ(frame, embedded_page->GetOpenerFrameNode());
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace performance_manager
