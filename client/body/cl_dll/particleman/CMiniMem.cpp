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

#include <algorithm>

#include "wrect.h"
#include "cl_dll.h"
#include "cl_util.h"

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "particleman.h"
#include "particleman_internal.h"
#include "CMiniMem.h"

CMiniMem* CMiniMem::_instance = nullptr;

void* CMiniMem::Allocate(std::size_t sizeInBytes)
{
	//auto particle = reinterpret_cast<CBaseParticle*>(_pool.allocate(sizeInBytes, alignment));
	auto particle = reinterpret_cast<CBaseParticle*>(malloc(sizeInBytes));

	if (nullptr != particle)
	{
		_particles.push_back(particle);
	}

	return particle;
}

void CMiniMem::Deallocate(void* memory)
{
	if (!memory)
	{
		return;
	}

	auto it = std::find(_particles.begin(), _particles.end(), memory);
	if (it != _particles.end())
		_particles.erase(it);
	else
		gEngfuncs.Con_Printf("Couldn't find a particle in the particles array to erase!\n");

	free(memory);
	//_pool.deallocate(memory, sizeInBytes, alignment);
}

void CMiniMem::Shutdown()
{
	delete _instance;
	_instance = nullptr;
}

CMiniMem* CMiniMem::Instance()
{
	if (!_instance)
	{
		_instance = new CMiniMem();
	}

	return _instance;
}

void CMiniMem::ProcessAll()
{
	AdvanceAll();
	RenderAll(true);
}

void CMiniMem::AdvanceAll()
{
	const float time = gEngfuncs.GetClientTime();

	for (std::size_t i = 0; i < _particles.size();)
	{
		auto effect = _particles[i];

		if (!IsGamePaused())
		{
			effect->Think(time);
		}

		if (0 != effect->m_flDieTime && time >= effect->m_flDieTime)
		{
			effect->Die();
			delete effect;
			continue;
		}

		++i;
	}

	g_flOldTime = time;
}

void CMiniMem::RenderAll(bool update_pvs_cache)
{
	struct RenderParticleRef
	{
		CBaseParticle* particle;
		float distance;
	};

	_visibleParticles = 0;
	g_cFrustum.CalculateFrustum();

	std::vector<RenderParticleRef> refs;
	refs.reserve(_particles.size());

	cl_entity_t* player = gEngfuncs.GetLocalPlayer();
	const Vector playerOrigin = player ? player->origin : Vector(0, 0, 0);

	for (auto* effect : _particles)
	{
		if (!effect->EvaluateVisibilityForRender(update_pvs_cache))
			continue;

		const Vector delta = playerOrigin - effect->m_vOrigin;
		const float distance = delta.Length() * delta.Length();
		if (update_pvs_cache)
			effect->SetPlayerDistance(distance);
		refs.push_back({effect, distance});
	}

	std::sort(refs.begin(), refs.end(), [](const RenderParticleRef& lhs, const RenderParticleRef& rhs)
		{
			return lhs.distance > rhs.distance;
		});

	_visibleParticles = refs.size();
	for (const auto& ref : refs)
		ref.particle->Draw();
}

static unsigned int MiniHashMix(unsigned int h, unsigned int v)
{
	h ^= v;
	h *= 16777619u;
	return h;
}

static unsigned int MiniHashFloat(unsigned int h, float f)
{
	union { float f; unsigned int u; } x;
	x.f = f;
	return MiniHashMix(h, x.u);
}

unsigned int CMiniMem::StateHash() const
{
	unsigned int h = 2166136261u;
	h = MiniHashMix(h, static_cast<unsigned int>(_particles.size()));
	h = MiniHashFloat(h, g_flOldTime);
	for (std::size_t i = 0; i < _particles.size(); ++i)
	{
		h = MiniHashMix(h, static_cast<unsigned int>(i));
		if (_particles[i])
			_particles[i]->HashSimState(h);
	}
	return h;
}

int CMiniMem::ApplyForce(Vector vOrigin, Vector vDirection, float flRadius, float flStrength)
{
	const float radiusSquared = flRadius * flRadius;

	for (auto effect : _particles)
	{
		if (!effect->m_bAffectedByForce)
		{
			continue;
		}

		const float size = effect->m_flSize / 5;

		const Vector mins = effect->m_vOrigin - Vector{size, size, size};
		const Vector maxs = effect->m_vOrigin + Vector{size, size, size};

		//If the force origin lies outside the effect's bounding box, calculate the distance from the box.
		float totalDistanceSquared = 0;

		for (int i = 0; i < 3; ++i)
		{
			float boundingValue;

			if (vOrigin[i] < mins[i])
			{
				boundingValue = mins[i];
			}
			else if (vOrigin[i] > maxs[i])
			{
				boundingValue = maxs[i];
			}
			else
			{
				continue;
			}

			totalDistanceSquared += (vOrigin[i] - boundingValue) * (vOrigin[i] - boundingValue);
		}

		//Effect is further away from position than force radius, don't apply force.
		if (totalDistanceSquared > radiusSquared)
		{
			continue;
		}

		const float strength = std::max(0.f, flStrength - (vOrigin - effect->m_vOrigin).Length() * (flStrength / (0.5f * radiusSquared)));

		if (vDirection == Vector(0,0,0))
		{
			const float acceleration = -(strength / effect->m_flMass);

			const Vector direction = (vOrigin - effect->m_vOrigin).Normalize();
			const Vector velocity = effect->m_vVelocity.Normalize();

			effect->m_vVelocity = acceleration * (direction + velocity);
		}
		else
		{
			const float acceleration = strength / effect->m_flMass;

			const Vector direction = vDirection.Normalize();
			const Vector velocity = effect->m_vVelocity.Normalize();

			effect->m_vVelocity = acceleration * (direction + velocity);
		}

		effect->Force();
	}

	return 1;
}

void CMiniMem::Reset()
{
	_visibleParticles = 0;

	auto particles = _particles;

	for (auto particle : particles)
	{
		particle->Die();
		delete particle;
	}

	if (_particles.size() != 0)
	{
		gEngfuncs.Con_Printf("Didn't delete all particles?!");
		_particles.clear();
	}

	//Wipe away previously allocated memory so maps with loads of particles don't eat up memory forever.
	//_pool.release();
	_particles.shrink_to_fit();
}
