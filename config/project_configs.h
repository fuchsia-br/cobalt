// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_CONFIG_PROJECT_CONFIGS_H_
#define COBALT_CONFIG_PROJECT_CONFIGS_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "config/cobalt_config.pb.h"

namespace cobalt {
namespace config {

// ProjectConfigs wraps a CobaltConfig and offers convenient and efficient
// methods for looking up a project.
class ProjectConfigs {
 public:
  // Constructs and returns an instance of ProjectConfigs by first parsing
  // a CobaltConfig proto message from |cobalt_config_base64|, which should
  // contain the Base64 encoding of the bytes of the binary serialization of
  // such a message.
  static std::unique_ptr<ProjectConfigs> CreateFromCobaltConfigBase64(
      const std::string& cobalt_config_base64);

  // Constructs and returns an instance of ProjectConfigs by first parsing
  // a CobaltConfig proto message from |cobalt_config_bytes|, which should
  // contain the bytes of the binary serialization of such a message.
  static std::unique_ptr<ProjectConfigs> CreateFromCobaltConfigBytes(
      const std::string& cobalt_config_bytes);

  // Constructs a ProjectConfigs that wraps the given |cobalt_config|.
  explicit ProjectConfigs(std::unique_ptr<CobaltConfig> cobalt_config);

  // Returns the ProjectConfig for the project with the given
  // (customer_name, project_name), or nullptr if there is no such project.
  const ProjectConfig* GetProjectConfig(const std::string& customer_name,
                                        const std::string& project_name) const;

  // Returns the ProjectConfig for the project with the given
  // (customer_id, project_id), or nullptr if there is no such project.
  const ProjectConfig* GetProjectConfig(uint32_t customer_id,
                                        uint32_t project_id) const;

 private:
  std::unique_ptr<CobaltConfig> cobalt_config_;

  std::map<std::pair<std::string, std::string>, const ProjectConfig*>
      projects_by_name_;

  std::map<std::pair<uint32_t, uint32_t>, const ProjectConfig*> projects_by_id_;
};

}  // namespace config
}  // namespace cobalt

#endif  // COBALT_CONFIG_PROJECT_CONFIGS_H_
