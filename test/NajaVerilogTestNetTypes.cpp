// SPDX-FileCopyrightText: 2023 The Naja verilog authors <https://github.com/najaeda/naja-verilog/blob/main/AUTHORS>
//
// SPDX-License-Identifier: Apache-2.0

#include "gtest/gtest.h"

#include <filesystem>
#include <utility>
#include <vector>

#include "VerilogConstructor.h"

using namespace naja::verilog;

#include "VerilogConstructorTest.h"

#ifndef NAJA_VERILOG_BENCHMARKS
#define NAJA_VERILOG_BENCHMARKS "Undefined"
#endif

namespace {

class NetTypeConstructor: public VerilogConstructorTest {
  public:
    void addPortNetType(const Identifier& port, const Net::Type& type) override {
      if (not inFirstPass()) {
        portNetTypes_.emplace_back(port.name_, type);
      }
    }
    std::vector<std::pair<std::string, Net::Type>> portNetTypes_ {};
};

}

TEST(NajaVerilogTestNetTypes, everyBuiltInNetType) {
  NetTypeConstructor constructor;
  std::filesystem::path path(
      std::filesystem::path(NAJA_VERILOG_BENCHMARKS)
      / std::filesystem::path("net_types.v"));
  constructor.parse(path);
  constructor.setFirstPass(false);
  constructor.parse(path);
  ASSERT_EQ(1, constructor.modules_.size());
  const auto& nets = constructor.modules_[0]->nets_;
  const std::vector<std::pair<std::string, Net::Type::TypeEnum>> expected = {
    {"Wire", Net::Type::Wire}, {"Tri", Net::Type::Tri},
    {"Tri0", Net::Type::Tri0}, {"Tri1", Net::Type::Tri1},
    {"Supply0", Net::Type::Supply0}, {"Supply1", Net::Type::Supply1},
    {"WAnd", Net::Type::WAnd}, {"WOr", Net::Type::WOr},
    {"TriAnd", Net::Type::TriAnd}, {"TriOr", Net::Type::TriOr},
    {"TriReg", Net::Type::TriReg}, {"UWire", Net::Type::UWire}};
  ASSERT_EQ(expected.size(), nets.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i].second, nets[i].type_) << i;
    EXPECT_EQ(expected[i].first, nets[i].type_.getString()) << i;
  }
  ASSERT_EQ(1, constructor.portNetTypes_.size());
  EXPECT_EQ("pa", constructor.portNetTypes_[0].first);
  EXPECT_EQ(Net::Type::Tri1, constructor.portNetTypes_[0].second);
}

TEST(NajaVerilogTestNetTypes, portNetTypes) {
  NetTypeConstructor constructor;
  std::filesystem::path path(
      std::filesystem::path(NAJA_VERILOG_BENCHMARKS)
      / std::filesystem::path("net_types_ports.v"));
  constructor.parse(path);
  constructor.setFirstPass(false);
  constructor.parse(path);
  // The ANSI net type carries over to "q" and stops at the next declaration.
  const std::vector<std::pair<std::string, Net::Type::TypeEnum>> expected = {
    {"p", Net::Type::WAnd}, {"q", Net::Type::WAnd}, {"s", Net::Type::Tri0},
    {"a", Net::Type::Tri1}, {"b", Net::Type::Tri1}};
  ASSERT_EQ(expected.size(), constructor.portNetTypes_.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i].first, constructor.portNetTypes_[i].first);
    EXPECT_EQ(expected[i].second, constructor.portNetTypes_[i].second);
  }
}
