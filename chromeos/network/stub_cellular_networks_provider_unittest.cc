// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/stub_cellular_networks_provider.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/network/cellular_utils.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/network/test_cellular_esim_profile_handler.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {
namespace {

const char kDefaultCellularDevicePath[] = "stub_cellular_device";
const char kTestEuiccBasePath[] = "/org/chromium/Hermes/Euicc/";
const char kTestBaseEid[] = "12345678901234567890123456789012";
const char kTestPSimIccid[] = "1234567890";
const char kTestCellularServicePath[] = "/service/cellular";

std::string CreateTestEuiccPath(int euicc_num) {
  return base::StringPrintf("%s%d", kTestEuiccBasePath, euicc_num);
}

std::string CreateTestEid(int euicc_num) {
  return base::StringPrintf("%s%d", kTestBaseEid, euicc_num);
}

}  // namespace

class StubCellularNetworksProviderTest : public testing::Test {
 protected:
  StubCellularNetworksProviderTest()
      : helper_(/*use_default_devices_and_services=*/false) {}

  ~StubCellularNetworksProviderTest() override = default;

  // testing::Test:
  void SetUp() override {
    helper_.device_test()->AddDevice(kDefaultCellularDevicePath,
                                     shill::kTypeCellular, "cellular1");
  }

  void Init() {
    handler_.Init(helper_.network_state_handler(),
                  /*cellular_inhibitor=*/nullptr);

    provider_ = std::make_unique<StubCellularNetworksProvider>();
    provider_->Init(helper_.network_state_handler(), &handler_);
  }

  void AddEuicc(int euicc_num) {
    std::string euicc_path = CreateTestEuiccPath(euicc_num);

    helper_.hermes_manager_test()->AddEuicc(
        dbus::ObjectPath(euicc_path), CreateTestEid(euicc_num),
        /*is_active=*/true, /*physical_slot=*/0);
    base::RunLoop().RunUntilIdle();
  }

  dbus::ObjectPath AddProfile(int euicc_num,
                              hermes::profile::State state,
                              const std::string& activation_code) {
    dbus::ObjectPath path = helper_.hermes_euicc_test()->AddFakeCarrierProfile(
        dbus::ObjectPath(CreateTestEuiccPath(euicc_num)), state,
        activation_code,
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);
    base::RunLoop().RunUntilIdle();
    return path;
  }

  bool AddOrRemoveStubCellularNetworks(
      NetworkStateHandler::ManagedStateList& network_list,
      NetworkStateHandler::ManagedStateList& new_stub_networks) {
    const DeviceState* device_state =
        helper_.network_state_handler()->GetDeviceStateByType(
            NetworkTypePattern::Cellular());
    return provider_->AddOrRemoveStubCellularNetworks(
        network_list, new_stub_networks, device_state);
  }

  void SetPSimSlotInfo(const std::string& iccid) {
    base::Value::ListStorage sim_slot_infos;
    base::Value slot_info_item(base::Value::Type::DICTIONARY);
    slot_info_item.SetStringKey(shill::kSIMSlotInfoEID, std::string());
    slot_info_item.SetStringKey(shill::kSIMSlotInfoICCID, iccid);
    slot_info_item.SetBoolKey(shill::kSIMSlotInfoPrimary, true);
    sim_slot_infos.push_back(std::move(slot_info_item));

    helper_.device_test()->SetDeviceProperty(kDefaultCellularDevicePath,
                                             shill::kSIMSlotInfoProperty,
                                             base::Value(sim_slot_infos),
                                             /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void CallGetStubNetworkMetadata(const std::string& iccid, bool should_exist) {
    std::string service_path, guid;
    bool exists = provider_->GetStubNetworkMetadata(
        iccid,
        helper_.network_state_handler()->GetDeviceStateByType(
            NetworkTypePattern::Cellular()),
        &service_path, &guid);

    if (!should_exist) {
      EXPECT_FALSE(exists);
      return;
    }

    EXPECT_TRUE(exists);
    EXPECT_EQ(GenerateStubCellularServicePath(iccid), service_path);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper helper_;
  TestCellularESimProfileHandler handler_;

  std::unique_ptr<StubCellularNetworksProvider> provider_;
};

TEST_F(StubCellularNetworksProviderTest, AddOrRemoveStubCellularNetworks) {
  SetPSimSlotInfo(kTestPSimIccid);
  AddEuicc(/*euicc_num=*/1);
  Init();
  dbus::ObjectPath profile1_path =
      AddProfile(/*euicc_num=*/1, hermes::profile::State::kPending,
                 /*activation_code=*/"code1");
  dbus::ObjectPath profile2_path =
      AddProfile(/*euicc_num=*/1, hermes::profile::State::kInactive,
                 /*activation_code=*/"code1");
  HermesProfileClient::Properties* profile1_properties =
      HermesProfileClient::Get()->GetProperties(profile1_path);
  HermesProfileClient::Properties* profile2_properties =
      HermesProfileClient::Get()->GetProperties(profile2_path);

  CallGetStubNetworkMetadata(profile1_properties->iccid().value(),
                             /*should_exist=*/false);
  CallGetStubNetworkMetadata(profile2_properties->iccid().value(),
                             /*should_exist=*/true);
  CallGetStubNetworkMetadata(kTestPSimIccid, /*should_exist=*/true);
  CallGetStubNetworkMetadata("nonexistent_iccid", /*should_exist=*/false);

  NetworkStateHandler::ManagedStateList network_list, new_stub_networks;

  // Verify that stub services are created for eSIM profiles and pSIM iccids
  // on sim slot info.
  AddOrRemoveStubCellularNetworks(network_list, new_stub_networks);
  EXPECT_EQ(2u, new_stub_networks.size());
  NetworkState* network1 = new_stub_networks[0]->AsNetworkState();
  NetworkState* network2 = new_stub_networks[1]->AsNetworkState();
  EXPECT_TRUE(network1->IsNonShillCellularNetwork());
  EXPECT_TRUE(network2->IsNonShillCellularNetwork());
  EXPECT_EQ(network1->iccid(), profile2_properties->iccid().value());
  EXPECT_EQ(network2->iccid(), kTestPSimIccid);

  // Verify the stub networks are removed when corresponding slot is no longer
  // present. e.g. SIM removed.
  network_list = std::move(new_stub_networks);
  new_stub_networks.clear();
  SetPSimSlotInfo(/*iccid=*/std::string());
  base::RunLoop().RunUntilIdle();
  AddOrRemoveStubCellularNetworks(network_list, new_stub_networks);
  EXPECT_EQ(1u, network_list.size());

  // Verify that stub networks are removed when real networks are added to the
  // list.
  std::unique_ptr<NetworkState> test_network =
      std::make_unique<NetworkState>(kTestCellularServicePath);
  test_network->PropertyChanged(shill::kTypeProperty,
                                base::Value(shill::kTypeCellular));
  test_network->PropertyChanged(
      shill::kIccidProperty, base::Value(profile2_properties->iccid().value()));
  test_network->set_update_received();
  network_list.push_back(std::move(test_network));
  AddOrRemoveStubCellularNetworks(network_list, new_stub_networks);
  EXPECT_EQ(1u, network_list.size());
}

}  // namespace chromeos
