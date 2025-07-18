// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/storage/distributor/bucket_spaces_stats_provider.h>
#include <vespa/storage/distributor/content_node_stats_provider.h>
#include <vespa/storage/distributor/distributor_host_info_reporter.h>
#include <vespa/storage/distributor/min_replica_provider.h>
#include <tests/common/hostreporter/util.h>
#include <vespa/vespalib/data/slime/slime.h>
#include <vespa/vespalib/io/fileutil.h>
#include <vespa/vespalib/stllike/asciistream.h>
#include <vespa/vespalib/stllike/hash_map.hpp>
#include <chrono>
#include <vespa/vespalib/gtest/gtest.h>
#include <vespa/vespalib/test/test_path.h>

namespace storage::distributor {

using End = vespalib::JsonStream::End;
using File = vespalib::File;
using MinReplicaStats = MinReplicaMap;
using Object = vespalib::JsonStream::Object;
using PerNodeBucketSpacesStats = BucketSpacesStatsProvider::PerNodeBucketSpacesStats;
using BucketSpacesStats = BucketSpacesStatsProvider::BucketSpacesStats;
using namespace ::testing;

struct DistributorHostInfoReporterTest : Test {
    static void verifyBucketSpaceStats(const vespalib::Slime& root, uint16_t nodeIndex, const std::string& bucketSpaceName,
                                       size_t bucketsTotal, size_t bucketsPending);
    static void verifyBucketSpaceStats(const vespalib::Slime& root, uint16_t nodeIndex, const std::string& bucketSpaceName);
};

using ms = std::chrono::milliseconds;

namespace {

// My kingdom for GoogleMock!
struct MockedMinReplicaProvider : MinReplicaProvider {
    MinReplicaStats minReplica;

    ~MockedMinReplicaProvider() override;
    MinReplicaMap getMinReplica() const override {
        return minReplica;
    }
};

MockedMinReplicaProvider::~MockedMinReplicaProvider() = default;

struct MockedBucketSpacesStatsProvider : public BucketSpacesStatsProvider {
    PerNodeBucketSpacesStats _stats;
    DistributorGlobalStats _global_stats;

    ~MockedBucketSpacesStatsProvider() override;
    PerNodeBucketSpacesStats per_node_bucket_spaces_stats() const override {
        return _stats;
    }
    DistributorGlobalStats distributor_global_stats() const override {
        return _global_stats;
    }
};

MockedBucketSpacesStatsProvider::~MockedBucketSpacesStatsProvider() = default;

struct MockedContentNodeStatsProvider : ContentNodeStatsProvider {
    ContentNodeMessageStatsTracker::NodeStats _stats;

    ~MockedContentNodeStatsProvider() override = default;

    ContentNodeMessageStatsTracker::NodeStats content_node_stats() const override {
        return _stats;
    }
};

const vespalib::slime::Inspector&
getNode(const vespalib::Slime& root, uint16_t nodeIndex)
{
    auto& storage_nodes = root.get()["distributor"]["storage-nodes"];
    const size_t n = storage_nodes.entries();
    for (size_t i = 0; i < n; ++i) {
        if (storage_nodes[i]["node-index"].asLong() == nodeIndex) {
            return storage_nodes[i];
        }
    }
    throw std::runtime_error("No node found with index "
                             + std::to_string(nodeIndex));
}

int
getMinReplica(const vespalib::Slime& root, uint16_t nodeIndex)
{
    return getNode(root, nodeIndex)["min-current-replication-factor"].asLong();
}

const vespalib::slime::Inspector&
getBucketSpaceStats(const vespalib::Slime& root, uint16_t nodeIndex, const std::string& bucketSpaceName)
{
    const auto& bucketSpaces = getNode(root, nodeIndex)["bucket-spaces"];
    for (size_t i = 0; i < bucketSpaces.entries(); ++i) {
        if (bucketSpaces[i]["name"].asString().make_stringview() == bucketSpaceName) {
            return bucketSpaces[i];
        }
    }
    throw std::runtime_error("No bucket space found with name " + bucketSpaceName);
}

std::optional<int64_t> document_count_total_from(const vespalib::Slime& root) {
    auto& node = root["distributor"]["global-stats"]["stored-document-count"];
    if (!node.valid()) {
        return std::nullopt;
    }
    return node.asLong();
}

std::optional<int64_t> bytes_total_from(const vespalib::Slime& root) {
    auto& node = root["distributor"]["global-stats"]["stored-document-bytes"];
    if (!node.valid()) {
        return std::nullopt;
    }
    return node.asLong();
}

struct ExpectedResponseStats {
    uint64_t responses = 0;
    uint64_t errors = 0; // <= responses
};

void verify_node_response_stats(const vespalib::Slime& root, uint16_t node_index, std::optional<ExpectedResponseStats> expected) {
    auto& inspector = getNode(root, node_index);
    if (!expected) {
        ASSERT_FALSE(inspector["response-stats"].valid());
        return;
    }
    auto& stats = inspector["response-stats"];
    ASSERT_EQ(stats["total-count"].asLong(), expected->responses);
    auto& errors = stats["errors"];
    ASSERT_TRUE(errors.valid());
    ASSERT_EQ(errors["network"].asLong(), expected->errors);
}

[[nodiscard]] ContentNodeMessageStats make_node_recv_stats(size_t recv_ok, size_t net_errors) {
    const size_t fake_sent = recv_ok + net_errors; // Fudge the numbers to ensure sent is always >= received
    return ContentNodeMessageStats(fake_sent, recv_ok, net_errors, 0, 0, 0);
}

}

void
DistributorHostInfoReporterTest::verifyBucketSpaceStats(const vespalib::Slime& root,
                                                        uint16_t nodeIndex,
                                                        const std::string& bucketSpaceName,
                                                        size_t bucketsTotal,
                                                        size_t bucketsPending)
{
    const auto &stats = getBucketSpaceStats(root, nodeIndex, bucketSpaceName);
    const auto &buckets = stats["buckets"];
    EXPECT_EQ(bucketsTotal, static_cast<size_t>(buckets["total"].asLong()));
    EXPECT_EQ(bucketsPending, static_cast<size_t>(buckets["pending"].asLong()));
}

void
DistributorHostInfoReporterTest::verifyBucketSpaceStats(const vespalib::Slime& root,
                                                        uint16_t nodeIndex,
                                                        const std::string& bucketSpaceName)
{
    const auto &stats = getBucketSpaceStats(root, nodeIndex, bucketSpaceName);
    EXPECT_FALSE(stats["buckets"].valid());
}

struct Fixture {
    MockedMinReplicaProvider minReplicaProvider;
    MockedBucketSpacesStatsProvider bucketSpacesStatsProvider;
    MockedContentNodeStatsProvider content_node_stats_provider;
    DistributorHostInfoReporter reporter;
    Fixture()
        : minReplicaProvider(),
          bucketSpacesStatsProvider(),
          reporter(minReplicaProvider, bucketSpacesStatsProvider, content_node_stats_provider)
    {}
    ~Fixture();

    [[nodiscard]] vespalib::Slime to_slime() {
        vespalib::Slime root;
        util::reporterToSlime(reporter, root);
        return root;
    }

    void set_node_stats(uint16_t node_index, const ContentNodeMessageStats& stats) {
        content_node_stats_provider._stats.per_node[node_index] = stats;
    }

    void prime_nodes_with_dummy_min_replica_stats(std::initializer_list<uint16_t> nodes) {
        // Prime some unrelated information for the content node reporting so that they're
        // always present independent of the error stats
        MinReplicaStats min_replica;
        for (uint16_t n : nodes) {
            min_replica[n] = 1234;
        }
        minReplicaProvider.minReplica = min_replica;
    }

};

Fixture::~Fixture() = default;

TEST_F(DistributorHostInfoReporterTest, min_replica_stats_are_reported) {
    Fixture f;

    MinReplicaStats minReplica;
    minReplica[0] = 2;
    minReplica[5] = 9;
    f.minReplicaProvider.minReplica = minReplica;

    auto root = f.to_slime();
    EXPECT_EQ(2, getMinReplica(root, 0));
    EXPECT_EQ(9, getMinReplica(root, 5));
}

TEST_F(DistributorHostInfoReporterTest, merge_min_replica_stats) {

    MinReplicaStats min_replica_a;
    min_replica_a[3] = 2;
    min_replica_a[5] = 4;

    MinReplicaStats min_replica_b;
    min_replica_b[5] = 6;
    min_replica_b[7] = 8;

    MinReplicaStats result;
    merge_min_replica_stats(result, min_replica_a);
    merge_min_replica_stats(result, min_replica_b);

    EXPECT_EQ(3, result.size());
    EXPECT_EQ(2, result[3]);
    EXPECT_EQ(4, result[5]);
    EXPECT_EQ(8, result[7]);
}

TEST_F(DistributorHostInfoReporterTest, generate_example_json) {
    Fixture f;

    MinReplicaStats minReplica;
    minReplica[0] = 2;
    minReplica[5] = 9;
    f.minReplicaProvider.minReplica = minReplica;

    PerNodeBucketSpacesStats stats;
    stats[0]["default"] = BucketSpaceStats(11, 3);
    stats[0]["global"]  = BucketSpaceStats(13, 5);
    stats[5]["default"] = BucketSpaceStats();
    f.bucketSpacesStatsProvider._stats = stats;
    f.bucketSpacesStatsProvider._global_stats = DistributorGlobalStats(1337, 456789);

    vespalib::asciistream json;
    vespalib::JsonStream stream(json, true);

    stream << Object();
    f.reporter.report(stream);
    stream << End();
    stream.finalize();

    std::string_view jsonString = json.view();

    std::string path = TEST_PATH("../../../../protocols/getnodestate/distributor.json");
    std::string goldenString = File::readAll(path);

    vespalib::Memory goldenMemory(goldenString);
    vespalib::Slime goldenSlime;
    vespalib::slime::JsonFormat::decode(goldenMemory, goldenSlime);

    vespalib::Memory jsonMemory(jsonString);
    vespalib::Slime jsonSlime;
    vespalib::slime::JsonFormat::decode(jsonMemory, jsonSlime);

    EXPECT_EQ(goldenSlime, jsonSlime);
}

TEST_F(DistributorHostInfoReporterTest, bucket_spaces_stats_are_reported) {
    Fixture f;
    PerNodeBucketSpacesStats stats;
    stats[1]["default"] = BucketSpaceStats(11, 3);
    stats[1]["global"]  = BucketSpaceStats(13, 5);
    stats[2]["default"] = BucketSpaceStats(17, 7);
    stats[2]["global"]  = BucketSpaceStats();
    stats[3]["default"] = BucketSpaceStats(19, 11);
    f.bucketSpacesStatsProvider._stats = stats;

    vespalib::Slime root;
    util::reporterToSlime(f.reporter, root);
    verifyBucketSpaceStats(root, 1, "default", 11, 3);
    verifyBucketSpaceStats(root, 1, "global",  13, 5);
    verifyBucketSpaceStats(root, 2, "default", 17, 7);
    verifyBucketSpaceStats(root, 2, "global");
    verifyBucketSpaceStats(root, 3, "default", 19, 11);
    try {
        verifyBucketSpaceStats(root, 3, "global");
        FAIL() << "No exception thrown";
    } catch (const std::runtime_error& ex) {
        EXPECT_EQ("No bucket space found with name global", std::string(ex.what()));
    }
}

TEST_F(DistributorHostInfoReporterTest, global_stats_are_reported_when_present) {
    Fixture f;
    {
        auto root = f.to_slime();
        // No global stats present
        EXPECT_EQ(document_count_total_from(root), std::nullopt);
        EXPECT_EQ(bytes_total_from(root), std::nullopt);
    }
    f.bucketSpacesStatsProvider._global_stats = DistributorGlobalStats(12345, 567890);
    {
        auto root = f.to_slime();
        EXPECT_EQ(document_count_total_from(root), 12345);
        EXPECT_EQ(bytes_total_from(root), 567890);
    }
}

TEST_F(DistributorHostInfoReporterTest, per_content_node_response_stats_report_stable_60s_deltas) {
    Fixture f;
    f.prime_nodes_with_dummy_min_replica_stats({0, 3});

    f.reporter.on_periodic_callback(std::chrono::steady_clock::time_point(60s)); // Start tracking time
    auto root = f.to_slime();
    EXPECT_NO_FATAL_FAILURE(verify_node_response_stats(root, 0, std::nullopt));
    EXPECT_NO_FATAL_FAILURE(verify_node_response_stats(root, 3, std::nullopt));

    f.set_node_stats(0, make_node_recv_stats(1, 2));
    f.set_node_stats(3, make_node_recv_stats(10, 20));
    f.reporter.on_periodic_callback(std::chrono::steady_clock::time_point(60s*2));

    root = f.to_slime();
    EXPECT_NO_FATAL_FAILURE(verify_node_response_stats(root, 0, ExpectedResponseStats{1+2, 2}));
    EXPECT_NO_FATAL_FAILURE(verify_node_response_stats(root, 3, ExpectedResponseStats{10+20, 20}));

    // Timer callbacks within the current window does not update deltas
    f.reporter.on_periodic_callback(std::chrono::steady_clock::time_point(60s*2 + 59s));
    root = f.to_slime();
    EXPECT_NO_FATAL_FAILURE(verify_node_response_stats(root, 0, ExpectedResponseStats{1+2, 2}));
    EXPECT_NO_FATAL_FAILURE(verify_node_response_stats(root, 3, ExpectedResponseStats{10+20, 20}));

    f.set_node_stats(0, make_node_recv_stats(6, 5));
    f.set_node_stats(3, make_node_recv_stats(35, 30));

    // A new time window _does_ update deltas
    f.reporter.on_periodic_callback(std::chrono::steady_clock::time_point(60s*3));
    root = f.to_slime();
    EXPECT_NO_FATAL_FAILURE(verify_node_response_stats(root, 0, ExpectedResponseStats{(6+5)   - (1+2),     5-2}));
    EXPECT_NO_FATAL_FAILURE(verify_node_response_stats(root, 3, ExpectedResponseStats{(35+30) - (10+20), 30-20}));
}

TEST_F(DistributorHostInfoReporterTest, per_content_node_response_stats_are_only_reported_when_present) {
    Fixture f;
    f.prime_nodes_with_dummy_min_replica_stats({0, 3});

    // Stats are present, but without any errors. Nothing to report
    f.set_node_stats(0, make_node_recv_stats(10, 0));
    f.set_node_stats(3, make_node_recv_stats(30, 0));
    f.reporter.on_periodic_callback(std::chrono::steady_clock::time_point(60s));

    auto root = f.to_slime();
    EXPECT_NO_FATAL_FAILURE(verify_node_response_stats(root, 0, std::nullopt));
    EXPECT_NO_FATAL_FAILURE(verify_node_response_stats(root, 3, std::nullopt));
}

TEST_F(DistributorHostInfoReporterTest, content_node_response_stats_not_present_when_error_delta_is_zero) {
    Fixture f;
    f.prime_nodes_with_dummy_min_replica_stats({0, 3});

    f.set_node_stats(0, make_node_recv_stats(10, 10));
    f.set_node_stats(3, make_node_recv_stats(30, 20));
    f.reporter.on_periodic_callback(std::chrono::steady_clock::time_point(60s));
    f.set_node_stats(0, make_node_recv_stats(10, 10)); // unchanged
    f.set_node_stats(3, make_node_recv_stats(40, 31)); // has new errors
    f.reporter.on_periodic_callback(std::chrono::steady_clock::time_point(60s*2));

    // Node 3 should be the only node present in the output
    auto root = f.to_slime();
    EXPECT_NO_FATAL_FAILURE(verify_node_response_stats(root, 0, std::nullopt));
    EXPECT_NO_FATAL_FAILURE(verify_node_response_stats(root, 3, ExpectedResponseStats{21, 11}));
}

TEST_F(DistributorHostInfoReporterTest, merge_per_node_bucket_spaces_stats) {

    PerNodeBucketSpacesStats stats_a;
    stats_a[3]["default"] = BucketSpaceStats(3, 2);
    stats_a[3]["global"]  = BucketSpaceStats(5, 4);
    stats_a[5]["default"] = BucketSpaceStats(7, 6);
    stats_a[5]["global"]  = BucketSpaceStats(9, 8);

    PerNodeBucketSpacesStats stats_b;
    stats_b[5]["default"] = BucketSpaceStats(11, 10);
    stats_b[5]["global"]  = BucketSpaceStats(13, 12);
    stats_b[7]["default"] = BucketSpaceStats(15, 14);

    PerNodeBucketSpacesStats result;
    merge_per_node_bucket_spaces_stats(result, stats_a);
    merge_per_node_bucket_spaces_stats(result, stats_b);

    PerNodeBucketSpacesStats exp;
    exp[3]["default"] = BucketSpaceStats(3, 2);
    exp[3]["global"]  = BucketSpaceStats(5, 4);
    exp[5]["default"] = BucketSpaceStats(7+11, 6+10);
    exp[5]["global"]  = BucketSpaceStats(9+13, 8+12);
    exp[7]["default"] = BucketSpaceStats(15, 14);

    EXPECT_EQ(exp, result);
}

TEST_F(DistributorHostInfoReporterTest, merge_bucket_space_stats_maintains_valid_flag) {
    BucketSpaceStats stats_a(5, 3);
    BucketSpaceStats stats_b;

    stats_a.merge(stats_b);
    EXPECT_FALSE(stats_a.valid());
    EXPECT_EQ(5, stats_a.bucketsTotal());
    EXPECT_EQ(3, stats_a.bucketsPending());
}

}
