#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Ship {
class Console;

typedef std::function<int32_t(std::shared_ptr<Console> console, std::vector<std::string> args, std::string* output)>
    CommandHandler;

enum class ArgumentType { TEXT, NUMBER };

struct CommandArgument {
    std::string Info;
    ArgumentType Type = ArgumentType::NUMBER;
    bool Optional = false;
};

struct CommandEntry {
    CommandHandler Handler;
    std::string Description;
    std::vector<CommandArgument> Arguments;
};

// Developer console: command registry + dispatch. No ImGui UI (that lives in the
// inert ConsoleWindow); the command system itself is functional so registered
// commands can still be invoked programmatically.
class Console {
  public:
    Console() = default;
    ~Console() = default;

    void Init();
    int32_t Run(const std::string& command, std::string* output);
    bool HasCommand(const std::string& command);
    void AddCommand(const std::string& command, CommandEntry entry);
    std::string BuildUsage(const std::string& command);
    std::string BuildUsage(const CommandEntry& entry);
    CommandEntry& GetCommand(const std::string& command);

  private:
    std::map<std::string, CommandEntry> mCommands;
};

} // namespace Ship
