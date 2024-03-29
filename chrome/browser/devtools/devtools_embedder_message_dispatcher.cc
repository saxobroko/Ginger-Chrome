// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_embedder_message_dispatcher.h"

#include <memory>

#include "base/bind.h"
#include "base/values.h"

namespace {

using DispatchCallback = DevToolsEmbedderMessageDispatcher::DispatchCallback;

bool GetValue(const base::Value& value, std::string* result) {
  if (result && value.is_string()) {
    *result = value.GetString();
    return true;
  }
  return value.is_string();
}

bool GetValue(const base::Value& value, int* result) {
  if (result && value.is_int()) {
    *result = value.GetInt();
    return true;
  }
  return value.is_int();
}

bool GetValue(const base::Value& value, double* result) {
  if (result && (value.is_double() || value.is_int())) {
    *result = value.GetDouble();
    return true;
  }
  return value.is_double() || value.is_int();
}

bool GetValue(const base::Value& value, bool* result) {
  if (result && value.is_bool()) {
    *result = value.GetBool();
    return true;
  }
  return value.is_bool();
}

bool GetValue(const base::Value& value, gfx::Rect* rect) {
  const base::DictionaryValue* dict;
  if (!value.GetAsDictionary(&dict))
    return false;
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  if (!dict->GetInteger("x", &x) ||
      !dict->GetInteger("y", &y) ||
      !dict->GetInteger("width", &width) ||
      !dict->GetInteger("height", &height))
    return false;
  rect->SetRect(x, y, width, height);
  return true;
}

template <typename T>
struct StorageTraits {
  using StorageType = T;
};

template <typename T>
struct StorageTraits<const T&> {
  using StorageType = T;
};

template <typename... Ts>
struct ParamTuple {
  bool Parse(const base::ListValue& list,
             const base::ListValue::const_iterator& it) {
    return it == list.GetList().end();
  }

  template <typename H, typename... As>
  void Apply(const H& handler, As... args) {
    handler.Run(std::forward<As>(args)...);
  }
};

template <typename T, typename... Ts>
struct ParamTuple<T, Ts...> {
  bool Parse(const base::ListValue& list,
             const base::ListValue::const_iterator& it) {
    return it != list.GetList().end() && GetValue(*it, &head) &&
           tail.Parse(list, it + 1);
  }

  template <typename H, typename... As>
  void Apply(const H& handler, As... args) {
    tail.template Apply<H, As..., T>(handler, std::forward<As>(args)..., head);
  }

  typename StorageTraits<T>::StorageType head;
  ParamTuple<Ts...> tail;
};

template <typename... As>
bool ParseAndHandle(const base::RepeatingCallback<void(As...)>& handler,
                    DispatchCallback callback,
                    const base::ListValue& list) {
  ParamTuple<As...> tuple;
  if (!tuple.Parse(list, list.GetList().begin()))
    return false;
  tuple.Apply(handler);
  return true;
}

template <typename... As>
bool ParseAndHandleWithCallback(
    const base::RepeatingCallback<void(DispatchCallback, As...)>& handler,
    DispatchCallback callback,
    const base::ListValue& list) {
  ParamTuple<As...> tuple;
  if (!tuple.Parse(list, list.GetList().begin()))
    return false;
  tuple.Apply(handler, std::move(callback));
  return true;
}

}  // namespace

/**
 * Dispatcher for messages sent from the frontend running in an
 * isolated renderer (devtools:// or chrome://inspect) to the embedder
 * in the browser.
 *
 * The messages are sent via InspectorFrontendHost.sendMessageToEmbedder or
 * chrome.send method accordingly.
 */
class DispatcherImpl : public DevToolsEmbedderMessageDispatcher {
 public:
  ~DispatcherImpl() override = default;

  bool Dispatch(DispatchCallback callback,
                const std::string& method,
                const base::ListValue* params) override {
    auto it = handlers_.find(method);
    return it != handlers_.end() &&
           it->second.Run(std::move(callback), *params);
  }

  template<typename... As>
  void RegisterHandler(const std::string& method,
                       void (Delegate::*handler)(As...),
                       Delegate* delegate) {
    handlers_[method] = base::BindRepeating(
        &ParseAndHandle<As...>,
        base::BindRepeating(handler, base::Unretained(delegate)));
  }

  template <typename... As>
  void RegisterHandlerWithCallback(const std::string& method,
                                   void (Delegate::*handler)(DispatchCallback,
                                                             As...),
                                   Delegate* delegate) {
    handlers_[method] = base::BindRepeating(
        &ParseAndHandleWithCallback<As...>,
        base::BindRepeating(handler, base::Unretained(delegate)));
  }

 private:
  using Handler =
      base::RepeatingCallback<bool(DispatchCallback, const base::ListValue&)>;
  using HandlerMap = std::map<std::string, Handler>;
  HandlerMap handlers_;
};

// static
std::unique_ptr<DevToolsEmbedderMessageDispatcher>
DevToolsEmbedderMessageDispatcher::CreateForDevToolsFrontend(
    Delegate* delegate) {
  auto d = std::make_unique<DispatcherImpl>();

  d->RegisterHandler("bringToFront", &Delegate::ActivateWindow, delegate);
  d->RegisterHandler("closeWindow", &Delegate::CloseWindow, delegate);
  d->RegisterHandler("loadCompleted", &Delegate::LoadCompleted, delegate);
  d->RegisterHandler("setInspectedPageBounds",
                     &Delegate::SetInspectedPageBounds, delegate);
  d->RegisterHandler("inspectElementCompleted",
                     &Delegate::InspectElementCompleted, delegate);
  d->RegisterHandler("inspectedURLChanged",
                     &Delegate::InspectedURLChanged, delegate);
  d->RegisterHandlerWithCallback("setIsDocked",
                                 &Delegate::SetIsDocked, delegate);
  d->RegisterHandler("openInNewTab", &Delegate::OpenInNewTab, delegate);
  d->RegisterHandler("showItemInFolder", &Delegate::ShowItemInFolder, delegate);
  d->RegisterHandler("save", &Delegate::SaveToFile, delegate);
  d->RegisterHandler("append", &Delegate::AppendToFile, delegate);
  d->RegisterHandler("requestFileSystems",
                     &Delegate::RequestFileSystems, delegate);
  d->RegisterHandler("addFileSystem", &Delegate::AddFileSystem, delegate);
  d->RegisterHandler("removeFileSystem", &Delegate::RemoveFileSystem, delegate);
  d->RegisterHandler("upgradeDraggedFileSystemPermissions",
                     &Delegate::UpgradeDraggedFileSystemPermissions, delegate);
  d->RegisterHandler("indexPath", &Delegate::IndexPath, delegate);
  d->RegisterHandlerWithCallback("loadNetworkResource",
                                 &Delegate::LoadNetworkResource, delegate);
  d->RegisterHandler("stopIndexing", &Delegate::StopIndexing, delegate);
  d->RegisterHandler("searchInPath", &Delegate::SearchInPath, delegate);
  d->RegisterHandler("setWhitelistedShortcuts",
                     &Delegate::SetWhitelistedShortcuts, delegate);
  d->RegisterHandler("setEyeDropperActive", &Delegate::SetEyeDropperActive,
                     delegate);
  d->RegisterHandler("showCertificateViewer",
                     &Delegate::ShowCertificateViewer, delegate);
  d->RegisterHandler("zoomIn", &Delegate::ZoomIn, delegate);
  d->RegisterHandler("zoomOut", &Delegate::ZoomOut, delegate);
  d->RegisterHandler("resetZoom", &Delegate::ResetZoom, delegate);
  d->RegisterHandler("setDevicesDiscoveryConfig",
                     &Delegate::SetDevicesDiscoveryConfig, delegate);
  d->RegisterHandler("setDevicesUpdatesEnabled",
                     &Delegate::SetDevicesUpdatesEnabled, delegate);
  d->RegisterHandler("performActionOnRemotePage",
                     &Delegate::PerformActionOnRemotePage, delegate);
  d->RegisterHandler("openRemotePage", &Delegate::OpenRemotePage, delegate);
  d->RegisterHandler("openNodeFrontend", &Delegate::OpenNodeFrontend, delegate);
  d->RegisterHandler("dispatchProtocolMessage",
                     &Delegate::DispatchProtocolMessageFromDevToolsFrontend,
                     delegate);
  d->RegisterHandler("recordEnumeratedHistogram",
                     &Delegate::RecordEnumeratedHistogram, delegate);
  d->RegisterHandler("recordPerformanceHistogram",
                     &Delegate::RecordPerformanceHistogram, delegate);
  d->RegisterHandler("recordUserMetricsAction",
                     &Delegate::RecordUserMetricsAction, delegate);
  d->RegisterHandlerWithCallback("sendJsonRequest",
                                 &Delegate::SendJsonRequest, delegate);
  d->RegisterHandlerWithCallback("getPreferences",
                                 &Delegate::GetPreferences, delegate);
  d->RegisterHandler("setPreference",
                     &Delegate::SetPreference, delegate);
  d->RegisterHandler("removePreference",
                     &Delegate::RemovePreference, delegate);
  d->RegisterHandler("clearPreferences",
                     &Delegate::ClearPreferences, delegate);
  d->RegisterHandlerWithCallback("reattach",
                                 &Delegate::Reattach, delegate);
  d->RegisterHandler("readyForTest",
                     &Delegate::ReadyForTest, delegate);
  d->RegisterHandler("connectionReady", &Delegate::ConnectionReady, delegate);
  d->RegisterHandler("setOpenNewWindowForPopups",
                     &Delegate::SetOpenNewWindowForPopups, delegate);
  d->RegisterHandler("registerExtensionsAPI", &Delegate::RegisterExtensionsAPI,
                     delegate);
  d->RegisterHandlerWithCallback("showSurvey", &Delegate::ShowSurvey, delegate);
  d->RegisterHandlerWithCallback("canShowSurvey", &Delegate::CanShowSurvey,
                                 delegate);
  return d;
}
