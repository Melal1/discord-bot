#ifndef POMODORO_HANDLER
#define POMODORO_HANDLER
#include "session_manager.h"

class Pomodoro
{
public:
  Pomodoro(SessionManager &manager) noexcept;
  void Handler(dpp::slashcommand_t const &event) noexcept;
  SessionManager &ManagerRef;
};
#endif

void AddPomodoroSlashCommand(std::vector<dpp::slashcommand> &SlashCommands, dpp::snowflake BotId) noexcept;
