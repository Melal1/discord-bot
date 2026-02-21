#include "session_manager.h"
#include "utils.h"
#include "voice.h"
#include <chrono>
#include <dpp/cache.h>
#include <dpp/channel.h>
#include <dpp/cluster.h>
#include <dpp/guild.h>
#include <dpp/message.h>
#include <dpp/misc-enum.h>
#include <dpp/timer.h>
#include <fmt/format.h>
#include <functional>
#include <string>
#include <type_traits>
using SMS = SessionManager::Session;
constexpr const uint32_t sec_in_min = 2;

// Constructors
SessionManager::SessionManager(dpp::cluster &bot) : Bot(bot)
{
  Bot.log(dpp::loglevel::ll_info, "Session manager init");
}

static_assert(sec_in_min >= 1, "sec in min var must be positve");
static_assert(std::is_integral_v<decltype(sec_in_min)>, "sec in min var must be and intger");

SMS::Session(
    snflake usr_id,
    snflake channel_id,
    snflake guild_id,
    std::vector<snflake> &&members_ids,
    unsigned work_period,
    unsigned break_period,
    unsigned repeat,
    std::string_view vc_channel_name,
    flag_t flags)
    : OwnerId(usr_id), ChannelId(channel_id), GuildId(guild_id), MembersId(std::move(members_ids)),
      WorkPeriod(work_period * sec_in_min), BreakPeriod(break_period * sec_in_min), Repeat(repeat + 1),
      CurrentSessionNumber(1), VoiceChannelName(vc_channel_name), Flags(flags)
{
  Flags |= 1u << 0; // Set starting phase to break to start togggling correctly
}

//-------------

// Getters

//// Session Manager
SMS *SessionManager::GetSessionByOwnerId(snflake owner_id)
{
  auto it = _active_sessions.find(owner_id);
  return it == _active_sessions.end() ? nullptr : &it->second;
}

SMS const *SessionManager::GetSessionByOwnerId(snflake owner_id) const
{
  auto it = _active_sessions.find(owner_id);
  return it == _active_sessions.end() ? nullptr : &it->second;
}

SMS *SessionManager::GetSessionByUserId(snflake usr_id)
{
  for (auto &[_, Session] : _active_sessions)
    for (auto id : Session.MembersId)
      if (usr_id == id)
        return &Session;

  return nullptr;
}

SMS const *SessionManager::GetSessionByUserId(snflake usr_id) const
{
  for (auto &[_, Session] : _active_sessions)
    for (auto id : Session.MembersId)
      if (usr_id == id)
        return &Session;

  return nullptr;
}

//// Session
long SMS::GetRemainingTime()
{
  using namespace std::chrono;
  auto elapsed = duration_cast<seconds>(steady_clock::now() - PhaseStartTime).count();
  return !mFlagCmp(Flags, Break) ? WorkPeriod - elapsed : BreakPeriod - elapsed;
}

void SMS::SchedulePhase(SessionManager &manager)
{

  PhaseStartTime = std::chrono::steady_clock::now();
  auto &Bot = manager.Bot;
  if (CurrentSessionNumber >= Repeat)
  {
    Bot.message_create(dpp::message(ChannelId, "Pomodoro session finished!"));
    ChangeMembersStatus(manager, 0);
    manager.CancelTimer(OwnerId);
    return;
  }

  auto ScheduleNext = [&](unsigned period)
  {
    TimerId = Bot.start_timer(
        [&](dpp::timer t)
        {
          Bot.stop_timer(t);
          SchedulePhase(manager);
        },
        period);
  };

  dpp::channel *channel = dpp::find_channel(ChannelId);
  if (!channel)
    return; // TODO: Handle this

  std::string msg;
  msg.reserve(1024);
  Flags ^= 1u;

  msg.append(fmt::format(
      "Session **{} {}** - Started !\n",
      !mFlagCmp(Flags, Break) ? "Work" : "Break",
      mFlagCmp(Flags, Break) ? CurrentSessionNumber - 1 : CurrentSessionNumber));
  for (auto const &id : MembersId)
    msg.append(fmt::format("<@{}>  ", (long)id));
  Bot.message_create(dpp::message(ChannelId, msg));

  switch (Flags & 1u)
  {
  case 0: // Starting work session
    if (mFlagCmp(Flags, Mute))
      ChangeMembersStatus(manager, 1);
    if (CurrentSessionNumber > 1 && mFlagCmp(Flags, Voice))
      PlayAudio(Bot, GuildId, ChannelId, BreakToWorkAudio.path, BreakToWorkAudio.duration);
    ScheduleNext(WorkPeriod);
    CurrentSessionNumber++;
    channel->set_name(fmt::format("{} - {}", "Work", VoiceChannelName));
    break;
  case 1: // Starting break session
    if (mFlagCmp(Flags, Mute))
      ChangeMembersStatus(manager, 0);
    if (mFlagCmp(Flags, Voice))
      PlayAudio(Bot, GuildId, ChannelId, WorkToBreakAudio.path, WorkToBreakAudio.duration);
    ScheduleNext(BreakPeriod);
    channel->set_name(fmt::format("{} - {}", "Break", VoiceChannelName));
    break;
  }
  manager.Bot.channel_edit(*channel);
}

void SMS::ChangeMembersStatus(SessionManager &manager, bool mute)
{
  dpp::guild_member GuildMember;
  GuildMember.guild_id = GuildId;
  GuildMember.set_mute(mute);
  for (auto const &id : MembersId)
  {
    GuildMember.user_id = id;
    manager.Bot.guild_edit_member(GuildMember);
  }
};

void SessionManager::StartTimer(
    snflake usr_id,
    dpp::channel *channel,
    unsigned work_period_in_min,
    unsigned break_period_in_min,
    unsigned repeat,
    flag_t flags,
    std::function<void(Session const &session)> call_back)
{

  std::vector<snflake> MembersIds;
  MembersIds.reserve(5);
  for (auto [UsrId, _] : channel->get_voice_members())
    MembersIds.push_back(UsrId);

  auto res = _active_sessions.emplace(
      usr_id,
      Session(
          usr_id,
          channel->id,
          channel->guild_id,
          std::move(MembersIds),
          work_period_in_min,
          break_period_in_min,
          repeat,
          channel->name,
          flags //
          ));
  if (call_back)
    call_back(res.first->second);
  res.first->second.SchedulePhase(*this);
}

bool SessionManager::CancelTimer(
    snflake owner_id, std::function<void(SessionManager::Session const &session)> call_before_remove)
{
  auto it = _active_sessions.find(owner_id);
  if (it == _active_sessions.end())
    return 0;

  Bot.stop_timer(it->second.TimerId);
  Bot.channel_edit(dpp::find_channel(it->second.ChannelId)->set_name(it->second.VoiceChannelName));

  if (call_before_remove)
    call_before_remove(it->second);

  _active_sessions.erase(it);

  return 1;
}

void SessionManager::CancelTimer(
    SMS *session, std::function<void(SessionManager::Session const &session)> call_before_remove)
{
  Bot.stop_timer(session->TimerId);
  session->ChangeMembersStatus(*this, 0);

  if (call_before_remove)
    call_before_remove(*session);

  _active_sessions.erase(session->OwnerId);
}
