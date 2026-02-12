#include "registry.h"
#include <fmt/format.h>

void Registry::Add(std::string_view command_name, Handler call)
{
  Bot.log(DL::ll_info, fmt::format("Loading '{}'", command_name));
  Handlers[command_name] = std::move(call);
}

bool Registry::Dispatch(std::string_view command_name, dpp::slashcommand_t const &event)
{
  auto it = Handlers.find(command_name);
  if (it == Handlers.end())
    return 0;

  it->second(event);

  return 1;
}
