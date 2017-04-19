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

"""A library with functions to help work with Docker, Kubernetes and GKE."""

import fileinput
import os
import shutil
import subprocess
import sys

import process_starter
from process_starter import ANALYZER_PRIVATE_KEY_PEM_NAME
from process_starter import DEFAULT_ANALYZER_PRIVATE_KEY_PEM
from process_starter import ANALYZER_SERVICE_PATH
from process_starter import DEFAULT_ANALYZER_SERVICE_PORT
from process_starter import DEFAULT_REPORT_MASTER_PORT
from process_starter import DEFAULT_SHUFFLER_PORT
from process_starter import REGISTERED_CONFIG_DIR
from process_starter import REPORT_MASTER_PATH
from process_starter import SHUFFLER_CONFIG_FILE
from process_starter import SHUFFLER_PATH

THIS_DIR = os.path.dirname(__file__)
SRC_ROOT_DIR = os.path.join(THIS_DIR, os.pardir)
OUT_DIR = os.path.abspath(os.path.join(SRC_ROOT_DIR, 'out'))
SYS_ROOT_DIR = os.path.join(SRC_ROOT_DIR, 'sysroot')

# The URI of the Google Container Registry.
CONTAINER_REGISTRY_URI = 'us.gcr.io'

# Dockerfile/Kubernetes source file paths
KUBE_SRC_DIR = os.path.join(SRC_ROOT_DIR, 'kubernetes')
COBALT_COMMON_DOCKER_FILE= os.path.join(KUBE_SRC_DIR, 'cobalt_common',
    'Dockerfile')
ANALYZER_SERVICE_DOCKER_FILE = os.path.join(KUBE_SRC_DIR, 'analyzer_service',
    'Dockerfile')
REPORT_MASTER_DOCKER_FILE = os.path.join(KUBE_SRC_DIR, 'report_master',
    'Dockerfile')
SHUFFLER_DOCKER_FILE = os.path.join(KUBE_SRC_DIR, 'shuffler',
    'Dockerfile')

# Kubernetes deployment yaml template files with replaceable tokens.
ANALYZER_SERVICE_DEPLOYMENT_YAML = 'analyzer_service_deployment.yaml'
ANALYZER_SERVICE_DEPLOYMENT_TEMPLATE_FILE = os.path.join(KUBE_SRC_DIR,
    'analyzer_service', ANALYZER_SERVICE_DEPLOYMENT_YAML)
REPORT_MASTER_DEPLOYMENT_YAML = 'report_master_deployment.yaml'
REPORT_MASTER_DEPLOYMENT_TEMPLATE_FILE = os.path.join(KUBE_SRC_DIR,
    'report_master', REPORT_MASTER_DEPLOYMENT_YAML)
SHUFFLER_DEPLOYMENT_YAML = 'shuffler_deployment.yaml'
SHUFFLER_DEPLOYMENT_TEMPLATE_FILE = os.path.join(KUBE_SRC_DIR, 'shuffler',
    SHUFFLER_DEPLOYMENT_YAML)

# Kubernetes output directory
KUBE_OUT_DIR = os.path.join(OUT_DIR, 'kubernetes')

# Post-processed kubernetes deployment yaml files. These have had their tokens
# replaced and are ready to be used by "kubectl create"
ANALYZER_SERVICE_DEPLOYMENT_FILE = os.path.join(KUBE_OUT_DIR,
    ANALYZER_SERVICE_DEPLOYMENT_YAML)
REPORT_MASTER_DEPLOYMENT_FILE = os.path.join(KUBE_OUT_DIR,
    REPORT_MASTER_DEPLOYMENT_YAML)
SHUFFLER_DEPLOYMENT_FILE = os.path.join(KUBE_OUT_DIR, SHUFFLER_DEPLOYMENT_YAML)

# Docker image deployment directories
COBALT_COMMON_DOCKER_BUILD_DIR = os.path.join(KUBE_OUT_DIR,
    'cobalt_common')
ANALYZER_SERVICE_DOCKER_BUILD_DIR = os.path.join(KUBE_OUT_DIR,
    'analyzer_service')
REPORT_MASTER_DOCKER_BUILD_DIR = os.path.join(KUBE_OUT_DIR,
    'report_master')
SHUFFLER_DOCKER_BUILD_DIR = os.path.join(KUBE_OUT_DIR,
    'shuffler')

# Docker Image Names
COBALT_COMMON_IMAGE_NAME = "cobalt-common"
ANALYZER_SERVICE_IMAGE_NAME = "analyzer-service"
REPORT_MASTER_IMAGE_NAME = "report-master"
SHUFFLER_IMAGE_NAME = "shuffler"

COBALT_COMMON_SO_FILES = [os.path.join(SYS_ROOT_DIR, 'lib', f) for f in
    ["libgoogleapis.so",
     "libgrpc.so.1",
     "libgrpc++.so.1",
     "libprotobuf.so.10",
     "libunwind.so.1",
    ]]

ROOTS_PEM = os.path.join(SYS_ROOT_DIR, 'share', 'grpc', 'roots.pem')

ANALYZER_CONFIG_FILES = [os.path.join(REGISTERED_CONFIG_DIR, f) for f in
    ["registered_encodings.txt",
     "registered_metrics.txt",
     "registered_reports.txt"
    ]]

ANALYZER_PRIVATE_KEY_SECRET_NAME = "analyzer-private-key"

def _ensure_dir(dir_path):
  """Ensures that the directory at |dir_path| exists. If not it is created.

  Args:
    dir_path{string} The path to a directory. If it does not exist it will be
    created.
  """
  if not os.path.exists(dir_path):
    os.makedirs(dir_path)

def _set_contents_of_dir(dir_name, files_to_copy):
  shutil.rmtree(dir_name, ignore_errors=True)
  os.makedirs(dir_name)
  for f in files_to_copy:
    shutil.copy(f, dir_name)

def _build_cobalt_common_deploy_dir():
  files_to_copy = [COBALT_COMMON_DOCKER_FILE, ROOTS_PEM] +  \
                  COBALT_COMMON_SO_FILES
  _set_contents_of_dir(COBALT_COMMON_DOCKER_BUILD_DIR, files_to_copy)

def _build_analyzer_service_deploy_dir():
  files_to_copy = [ANALYZER_SERVICE_DOCKER_FILE, ANALYZER_SERVICE_PATH]
  _set_contents_of_dir(ANALYZER_SERVICE_DOCKER_BUILD_DIR, files_to_copy)

def _build_report_master_deploy_dir():
  files_to_copy = [REPORT_MASTER_DOCKER_FILE, REPORT_MASTER_PATH] + \
                   ANALYZER_CONFIG_FILES
  _set_contents_of_dir(REPORT_MASTER_DOCKER_BUILD_DIR, files_to_copy)

def _build_shuffler_deploy_dir(config_file):
  files_to_copy = [SHUFFLER_DOCKER_FILE, SHUFFLER_PATH, config_file]
  _set_contents_of_dir(SHUFFLER_DOCKER_BUILD_DIR, files_to_copy)

def _build_docker_image(image_name, deploy_dir, extra_args=None):
  cmd = ["docker", "build"]
  if extra_args:
    cmd = cmd + extra_args
  cmd = cmd + ["-t", image_name, deploy_dir]
  subprocess.check_call(cmd)

def build_all_docker_images(shuffler_config_file=SHUFFLER_CONFIG_FILE):
  _build_cobalt_common_deploy_dir()
  _build_docker_image(COBALT_COMMON_IMAGE_NAME,
                      COBALT_COMMON_DOCKER_BUILD_DIR)

  _build_analyzer_service_deploy_dir()
  _build_docker_image(ANALYZER_SERVICE_IMAGE_NAME,
                      ANALYZER_SERVICE_DOCKER_BUILD_DIR)

  _build_report_master_deploy_dir()
  _build_docker_image(REPORT_MASTER_IMAGE_NAME,
                      REPORT_MASTER_DOCKER_BUILD_DIR)

  # Pass the full path of the config file to be copied into the deoply dir.
  _build_shuffler_deploy_dir(shuffler_config_file)

  # But pass only the basename to be found by Docker and copied into the image.
  config_file_name = os.path.basename(shuffler_config_file)
  _build_docker_image(SHUFFLER_IMAGE_NAME, SHUFFLER_DOCKER_BUILD_DIR,
      extra_args=["--build-arg", "config_file=%s"%config_file_name])

def _image_registry_uri(cloud_project_prefix, cloud_project_name, image_name):
  if not cloud_project_prefix:
    return "%s/%s/%s" % (CONTAINER_REGISTRY_URI, cloud_project_name, image_name)
  return "%s/%s/%s/%s" % (CONTAINER_REGISTRY_URI, cloud_project_prefix,
                          cloud_project_name, image_name)

def _push_to_container_registry(cloud_project_prefix, cloud_project_name,
                                image_name):
  registry_tag = _image_registry_uri(cloud_project_prefix, cloud_project_name,
                                     image_name)
  subprocess.check_call(["docker", "tag", image_name, registry_tag])
  subprocess.check_call(["gcloud", "docker", "--", "push", registry_tag])

def push_analyzer_service_to_container_registry(cloud_project_prefix,
                                               cloud_project_name):
  _push_to_container_registry(cloud_project_prefix, cloud_project_name,
                              ANALYZER_SERVICE_IMAGE_NAME)

def push_report_master_to_container_registry(cloud_project_prefix,
                                             cloud_project_name):
  _push_to_container_registry(cloud_project_prefix, cloud_project_name,
                              REPORT_MASTER_IMAGE_NAME)

def push_shuffler_to_container_registry(cloud_project_prefix,
                                        cloud_project_name):
  _push_to_container_registry(cloud_project_prefix, cloud_project_name,
                              SHUFFLER_IMAGE_NAME)

def _replace_tokens_in_template(template_file, out_file, token_replacements):
  _ensure_dir(os.path.dirname(out_file))
  with open(out_file, 'w+b') as f:
    for line in fileinput.input(template_file):
      for token in token_replacements:
        line = line.replace(token, token_replacements[token])
      f.write(line)

def _compound_project_name(cloud_project_prefix, cloud_project_name):
  if not cloud_project_prefix:
    return cloud_project_name
  return "%s:%s"%(cloud_project_prefix, cloud_project_name)


def _create_secret_from_file(secret_name, data_key, file_path):
  subprocess.check_call(["kubectl", "create", "secret", "generic", secret_name,
    "--from-file", "%s=%s"%(data_key, file_path)])

def _delete_secret(secret_name):
  subprocess.check_call(["kubectl", "delete", "secret",  secret_name])

def create_analyzer_private_key_secret(
    path_to_pem=DEFAULT_ANALYZER_PRIVATE_KEY_PEM):
  _create_secret_from_file(ANALYZER_PRIVATE_KEY_SECRET_NAME,
                           ANALYZER_PRIVATE_KEY_PEM_NAME,
                           path_to_pem)

def delete_analyzer_private_key_secret():
  _delete_secret(ANALYZER_PRIVATE_KEY_SECRET_NAME)

def _start_gke_service(deployment_template_file, deployment_file,
                       token_substitutions):
  # Generate the kubernetes deployment file by performing token replacement.
  _replace_tokens_in_template(deployment_template_file, deployment_file,
                              token_substitutions)

  # Invoke "kubectl create" on the deployment file we just generated.
  subprocess.check_call(["kubectl", "create", "-f", deployment_file])

def start_analyzer_service(cloud_project_prefix,
                           cloud_project_name,
                           bigtable_instance_name):
  """ Starts the analyzer-service deployment and service.
  cloud_project_prefix {sring}: For example "google.com"
  cloud_project_name {sring}: For example "shuffler-test". The prefix and
      name are used when forming the URI to the image in the registry and
      also the bigtable project name.
  bigtable_instance_name {string}: The name of the instance of Cloud Bigtable
      within the specified project to be used by the Analyzer Service.
  """
  image_uri = _image_registry_uri(cloud_project_prefix, cloud_project_name,
                                  ANALYZER_SERVICE_IMAGE_NAME)

  bigtable_project_name = _compound_project_name(cloud_project_prefix,
                                                 cloud_project_name)

  # These are the token replacements that must be made inside the deployment
  # template file.
  token_substitutions = {
      '$$ANALYZER_SERVICE_IMAGE_URI$$' : image_uri,
      '$$BIGTABLE_PROJECT_NAME$$' : bigtable_project_name,
      '$$BIGTABLE_INSTANCE_NAME$$' :bigtable_instance_name,
      '$$ANALYZER_PRIVATE_PEM_NAME$$' : ANALYZER_PRIVATE_KEY_PEM_NAME,
      '$$ANALYZER_PRIVATE_KEY_SECRET_NAME$$' : ANALYZER_PRIVATE_KEY_SECRET_NAME}
  _start_gke_service(ANALYZER_SERVICE_DEPLOYMENT_TEMPLATE_FILE,
                     ANALYZER_SERVICE_DEPLOYMENT_FILE,
                     token_substitutions)

def start_report_master(cloud_project_prefix,
                        cloud_project_name,
                        bigtable_instance_name):
  """ Starts the report-master deployment and service.
  cloud_project_prefix {sring}: For example "google.com"
  cloud_project_name {sring}: For example "shuffler-test". The prefix and
      name are used when forming the URI to the image in the registry and
      also the bigtable project name.
  bigtable_instance_name {string}: The name of the instance of Cloud Bigtable
      within the specified project to be used by the Report Master.
  """
  image_uri = _image_registry_uri(cloud_project_prefix, cloud_project_name,
                                  REPORT_MASTER_IMAGE_NAME)

  bigtable_project_name = _compound_project_name(cloud_project_prefix,
                                                 cloud_project_name)

  # These are the token replacements that must be made inside the deployment
  # template file.
  token_substitutions = {'$$REPORT_MASTER_IMAGE_URI$$' : image_uri,
                         '$$BIGTABLE_PROJECT_NAME$$' : bigtable_project_name,
                         '$$BIGTABLE_INSTANCE_NAME$$' :bigtable_instance_name}
  _start_gke_service(REPORT_MASTER_DEPLOYMENT_TEMPLATE_FILE,
                     REPORT_MASTER_DEPLOYMENT_FILE,
                     token_substitutions)

def start_shuffler(cloud_project_prefix,
                   cloud_project_name,
                   gce_pd_name):
  """ Starts the shuffler deployment and service.
  cloud_project_prefix {sring}: For example "google.com"
  cloud_project_name {sring}: For example "shuffler-test". The prefix and
      name are used when forming the URI to the image in the registry.
  gce_pd_name: {string} The name of a GCE persistent disk. This must have
      already been created. The shuffler will use this disk for it LevelDB
      storage so that the data persists between Shuffler updates.
  """
  image_uri = _image_registry_uri(cloud_project_prefix, cloud_project_name,
                                  SHUFFLER_IMAGE_NAME)
  # These are the token replacements that must be made inside the deployment
  # template file.
  token_substitutions = {'$$SHUFFLER_IMAGE_URI$$' : image_uri,
                         '$$GCE_PERSISTENT_DISK_NAME$$' : gce_pd_name}
  _start_gke_service(SHUFFLER_DEPLOYMENT_TEMPLATE_FILE,
                     SHUFFLER_DEPLOYMENT_FILE,
                     token_substitutions)

def _stop_gke_service(name):
  subprocess.check_call(["kubectl", "delete", "service,deployment", name])

def stop_analyzer_service():
  _stop_gke_service(ANALYZER_SERVICE_IMAGE_NAME)

def stop_report_master():
  _stop_gke_service(REPORT_MASTER_IMAGE_NAME)

def stop_shuffler():
  _stop_gke_service(SHUFFLER_IMAGE_NAME)

def authenticate(cluster_name,
                 cloud_project_prefix,
                 cloud_project_name):
  subprocess.check_call(["gcloud", "container", "clusters", "get-credentials",
      cluster_name, "--project",
      _compound_project_name(cloud_project_prefix, cloud_project_name)])

def display():
   subprocess.check_call(["kubectl", "get", "services"])

def main():
  _process_shuffler_yaml_file('cloud_project_prefix', 'cloud_project_name',
                              'gce_pd_name')

if __name__ == '__main__':
  main()

