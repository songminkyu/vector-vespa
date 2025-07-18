// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
/**
 * \class storage::lib::Distribution
 * \ingroup distribution
 *
 * \brief Class used to distribute load between storage nodes.
 */

#pragma once

#include "group.h"
#include <vespa/document/bucket/bucketid.h>
#include <vespa/vdslib/state/nodetype.h>
#include <vespa/vespalib/util/exception.h>
#include <vespa/vespalib/util/small_vector.h>

namespace vespa::config::content::internal {
    class InternalStorDistributionType;
}
namespace storage::lib {

VESPA_DEFINE_EXCEPTION(NoDistributorsAvailableException, vespalib::Exception);
VESPA_DEFINE_EXCEPTION(TooFewBucketBitsInUseException, vespalib::Exception);

class ClusterState;
class NodeState;

class Distribution : public document::Printable {
public:
    using DistributionConfig = const vespa::config::content::internal::InternalStorDistributionType;
    using DistributionConfigBuilder = vespa::config::content::internal::InternalStorDistributionType;

private:
    std::vector<uint32_t>      _distributionBitMasks;
    std::unique_ptr<Group>     _nodeGraph;
    std::vector<const Group *> _node2Group;
    uint16_t                   _redundancy;
    uint16_t                   _initialRedundancy;
    uint16_t                   _readyCopies;
    bool                       _global;
    bool                       _activePerGroup;
    bool                       _ensurePrimaryPersisted;
    std::string                _serialized;

    struct ResultGroup {
        const Group* _group;
        uint16_t _redundancy;

        ResultGroup(const Group& group, uint16_t redundancy) noexcept
            : _group(&group), _redundancy(redundancy) {}

        bool operator<(const ResultGroup& other) const noexcept {
            return _group->getIndex() < other._group->getIndex();
        }
    };

    /**
     * Get seed to use for ideal state algorithm's random number generator
     * to decide which hierarchical group we should pick.
     */
    uint32_t getGroupSeed(const document::BucketId&, const ClusterState&, const Group&) const;

    /**
     * Get seed to use for ideal state algorithm's random number generator
     * to decide which distributor node this bucket should be mapped to.
     */
    uint32_t getDistributorSeed(const document::BucketId&, const ClusterState&) const;
    /**
     * Get seed to use for ideal state algorithm's random number generator
     * to decide which storage node this bucket should be mapped to.
     */
    uint32_t getStorageSeed(const document::BucketId&, const ClusterState&) const;

    void getIdealGroups(const document::BucketId& bucket, const ClusterState& clusterState, const Group& parent,
                        uint16_t redundancy, std::vector<ResultGroup>& results) const;

    const Group* getIdealDistributorGroup(const document::BucketId& bucket, const ClusterState& clusterState, const Group& parent) const;

    /**
     * Since distribution object may be used often in ideal state calculations
     * we'd like to avoid locking using it. Thus we don't support live config.
     * You need to create a new distribution object to change it. This function
     * is thus private so only constructor can call it.
     */
    void configure(const DistributionConfig & config);

public:
    class ConfigWrapper {
    public:
        ConfigWrapper(ConfigWrapper&& rhs) noexcept = default;
        ConfigWrapper& operator=(ConfigWrapper&& rhs) noexcept = default;
        explicit ConfigWrapper(std::unique_ptr<DistributionConfig> cfg) noexcept;
        ~ConfigWrapper();
        [[nodiscard]] const DistributionConfig& get() const noexcept { return *_cfg; }
        [[nodiscard]] std::unique_ptr<DistributionConfig> steal() noexcept;
    private:
        std::unique_ptr<DistributionConfig> _cfg;
    };
    Distribution();
    Distribution(const Distribution&);
    explicit Distribution(const ConfigWrapper& cfg);
    explicit Distribution(const DistributionConfig& cfg);
    // If `is_global == true`, it is the responsibility of the caller to ensure the
    // config has been transformed in such a way that it passes legacy cross-node
    // distribution config hash checks for global distribution configs. This constructor
    // does _not_ perform any config rewriting.
    Distribution(const DistributionConfig& cfg, bool is_global);
    explicit Distribution(const std::string& serialized);
    ~Distribution() override;

    Distribution& operator=(const Distribution&) = delete;

    [[nodiscard]] const std::string& serialized() const noexcept { return _serialized; }

    [[nodiscard]] const Group& getNodeGraph() const noexcept { return *_nodeGraph; }
    [[nodiscard]] uint16_t getRedundancy() const noexcept { return _redundancy; }
    [[nodiscard]] uint16_t getInitialRedundancy() const noexcept { return _initialRedundancy; }
    [[nodiscard]] uint16_t getReadyCopies() const noexcept { return _readyCopies; }
    [[nodiscard]] bool ensurePrimaryPersisted() const noexcept { return _ensurePrimaryPersisted; }
    [[nodiscard]] bool activePerGroup() const noexcept { return _activePerGroup; }
    [[nodiscard]] bool is_global() const noexcept { return _global; }

    bool operator==(const Distribution& o) const noexcept { return (_serialized == o._serialized); }
    bool operator!=(const Distribution& o) const noexcept { return (_serialized != o._serialized); }

    void print(std::ostream& out, bool, const std::string&) const override;

    /** Simplified wrapper for getIdealNodes() */
    [[nodiscard]] std::vector<uint16_t> getIdealStorageNodes(const ClusterState&, const document::BucketId&, const char* upStates = "uim") const;

    /** Simplified wrapper for getIdealNodes() */
    [[nodiscard]] uint16_t getIdealDistributorNode(const ClusterState&, const document::BucketId&, const char* upStates = "uim") const;

    /**
     * @throws TooFewBucketBitsInUseException If distribution bit count is
     *         larger than the number of bits used in bucket.
     * @throws NoDistributorsAvailableException If no distributors are available
     *         in any upstate.
     */
    enum { DEFAULT_REDUNDANCY = 0xffff };
    void getIdealNodes(const NodeType&, const ClusterState&, const document::BucketId&, std::vector<uint16_t>& nodes,
                       const char* upStates, uint16_t redundancy = DEFAULT_REDUNDANCY) const;

    /**
     * Unit tests can use this function to get raw config for this class to use
     * with a really simple setup with no hierarchical grouping. This function
     * should not be used by any production code.
     */
    static ConfigWrapper getDefaultDistributionConfig(uint16_t redundancy = 2, uint16_t nodeCount = 10);

    /**
     * Utility function used by distributor to split copies into groups to
     * handle active per group feature.
     */
    using IndexList = vespalib::SmallVector<uint16_t, 4>;
    std::vector<IndexList> splitNodesIntoLeafGroups(std::span<const uint16_t> nodes) const;

    static bool allDistributorsDown(const Group&, const ClusterState&);
};

} // storage::lib

