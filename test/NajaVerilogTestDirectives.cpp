// SPDX-FileCopyrightText: 2026 The Naja verilog authors
//
// SPDX-License-Identifier: Apache-2.0

#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "VerilogConstructor.h"
#include "VerilogException.h"

using namespace naja::verilog;

namespace {

// Records the unconnected-drive value in force as each module starts.
class DriveRecorder: public VerilogConstructor {
  public:
    void setUnconnectedDrive(int pull) override { current_ = pull; }
    void startModule(const Identifier& id) override {
      started_.emplace_back(id.name_, current_);
    }
    int current_ {-1};
    std::vector<std::pair<std::string, int>> started_ {};
};

std::filesystem::path writeText(const std::string& name, const std::string& text) {
  const auto path = std::filesystem::temp_directory_path() / name;
  std::ofstream file(path);
  file << text;
  return path;
}

}

TEST(NajaVerilogTestDirectives, unconnectedDriveAndCellDirectives) {
  const auto path = writeText("naja_directives_test.v",
    "`timescale 1ns/1ps\n"
    "`celldefine\n"
    "`default_nettype none\n"
    "`unconnected_drive pull1\n"
    "module a(input x); endmodule\n"
    "`nounconnected_drive\n"
    "module b(input x); endmodule\n"
    "`unconnected_drive pull0\n"
    "`resetall\n"
    "module c(input x); endmodule\n"
    "`endcelldefine\n");
  DriveRecorder constructor;
  constructor.setPreprocessEnabled(true);
  constructor.parse(path);
  ASSERT_EQ(3u, constructor.started_.size());
  EXPECT_EQ(std::make_pair(std::string("a"), 1), constructor.started_[0]);
  EXPECT_EQ(std::make_pair(std::string("b"), -1), constructor.started_[1]);
  EXPECT_EQ(std::make_pair(std::string("c"), -1), constructor.started_[2]);
}

TEST(NajaVerilogTestDirectives, pull0IsReported) {
  const auto path = writeText("naja_directives_pull0.v",
    "`unconnected_drive pull0\nmodule a(input x); endmodule\n");
  DriveRecorder constructor;
  constructor.setPreprocessEnabled(true);
  constructor.parse(path);
  ASSERT_EQ(1u, constructor.started_.size());
  EXPECT_EQ(0, constructor.started_[0].second);
}

TEST(NajaVerilogTestDirectives, malformedUnconnectedDriveNamesTheLine) {
  const auto path = writeText("naja_directives_bad.v",
    "module a(input x); endmodule\n`unconnected_drive pull2\n");
  DriveRecorder constructor;
  constructor.setPreprocessEnabled(true);
  try {
    constructor.parse(path);
    FAIL() << "did not throw";
  } catch (const VerilogException& e) {
    EXPECT_NE(std::string::npos, std::string(e.what()).find(":2")) << e.what();
  }
}

TEST(NajaVerilogTestDirectives, consumedDirectivesKeepLineNumbers) {
  // The error is on line 6 of the source; each consumed directive and each
  // inactive line must still occupy a line of the preprocessed text.
  const auto path = writeText("naja_directives_lines.v",
    "`timescale 1ns/1ps\n"
    "`define X 1\n"
    "`ifdef Y\nskipped\n`endif\n"
    "module a(input x) endmodule\n");
  VerilogConstructor constructor;
  constructor.setPreprocessEnabled(true);
  try {
    constructor.parse(path);
    FAIL() << "did not throw";
  } catch (const VerilogException& e) {
    EXPECT_NE(std::string::npos, std::string(e.what()).find("line 6")) << e.what();
  }
}

TEST(NajaVerilogTestDirectives, macroAtLineStartExpands) {
  const auto path = writeText("naja_directives_macro.v",
    "`define MOD module\n"
    "`MOD a(input x); endmodule\n");
  DriveRecorder constructor;
  constructor.setPreprocessEnabled(true);
  constructor.parse(path);
  ASSERT_EQ(1u, constructor.started_.size());
  EXPECT_EQ("a", constructor.started_[0].first);
}
