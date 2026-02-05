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
    
public:
    //==========================================================================
    // Gruppen-Alternativen-Berechnung
    //==========================================================================
    
    /**
     * Informationen über eine Note in einer Gruppe.
     */
    struct GroupNoteInfo
    {
        int midiNote = -1;
        int currentString = 0;
        int currentFret = 0;
        int measureIndex = -1;
        int beatIndex = -1;
        int noteIndex = -1;
    };
    
    /**
     * Eine alternative Position für eine Gruppe von Noten.
     * Enthält für jede Note der Gruppe die neue Position.
     */
    struct GroupAlternative
    {
        struct NotePosition
        {
            int string = 0;
            int fret = 0;
        };
        juce::Array<NotePosition> positions;  // Eine Position pro Note in der Gruppe
        float totalCost = 0.0f;               // Gesamtkosten dieser Alternative
        int fretSpan = 0;                     // Maximaler Bund-Abstand in der Gruppe
        int averageFret = 0;                  // Durchschnittlicher Bund (für Anzeige)
        
        bool operator<(const GroupAlternative& other) const { return totalCost < other.totalCost; }
    };
    
    /**
     * Berechnet alternative Positionierungen für eine Gruppe von Noten.
     * Findet bis zu maxAlternatives sinnvolle Alternativen, die alle Noten
     * in einer spielbaren Griffposition halten.
     * 
     * @param notes Die Noten der Gruppe mit ihren aktuellen Positionen
     * @param maxAlternatives Maximale Anzahl der Alternativen (default: 5)
     * @return Sortierte Liste von Gruppen-Alternativen (niedrigste Kosten zuerst)
     */
    juce::Array<GroupAlternative> calculateGroupAlternatives(
        const juce::Array<GroupNoteInfo>& notes,
        int maxAlternatives = 5) const
    {
        juce::Array<GroupAlternative> alternatives;
        
        if (notes.isEmpty() || tuning.isEmpty())
            return alternatives;
        
        // Schritt 1: Berechne für jede Note alle möglichen Positionen
        std::vector<juce::Array<AlternatePosition>> allPositions;
        for (const auto& note : notes)
        {
            auto positions = calculatePositions(note.midiNote);
            if (positions.isEmpty())
                return alternatives;  // Eine Note kann nicht gespielt werden
            allPositions.push_back(positions);
        }
        
        // Schritt 2: Finde verschiedene Fret-Regionen
        // Sammle alle möglichen Fret-Positionen
        std::set<int> allFrets;
        for (const auto& positions : allPositions)
        {
            for (const auto& pos : positions)
            {
                allFrets.insert(pos.fret);
            }
        }
        
        // Schritt 3: Für jede "Anker-Fret-Region" berechne die beste Gruppenposition
        std::vector<GroupAlternative> candidateAlternatives;
        
        for (int anchorFret : allFrets)
        {
            // Versuche die Gruppe um diese Fret-Region zu positionieren
            GroupAlternative alt;
            alt.totalCost = 0.0f;
            bool validAlternative = true;
            int minFret = 999, maxFret = 0, fretSum = 0;
            
            for (size_t i = 0; i < notes.size(); ++i)
            {
                // Finde die beste Position für diese Note nahe dem Anker-Fret
                const auto& positions = allPositions[i];
                AlternatePosition bestPos;
                float bestScore = 999999.0f;
                
                for (const auto& pos : positions)
                {
                    // Berechne Score basierend auf Distanz zum Anker und Positionskosten
                    float distanceCost = std::abs(pos.fret - anchorFret) * 2.0f;
                    float score = pos.cost + distanceCost;
                    
                    if (score < bestScore)
                    {
                        bestScore = score;
                        bestPos = pos;
                    }
                }
                
                GroupAlternative::NotePosition notePos;
                notePos.string = bestPos.string;
                notePos.fret = bestPos.fret;
                alt.positions.add(notePos);
                alt.totalCost += bestScore;
                
                minFret = juce::jmin(minFret, bestPos.fret);
                maxFret = juce::jmax(maxFret, bestPos.fret);
                fretSum += bestPos.fret;
            }
            
            alt.fretSpan = maxFret - minFret;
            alt.averageFret = fretSum / static_cast<int>(notes.size());
            
            // Bevorzuge kleine Fret-Spans (leichter zu greifen)
            alt.totalCost += alt.fretSpan * 1.5f;
            
            // Prüfe ob diese Alternative sich von der aktuellen Position unterscheidet
            bool isDifferent = false;
            for (int i = 0; i < notes.size(); ++i)
            {
                if (alt.positions[i].string != notes[i].currentString ||
                    alt.positions[i].fret != notes[i].currentFret)
                {
                    isDifferent = true;
                    break;
                }
            }
            
            if (isDifferent && validAlternative)
            {
                candidateAlternatives.push_back(alt);
            }
        }
        
        // Schritt 4: Sortiere und entferne Duplikate
        std::sort(candidateAlternatives.begin(), candidateAlternatives.end());
        
        // Entferne identische Alternativen
        for (size_t i = 0; i < candidateAlternatives.size(); ++i)
        {
            bool isDuplicate = false;
            for (int j = 0; j < alternatives.size(); ++j)
            {
                bool same = true;
                for (int k = 0; k < notes.size(); ++k)
                {
                    if (candidateAlternatives[i].positions[k].string != alternatives[j].positions[k].string ||
                        candidateAlternatives[i].positions[k].fret != alternatives[j].positions[k].fret)
                    {
                        same = false;
                        break;
                    }
                }
                if (same) { isDuplicate = true; break; }
            }
            
            if (!isDuplicate)
            {
                alternatives.add(candidateAlternatives[i]);
                if (alternatives.size() >= maxAlternatives)
                    break;
            }
        }
        
        return alternatives;
    }
};

