#pragma once

#include "tracer/propagation/propagator.h"

namespace lightstep {
class CloudTracePropagator final : public Propagator {
 public:
  // Propagator
  opentracing::expected<void> InjectSpanContext(
      const opentracing::TextMapWriter& carrier,
      const TraceContext& trace_context, opentracing::string_view trace_state,
      const BaggageProtobufMap& baggage) const override;

  opentracing::expected<void> InjectSpanContext(
      const opentracing::TextMapWriter& carrier,
      const TraceContext& trace_context, opentracing::string_view trace_state,
      const BaggageFlatMap& baggage) const override;

  opentracing::expected<bool> ExtractSpanContext(
      const opentracing::TextMapReader& carrier, bool case_sensitive,
      TraceContext& trace_context, std::string& trace_state,
      BaggageProtobufMap& baggage) const override;

  private:
   opentracing::expected<void> InjectSpanContextImpl(
       const opentracing::TextMapWriter& carrier,
       const TraceContext& trace_context) const;

   template <class KeyCompare>
   opentracing::expected<bool> ExtractSpanContextImpl(
       const opentracing::TextMapReader& carrier, TraceContext& trace_context,
       const KeyCompare& key_compare, BaggageProtobufMap& baggage) const;

   opentracing::expected<void> ParseCloudTrace(
       opentracing::string_view s, lightstep::TraceContext& trace_context) const noexcept;

   void SerializeCloudTrace(const TraceContext& trace_context,
                              char* s) const noexcept;
};
}  // namespace lightstep