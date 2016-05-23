#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" This script reads data.csv and generates data.js. 
    The former is the output from the analysis pipeline.
    The later is used by visualization.html to generate a visualization.
"""

import csv
import os
import sys

# Add the third_party directory to the Python path so that we can import the gviz library.
THIS_DIR = os.path.dirname(__file__)
THIRD_PARTY_DIR = os.path.abspath(os.path.join(THIS_DIR, os.path.pardir, 'third_party'))
sys.path.insert(0, THIRD_PARTY_DIR)

import google_visualization.gviz_api as gviz_api

# The csv file to read.
CSV_FILE_NAME = 'data.csv'

def main():
  # Read the data from the csv file and put it into a dictionary.
  data = None
  with open(CSV_FILE_NAME, 'rb') as csvfile:
    reader = csv.reader(csvfile)
    data = [{"module" : row[0], "count": int(row[1])} for row in reader]

  # Then load the data into a gviz_api.DataTable
  description = {"module": ("string", "Module"),
                 "count": ("number", "Count")}
  data_table = gviz_api.DataTable(description)
  data_table.LoadData(data)

  # Create a JSON string.
  json = data_table.ToJSon(columns_order=("module", "count"),
                           order_by="module")

  # Write the JSON string.
  f = open('data.js', 'w')
  f.write("// This js file is generated by the script generate_data_js.py\n\n")
  f.write("var module_usage_data=%s;" % json)


if __name__ == '__main__':
  main()