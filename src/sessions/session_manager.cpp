#include "session_manager.h"
#include "utils.h"
#include "voice.h"
#include <chrono>
#include <dpp/cluster.h>
#include <dpp/guild.h>
#include <dpp/message.h>
#include <dpp/misc-enum.h>
#include <dpp/timer.h>
#include <fmt/format.h>
#include <functional>
#include <string>
using SMS = SessionManager::Session;

// Helper functions

void ChangeMembersStatus(SessionManager &mgr, SessionManager::Session *session, bool mute)
{
  dpp::guild_member GuildMember;
  GuildMember.guild_id = session->GuildId;
  GuildMember.set_mute(mute);
  for (auto const &id : session->MembersId)
  {
    GuildMember.user_id = id;
    mgr.Bot.guild_edit_member(GuildMember);
  }
};

//----------------------

// Constructors
SessionManager::SessionManager(dpp::cluster &bot) : Bot(bot)
{
  Bot.log(dpp::loglevel::ll_info, "Session manager init");
}

SMS::Session(
    snflake usr_id,
    snflake channel_id,
    snflake guild_id,
    std::vector<snflake> &&members_ids,
    unsigned work_period,
    unsigned break_period,
    unsigned repeat,
    uint8_t flags)
    : OwnerId(usr_id), ChannelId(channel_id), GuildId(guild_id), MembersId(std::move(members_ids)),
      WorkPeriod(work_period), BreakPeriod(break_period), Repeat(repeat + 1), CurrentSessionNo(1), Flags(flags)
{
  Flags |= 1u << 0; // Set starting phase to break to start togggling correctly
}

//-------------

// Getters

//// Session Manager
SMS *SessionManager::GetSessionByOwnerId(snflake owner_id)
{
  auto it = ActiveSessions.find(owner_id);
  return it == ActiveSessions.end() ? nullptr : &it->second;
}

SMS const *SessionManager::GetSessionByOwnerId(snflake owner_id) const
{
  auto it = ActiveSessions.find(owner_id);
  return it == ActiveSessions.end() ? nullptr : &it->second;
}

SMS *SessionManager::GetSessionByUserId(snflake usr_id)
{
  for (auto &[_, Session] : ActiveSessions)
    for (auto id : Session.MembersId)
      if (usr_id == id)
        return &Session;

  return nullptr;
}

SMS const *SessionManager::GetSessionByUserId(snflake usr_id) const
{
  for (auto &[_, Session] : ActiveSessions)
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

//------------

void SMS::SchedulePhase(SessionManager &manager)
{

  PhaseStartTime = std::chrono::steady_clock::now();
  auto &Bot = manager.Bot;
  if (CurrentSessionNo >= Repeat)
  {
    Bot.message_create(dpp::message(ChannelId, "Pomodoro session finished!"));
    ChangeMembersStatus(manager, this, 0);
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

  std::string msg;
  msg.reserve(1024);
  Flags ^= 1u;

  msg.append(fmt::format(
      "Session **{} {}** - Started !\n",
      !mFlagCmp(Flags, Break) ? "Work" : "Break",
      mFlagCmp(Flags, Break) ? CurrentSessionNo - 1 : CurrentSessionNo));
  for (auto const &id : MembersId)
    msg.append(fmt::format("<@{}>  ", (long)id));
  Bot.message_create(dpp::message(ChannelId, msg));

  switch (Flags & 1u)
  {
  case 0: // Starting work session
    if (mFlagCmp(Flags, Mute))
      ChangeMembersStatus(manager, this, 1);
    if (CurrentSessionNo > 1 && mFlagCmp(Flags, Voice))
      PlayAudio(Bot, GuildId, ChannelId, BreakToWorkAudio.path, BreakToWorkAudio.duration);
    ScheduleNext(WorkPeriod);
    CurrentSessionNo++;
    break;
  case 1: // Starting break session
    if (mFlagCmp(Flags, Mute))
      ChangeMembersStatus(manager, this, 0);
    if (mFlagCmp(Flags, Voice))
      PlayAudio(Bot, GuildId, ChannelId, WorkToBreakAudio.path, WorkToBreakAudio.duration);
    ScheduleNext(BreakPeriod);
    break;
  }
}

void SessionManager::StartTimer(
    snflake usr_id,
    snflake channel_id,
    dpp::guild *guild,
    unsigned work_period_in_min,
    unsigned break_period_in_min,
    unsigned repeat,
    uint8_t flags,
    std::function<void(Session const &session)> call_back)
{

  std::vector<snflake> MembersIds;
  MembersIds.reserve(5);
  for (auto const &[usr, vc] : guild->voice_members)
    if (vc.channel_id == channel_id)
      MembersIds.push_back(usr);

  auto res = ActiveSessions.emplace(
      usr_id,
      Session(
          usr_id,
          channel_id,
          guild->id,
          std::move(MembersIds),
          work_period_in_min,
          break_period_in_min,
          repeat,
          flags //
          ));
  if (call_back)
    call_back(res.first->second);
  res.first->second.SchedulePhase(*this);
}

bool SessionManager::CancelTimer(
    snflake owner_id, std::function<void(SessionManager::Session const &session)> call_before_remove)
{
  auto it = ActiveSessions.find(owner_id);
  if (it == ActiveSessions.end())
    return 0;

  Bot.stop_timer(it->second.TimerId);

  if (call_before_remove)
    call_before_remove(it->second);

  ActiveSessions.erase(it);

  return 1;
}

void SessionManager::CancelTimer(
    SMS *session, std::function<void(SessionManager::Session const &session)> call_before_remove)
{
  Bot.stop_timer(session->TimerId);
  ChangeMembersStatus(*this, session, 0);

  if (call_before_remove)
    call_before_remove(*session);

  ActiveSessions.erase(session->OwnerId);
}
