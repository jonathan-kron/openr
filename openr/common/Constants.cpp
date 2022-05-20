/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Constants.h"

namespace openr {

constexpr folly::StringPiece Constants::kAdjDbMarker;
constexpr folly::StringPiece Constants::kDefaultArea;
constexpr folly::StringPiece Constants::kEventLogCategory;
constexpr folly::StringPiece Constants::kNodeLabelRangePrefix;
constexpr folly::StringPiece Constants::kOpenrCtrlSessionContext;
constexpr folly::StringPiece Constants::kPlatformHost;
constexpr folly::StringPiece Constants::kPrefixAllocMarker;
constexpr folly::StringPiece Constants::kPrefixDbMarker;
constexpr folly::StringPiece Constants::kPrefixNameSeparator;
constexpr folly::StringPiece Constants::kSeedPrefixAllocLenSeparator;
constexpr folly::StringPiece Constants::kSeedPrefixAllocParamKey;
constexpr folly::StringPiece Constants::kSparkMcastAddr;
constexpr folly::StringPiece Constants::kStaticPrefixAllocParamKey;
constexpr int32_t Constants::kDefaultPathPreference;
constexpr int32_t Constants::kDefaultSourcePreference;
constexpr int32_t Constants::kOpenrCtrlPort;
constexpr int32_t Constants::kOpenrSupportedVersion;
constexpr int32_t Constants::kOpenrVersion;
constexpr int64_t Constants::kDefaultAdjWeight;
constexpr int64_t Constants::kTtlInfinity;
constexpr size_t Constants::kMaxFullSyncPendingCountThreshold;
constexpr size_t Constants::kNumTimeSeries;
constexpr std::chrono::milliseconds Constants::kAdjacencyThrottleTimeout;
constexpr std::chrono::milliseconds Constants::kFibInitialBackoff;
constexpr std::chrono::milliseconds Constants::kFibMaxBackoff;
constexpr std::chrono::milliseconds Constants::kFloodPendingPublication;
constexpr std::chrono::milliseconds Constants::kInitialBackoff;
constexpr std::chrono::milliseconds Constants::kKeepAliveCheckInterval;
constexpr std::chrono::milliseconds Constants::kKvStoreDbTtl;
constexpr std::chrono::milliseconds Constants::kKvStoreClearThrottleTimeout;
constexpr std::chrono::milliseconds Constants::kKvStoreSyncThrottleTimeout;
constexpr std::chrono::milliseconds Constants::kLinkImmediateTimeout;
constexpr std::chrono::milliseconds Constants::kLinkThrottleTimeout;
constexpr std::chrono::milliseconds Constants::kLongPollReqHoldTime;
constexpr std::chrono::milliseconds Constants::kMaxBackoff;
constexpr std::chrono::milliseconds Constants::kMaxTtlUpdateInterval;
constexpr std::chrono::milliseconds Constants::kPersistentStoreInitialBackoff;
constexpr std::chrono::milliseconds Constants::kPersistentStoreMaxBackoff;
constexpr std::chrono::milliseconds Constants::kPlatformConnTimeout;
constexpr std::chrono::milliseconds Constants::kPlatformRoutesProcTimeout;
constexpr std::chrono::milliseconds Constants::kPrefixAllocatorRetryInterval;
constexpr std::chrono::milliseconds Constants::kPrefixAllocatorSyncInterval;
constexpr std::chrono::milliseconds Constants::kRangeAllocTtl;
constexpr std::chrono::milliseconds Constants::kReadTimeout;
constexpr std::chrono::milliseconds Constants::kServiceConnTimeout;
constexpr std::chrono::milliseconds Constants::kServiceConnSSLTimeout;
constexpr std::chrono::milliseconds Constants::kServiceProcTimeout;
constexpr std::chrono::milliseconds Constants::kTtlDecrement;
constexpr std::chrono::milliseconds Constants::kTtlInfInterval;
constexpr std::chrono::milliseconds Constants::kTtlThreshold;
constexpr std::chrono::seconds Constants::kConvergenceMaxDuration;
constexpr std::chrono::seconds Constants::kCounterSubmitInterval;
constexpr std::chrono::seconds Constants::kFloodTopoDumpInterval;
constexpr std::chrono::seconds Constants::kMemoryThresholdTime;
constexpr std::chrono::seconds Constants::kPlatformSyncInterval;
constexpr std::chrono::seconds Constants::kPlatformThriftIdleTimeout;
constexpr std::chrono::seconds Constants::kThriftClientKeepAliveInterval;
constexpr uint16_t Constants::kPerfBufferSize;
constexpr uint32_t Constants::kMaxAllowedPps;

} // namespace openr
