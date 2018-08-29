// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package config_validator

import (
	"config"
	"config_parser"
	"fmt"
	"net/mail"
	"regexp"
	"time"
)

// This file contains logic to validate lists of MetricDefinition protos.

// Date format is yyyy/mm/dd. This is done by specifying in the desired format
// Mon Jan 2 15:04:05 MST 2006. See time module documentation.
const dateFormat = "2006/01/02"

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
		return fmt.Errorf("Metric hashes to a zero metric id. This is invalid. Please change the metric name.")
	}

	if m.MetricType == config.MetricDefinition_UNSET {
		return fmt.Errorf("metric_type is not set.")
	}

	if m.MetaData == nil {
		return fmt.Errorf("meta_data is not set.")
	}

	if err := validateMetadata(*m.MetaData); err != nil {
		return fmt.Errorf("Error in meta_data: %v", err)
	}

	if m.MaxEventTypeIndex != 0 && m.MetricType != config.MetricDefinition_EVENT_OCCURRED {
		return fmt.Errorf("Metric %s has max_event_type_index set. max_event_type_index can only be set for metrics for metric type EVENT_OCCURRED.", m.MetricName)
	}

	if m.IntBuckets != nil && m.MetricType != config.MetricDefinition_INT_HISTOGRAM {
		return fmt.Errorf("Metric %s has int_buckets set. int_buckets can only be set for metrics for metric type INT_HISTOGRAM.", m.MetricName)
	}

	if len(m.Parts) > 0 && m.MetricType != config.MetricDefinition_CUSTOM {
		return fmt.Errorf("Metric %s has parts set. parts can only be set for metrics for metric type CUSTOM.", m.MetricName)
	}

	if err := validateMetricDefinitionForType(m); err != nil {
		return fmt.Errorf("Metric %s: %v", m.MetricName, err)
	}

	return validateReportDefinitions(m)
}

// Validate a single instance of Metadata.
func validateMetadata(m config.MetricDefinition_Metadata) (err error) {
	if len(m.ExpirationDate) == 0 {
		return fmt.Errorf("No expiration_date set.")
	}

	var exp time.Time
	exp, err = time.ParseInLocation(dateFormat, m.ExpirationDate, time.UTC)
	if err != nil {
		return fmt.Errorf("Invalid expiration_date '%v'. expiration_date must use the yyyy/mm/dd format.", m.ExpirationDate)
	}

	// Date one year and one day from today.
	maxExp := time.Now().AddDate(1, 0, 0)
	if exp.After(maxExp) {
		return fmt.Errorf("The expiration_date must be set no more than 1 year in the future.")
	}

	for _, o := range m.Owner {
		if _, err = mail.ParseAddress(o); err != nil {
			return fmt.Errorf("'%v' is not a valid email address in owner field", o)
		}
	}

	if m.MaxReleaseStage == config.ReleaseStage_RELEASE_STAGE_NOT_SET {
		return fmt.Errorf("No max_release_stage set.")
	}

	return nil
}

// Validate the event_types and max_event_type_index fields.
func validateEventTypes(m config.MetricDefinition) error {
	if len(m.EventTypes) == 0 {
		return fmt.Errorf("no event_types listed for metric of type %s.", m.MetricType)
	}

	if m.MaxEventTypeIndex >= 1024 {
		return fmt.Errorf("max_event_type_index must be less than 1024.")
	}

	if m.MaxEventTypeIndex == 0 {
		return nil
	}

	for i, _ := range m.EventTypes {
		if i > m.MaxEventTypeIndex {
			return fmt.Errorf("Event index %v is greater than max_event_type_index %v.", i, m.MaxEventTypeIndex)
		}
	}

	return nil
}

///////////////////////////////////////////////////////////////
// Validation for specific metric types:
///////////////////////////////////////////////////////////////

func validateMetricDefinitionForType(m config.MetricDefinition) error {
	switch m.MetricType {
	case config.MetricDefinition_EVENT_OCCURRED:
		return validateEventOccurred(m)
	case config.MetricDefinition_EVENT_COUNT:
		return validateEventTypes(m)
	case config.MetricDefinition_ELAPSED_TIME:
		return validateEventTypes(m)
	case config.MetricDefinition_FRAME_RATE:
		return validateEventTypes(m)
	case config.MetricDefinition_MEMORY_USAGE:
		return validateEventTypes(m)
	case config.MetricDefinition_INT_HISTOGRAM:
		return validateIntHistogram(m)
	case config.MetricDefinition_STRING_USED:
		return validateStringUsed(m)
	case config.MetricDefinition_CUSTOM:
		return validateCustom(m)
	}

	return fmt.Errorf("Unknown MetricType: %v", m.MetricType)
}

func validateEventOccurred(m config.MetricDefinition) error {
	if m.MaxEventTypeIndex == 0 {
		return fmt.Errorf("No max_event_type_index specified for metric of type EVENT_OCCURRED.")
	}

	return validateEventTypes(m)
}
func validateIntHistogram(m config.MetricDefinition) error {
	if m.IntBuckets == nil {
		return fmt.Errorf("No int_buckets specified for metric of type INT_HISTOGRAM.")
	}

	// TODO(azani): Validate bucket definition.

	return validateEventTypes(m)
}

func validateStringUsed(m config.MetricDefinition) error {
	if len(m.EventTypes) > 0 {
		return fmt.Errorf("event_types must not be set for metrics of type STRING_USED")
	}
	return nil
}

func validateCustom(m config.MetricDefinition) error {
	if len(m.EventTypes) > 0 {
		return fmt.Errorf("event_types must not be set for metrics of type CUSTOM")
	}

	if len(m.Parts) == 0 {
		return fmt.Errorf("No parts specified for metric of type CUSTOM.")
	}

	for n := range m.Parts {
		if !validNameRegexp.MatchString(n) {
			return fmt.Errorf("Invalid part name '%s'. Part names must match the regular expression '%v'", n, validNameRegexp)
		}

		// TODO(azani): Validate the MetricPart itself.
	}
	return nil
}
