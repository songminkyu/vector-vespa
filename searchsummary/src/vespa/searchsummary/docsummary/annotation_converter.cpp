// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "annotation_converter.h"
#include "i_juniper_converter.h"
#include <vespa/document/annotation/annotation.h>
#include <vespa/document/annotation/span.h>
#include <vespa/document/fieldvalue/stringfieldvalue.h>
#include <vespa/juniper/juniper_separators.h>
#include <vespa/searchlib/memoryindex/field_inverter.h>
#include <vespa/searchlib/util/linguisticsannotation.h>
#include <vespa/searchlib/util/token_extractor.h>
#include <vespa/vespalib/stllike/asciistream.h>
#include <vespa/vespalib/util/exceptions.h>
#include <utility>

using document::Annotation;
using document::FieldValue;
using document::Span;
using document::StringFieldValue;
using search::linguistics::TokenExtractor;
using search::memoryindex::FieldInverter;

namespace search::docsummary {

namespace {

std::string_view
getSpanString(std::string_view s, const Span &span)
{
    return {s.data() + span.from(), static_cast<size_t>(span.length())};
}

std::string dummy_field_name;

}

AnnotationConverter::AnnotationConverter(IJuniperConverter& juniper_converter)
    : IStringFieldConverter(),
      _juniper_converter(juniper_converter),
      _text(),
      _out()
{
}

AnnotationConverter::~AnnotationConverter() = default;

template <typename ForwardIt>
void
AnnotationConverter::handleAnnotations(const document::Span& span, ForwardIt it, ForwardIt last) {
    int annCnt = (last - it);
    if (annCnt > 1 || (annCnt == 1 && it->altered)) {
        annotateSpans(span, it, last);
    } else {
        _out << getSpanString(_text, span) << juniper::separators::unit_separator_string;
    }
}

template <typename ForwardIt>
void
AnnotationConverter::annotateSpans(const document::Span& span, ForwardIt it, ForwardIt last) {
    _out << juniper::separators::interlinear_annotation_anchor_string  // ANCHOR
         << (getSpanString(_text, span))
         << juniper::separators::interlinear_annotation_separator_string; // SEPARATOR
    while (it != last) {
        _out << it->word;
        if (++it != last) {
            _out << " ";
        }
    }
    _out << juniper::separators::interlinear_annotation_terminator_string  // TERMINATOR
         << juniper::separators::unit_separator_string;
}

void
AnnotationConverter::handleIndexingTerms(const StringFieldValue& value)
{
    using SpanTermVector = TokenExtractor::SpanTermVector;
    SpanTermVector terms;
    auto span_trees = value.getSpanTrees();
    TokenExtractor token_extractor(dummy_field_name, FieldInverter::max_word_len);
    token_extractor.extract(terms, span_trees, _text, nullptr);
    auto it = terms.begin();
    auto ite = terms.end();
    int32_t endPos = 0;
    for (; it != ite; ) {
        int32_t curStart = it->span.from();
        if (curStart > endPos) {
            // text not indexed
            Span tmpSpan(endPos, curStart - endPos);
            handleAnnotations(tmpSpan, it, it);
            endPos = curStart;
        }
        int32_t curEnd = it->span.to();
        auto it_begin = it++;
        for (; it != ite && it->span.from() == curStart; ++it) {
            // find longest span starting at same place
            curEnd = std::max(curEnd, it->span.to());
        }
        for (; it != ite && it->span.to() <= curEnd; ++it) {
            // gather all spans contained within the block
        }
        document::Span curSpan(curStart, curEnd - curStart);
        handleAnnotations(curSpan, it_begin, it);
        endPos = curEnd;
    }
    int32_t wantEndPos = _text.size();
    if (endPos < wantEndPos) {
        Span tmpSpan(endPos, wantEndPos - endPos);
        handleAnnotations(tmpSpan, ite, ite);
    }
}

void
AnnotationConverter::convert(const StringFieldValue &input, vespalib::slime::Inserter& inserter)
{
    _out.clear();
    _text = input.getValueRef();
    handleIndexingTerms(input);
    _juniper_converter.convert(_out.view(), inserter);
}

bool
AnnotationConverter::render_weighted_set_as_array() const
{
    return false;
}

}
