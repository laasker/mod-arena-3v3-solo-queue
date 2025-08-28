/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "solo3v3.h"
#include "ArenaTeamMgr.h"
#include "BattlegroundMgr.h"
#include "Config.h"
#include "Log.h"
#include "ScriptMgr.h"
#include "Chat.h"
#include "DisableMgr.h"

uint32 ARENA_TYPE_3v3_SOLO = 4;
uint32 ARENA_TEAM_SOLO_3v3 = 4;
uint32 ARENA_SLOT_SOLO_3v3 = 4;
uint32 BATTLEGROUND_QUEUE_3v3_SOLO = 12;
BattlegroundQueueTypeId bgQueueTypeId = (BattlegroundQueueTypeId)12;

Solo3v3* Solo3v3::instance()
{
    static Solo3v3 instance;
    return &instance;
}

uint32 Solo3v3::GetAverageMMR(ArenaTeam* team)
{
    if (!team)
        return 0;

    // this could be improved with a better balanced calculation
    uint32 matchMakerRating = team->GetStats().Rating;

    return matchMakerRating;
}

void Solo3v3::CountAsLoss(Player* player, bool isInProgress)
{
    if (player->IsSpectator())
        return;

    ArenaTeam* plrArenaTeam = sArenaTeamMgr->GetArenaTeamById(player->GetArenaTeamId(ARENA_SLOT_SOLO_3v3));

    if (!plrArenaTeam)
        return;

    int32 ratingLoss = 0;

    // leave while arena is in progress
    if (isInProgress)
    {
        ratingLoss = sConfigMgr->GetOption<int32>("Solo.3v3.RatingPenalty.LeaveDuringMatch", 24);
    }
    // leave while arena is in preparation || don't accept queue || logout while invited
    else
    {
        ratingLoss = sConfigMgr->GetOption<int32>("Solo.3v3.RatingPenalty.LeaveBeforeMatchStart", 50);
    }

    ArenaTeamStats atStats = plrArenaTeam->GetStats();

    if (int32(atStats.Rating) - ratingLoss < 0)
        atStats.Rating = 0;
    else
        atStats.Rating -= ratingLoss;

    atStats.SeasonGames += 1;
    atStats.WeekGames += 1;
    atStats.Rank = 1;

    // Update team's rank, start with rank 1 and increase until no team with more rating was found
    ArenaTeamMgr::ArenaTeamContainer::const_iterator i = sArenaTeamMgr->GetArenaTeamMapBegin();
    for (; i != sArenaTeamMgr->GetArenaTeamMapEnd(); ++i) {
        if (i->second->GetType() == ARENA_TEAM_SOLO_3v3 && i->second->GetStats().Rating > atStats.Rating)
            ++atStats.Rank;
    }

    for (ArenaTeam::MemberList::iterator itr = plrArenaTeam->GetMembers().begin(); itr != plrArenaTeam->GetMembers().end(); ++itr) {
        if (itr->Guid == player->GetGUID()) {
            itr->WeekGames += 1;
            itr->SeasonGames += 1;
            itr->PersonalRating = atStats.Rating;

            if (int32(itr->MatchMakerRating) - ratingLoss < 0)
                itr->MatchMakerRating = 0;
            else
                itr->MatchMakerRating -= ratingLoss;

            break;
        }
    }

    plrArenaTeam->SetArenaTeamStats(atStats);
    plrArenaTeam->NotifyStatsChanged();
    plrArenaTeam->SaveToDB(true);
}

void Solo3v3::CleanUp3v3SoloQ(Battleground* bg)
{
    // Cleanup temp arena teams for solo 3v3
    if (bg->isArena() && bg->GetArenaType() == ARENA_TYPE_3v3_SOLO)
    {
        ArenaTeam* tempAlliArenaTeam = sArenaTeamMgr->GetArenaTeamById(bg->GetArenaTeamIdForTeam(TEAM_ALLIANCE));
        ArenaTeam* tempHordeArenaTeam = sArenaTeamMgr->GetArenaTeamById(bg->GetArenaTeamIdForTeam(TEAM_HORDE));

        if (tempAlliArenaTeam && tempAlliArenaTeam->GetId() >= MAX_ARENA_TEAM_ID)
        {
            sArenaTeamMgr->RemoveArenaTeam(tempAlliArenaTeam->GetId());
            delete tempAlliArenaTeam;
        }

        if (tempHordeArenaTeam && tempHordeArenaTeam->GetId() >= MAX_ARENA_TEAM_ID)
        {
            sArenaTeamMgr->RemoveArenaTeam(tempHordeArenaTeam->GetId());
            delete tempHordeArenaTeam;
        }
    }
}

void Solo3v3::CheckStartSolo3v3Arena(Battleground* bg)
{
    bool someoneNotInArena = false;
    uint32 PlayersInArena = 0;

    for (const auto& playerPair : bg->GetPlayers())
    {
        Player* player = playerPair.second;

        if (!player)
            continue;

        // prevent crash with Arena Replay module
        if (player->IsSpectator())
            return;

        PlayersInArena++;
    }

    uint32 AmountPlayersSolo3v3 = 6;
    if (PlayersInArena < AmountPlayersSolo3v3)
    {
        someoneNotInArena = true;
    }

    // if one player didn't enter arena and StopGameIncomplete is true, then end arena
    if (someoneNotInArena && sConfigMgr->GetOption<bool>("Solo.3v3.StopGameIncomplete", true))
    {
        bg->SetRated(false);
        bg->EndBattleground(TEAM_NEUTRAL);
    }
}

bool Solo3v3::CheckSolo3v3Arena(BattlegroundQueue* queue, BattlegroundBracketId bracket_id, bool isRated)
{
    queue->m_SelectionPools[TEAM_ALLIANCE].Init();
    queue->m_SelectionPools[TEAM_HORDE].Init();

    uint32 MinPlayersPerTeam = sBattlegroundMgr->isArenaTesting() ? 1 : 3;
    bool MeleeCasterHealer = sConfigMgr->GetOption<bool>("Solo.3v3.MeleeCasterHealer", false);

    uint8 factionGroupTypeAlliance = isRated ? BG_QUEUE_PREMADE_ALLIANCE : BG_QUEUE_NORMAL_ALLIANCE;
    uint8 factionGroupTypeHorde = isRated ? BG_QUEUE_PREMADE_HORDE : BG_QUEUE_NORMAL_HORDE;

    struct TeamComposition
    {
        int healerCount = 0, meleeCount = 0, rangedCount = 0;
        int getTotalPlayers() const { return healerCount + meleeCount + rangedCount; }
        int getTotalDPS() const { return meleeCount + rangedCount; }

        bool canAddPlayer(Solo3v3TalentCat role, bool MeleeCasterHealer) const
        {
            if (getTotalPlayers() >= 3) return false;

            if (MeleeCasterHealer)
                if (role == HEALER && healerCount >= 1) return false;
                if (role == MELEE && meleeCount >= 1) return false;
                if (role == RANGE && rangedCount >= 1) return false;
            else 
                if (role == HEALER && healerCount >= 1) return false;
                if (role != HEALER && getTotalDPS() >= 2) return false;

            return true;
        }

        void addPlayer(Solo3v3TalentCat role)
        {
            if (role == HEALER) healerCount++;
            else if (role == MELEE) meleeCount++;
            else if (role == RANGE) rangedCount++;
        }

        bool isValidComposition(bool MeleeCasterHealer) const
        {
            if (MeleeCasterHealer)
                return healerCount == 1 && meleeCount == 1 && rangedCount == 1;
            else
                return healerCount == 1 && getTotalDPS() == 2;
        }
    };

    TeamComposition teams[2]; // 0 = Alliance, 1 = Horde

    for (int teamId = 0; teamId < 2; teamId++) // BG_QUEUE_PREMADE_ALLIANCE and BG_QUEUE_PREMADE_HORDE
    {
        int index = teamId;

        if (!isRated)
            index += PVP_TEAMS_COUNT;

        for (BattlegroundQueue::GroupsQueueType::iterator itr = queue->m_QueuedGroups[bracket_id][index].begin(); itr != queue->m_QueuedGroups[bracket_id][index].end(); ++itr)
        {
            if ((*itr)->IsInvitedToBGInstanceGUID) // Skip when invited
                continue;

            for (auto const& playerGuid : (*itr)->Players)
            {
                Player* plr = ObjectAccessor::FindPlayer(playerGuid);
                if (!plr)
                    continue;

                Solo3v3TalentCat playerRole = GetTalentCatForSolo3v3(plr);

                int targetTeam = -1;

                bool allianceCanAdd = teams[TEAM_ALLIANCE].getTotalPlayers() < MinPlayersPerTeam && teams[TEAM_ALLIANCE].canAddPlayer(playerRole, MeleeCasterHealer);
                bool hordeCanAdd = teams[TEAM_HORDE].getTotalPlayers() < MinPlayersPerTeam && teams[TEAM_HORDE].canAddPlayer(playerRole, MeleeCasterHealer);

                if (allianceCanAdd && hordeCanAdd)
                    targetTeam = urand(0, 1) == 0 ? TEAM_ALLIANCE : TEAM_HORDE; // add players to random team
                    // balance teams (se tiver 2v1, vai ficar 2v2, se tiver 1v1, 50% de chance de ir pra qualquer time)
                    /*int allianceCount = teams[TEAM_ALLIANCE].getTotalPlayers();
                    int hordeCount = teams[TEAM_HORDE].getTotalPlayers();
                    if (allianceCount == hordeCount) // 1v1, 2v2
                    {
                        // Teams are balanced, pure random choice
                        targetTeam = urand(0, 1) == 0 ? TEAM_ALLIANCE : TEAM_HORDE;
                    }
                    else // 0v1, 1v2, 3v2
                    {
                        // Teams are unbalanced, consider balance priority
                        int smallerTeam = (allianceCount < hordeCount) ? TEAM_ALLIANCE : TEAM_HORDE;

                        if (urand(0, 100) < 50) // 50% de chance
                            targetTeam = smallerTeam;
                        else
                            targetTeam = urand(0, 1) == 0 ? TEAM_ALLIANCE : TEAM_HORDE;
                    }*/
                //else if (allianceCanAdd)
                //    targetTeam = TEAM_ALLIANCE;
                //else if (hordeCanAdd)
                //    targetTeam = TEAM_HORDE;
                else
                    targetTeam = allianceCanAdd ? TEAM_ALLIANCE : TEAM_HORDE; // se um team tiver cheio, adiciona no outro (se possivel)

                if (targetTeam == -1)
                    continue;

                if (targetTeam == TEAM_ALLIANCE)
                {
                    if (queue->m_SelectionPools[TEAM_ALLIANCE].AddGroup((*itr), MinPlayersPerTeam)) // added successfully?
                    {
                        teams[TEAM_ALLIANCE].addPlayer(playerRole);

                        if ((*itr)->teamId != TEAM_ALLIANCE) // move to other team
                        {
                            (*itr)->teamId = TEAM_ALLIANCE;
                            (*itr)->GroupType = factionGroupTypeAlliance;
                            queue->m_QueuedGroups[bracket_id][factionGroupTypeAlliance].push_front((*itr));
                            itr = queue->m_QueuedGroups[bracket_id][factionGroupTypeHorde].erase(itr);
                            return CheckSolo3v3Arena(queue, bracket_id, isRated);
                        }
                    }
                }
                else // targetTeam == TEAM_HORDE
                {
                    if (queue->m_SelectionPools[TEAM_HORDE].AddGroup((*itr), MinPlayersPerTeam))
                    {
                        teams[TEAM_HORDE].addPlayer(playerRole);

                        if ((*itr)->teamId != TEAM_HORDE) // move to other team
                        {
                            (*itr)->teamId = TEAM_HORDE;
                            (*itr)->GroupType = factionGroupTypeHorde;
                            queue->m_QueuedGroups[bracket_id][factionGroupTypeHorde].push_front((*itr));
                            itr = queue->m_QueuedGroups[bracket_id][factionGroupTypeAlliance].erase(itr);
                            return CheckSolo3v3Arena(queue, bracket_id, isRated);
                        }
                    }
                }
            }
        }
    }

    return teams[TEAM_ALLIANCE].isValidComposition(MeleeCasterHealer) && teams[TEAM_HORDE].isValidComposition(MeleeCasterHealer);
}

void Solo3v3::CreateTempArenaTeamForQueue(BattlegroundQueue* queue, ArenaTeam* arenaTeams[])
{
    // Create temp arena team
    for (uint32 i = 0; i < BG_TEAMS_COUNT; i++)
    {
        ArenaTeam* tempArenaTeam = new ArenaTeam();  // delete it when all players have left the arena match. Stored in sArenaTeamMgr
        std::vector<Player*> playersList;
        uint32 atPlrItr = 0;

        for (auto const& itr : queue->m_SelectionPools[TEAM_ALLIANCE + i].SelectedGroups)
        {
            if (atPlrItr >= 3)
                break; // Should never happen

            for (auto const& itr2 : itr->Players)
            {
                auto _PlayerGuid = itr2;
                if (Player * _player = ObjectAccessor::FindPlayer(_PlayerGuid))
                {
                    playersList.push_back(_player);
                    atPlrItr++;
                }

                break;
            }
        }

        std::stringstream ssTeamName;
        ssTeamName << "Solo Team - " << (i + 1);

        tempArenaTeam->CreateTempArenaTeam(playersList, ARENA_TEAM_SOLO_3v3, ssTeamName.str());
        sArenaTeamMgr->AddArenaTeam(tempArenaTeam);
        arenaTeams[i] = tempArenaTeam;
    }
}

bool Solo3v3::Arena3v3CheckTalents(Player* player)
{
    if (!player)
        return false;

    if (!sConfigMgr->GetOption<bool>("Arena.3v3.BlockForbiddenTalents", false))
        return true;

    uint32 count = 0;
    for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

        if (!talentInfo)
            continue;

        for (int8 rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
        {
            if (talentInfo->RankID[rank] == 0)
                continue;

            if (player->HasTalent(talentInfo->RankID[rank], player->GetActiveSpec()))
            {
                for (int8 i = 0; FORBIDDEN_TALENTS_IN_1V1_ARENA[i] != 0; i++)
                    if (FORBIDDEN_TALENTS_IN_1V1_ARENA[i] == talentInfo->TalentTab)
                        count += rank + 1;
            }
        }
    }

    if (count >= 36)
    {
        ChatHandler(player->GetSession()).SendSysMessage("You can't join, because you have invested to much points in a forbidden talent. Please edit your talents.");
        return false;
    }

    return true;
}

Solo3v3TalentCat Solo3v3::GetTalentCatForSolo3v3(Player* player)
{
    uint32 count[MAX_TALENT_CAT];

    for (int i = 0; i < MAX_TALENT_CAT; i++)
        count[i] = 0;

    for (uint32 talentId = 0; talentId < sTalentStore.GetNumRows(); ++talentId)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

        if (!talentInfo)
            continue;

        for (int8 rank = MAX_TALENT_RANK - 1; rank >= 0; --rank)
        {
            if (talentInfo->RankID[rank] == 0)
                continue;

            if (player->HasTalent(talentInfo->RankID[rank], player->GetActiveSpec()))
            {
                for (int8 i = 0; SOLO_3V3_TALENTS_MELEE[i] != 0; i++)
                    if (SOLO_3V3_TALENTS_MELEE[i] == talentInfo->TalentTab)
                        count[MELEE] += rank + 1;

                for (int8 i = 0; SOLO_3V3_TALENTS_RANGE[i] != 0; i++)
                    if (SOLO_3V3_TALENTS_RANGE[i] == talentInfo->TalentTab)
                        count[RANGE] += rank + 1;

                for (int8 i = 0; SOLO_3V3_TALENTS_HEAL[i] != 0; i++)
                    if (SOLO_3V3_TALENTS_HEAL[i] == talentInfo->TalentTab)
                        count[HEALER] += rank + 1;
            }
        }
    }

    uint32 prevCount = 0;

    Solo3v3TalentCat talCat = MELEE; // Default MELEE (if no talent points set)

    for (int i = 0; i < MAX_TALENT_CAT; i++)
    {
        if (count[i] > prevCount)
        {
            talCat = (Solo3v3TalentCat)i;
            prevCount = count[i];
        }
    }

    return talCat;
}
