#include "PitCollection.h"

#include "Renderer/TextureManager.h"

#include <algorithm>

BRWL_RENDERER_NS


PitCollection::PitCollection() :
	tables {
		BaseTextureHandle::Invalid,
		BaseTextureHandle::Invalid,
		BaseTextureHandle::Invalid,
		BaseTextureHandle::Invalid
	}
{ }


PitCollection::~PitCollection()
{
	destroy();
	tables.~Aliases();
}

void PitCollection::create(BaseTextureManager* mgr)
{
	tables = {
		mgr->createTexture<PitImage>(BRWL_CHAR_LITERAL("Refraction PIT")),
		mgr->createTexture<PitImage>(BRWL_CHAR_LITERAL("Particle Color PIT")),
		mgr->createTexture<PitImage>(BRWL_CHAR_LITERAL("Opacity PIT")),
		mgr->createTexture<PitImage>(BRWL_CHAR_LITERAL("Medium Color PIT"))
	};
}

bool PitCollection::isResident() const
{
	return std::all_of(array, array + countof(array), [](const BaseTextureHandle& p) { return p.isResident(); });
}

void PitCollection::destroy()
{
	for (int i = 0; i < countof(array); ++i)
	{
		// destroy gpu resources
		array[i].destroy();
	}
}


BRWL_RENDERER_NS_END

