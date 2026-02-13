#include "pomodoro.h"
#include "utils.h"
#include <cctype>
#include <dpp/appcommand.h>
#include <dpp/message.h>
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
    auto VC = utl::get_voice_state(guild_id, user_id);
    if (!VC)
    {
      event.reply(msg("You have to be in a VC to start a sessiona", dpp::m_ephemeral));
      return;
    }

    if (ManagerRef.GetSession(user_id))
    {
      event.reply(msg("There is an already running session under your name", dpp::m_ephemeral));
      return;
    }

    unsigned Uwork = DefaultWorkPeriod;
    unsigned Ubreak = DefaultBreakPeriod;
    unsigned Urepeat = DefaultRepeat;
    if (!subcmd.empty())
    {
      for (auto &it : subcmd.options)
      {
        long Value = std::get<long>(it.value);
        if (Value <= 0)
          continue;
        switch (tolower(it.name[0]))
        {
        case 'w':
          Uwork = Value;
          break;
        case 'b':
          Ubreak = Value;
          break;
        case 'r':
          Urepeat = Value;
          break;
        default:
          ManagerRef.Bot.log(DL::ll_error, fmt::format("Option {} not recognized assigning to default", it.name));
        }
      }
    }

    if (!ManagerRef.StartTimer(user_id, VC->channel_id, Uwork, Ubreak, Urepeat))
    {
      event.reply(msg("You can't start a session with an existing one under your name", dpp::m_ephemeral));
      return;
    }

    event.reply(fmt::format("Okay started a pomodoro session notification will be sent on the channel <#{}>", VC->channel_id));
    return;
  }
  if (subcmd.name == "stop")
  {
    if (!ManagerRef.CancelTimer(user_id))
    {
      event.reply(msg("There is not an active session to stop", dpp::m_ephemeral));
      return;
    }
    event.reply("Session canceled seccusfully");
    return;
  }
}
