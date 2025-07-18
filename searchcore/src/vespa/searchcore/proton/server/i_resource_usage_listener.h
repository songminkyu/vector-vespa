// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include "resource_usage_state.h"

namespace proton {

/**
 * Interface used to receive notification when resource usage state
 * has changed.
 */
class IResourceUsageListener
{
public:
    virtual ~IResourceUsageListener() = default;
    virtual void notify_resource_usage(const ResourceUsageState& state) = 0;
};

} // namespace proton
