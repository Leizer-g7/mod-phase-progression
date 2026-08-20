#include "TravelPhaseAdapter.h"

#include "PhaseMgr.h"

#include "AreaDefines.h"
#include "Chat.h"
#include "Log.h"
#include "MapMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Transport.h"

#include <cstdint>

namespace
{
    constexpr std::uint32_t MAP_OUTLAND_ID = 530;
    constexpr std::uint32_t MAP_NORTHREND_ID = 571;

    enum class TravelRestriction
    {
        None,
        OutlandPhase,
        OutlandLevel,
        NorthrendPhase,
        NorthrendLevel
    };

    bool IsTbcStarterZone(std::uint32_t zoneId)
    {
        switch (zoneId)
        {
            case AREA_EVERSONG_WOODS:
            case AREA_GHOSTLANDS:
            case AREA_SILVERMOON_CITY:
            case AREA_AZUREMYST_ISLE:
            case AREA_BLOODMYST_ISLE:
            case AREA_THE_EXODAR:
                return true;

            default:
                return false;
        }
    }

    bool IsBypassed(Player* player)
    {
        return !player ||
               !sPhaseMgr.IsEnabled() ||
               (sPhaseMgr.IsGMBypassEnabled() &&
                player->IsGameMaster());
    }

    TravelRestriction GetRestriction(
        Player* player,
        std::uint32_t mapId,
        float x,
        float y,
        float z)
    {
        if (!player)
            return TravelRestriction::None;

        PhaseDefinition const& phase =
            sPhaseMgr.GetActiveDefinition();

        /*
         * NORTHREND
         *
         * 1. Debe estar abierto por fase.
         * 2. El personaje debe ser nivel 68+.
         */
        if (mapId == MAP_NORTHREND_ID)
        {
            if (!phase.northrendEnabled)
                return TravelRestriction::NorthrendPhase;

            if (player->GetLevel() < 68)
                return TravelRestriction::NorthrendLevel;

            return TravelRestriction::None;
        }

        /*
         * OUTLAND / MAP 530
         *
         * Las zonas iniciales de Blood Elf y Draenei
         * siempre permanecen accesibles.
         */
        if (mapId == MAP_OUTLAND_ID)
        {
            std::uint32_t zoneId =
                sMapMgr->GetZoneId(
                    player->GetPhaseMask(),
                    mapId,
                    x,
                    y,
                    z);

            if (IsTbcStarterZone(zoneId))
                return TravelRestriction::None;

            if (!phase.outlandEnabled)
                return TravelRestriction::OutlandPhase;

            if (player->GetLevel() < 58)
                return TravelRestriction::OutlandLevel;
        }

        return TravelRestriction::None;
    }

    char const* GetRestrictionMessage(
        TravelRestriction restriction)
    {
        switch (restriction)
        {
            case TravelRestriction::OutlandPhase:
                return
                    "Terrallende aun no esta disponible "
                    "en la fase actual del servidor.";

            case TravelRestriction::OutlandLevel:
                return
                    "Necesitas ser al menos nivel 58 "
                    "para viajar a Terrallende.";

            case TravelRestriction::NorthrendPhase:
                return
                    "Rasganorte aun no esta disponible "
                    "en la fase actual del servidor.";

            case TravelRestriction::NorthrendLevel:
                return
                    "Necesitas ser al menos nivel 68 "
                    "para viajar a Rasganorte.";

            case TravelRestriction::None:
            default:
                return "";
        }
    }

    void SendBlockedMessage(
        Player* player,
        char const* message)
    {
        if (!player || !player->GetSession())
            return;

        ChatHandler handler(player->GetSession());
        handler.SendNotification(message);
    }

    void SafelyRemoveFromTransport(Player* player)
    {
        if (!player)
            return;

        Transport* transport = player->GetTransport();

        if (!transport)
            return;

        transport->RemovePassenger(
            player,
            true);

        player->RepopAtGraveyard();
    }

    bool BlockTravel(
        Player* player,
        TravelRestriction restriction)
    {
        if (player && player->GetTransport())
            SafelyRemoveFromTransport(player);

        SendBlockedMessage(
            player,
            GetRestrictionMessage(restriction));

        return false;
    }
}

namespace TravelPhaseAdapter
{
    bool CanTeleport(
        Player* player,
        std::uint32_t mapId,
        float x,
        float y,
        float z)
    {
        if (IsBypassed(player))
            return true;

        TravelRestriction restriction =
            GetRestriction(
                player,
                mapId,
                x,
                y,
                z);

        if (restriction == TravelRestriction::None)
            return true;

        return BlockTravel(
            player,
            restriction);
    }

    bool EnsureValidLocation(Player* player)
    {
        if (IsBypassed(player))
            return true;

        TravelRestriction restriction =
            GetRestriction(
                player,
                player->GetMapId(),
                player->GetPositionX(),
                player->GetPositionY(),
                player->GetPositionZ());

        if (restriction == TravelRestriction::None)
            return true;

        /*
         * Usamos la ubicación inicial real de la raza/clase
         * como destino seguro.
         *
         * Esto evita hardcodear Stormwind/Orgrimmar y además
         * mantiene correctamente Blood Elf y Draenei.
         */
        PlayerInfo const* startInfo =
            sObjectMgr->GetPlayerInfo(
                player->getRace(),
                player->getClass());

        if (!startInfo)
        {
            LOG_ERROR(
                "module",
                "PhaseProgression: login safety no pudo "
                "obtener PlayerInfo para {} (race={}, class={}).",
                player->GetName(),
                static_cast<unsigned>(player->getRace()),
                static_cast<unsigned>(player->getClass()));

            return false;
        }

        std::uint32_t oldMap =
            player->GetMapId();

        bool const teleported =
            player->TeleportTo(
                startInfo->mapId,
                startInfo->positionX,
                startInfo->positionY,
                startInfo->positionZ,
                startInfo->orientation);

        if (!teleported)
        {
            LOG_ERROR(
                "module",
                "PhaseProgression: login safety no pudo "
                "reubicar a {} desde map {}.",
                player->GetName(),
                oldMap);

            return false;
        }

        if (player->GetSession())
        {
            ChatHandler handler(player->GetSession());

            handler.SendNotification(
                "Tu ubicacion anterior no esta disponible "
                "en la fase actual. Has sido trasladado "
                "a una zona segura.");
        }

        LOG_INFO(
            "module",
            "PhaseProgression: login safety reubico a {} "
            "desde map {} hacia map {}. Motivo={}.",
            player->GetName(),
            oldMap,
            startInfo->mapId,
            GetRestrictionMessage(restriction));

        return true;
    }
}
