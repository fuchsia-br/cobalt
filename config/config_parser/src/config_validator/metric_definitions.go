// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package config_validator

import (
	"config"
	"config_parser"
	"fmt"
	"regexp"
)

// This file contains logic to validate lists of MetricDefinition protos.

// Valid names are 1-64 characters long. They have the same syntactic requirements
// as a C variable name.
var validNameRegexp = regexp.MustCompile("^[a-zA-Z][_a-zA-Z0-9]{1,65}$")

// Validate a list of MetricDefinitions.
func validateConfiguredMetricDefinitions(metrics []*config.MetricDefinition) (err error) {
	metricIds := map[uint32]int{}
	for i, metric := range metrics {
		if ci, ok := metricIds[metric.Id]; ok {
			return fmt.Errorf("Metrics named '%s' and '%s' hash to the same metric ids. One must be renamed.", metric.MetricName, metrics[ci].MetricName)
		}
		metricIds[metric.Id] = i

		if err = validateMetricDefinition(*metric); err != nil {
			return fmt.Errorf("Error validating metric '%s': %v", metric.MetricName, err)
		}
	}

	return nil
}

// Validate a single MetricDefinition.
func validateMetricDefinition(m config.MetricDefinition) (err error) {
	if !validNameRegexp.MatchString(m.MetricName) {
		return fmt.Errorf("Invalid metric name. Metric names must match the regular expression '%v'.", validNameRegexp)
	}

	if m.Id != config_parser.IdFromName(m.MetricName) {
		return fmt.Errorf("Metric id specified in config file. Metric ids may not be set by users.")
	}

	if m.Id == 0 {
		return fmt.Errorf("Metric hashes to a zero metric id. This is invalid. Please change the name.")
	}

	if m.MetricType == config.MetricDefinition_UNSET {
		return fmt.Errorf("metric_type is not set.")
	}

	if m.MetaData == nil {
		return fmt.Errorf("meta_data is not set.")
	}
	validateMetadata(*m.MetaData)

	if m.MaxEventTypeIndex != 0 && m.MetricType != config.MetricDefinition_EVENT_OCCURRED {
		return fmt.Errorf("Metric %s has max_event_type_index set. max_event_type_index can only be set for metrics for metric type EVENT_OCCURRED.", m.MetricName)
	}

	if m.MaxEventTypeIndex >= 1024 {
		return fmt.Errorf("Metric %s has max_event_type_index = %v. The maximum value for max_event_type_index is 1024.", m.MetricName, m.MaxEventTypeIndex)
	}

	if m.IntBuckets != nil && m.MetricType != config.MetricDefinition_INT_HISTOGRAM {
		return fmt.Errorf("Metric %s has int_buckets set. int_buckets can only be set for metrics for metric type INT_HISTOGRAM.", m.MetricName)
	}

	// TODO(azani): Validate the content of int_buckets.

	if len(m.Parts) > 0 && m.MetricType != config.MetricDefinition_CUSTOM {
		return fmt.Errorf("Metric %s has parts set. parts can only be set for metrics for metric type CUSTOM.", m.MetricName)
	}

	// TODO(azani): Validate the content of parts.

	// TODO(azani): Validate reports.
	return nil
}

// Validate a single instance of Metadata.
func validateMetadata(m config.MetricDefinition_Metadata) (err error) {
	// TODO(azani): Validate metadata.
	return nil
}
