// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_UTIL_H_
#define COMPONENTS_DOMAIN_RELIABILITY_UTIL_H_

#include <map>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "base/tracked_objects.h"
#include "components/domain_reliability/domain_reliability_export.h"

namespace domain_reliability {

class DOMAIN_RELIABILITY_EXPORT DomainReliabilityUtil {
 public:
  // Attempts to convert a net error and an HTTP response code into the status
  // string that should be recorded in a beacon. Returns true if it could.
  static bool GetBeaconStatus(
      int net_error,
      int http_response_code,
      std::string* beacon_status_out);
};

// Mockable wrapper around TimeTicks::Now and Timer. Mock version is in
// test_util.h.
class DOMAIN_RELIABILITY_EXPORT MockableTime {
 public:
  // Mockable wrapper around (a subset of) base::Timer.
  class DOMAIN_RELIABILITY_EXPORT Timer {
   public:
    virtual ~Timer();

    virtual void Start(const tracked_objects::Location& posted_from,
                       base::TimeDelta delay,
                       const base::Closure& user_task) = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() = 0;

   protected:
    Timer();
  };

  virtual ~MockableTime();

  // Returns base::TimeTicks::Now() or a mocked version thereof.
  virtual base::TimeTicks Now() = 0;
  // Returns a new Timer, or a mocked version thereof.
  virtual scoped_ptr<MockableTime::Timer> CreateTimer() = 0;

 protected:
  MockableTime();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockableTime);
};

// Implementation of MockableTime that passes through to base::TimeTicks::Now()
// and base::Timer.
class DOMAIN_RELIABILITY_EXPORT ActualTime : public MockableTime {
 public:
  ActualTime();
  virtual ~ActualTime();

  // MockableTime implementation:
  virtual base::TimeTicks Now() OVERRIDE;
  virtual scoped_ptr<MockableTime::Timer> CreateTimer() OVERRIDE;
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_UTIL_H_
