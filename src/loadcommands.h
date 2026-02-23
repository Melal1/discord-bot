#ifndef LOADCOMMANDS_H
#define LOADCOMMANDS_H
// This is a helper header to load all commands in one place
#include "pomodoro.h"
#include "registry.h"
constexpr uint32_t command_count = 1;

constexpr inline void LoadAllCommands(Registry &registry, Pomodoro &pomodoro_handler) noexcept
{
  Registry::command commands[command_count] = {
      {"pomodoro",
       [&pomodoro_handler](dpp::slashcommand_t const &event) { pomodoro_handler.SlashCommandHandler(event); }}};

  registry.LoadBulk(commands, command_count);
  return;
}

#endif
