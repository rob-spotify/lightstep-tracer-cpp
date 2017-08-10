#pragma once

#include <lightstep/transporter.h>
#include <opentracing/tracer.h>
#include <opentracing/value.h>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

namespace lightstep {

const std::string& CollectorServiceFullName();

const std::string& CollectorMethodName();

struct LightStepTracerOptions {
  // `component_name` is the human-readable identity of the instrumented
  // process. I.e., if one drew a block diagram of the distributed system,
  // the component_name would be the name inside the box that includes this
  // process.
  std::string component_name;

  // `access_token` is the unique API key for your LightStep project. It is
  // available on your account page at https://app.lightstep.com/account
  std::string access_token;

  // The host, port, and encryption option to use for the collector. Ignored if
  // a custom transporter is used.
  std::string collector_host = "collector-grpc.lightstep.com";
  uint32_t collector_port = 443;
  std::string collector_encryption = "tls";

  // `tags` are arbitrary key-value pairs that apply to all spans generated by
  // this Tracer.
  std::unordered_map<std::string, opentracing::Value> tags;

  // Set `verbose` to true to enable more text logging.
  bool verbose = false;

  // Set `logger_sink` to a custom function to override where logging is
  // printed; otherwise, it defaults to stderr.
  std::function<void(opentracing::string_view)> logger_sink;

  // `max_buffered_spans` is the maximum number of spans that will be buffered
  // before sending them to a collector.
  size_t max_buffered_spans = 2000;

  // If `use_thread` is true, then the tracer will internally manage a thread to
  // regularly send reports to the collector; otherwise, if false,
  // LightStepTracer::Flush must be manually invoked to send reports.
  bool use_thread = true;

  // `reporting_period` is the maximum duration of time between sending spans
  // to a collector.  If zero, the default will be used; and ignored if
  // `use_thread` is false.
  std::chrono::steady_clock::duration reporting_period =
      std::chrono::milliseconds(500);

  // `report_timeout` is the timeout to use when sending a reports to the
  // collector. Ignored if a custom transport is used.
  std::chrono::system_clock::duration report_timeout = std::chrono::seconds(5);
};

struct LightStepManualTracerOptions {
  // `component_name` is the human-readable identity of the instrumented
  // process. I.e., if one drew a block diagram of the distributed system,
  // the component_name would be the name inside the box that includes this
  // process.
  std::string component_name;

  // `access_token` is the unique API key for your LightStep project. It is
  // available on your account page at https://app.lightstep.com/account
  std::string access_token;

  // `max_buffered_spans` is the maximum number of spans that will be buffered
  // before sending them to a collector.
  size_t max_buffered_spans = 2000;

  // `tags` are arbitrary key-value pairs that apply to all spans generated by
  // this Tracer.
  std::unordered_map<std::string, opentracing::Value> tags;

  // Set `verbose` to true to enable more text logging.
  bool verbose = false;

  // Set `logger_sink` to a custom function to override where logging is
  // printed; otherwise, it defaults to stderr.
  std::function<void(opentracing::string_view)> logger_sink;

  std::unique_ptr<AsyncTransporter> transporter;
};

// The LightStepTracer interface can be used by custom carriers that need more
// direct access to a span context's data so as to propagate more efficiently.
class LightStepTracer : public opentracing::Tracer {
 public:
  opentracing::expected<std::array<uint64_t, 2>> GetTraceSpanIds(
      const opentracing::SpanContext& span_context) const noexcept;

  opentracing::expected<std::unique_ptr<opentracing::SpanContext>>
  MakeSpanContext(uint64_t trace_id, uint64_t span_id,
                  std::unordered_map<std::string, std::string>&& baggage) const
      noexcept;

  virtual void Flush() noexcept = 0;
};

// Returns a std::shared_ptr to a LightStepTracer or nullptr on failure.
std::shared_ptr<LightStepTracer> MakeLightStepTracer(
    LightStepTracerOptions options) noexcept;

std::shared_ptr<LightStepTracer> MakeLightStepTracer(
    LightStepManualTracerOptions options) noexcept;
}  // namespace lightstep
