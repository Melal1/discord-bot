#include "pomodoro.h"
#include "utils.h"
#include <cctype>
#include <dpp/appcommand.h>
#include <fmt/format.h>
constexpr unsigned DefaultWorkPeriod = 10;
constexpr unsigned DefaultBreakPeriod = 10;
constexpr unsigned DefaultRepeat = 3;

Pomodoro::Pomodoro(SessionManager &Manager) : ManagerRef(Manager)
{
  ManagerRef.Bot.log(DL::ll_info, "Pomodoro init");
}

void Pomodoro::Handler(dpp::slashcommand_t const &event)
{
  const dpp::snowflake guild_id = event.command.guild_id;
  const dpp::snowflake user_id = event.command.usr.id;

  dpp::command_interaction cmd_data = event.command.get_command_interaction();
  auto subcmd = cmd_data.options[0];

  if (subcmd.name == "start")
  {
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
    unsigned Uwork = DefaultWorkPeriod;
    unsigned Ubreak = DefaultBreakPeriod;
    unsigned Urepeat = DefaultRepeat;
    if (!subcmd.empty())
    {
      for (auto &it : subcmd.options)
      {
        switch (tolower(it.name[0]))
        {
        case 'w':
          Uwork = std::get<long>(it.value);
          break;
        case 'b':
          Ubreak = std::get<long>(it.value);
          break;
        case 'r':
          Urepeat = std::get<long>(it.value);
          break;
          default:
          ManagerRef.Bot.log(DL::ll_error,fmt::format("Option {} not recognized assigned to default",it.name));
        }
      }
    }

    if (!ManagerRef.StartTimer(user_id, it->second.channel_id, Uwork, Ubreak, Urepeat))
    {
      event.reply("You can't start a session with an existing one under your name");
      return;
    }

    event.reply(fmt::format("Okay started a pomodoro session notification will be sent on the channel <#{}>", it->second.channel_id));
    return;
  }
  if (subcmd.name == "stop")
  {
    if (!ManagerRef.CancelTimer(user_id))
    {
      event.reply("There is not an active session to stop");
      return;
    }
    event.reply("Session canceled seccusfully");
  }
}
