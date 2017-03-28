#!/usr/bin/env python
# Copyright 2017 The Fuchsia Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

""" This script is used as part of the Cobalt demo in order to generate a
    chart for the Basic RAPPOR / hour-of-the-day example. The script reads a
    csv files containing data to be visualized and uses the Google Data
    Visualization API to transform the data into  a JavaScript DataTable.
    The generated JavaScript is then imported by visualization.html and used
    to generate a chart.
"""

import csv
import os
import sys

DEMO_DIR = os.path.abspath(os.path.dirname(__file__))
ROOT_DIR = os.path.abspath(
    os.path.join(DEMO_DIR, os.path.pardir, os.path.pardir))

sys.path.insert(0, ROOT_DIR)
import third_party.google_visualization.gviz_api as gviz_api

OUT_DIR =  os.path.join(ROOT_DIR,'out')

# The output JavaScript file to be created.
OUTPUT_JS_FILE = os.path.join(OUT_DIR, 'demo_viz_data.js')

# The input csv file to be read.
USAGE_BY_HOUR_CSV_FILE = os.path.join(OUT_DIR, 'usage_by_hour.csv')

# The HTML file
VIZ_HTML_FILE = os.path.join(DEMO_DIR, 'visualization.html')

# The javascript variables to write.
USAGE_BY_HOUR_JS_VAR_NAME = 'usage_by_hour_data'

def buildDataTableJs(data=None, var_name=None, description=None,
    columns_order=None, order_by=()):
  """Builds a JavaScript string defining a DataTable containing the given data.

  Args:
    data: {dictionary}:  The data with which to populate the DataTable.
    var_name {string}: The name of the JavaScript variable to write.
    description {dictionary}: Passed to the constructor of gviz_api.DataTable()
    columns_order {tuple of string}: The names of the table columns in the
      order they should be written. Optional.
    order_by {tuple of string}: Optional. Specify something like ('foo', 'des')
      to sort the rows by the 'foo' column in descending order.

  Returns:
    {string} of the form |var_name|=<json>, where <json> is a json string
    defining a data table.
  """
  # Load data into a gviz_api.DataTable
  data_table = gviz_api.DataTable(description)
  data_table.LoadData(data)
  json = data_table.ToJSon(columns_order=columns_order,order_by=order_by)

  return "%s=%s;" % (var_name, json)

def buildUsageByHourJs():
  """Builds a string defining variables used for visualization.

  Reads the CSV file containing the usage-by-hour data and builds a JavaScript
  string defining a DataTable containing the data

  Returns: {string} of the form <var_name>=<json>, where |json| is a json string
    defining a data table abd |var_name|s is USAGE_BY_HOUR_JS_VAR_NAME.
  """
  # The CSV file is the output of the RAPPOR analyzer.
  # We read it and put the data into a dictionary.
  # We are going to visualize the data as an interval chart and so we want to
  # compute the high and low 95% confidence interval values wich we may do
  # using the "std_error" column, column 2.
  if (not os.path.exists(USAGE_BY_HOUR_CSV_FILE)):
    print "File not found: %s" % USAGE_BY_HOUR_CSV_FILE
    return None
  with open(USAGE_BY_HOUR_CSV_FILE, 'rb') as csvfile:
    reader = csv.reader(csvfile)
    data = [{"hour" : int(row[0]), "estimate": max(float(row[1]), 0),
             "low" : max(float(row[1])  - 1.96 * float(row[2]), 0),
             "high": float(row[1]) + 1.96 * float(row[2])}
        for row in reader if reader.line_num > 1]
  usage_by_hour_cobalt_js = buildDataTableJs(
      data=data,
      var_name=USAGE_BY_HOUR_JS_VAR_NAME,
      description={"hour": ("number", "Hour"),
                   "estimate": ("number", "Estimate"),
                   # The role: 'interval' property is what tells the Google
                   # Visualization API to draw an interval chart.
                   "low": ("number", "Low", {'role': 'interval'}),
                   "high": ("number", "High", {'role': 'interval'})},
      columns_order=("hour", "estimate", "low", "high"),
      order_by=("hour", "asc"))

  return usage_by_hour_cobalt_js

def generateViz():
  print "Generating visualization..."

  # Read the input files and build the JavaScript strings to write.
  usage_by_hour_js = buildUsageByHourJs()
  if usage_by_hour_js is None:
    return

  # Write the output file.
  with open(OUTPUT_JS_FILE, 'w+b') as f:
    f.write("// This js file is generated by the script "
            "generate_viz.py\n\n")

    f.write("%s\n\n" % usage_by_hour_js)

    f.write("")

  print "View the vizualization in your browser:"
  print "file://%s" % VIZ_HTML_FILE
