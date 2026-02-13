#include "pomodoro.h"
#include "session_manager.h"
#include "utils.h"
#include <cctype>
#include <dpp/appcommand.h>
#include <dpp/cache.h>
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
    dpp::guild *g = dpp::find_guild(guild_id);
    auto VC = utl::get_voice_state(g, user_id);
    if (!VC)
    {
      event.reply(msg_fl("You have to be in a VC to start a sessiona", dpp::m_ephemeral));
      return;
    }

    if (ManagerRef.GetSession(user_id))
    {
      event.reply(msg_fl("There is an already running session under your name", dpp::m_ephemeral));
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

    if (!ManagerRef.StartTimer(
            user_id,
            VC->channel_id,
            g,
            Uwork,
            Ubreak,
            Urepeat,
            [&event, &VC](SessionManager::Session const &s)
            {
              std::string msg(fmt::format("Okay starting a session in channel <#{}>\nMembers are: ", VC->channel_id));
              for (auto const &id : s.MembersId)
                msg += fmt::format("<@{}>", (long)id);

              event.reply(msg);
            } //
            ))
    {
      event.reply(msg_fl("You can't start a session with an existing one under your name", dpp::m_ephemeral));
      return;
    }

    return;
  }
  if (subcmd.name == "stop")
  {
    if (!ManagerRef.CancelTimer(user_id,
                                [&event,this](SessionManager::Session const &s)
                                { // Called if session found and before it removed
                                  if (event.command.channel_id != s.ChannelId)
                                    ManagerRef.Bot.message_create(dpp::message(s.ChannelId,std::format("Session has been canceled by <@{}>", (long)s.OwnerId)));
                                }))
    { // if body
      event.reply(msg_fl("There is not an active session to stop", dpp::m_ephemeral));
      return;
    }
    event.reply("Session canceled seccusfully");
    return;
  }
}
