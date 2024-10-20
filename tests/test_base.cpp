#include "gtest/gtest.h"

#include <sstream>
#include "../bnf.h"

TEST(Rule, Literal)
{
  bnf::literal rule_foo("Foo");

  std::stringstream ss;
  ss << "Foo";

  auto token = rule_foo.match(ss);

  ASSERT_NE(token, nullptr);
  EXPECT_EQ(token->start_pos, 0);
  EXPECT_EQ(token->end_pos, 3);

  ss.seekg(ss.beg);
  ss.clear();
  
  auto second_token = rule_foo.match(ss);

  ASSERT_NE(second_token, nullptr);
  EXPECT_EQ(second_token->start_pos, 0);
  EXPECT_EQ(second_token->end_pos, 3);

  std::stringstream ss_fail;
  ss_fail << "Bar";

  auto fail_token = rule_foo.match(ss_fail);

  ASSERT_EQ(fail_token, nullptr);
}

