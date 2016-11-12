/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "cg_local.h"
#include "game/default/bg_pmove.h"

#define SMOKE_DENSITY 2.0

/**
 * @brief
 */
static _Bool Cg_SmokeTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	cg_particle_t *p;

	if (ent) { // trails should be framerate independent
		if (ent->time > cgi.client->systime) {
			return false;
		}

		// don't emit smoke trails for static entities (grenades on the floor)
		if (VectorCompare(start, end)) {
			return false;
		}

		ent->time = cgi.client->systime + 16;
	}

	if (cgi.PointContents(end) & MASK_LIQUID) {
		return Cg_BubbleTrail(start, end, 24.0);
	}

	vec3_t vec, move;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const vec_t len = VectorNormalize(vec);

	if (len < SMOKE_DENSITY) {
		return false;
	}

	VectorScale(vec, SMOKE_DENSITY, vec);
	VectorSubtract(move, vec, move);

	for (vec_t i = 0.0; i < len; i += SMOKE_DENSITY) {
		VectorAdd(move, vec, move);

		if (!(p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_smoke))) {
			return true;
		}

		const vec_t c = 0.3 - Randomf() * 0.2;
		Vector4Set(p->part.color, c, c, c, 0.5);
		Vector4Set(p->color_vel, 0.2, 0.2, 0.2, -0.8 + Randomf() * 0.3);

		p->part.scale = 1.0 + Randomf();
		p->scale_vel = 10.0 + (Randomf() * 5.0);
		p->part.roll = Randomc() * 350.0;

		VectorScale(vec, len, p->vel);

		for (int32_t j = 0; j < 3; j++) {
			p->part.org[j] = move[j];
			p->vel[j] += Randomc();
		}

		p->accel[2] = 10.0;
	}

	return true;
}

/**
 * @brief
 */
_Bool Cg_FlameTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	cg_particle_t *p;
	int32_t j;
	_Bool static_flame = false;

	if (ent) { // trails should be framerate independent
		if (ent->time > cgi.client->systime) {
			return false;
		}
		ent->time = cgi.client->systime + 16;
		static_flame = VectorCompare(ent->current.origin, ent->prev.origin);
	}

	if (cgi.PointContents(end) & MASK_LIQUID) {
		return Cg_BubbleTrail(start, end, 10.0);
	}

	vec3_t vec, move;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	const vec_t len = VectorNormalize(vec);

	if (len < 1.0) {
		return false;
	}

	VectorScale(vec, 1.0, vec);
	VectorSubtract(move, vec, move);

	for (vec_t i = 0.0; i < len; i += SMOKE_DENSITY) {
		VectorAdd(move, vec, move);

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_flame))) {
			return true;
		}

		cgi.ColorFromPalette(220 + (Random() & 7), p->part.color);
		p->part.color[3] = 0.75;

		Vector4Set(p->color_vel, 0.0, 0.0, 0.0, -1.0 / (2 + Randomf() * 0.3));

		p->part.scale = 10.0 + Randomc();

		for (j = 0; j < 3; j++) {
			p->part.org[j] = move[j];
			p->vel[j] = Randomc() * 1.5;
		}

		p->accel[2] = 15.0;

		// make static flames rise
		if (ent && static_flame) {
			p->color_vel[3] *= 0.65;
			p->accel[2] = 20.0;
		}
	}

	return true;
}

/**
 * @brief
 */
void Cg_SteamTrail(cl_entity_t *ent, const vec3_t org, const vec3_t vel) {
	cg_particle_t *p;
	int32_t i;

	if (ent) { // trails should be framerate independent
		if (ent->time > cgi.client->systime) {
			return;
		}
		ent->time = cgi.client->systime + 16;
	}

	vec3_t end;
	VectorAdd(org, vel, end);

	if (cgi.PointContents(org) & MASK_LIQUID) {
		Cg_BubbleTrail(org, end, 10.0);
		return;
	}

	if (!(p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_steam))) {
		return;
	}

	cgi.ColorFromPalette(6 + (Random() & 7), p->part.color);
	p->part.color[3] = 0.3;

	Vector4Set(p->color_vel, 0.0, 0.0, 0.0, -1.0 / (5.0 + Randomf() * 0.5));

	p->part.scale = 8.0;
	p->scale_vel = 20.0;

	p->part.roll = Randomc() * 100.0;

	VectorCopy(org, p->part.org);
	VectorCopy(vel, p->vel);

	for (i = 0; i < 3; i++) {
		p->vel[i] += 2.0 * Randomc();
	}
}

/**
 * @brief
 */
_Bool Cg_BubbleTrail(const vec3_t start, const vec3_t end, vec_t density) {
	vec3_t vec, move;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);

	const vec_t len = VectorNormalize(vec);
	const vec_t delta = 16.0 / density;

	if (len < delta) {
		return false;
	}

	VectorScale(vec, delta, vec);
	VectorSubtract(move, vec, move);

	for (vec_t i = 0.0; i < len; i += delta) {
		VectorAdd(move, vec, move);

		if (!(cgi.PointContents(move) & MASK_LIQUID)) {
			continue;
		}

		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_BUBBLE, cg_particles_bubble))) {
			return true;
		}

		cgi.ColorFromPalette(6 + (Random() & 3), p->part.color);
		Vector4Set(p->color_vel, 0.0, 0.0, 0.0, -0.2 - Randomf() * 0.2);

		p->part.scale = 1.0;
		p->scale_vel = -0.4 - Randomf() * 0.2;

		for (int32_t j = 0; j < 3; j++) {
			p->part.org[j] = move[j] + Randomc() * 2.0;
			p->vel[j] = Randomc() * 5.0;
		}

		p->vel[2] += 6.0;
		p->accel[2] = 10.0;
	}

	return true;
}

/**
 * @brief
 */
static _Bool Cg_BlasterTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	const uint8_t col = ent->current.client ? ent->current.client : EFFECT_COLOR_ORANGE;
	cg_particle_t *p;
	_Bool trail_rendered = false;

	if (ent->time < cgi.client->systime) {
		vec3_t delta;
		vec_t step = 1.5;

		if (cgi.PointContents(end) & MASK_LIQUID) {
			step = 2.0;
		}

		vec_t d = 0.0;

		VectorSubtract(end, start, delta);
		const vec_t dist = VectorNormalize(delta);

		if (dist >= step) {
			Cg_BubbleTrail(start, end, 12.0);

			while (d < dist) {
				if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL))) {
					break;
				}

				cgi.ColorFromPalette(col + (Random() & 5), p->part.color);
				Vector4Set(p->color_vel, 1.0, 1.0, 1.0, -3.0 + Randomc());

				p->part.scale = 2.0;
				p->scale_vel = -4.0;

				VectorMA(start, d, delta, p->part.org);
				VectorScale(delta, 600.0, p->vel);
				VectorScale(delta, -800.0, p->accel);

				d += step;
			}

			trail_rendered = true;
		}

		ent->time = cgi.client->systime + 16;
	}

	vec3_t color;
	cgi.ColorFromPalette(col, color);

	VectorScale(color, 3.0, color);

	for (int32_t i = 0; i < 3; i++) {
		if (color[i] > 1.0) {
			color[i] = 1.0;
		}
	}

	if ((p = Cg_AllocParticle(PARTICLE_CORONA, NULL))) {
		VectorCopy(color, p->part.color);
		VectorCopy(end, p->part.org);

		p->color_vel[3] = -FLT_MAX;
		p->part.scale = CORONA_SCALE(3.0, 0.125);
	}

	r_light_t l;
	VectorCopy(end, l.origin);
	l.radius = 75.0;
	VectorCopy(color, l.color);

	cgi.AddLight(&l);

	return trail_rendered;
}

/**
 * @brief
 */
static _Bool Cg_GrenadeTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	return Cg_SmokeTrail(ent, start, end);
}

/**
 * @brief
 */
static _Bool Cg_RocketTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	cg_particle_t *p;
	_Bool trail_rendered;

	trail_rendered = Cg_SmokeTrail(ent, start, end);

	if (trail_rendered) { // time to add new particles
		vec3_t delta;

		VectorSubtract(end, start, delta);
		const vec_t dist = VectorNormalize(delta);

		vec_t d = 0.0;
		while (d < dist) {

			if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_flame))) {
				break;
			}

			cgi.ColorFromPalette(EFFECT_COLOR_ORANGE + (Random() & 5), p->part.color);
			Vector4Set(p->color_vel, 1.0, 1.0, 1.0, -2.0);

			p->part.scale = 4.0;
			p->scale_vel = -6.0;

			VectorMA(start, d, delta, p->part.org);
			VectorScale(delta, 800.0, p->vel);
			VectorScale(delta, -1200.0, p->accel);

			d += 1.0;
		}
	}

	if ((p = Cg_AllocParticle(PARTICLE_CORONA, NULL))) {
		VectorSet(p->part.color, 0.8, 0.4, 0.2);
		VectorCopy(end, p->part.org);

		p->color_vel[3] = -FLT_MAX;
		p->part.scale = CORONA_SCALE(3.0, 0.125);
	}

	r_light_t l;
	VectorCopy(end, l.origin);
	l.radius = 150.0;
	VectorSet(l.color, 0.8, 0.4, 0.2);

	cgi.AddLight(&l);
	return trail_rendered;
}

/**
 * @brief
 */
static void Cg_EnergyTrail(cl_entity_t *ent, const vec3_t org, vec_t radius, int32_t color) {
	int32_t i;

	const vec_t ltime = (vec_t) (cgi.client->systime + ent->current.number) / 300.0;

	for (i = 0; i < NUM_APPROXIMATE_NORMALS; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, NULL))) {
			return;
		}

		vec_t angle = ltime * approximate_normals[i][0];
		const vec_t sp = sin(angle);
		const vec_t cp = cos(angle);

		angle = ltime * approximate_normals[i][1];
		const vec_t sy = sin(angle);
		const vec_t cy = cos(angle);

		vec3_t forward;
		VectorSet(forward, cp * sy, cy * sy, -sp);

		vec_t dist = sin(ltime + i) * radius;

		p->part.scale = 0.5 + (0.05 * radius);

		int32_t j;
		for (j = 0; j < 3; j++) {
			// project the origin outward and forward
			p->part.org[j] = org[j] + (approximate_normals[i][j] * dist) + forward[j] * radius;
		}

		vec3_t delta;
		VectorSubtract(p->part.org, org, delta);
		dist = VectorLength(delta) / (3.0 * radius);

		cgi.ColorFromPalette(color + dist * 7.0, p->part.color);
		Vector4Set(p->color_vel, -dist * 100.0, -dist * 100.0, -dist * 100.0, -100.0);

		VectorScale(delta, 100.0, p->accel);
	}

	// add a bubble trail if appropriate
	if (ent->time > cgi.client->systime) {
		return;
	}

	ent->time = cgi.client->systime + 16;

	if (cgi.PointContents(org) & MASK_LIQUID) {
		Cg_BubbleTrail(ent->prev.origin, ent->current.origin, radius / 4.0);
	}
}

/**
 * @brief
 */
static void Cg_HyperblasterTrail(cl_entity_t *ent, const vec3_t org) {
	r_light_t l;
	cg_particle_t *p;

	Cg_EnergyTrail(ent, org, 6.0, 107);

	if ((p = Cg_AllocParticle(PARTICLE_CORONA, NULL))) {
		VectorSet(p->part.color, 0.4, 0.7, 1.0);
		VectorCopy(org, p->part.org);

		p->color_vel[3] = -FLT_MAX;
		p->part.scale = CORONA_SCALE(10.0, 0.15);
	}

	VectorCopy(org, l.origin);
	l.radius = 100.0;
	VectorSet(l.color, 0.4, 0.7, 1.0);

	cgi.AddLight(&l);
}

/**
 * @brief
 */
static void Cg_LightningTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	r_light_t l;
	vec3_t dir, delta, pos, vel;
	vec_t dist, offset;
	int32_t i;

	VectorCopy(start, l.origin);
	l.radius = 90.0 + 10.0 * Randomc();
	VectorSet(l.color, 0.6, 0.6, 1.0);
	cgi.AddLight(&l);

	VectorSubtract(start, end, dir);
	dist = VectorNormalize(dir);

	VectorScale(dir, -48.0, delta);
	VectorCopy(start, pos);

	VectorSubtract(ent->current.origin, ent->prev.origin, vel);

	i = 0;
	while (dist > 0.0) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_BEAM, cg_particles_lightning))) {
			return;
		}

		cgi.ColorFromPalette(12 + (Random() & 3), p->part.color);
		Vector4Set(p->color_vel, 0.0, 0.0, 0.0, -100.0);

		p->part.scale = 8.0;
		p->part.scroll_s = -8.0;

		VectorCopy(pos, p->part.org);

		if (dist <= 48.0) {
			VectorScale(dir, -dist, delta);
			offset = 0.0;
		} else {
			offset = fabs(2.0 * sin(dist));
		}

		VectorAdd(pos, delta, pos);
		pos[2] += (++i & 1) ? offset : -offset;

		VectorCopy(pos, p->part.end);
		VectorCopy(vel, p->vel);

		dist -= 48.0;

		if (dist > 12.0) {
			VectorCopy(p->part.end, l.origin);
			l.radius = 90.0 + 10.0 * Randomc();
			cgi.AddLight(&l);
		}
	}

	VectorMA(end, 12.0, dir, l.origin);
	l.radius = 90.0 + 10.0 * Randomc();
	cgi.AddLight(&l);
}

/**
 * @brief
 */
static void Cg_BfgTrail(cl_entity_t *ent, const vec3_t org) {

	Cg_EnergyTrail(ent, org, 48.0, 206);

	const vec_t mod = sin(cgi.client->systime >> 5);

	cg_particle_t *p;
	if ((p = Cg_AllocParticle(PARTICLE_ROLL, cg_particles_explosion))) {

		cgi.ColorFromPalette(206, p->part.color);
		Vector4Set(p->color_vel, 0.0, 0.0, 0.0, -4.0);

		p->part.scale = 48.0 + 12.0 * mod;

		p->part.roll = Randomc() * 100.0;

		VectorCopy(org, p->part.org);
	}

	r_light_t l;
	VectorCopy(org, l.origin);
	l.radius = 160.0 + 48.0 * mod;
	VectorSet(l.color, 0.4, 1.0, 0.4);

	cgi.AddLight(&l);
}

/**
 * @brief
 */
static void Cg_TeleporterTrail(cl_entity_t *ent, const vec3_t org) {

	if (ent->time > cgi.client->systime) {
		return;
	}

	cgi.AddSample(&(const s_play_sample_t) {
		.sample = cg_sample_respawn,
		 .entity = ent->current.number,
		  .attenuation = ATTEN_IDLE,
		   .flags = S_PLAY_ENTITY
	});

	ent->time = cgi.client->systime + 1000 + (2000 * Randomf());

	for (int32_t i = 0; i < 4; i++) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_SPLASH, cg_particles_teleporter))) {
			return;
		}

		cgi.ColorFromPalette(216, p->part.color);
		Vector4Set(p->color_vel, 0.0, 0.0, 0.0, -1.8);

		p->part.scale = 16.0;
		p->scale_vel = 24.0;

		VectorCopy(org, p->part.org);
		p->part.org[2] -= (6.0 * i);
		p->vel[2] = 140.0;
	}
}

/**
 * @brief
 */
static _Bool Cg_GibTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {

	if (ent) {
		if (ent->time > cgi.client->systime) {
			return false;
		}
		ent->time = cgi.client->systime + 16;
	}

	if (cgi.PointContents(end) & MASK_LIQUID) {
		return Cg_BubbleTrail(start, end, 8.0);
	}

	vec3_t move;
	VectorSubtract(end, start, move);

	vec_t dist = VectorNormalize(move);

	if (dist < 1.5) {
		return false;
	}

	while (dist > 0.0) {
		cg_particle_t *p;

		if (!(p = Cg_AllocParticle(PARTICLE_NORMAL, cg_particles_blood))) {
			break;
		}

		VectorMA(end, dist, move, p->part.org);

		cgi.ColorFromPalette(232 + (Random() & 7), p->part.color);
		Vector4Set(p->color_vel, 0.0, 0.0, 0.0, -1.0 / (0.5 + Randomf() * 0.3));

		p->part.scale = 3.0;

		VectorScale(move, 40.0, p->vel);

		p->accel[0] = p->accel[1] = 0.0;
		p->accel[2] = PARTICLE_GRAVITY / 2.0;

		dist -= 1.5;
	}

	return true;
}

/**
 * @brief
 */
static _Bool Cg_FireballTrail(cl_entity_t *ent, const vec3_t start, const vec3_t end) {
	const vec3_t color = { 0.9, 0.3, 0.1 };
	_Bool trail_rendered = false;

	if (cgi.PointContents(end) & MASK_LIQUID) {
		return false;
	}

	r_light_t l;
	VectorCopy(end, l.origin);
	VectorCopy(color, l.color);
	l.radius = 85.0;

	if (ent->current.effects & EF_DESPAWN) {
		const vec_t decay = Clamp((cgi.client->systime - ent->time) / 1000.0, 0.0, 1.0);
		l.radius *= (1.0 - decay);
		trail_rendered = true;
	} else {
		trail_rendered = Cg_SmokeTrail(ent, start, end);
		ent->time = cgi.client->systime;
		trail_rendered = Cg_FlameTrail(ent, start, end) || trail_rendered;
	}

	cgi.AddLight(&l);
	return trail_rendered;
}

/**
 * @brief Apply unique trails to entities between their previous packet origin
 * and their current interpolated origin. Beam trails are a special case: the
 * old origin field is overridden to specify the endpoint of the beam.
 */
void Cg_EntityTrail(cl_entity_t *ent, r_entity_t *e) {
	const entity_state_t *s = &ent->current;

	vec3_t start, end;

	// beams have two origins, most entities have just one
	if (s->effects & EF_BEAM) {

		// client is overridden to specify owner of the beam
		if (Cg_IsSelf(ent) && !cg_third_person->value) {
			// we own this beam (lightning, grapple, etc..)
			// project start position below the view origin

			VectorMA(cgi.view->origin, 8.0, cgi.view->forward, start);

			switch (cg_hand->integer) {
				case HAND_LEFT:
					VectorMA(start, -5.5, cgi.view->right, start);
					break;
				case HAND_RIGHT:
					VectorMA(start, 5.5, cgi.view->right, start);
					break;
				default:
					break;
			}

			VectorMA(start, -8.0, cgi.view->up, start);
		} else {
			VectorCopy(ent->prev.origin, start);
		}

		VectorCopy(ent->termination, end);
	} else {
		VectorCopy(ent->trail_start, start);
		VectorCopy(e->origin, end);
	}

	// add the trail
	_Bool trail_rendered = false;

	switch (s->trail) {
		case TRAIL_BLASTER:
			trail_rendered = Cg_BlasterTrail(ent, start, end);
			break;
		case TRAIL_GRENADE:
			trail_rendered = Cg_GrenadeTrail(ent, start, end);
			break;
		case TRAIL_ROCKET:
			trail_rendered = Cg_RocketTrail(ent, start, end);
			break;
		case TRAIL_HYPERBLASTER:
			Cg_HyperblasterTrail(ent, e->origin);
			break;
		case TRAIL_LIGHTNING:
			Cg_LightningTrail(ent, start, end);
			break;
		case TRAIL_BFG:
			Cg_BfgTrail(ent, e->origin);
			break;
		case TRAIL_TELEPORTER:
			Cg_TeleporterTrail(ent, e->origin);
			break;
		case TRAIL_GIB:
			trail_rendered = Cg_GibTrail(ent, start, end);
			break;
		case TRAIL_FIREBALL:
			trail_rendered = Cg_FireballTrail(ent, start, end);
			break;
		default:
			break;
	}

	if (trail_rendered) {
		VectorCopy(end, ent->trail_start);
	}
}
