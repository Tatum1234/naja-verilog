// SPDX-FileCopyrightText: 2023 The Naja verilog authors <https://github.com/najaeda/naja-verilog/blob/main/AUTHORS>
//
// SPDX-License-Identifier: Apache-2.0

#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "VerilogConstructor.h"
#include "VerilogException.h"

using namespace naja::verilog;

#include "VerilogConstructorTest.h"

namespace {

// Writes `text` to a scratch file and parses it in both passes.
void parseText(VerilogConstructorTest& constructor, const std::string& text) {
  const auto path = std::filesystem::temp_directory_path() / "naja_literals_test.v";
  {
    std::ofstream file(path);
    file << text;
  }
  constructor.parse(path);
  constructor.setFirstPass(false);
  constructor.parse(path);
}

std::string oneInstance(const std::string& connection) {
  return "module t(input [3:0] a, input n, output y);\n"
         "  c0 u0(" + connection + ");\n"
         "endmodule\n";
}

const Concatenation& asConcat(const Expression& expression) {
  return *std::get<std::shared_ptr<Concatenation>>(expression.value_);
}

}

TEST(NajaVerilogTestLiterals, replicationAndUnbasedUnsized) {
  VerilogConstructorTest constructor;
  parseText(constructor,
    "module t(input [3:0] a, output y);\n"
    "  c0 u0(.A({4{1'b1}}), .B({2{a[1:0]}}), .C({1'b0, {2{1'b1}}}));\n"
    "  c0 u1(.A('1), .B('0), .C('z), .D('x));\n"
    "endmodule\n");
  ASSERT_EQ(1, constructor.modules_.size());
  const auto& instances = constructor.modules_[0]->instances_;
  ASSERT_EQ(2, instances.size());
  const auto& u0 = dynamic_cast<VerilogConstructorTest::ModuleInstance&>(*instances[0]);
  const auto& u1 = dynamic_cast<VerilogConstructorTest::ModuleInstance&>(*instances[1]);

  ASSERT_EQ(3, u0.connections_.size());
  const auto& a = asConcat(u0.connections_[0].expression_).expressions_;
  ASSERT_EQ(4, a.size());
  for (const auto& e : a) {
    ASSERT_EQ(Expression::NUMBER, e.value_.index());
    EXPECT_EQ("1'b1", std::get<Number>(e.value_).getString());
  }

  const auto& b = asConcat(u0.connections_[1].expression_).expressions_;
  ASSERT_EQ(2, b.size());
  for (const auto& e : b) {
    ASSERT_EQ(Expression::RANGEIDENTIFIER, e.value_.index());
    const auto& ri = std::get<RangeIdentifier>(e.value_);
    EXPECT_EQ("a", ri.identifier_.name_);
    EXPECT_EQ(1, ri.range_.msb_);
    EXPECT_EQ(0, ri.range_.lsb_);
  }

  const auto& c = asConcat(u0.connections_[2].expression_).expressions_;
  ASSERT_EQ(2, c.size());
  EXPECT_EQ("1'b0", std::get<Number>(c[0].value_).getString());
  ASSERT_EQ(Expression::CONCATENATION, c[1].value_.index());
  const auto& inner = asConcat(c[1]).expressions_;
  ASSERT_EQ(2, inner.size());
  for (const auto& e : inner) {
    EXPECT_EQ("1'b1", std::get<Number>(e.value_).getString());
  }

  ASSERT_EQ(4, u1.connections_.size());
  const char expected[] = {'1', '0', 'z', 'x'};
  for (size_t i = 0; i < 4; ++i) {
    const auto& e = u1.connections_[i].expression_;
    ASSERT_EQ(Expression::NUMBER, e.value_.index());
    const auto& number = std::get<Number>(e.value_);
    EXPECT_TRUE(number.isUnbasedUnsized());
    EXPECT_EQ(expected[i], std::get<UnbasedUnsized>(number.value_).value_);
    EXPECT_EQ(std::string("'") + expected[i], number.getString());
    EXPECT_EQ(1, number.getSize());
  }
}

TEST(NajaVerilogTestLiterals, unbasedUnsizedIsLowerCased) {
  VerilogConstructorTest constructor;
  parseText(constructor, oneInstance(".A('X), .B('Z)"));
  const auto& u0 = dynamic_cast<VerilogConstructorTest::ModuleInstance&>(
      *constructor.modules_[0]->instances_[0]);
  EXPECT_EQ("'x", std::get<Number>(u0.connections_[0].expression_.value_).getString());
  EXPECT_EQ("'z", std::get<Number>(u0.connections_[1].expression_.value_).getString());
}

namespace {

// Parses a single connection and returns its expression.
Expression connectionOf(VerilogConstructorTest& constructor, const std::string& connection) {
  parseText(constructor, oneInstance(connection));
  const auto& u0 = dynamic_cast<VerilogConstructorTest::ModuleInstance&>(
      *constructor.modules_[0]->instances_[0]);
  return u0.connections_[0].expression_;
}

// Expects the parse to throw a VerilogException whose text contains `word`.
void expectParseError(const std::string& connection, const std::string& word) {
  VerilogConstructorTest constructor;
  try {
    parseText(constructor, oneInstance(connection));
    FAIL() << connection << " did not throw";
  } catch (const VerilogException& e) {
    EXPECT_NE(std::string::npos, std::string(e.what()).find(word))
        << connection << ": " << e.what();
  }
}

}

TEST(NajaVerilogTestLiterals, zeroCountInsideConcatenationIsDropped) {
  VerilogConstructorTest constructor;
  const auto expression = connectionOf(constructor, ".A({1'b1, {0{1'b0}}})");
  const auto& operands = asConcat(expression).expressions_;
  ASSERT_EQ(1, operands.size());
  EXPECT_EQ("1'b1", std::get<Number>(operands[0].value_).getString());
}

TEST(NajaVerilogTestLiterals, basedAndSignedCounts) {
  VerilogConstructorTest constructor;
  EXPECT_EQ(3, asConcat(connectionOf(constructor, ".A({4'd3{1'b1}})")).expressions_.size());
  VerilogConstructorTest hex;
  EXPECT_EQ(2, asConcat(connectionOf(hex, ".A({'h2{1'b1}})")).expressions_.size());
  VerilogConstructorTest plus;
  EXPECT_EQ(2, asConcat(connectionOf(plus, ".A({+2{1'b0}})")).expressions_.size());
  VerilogConstructorTest binary;
  EXPECT_EQ(5, asConcat(connectionOf(binary, ".A({3'b1_01{1'b0}})")).expressions_.size());
}

TEST(NajaVerilogTestLiterals, badReplicationCount) {
  // Zero on its own has no bits; the same for a concatenation of only zero-size operands.
  expectParseError(".A({0{1'b1}})", "replication");
  expectParseError(".A({{0{1'b1}}, {0{1'b0}}})", "positive size");
  // Negative, non-constant, x/z and unbased counts have no usable value.
  expectParseError(".A({-1{1'b1}})", "replication count");
  expectParseError(".A({n{1'b1}})", "replication count");
  expectParseError(".A({4'bx1{1'b1}})", "replication count");
  expectParseError(".A({4'hz{1'b1}})", "replication count");
  expectParseError(".A({'1{1'b1}})", "replication count");
  expectParseError(".A({4'sd8{1'b1}})", "replication count");
  // A radix prefix is not a digit: stoull would otherwise take 0x1 as 1.
  expectParseError(".A({8'h0x1{1'b1}})", "replication count");
  expectParseError(".A({4'b0b1{1'b1}})", "replication count");
  expectParseError(".A({4'b102{1'b1}})", "replication count");
  expectParseError(".A({4'o8{1'b1}})", "replication count");
  // Past the size cap, including a decimal that does not fit 32 bits.
  expectParseError(".A({4294967296{1'b1}})", "does not fit");
  expectParseError(".A({99999999999999999999999{1'b1}})", "does not fit");
  expectParseError(".A({4294967295{1'b1}})", "1048576");
  expectParseError(".A({1048577{1'b1}})", "1048576");
}

TEST(NajaVerilogTestLiterals, unsizedDecimalMustFit32Bits) {
  expectParseError(".A(4294967296)", "does not fit");
  expectParseError(".A(4294967296)", "33'd4294967296");
  expectParseError(".A(4294967296)", "line 2");
  expectParseError(".A(99999999999999999999999)", "does not fit");
  VerilogConstructorTest constructor;
  const Expression e = connectionOf(constructor, ".A(4294967295)");
  ASSERT_EQ(Expression::NUMBER, e.value_.index());
  const auto& number = std::get<Number>(e.value_);
  ASSERT_EQ(Number::UNSIGNED, number.value_.index());
  EXPECT_EQ(4294967295u, std::get<Number::UNSIGNED>(number.value_));
  // A sized decimal is a digit string and is unaffected.
  VerilogConstructorTest sized;
  EXPECT_NO_THROW(connectionOf(sized, ".A(33'd4294967296)"));
}

TEST(NajaVerilogTestLiterals, underscoreInUnsizedDecimal) {
  VerilogConstructorTest constructor;
  const Expression e = connectionOf(constructor, ".A(1_000)");
  ASSERT_EQ(Expression::NUMBER, e.value_.index());
  const auto& number = std::get<Number>(e.value_);
  ASSERT_EQ(Number::UNSIGNED, number.value_.index());
  EXPECT_EQ(1000u, std::get<Number::UNSIGNED>(number.value_));
  VerilogConstructorTest replicated;
  const Expression r = connectionOf(replicated, ".A({1_0{1'b1}})");
  EXPECT_EQ(10u, asConcat(r).expressions_.size());
  expectParseError(".A(4_294_967_296)", "4294967296");
}
