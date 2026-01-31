/*
  ==============================================================================

    ChordMatcher.h
    
    Akkord-Erkennungs- und Matching-System für MIDI-zu-Tab Konvertierung.
    
    Verwendet eine Bibliothek vordefinierter Griffbilder (Shapes) mit einem
    Kostensystem:
    - Open Chords: günstig
    - Barre Chords: teurer
    - Gedämpfte Saiten innerhalb: sehr teuer
    
    Berücksichtigt auch Transitions-Kosten (Handpositionswechsel).

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <string>

//==============================================================================
/**
 * ChordShape
 * 
 * Repräsentiert ein einzelnes Akkord-Griffbild auf der Gitarre.
 * Beinhaltet Bund-Positionen, Kosten und Metadaten.
 */
struct ChordShape
{
    // Akkord-Identifikation
    juce::String name;           // z.B. "C", "Am7", "Dm/F"
    juce::String rootNote;       // Grundton: "C", "A", "D" etc.
    juce::String quality;        // "major", "minor", "7", "m7", "dim" etc.
    juce::String bassNote;       // Bass-Note (für Slash-Akkorde), leer = Grundton
    
    // Griffbild: -1 = gedämpft (x), 0 = Leersaite, 1-24 = Bundnummer
    // Index: 0 = tiefste Saite (E2), 5 = höchste Saite (E4)
    std::array<int, 6> frets = { -1, -1, -1, -1, -1, -1 };
    
    // MIDI-Noten die dieser Akkord produziert (berechnet aus Tuning + Frets)
    std::vector<int> midiNotes;
    
    // Tiefste klingende Note (für Bass-Matching)
    int bassMidiNote = 0;
    
    // Basis-Bund-Position (für Barre-Akkorde)
    int baseFret = 0;
    
    // Kosten-Faktoren
    float baseCost = 0.0f;           // Intrinsische Schwierigkeit
    bool isOpenChord = false;         // Open Position
    bool isBarreChord = false;        // Erfordert Barre
    int barreStrings = 0;             // Anzahl Saiten im Barre
    
    // Anzahl gedämpfter Saiten "innerhalb" (zwischen gespielten Saiten)
    int mutedStringsInside = 0;
    
    // Shape-Typ für Verschiebung (E-Shape, A-Shape, etc.)
    enum class ShapeType { Open, EShape, AShape, DShape, CShape, GShape, Other };
    ShapeType shapeType = ShapeType::Open;
    
    // Berechne die MIDI-Noten aus den Bund-Positionen
    void calculateMidiNotes(const std::array<int, 6>& tuning)
    {
        midiNotes.clear();
        bassMidiNote = 127;
        
        for (int s = 0; s < 6; ++s)
        {
            if (frets[s] >= 0)  // Nicht gedämpft
            {
                int midiNote = tuning[s] + frets[s];
                midiNotes.push_back(midiNote);
                bassMidiNote = std::min(bassMidiNote, midiNote);
            }
        }
        
        // Sortiere für einfachen Vergleich
        std::sort(midiNotes.begin(), midiNotes.end());
    }
    
    // Berechne die Anzahl gedämpfter Saiten "innerhalb" des Griffs
    void calculateMutedInside()
    {
        int firstPlayed = -1;
        int lastPlayed = -1;
        
        for (int s = 0; s < 6; ++s)
        {
            if (frets[s] >= 0)
            {
                if (firstPlayed < 0) firstPlayed = s;
                lastPlayed = s;
            }
        }
        
        mutedStringsInside = 0;
        if (firstPlayed >= 0 && lastPlayed > firstPlayed)
        {
            for (int s = firstPlayed + 1; s < lastPlayed; ++s)
            {
                if (frets[s] < 0)
                    mutedStringsInside++;
            }
        }
    }
    
    // Berechne die Basis-Kosten basierend auf Shape-Eigenschaften
    void calculateBaseCost()
    {
        baseCost = 0.0f;
        
        // Open Chords sind am einfachsten
        if (isOpenChord)
        {
            baseCost = 0.0f;
        }
        // Barre-Akkorde sind anstrengender
        else if (isBarreChord)
        {
            baseCost = 5.0f;
            // Mehr Saiten im Barre = schwieriger
            baseCost += barreStrings * 0.5f;
        }
        else
        {
            baseCost = 3.0f;  // Normale verschobene Akkorde
        }
        
        // Gedämpfte Saiten innerhalb sind SEHR teuer (schwer sauber zu strummen)
        baseCost += mutedStringsInside * 15.0f;
        
        // Große Spannweite ist auch teurer
        int minFret = 100, maxFret = 0;
        for (int s = 0; s < 6; ++s)
        {
            if (frets[s] > 0)  // Nicht Leersaite, nicht gedämpft
            {
                minFret = std::min(minFret, frets[s]);
                maxFret = std::max(maxFret, frets[s]);
            }
        }
        if (maxFret - minFret > 3)
        {
            baseCost += (maxFret - minFret - 3) * 2.0f;
        }
    }
};

//==============================================================================
/**
 * ChordLibrary
 * 
 * Enthält eine Sammlung vordefinierter Akkord-Shapes.
 * Shapes können verschoben werden, um alle Tonarten abzudecken.
 */
class ChordLibrary
{
public:
    ChordLibrary()
    {
        buildLibrary();
    }
    
    // Alle verfügbaren Shapes
    const std::vector<ChordShape>& getAllShapes() const { return shapes; }
    
    // Shapes für einen bestimmten Akkordnamen finden
    std::vector<const ChordShape*> findShapesByName(const juce::String& name) const
    {
        std::vector<const ChordShape*> result;
        for (const auto& shape : shapes)
        {
            if (shape.name.equalsIgnoreCase(name))
                result.push_back(&shape);
        }
        return result;
    }
    
    // Shapes für bestimmte MIDI-Noten finden
    std::vector<const ChordShape*> findShapesForNotes(const std::vector<int>& midiNotes) const
    {
        std::vector<const ChordShape*> result;
        
        // Extrahiere Pitch Classes (0-11) aus MIDI-Noten
        std::set<int> targetPitchClasses;
        for (int note : midiNotes)
        {
            targetPitchClasses.insert(note % 12);
        }
        
        for (const auto& shape : shapes)
        {
            // Prüfe ob Shape die gleichen Pitch Classes enthält
            std::set<int> shapePitchClasses;
            for (int note : shape.midiNotes)
            {
                shapePitchClasses.insert(note % 12);
            }
            
            // Shape muss alle Target-Noten enthalten
            bool matches = true;
            for (int pc : targetPitchClasses)
            {
                if (shapePitchClasses.find(pc) == shapePitchClasses.end())
                {
                    matches = false;
                    break;
                }
            }
            
            if (matches)
                result.push_back(&shape);
        }
        
        return result;
    }

private:
    std::vector<ChordShape> shapes;
    
    // Standard-Tuning (E2, A2, D3, G3, B3, E4)
    const std::array<int, 6> standardTuning = { 40, 45, 50, 55, 59, 64 };
    
    void buildLibrary()
    {
        shapes.clear();
        
        // ============================================================
        // OPEN CHORDS - Major
        // ============================================================
        addOpenChord("C", {-1, 3, 2, 0, 1, 0}, "C", "major");
        addOpenChord("D", {-1, -1, 0, 2, 3, 2}, "D", "major");
        addOpenChord("E", {0, 2, 2, 1, 0, 0}, "E", "major");
        addOpenChord("G", {3, 2, 0, 0, 0, 3}, "G", "major");
        addOpenChord("A", {-1, 0, 2, 2, 2, 0}, "A", "major");
        
        // ============================================================
        // OPEN CHORDS - Minor
        // ============================================================
        addOpenChord("Am", {-1, 0, 2, 2, 1, 0}, "A", "minor");
        addOpenChord("Dm", {-1, -1, 0, 2, 3, 1}, "D", "minor");
        addOpenChord("Em", {0, 2, 2, 0, 0, 0}, "E", "minor");
        
        // ============================================================
        // OPEN CHORDS - Seventh
        // ============================================================
        addOpenChord("A7", {-1, 0, 2, 0, 2, 0}, "A", "7");
        addOpenChord("B7", {-1, 2, 1, 2, 0, 2}, "B", "7");
        addOpenChord("C7", {-1, 3, 2, 3, 1, 0}, "C", "7");
        addOpenChord("D7", {-1, -1, 0, 2, 1, 2}, "D", "7");
        addOpenChord("E7", {0, 2, 0, 1, 0, 0}, "E", "7");
        addOpenChord("G7", {3, 2, 0, 0, 0, 1}, "G", "7");
        
        // ============================================================
        // OPEN CHORDS - Minor Seventh
        // ============================================================
        addOpenChord("Am7", {-1, 0, 2, 0, 1, 0}, "A", "m7");
        addOpenChord("Dm7", {-1, -1, 0, 2, 1, 1}, "D", "m7");
        addOpenChord("Em7", {0, 2, 0, 0, 0, 0}, "E", "m7");
        
        // ============================================================
        // BARRE CHORDS - E-Shape (Root auf Saite 6)
        // ============================================================
        for (int fret = 1; fret <= 12; ++fret)
        {
            juce::String root = getNoteNameFromMidi(40 + fret);  // E + fret
            
            // Major E-Shape
            addBarreChord(root, {fret, fret+2, fret+2, fret+1, fret, fret}, 
                         root, "major", ChordShape::ShapeType::EShape, fret);
            
            // Minor E-Shape
            addBarreChord(root + "m", {fret, fret+2, fret+2, fret, fret, fret}, 
                         root, "minor", ChordShape::ShapeType::EShape, fret);
            
            // 7 E-Shape
            addBarreChord(root + "7", {fret, fret+2, fret, fret+1, fret, fret}, 
                         root, "7", ChordShape::ShapeType::EShape, fret);
            
            // m7 E-Shape
            addBarreChord(root + "m7", {fret, fret+2, fret, fret, fret, fret}, 
                         root, "m7", ChordShape::ShapeType::EShape, fret);
        }
        
        // ============================================================
        // BARRE CHORDS - A-Shape (Root auf Saite 5)
        // ============================================================
        for (int fret = 1; fret <= 12; ++fret)
        {
            juce::String root = getNoteNameFromMidi(45 + fret);  // A + fret
            
            // Major A-Shape
            addBarreChord(root, {-1, fret, fret+2, fret+2, fret+2, fret}, 
                         root, "major", ChordShape::ShapeType::AShape, fret);
            
            // Minor A-Shape
            addBarreChord(root + "m", {-1, fret, fret+2, fret+2, fret+1, fret}, 
                         root, "minor", ChordShape::ShapeType::AShape, fret);
            
            // 7 A-Shape
            addBarreChord(root + "7", {-1, fret, fret+2, fret, fret+2, fret}, 
                         root, "7", ChordShape::ShapeType::AShape, fret);
            
            // m7 A-Shape
            addBarreChord(root + "m7", {-1, fret, fret+2, fret, fret+1, fret}, 
                         root, "m7", ChordShape::ShapeType::AShape, fret);
        }
        
        // ============================================================
        // POWER CHORDS (für Rock/Metal)
        // ============================================================
        for (int fret = 1; fret <= 12; ++fret)
        {
            // Power Chord auf Saite 6
            juce::String root6 = getNoteNameFromMidi(40 + fret);
            addPowerChord(root6 + "5", {fret, fret+2, fret+2, -1, -1, -1}, 
                         root6, fret, 6);
            
            // Power Chord auf Saite 5
            juce::String root5 = getNoteNameFromMidi(45 + fret);
            addPowerChord(root5 + "5", {-1, fret, fret+2, fret+2, -1, -1}, 
                         root5, fret, 5);
        }
        
        // ============================================================
        // SLASH CHORDS (Inversionen)
        // ============================================================
        // C/G - C-Dur mit G im Bass
        addSlashChord("C/G", {3, 3, 2, 0, 1, 0}, "C", "major", "G");
        
        // D/F# - D-Dur mit F# im Bass
        addSlashChord("D/F#", {2, -1, 0, 2, 3, 2}, "D", "major", "F#");
        
        // Am/E - Am mit E im Bass
        addSlashChord("Am/E", {0, 0, 2, 2, 1, 0}, "A", "minor", "E");
        
        // Am/G - Am mit G im Bass  
        addSlashChord("Am/G", {3, 0, 2, 2, 1, 0}, "A", "minor", "G");
        
        // G/B - G-Dur mit B im Bass
        addSlashChord("G/B", {-1, 2, 0, 0, 0, 3}, "G", "major", "B");
        
        // Initialisiere alle Shapes
        for (auto& shape : shapes)
        {
            shape.calculateMidiNotes(standardTuning);
            shape.calculateMutedInside();
            shape.calculateBaseCost();
        }
    }
    
    void addOpenChord(const juce::String& name, std::array<int, 6> frets,
                      const juce::String& root, const juce::String& quality)
    {
        ChordShape shape;
        shape.name = name;
        shape.rootNote = root;
        shape.quality = quality;
        shape.frets = frets;
        shape.isOpenChord = true;
        shape.isBarreChord = false;
        shape.shapeType = ChordShape::ShapeType::Open;
        shape.baseFret = 0;
        shapes.push_back(shape);
    }
    
    void addBarreChord(const juce::String& name, std::array<int, 6> frets,
                       const juce::String& root, const juce::String& quality,
                       ChordShape::ShapeType shapeType, int baseFret)
    {
        ChordShape shape;
        shape.name = name;
        shape.rootNote = root;
        shape.quality = quality;
        shape.frets = frets;
        shape.isOpenChord = false;
        shape.isBarreChord = true;
        shape.shapeType = shapeType;
        shape.baseFret = baseFret;
        
        // Zähle Barre-Saiten
        shape.barreStrings = 0;
        for (int f : frets)
        {
            if (f == baseFret)
                shape.barreStrings++;
        }
        
        shapes.push_back(shape);
    }
    
    void addPowerChord(const juce::String& name, std::array<int, 6> frets,
                       const juce::String& root, int baseFret, int rootString)
    {
        ChordShape shape;
        shape.name = name;
        shape.rootNote = root;
        shape.quality = "5";
        shape.frets = frets;
        shape.isOpenChord = false;
        shape.isBarreChord = false;  // Power Chords brauchen kein Barre
        shape.shapeType = (rootString == 6) ? ChordShape::ShapeType::EShape 
                                             : ChordShape::ShapeType::AShape;
        shape.baseFret = baseFret;
        shapes.push_back(shape);
    }
    
    void addSlashChord(const juce::String& name, std::array<int, 6> frets,
                       const juce::String& root, const juce::String& quality,
                       const juce::String& bassNote)
    {
        ChordShape shape;
        shape.name = name;
        shape.rootNote = root;
        shape.quality = quality;
        shape.bassNote = bassNote;
        shape.frets = frets;
        shape.isOpenChord = true;
        shape.isBarreChord = false;
        shape.shapeType = ChordShape::ShapeType::Open;
        shape.baseFret = 0;
        shapes.push_back(shape);
    }
    
    juce::String getNoteNameFromMidi(int midiNote) const
    {
        static const char* names[] = {"C", "C#", "D", "D#", "E", "F", 
                                       "F#", "G", "G#", "A", "A#", "B"};
        return names[midiNote % 12];
    }
};

//==============================================================================
/**
 * ChordMatcher
 * 
 * Findet das beste Akkord-Shape für eine gegebene Menge von MIDI-Noten.
 * Verwendet ein Hybrid-Kostensystem:
 * - ShapeCost: Intrinsische Schwierigkeit des Griffs
 * - TransitionCost: Kosten für Handpositionswechsel
 */
class ChordMatcher
{
public:
    ChordMatcher() = default;
    
    struct MatchResult
    {
        const ChordShape* shape = nullptr;
        float totalCost = 0.0f;
        float shapeCost = 0.0f;
        float transitionCost = 0.0f;
        bool isMatch = false;
    };
    
    /**
     * Findet das beste Akkord-Shape für die gegebenen MIDI-Noten.
     * 
     * @param midiNotes Die zu matchenden MIDI-Noten
     * @param currentFretPosition Aktuelle Handposition (für Transition-Kosten)
     * @param requireExactBass Wenn true, muss der Bass-Ton exakt übereinstimmen
     * @return Das beste Match mit Kosteninformationen
     */
    MatchResult findBestChord(const std::vector<int>& midiNotes, 
                              int currentFretPosition = 0,
                              bool requireExactBass = true) const
    {
        if (midiNotes.size() < 2)
            return {};  // Mindestens 2 Noten für einen Akkord
        
        // Extrahiere Pitch Classes und finde Bass-Note
        std::set<int> targetPitchClasses;
        int lowestNote = 127;
        
        for (int note : midiNotes)
        {
            targetPitchClasses.insert(note % 12);
            lowestNote = std::min(lowestNote, note);
        }
        
        int targetBassPitchClass = lowestNote % 12;
        
        MatchResult bestResult;
        bestResult.totalCost = std::numeric_limits<float>::max();
        
        for (const auto& shape : library.getAllShapes())
        {
            // 1. Pitch Class Matching
            std::set<int> shapePitchClasses;
            for (int note : shape.midiNotes)
            {
                shapePitchClasses.insert(note % 12);
            }
            
            // Prüfe ob Shape alle Target-Pitch-Classes enthält
            bool pitchMatch = true;
            for (int pc : targetPitchClasses)
            {
                if (shapePitchClasses.find(pc) == shapePitchClasses.end())
                {
                    pitchMatch = false;
                    break;
                }
            }
            
            if (!pitchMatch)
                continue;
            
            // 2. Bass-Matching (für Inversionen)
            if (requireExactBass)
            {
                int shapeBassPitchClass = shape.bassMidiNote % 12;
                if (shapeBassPitchClass != targetBassPitchClass)
                    continue;
            }
            
            // 3. Kosten berechnen
            float shapeCost = shape.baseCost;
            
            // Transition-Kosten basierend auf Distanz zur aktuellen Position
            int shapePosition = shape.baseFret;
            if (shape.isOpenChord)
                shapePosition = 0;
            
            float transitionCost = std::abs(shapePosition - currentFretPosition) * 1.5f;
            
            // Bonus für Open Chords wenn wir am Sattel sind
            if (currentFretPosition <= 3 && shape.isOpenChord)
            {
                transitionCost = 0.0f;
            }
            
            float totalCost = shapeCost + transitionCost;
            
            if (totalCost < bestResult.totalCost)
            {
                bestResult.shape = &shape;
                bestResult.shapeCost = shapeCost;
                bestResult.transitionCost = transitionCost;
                bestResult.totalCost = totalCost;
                bestResult.isMatch = true;
            }
        }
        
        return bestResult;
    }
    
    /**
     * Prüft ob die gegebenen MIDI-Noten einen bekannten Akkord bilden.
     */
    bool isChord(const std::vector<int>& midiNotes) const
    {
        if (midiNotes.size() < 2)
            return false;
        
        auto result = findBestChord(midiNotes, 0, false);
        return result.isMatch;
    }
    
    /**
     * Gibt alle möglichen Shapes für die gegebenen Noten zurück (für UI).
     */
    std::vector<MatchResult> findAllMatches(const std::vector<int>& midiNotes,
                                            int currentFretPosition = 0) const
    {
        std::vector<MatchResult> results;
        
        if (midiNotes.size() < 2)
            return results;
        
        std::set<int> targetPitchClasses;
        int lowestNote = 127;
        
        for (int note : midiNotes)
        {
            targetPitchClasses.insert(note % 12);
            lowestNote = std::min(lowestNote, note);
        }
        
        for (const auto& shape : library.getAllShapes())
        {
            std::set<int> shapePitchClasses;
            for (int note : shape.midiNotes)
            {
                shapePitchClasses.insert(note % 12);
            }
            
            bool pitchMatch = true;
            for (int pc : targetPitchClasses)
            {
                if (shapePitchClasses.find(pc) == shapePitchClasses.end())
                {
                    pitchMatch = false;
                    break;
                }
            }
            
            if (pitchMatch)
            {
                MatchResult result;
                result.shape = &shape;
                result.shapeCost = shape.baseCost;
                result.transitionCost = std::abs(shape.baseFret - currentFretPosition) * 1.5f;
                result.totalCost = result.shapeCost + result.transitionCost;
                result.isMatch = true;
                results.push_back(result);
            }
        }
        
        // Sortiere nach Kosten
        std::sort(results.begin(), results.end(), 
            [](const MatchResult& a, const MatchResult& b) {
                return a.totalCost < b.totalCost;
            });
        
        return results;
    }
    
    const ChordLibrary& getLibrary() const { return library; }

private:
    ChordLibrary library;
};
