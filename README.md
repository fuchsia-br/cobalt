# Cobalt
An extensible, privacy-preserving, user-data analysis pipeline.
[go/cobalt-for-privacy](https://goto.google.com/cobalt-for-privacy)

[TOC]

## Requirements
  1. We only develop on Ubuntu Linux. You may or may not have success on other
  systems.
  2. Python 2.7
  3. libstdc++

## One-time setup
* Fetch the code, for example via
`git clone https://fuchsia.googlesource.com/cobalt`

* Run `cobaltb.py setup`.
  * This will set up the git submodules and will download dependencies into the
  sysroot directory.

## Orchestration Script
The Python script cobaltb.py in the root directory is used to orchestrate
building, testing
and deploying to Google Container Engine. (It was already used above for
one-time setup.)

* `cobaltb.py -h` for general help
* `cobaltb.py <command> -h` for help on a command
* `cobaltb.py <command> <subcommand> -h` for help on a sub-command

## Build
  * `cobaltb.py clean`
  * `cobaltb.py build`

## Run the Tests
  * `cobaltb.py test` This runs the whole suite of tests finally running the
  the end-to-end test.
  * It is possible to run subsets of the tests by passing the `--tests=`
  argument.
  * See `cobaltb.py test -h`

### The end-to-end test
 `cobaltb.py test --tests=e2e --verbose --verbose --verbose`
This stands up a complete Cobalt system running locally:
* An instance of Bigtable Emulator
* The Shuffler
* The Analyzer Service
* The Report Master Service

It then uses the **test app** to send Observations to the Shuffler, uses the
**observation querier** to wait until the Observations have arrived at the
Analyzer, uses the **report client** library to generate a report and
wait for the report to complete, and finally checks the result of the report.

The code for the end-to-end test is written in Go and is in the
*end_to_end_tests/src* directory.

## Generating PEM Files
Cobalt uses a custom public-key encryption scheme in which the Encoder encrypts
Observations to the public key of the Analyzer before sending them to the
Shuffler. This is a key part of the design of Cobalt and we refer to it
via the slogan "The Shuffler shuffles sealed envelopes" meaning that the
Shuffler does not get to see the data that it is shuffling. In order for this
to work it is necessary for there to be public/private key PEM files that
can be read by the Encoder and the Analyzer. The end-to-end test uses the
PEM files located in the *end_to_end_tests* directory named
*analyzer_private_key.pem.e2e_test* and
*analyzer_public_key.pem.e2e_test*. But for running Cobalt in any other
environment we do not want to check in a private key into source control
and so we ask each developer to generate their own key pair.

`./cobaltb.py keygen`

Then follow the instructions to copy the generated contents into files
named *analyzer_public.pem* and *analyzer_private.pem* in your
source root directory. These will get used by several of the following
steps including running the demo manually and deploying to Google Container
Engine.

### Encryption to the Shuffler
In addition to the encryption to the Analyzer mentioned above there is a second
layer of encryption in which *Envelopes* are encrypted to the public key of the
*Shuffler*. The purpose of this layer of encryption is that TLS between the
Encoder and the Shuffler may be terminated prior to reaching the Shuffler in
some load-balanced environments. We need a second public/private key pair for
this encryption. The end-to-end test uses the PEM files located in the
*end_to_end_tests* directory named *shuffler_private_key.pem.e2e_test* and
*shuffler_public_key.pem.e2e_test*. But for running Cobalt in any other
environment follow the instructions above for generating *analyzer_public.pem*
and *analyzer_private.pem* but this time create two new files named
*shuffler_public.pem* and *shuffler_private.pem*


## Running the Demo Manually
You can run a complete Cobalt system locally (for example in order to give a
demo) as follows. Open seven different command-line console windows and run the
following commands in each one respectively:
* `./cobaltb.py start bigtable_emulator`
* ` ./cobaltb.py start analyzer_service`
* ` ./cobaltb.py start shuffler`
* `./cobaltb.py start report_master`
* ` ./cobaltb.py start test_app`
* `./cobaltb.py start observation_querier`
* `./tools/demo/demo_reporter.py`

It is a good idea to label the tabs so you can keep track of them.

Instead of the last command `./tools/demo/demo_reporter.py` you could do
`./cobaltb.py start report_client`.
The script *demo_reporter.py* invokes the *report_client* but it has been
custom tailored for a demo: Whereas *report_client* is a generic tool,
*demo_reporter.py* knows specifically which metrics, encodings and reports are
being used for the demo and it knows how to generate a visualization of the
Basic Rappor report.

Note that the `./cobaltb.py start` command automatically sets the flag `-v=3`
on all of the started processes. This sets the virtual logging level to 3.
The Cobalt log messages have been specifically tuned to give interesting
output during a demo at this virtual logging level. For example the Analyzer
service will log each time it receives a batch of Observations.

To perform the demo follow these steps.

* Use the test_app to send Forculus Observations through the Shuffler to the
Analyzer
  * `encode 19 www.AAAA`
  * `encode 20 www.BBBB`
  * `send`
  * See that the Shuffler says it received the Observations but it has not yet
  forwarded them to the Analyzer because the number of Observations have not
  yet crossed the Shuffler's threshold (100).
  * `encode 100 www.CCCC`
  * `send`
  * See that the Shuffler says it received another batch of Observations and
  that it has now forwarded all of the Observations to the Analyzer.
  * See that the Analyzer says it has received the Observations

* Use the observation_querier to inspect the state of the Observation store.
  * `query 50`
  * See the debug output that shows the Forculus Observations

* Use the demo_reporter to generate a report
  * Type `1` to run the Forculus report demo
  * See that the strings that were encoded at least the Forculus threshold (20)
  times have been decrypted and the others have not.

* Use the test_app to send Basic RAPPOR Observations through the Shuffler to the
Analyzer
  * `set metric 2`
  * `set encoding 2`
  * `encode 500 11`
  * `encode 1000 12`
  * `encode 500 13`
  * `send`
  * See that the Shuffler says it received the Observations and forwarded them
  to the Analyzer.
  * See that the Analyzer says it received the Observations.

* Use the observation_querier to inspect the state of the Observation store.
  * `set metric 2`
  * `query 50`
  * See the debug output that shows the Basic RAPPOR Observations

* Use the demo_reporter to generate a report
  * Type `2` to run the Forculus report demo
  * Open the displayed URL in your browser and see the visualization.


## Using Cloud Bigtable
You can use [Cloud Bigtable](https://cloud.google.com/bigtable/) instead of a
local instance of the Bigtable Emulator for
* Running some of the unit tests
* Doing the manual demo
* Running the end-to-end test

In this section we describe a configuration in which the Cobalt processes are
running locally but connect to Cloud Bigtable. In a later section we describe
how to run the Cobalt processes themselves on Google Container Engine.

### One-time setup for using Cloud Bigtable

#### Google Cloud Project
You will need a Google Cloud project in which to create an instance of Cloud
Bigtable and also in which to create an instance of Google Container Engine if
you wish to do that later. Create a new one or use an existing one. You will
need to enable billing. If you are a member of the core Cobalt team you can
request access to our
[shared project](https://console.cloud.google.com/home/dashboard?project=fuchsia-cobalt-testing).

#### Enable the Bigtable APIs for your project
You must enable Cloud Bigtable API and Cloud Bigtable Admin API.
* Navigate to the [API Manager Library](https://console.cloud.google.com/apis/library).
* Search for "Bigtable".
* Select Cloud Bigtable Admin API.
* Click the "Enable" button.
* Return to the API Manager Library.
* Search for "Bigtable".
* Select Cloud Bigtable API.
* Click the "Enable" button.

#### Create an instance of Cloud Bigtable
Navigate to the Bigtable section of the Cloud console for your project.
Here is the link for the core Cobalt team's
[shared project](https://console.cloud.google.com/bigtable/instances?project=fuchsia-cobalt-testing)
* Select **CREATE INSTANCE**
* Name your instance whatever you would like
* If this feature is available to you, select **Development** and
not *Production*. This is one-third the cost.
* Under *Zone* select a zone, for example *us-central1-c*. (It is best if you
use the same zone for all the different Google Cloud components you create for
a Cobalt cluster.)
* If you don't plan to use your instance for a few days you may delete it and
then recreate another one later in order to save money.

#### Optionally install cbt
[cbt](https://cloud.google.com/bigtable/docs/go/cbt-overview) is a command-line
program for interacting with Cloud Bigtable. You do not strictly need cbt in order
to follow the other steps in the document but you may choose to install it anyway.

#### Install a Service Account Credential
You must install a Service Account Credential on your computer in order for the
Cobalt code running on your computer to be able to access Cloud Bigtable.

* Go to the
[Credentials page](https://cloud.google.com/storage/docs/authentication#generating-a-private-key)
of your Cloud project. Here is [the link](https://console.cloud.google.com/apis/credentials?debugUI=DEVELOPERS&project=fuchsia-cobalt-testing) for the core
Cobalt team's shared project.
* Click `Create Credentials`
* Select `Service Account Key` as the type of key
* In the Service Account dropdown select `New Service Account` and assign your
service account any name.
* Select the Bigtable Administrator and the Bigtable User roles.
* Select `JSON` as the key type
* Click `Create`
* A JSON file containing the credentials will be generated and downloaded to
your computer. Save it anywhere.
* Rename and move the JSON file. You must name the file exactly
`personal_bt_admin_service_account.json` and you must put
the file in the Cobalt source root directory (next to this README file.)
* cobaltb.py sets the environment variable `GOOGLE_APPLICATION_CREDENTIALS` to
the path to that file. This is necessary for the gRPC C++ code linked with
Cobalt to find the credential at run-time.

*Note:* An alternative solution is to use oauth tokens in order to authenticate
your computer to Google Cloud Bigtable. However at this time there seems to be a
[bug](https://github.com/grpc/grpc/issues/7131) that is preventing this from
working. The symptom is you will see the following error message:
` assertion failed: deadline.clock_type == g_clock_type`.
If you see this error message it means that the oauth flow is being attempted
and has hit this bug. This happens if the gRPC code is
not able to use the service account credential located at
`GOOGLE_APPLICATION_CREDENTIALS`.

#### Provision the Tables
`./cobaltb.py bigtable provision`
This creates the Cobalt Bigtable tables in your Cloud Bigtable instance.

#### Delete data from the Tables
`./cobaltb.py bigtable delete_observations --customer_id=<customer_id> --project_id=<project_id> --metric_id=<metric_id>`

WARNING: This will permanently delete the Observations corresponding to the
specified metric in whichever Cloud Bigtable instance you point it at.
Be careful.

`./cobaltb.py bigtable delete_reports --customer_id=<customer_id> --project_id=<project_id> --report_config_id=<report_config_id>`

WARNING: This will permanently delete all report data corresponding to the
specified report config from the Report Store in whichever Cloud Bigtable
instance you point it at. Be careful.

### Cloud Bigtable tests
These are a set of gunit tests that run locally but use Cloud Bigtable. These
tests are not run automatically, they are not run on the continuous integration
machine and they are not run if you type `./cobaltb.py test --tests=all`.
Instead you must explicitly invoke them.

`./cobaltb.py test --tests=cloud_bt --cloud_project_name=<project_name> --bigtable_instance_name=<instance_name>`

WARNING: This will modify the contents of the tables in whichever
Cloud Bigtable instance you point it at. Be careful.

### Cloud Project Name Prefix
**NOTE:** The project name prefix described in this sub-section is now
deprecated by Google Cloud. If you make a new Google Cloud project as of
this writing the project name will not have a prefix.

If your Cloud project name has a domain prefix (for example your Cloud project
name is `google.com:myproject`) then you must specify the prefix separately
with the flag `--cloud_project_prefix.` For example you would type
`./cobaltb.py test --tests=cloud_bt --cloud_project_prefix=google.com --cloud_project_name=myproject --bigtable_instance_name=<instance_name>`

Note that if you follow the instructions below and create a
*personal_cluster.json* file then this command may be simplified to
`./cobaltb.py test --tests=cloud_bt`

### Running the End-to-End Tests Against Cloud Bigtable
This is also not done automatically but you may do it manually as follows

`./cobaltb.py test --tests=e2e -use_cloud_bt --cloud_project_name=<project_name> --bigtable_instance_name=<instance_name>`

WARNING: This will modify the contents of the tables in whichever
Cloud Bigtable instance you point it at. Be careful.

See the note above about the flag `--cloud_project_prefix`.

Note that if you follow the instructions below and create a
*personal_cluster.json* file then this command may be simplified to
`./cobaltb.py test --tests=e2e -use_cloud_bt`

### Running the Demo Manually Against Cloud Bigtable
Follow the instructions above for running the demo manually with the following
changes
* Open only six tabs instead of seven.
* Do not start the Bigtable Emulator
* When starting the Analyzer Service, the Report Master and the Observation
Querier pass the flags
`-use_cloud_bt`
`--cloud_project_name=<project_name>`
and
`--bigtable_instance_name=<instance_name>`
so that these processes will connect to your instance of Cloud Bigtable rather
than attempting to connect to a local instance of Bigtable Emulator.

See the notes above about the flag `--cloud_project_prefix` and
about creating a *personal_cluster.json* file.

## Google Container Engine (GKE)
You can deploy the Shuffler, Analyzer Service and Report Master on Google
Container Engine and then run the demo or the end-to-end test using your
cloud instance.


### One-time setup for using Google Container Engine

#### Install Docker

In order to deploy to Container Engine you need to be able to build Docker
containers and that requires having the Docker daemon running on your machine.

Install [Docker](https://docs.docker.com/engine/installation/).
If you are a Googler the following instructions should work:

* `sudo apt-get install docker-engine`
* `sudo usermod -aG docker <your username>`
* Login again to be added to the docker group.
* `groups | grep docker` to verify you have been added to the docker group.

We also will be using the tools [gcloud]( https://cloud.google.com/sdk/)
and [kubectl](https://kubernetes.io/docs/user-guide/kubectl-overview/).
These tools are included in Cobalt's *sysroot* directory and when invoked via
*cobaltb.py* the versions in sysroot will be used. But you still must install
these tools on your computer anyway so that you can perform the initialization
step and also because the full functionality of these tools is not exposed
through *cobatlb.py*.

* Install [gcloud](https://cloud.google.com/sdk/)
* `gcloud init`
* `gcloud components install kubectl`

#### Enable the Container Engine API in your Project
* Navigate to the [API Manager Library](https://console.cloud.google.com/apis/library).
* Search for "Container".
* Select "Google Container Engine API"
* Click the "Enable" button.

#### Create a GKE Cluster
Navigate to the Container Clusters section of the Cloud console for your project.
Here is the link for the core Cobalt team's
[shared project](https://console.cloud.google.com/kubernetes/list?project=fuchsia-cobalt-testing)
* Click **Create a container cluster**
* Name your cluster whatever you would like
* Put your cluster in the same zone as your Bigtable instance. If this is not
possible put it into a different zone in the same region.
* Click **More** at the bottom of the page to open more options.
* Find the **Bigtable Data** Project Access drop-down and set it to
*Read Write*. This is necessary so that code running in your cluster has
Read/Write access to your Bigtable instance.
* Select **Create**

#### Create a GCE Persistent Disk
Note that GCE stands for *Google Compute Engine* and GKE stands for
*Google Container Engine*.
Even though we are deploying Cobalt to GKE we create a persistent disk on GCE.

We create a GCE persistent disk in order to store the Shuffler's LevelDB
database. The reason for using a persistent disk is that otherwise the database
gets blown away between deployments of the Shuffler. (TODO(rudominer) Make
this optional. It may be desirable to have the option of blowing away the
database between deployments. The database will still persist between
*restarts*.)

Navigate to the Compute Engine / Disks section of the Cloud console for your
project. Here is the link for the core Cobalt team's
[shared project](https://console.cloud.google.com/compute/disks?project=fuchsia-cobalt-testing)
* Click **CREATE DISK**
* Name your disk whatever you would like
* Put your disk in the same zone as your GKE cluster. If this is not possible
put it into a different zone in the same region.
* Select "None (blank disk) for **Source Type**
* Select **Create**

#### Create a personal_cluster.json file
Optionally create a new file in your Cobalt source root named exactly
`personal_cluster.json`.
This will save you having to type many command-line flags refering to your
personal cluster.
Its contents should be exactly the following

```
{
  "cloud_project_prefix": "<your-project-prefix (now deprecated)>",
  "cloud_project_name": "<your-project-name>",
  "cluster_name": "<your-cluster-name>",
  "cluster_zone": "<your-cluster-zone>",
  "gce_pd_name": "<your-persistent-disk-name>",
  "bigtable_instance_name": "<your-bigtable-instance-name>"
  "shuffler_static_ip" : "<optional-pre-allocated-static-ip>",
  "report_master_static_ip" : "<optional-pre-allocated-static-ip>",
  "analyzer_service_static_ip" : "<optional-pre-allocated-static-ip>"",
  "shuffler_config_file" : "<optional-path-to-non-default-config-file>",
  "cobalt_config_dir" : "<optional-path-to-non-default-config-dir>",
  "shuffler_use_memstore" : "<specify true or false. Default false.>",
  "report_master_preferred_address": "<host:port>",
  "shuffler_preferred_address": "<host:port>",
  "use_tls": "<specify true or false. Default false.>"
}
```

For example:

```
{
  "cloud_project_name": "fuchsia-cobalt-testing",
  "cluster_name": "rudominer-test-1",
  "cluster_zone": "us-central1-a",
  "gce_pd_name": "rudominer-shuffler-1",
  "bigtable_instance_name": "rudominer-test-1"
  "shuffler_config_file" : "shuffler/src/shuffler_config/config_demo.txt"
  "cobalt_config_dir" : "config/demo"
  "shuffler_preferred_address": "shuffler.cobalt-api.fuchsia.com:443",
  "report_master_preferred_address": "reportmaster.cobalt-api.fuchsia.com:443",
  "use_tls": "true"
}
```

The script *cobaltb.py* looks for this file and uses it to set defaults for
flags. It is ok for some of the values to be the empty string or missing.
For example if you have not yet created a GKE cluster but you have already
created a Bigtable instance you can omit all fields other than
`cloud_project_name` and `bigtable_instance_name` and
then when performing the steps described above in the section
**Using Cloud Bigtable** you will not have to type the flags
`--cloud_project_prefix`, `--cloud_project_name` or `--bigtable_instance_name`.

Here is an explanation of each of the entries.
* *cloud_project_prefix*: (This is now deprecated. New Google Cloud projects
will not have a prefix.) This should be the empty string unless you created
your Google cloud project in such a way that it is part of a custom domain in
which case this should be the custom domain. The fully qualified name of your
project will then be `<your-project-prefix>:<your-project-name>`
* *cloud_project_name*: This should be the full name of your project if you
used the empty string for *cloud_project_prefix* otherwise it should be the
project name without the prefix and the colon.
* *cluster_name*: The name of the GKE cluster you created
* *cluster_zone*: The zone in which the GKE cluster was created
* *gce_pd_name*: The name of the GCE persistent disk you created
* *bigtable_instance_name*: The name of the Bigtable instance you created.
* *shuffler_static_ip*: If you have allocated a static IP address in your
  Google Cloud Project you can assign it to the shuffler by writing it here.
  Otherwise the shuffler will use a new dynamic IP address every time it is
  deployed. WARNING: Google Cloud Platform has a quota on static IP addresses.
  If you manually create static IP addresses and the deployment of Cobalt fails
  you may have hit this quota. It is possible to request a quota increase.
* *report_master_static_ip*: Same as above for the Report Master
* *analyzer_service_static_ip*: Same as above for the Analyzer Service.
* shuffler_config_file: If not specified uses the *demo* configuration file.
  If specified it should be a *source-root-relative* path to a Shuffler config
  file.
* cobalt_config_dir: If not specified uses the *demo* Cobalt config dir. If
  specified it should be a *source-root-relative* path to a directory containing
  the cobalt config files `registered_encodings.txt`, `registered_metrics.txt`,
  `registered_reports.txt`.
* shuffler_use_memstore: If not specified defaults to false meaning that the
  Shuffler will not use memstore but rather will use a persistent datastore.
  If specified it should be the string `true` or `false`.
* shuffler_preferred_address: The preferred connection string for targeting the
  Shuffler, of the form <host>:<port>.  This has no effect on the server
  deployment but rather acts as an instruction to clients regarding which
  connection string to use when connecting to the server.
* report_master_preferred_address: Same as above for the Report Master.
* use_tls: This has no effect on the server deployment but rather acts as
  an instruction to clients regarding whether or not to use TLS when connecting
  to the Shuffler and ReportMaster. This should be set consistently with the
  values of *shuffler_preferred_address* and *report_master_preferred_address*.

### Deploying Cobalt to GKE

`./cobaltb.py deploy login`
Run this one time to login with the copy of gcloud included in the cobalt
sysroot.

`./cobaltb.py deploy authenticate`
Run this one time in order to register your personal cluster within the
Kubernets configuration file on your computer.

`./cobaltb.py deploy upload_secret_keys`
Run this one time in order to upload the PEM files containing the Analyzer's
and Shuffler's private keys. These are the files *analyzer_private.pem* and
*shuffler_private.pem* that were created in the section
**Generating PEM Files** above. To upload different private keys,
first delete any previously upload secret keys by running
`./cobaltb.py deploy delete_secret_keys`

`./cobaltb.py deploy addresses`
Run this one time to provision static IPs for the Cobalt jobs.

`./cobaltb.py deploy endpoint --job=report-master`

`./cobaltb.py deploy endpoint --job=shuffler`
Run these one time for the report-master and shuffler each to configure the
Cloud Endpoints. If you run this multiple times, it will upload multiple copies
of the Cloud Endpoint configuration but will otherwise have no effect. (The
latest uploaded config will be used.)

`./cobaltb.py deploy upload_certificate --job=report-master --dev-generate`

`./cobaltb.py deploy upload_certificate --job=shuffler --dev-generate`
Run these one time for the report-master and shuffler each to upload self-signed
certificates. This command will generate a self-signed certificate using
`openssl` and deploy it.  DO NOT USE FOR PRODUCTION!!! (See Deploying Cobalt to
GKE in a Production Cluster for instructions on deploying certificates to
production.)
To upload a different certificate, you must first delete the existing
certificate if there is one:
`./cobaltb.py deploy delete_certificate --job=<job-name>`

`./cobaltb.py deploy build`
Run this to build Docker containers for the Shuffler, Analyzer Service and
Report Master. Run it any time the Cobalt code changes. The generated
containers are stored on your computer.

`./cobaltb.py deploy push --job=shuffler`

`./cobaltb.py deploy push --job=analyzer-service`

`./cobaltb.py deploy push --job=report-master`
Run these to push each of the containers built via the previous step
up to the cloud repository.

`./cobaltb.py deploy start --job=shuffler`

`./cobaltb.py deploy start --job=analyzer-service`

`./cobaltb.py deploy start --job=report-master`
Run these to start each of the jobs on GKE. Each of these will start multiple
Kubernetes entities on GKE: a *Service*, a *Deployment*, a *Replica Set*,
and a *Pod*.

`./cobaltb.py deploy start --job=shuffler -danger_danger_delete_all_data_at_startup`
Run this version of the start command to start the Shuffler while deleting all
Observations collected during previous runs. This is useful when running the
end-to-end tests or the demo to ensure that you know exactly what is in the
Shuffler's datastore.

`./cobaltb.py deploy stop --job=shuffler`

`./cobaltb.py deploy stop --job=analyzer-service`

`./cobaltb.py deploy stop --job=report-master`
Run these to stop each of the jobs on GKE. Each of these will stop the
Kubernetes entities that were started by the corresponding *start* command.

`./cobaltb.py deploy show`
Run this in order to see the list of running jobs and their
externally facing IP addresses and ports.

### Running the End-to-End test on GKE
`./cobaltb.py test --tests=e2e -cobalt_on_personal_cluster`

If your GKE cluster has been set up correctly and your *personal_cluster.json*
file is set up correclty, this will run the end-to-end test using your
personal Cobalt GKE cluster.

### Running Manual Demo on GKE

See the instructions above for running the manual demo. In this configuration
you do not need to start the Shuffler, Analyzer Service, Report Master or
Bigtable as these are all running in the cloud. You still need to start
the test app, the observation querier and the report client.

*  ` ./cobaltb.py start test_app -cobalt_on_personal_cluster`
*  `./cobaltb.py start observation_querier -use_cloud_bt`
*  `./tools/demo/demo_reporter.py -cobalt_on_personal_cluster`

## Managing Production Clusters
In addition to developers' personal clusters Cobalt also has production
clusters. As of this writing there is exactly one production cluster called
`fuchsia-cobalt-us-central1-c` because it is used to run Cobalt for Fuchsia
and it is located in the us-central1-c zone.

In the `production` directory there are sub-directories corresponding to
each of the production clusters. For example as of this writing there is
a directory called `production/fuchsia-cobalt-us-central1-c`.

Several of the operations described above pertaining to a developer's personal
cluster are also applicable to a Cobalt production cluster. To manage
a production cluster, pass the flag `--production_dir` to
`cobaltb.py`, specifying the production directory corresponding to the
production cluster you wish to manage.

### The cluster.json file
Similar to the `personal_cluster.json` for personal clusters described
above, there is a file called `cluster.json` located in each of the
production directories. For example, as of this writing, there is a file
called `production/fuchsia-cobalt-us-central1-c/cluster.json` with the
following contents:

```
{
  "cloud_project_name": "fuchsia-cobalt",
  "cluster_name": "cobalt",
  "cluster_zone": "us-central1-c",
  "gce_pd_name": "cobalt",
  "bigtable_instance_name": "cobalt",
  "shuffler_static_ip" : "130.211.188.46",
  "report_master_static_ip" : "104.197.76.143",
  "shuffler_config_file" : "shuffler/src/shuffler_config/config_v0.txt",
  "cobalt_config_dir" : "config/production",
  "shuffler_use_memstore" : "true",
  "shuffler_preferred_address": "shuffler.cobalt-api.fuchsia.com:443",
  "report_master_preferred_address": "reportmaster.cobalt-api.fuchsia.com:443",
  "use_tls": "true"
}
```

The `cobaltb.py` script will read this `cluster.json` file and use its contents
to set the default values for deployment-related flags if the flag
`--production_dir` is passed.

Unlike a `personal_cluster.json` file the production `cluster.json` files are
checked in to source control.

### Generating PEM Files
The production Analyzer needs a file called `analyzer_private.pem` and the
production Shuffler needs a file called `shuffler_public.pem`. The current
procedure, which will likely change in the future, is for the developer that
first creates the production cluster to run `./cobaltb.py keygen` twice
and create four files in the appropriate production directory named
`analyzer_private.pem`, `analyzer_public.pem`, `shuffler_private.pem` and
`shuffler_public.pem`. As described below, the private files are uploaded as
GKE secrets to the Analyzer and Shuffler respectively. Of course the private
files must never be checked in to source control. The public files may be
deployed with clients that will access the production cluster.

### Deploying Cobalt to GKE in a Production Cluster

`./cobaltb.py --production_dir=<production_dir> deploy authenticate`
Run this one time in order to register the production cluster within the
Kubernets configuration file on your computer.

`./cobaltb.py --production_dir=<production_dir> deploy upload_secret_keys`
The current procedure, which will likely change in the future, is for the
developer who first creates the production cluster to upload the PEM files
containing the Analyzer's and Shuffler's private keys. These are the files
*analyzer_private.pem* and *shuffler_private.pem* that were created in the
section **Generating PEM Files** above. To upload different private keys,
first delete any previously upload secret keys by running
`./cobaltb.py --production_dir=<production_dir> deploy delete_secret_keys`

```
./cobaltb.py --production_dir=<production_dir> deploy upload_certificate \
    --job=<job> \
    --path-to-cert=<path-to-certificate-file> \
    --path-to-key=<path-to-private-file>
```
Run this command once for each of the "report-master" and "shuffler" jobs.
Generating keys and obtaining certificates is beyond the scope of this document.
If you are a Googler and need to generate new certs, see
[go/cobalt-get-certs](http://go/cobalt-get-certs)
To upload a different certificate, you must first delete the existing
certificate if there is one:
`./cobaltb.py deploy delete_certificate --job=<job-name>`

`./cobaltb.py --production_dir=<production_dir> deploy build`
Run this to build Docker containers for the Shuffler, Analyzer Service and
Report Master. Run it any time the Cobalt code changes. The generated
containers are stored on your computer. Note that the Docker images for the
Shuffler and the Report Master contain config files that are copied from the
locations specified in the `cluster.json` file and so the Docker images
for production are different than the docker images for a personal cluster.

`./cobaltb.py --production_dir=<production_dir> deploy push --job=shuffler`

`./cobaltb.py --production_dir=<production_dir> deploy push --job=analyzer-service`

`./cobaltb.py --production_dir=<production_dir> deploy push --job=report-master`
Run these to push each of the containers built via the previous step
up to the cloud repository.

`./cobaltb.py --production_dir=<production_dir> deploy start --job=shuffler`

`./cobaltb.py --production_dir=<production_dir> deploy start --job=analyzer-service`

`./cobaltb.py --production_dir=<production_dir> deploy start --job=report-master`
Run these to start each of the jobs on GKE. Each of these will start multiple
Kubernetes entities on GKE: a *Service*, a *Deployment*, a *Replica Set*,
and a *Pod*.

`./cobaltb.py --production_dir=<production_dir> deploy stop --job=shuffler`

`./cobaltb.py --production_dir=<production_dir> deploy stop --job=analyzer-service`

`./cobaltb.py --production_dir=<production_dir> deploy stop --job=report-master`
Run these to stop each of the jobs on GKE. Each of these will stop the
Kubernetes entities that were started by the corresponding *start* command.

`./cobaltb.py --production_dir=<production_dir> deploy show`
Run this in order to see the list of running jobs and their
externally facing IP addresses and ports.

### Managing Production Bigtable

This section describes how to manage Cobalt's production Bigtable instances.

#### Install a Service Account Credential
In order to use Cobalt utilities to directly access a production instance
of Cloud Bigtable, you must install a Service Account Credential on your
computer. Follow the instructions above for creating the file
`personal_bt_admin_service_account.json` with the following exceptions.

* Instead of your personal project use the production project. (You must
have permission to access the production project.)
* Instead of creating a file in the root directory called
`personal_bt_admin_service_account.json`, create a file in the appropriate
production directory called `bt_admin_service_account.json`. For example for
the Cobalt instance `fuchsia-cobalt-us-central1-c` described above you would
create the file
`production/fuchsia-cobalt-us-central1-c/bt_admin_service_account.json`.

The script `cobaltb.py` sets the environment variable
`GOOGLE_APPLICATION_CREDENTIALS` equal to the path to
`bt_admin_service_account.json` when the flag `--production_dir` is passed.

**Important:** Do not check `bt_admin_service_account.json` into source control.
It is created for your own use and should live only on your development
machine. (That is why it is listed in the `.gitignore` file.)

#### Provision the Tables
To provision the Bigtable tables type
`./cobaltb.py --production_dir=<production_dir> bigtable provision`
This creates the Cobalt Bigtable tables in the **production** Cloud Bigtable
if they have not already been created.

#### Delete data from the Tables
`./cobaltb.py bigtable delete_observations  --production_dir=<production_dir> --customer_id=<customer_id> --project_id=<project_id> --metric_id=<metric_id>`

WARNING: This will permanently delete the Observations corresponding to the
specified metric in the **production** Observation Store.
Be extremely careful.

`./cobaltb.py bigtable delete_reports --production_dir=<production_dir> --customer_id=<customer_id> --project_id=<project_id> --report_config_id=<report_config_id>`

WARNING: This will permanently delete all report data corresponding to the
specified report config from the **production**  Report Store.
Be extremely careful.

