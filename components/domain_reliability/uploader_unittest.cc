// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/uploader.h"

#include <map>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "components/domain_reliability/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {

class DomainReliabilityUploaderTest : public testing::Test {
 protected:
  DomainReliabilityUploaderTest()
      : test_browser_thread_bundle_(
            content::TestBrowserThreadBundle::IO_MAINLOOP),
        url_request_context_getter_(new net::TestURLRequestContextGetter(
            base::MessageLoopProxy::current())),
        uploader_(DomainReliabilityUploader::Create(
            url_request_context_getter_)) {}

  DomainReliabilityUploader::UploadCallback MakeUploadCallback(int index) {
    return base::Bind(&DomainReliabilityUploaderTest::OnUploadComplete,
                      base::Unretained(this),
                      index);
  }

  void OnUploadComplete(int index, bool success) {
    EXPECT_FALSE(upload_complete_[index]);
    upload_complete_[index] = true;
    upload_successful_[index] = success;
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;
  scoped_ptr<DomainReliabilityUploader> uploader_;

  std::map<int, bool> upload_complete_;
  std::map<int, bool> upload_successful_;
};

TEST_F(DomainReliabilityUploaderTest, Create) {
  net::TestURLFetcher* fetcher;

  fetcher = url_fetcher_factory_.GetFetcherByID(0);
  EXPECT_FALSE(fetcher);
}

TEST_F(DomainReliabilityUploaderTest, SuccessfulUpload) {
  net::TestURLFetcher* fetcher;

  std::string report_json = "{}";
  GURL upload_url = GURL("https://test.example/upload");
  uploader_->UploadReport(report_json, upload_url, MakeUploadCallback(0));

  fetcher = url_fetcher_factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  EXPECT_EQ(report_json, fetcher->upload_data());
  EXPECT_EQ(upload_url, fetcher->GetOriginalURL());

  fetcher->set_url(upload_url);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(200);
  fetcher->SetResponseString("");

  EXPECT_FALSE(upload_complete_[0]);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_TRUE(upload_complete_[0]);
  EXPECT_TRUE(upload_successful_[0]);
}

TEST_F(DomainReliabilityUploaderTest, FailedUpload) {
  net::TestURLFetcher* fetcher;

  std::string report_json = "{}";
  GURL upload_url = GURL("https://test.example/upload");
  uploader_->UploadReport(report_json, upload_url, MakeUploadCallback(0));

  fetcher = url_fetcher_factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  EXPECT_EQ(report_json, fetcher->upload_data());
  EXPECT_EQ(upload_url, fetcher->GetOriginalURL());

  fetcher->set_url(upload_url);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(500);
  fetcher->SetResponseString("");

  EXPECT_FALSE(upload_complete_[0]);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_TRUE(upload_complete_[0]);
  EXPECT_FALSE(upload_successful_[0]);
}

}  // namespace domain_reliability
