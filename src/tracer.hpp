#pragma once

#ifdef ENABLE_TRACY
#include <tracy/Tracy.hpp>

#define TRACY_ZONE			ZoneScoped
#define TRACY_ZONE_NAMED(x)	ZoneScopedN(x)
#else
#define TRACY_ZONE 
#define TRACY_ZONE_NAMED(x)
#endif