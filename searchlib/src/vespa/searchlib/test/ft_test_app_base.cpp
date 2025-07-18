// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "ft_test_app_base.h"
#include <vespa/searchlib/fef/test/dummy_dependency_handler.h>
#include <vespa/vespalib/util/stringfmt.h>

#include <vespa/log/log.h>
LOG_SETUP(".ft_test_app_base");

namespace fieldmatch = search::features::fieldmatch;
using search::fef::test::DummyDependencyHandler;
using search::fef::FieldInfo;
using search::fef::FieldType;
using search::fef::Properties;
using search::fef::test::RankResult;

void
FtTestAppBase::FT_SETUP_FAIL(const search::fef::Blueprint &prototype, const StringList &params)
{
    search::fef::test::IndexEnvironment ie;
    FT_SETUP_FAIL(prototype, ie, params);
}

void
FtTestAppBase::FT_SETUP_FAIL(const search::fef::Blueprint &prototype, const search::fef::test::IndexEnvironment &env,
                             const StringList &params)
{
    FT_LOG(prototype, env, params);
    search::fef::Blueprint::UP bp = prototype.createInstance();
    DummyDependencyHandler deps(*bp);
    EXPECT_TRUE(!bp->setup(env, params));
}

void
FtTestAppBase::FT_SETUP_OK(const search::fef::Blueprint &prototype, const StringList &params,
                           const StringList &expectedIn, const StringList &expectedOut)
{
    search::fef::test::IndexEnvironment ie;
    FT_SETUP_OK(prototype, ie, params, expectedIn, expectedOut);
}

void
FtTestAppBase::FT_SETUP_OK(const search::fef::Blueprint &prototype, const search::fef::test::IndexEnvironment &env,
                           const StringList &params, const StringList &expectedIn, const StringList &expectedOut)
{
    FT_LOG(prototype, env, params);
    search::fef::Blueprint::UP bp = prototype.createInstance();
    DummyDependencyHandler deps(*bp);
    ASSERT_TRUE(bp->setup(env, params));
    FT_EQUAL(expectedIn,  deps.input, "In, ");
    FT_EQUAL(expectedOut, deps.output, "Out,");
}

void
FtTestAppBase::FT_DUMP_EMPTY(search::fef::BlueprintFactory &factory, const std::string &baseName)
{
    StringList empty;
    FT_DUMP(factory, baseName, empty);
}

void
FtTestAppBase::FT_DUMP_EMPTY(search::fef::BlueprintFactory &factory, const std::string &baseName,
                             search::fef::test::IndexEnvironment &env)
{
    StringList empty;
    FT_DUMP(factory, baseName, env, empty);
}

void
FtTestAppBase::FT_DUMP(search::fef::BlueprintFactory &factory, const std::string &baseName,
                       const StringList &expected)
{
    search::fef::test::IndexEnvironment ie;
    FT_DUMP(factory, baseName, ie, expected);
}

void
FtTestAppBase::FT_DUMP(search::fef::BlueprintFactory &factory, const std::string &baseName,
                       search::fef::test::IndexEnvironment &env,
                       const StringList &expected)
{
    FtDumpFeatureVisitor dfv;
    search::fef::Blueprint::SP bp = factory.createBlueprint(baseName);
    if ( ! bp) {
        LOG(error, "Blueprint '%s' does not exist in factory, did you forget to add it?", baseName.c_str());
        ASSERT_TRUE(bp);
    }
    bp->visitDumpFeatures(env, dfv);
    FT_EQUAL(expected, dfv.features(), "Dump");
}

void
FtTestAppBase::FT_EQUAL(const std::vector<string> &expected, const std::vector<string> &actual,
                        const std::string &prefix)
{
    FT_LOG(prefix + " expected", expected);
    FT_LOG(prefix + " actual  ", actual);
    EXPECT_EQ(expected.size(), actual.size());
    ASSERT_TRUE(expected.size() == actual.size());
    for (uint32_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(expected[i], actual[i]);
        ASSERT_TRUE(expected[i] == actual[i]);
    }
}

void
FtTestAppBase::FT_LOG(const search::fef::Blueprint &prototype, const search::fef::test::IndexEnvironment &env,
                      const StringList &params)
{
    LOG(info, "Testing blueprint '%s'.", prototype.getBaseName().c_str());
    std::vector<std::string> arr;
    for (const auto & it : env.getFields()) {
        arr.push_back(it.name());
    }
    FT_LOG("Environment  ", arr);
    FT_LOG("Parameters   ", params);
}

void
FtTestAppBase::FT_LOG(const std::string &prefix, const std::vector<std::string> &arr)
{
    std::string str = prefix + " = [ ";
    for (uint32_t i = 0; i < arr.size(); ++i) {
        str.append("'").append(arr[i]).append("'");
        if (i < arr.size() - 1) {
            str.append(", ");
        }
    }
    str.append(" ]");
    LOG(info, "%s", str.c_str());
}

void
FtTestAppBase::FT_SETUP(FtFeatureTest &test, const std::string &query, const StringMap &index,
                        uint32_t docId)
{
    LOG(info, "Setup test for query '%s'.", query.c_str());

    // Add all query terms.
    FtQueryEnvironment &queryEnv = test.getQueryEnv();
    for (uint32_t i = 0; i < query.size(); ++i) {
        queryEnv.getBuilder().addAllFields();
    }
    ASSERT_TRUE(test.setup());

    // Add all occurences.
    search::fef::test::MatchDataBuilder::UP mdb = test.createMatchDataBuilder();
    for (auto it = index.begin();it != index.end(); ++it) {
        ASSERT_TRUE(mdb->setFieldLength(it->first, it->second.size()));
        for (uint32_t i = 0; i < it->second.size(); ++i) {
            size_t pos = query.find_first_of(it->second[i]);
            if (pos != std::string::npos) {
                LOG(debug, "Occurence of '%c' added to field '%s' at position %d.", query[pos], it->first.c_str(), i);
                ASSERT_TRUE(mdb->addOccurence(it->first, pos, i));
            }
        }
    }
    ASSERT_TRUE(mdb->apply(docId));
}

void
FtTestAppBase::FT_SETUP(FtFeatureTest & test, const std::vector<FtQueryTerm> & query, const StringVectorMap & index,
                        uint32_t docId)
{
    setupQueryEnv(test.getQueryEnv(), query);
    ASSERT_TRUE(test.setup());

    search::fef::test::MatchDataBuilder::UP mdb = test.createMatchDataBuilder();

    // Add all occurences.
    for (auto itr = index.begin(); itr != index.end(); ++itr) {
        ASSERT_TRUE(mdb->setFieldLength(itr->first, itr->second.size()));
        for (uint32_t i = 0; i < itr->second.size(); ++i) {
            auto fitr = query.begin();
            for (;;) {
                fitr = std::find(fitr, query.end(), FtQueryTerm(itr->second[i]));
                if (fitr != query.end()) {
                    uint32_t termId = fitr - query.begin();
                    LOG(debug, "Occurence of '%s' added to field '%s' at position %u.", fitr->term.c_str(), itr->first.c_str(), i);
                    ASSERT_TRUE(mdb->addOccurence(itr->first, termId, i));
                    ++fitr;
                } else {
                    break;
                }
            }
        }
    }
    ASSERT_TRUE(mdb->apply(docId));
}

void
FtTestAppBase::FT_SETUP(FtFeatureTest &test, const FtQuery &query, const FtIndex &index, uint32_t docId)
{
    setupQueryEnv(test.getQueryEnv(), query);
    ASSERT_TRUE(test.setup());
    search::fef::test::MatchDataBuilder::UP mdb = test.createMatchDataBuilder();

    // Add all occurences.
    for (auto itr = index.index.begin(); itr != index.index.end(); ++itr) {
        const FtIndex::Field &field = itr->second;
        for (size_t e = 0; e < field.size(); ++e) {
            const FtIndex::Element &element = field[e];
            ASSERT_TRUE(mdb->addElement(itr->first, element.weight, element.tokens.size()));
            for (size_t t = 0; t < element.tokens.size(); ++t) {
                const std::string &token = element.tokens[t];
                for (size_t q = 0; q < query.size(); ++q) {
                    if (query[q].term == token) {
                        ASSERT_TRUE(mdb->addOccurence(itr->first, q, t, e));
                    }
                }
            }
        }
    }
    ASSERT_TRUE(mdb->apply(docId));
}

void
FtTestAppBase::setupQueryEnv(FtQueryEnvironment & queryEnv, const FtQuery & query)
{
    // Add all query terms.
    for (uint32_t i = 0; i < query.size(); ++i) {
        queryEnv.getBuilder().addAllFields();
        queryEnv.getTerms()[i].setPhraseLength(1);
        queryEnv.getTerms()[i].setUniqueId(i);
        queryEnv.getTerms()[i].setWeight(query[i].termWeight);
        if (i > 0) {
            std::string from = vespalib::make_string("vespa.term.%u.connexity", i);
            std::string to = vespalib::make_string("%u", i - 1);
            std::string connexity = vespalib::make_string("%f", query[i].connexity);
            queryEnv.getProperties().add(from, to);
            queryEnv.getProperties().add(from, connexity);
        }
        std::string term = vespalib::make_string("vespa.term.%u.significance", i);
        std::string significance = vespalib::make_string("%f", query[i].significance);
        queryEnv.getProperties().add(term, significance);
        LOG(debug, "Add term node: '%s'", query[i].term.c_str());
    }
}

void
FtTestAppBase::setupFieldMatch(FtFeatureTest & ft, const std::string & indexName,
                               const std::string & query, const std::string & field,
                               const fieldmatch::Params * params, uint32_t totalTermWeight, feature_t totalSignificance,
                               uint32_t docId)
{
    ft.getIndexEnv().getBuilder().addField(FieldType::INDEX, FieldInfo::CollectionType::SINGLE, indexName);

    if (params != nullptr) {
        Properties & p = ft.getIndexEnv().getProperties();
        p.add("fieldMatch(" + indexName + ").proximityLimit", vespalib::make_string("%u", params->getProximityLimit()));
        p.add("fieldMatch(" + indexName + ").maxAlternativeSegmentations", vespalib::make_string("%u", params->getMaxAlternativeSegmentations()));
        p.add("fieldMatch(" + indexName + ").maxOccurrences", vespalib::make_string("%u", params->getMaxOccurrences()));
        p.add("fieldMatch(" + indexName + ").proximityCompletenessImportance", vespalib::make_string("%f", params->getProximityCompletenessImportance()));
        p.add("fieldMatch(" + indexName + ").relatednessImportance", vespalib::make_string("%f", params->getRelatednessImportance()));
        p.add("fieldMatch(" + indexName + ").earlinessImportance", vespalib::make_string("%f", params->getEarlinessImportance()));
        p.add("fieldMatch(" + indexName + ").segmentProximityImportance", vespalib::make_string("%f", params->getSegmentProximityImportance()));
        p.add("fieldMatch(" + indexName + ").occurrenceImportance", vespalib::make_string("%f", params->getOccurrenceImportance()));
        p.add("fieldMatch(" + indexName + ").fieldCompletenessImportance", vespalib::make_string("%f", params->getFieldCompletenessImportance()));
        for (double it : params->getProximityTable()) {
            p.add("fieldMatch(" + indexName + ").proximityTable", vespalib::make_string("%f", it));
        }
    }

    if (totalTermWeight > 0) {
        ft.getQueryEnv().getProperties().add("fieldMatch(" + indexName + ").totalTermWeight",
                                             vespalib::make_string("%u", totalTermWeight));
    }

    if (totalSignificance > 0.0f) {
        ft.getQueryEnv().getProperties().add("fieldMatch(" + indexName + ").totalTermSignificance",
                vespalib::make_string("%f", totalSignificance));
    }

    std::map<std::string, std::vector<std::string> > index;
    index[indexName] = FtUtil::tokenize(field);
    FT_SETUP(ft, FtUtil::toQuery(query), index, docId);
}


RankResult
FtTestAppBase::toRankResult(const std::string & baseName,
                            const std::string & result,
                            const std::string & separator)
{
    return FtUtil::toRankResult(baseName, result, separator);
}
