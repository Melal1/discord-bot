#ifndef REGISTRY_H
#define REGISTRY_H
#include "utils.h"
#include <dpp/cluster.h>
#include <dpp/dispatcher.h>
#include <functional>
#include <unordered_map>

class Registry
{
  using Handler = std::function<void(dpp::slashcommand_t const &)>;

public:
  struct command
  {
    std::string_view command_name;
    Handler function_to_call;
    command(std::string_view command_name, Handler function_to_call) noexcept
        : command_name(command_name), function_to_call(function_to_call) {};
    command(char const *command_name, Handler function_to_call) noexcept
        : command_name(command_name), function_to_call(function_to_call) {};
  };

  /**/
  void Add(command const &cmd) noexcept;

  bool Dispatch(std::string_view command_name, dpp::slashcommand_t const &event) noexcept;

  /*
     @brief Loads commands in bulk, useful for loading from an array of commands
     @param cmds Pointer to the first command in the array
     @param count Number of commands in the array
  */
  void LoadBulk(command const *cmds, size_t count) noexcept;

  Registry(dpp::cluster &bot) noexcept : Bot(bot)
  {
    Bot.log(DL::ll_info, "Registry init");
  };

  dpp::cluster &Bot;

private:
  std::unordered_map<std::string_view, Handler> _handlers;
};

#endif
