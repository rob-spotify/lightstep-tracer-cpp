#include <algorithm>

#include "test/recorder/in_memory_recorder.h"
#include "test/tracer/propagation/http_headers_carrier.h"
#include "test/tracer/propagation/text_map_carrier.h"
#include "test/tracer/propagation/utility.h"
#include "tracer/legacy/legacy_tracer_impl.h"
#include "tracer/tracer_impl.h"

#include "3rd_party/catch2/catch.hpp"
using namespace lightstep;

TEST_CASE("cloud_trace propagation") {
  LightStepTracerOptions tracer_options;
  tracer_options.propagation_modes = {PropagationMode::cloud_trace};
  auto recorder = new InMemoryRecorder{};
  auto tracer = std::shared_ptr<opentracing::Tracer>{
      new TracerImpl{MakePropagationOptions(tracer_options),
                     std::unique_ptr<Recorder>{recorder}}};
  std::unordered_map<std::string, std::string> text_map;
  TextMapCarrier text_map_carrier{text_map};
  HTTPHeadersCarrier http_headers_carrier{text_map};

  SECTION("Inject, extract yields the same span context.") {
    for (auto use_128bit_trace_ids : {true, false}) {
      auto test_span_contexts = MakeTestSpanContexts(use_128bit_trace_ids);
      for (auto& span_context : test_span_contexts) {
        // text map carrier
        CHECK_NOTHROW(
            VerifyInjectExtract(*tracer, *span_context, text_map_carrier));
        text_map.clear();

        // http headers carrier
        CHECK_NOTHROW(
            VerifyInjectExtract(*tracer, *span_context, http_headers_carrier));
        text_map.clear();
      }
    }
  }

  SECTION("Verify extraction against a long-form header with sampled set to true") {
    text_map = {{"x-cloud-trace-context", "aef5705a090040838f1359ebafa5c0c6/aef5705a09004083;o=1"}};
    auto span_context_maybe = tracer->Extract(http_headers_carrier);
    REQUIRE(span_context_maybe);
    auto span_context =
        dynamic_cast<LightStepSpanContext*>(span_context_maybe->get());
    REQUIRE(span_context->trace_id_high() == 0xaef5705a09004083ul);
    REQUIRE(span_context->trace_id_low() == 0x8f1359ebafa5c0c6ul);
    REQUIRE(span_context->span_id() == 0xaef5705a09004083ul);
    REQUIRE(span_context->sampled() == true);
  }

  SECTION("Verify extraction against a long-form header when sampled is false") {
    text_map = {{"x-cloud-trace-context", "aef5705a090040838f1359ebafa5c0c6/aef5705a09004083;o=0"}};
    auto span_context_maybe = tracer->Extract(http_headers_carrier);
    REQUIRE(span_context_maybe);
    auto span_context =
        dynamic_cast<LightStepSpanContext*>(span_context_maybe->get());
    REQUIRE(span_context->trace_id_high() == 0xaef5705a09004083ul);
    REQUIRE(span_context->trace_id_low() == 0x8f1359ebafa5c0c6ul);
    REQUIRE(span_context->span_id() == 0xaef5705a09004083ul);
    REQUIRE(span_context->sampled() == false);
  }

  SECTION("Verify extraction against a medium-form header (trace id/span id)") {
    text_map = {{"x-cloud-trace-context", "aef5705a090040838f1359ebafa5c0c6/aef5705a09004083"}};
    auto span_context_maybe = tracer->Extract(http_headers_carrier);
    REQUIRE(span_context_maybe);
    auto span_context =
        dynamic_cast<LightStepSpanContext*>(span_context_maybe->get());
    REQUIRE(span_context->trace_id_high() == 0xaef5705a09004083ul);
    REQUIRE(span_context->trace_id_low() == 0x8f1359ebafa5c0c6ul);
    REQUIRE(span_context->span_id() == 0xaef5705a09004083ul);
    REQUIRE(span_context->sampled() == true);
  }

  SECTION("Verify extraction against a short-form header (trace id)") {
    text_map = {{"x-cloud-trace-context", "aef5705a090040838f1359ebafa5c0c6"}};
    auto span_context_maybe = tracer->Extract(http_headers_carrier);
    REQUIRE(span_context_maybe);
    auto span_context =
        dynamic_cast<LightStepSpanContext*>(span_context_maybe->get());
    REQUIRE(span_context->trace_id_high() == 0xaef5705a09004083ul);
    REQUIRE(span_context->trace_id_low() == 0x8f1359ebafa5c0c6ul);

    // TODO - this will fail - I'm not sure what the behaviour should be if a
    //                         span ID isn't given
    REQUIRE(span_context->span_id() == 0x0);

    REQUIRE(span_context->sampled() == true);
  }

  SECTION("A child keeps the same trace id as its parent") {
    text_map = {{"x-cloud-trace-context", "aef5705a090040838f1359ebafa5c0c6/aef5705a09004083;o=0"}};
    auto span_context_maybe = tracer->Extract(http_headers_carrier);
    REQUIRE(span_context_maybe);
    auto span = tracer->StartSpan(
        "abc", {opentracing::ChildOf(span_context_maybe->get())});
    auto span_context1 =
        dynamic_cast<LightStepSpanContext*>(span_context_maybe->get());
    auto span_context2 =
        dynamic_cast<const LightStepSpanContext*>(&span->context());
    REQUIRE(span_context1->trace_id_high() == span_context2->trace_id_high());
    REQUIRE(span_context1->trace_id_low() == span_context2->trace_id_low());
  }

  SECTION("The low part of 128-bit trace ids are sent to satellites") {
    text_map = {{"x-cloud-trace-context", "aef5705a090040838f1359ebafa5c0c6/aef5705a09004083;o=1"}};
    auto span_context_maybe = tracer->Extract(http_headers_carrier);
    REQUIRE(span_context_maybe);
    tracer->StartSpan("abc", {opentracing::ChildOf(span_context_maybe->get())});
    REQUIRE(recorder->top().span_context().trace_id() == 0x8f1359ebafa5c0c6ul);
  }
}
