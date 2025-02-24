#ifndef _RIVE_BACKBOARD_IMPORTER_HPP_
#define _RIVE_BACKBOARD_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include <unordered_map>
#include <vector>

namespace rive
{
class Artboard;
class NestedArtboard;
class Backboard;
class FileAsset;
class FileAssetReferencer;
class DataConverter;
class DataBind;
class DataConverterGroupItem;
class ScrollPhysics;
class BackboardImporter : public ImportStackObject
{
private:
    Backboard* m_Backboard;
    std::unordered_map<int, Artboard*> m_ArtboardLookup;
    std::vector<NestedArtboard*> m_NestedArtboards;
    std::vector<FileAsset*> m_FileAssets;
    std::vector<FileAssetReferencer*> m_FileAssetReferencers;
    std::vector<DataConverter*> m_DataConverters;
    std::vector<DataBind*> m_DataConverterReferencers;
    std::vector<DataConverterGroupItem*> m_DataConverterGroupItemReferencers;
    std::vector<KeyFrameInterpolator*> m_interpolators;
    std::vector<ScrollPhysics*> m_physics;
    int m_NextArtboardId;

public:
    BackboardImporter(Backboard* backboard);
    void addArtboard(Artboard* artboard);
    void addMissingArtboard();
    void addNestedArtboard(NestedArtboard* artboard);
    void addFileAsset(FileAsset* asset);
    void addFileAssetReferencer(FileAssetReferencer* referencer);
    void addDataConverterReferencer(DataBind* referencer);
    void addDataConverter(DataConverter* converter);
    void addDataConverterGroupItemReferencer(
        DataConverterGroupItem* referencer);
    void addInterpolator(KeyFrameInterpolator* interpolator);
    void addPhysics(ScrollPhysics* physics);
    std::vector<ScrollPhysics*> physics() { return m_physics; }

    StatusCode resolve() override;
    const Backboard* backboard() const { return m_Backboard; }
};
} // namespace rive
#endif
