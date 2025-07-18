# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_executable(juniper_mcandTest_app TEST
    SOURCES
    mcandTest.cpp
    queryparser.cpp
    testenv.cpp
    DEPENDS
    vespa_searchsummary
    GTest::gtest
)
vespa_add_test(NAME juniper_mcandTest_app COMMAND juniper_mcandTest_app)
vespa_add_executable(juniper_queryparserTest_app TEST
    SOURCES
    queryparser.cpp
    queryparserTest.cpp
    testenv.cpp
    DEPENDS
    vespa_searchsummary
    GTest::gtest
)
vespa_add_test(NAME juniper_queryparserTest_app COMMAND juniper_queryparserTest_app)
vespa_add_executable(juniper_matchobjectTest_app TEST
    SOURCES
    matchobjectTest.cpp
    queryparser.cpp
    testenv.cpp
    DEPENDS
    vespa_searchsummary
    GTest::gtest
)
vespa_add_test(NAME juniper_matchobjectTest_app COMMAND juniper_matchobjectTest_app)
vespa_add_executable(juniper_appender_test_app TEST
    SOURCES
    appender_test.cpp
    DEPENDS
    vespa_searchsummary
    GTest::gtest
)
vespa_add_test(NAME juniper_appender_test_app COMMAND juniper_appender_test_app)
vespa_add_executable(juniper_queryvisitor_test_app TEST
    SOURCES
    queryparser.cpp
    queryvisitor_test.cpp
    DEPENDS
    vespa_searchsummary
    GTest::gtest
)
vespa_add_test(NAME juniper_queryvisitor_test_app COMMAND juniper_queryvisitor_test_app)
vespa_add_executable(juniper_auxTest_app TEST
    SOURCES
    auxTest.cpp
    queryparser.cpp
    testenv.cpp
    DEPENDS
    vespa_searchsummary
    GTest::gtest
)
vespa_add_test(NAME juniper_auxTest_app COMMAND juniper_auxTest_app)
vespa_add_executable(juniper_latintokenizertest_app TEST
    SOURCES
    latintokenizertest.cpp
    DEPENDS
    GTest::gtest
)
vespa_add_test(NAME juniper_latintokenizertest_app NO_VALGRIND COMMAND juniper_latintokenizertest_app)
