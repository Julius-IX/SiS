#include <Evaluator.h>
#include <Parser.h>
#include <Value.h>
#include <gtest/gtest.h>

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Locates sis_sources/ relative to the test binary via argv[0], so no path
// needs to be hard-coded or passed on the command line.
//
// runFile()   – parse + evaluate a .sis file, capture stdout, return the last
//               Value. Throws std::runtime_error on parse failure so ASSERT_NO_THROW
//               can be used when the test cares about that specifically.
//
// captureRun() – same but only returns the captured stdout lines.

namespace {
  namespace fs = std::filesystem;

  // Derived once at test-binary startup from argv[0]. Everything is relative
  // to this so the suite runs from any working directory.
  fs::path g_sis_sources_dir;

  // Called by the fixture below; separated so it can also be used directly.
  fs::path sourcePath(const std::string& relative) { return g_sis_sources_dir / relative; }

  // Full pipeline: read file → parse (with real include resolution rooted at
  // the file's own directory) → evaluate. Captured stdout is appended to
  // `output_lines`. Returns the last Value produced by the program.
  eval::Value runFile(const fs::path& path, std::vector<std::string>& output_lines) {
    par::Parser parser;
    bool ok = parser.parseRoot(path);
    LOG_DEBUG_FLUSH("parseRoot returned: {}", ok);
    if (!ok) throw std::runtime_error("Parse failed: " + path.string());

    // Redirect stdout to a temp file — avoids pipe buffer deadlock if the
    // evaluator produces more output than the pipe can hold before we drain it.
    fs::path tmp = fs::temp_directory_path() / "sis_test_capture.txt";
    fflush(stdout);
    int saved_fd = dup(STDOUT_FILENO);
    int out_fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    dup2(out_fd, STDOUT_FILENO);
    close(out_fd);

    eval::Evaluator evaluator;
    eval::Value result = evaluator.run(parser.peekRoot());

    fflush(stdout);
    dup2(saved_fd, STDOUT_FILENO);
    close(saved_fd);

    // Read captured output back from the temp file
    std::ifstream captured(tmp);
    std::string line;
    while (std::getline(captured, line))
      output_lines.push_back(line);

    return result;
  }

  // Convenience wrapper when the caller only cares about output lines.
  std::vector<std::string> captureRun(const std::string& relative_path) {
    std::vector<std::string> out;
    runFile(sourcePath(relative_path), out);
    return out;
  }

  // Convenience wrapper when the caller only cares about the final Value.
  eval::Value evalRun(const std::string& relative_path) {
    std::vector<std::string> out;
    return runFile(sourcePath(relative_path), out);
  }

  // Base fixture resolves paths and surfaces runFile/captureRun as members.
  class E2E : public ::testing::Test {
    protected:
    static std::vector<std::string> run(const std::string& relative_path) { return captureRun(relative_path); }
    static eval::Value eval(const std::string& relative_path) { return evalRun(relative_path); }
  };

  // Locates sis_sources/ by walking up from the binary until the directory
  // is found, or falls back to CWD/sis_sources if nothing is found.
  // Registered as a global environment setup so it runs before any test.
  class PathSetup : public ::testing::Environment {
    public:
    void SetUp() override {
      const auto& argv = ::testing::internal::GetArgvs();
      fs::path binary = argv.empty() ? fs::current_path() / "test_binary" : fs::path(argv[0]);
      fs::path dir = fs::weakly_canonical(binary).parent_path();
      while (true) {
        fs::path candidate = dir / "sis_sources" / "tests";
        if (fs::is_directory(candidate)) {
          g_sis_sources_dir = candidate;
          return;
        }
        fs::path parent = dir.parent_path();
        if (parent == dir) break; // filesystem root
        dir = parent;
      }
      // Last resort: CWD
      g_sis_sources_dir = fs::current_path() / "sis_sources" / "tests";
    }
  };

  // Registers PathSetup with GTest without touching main().
  const ::testing::Environment* const m_path_env = ::testing::AddGlobalTestEnvironment(new PathSetup);

} // namespace
