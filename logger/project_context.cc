// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "logger/project_context.h"

#include <sstream>

#include "./logging.h"

namespace cobalt {
namespace logger {

Project::Project(uint32_t customer_id, uint32_t project_id,
                 std::string customer_name, std::string project_name,
                 ReleaseStage release_stage)
    : customer_id_(customer_id),
      project_id_(project_id),
      customer_name_(std::move(customer_name)),
      project_name_(std::move(project_name)),
      release_stage_(release_stage) {}

ProjectContext::MetricRef::MetricRef(const ProjectContext* project,
                                     const MetricDefinition* metric_definition)
    : project_context_(project), metric_definition_(metric_definition) {}

const Project& ProjectContext::MetricRef::project() const {
  return project_context_->project();
}

uint32_t ProjectContext::MetricRef::metric_id() const {
  return metric_definition_->id();
}

const std::string& ProjectContext::MetricRef::metric_name() const {
  return metric_definition_->metric_name();
}

const ProjectContext::MetricRef ProjectContext::RefMetric(
    const MetricDefinition* metric_definition) {
  return MetricRef(this, metric_definition);
}

ProjectContext::ProjectContext(
    uint32_t customer_id, uint32_t project_id, std::string customer_name,
    std::string project_name,
    std::unique_ptr<MetricDefinitions> metric_definitions,
    ReleaseStage release_stage)
    : project_(customer_id, project_id, std::move(customer_name),
               std::move(project_name), release_stage),
      metric_definitions_(std::move(metric_definitions)) {
  for (const auto& metric : metric_definitions_->metric()) {
    if (metric.customer_id() == project_.customer_id() &&
        metric.project_id() == project_.project_id()) {
      metrics_[metric.metric_name()] = &metric;
    } else {
      LOG(ERROR) << "ProjectContext constructor found a MetricDefinition "
                    "for the wrong project. Expected customer "
                 << project_.customer_name()
                 << " (id=" << project_.customer_id() << "), project "
                 << project_.project_name() << " (id=" << project_.project_id()
                 << "). Found customer_id=" << metric.customer_id()
                 << " project_id=" << metric.project_id();
    }
  }
}

const MetricDefinition* ProjectContext::GetMetric(
    const std::string& metric_name) const {
  auto iter = metrics_.find(metric_name);
  if (iter == metrics_.end()) {
    return nullptr;
  }
  return iter->second;
}

const std::string Project::DebugString() const {
  std::ostringstream stream;
  stream << customer_name_ << "." << project_name_;
  return stream.str();
}

}  // namespace logger
}  // namespace cobalt
