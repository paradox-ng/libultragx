#include "ship/debug/Console.h"

#include <sstream>

namespace Ship {

void Console::Init() {}

void Console::AddCommand(const std::string& command, CommandEntry entry) {
    mCommands[command] = std::move(entry);
}

bool Console::HasCommand(const std::string& command) {
    return mCommands.find(command) != mCommands.end();
}

CommandEntry& Console::GetCommand(const std::string& command) {
    return mCommands[command];
}

int32_t Console::Run(const std::string& command, std::string* output) {
    // Split into whitespace-separated tokens; token 0 is the command name.
    std::istringstream iss(command);
    std::vector<std::string> args;
    std::string token;
    while (iss >> token) {
        args.push_back(token);
    }
    if (args.empty()) {
        return -1;
    }
    auto it = mCommands.find(args[0]);
    if (it == mCommands.end() || !it->second.Handler) {
        if (output) *output = "command not found: " + args[0];
        return -1;
    }
    // No live Console instance is handed to the handler (there is no console UI);
    // handlers that need it must tolerate a null pointer.
    return it->second.Handler(nullptr, args, output);
}

std::string Console::BuildUsage(const std::string& command) {
    auto it = mCommands.find(command);
    return it != mCommands.end() ? BuildUsage(it->second) : "";
}

std::string Console::BuildUsage(const CommandEntry& entry) {
    return entry.Description;
}

} // namespace Ship
