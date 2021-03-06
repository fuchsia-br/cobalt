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

set_source_files_properties(${COBALT_PROTO_HDRS} PROPERTIES GENERATED TRUE)
set_source_files_properties(${CONFIG_PROTO_HDRS} PROPERTIES GENERATED TRUE)
set_source_files_properties(${REPORT_PROTO_HDRS} PROPERTIES GENERATED TRUE)

# Build the analyzer store library
add_library(analyzer_store
            bigtable_admin.cc
            bigtable_flags.cc
            bigtable_store.cc
            data_store.cc
            observation_store.cc
            report_store.cc
            ${COBALT_PROTO_HDRS}
            ${REPORT_PROTO_HDRS})
target_link_libraries(analyzer_store
                      analyzer_config
                      cobalt_crypto
                      datetime_util
                      bigtable_grpc_client
                      report_master_proto_lib)
add_cobalt_dependencies(analyzer_store)

add_library(report_store_testutils report_store_test_utils.cc)
target_link_libraries(report_store_testutils analyzer_store)

# Build the tests
add_executable(analyzer_store_tests
               memory_store.cc
               memory_store_test.cc observation_store_test.cc report_store_test.cc)
target_link_libraries(analyzer_store_tests
                      analyzer_store
                      report_store_testutils)
add_cobalt_test_dependencies(analyzer_store_tests ${DIR_GTESTS})

# Versions of our tests that expect the existence of a running local instance of
# the Bigtable Emulator process.
add_executable(bigtable_emulator_tests
               bigtable_store_emulator_test.cc
               observation_store_emulator_test.cc
               report_store_emulator_test.cc)
target_link_libraries(bigtable_emulator_tests
                      analyzer_store
                      report_store_testutils)
add_cobalt_test_dependencies(bigtable_emulator_tests ${DIR_GTESTS_BT_EMULATOR})

# Versions of our tests that connect to the real Cloud Bigtable. See
# notes in bigtabe_cloud_helper.h for how this works.
add_executable(bigtable_cloud_tests
               bigtable_store_cloud_test.cc
               observation_store_cloud_test.cc
               report_store_cloud_test.cc)
target_link_libraries(bigtable_cloud_tests
                      analyzer_store
                      report_store_testutils)
add_cobalt_test_dependencies(bigtable_cloud_tests ${DIR_GTESTS_CLOUD_BT})
