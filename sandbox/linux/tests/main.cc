// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/logging.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace {

// Check for leaks in our tests.
void RunPostTestsChecks() {
  if (TestUtils::CurrentProcessHasChildren()) {
    LOG(ERROR) << "One of the tests created a child that was not waited for. "
               << "Please, clean-up after your tests!";
  }
}

}  // namespace
}  // namespace sandbox

int main(int argc, char* argv[]) {
  // The use of Callbacks requires an AtExitManager.
  base::AtExitManager exit_manager;
  testing::InitGoogleTest(&argc, argv);
  // Always go through re-execution for death tests.
  // This makes gtest only marginally slower for us and has the
  // additional side effect of getting rid of gtest warnings about fork()
  // safety.
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  int tests_result = RUN_ALL_TESTS();

  sandbox::RunPostTestsChecks();
  return tests_result;
}
