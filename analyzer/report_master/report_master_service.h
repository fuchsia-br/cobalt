// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_ANALYZER_REPORT_MASTER_REPORT_MASTER_SERVICE_H_
#define COBALT_ANALYZER_REPORT_MASTER_REPORT_MASTER_SERVICE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "analyzer/report_master/auth_enforcer.h"
#include "analyzer/report_master/report_executor.h"
#include "analyzer/report_master/report_exporter.h"
#include "analyzer/report_master/report_master.grpc.pb.h"
#include "analyzer/report_master/report_scheduler.h"
#include "analyzer/store/observation_store.h"
#include "analyzer/store/report_store.h"
#include "config/analyzer_config.h"
#include "config/analyzer_config_manager.h"
#include "grpc++/grpc++.h"
#include "third_party/googletest/googletest/include/gtest/gtest_prod.h"

namespace cobalt {
namespace analyzer {

// Forward declare ReportScheduler because report_master_service.h and
// report_scheduler.h include each other.
class ReportScheduler;

class ReportMasterService final : public ReportMaster::Service {
 public:
  static std::unique_ptr<ReportMasterService> CreateFromFlagsOrDie();

  // |report_exporter| is allowed to be NULL, in which case no exporting
  // will occur.
  ReportMasterService(
      int port, std::shared_ptr<store::ObservationStore> observation_store,
      std::shared_ptr<store::ReportStore> report_store,
      std::shared_ptr<config::AnalyzerConfigManager> config_manager,
      std::shared_ptr<grpc::ServerCredentials> server_credentials,
      std::shared_ptr<AuthEnforcer> auth_enforcer,
      std::unique_ptr<ReportExporter> report_exporter);

  // Starts the service
  void Start();

  // Stops the service
  void Shutdown();

  // Waits for the service to terminate. Shutdown() must be called for
  // Wait() to return.
  void Wait();

  grpc::Status StartReport(grpc::ServerContext* context,
                           const StartReportRequest* request,
                           StartReportResponse* response) override;

  grpc::Status GetReport(grpc::ServerContext* context,
                         const GetReportRequest* request,
                         Report* response) override;

  grpc::Status QueryReports(
      grpc::ServerContext* context, const QueryReportsRequest* request,
      grpc::ServerWriter<QueryReportsResponse>* writer) override;

  // StartReportNoAuth is inoked by StartReport after the authentication
  // check. Because it does not need to perform an authentication check it
  // does not need the |context| parameter from StartReport. This method is
  // also invoked by ReportScheduler when ReportScheduler wants to start a
  // report. In addition to the arguments to StartReport this method also takes:
  //
  // |one_off| indicates whether this report is being explicitly requested
  // (for example by gRPC) as opposed to being generated by a regular schedule.
  //
  // |export_name| specifies the location to where this report will be exported.
  // See the comments on the |export_name| field of ReportMetadataLite.
  //
  // |in_store| specifies whether or not the rows of this report will be
  // stored in the ReportStore. See the comments on the |in_store| field of
  // ReportMetadataLite.
  //
  // |report_id_out| This must be non null. The ReportId pointed to will be
  // cleared and then populated with the internal version of the report id
  // for the newly started report. Note that the external version of the
  // report id is included in |response|.
  grpc::Status StartReportNoAuth(const StartReportRequest* request,
                                 bool one_off, const std::string& export_name,
                                 bool in_store, ReportId* report_id_out,
                                 StartReportResponse* response);

  // GetReportNoAuth is identical to GetReports but authorization is not
  // checked when it is run.
  grpc::Status GetReportNoAuth(const GetReportRequest* request,
                               Report* response);

  // QueryReportNoAuth is identical to QueryReports but authorization is not
  // checked when it is run.
  grpc::Status QueryReportsNoAuth(
      const QueryReportsRequest* request,
      grpc::WriterInterface<QueryReportsResponse>* writer);

  // If there is a ReportScheduler running and invoking StartReportNoAuth()
  // on this instance of ReportMasterService, then invoke this method to
  // give ownership of that ReportScheduler to the ReportMasterService.
  void set_report_scheduler(std::unique_ptr<ReportScheduler> report_scheduler) {
    report_scheduler_ = std::move(report_scheduler);
  }

 private:
  // Makes all instantiations of ReportMasterServiceAbstractTest friends.
  template <class X>
  friend class ReportMasterServiceAbstractTest;

  // Allows this test to access private members of ReportMasterService.
  FRIEND_TEST(ReportMasterServiceFriendTest, AuthEnforcerTest);

  // Gets amd validates a ReportConfig. Returns OK or
  // does Log(ERROR) and returns an error status on error.
  grpc::Status GetAndValidateReportConfig(
      uint32_t customer_id, uint32_t project_id, uint32_t report_config_id,
      const ReportConfig** report_config_out);

  // Encapsulates the logic for starting a HISTOGRAM report. Invoked by
  // StartReportNoAuth in the case that the type of report to be started is
  // HISTOGRAM.
  //
  // |report_id| will be modified. On input customer_id, project_id and
  // report_config_id should be set. This method will set the remaining fields
  // thereby forming a new unique ReportId for the newly started HISTOGRAM
  // report.
  grpc::Status StartHistogramReport(const StartReportRequest& request,
                                    bool one_off,
                                    const std::string& export_name,
                                    bool in_store, ReportId* report_id,
                                    StartReportResponse* response);

  // Encapsulates the logic for starting a JOINT report. Invoked by
  // StartReportNoAuth in the case that the type of report to be started is
  // JOINT. Three reports will be created: The joint report itself and the two
  // one-way marginal reports. The sequence numbers of the three ReportIds will
  // be as follows:
  // 0: The one-way marginal for the first variable.
  // 1: The one-way marginal for the second variable.
  // 2: The joint report.
  // The first one-way marginal will be started and the other two will be
  // created in the WAITING_TO_START state.
  //
  // |one_off| indicates whether this report is being explicitly requested
  // as opposed to being generated by a regular schedule.
  //
  // |export_name| specifies the location to where the joint report will be
  // exported. See the comments on the |export_name| field of
  // ReportMetadataLite. The one-way marginal reports are not exported.
  //
  // |report_id| will be modified. On input customer_id, project_id and
  // report_config_id should be set. This method will set the remaining fields
  // thereby forming a new unique ReportId for the newly started HISTOGRAM
  // report. On exit the |sequence_num| field will be set to 2.
  grpc::Status StartJointReport(const StartReportRequest& request, bool one_off,
                                const std::string& export_name, bool in_store,
                                ReportId* report_id,
                                StartReportResponse* response);

  // Invokes ReportStore::StartNewReport().
  // Does Log(ERROR) and returns an error status on error.
  grpc::Status StartNewReport(const StartReportRequest& request, bool one_off,
                              const std::string& export_name, bool in_store,
                              ReportType report_type,
                              const std::vector<uint32_t>& variable_indices,
                              ReportId* report_id);

  // Invokes ReportStore::CreateDependentReport().
  // Does Log(ERROR) and returns an error status on error.
  grpc::Status CreateDependentReport(
      uint32_t sequence_number, const std::string& export_name, bool in_store,
      ReportType report_type, const std::vector<uint32_t>& variable_indices,
      ReportId* report_id);

  // Invokes ReportStore::GetReport().
  // Does Log(ERROR) and returns an error status on error.
  grpc::Status GetReport(const ReportId& report_id,
                         ReportMetadataLite* metadata_out,
                         ReportRows* report_out);

  // Starts the worker thread in the ReportExecutor.
  void StartWorkerThread();

  // Blocks until the ReportExecutor is idle. See comments for
  // ReportExecutor::WaitUntilIdle.
  void WaitUntilIdle();

  // gRPC server-side streaming is unmocakble as written so we add a thin
  // mockable wrapper around it so that we can test QueryReports without
  // using the network stack.
  grpc::Status QueryReportsInternal(
      grpc::ServerContext* context, const QueryReportsRequest* request,
      grpc::WriterInterface<QueryReportsResponse>* writer);

  // Returns the string version of a ReportId as used in the gRPC API. This
  // is exposed for use by tests.
  std::string static MakeStringReportId(const ReportId& report_id);

  int port_;
  std::shared_ptr<store::ObservationStore> observation_store_;
  std::shared_ptr<store::ReportStore> report_store_;
  std::shared_ptr<config::AnalyzerConfigManager> config_manager_;
  std::unique_ptr<ReportExecutor> report_executor_;
  std::shared_ptr<grpc::ServerCredentials> server_credentials_;
  std::unique_ptr<grpc::Server> server_;
  std::shared_ptr<AuthEnforcer> auth_enforcer_;
  // The ReportScheduler contains a pointer back to this ReportMasterService.
  // The ReportMasterService does not use the ReportScheduler, rather
  // the ReportScheduler uses the ReportMasterService. But we want all objects
  // to be owned by the ReportMasterService so that is why this
  // pointer is here. This may be null if set_report_scheduler() was
  // never invoked with a non-null ReportScheduler.
  std::unique_ptr<ReportScheduler> report_scheduler_;
};

}  // namespace analyzer
}  // namespace cobalt

#endif  // COBALT_ANALYZER_REPORT_MASTER_REPORT_MASTER_SERVICE_H_
