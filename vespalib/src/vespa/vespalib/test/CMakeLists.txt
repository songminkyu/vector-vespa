# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_library(vespalib_vespalib_test OBJECT
    SOURCES
    make_tls_options_for_testing.cpp
    memory_allocator_observer.cpp
    nexus.cpp
    peer_policy_utils.cpp
    test_data_base.cpp
    test_path.cpp
    thread_meets.cpp
    time_bomb.cpp
    time_tracer.cpp
    DEPENDS
)
