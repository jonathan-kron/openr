/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <folly/FileUtil.h>
#include <folly/experimental/TestUtil.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

#define Config_TEST_FRIENDS FRIEND_TEST(ConfigTest, PopulateInternalDb);

#include <openr/common/Constants.h>
#include <openr/common/MplsUtil.h>
#include <openr/config/Config.h>
#include <openr/if/gen-cpp2/BgpConfig_types.h>
#include <openr/if/gen-cpp2/Network_types.h>
#include <openr/if/gen-cpp2/OpenrConfig_types.h>
#include <openr/tests/utils/Utils.h>

#include <thrift/lib/cpp2/protocol/Serializer.h>

namespace {
const auto& testSeedPrefix =
    folly::IPAddress::createNetwork("fc00:cafe:babe::/64");
const uint8_t testAllocationPfxLen = 128;

openr::thrift::KvstoreFloodRate
getFloodRate() {
  openr::thrift::KvstoreFloodRate floodrate;
  floodrate.flood_msg_per_sec() = 1;
  floodrate.flood_msg_burst_size() = 1;
  return floodrate;
}

openr::thrift::PrefixAllocationConfig
getPrefixAllocationConfig(openr::thrift::PrefixAllocationMode mode) {
  openr::thrift::PrefixAllocationConfig pfxAllocationConf;
  pfxAllocationConf.prefix_allocation_mode() = mode;
  if (mode == openr::thrift::PrefixAllocationMode::DYNAMIC_ROOT_NODE) {
    pfxAllocationConf.seed_prefix() =
        folly::IPAddress::networkToString(testSeedPrefix);
    pfxAllocationConf.allocate_prefix_len() = testAllocationPfxLen;
  }
  return pfxAllocationConf;
}

openr::thrift::AreaConfig
getAreaConfig(const std::string& areaId) {
  openr::thrift::AreaConfig area;
  area.area_id() = areaId;
  area.include_interface_regexes()->emplace_back("fboss.*");
  area.neighbor_regexes()->emplace_back("rsw.*");
  return area;
}

openr::thrift::SegmentRoutingConfig
getSegmentRoutingConfig() {
  openr::thrift::SegmentRoutingConfig segment_routing_config;
  openr::thrift::SegmentRoutingAdjLabel adj_segment_label;
  openr::thrift::MplsLabelRanges prepend_labels;

  openr::thrift::LabelRange lrp4;
  openr::thrift::LabelRange lrp6;

  // prepend labels
  lrp4.start_label() = openr::MplsConstants::kSrV4StaticMplsRouteRange.first;
  lrp4.end_label() = openr::MplsConstants::kSrV4StaticMplsRouteRange.second;
  lrp6.start_label() = openr::MplsConstants::kSrV6StaticMplsRouteRange.first;
  lrp6.end_label() = openr::MplsConstants::kSrV6StaticMplsRouteRange.second;
  prepend_labels.v4() = lrp4;
  prepend_labels.v6() = lrp6;

  lrp4.start_label() = openr::MplsConstants::kSrV4StaticMplsRouteRange.first;
  lrp4.end_label() = openr::MplsConstants::kSrV4StaticMplsRouteRange.second;

  lrp6.start_label() = openr::MplsConstants::kSrV6StaticMplsRouteRange.first;
  lrp6.end_label() = openr::MplsConstants::kSrV6StaticMplsRouteRange.second;

  // adj segment label range
  openr::thrift::LabelRange adj_label_range;
  adj_label_range.start_label() = openr::MplsConstants::kSrLocalRange.first;
  adj_label_range.end_label() = openr::MplsConstants::kSrLocalRange.second;
  adj_segment_label.sr_adj_label_type() =
      openr::thrift::SegmentRoutingAdjLabelType::AUTO_IFINDEX;
  adj_segment_label.adj_label_range() = adj_label_range;

  // segment routing config
  segment_routing_config.sr_adj_label() = adj_segment_label;
  segment_routing_config.prepend_label_ranges() = prepend_labels;

  return segment_routing_config;
}

const std::string myArea = "myArea";

} // namespace

namespace openr {

class ConfigTestFixture : public ::testing::Test {
 public:
  void
  SetUp() override {
    validConfig_ = getBasicOpenrConfig();

    auto jsonSerializer = apache::thrift::SimpleJSONSerializer();
    try {
      jsonSerializer.serialize(validConfig_, &validConfigStr_);
    } catch (const std::exception& ex) {
      LOG(ERROR) << "Could not serialize config: " << folly::exceptionStr(ex);
    }

    folly::writeFileAtomic(validConfigFile_.path().string(), validConfigStr_);
  }

 protected:
  thrift::OpenrConfig validConfig_;
  std::string validConfigStr_;
  folly::test::TemporaryFile validConfigFile_;
};

TEST_F(ConfigTestFixture, ConstructFromFile) {
  EXPECT_NO_THROW(Config(validConfigFile_.path().string()));

  // thrift format error
  {
    folly::dynamic invalidConfig;
    invalidConfig = folly::parseJson(validConfigStr_);
    invalidConfig["areas"] = "This should be a vector";
    LOG(INFO) << invalidConfig;

    folly::test::TemporaryFile invalidConfigFile;
    folly::writeFileAtomic(
        invalidConfigFile.path().string(), folly::toJson(invalidConfig));
    EXPECT_ANY_THROW(Config(invalidConfigFile.path().string()));
  }

  // out of range enum: prefix_allocation_mode
  {
    auto validTConf = getBasicOpenrConfig();
    validTConf.enable_prefix_allocation() = true;
    validTConf.prefix_allocation_config() = thrift::PrefixAllocationConfig();

    std::string validConfStr;
    EXPECT_NO_THROW(apache::thrift::SimpleJSONSerializer().serialize(
        validTConf, &validConfStr));

    folly::dynamic invalidConf = folly::parseJson(validConfStr);
    // prefix_allocation_mode range [0-2]
    invalidConf["prefix_allocation_config"]["prefix_allocation_mode"] = 3;

    folly::test::TemporaryFile invalidConfFile;
    folly::writeFileAtomic(
        invalidConfFile.path().string(), folly::toJson(invalidConf));
    EXPECT_ANY_THROW(Config(invalidConfFile.path().string()));
  }
  // out of range enum: prefix_forwarding_type
  {
    folly::dynamic invalidConfig;
    invalidConfig = folly::parseJson(validConfigStr_);
    invalidConfig["prefix_forwarding_type"] = 3;
    LOG(INFO) << invalidConfig;

    folly::test::TemporaryFile invalidConfigFile;
    folly::writeFileAtomic(
        invalidConfigFile.path().string(), folly::toJson(invalidConfig));
    EXPECT_ANY_THROW(Config(invalidConfigFile.path().string()));
  }
}

TEST(ConfigTest, PopulateAreaConfig) {
  // area

  // duplicate area id
  {
    auto confInvalidArea = getBasicOpenrConfig();
    confInvalidArea.areas()->emplace_back(getAreaConfig("1"));
    confInvalidArea.areas()->emplace_back(getAreaConfig("1"));
    EXPECT_THROW((Config(confInvalidArea)), std::invalid_argument);
  }
  // cannot find policy definition for area policy
  {
    auto confInvalidAreaPolicy = getBasicOpenrConfig();
    auto areaConfig = getAreaConfig("1");
    areaConfig.import_policy_name() = "BLA";
    confInvalidAreaPolicy.areas()->emplace_back(std::move(areaConfig));
    EXPECT_THROW((Config(confInvalidAreaPolicy)), std::invalid_argument);
  }

  // non-empty interface regex
  {
    openr::thrift::AreaConfig areaConfig;
    areaConfig.area_id() = myArea;
    areaConfig.include_interface_regexes()->emplace_back("iface.*");
    std::vector<openr::thrift::AreaConfig> vec = {areaConfig};
    auto confValidArea = getBasicOpenrConfig("node-1", vec);
    EXPECT_NO_THROW((Config(confValidArea)));
  }

  // non-empty neighbor regexes
  {
    openr::thrift::AreaConfig areaConfig;
    areaConfig.area_id() = myArea;
    areaConfig.neighbor_regexes()->emplace_back("fsw.*");
    std::vector<openr::thrift::AreaConfig> vec = {areaConfig};
    auto confValidArea = getBasicOpenrConfig("node-1", vec);
    EXPECT_NO_THROW((Config(confValidArea)));
  }

  // non-empty neighbor and interface regexes
  {
    openr::thrift::AreaConfig areaConfig;
    areaConfig.area_id() = myArea;
    areaConfig.include_interface_regexes()->emplace_back("iface.*");
    areaConfig.neighbor_regexes()->emplace_back("fsw.*");
    std::vector<openr::thrift::AreaConfig> vec = {areaConfig};
    auto confValidArea = getBasicOpenrConfig("node-1", vec);
    EXPECT_NO_THROW((Config(confValidArea)));
  }

  {
    openr::thrift::AreaConfig areaConfig;
    areaConfig.area_id() = myArea;
    areaConfig.include_interface_regexes()->emplace_back("iface.*");
    areaConfig.neighbor_regexes()->emplace_back("fsw.*");
    std::vector<openr::thrift::AreaConfig> vec = {areaConfig};
    auto confValidArea = getBasicOpenrConfig("node-1", vec);
    Config cfg = Config(confValidArea);
    // default area and domain area
    EXPECT_EQ(cfg.getAreas().size(), 1);
    EXPECT_EQ(cfg.getAreas().count(myArea), 1);
    EXPECT_EQ(cfg.getAreas().count("1"), 0);
  }

  // invalid include_interface_regexes
  {
    openr::thrift::AreaConfig areaConfig;
    areaConfig.area_id() = myArea;
    areaConfig.include_interface_regexes()->emplace_back("[0-9]++");
    std::vector<openr::thrift::AreaConfig> vec = {areaConfig};
    auto conf = getBasicOpenrConfig("node-1", vec);
    EXPECT_THROW(auto c = Config(conf), std::invalid_argument);
  }
  //  invalid exclude_interface_regexes
  {
    openr::thrift::AreaConfig areaConfig;
    *areaConfig.area_id() = myArea;
    areaConfig.exclude_interface_regexes()->emplace_back("boom\\");
    std::vector<openr::thrift::AreaConfig> vec = {areaConfig};
    auto conf = getBasicOpenrConfig("node-1", vec);
    EXPECT_THROW(auto c = Config(conf), std::invalid_argument);
  }
  //  invalid redistribute_interface_regexes
  {
    openr::thrift::AreaConfig areaConfig;
    areaConfig.area_id() = myArea;
    areaConfig.redistribute_interface_regexes()->emplace_back("*");
    std::vector<openr::thrift::AreaConfig> vec = {areaConfig};
    auto conf = getBasicOpenrConfig("node-1", vec);
    EXPECT_THROW(auto c = Config(conf), std::invalid_argument);
  }

  // area segment node label
  {
    auto confAreaPolicy = getBasicOpenrConfig();
    openr::thrift::AreaConfig areaConfig = getAreaConfig("1");

    openr::thrift::SegmentRoutingNodeLabel node_segment_label;
    areaConfig.area_sr_node_label() = node_segment_label;
    confAreaPolicy.areas()->emplace_back(areaConfig);

    // Area config with incomplete segment node label config
    EXPECT_THROW((Config(confAreaPolicy)), std::invalid_argument);

    openr::thrift::LabelRange node_segment_label_range;
    node_segment_label_range.start_label() =
        openr::MplsConstants::kSrGlobalRange.first;
    node_segment_label_range.end_label() =
        openr::MplsConstants::kSrGlobalRange.second;
    node_segment_label.node_segment_label_range() = node_segment_label_range;

    // Type is AUTO
    node_segment_label.sr_node_label_type() =
        openr::thrift::SegmentRoutingNodeLabelType::AUTO;

    for (auto& areaConf : *confAreaPolicy.areas()) {
      areaConf.area_sr_node_label() = node_segment_label;
    }

    // valid node segment label config
    EXPECT_NO_THROW((Config(confAreaPolicy)));

    // invalid label range and type is AUTO
    node_segment_label_range.end_label() =
        openr::MplsConstants::kSrGlobalRange.first;
    node_segment_label_range.start_label() =
        openr::MplsConstants::kSrGlobalRange.second;
    node_segment_label.node_segment_label_range() = node_segment_label_range;
    for (auto& areaConf : *confAreaPolicy.areas()) {
      areaConf.area_sr_node_label() = node_segment_label;
    }
    EXPECT_THROW((Config(confAreaPolicy)), std::invalid_argument);

    // Type is STATIC but no static label provided
    node_segment_label.sr_node_label_type() =
        openr::thrift::SegmentRoutingNodeLabelType::STATIC;
    for (auto& areaConf : *confAreaPolicy.areas()) {
      areaConf.area_sr_node_label() = node_segment_label;
    }

    EXPECT_THROW((Config(confAreaPolicy)), std::invalid_argument);
  }

  // area prepend label
  {
    auto confAreaPolicy = getBasicOpenrConfig();
    openr::thrift::AreaConfig areaConfig = getAreaConfig("1");

    openr::thrift::MplsLabelRanges prepend_labels;
    openr::thrift::LabelRange lrp4;
    openr::thrift::LabelRange lrp6;

    // prepend labels
    lrp4.start_label() = openr::MplsConstants::kSrV4StaticMplsRouteRange.first;
    lrp4.end_label() = openr::MplsConstants::kSrV4StaticMplsRouteRange.second;
    lrp6.start_label() = openr::MplsConstants::kSrV6StaticMplsRouteRange.first;
    lrp6.end_label() = openr::MplsConstants::kSrV6StaticMplsRouteRange.second;
    prepend_labels.v4() = lrp4;
    prepend_labels.v6() = lrp6;

    lrp4.start_label() = openr::MplsConstants::kSrV4StaticMplsRouteRange.first;
    lrp4.end_label() = openr::MplsConstants::kSrV4StaticMplsRouteRange.second;

    lrp6.start_label() = openr::MplsConstants::kSrV6StaticMplsRouteRange.first;
    lrp6.end_label() = openr::MplsConstants::kSrV6StaticMplsRouteRange.second;

    areaConfig.prepend_label_ranges() = prepend_labels;

    // valid prepend label config
    confAreaPolicy.areas()->emplace_back(areaConfig);
    EXPECT_NO_THROW((Config(confAreaPolicy)));
  }

  // area adjacency label
  {
    auto confAreaPolicy = getBasicOpenrConfig();
    openr::thrift::AreaConfig areaConfig = getAreaConfig("1");

    openr::thrift::SegmentRoutingAdjLabel adj_segment_label;
    openr::thrift::LabelRange adj_label_range;
    adj_label_range.start_label() = openr::MplsConstants::kSrLocalRange.first;
    adj_label_range.end_label() = openr::MplsConstants::kSrLocalRange.second;
    adj_segment_label.sr_adj_label_type() =
        openr::thrift::SegmentRoutingAdjLabelType::AUTO_IFINDEX;
    adj_segment_label.adj_label_range() = adj_label_range;

    areaConfig.sr_adj_label() = adj_segment_label;

    // valid adj label config
    confAreaPolicy.areas()->emplace_back(areaConfig);
    EXPECT_NO_THROW((Config(confAreaPolicy)));

    // disabled adj label
    (*confAreaPolicy.areas()).clear();
    adj_segment_label.sr_adj_label_type() =
        openr::thrift::SegmentRoutingAdjLabelType::AUTO_IFINDEX;
    EXPECT_NO_THROW((Config(confAreaPolicy)));
  }
}

TEST(ConfigTest, AreaConfiguration) {
  openr::thrift::AreaConfig areaConfig;
  areaConfig.area_id() = "myArea";
  areaConfig.include_interface_regexes()->emplace_back("iface.*");
  areaConfig.exclude_interface_regexes()->emplace_back(".*400.*");
  areaConfig.exclude_interface_regexes()->emplace_back(".*450.*");
  areaConfig.redistribute_interface_regexes()->emplace_back("loopback1");
  areaConfig.neighbor_regexes()->emplace_back("fsw.*");
  Config cfg{getBasicOpenrConfig("node-1", {areaConfig})};

  auto const& areaConf = cfg.getAreas().at("myArea");
  EXPECT_TRUE(areaConf.shouldPeerWithNeighbor("fsw001"));
  EXPECT_FALSE(areaConf.shouldPeerWithNeighbor("rsw001"));
  EXPECT_FALSE(areaConf.shouldPeerWithNeighbor(""));

  EXPECT_TRUE(areaConf.shouldDiscoverOnIface("iface20"));
  EXPECT_FALSE(areaConf.shouldDiscoverOnIface("iface400"));
  EXPECT_FALSE(areaConf.shouldDiscoverOnIface("iface450"));
  EXPECT_FALSE(areaConf.shouldDiscoverOnIface("loopback1"));
  EXPECT_FALSE(areaConf.shouldDiscoverOnIface(""));

  EXPECT_TRUE(areaConf.shouldRedistributeIface("loopback1"));
  EXPECT_FALSE(areaConf.shouldRedistributeIface("loopback10"));
  EXPECT_FALSE(areaConf.shouldRedistributeIface("iface450"));
  EXPECT_FALSE(areaConf.shouldRedistributeIface(""));
}

TEST(ConfigTest, BgpTranslationConfig) {
  auto tConfig = getBasicOpenrConfig();
  tConfig.enable_bgp_peering() = true;
  tConfig.bgp_config() = thrift::BgpConfig();
  tConfig.bgp_translation_config() = thrift::BgpRouteTranslationConfig();
  auto& translationConfig = *tConfig.bgp_translation_config();

  // Legacy translation disabled - But new translation hasn't been
  {
    translationConfig.enable_bgp_to_openr() = true;
    translationConfig.enable_openr_to_bgp() = false;
    translationConfig.disable_legacy_translation() = true;
    EXPECT_THROW((Config(tConfig)), std::invalid_argument);
  }

  // Legacy translation disabled - But new translation hasn't been
  {
    translationConfig.enable_bgp_to_openr() = false;
    translationConfig.enable_openr_to_bgp() = true;
    translationConfig.disable_legacy_translation() = true;
    EXPECT_THROW((Config(tConfig)), std::invalid_argument);
  }

  // Legacy translation disabled and new translation enabled
  {
    translationConfig.enable_bgp_to_openr() = true;
    translationConfig.enable_openr_to_bgp() = true;
    translationConfig.disable_legacy_translation() = true;
    EXPECT_NO_THROW((Config(tConfig)));
  }
}

TEST(ConfigTest, PopulateInternalDb) {
  // features

  // KSP2_ED_ECMP with IP
  {
    auto confInvalid = getBasicOpenrConfig();
    confInvalid.prefix_forwarding_type() = thrift::PrefixForwardingType::IP;
    confInvalid.prefix_forwarding_algorithm() =
        thrift::PrefixForwardingAlgorithm::KSP2_ED_ECMP;
    EXPECT_THROW((Config(confInvalid)), std::invalid_argument);
  }

  // RibPolicy
  {
    auto conf = getBasicOpenrConfig();
    conf.enable_rib_policy() = true;
    EXPECT_TRUE(Config(conf).isRibPolicyEnabled());
  }

  // kvstore

  // flood_msg_per_sec <= 0
  {
    auto confInvalidFloodMsgPerSec = getBasicOpenrConfig();
    confInvalidFloodMsgPerSec.kvstore_config()->flood_rate() = getFloodRate();
    confInvalidFloodMsgPerSec.kvstore_config()
        ->flood_rate()
        ->flood_msg_per_sec() = 0;
    EXPECT_THROW((Config(confInvalidFloodMsgPerSec)), std::out_of_range);
  }
  // flood_msg_burst_size <= 0
  {
    auto confInvalidFloodMsgPerSec = getBasicOpenrConfig();
    confInvalidFloodMsgPerSec.kvstore_config()->flood_rate() = getFloodRate();
    confInvalidFloodMsgPerSec.kvstore_config()
        ->flood_rate()
        ->flood_msg_burst_size() = 0;
    EXPECT_THROW((Config(confInvalidFloodMsgPerSec)), std::out_of_range);
  }

  // Spark

  // Exception: neighbor_discovery_port <= 0 or > 65535
  {
    auto confInvalidSpark = getBasicOpenrConfig();
    confInvalidSpark.spark_config()->neighbor_discovery_port() = -1;
    EXPECT_THROW(auto c = Config(confInvalidSpark), std::out_of_range);

    confInvalidSpark.spark_config()->neighbor_discovery_port() = 65536;
    EXPECT_THROW(auto c = Config(confInvalidSpark), std::out_of_range);
  }

  // Exception: hello_time_s <= 0
  {
    auto confInvalidSpark = getBasicOpenrConfig();
    confInvalidSpark.spark_config()->hello_time_s() = -1;
    EXPECT_THROW(auto c = Config(confInvalidSpark), std::out_of_range);
  }

  // Exception: fastinit_hello_time_ms <= 0
  {
    auto confInvalidSpark = getBasicOpenrConfig();
    confInvalidSpark.spark_config()->fastinit_hello_time_ms() = -1;
    EXPECT_THROW(auto c = Config(confInvalidSpark), std::out_of_range);
  }

  // Exception: fastinit_hello_time_ms > hello_time_s
  {
    auto confInvalidSpark2 = getBasicOpenrConfig();
    confInvalidSpark2.spark_config()->fastinit_hello_time_ms() = 10000;
    confInvalidSpark2.spark_config()->hello_time_s() = 2;
    EXPECT_THROW(auto c = Config(confInvalidSpark2), std::invalid_argument);
  }

  // Exception: keepalive_time_s <= 0
  {
    auto confInvalidSpark = getBasicOpenrConfig();
    confInvalidSpark.spark_config()->keepalive_time_s() = -1;
    EXPECT_THROW(auto c = Config(confInvalidSpark), std::out_of_range);
  }

  // Exception: keepalive_time_s > hold_time_s
  {
    auto confInvalidSpark = getBasicOpenrConfig();
    confInvalidSpark.spark_config()->keepalive_time_s() = 10;
    confInvalidSpark.spark_config()->hold_time_s() = 5;
    EXPECT_THROW(auto c = Config(confInvalidSpark), std::invalid_argument);
  }

  // Exception: graceful_restart_time_s < 3 * keepalive_time_s
  {
    auto confInvalidSpark = getBasicOpenrConfig();
    confInvalidSpark.spark_config()->keepalive_time_s() = 10;
    confInvalidSpark.spark_config()->graceful_restart_time_s() = 20;
    EXPECT_THROW(auto c = Config(confInvalidSpark), std::invalid_argument);
  }

  // Exception step_detector_fast_window_size >= 0
  //           step_detector_slow_window_size >= 0
  //           step_detector_lower_threshold >= 0
  //           step_detector_upper_threshold >= 0
  {
    auto confInvalidSpark = getBasicOpenrConfig();
    confInvalidSpark.spark_config()->step_detector_conf()->fast_window_size() =
        -1;
    EXPECT_THROW(auto c = Config(confInvalidSpark), std::invalid_argument);
    confInvalidSpark.spark_config()->step_detector_conf()->slow_window_size() =
        -1;
    EXPECT_THROW(auto c = Config(confInvalidSpark), std::invalid_argument);
    confInvalidSpark.spark_config()->step_detector_conf()->lower_threshold() =
        -1;
    EXPECT_THROW(auto c = Config(confInvalidSpark), std::invalid_argument);
    confInvalidSpark.spark_config()->step_detector_conf()->upper_threshold() =
        -1;
    EXPECT_THROW(auto c = Config(confInvalidSpark), std::invalid_argument);
  }

  // Exception step_detector_fast_window_size > step_detector_slow_window_size
  //           step_detector_lower_threshold > step_detector_upper_threshold
  {
    auto confInvalidSpark = getBasicOpenrConfig();
    confInvalidSpark.spark_config()->step_detector_conf()->fast_window_size() =
        10;
    confInvalidSpark.spark_config()->step_detector_conf()->slow_window_size() =
        5;
    EXPECT_THROW(auto c = Config(confInvalidSpark), std::invalid_argument);

    confInvalidSpark.spark_config()->step_detector_conf()->upper_threshold() =
        5;
    confInvalidSpark.spark_config()->step_detector_conf()->lower_threshold() =
        10;
    EXPECT_THROW(auto c = Config(confInvalidSpark), std::invalid_argument);
  }

  // Monitor

  // Exception monitor_max_event_log >= 0
  {
    auto confInvalidMon = getBasicOpenrConfig();
    confInvalidMon.monitor_config()->max_event_log() = -1;
    EXPECT_THROW(auto c = Config(confInvalidMon), std::out_of_range);
  }

  // link monitor

  // linkflap_initial_backoff_ms < 0
  {
    auto confInvalidLm = getBasicOpenrConfig();
    confInvalidLm.link_monitor_config()->linkflap_initial_backoff_ms() = -1;
    EXPECT_THROW(auto c = Config(confInvalidLm), std::out_of_range);
  }
  // linkflap_max_backoff_ms < 0
  {
    auto confInvalidLm = getBasicOpenrConfig();
    confInvalidLm.link_monitor_config()->linkflap_max_backoff_ms() = -1;
    EXPECT_THROW(auto c = Config(confInvalidLm), std::out_of_range);
  }
  // linkflap_initial_backoff_ms > linkflap_max_backoff_ms
  {
    auto confInvalidLm = getBasicOpenrConfig();
    confInvalidLm.link_monitor_config()->linkflap_initial_backoff_ms() = 360000;
    confInvalidLm.link_monitor_config()->linkflap_max_backoff_ms() = 300000;
    EXPECT_THROW(auto c = Config(confInvalidLm), std::out_of_range);
  }

  // prefix allocation

  // enable_prefix_allocation = true, prefix_allocation_config = null
  {
    auto confInvalidPa = getBasicOpenrConfig();
    confInvalidPa.enable_prefix_allocation() = true;
    EXPECT_THROW((Config(confInvalidPa)), std::invalid_argument);
  }
  // prefix_allocation_mode != DYNAMIC_ROOT_NODE, seed_prefix and
  // allocate_prefix_len set
  {
    auto confInvalidPa = getBasicOpenrConfig();
    confInvalidPa.enable_prefix_allocation() = true;
    confInvalidPa.prefix_allocation_config() = getPrefixAllocationConfig(
        thrift::PrefixAllocationMode::DYNAMIC_ROOT_NODE);
    confInvalidPa.prefix_allocation_config()->prefix_allocation_mode() =
        thrift::PrefixAllocationMode::DYNAMIC_LEAF_NODE;
    EXPECT_THROW((Config(confInvalidPa)), std::invalid_argument);
  }
  // prefix_allocation_mode = DYNAMIC_ROOT_NODE, seed_prefix and
  // allocate_prefix_len = null
  {
    auto confInvalidPa = getBasicOpenrConfig();
    confInvalidPa.enable_prefix_allocation() = true;
    confInvalidPa.prefix_allocation_config() = thrift::PrefixAllocationConfig();
    confInvalidPa.prefix_allocation_config()->prefix_allocation_mode() =
        thrift::PrefixAllocationMode::DYNAMIC_ROOT_NODE;
    EXPECT_THROW((Config(confInvalidPa)), std::invalid_argument);
  }
  // seed_prefix: invalid ipadrres format
  {
    auto confInvalidPa = getBasicOpenrConfig();
    confInvalidPa.enable_prefix_allocation() = true;
    confInvalidPa.prefix_allocation_config() = getPrefixAllocationConfig(
        thrift::PrefixAllocationMode::DYNAMIC_ROOT_NODE);
    confInvalidPa.prefix_allocation_config()->seed_prefix() =
        "fc00:cafe:babe:/64";
    EXPECT_ANY_THROW((Config(confInvalidPa)));
  }
  // allocate_prefix_len: <= seed_prefix subnet length
  {
    auto confInvalidPa = getBasicOpenrConfig();
    confInvalidPa.enable_prefix_allocation() = true;
    confInvalidPa.prefix_allocation_config() = getPrefixAllocationConfig(
        thrift::PrefixAllocationMode::DYNAMIC_ROOT_NODE);
    confInvalidPa.prefix_allocation_config()->allocate_prefix_len() = 60;
    EXPECT_THROW((Config(confInvalidPa)), std::out_of_range);
  }
  // seed_prefix v4, enable_v4 = false
  {
    auto confInvalidPa = getBasicOpenrConfig();
    confInvalidPa.enable_v4() = false;

    confInvalidPa.enable_prefix_allocation() = true;
    confInvalidPa.prefix_allocation_config() = getPrefixAllocationConfig(
        thrift::PrefixAllocationMode::DYNAMIC_ROOT_NODE);
    confInvalidPa.prefix_allocation_config()->seed_prefix() = "127.0.0.1/24";
    confInvalidPa.prefix_allocation_config()->allocate_prefix_len() = 32;
    EXPECT_THROW((Config(confInvalidPa)), std::invalid_argument);
  }

  // bgp peering

  // bgp peering enabled with empty bgp_config
  {
    auto confInvalid = getBasicOpenrConfig();
    confInvalid.enable_bgp_peering() = true;

    // Both bgp-config & translation-config are none
    confInvalid.bgp_config().reset();
    confInvalid.bgp_translation_config().reset();
    EXPECT_THROW((Config(confInvalid)), std::invalid_argument);

    // bgp config is set but translation-config is not
    confInvalid.bgp_config() = thrift::BgpConfig();
    confInvalid.bgp_translation_config().reset();
    // TODO: Expect an exception instead of default initialization
    // EXPECT_THROW((Config(confInvalid)), std::invalid_argument);
    EXPECT_EQ(
        thrift::BgpRouteTranslationConfig(),
        Config(confInvalid).getBgpTranslationConfig());

    // translation-config is set but bgp-config is not
    confInvalid.bgp_config().reset();
    confInvalid.bgp_translation_config() = thrift::BgpRouteTranslationConfig();
    EXPECT_THROW((Config(confInvalid)), std::invalid_argument);
  }

  // watchdog

  // watchdog enabled with empty watchdog_config
  {
    auto confInvalid = getBasicOpenrConfig();
    confInvalid.enable_watchdog() = true;
    EXPECT_THROW((Config(confInvalid)), std::invalid_argument);
  }

  // vip service
  {
    auto conf = getBasicOpenrConfig();
    EXPECT_FALSE(Config(conf).isVipServiceEnabled());
    conf.enable_vip_service() = true;
    EXPECT_THROW(Config(conf).isVipServiceEnabled(), std::invalid_argument);
    EXPECT_THROW(Config(conf).checkVipServiceConfig(), std::invalid_argument);
    conf.vip_service_config() = {};
    conf.vip_service_config()->ingress_policy() = "test_policy";
    // There is no area_policies, so should throw.
    EXPECT_THROW(Config(conf).checkVipServiceConfig(), std::invalid_argument);
    conf.area_policies() = neteng::config::routing_policy::PolicyConfig();
    conf.area_policies()->filters() =
        neteng::config::routing_policy::PolicyFilters();
    conf.area_policies()->filters()->routePropagationPolicy() =
        neteng::config::routing_policy::Filters();
    // There is policies, but no vip ingress policy, should throw
    EXPECT_THROW(Config(conf).checkVipServiceConfig(), std::invalid_argument);
    std::map<std::string, neteng::config::routing_policy::Filter> policy;
    policy["test_policy"] = neteng::config::routing_policy::Filter();
    conf.area_policies()->filters()->routePropagationPolicy()->objects() =
        policy;
    // There is vip ingress policy in area_policies, should pass
    EXPECT_NO_THROW(Config(conf).checkVipServiceConfig());
  }

  // FIB route deletion
  {
    auto conf = getBasicOpenrConfig();
    conf.route_delete_delay_ms() = -1;
    EXPECT_THROW((Config(conf)), std::invalid_argument);

    conf.route_delete_delay_ms() = 0;
    EXPECT_NO_THROW((Config(conf)));

    conf.route_delete_delay_ms() = 1000;
    EXPECT_NO_THROW((Config(conf)));
  }
}

TEST(ConfigTest, SoftdrainConfigTest) {
  auto tConfig = getBasicOpenrConfig();
  tConfig.enable_soft_drain() = true;

  // no soft-drained flag
  auto config = Config(tConfig);
  EXPECT_TRUE(config.isSoftdrainEnabled());
}

TEST(ConfigTest, GeneralGetter) {
  // config without bgp peering
  {
    auto tConfig = getBasicOpenrConfig(
        "node-1",
        {}, /* area config */
        true /* enableV4 */,
        false /* enableSegmentRouting */,
        true /*dryrun*/);
    auto config = Config(tConfig);

    // getNodeName
    EXPECT_EQ("node-1", config.getNodeName());

    // getAreaIds
    EXPECT_EQ(1, config.getAreas().size());
    EXPECT_EQ(1, config.getAreas().count(kTestingAreaName));

    // enable_v4
    EXPECT_TRUE(config.isV4Enabled());
    // enable_segment_routing
    EXPECT_FALSE(config.isSegmentRoutingEnabled());
    // isBgpPeeringEnabled
    EXPECT_FALSE(config.isBgpPeeringEnabled());
    // enable_best_route_selection
    EXPECT_FALSE(config.isBestRouteSelectionEnabled());
    // enable_v4_over_v6_nexthop
    EXPECT_FALSE(config.isV4OverV6NexthopEnabled());
    // enable_vip_service
    EXPECT_FALSE(config.isVipServiceEnabled());
    // enable_soft_drain
    EXPECT_FALSE(config.isSoftdrainEnabled());

    // getSparkConfig
    EXPECT_EQ(*tConfig.spark_config(), config.getSparkConfig());
  }

  // config without bgp peering and only for v4_over_v6_nexthop
  {
    auto tConfig = getBasicOpenrConfig(
        "node-1",
        {} /* area config */,
        true /* enable v4 */,
        false /* enableSegmentRouting */,
        true /* dryrun */,
        true /* enableV4OverV6Nexthop */);
    auto config = Config(tConfig);

    // enable_v4_over_v6_nexthop
    EXPECT_TRUE(config.isV4OverV6NexthopEnabled());
  }

  // config with watchdog
  {
    auto tConfig = getBasicOpenrConfig("fsw001");
    tConfig.enable_watchdog() = true;
    const auto& watchdogConf = thrift::WatchdogConfig();
    tConfig.watchdog_config() = watchdogConf;

    auto config = Config(tConfig);

    EXPECT_TRUE(config.isWatchdogEnabled());
    EXPECT_EQ(watchdogConf, config.getWatchdogConfig());
  }
}

TEST(ConfigTest, KvstoreGetter) {
  auto tConfig = getBasicOpenrConfig();
  auto config = Config(tConfig);
  const auto& kvstoreConf = thrift::KvstoreConfig();

  // getKvStoreConfig
  EXPECT_EQ(kvstoreConf, config.getKvStoreConfig());

  // getKvStoreKeyTtl
  EXPECT_EQ(std::chrono::milliseconds(300000), config.getKvStoreKeyTtl());
}

TEST(ConfigTest, LinkMonitorGetter) {
  auto tConfig = getBasicOpenrConfig();
  // set empty area list to see doamin get converted to area
  tConfig.areas().emplace();
  auto config = Config(tConfig);

  // check to see the link monitor options got converted to an area config with
  // domainName
  auto const& domainNameArea =
      config.getAreas().at(Constants::kDefaultArea.toString());
  EXPECT_FALSE(domainNameArea.shouldDiscoverOnIface("eth0"));

  EXPECT_FALSE(domainNameArea.shouldRedistributeIface("eth0"));
}

TEST(ConfigTest, PrefixAllocatorGetter) {
  auto tConfig = getBasicOpenrConfig();
  tConfig.enable_prefix_allocation() = true;
  const auto paConf = getPrefixAllocationConfig(
      thrift::PrefixAllocationMode::DYNAMIC_ROOT_NODE);
  tConfig.prefix_allocation_config() = paConf;
  auto config = Config(tConfig);

  // isPrefixAllocationEnabled
  EXPECT_TRUE(config.isPrefixAllocationEnabled());

  // getPrefixAllocationConfig
  EXPECT_EQ(paConf, config.getPrefixAllocationConfig());

  // getPrefixAllocationParams
  const PrefixAllocationParams& params = {testSeedPrefix, testAllocationPfxLen};
  EXPECT_EQ(params, config.getPrefixAllocationParams());
}

TEST(ConfigTest, SegmentRoutingConfig) {
  auto tConfig = getBasicOpenrConfig();
  const auto& srConf = getSegmentRoutingConfig();
  tConfig.segment_routing_config() = srConf;
  auto config = Config(tConfig);

  // getSegmentRoutingConfig
  EXPECT_EQ(srConf, config.getSegmentRoutingConfig());
  EXPECT_EQ(
      *config.getAdjSegmentLabels().sr_adj_label_type(),
      openr::thrift::SegmentRoutingAdjLabelType::AUTO_IFINDEX);
}

TEST(ConfigTest, ToThriftKvStoreConfig) {
  auto tConfig = getBasicOpenrConfig();
  const auto& srConf = getSegmentRoutingConfig();
  tConfig.segment_routing_config() = srConf;
  auto config = Config(tConfig);

  EXPECT_NO_THROW(config.toThriftKvStoreConfig());
}

} // namespace openr
