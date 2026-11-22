#pragma once

#include <memory>
#include <vector>

#include "tjs.h"
#include "PSBFile.h"

namespace PSB {
    void initPSBMedia();
    void deInitPSBMedia();

    void
    registerRootResources(const ttstr &container,
                          const std::shared_ptr<const PSBDictionary> &root);
    void
    registerRootResources(const std::vector<ttstr> &containers,
                          const std::shared_ptr<const PSBDictionary> &root);

    // Loads a PSB container from ordinary storage and registers its nested
    // resources under "<container>/<key>".  Used to satisfy psb:// requests
    // lazily, the way the KR Android ports expect: a game never calls
    // PSBFile.load() for a .dpak, it just asks for
    // "psb://<file>.dpak/<entry>.tlg".
    bool loadContainer(const ttstr &path);

    void registerRootResources(const ttstr &container, const PSBFile &file);
    void registerRootResources(const std::vector<ttstr> &containers,
                               const PSBFile &file);
} // namespace PSB
