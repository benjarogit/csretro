#include "engine.h"

#include <ncl_perf/profiler.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/types.h>

#include "common/model.h"
#include "common/zone.h"

goldsrc::cache_user_t model_texture_cache[goldsrc::MAX_KNOWN_MODELS][32];

int* R_StudioReloadSkin(goldsrc::model_t* pModel, int index, goldsrc::skin_t* pskin)
{
    NCL_PROFILE_SCOPE();

    const int model_index = static_cast<int>(pModel - mod_known);
    if (model_index < 0 || model_index >= goldsrc::MAX_KNOWN_MODELS)
        return nullptr;

    goldsrc::cache_user_t* slot = &model_texture_cache[model_index][index];
    int* pixels = nullptr;
    if (Cache_Check(slot) != nullptr)
    {
        pixels = static_cast<int*>(slot->data);
    }
    else
    {
        auto* file = static_cast<unsigned char*>(COM_LoadFileForMe(pModel->name, nullptr));
        auto* texture = reinterpret_cast<int*>(file + 80 * index + *reinterpret_cast<int*>(file + 184));
        const int bytes = texture[17] * texture[18] + 776;
        Cache_Alloc(slot, bytes, reinterpret_cast<char*>(pskin + 16));
        pixels = static_cast<int*>(slot->data);
        pixels[0] = texture[17];
        pixels[1] = texture[18];
        Q_memcpy(pixels + 2, file + texture[19], static_cast<size_t>(bytes - 8));
        Mem_Free(file);
    }

    pskin->index = index;
    pskin->width = pixels[0];
    pskin->height = pixels[1];

    return pixels + 2;
}
