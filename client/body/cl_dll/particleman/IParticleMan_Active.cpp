/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include <vector>
#include <cstring>

#include "wrect.h"
#include "cl_dll.h"
#include "cl_util.h"

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "triangleapi.h"
#include "particleman.h"
#include "particleman_internal.h"
#include "CMiniMem.h"
#include "IParticleMan_Active.h"

CFrustum g_cFrustum;
float g_flGravity;
float g_flOldTime;
Vector g_vViewAngles;
Vector g_viewPlaneNormal;

static bool g_iRenderMode = true;

static cvar_t* cl_pmanstats = nullptr;

static std::vector<ForceMember> g_pForceList;

IParticleMan_Active::IParticleMan_Active()
{
	g_pForceList.reserve(MaxForceElements);
}

void IParticleMan_Active::SetRender(int iRender)
{
	g_iRenderMode = iRender != 0;
}

void IParticleMan_Active::ApplyForce(Vector vOrigin, Vector vDirection, float flRadius, float flStrength, float flDuration)
{
	if (g_pForceList.size() >= MaxForceElements)
	{
		return;
	}

	ForceMember member;

	member.m_vOrigin = vOrigin;
	member.m_vDirection = vDirection;
	member.m_flRadius = flRadius;
	member.m_flStrength = flStrength;
	member.m_flDieTime = gEngfuncs.GetClientTime() + flDuration;

	g_pForceList.push_back(member);
}

void IParticleMan_Active::SetUp(cl_enginefunc_t* pEnginefuncs)
{
	//Note: disabled because we're in the client dll.
	//std::memcpy(&gEngfuncs, pEnginefuncs, sizeof(gEngfuncs));

	cl_pmanstats = gEngfuncs.pfnRegisterVariable("cl_pmanstats", "0", 0);
}

CBaseParticle* IParticleMan_Active::CreateParticle(Vector org, Vector normal, model_s* sprite, float size, float brightness, const char* classname)
{
	auto particle = new CBaseParticle();

	particle->InitializeSprite(org, normal, sprite, size, brightness);
	strncpy(particle->m_szClassname, classname, sizeof(particle->m_szClassname) - 1);
	particle->m_szClassname[sizeof(particle->m_szClassname) - 1] = '\0';

	return particle;
}

void IParticleMan_Active::ResetParticles()
{
	CMiniMem::Instance()->Reset();
	g_pForceList.clear();
}

void IParticleMan_Active::SetVariables(float flGravity, Vector vViewAngles)
{
	g_flGravity = flGravity;

	if (gEngfuncs.GetClientTime() != g_flOldTime)
	{
		g_vViewAngles = vViewAngles;

		Vector right, up;
		AngleVectors(g_vViewAngles, g_viewPlaneNormal, right, up);
	}
}

void IParticleMan_Active::Update()
{
	Advance();
	Render(true);
}

void IParticleMan_Active::Advance()
{
	g_pParticleMan = this;

	const float time = gEngfuncs.GetClientTime();

	for (std::size_t i = 0; i < g_pForceList.size();)
	{
		auto& member = g_pForceList[i];
		if (member.m_flDieTime != 0 && member.m_flDieTime < time)
		{
			if (i + 1 < g_pForceList.size())
			{
				std::swap(member, g_pForceList[g_pForceList.size() - 1]);
			}

			g_pForceList.erase(g_pForceList.begin() + (g_pForceList.size() - 1));
		}
		else
		{
			++i;
		}
	}

	auto memory = CMiniMem::Instance();

	for (const auto& member : g_pForceList)
	{
		memory->ApplyForce(member.m_vOrigin, member.m_vDirection, member.m_flRadius, member.m_flStrength);
	}

	memory->AdvanceAll();
}

void IParticleMan_Active::Render(bool update_pvs_cache)
{
	g_pParticleMan = this;
	CMiniMem::Instance()->RenderAll(update_pvs_cache);

	if (nullptr != cl_pmanstats && cl_pmanstats->value == 1)
	{
		gEngfuncs.Con_NPrintf(15, "Number of Particles: %d", static_cast<int>(CMiniMem::Instance()->GetTotalParticles()));
		gEngfuncs.Con_NPrintf(16, "Particles Drawn: %d", static_cast<int>(CMiniMem::Instance()->GetDrawnParticles()));
	}
}

static unsigned int PManHashMix(unsigned int h, unsigned int v)
{
	h ^= v;
	h *= 16777619u;
	return h;
}

static unsigned int PManHashFloat(unsigned int h, float f)
{
	union { float f; unsigned int u; } x;
	x.f = f;
	return PManHashMix(h, x.u);
}

unsigned int IParticleMan_Active::StateHash() const
{
	unsigned int h = CMiniMem::Instance()->StateHash();
	h = PManHashMix(h, static_cast<unsigned int>(g_pForceList.size()));
	for (const auto& member : g_pForceList)
	{
		h = PManHashFloat(h, member.m_vOrigin.x);
		h = PManHashFloat(h, member.m_vOrigin.y);
		h = PManHashFloat(h, member.m_vOrigin.z);
		h = PManHashFloat(h, member.m_vDirection.x);
		h = PManHashFloat(h, member.m_vDirection.y);
		h = PManHashFloat(h, member.m_vDirection.z);
		h = PManHashFloat(h, member.m_flRadius);
		h = PManHashFloat(h, member.m_flStrength);
		h = PManHashFloat(h, member.m_flDieTime);
	}
	return h;
}

int IParticleMan_Active::ParticleCount() const
{
	return static_cast<int>(CMiniMem::Instance()->GetTotalParticles());
}
