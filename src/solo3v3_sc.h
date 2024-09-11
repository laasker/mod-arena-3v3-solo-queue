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

#include "ScriptMgr.h"
#include "ArenaTeamMgr.h"
#include "BattlegroundMgr.h"
#include "DisableMgr.h"
#include "Chat.h"
#include "Language.h"
#include "ScriptedGossip.h"
#include "Config.h"
#include "Battleground.h"
#include "solo3v3.h"

class NpcSolo3v3 : public CreatureScript
{
public:
    NpcSolo3v3() : CreatureScript("npc_solo3v3")
    {
        Initialize();
    }

    void Initialize();
    bool OnGossipHello(Player* player, Creature* creature) override;
    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override;

private:
    bool ArenaCheckFullEquipAndTalents(Player* player);
    bool JoinQueueArena(Player* player, Creature* creature, bool isRated);
    bool CreateArenateam(Player* player, Creature* creature);
    void fetchQueueList();

    int cache3v3Queue[MAX_TALENT_CAT];
    uint32 lastFetchQueueList;
};

class Solo3v3BG : public AllBattlegroundScript
{
public:
    Solo3v3BG() : AllBattlegroundScript("Solo3v3_BG") {}

    void OnQueueUpdate(BattlegroundQueue* queue, uint32 /*diff*/, BattlegroundTypeId bgTypeId, BattlegroundBracketId bracket_id, uint8 arenaType, bool isRated, uint32 /*arenaRatedTeamId*/) override;
    bool OnQueueUpdateValidity(BattlegroundQueue* /* queue */, uint32 /*diff*/, BattlegroundTypeId /* bgTypeId */, BattlegroundBracketId /* bracket_id */, uint8 arenaType, bool /* isRated */, uint32 /*arenaRatedTeamId*/) override;
    void OnBattlegroundDestroy(Battleground* bg) override;
    void SaveSoloTeam(Battleground* bg, Player* player, TeamId winnerTeamId);
    void SaveSoloTeamOnLeave(Battleground* bg, Player* player);
    void OnBattlegroundEndReward(Battleground* bg, Player* player, TeamId /* winnerTeamId */) override;
    void OnArenaRemovePlayerAtLeave(Battleground* bg, Player* player) override;
};

class ConfigLoader3v3Arena : public WorldScript
{
public:
    ConfigLoader3v3Arena() : WorldScript("config_loader_3v3_arena") {}

    virtual void OnAfterConfigLoad(bool /*Reload*/) override;
};

class Team3v3arena : public ArenaTeamScript
{
public:
    Team3v3arena() : ArenaTeamScript("team_3v3_arena") {}

    void OnGetSlotByType(const uint32 type, uint8& slot) override;
    void OnGetArenaPoints(ArenaTeam* at, float& points) override;
    void OnTypeIDToQueueID(const BattlegroundTypeId, const uint8 arenaType, uint32& _bgQueueTypeId) override;
    void OnQueueIdToArenaType(const BattlegroundQueueTypeId _bgQueueTypeId, uint8& arenaType) override;
};

class PlayerScript3v3Arena : public PlayerScript
{
public:
    PlayerScript3v3Arena() : PlayerScript("player_script_3v3_arena") {}

    void OnLogin(Player* pPlayer) override;
    void GetCustomGetArenaTeamId(const Player* player, uint8 slot, uint32& id) const override;
    void GetCustomArenaPersonalRating(const Player* player, uint8 slot, uint32& rating) const override;
    void OnGetMaxPersonalArenaRatingRequirement(const Player* player, uint32 minslot, uint32& maxArenaRating) const override;
    void OnGetArenaTeamId(Player* player, uint8 slot, uint32& result) override;
    bool NotSetArenaTeamInfoField(Player* player, uint8 slot, ArenaTeamInfoType type, uint32 value) override;
    bool CanBattleFieldPort(Player* player, uint8 arenaType, BattlegroundTypeId BGTypeID, uint8 action) override;
    void OnBattlegroundDesertion(Player* player, const BattlegroundDesertionType type);
};

<<<<<<< Updated upstream
=======
class Arena_SC : public ArenaScript
{
public:
    Arena_SC() : ArenaScript("Arena_SC") { }

    bool CanAddMember(ArenaTeam* team, ObjectGuid /* playerGuid */) override
    {
        if (!team)
            return false;

        if (!team->GetMembersSize())
            return true;

        if (/* team->GetType() == ARENA_TEAM_1v1 || */ team->GetType() == ARENA_TEAM_SOLO_3v3)
            return false;

        return true;
    }

    void OnGetPoints(ArenaTeam* team, uint32 /* memberRating */, float& points) override
    {
        if (!team)
            return;

        // if (team->GetType() == ARENA_TEAM_1v1)
        //     points *= sConfigMgr->GetOption<float>("Azth.Rate.Arena1v1", 0.40f);

        if (team->GetType() == ARENA_TEAM_SOLO_3v3)
            points *= sConfigMgr->GetOption<float>("Solo.3v3.ArenaPointsMulti", 0.88f);
    }

    bool CanSaveToDB(ArenaTeam* team) override
    {
        if (!team)
            return false;

        if (team->GetId() >= MAX_ARENA_TEAM_ID)
        {
            //return false;

            for (auto const& itr : team->GetMembers())
            {
                ArenaTeam* plrArenaTeam = nullptr;
                // Find real arena team for player
                for (auto const& itrMgr : sArenaTeamMgr->GetArenaTeams())
                {
                    if (itrMgr.first < MAX_ARENA_TEAM_ID && itrMgr.second->GetCaptain() == itr.Guid)// && itrMgr.second->GetType() == ARENA_TEAM_SOLO_3v3)
                    {
                        plrArenaTeam = itrMgr.second; // found!
                        LOG_ERROR("solo3v3", "SaveSoloDB - Found team for player, Team ID: {}", plrArenaTeam->GetId());
                        break;
                    }
                    else
                    {
                        //LOG_ERROR("solo3v3", "SaveSoloDB - Didnt find team for player GUID: {}", itr.Guid.ToString());
                    }
                }
            }

        }
        else
        {
            LOG_ERROR("solo3v3", "Solo3v3 CanSaveToDB found team ID: {}", team->GetId());
        }

        return true;
    }

    void OnArenaStart(Battleground* bg) override
    {
        // already has a check for arenatype != ARENA_TYPE_3v3_SOLO)

        // if missing one player after ArenaStart, closes arena
        sSolo->CheckStartSolo3v3Arena(bg);
    }
};

using namespace Acore::ChatCommands;

class CommandJoinSolo : public CommandScript
{
public:
    CommandJoinSolo() : CommandScript("CommandJoinSolo") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable SoloCommandTable =
        {
            { "qsolo",     HandleQueueSoloArena,         SEC_PLAYER,        Console::No }, // command .qsolo
            { "qsolotest", HandleQueueSoloArenaTesting,  SEC_ADMINISTRATOR, Console::No }  // command .qsolotest
        };

        return SoloCommandTable;
    }

    static bool HandleQueueSoloArena(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!sConfigMgr->GetOption<bool>("Solo.3v3.EnableCommand", true))
        {
            ChatHandler(player->GetSession()).SendSysMessage("Solo 3v3 Arena command is disabled.");
                return false;
        }
        if (!sConfigMgr->GetOption<bool>("Solo.3v3.Enable", true))
        {
            ChatHandler(player->GetSession()).SendSysMessage("Solo 3v3 Arena is disabled.");
                return false;
        }
        if (player->IsInCombat())
        {
            ChatHandler(player->GetSession()).SendSysMessage("Can't be in combat.");
               return false;
        }
        NpcSolo3v3 SoloCommand;
        if (player->HasAura(26013) && (sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnAfk", true) || sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnLeave", true)))
        {
            WorldPacket data;
            sBattlegroundMgr->BuildGroupJoinedBattlegroundPacket(&data, ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
            player->GetSession()->SendPacket(&data);
                return false;
        }
        uint32 minLevel = sConfigMgr->GetOption<uint32>("Solo.3v3.MinLevel", 80);
        if (player->GetLevel() < minLevel)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("You need level {}+ to join solo arena.", minLevel);
                return false;
        }
        if (!player->GetArenaTeamId(ARENA_SLOT_SOLO_3v3))
        {
            // create solo3v3 team if player don't have it
            if (!SoloCommand.CreateArenateam(player, nullptr))
            {
                return false;
            }
        }
        else
        {
            if (!SoloCommand.ArenaCheckFullEquipAndTalents(player))
            {
                return false;
            }
            if (SoloCommand.JoinQueueArena(player, nullptr, true))
            {
                handler->PSendSysMessage("You have joined the solo 3v3 arena queue.");
            }
        }

        return true;
    }

    // USED IN TESTING ONLY!!! (time saving when alt tabbing) will join solo 3v3 on all players!!
    // also use macros: /run AcceptBattlefieldPort(1,1); to accept queue and /afk to leave arena
    static bool HandleQueueSoloArenaTesting(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!sConfigMgr->GetOption<bool>("Solo.3v3.EnableTestingCommand", true))
        {
            ChatHandler(player->GetSession()).SendSysMessage("Solo 3v3 Arena testing command is disabled.");
                return false;
        }
        if (!sConfigMgr->GetOption<bool>("Solo.3v3.Enable", true))
        {
            ChatHandler(player->GetSession()).SendSysMessage("Solo 3v3 Arena is disabled.");
                return false;
        }
        NpcSolo3v3 SoloCommand;
        for (auto& pair : ObjectAccessor::GetPlayers())
        {
            Player* currentPlayer = pair.second;
            if (currentPlayer)
            {
                if (currentPlayer->IsInCombat())
                {
                    handler->PSendSysMessage("Player {} can't be in combat.", currentPlayer->GetName().c_str());
                        continue;
                }
                if (currentPlayer->HasAura(26013) && (sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnAfk", true) || sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnLeave", true)))
                {
                    WorldPacket data;
                    sBattlegroundMgr->BuildGroupJoinedBattlegroundPacket(&data, ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
                    currentPlayer->GetSession()->SendPacket(&data);
                        continue;
                }
                uint32 minLevel = sConfigMgr->GetOption<uint32>("Solo.3v3.MinLevel", 80);
                if (currentPlayer->GetLevel() < minLevel)
                {
                    handler->PSendSysMessage("Player {} needs level {}+ to join solo arena.", player->GetName().c_str(), minLevel);
                        continue;
                }
                if (!currentPlayer->GetArenaTeamId(ARENA_SLOT_SOLO_3v3)) // ARENA_SLOT_SOLO_3v3 | ARENA_TEAM_SOLO_3v3
                {
                    if (!SoloCommand.CreateArenateam(currentPlayer, nullptr))
                    {
                        continue;
                    }
                }
                else
                {
                    if (!SoloCommand.ArenaCheckFullEquipAndTalents(currentPlayer))
                    {
                        continue;
                    }
                    if (SoloCommand.JoinQueueArena(currentPlayer, nullptr, true))
                    {
                        handler->PSendSysMessage("Player {} has joined the solo 3v3 arena queue.", currentPlayer->GetName().c_str());
                    }
                    else
                    {
                        handler->PSendSysMessage("Failed to join queue for player {}.", currentPlayer->GetName().c_str());
                    }
                }
            }
        }

        return true;
    }
};

>>>>>>> Stashed changes
void AddSC_Solo_3v3_Arena()
{
    if (!ArenaTeam::ArenaSlotByType.count(ARENA_TEAM_SOLO_3v3))
        ArenaTeam::ArenaSlotByType[ARENA_TEAM_SOLO_3v3] = ARENA_SLOT_SOLO_3v3;

    if (!ArenaTeam::ArenaReqPlayersForType.count(ARENA_TYPE_3v3_SOLO))
        ArenaTeam::ArenaReqPlayersForType[ARENA_TYPE_3v3_SOLO] = 6;

    if (!BattlegroundMgr::queueToBg.count(BATTLEGROUND_QUEUE_3v3_SOLO))
        BattlegroundMgr::queueToBg[BATTLEGROUND_QUEUE_3v3_SOLO] = BATTLEGROUND_AA;

    if (!BattlegroundMgr::ArenaTypeToQueue.count(ARENA_TYPE_3v3_SOLO))
        BattlegroundMgr::ArenaTypeToQueue[ARENA_TYPE_3v3_SOLO] = (BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO;

    if (!BattlegroundMgr::QueueToArenaType.count(BATTLEGROUND_QUEUE_3v3_SOLO))
        BattlegroundMgr::QueueToArenaType[BATTLEGROUND_QUEUE_3v3_SOLO] = (ArenaType)ARENA_TYPE_3v3_SOLO;

    new NpcSolo3v3();
    new Solo3v3BG();
    new Team3v3arena();
    new ConfigLoader3v3Arena();
    new PlayerScript3v3Arena();
}
