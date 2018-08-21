// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_LOGGER_PROJECT_CONTEXT_H_
#define COBALT_LOGGER_PROJECT_CONTEXT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "config/metric_definition.pb.h"

namespace cobalt {
namespace logger {

// Project represents some of the metadata about a Cobalt project including
// the name and ID of the project and customer and also the
// project's declared release stage. An instance of Project does not contain
// all of the MetricDefinitions for the project. See |ProjectContext|.
class Project {
 public:
  Project(uint32_t customer_id, uint32_t project_id, std::string customer_name,
          std::string project_name, ReleaseStage release_stage);

  uint32_t customer_id() const { return customer_id_; }
  uint32_t project_id() const { return project_id_; }
  const std::string customer_name() const { return customer_name_; }
  const std::string project_name() const { return project_name_; }
  ReleaseStage release_stage() const { return release_stage_; }

  const std::string DebugString() const;

 private:
  const uint32_t customer_id_;
  const uint32_t project_id_;
  const std::string customer_name_;
  const std::string project_name_;

  // The stage in the release cycle that this project declares itself to be.
  const ReleaseStage release_stage_;
};


// ProjectContext stores the Cobalt configuration for a single Cobalt project.
class ProjectContext {
 public:
  // A reference object that gives access to the names and IDs of a Metric and
  // its owning Project and Customer. A MetricRef is obtained via the method
  // RefMetric(). The ProjectContext must remain valid as long as the MetricRef
  // is being used.
  class MetricRef {
   public:
    const Project& project() const;
    uint32_t metric_id() const;
    const std::string& metric_name() const;

   private:
    friend class ProjectContext;

    MetricRef(const ProjectContext* project,
              const MetricDefinition* metric_definition);
    const ProjectContext* project_context_;
    const MetricDefinition* metric_definition_;
  };

  ProjectContext(uint32_t customer_id, uint32_t project_id,
                 std::string customer_name, std::string project_name,
                 std::unique_ptr<MetricDefinitions> metric_definitions,
                 ReleaseStage release_stage = GA);

  const MetricDefinition* GetMetric(const std::string& metric_name) const;

  // Makes a MetricRef that wraps this ProjectContext and the given
  // metric_definition (which should have been obtained via GetMetric()).
  // This ProjectContext must remain valid as long as the returned MetricRef
  // is being used.
  const MetricRef RefMetric(const MetricDefinition* metric_definition);

  const Project& project() const { return project_; }

  const std::string DebugString() const { return project_.DebugString(); }

 private:
  Project project_;
  const std::unique_ptr<MetricDefinitions> metric_definitions_;
  std::map<const std::string, const MetricDefinition*> metrics_;
};

}  // namespace logger
}  // namespace cobalt

#endif  // COBALT_LOGGER_PROJECT_CONTEXT_H_
