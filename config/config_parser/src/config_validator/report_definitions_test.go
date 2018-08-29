// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package config_validator

import (
	"config"
	"config_parser"
	"testing"
)

// makeValidReport returns a valid instance of config.ReportDefinition which
// can be modified to fail various validation checks for testing purposes.
func makeValidReport() config.ReportDefinition {
	return makeValidReportWithName("the_report_name")
}

func makeValidReportWithName(name string) config.ReportDefinition {
	return config.ReportDefinition{
		Id:         config_parser.IdFromName(name),
		ReportName: name,
		ReportType: config.ReportDefinition_EVENT_COMPONENT_OCCURRENCE_COUNT,
	}
}

// Test that makeValidReport returns a valid report.
func TestValidateMakeValidReport(t *testing.T) {
	r := makeValidReport()
	if err := validateReportDefinition(r); err != nil {
		t.Errorf("Rejected valid report: %v", err)
	}
}

// Test
func TestValidateCorrectReportId(t *testing.T) {
	r := makeValidReport()
	r.Id += 1

	if err := validateReportDefinition(r); err == nil {
		t.Error("Accepted report with wrong report id.")
	}
}

func TestValidateInvalidName(t *testing.T) {
	r := makeValidReportWithName("_invalid_name")

	if err := validateReportDefinition(r); err == nil {
		t.Error("Accepted report with invalid name.")
	}
}

func TestValidateZeroReportId(t *testing.T) {
	r := makeValidReportWithName("NRaMinLNcqiYmgEypLLVGnXymNpxJzqabtbbjLycCMEohvVzZtAYpah")

	if err := validateReportDefinition(r); err == nil {
		t.Error("Accepted report with 0 id.")
	}
}

func TestValidateUnsetReportType(t *testing.T) {
	if err := validateReportType(config.MetricDefinition_EVENT_OCCURRED, config.ReportDefinition_REPORT_TYPE_UNSET); err == nil {
		t.Error("Accepted report with no report type set.")
	}
}
