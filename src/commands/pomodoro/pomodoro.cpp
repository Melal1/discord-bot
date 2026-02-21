#include "pomodoro.h"
#include "session_manager.h"
#include "utils.h"
#include <dpp/appcommand.h>
#include <dpp/cache.h>
#include <dpp/channel.h>
#include <dpp/cluster.h>
#include <dpp/dispatcher.h>
#include <dpp/managed.h>
#include <dpp/message.h>
#include <dpp/snowflake.h>
#include <fmt/format.h>

constexpr unsigned DefaultWorkPeriod = 40;
constexpr unsigned DefaultBreakPeriod = 15;
constexpr unsigned DefaultRepeat = 3;

Pomodoro::Pomodoro(SessionManager &Manager) : ManagerRef(Manager)
{
  ManagerRef.Bot.log(DL::ll_info, "Pomodoro init");
}

template <bool owner_search, bool member_search> // IsInActiveSession
static inline SessionManager::Session *IsInActiveSession(Pomodoro &self, dpp::snowflake usr_id)
{
  static_assert(owner_search || member_search, "You can't set both to false");
  SessionManager::Session *res = nullptr;

  if constexpr (owner_search)
  {
    res = self.ManagerRef.GetSessionByOwnerId(usr_id);
    if (res)
      return res;
  }

  if constexpr (member_search)
  {
    res = self.ManagerRef.GetSessionByUserId(usr_id);
    if (res)
      return res;
  }

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

  if (IsInActiveSession<true, false>(self, usr_id))
  {
    event.reply(msg_fl("You can't start a session while you are in an active one!", dpp::m_ephemeral));
    return;
  }
  dpp::channel *Channel = dpp::find_channel(VC->channel_id);
  if (!Channel) // I don't think this is reqiured becasue we already checked VC
  {
    event.reply(msg_fl("Channel is not valid", dpp::m_ephemeral));
    return;
  }

  unsigned Uwork = DefaultWorkPeriod;
  unsigned Ubreak = DefaultBreakPeriod;
  unsigned Urepeat = DefaultRepeat;
  flag_t flags = 0;
  if (!subcmd.empty())
  {
    for (auto &it : subcmd.options)
    {
      using Flag = SessionManager::Session::Flag;
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
        SessionManager::SetFlag(flags, Flag::Mute, std::get<bool>(it.value));
        break;
      case 'v':
        SessionManager::SetFlag(flags, Flag::Voice, std::get<bool>(it.value));
        break;
      default:
        self.ManagerRef.Bot.log(DL::ll_error, fmt::format("Option {} not recognized assigning to default", it.name));
      }
    }
  }

  self.ManagerRef.StartTimer(
      usr_id,
      Channel,
      Uwork,
      Ubreak,
      Urepeat,
      flags,
      [&event, Channel](SessionManager::Session const &s)
      {
        std::string msg;
        msg.reserve(1024);
        msg.append(fmt::format("Okay starting a session in channel <#{}>\nMembers are: ", Channel->id));
        for (auto const &id : s.MembersId)
        {
          msg.append(fmt::format("<@{}>  ", (long)id));
        }

        if (event.command.channel_id == Channel->id)
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
  dpp::snowflake usr_id = event.command.usr.id;

  if (subcmd.name == "start")
  {
    HandlePomodoroStart(*this, event, subcmd);
    return;
  }
  if (subcmd.name == "stop")
  {
    auto Session = IsInActiveSession<true, false>(*this, usr_id);
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
    auto Session = IsInActiveSession<true, true>(*this, usr_id);
    if (!Session)
    {
      event.reply(msg_fl("You aren't in any active session !", dpp::m_ephemeral));
      return;
    }
    long RemainingTime = Session->GetRemainingTime();
    bool IsMinute = RemainingTime > 60;
    std::string msg = fmt::format(
        "Remaining time for **{}** '{}' is `{}{}`",
        Session->Flags & 1u ? "Break" : "Work",
        Session->CurrentSessionNumber - 1,
        IsMinute ? RemainingTime / 60 : RemainingTime, // If RemainingTime is more than minute display it in minutes
        IsMinute ? 'm' : 's'                           // Unit
    );

    if (event.command.channel_id == Session->ChannelId)
      event.reply(msg_fl(msg, dpp::m_ephemeral));
    else
      event.reply(msg);
    return;
  }
  if (subcmd.name == "set")
  {
    auto Session = IsInActiveSession<true, false>(*this, usr_id);
    if (!Session)
    {
      event.reply(msg_fl("You aren't an owner of any active session", dpp::m_ephemeral));
      return;
    }

    if (subcmd.empty())
      return;

    for (auto &option : subcmd.options)
    {
      using Flag = SessionManager::Session::Flag;
      bool mode = std::get<bool>(option.value);
      switch (tolower(option.name[0]))
      {
      case 'm': // mute
        if (SessionManager::HasFlag(Session->Flags, Flag::Mute) == mode)
          break;
        if (!SessionManager::HasFlag(Session->Flags, Flag::Break)) // If work session the apply it immediatly
          Session->ChangeMembersStatus(ManagerRef, mode);
        SessionManager::SetFlag(Session->Flags, Flag::Mute, mode);
        break;
      case 'v': // voice
        SessionManager::SetFlag(Session->Flags, Flag::Voice, mode);
        break;
      default:
        ManagerRef.Bot.log(DL::ll_error, fmt::format("Option {} not recognized in pomodoro set", option.name));
      }
    }
    event.reply("Option(s) has been successfully changed");
    return;
  }
}

void AddPomodoroSlashCommand(std::vector<dpp::slashcommand> &SlashCommands, dpp::snowflake BotId)
{
  dpp::slashcommand Pomodoro("pomodoro", "Manage pomodoro sessions", BotId);

  // Start

  dpp::command_option Start{dpp::co_sub_command, "start", "Start the a session"};
  Start.add_option({dpp::co_integer, "work", "Work period in minutes, defaults to 40", false});
  Start.add_option({dpp::co_integer, "break", "Break period in minutes, defaults to 15", false});
  Start.add_option({dpp::co_integer, "repeat", "How many work sessions, defaults to 3", false});

  Start.add_option(
      {dpp::co_boolean, "mute", "If you want the bot to mute members during work sessions, defaults to off", false});

  Start.add_option(
      {dpp::co_boolean,
       "voice",
       "If you want the bot to join and notify when a work/break session ends, defaults to off",
       false});

  Pomodoro.add_option(std::move(Start));

  // Stop

  Pomodoro.add_option({dpp::co_sub_command, "stop", "Stop the current working session"});

  // Time

  Pomodoro.add_option({dpp::co_sub_command, "time", "Show remaining time"});

  // Set
  dpp::command_option Set{dpp::co_sub_command, "set", "Change an active session settings"};
  Set.add_option({dpp::co_boolean, "mute", "Turn mute between session on/off", false});
  Set.add_option({dpp::co_boolean, "voice", "Turn voice notifications between session on/off", false});

  Pomodoro.add_option(std::move(Set));
  SlashCommands.push_back(std::move(Pomodoro));
}
