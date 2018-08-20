// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package config_validator

import (
	"config"
	"config_parser"
	"fmt"
)

func ValidateProjectConfig(c *config_parser.ProjectConfig) (err error) {
	if c.CobaltVersion == config_parser.CobaltVersion0 {
		return ValidateConfig(c.ProjectName, &c.ProjectConfig)
	} else {
		return
		// fill this in with CobaltVersion1 validation
	}
}

func ValidateConfig(projectName string, config *config.CobaltConfig) (err error) {
	if err = validateConfiguredEncodings(config); err != nil {
		return fmt.Errorf("Error in configuration for project %s: %v", projectName, err)
	}

	if err = validateConfiguredMetrics(config); err != nil {
		return fmt.Errorf("Error in configuration for project %s: %v", projectName, err)
	}

	if err = validateConfiguredReports(config); err != nil {
		return fmt.Errorf("Error in configuration for project %s: %v", projectName, err)
	}

	if err = validateSystemProfileFields(config); err != nil {
		return fmt.Errorf("Error in configuration for project %s: %v", projectName, err)
	}

	if err = runCommonValidations(config); err != nil {
		return fmt.Errorf("Error in configuration for project %s: %v", projectName, err)
	}

	return nil
}
