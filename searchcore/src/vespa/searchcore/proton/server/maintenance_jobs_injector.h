// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include "document_db_maintenance_config.h"
#include "maintenancecontroller.h"
#include "i_lid_space_compaction_handler.h"
#include "i_operation_storer.h"
#include "iheartbeathandler.h"
#include <vespa/searchcore/proton/metrics/documentdb_job_trackers.h>

namespace searchcorespi { class IIndexManager; }
namespace storage::spi {struct BucketExecutor; }
namespace proton {

class AttributeUsageFilter;
class IBucketModifiedHandler;
class IBucketStateChangedNotifier;
class IClusterStateChangedNotifier;
class IResourceUsageNotifier;
class IPruneRemovedDocumentsHandler;
class MaintenanceJobTokenSource;
struct IAttributeManager;
struct IBucketStateCalculator;
struct IDocumentMoveHandler;
namespace bucketdb { class IBucketCreateNotifier; }

/**
 * Class that injects all concrete maintenance jobs used in document db
 * into a MaintenanceController.
 */
struct MaintenanceJobsInjector
{
    using IAttributeManagerSP = std::shared_ptr<IAttributeManager>;
    static void injectJobs(MaintenanceController &controller,
                           const DocumentDBMaintenanceConfig &config,
                           storage::spi::BucketExecutor & bucketExecutor,
                           IHeartBeatHandler &hbHandler,
                           IOperationStorer &opStorer,
                           bucketdb::IBucketCreateNotifier &bucketCreateNotifier,
                           document::BucketSpace bucketSpace,
                           IPruneRemovedDocumentsHandler &prdHandler,
                           IDocumentMoveHandler &moveHandler,
                           IBucketModifiedHandler &bucketModifiedHandler,
                           IClusterStateChangedNotifier & clusterStateChangedNotifier,
                           IBucketStateChangedNotifier & bucketStateChangedNotifier,
                           const std::shared_ptr<IBucketStateCalculator> &calc,
                           IResourceUsageNotifier &resource_usage_notifier,
                           DocumentDBJobTrackers &jobTrackers,
                           IAttributeManagerSP readyAttributeManager,
                           IAttributeManagerSP notReadyAttributeManager,
                           AttributeUsageFilter &attributeUsageFilter,
                           std::shared_ptr<MaintenanceJobTokenSource> lid_space_compaction_job_token_source,
                           std::shared_ptr<searchcorespi::IIndexManager> index_mgr);
};

} // namespace proton

