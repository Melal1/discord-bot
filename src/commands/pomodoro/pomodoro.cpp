#include "pomodoro.h"
#include "session_manager.h"
#include "utils.h"
#include <dpp/appcommand.h>
#include <dpp/cache.h>
#include <dpp/managed.h>
#include <dpp/message.h>
#include <dpp/snowflake.h>
#include <fmt/format.h>

constexpr unsigned DefaultWorkPeriod = 10;
constexpr unsigned DefaultBreakPeriod = 10;
constexpr unsigned DefaultRepeat = 3;

Pomodoro::Pomodoro(SessionManager &Manager) : ManagerRef(Manager)
{
  ManagerRef.Bot.log(DL::ll_info, "Pomodoro init");
}

static inline SessionManager::Session *FindUsrSession(Pomodoro &self, dpp::snowflake usr_id)
{
  auto res = self.ManagerRef.GetSessionByOwnerId(usr_id);
  if (res)
    return res;
  res = self.ManagerRef.GetSessionByUserId(usr_id);
  if (res)
    return res;
  return nullptr;
}

static inline void
HandlePomodoroStart(Pomodoro &self, const dpp::slashcommand_t &event, dpp::command_data_option &subcmd)
{
  dpp::snowflake guild_id = event.command.guild_id, usr_id = event.command.usr.id;
  dpp::guild *g = dpp::find_guild(guild_id);
  auto VC = utl::get_voice_state(g, usr_id);
  if (!VC)
  {
    event.reply(msg_fl("You have to be in a VC to start a session", dpp::m_ephemeral));
    return;
  }

  if (FindUsrSession(self, usr_id))
  {
    event.reply(msg_fl("There is an already running session under your name", dpp::m_ephemeral));
    return;
  }

  unsigned Uwork = DefaultWorkPeriod;
  unsigned Ubreak = DefaultBreakPeriod;
  unsigned Urepeat = DefaultRepeat;
  uint8_t flags = 0;
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
      case 'm':
        if (std::get<bool>(it.value))
          flags |= (uint8_t)SessionManager::Session::Flag::Mute;

        break;
      default:
        self.ManagerRef.Bot.log(DL::ll_error, fmt::format("Option {} not recognized assigning to default", it.name));
      }
    }
  }

  self.ManagerRef.StartTimer(
      usr_id,
      VC->channel_id,
      g,
      Uwork,
      Ubreak,
      Urepeat,
      flags,
      [&event, &VC](SessionManager::Session const &s)
      {
        std::string msg;
        msg.reserve(1024);
        msg.append(fmt::format("Okay starting a session in channel <#{}>\nMembers are: ", VC->channel_id));
        for (auto const &id : s.MembersId)
        {
          msg.append(fmt::format("<@{}>  ", (long)id));
        }

        if (event.command.channel_id == VC->channel_id)
          event.reply(msg_fl(msg, dpp::m_ephemeral));
        else
          event.reply(msg);
      } //
  );

  return;
}

void Pomodoro::Handler(dpp::slashcommand_t const &event)
{
  dpp::command_interaction cmd_data = event.command.get_command_interaction();
  auto subcmd = cmd_data.options[0];

  if (subcmd.name == "start")
  {
    HandlePomodoroStart(*this, event, subcmd);
    return;
  }
  if (subcmd.name == "stop")
  {
    auto Session = ManagerRef.GetSessionByOwnerId(event.command.usr.id);
    if (!Session)
    {
      event.reply(msg_fl("You aren't an owner of any active session", dpp::m_ephemeral));
      return;
    }
    ManagerRef.CancelTimer(Session,

                                [this](SessionManager::Session const &s)
                                { // Called if session found and before it removed
        ManagerRef.Bot.message_create(
            dpp::message(s.ChannelId, std::format("Session has been canceled by <@{}>", (long)s.OwnerId)));
                                }
        );
    if (event.command.channel_id != Session->ChannelId)
      event.reply("Session is seccusfully canceled");
    else
      event.reply(msg_fl("Session is seccusfully canceled", dpp::m_ephemeral));
  }
  if (subcmd.name == "time")
  {
    auto Session = ManagerRef.GetSessionByUserId(event.command.usr.id);
    if (!Session)
    {
      event.reply(msg_fl("You aren't in any active session", dpp::m_ephemeral));
      return;
    }
    long RemainingTime = Session->GetRemainingTime();
    bool IsMinute = RemainingTime > 60;
    std::string msg = fmt::format(
        "Remaining time for **{}** '{}' is `{}{}`",
        Session->Flags & 1u ? "Break" : "Work",
        Session->CurrentSessionNo - 1,
        IsMinute ? RemainingTime / 60 : RemainingTime, // If RemainingTime is more than minute display it in minutes
        IsMinute ? 'm' : 's'                           // Unit
    );

    if (event.command.channel_id == Session->ChannelId)
      event.reply(msg_fl(msg, dpp::m_ephemeral));
    else
      event.reply(msg);
    return;
  }
}
