#include "loadcommands.h"
#include "session_manager.h"
#include "utils.h"
#include <dpp/appcommand.h>
#include <dpp/message.h>
#include <dpp/misc-enum.h>
#include <vector>

int main()
{
  std::string BotToken;
  if (!utl::GetBotToken(BotToken))
  {
    fmt::print(stderr, "No Bot Token found in env var DisBotTok");
    return 1;
  }
  dpp::cluster bot(BotToken, dpp::i_default_intents | dpp::i_message_content | dpp::i_guild_voice_states);

  bot.on_log(
      [](dpp::log_t const &e)
      {
        if (e.severity >= dpp::loglevel::ll_debug)
          fmt::print(stderr, "[{}\x1b[0m] {}\n", utl::SeverityName(e.severity), e.message);
      });

  SessionManager mgr(bot);
  Pomodoro PomHandler(mgr);
  Registry Commands(bot);
  LoadAllCommands(Commands, PomHandler);

  bot.on_slashcommand(
      [&](const dpp::slashcommand_t &event)
      {
        if (!Commands.Dispatch(event.command.get_command_name(), event))
          event.reply(msg_fl("UNKOWN COMMAND Please contact Melal", dpp::m_ephemeral));
      });

  bot.on_voice_state_update([&bot, &PomHandler](dpp::voice_state_update_t const &e) { PomHandler.VCHandler(e); });

  bot.on_ready(
      [&bot](const dpp::ready_t &event)
      {
        if (dpp::run_once<struct register_bot_commands>())
        {
          std::vector<dpp::slashcommand> SlashCommands;
          SlashCommands.reserve(1);
          AddPomodoroSlashCommand(SlashCommands, bot.me.id);

          bot.global_bulk_command_create(SlashCommands);
        }
      });

  bot.start(dpp::st_wait);
}
