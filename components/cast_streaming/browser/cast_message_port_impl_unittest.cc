// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/cast_message_port_impl.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/cast/message_port/test_message_port_receiver.h"
#include "components/cast_streaming/browser/message_serialization.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_streaming {

namespace {
const char kSenderId[] = "senderId";
}  // namespace

class CastMessagePortImplTest : public testing::Test,
                                public openscreen::cast::MessagePort::Client {
 public:
  CastMessagePortImplTest() = default;
  ~CastMessagePortImplTest() override = default;

  CastMessagePortImplTest(const CastMessagePortImplTest&) = delete;
  CastMessagePortImplTest& operator=(const CastMessagePortImplTest&) = delete;

  void SetUp() override {
    std::unique_ptr<cast_api_bindings::MessagePort> receiver;
    cast_api_bindings::MessagePort::CreatePair(&sender_message_port_,
                                               &receiver);

    sender_message_port_->SetReceiver(&sender_message_port_receiver_);
    receiver_message_port_ = std::make_unique<CastMessagePortImpl>(
        std::move(receiver),
        base::BindOnce(&CastMessagePortImplTest::OnCastChannelClosed,
                       base::Unretained(this)));
    receiver_message_port_->SetClient(this, kSenderId);
  }

 protected:
  struct CastMessage {
    std::string sender_id;
    std::string message_namespace;
    std::string message;
  };

  void RunUntilMessageCountIsAtLeast(size_t message_count) {
    while (receiver_messages_.size() < message_count) {
      base::RunLoop run_loop;
      receiver_message_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  void RunUntilError() {
    base::RunLoop run_loop;
    error_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void RunUntilCastChannelClosed() {
    base::RunLoop run_loop;
    cast_channel_closed_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnCastChannelClosed() {
    if (cast_channel_closed_closure_) {
      std::move(cast_channel_closed_closure_).Run();
    } else {
      ADD_FAILURE() << "Cast Streaming Session MessagePort disconnected";
    }
  }

  // openscreen::cast::MessagePort::Client implementation.
  void OnMessage(const std::string& source_sender_id,
                 const std::string& message_namespace,
                 const std::string& message) override {
    receiver_messages_.push_back(
        {source_sender_id, message_namespace, message});
    if (receiver_message_closure_) {
      std::move(receiver_message_closure_).Run();
    }
  }
  void OnError(openscreen::Error error) override {
    latest_error_ = error;
    if (error_closure_) {
      std::move(error_closure_).Run();
    }
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  openscreen::Error latest_error_ = openscreen::Error::None();
  std::vector<CastMessage> receiver_messages_;
  base::OnceClosure receiver_message_closure_;
  base::OnceClosure error_closure_;
  base::OnceClosure cast_channel_closed_closure_;

  std::unique_ptr<CastMessagePortImpl> receiver_message_port_;
  std::unique_ptr<cast_api_bindings::MessagePort> sender_message_port_;
  cast_api_bindings::TestMessagePortReceiver sender_message_port_receiver_;
};

// Tests basic connection between the sender and receiver message port is
// working.
TEST_F(CastMessagePortImplTest, BasicConnection) {
  std::string sender_id;
  std::string message_namespace;
  std::string message;
  const std::string test_message = "testMessage";

  // Check the initial connect message is properly sent.
  sender_message_port_receiver_.RunUntilMessageCountEqual(1u);
  ASSERT_EQ(sender_message_port_receiver_.buffer().size(), 1u);
  ASSERT_TRUE(
      DeserializeCastMessage(sender_message_port_receiver_.buffer().at(0).first,
                             &sender_id, &message_namespace, &message));
  EXPECT_EQ(sender_id, kValueSystemSenderId);
  EXPECT_EQ(message_namespace, kSystemNamespace);
  EXPECT_EQ(message, kInitialConnectMessage);

  // Check the the connection from the sender to the receiver is working.
  sender_message_port_->PostMessage(
      SerializeCastMessage(kSenderId, kMirroringNamespace, test_message));
  RunUntilMessageCountIsAtLeast(1u);
  ASSERT_EQ(receiver_messages_.size(), 1u);
  EXPECT_EQ(receiver_messages_.at(0).sender_id, kSenderId);
  EXPECT_EQ(receiver_messages_.at(0).message_namespace, kMirroringNamespace);
  EXPECT_EQ(receiver_messages_.at(0).message, test_message);

  // Check the connection from the receiver to the sender is working.
  receiver_message_port_->PostMessage(kSenderId, kMirroringNamespace,
                                      test_message);
  sender_message_port_receiver_.RunUntilMessageCountEqual(2u);
  ASSERT_EQ(sender_message_port_receiver_.buffer().size(), 2u);
  ASSERT_TRUE(
      DeserializeCastMessage(sender_message_port_receiver_.buffer().at(1).first,
                             &sender_id, &message_namespace, &message));
  EXPECT_EQ(sender_id, kSenderId);
  EXPECT_EQ(message_namespace, kMirroringNamespace);
  EXPECT_EQ(message, test_message);
}

// Tests the "not supported" message is properly received for the inject
// message.
TEST_F(CastMessagePortImplTest, InjectMessage) {
  const int kRequestId = 42;
  base::Value inject_value(base::Value::Type::DICTIONARY);
  inject_value.SetKey(kKeyType, base::Value(kValueWrapped));
  inject_value.SetKey(kKeyRequestId, base::Value(kRequestId));
  std::string inject_message;
  ASSERT_TRUE(base::JSONWriter::Write(inject_value, &inject_message));

  sender_message_port_->PostMessage(
      SerializeCastMessage(kSenderId, kInjectNamespace, inject_message));
  sender_message_port_receiver_.RunUntilMessageCountEqual(2u);
  ASSERT_EQ(sender_message_port_receiver_.buffer().size(), 2u);

  std::string sender_id;
  std::string message_namespace;
  std::string message;
  ASSERT_TRUE(
      DeserializeCastMessage(sender_message_port_receiver_.buffer().at(1).first,
                             &sender_id, &message_namespace, &message));
  EXPECT_EQ(sender_id, kSenderId);
  EXPECT_EQ(message_namespace, kInjectNamespace);

  absl::optional<base::Value> return_value = base::JSONReader::Read(message);
  ASSERT_TRUE(return_value);
  ASSERT_TRUE(return_value->is_dict());

  const std::string* type_value = return_value->FindStringKey(kKeyType);
  ASSERT_TRUE(type_value);
  EXPECT_EQ(*type_value, kValueError);

  absl::optional<int> request_id_value =
      return_value->FindIntKey(kKeyRequestId);
  ASSERT_TRUE(request_id_value);
  EXPECT_EQ(request_id_value.value(), kRequestId);

  const std::string* data_value = return_value->FindStringKey(kKeyData);
  ASSERT_TRUE(data_value);
  EXPECT_EQ(*data_value, kValueInjectNotSupportedError);

  const std::string* code_value = return_value->FindStringKey(kKeyCode);
  ASSERT_TRUE(code_value);
  EXPECT_EQ(*code_value, kValueWrappedError);
}

// Tests sending a bad message properly reports an error to Open Screen without
// crashing.
TEST_F(CastMessagePortImplTest, BadMessage) {
  const std::string kBadMessage = "42";
  sender_message_port_->PostMessage(kBadMessage);
  RunUntilError();
  EXPECT_EQ(latest_error_,
            openscreen::Error(openscreen::Error::Code::kCastV2InvalidMessage));
}

// Tests closing the sender-end of the Cast Channel properly runs the closure.
TEST_F(CastMessagePortImplTest, CastChannelClosed) {
  sender_message_port_.reset();
  RunUntilCastChannelClosed();
}

}  // namespace cast_streaming
