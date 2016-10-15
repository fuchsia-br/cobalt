// Copyright 2016 The Fuchsia Authors
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

#include <err.h>

#include "analyzer/analyzer.h"
#include "analyzer/store/bigtable_store.h"

int main(int argc, char *argv[]) {
  if (argc < 2)
    errx(1, "Usage: %s <table_name>", argv[0]);

  printf("Starting analyzer...\n");

  cobalt::analyzer::BigtableStore store;
  store.initialize(argv[1]);

  cobalt::analyzer::AnalyzerServiceImpl analyzer(&store);
  analyzer.Start();
  analyzer.Wait();
}