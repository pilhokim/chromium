// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/scheduler.h"

#include "base/bind.h"
#include "base/time/time.h"
#include "components/domain_reliability/test_util.h"
#include "components/domain_reliability/util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;

namespace domain_reliability {

class DomainReliabilitySchedulerTest : public testing::Test {
 public:
  DomainReliabilitySchedulerTest() :
      num_collectors_(0),
      params_(CreateDefaultParams()),
      callback_called_(false) {
  }

  void CreateScheduler(int num_collectors) {
    DCHECK_LT(0, num_collectors);
    DCHECK(!scheduler_);

    num_collectors_ = num_collectors;
    scheduler_.reset(new DomainReliabilityScheduler(
        &time_,
        num_collectors_,
        params_,
        base::Bind(&DomainReliabilitySchedulerTest::ScheduleUploadCallback,
                   base::Unretained(this))));
  }

  static DomainReliabilityScheduler::Params CreateDefaultParams() {
    DomainReliabilityScheduler::Params params;
    params.minimum_upload_delay = base::TimeDelta::FromSeconds(60);
    params.maximum_upload_delay = base::TimeDelta::FromSeconds(300);
    params.upload_retry_interval = base::TimeDelta::FromSeconds(15);
    return params;
  }

  ::testing::AssertionResult CheckNoPendingUpload() {
    DCHECK(scheduler_);

    if (!callback_called_)
      return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
           << "expected no upload, got upload between "
           << callback_min_.InSeconds() << " and "
           << callback_max_.InSeconds() << " seconds from now";
  }

  ::testing::AssertionResult CheckPendingUpload(TimeDelta expected_min,
                                                TimeDelta expected_max) {
    DCHECK(scheduler_);
    DCHECK_LE(expected_min.InMicroseconds(), expected_max.InMicroseconds());

    if (callback_called_ && expected_min == callback_min_
                         && expected_max == callback_max_) {
      return ::testing::AssertionSuccess();
    }

    if (callback_called_) {
      return ::testing::AssertionFailure()
             << "expected upload between " << expected_min.InSeconds()
             << " and " << expected_max.InSeconds() << " seconds from now, "
             << "got upload between " << callback_min_.InSeconds()
             << " and " << callback_max_.InSeconds() << " seconds from now";
    } else {
      return ::testing::AssertionFailure()
             << "expected upload between " << expected_min.InSeconds()
             << " and " << expected_max.InSeconds() << " seconds from now, "
             << "got no upload";
    }
  }

  ::testing::AssertionResult CheckStartingUpload(int expected_collector) {
    DCHECK(scheduler_);
    DCHECK_LE(0, expected_collector);
    DCHECK_GT(num_collectors_, expected_collector);

    int collector;
    scheduler_->OnUploadStart(&collector);
    if (collector == expected_collector)
      return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
           << "expected upload to collector " << expected_collector
           << ", got upload to collector " << collector;
  }

  TimeDelta min_delay() const { return params_.minimum_upload_delay; }
  TimeDelta max_delay() const { return params_.maximum_upload_delay; }
  TimeDelta retry_interval() const { return params_.upload_retry_interval; }
  TimeDelta zero_delta() const { return base::TimeDelta::FromMicroseconds(0); }

 protected:
  void ScheduleUploadCallback(TimeDelta min, TimeDelta max) {
    callback_called_ = true;
    callback_min_ = min;
    callback_max_ = max;
  }

  MockTime time_;
  int num_collectors_;
  DomainReliabilityScheduler::Params params_;
  scoped_ptr<DomainReliabilityScheduler> scheduler_;

  bool callback_called_;
  TimeDelta callback_min_;
  TimeDelta callback_max_;
};

TEST_F(DomainReliabilitySchedulerTest, Create) {
  CreateScheduler(1);
}

TEST_F(DomainReliabilitySchedulerTest, UploadNotPendingWithoutBeacon) {
  CreateScheduler(1);

  ASSERT_TRUE(CheckNoPendingUpload());
}

TEST_F(DomainReliabilitySchedulerTest, SuccessfulUploads) {
  CreateScheduler(1);

  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(min_delay(), max_delay()));
  time_.Advance(min_delay());
  ASSERT_TRUE(CheckStartingUpload(0));
  scheduler_->OnUploadComplete(true);

  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(min_delay(), max_delay()));
  time_.Advance(min_delay());
  ASSERT_TRUE(CheckStartingUpload(0));
  scheduler_->OnUploadComplete(true);
}

TEST_F(DomainReliabilitySchedulerTest, Failover) {
  CreateScheduler(2);

  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(min_delay(), max_delay()));
  time_.Advance(min_delay());
  ASSERT_TRUE(CheckStartingUpload(0));
  scheduler_->OnUploadComplete(false);

  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(zero_delta(), max_delay() - min_delay()));
  // Don't need to advance; should retry immediately.
  ASSERT_TRUE(CheckStartingUpload(1));
  scheduler_->OnUploadComplete(true);
}

TEST_F(DomainReliabilitySchedulerTest, FailedAllCollectors) {
  CreateScheduler(2);

  // T = 0
  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(min_delay(), max_delay()));
  time_.Advance(min_delay());

  // T = min_delay
  ASSERT_TRUE(CheckStartingUpload(0));
  scheduler_->OnUploadComplete(false);

  ASSERT_TRUE(CheckPendingUpload(zero_delta(), max_delay() - min_delay()));
  // Don't need to advance; should retry immediately.
  ASSERT_TRUE(CheckStartingUpload(1));
  scheduler_->OnUploadComplete(false);

  ASSERT_TRUE(CheckPendingUpload(retry_interval(), max_delay() - min_delay()));
  time_.Advance(retry_interval());

  // T = min_delay + retry_interval
  ASSERT_TRUE(CheckStartingUpload(0));
  scheduler_->OnUploadComplete(false);

  ASSERT_TRUE(CheckPendingUpload(
      zero_delta(),
      max_delay() - min_delay() - retry_interval()));
  ASSERT_TRUE(CheckStartingUpload(1));
  scheduler_->OnUploadComplete(false);
}

// Make sure that the scheduler uses the first available collector at upload
// time, even if it wasn't available at scheduling time.
TEST_F(DomainReliabilitySchedulerTest, DetermineCollectorAtUpload) {
  CreateScheduler(2);

  // T = 0
  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(min_delay(), max_delay()));
  time_.Advance(min_delay());

  // T = min_delay
  ASSERT_TRUE(CheckStartingUpload(0));
  scheduler_->OnUploadComplete(false);

  ASSERT_TRUE(CheckPendingUpload(zero_delta(), max_delay() - min_delay()));
  time_.Advance(retry_interval());

  // T = min_delay + retry_interval; collector 0 should be active again.
  ASSERT_TRUE(CheckStartingUpload(0));
  scheduler_->OnUploadComplete(true);
}

}  // namespace domain_reliability
