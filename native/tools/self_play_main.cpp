#include "ashen/benchmark/SelfPlay.hpp"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {

void print_usage() {
  std::cout << "Usage: ashen_self_play [options]\n"
               "\n"
               "Options:\n"
               "  --output PATH          Write the stable JSON report to PATH.\n"
               "  --seeds COUNT          Number of consecutive seeds to run (default: 2).\n"
               "  --first-seed SEED      First match seed (default: 1).\n"
               "  --max-ticks TICKS      Per-match tick budget (default: 12000).\n"
               "  --checkpoint TICKS     State checkpoint interval (default: 500).\n"
               "  --no-verify            Skip the duplicate determinism replay.\n"
               "  --help                 Show this message.\n";
}

template <typename Value>
[[nodiscard]] bool parse_unsigned(const std::string_view input, Value& output) noexcept {
  static_assert(std::numeric_limits<Value>::is_integer && !std::numeric_limits<Value>::is_signed);
  if (input.empty()) {
    return false;
  }
  Value parsed{};
  const auto result = std::from_chars(input.data(), input.data() + input.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != input.data() + input.size()) {
    return false;
  }
  output = parsed;
  return true;
}

[[nodiscard]] bool require_value(const int argc, char** argv, int& index,
                                 std::string_view& value) {
  if (index + 1 >= argc) {
    std::cerr << "Missing value after " << argv[index] << ".\n";
    return false;
  }
  value = argv[++index];
  return true;
}

}  // namespace

int main(const int argc, char** argv) {
  ashen::benchmark::SuiteOptions options{};
  std::filesystem::path output_path{"self-play-report.json"};

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--help") {
      print_usage();
      return EXIT_SUCCESS;
    }
    if (argument == "--no-verify") {
      options.verify_determinism = false;
      continue;
    }

    std::string_view value;
    if (!require_value(argc, argv, index, value)) {
      return EXIT_FAILURE;
    }
    if (argument == "--output") {
      output_path = std::filesystem::path{value};
    } else if (argument == "--seeds") {
      if (!parse_unsigned(value, options.seed_count) || options.seed_count == 0) {
        std::cerr << "--seeds must be a positive integer.\n";
        return EXIT_FAILURE;
      }
    } else if (argument == "--first-seed") {
      if (!parse_unsigned(value, options.first_seed)) {
        std::cerr << "--first-seed must be an unsigned integer.\n";
        return EXIT_FAILURE;
      }
    } else if (argument == "--max-ticks") {
      if (!parse_unsigned(value, options.maximum_match_ticks) || options.maximum_match_ticks == 0) {
        std::cerr << "--max-ticks must be a positive integer.\n";
        return EXIT_FAILURE;
      }
    } else if (argument == "--checkpoint") {
      if (!parse_unsigned(value, options.checkpoint_interval) || options.checkpoint_interval == 0) {
        std::cerr << "--checkpoint must be a positive integer.\n";
        return EXIT_FAILURE;
      }
    } else {
      std::cerr << "Unknown option: " << argument << "\n";
      print_usage();
      return EXIT_FAILURE;
    }
  }

  const auto report = ashen::benchmark::run_suite(options);
  const auto json = ashen::benchmark::to_json(report);
  std::error_code directory_error;
  if (output_path.has_parent_path()) {
    std::filesystem::create_directories(output_path.parent_path(), directory_error);
  }
  if (directory_error) {
    std::cerr << "Could not create report directory: " << directory_error.message() << "\n";
    return EXIT_FAILURE;
  }

  std::ofstream output{output_path, std::ios::binary | std::ios::trunc};
  if (!output) {
    std::cerr << "Could not open report path: " << output_path.string() << "\n";
    return EXIT_FAILURE;
  }
  output.write(json.data(), static_cast<std::streamsize>(json.size()));
  output.close();
  if (!output) {
    std::cerr << "Could not finish writing report: " << output_path.string() << "\n";
    return EXIT_FAILURE;
  }

  std::cout << "Vowfall deterministic self-play\n"
            << "  matches: " << report.matches.size() << "\n"
            << "  fixed scenarios: " << report.fixed_scenarios.size() << "\n"
            << "  scenario checks: " << report.scenario_checks_passed << '/'
            << report.scenario_checks_total << "\n"
            << "  hard failures: " << report.hard_failures.size() << "\n"
            << "  balance alerts: " << report.balance_alerts.size() << "\n"
            << "  report: " << output_path.string() << "\n";
  for (const auto& failure : report.hard_failures) {
    std::cerr << "  [FAIL] " << failure.scenario << " seed " << failure.seed << ": "
              << failure.code << " - " << failure.message << "\n";
  }
  for (const auto& alert : report.balance_alerts) {
    std::cout << "  [BALANCE] " << alert.cohort << " - " << alert.message << "\n";
  }
  return report.hard_failures.empty() ? EXIT_SUCCESS : EXIT_FAILURE;
}
