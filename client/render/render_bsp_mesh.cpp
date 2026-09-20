// Shared BSP triangulator. Provenance: CS Retro world capture (PX3B) plus
// Xash surface walk (firstmodelsurface/nummodelsurfaces). PrimeXT
// refs/primext/client/render/gl_rsurf.cpp 46fb05b only for texture-anim
// semantics (not used at cache time). No second BSP parser.
#include "render_bsp_mesh.h"
#include "render_backend.h"
#include "render_xash_brush.h"

#include <stdlib.h>
#include <string.h>

static void AddTri( CSRETRO_MeshVert **cursor, int *used, int cap, const float *a, const float *b, const float *c )
{
	CSRETRO_MeshVert *v;
	if( *used + 3 > cap )
		return;
	v = *cursor;
	memcpy( v[0].xyz, a, sizeof( float ) * 3 );
	v[0].st[0] = a[3];
	v[0].st[1] = a[4];
	v[0].lm[0] = a[5];
	v[0].lm[1] = a[6];
	memcpy( v[1].xyz, b, sizeof( float ) * 3 );
	v[1].st[0] = b[3];
	v[1].st[1] = b[4];
	v[1].lm[0] = b[5];
	v[1].lm[1] = b[6];
	memcpy( v[2].xyz, c, sizeof( float ) * 3 );
	v[2].st[0] = c[3];
	v[2].st[1] = c[4];
	v[2].lm[0] = c[5];
	v[2].lm[1] = c[6];
	*cursor += 3;
	*used += 3;
}

void CSRETRO_BspMesh_Clear( CSRETRO_BspMesh *mesh )
{
	if( !mesh )
		return;
	free( mesh->verts );
	free( mesh->batches );
	memset( mesh, 0, sizeof( *mesh ) );
}

int CSRETRO_BspMesh_Build( CSRETRO_BspMesh *mesh, void *modp )
{
	xr_model_t *mod = (xr_model_t *)modp;
	int first, count, i, guess_verts = 0, used = 0, polys = 0, surfaces = 0;
	int lm_max = -1, flags_union = 0, skipped_turb = 0, skipped_sky = 0;
	CSRETRO_MeshVert *cursor;
	CSRETRO_MeshBatch *batch = NULL;

	if( !mesh )
		return 0;
	CSRETRO_BspMesh_Clear( mesh );

	if( !mod || !mod->surfaces || mod->numsurfaces <= 0 || mod->numsurfaces > 262144 )
		return 0;

	first = mod->firstmodelsurface;
	count = mod->nummodelsurfaces;
	if( count <= 0 || first < 0 || first + count > mod->numsurfaces )
	{
		first = 0;
		count = mod->numsurfaces;
	}

	for( i = 0; i < count; i++ )
	{
		xr_msurface_t *surf = &mod->surfaces[first + i];
		xr_glpoly_t *p;
		if( surf->flags & XR_SURF_DRAWSKY )
			continue;
		if( surf->flags & XR_SURF_DRAWTURB )
			continue;
		if( !surf->polys )
		{
			if( surf->numedges >= 3 )
				guess_verts += ( surf->numedges - 2 ) * 3;
			continue;
		}
		for( p = surf->polys; p; p = p->next )
		{
			if( p->numverts >= 3 )
				guess_verts += ( p->numverts - 2 ) * 3;
		}
	}

	if( guess_verts <= 0 )
		return 0;

	mesh->verts = (CSRETRO_MeshVert *)malloc( sizeof( CSRETRO_MeshVert ) * (size_t)guess_verts );
	mesh->batches = (CSRETRO_MeshBatch *)malloc( sizeof( CSRETRO_MeshBatch ) * XR_MESH_MAX_BATCH );
	if( !mesh->verts || !mesh->batches )
	{
		CSRETRO_BspMesh_Clear( mesh );
		return 0;
	}

	cursor = mesh->verts;

	for( i = 0; i < count; i++ )
	{
		xr_msurface_t *surf = &mod->surfaces[first + i];
		xr_glpoly_t *p;
		unsigned int tex = 0, lm = 0;

		if( surf->flags & XR_SURF_DRAWSKY )
		{
			skipped_sky++;
			continue;
		}
		if( surf->flags & XR_SURF_DRAWTURB )
		{
			skipped_turb++;
			continue;
		}

		flags_union |= surf->flags;

		if( surf->texinfo && surf->texinfo->texture )
			tex = (unsigned int)surf->texinfo->texture->gl_texturenum;
		if( !( surf->flags & XR_SURF_DRAWTILED ) && surf->lightmaptexturenum >= 0 )
			lm = (unsigned int)surf->lightmaptexturenum;

		if( !batch || batch->tex != tex || batch->lightmap != lm || batch->flags != surf->flags )
		{
			if( mesh->batch_count >= XR_MESH_MAX_BATCH )
				break;
			batch = &mesh->batches[mesh->batch_count++];
			batch->tex = tex;
			batch->lightmap = lm;
			batch->first_tri = used;
			batch->tri_count = 0;
			batch->flags = surf->flags;
		}

		if( surf->polys )
		{
			for( p = surf->polys; p; p = p->next )
			{
				int v;
				if( p->numverts < 3 )
					continue;
				polys++;
				for( v = 1; v + 1 < p->numverts; v++ )
				{
					AddTri( &cursor, &used, guess_verts, p->verts[0], p->verts[v], p->verts[v + 1] );
					batch->tri_count++;
				}
			}
			surfaces++;
		}
		else if( surf->numedges >= 3 && mod->surfedges && mod->vertexes && mod->edges16 )
		{
			float fan[64][XR_VERTEXSIZE];
			int e, n = surf->numedges;
			if( n > 64 )
				n = 64;
			for( e = 0; e < n; e++ )
			{
				int l = mod->surfedges[surf->firstedge + e];
				int idx = l < 0 ? -l : l;
				int vert;
				if( idx < 0 || idx >= mod->numedges )
					break;
				vert = (int)mod->edges16[idx].v[l < 0 ? 1 : 0];
				if( vert < 0 || vert >= mod->numvertexes )
					break;
				fan[e][0] = mod->vertexes[vert].position[0];
				fan[e][1] = mod->vertexes[vert].position[1];
				fan[e][2] = mod->vertexes[vert].position[2];
				if( surf->texinfo )
				{
					fan[e][3] = fan[e][0] * surf->texinfo->vecs[0][0] + fan[e][1] * surf->texinfo->vecs[0][1]
						+ fan[e][2] * surf->texinfo->vecs[0][2] + surf->texinfo->vecs[0][3];
					fan[e][4] = fan[e][0] * surf->texinfo->vecs[1][0] + fan[e][1] * surf->texinfo->vecs[1][1]
						+ fan[e][2] * surf->texinfo->vecs[1][2] + surf->texinfo->vecs[1][3];
					if( surf->texinfo->texture && surf->texinfo->texture->width )
						fan[e][3] /= (float)surf->texinfo->texture->width;
					if( surf->texinfo->texture && surf->texinfo->texture->height )
						fan[e][4] /= (float)surf->texinfo->texture->height;
				}
				else
					fan[e][3] = fan[e][4] = 0.0f;
				fan[e][5] = fan[e][6] = 0.0f;
			}
			if( e == n )
			{
				int v;
				polys++;
				surfaces++;
				for( v = 1; v + 1 < n; v++ )
				{
					AddTri( &cursor, &used, guess_verts, fan[0], fan[v], fan[v + 1] );
					batch->tri_count++;
				}
			}
		}

		if( (int)lm > lm_max )
			lm_max = (int)lm;
	}

	mesh->model = mod;
	mesh->firstmodelsurface = first;
	mesh->nummodelsurfaces = count;
	mesh->vert_count = used;
	mesh->lightmap_pages = lm_max >= 0 ? lm_max + 1 : 0;
	mesh->flags_union = flags_union;
	mesh->surfaces = surfaces;
	mesh->polys = polys;
	mesh->skipped_turb = skipped_turb;
	mesh->skipped_sky = skipped_sky;
	mesh->captured = used > 0;
	return mesh->captured;
}

void CSRETRO_BspMesh_Draw( const CSRETRO_BspMesh *mesh, long ( *get_parm )( int parm, int arg ), int bind_textures )
{
	int b;

	if( !mesh || !mesh->captured || mesh->vert_count < 3 || !gXRGL.Begin )
		return;

	if( gXRGL.Enable )
		gXRGL.Enable( 0x0DE1 ); // GL_TEXTURE_2D

	for( b = 0; b < mesh->batch_count; b++ )
	{
		const CSRETRO_MeshBatch *batch = &mesh->batches[b];
		int i, has_lm;
		if( batch->tri_count <= 0 )
			continue;

		if( bind_textures )
			CSRETRO_Backend_BindTexture( 0, batch->tex );
		has_lm = bind_textures && ( batch->lightmap != 0 || ( mesh->lightmap_pages > 0 && !( batch->flags & XR_SURF_DRAWTILED ) ) );
		if( has_lm && get_parm )
		{
			int lmtex = (int)get_parm( 7, (int)batch->lightmap ); // PARM_TEX_LIGHTMAP
			if( lmtex > 0 && gXRGL.ActiveTexture )
			{
				CSRETRO_Backend_BindTexture( 1, (unsigned int)lmtex );
				gXRGL.ActiveTexture( 0x84C1 );
				gXRGL.Enable( 0x0DE1 );
				if( gXRGL.TexEnvi )
					gXRGL.TexEnvi( 0x2300, 0x2200, 0x2100 );
				gXRGL.ActiveTexture( 0x84C0 );
			}
			else
				has_lm = 0;
		}
		else
			has_lm = 0;

		gXRGL.Begin( 0x0004 ); // GL_TRIANGLES
		for( i = 0; i < batch->tri_count * 3; i++ )
		{
			const CSRETRO_MeshVert *v = &mesh->verts[batch->first_tri + i];
			if( gXRGL.TexCoord2f )
				gXRGL.TexCoord2f( v->st[0], v->st[1] );
			if( has_lm && gXRGL.MultiTexCoord2f )
				gXRGL.MultiTexCoord2f( 0x84C1, v->lm[0], v->lm[1] );
			gXRGL.Vertex3f( v->xyz[0], v->xyz[1], v->xyz[2] );
		}
		gXRGL.End();
	}

	if( bind_textures )
		CSRETRO_Backend_CleanupTextures();
}
