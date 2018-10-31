package config_parser

import (
	"config"
	"io/ioutil"
	"path"
	"runtime"
	"strings"
	"testing"

	"github.com/google/go-cmp/cmp"
)

const v0ProjectConfigYaml = `
metric_configs:
- id: 1
  name: "Daily rare event counts"
  description: "Daily counts of several events that are expected to occur rarely if ever."
  time_zone_policy: UTC
  parts:
    "Event name":
      description: "Which rare event occurred?"
- id: 2
  name: "Module views"
  description: "Tracks each incidence of viewing a module by its URL."
  time_zone_policy: UTC
  parts:
    "url":
      description: "The URL of the module being launched."

encoding_configs:
- id: 1
  basic_rappor:
    prob_0_becomes_1: 0.0
    prob_1_stays_1: 1.0
    string_categories:
      category:
      - "Ledger-startup"
      - "Commits-received-out-of-order"
      - "Commits-merged"
      - "Merged-commits-merged"
- id: 2
  forculus:
    threshold: 2
    epoch_type: MONTH

report_configs:
- id: 1
  name: "Fuchsia Ledger Daily Rare Events"
  description: "A daily report of events that are expected to happen rarely."
  metric_id: 1
  variable:
  - metric_part: "Event name"
  scheduling:
    report_finalization_days: 3
    aggregation_epoch_type: DAY
  export_configs:
  - csv: {}
    gcs:
      bucket: "fuchsia-cobalt-reports-p2-test-app"

- id: 2
  name: "Fuchsia Module Daily Launch Counts"
  description: "A daily report of the daily counts of module launches by URL."
  metric_id: 2
  variable:
  - metric_part: "url"
  scheduling:
    report_finalization_days: 3
    aggregation_epoch_type: DAY
  export_configs:
  - csv: {}
    gcs:
      bucket: "fuchsia-cobalt-reports-p2-test-app"
`

const v1ProjectConfigYaml = `
metric_definitions:
- metric_name: the_metric_name
  time_zone_policy: UTC
  reports:
  - report_name: the_report
    report_type: CUSTOM_RAW_DUMP
  - report_name: the_other_report
    report_type: STRING_COUNTS_WITH_THRESHOLD
- metric_name: the_other_metric_name
  time_zone_policy: LOCAL
  metric_type: EVENT_OCCURRED
  event_codes:
    0: AnEvent
    1: AnotherEvent
    2: A third event
  max_event_code: 200
  reports:
  - report_name: the_report
    report_type: NUMERIC_PERF_RAW_DUMP
`

func readGoldenFile(filename string) (string, error) {
	_, thisFname, _, _ := runtime.Caller(0)
	goldenFname := path.Join(path.Dir(thisFname), "output_test_files", filename)
	contents, err := ioutil.ReadFile(goldenFname)
	if err != nil {
		return "", err
	}
	return string(contents), nil
}

func getConfigFrom(config string, cobalt_version CobaltVersion) config.CobaltConfig {
	r := memConfigReader{}
	r.SetProject("customer", "project", config)
	con := ProjectConfig{
		CustomerName:  "customer",
		CustomerId:    10,
		ProjectName:   "project",
		ProjectId:     5,
		CobaltVersion: cobalt_version,
	}

	_ = readProjectConfig(r, &con)
	return MergeConfigs([]ProjectConfig{con})
}

var cfgTests = []struct {
	yaml           string
	goldenFile     string
	cobalt_version CobaltVersion
	formatter      OutputFormatter
}{
	{v0ProjectConfigYaml, "golden_v0.cb.h", CobaltVersion0, CppOutputFactory("config", []string{"a", "b"})},
	{v1ProjectConfigYaml, "golden_v1.cb.h", CobaltVersion1, CppOutputFactory("config", []string{})},
	{v0ProjectConfigYaml, "golden_v0.cb.dart", CobaltVersion0, DartOutputFactory("config")},
	{v1ProjectConfigYaml, "golden_v1.cb.dart", CobaltVersion1, DartOutputFactory("config")},
}

func TestPrintConfig(t *testing.T) {
	for _, tt := range cfgTests {
		c := getConfigFrom(tt.yaml, tt.cobalt_version)
		configBytes, err := tt.formatter(&c)
		if err != nil {
			t.Errorf("Error generating file: %v", err)
		}
		goldenFile, err := readGoldenFile(tt.goldenFile)
		if err != nil {
			t.Errorf("Error reading golden file: %v", err)
		}
		generatedConfig := string(configBytes)
		goldenLines := strings.Split(goldenFile, "\n")
		generatedLines := strings.Split(generatedConfig, "\n")
		if diff := cmp.Diff(goldenLines, generatedLines); diff != "" {
			genFile := "/tmp/" + tt.goldenFile + ".gen"
			ioutil.WriteFile(genFile, configBytes, 0644)
			t.Errorf("Golden file %s dosen't match the generated config (%s). Diff: %s", tt.goldenFile, genFile, diff)
		}
	}
}
