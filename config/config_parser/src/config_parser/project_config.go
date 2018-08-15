// Copyright 2017 The Fuchsia Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file contains the representation for the configuration of a cobalt
// project (See projectConfig) and a way to parse that configuration information
// from a yaml string.

package config_parser

import (
	"config"
	"fmt"
	"yamlpb"
)

type cobaltVersion int

const (
	// Cobalt version 0.1
	cobaltVersion0 = iota

	// Cobalt version 1.0
	cobaltVersion1
)

// Represents the configuration of a single project.
type projectConfig struct {
	customerName  string
	customerId    uint32
	projectName   string
	projectId     uint32
	contact       string
	cobaltVersion cobaltVersion
	projectConfig config.CobaltConfig
}

// Parse the configuration for one project from the yaml string provided into
// the config field in projectConfig.
func parseProjectConfig(y string, c *projectConfig) (err error) {
	if err := yamlpb.UnmarshalString(y, &c.projectConfig); err != nil {
		return fmt.Errorf("Error while parsing yaml: %v", err)
	}

	// Set of encoding ids. Used to detect duplicates.
	encodingIds := map[uint32]bool{}

	for i, e := range c.projectConfig.EncodingConfigs {
		if encodingIds[e.Id] {
			return fmt.Errorf("Encoding id '%v' is repeated in encoding config entry number %v. Encoding ids must be unique.", e.Id, i)
		}
		encodingIds[e.Id] = true
		e.CustomerId = c.customerId
		e.ProjectId = uint32(c.projectId)
	}

	for _, e := range c.projectConfig.MetricConfigs {
		e.CustomerId = c.customerId
		e.ProjectId = uint32(c.projectId)
	}

	for _, e := range c.projectConfig.ReportConfigs {
		e.CustomerId = c.customerId
		e.ProjectId = uint32(c.projectId)
	}

	for _, e := range c.projectConfig.MetricDefinitions {
		e.CustomerId = c.customerId
		e.ProjectId = c.projectId
		e.Id = idFromName(e.MetricName)
		for _, r := range e.Reports {
			r.Id = idFromName(r.ReportName)
		}
	}

	return nil
}
