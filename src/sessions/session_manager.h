#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H
#include <chrono>
#include <cstddef>
#include <dpp/channel.h>
#include <dpp/cluster.h>
#include <dpp/snowflake.h>
#include <dpp/timer.h>
#include <functional>
#include <unordered_map>
#include <vector>
using flag_t = uint8_t;

struct Audio
{
  const char *path;
  const uint32_t &&duration;
};

constexpr const Audio BreakToWorkAudio{"assests/audio/BreakToWork.opus", 6};
constexpr const Audio WorkToBreakAudio{"assests/audio/WorkToBreak.opus", 4};

// constexpr const Audio QadTasama { };

class SessionManager
{
  using snflake = dpp::snowflake;

public:
  struct Session
  {
    enum class Flag : flag_t
    {
      Break = 1u << 0, // So if bit-0 was 1 in the flags then it's a break session
      Mute = 1u << 1,  // So if the bit-1 was 1 in the flags then mute is on ( 0-off )
      Voice = 1u << 2
    };
    snflake OwnerId;
    snflake ChannelId;
    snflake GuildId;

    std::vector<snflake> MembersId;
    dpp::timer TimerId;
    std::chrono::steady_clock::time_point PhaseStartTime;

    unsigned WorkPeriod;
    unsigned BreakPeriod;
    unsigned Repeat;
    unsigned CurrentSessionNumber;

    std::string VoiceChannelName;
    // 1-byte
    flag_t Flags; // bit-0 for current phase , bit-1 for mute flag
    Session(
        snflake usr_id,
        snflake channel_id,
        snflake guild_id,
        std::vector<snflake> &&members_ids,
        unsigned work_period,
        unsigned break_period,
        unsigned repeat,
        std::string_view vc_channel_name,
        flag_t flags = 1u << 0 //
    );
    void SchedulePhase(SessionManager &manager) noexcept;
    long GetRemainingTime() noexcept;

    void ChangeMembersStatus(SessionManager &manager, bool mute) noexcept;
  };

  explicit SessionManager(dpp::cluster &bot) noexcept;

  Session *GetSessionByOwnerId(snflake owner_id) noexcept;
  Session const *GetSessionByOwnerId(snflake owner_id) const noexcept;
  Session *GetSessionByUserId(snflake usr_id);

  Session const *GetSessionByUserId(snflake usr_id) const noexcept;
  void StartSession(
      snflake usr_id,
      dpp::channel *channel,
      unsigned work_period_in_min,
      unsigned break_period_in_min,
      unsigned repeat,
      flag_t flags = 1u << 0,
      std::function<void(Session const &session)> call_back = nullptr);
  /*
     @brief Cancel the session associated with the given owner_id and remove it from the active sessions
     @param owner_id the snowflake id of the session owner.
     @param call_before_remove optional callback function that will be called before session is removed
     from the active sessions, it must take a single parameter of type Session const& , any return value is ignored so
     return void.
     @return true if the session was found and canceled successfully, false if no session with the given owner_id was
     found.
  */
  template <class F = std::nullptr_t> //
  bool CancelSession(snflake owner_id, F &&call_before_remove = nullptr) noexcept;
  /*
     @brief Cancel the session associated with the given session pointer.
     @param session pointer to the session to be canceled.
     @param call_before_remove optional callback function that will be called before session is removed
     from the active sessions, it must take a single parameter of type Session const& , any return value is ignored so
     return void.

     @param erase if set to true, the session will be removed from the active sessions after calling the callback
     function; if set to false, the session will not be remove ,please use this with caution as it can lead to memory
     leaks if the session is not removed later or double erase if erase == 1 and the caller also erases the session
     after calling this function.
  */
  template <class F = std::nullptr_t> //
  void CancelSession(Session *session, F &&call_before_remove = nullptr, bool erase = 1) noexcept;

  dpp::cluster &Bot;

  /*
     @brief return the number of active sessions
     @return uint32_t that represent the number of active sessions
  */
  uint32_t GetActiveSessions() noexcept
  {
    return _active_sessions.size();
  }

  /*
     @brief Changes the owner_id for the session and the hash map key
     @param owner_id the current owner_id of the session
     @param new_owner_id the new owner_id to be set for the session
     @return true if the owner_id was changed successfully, false if the session was not found
   */
  bool ChangeOwnerId(dpp::snowflake owner_id, dpp::snowflake new_owner_id) noexcept;

  /*
     @brief Changes the owner_id for the session and the hash map key
     @param session pointer to the session to change its owner_id
     @param new_owner_id the new owner_id to be set for the session
  */
  void ChangeOwnerId(Session *session, dpp::snowflake new_owner_id) noexcept;

  // Static methods

  static inline void SetFlag(flag_t &flags, Session::Flag type, bool mode)
  {
    if (mode)
      flags |= static_cast<flag_t>(type); // set bit
    else
      flags &= ~static_cast<flag_t>(type); // clear bit
  }

  static bool HasFlag(flag_t flags, Session::Flag type)
  {
    return flags & static_cast<flag_t>(type);
  }

private:
  std::unordered_map<snflake, Session> _active_sessions;
};

template <class F> //
bool SessionManager::CancelSession(snflake owner_id, F &&call_before_remove) noexcept
{
  auto it = _active_sessions.find(owner_id);
  if (it == _active_sessions.end())
    return 0;

  CancelSession(&it->second, call_before_remove, 0);

  _active_sessions.erase(it);

  return 1;
}

template <class F> //
void SessionManager::CancelSession(SessionManager::Session *session, F &&call_before_remove, bool erase) noexcept
{
  Bot.stop_timer(session->TimerId);
  if (HasFlag(session->Flags, Session::Flag::Mute))
    session->ChangeMembersStatus(*this, 0);
  // Bot.channel_edit(dpp::find_channel(it->second.ChannelId)->set_name(it->second.VoiceChannelName));
  if constexpr (!std::is_same_v<std::decay_t<F>, std::nullptr_t>)
  {
    std::forward<F>(call_before_remove)(*session);
  }

  if (erase)
    _active_sessions.erase(session->OwnerId);
}

#endif
