// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/test/frame_test_util.h"

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/test/fit_adapter.h"
#include "fuchsia/base/test/result_receiver.h"
#include "fuchsia/base/test/test_navigation_listener.h"

namespace cr_fuchsia {

bool LoadUrlAndExpectResponse(
    fuchsia::web::NavigationController* navigation_controller,
    fuchsia::web::LoadUrlParams load_url_params,
    base::StringPiece url) {
  DCHECK(navigation_controller);
  base::RunLoop run_loop;
  ResultReceiver<fuchsia::web::NavigationController_LoadUrl_Result> result(
      run_loop.QuitClosure());
  navigation_controller->LoadUrl(
      std::string(url), std::move(load_url_params),
      CallbackToFitFunction(result.GetReceiveCallback()));
  run_loop.Run();
  return result->is_response();
}

bool LoadUrlAndExpectResponse(
    const fuchsia::web::NavigationControllerPtr& controller,
    fuchsia::web::LoadUrlParams params,
    base::StringPiece url) {
  return LoadUrlAndExpectResponse(controller.get(), std::move(params), url);
}

absl::optional<base::Value> ExecuteJavaScript(fuchsia::web::Frame* frame,
                                              base::StringPiece script) {
  base::RunLoop run_loop;
  ResultReceiver<fuchsia::web::Frame_ExecuteJavaScript_Result> result(
      run_loop.QuitClosure());
  frame->ExecuteJavaScript({"*"}, MemBufferFromString(script, "test"),
                           CallbackToFitFunction(result.GetReceiveCallback()));
  run_loop.Run();

  if (!result.has_value() || !result->is_response())
    return {};

  std::string result_json;
  if (!StringFromMemBuffer(result->response().result, &result_json)) {
    return {};
  }

  return base::JSONReader::Read(result_json);
}

fuchsia::web::LoadUrlParams CreateLoadUrlParamsWithUserActivation() {
  fuchsia::web::LoadUrlParams load_url_params;
  load_url_params.set_was_user_activated(true);
  return load_url_params;
}

fuchsia::web::WebMessage CreateWebMessageWithMessagePortRequest(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request,
    fuchsia::mem::Buffer buffer) {
  fuchsia::web::OutgoingTransferable outgoing;
  outgoing.set_message_port(std::move(message_port_request));

  std::vector<fuchsia::web::OutgoingTransferable> outgoing_vector;
  outgoing_vector.push_back(std::move(outgoing));

  fuchsia::web::WebMessage web_message;
  web_message.set_outgoing_transfer(std::move(outgoing_vector));
  web_message.set_data(std::move(buffer));
  return web_message;
}

}  // namespace cr_fuchsia
