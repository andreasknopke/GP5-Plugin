/*
  ==============================================================================

    FretPositionCalculator.h
    
    Berechnet alle möglichen Bund/Saiten-Positionen für eine gegebene MIDI-Note
    auf einem Gitarrengriffbrett. Berücksichtigt Tuning, Capo und Spielbarkeit.

  ==============================================================================
*/

#pragma once

#include "TabModels.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>

//==============================================================================
/**
 * AlternatePosition
 * 
 * Alternative Bund/Saiten-Position für eine Note.
 */
struct AlternatePosition
{
    int string = 0;         // Saitennummer
    int fret = 0;           // Bundnummer
    float cost = 0.0f;      // Schwierigkeits-/Präferenz-Kosten (niedriger = besser)
    
    bool operator<(const AlternatePosition& other) const { return cost < other.cost; }
};

//==============================================================================
/**
 * FretPositionCalculator
 * 
 * Berechnet alle möglichen Positionen für eine MIDI-Note auf dem Griffbrett
 * und bewertet diese nach Spielbarkeit.
 */
class FretPositionCalculator
{
public:
    FretPositionCalculator() = default;
    
    //==========================================================================
    // Konfiguration
    //==========================================================================
    
    void setTuning(const juce::Array<int>& newTuning) { tuning = newTuning; }
    void setCapo(int fret) { capoFret = juce::jmax(0, fret); }
    void setMaxFret(int fret) { maxFret = juce::jmax(1, fret); }
    void setPreferredPosition(int fret) { preferredFret = juce::jmax(0, fret); }
    
    //==========================================================================
    // Berechnung
    //==========================================================================
    
    /**
     * Berechnet alle möglichen Positionen für eine MIDI-Note.
     * @return Sortierte Liste von Positionen (niedrigste Kosten zuerst)
     */
    juce::Array<AlternatePosition> calculatePositions(int midiNote) const
    {
        juce::Array<AlternatePosition> positions;
        
        if (tuning.isEmpty())
            return positions;
        
        for (int stringIdx = 0; stringIdx < tuning.size(); ++stringIdx)
        {
            int openStringNote = tuning[stringIdx] + capoFret;
            int requiredFret = midiNote - openStringNote;
            
            if (requiredFret >= 0 && requiredFret <= maxFret)
            {
                AlternatePosition pos;
                pos.string = stringIdx;
                pos.fret = requiredFret;
                pos.cost = calculatePositionCost(stringIdx, requiredFret, midiNote);
                positions.add(pos);
            }
        }
        
        std::sort(positions.begin(), positions.end());
        return positions;
    }
    
    /**
     * Berechnet Alternativen ohne die aktuelle Position.
     */
    juce::Array<AlternatePosition> calculateAlternatives(int midiNote, int excludeString, int excludeFret) const
    {
        auto allPositions = calculatePositions(midiNote);
        
        for (int i = allPositions.size() - 1; i >= 0; --i)
        {
            if (allPositions[i].string == excludeString && allPositions[i].fret == excludeFret)
            {
                allPositions.remove(i);
                break;
            }
        }
        
        return allPositions;
    }
    
    /**
     * Berechnet die MIDI-Note für eine Bund/Saiten-Position.
     */
    int getMidiNote(int stringIdx, int fret) const
    {
        if (stringIdx < 0 || stringIdx >= tuning.size() || fret < 0)
            return -1;
        return tuning[stringIdx] + capoFret + fret;
    }
    
    /**
     * Gibt einen lesbaren Namen für eine MIDI-Note zurück.
     */
    static juce::String getMidiNoteName(int midiNote)
    {
        static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (midiNote < 0 || midiNote > 127) return "?";
        int octave = (midiNote / 12) - 1;
        int noteIndex = midiNote % 12;
        return juce::String(noteNames[noteIndex]) + juce::String(octave);
    }
    
private:
    juce::Array<int> tuning = { 64, 59, 55, 50, 45, 40 };  // Standard E-Tuning
    int capoFret = 0;
    int maxFret = 24;
    int preferredFret = 0;
    
    float calculatePositionCost(int stringIdx, int fret, int /*midiNote*/) const
    {
        float cost = 0.0f;
        
        // Leersaiten bevorzugt
        if (fret == 0) cost -= 2.0f;
        
        // Niedrige Bünde einfacher
        cost += fret * 0.1f;
        
        // Hohe Bünde schwieriger
        if (fret > 12) cost += (fret - 12) * 0.3f;
        
        // Mittlere Saiten bevorzugt
        int stringCount = tuning.size();
        float middleString = (stringCount - 1) / 2.0f;
        cost += std::abs(stringIdx - middleString) * 0.2f;
        
        // Abstand von bevorzugter Position
        if (preferredFret > 0)
            cost += std::abs(fret - preferredFret) * 0.5f;
        
        return cost;
    }
};

