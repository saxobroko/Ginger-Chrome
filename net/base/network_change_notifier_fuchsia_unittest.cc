// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_fuchsia.h"

#include <fuchsia/net/interfaces/cpp/fidl_test_base.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "net/base/ip_address.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/system_dns_config_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

enum : uint32_t { kDefaultInterfaceId = 1, kSecondaryInterfaceId = 2 };

using IPv4Octets = std::array<uint8_t, 4>;
using IPv6Octets = std::array<uint8_t, 16>;

constexpr IPv4Octets kDefaultIPv4Address = {192, 168, 0, 2};
constexpr uint8_t kDefaultIPv4Prefix = 16;
constexpr IPv4Octets kSecondaryIPv4Address = {10, 0, 0, 1};
constexpr uint8_t kSecondaryIPv4Prefix = 8;

constexpr IPv6Octets kDefaultIPv6Address = {0x20, 0x01, 0x01};
constexpr uint8_t kDefaultIPv6Prefix = 16;
constexpr IPv6Octets kSecondaryIPv6Address = {0x20, 0x01, 0x02};
constexpr uint8_t kSecondaryIPv6Prefix = 16;

fuchsia::net::IpAddress IpAddressFrom(IPv4Octets octets) {
  fuchsia::net::IpAddress output;
  output.ipv4().addr = octets;
  return output;
}

fuchsia::net::IpAddress IpAddressFrom(IPv6Octets octets) {
  fuchsia::net::IpAddress output;
  output.ipv6().addr = octets;
  return output;
}

template <typename T>
fuchsia::net::Subnet SubnetFrom(T octets, uint8_t prefix) {
  fuchsia::net::Subnet output;
  output.addr = IpAddressFrom(octets);
  output.prefix_len = prefix;
  return output;
}

template <typename T>
fuchsia::net::interfaces::Address InterfaceAddressFrom(T octets,
                                                       uint8_t prefix) {
  fuchsia::net::interfaces::Address addr;
  addr.set_addr(SubnetFrom(octets, prefix));
  return addr;
}

template <typename T>
std::vector<T> MakeSingleItemVec(T item) {
  std::vector<T> vec;
  vec.push_back(std::move(item));
  return vec;
}

fuchsia::net::interfaces::Properties DefaultInterfaceProperties(
    fuchsia::hardware::network::DeviceClass device_class =
        fuchsia::hardware::network::DeviceClass::UNKNOWN) {
  // For most tests a live interface with an IPv4 address and an unknown class
  // is sufficient.
  fuchsia::net::interfaces::Properties interface;
  interface.set_id(kDefaultInterfaceId);
  interface.set_online(true);
  interface.set_has_default_ipv4_route(true);
  interface.set_has_default_ipv6_route(true);
  interface.set_device_class(fuchsia::net::interfaces::DeviceClass::WithDevice(
      std::move(device_class)));
  interface.set_addresses(MakeSingleItemVec(
      InterfaceAddressFrom(kDefaultIPv4Address, kDefaultIPv4Prefix)));
  return interface;
}

fuchsia::net::interfaces::Properties SecondaryInterfaceProperties() {
  // For most tests a live interface with an IPv4 address and an unknown class
  // is sufficient.
  fuchsia::net::interfaces::Properties interface;
  interface.set_id(kSecondaryInterfaceId);
  interface.set_online(true);
  interface.set_has_default_ipv4_route(false);
  interface.set_has_default_ipv6_route(false);
  interface.set_device_class(fuchsia::net::interfaces::DeviceClass::WithDevice(
      fuchsia::hardware::network::DeviceClass::UNKNOWN));
  interface.set_addresses(MakeSingleItemVec(
      InterfaceAddressFrom(kSecondaryIPv4Address, kSecondaryIPv4Prefix)));
  return interface;
}

template <typename F>
fuchsia::net::interfaces::Event MakeChangeEvent(uint64_t interface_id, F fn) {
  fuchsia::net::interfaces::Properties props;
  props.set_id(interface_id);
  fn(&props);
  return fuchsia::net::interfaces::Event::WithChanged(std::move(props));
}

// Partial fake implementation of a fuchsia.net.interfaces/Watcher.
class FakeWatcher : public fuchsia::net::interfaces::testing::Watcher_TestBase {
 public:
  explicit FakeWatcher() : binding_(this) {
    // Always create the watcher with an empty set of interfaces.
    // Callers can override the initial set of events with SetInitial.
    pending_.push(fuchsia::net::interfaces::Event::WithIdle(
        fuchsia::net::interfaces::Empty{}));
  }
  FakeWatcher(const FakeWatcher&) = delete;
  FakeWatcher& operator=(const FakeWatcher&) = delete;
  ~FakeWatcher() override = default;

  void Bind(fidl::InterfaceRequest<fuchsia::net::interfaces::Watcher> request) {
    CHECK_EQ(ZX_OK, binding_.Bind(std::move(request)));
  }

  void PushEvent(fuchsia::net::interfaces::Event event) {
    if (pending_callback_) {
      pending_callback_(std::move(event));
      pending_callback_ = nullptr;
    } else {
      pending_.push(std::move(event));
    }
  }

  void SetInitial(std::vector<fuchsia::net::interfaces::Properties> props) {
    // Discard any pending events.
    pending_ = std::queue<fuchsia::net::interfaces::Event>();
    for (auto& prop : props) {
      pending_.push(
          fuchsia::net::interfaces::Event::WithExisting(std::move(prop)));
    }
    pending_.push(fuchsia::net::interfaces::Event::WithIdle(
        fuchsia::net::interfaces::Empty{}));
    // We should not have a pending callback already when setting initial state.
    CHECK(!pending_callback_);
  }

 private:
  void Watch(WatchCallback callback) override {
    ASSERT_FALSE(pending_callback_);
    if (pending_.empty()) {
      pending_callback_ = std::move(callback);
    } else {
      callback(std::move(pending_.front()));
      pending_.pop();
    }
  }

  void NotImplemented_(const std::string& name) override {
    LOG(FATAL) << "Unimplemented function called: " << name;
  }

  std::queue<fuchsia::net::interfaces::Event> pending_;
  fidl::Binding<fuchsia::net::interfaces::Watcher> binding_;
  WatchCallback pending_callback_ = nullptr;
};

class FakeWatcherAsync {
 public:
  explicit FakeWatcherAsync() {
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    CHECK(thread_.StartWithOptions(std::move(options)));
    watcher_ = base::SequenceBound<FakeWatcher>(thread_.task_runner());
  }
  FakeWatcherAsync(const FakeWatcherAsync&) = delete;
  FakeWatcherAsync& operator=(const FakeWatcherAsync&) = delete;
  ~FakeWatcherAsync() = default;

  void Bind(fidl::InterfaceRequest<fuchsia::net::interfaces::Watcher> request) {
    watcher_.AsyncCall(&FakeWatcher::Bind).WithArgs(std::move(request));
  }

  // Asynchronously push an event to the watcher.
  void PushEvent(fuchsia::net::interfaces::Event event) {
    watcher_.AsyncCall(&FakeWatcher::PushEvent).WithArgs(std::move(event));
  }

  // Asynchronously push an initial set of interfaces to the watcher.
  void SetInitial(std::vector<fuchsia::net::interfaces::Properties> props) {
    watcher_.AsyncCall(&FakeWatcher::SetInitial).WithArgs(std::move(props));
  }

  // Asynchronously push an initial single intface to the watcher.
  void SetInitial(fuchsia::net::interfaces::Properties prop) {
    SetInitial(MakeSingleItemVec(std::move(prop)));
  }

  // Ensures that any PushEvent() or SetInitial() calls have
  // been processed.
  void FlushThread() { thread_.FlushForTesting(); }

 private:
  base::Thread thread_{"Watcher Thread"};
  base::SequenceBound<FakeWatcher> watcher_;
};

template <class T>
class ResultReceiver {
 public:
  ~ResultReceiver() { EXPECT_EQ(entries_.size(), 0u); }
  bool RunAndExpectEntries(std::vector<T> expected_entries) {
    if (entries_.size() < expected_entries.size()) {
      base::RunLoop loop;
      base::AutoReset<size_t> size(&expected_count_, expected_entries.size());
      base::AutoReset<base::OnceClosure> quit(&quit_loop_, loop.QuitClosure());
      loop.Run();
    }
    return expected_entries == std::exchange(entries_, {});
  }
  void AddEntry(T entry) {
    entries_.push_back(entry);
    if (quit_loop_ && entries_.size() >= expected_count_)
      std::move(quit_loop_).Run();
  }

 protected:
  size_t expected_count_ = 0u;
  std::vector<T> entries_;
  base::OnceClosure quit_loop_;
};

// Accumulates the list of ConnectionTypes notified via OnConnectionTypeChanged.
class FakeConnectionTypeObserver
    : public NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  FakeConnectionTypeObserver() {
    NetworkChangeNotifier::AddConnectionTypeObserver(this);
  }
  ~FakeConnectionTypeObserver() final {
    NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  }

  bool RunAndExpectConnectionTypes(
      std::vector<NetworkChangeNotifier::ConnectionType> sequence) {
    return receiver_.RunAndExpectEntries(sequence);
  }

  // ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) final {
    receiver_.AddEntry(type);
  }

 protected:
  ResultReceiver<NetworkChangeNotifier::ConnectionType> receiver_;
};

// Accumulates the list of ConnectionTypes notified via OnConnectionTypeChanged.
class FakeNetworkChangeObserver
    : public NetworkChangeNotifier::NetworkChangeObserver {
 public:
  FakeNetworkChangeObserver() {
    NetworkChangeNotifier::AddNetworkChangeObserver(this);
  }
  ~FakeNetworkChangeObserver() final {
    NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

  bool RunAndExpectNetworkChanges(
      std::vector<NetworkChangeNotifier::ConnectionType> sequence) {
    return receiver_.RunAndExpectEntries(sequence);
  }

  // NetworkChangeObserver implementation.
  void OnNetworkChanged(NetworkChangeNotifier::ConnectionType type) final {
    receiver_.AddEntry(type);
  }

 protected:
  ResultReceiver<NetworkChangeNotifier::ConnectionType> receiver_;
};

// Accumulates the list of ConnectionTypes notified via OnConnectionTypeChanged.
class FakeIPAddressObserver : public NetworkChangeNotifier::IPAddressObserver {
 public:
  FakeIPAddressObserver() { NetworkChangeNotifier::AddIPAddressObserver(this); }
  ~FakeIPAddressObserver() final {
    NetworkChangeNotifier::RemoveIPAddressObserver(this);
    EXPECT_EQ(ip_change_count_, 0u);
  }

  size_t ip_change_count() const { return ip_change_count_; }

  bool RunAndExpectCallCount(size_t expected_count) {
    if (ip_change_count_ < expected_count) {
      base::RunLoop loop;
      base::AutoReset<size_t> expectation(&expected_count_, expected_count);
      base::AutoReset<base::OnceClosure> quit(&quit_loop_, loop.QuitClosure());
      loop.Run();
    }
    return std::exchange(ip_change_count_, 0u) == expected_count;
  }

  // IPAddressObserver implementation.
  void OnIPAddressChanged() final {
    ip_change_count_++;
    if (quit_loop_ && ip_change_count_ >= expected_count_)
      std::move(quit_loop_).Run();
  }

 protected:
  size_t expected_count_ = 0u;
  size_t ip_change_count_ = 0u;
  base::OnceClosure quit_loop_;
};

}  // namespace

class NetworkChangeNotifierFuchsiaTest : public testing::Test {
 public:
  NetworkChangeNotifierFuchsiaTest() = default;
  NetworkChangeNotifierFuchsiaTest(const NetworkChangeNotifierFuchsiaTest&) =
      delete;
  NetworkChangeNotifierFuchsiaTest& operator=(
      const NetworkChangeNotifierFuchsiaTest&) = delete;
  ~NetworkChangeNotifierFuchsiaTest() override = default;

  // Creates a NetworkChangeNotifier that binds to |watcher_|.
  // |observer_| is registered last, so that tests need only express
  // expectations on changes they make themselves.
  void CreateNotifier(bool requires_wlan = false) {
    // Ensure that internal state is up-to-date before the
    // notifier queries it.
    watcher_.FlushThread();

    CHECK(!watcher_handle_);
    watcher_.Bind(watcher_handle_.NewRequest());

    // Use a noop DNS notifier.
    dns_config_notifier_ = std::make_unique<SystemDnsConfigChangeNotifier>(
        nullptr /* task_runner */, nullptr /* dns_config_service */);
    notifier_.reset(new NetworkChangeNotifierFuchsia(
        std::move(watcher_handle_), requires_wlan, dns_config_notifier_.get()));

    type_observer_ = std::make_unique<FakeConnectionTypeObserver>();
    ip_observer_ = std::make_unique<FakeIPAddressObserver>();
  }

  void TearDown() override {
    // Spin the loops to catch any unintended notifications.
    watcher_.FlushThread();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  fidl::InterfaceHandle<fuchsia::net::interfaces::Watcher> watcher_handle_;
  FakeWatcherAsync watcher_;

  // Allows us to allocate our own NetworkChangeNotifier for unit testing.
  NetworkChangeNotifier::DisableForTest disable_for_test_;
  std::unique_ptr<SystemDnsConfigChangeNotifier> dns_config_notifier_;
  std::unique_ptr<NetworkChangeNotifierFuchsia> notifier_;

  std::unique_ptr<FakeConnectionTypeObserver> type_observer_;
  std::unique_ptr<FakeIPAddressObserver> ip_observer_;
};

TEST_F(NetworkChangeNotifierFuchsiaTest, InitialState) {
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
            notifier_->GetCurrentConnectionType());
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfacesChangeDuringConstruction) {
  // Set a live interface with an IP address.
  watcher_.SetInitial(DefaultInterfaceProperties(
      fuchsia::hardware::network::DeviceClass::WLAN));

  // Inject an interfaces change event so that the notifier will receive it
  // immediately after the initial state.
  watcher_.PushEvent(MakeChangeEvent(
      kDefaultInterfaceId, [](fuchsia::net::interfaces::Properties* props) {
        props->set_addresses(MakeSingleItemVec(
            InterfaceAddressFrom(kSecondaryIPv4Address, kSecondaryIPv4Prefix)));
      }));

  // Create the Notifier, which should process the initial network state before
  // returning, but not the change event, yet.
  CreateNotifier();
  EXPECT_EQ(ip_observer_->ip_change_count(), 0u);

  // Now spin the loop to allow the change event to be processed, triggering a
  // call to the |ip_observer_|.
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, NotifyNetworkChangeOnInitialIPChange) {
  // Set a live interface with an IP address and create the notifier.
  watcher_.SetInitial(DefaultInterfaceProperties(
      fuchsia::hardware::network::DeviceClass::WLAN));
  CreateNotifier();

  // Add the NetworkChangeNotifier, and change the IP address. This should
  // trigger a network change notification.
  FakeNetworkChangeObserver network_change_observer;

  watcher_.PushEvent(MakeChangeEvent(
      kDefaultInterfaceId, [](fuchsia::net::interfaces::Properties* props) {
        props->set_addresses(MakeSingleItemVec(
            InterfaceAddressFrom(kSecondaryIPv4Address, kSecondaryIPv4Prefix)));
      }));

  EXPECT_TRUE(network_change_observer.RunAndExpectNetworkChanges(
      {NetworkChangeNotifier::CONNECTION_NONE,
       NetworkChangeNotifier::CONNECTION_WIFI}));
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, NoChange) {
  // Set a live interface with an IP address and create the notifier.
  watcher_.SetInitial(DefaultInterfaceProperties());
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());
  // Push an event with no side-effects.
  watcher_.PushEvent(MakeChangeEvent(kDefaultInterfaceId, [](auto*) {}));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, NoChangeV6) {
  auto initial = DefaultInterfaceProperties();
  initial.set_addresses(MakeSingleItemVec(
      InterfaceAddressFrom(kDefaultIPv6Address, kDefaultIPv6Prefix)));
  watcher_.SetInitial(std::move(initial));
  CreateNotifier();
  // Push an event with no side-effects.
  watcher_.PushEvent(MakeChangeEvent(kDefaultInterfaceId, [](auto*) {}));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, MultiInterfaceNoChange) {
  std::vector<fuchsia::net::interfaces::Properties> props;
  props.push_back(DefaultInterfaceProperties());
  props.push_back(SecondaryInterfaceProperties());
  watcher_.SetInitial(std::move(props));
  CreateNotifier();
  // Push an event with no side-effects.
  watcher_.PushEvent(MakeChangeEvent(kDefaultInterfaceId, [](auto*) {}));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, MultiV6IPNoChange) {
  auto props = DefaultInterfaceProperties();
  props.mutable_addresses()->push_back(
      InterfaceAddressFrom(kDefaultIPv6Address, kDefaultIPv6Prefix));
  props.mutable_addresses()->push_back(
      InterfaceAddressFrom(kSecondaryIPv6Address, kSecondaryIPv6Prefix));

  watcher_.SetInitial(std::move(props));
  CreateNotifier();

  // Push an event with no side-effects.
  watcher_.PushEvent(MakeChangeEvent(kDefaultInterfaceId, [](auto*) {}));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, IpChange) {
  watcher_.SetInitial(DefaultInterfaceProperties());
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  watcher_.PushEvent(MakeChangeEvent(
      kDefaultInterfaceId, [](fuchsia::net::interfaces::Properties* props) {
        props->set_addresses(MakeSingleItemVec(
            InterfaceAddressFrom(kSecondaryIPv4Address, kSecondaryIPv4Prefix)));
      }));

  // Expect a single OnIPAddressChanged() notification.
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, IpChangeV6) {
  auto props = DefaultInterfaceProperties();
  props.set_addresses(MakeSingleItemVec(
      InterfaceAddressFrom(kDefaultIPv6Address, kDefaultIPv6Prefix)));
  watcher_.SetInitial(std::move(props));
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  watcher_.PushEvent(MakeChangeEvent(
      kDefaultInterfaceId, [](fuchsia::net::interfaces::Properties* props) {
        props->set_addresses(MakeSingleItemVec(
            InterfaceAddressFrom(kSecondaryIPv6Address, kSecondaryIPv6Prefix)));
      }));

  // Expect a single OnIPAddressChanged() notification.
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, MultiV6IPChanged) {
  auto props = DefaultInterfaceProperties();
  props.mutable_addresses()->push_back(
      InterfaceAddressFrom(kDefaultIPv6Address, kDefaultIPv6Prefix));

  watcher_.SetInitial(std::move(props));
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  watcher_.PushEvent(MakeChangeEvent(
      kDefaultInterfaceId, [](fuchsia::net::interfaces::Properties* props) {
        std::vector<fuchsia::net::interfaces::Address> addrs;
        addrs.push_back(
            InterfaceAddressFrom(kSecondaryIPv4Address, kSecondaryIPv4Prefix));
        addrs.push_back(
            InterfaceAddressFrom(kSecondaryIPv6Address, kSecondaryIPv6Prefix));
        props->set_addresses(std::move(addrs));
      }));

  // Expect a single OnIPAddressChanged() notification.
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, Ipv6AdditionalIpChange) {
  watcher_.SetInitial(DefaultInterfaceProperties());
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  watcher_.PushEvent(MakeChangeEvent(
      kDefaultInterfaceId, [](fuchsia::net::interfaces::Properties* props) {
        // Add the initial default address + a new IPv6 one. Address changes are
        // always sent as the entire new list of addresses.
        props->mutable_addresses()->push_back(
            InterfaceAddressFrom(kDefaultIPv4Address, kDefaultIPv4Prefix));
        props->mutable_addresses()->push_back(
            InterfaceAddressFrom(kDefaultIPv6Address, kDefaultIPv6Prefix));
      }));

  // Expect a single OnIPAddressChanged() notification.
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceDown) {
  watcher_.SetInitial(DefaultInterfaceProperties());
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  watcher_.PushEvent(MakeChangeEvent(
      kDefaultInterfaceId, [](fuchsia::net::interfaces::Properties* props) {
        props->set_online(false);
      }));

  EXPECT_TRUE(type_observer_->RunAndExpectConnectionTypes(
      {NetworkChangeNotifier::ConnectionType::CONNECTION_NONE}));
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceUp) {
  auto props = DefaultInterfaceProperties();
  props.set_online(false);
  watcher_.SetInitial(std::move(props));
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
            notifier_->GetCurrentConnectionType());

  watcher_.PushEvent(MakeChangeEvent(
      kDefaultInterfaceId, [](fuchsia::net::interfaces::Properties* props) {
        props->set_online(true);
      }));

  EXPECT_TRUE(type_observer_->RunAndExpectConnectionTypes(
      {NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN}));
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceDeleted) {
  watcher_.SetInitial(DefaultInterfaceProperties());
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
            notifier_->GetCurrentConnectionType());

  watcher_.PushEvent(
      fuchsia::net::interfaces::Event::WithRemoved(kDefaultInterfaceId));

  EXPECT_TRUE(type_observer_->RunAndExpectConnectionTypes(
      {NetworkChangeNotifier::ConnectionType::CONNECTION_NONE}));
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, InterfaceAdded) {
  // Initial interface list is intentionally left empty.
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
            notifier_->GetCurrentConnectionType());

  watcher_.PushEvent(
      fuchsia::net::interfaces::Event::WithAdded(DefaultInterfaceProperties(
          fuchsia::hardware::network::DeviceClass::WLAN)));

  EXPECT_TRUE(type_observer_->RunAndExpectConnectionTypes(
      {NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI}));
  EXPECT_TRUE(ip_observer_->RunAndExpectCallCount(1));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, SecondaryInterfaceAddedNoop) {
  watcher_.SetInitial(DefaultInterfaceProperties());
  CreateNotifier();

  watcher_.PushEvent(fuchsia::net::interfaces::Event::WithAdded(
      SecondaryInterfaceProperties()));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, SecondaryInterfaceDeletedNoop) {
  std::vector<fuchsia::net::interfaces::Properties> interfaces;
  interfaces.push_back(DefaultInterfaceProperties());
  interfaces.push_back(SecondaryInterfaceProperties());

  watcher_.SetInitial(std::move(interfaces));
  CreateNotifier();

  watcher_.PushEvent(
      fuchsia::net::interfaces::Event::WithRemoved(kSecondaryInterfaceId));
}

TEST_F(NetworkChangeNotifierFuchsiaTest, FoundWiFi) {
  watcher_.SetInitial(DefaultInterfaceProperties(
      fuchsia::hardware::network::DeviceClass::WLAN));
  CreateNotifier();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
            notifier_->GetCurrentConnectionType());
}

TEST_F(NetworkChangeNotifierFuchsiaTest, FindsInterfaceWithRequiredWlan) {
  watcher_.SetInitial(DefaultInterfaceProperties(
      fuchsia::hardware::network::DeviceClass::WLAN));
  CreateNotifier(/*require_wlan=*/true);
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
            notifier_->GetCurrentConnectionType());
}

TEST_F(NetworkChangeNotifierFuchsiaTest, IgnoresNonWlanInterface) {
  watcher_.SetInitial(DefaultInterfaceProperties());
  CreateNotifier(/*require_wlan=*/true);
  EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_NONE,
            notifier_->GetCurrentConnectionType());
}

}  // namespace net
