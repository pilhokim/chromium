// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/test_with_scope.h"

TEST(ParseTree, Accessor) {
  TestWithScope setup;

  // Make a pretend parse node with proper tracking that we can blame for the
  // given value.
  InputFile input_file(SourceFile("//foo"));
  Token base_token(Location(&input_file, 1, 1), Token::IDENTIFIER, "a");
  Token member_token(Location(&input_file, 1, 1), Token::IDENTIFIER, "b");

  AccessorNode accessor;
  accessor.set_base(base_token);

  scoped_ptr<IdentifierNode> member_identifier(
      new IdentifierNode(member_token));
  accessor.set_member(member_identifier.Pass());

  // The access should fail because a is not defined.
  Err err;
  Value result = accessor.Execute(setup.scope(), &err);
  EXPECT_TRUE(err.has_error());
  EXPECT_EQ(Value::NONE, result.type());

  // Define a as a Scope. It should still fail because b isn't defined.
  Scope a_scope(setup.scope());
  err = Err();
  setup.scope()->SetValue("a", Value(NULL, &a_scope), NULL);
  result = accessor.Execute(setup.scope(), &err);
  EXPECT_TRUE(err.has_error());
  EXPECT_EQ(Value::NONE, result.type());

  // Define b, accessor should succeed now.
  const int64 kBValue = 42;
  err = Err();
  a_scope.SetValue("b", Value(NULL, kBValue), NULL);
  result = accessor.Execute(setup.scope(), &err);
  EXPECT_FALSE(err.has_error());
  ASSERT_EQ(Value::INTEGER, result.type());
  EXPECT_EQ(kBValue, result.int_value());
}
