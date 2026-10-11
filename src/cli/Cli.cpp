#include "Cli.h"

#include "../IOJson.h"
#include "../Model.h"
#include "../ProjectLoad.h"
#include "../Reachability.h"
#include "../platform/Fs.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace Cartograph {

namespace {

const char* kUsage =
    "usage: cartograph-cli <command> [options]\n"
    "\n"
    "commands:\n"
    "  export <input> [-o <output.json>]   write project JSON\n"
    "                                       (-o - or no -o: stdout)\n"
    "  validate <input> [--order a,b,c]    check gates and reachability\n"
    "  help                                show this text\n"
    "\n"
    "<input> is a .cart package, a .cartproj folder, or a project.json.\n"
    "exit codes: 0 ok, 1 validation errors, 2 usage, 3 read/write failure\n";

std::vector<std::string> SplitCommas(const std::string& text) {
    std::vector<std::string> out;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

int RunExport(const std::vector<std::string>& args, std::ostream& out,
              std::ostream& err) {
    std::string input;
    std::string output = "-";
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-o" || args[i] == "--output") {
            if (i + 1 >= args.size()) {
                err << "error: " << args[i] << " needs a path\n";
                return kCliUsage;
            }
            output = args[++i];
        } else if (!args[i].empty() && args[i][0] == '-' &&
                   args[i] != "-") {
            err << "error: unknown option " << args[i] << "\n";
            return kCliUsage;
        } else if (input.empty()) {
            input = args[i];
        } else {
            err << "error: unexpected argument " << args[i] << "\n";
            return kCliUsage;
        }
    }
    if (input.empty()) {
        err << "error: export needs an input path\n" << kUsage;
        return kCliUsage;
    }

    Model model;
    std::string error;
    if (!LoadProjectAny(input, model, &error)) {
        err << "error: " << error << "\n";
        return kCliIoError;
    }
    std::string json = IOJson::SaveToString(model);
    if (output == "-") {
        out << json << "\n";
        return kCliOk;
    }
    if (!Platform::WriteTextFile(output, json + "\n")) {
        err << "error: could not write " << output << "\n";
        return kCliIoError;
    }
    return kCliOk;
}

int RunValidate(const std::vector<std::string>& args, std::ostream& out,
                std::ostream& err) {
    std::string input;
    std::vector<std::string> order;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--order") {
            if (i + 1 >= args.size()) {
                err << "error: --order needs a comma-separated list\n";
                return kCliUsage;
            }
            order = SplitCommas(args[++i]);
        } else if (!args[i].empty() && args[i][0] == '-') {
            err << "error: unknown option " << args[i] << "\n";
            return kCliUsage;
        } else if (input.empty()) {
            input = args[i];
        } else {
            err << "error: unexpected argument " << args[i] << "\n";
            return kCliUsage;
        }
    }
    if (input.empty()) {
        err << "error: validate needs an input path\n" << kUsage;
        return kCliUsage;
    }

    Model model;
    std::string error;
    if (!LoadProjectAny(input, model, &error)) {
        err << "error: " << error << "\n";
        return kCliIoError;
    }
    ValidationReport report = ValidateReachability(model, order);
    for (const auto& line : DescribeReport(model, report)) {
        out << line << "\n";
    }
    return report.HasErrors() ? kCliValidationFailed : kCliOk;
}

}  // namespace

int RunCli(int argc, const char* const* argv, std::ostream& out,
           std::ostream& err) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.push_back(argv[i]);
    if (args.empty()) {
        err << kUsage;
        return kCliUsage;
    }
    std::string command = args[0];
    args.erase(args.begin());
    if (command == "help" || command == "--help" || command == "-h") {
        out << kUsage;
        return kCliOk;
    }
    if (command == "export") return RunExport(args, out, err);
    if (command == "validate") return RunValidate(args, out, err);
    err << "error: unknown command " << command << "\n" << kUsage;
    return kCliUsage;
}

}  // namespace Cartograph
