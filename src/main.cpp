#include "pomodoro.h"
#include "registry.h"
#include "session_manager.h"
#include "utils.h"

int main()
{
  std::string BotToken;
  if (!GetBotToken(BotToken))
  {
    fmt::print(stderr, "No Bot Token found in env var DisBotTok");
    return 1;
  }
  dpp::cluster bot(BotToken, dpp::i_default_intents | dpp::i_message_content);
  bot.on_log([](const dpp::log_t &e) { fmt::print(stderr, "[{}\x1b[0m] {}\n", SeverityName(e.severity), e.message); });

  SessionManager mgr(bot);
  Pomodoro PomHandler(mgr);
  Registry Commands(bot);

  Commands.Add("pomodoro", [&PomHandler](dpp::slashcommand_t const &e) { PomHandler.Handler(e); });

  bot.on_slashcommand(
      [&](const dpp::slashcommand_t &event)
      {
        if (!Commands.Dispatch(event.command.get_command_name(), event))
          event.reply("UNKOWN COMMAND Please contact Melal");
      });

  bot.on_ready(
      [&bot](const dpp::ready_t &event)
      {
        if (dpp::run_once<struct register_bot_commands>())
        {
          bot.global_command_create(dpp::slashcommand("pomodoro", "Starts a pomodoro session", bot.me.id));
        }
      });

  bot.start(dpp::st_wait);
}
