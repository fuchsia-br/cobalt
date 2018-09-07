// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "tools/test_app2/test_app.h"

#include <google/protobuf/text_format.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "gflags/gflags.h"
#include "glog/logging.h"
#include "logger/project_context.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace cobalt {

using logger::LoggerInterface;
using logger::ProjectContext;

DECLARE_uint32(num_clients);
DECLARE_string(values);

namespace {
static const uint32_t kCustomerId = 1;
static const uint32_t kProjectId = 1;
static const char kCustomerName[] = "Fuchsia";
static const char kProjectName[] = "Cobalt";
static const char kErrorOccurredMetricName[] = "ErrorOccurred";

const char* const kMetricDefinitions = R"(
metric {
  metric_name: "ErrorOccurred"
  metric_type: EVENT_OCCURRED
  customer_id: 1
  project_id: 1
  id: 1
  max_event_type_index: 100
  reports: {
    report_name: "ErrorCountsByType"
    id: 123
    report_type: SIMPLE_OCCURRENCE_COUNT
    local_privacy_noise_level: SMALL
  }
}

metric {
  metric_name: "CacheMiss"
  metric_type: EVENT_COUNT
  customer_id: 1
  project_id: 1
  id: 2
  reports: {
    report_name: "CacheMissCounts"
    id: 111
    report_type: EVENT_COMPONENT_OCCURRENCE_COUNT
  }
}

)";

bool PopulateMetricDefinitions(MetricDefinitions* metric_definitions) {
  google::protobuf::TextFormat::Parser parser;
  return parser.ParseFromString(kMetricDefinitions, metric_definitions);
}

class TestLoggerFactory : public LoggerFactory {
 public:
  explicit TestLoggerFactory(const ProjectContext* project_context);

  std::unique_ptr<LoggerInterface> NewLogger() override;

  const ProjectContext* project_context() override;
  bool SendAccumulatedObservtions() override;

 private:
  const ProjectContext* project_context_;
};

TestLoggerFactory::TestLoggerFactory(const ProjectContext* project_context)
    : project_context_(project_context) {}

std::unique_ptr<LoggerInterface> TestLoggerFactory::NewLogger() {
  return nullptr;
}

const ProjectContext* TestLoggerFactory::project_context() {
  return project_context_;
}

bool TestLoggerFactory::SendAccumulatedObservtions() { return true; }

}  // namespace

// Tests of the TestApp class.
class TestAppTest : public ::testing::Test {
 public:
  void SetUp() {
    auto metric_definitions = std::make_unique<MetricDefinitions>();
    ASSERT_TRUE(PopulateMetricDefinitions(metric_definitions.get()));
    project_context_.reset(new ProjectContext(kCustomerId, kProjectId,
                                              kCustomerName, kProjectName,
                                              std::move(metric_definitions)));
    std::unique_ptr<LoggerFactory> logger_factory(
        new TestLoggerFactory(project_context_.get()));
    test_app_.reset(new TestApp(std::move(logger_factory),
                                kErrorOccurredMetricName, TestApp::kInteractive,
                                &output_stream_));
  }

 protected:
  // Clears the contents of the TestApp's output stream and returns the
  // contents prior to clearing.
  std::string ClearOutput() {
    std::string s = output_stream_.str();
    output_stream_.str("");
    return s;
  }

  // Does the current contents of the TestApp's output stream contain the
  // given text.
  bool OutputContains(const std::string text) {
    return std::string::npos != output_stream_.str().find(text);
  }

  // Is the TestApp's output stream curently empty?
  bool NoOutput() { return output_stream_.str().empty(); }

  std::unique_ptr<ProjectContext> project_context_;

  // The output stream that the TestApp has been given.
  std::ostringstream output_stream_;

  // The TestApp under test.
  std::unique_ptr<TestApp> test_app_;
};

//////////////////////////////////////
// Tests of interactive mode.
/////////////////////////////////////

// Tests processing a bad command line.
TEST_F(TestAppTest, ProcessCommandLineBad) {
  EXPECT_TRUE(test_app_->ProcessCommandLine("this is not a command"));
  EXPECT_TRUE(OutputContains("Unrecognized command: this"));
}

// Tests processing the "help" command
TEST_F(TestAppTest, ProcessCommandLineHelp) {
  EXPECT_TRUE(test_app_->ProcessCommandLine("help"));
  // We don't want to test the actual output too rigorously because that would
  // be a very fragile test. Just doing a sanity test.
  EXPECT_TRUE(OutputContains("Print this help message."));
}

// Tests processing a bad set command line.
TEST_F(TestAppTest, ProcessCommandLineSetBad) {
  EXPECT_TRUE(test_app_->ProcessCommandLine("set"));
  EXPECT_TRUE(OutputContains("Malformed set command."));
  ClearOutput();

  EXPECT_TRUE(test_app_->ProcessCommandLine("set a b c"));
  EXPECT_TRUE(OutputContains("Malformed set command."));
  ClearOutput();

  EXPECT_TRUE(test_app_->ProcessCommandLine("set a b"));
  EXPECT_TRUE(OutputContains("a is not a settable parameter"));
  ClearOutput();

  EXPECT_TRUE(test_app_->ProcessCommandLine("set metric b"));
  EXPECT_TRUE(OutputContains("There is no metric named 'b'"));
  EXPECT_TRUE(OutputContains("Current metric unchanged."));
  ClearOutput();
}

// Tests processing the set and ls commands
TEST_F(TestAppTest, ProcessCommandLineSetAndLs) {
  EXPECT_TRUE(test_app_->ProcessCommandLine("ls"));
  EXPECT_TRUE(OutputContains("Metric: 'ErrorOccurred'"));
  EXPECT_TRUE(OutputContains("Customer: Fuchsia"));
  ClearOutput();

  EXPECT_TRUE(test_app_->ProcessCommandLine("set metric CacheMiss"));
  EXPECT_TRUE(OutputContains("Metric set."));

  EXPECT_TRUE(test_app_->ProcessCommandLine("ls"));
  EXPECT_TRUE(OutputContains("Metric: 'CacheMiss'"));
  EXPECT_TRUE(OutputContains("Customer: Fuchsia"));
  ClearOutput();
}

// Tests processing a bad show command line.
TEST_F(TestAppTest, ProcessCommandLineShowBad) {
  EXPECT_TRUE(test_app_->ProcessCommandLine("show"));
  EXPECT_TRUE(OutputContains("Expected 'show config'."));
  ClearOutput();

  EXPECT_TRUE(test_app_->ProcessCommandLine("show confi"));
  EXPECT_TRUE(OutputContains("Expected 'show config'."));
  ClearOutput();

  EXPECT_TRUE(test_app_->ProcessCommandLine("show config foo"));
  EXPECT_TRUE(OutputContains("Expected 'show config'."));
  ClearOutput();
}

// Tests processing the set and show config commands
TEST_F(TestAppTest, ProcessCommandLineSetAndShowConfig) {
  EXPECT_TRUE(test_app_->ProcessCommandLine("show config"));
  EXPECT_TRUE(OutputContains("metric_name: \"ErrorOccurred\""));
  EXPECT_TRUE(OutputContains("metric_type: EVENT_OCCURRED"));
  EXPECT_TRUE(OutputContains("report_name: \"ErrorCountsByType\""));
  EXPECT_TRUE(OutputContains("report_type: SIMPLE_OCCURRENCE_COUNT"));
  ClearOutput();

  EXPECT_TRUE(test_app_->ProcessCommandLine("set metric CacheMiss"));
  EXPECT_TRUE(OutputContains("Metric set."));

  EXPECT_TRUE(test_app_->ProcessCommandLine("show config"));
  EXPECT_TRUE(OutputContains("metric_name: \"CacheMiss\""));
  EXPECT_TRUE(OutputContains("metric_type: EVENT_COUNT"));
  EXPECT_TRUE(OutputContains("report_name: \"CacheMissCounts\""));
  EXPECT_TRUE(OutputContains("report_type: EVENT_COMPONENT_OCCURRENCE_COUNT"));
}

// Tests processing a bad log command line.
TEST_F(TestAppTest, ProcessCommandLineEncodeBad) {
  EXPECT_TRUE(test_app_->ProcessCommandLine("log"));
  EXPECT_TRUE(OutputContains("Malformed log command."));
  ClearOutput();
  EXPECT_TRUE(test_app_->ProcessCommandLine("log foo"));
  EXPECT_TRUE(OutputContains("Expected non-negative integer instead of foo."));

  ClearOutput();
  EXPECT_TRUE(test_app_->ProcessCommandLine("log -1"));
  EXPECT_TRUE(OutputContains("Expected non-negative integer instead of -1."));

  ClearOutput();
  EXPECT_TRUE(test_app_->ProcessCommandLine("log 0"));
  EXPECT_TRUE(
      OutputContains("Malformed log command. <num> must be positive: 0"));

  ClearOutput();
  EXPECT_TRUE(test_app_->ProcessCommandLine("log 3.14 bar"));
  EXPECT_TRUE(OutputContains("Expected non-negative integer instead of 3.14."));

  ClearOutput();
  EXPECT_TRUE(test_app_->ProcessCommandLine("log 100"));
  EXPECT_TRUE(OutputContains(
      "Malformed log command. Expected log method to be specified "
      "after <num>."));

  ClearOutput();
  EXPECT_TRUE(test_app_->ProcessCommandLine("log 100 foo"));
  EXPECT_TRUE(OutputContains("Unrecognized log method specified: foo"));

  ClearOutput();
  EXPECT_TRUE(test_app_->ProcessCommandLine("log 100 event"));
  EXPECT_TRUE(
      OutputContains("Malformed log event command. Expected exactly one more "
                     "argument for <event_type_index>."));

  ClearOutput();
  EXPECT_TRUE(test_app_->ProcessCommandLine("log 100 event foo bar"));
  EXPECT_TRUE(
      OutputContains("Malformed log event command. Expected exactly one more "
                     "argument for <event_type_index>."));

  ClearOutput();
  EXPECT_TRUE(test_app_->ProcessCommandLine("log 100 event foo"));
  EXPECT_TRUE(OutputContains("Expected non-negative integer instead of foo."));
}

// Tests processing a bad send command line.
TEST_F(TestAppTest, ProcessCommandLineSendBad) {
  EXPECT_TRUE(test_app_->ProcessCommandLine("send foo"));
  EXPECT_TRUE(OutputContains("The send command doesn't take any arguments."));
}
}  // namespace cobalt

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  return RUN_ALL_TESTS();
}
