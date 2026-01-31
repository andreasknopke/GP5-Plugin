/*
  ==============================================================================

    GP7Parser.cpp
    
    Parser for Guitar Pro 7/8 (.gp) files
    Based on alphaTab's GpifParser

  ==============================================================================
*/

#include "GP7Parser.h"
#include "TabModels.h"
#include <juce_core/juce_core.h>

//==============================================================================
// Constructor / Destructor
//==============================================================================

GP7Parser::GP7Parser()
{
}

GP7Parser::~GP7Parser()
{
}

//==============================================================================
// Main parsing interface
//==============================================================================

bool GP7Parser::parseFile(const juce::File& file)
{
    if (!file.existsAsFile())
    {
        lastError = "File does not exist: " + file.getFullPathName();
        return false;
    }
    
    // Step 1: Extract score.gpif from ZIP
    juce::String xmlContent;
    if (!extractGpifFromZip(file, xmlContent))
    {
        return false;
    }
    
    DBG("GP7Parser: Extracted GPIF XML (" << xmlContent.length() << " chars)");
    
    // Step 2: Parse the XML
    auto xmlDoc = juce::XmlDocument::parse(xmlContent);
    if (xmlDoc == nullptr)
    {
        lastError = "Failed to parse GPIF XML";
        return false;
    }
    
    // Step 3: Clear previous data
    tracksById.clear();
    barsById.clear();
    voicesById.clear();
    beatsById.clear();
    notesById.clear();
    rhythmsById.clear();
    trackMapping.clear();
    masterBars.clear();
    measureHeaders.clear();
    tracks.clear();
    
    // Step 4: Parse GPIF structure (Pass 1)
    parseGPIF(xmlDoc.get());
    
    // Step 5: Build model (Pass 2)
    buildModel();
    
    DBG("GP7Parser: Parsed " << tracks.size() << " tracks, " << measureHeaders.size() << " measures");
    
    return true;
}

//==============================================================================
// ZIP extraction
//==============================================================================

bool GP7Parser::extractGpifFromZip(const juce::File& file, juce::String& xmlContent)
{
    // Open the file as a ZIP archive
    juce::ZipFile zip(file);
    
    if (zip.getNumEntries() == 0)
    {
        lastError = "Not a valid ZIP archive or empty: " + file.getFullPathName();
        return false;
    }
    
    DBG("GP7Parser: ZIP has " << zip.getNumEntries() << " entries");
    
    // Look for score.gpif (can be in root or Content/ folder)
    const juce::ZipFile::ZipEntry* gpifEntry = nullptr;
    
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        auto* entry = zip.getEntry(i);
        DBG("  ZIP entry: " << entry->filename);
        
        if (entry->filename.endsWith("score.gpif"))
        {
            gpifEntry = entry;
            break;
        }
    }
    
    if (gpifEntry == nullptr)
    {
        lastError = "No score.gpif found in ZIP archive";
        return false;
    }
    
    // Extract the content
    auto inputStream = zip.createStreamForEntry(*gpifEntry);
    if (inputStream == nullptr)
    {
        lastError = "Failed to read score.gpif from ZIP";
        return false;
    }
    
    xmlContent = inputStream->readEntireStreamAsString();
    
    if (xmlContent.isEmpty())
    {
        lastError = "score.gpif is empty";
        return false;
    }
    
    return true;
}

//==============================================================================
// XML Parsing - Pass 1: Collect all elements
//==============================================================================

void GP7Parser::parseGPIF(juce::XmlElement* root)
{
    if (root == nullptr || root->getTagName() != "GPIF")
    {
        lastError = "Root element is not GPIF";
        return;
    }
    
    // Parse all child elements
    for (auto* child : root->getChildIterator())
    {
        auto tagName = child->getTagName();
        
        if (tagName == "Score")
            parseScore(child);
        else if (tagName == "MasterTrack")
            parseMasterTrack(child);
        else if (tagName == "Tracks")
            parseTracks(child);
        else if (tagName == "MasterBars")
            parseMasterBars(child);
        else if (tagName == "Bars")
            parseBars(child);
        else if (tagName == "Voices")
            parseVoices(child);
        else if (tagName == "Beats")
            parseBeats(child);
        else if (tagName == "Notes")
            parseNotes(child);
        else if (tagName == "Rhythms")
            parseRhythms(child);
    }
}

void GP7Parser::parseScore(juce::XmlElement* node)
{
    for (auto* child : node->getChildIterator())
    {
        auto tagName = child->getTagName();
        auto text = child->getAllSubText().trim();
        
        if (tagName == "Title")
            songInfo.title = text;
        else if (tagName == "Artist")
            songInfo.artist = text;
        else if (tagName == "Album")
            songInfo.album = text;
        else if (tagName == "Words" || tagName == "Lyricist")
            songInfo.words = text;
        else if (tagName == "Music" || tagName == "Composer")
            songInfo.music = text;
        else if (tagName == "Copyright")
            songInfo.copyright = text;
        else if (tagName == "Tabber" || tagName == "Tab")
            songInfo.tab = text;
        else if (tagName == "Instructions" || tagName == "Notices")
            songInfo.instructions = text;
    }
    
    DBG("GP7Parser: Score - Title: " << songInfo.title << ", Artist: " << songInfo.artist);
}

void GP7Parser::parseMasterTrack(juce::XmlElement* node)
{
    for (auto* child : node->getChildIterator())
    {
        auto tagName = child->getTagName();
        
        if (tagName == "Tracks")
        {
            // Track order mapping
            trackMapping = splitString(child->getAllSubText().trim());
            DBG("GP7Parser: Track mapping: " << trackMapping.joinIntoString(", "));
        }
        else if (tagName == "Automations")
        {
            // Parse tempo automations
            for (auto* automation : child->getChildIterator())
            {
                if (automation->getTagName() == "Automation")
                {
                    juce::String type;
                    int barIndex = 0;
                    float value = 0;
                    
                    for (auto* prop : automation->getChildIterator())
                    {
                        if (prop->getTagName() == "Type")
                            type = prop->getAllSubText().trim();
                        else if (prop->getTagName() == "Bar")
                            barIndex = parseIntSafe(prop->getAllSubText().trim());
                        else if (prop->getTagName() == "Value")
                        {
                            auto parts = splitString(prop->getAllSubText().trim());
                            if (parts.size() > 0)
                                value = parseFloatSafe(parts[0]);
                        }
                    }
                    
                    if (type == "Tempo" && barIndex == 0)
                    {
                        currentTempo = static_cast<int>(value);
                        DBG("GP7Parser: Initial tempo = " << currentTempo);
                    }
                }
            }
        }
    }
}

void GP7Parser::parseTracks(juce::XmlElement* node)
{
    for (auto* child : node->getChildIterator())
    {
        if (child->getTagName() == "Track")
        {
            parseTrack(child);
        }
    }
}

void GP7Parser::parseTrack(juce::XmlElement* node)
{
    GP5Track track;
    juce::String trackId = node->getStringAttribute("id");
    
    for (auto* child : node->getChildIterator())
    {
        auto tagName = child->getTagName();
        auto text = child->getAllSubText().trim();
        
        if (tagName == "Name")
        {
            track.name = text;
        }
        else if (tagName == "ShortName")
        {
            // Could use for display
        }
        else if (tagName == "Color")
        {
            auto parts = splitString(text);
            if (parts.size() >= 3)
            {
                int r = parseIntSafe(parts[0]);
                int g = parseIntSafe(parts[1]);
                int b = parseIntSafe(parts[2]);
                track.colour = juce::Colour(r, g, b);
            }
        }
        else if (tagName == "Properties")
        {
            // Parse tuning and other properties
            for (auto* prop : child->getChildIterator())
            {
                if (prop->getTagName() == "Property")
                {
                    auto propName = prop->getStringAttribute("name");
                    
                    if (propName == "Tuning")
                    {
                        auto* pitchesElem = prop->getChildByName("Pitches");
                        if (pitchesElem)
                        {
                            auto tuningParts = splitString(pitchesElem->getAllSubText().trim());
                            track.stringCount = tuningParts.size();
                            track.tuning.clear();
                            
                            // Reverse order (GP7 is high to low, we want low to high)
                            for (int i = tuningParts.size() - 1; i >= 0; --i)
                            {
                                track.tuning.add(parseIntSafe(tuningParts[i]));
                            }
                        }
                    }
                    else if (propName == "CapoFret")
                    {
                        auto* fretElem = prop->getChildByName("Fret");
                        if (fretElem)
                            track.capo = parseIntSafe(fretElem->getAllSubText().trim());
                    }
                }
            }
        }
        else if (tagName == "GeneralMidi" || tagName == "MidiConnection")
        {
            for (auto* midi : child->getChildIterator())
            {
                if (midi->getTagName() == "PrimaryChannel")
                    track.midiChannel = parseIntSafe(midi->getAllSubText().trim()) + 1;  // 0-based to 1-based
                else if (midi->getTagName() == "Program")
                    ; // MIDI program number
                else if (midi->getTagName() == "Port")
                    track.port = parseIntSafe(midi->getAllSubText().trim());
            }
            
            // Check for percussion
            if (child->getStringAttribute("table") == "Percussion")
                track.isPercussion = true;
        }
        else if (tagName == "PlaybackState")
        {
            // Solo/Mute state - could be used
        }
    }
    
    // Set default tuning if not specified (standard 6-string guitar)
    if (track.tuning.isEmpty())
    {
        track.stringCount = 6;
        track.tuning = { 64, 59, 55, 50, 45, 40 };  // E4, B3, G3, D3, A2, E2
    }
    
    tracksById[trackId] = track;
    DBG("GP7Parser: Track '" << track.name << "' (id=" << trackId << ") - " 
        << track.stringCount << " strings, ch=" << track.midiChannel);
}

void GP7Parser::parseMasterBars(juce::XmlElement* node)
{
    for (auto* child : node->getChildIterator())
    {
        if (child->getTagName() == "MasterBar")
        {
            parseMasterBar(child);
        }
    }
}

void GP7Parser::parseMasterBar(juce::XmlElement* node)
{
    GpifMasterBar masterBar;
    
    for (auto* child : node->getChildIterator())
    {
        auto tagName = child->getTagName();
        auto text = child->getAllSubText().trim();
        
        if (tagName == "Bars")
        {
            masterBar.barRefs = splitString(text);
        }
        else if (tagName == "Time")
        {
            auto parts = splitString(text, "/");
            if (parts.size() == 2)
            {
                masterBar.timeNumerator = parseIntSafe(parts[0], 4);
                masterBar.timeDenominator = parseIntSafe(parts[1], 4);
            }
        }
        else if (tagName == "Key")
        {
            for (auto* keyChild : child->getChildIterator())
            {
                if (keyChild->getTagName() == "AccidentalCount")
                    masterBar.keySignature = parseIntSafe(keyChild->getAllSubText().trim());
            }
        }
        else if (tagName == "Repeat")
        {
            if (child->getBoolAttribute("start", false))
                masterBar.isRepeatStart = true;
            if (child->getBoolAttribute("end", false))
            {
                masterBar.isRepeatEnd = true;
                masterBar.repeatCount = parseIntSafe(child->getStringAttribute("count"), 2);
            }
        }
        else if (tagName == "AlternateEndings")
        {
            // Bit flags for alternate endings
            masterBar.alternateEnding = parseIntSafe(text);
        }
        else if (tagName == "Section")
        {
            for (auto* secChild : child->getChildIterator())
            {
                if (secChild->getTagName() == "Text")
                    masterBar.marker = secChild->getAllSubText().trim();
            }
        }
    }
    
    masterBars.add(masterBar);
}

void GP7Parser::parseBars(juce::XmlElement* node)
{
    for (auto* child : node->getChildIterator())
    {
        if (child->getTagName() == "Bar")
        {
            parseBar(child);
        }
    }
}

void GP7Parser::parseBar(juce::XmlElement* node)
{
    GpifBar bar;
    bar.id = node->getStringAttribute("id");
    
    for (auto* child : node->getChildIterator())
    {
        auto tagName = child->getTagName();
        
        if (tagName == "Voices")
        {
            bar.voiceRefs = splitString(child->getAllSubText().trim());
        }
        else if (tagName == "Clef")
        {
            auto clefText = child->getAllSubText().trim();
            if (clefText == "G2") bar.clef = 0;
            else if (clefText == "F4") bar.clef = 1;
            else if (clefText == "C3") bar.clef = 2;
            else if (clefText == "C4") bar.clef = 3;
            else if (clefText == "Neutral") bar.clef = 4;
        }
    }
    
    barsById[bar.id] = bar;
}

void GP7Parser::parseVoices(juce::XmlElement* node)
{
    for (auto* child : node->getChildIterator())
    {
        if (child->getTagName() == "Voice")
        {
            parseVoice(child);
        }
    }
}

void GP7Parser::parseVoice(juce::XmlElement* node)
{
    GpifVoice voice;
    voice.id = node->getStringAttribute("id");
    
    for (auto* child : node->getChildIterator())
    {
        if (child->getTagName() == "Beats")
        {
            voice.beatRefs = splitString(child->getAllSubText().trim());
        }
    }
    
    voicesById[voice.id] = voice;
}

void GP7Parser::parseBeats(juce::XmlElement* node)
{
    for (auto* child : node->getChildIterator())
    {
        if (child->getTagName() == "Beat")
        {
            parseBeat(child);
        }
    }
}

void GP7Parser::parseBeat(juce::XmlElement* node)
{
    GpifBeat beat;
    beat.id = node->getStringAttribute("id");
    
    for (auto* child : node->getChildIterator())
    {
        auto tagName = child->getTagName();
        
        if (tagName == "Rhythm")
        {
            beat.rhythmRef = child->getStringAttribute("ref");
        }
        else if (tagName == "Notes")
        {
            beat.noteRefs = splitString(child->getAllSubText().trim());
        }
        else if (tagName == "Chord")
        {
            beat.chordName = child->getAllSubText().trim();
        }
        else if (tagName == "FreeText")
        {
            beat.text = child->getAllSubText().trim();
        }
        else if (tagName == "GraceNotes")
        {
            // Handle grace notes
        }
        else if (tagName == "Ottavia")
        {
            // Octave shift
        }
        else if (tagName == "Properties")
        {
            for (auto* prop : child->getChildIterator())
            {
                if (prop->getTagName() == "Property")
                {
                    auto propName = prop->getStringAttribute("name");
                    
                    if (propName == "Brush")
                    {
                        auto* dirElem = prop->getChildByName("Direction");
                        if (dirElem)
                        {
                            auto dir = dirElem->getAllSubText().trim();
                            if (dir == "Down") beat.hasDownstroke = true;
                            else if (dir == "Up") beat.hasUpstroke = true;
                        }
                    }
                }
            }
        }
    }
    
    // Check if this is a rest (no notes)
    if (beat.noteRefs.isEmpty())
    {
        // Check for explicit rest markers
        for (auto* child : node->getChildIterator())
        {
            if (child->getTagName() == "Rest")
            {
                beat.isRest = true;
                break;
            }
        }
    }
    
    beatsById[beat.id] = beat;
}

void GP7Parser::parseNotes(juce::XmlElement* node)
{
    for (auto* child : node->getChildIterator())
    {
        if (child->getTagName() == "Note")
        {
            parseNote(child);
        }
    }
}

void GP7Parser::parseNote(juce::XmlElement* node)
{
    GpifNote note;
    juce::String noteId = node->getStringAttribute("id");
    
    for (auto* child : node->getChildIterator())
    {
        auto tagName = child->getTagName();
        
        if (tagName == "Properties")
        {
            parseNoteProperties(child, note);
        }
        else if (tagName == "LetRing")
        {
            note.isLetRing = true;
        }
        else if (tagName == "AntiAccent")
        {
            if (child->getAllSubText().trim().toLowerCase() == "normal")
                note.isGhost = true;
        }
        else if (tagName == "Accent")
        {
            // Accent type
        }
        else if (tagName == "Tie")
        {
            auto origin = child->getStringAttribute("origin");
            if (origin == "true")
                note.isTied = true;
        }
        else if (tagName == "Vibrato")
        {
            note.hasVibrato = true;
        }
    }
    
    notesById[noteId] = note;
}

void GP7Parser::parseNoteProperties(juce::XmlElement* node, GpifNote& note)
{
    for (auto* prop : node->getChildIterator())
    {
        if (prop->getTagName() != "Property")
            continue;
        
        auto propName = prop->getStringAttribute("name");
        
        if (propName == "String")
        {
            auto* strElem = prop->getChildByName("String");
            if (strElem)
                note.string = parseIntSafe(strElem->getAllSubText().trim());
        }
        else if (propName == "Fret")
        {
            auto* fretElem = prop->getChildByName("Fret");
            if (fretElem)
                note.fret = parseIntSafe(fretElem->getAllSubText().trim());
        }
        else if (propName == "Midi")
        {
            auto* numElem = prop->getChildByName("Number");
            if (numElem)
            {
                // MIDI note number - could derive fret from this
            }
        }
        else if (propName == "PalmMuted")
        {
            auto* enableElem = prop->getChildByName("Enable");
            if (enableElem)
                note.isPalmMuted = true;
        }
        else if (propName == "Muted")
        {
            auto* enableElem = prop->getChildByName("Enable");
            if (enableElem)
                note.isDead = true;
        }
        else if (propName == "HopoOrigin" || propName == "HopoDestination")
        {
            note.isHammerOn = true;
        }
        else if (propName == "Slide")
        {
            note.hasSlide = true;
            auto* flagsElem = prop->getChildByName("Flags");
            if (flagsElem)
                note.slideType = parseIntSafe(flagsElem->getAllSubText().trim());
        }
        else if (propName == "HarmonicType")
        {
            auto* hTypeElem = prop->getChildByName("HType");
            if (hTypeElem)
            {
                auto hType = hTypeElem->getAllSubText().trim();
                if (hType == "Natural") note.harmonicType = 1;
                else if (hType == "Artificial") note.harmonicType = 2;
                else if (hType == "Pinch") note.harmonicType = 3;
                else if (hType == "Tap") note.harmonicType = 4;
                else if (hType == "Semi") note.harmonicType = 5;
            }
        }
        else if (propName == "Bended")
        {
            note.hasBend = true;
            // Bend details would be parsed here
        }
    }
}

void GP7Parser::parseRhythms(juce::XmlElement* node)
{
    for (auto* child : node->getChildIterator())
    {
        if (child->getTagName() == "Rhythm")
        {
            parseRhythm(child);
        }
    }
}

void GP7Parser::parseRhythm(juce::XmlElement* node)
{
    GpifRhythm rhythm;
    juce::String rhythmId = node->getStringAttribute("id");
    
    for (auto* child : node->getChildIterator())
    {
        auto tagName = child->getTagName();
        auto text = child->getAllSubText().trim();
        
        if (tagName == "NoteValue")
        {
            // Convert GP7 duration names to GP5 format
            if (text == "Whole") rhythm.duration = -2;
            else if (text == "Half") rhythm.duration = -1;
            else if (text == "Quarter") rhythm.duration = 0;
            else if (text == "Eighth") rhythm.duration = 1;
            else if (text == "16th") rhythm.duration = 2;
            else if (text == "32nd") rhythm.duration = 3;
            else if (text == "64th") rhythm.duration = 4;
        }
        else if (tagName == "AugmentationDot")
        {
            int count = parseIntSafe(child->getStringAttribute("count"), 1);
            if (count == 1) rhythm.isDotted = true;
            else if (count == 2) rhythm.isDoubleDotted = true;
        }
        else if (tagName == "PrimaryTuplet")
        {
            rhythm.tupletN = parseIntSafe(child->getStringAttribute("num"), 1);
            rhythm.tupletD = parseIntSafe(child->getStringAttribute("den"), 1);
        }
    }
    
    rhythmsById[rhythmId] = rhythm;
}

//==============================================================================
// XML Parsing - Pass 2: Build model from collected elements
//==============================================================================

void GP7Parser::buildModel()
{
    // 1. Build tracks in the correct order
    for (const auto& trackId : trackMapping)
    {
        if (tracksById.find(trackId) != tracksById.end())
        {
            tracks.add(tracksById[trackId]);
        }
    }
    
    // 2. Build measure headers from master bars
    for (int m = 0; m < masterBars.size(); ++m)
    {
        const auto& mb = masterBars[m];
        
        GP5MeasureHeader header;
        header.number = m + 1;
        header.numerator = mb.timeNumerator;
        header.denominator = mb.timeDenominator;
        header.isRepeatOpen = mb.isRepeatStart;
        header.repeatClose = mb.isRepeatEnd ? mb.repeatCount : 0;
        header.repeatAlternative = mb.alternateEnding;
        header.marker = mb.marker;
        
        measureHeaders.add(header);
    }
    
    // 3. Build measures for each track
    for (int t = 0; t < tracks.size(); ++t)
    {
        auto& track = tracks.getReference(t);
        juce::String trackId = trackMapping[t];
        
        track.measures.clear();
        
        for (int m = 0; m < masterBars.size(); ++m)
        {
            const auto& mb = masterBars[m];
            
            // Find the bar for this track in this master bar
            if (t >= mb.barRefs.size())
            {
                // No bar for this track - add empty measure
                GP5TrackMeasure emptyMeasure;
                track.measures.add(emptyMeasure);
                continue;
            }
            
            juce::String barId = mb.barRefs[t];
            
            GP5TrackMeasure trackMeasure;
            
            // Find the bar
            auto barIt = barsById.find(barId);
            if (barIt == barsById.end())
            {
                track.measures.add(trackMeasure);
                continue;
            }
            
            const auto& bar = barIt->second;
            
            // Process voices (use first voice as primary)
            for (int v = 0; v < bar.voiceRefs.size() && v < 2; ++v)
            {
                juce::String voiceId = bar.voiceRefs[v];
                
                // Skip "-1" placeholder voices
                if (voiceId == "-1")
                    continue;
                
                auto voiceIt = voicesById.find(voiceId);
                if (voiceIt == voicesById.end())
                    continue;
                
                const auto& voice = voiceIt->second;
                juce::Array<GP5Beat>& targetVoice = (v == 0) ? trackMeasure.voice1 : trackMeasure.voice2;
                
                // Process beats in this voice
                for (const auto& beatId : voice.beatRefs)
                {
                    auto beatIt = beatsById.find(beatId);
                    if (beatIt == beatsById.end())
                        continue;
                    
                    const auto& gpifBeat = beatIt->second;
                    
                    GP5Beat gp5Beat;
                    gp5Beat.isRest = gpifBeat.isRest || gpifBeat.noteRefs.isEmpty();
                    gp5Beat.text = gpifBeat.text;
                    gp5Beat.chordName = gpifBeat.chordName;
                    gp5Beat.hasDownstroke = gpifBeat.hasDownstroke;
                    gp5Beat.hasUpstroke = gpifBeat.hasUpstroke;
                    
                    // Get rhythm
                    auto rhythmIt = rhythmsById.find(gpifBeat.rhythmRef);
                    if (rhythmIt != rhythmsById.end())
                    {
                        const auto& rhythm = rhythmIt->second;
                        gp5Beat.duration = rhythm.duration;
                        gp5Beat.isDotted = rhythm.isDotted || rhythm.isDoubleDotted;
                        gp5Beat.tupletN = rhythm.tupletN;
                    }
                    
                    // Process notes
                    for (const auto& noteId : gpifBeat.noteRefs)
                    {
                        auto noteIt = notesById.find(noteId);
                        if (noteIt == notesById.end())
                            continue;
                        
                        const auto& gpifNote = noteIt->second;
                        
                        GP5Note gp5Note;
                        gp5Note.fret = gpifNote.fret;
                        gp5Note.velocity = gpifNote.velocity;
                        gp5Note.isTied = gpifNote.isTied;
                        gp5Note.isGhost = gpifNote.isGhost;
                        gp5Note.isDead = gpifNote.isDead;
                        gp5Note.hasHammerOn = gpifNote.isHammerOn;
                        gp5Note.hasVibrato = gpifNote.hasVibrato;
                        
                        if (gpifNote.hasSlide)
                        {
                            gp5Note.hasSlide = true;
                            gp5Note.slideType = (gpifNote.slideType > 0) ? gpifNote.slideType : 1;
                        }
                        
                        if (gpifNote.hasBend)
                        {
                            gp5Note.hasBend = true;
                            gp5Note.bendValue = gpifNote.bendValue;
                            gp5Note.bendType = gpifNote.bendType;
                        }
                        
                        gp5Note.harmonicType = gpifNote.harmonicType;
                        
                        gp5Beat.notes[gpifNote.string] = gp5Note;
                        
                        // Check palm mute at beat level
                        if (gpifNote.isPalmMuted)
                            gp5Beat.isPalmMute = true;
                    }
                    
                    targetVoice.add(gp5Beat);
                }
            }
            
            track.measures.add(trackMeasure);
        }
    }
    
    DBG("GP7Parser: Model built - " << tracks.size() << " tracks, " << measureHeaders.size() << " measures");
}

//==============================================================================
// Helper functions
//==============================================================================

juce::StringArray GP7Parser::splitString(const juce::String& text, const juce::String& separator)
{
    juce::StringArray result;
    result.addTokens(text, separator, "");
    result.removeEmptyStrings();
    return result;
}

int GP7Parser::parseIntSafe(const juce::String& text, int fallback)
{
    if (text.isEmpty())
        return fallback;
    
    return text.getIntValue();
}

float GP7Parser::parseFloatSafe(const juce::String& text, float fallback)
{
    if (text.isEmpty())
        return fallback;
    
    return text.getFloatValue();
}

//==============================================================================
// Convert to TabModels (delegates to existing GP5Parser logic)
//==============================================================================

juce::Array<TabMeasure> GP7Parser::convertToTabMeasures(int trackIndex) const
{
    // This conversion mirrors GP5Parser::convertToTabMeasures
    // For now, we'll create a temporary GP5Parser-like structure
    // In the future, this should be refactored to share code
    
    juce::Array<TabMeasure> result;
    
    if (trackIndex < 0 || trackIndex >= tracks.size())
        return result;
    
    const auto& track = tracks[trackIndex];
    
    for (int m = 0; m < track.measures.size() && m < measureHeaders.size(); ++m)
    {
        const auto& gp5Measure = track.measures[m];
        const auto& header = measureHeaders[m];
        
        TabMeasure tabMeasure;
        tabMeasure.measureNumber = m + 1;
        tabMeasure.timeSignatureNumerator = header.numerator;
        tabMeasure.timeSignatureDenominator = header.denominator;
        tabMeasure.isRepeatOpen = header.isRepeatOpen;
        tabMeasure.isRepeatClose = header.repeatClose > 0;
        tabMeasure.repeatCount = header.repeatClose;
        tabMeasure.alternateEnding = header.repeatAlternative;
        tabMeasure.marker = header.marker;
        
        // Convert beats from voice 1
        for (const auto& gp5Beat : gp5Measure.voice1)
        {
            TabBeat tabBeat;
            
            // Duration conversion
            switch (gp5Beat.duration)
            {
                case -2: tabBeat.duration = NoteDuration::Whole; break;
                case -1: tabBeat.duration = NoteDuration::Half; break;
                case 0:  tabBeat.duration = NoteDuration::Quarter; break;
                case 1:  tabBeat.duration = NoteDuration::Eighth; break;
                case 2:  tabBeat.duration = NoteDuration::Sixteenth; break;
                case 3:  tabBeat.duration = NoteDuration::ThirtySecond; break;
                default: tabBeat.duration = NoteDuration::Quarter; break;
            }
            
            tabBeat.isDotted = gp5Beat.isDotted;
            tabBeat.isRest = gp5Beat.isRest;
            tabBeat.isPalmMuted = gp5Beat.isPalmMute;
            tabBeat.hasDownstroke = gp5Beat.hasDownstroke;
            tabBeat.hasUpstroke = gp5Beat.hasUpstroke;
            tabBeat.text = gp5Beat.text;
            tabBeat.chordName = gp5Beat.chordName;
            
            if (gp5Beat.tupletN > 0)
            {
                tabBeat.tupletNumerator = gp5Beat.tupletN;
                tabBeat.tupletDenominator = (gp5Beat.tupletN == 3) ? 2 : gp5Beat.tupletN - 1;
            }
            
            // Convert notes
            if (!gp5Beat.isRest)
            {
                for (const auto& [stringIndex, gp5Note] : gp5Beat.notes)
                {
                    TabNote tabNote;
                    tabNote.string = stringIndex;
                    tabNote.fret = gp5Note.fret;
                    tabNote.velocity = gp5Note.velocity;
                    tabNote.isTied = gp5Note.isTied;
                    
                    tabNote.effects.ghostNote = gp5Note.isGhost;
                    tabNote.effects.deadNote = gp5Note.isDead;
                    tabNote.effects.hammerOn = gp5Note.hasHammerOn;
                    tabNote.effects.vibrato = gp5Note.hasVibrato;
                    
                    if (gp5Note.hasSlide)
                    {
                        tabNote.effects.slideType = static_cast<SlideType>(gp5Note.slideType);
                    }
                    
                    if (gp5Note.hasBend)
                    {
                        tabNote.effects.bend = true;
                        tabNote.effects.bendValue = gp5Note.bendValue;
                        tabNote.effects.bendType = gp5Note.bendType;
                    }
                    
                    if (gp5Note.harmonicType > 0)
                    {
                        tabNote.effects.harmonic = static_cast<HarmonicType>(gp5Note.harmonicType);
                    }
                    
                    tabBeat.notes.add(tabNote);
                }
            }
            
            tabMeasure.beats.add(tabBeat);
        }
        
        result.add(tabMeasure);
    }
    
    return result;
}
