#include "SessionManager.h"
#include <dpp/dpp.h>
#include <fmt/core.h>

 bool GetBotToken(std::string &Buffer)
{
  const char* res =  getenv("DisBotTok");
  if(!res)
    return 0;
  Buffer = res;
  return 1;
}


constexpr const char *SeverityName(dpp::loglevel lvl)
{
  switch (lvl)
  {
  case dpp::ll_trace:
    return "\x1b[90mtrace"; // gray
  case dpp::ll_debug:
    return "\x1b[36mdebug"; // cyan
  case dpp::ll_info:
    return "\x1b[32minfo"; // green
  case dpp::ll_warning:
    return "\x1b[33mwarn"; // yellow
  case dpp::ll_error:
    return "\x1b[31merror"; // red
  case dpp::ll_critical:
    return "\x1b[91mcritical"; // bright red
  default:
    return "\x1b[0munknown";
  }
}

int main()
{
  std::string BotToken;
  if(!GetBotToken(BotToken))
  {
    fmt::print(stderr,"No Bot Token found in env var DisBotTok");
    return 1;
  }
  dpp::cluster bot(BotToken, dpp::i_default_intents | dpp::i_message_content);
  SessionManager mgr(bot);

  bot.on_log([](const dpp::log_t &e) { fmt::print(stderr, "[{}\x1b[0m] {}\n", SeverityName(e.severity), e.message); });

  bot.on_slashcommand(
      [&](const dpp::slashcommand_t &event)
      {
        if (event.command.get_command_name() == "pomodoro")
        {
          const dpp::snowflake guild_id = event.command.guild_id;
          const dpp::snowflake user_id = event.command.usr.id;

          dpp::guild *g = dpp::find_guild(guild_id);
          if (!g)
          {
            event.reply("Couldn't find the guild try again later");
            return;
          }

          auto it = g->voice_members.find(user_id);
          if (it == g->voice_members.end() || it->second.channel_id.empty())
          {
            event.reply("To start a session you need to be in a voice channel first!");
            return;
          }
          mgr.StartTimer(event.command.usr.id, it->second.channel_id);
          event.reply(fmt::format("Okay started a pomodoro session, notification will be sent on the channel <#{}>", it->second.channel_id));
        }
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
