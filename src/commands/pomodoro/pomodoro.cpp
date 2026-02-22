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
#include <optional>

constexpr unsigned DefaultWorkPeriod = 40;
constexpr unsigned DefaultBreakPeriod = 15;
constexpr unsigned DefaultRepeat = 3;

// constructor-------

Pomodoro::Pomodoro(SessionManager &Manager) noexcept : ManagerRef(Manager)
{
  ManagerRef.Bot.log(DL::ll_info, "Pomodoro init");
}

// ------------------

// Helper functions--

/*
   @brief Get a value from a command_data_option variant.
   @param bot if provided then the function will use the bot to log errors.
   @param event if provided then the function will reply to the event when error happend.
   @return returns the value if it's there and nullopt if not.
 */
template <class T>
[[nodiscard]]
static inline std::optional<T> GetValueSafe(
    dpp::command_data_option const &option,
    dpp::cluster *bot = nullptr,
    dpp::slashcommand_t const *event = nullptr) noexcept
{
  static_assert(
      std::disjunction_v<
          std::is_same<T, std::string>,
          std::is_same<T, int64_t>,
          std::is_same<T, bool>,
          std::is_same<T, dpp::snowflake>,
          std::is_same<T, double>>,
      "T must be a valid command_value type");

  if (const T *v = std::get_if<T>(&option.value))
    return *v;

  if (event)
    event->reply("Bot error happened; please contact Melal");

  if (bot)
    bot->log(DL::ll_error, fmt::format("Error when trying to get value from option {}", option.name));

  return std::nullopt;
}

template <bool owner_search, bool member_search> // IsInActiveSession
static inline SessionManager::Session *IsInActiveSession(Pomodoro &self, dpp::snowflake usr_id) noexcept
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

  uint32_t Uwork = DefaultWorkPeriod;
  uint32_t Ubreak = DefaultBreakPeriod;
  uint32_t Urepeat = DefaultRepeat;
  flag_t flags = 0;

  auto get_period = [&self, &event](uint32_t &var, dpp::command_data_option const &option) noexcept -> bool
  {
    if (auto v = GetValueSafe<int64_t>(option, &self.ManagerRef.Bot, &event))
    {
      if (*v <= 0)
      {
        event.reply(msg_fl(fmt::format("Not valid period {}", *v), dpp::m_ephemeral));
        return 0;
      }
      if (*v > 4 * 3600)
      {
        event.reply(msg_fl("Max period is 4 hours for work/break", dpp::m_ephemeral));
        return 0;
      }
      var = *v;
      return 1;
    }
    else
      return 0;
  };
  using Flag = SessionManager::Session::Flag;

  auto get_flag = [&self, &event](flag_t &var, Flag flag, dpp::command_data_option const &option) noexcept -> bool
  {
    if (auto v = GetValueSafe<bool>(option, &self.ManagerRef.Bot, &event))
    {
      SessionManager::SetFlag(var, flag, *v);
      return 1;
    }
    else
      return 0;
  };

  if (!subcmd.empty())
  {
    for (auto &it : subcmd.options)
    {
      switch (tolower(it.name[0]))
      {
      case 'w':
        if (!get_period(Uwork, it))
          return;
        break;
      case 'b':
        if (!get_period(Ubreak, it))
          return;
        break;
      case 'r':
        if (!get_period(Urepeat, it))
          return;
        break;
      case 'm':
        if (!get_flag(flags, Flag::Mute, it))
          return;
        break;
      case 'v':
        if (!get_flag(flags, Flag::Voice, it))
          return;
        break;
      default:
        self.ManagerRef.Bot.log(DL::ll_error, fmt::format("Option {} not recognized", it.name));
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
          msg.append(fmt::format("<@{}>  ", (int64_t)id));
        }

        if (event.command.channel_id == Channel->id)
          event.reply(msg_fl(msg, dpp::m_ephemeral));
        else
          event.reply(msg);
      } //
  );

  return;
}

// ------------------

void Pomodoro::Handler(dpp::slashcommand_t const &event) noexcept
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
        "Remaining time for **{}** '{}' is `{}` {}",
        Session->Flags & 1u ? "Break" : "Work",
        Session->CurrentSessionNumber - 1,
        IsMinute ? RemainingTime / 60 : RemainingTime, // If RemainingTime is more than minute display it in minutes
        IsMinute ? "minutes" : "seconds"               // Unit
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
      bool mode;
      if (auto v = GetValueSafe<bool>(option))
        mode = *v;
      else
      {
        event.reply("Error happend please contact melal");
        ManagerRef.Bot.log(DL::ll_error, fmt::format("While obtaining value {}", option.name));
        return;
      }

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

void AddPomodoroSlashCommand(std::vector<dpp::slashcommand> &SlashCommands, dpp::snowflake BotId) noexcept
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
