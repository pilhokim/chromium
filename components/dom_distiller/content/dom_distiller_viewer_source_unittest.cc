// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/dom_distiller_viewer_source.h"

#include <vector>

#include "base/callback.h"
#include "components/dom_distiller/core/article_distillation_update.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/dom_distiller_store.h"
#include "components/dom_distiller/core/dom_distiller_test_util.h"
#include "components/dom_distiller/core/fake_db.h"
#include "components/dom_distiller/core/fake_distiller.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

const char kTestScheme[] = "myscheme";

class FakeViewRequestDelegate : public ViewRequestDelegate {
 public:
  virtual ~FakeViewRequestDelegate() {}
  MOCK_METHOD1(OnArticleReady, void(const DistilledArticleProto* proto));
  MOCK_METHOD1(OnArticleUpdated,
               void(ArticleDistillationUpdate article_update));
};

class TestDomDistillerService : public DomDistillerServiceInterface {
 public:
  TestDomDistillerService() {}
  virtual ~TestDomDistillerService() {}

  MOCK_CONST_METHOD0(GetSyncableService, syncer::SyncableService*());
  MOCK_METHOD2(AddToList,
               const std::string(const GURL&, const ArticleAvailableCallback&));
  MOCK_CONST_METHOD0(GetEntries, std::vector<ArticleEntry>());
  MOCK_METHOD1(AddObserver, void(DomDistillerObserver*));
  MOCK_METHOD1(RemoveObserver, void(DomDistillerObserver*));
  MOCK_METHOD0(ViewUrlImpl, ViewerHandle*());
  virtual scoped_ptr<ViewerHandle> ViewUrl(ViewRequestDelegate*, const GURL&) {
    return scoped_ptr<ViewerHandle>(ViewUrlImpl());
  }
  MOCK_METHOD0(ViewEntryImpl, ViewerHandle*());
  virtual scoped_ptr<ViewerHandle> ViewEntry(ViewRequestDelegate*,
                                             const std::string&) {
    return scoped_ptr<ViewerHandle>(ViewEntryImpl());
  }
  MOCK_METHOD0(RemoveEntryImpl, ArticleEntry*());
  virtual scoped_ptr<ArticleEntry> RemoveEntry(const std::string&) {
    return scoped_ptr<ArticleEntry>(RemoveEntryImpl());
  }
};

class DomDistillerViewerSourceTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    service_.reset(new TestDomDistillerService());
    source_.reset(new DomDistillerViewerSource(service_.get(), kTestScheme));
  }

 protected:
  scoped_ptr<ViewerHandle> CreateViewRequest(
      const std::string& path,
      ViewRequestDelegate* view_request_delegate) {
    return source_.get()->CreateViewRequest(path, view_request_delegate);
  }

  scoped_ptr<TestDomDistillerService> service_;
  scoped_ptr<DomDistillerViewerSource> source_;
};

TEST_F(DomDistillerViewerSourceTest, TestMimeType) {
  EXPECT_EQ("text/css", source_.get()->GetMimeType(kCssPath));
  EXPECT_EQ("text/html", source_.get()->GetMimeType("anythingelse"));
}

TEST_F(DomDistillerViewerSourceTest, TestCreatingViewUrlRequest) {
  scoped_ptr<FakeViewRequestDelegate> view_request_delegate(
      new FakeViewRequestDelegate());
  ViewerHandle* viewer_handle(new ViewerHandle(ViewerHandle::CancelCallback()));
  EXPECT_CALL(*service_.get(), ViewUrlImpl())
      .WillOnce(testing::Return(viewer_handle));
  EXPECT_CALL(*service_.get(), ViewEntryImpl()).Times(0);
  CreateViewRequest(
      std::string("?") + kUrlKey + "=http%3A%2F%2Fwww.example.com%2F",
      view_request_delegate.get());
}

TEST_F(DomDistillerViewerSourceTest, TestCreatingViewEntryRequest) {
  scoped_ptr<FakeViewRequestDelegate> view_request_delegate(
      new FakeViewRequestDelegate());
  ViewerHandle* viewer_handle(new ViewerHandle(ViewerHandle::CancelCallback()));
  EXPECT_CALL(*service_.get(), ViewEntryImpl())
      .WillOnce(testing::Return(viewer_handle));
  EXPECT_CALL(*service_.get(), ViewUrlImpl()).Times(0);
  CreateViewRequest(std::string("?") + kEntryIdKey + "=abc-def",
                    view_request_delegate.get());
}

TEST_F(DomDistillerViewerSourceTest, TestCreatingInvalidViewRequest) {
  scoped_ptr<FakeViewRequestDelegate> view_request_delegate(
      new FakeViewRequestDelegate());
  EXPECT_CALL(*service_.get(), ViewEntryImpl()).Times(0);
  EXPECT_CALL(*service_.get(), ViewUrlImpl()).Times(0);
  // Specify none of the required query parameters.
  CreateViewRequest("?foo=bar", view_request_delegate.get());
  // Specify both of the required query parameters.
  CreateViewRequest("?" + std::string(kUrlKey) +
                        "=http%3A%2F%2Fwww.example.com%2F&" +
                        std::string(kEntryIdKey) + "=abc-def",
                    view_request_delegate.get());
  // Specify an internal Chrome page.
  CreateViewRequest("?" + std::string(kUrlKey) + "=chrome%3A%2F%2Fsettings%2F",
                    view_request_delegate.get());
  // Specify a recursive URL.
  CreateViewRequest("?" + std::string(kUrlKey) + "=" +
                        std::string(kTestScheme) + "%3A%2F%2Fabc-def%2F",
                    view_request_delegate.get());
}

}  // namespace dom_distiller
