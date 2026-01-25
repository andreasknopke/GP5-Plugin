/*
  ==============================================================================

    TabLayoutEngine.h
    
    Layout-Engine für die Tabulatur-Darstellung
    Berechnet X-Positionen basierend auf Notenwerten (wie TuxGuitar's TGLayouter)

  ==============================================================================
*/

#pragma once

#include "TabModels.h"

//==============================================================================
/**
 * TabLayoutEngine
 * 
 * Berechnet das Layout einer Tabulatur basierend auf den Notenwerten.
 * Takte mit vielen kurzen Noten werden breiter dargestellt als Takte
 * mit wenigen langen Noten.
 */
class TabLayoutEngine
{
public:
    TabLayoutEngine() = default;
    
    /**
     * Berechnet die X-Positionen für alle Takte eines Tracks.
     * 
     * @param track Der Track mit den Takten
     * @param config Die Layout-Konfiguration
     * @param availableWidth Die verfügbare Breite (für Zeilenumbrüche)
     * @return Die Gesamtbreite aller Takte
     */
    float calculateLayout(TabTrack& track, const TabLayoutConfig& config, float availableWidth)
    {
        if (track.measures.isEmpty())
            return 0.0f;
        
        float currentX = 0.0f;
        
        for (auto& measure : track.measures)
        {
            // Berechne die Breite dieses Taktes
            measure.calculatedWidth = calculateMeasureWidth(measure, config);
            measure.xPosition = currentX;
            currentX += measure.calculatedWidth;
        }
        
        return currentX;
    }
    
    /**
     * Berechnet die X-Positionen für alle Beats innerhalb eines Taktes.
     * 
     * @param measure Der Takt
     * @param config Die Layout-Konfiguration
     * @return Array mit X-Positionen für jeden Beat (relativ zum Taktanfang)
     */
    juce::Array<float> calculateBeatPositions(const TabMeasure& measure, const TabLayoutConfig& config)
    {
        juce::Array<float> positions;
        
        if (measure.beats.isEmpty())
            return positions;
        
        // Berechne Gesamt-"Gewicht" aller Beats
        float totalWeight = 0.0f;
        for (const auto& beat : measure.beats)
        {
            totalWeight += getBeatWeight(beat);
        }
        
        if (totalWeight <= 0.0f)
            totalWeight = 1.0f;
        
        // Verfügbare Breite (ohne Padding)
        float availableWidth = measure.calculatedWidth - (config.measurePadding * 2.0f);
        
        // Berechne Positionen proportional zum Gewicht
        float currentX = config.measurePadding;
        
        for (const auto& beat : measure.beats)
        {
            positions.add(currentX);
            
            float beatWeight = getBeatWeight(beat);
            float beatWidth = (beatWeight / totalWeight) * availableWidth;
            beatWidth = juce::jmax(beatWidth, config.minBeatSpacing);
            
            currentX += beatWidth;
        }
        
        return positions;
    }
    
    /**
     * Findet den Takt an einer bestimmten X-Position.
     */
    int findMeasureAtX(const TabTrack& track, float x)
    {
        for (int i = 0; i < track.measures.size(); ++i)
        {
            const auto& measure = track.measures[i];
            if (x >= measure.xPosition && x < measure.xPosition + measure.calculatedWidth)
                return i;
        }
        return -1;
    }

private:
    /**
     * Berechnet die Breite eines einzelnen Taktes.
     */
    float calculateMeasureWidth(const TabMeasure& measure, const TabLayoutConfig& config)
    {
        if (measure.beats.isEmpty())
            return config.baseNoteWidth * 4.0f; // Leerer Takt = 4 Viertelnoten breit
        
        float totalWeight = 0.0f;
        
        for (const auto& beat : measure.beats)
        {
            totalWeight += getBeatWeight(beat);
        }
        
        // Mindestbreite basierend auf Anzahl der Beats
        float minWidth = measure.beats.size() * config.minBeatSpacing;
        
        // Breite basierend auf Gewichtung
        float weightedWidth = totalWeight * config.baseNoteWidth;
        
        // Padding hinzufügen
        float finalWidth = juce::jmax(minWidth, weightedWidth) + (config.measurePadding * 2.0f);
        
        return finalWidth;
    }
    
    /**
     * Berechnet das "Gewicht" eines Beats für das Layout.
     * Kürzere Noten haben höheres Gewicht (brauchen mehr Platz).
     */
    float getBeatWeight(const TabBeat& beat)
    {
        // Basis-Gewicht: Je kürzer die Note, desto mehr Gewicht
        float weight = 1.0f;
        
        switch (beat.duration)
        {
            case NoteDuration::Whole:         weight = 4.0f; break;
            case NoteDuration::Half:          weight = 2.0f; break;
            case NoteDuration::Quarter:       weight = 1.0f; break;
            case NoteDuration::Eighth:        weight = 0.75f; break;
            case NoteDuration::Sixteenth:     weight = 0.6f; break;
            case NoteDuration::ThirtySecond:  weight = 0.5f; break;
        }
        
        // Punktierte Noten brauchen etwas mehr Platz
        if (beat.isDotted) weight *= 1.2f;
        
        // Tuplets komprimieren
        if (beat.tupletNumerator > beat.tupletDenominator)
        {
            weight *= static_cast<float>(beat.tupletDenominator) / 
                      static_cast<float>(beat.tupletNumerator);
        }
        
        return weight;
    }
};
