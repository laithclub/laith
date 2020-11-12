#include "Hooks.h"
#include "Resolver.h"
#include "RageBacktracking.h"
#include "Ragebot.h"
#include "AnimationFix.h"

std::string ResolverMode[65];
std::string ResolverMode_freestand[65];
std::string DidShot[65];

CResolver g_CResolver;




int last_ticks[65];
int IBasePlayer::GetChokedPackets() {
    auto ticks = TIME_TO_TICKS(GetSimulationTime() - GetOldSimulationTime());
    if (ticks == 0 && last_ticks[GetIndex()] > 0) {
        return last_ticks[GetIndex()] - 1;
    }
    else {
        last_ticks[GetIndex()] = ticks;
        return ticks;
    }
}

/*void CResolver::HandleHits(IBasePlayer* pEnt) //def handle
{
    auto NetChannel = interfaces.engine->GetNetChannelInfo();

    if (!NetChannel)
        return;

    static float predTime[65];
    static bool init[65];

    if (csgo->firedshots[pEnt->EntIndex()])
    {
        if (init[pEnt->EntIndex()])
        {
            g_CResolver.pitchHit[pEnt->EntIndex()] = pEnt->GetEyeAngles().x;
            predTime[pEnt->EntIndex()] = interfaces.global_vars->curtime + NetChannel->GetAvgLatency(FLOW_INCOMING) + NetChannel->GetAvgLatency(FLOW_OUTGOING) + TICKS_TO_TIME(1) + TICKS_TO_TIME(interfaces.engine->GetNetChannel()->iChokedPackets);
            init[pEnt->EntIndex()] = false;
        }

        if (interfaces.global_vars->curtime > predTime[pEnt->EntIndex()] && !csgo->hitshots[pEnt->EntIndex()])
        {
            csgo->missedshots[pEnt->EntIndex()] += 1;
            csgo->firedshots[pEnt->EntIndex()] = false;
        }
        else if (interfaces.global_vars->curtime <= predTime[pEnt->EntIndex()] && csgo->hitshots[pEnt->EntIndex()])
            csgo->firedshots[pEnt->EntIndex()] = false;

    }
    else
        init[pEnt->EntIndex()] = true;

    csgo->hitshots[pEnt->EntIndex()] = false;
}*/

/*void CResolver::fix_local_player_animations()
{
    auto local_player = interfaces.ent_list->GetClientEntity(interfaces.engine->GetLocalPlayer());

    if (!local_player)
        return;

    static float sim_time;
    if (sim_time != local_player->GetSimulationTime())
    {
        auto state = local_player->GetPlayerAnimState(); if (!state) return;

        const float curtime = interfaces.global_vars->curtime;
        const float frametime = interfaces.global_vars->frametime;
        const float realtime = interfaces.global_vars->realtime;
        const float absoluteframetime = interfaces.global_vars->absoluteframetime;
        const float absoluteframestarttimestddev = interfaces.global_vars->absoluteframestarttimestddev;
        const float interpolation_amount = interfaces.global_vars->interpolation_amount;
        const float framecount = interfaces.global_vars->framecount;
        const float tickcount = interfaces.global_vars->tickcount;

        static auto host_timescale = interfaces.cvars->FindVar(("host_timescale"));

        interfaces.global_vars->curtime = local_player->GetSimulationTime();
        interfaces.global_vars->realtime = local_player->GetSimulationTime();
        interfaces.global_vars->frametime = interfaces.global_vars->interval_per_tick * host_timescale->GetFloat();
        interfaces.global_vars->absoluteframetime = interfaces.global_vars->interval_per_tick * host_timescale->GetFloat();
        interfaces.global_vars->absoluteframestarttimestddev = local_player->GetSimulationTime() - interfaces.global_vars->interval_per_tick * host_timescale->GetFloat();
        interfaces.global_vars->interpolation_amount = 0;
        interfaces.global_vars->framecount = TICKS_TO_TIME(local_player->GetSimulationTime());
        interfaces.global_vars->tickcount = TICKS_TO_TIME(local_player->GetSimulationTime());
        int backup_flags = local_player->GetFlags();

        CAnimationLayer backup_layers[15];
        std::memcpy(backup_layers, local_player->GetAnimOverlays(), (sizeof(CAnimationLayer) * 15));

        if (state->m_iLastClientSideAnimationUpdateFramecount == interfaces.global_vars->framecount)
            state->m_iLastClientSideAnimationUpdateFramecount = interfaces.global_vars->framecount - 1;

        std::memcpy(local_player->GetAnimOverlays(), backup_layers, (sizeof(CAnimationLayer) * 15));


        interfaces.global_vars->curtime = curtime;
        interfaces.global_vars->realtime = realtime;
        interfaces.global_vars->frametime = frametime;
        interfaces.global_vars->absoluteframetime = absoluteframetime;
        interfaces.global_vars->absoluteframestarttimestddev = absoluteframestarttimestddev;
        interfaces.global_vars->interpolation_amount = interpolation_amount;
        interfaces.global_vars->framecount = framecount;
        interfaces.global_vars->tickcount = tickcount;
        sim_time = local_player->GetSimulationTime();
    }
    local_player->InvalidateBoneCache();
    local_player->SetupBones(nullptr, -1, 0x7FF00, interfaces.global_vars->curtime);
}*/

float CResolver::AngleNormalize(float angle)
{
    angle = fmodf(angle, 360.0f);
    if (angle > 180)
    {
        angle -= 360;
    }
    if (angle < -180)
    {
        angle += 360;
    }
    return angle;
}

float CResolver::NormalizeYaw180(float yaw)
{
    if (yaw > 180)
        yaw -= (round(yaw / 360) * 360.f);
    else if (yaw < -180)
        yaw += (round(yaw / 360) * -360.f);

    return yaw;
}

float CResolver::angle_difference(float a, float b) {
    auto diff = NormalizeYaw180(a - b);

    if (diff < 180)
        return diff;
    return diff - 360;
}

float CResolver::approach(float cur, float target, float inc) {
    inc = abs(inc);

    if (cur < target)
        return min(cur + inc, target);
    if (cur > target)
        return max(cur - inc, target);

    return target;
}

float CResolver::approach_angle(float cur, float target, float inc) {
    auto diff = angle_difference(target, cur);
    return approach(cur, cur + diff, inc);
}



float CResolver::GetBackwardYaw(IBasePlayer* player) {
    return Math::CalculateAngle(csgo->local->GetOrigin(), player->GetOrigin()).y;
}

void CResolver::FixPitch(IBasePlayer* pEnt)
{
    float last_simtime[64] = { 0.f };
    float stored_pitch_1[64] = { 0.f };
    float fixed_pitch[64] = { 0.f };

    bool has_been_set[64] = { false };

    const auto local = csgo->local;
    if (!local)
        return;

    for (auto i = 0; i < interfaces.engine->GetMaxClients(); ++i)
    {

        const auto eye = pEnt->GetEyeAnglesPointer();

        auto pitch = 0.f;

        if (stored_pitch_1[i] == FLT_MAX || !has_been_set[i])
        {
            stored_pitch_1[i] = eye->x;
            has_been_set[i] = true;
        }

        if (stored_pitch_1[i] - eye->x < 30 && stored_pitch_1[i] - eye->x > -30)
        {
            pitch = eye->x;
        }
        else
        {
            pitch = stored_pitch_1[i];
        }

        pEnt->GetEyeAnglesPointer()->x = pitch;
    }
}



float fast_float_normalize(float srcAngle, float distAngle) {
    float difference;

    for (; srcAngle > 180.0; srcAngle = srcAngle - 360.0)
        ;
    for (; srcAngle < -180.0; srcAngle = srcAngle + 360.0)
        ;
    for (; distAngle > 180.0; distAngle = distAngle - 360.0)
        ;
    for (; distAngle < -180.0; distAngle = distAngle + 360.0)
        ;
    for (difference = distAngle - srcAngle; difference > 180.0; difference = difference - 360.0)
        ;
    for (; difference < -180.0; difference = difference + 360.0)
        ;
    return difference;
}



void CResolver::resolve(IBasePlayer* ent)
{





    if (!vars.ragebot.resolver3)
        return;

    if (ent->GetTeam() != csgo->local->GetTeam())
    {
        if (!csgo->local->isAlive())
            return;


        if (!interfaces.engine->IsInGame())
            return;

        if (csgo->game_rules->IsFreezeTime())
            return;


        auto animState = ent->GetPlayerAnimState();
        if (!animState)
            return;


        if (ent->GetChokedPackets() <= 0)
            return;



        static auto GetSmoothedVelocity = [](float min_delta, Vector a, Vector b) {
            Vector delta = a - b;
            float delta_length = delta.Length();

            if (delta_length <= min_delta) {
                Vector result;
                if (-min_delta <= delta_length) {
                    return a;
                }
                else {
                    float iradius = 1.0f / (delta_length + FLT_EPSILON);
                    return b - ((delta * iradius) * min_delta);
                }
            }
            else {
                float iradius = 1.0f / (delta_length + FLT_EPSILON);
                return b + ((delta * iradius) * min_delta);
            }
        };
        float v25;
        float v32;
        v32 = ent->GetSimulationTime() - ent->GetOldSimulationTime();
        v25 = clamp(animState->m_fLandingDuckAdditiveSomething + ent->GetDuckSpeed(), 1.0f, 0.0f);
        float v26 = animState->m_fDuckAmount;
        float v27 = v32 * 6.0f;
        float v28;

        // clamp
        if ((v25 - v26) <= v27) {
            if (-v27 <= (v25 - v26))
                v28 = v25;
            else
                v28 = v26 - v27;
        }
        else {
            v28 = v26 + v27;
        }

        Vector velocity = ent->GetVelocity();
        float flDuckAmount = std::clamp(v28, 1.0f, 0.0f);

        Vector animationVelocity = GetSmoothedVelocity(v32 * 2000.0f, velocity, ent->GetVelocity());
        float speed = std::fminf(animationVelocity.Length(), 260.0f);

        auto weapon = ent->GetWeapon();


        float flMaxMovementSpeed = 260.0f;
        if (weapon) {
            flMaxMovementSpeed = std::fmaxf(weapon->GetCSWpnData()->m_flMaxSpeed, 0.001f);
        }

        float flRunningSpeed = speed / (flMaxMovementSpeed * 0.520f);
        float flDuckingSpeed = speed / (flMaxMovementSpeed * 0.340f);

        flRunningSpeed = std::clamp(flRunningSpeed, 0.0f, 1.0f);

        float flYawModifier = (((animState->m_bInHitGroundAnimation * -0.3f) - 0.2f) * flRunningSpeed) + 1.0f;
        if (flDuckAmount > 0.0f) {
            float flDuckingSpeed = std::clamp(flDuckingSpeed, 0.0f, 1.0f);
            flYawModifier += (flDuckAmount * flDuckingSpeed) * (0.5f - flYawModifier);
        }

        float m_flMaxBodyYaw = *(float*)(uintptr_t(animState) + 0x334) * flYawModifier;
        float m_flMinBodyYaw = *(float*)(uintptr_t(animState) + 0x330) * flYawModifier;

        float flEyeYaw = ent->GetEyeAngles().y;
        float flEyeDiff = std::remainderf(flEyeYaw - g_CResolver.resolverinfoo.fakegoalfeetyaw, 360.f);

        if (flEyeDiff <= m_flMaxBodyYaw) {
            if (m_flMinBodyYaw > flEyeDiff)
                g_CResolver.resolverinfoo.fakegoalfeetyaw = fabs(m_flMinBodyYaw) + flEyeYaw;
        }
        else {
            g_CResolver.resolverinfoo.fakegoalfeetyaw = flEyeYaw - fabs(m_flMaxBodyYaw);
        }

        g_CResolver.resolverinfoo.fakegoalfeetyaw = std::remainderf(g_CResolver.resolverinfoo.fakegoalfeetyaw, 360.f);

        if (speed > 0.1f || fabs(velocity.z) > 100.0f) {
            g_CResolver.resolverinfoo.fakegoalfeetyaw = g_CResolver.approach_angle(
                flEyeYaw,
                g_CResolver.resolverinfoo.fakegoalfeetyaw,
                ((animState->m_flGoalFeetYaw * 20.0f) + 30.0f)
                * v32);
        }
        else {
            g_CResolver.resolverinfoo.fakegoalfeetyaw = g_CResolver.approach_angle(
                ent->GetLBY(),
                g_CResolver.resolverinfoo.fakegoalfeetyaw,
                v32 * 100.0f);
        }

        float Left = flEyeYaw - m_flMinBodyYaw;
        float Right = flEyeYaw - m_flMaxBodyYaw;
        float Left_x = flEyeYaw + m_flMinBodyYaw;
        float Right_x = flEyeYaw + m_flMaxBodyYaw;
        float resolveYaw;


        if (ent->GetLBY() > -10.f && ent->GetLBY() < 10.f)
        {
            Vector src3D, dst3D, forward, right, up, src, dst;
            float back_two, right_two, left_two;
            trace_t tr;
            Ray_t ray, ray2, ray3, ray4, ray5;
            CTraceFilter filter;

            Math::AngleVectors(Vector(0, GetBackwardYaw(ent), 0), &forward, &right, &up);

            filter.pSkip = ent;
            src3D = ent->GetEyePosition();
            dst3D = src3D + (forward * 400); //Might want to experiment with other numbers, incase you don't know what the number does, its how far the trace will go. Lower = shorter.

            ray.Init(src3D, dst3D);
            interfaces.trace->TraceRay(ray, MASK_SHOT, &filter, &tr);
            back_two = (tr.endpos - tr.startpos).Length();

            ray2.Init(src3D + right * 35, dst3D + right * 35);
            interfaces.trace->TraceRay(ray2, MASK_SHOT, &filter, &tr);
            right_two = (tr.endpos - tr.startpos).Length();

            ray3.Init(src3D - right * 35, dst3D - right * 35);
            interfaces.trace->TraceRay(ray3, MASK_SHOT, &filter, &tr);
            left_two = (tr.endpos - tr.startpos).Length();

            if (left_two > right_two) {
                ResolverSide = 1;

                //Body should be right
            }
            else if (right_two > left_two) {
                ResolverSide = -1;

                //Body should be left
            }
            else
            {
                ResolverSide = 0;

                //Why does this even exist tbh we have a lby detection system and a bruteforce like wtf
            }
        }
        else if (ent->GetLBY() <= -69.f && ent->GetLBY() > 11.f)
        {
            //bro this is premium meme right here
            ResolverSide = -1;

        }
        else if (ent->GetLBY() >= 58.f && ent->GetLBY() < -11.f)
        {
            //hahahahahahahhahahahahahaaha
            ResolverSide = 1;

        }


                
        if (ent->GetEyeAnglesPointer()->x > 89.f)
            ent->GetEyeAnglesPointer()->x == -89.f;
        else if (ent->GetEyeAnglesPointer()->x < -89.f)
            ent->GetEyeAnglesPointer()->x == 89.f;

        FixPitch(ent);


        switch (csgo->missedshots[ent->EntIndex()] % 3)
        {
        case 0: // brute left side

            if (ResolverSide == +1)
            {
                resolveYaw = -Left;
                ResolverMode[ent->EntIndex()] = "left";
            }

            else if (ResolverSide == -1)
            {
                resolveYaw = -Right;
                ResolverMode[ent->EntIndex()] = "Right";
            }

            else if (ResolverSide == 0)
            {
                resolveYaw = ent->GetPlayerAnimState()->m_flEyeYaw;
                ResolverMode[ent->EntIndex()] = "Middle";
            }

                break;
        case 1: // brute fake side
            if (ResolverSide == +1)
            {
                resolveYaw = Left;
                ResolverMode[ent->EntIndex()] = "Left-x";
            }

            else if (ResolverSide == -1)
            {
                resolveYaw = Right;
                ResolverMode[ent->EntIndex()] = "Right-x";
            }


            else if (ResolverSide == 0)
            {
                resolveYaw = ent->GetPlayerAnimState()->m_flEyeYaw;
                ResolverMode[ent->EntIndex()] = "Middle-x";
            }
            break;
        case 2: // brute fake side

                resolveYaw = ent->GetPlayerAnimState()->m_flEyeYaw;
                ResolverMode[ent->EntIndex()] = "0";
            
            break;
            if (csgo->missedshots[ent->EntIndex()] > 2)
                csgo->missedshots[ent->EntIndex()] = 0;
           default:
            break;
        }

        animState->m_flGoalFeetYaw = resolveYaw;
    }

    else
    return;

}

/*void CResolver::FrameStage(ClientFrameStage_t stage)
{
    if (csgo->local || !interfaces.engine->IsInGame())
        return;

    static bool  wasDormant[65];
    for (int i = 1; i < interfaces.engine->GetMaxClients(); ++i)
    {
        IBasePlayer* pPlayerEntity =  interfaces.ent_list->GetClientEntity(i);
        if (vars.ragebot.resolver3)
            g_CResolver.resolve(pPlayerEntity);

        if (!pPlayerEntity
            || !pPlayerEntity->isAlive())
            continue;
        if (pPlayerEntity->IsDormant())
        {
            wasDormant[i] = true;
            continue;
        }



        if (stage == FRAME_RENDER_START)
        {
            if (pPlayerEntity == csgo->local) {
                pPlayerEntity->GetClientSideAnims();
                fix_local_player_animations();
                pPlayerEntity->UpdateClientSideAnimation();
            }
            g_CResolver.HandleHits(pPlayerEntity);
        }
        if (stage == FRAME_NET_UPDATE_END && pPlayerEntity != csgo->local)
        {
            auto VarMap = reinterpret_cast<uintptr_t>(pPlayerEntity) + 36;
            auto VarMapSize = *reinterpret_cast <int*> (VarMap + 20);

            for (auto index = 0; index < VarMapSize; index++)
                *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(VarMap) + index * 12) = 0;
        }

        wasDormant[i] = false;
    }
}*/


void CResolver::Do2(IBasePlayer* player , animation* record)
{
    if (!vars.ragebot.resolver5)
        return;
    
  

}