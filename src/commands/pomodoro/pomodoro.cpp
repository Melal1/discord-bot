#include "pomodoro.h"
#include "utils.h"

Pomodoro::Pomodoro(SessionManager &Manager) : ManagerRef(Manager)
{
  ManagerRef.Bot.log(DL::ll_info, "Pomodoro init");
}

void Pomodoro::Handler(dpp::slashcommand_t const &event)
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
  ManagerRef.StartTimer(event.command.usr.id, it->second.channel_id);
  event.reply(fmt::format("Okay started a pomodoro session, notification will be sent on the channel <#{}>", it->second.channel_id));
}
