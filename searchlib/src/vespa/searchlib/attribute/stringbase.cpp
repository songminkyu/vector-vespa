// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "stringbase.h"
#include "attributevector.hpp"
#include "load_utils.h"
#include "readerbase.h"
#include "enum_store_loaders.h"
#include "string_sort_blob_writer.h"
#include <vespa/searchlib/common/sort.h>
#include <vespa/searchlib/query/query_term_ucs4.h>
#include <vespa/searchcommon/attribute/i_sort_blob_writer.h>
#include <vespa/searchcommon/attribute/config.h>
#include <vespa/vespalib/locale/c.h>

#include <vespa/log/log.h>
LOG_SETUP(".searchlib.attribute.stringbase");

namespace search {

size_t
StringAttribute::countZero(const char * bt, size_t sz)
{
    size_t size(0);
    for(size_t i(0); i < sz; i++) {
        if (bt[i] == '\0') {
            size++;
        }
    }
    return size;
}

StringAttribute::StringAttribute(const std::string & name) :
    AttributeVector(name, Config(BasicType::STRING)),
    _changes(),
    _defaultValue(ChangeBase::UPDATE, 0, std::string(""))
{
}

StringAttribute::StringAttribute(const std::string & name, const Config & c) :
    AttributeVector(name, c),
    _changes(),
    _defaultValue(ChangeBase::UPDATE, 0, std::string(""))
{
}

StringAttribute::~StringAttribute() = default;

uint32_t
StringAttribute::get(DocId doc, WeightedInt * v, uint32_t sz) const
{
    auto * s = new WeightedConstChar[sz];
    uint32_t n = static_cast<const AttributeVector *>(this)->get(doc, s, sz);
    for(uint32_t i(0),m(std::min(n,sz)); i<m; i++) {
        v[i] = WeightedInt(strtoll(s[i].getValue(), nullptr, 0), s[i].getWeight());
    }
    delete [] s;
    return n;
}

uint32_t
StringAttribute::get(DocId doc, WeightedFloat * v, uint32_t sz) const
{
    auto * s = new WeightedConstChar[sz];
    uint32_t n = static_cast<const AttributeVector *>(this)->get(doc, s, sz);
    for(uint32_t i(0),m(std::min(n,sz)); i<m; i++) {
        v[i] = WeightedFloat(vespalib::locale::c::strtod(s[i].getValue(), nullptr), s[i].getWeight());
    }
    delete [] s;
    return n;
}

double
StringAttribute::getFloat(DocId doc) const {
    return vespalib::locale::c::strtod(get(doc), nullptr);
}

std::span<const char>
StringAttribute::get_raw(DocId doc) const
{
    const char * s = get(doc);
    return {s, s ? ::strlen(s) : 0u};
}

uint32_t
StringAttribute::get(DocId doc, double * v, uint32_t sz) const
{
    const char ** s = new const char *[sz];
    uint32_t n = static_cast<const AttributeVector *>(this)->get(doc, s, sz);
    for(uint32_t i(0),m(std::min(n,sz)); i<m; i++) {
        v[i] = vespalib::locale::c::strtod(s[i], nullptr);
    }
    delete [] s;
    return n;
}

uint32_t
StringAttribute::get(DocId doc, largeint_t * v, uint32_t sz) const
{
    const char ** s = new const char *[sz];
    uint32_t n = static_cast<const AttributeVector *>(this)->get(doc, s, sz);
    for(uint32_t i(0),m(std::min(n,sz)); i<m; i++) {
        v[i] = strtoll(s[i], nullptr, 0);
    }
    delete [] s;
    return n;
}

bool
StringAttribute::is_sortable() const noexcept
{
    return true;
}

namespace {

template <bool asc>
class SingleStringSortBlobWriter : public attribute::ISortBlobWriter {
    const StringAttribute& _attr;
    attribute::StringSortBlobWriter<asc> _writer;
public:
    SingleStringSortBlobWriter(const StringAttribute& attr, const common::BlobConverter* bc,
                               common::sortspec::MissingPolicy policy, std::string_view missing_value) noexcept;
    ~SingleStringSortBlobWriter() override;
    long write(uint32_t docid, void* ser_to, long available) override;
};

template <bool asc>
SingleStringSortBlobWriter<asc>::SingleStringSortBlobWriter(const StringAttribute& attr,
                                                            const common::BlobConverter* bc,
                                                            common::sortspec::MissingPolicy policy,
                                                            std::string_view missing_value) noexcept
    : _attr(attr),
      _writer(bc, policy, missing_value, false)
{
}

template <bool asc>
SingleStringSortBlobWriter<asc>::~SingleStringSortBlobWriter() = default;

template <bool asc>
long
SingleStringSortBlobWriter<asc>::write(uint32_t docid, void* ser_to, long available) {
    _writer.reset(ser_to, available);
    const char* value = _attr.get(docid);
    if (!attribute::isUndefined(value)) {
        if (!_writer.candidate(value)) {
            return -1;
        }
    }
    return _writer.write();
}

}

std::unique_ptr<attribute::ISortBlobWriter>
StringAttribute::make_sort_blob_writer(bool ascending, const common::BlobConverter* bc,
                                       common::sortspec::MissingPolicy policy,
                                       std::string_view missing_value) const
{
    if (ascending) {
        return std::make_unique<SingleStringSortBlobWriter<true>>(*this, bc, policy, missing_value);
    } else {
        return std::make_unique<SingleStringSortBlobWriter<false>>(*this, bc, policy, missing_value);
    }
}

uint32_t
StringAttribute::clearDoc(DocId doc)
{
    uint32_t removed(0);
    if (hasMultiValue() && (doc < getNumDocs())) {
        removed = getValueCount(doc);
    }
    AttributeVector::clearDoc(_changes, doc);

    return removed;
}

bool
StringAttribute::applyWeight(DocId doc, const FieldValue & fv, const ArithmeticValueUpdate & wAdjust)
{
    std::string v = fv.getAsString();
    return AttributeVector::adjustWeight(_changes, doc, StringChangeData(v), wAdjust);
}

bool
StringAttribute::applyWeight(DocId doc, const FieldValue& fv, const document::AssignValueUpdate& wAdjust)
{
    std::string v = fv.getAsString();
    return AttributeVector::adjustWeight(_changes, doc, StringChangeData(v), wAdjust);
}

bool
StringAttribute::apply(DocId, const ArithmeticValueUpdate & )
{
    return false;
}

bool
StringAttribute::onLoadEnumerated(ReaderBase &attrReader)
{
    auto udatBuffer = attribute::LoadUtils::loadUDAT(*this);

    bool hasIdx(attrReader.hasIdx());
    size_t numDocs(0);
    uint64_t numValues(0);
    if (hasIdx) {
        numDocs = attrReader.getNumIdx() - 1;
        numValues = attrReader.getNumValues();
        uint64_t enumCount = attrReader.getEnumCount();
        assert(numValues == enumCount);
        (void) enumCount;
    } else {
        numValues = attrReader.getEnumCount();
        numDocs = numValues;
    }

    setNumDocs(numDocs);
    setCommittedDocIdLimit(numDocs);
    set_size_on_disk(attrReader.size_on_disk() + udatBuffer->size_on_disk());
    set_last_flush_duration(attrReader.flush_duration());

    if (hasPostings()) {
        auto loader = this->getEnumStoreBase()->make_enumerated_postings_loader();
        loader.load_unique_values(udatBuffer->buffer(), udatBuffer->size());
        loader.build_enum_value_remapping();
        load_enumerated_data(attrReader, loader, numValues);
        if (numDocs > 0) {
            onAddDoc(numDocs - 1);
        }
        load_posting_lists_and_update_enum_store(loader);
    } else {
        auto loader = this->getEnumStoreBase()->make_enumerated_loader();
        loader.load_unique_values(udatBuffer->buffer(), udatBuffer->size());
        loader.build_enum_value_remapping();
        load_enumerated_data(attrReader, loader);
    }
    return true;
}

bool
StringAttribute::onLoad(vespalib::Executor *)
{
    ReaderBase attrReader(*this);
    bool ok(attrReader.getHasLoadData());

    if (!ok) {
        return false;
    }

    getEnumStoreBase()->clear_default_value_ref();
    commit();
    incGeneration();

    setCreateSerialNum(attrReader.getCreateSerialNum());

    assert(attrReader.getEnumerated());
    return onLoadEnumerated(attrReader);
}

bool
StringAttribute::onAddDoc(DocId )
{
    return false;
}

void
StringAttribute::load_posting_lists(LoadedVector&)
{
}

void
StringAttribute::load_enum_store(LoadedVector&)
{
}

void
StringAttribute::fillValues(LoadedVector & )
{
}

void
StringAttribute::load_enumerated_data(ReaderBase&, enumstore::EnumeratedPostingsLoader&, size_t)
{
    LOG_ABORT("Should not be reached");
}

void
StringAttribute::load_enumerated_data(ReaderBase&, enumstore::EnumeratedLoader&)
{
    LOG_ABORT("Should not be reached");
}

void
StringAttribute::load_posting_lists_and_update_enum_store(enumstore::EnumeratedPostingsLoader&)
{
    LOG_ABORT("Should not be reached");
}

vespalib::MemoryUsage
StringAttribute::getChangeVectorMemoryUsage() const
{
    return _changes.getMemoryUsage();
}

bool
StringAttribute::get_match_is_cased() const noexcept {
    return getConfig().get_match() == attribute::Config::Match::CASED;
}

bool
StringAttribute::has_uncased_matching() const noexcept
{
    return !get_match_is_cased();
}

template bool AttributeVector::clearDoc(StringAttribute::ChangeVector& changes, DocId doc);
template bool AttributeVector::update(StringAttribute::ChangeVector& changes, DocId doc, const StringChangeData& v);
template bool AttributeVector::append(StringAttribute::ChangeVector& changes, DocId doc, const StringChangeData& v, int32_t w, bool doCount);
template bool AttributeVector::remove(StringAttribute::ChangeVector& changes, DocId doc, const StringChangeData& v, int32_t w);

}
