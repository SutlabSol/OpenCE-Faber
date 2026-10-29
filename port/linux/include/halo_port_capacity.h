/*
HALO_PORT_CAPACITY.H

Memory capacity of the native builds (Windows, Linux, Android), sized for the
session limits in halo_port_limits.h, which includes this file. The Xbox
sizes are given in parentheses below.

Every machine in a session must be built with the same values: the
distributed netcode names objects and players by their datum index, the
same on every machine (port/linux/game/network_objects.c tracks
MAXIMUM_TRACKED_OBJECTS = HALO_PORT_MAXIMUM_OBJECTS_PER_MAP objects, and a
client's own objects take the upper half of the object array).
*/

#ifndef __HALO_PORT_CAPACITY_H
#define __HALO_PORT_CAPACITY_H

/* ---------- game state */

#if defined(HALO_WEB) || (defined(HALO_GUEST) && defined(__aarch64__)) || defined(HALO_ARM64_GUEST)
#define HALO_PORT_TAG_CACHE_SIZE 0x1600000 /* (0x1600000) */
#define HALO_PORT_GAME_STATE_BASE_ADDRESS 0x81A00000 /* (0x80061000) */
#else
#define HALO_PORT_TAG_CACHE_SIZE 0x07A00000 /* 122 MB (0x1600000) */
#define HALO_PORT_GAME_STATE_BASE_ADDRESS 0x87E00000 /* above 0x803A6000 + 0x07A00000 (0x80061000) */
#endif
#define HALO_PORT_GAME_STATE_CPU_SIZE 0x13C0000 /* (0x305000) */
#define HALO_PORT_GAME_STATE_GPU_SIZE 0x40000 /* (0x40000) */
#define HALO_PORT_GAME_STATE_SIZE (HALO_PORT_GAME_STATE_CPU_SIZE+HALO_PORT_GAME_STATE_GPU_SIZE)

/* ---------- textures */

#ifdef HALO_ANDROID
extern unsigned long platform_contiguous_size;
#define HALO_PORT_TEXTURE_CACHE_PAGE_COUNT \
	(platform_contiguous_size > 0x08000000UL ? platform_contiguous_size / 2 / 0x4000 : 0x580) /* (0x580) */
#define HALO_PORT_TEXTURE_CACHE_MAXIMUM_PAGE_COUNT 0x4000
#elif defined(HALO_WEB) || (defined(HALO_GUEST) && defined(__aarch64__)) || defined(HALO_ARM64_GUEST)
#define HALO_PORT_TEXTURE_CACHE_PAGE_COUNT 0x580 /* (0x580) */
#define HALO_PORT_TEXTURE_CACHE_MAXIMUM_PAGE_COUNT HALO_PORT_TEXTURE_CACHE_PAGE_COUNT
#else
#define HALO_PORT_TEXTURE_CACHE_PAGE_COUNT 0x4000 /* (0x580) */
#define HALO_PORT_TEXTURE_CACHE_MAXIMUM_PAGE_COUNT HALO_PORT_TEXTURE_CACHE_PAGE_COUNT
#endif
#define HALO_PORT_TEXTURE_CACHE_SIZE (HALO_PORT_TEXTURE_CACHE_PAGE_COUNT*0x4000) /* (0x1600000) */

/* ---------- sounds */

#if defined(HALO_WEB) || (defined(HALO_GUEST) && defined(__aarch64__)) || defined(HALO_ARM64_GUEST)
#define HALO_PORT_SOUND_CACHE_SIZE 0x400000 /* (0x400000) */
#else
#define HALO_PORT_SOUND_CACHE_SIZE 0x1000000 /* (0x400000) */
#endif

/* ---------- particles and lights a frame draws */

#if defined(HALO_ARM64_GUEST) || defined(HALO_WEB)
#define HALO_PORT_MAXIMUM_RENDERED_PARTICLE_GROUPS 512 /* (512) */
#define HALO_PORT_MAXIMUM_RENDERED_LIGHTS 128 /* (128) */
#else
#define HALO_PORT_MAXIMUM_RENDERED_PARTICLE_GROUPS 2048 /* (512) */
#define HALO_PORT_MAXIMUM_RENDERED_LIGHTS 512 /* (128) */
#endif

/* ---------- AI */

#define HALO_PORT_MAXIMUM_ACTORS 1024 /* (256) */
#define HALO_PORT_MAXIMUM_PROPS 8192 /* (768) */
#define HALO_PORT_MAXIMUM_SWARMS 128 /* (32) */
#define HALO_PORT_MAXIMUM_SWARM_COMPONENTS 1024 /* (256) */

/* ---------- objects */

#define HALO_PORT_MAXIMUM_OBJECTS_PER_MAP 8192 /* (2048) */
#define HALO_PORT_OBJECT_MEMORY_POOL_SIZE 0x800000 /* (0x100000) */
#define HALO_PORT_MAXIMUM_CLUSTER_REFERENCES 8192 /* (2048) */
#define HALO_PORT_MAXIMUM_RENDERED_OBJECTS 1024 /* (256) */
#define HALO_PORT_MAXIMUM_CACHED_OBJECT_RENDER_STATES 1024 /* (256) */
#define HALO_PORT_MAXIMUM_AREA_OF_EFFECT_OBJECTS 256 /* (64) */
#define HALO_PORT_MAXIMUM_OBJECT_LISTS_PER_MAP 1024 /* (48) */
#define HALO_PORT_MAXIMUM_LISTED_OBJECTS_PER_MAP 8192 /* (128) */
#define HALO_PORT_MAXIMUM_WIDGETS 2048 /* (64) */
#define HALO_PORT_MAXIMUM_LIGHT_VOLUMES 2048 /* (256) */

/* ---------- effects, particles, lights and sounds */

#define HALO_PORT_MAXIMUM_EFFECTS 2048 /* (256) */
#define HALO_PORT_MAXIMUM_EFFECT_LOCATIONS 4096 /* (512) */
#define HALO_PORT_MAXIMUM_PARTICLES 8192 /* (1024) */
#define HALO_PORT_MAXIMUM_PARTICLE_SYSTEMS 256 /* (64) */
#define HALO_PORT_MAXIMUM_SYSTEM_PARTICLES 4096 /* (512) */
#define HALO_PORT_MAXIMUM_CONTRAILS 1024 /* (256) */
#define HALO_PORT_MAXIMUM_CONTRAIL_POINTS 8192 /* (1024) */
#define HALO_PORT_MAXIMUM_LIGHTS_PER_MAP 4096 /* (896) */
#define HALO_PORT_MAXIMUM_GAME_LOOPING_SOUNDS 4096 /* (1024) */

/* ---------- rasterizer */

#define HALO_PORT_MAXIMUM_TRANSPARENT_GEOMETRY_GROUPS 2048 /* (384) */

#endif /* __HALO_PORT_CAPACITY_H */
