#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
import os
import sys

THIS_DIR = os.path.dirname(__file__)
ROOT_DIR = os.path.abspath(os.path.join(THIS_DIR, os.path.pardir))
sys.path.insert(0, ROOT_DIR)

import utils.data as data
import utils.file_util as file_util

class HelpQueryRandomizer:
  """ A Randomizer that extracts the help query string from an |Entry| and uses
  Forculus threshold encryption to emit an encrypted version of it.
  """

  def randomize(self, entries):
    """ Extracts the help query string from each |Entry| in |entries| and
    uses Forculus threshold encryption to emit an encrypted version of it.

    This function does not return anything but it writes a file
    into the "r_to_s" output directory containing the randomizer output.

    Args:
      entries {list of Entry}: The entries to be randomized.
    """
    with file_util.openForRandomizerWriting(
      file_util.HELP_QUERY_RANDOMIZER_OUTPUT_FILE_NAME) as f:
      #TODO(rudominer) This is just placeholder--we are emitting the
      # plaintext entry. Replace this with an invocation of Forculus.
      writer = csv.writer(f)
      for entry in entries:
        writer.writerow([entry.help_query])
