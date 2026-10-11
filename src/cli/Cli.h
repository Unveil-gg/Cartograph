#pragma once

#include <iosfwd>

namespace Cartograph {

// Process exit codes of the command line tool.
constexpr int kCliOk = 0;
constexpr int kCliValidationFailed = 1;
constexpr int kCliUsage = 2;
constexpr int kCliIoError = 3;

/**
 * Run the headless command line tool.
 *
 *   export <input> [-o <output.json>]    write project.json (v3) JSON
 *   validate <input> [--order a,b,c]     check gates and reachability
 *
 * @param argc Argument count (as passed to main)
 * @param argv Arguments; argv[0] is the program name
 * @param out Stream for normal output
 * @param err Stream for errors and usage
 * @return Exit code (kCli*)
 */
int RunCli(int argc, const char* const* argv, std::ostream& out,
           std::ostream& err);

}  // namespace Cartograph
