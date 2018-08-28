// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config/project_configs.h"

#include "./logging.h"
#include "util/crypto_util/base64.h"

namespace cobalt {
namespace config {

std::unique_ptr<ProjectConfigs> ProjectConfigs::CreateFromCobaltConfigBase64(
    const std::string& cobalt_config_base64) {
  std::string cobalt_config_bytes;
  if (!crypto::Base64Decode(cobalt_config_base64, &cobalt_config_bytes)) {
    LOG(ERROR) << "Unable to parse the provided string as base-64";
    return nullptr;
  }
  return CreateFromCobaltConfigBytes(cobalt_config_bytes);
}

std::unique_ptr<ProjectConfigs> ProjectConfigs::CreateFromCobaltConfigBytes(
    const std::string& cobalt_config_bytes) {
  auto cobalt_config = std::make_unique<CobaltConfig>();
  if (!cobalt_config->ParseFromString(cobalt_config_bytes)) {
    LOG(ERROR) << "Unable to parse a CobaltConfig from the provided bytes.";
    return nullptr;
  }
  return std::make_unique<ProjectConfigs>(std::move(cobalt_config));
}

ProjectConfigs::ProjectConfigs(std::unique_ptr<CobaltConfig> cobalt_config)
    : cobalt_config_(std::move(cobalt_config)) {
  for (const auto& customer : cobalt_config_->customers()) {
    for (const auto& project : customer.projects()) {
      projects_by_id_[std::make_pair(customer.customer_id(),
                                     project.project_id())] = &project;
      projects_by_name_[std::make_pair(customer.customer_name(),
                                       project.project_name())] = &project;
    }
  }
}

const ProjectConfig* ProjectConfigs::GetProjectConfig(
    const std::string& customer_name, const std::string& project_name) const {
  auto iter =
      projects_by_name_.find(std::make_pair(customer_name, project_name));
  if (iter == projects_by_name_.end()) {
    return nullptr;
  }
  return iter->second;
}
const ProjectConfig* ProjectConfigs::GetProjectConfig(
    uint32_t customer_id, uint32_t project_id) const {
  auto iter = projects_by_id_.find(std::make_pair(customer_id, project_id));
  if (iter == projects_by_id_.end()) {
    return nullptr;
  }
  return iter->second;
}

}  // namespace config
}  // namespace cobalt
