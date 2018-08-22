package config_validator

import (
	"config"
	"config_parser"
	"testing"
)

// Allows generating a list of MetricTypes for which we can run tests.
func metricTypesExcept(remove ...config.MetricDefinition_MetricType) (s []config.MetricDefinition_MetricType) {
	types := map[config.MetricDefinition_MetricType]bool{}
	for t := range config.MetricDefinition_MetricType_name {
		types[config.MetricDefinition_MetricType(t)] = true
	}

	for _, r := range remove {
		delete(types, r)
	}
	delete(types, config.MetricDefinition_UNSET)

	for t, _ := range types {
		s = append(s, t)
	}

	return
}

func makeValidMetric() (metric config.MetricDefinition) {
	return makeValidMetricWithName("the_metric_name")
}

// makeValidMetric returns a valid instance of config.MetricDefinition which
// can be modified to fail various validation checks for testing purposes.
func makeValidMetricWithName(name string) (metric config.MetricDefinition) {
	return config.MetricDefinition{
		Id:         config_parser.IdFromName(name),
		MetricName: name,
		MetricType: config.MetricDefinition_EVENT_OCCURRED,
		MetaData:   &config.MetricDefinition_Metadata{},
	}
}

// Test that makeValidMetric returns a valid metric.
func TestValidateMakeValidMetric(t *testing.T) {
	m := makeValidMetric()
	if err := validateMetricDefinition(m); err != nil {
		t.Errorf("Rejected valid metric: %v", err)
	}
}

// Test that repeated ids are rejected.
func TestValidateUniqueMetricId(t *testing.T) {
	m1Name := "TpzQweXFfRXpQrDWvplhfXFbJptlKmkIlHBAzjPnADtJWVAawVrbPGg"
	m2Name := "zDnlfjrXpwuQYpBrNTeCbtsRBydKuKdCvEjlGwdRJlxMjbYOSGPjhif"
	m1 := makeValidMetricWithName(m1Name)
	m2 := makeValidMetricWithName(m2Name)

	metrics := []*config.MetricDefinition{&m1, &m2}

	if err := validateConfiguredMetricDefinitions(metrics); err == nil {
		t.Error("Accepted metric definitions with identical ids.")
	}
}

func TestValidateCorrectMetricId(t *testing.T) {
	m := makeValidMetric()
	m.Id += 1

	if err := validateMetricDefinition(m); err == nil {
		t.Error("Accepted metric definition with wrong metric id.")
	}
}

// Test that invalid names are rejected.
func TestValidateMetricInvalidMetricName(t *testing.T) {
	m := makeValidMetricWithName("_invalid_name")

	if err := validateMetricDefinition(m); err == nil {
		t.Error("Accepted metric definition with invalid name.")
	}
}

// Test that metric id 0 is not accepted.
func TestValidateZeroMetricId(t *testing.T) {
	m := makeValidMetricWithName("NRaMinLNcqiYmgEypLLVGnXymNpxJzqabtbbjLycCMEohvVzZtAYpah")

	if err := validateMetricDefinition(m); err == nil {
		t.Error("Accepted metric definition with 0 id.")
	}
}

// Test that we do not accept a metric with type UNSET.
func TestValidateUnsetMetricType(t *testing.T) {
	m := makeValidMetric()
	m.MetricType = config.MetricDefinition_UNSET

	if err := validateMetricDefinition(m); err == nil {
		t.Error("Accepted metric definition with unset metric type.")
	}
}

// Test that max_event_type_index can only be set if the metric type is EVENT_OCCURRED.
func TestValidateMaxEventTypeIndexOnlySetIfEventOccurred(t *testing.T) {
	m := makeValidMetric()
	m.MaxEventTypeIndex = 10
	m.MetricType = config.MetricDefinition_EVENT_OCCURRED

	if err := validateMetricDefinition(m); err != nil {
		t.Error("Rejected valid metric definition with max_event_type_index set.")
	}

	for _, mt := range metricTypesExcept(config.MetricDefinition_EVENT_OCCURRED) {
		m.MetricType = mt
		if err := validateMetricDefinition(m); err == nil {
			t.Errorf("Accepted metric definition with type %s with max_event_type_index set.", mt)
		}
	}
}

// Test that max_event_type_index >= 1024 generates an error.
func TestValidateMaxEventTypeIndexTooBig(t *testing.T) {
	m := makeValidMetric()
	m.MaxEventTypeIndex = 1024

	if err := validateMetricDefinition(m); err == nil {
		t.Error("Accepted metric definition with too large a max_event_type_index.")
	}
}

// Test that int_buckets can only be set if the metric type is INT_HISTOGRAM.
func TestValidateIntBucketsSetOnlyForIntHistogram(t *testing.T) {
	m := makeValidMetric()
	m.IntBuckets = &config.IntegerBuckets{}
	m.MetricType = config.MetricDefinition_INT_HISTOGRAM

	if err := validateMetricDefinition(m); err != nil {
		t.Error("Rejected valid metric definition with int_buckets set.")
	}

	for _, mt := range metricTypesExcept(config.MetricDefinition_INT_HISTOGRAM) {
		m.MetricType = mt
		if err := validateMetricDefinition(m); err == nil {
			t.Errorf("Accepted metric definition with type %s with int_buckets set.", mt)
		}
	}
}

// Test that parts can only be set if the metric type is CUSTOM.
func TestValidatePartsSetOnlyForCustom(t *testing.T) {
	m := makeValidMetric()
	m.MetricType = config.MetricDefinition_CUSTOM
	m.Parts = map[string]*config.MetricPart{"hello": nil}

	if err := validateMetricDefinition(m); err != nil {
		t.Error("Rejected valid metric definition with parts set.")
	}

	for _, mt := range metricTypesExcept(config.MetricDefinition_CUSTOM) {
		m.MetricType = mt
		if err := validateMetricDefinition(m); err == nil {
			t.Errorf("Accepted metric definition with type %s with parts set.", mt)
		}
	}
}

// Test that meta_data must be set.
func TestValidatePartsNoMetadata(t *testing.T) {
	m := makeValidMetric()
	m.MetaData = nil

	if err := validateMetricDefinition(m); err == nil {
		t.Error("Accepted metric definition with no meta_data set.")
	}
}
