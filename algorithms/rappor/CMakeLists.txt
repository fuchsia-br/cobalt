# Copyright 2016 The Fuchsia Authors
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

# The rappor directory contains the implementation of the RAPPOR encoder
# (and will in the future contain the RAPPOR analysis code also.)

set_source_files_properties(${COBALT_PROTO_HDRS} PROPERTIES GENERATED TRUE)
set_source_files_properties(${CONFIG_PROTO_HDRS} PROPERTIES GENERATED TRUE)

# Use O3 level optimizations for RAPPOR
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

add_library(rappor_config_validator rappor_config_validator.cc ${CONFIG_PROTO_HDRS})
target_link_libraries(rappor_config_validator
                      client_secret)
add_cobalt_dependencies(rappor_config_validator)

add_library(rappor_encoder rappor_encoder.cc
            ${COBALT_PROTO_HDRS} ${CONFIG_PROTO_HDRS})
target_link_libraries(rappor_encoder
                      client_secret cobalt_crypto rappor_config_validator)

add_library(lasso_runner lasso_runner.cc
            ${COBALT_PROTO_HDRS} ${CONFIG_PROTO_HDRS})
target_link_libraries(lasso_runner
                      lossmin_loss_minimizers)
add_cobalt_dependencies(lasso_runner)

add_library(rappor_analyzer
            basic_rappor_analyzer.cc bloom_bit_counter.cc rappor_analyzer.cc
            rappor_analyzer_utils.cc
            ${COBALT_PROTO_HDRS} ${CONFIG_PROTO_HDRS})
target_link_libraries(rappor_analyzer
                      rappor_encoder
                      rappor_config_validator
                      cobalt_crypto
                      lasso_runner
                      lossmin_loss_minimizers)

# The test depends directly on Boring SSL for the deterministic random.
include_directories(BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/third_party/boringssl/include")
add_executable(rappor_tests
               basic_rappor_analyzer_test.cc
               bloom_bit_counter_test rappor_encoder_test.cc rappor_analyzer_test.cc
               rappor_analyzer_unit_tests.cc rappor_test_utils.cc rappor_test_utils_test.cc)
target_link_libraries(rappor_tests rappor_encoder rappor_analyzer)
add_cobalt_test_dependencies(rappor_tests ${DIR_GTESTS})

add_executable(rappor_manual_tests
               rappor_analyzer_test.cc rappor_analyzer_manual_tests.cc
               rappor_test_utils.cc rappor_test_utils_test.cc)
target_link_libraries(rappor_manual_tests rappor_encoder rappor_analyzer)
add_cobalt_test_dependencies(rappor_manual_tests ${DIR_MANUAL_TESTS})

add_executable(lasso_runner_tests
               lasso_runner_test.cc lasso_runner_unit_tests.cc)
target_link_libraries(lasso_runner_tests lasso_runner)
add_cobalt_test_dependencies(lasso_runner_tests ${DIR_GTESTS})
