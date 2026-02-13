#include "pomodoro.h"
#include "registry.h"
#include "session_manager.h"
#include "utils.h"
#include <dpp/appcommand.h>
#include <dpp/message.h>
#include <vector>

int main()
{
  std::string BotToken;
  if (!utl::GetBotToken(BotToken))
  {
    fmt::print(stderr, "No Bot Token found in env var DisBotTok");
    return 1;
  }
  dpp::cluster bot(BotToken, dpp::i_default_intents | dpp::i_message_content);
  bot.on_log([](const dpp::log_t &e)
             { fmt::print(stderr, "[{}\x1b[0m] {}\n", utl::SeverityName(e.severity), e.message); });

  SessionManager mgr(bot);

  Pomodoro PomHandler(mgr);
  Registry Commands(bot);

  Commands.Add("pomodoro", [&PomHandler](dpp::slashcommand_t const &e) { PomHandler.Handler(e); });

  bot.on_slashcommand(
      [&](const dpp::slashcommand_t &event)
      {
        if (!Commands.Dispatch(event.command.get_command_name(), event))
          event.reply(msg_fl("UNKOWN COMMAND Please contact Melal", dpp::m_ephemeral));
      });

  bot.on_ready(
      [&bot](const dpp::ready_t &event)
      {
        if (dpp::run_once<struct register_bot_commands>())
        {
          std::vector<dpp::slashcommand> SlashCommands{{"pomodoro", "Manage pomodoro sessions", bot.me.id}};

          SlashCommands[0].add_option(
              dpp::command_option(dpp::co_sub_command, "start", "Start the a session")
                  .add_option(dpp::command_option(dpp::co_integer, "work", "Work period in minutes", false))
                  .add_option(dpp::command_option(dpp::co_integer, "break", "Break period in minutes", false))
                  .add_option(dpp::command_option(dpp::co_integer, "repeat", "How many work sessions", false)));

          SlashCommands[0].add_option(
              dpp::command_option(dpp::co_sub_command, "stop", "Stop the current working session"));

          bot.global_bulk_command_create(SlashCommands);
        }
      });

  bot.start(dpp::st_wait);
}
