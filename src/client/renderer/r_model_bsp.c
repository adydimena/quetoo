/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "renderer.h"

/*
 * The beginning of the BSP model (disk format) in memory.  All lumps are
 * resolved by a relative offset from this address.
 */
static const byte *mod_base;


#define MIN_AMBIENT_COMPONENT 0.05

/*
 * R_LoadBspLighting
 */
static void R_LoadBspLighting(const d_bsp_lump_t *l){
	const char *s, *c;

	if(!l->file_len){
		r_loadmodel->lightmap_data = NULL;
		r_loadmodel->lightmap_scale = DEFAULT_LIGHTMAP_SCALE;
	}
	else {
		r_loadmodel->lightmap_data_size = l->file_len;
		r_loadmodel->lightmap_data = R_HunkAlloc(l->file_len);

		memcpy(r_loadmodel->lightmap_data, mod_base + l->file_ofs, l->file_len);
	}

	r_loadmodel->lightmap_scale = -1;

	// resolve lightmap scale
	if((s = strstr(Cm_EntityString(), "\"lightmap_scale\""))){

		c = Com_Parse(&s);  // parse the string itself
		c = Com_Parse(&s);  // and then the value

		r_loadmodel->lightmap_scale = atoi(c);

		Com_Debug("Resolved lightmap_scale: %d\n", r_loadmodel->lightmap_scale);
	}

	if(r_loadmodel->lightmap_scale == -1)  // ensure safe default
		r_loadmodel->lightmap_scale = DEFAULT_LIGHTMAP_SCALE;

	// resolve ambient light
	if((s = strstr(Cm_EntityString(), "\"ambient_light\""))){
		int i;

		c = Com_Parse(&s);  // parse the string itself
		c = Com_Parse(&s);  // and the vector

		sscanf(c, "%f %f %f", &r_view.ambient_light[0],
				&r_view.ambient_light[1], &r_view.ambient_light[2]);

		for(i = 0; i < 3; i++){  // clamp it
			if(r_view.ambient_light[i] < MIN_AMBIENT_COMPONENT)
				r_view.ambient_light[i] = MIN_AMBIENT_COMPONENT;
		}

		Com_Debug("Resolved ambient_light: %1.2f %1.2f %1.2f\n",
				r_view.ambient_light[0], r_view.ambient_light[1], r_view.ambient_light[2]);
	}
	else {  // ensure sane default
		VectorSet(r_view.ambient_light,
			MIN_AMBIENT_COMPONENT, MIN_AMBIENT_COMPONENT, MIN_AMBIENT_COMPONENT);
	}
}


/*
 * R_LoadBspVisibility
 */
static void R_LoadBspVisibility(const d_bsp_lump_t *l){
	int i;

	if(!l->file_len){
		r_loadmodel->vis = NULL;
		return;
	}

	r_loadmodel->vis_size = l->file_len;
	r_loadmodel->vis = R_HunkAlloc(l->file_len);
	memcpy(r_loadmodel->vis, mod_base + l->file_ofs, l->file_len);

	r_loadmodel->vis->num_clusters = LittleLong(r_loadmodel->vis->num_clusters);

	for(i = 0; i < r_loadmodel->vis->num_clusters; i++){
		r_loadmodel->vis->bit_ofs[i][0] = LittleLong(r_loadmodel->vis->bit_ofs[i][0]);
		r_loadmodel->vis->bit_ofs[i][1] = LittleLong(r_loadmodel->vis->bit_ofs[i][1]);
	}
}


/*
 * R_LoadBspVertexes
 */
static void R_LoadBspVertexes(const d_bsp_lump_t *l){
	const d_bsp_vertex_t *in;
	r_bsp_vertex_t *out;
	int i, count;

	in = (const void *)(mod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadBspVertexes: Funny lump size in %s.", r_loadmodel->name);
	}
	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->vertexes = out;
	r_loadmodel->num_vertexes = count;

	for(i = 0; i < count; i++, in++, out++){
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}


/*
 * R_LoadBspNormals
 */
static void R_LoadBspNormals(const d_bsp_lump_t *l){
	const d_bsp_normal_t *in;
	r_bsp_vertex_t *out;
	int i, count;

	in = (const void *)(mod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadBspNormals: Funny lump size in %s.", r_loadmodel->name);
	}
	count = l->file_len / sizeof(*in);

	if(count != r_loadmodel->num_vertexes){  // ensure sane normals count
		Com_Error(ERR_DROP, "R_LoadBspNormals: unexpected normals count in %s: (%d != %d).",
				r_loadmodel->name, count, r_loadmodel->num_vertexes);
	}

	out = r_loadmodel->vertexes;

	for(i = 0; i < count; i++, in++, out++){
		out->normal[0] = LittleFloat(in->normal[0]);
		out->normal[1] = LittleFloat(in->normal[1]);
		out->normal[2] = LittleFloat(in->normal[2]);
	}
}


/*
 * R_RadiusFromBounds
 */
static float R_RadiusFromBounds(const vec3_t mins, const vec3_t maxs){
	int i;
	vec3_t corner;

	for(i = 0; i < 3; i++){
		corner[i] = fabsf(mins[i]) > fabsf(maxs[i]) ? fabsf(mins[i]) : fabsf(maxs[i]);
	}

	return VectorLength(corner);
}


/*
 * R_LoadBspSubmodels
 */
static void R_LoadBspSubmodels(const d_bsp_lump_t *l){
	const d_bsp_model_t *in;
	r_bsp_submodel_t *out;
	int i, j, count;

	in = (const void *)(mod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadBspSubmodels: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->submodels = out;
	r_loadmodel->num_submodels = count;

	for(i = 0; i < count; i++, in++, out++){
		for(j = 0; j < 3; j++){  // spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat(in->mins[j]) - 1.0;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1.0;
			out->origin[j] = LittleFloat(in->origin[j]);
		}
		out->radius = R_RadiusFromBounds(out->mins, out->maxs);

		out->head_node = LittleLong(in->head_node);

		out->first_face = LittleLong(in->first_face);
		out->num_faces = LittleLong(in->num_faces);
	}
}


/*
 * R_LoadBspEdges
 */
static void R_LoadBspEdges(const d_bsp_lump_t *l){
	const d_bsp_edge_t *in;
	r_bsp_edge_t *out;
	int i, count;

	in = (const void *)(mod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadBspEdges: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc((count + 1) * sizeof(*out));

	r_loadmodel->edges = out;
	r_loadmodel->num_edges = count;

	for(i = 0; i < count; i++, in++, out++){
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}


/*
 * R_LoadBspTexinfo
 */
static void R_LoadBspTexinfo(const d_bsp_lump_t *l){
	const d_bsp_texinfo_t *in;
	r_bsp_texinfo_t *out;
	int i, j, count;

	in = (const void *)(mod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadBspTexinfo: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->texinfo = out;
	r_loadmodel->num_texinfo = count;

	for(i = 0; i < count; i++, in++, out++){
		for(j = 0; j < 8; j++)
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);

		out->flags = LittleLong(in->flags);
		out->value = LittleLong(in->value);

		strncpy(out->name, in->texture, sizeof(in->texture));
		out->image = R_LoadImage(va("textures/%s", out->name), it_world);
	}
}


/*
 * R_SetupBspSurface
 *
 * Sets in s->mins, s->maxs, s->st_mins, s->st_maxs, ..
 */
static void R_SetupBspSurface(r_bsp_surface_t *surf){
	vec3_t mins, maxs;
	vec2_t st_mins, st_maxs;
	int i, j;
	const r_bsp_vertex_t *v;
	const r_bsp_texinfo_t *tex;

	VectorSet(mins, 999999, 999999, 999999);
	VectorSet(maxs, -999999, -999999, -999999);

	st_mins[0] = st_mins[1] = 999999;
	st_maxs[0] = st_maxs[1] = -999999;

	tex = surf->texinfo;

	for(i = 0; i < surf->num_edges; i++){
		const int e = r_loadmodel->surfedges[surf->first_edge + i];
		if(e >= 0)
			v = &r_loadmodel->vertexes[r_loadmodel->edges[e].v[0]];
		else
			v = &r_loadmodel->vertexes[r_loadmodel->edges[-e].v[1]];

		for(j = 0; j < 3; j++){  // calculate mins, maxs
			if(v->position[j] > maxs[j])
				maxs[j] = v->position[j];
			if(v->position[j] < mins[j])
				mins[j] = v->position[j];
		}

		for(j = 0; j < 2; j++){  // calculate st_mins, st_maxs
			const float val = DotProduct(v->position, tex->vecs[j]) + tex->vecs[j][3];
			if(val < st_mins[j])
				st_mins[j] = val;
			if(val > st_maxs[j])
				st_maxs[j] = val;
		}
	}

	VectorCopy(mins, surf->mins);
	VectorCopy(maxs, surf->maxs);

	for(i = 0; i < 3; i++)
		surf->center[i] = (surf->mins[i] + surf->maxs[i]) / 2.0;

	for(i = 0; i < 2; i++){
		const int bmins = floor(st_mins[i] / r_loadmodel->lightmap_scale);
		const int bmaxs = ceil(st_maxs[i] / r_loadmodel->lightmap_scale);

		surf->st_mins[i] = bmins * r_loadmodel->lightmap_scale;
		surf->st_maxs[i] = bmaxs * r_loadmodel->lightmap_scale;

		surf->st_center[i] = (surf->st_maxs[i] + surf->st_mins[i]) / 2.0;
		surf->st_extents[i] = surf->st_maxs[i] - surf->st_mins[i];
	}
}


/*
 * R_LoadBspSurfaces
 */
static void R_LoadBspSurfaces(const d_bsp_lump_t *l){
	const d_bsp_face_t *in;
	r_bsp_surface_t *out;
	int i, count, surf_num;
	int plane_num, side;
	int ti;

	in = (const void *)(mod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadBspSurfaces: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->surfaces = out;
	r_loadmodel->num_surfaces = count;

	R_BeginBuildingLightmaps();

	for(surf_num = 0; surf_num < count; surf_num++, in++, out++){

		out->first_edge = LittleLong(in->first_edge);
		out->num_edges = LittleShort(in->num_edges);

		// resolve plane
		plane_num = LittleShort(in->plane_num);
		out->plane = r_loadmodel->planes + plane_num;

		// and sidedness
		side = LittleShort(in->side);
		if(side){
			out->flags |= MSURF_PLANEBACK;
			VectorNegate(out->plane->normal, out->normal);
		}
		else
			VectorCopy(out->plane->normal, out->normal);

		// then texinfo
		ti = LittleShort(in->texinfo);
		if(ti < 0 || ti >= r_loadmodel->num_texinfo){
			Com_Error(ERR_DROP, "R_LoadBspSurfaces: Bad texinfo number: %d.", ti);
		}
		out->texinfo = r_loadmodel->texinfo + ti;

		if(!(out->texinfo->flags & (SURF_WARP | SURF_SKY)))
			out->flags |= MSURF_LIGHTMAP;

		// and size, texcoords, etc
		R_SetupBspSurface(out);

		// lastly lighting info
		i = LittleLong(in->light_ofs);

		if(i != -1)
			out->samples = r_loadmodel->lightmap_data + i;
		else
			out->samples = NULL;

		// create lightmaps
		R_CreateSurfaceLightmap(out);

		// and flare
		R_CreateSurfaceFlare(out);
	}

	R_EndBuildingLightmaps();
}


/*
 * R_SetupBspNode
 */
static void R_SetupBspNode(r_bsp_node_t *node, r_bsp_node_t *parent){

	node->parent = parent;

	if(node->contents != CONTENTS_NODE)  // leaf
		return;

	R_SetupBspNode(node->children[0], node);
	R_SetupBspNode(node->children[1], node);
}


/*
 * R_LoadBspNodes
 */
static void R_LoadBspNodes(const d_bsp_lump_t *l){
	int i, j, count, p;
	const d_bsp_node_t *in;
	r_bsp_node_t *out;

	in = (const void *)(mod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadBspNodes: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->nodes = out;
	r_loadmodel->num_nodes = count;

	for(i = 0; i < count; i++, in++, out++){

		for(j = 0; j < 3; j++){
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->plane_num);
		out->plane = r_loadmodel->planes + p;

		out->first_surface = LittleShort(in->first_face);
		out->num_surfaces = LittleShort(in->num_faces);
		out->contents = CONTENTS_NODE;  // differentiate from leafs

		for(j = 0; j < 2; j++){
			p = LittleLong(in->children[j]);
			if(p >= 0)
				out->children[j] = r_loadmodel->nodes + p;
			else
				out->children[j] = (r_bsp_node_t *)(r_loadmodel->leafs + (-1 - p));
		}
	}

	R_SetupBspNode(r_loadmodel->nodes, NULL);  // sets nodes and leafs
}


/*
 * R_LoadBspLeafs
 */
static void R_LoadBspLeafs(const d_bsp_lump_t *l){
	const d_bsp_leaf_t *in;
	r_bsp_leaf_t *out;
	int i, j, count;

	in = (const void *)(mod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadBspLeafs: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->leafs = out;
	r_loadmodel->num_leafs = count;

	for(i = 0; i < count; i++, in++, out++){

		for(j = 0; j < 3; j++){
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
		}

		out->contents = LittleLong(in->contents);

		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		out->first_leaf_surface = r_loadmodel->leafsurfaces +
				LittleShort(in->first_leaf_face);

		out->num_leaf_surfaces = LittleShort(in->num_leaf_faces);
	}
}


/*
 * R_LoadBspLeafsurfaces
 */
static void R_LoadBspLeafsurfaces(const d_bsp_lump_t *l){
	int i, count;
	const unsigned short *in;
	r_bsp_surface_t **out;

	in = (const void *)(mod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadBspLeafsurfaces: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->leafsurfaces = out;
	r_loadmodel->num_leaf_surfaces = count;

	for(i = 0; i < count; i++){

		const unsigned short j = (unsigned short)LittleShort(in[i]);

		if(j >= r_loadmodel->num_surfaces){
			Com_Error(ERR_DROP, "R_LoadBspLeafsurfaces: Bad surface number: %d.", j);
		}

		out[i] = r_loadmodel->surfaces + j;
	}
}


/*
 * R_LoadBspSurfedges
 */
static void R_LoadBspSurfedges(const d_bsp_lump_t *l){
	int i, count;
	const int *in;
	int *out;

	in = (const void *)(mod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadBspSurfedges: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->file_len / sizeof(*in);
	if(count < 1 || count >= MAX_BSP_SURFEDGES){
		Com_Error(ERR_DROP, "R_LoadBspSurfedges: Bad surfedges count: %i.", count);
	}

	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->surfedges = out;
	r_loadmodel->num_surfedges = count;

	for(i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}


/*
 * R_LoadBspPlanes
 */
static void R_LoadBspPlanes(const d_bsp_lump_t *l){
	int i, j;
	cplane_t *out;
	const d_bsp_plane_t *in;
	int count;

	in = (const void *)(mod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadBspPlanes: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * 2 * sizeof(*out));

	r_loadmodel->planes = out;
	r_loadmodel->num_planes = count;

	for(i = 0; i < count; i++, in++, out++){
		int bits = 0;
		for(j = 0; j < 3; j++){
			out->normal[j] = LittleFloat(in->normal[j]);
			if(out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->sign_bits = bits;
	}
}


/*
 * R_AddBspVertexColor
 */
static void R_AddBspVertexColor(r_bsp_vertex_t *vert, const r_bsp_surface_t *surf){
	int i;

	vert->surfaces++;

	for(i = 0; i < 4; i++){

		const float vc = vert->color[i] * (vert->surfaces - 1) / vert->surfaces;
		const float sc = surf->color[i] / vert->surfaces;

		vert->color[i] = vc + sc;
	}
}


/*
 * R_LoadBspVertexArrays
 */
static void R_LoadBspVertexArrays(void){
	int i, j;
	int vertind, coordind, tangind, colorind;
	float soff, toff, s, t;
	float *point, *normal, *sdir, *tdir;
	vec4_t tangent;
	vec3_t bitangent;
	r_bsp_surface_t *surf;
	r_bsp_edge_t *edge;
	r_bsp_vertex_t *vert;

	R_AllocVertexArrays(r_loadmodel);  // allocate the arrays

	vertind = coordind = tangind = 0;
	surf = r_loadmodel->surfaces;

	for(i = 0; i < r_loadmodel->num_surfaces; i++, surf++){

		surf->index = vertind / 3;

		for(j = 0; j < surf->num_edges; j++){
			const int index = r_loadmodel->surfedges[surf->first_edge + j];

			// vertex
			if(index > 0){  // negative indices to differentiate which end of the edge
				edge = &r_loadmodel->edges[index];
				vert = &r_loadmodel->vertexes[edge->v[0]];
			} else {
				edge = &r_loadmodel->edges[-index];
				vert = &r_loadmodel->vertexes[edge->v[1]];
			}

			point = vert->position;
			memcpy(&r_loadmodel->verts[vertind], point, sizeof(vec3_t));

			// texture directional vectors and offsets
			sdir = surf->texinfo->vecs[0];
			soff = surf->texinfo->vecs[0][3];

			tdir = surf->texinfo->vecs[1];
			toff = surf->texinfo->vecs[1][3];

			// texture coordinates
			s = DotProduct(point, sdir) + soff;
			s /= surf->texinfo->image->width;

			t = DotProduct(point, tdir) + toff;
			t /= surf->texinfo->image->height;

			r_loadmodel->texcoords[coordind + 0] = s;
			r_loadmodel->texcoords[coordind + 1] = t;

			if(surf->flags & MSURF_LIGHTMAP){  // lightmap coordinates
				s = DotProduct(point, sdir) + soff;
				s -= surf->st_mins[0];
				s += surf->light_s * r_loadmodel->lightmap_scale;
				s += r_loadmodel->lightmap_scale / 2.0;
				s /= r_lightmaps.size * r_loadmodel->lightmap_scale;

				t = DotProduct(point, tdir) + toff;
				t -= surf->st_mins[1];
				t += surf->light_t * r_loadmodel->lightmap_scale;
				t += r_loadmodel->lightmap_scale / 2.0;
				t /= r_lightmaps.size * r_loadmodel->lightmap_scale;
			}

			r_loadmodel->lmtexcoords[coordind + 0] = s;
			r_loadmodel->lmtexcoords[coordind + 1] = t;

			// normal vector
			if(surf->texinfo->flags & SURF_PHONG &&
					!VectorCompare(vert->normal, vec3_origin))  // phong shaded
				normal = vert->normal;
			else  // per-plane
				normal = surf->normal;

			memcpy(&r_loadmodel->normals[vertind], normal, sizeof(vec3_t));

			// tangent vector
			TangentVectors(normal, sdir, tdir, tangent, bitangent);
			memcpy(&r_loadmodel->tangents[tangind], tangent, sizeof(vec4_t));

			// accumulate colors
			R_AddBspVertexColor(vert, surf);

			vertind += 3;
			coordind += 2;
			tangind += 4;
		}
	}

	colorind = 0;
	surf = r_loadmodel->surfaces;

	// now iterate over the verts again, assembling the accumulated colors
	for(i = 0; i < r_loadmodel->num_surfaces; i++, surf++){

		for(j = 0; j < surf->num_edges; j++){
			const int index = r_loadmodel->surfedges[surf->first_edge + j];

			// vertex
			if(index > 0){  // negative indices to differentiate which end of the edge
				edge = &r_loadmodel->edges[index];
				vert = &r_loadmodel->vertexes[edge->v[0]];
			} else {
				edge = &r_loadmodel->edges[-index];
				vert = &r_loadmodel->vertexes[edge->v[1]];
			}

			memcpy(&r_loadmodel->colors[colorind], vert->color, sizeof(vec4_t));
			colorind += 4;
		}
	}
}


// temporary space for sorting surfaces by texnum
static r_bsp_surfaces_t *r_sorted_surfaces[MAX_GL_TEXTURES];

/*
 * R_SortBspSurfacesArrays_
 */
static void R_SortBspSurfacesArrays_(r_bsp_surfaces_t *surfs){
	int i, j;

	for(i = 0; i < surfs->count; i++){

		const int texnum = surfs->surfaces[i]->texinfo->image->texnum;

		R_SurfaceToSurfaces(r_sorted_surfaces[texnum], surfs->surfaces[i]);
	}

	surfs->count = 0;

	for(i = 0; i < r_num_images; i++){

		r_bsp_surfaces_t *sorted = r_sorted_surfaces[r_images[i].texnum];

		if(sorted && sorted->count){

			for(j = 0; j < sorted->count; j++)
				R_SurfaceToSurfaces(surfs, sorted->surfaces[j]);

			sorted->count = 0;
		}
	}
}


/*
 * R_SortBspSurfacesArrays
 *
 * Reorders all surfaces arrays for the specified model, grouping the surface
 * pointers by texture.  This dramatically reduces glBindTexture calls.
 */
static void R_SortBspSurfacesArrays(r_model_t *mod){
	r_bsp_surface_t *surf, *s;
	int i, ns;

	// resolve the start surface and total surface count
	if(mod->type == mod_bsp){  // world model
		s = mod->surfaces;
		ns = mod->num_surfaces;
	}
	else {  // submodels
		s = &mod->surfaces[mod->first_model_surface];
		ns = mod->num_model_surfaces;
	}

	memset(r_sorted_surfaces, 0, sizeof(r_sorted_surfaces));

	// allocate the per-texture surfaces arrays and determine counts
	for(i = 0, surf = s; i < ns; i++, surf++){

		r_bsp_surfaces_t *surfs = r_sorted_surfaces[surf->texinfo->image->texnum];

		if(!surfs){  // allocate it
			surfs = (r_bsp_surfaces_t *)Z_Malloc(sizeof(*surfs));
			r_sorted_surfaces[surf->texinfo->image->texnum] = surfs;
		}

		surfs->count++;
	}

	// allocate the surfaces pointers based on counts
	for(i = 0; i < r_num_images; i++){

		r_bsp_surfaces_t *surfs = r_sorted_surfaces[r_images[i].texnum];

		if(surfs){
			surfs->surfaces = (r_bsp_surface_t **)Z_Malloc(sizeof(r_bsp_surface_t *) * surfs->count);
			surfs->count = 0;
		}
	}

	// sort the model's surfaces arrays into the per-texture arrays
	for(i = 0; i < NUM_SURFACES_ARRAYS; i++){

		if(mod->sorted_surfaces[i]->count)
			R_SortBspSurfacesArrays_(mod->sorted_surfaces[i]);
	}

	// free the per-texture surfaces arrays
	for(i = 0; i < r_num_images; i++){

		r_bsp_surfaces_t *surfs = r_sorted_surfaces[r_images[i].texnum];

		if(surfs){
			if(surfs->surfaces)
				Z_Free(surfs->surfaces);
			Z_Free(surfs);
		}
	}
}


/*
 * R_LoadBspSurfacesArrays_
 */
static void R_LoadBspSurfacesArrays_(r_model_t *mod){
	r_bsp_surface_t *surf, *s;
	int i, ns;

	// allocate the surfaces array structures
	for(i = 0; i < NUM_SURFACES_ARRAYS; i++)
		mod->sorted_surfaces[i] = (r_bsp_surfaces_t *)R_HunkAlloc(sizeof(r_bsp_surfaces_t));

	// resolve the start surface and total surface count
	if(mod->type == mod_bsp){  // world model
		s = mod->surfaces;
		ns = mod->num_surfaces;
	}
	else {  // submodels
		s = &mod->surfaces[mod->first_model_surface];
		ns = mod->num_model_surfaces;
	}

	// determine the maximum counts for each rendered type in order to
	// allocate only what is necessary for the specified model
	for(i = 0, surf = s; i < ns; i++, surf++){

		if(surf->texinfo->flags & SURF_SKY){
			mod->sky_surfaces->count++;
			continue;
		}

		if(surf->texinfo->flags & (SURF_BLEND33 | SURF_BLEND66)){
			if(surf->texinfo->flags & SURF_WARP)
				mod->blend_warp_surfaces->count++;
			else
				mod->blend_surfaces->count++;
		} else {
			if(surf->texinfo->flags & SURF_WARP)
				mod->opaque_warp_surfaces->count++;
			else if(surf->texinfo->flags & SURF_ALPHATEST)
				mod->alpha_test_surfaces->count++;
			else
				mod->opaque_surfaces->count++;
		}

		if(surf->texinfo->image->material.flags & STAGE_RENDER)
			mod->material_surfaces->count++;

		if(surf->texinfo->image->material.flags & STAGE_FLARE)
			mod->flare_surfaces->count++;

		if(!(surf->texinfo->flags & SURF_WARP))
			mod->back_surfaces->count++;
	}

	// allocate the surfaces pointers based on the counts
	for(i = 0; i < NUM_SURFACES_ARRAYS; i++){

		if(mod->sorted_surfaces[i]->count){
			mod->sorted_surfaces[i]->surfaces = (r_bsp_surface_t **)R_HunkAlloc(
					sizeof(r_bsp_surface_t *) * mod->sorted_surfaces[i]->count);

			mod->sorted_surfaces[i]->count = 0;
		}
	}

	// iterate the surfaces again, populating the allocated arrays based
	// on primary render type
	for(i = 0, surf = s; i < ns; i++, surf++){

		if(surf->texinfo->flags & SURF_SKY){
			R_SurfaceToSurfaces(mod->sky_surfaces, surf);
			continue;
		}

		if(surf->texinfo->flags & (SURF_BLEND33 | SURF_BLEND66)){
			if(surf->texinfo->flags & SURF_WARP)
				R_SurfaceToSurfaces(mod->blend_warp_surfaces, surf);
			else
				R_SurfaceToSurfaces(mod->blend_surfaces, surf);
		} else {
			if(surf->texinfo->flags & SURF_WARP)
				R_SurfaceToSurfaces(mod->opaque_warp_surfaces, surf);
			else if(surf->texinfo->flags & SURF_ALPHATEST)
				R_SurfaceToSurfaces(mod->alpha_test_surfaces, surf);
			else
				R_SurfaceToSurfaces(mod->opaque_surfaces, surf);
		}

		if(surf->texinfo->image->material.flags & STAGE_RENDER)
			R_SurfaceToSurfaces(mod->material_surfaces, surf);

		if(surf->texinfo->image->material.flags & STAGE_FLARE)
			R_SurfaceToSurfaces(mod->flare_surfaces, surf);

		if(!(surf->texinfo->flags & SURF_WARP))
			R_SurfaceToSurfaces(mod->back_surfaces, surf);
	}

	// now sort them by texture
	R_SortBspSurfacesArrays(mod);
}


/*
 * R_LoadBspSurfacesArrays
 */
static void R_LoadBspSurfacesArrays(void){
	int i;

	R_LoadBspSurfacesArrays_(r_loadmodel);

	for(i = 0; i < r_loadmodel->num_submodels; i++){
		R_LoadBspSurfacesArrays_(&r_inline_models[i]);
	}
}


/*
 * R_AddBspLight
 *
 * Adds the specified static light source to the world model, after first
 * ensuring that it can not be merged with any known sources.
 */
static void R_AddBspLight(vec3_t org, float radius){
	r_bsp_light_t *l;
	vec3_t delta;
	int i;

	l = r_loadmodel->bsp_lights;
	for(i = 0; i < r_loadmodel->num_bsp_lights; i++, l++){

		VectorSubtract(org, l->org, delta);

		if(VectorLength(delta) <= 32.0)  // merge them
			break;
	}

	if(i == r_loadmodel->num_bsp_lights){
		// we can assume that all the bsplight memory is
		// next to each other because of the hunk allocation
		// implementation.
		l = (r_bsp_light_t *)R_HunkAlloc(sizeof(*l));

		VectorCopy(org, l->org);
		l->leaf = R_LeafForPoint(l->org, r_loadmodel);

		if(!r_loadmodel->bsp_lights)  // first source
			r_loadmodel->bsp_lights = l;

		r_loadmodel->num_bsp_lights++;
	}

	l->radius += radius;

	if(l->radius > 250.0)  // clamp
		l->radius = 250.0;
}


/*
 * R_LoadBspLights
 *
 * Parse the entity string and resolve all static light sources.  Also
 * iterate the world surfaces, allocating static light sources for those
 * which emit light.
 */
static void R_LoadBspLights(void){
	const char *ents;
	char class_name[128];
	vec3_t org, tmp;
	float radius;
	qboolean entity, light;
	r_bsp_surface_t *surf;
	int i;

	// iterate the world surfaces for surface lights
	surf = r_loadmodel->surfaces;

	VectorClear(org);
	radius = 0.0;

	for(i = 0; i < r_loadmodel->num_surfaces; i++, surf++){

		if((surf->texinfo->flags & SURF_LIGHT) && surf->texinfo->value){
			VectorMA(surf->center, 1.0, surf->normal, org);

			VectorSubtract(surf->maxs, surf->mins, tmp);
			radius = VectorLength(tmp);

			R_AddBspLight(org, radius > 100.0 ? radius : 100.0);
		}
	}

	// parse the entity string for point lights
	ents = Cm_EntityString();

	VectorClear(org);
	radius = 0.0;

	memset(class_name, 0, sizeof(class_name));
	entity = light = false;

	while(true){

		const char *c = Com_Parse(&ents);

		if(!strlen(c))
			break;

		if(*c == '{')
			entity = true;

		if(!entity)  // skip any whitespace between ents
			continue;

		if(*c == '}'){
			entity = false;

			if(light){  // add it

				if(radius <= 0.0)  // clamp it
					radius = 100.0;

				R_AddBspLight(org, radius);

				light = false;
				radius = 0.0;
			}
		}

		if(!strcmp(c, "classname")){

			c = Com_Parse(&ents);
			strncpy(class_name, c, sizeof(class_name) - 1);

			if(!strncmp(c, "light", 5))  // light, light_spot, etc..
				light = true;

			continue;
		}

		if(!strcmp(c, "origin")){
			sscanf(Com_Parse(&ents), "%f %f %f", &org[0], &org[1], &org[2]);
			continue;
		}

		if(!strcmp(c, "light")){
			radius = atof(Com_Parse(&ents));
			continue;
		}
	}

	Com_Debug("Loaded %d bsp lights\n", r_loadmodel->num_bsp_lights);
}


/*
 * R_SetupBspSubmodel
 *
 * Recurses the specified submodel nodes, assigning the model so that it can
 * be quickly resolved during traces and dynamic light processing.
 */
static void R_SetupBspSubmodel(r_bsp_node_t *node, r_model_t *model){

	node->model = model;

	if(node->contents != CONTENTS_NODE)
		return;

	R_SetupBspSubmodel(node->children[0], model);
	R_SetupBspSubmodel(node->children[1], model);
}


/*
 * R_SetupBspSubmodels
 *
 * The submodels have been loaded into memory, but are not yet
 * represented as model_t.  Convert them.
 */
static void R_SetupBspSubmodels(void){
	int i;

	for(i = 0; i < r_loadmodel->num_submodels; i++){

		r_model_t *mod = &r_inline_models[i];
		const r_bsp_submodel_t *sub = &r_loadmodel->submodels[i];

		*mod = *r_loadmodel;  // copy most info from world
		mod->type = mod_bsp_submodel;

		snprintf(mod->name, sizeof(mod->name), "*%d", i);

		// copy the rest from the submodel
		VectorCopy(sub->maxs, mod->maxs);
		VectorCopy(sub->mins, mod->mins);
		mod->radius = sub->radius;

		mod->firstnode = sub->head_node;
		mod->nodes = &r_loadmodel->nodes[mod->firstnode];

		R_SetupBspSubmodel(mod->nodes, mod);

		mod->first_model_surface = sub->first_face;
		mod->num_model_surfaces = sub->num_faces;
	}
}


/*
 * R_LoadBspModel
 */
void R_LoadBspModel(r_model_t *mod, void *buffer){
	extern void Cl_LoadProgress(int percent);
	d_bsp_header_t *header;
	int i, version;

	if(r_worldmodel){
		Com_Error(ERR_DROP, "R_LoadBspModel: Loaded bsp model after world.");
	}

	header = (d_bsp_header_t *)buffer;

	version = LittleLong(header->version);
	if(version != BSP_VERSION && version != BSP_VERSION_){
		Com_Error(ERR_DROP, "R_LoadBspModel: %s has unsupported version: %d",
				mod->name, version);
	}

	mod->type = mod_bsp;
	mod->version = version;

	// swap all the lumps
	mod_base = (byte *)header;

	for(i = 0; i < sizeof(*header) / 4; i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	// load into heap
	R_LoadBspVertexes(&header->lumps[LUMP_VERTEXES]);
	Cl_LoadProgress( 4);

	if(header->version == BSP_VERSION_)  // enhanced format
		R_LoadBspNormals(&header->lumps[LUMP_NORMALS]);

	R_LoadBspEdges(&header->lumps[LUMP_EDGES]);
	Cl_LoadProgress( 8);

	R_LoadBspSurfedges(&header->lumps[LUMP_SURFEDGES]);
	Cl_LoadProgress(12);

	R_LoadBspLighting(&header->lumps[LUMP_LIGHTING]);
	Cl_LoadProgress(16);

	R_LoadBspPlanes(&header->lumps[LUMP_PLANES]);
	Cl_LoadProgress(20);

	R_LoadBspTexinfo(&header->lumps[LUMP_TEXINFO]);
	Cl_LoadProgress(24);

	R_LoadBspSurfaces(&header->lumps[LUMP_FACES]);
	Cl_LoadProgress(28);

	R_LoadBspLeafsurfaces(&header->lumps[LUMP_LEAFFACES]);
	Cl_LoadProgress(32);

	R_LoadBspVisibility(&header->lumps[LUMP_VISIBILITY]);
	Cl_LoadProgress(36);

	R_LoadBspLeafs(&header->lumps[LUMP_LEAFS]);
	Cl_LoadProgress(40);

	R_LoadBspNodes(&header->lumps[LUMP_NODES]);
	Cl_LoadProgress(44);

	R_LoadBspSubmodels(&header->lumps[LUMP_MODELS]);
	Cl_LoadProgress(48);

	Com_Debug("================================\n");
	Com_Debug("R_LoadBspModel: %s\n", r_loadmodel->name);
	Com_Debug("  Verts:      %d\n", r_loadmodel->num_vertexes);
	Com_Debug("  Edges:      %d\n", r_loadmodel->num_edges);
	Com_Debug("  Surfedges:  %d\n", r_loadmodel->num_surfedges);
	Com_Debug("  Faces:      %d\n", r_loadmodel->num_surfaces);
	Com_Debug("  Nodes:      %d\n", r_loadmodel->num_nodes);
	Com_Debug("  Leafs:      %d\n", r_loadmodel->num_leafs);
	Com_Debug("  Leaf faces: %d\n", r_loadmodel->num_leaf_surfaces);
	Com_Debug("  Models:     %d\n", r_loadmodel->num_submodels);
	Com_Debug("  Lightdata:  %d\n", r_loadmodel->lightmap_data_size);
	Com_Debug("  Vis:        %d\n", r_loadmodel->vis_size);

	if(r_loadmodel->vis)
		Com_Debug("  Clusters:   %d\n", r_loadmodel->vis->num_clusters);

	Com_Debug("================================\n");

	R_LoadBspLights();

	R_SetupBspSubmodels();

	R_LoadBspVertexArrays();

	R_LoadBspSurfacesArrays();

	memset(&r_locals, 0, sizeof(r_locals));
	r_locals.old_cluster = -1;  // force bsp iteration
}
