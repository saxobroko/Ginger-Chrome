// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/prerender_test_util.h"

#include "base/callback_helpers.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace test {
namespace {

constexpr char kAddPrerenderScript[] = R"({
    const link = document.createElement('link');
    link.rel = 'prerender';
    link.href = $1;
    document.head.appendChild(link);
  })";

constexpr char kAddSpeculationRuleScript[] = R"({
    const script = document.createElement('script');
    script.type = 'speculationrules';
    script.text = `{
      "prerender": [{
        "source": "list",
        "urls": [$1]
      }]
    }`;
    document.head.appendChild(script);
  })";

PrerenderHostRegistry& GetPrerenderHostRegistry(
    content::WebContents* web_contents) {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  return *static_cast<WebContentsImpl*>(web_contents)
              ->GetPrerenderHostRegistry();
}

PrerenderHost* GetPrerenderHostById(content::WebContents* web_contents,
                                    int host_id) {
  auto& registry = GetPrerenderHostRegistry(web_contents);
  return registry.FindNonReservedHostById(host_id);
}

}  // namespace

class PrerenderHostRegistryObserverImpl
    : public PrerenderHostRegistry::Observer {
 public:
  explicit PrerenderHostRegistryObserverImpl(
      content::WebContents& web_contents) {
    observation_.Observe(&GetPrerenderHostRegistry(&web_contents));
  }

  void WaitForTrigger(const GURL& url) {
    if (triggered_.contains(url)) {
      return;
    }
    EXPECT_FALSE(waiting_.contains(url));
    base::RunLoop loop;
    waiting_[url] = loop.QuitClosure();
    loop.Run();
  }

  void NotifyOnTrigger(const GURL& url, base::OnceClosure callback) {
    if (triggered_.contains(url)) {
      std::move(callback).Run();
      return;
    }
    EXPECT_FALSE(waiting_.contains(url));
    waiting_[url] = std::move(callback);
  }

  void OnTrigger(const GURL& url) override {
    auto iter = waiting_.find(url);
    if (iter != waiting_.end()) {
      auto callback = std::move(iter->second);
      waiting_.erase(iter);
      std::move(callback).Run();
    } else {
      EXPECT_FALSE(triggered_.contains(url))
          << "this observer doesn't yet support multiple triggers";
      triggered_.insert(url);
    }
  }

  void OnRegistryDestroyed() override {
    EXPECT_TRUE(waiting_.empty());
    observation_.Reset();
  }

  base::ScopedObservation<PrerenderHostRegistry,
                          PrerenderHostRegistry::Observer>
      observation_{this};

  base::flat_map<GURL, base::OnceClosure> waiting_;
  base::flat_set<GURL> triggered_;
};

PrerenderHostRegistryObserver::PrerenderHostRegistryObserver(
    content::WebContents& web_contents)
    : impl_(std::make_unique<PrerenderHostRegistryObserverImpl>(web_contents)) {
}

PrerenderHostRegistryObserver::~PrerenderHostRegistryObserver() = default;

void PrerenderHostRegistryObserver::WaitForTrigger(const GURL& gurl) {
  impl_->WaitForTrigger(gurl);
}

void PrerenderHostRegistryObserver::NotifyOnTrigger(
    const GURL& gurl,
    base::OnceClosure callback) {
  impl_->NotifyOnTrigger(gurl, std::move(callback));
}

class PrerenderHostObserverImpl : public PrerenderHost::Observer {
 public:
  PrerenderHostObserverImpl(content::WebContents& web_contents, int host_id) {
    StartObserving(
        web_contents,
        GetPrerenderHostById(&web_contents, host_id)->GetInitialUrl());
  }

  PrerenderHostObserverImpl(content::WebContents& web_contents,
                            const GURL& gurl) {
    registry_observer_ =
        std::make_unique<PrerenderHostRegistryObserver>(web_contents);
    if (PrerenderHost* host = GetPrerenderHostRegistry(&web_contents)
                                  .FindHostByUrlForTesting(gurl)) {
      StartObserving(web_contents, host->GetInitialUrl());
    } else {
      registry_observer_->NotifyOnTrigger(
          gurl,
          base::BindOnce(&PrerenderHostObserverImpl::StartObserving,
                         base::Unretained(this), std::ref(web_contents), gurl));
    }
  }

  void OnActivated() override {
    was_activated_ = true;
    if (waiting_for_activation_)
      std::move(waiting_for_activation_).Run();
  }

  void OnHostDestroyed() override {
    observation_.Reset();
    if (waiting_for_destruction_)
      std::move(waiting_for_destruction_).Run();
  }

  void WaitForActivation() {
    if (was_activated_)
      return;
    EXPECT_FALSE(waiting_for_activation_);
    base::RunLoop loop;
    waiting_for_activation_ = loop.QuitClosure();
    loop.Run();
  }

  void WaitForDestroyed() {
    if (did_observe_ && !observation_.IsObserving())
      return;
    EXPECT_FALSE(waiting_for_destruction_);
    base::RunLoop loop;
    waiting_for_destruction_ = loop.QuitClosure();
    loop.Run();
  }

  bool was_activated() const { return was_activated_; }

 private:
  void StartObserving(content::WebContents& web_contents, const GURL& gurl) {
    PrerenderHost* host =
        GetPrerenderHostRegistry(&web_contents).FindHostByUrlForTesting(gurl);
    DCHECK_NE(host, nullptr);
    did_observe_ = true;
    observation_.Observe(host);

    // This method may be bound and called from |registry_observer_| so don't
    // add code below the reset.
    registry_observer_.reset();
  }

  base::ScopedObservation<PrerenderHost, PrerenderHost::Observer> observation_{
      this};
  base::OnceClosure waiting_for_activation_;
  base::OnceClosure waiting_for_destruction_;
  std::unique_ptr<PrerenderHostRegistryObserver> registry_observer_;
  bool was_activated_ = false;
  bool did_observe_ = false;
};

PrerenderHostObserver::PrerenderHostObserver(content::WebContents& web_contents,
                                             int prerender_host)
    : impl_(std::make_unique<PrerenderHostObserverImpl>(web_contents,
                                                        prerender_host)) {}

PrerenderHostObserver::PrerenderHostObserver(content::WebContents& web_contents,
                                             const GURL& gurl)
    : impl_(std::make_unique<PrerenderHostObserverImpl>(web_contents, gurl)) {}

PrerenderHostObserver::~PrerenderHostObserver() = default;

void PrerenderHostObserver::WaitForActivation() {
  impl_->WaitForActivation();
}

void PrerenderHostObserver::WaitForDestroyed() {
  impl_->WaitForDestroyed();
}

bool PrerenderHostObserver::was_activated() const {
  return impl_->was_activated();
}

PrerenderTestHelper::PrerenderTestHelper(const content::WebContents::Getter& fn)
    : get_web_contents_fn_(fn) {
  feature_list_.InitAndEnableFeature(blink::features::kPrerender2);
}

PrerenderTestHelper::~PrerenderTestHelper() = default;

void PrerenderTestHelper::SetUpOnMainThread(
    net::test_server::EmbeddedTestServer* http_server) {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  EXPECT_FALSE(http_server->Started());
  http_server->RegisterRequestMonitor(base::BindRepeating(
      &PrerenderTestHelper::MonitorResourceRequest, base::Unretained(this)));
}

int PrerenderTestHelper::GetHostForUrl(const GURL& gurl) {
  auto* host =
      GetPrerenderHostRegistry(GetWebContents()).FindHostByUrlForTesting(gurl);
  return host ? host->frame_tree_node_id()
              : RenderFrameHost::kNoFrameTreeNodeId;
}

void PrerenderTestHelper::WaitForPrerenderLoadCompletion(int host_id) {
  auto* host = GetPrerenderHostById(GetWebContents(), host_id);
  ASSERT_NE(host, nullptr);
  host->WaitForLoadStopForTesting();
}

void PrerenderTestHelper::WaitForPrerenderLoadCompletion(const GURL& gurl) {
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry(GetWebContents());
  PrerenderHost* host = registry.FindHostByUrlForTesting(gurl);
  // Wait for the host to be created if it hasn't yet.
  if (!host) {
    PrerenderHostRegistryObserver observer(*GetWebContents());
    observer.WaitForTrigger(gurl);
    host = registry.FindHostByUrlForTesting(gurl);
    ASSERT_NE(host, nullptr);
  }
  host->WaitForLoadStopForTesting();
}

int PrerenderTestHelper::AddPrerender(const GURL& gurl) {
  AddPrerenderAsync(gurl);

  WaitForPrerenderLoadCompletion(gurl);
  int host_id = GetHostForUrl(gurl);
  EXPECT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
  return host_id;
}

void PrerenderTestHelper::AddPrerenderAsync(const GURL& gurl) {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));

  const auto script = JsReplace(kAddPrerenderScript, gurl);
  GetWebContents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script), base::NullCallback());
}

int PrerenderTestHelper::AddSpeculationRules(const GURL& prerendering_url) {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  AddSpeculationRulesAsync(prerendering_url);

  WaitForPrerenderLoadCompletion(prerendering_url);
  const int host_id = GetHostForUrl(prerendering_url);
  EXPECT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
  return host_id;
}

void PrerenderTestHelper::AddSpeculationRulesAsync(
    const GURL& prerendering_url) {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  const auto script = JsReplace(kAddSpeculationRuleScript, prerendering_url);
  GetWebContents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script), base::NullCallback());
}

void PrerenderTestHelper::NavigatePrerenderedPage(int host_id,
                                                  const GURL& gurl) {
  auto* prerender_host = GetPrerenderHostById(GetWebContents(), host_id);
  ASSERT_NE(prerender_host, nullptr);
  RenderFrameHostImpl* prerender_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();
  // Ignore the result of ExecJs().
  //
  // Navigation from the prerendered page could cancel prerendering and
  // destroy the prerendered frame before ExecJs() gets a result from that.
  // This results in execution failure even when the execution succeeded. See
  // https://crbug.com/1186584 for details.
  //
  // This part will drastically be modified by the MPArch, so we take the
  // approach just to ignore it instead of fixing the timing issue. When
  // ExecJs() actually fails, the remaining test steps should fail, so it
  // should be safe to ignore it.
  ignore_result(
      ExecJs(prerender_render_frame_host, JsReplace("location = $1", gurl)));
}

void PrerenderTestHelper::NavigatePrimaryPage(const GURL& url) {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::TestNavigationObserver observer(GetWebContents());
  // Ignore the result of ExecJs().
  //
  // Depending on timing, activation could destroy the current WebContents
  // before ExecJs() gets a result from the frame that executed scripts. This
  // results in execution failure even when the execution succeeded. See
  // https://crbug.com/1156141 for details.
  //
  // This part will drastically be modified by the MPArch, so we take the
  // approach just to ignore it instead of fixing the timing issue. When
  // ExecJs() actually fails, the remaining test steps should fail, so it
  // should be safe to ignore it.
  ignore_result(ExecJs(GetWebContents()->GetMainFrame(),
                       JsReplace("location = $1", url)));
  observer.Wait();
}

::testing::AssertionResult PrerenderTestHelper::VerifyPrerenderingState(
    const GURL& gurl) {
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry(GetWebContents());
  PrerenderHost* prerender_host = registry.FindHostByUrlForTesting(gurl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();
  std::vector<RenderFrameHost*> frames =
      prerendered_render_frame_host->GetFramesInSubtree();
  for (auto* frame : frames) {
    auto* rfhi = static_cast<RenderFrameHostImpl*>(frame);
    // All the subframes should be in LifecycleStateImpl::kPrerendering state
    // before activation.
    if (rfhi->lifecycle_state() !=
        RenderFrameHostImpl::LifecycleStateImpl::kPrerendering) {
      return ::testing::AssertionFailure() << "subframe in incorrect state";
    }
    if (rfhi->frame_tree()->type() != FrameTree::Type::kPrerender) {
      return ::testing::AssertionFailure() << "frame tree had incorrect type";
    }
  }
  return ::testing::AssertionSuccess();
}

RenderFrameHost* PrerenderTestHelper::GetPrerenderedMainFrameHost(int host_id) {
  auto* prerender_host = GetPrerenderHostById(GetWebContents(), host_id);
  EXPECT_NE(prerender_host, nullptr);
  return prerender_host->GetPrerenderedMainFrameHost();
}

int PrerenderTestHelper::GetRequestCount(const GURL& url) {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock auto_lock(lock_);
  return request_count_by_path_[url.PathForRequest()];
}

void PrerenderTestHelper::WaitForRequest(const GURL& url, int count) {
  for (;;) {
    base::RunLoop run_loop;
    {
      base::AutoLock auto_lock(lock_);
      if (request_count_by_path_[url.PathForRequest()] >= count)
        return;
      monitor_callback_ = run_loop.QuitClosure();
    }
    run_loop.Run();
  }
}

void PrerenderTestHelper::MonitorResourceRequest(
    const net::test_server::HttpRequest& request) {
  // This should be called on `EmbeddedTestServer::io_thread_`.
  EXPECT_FALSE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::AutoLock auto_lock(lock_);
  request_count_by_path_[request.GetURL().PathForRequest()]++;
  if (monitor_callback_)
    std::move(monitor_callback_).Run();
}

content::WebContents* PrerenderTestHelper::GetWebContents() {
  return get_web_contents_fn_.Run();
}

}  // namespace test

}  // namespace content
