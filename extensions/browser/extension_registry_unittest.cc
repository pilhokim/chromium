// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registry.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

typedef testing::Test ExtensionRegistryTest;

testing::AssertionResult HasSingleExtension(
    const ExtensionList& list,
    const scoped_refptr<const Extension>& extension) {
  if (list.empty())
    return testing::AssertionFailure() << "No extensions in list";
  if (list.size() > 1)
    return testing::AssertionFailure() << list.size()
                                       << " extensions, expected 1";
  const Extension* did_load = list[0].get();
  if (did_load != extension)
    return testing::AssertionFailure() << "Expected " << extension->id()
                                       << " found " << did_load->id();
  return testing::AssertionSuccess();
}

class TestObserver : public ExtensionRegistryObserver {
 public:
  void Reset() {
    loaded_.clear();
    unloaded_.clear();
  }

  const ExtensionList& loaded() { return loaded_; }
  const ExtensionList& unloaded() { return unloaded_; }

 private:
  virtual void OnExtensionLoaded(const Extension* extension) OVERRIDE {
    loaded_.push_back(extension);
  }

  virtual void OnExtensionUnloaded(const Extension* extension) OVERRIDE {
    unloaded_.push_back(extension);
  }

  ExtensionList loaded_;
  ExtensionList unloaded_;
};

TEST_F(ExtensionRegistryTest, FillAndClearRegistry) {
  ExtensionRegistry registry;
  scoped_refptr<Extension> extension1 = test_util::CreateExtensionWithID("id1");
  scoped_refptr<Extension> extension2 = test_util::CreateExtensionWithID("id2");
  scoped_refptr<Extension> extension3 = test_util::CreateExtensionWithID("id3");
  scoped_refptr<Extension> extension4 = test_util::CreateExtensionWithID("id4");

  // All the sets start empty.
  EXPECT_EQ(0u, registry.enabled_extensions().size());
  EXPECT_EQ(0u, registry.disabled_extensions().size());
  EXPECT_EQ(0u, registry.terminated_extensions().size());
  EXPECT_EQ(0u, registry.blacklisted_extensions().size());

  // Extensions can be added to each set.
  registry.AddEnabled(extension1);
  registry.AddDisabled(extension2);
  registry.AddTerminated(extension3);
  registry.AddBlacklisted(extension4);

  EXPECT_EQ(1u, registry.enabled_extensions().size());
  EXPECT_EQ(1u, registry.disabled_extensions().size());
  EXPECT_EQ(1u, registry.terminated_extensions().size());
  EXPECT_EQ(1u, registry.blacklisted_extensions().size());

  // Clearing the registry clears all sets.
  registry.ClearAll();

  EXPECT_EQ(0u, registry.enabled_extensions().size());
  EXPECT_EQ(0u, registry.disabled_extensions().size());
  EXPECT_EQ(0u, registry.terminated_extensions().size());
  EXPECT_EQ(0u, registry.blacklisted_extensions().size());
}

// A simple test of adding and removing things from sets.
TEST_F(ExtensionRegistryTest, AddAndRemoveExtensionFromRegistry) {
  ExtensionRegistry registry;

  // Adding an extension works.
  scoped_refptr<Extension> extension = test_util::CreateExtensionWithID("id");
  EXPECT_TRUE(registry.AddEnabled(extension));
  EXPECT_EQ(1u, registry.enabled_extensions().size());

  // The extension was only added to one set.
  EXPECT_EQ(0u, registry.disabled_extensions().size());
  EXPECT_EQ(0u, registry.terminated_extensions().size());
  EXPECT_EQ(0u, registry.blacklisted_extensions().size());

  // Removing an extension works.
  EXPECT_TRUE(registry.RemoveEnabled(extension->id()));
  EXPECT_EQ(0u, registry.enabled_extensions().size());

  // Trying to remove an extension that isn't in the set fails cleanly.
  EXPECT_FALSE(registry.RemoveEnabled(extension->id()));
}

TEST_F(ExtensionRegistryTest, AddExtensionToRegistryTwice) {
  ExtensionRegistry registry;
  scoped_refptr<Extension> extension = test_util::CreateExtensionWithID("id");

  // An extension can exist in two sets at once. It would be nice to eliminate
  // this functionality, but some users of ExtensionRegistry need it.
  EXPECT_TRUE(registry.AddEnabled(extension));
  EXPECT_TRUE(registry.AddDisabled(extension));

  EXPECT_EQ(1u, registry.enabled_extensions().size());
  EXPECT_EQ(1u, registry.disabled_extensions().size());
  EXPECT_EQ(0u, registry.terminated_extensions().size());
  EXPECT_EQ(0u, registry.blacklisted_extensions().size());
}

TEST_F(ExtensionRegistryTest, GetExtensionById) {
  ExtensionRegistry registry;

  // Trying to get an extension fails cleanly when the sets are empty.
  EXPECT_FALSE(
      registry.GetExtensionById("id", ExtensionRegistry::EVERYTHING));

  scoped_refptr<Extension> enabled =
      test_util::CreateExtensionWithID("enabled");
  scoped_refptr<Extension> disabled =
      test_util::CreateExtensionWithID("disabled");
  scoped_refptr<Extension> terminated =
      test_util::CreateExtensionWithID("terminated");
  scoped_refptr<Extension> blacklisted =
      test_util::CreateExtensionWithID("blacklisted");

  // Add an extension to each set.
  registry.AddEnabled(enabled);
  registry.AddDisabled(disabled);
  registry.AddTerminated(terminated);
  registry.AddBlacklisted(blacklisted);

  // Enabled is part of everything and the enabled list.
  EXPECT_TRUE(
      registry.GetExtensionById("enabled", ExtensionRegistry::EVERYTHING));
  EXPECT_TRUE(
      registry.GetExtensionById("enabled", ExtensionRegistry::ENABLED));
  EXPECT_FALSE(
      registry.GetExtensionById("enabled", ExtensionRegistry::DISABLED));
  EXPECT_FALSE(
      registry.GetExtensionById("enabled", ExtensionRegistry::TERMINATED));
  EXPECT_FALSE(
      registry.GetExtensionById("enabled", ExtensionRegistry::BLACKLISTED));

  // Disabled is part of everything and the disabled list.
  EXPECT_TRUE(
      registry.GetExtensionById("disabled", ExtensionRegistry::EVERYTHING));
  EXPECT_FALSE(
      registry.GetExtensionById("disabled", ExtensionRegistry::ENABLED));
  EXPECT_TRUE(
      registry.GetExtensionById("disabled", ExtensionRegistry::DISABLED));
  EXPECT_FALSE(
      registry.GetExtensionById("disabled", ExtensionRegistry::TERMINATED));
  EXPECT_FALSE(
      registry.GetExtensionById("disabled", ExtensionRegistry::BLACKLISTED));

  // Terminated is part of everything and the terminated list.
  EXPECT_TRUE(
      registry.GetExtensionById("terminated", ExtensionRegistry::EVERYTHING));
  EXPECT_FALSE(
      registry.GetExtensionById("terminated", ExtensionRegistry::ENABLED));
  EXPECT_FALSE(
      registry.GetExtensionById("terminated", ExtensionRegistry::DISABLED));
  EXPECT_TRUE(
      registry.GetExtensionById("terminated", ExtensionRegistry::TERMINATED));
  EXPECT_FALSE(
      registry.GetExtensionById("terminated", ExtensionRegistry::BLACKLISTED));

  // Blacklisted is part of everything and the blacklisted list.
  EXPECT_TRUE(
      registry.GetExtensionById("blacklisted", ExtensionRegistry::EVERYTHING));
  EXPECT_FALSE(
      registry.GetExtensionById("blacklisted", ExtensionRegistry::ENABLED));
  EXPECT_FALSE(
      registry.GetExtensionById("blacklisted", ExtensionRegistry::DISABLED));
  EXPECT_FALSE(
      registry.GetExtensionById("blacklisted", ExtensionRegistry::TERMINATED));
  EXPECT_TRUE(
      registry.GetExtensionById("blacklisted", ExtensionRegistry::BLACKLISTED));

  // Enabled can be found with multiple flags set.
  EXPECT_TRUE(registry.GetExtensionById(
      "enabled", ExtensionRegistry::ENABLED | ExtensionRegistry::TERMINATED));

  // Enabled isn't found if the wrong flags are set.
  EXPECT_FALSE(registry.GetExtensionById(
      "enabled", ExtensionRegistry::DISABLED | ExtensionRegistry::BLACKLISTED));
}

TEST_F(ExtensionRegistryTest, Observer) {
  ExtensionRegistry registry;
  TestObserver observer;
  registry.AddObserver(&observer);

  EXPECT_TRUE(observer.loaded().empty());
  EXPECT_TRUE(observer.unloaded().empty());

  scoped_refptr<const Extension> extension =
      test_util::CreateExtensionWithID("id");

  registry.AddEnabled(extension);
  registry.TriggerOnLoaded(extension);

  EXPECT_TRUE(HasSingleExtension(observer.loaded(), extension.get()));
  EXPECT_TRUE(observer.unloaded().empty());
  observer.Reset();

  registry.RemoveEnabled(extension->id());
  registry.TriggerOnUnloaded(extension);

  EXPECT_TRUE(observer.loaded().empty());
  EXPECT_TRUE(HasSingleExtension(observer.unloaded(), extension.get()));
  observer.Reset();

  registry.RemoveObserver(&observer);
}

}  // namespace
}  // namespace extensions
