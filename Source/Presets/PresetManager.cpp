#include "PresetManager.h"
#include "FactoryPresets.h"

using namespace juce;

namespace mm
{
namespace
{
    const char* defaultMacroNames[] { "AGGRESSION", "MOVEMENT", "DARKNESS", "SPACE" };

    String sanitiseFileName (const String& s)
    {
        return File::createLegalFileName (s.trim()).substring (0, 60);
    }
}

PresetManager::PresetManager (AudioProcessor& p, AudioProcessorValueTreeState& s)
    : processor (p), apvts (s)
{
    loadFavorites();
    rescan();
}

const StringArray& PresetManager::categories()
{
    static const StringArray c { "BASS", "LEADS", "PADS", "ATMOSPHERE", "KEYS", "PLUCKS", "STABS", "ARPS",
                                 "SEQUENCES", "BREAKDOWN", "DUBSTEP", "TRAP", "DRUMS", "SCRATCH", "FX", "REFERENCE" };
    return c;
}

File PresetManager::getBaseFolder()
{
    const auto docs = File::getSpecialLocation (File::userDocumentsDirectory);
    const auto folder = docs.getChildFile ("ATDT Synth Machine");
    // MIGRACAO (novo nome): presets, favoritos, MIDI e cache da pasta antiga vem junto, uma unica vez
    static const bool migrated = [&]
    {
        const auto old = docs.getChildFile ("ATDT Metal Machine");
        if (old.isDirectory() && ! folder.exists())
            return old.copyDirectoryTo (folder);
        return false;
    }();
    juce::ignoreUnused (migrated);
    return folder;
}

// ------------------------------------------------------------------ lista
void PresetManager::rescan()
{
    presets.clear();

    const auto& factory = getFactoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
    {
        PresetInfo info;
        info.name = factory[(size_t) i].name;
        info.category = factory[(size_t) i].category;
        info.author = "ATDT";
        info.isFactory = true;
        info.factoryIndex = i;
        presets.add (info);
    }

    const auto folder = getUserPresetFolder();
    if (folder.isDirectory())
    {
        auto files = folder.findChildFiles (File::findFiles, true, String ("*") + fileExtension);
        files.sort();
        for (auto& f : files)
            if (auto xml = parseXML (f))
                if (xml->hasTagName ("ATDT_PRESET"))
                {
                    PresetInfo info;
                    info.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
                    info.category = xml->getStringAttribute ("category", "FX").toUpperCase();
                    info.author = xml->getStringAttribute ("author", "USER");
                    info.file = f;
                    presets.add (info);
                }
    }

    sendChangeMessage();
}

Array<int> PresetManager::filter (const String& category, const String& search) const
{
    Array<int> result;
    const auto words = StringArray::fromTokens (search.trim(), true);

    for (int i = 0; i < presets.size(); ++i)
    {
        const auto& p = presets.getReference (i);
        if (category == "FAVORITES" && ! isFavorite (i)) continue;
        if (category != "ALL" && category != "FAVORITES" && p.category != category) continue;

        bool match = true;
        for (auto& w : words)
            if (! (p.name.containsIgnoreCase (w) || p.category.containsIgnoreCase (w) || p.author.containsIgnoreCase (w)))
                match = false;
        if (match)
            result.add (i);
    }
    return result;
}

void PresetManager::setBrowseFilter (const String& category, const String& search)
{
    browseCategory = category;
    browseSearch = search;
}

// ------------------------------------------------------------------ carregar
void PresetManager::applyValues (const StringPairArray& values, const StringArray& macroNames)
{
    for (auto* ap : processor.getParameters())
    {
        auto* p = dynamic_cast<RangedAudioParameter*> (ap);
        if (p == nullptr)
            continue;

        float norm = p->getDefaultValue();
        const auto id = p->getParameterID();
        if (values.containsKey (id))
            norm = p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue (values[id].getFloatValue()));

        if (std::abs (p->getValue() - norm) > 1.0e-6f)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (norm);
            p->endChangeGesture();
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        const auto name = i < macroNames.size() && macroNames[i].isNotEmpty() ? macroNames[i] : String (defaultMacroNames[i]);
        apvts.state.setProperty ("macroName" + String (i + 1), name.toUpperCase().substring (0, 16), nullptr);
    }
}

void PresetManager::setCurrent (const PresetInfo& info)
{
    apvts.state.setProperty ("presetKey", info.key(), nullptr);
    apvts.state.setProperty ("presetName", info.name, nullptr);
    apvts.state.setProperty ("presetCategory", info.category, nullptr);
    sendChangeMessage();
}

bool PresetManager::load (int index)
{
    if (! isPositiveAndBelow (index, presets.size()))
        return false;

    const auto& info = presets.getReference (index);
    StringPairArray values;
    StringArray macroNames;
    ValueTree sequence, drums;
    String patternName, drumName;

    if (info.isFactory)
    {
        const auto& fp = getFactoryPresets()[(size_t) info.factoryIndex];
        for (auto& [id, v] : fp.values)
            values.set (id, String (v, 6));
        macroNames = fp.macroNames;
        patternName = fp.patternName;
        drumName = fp.drumPatternName;
    }
    else
    {
        auto xml = parseXML (info.file);
        if (xml == nullptr || ! xml->hasTagName ("ATDT_PRESET"))
            return false;

        for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
            values.set (e->getStringAttribute ("id"), e->getStringAttribute ("value"));
        for (auto* e : xml->getChildWithTagNameIterator ("MACRO"))
            macroNames.set (jlimit (0, 3, e->getIntAttribute ("index", 1) - 1), e->getStringAttribute ("name"));
        if (auto* seq = xml->getChildByName ("SEQUENCE"))
            sequence = ValueTree::fromXml (*seq);
        if (auto* dr = xml->getChildByName ("DRUMS"))
            drums = ValueTree::fromXml (*dr);
    }

    applyValues (values, macroNames);

    // sequencia: do preset do usuario, da biblioteca (fabrica) ou padrao INIT
    if (pattern != nullptr)
    {
        if (sequence.isValid())                          pattern->fromValueTree (sequence);
        else if (const int pi = findPattern (patternName); pi >= 0) applyPattern (*pattern, getPatternLibrary()[(size_t) pi]);
        else                                             pattern->setInit();
    }
    // bateria: do preset do usuario, da biblioteca (fabrica) ou Basic Rock
    if (drumPattern != nullptr)
    {
        if (drums.isValid())                                         drumPattern->fromValueTree (drums);
        else if (const int di = findDrumPattern (drumName); di >= 0) applyDrumPattern (*drumPattern, getDrumLibrary()[(size_t) di]);
        else                                                         setInitDrums (*drumPattern);
    }
    setCurrent (info);
    return true;
}

void PresetManager::step (int delta)
{
    auto list = filter (browseCategory, browseSearch);
    if (list.isEmpty())
        list = filter ("ALL", {});
    if (list.isEmpty())
        return;

    const int pos = list.indexOf (getCurrentIndex());
    const int next = pos < 0 ? (delta > 0 ? 0 : list.size() - 1)
                             : (pos + delta + list.size()) % list.size();
    load (list[next]);
}

void PresetManager::markInit()
{
    if (pattern != nullptr)
        pattern->setInit();
    if (drumPattern != nullptr)
        setInitDrums (*drumPattern);
    apvts.state.setProperty ("presetKey", "", nullptr);
    apvts.state.setProperty ("presetName", "INIT", nullptr);
    apvts.state.setProperty ("presetCategory", "", nullptr);
    for (int i = 0; i < 4; ++i)
        apvts.state.setProperty ("macroName" + String (i + 1), defaultMacroNames[i], nullptr);
    sendChangeMessage();
}

int PresetManager::getCurrentIndex() const
{
    const auto key = apvts.state.getProperty ("presetKey").toString();
    if (key.isEmpty())
        return -1;
    for (int i = 0; i < presets.size(); ++i)
        if (presets.getReference (i).key() == key)
            return i;
    return -1;
}

String PresetManager::getCurrentName() const     { return apvts.state.getProperty ("presetName", "INIT").toString(); }
String PresetManager::getCurrentCategory() const { return apvts.state.getProperty ("presetCategory", "").toString(); }

bool PresetManager::isCurrentFactory() const
{
    const int i = getCurrentIndex();
    return i >= 0 && presets.getReference (i).isFactory;
}

// ------------------------------------------------------------------ salvar
bool PresetManager::saveUserPreset (const String& rawName, const String& rawCategory)
{
    const auto name = rawName.trim().substring (0, 40);
    const auto category = categories().contains (rawCategory.toUpperCase()) ? rawCategory.toUpperCase() : String ("FX");
    if (name.isEmpty())
        return false;

    XmlElement xml ("ATDT_PRESET");
    xml.setAttribute ("name", name);
    xml.setAttribute ("category", category);
    xml.setAttribute ("author", "USER");
    xml.setAttribute ("version", JucePlugin_VersionString);

    for (auto* ap : processor.getParameters())
        if (auto* p = dynamic_cast<RangedAudioParameter*> (ap))
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", p->getParameterID());
            e->setAttribute ("value", (double) p->convertFrom0to1 (p->getValue()));
        }

    for (int i = 0; i < 4; ++i)
    {
        auto* e = xml.createNewChildElement ("MACRO");
        e->setAttribute ("index", i + 1);
        e->setAttribute ("name", apvts.state.getProperty ("macroName" + String (i + 1), defaultMacroNames[i]).toString());
    }

    if (pattern != nullptr)
        if (auto seqXml = pattern->toValueTree().createXml())
            xml.addChildElement (seqXml.release());
    if (drumPattern != nullptr)
        if (auto drXml = drumPattern->toValueTree().createXml())
            xml.addChildElement (drXml.release());

    const auto folder = getUserPresetFolder().getChildFile (category);
    if (! folder.createDirectory())
        return false;

    const auto file = folder.getChildFile (sanitiseFileName (name) + fileExtension);
    if (! xml.writeTo (file))
        return false;

    rescan();
    for (auto& p : presets)
        if (! p.isFactory && p.file == file)
            setCurrent (p);
    return true;
}

bool PresetManager::deleteUserPreset (int index)
{
    if (! isPositiveAndBelow (index, presets.size()) || presets.getReference (index).isFactory)
        return false;

    const auto key = presets.getReference (index).key();
    if (! presets.getReference (index).file.moveToTrash())
        return false;

    favorites.removeString (key);
    saveFavorites();
    if (apvts.state.getProperty ("presetKey").toString() == key)
        apvts.state.setProperty ("presetKey", "", nullptr);   // o som continua; so perde o vinculo
    rescan();
    return true;
}

// ------------------------------------------------------------------ favoritos
bool PresetManager::isFavorite (int index) const
{
    return isPositiveAndBelow (index, presets.size()) && favorites.contains (presets.getReference (index).key());
}

void PresetManager::toggleFavorite (int index)
{
    if (! isPositiveAndBelow (index, presets.size()))
        return;
    const auto key = presets.getReference (index).key();
    if (favorites.contains (key)) favorites.removeString (key);
    else                          favorites.add (key);
    saveFavorites();
    sendChangeMessage();
}

void PresetManager::loadFavorites()
{
    favorites.clear();
    if (auto xml = parseXML (getBaseFolder().getChildFile ("favorites.xml")))
        for (auto* e : xml->getChildWithTagNameIterator ("FAV"))
            favorites.addIfNotAlreadyThere (e->getStringAttribute ("key"));
}

void PresetManager::saveFavorites() const
{
    XmlElement xml ("ATDT_FAVORITES");
    for (auto& f : favorites)
        xml.createNewChildElement ("FAV")->setAttribute ("key", f);
    if (getBaseFolder().createDirectory())
        xml.writeTo (getBaseFolder().getChildFile ("favorites.xml"));
}
} // namespace mm
