#ifndef POMODORO_HANDLER
#define POMODORO_HANDLER
#include "session_manager.h"

class Pomodoro
{
public:
  Pomodoro(SessionManager &manager);
  void Handler(dpp::slashcommand_t const &event);
  SessionManager &ManagerRef;
};
#endif

void AddPomodoroSlashCommand(std::vector<dpp::slashcommand> &SlashCommands, dpp::snowflake BotId);
