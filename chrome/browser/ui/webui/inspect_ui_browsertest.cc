// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_adb_bridge.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browsertest.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

using content::WebContents;

namespace {

const char kSharedWorkerTestPage[] =
    "files/workers/workers_ui_shared_worker.html";
const char kSharedWorkerJs[] =
    "files/workers/workers_ui_shared_worker.js";

class InspectUITest : public WebUIBrowserTest {
 public:
  InspectUITest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    WebUIBrowserTest::SetUpOnMainThread();
    AddLibrary(base::FilePath(FILE_PATH_LITERAL("inspect_ui_test.js")));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InspectUITest);
};

IN_PROC_BROWSER_TEST_F(InspectUITest, InspectUIPage) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIInspectURL));
  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testTargetListed",
      new base::StringValue("#pages"),
      new base::StringValue("populateWebContentsTargets"),
      new base::StringValue(chrome::kChromeUIInspectURL)));
}

IN_PROC_BROWSER_TEST_F(InspectUITest, SharedWorker) {
  ASSERT_TRUE(test_server()->Start());
  GURL url = test_server()->GetURL(kSharedWorkerTestPage);
  ui_test_utils::NavigateToURL(browser(), url);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUIInspectURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testTargetListed",
      new base::StringValue("#workers"),
      new base::StringValue("populateWorkerTargets"),
      new base::StringValue(kSharedWorkerJs)));

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testTargetListed",
      new base::StringValue("#pages"),
      new base::StringValue("populateWebContentsTargets"),
      new base::StringValue(kSharedWorkerTestPage)));
}

IN_PROC_BROWSER_TEST_F(InspectUITest, AdbTargets) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIInspectURL));

  scoped_refptr<DevToolsAdbBridge> adb_bridge =
      DevToolsAdbBridge::Factory::GetForProfile(browser()->profile());
  adb_bridge->set_device_provider_for_test(
      AndroidDeviceProvider::GetMockDeviceProviderForTest());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest("testAdbTargetsListed"));
}

IN_PROC_BROWSER_TEST_F(InspectUITest, ReloadCrash) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIInspectURL));
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIInspectURL));
}

}  // namespace
