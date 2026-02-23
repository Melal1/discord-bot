#include "registry.h"
#include <fmt/format.h>

void Registry::Add(command const &cmd) noexcept
{
  Bot.log(DL::ll_info, fmt::format("Loading '{}'", cmd.command_name));
  _handlers[cmd.command_name] = std::move(cmd.function_to_call);
}

bool Registry::Dispatch(std::string_view command_name, dpp::slashcommand_t const &event) noexcept
{
  auto it = _handlers.find(command_name);
  if (it == _handlers.end())
    return 0;

  it->second(event);

  return 1;
}

void Registry::LoadBulk(command const *cmds, size_t count) noexcept
{
  _handlers.reserve(count);
  for (size_t i = 0; i < count; ++i)
    Add(cmds[i]);
}
