/*
  ==============================================================================

    TabRenderer.h
    
    Renderer für die Tabulatur-Darstellung
    Zeichnet Saiten, Noten, Effekte und Taktstriche

  ==============================================================================
*/

#pragma once

#include "TabModels.h"
#include "TabLayoutEngine.h"
#include <juce_graphics/juce_graphics.h>

//==============================================================================
/**
 * TabRenderer
 * 
 * Zeichnet eine komplette Tabulatur mit allen visuellen Elementen.
 */
class TabRenderer
{
public:
    TabRenderer() = default;
    
    /**
     * Zeichnet einen Track in den angegebenen Bereich.
     */
    void render(juce::Graphics& g, 
                const TabTrack& track, 
                const TabLayoutConfig& config,
                const juce::Rectangle<float>& bounds,
                float scrollOffset = 0.0f,
                int highlightMeasure = -1)
    {
        if (track.measures.isEmpty())
            return;
        
        this->config = config;
        this->bounds = bounds;
        
        const int stringCount = track.stringCount;
        const float topMargin = 35.0f;  // Space for rhythm notation
        const float firstStringY = bounds.getY() + topMargin;
        
        // Background
        g.setColour(config.backgroundColor);
        g.fillRect(bounds);
        
        // Calculate visible range
        float visibleStart = scrollOffset;
        float visibleEnd = scrollOffset + bounds.getWidth();
        
        // Draw TAB clef
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(config.stringSpacing * 0.9f).boldened());
        
        float clefX = bounds.getX() + 5.0f;
        float tabClefHeight = (stringCount - 1) * config.stringSpacing;
        float tabClefCenterY = firstStringY + tabClefHeight / 2.0f;
        
        g.drawText("T", juce::Rectangle<float>(clefX, tabClefCenterY - config.stringSpacing * 1.2f, 15, config.stringSpacing), 
                   juce::Justification::centred, false);
        g.drawText("A", juce::Rectangle<float>(clefX, tabClefCenterY - config.stringSpacing * 0.4f, 15, config.stringSpacing), 
                   juce::Justification::centred, false);
        g.drawText("B", juce::Rectangle<float>(clefX, tabClefCenterY + config.stringSpacing * 0.4f, 15, config.stringSpacing), 
                   juce::Justification::centred, false);
        
        float contentStartX = bounds.getX() + 25.0f;
        
        // Draw strings
        g.setColour(config.stringColour);
        for (int s = 0; s < stringCount; ++s)
        {
            float y = firstStringY + s * config.stringSpacing;
            g.drawHorizontalLine(static_cast<int>(y), contentStartX, bounds.getRight());
        }
        
        // Draw measures and notes
        TabLayoutEngine layoutEngine;
        
        for (int m = 0; m < track.measures.size(); ++m)
        {
            const auto& measure = track.measures[m];
            float measureX = contentStartX + measure.xPosition - scrollOffset;
            float measureEndX = measureX + measure.calculatedWidth;
            
            // Skip if not visible
            if (measureEndX < bounds.getX() || measureX > bounds.getRight())
                continue;
            
            // Highlight current measure
            if (m == highlightMeasure)
            {
                g.setColour(config.playheadColour.withAlpha(0.15f));
                g.fillRect(measureX, firstStringY - 5.0f, 
                           measure.calculatedWidth, 
                           (stringCount - 1) * config.stringSpacing + 10.0f);
            }
            
            // Draw measure number
            g.setColour(juce::Colours::grey);
            g.setFont(config.measureNumberFontSize);
            g.drawText(juce::String(measure.measureNumber), 
                       juce::Rectangle<float>(measureX, firstStringY - 18.0f, 30.0f, 15.0f),
                       juce::Justification::left, false);
            
            // Get beat positions
            auto beatPositions = layoutEngine.calculateBeatPositions(measure, config);
            
            // Draw beats
            for (int b = 0; b < measure.beats.size(); ++b)
            {
                const auto& beat = measure.beats[b];
                float beatX = measureX + (b < beatPositions.size() ? beatPositions[b] : 0);
                
                // Calculate next beat X position for vibrato drawing
                float nextBeatX = measureEndX; // Default to end of measure
                if (b + 1 < measure.beats.size() && b + 1 < beatPositions.size())
                    nextBeatX = measureX + beatPositions[b + 1];
                
                // Calculate bottom of strings for rhythm notation position
                float lastStringY = firstStringY + (stringCount - 1) * config.stringSpacing;
                
                // Draw rhythm notation BELOW the staff
                drawRhythmNotation(g, beat, beatX, lastStringY + 20.0f);
                
                // Draw beat text annotation (e.g., "Don't pick")
                if (beat.text.isNotEmpty())
                {
                    drawBeatText(g, beat.text, beatX, firstStringY - 25.0f);
                }
                
                // Draw notes or rest
                if (beat.isRest)
                {
                    drawRest(g, beat, beatX, firstStringY, stringCount);
                }
                else
                {
                    // Draw each note
                    for (const auto& note : beat.notes)
                    {
                        float noteY = firstStringY + note.string * config.stringSpacing;
                        drawNote(g, note, beatX, noteY, nextBeatX, firstStringY);
                        
                        // Draw bend symbol if note has bend
                        if (note.effects.bend)
                        {
                            drawBend(g, note, beatX, noteY);
                        }
                    }
                    
                    // Draw slurs (legato connections)
                    drawSlurs(g, beat, beatX, firstStringY);
                }
            }
            
            // Draw measure bar line
            g.setColour(config.measureLineColour);
            float lineTop = firstStringY;
            float lineBottom = firstStringY + (stringCount - 1) * config.stringSpacing;
            g.drawLine(measureEndX, lineTop, measureEndX, lineBottom, 1.5f);
            
            // Draw repeat signs
            if (measure.isRepeatOpen)
                drawRepeatOpen(g, measureX, firstStringY, stringCount);
            if (measure.repeatCount > 0)
                drawRepeatClose(g, measureEndX, firstStringY, stringCount, measure.repeatCount);
        }
    }

private:
    TabLayoutConfig config;
    juce::Rectangle<float> bounds;
    
    void drawNote(juce::Graphics& g, const TabNote& note, float x, float y, float nextBeatX, float firstStringY)
    {
        const float noteRadius = config.stringSpacing * 0.45f;
        
        // Build fret text first to determine background size
        juce::String fretText;
        if (note.effects.deadNote)
            fretText = "X";
        else if (note.effects.ghostNote)
            fretText = "(" + juce::String(note.fret) + ")";
        else
            fretText = juce::String(note.fret);
        
        // Calculate text width - need wider area for multi-digit fret numbers
        g.setFont(config.fretFontSize);
        float textWidth = g.getCurrentFont().getStringWidthFloat(fretText) + 4.0f;
        float bgWidth = juce::jmax(noteRadius * 2.0f, textWidth);
        float bgHeight = noteRadius * 2.0f;
        
        // Background (to cover the string line) - sized to fit text
        g.setColour(config.backgroundColor);
        g.fillRect(x - bgWidth / 2.0f, y - bgHeight / 2.0f, bgWidth, bgHeight);
        
        // Draw fret number or effect
        g.setColour(config.fretTextColour);
        
        g.drawText(fretText, 
                   juce::Rectangle<float>(x - bgWidth / 2.0f, y - bgHeight / 2.0f, bgWidth, bgHeight),
                   juce::Justification::centred, false);
        
        // Draw vibrato ABOVE the strings (at the top) and extending to next beat
        if (note.effects.vibrato)
        {
            float vibratoY = firstStringY - 12.0f;  // Above all strings
            float vibratoWidth = nextBeatX - x - 5.0f;  // Extend to next beat
            vibratoWidth = juce::jmax(vibratoWidth, noteRadius * 3.0f);  // Minimum width
            drawVibrato(g, x, vibratoY, vibratoWidth);
        }
        
        // Draw slide
        if (note.effects.slideType != SlideType::None)
        {
            drawSlide(g, note, x, y, noteRadius);
        }
    }
    
    void drawVibrato(juce::Graphics& g, float startX, float y, float width)
    {
        g.setColour(config.vibratoColour);
        
        juce::Path vibrato;
        float amplitude = 2.5f;
        float wavelength = 6.0f;  // Pixels per wave cycle
        
        vibrato.startNewSubPath(startX, y);
        for (float dx = 0; dx <= width; dx += 0.5f)
        {
            float yOffset = std::sin(dx * 2.0f * juce::MathConstants<float>::pi / wavelength) * amplitude;
            vibrato.lineTo(startX + dx, y + yOffset);
        }
        
        g.strokePath(vibrato, juce::PathStrokeType(1.5f));
    }
    
    void drawSlide(juce::Graphics& g, const TabNote& note, float x, float y, float radius)
    {
        g.setColour(config.slideColour);
        
        float lineLength = radius * 2.5f;
        
        if (note.effects.slideType == SlideType::ShiftSlide || 
            note.effects.slideType == SlideType::LegatoSlide)
        {
            // Slide to next note (diagonal line to the right)
            g.drawLine(x + radius, y, x + lineLength, y - radius * 0.7f, 1.5f);
        }
        else if (note.effects.slideType == SlideType::SlideIntoFromBelow)
        {
            // Slide in from below
            g.drawLine(x - lineLength, y + radius * 0.7f, x - radius, y, 1.5f);
        }
        else if (note.effects.slideType == SlideType::SlideIntoFromAbove)
        {
            // Slide in from above
            g.drawLine(x - lineLength, y - radius * 0.7f, x - radius, y, 1.5f);
        }
    }
    
    void drawBend(juce::Graphics& g, const TabNote& note, float x, float y)
    {
        g.setColour(juce::Colours::black);
        
        const float noteRadius = config.stringSpacing * 0.45f;
        const float bendHeight = 20.0f;
        const float bendWidth = 15.0f;
        
        // Draw curved bend arrow
        juce::Path bendPath;
        float startX = x + noteRadius + 2.0f;
        float startY = y;
        float endY = y - bendHeight;
        
        bendPath.startNewSubPath(startX, startY);
        bendPath.quadraticTo(startX + bendWidth * 0.3f, startY - bendHeight * 0.5f,
                             startX + bendWidth * 0.5f, endY);
        
        g.strokePath(bendPath, juce::PathStrokeType(1.5f));
        
        // Draw arrowhead
        juce::Path arrow;
        float arrowX = startX + bendWidth * 0.5f;
        float arrowY = endY;
        arrow.startNewSubPath(arrowX - 3.0f, arrowY + 5.0f);
        arrow.lineTo(arrowX, arrowY);
        arrow.lineTo(arrowX + 3.0f, arrowY + 5.0f);
        g.strokePath(arrow, juce::PathStrokeType(1.5f));
        
        // Draw bend value text (e.g., "full", "1/2")
        juce::String bendText;
        float bendValue = note.effects.bendValue / 100.0f;  // GP5 stores as percentage of semitone
        
        if (bendValue >= 0.9f && bendValue <= 1.1f)
            bendText = "full";
        else if (bendValue >= 0.45f && bendValue <= 0.55f)
            bendText = juce::CharPointer_UTF8("\xC2\xBD");  // 1/2 symbol
        else if (bendValue >= 0.2f && bendValue <= 0.3f)
            bendText = juce::CharPointer_UTF8("\xC2\xBC");  // 1/4 symbol
        else if (bendValue > 1.1f)
            bendText = juce::String(bendValue, 1);
        else
            bendText = juce::String(bendValue, 1);
        
        g.setFont(9.0f);
        g.drawText(bendText, 
                   juce::Rectangle<float>(startX + bendWidth * 0.5f - 10.0f, endY - 12.0f, 20.0f, 10.0f),
                   juce::Justification::centred, false);
    }
    
    void drawBeatText(juce::Graphics& g, const juce::String& text, float x, float y)
    {
        g.setColour(juce::Colours::darkgrey);
        g.setFont(juce::Font(10.0f).italicised());
        g.drawText(text, 
                   juce::Rectangle<float>(x - 30.0f, y, 100.0f, 14.0f),
                   juce::Justification::left, false);
    }
    
    void drawSlurs(juce::Graphics& g, const TabBeat& beat, float beatX, float firstStringY)
    {
        g.setColour(juce::Colours::black);
        
        for (const auto& note : beat.notes)
        {
            float noteY = firstStringY + note.string * config.stringSpacing;
            float noteRadius = config.stringSpacing * 0.45f;
            
            // Hammer-on / Pull-off / Legato Slide: draw arc above the note
            if (note.effects.hammerOn || 
                note.effects.slideType == SlideType::LegatoSlide ||
                note.effects.slideType == SlideType::ShiftSlide ||
                note.isTied)
            {
                // Draw slur arc ABOVE the note
                juce::Path slur;
                float slurStartX = beatX + noteRadius;
                float slurEndX = beatX + noteRadius * 4;
                float slurY = noteY - noteRadius - 3.0f;  // Above the note
                float slurHeight = 4.0f;
                
                slur.startNewSubPath(slurStartX, slurY);
                slur.quadraticTo((slurStartX + slurEndX) / 2, slurY - slurHeight, 
                                 slurEndX, slurY);
                
                g.strokePath(slur, juce::PathStrokeType(1.0f));
            }
        }
    }
    
    void drawRest(juce::Graphics& g, const TabBeat& beat, float x, float firstStringY, int stringCount)
    {
        g.setColour(config.fretTextColour);
        
        // Calculate center Y position
        float centerY = firstStringY + (stringCount - 1) * config.stringSpacing / 2.0f;
        
        // Draw rest symbol based on duration
        juce::String restSymbol;
        switch (beat.duration)
        {
            case NoteDuration::Whole:
                // Whole rest: filled rectangle below line
                g.fillRect(x - 6.0f, centerY - 2.0f, 12.0f, 4.0f);
                return;
            case NoteDuration::Half:
                // Half rest: filled rectangle above line
                g.fillRect(x - 6.0f, centerY - 6.0f, 12.0f, 4.0f);
                return;
            case NoteDuration::Quarter:
                restSymbol = juce::CharPointer_UTF8("\xF0\x9D\x84\xBD"); // 𝄽
                break;
            case NoteDuration::Eighth:
                restSymbol = juce::CharPointer_UTF8("\xF0\x9D\x84\xBE"); // 𝄾
                break;
            case NoteDuration::Sixteenth:
                restSymbol = juce::CharPointer_UTF8("\xF0\x9D\x84\xBF"); // 𝄿
                break;
            default:
                restSymbol = juce::CharPointer_UTF8("\xF0\x9D\x84\xBD"); // 𝄽 default to quarter
                break;
        }
        
        g.setFont(config.stringSpacing * 2.0f);
        g.drawText(restSymbol, 
                   juce::Rectangle<float>(x - 10.0f, centerY - config.stringSpacing, 20.0f, config.stringSpacing * 2),
                   juce::Justification::centred, false);
    }
    
    void drawRhythmNotation(juce::Graphics& g, const TabBeat& beat, float x, float y)
    {
        g.setColour(juce::Colours::black);
        
        // Skip for rests (rest symbol is drawn in the tab area)
        if (beat.isRest)
            return;
        
        const float stemHeight = 12.0f;
        const float noteheadWidth = 6.0f;
        
        // Draw stem
        g.drawLine(x, y, x, y - stemHeight, 1.5f);
        
        // Draw notehead
        bool filled = (beat.duration >= NoteDuration::Quarter);
        if (filled)
            g.fillEllipse(x - noteheadWidth/2, y - 3.0f, noteheadWidth, 5.0f);
        else
            g.drawEllipse(x - noteheadWidth/2, y - 3.0f, noteheadWidth, 5.0f, 1.0f);
        
        // Draw flags for eighth notes and shorter
        if (beat.duration >= NoteDuration::Eighth)
        {
            int flagCount = 0;
            if (beat.duration == NoteDuration::Eighth) flagCount = 1;
            else if (beat.duration == NoteDuration::Sixteenth) flagCount = 2;
            else if (beat.duration == NoteDuration::ThirtySecond) flagCount = 3;
            
            for (int f = 0; f < flagCount; ++f)
            {
                float flagY = y - stemHeight + f * 3.0f;
                juce::Path flag;
                flag.startNewSubPath(x, flagY);
                flag.quadraticTo(x + 4.0f, flagY + 3.0f, x + 6.0f, flagY + 6.0f);
                g.strokePath(flag, juce::PathStrokeType(1.5f));
            }
        }
        
        // Draw dot for dotted notes
        if (beat.isDotted)
        {
            g.fillEllipse(x + noteheadWidth/2 + 2.0f, y - 1.0f, 3.0f, 3.0f);
        }
    }
    
    void drawRepeatOpen(juce::Graphics& g, float x, float y, int stringCount)
    {
        g.setColour(config.measureLineColour);
        float bottom = y + (stringCount - 1) * config.stringSpacing;
        
        // Thick bar line
        g.fillRect(x - 2.0f, y, 3.0f, bottom - y);
        // Thin bar line
        g.drawLine(x + 3.0f, y, x + 3.0f, bottom, 1.0f);
        // Dots
        float dotY1 = y + (stringCount / 2 - 1) * config.stringSpacing + config.stringSpacing / 2;
        float dotY2 = y + (stringCount / 2) * config.stringSpacing + config.stringSpacing / 2;
        g.fillEllipse(x + 6.0f, dotY1 - 2.0f, 4.0f, 4.0f);
        g.fillEllipse(x + 6.0f, dotY2 - 2.0f, 4.0f, 4.0f);
    }
    
    void drawRepeatClose(juce::Graphics& g, float x, float y, int stringCount, int repeatCount)
    {
        g.setColour(config.measureLineColour);
        float bottom = y + (stringCount - 1) * config.stringSpacing;
        
        // Dots
        float dotY1 = y + (stringCount / 2 - 1) * config.stringSpacing + config.stringSpacing / 2;
        float dotY2 = y + (stringCount / 2) * config.stringSpacing + config.stringSpacing / 2;
        g.fillEllipse(x - 10.0f, dotY1 - 2.0f, 4.0f, 4.0f);
        g.fillEllipse(x - 10.0f, dotY2 - 2.0f, 4.0f, 4.0f);
        // Thin bar line
        g.drawLine(x - 5.0f, y, x - 5.0f, bottom, 1.0f);
        // Thick bar line
        g.fillRect(x - 2.0f, y, 3.0f, bottom - y);
        
        // Repeat count text
        if (repeatCount > 1)
        {
            g.setFont(9.0f);
            g.drawText("x" + juce::String(repeatCount), 
                       juce::Rectangle<float>(x - 20.0f, y - 15.0f, 20.0f, 12.0f),
                       juce::Justification::centred, false);
        }
    }
};
