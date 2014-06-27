// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

#if !defined(OS_ANDROID)
const char kAppUrl[] = "http://www.chromium.org";
const char kAppTitle[] = "Test title";
const char kAppDescription[] = "Test description";

const int kIconSizeSmall = extension_misc::EXTENSION_ICON_SMALL;
#endif

class BookmarkAppHelperTest : public testing::Test {
 public:
  BookmarkAppHelperTest() {}
  virtual ~BookmarkAppHelperTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkAppHelperTest);
};

class BookmarkAppHelperExtensionServiceTest : public ExtensionServiceTestBase {
 public:
  BookmarkAppHelperExtensionServiceTest() {}
  virtual ~BookmarkAppHelperExtensionServiceTest() {}

  virtual void SetUp() OVERRIDE {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    service_->Init();
    EXPECT_EQ(0u, service_->extensions()->size());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkAppHelperExtensionServiceTest);
};

SkBitmap CreateSquareBitmapWithColor(int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, size, size);
  bitmap.allocPixels();
  bitmap.eraseColor(color);
  return bitmap;
}

void ValidateBitmapSizeAndColor(SkBitmap bitmap, int size, SkColor color) {
  // Obtain pixel lock to access pixels.
  SkAutoLockPixels lock(bitmap);
  EXPECT_EQ(color, bitmap.getColor(0, 0));
  EXPECT_EQ(size, bitmap.width());
  EXPECT_EQ(size, bitmap.height());
}

}  // namespace

namespace extensions {

class TestBookmarkAppHelper : public BookmarkAppHelper {
 public:
  TestBookmarkAppHelper(ExtensionService* service,
                        WebApplicationInfo web_app_info,
                        content::WebContents* contents)
      : BookmarkAppHelper(service, web_app_info, contents) {}

  virtual ~TestBookmarkAppHelper() {}

  void CreationComplete(const extensions::Extension* extension,
                        const WebApplicationInfo& web_app_info) {
    extension_ = extension;
  }

  void CompleteIconDownload(
      bool success,
      const std::map<GURL, std::vector<SkBitmap> >& bitmaps) {
    BookmarkAppHelper::OnIconsDownloaded(success, bitmaps);
  }

  const Extension* extension() { return extension_; }

 private:
  const Extension* extension_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppHelper);
};

// Android doesn't support extensions.
#if !defined(OS_ANDROID)
TEST_F(BookmarkAppHelperExtensionServiceTest, CreateBookmarkApp) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);

  TestBookmarkAppHelper helper(service_, web_app_info, NULL);
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  std::map<GURL, std::vector<SkBitmap> > icon_map;
  icon_map[GURL(kAppUrl)].push_back(
      CreateSquareBitmapWithColor(kIconSizeSmall, SK_ColorRED));
  helper.CompleteIconDownload(true, icon_map);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper.extension());
  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(1u, service_->extensions()->size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(kAppDescription, extension->description());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_FALSE(
      IconsInfo::GetIconResource(
          extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY).empty());
}
#endif

TEST_F(BookmarkAppHelperTest, ConstrainBitmapsToSizes) {
  std::set<int> desired_sizes;
  desired_sizes.insert(16);
  desired_sizes.insert(32);
  desired_sizes.insert(128);
  desired_sizes.insert(256);

  {
    std::vector<SkBitmap> bitmaps;
    bitmaps.push_back(CreateSquareBitmapWithColor(16, SK_ColorRED));
    bitmaps.push_back(CreateSquareBitmapWithColor(32, SK_ColorGREEN));
    bitmaps.push_back(CreateSquareBitmapWithColor(48, SK_ColorBLUE));
    bitmaps.push_back(CreateSquareBitmapWithColor(144, SK_ColorYELLOW));

    std::map<int, SkBitmap> results(
        BookmarkAppHelper::ConstrainBitmapsToSizes(bitmaps, desired_sizes));

    EXPECT_EQ(3u, results.size());
    ValidateBitmapSizeAndColor(results[16], 16, SK_ColorRED);
    ValidateBitmapSizeAndColor(results[32], 32, SK_ColorGREEN);
    ValidateBitmapSizeAndColor(results[128], 128, SK_ColorYELLOW);
  }
  {
    std::vector<SkBitmap> bitmaps;
    bitmaps.push_back(CreateSquareBitmapWithColor(512, SK_ColorRED));
    bitmaps.push_back(CreateSquareBitmapWithColor(18, SK_ColorGREEN));
    bitmaps.push_back(CreateSquareBitmapWithColor(33, SK_ColorBLUE));
    bitmaps.push_back(CreateSquareBitmapWithColor(17, SK_ColorYELLOW));

    std::map<int, SkBitmap> results(
        BookmarkAppHelper::ConstrainBitmapsToSizes(bitmaps, desired_sizes));

    EXPECT_EQ(3u, results.size());
    ValidateBitmapSizeAndColor(results[16], 16, SK_ColorYELLOW);
    ValidateBitmapSizeAndColor(results[32], 32, SK_ColorBLUE);
    ValidateBitmapSizeAndColor(results[256], 256, SK_ColorRED);
  }
}

TEST_F(BookmarkAppHelperTest, GenerateIcons) {
  {
    // The 32x32 icon should be generated from the 16x16 icon.
    std::map<int, SkBitmap> bitmaps;
    bitmaps[16] = CreateSquareBitmapWithColor(16, SK_ColorRED);
    BookmarkAppHelper::GenerateContainerIcon(&bitmaps, 32);
    EXPECT_EQ(1u, bitmaps.count(32));
    EXPECT_EQ(32, bitmaps[32].width());
  }
  {
    // The 32x32 icon should not be generated because no smaller icon exists.
    std::map<int, SkBitmap> bitmaps;
    bitmaps[48] = CreateSquareBitmapWithColor(48, SK_ColorRED);
    BookmarkAppHelper::GenerateContainerIcon(&bitmaps, 32);
    EXPECT_EQ(0u, bitmaps.count(32));
  }
  {
    // The 32x32 icon should not be generated with no base icons.
    std::map<int, SkBitmap> bitmaps;
    BookmarkAppHelper::GenerateContainerIcon(&bitmaps, 32);
    EXPECT_EQ(0u, bitmaps.count(32));
  }
}

}  // namespace extensions
