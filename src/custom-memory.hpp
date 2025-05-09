#ifndef __CUSTOM_MEMORY_H_F8A78A44_37A7_49C3_BD6D_BCDE7027F75B__
#define __CUSTOM_MEMORY_H_F8A78A44_37A7_49C3_BD6D_BCDE7027F75B__

#if defined _DEBUG
#define MIMALLOC_ALLOW_DECOMMIT     0
#define MIMALLOC_VERBOSE            1
#define MIMALLOC_SHOW_ERRORS        1
#define MIMALLOC_SHOW_STATS         1
#endif

#include <mimalloc-override.h>
#include <mimalloc-new-delete.h>

#endif __CUSTOM_MEMORY_H_F8A78A44_37A7_49C3_BD6D_BCDE7027F75B__