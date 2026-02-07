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
 * RenderedNoteInfo - Speichert Informationen über eine gerenderte Note für Hit-Testing.
 */
struct RenderedNoteInfo
{
    juce::Rectangle<float> bounds;
    int measureIndex = -1;
    int beatIndex = -1;
    int noteIndex = -1;
    int stringIndex = 0;
    int fret = 0;
    int midiNote = -1;
};

/**
 * RenderedChordInfo - Speichert Position eines gerenderten Akkordnamens für Hit-Testing.
 */
struct RenderedChordInfo
{
    juce::Rectangle<float> bounds;   // Klickbarer Bereich des Akkordnamens
    juce::String chordName;          // Akkordname (z.B. "Am7")
    int measureIndex = -1;           // Takt-Index
    int beatIndex = -1;              // Beat-Index wo der Akkord steht
};

/**
 * RenderedRestInfo - Speichert Position einer gerenderten Pause für Hit-Testing.
 */
struct RenderedRestInfo
{
    juce::Rectangle<float> bounds;
    int measureIndex = -1;
    int beatIndex = -1;
    NoteDuration duration = NoteDuration::Quarter;
    bool isDotted = false;
};

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
    
    const juce::Array<RenderedNoteInfo>& getRenderedNotes() const { return renderedNotes; }
    const juce::Array<RenderedChordInfo>& getRenderedChords() const { return renderedChords; }
    const juce::Array<RenderedRestInfo>& getRenderedRests() const { return renderedRests; }
    void clearRenderedNotes() { renderedNotes.clear(); renderedChords.clear(); renderedRests.clear(); }
    
    /** Set notes to hide (for ghost preview - hide original notes when showing alternatives) */
    void setHiddenNotes(const std::vector<std::tuple<int, int, int>>& notes)
    {
        hiddenNotes = notes;  // Each tuple: measureIndex, beatIndex, noteIndex
    }
    
    void clearHiddenNotes() { hiddenNotes.clear(); }
    
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
        this->currentTrackTuning = track.tuning;
        renderedNotes.clear();
        renderedChords.clear();
        renderedRests.clear();
        
        const int stringCount = track.stringCount;
        const float firstStringY = bounds.getY() + config.topMargin;  // Verwende Config-Margin
        
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
            
            // Calculate bottom of strings for rhythm notation position
            float lastStringY = firstStringY + (stringCount - 1) * config.stringSpacing;
            
            // Draw rhythm notation with beaming ONCE per measure (outside beat loop)
            drawRhythmNotationWithBeaming(g, measure, beatPositions, measureX, lastStringY + 15.0f);
            
            // Draw beats
            for (int b = 0; b < measure.beats.size(); ++b)
            {
                const auto& beat = measure.beats[b];
                float beatX = measureX + (b < beatPositions.size() ? beatPositions[b] : 0);
                
                // Calculate next beat X position for vibrato drawing
                float nextBeatX = measureEndX; // Default to end of measure
                if (b + 1 < measure.beats.size() && b + 1 < beatPositions.size())
                    nextBeatX = measureX + beatPositions[b + 1];
                
                // Draw beat text annotation (e.g., "Don't pick")
                if (beat.text.isNotEmpty())
                {
                    drawBeatText(g, beat.text, beatX, firstStringY - 25.0f);
                }
                
                // Draw chord name above the beat (e.g., "Am7", "C")
                if (beat.chordName.isNotEmpty())
                {
                    drawChordName(g, beat.chordName, beatX, firstStringY - 40.0f, m, b);
                }
                
                // Draw Palm Mute indicator (P.M.)
                if (beat.isPalmMuted)
                {
                    drawPalmMute(g, beatX, nextBeatX, firstStringY - 20.0f);
                }
                
                // Setze Kontext für Note/Rest-Tracking
                currentMeasureIndex = m;
                currentBeatIndex = b;
                
                // Draw notes or rest - NIEMALS beides!
                if (beat.isRest)
                {
                    // Bei Pausen: NUR Pausensymbol zeichnen, KEINE Noten!
                    drawRest(g, beat, beatX, firstStringY, stringCount);
                }
                else if (!beat.notes.isEmpty())  // Nur zeichnen wenn Noten vorhanden
                {
                    
                    // Draw each note
                    int noteIdx = 0;
                    for (const auto& note : beat.notes)
                    {
                        // Überspringe leere Note-Slots (fret = -1 bedeutet keine Note auf dieser Saite)
                        if (note.fret < 0)
                        {
                            noteIdx++;
                            continue;
                        }
                        
                        // Skip hidden notes (for ghost preview)
                        if (isNoteHidden(m, b, noteIdx))
                        {
                            noteIdx++;
                            continue;
                        }
                        
                        currentNoteIndex = noteIdx;
                        float noteY = firstStringY + note.string * config.stringSpacing;
                        drawNote(g, note, beatX, noteY, nextBeatX, firstStringY);
                        noteIdx++;
                        
                        // Draw bend symbol if note has bend
                        if (note.effects.bend)
                        {
                            drawBend(g, note, beatX, noteY, nextBeatX, firstStringY);
                        }
                    }
                    
                    // Draw slurs (legato connections) - aber NICHT für Slides!
                    // Prüfe ob die NÄCHSTE Note eine Tied-Note ist (für Haltebögen)
                    if (b + 1 < measure.beats.size())
                    {
                        const auto& nextBeat = measure.beats[b + 1];
                        drawSlurs(g, beat, nextBeat, beatX, nextBeatX, firstStringY);
                    }
                    else
                    {
                        // Kein nächster Beat - nur Hammer-on/Pull-off ohne Ziel zeichnen
                        TabBeat emptyBeat;
                        drawSlurs(g, beat, emptyBeat, beatX, nextBeatX, firstStringY);
                    }
                    
                    // Draw slides to next beat
                    // Finde die nächste Note auf derselben Saite im nächsten Beat
                    if (b + 1 < measure.beats.size())
                    {
                        const auto& nextBeat = measure.beats[b + 1];
                        if (!nextBeat.isRest)
                        {
                            for (const auto& note : beat.notes)
                            {
                                if (note.fret < 0) continue;  // Skip empty note slots
                                if (note.effects.slideType == SlideType::ShiftSlide ||
                                    note.effects.slideType == SlideType::LegatoSlide)
                                {
                                    // Finde die Zielnote im nächsten Beat
                                    for (const auto& nextNote : nextBeat.notes)
                                    {
                                        // Slide geht typischerweise zur gleichen Saite
                                        if (nextNote.string == note.string)
                                        {
                                            float noteY = firstStringY + note.string * config.stringSpacing;
                                            float nextNoteY = firstStringY + nextNote.string * config.stringSpacing;
                                            drawSlideLine(g, beatX, nextBeatX, noteY, nextNoteY,
                                                         note.effects.slideType, note.fret, nextNote.fret);
                                            break;
                                        }
                                    }
                                    // Wenn keine Note auf derselben Saite, nimm die erste Note
                                    if (!nextBeat.notes.isEmpty())
                                    {
                                        bool foundOnSameString = false;
                                        for (const auto& nextNote : nextBeat.notes)
                                        {
                                            if (nextNote.string == note.string)
                                            {
                                                foundOnSameString = true;
                                                break;
                                            }
                                        }
                                        if (!foundOnSameString)
                                        {
                                            // Slide zur ersten Note des nächsten Beats
                                            const auto& nextNote = nextBeat.notes[0];
                                            float noteY = firstStringY + note.string * config.stringSpacing;
                                            float nextNoteY = firstStringY + nextNote.string * config.stringSpacing;
                                            drawSlideLine(g, beatX, nextBeatX, noteY, nextNoteY,
                                                         note.effects.slideType, note.fret, nextNote.fret);
                                        }
                                    }
                                }
                                else if (note.effects.slideType == SlideType::SlideIntoFromBelow ||
                                         note.effects.slideType == SlideType::SlideIntoFromAbove)
                                {
                                    float noteY = firstStringY + note.string * config.stringSpacing;
                                    drawSlideInto(g, beatX, noteY, note.effects.slideType);
                                }
                                else if (note.effects.slideType == SlideType::SlideOutDownwards ||
                                         note.effects.slideType == SlideType::SlideOutUpwards)
                                {
                                    float noteY = firstStringY + note.string * config.stringSpacing;
                                    drawSlideOut(g, beatX, noteY, note.effects.slideType);
                                }
                            }
                        }
                    }
                    else
                    {
                        // Letzter Beat im Takt - zeichne Slides ohne Zielnote
                        for (const auto& note : beat.notes)
                        {
                            if (note.fret < 0) continue;  // Skip empty note slots
                            float noteY = firstStringY + note.string * config.stringSpacing;
                            
                            if (note.effects.slideType == SlideType::SlideIntoFromBelow ||
                                note.effects.slideType == SlideType::SlideIntoFromAbove)
                            {
                                drawSlideInto(g, beatX, noteY, note.effects.slideType);
                            }
                            else if (note.effects.slideType == SlideType::SlideOutDownwards ||
                                     note.effects.slideType == SlideType::SlideOutUpwards ||
                                     note.effects.slideType == SlideType::ShiftSlide ||
                                     note.effects.slideType == SlideType::LegatoSlide)
                            {
                                // Slide nach rechts ohne definiertes Ziel
                                drawSlideOut(g, beatX, noteY, SlideType::SlideOutUpwards);
                            }
                        }
                    }
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
    juce::Array<RenderedNoteInfo> renderedNotes;
    juce::Array<RenderedChordInfo> renderedChords;
    juce::Array<RenderedRestInfo> renderedRests;
    juce::Array<int> currentTrackTuning;
    int currentMeasureIndex = 0;
    int currentBeatIndex = 0;
    int currentNoteIndex = 0;
    std::vector<std::tuple<int, int, int>> hiddenNotes;  // Notes to hide for ghost preview
    
    bool isNoteHidden(int measureIdx, int beatIdx, int noteIdx) const
    {
        for (const auto& hidden : hiddenNotes)
        {
            if (std::get<0>(hidden) == measureIdx && 
                std::get<1>(hidden) == beatIdx && 
                std::get<2>(hidden) == noteIdx)
                return true;
        }
        return false;
    }
    
    void drawNote(juce::Graphics& g, const TabNote& note, float x, float y, float nextBeatX, float firstStringY)
    {
        const float noteRadius = config.stringSpacing * 0.45f;
        
        // Build fret text first to determine background size
        juce::String fretText;
        if (note.effects.deadNote)
            fretText = "X";
        else if (note.isTied)
            fretText = "(" + juce::String(note.fret) + ")";  // Tied notes in Klammern
        else if (note.effects.ghostNote)
            fretText = "(" + juce::String(note.fret) + ")";
        else
            fretText = juce::String(note.fret);
        
        // Calculate text width - need wider area for multi-digit fret numbers
        g.setFont(config.fretFontSize);
        float textWidth = g.getCurrentFont().getStringWidthFloat(fretText) + 4.0f;
        float bgWidth = juce::jmax(noteRadius * 2.0f, textWidth);
        float bgHeight = noteRadius * 2.0f;
        
        // Speichere die Noten-Info für Hit-Testing
        juce::Rectangle<float> noteBounds(x - bgWidth / 2.0f - 2.0f, y - bgHeight / 2.0f - 2.0f, 
                                           bgWidth + 4.0f, bgHeight + 4.0f);
        RenderedNoteInfo noteInfo;
        noteInfo.bounds = noteBounds;
        noteInfo.measureIndex = currentMeasureIndex;
        noteInfo.beatIndex = currentBeatIndex;
        noteInfo.noteIndex = currentNoteIndex;
        noteInfo.stringIndex = note.string;
        noteInfo.fret = note.fret;
        noteInfo.midiNote = (note.midiNote >= 0) ? note.midiNote :
            ((note.string >= 0 && note.string < currentTrackTuning.size()) ? 
             currentTrackTuning[note.string] + note.fret : -1);
        renderedNotes.add(noteInfo);
        
        // Highlight für manuell editierte Noten
        if (note.isManuallyEdited)
        {
            g.setColour(juce::Colour(0x3000BFFF));
            g.fillRoundedRectangle(noteBounds, 3.0f);
        }
        
        // Background (to cover the string line) - sized to fit text
        g.setColour(config.backgroundColor);
        g.fillRect(x - bgWidth / 2.0f, y - bgHeight / 2.0f, bgWidth, bgHeight);
        
        // Draw fret number or effect
        g.setColour(config.fretTextColour);
        
        g.drawText(fretText, 
                   juce::Rectangle<float>(x - bgWidth / 2.0f, y - bgHeight / 2.0f, bgWidth, bgHeight),
                   juce::Justification::centred, false);
        
        // Draw Tapping indicator (T) above the note
        if (note.effects.tapping)
        {
            g.setFont(9.0f);
            g.drawText("T",
                       juce::Rectangle<float>(x - 5.0f, y - noteRadius - 12.0f, 10.0f, 10.0f),
                       juce::Justification::centred, false);
        }
        
        // Draw vibrato ABOVE the strings (at the top) and extending to next beat
        if (note.effects.vibrato)
        {
            float vibratoY = firstStringY - 12.0f;  // Above all strings
            float vibratoWidth = nextBeatX - x - 5.0f;  // Extend to next beat
            vibratoWidth = juce::jmax(vibratoWidth, noteRadius * 3.0f);  // Minimum width
            drawVibrato(g, x, vibratoY, vibratoWidth);
        }
        
        // Slides werden separat in drawSlides() gezeichnet, nicht hier
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
        // Diese Funktion wird nicht mehr verwendet - Slides werden in drawSlides() gezeichnet
        juce::ignoreUnused(g, note, x, y, radius);
    }
    
    /**
     * Zeichnet Slides zwischen der aktuellen Note und der nächsten Note.
     * Wird für jede Note aufgerufen die einen Slide zur nächsten Note hat.
     * 
     * @param fromX X-Position der Startnote
     * @param toX X-Position der Zielnote  
     * @param fromY Y-Position der Startnote (auf der Saite)
     * @param toY Y-Position der Zielnote (kann gleiche oder andere Saite sein)
     * @param slideType Art des Slides (Shift, Legato, etc.)
     * @param fromFret Bund der Startnote (für Richtungsbestimmung)
     * @param toFret Bund der Zielnote (für Richtungsbestimmung)
     */
    void drawSlideLine(juce::Graphics& g, float fromX, float toX, float fromY, float toY,
                       SlideType slideType, int fromFret, int toFret)
    {
        g.setColour(config.slideColour);
        
        const float noteRadius = config.stringSpacing * 0.45f;
        
        // Start- und Endpunkte der Slide-Linie
        // Die Linie beginnt rechts von der ersten Note und endet links von der zweiten
        float startX = fromX + noteRadius + 2.0f;
        float endX = toX - noteRadius - 2.0f;
        
        // Y-Offset für die schräge Linie
        float yOffset = noteRadius * 0.5f;
        float startY = fromY;
        float endY = toY;
        
        // Slide-Richtung basierend auf Bundnummern:
        // - Upslide (zu höherem Bund): Linie geht von UNTEN nach OBEN
        // - Downslide (zu niedrigerem Bund): Linie geht von OBEN nach UNTEN
        if (toFret > fromFret)
        {
            // Upslide: Start unten, Ende oben
            startY = fromY + yOffset;
            endY = toY - yOffset;
        }
        else if (toFret < fromFret)
        {
            // Downslide: Start oben, Ende unten
            startY = fromY - yOffset;
            endY = toY + yOffset;
        }
        // Bei gleichem Bund: horizontale Linie (selten, aber möglich)
        
        // Zeichne die diagonale Slide-Linie
        g.drawLine(startX, startY, endX, endY, 1.5f);
        
        // Bei Legato-Slide: zusätzlich einen Bogen zeichnen
        if (slideType == SlideType::LegatoSlide)
        {
            juce::Path slur;
            float midX = (startX + endX) / 2.0f;
            float slurY = juce::jmin(fromY, toY) - noteRadius - 5.0f;  // Über den Noten
            float slurHeight = 6.0f;
            
            slur.startNewSubPath(fromX + noteRadius, fromY - noteRadius - 2.0f);
            slur.quadraticTo(midX, slurY - slurHeight, toX - noteRadius, toY - noteRadius - 2.0f);
            
            g.strokePath(slur, juce::PathStrokeType(1.0f));
        }
    }
    
    /**
     * Zeichnet Slide-Into Effekte (von außerhalb kommend)
     */
    void drawSlideInto(juce::Graphics& g, float x, float y, SlideType slideType)
    {
        g.setColour(config.slideColour);
        
        const float noteRadius = config.stringSpacing * 0.45f;
        float lineLength = noteRadius * 2.0f;
        float yOffset = noteRadius * 0.5f;
        
        if (slideType == SlideType::SlideIntoFromBelow)
        {
            // Diagonale Linie von unten-links nach oben-rechts zur Note
            g.drawLine(x - lineLength, y + yOffset, x - noteRadius - 2.0f, y - yOffset * 0.3f, 1.5f);
        }
        else if (slideType == SlideType::SlideIntoFromAbove)
        {
            // Diagonale Linie von oben-links nach unten-rechts zur Note
            g.drawLine(x - lineLength, y - yOffset, x - noteRadius - 2.0f, y + yOffset * 0.3f, 1.5f);
        }
    }
    
    /**
     * Zeichnet Slide-Out Effekte (nach außen gehend)
     */
    void drawSlideOut(juce::Graphics& g, float x, float y, SlideType slideType)
    {
        g.setColour(config.slideColour);
        
        const float noteRadius = config.stringSpacing * 0.45f;
        float lineLength = noteRadius * 2.0f;
        float yOffset = noteRadius * 0.5f;
        
        if (slideType == SlideType::SlideOutDownwards)
        {
            // Diagonale Linie von der Note nach unten-rechts
            g.drawLine(x + noteRadius + 2.0f, y + yOffset * 0.3f, x + lineLength + noteRadius, y + yOffset, 1.5f);
        }
        else if (slideType == SlideType::SlideOutUpwards)
        {
            // Diagonale Linie von der Note nach oben-rechts
            g.drawLine(x + noteRadius + 2.0f, y - yOffset * 0.3f, x + lineLength + noteRadius, y - yOffset, 1.5f);
        }
    }
    
    void drawBend(juce::Graphics& g, const TabNote& note, float x, float y, float nextBeatX, float firstStringY)
    {
        g.setColour(juce::Colours::black);
        
        const float noteRadius = config.stringSpacing * 0.4f;
        const float bendHeight = 24.0f;  // FESTE Höhe für alle Bends
        
        float startX = x + noteRadius + 2.0f;
        float startY = y;
        
        // Berechne die verfügbare Breite bis zur nächsten Note
        float availableWidth = nextBeatX - startX - noteRadius - 4.0f;
        availableWidth = juce::jmax(availableWidth, 20.0f);  // Minimum 20px
        
        float endX = startX + availableWidth;

        // Custom drawing if bend points exist (Detailed Mode)
        if (!note.effects.bendPoints.empty())
        {
            float unitScale = bendHeight / 200.0f; // 200 units = Full (24px)
            
            juce::Path bendPath;
            bool first = true;
            
            for (const auto& bp : note.effects.bendPoints)
            {
                float px = startX + (bp.position / 60.0f) * availableWidth;
                float py = startY - (bp.value * unitScale);
                
                if (first) { bendPath.startNewSubPath(px, py); first = false; }
                else { bendPath.lineTo(px, py); }
            }
            g.strokePath(bendPath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            // Draw values at peaks/ends
            auto drawVal = [&](int value, int pos) {
                if (value < 50) return;  // Don't label bends < 0.5 semitones
                float valF = value / 100.0f;
                juce::String txt;
                if (valF >= 1.9f && valF <= 2.1f) txt = "full";
                else if (valF >= 0.9f && valF <= 1.1f) txt = juce::CharPointer_UTF8("\xC2\xBD");
                else if (valF >= 0.4f && valF <= 0.6f) txt = juce::CharPointer_UTF8("\xC2\xBC");
                else txt = juce::String(valF, 1);
                
                float px = startX + (pos / 60.0f) * availableWidth;
                float py = startY - (value * unitScale);
                g.setFont(9.0f);
                g.drawText(txt, juce::Rectangle<float>(px - 10, py - 12, 20, 10), juce::Justification::centred, false);
            };
            
            // Logic to find display points (peaks and high plateaus)
            for (size_t i = 0; i < note.effects.bendPoints.size(); ++i) {
                const auto& bp = note.effects.bendPoints[i];
                // Always draw last point if high
                if (i == note.effects.bendPoints.size() - 1) { drawVal(bp.value, bp.position); continue; }
                
                // Draw local maxima
                if (i > 0 && i < note.effects.bendPoints.size() - 1) {
                    if (bp.value > note.effects.bendPoints[i-1].value && bp.value >= note.effects.bendPoints[i+1].value)
                        drawVal(bp.value, bp.position);
                }
            }
            return;
        }

        float targetY = y - bendHeight;  // Feste Höhe über der Note
        
        // Bestimme Bend-Wert Text
        juce::String bendText;
        float bendValue = note.effects.bendValue;  // In Halbtönen
        
        if (bendValue >= 1.9f && bendValue <= 2.1f)
            bendText = "full";
        else if (bendValue >= 0.9f && bendValue <= 1.1f)
            bendText = juce::CharPointer_UTF8("\xC2\xBD");  // ½
        else if (bendValue >= 1.4f && bendValue <= 1.6f)
            bendText = juce::CharPointer_UTF8("\xC2\xBE");  // ¾
        else if (bendValue >= 0.4f && bendValue <= 0.6f)
            bendText = juce::CharPointer_UTF8("\xC2\xBC");  // ¼
        else if (bendValue >= 3.9f)
            bendText = "2";
        else if (bendValue > 2.1f && bendValue < 3.9f)
            bendText = juce::String("1") + juce::String(juce::CharPointer_UTF8("\xC2\xBD"));
        else
            bendText = juce::String(bendValue / 2.0f, 1);
        
        bool isRelease = note.effects.releaseBend;
        bool isBendRelease = (note.effects.bendType == 2 || note.effects.bendType == 5);
        
        float width = availableWidth;
        float height = bendHeight;  // Feste Höhe
        
        if (isBendRelease)
        {
            // Bend + Release: Peak in der Mitte, dann zurück
            float peakX = startX + width * 0.5f;
            float releaseEndX = endX;
            
            // === BEND UP (Peitschenschlag bis zur Mitte) ===
            juce::Path bendUp;
            bendUp.startNewSubPath(startX, startY);
            float cp1X = startX + (width * 0.25f);  // 50% der ersten Hälfte
            float cp1Y = startY;                     // Horizontal starten
            float cp2X = peakX - (width * 0.1f);    // Kurz vor Peak
            float cp2Y = targetY + (height * 0.2f); // Fast oben
            bendUp.cubicTo(cp1X, cp1Y, cp2X, cp2Y, peakX, targetY);
            g.strokePath(bendUp, juce::PathStrokeType(1.5f));
            
            // Pfeilspitze oben
            juce::Path arrowUp;
            arrowUp.startNewSubPath(peakX - 3.0f, targetY + 5.0f);
            arrowUp.lineTo(peakX, targetY);
            arrowUp.lineTo(peakX + 3.0f, targetY + 5.0f);
            g.strokePath(arrowUp, juce::PathStrokeType(1.5f));
            
            // === RELEASE (von Peak zur nächsten Note) ===
            juce::Path bendDown;
            bendDown.startNewSubPath(peakX, targetY);
            float cp1X_rel = peakX + (width * 0.1f);
            float cp1Y_rel = targetY;  // Horizontal starten
            float cp2X_rel = releaseEndX - (width * 0.15f);
            float cp2Y_rel = startY - (height * 0.2f);
            bendDown.cubicTo(cp1X_rel, cp1Y_rel, cp2X_rel, cp2Y_rel, releaseEndX, startY);
            g.strokePath(bendDown, juce::PathStrokeType(1.5f));
            
            // Pfeilspitze unten
            juce::Path arrowDown;
            arrowDown.startNewSubPath(releaseEndX - 3.0f, startY - 5.0f);
            arrowDown.lineTo(releaseEndX, startY);
            arrowDown.lineTo(releaseEndX + 3.0f, startY - 5.0f);
            g.strokePath(arrowDown, juce::PathStrokeType(1.5f));
            
            // Text über dem Peak
            g.setFont(9.0f);
            g.drawText(bendText, 
                       juce::Rectangle<float>(peakX - 12.0f, targetY - 13.0f, 24.0f, 12.0f),
                       juce::Justification::centred, false);
        }
        else if (isRelease)
        {
            // === NUR RELEASE (von oben nach unten, volle Breite) ===
            juce::Path releasePath;
            releasePath.startNewSubPath(startX, targetY);  // Startet OBEN
            
            // Kubische Kurve für weichen Bogen
            float cp1X_rel = startX + (width * 0.2f);
            float cp1Y_rel = targetY;  // Erst horizontal
            float cp2X_rel = endX - (width * 0.15f);
            float cp2Y_rel = startY - (height * 0.3f);
            
            releasePath.cubicTo(cp1X_rel, cp1Y_rel, cp2X_rel, cp2Y_rel, endX, startY);
            g.strokePath(releasePath, juce::PathStrokeType(1.5f));
            
            // Pfeilspitze unten
            juce::Path arrow;
            arrow.startNewSubPath(endX - 3.0f, startY - 5.0f);
            arrow.lineTo(endX, startY);
            arrow.lineTo(endX + 3.0f, startY - 5.0f);
            g.strokePath(arrow, juce::PathStrokeType(1.5f));
            
            // Text über dem Startpunkt
            g.setFont(9.0f);
            g.drawText(bendText, 
                       juce::Rectangle<float>(startX - 5.0f, targetY - 13.0f, 24.0f, 12.0f),
                       juce::Justification::centred, false);
        }
        else
        {
            // === NORMALER BEND UP (volle Breite bis zur nächsten Note) ===
            juce::Path bendPath;
            bendPath.startNewSubPath(startX, startY);
            
            // CP1: Bleibt lange flach (70% der Breite auf Starthöhe)
            float cp1X = startX + (width * 0.7f);
            float cp1Y = startY;
            
            // CP2: Steiler Anstieg am Ende
            float cp2X = endX;
            float cp2Y = targetY + (height * 0.2f);
            
            bendPath.cubicTo(cp1X, cp1Y, cp2X, cp2Y, endX, targetY);
            g.strokePath(bendPath, juce::PathStrokeType(1.5f));
            
            // Pfeilspitze oben
            juce::Path arrow;
            arrow.startNewSubPath(endX - 3.0f, targetY + 5.0f);
            arrow.lineTo(endX, targetY);
            arrow.lineTo(endX + 3.0f, targetY + 5.0f);
            g.strokePath(arrow, juce::PathStrokeType(1.5f));
            
            // Text über dem Pfeil
            g.setFont(9.0f);
            g.drawText(bendText, 
                       juce::Rectangle<float>(endX - 12.0f, targetY - 13.0f, 24.0f, 12.0f),
                       juce::Justification::centred, false);
        }
    }
    
    void drawBeatText(juce::Graphics& g, const juce::String& text, float x, float y)
    {
        g.setColour(juce::Colours::darkgrey);
        g.setFont(juce::Font(10.0f).italicised());
        g.drawText(text, 
                   juce::Rectangle<float>(x - 30.0f, y, 100.0f, 14.0f),
                   juce::Justification::left, false);
    }
    
    /**
     * Zeichnet Akkordname über dem Beat (z.B. "Am7", "C", "D/F#")
     */
    void drawChordName(juce::Graphics& g, const juce::String& chordName, float x, float y,
                        int measureIndex = -1, int beatIndex = -1)
    {
        g.setColour(config.fretTextColour);
        g.setFont(juce::Font(12.0f).boldened());
        
        // Berechne Textbreite für besseren Hit-Test-Bereich
        float textWidth = juce::jmax(60.0f, static_cast<float>(chordName.length()) * 8.0f + 10.0f);
        juce::Rectangle<float> chordBounds(x - 5.0f, y, textWidth, 16.0f);
        
        g.drawText(chordName, chordBounds, juce::Justification::left, false);
        
        // Speichere für Hit-Testing
        if (measureIndex >= 0)
        {
            RenderedChordInfo chordInfo;
            chordInfo.bounds = chordBounds;
            chordInfo.chordName = chordName;
            chordInfo.measureIndex = measureIndex;
            chordInfo.beatIndex = beatIndex;
            renderedChords.add(chordInfo);
        }
    }
    
    /**
     * Zeichnet Palm Mute Indikator (P.M. mit gepunkteter Linie)
     */
    void drawPalmMute(juce::Graphics& g, float startX, float endX, float y)
    {
        g.setColour(config.palmMuteColour);
        g.setFont(8.0f);
        
        // Zeichne "P.M." Text
        g.drawText("P.M.",
                   juce::Rectangle<float>(startX - 2.0f, y - 5.0f, 20.0f, 10.0f),
                   juce::Justification::left, false);
        
        // Zeichne gepunktete Linie bis zum nächsten Beat
        float lineStartX = startX + 18.0f;
        float lineY = y;
        float dotSpacing = 3.0f;
        
        for (float dx = lineStartX; dx < endX - 5.0f; dx += dotSpacing)
        {
            g.fillEllipse(dx, lineY - 1.0f, 2.0f, 2.0f);
        }
    }
    
    void drawSlurs(juce::Graphics& g, const TabBeat& beat, const TabBeat& nextBeat, 
                    float beatX, float nextBeatX, float firstStringY)
    {
        // WICHTIG: Keine Bögen zeichnen bei Pausen!
        if (beat.isRest)
            return;
        
        g.setColour(juce::Colours::black);
        
        for (const auto& note : beat.notes)
        {
            if (note.fret < 0) continue;  // Skip empty note slots
            float noteY = firstStringY + note.string * config.stringSpacing;
            float noteRadius = config.stringSpacing * 0.45f;
            
            // Prüfe ob die nächste Note auf derselben Saite eine Tied-Note ist
            bool nextIsTied = false;
            for (const auto& nextNote : nextBeat.notes)
            {
                if (nextNote.string == note.string && nextNote.isTied)
                {
                    nextIsTied = true;
                    break;
                }
            }
            
            // NUR Bögen zeichnen wenn EXPLIZIT im GP5 definiert:
            // - Hammer-on (H)
            // - Pull-off (P)
            // - Tied note - ABER: Der Bogen geht VON dieser Note ZUR nächsten (tied) Note
            // NICHT für Slides - die haben eigene Symbole
            bool isHammerOn = note.effects.hammerOn;
            bool isPullOff = note.effects.pullOff;
            
            if (isHammerOn || isPullOff || nextIsTied)
            {
                // Draw slur arc ABOVE the note - von dieser Note zur nächsten
                juce::Path slur;
                float slurStartX = beatX + noteRadius;
                float slurEndX = nextBeatX - noteRadius;  // Bis zur nächsten Beat-Position
                float slurY = noteY - noteRadius - 3.0f;  // Above the note
                float slurHeight = 6.0f;
                
                slur.startNewSubPath(slurStartX, slurY);
                slur.quadraticTo((slurStartX + slurEndX) / 2, slurY - slurHeight, 
                                 slurEndX, slurY);
                
                g.strokePath(slur, juce::PathStrokeType(1.0f));
                
                // Zeichne H oder P Kennzeichnung über dem Bogen (nicht für tied notes)
                if (isHammerOn || isPullOff)
                {
                    juce::String hpText = isHammerOn ? "H" : "P";
                    float textX = (slurStartX + slurEndX) / 2.0f;
                    float textY = slurY - slurHeight - 10.0f;  // Über dem Bogen
                    
                    g.setFont(9.0f);
                    g.drawText(hpText,
                               juce::Rectangle<float>(textX - 5.0f, textY, 10.0f, 10.0f),
                               juce::Justification::centred, false);
                }
            }
        }
    }
    
    void drawRest(juce::Graphics& g, const TabBeat& beat, float x, float firstStringY, int stringCount)
    {
        g.setColour(config.fretTextColour);
        
        // Calculate center Y position
        float centerY = firstStringY + (stringCount - 1) * config.stringSpacing / 2.0f;
        
        // Calculate bounds for hit-testing (unified for all rest types)
        float restBoundsW = 24.0f;
        float restBoundsH = config.stringSpacing * 2.0f;
        juce::Rectangle<float> restBounds(x - restBoundsW / 2.0f, centerY - restBoundsH / 2.0f, restBoundsW, restBoundsH);
        
        // Store rest info for hit-testing
        RenderedRestInfo restInfo;
        restInfo.bounds = restBounds;
        restInfo.measureIndex = currentMeasureIndex;
        restInfo.beatIndex = currentBeatIndex;
        restInfo.duration = beat.duration;
        restInfo.isDotted = beat.isDotted;
        renderedRests.add(restInfo);
        
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
    
    void drawRhythmNotationWithBeaming(juce::Graphics& g, const TabMeasure& measure,
                                         const juce::Array<float>& beatPositions, 
                                         float measureX, float y)
    {
        g.setColour(juce::Colours::black);
        
        const float stemHeight = 12.0f;
        const float noteheadWidth = 6.0f;
        const float beamThickness = 2.5f;
        
        // Berechne Beam-Gruppen-Dauer basierend auf Taktart
        // Guitar Pro Stil: Gruppiere Achtel pro HALBTAKT in 4/4, 2/4
        const int ppq = 960; // Ticks pro Viertel
        int ticksPerBeamGroup = ppq * 2; // Standard: Halbe Note (4 Achtel pro Gruppe)
        
        // Für zusammengesetzte Taktarten (6/8, 9/8, 12/8) gruppiere in punktierten Vierteln (3 Achtel)
        if (measure.timeSignatureDenominator == 8 && 
            (measure.timeSignatureNumerator == 6 || 
             measure.timeSignatureNumerator == 9 || 
             measure.timeSignatureNumerator == 12))
        {
            ticksPerBeamGroup = (ppq * 3) / 2; // Punktierte Viertel = 3 Achtel
        }
        // Für ungerade Taktarten (3/4, 5/4, 7/4) gruppiere pro Viertel (2 Achtel)
        else if (measure.timeSignatureNumerator % 2 != 0)
        {
            ticksPerBeamGroup = ppq; // Viertel = 2 Achtel pro Gruppe
        }
        // Für 2/4 gruppiere den ganzen Takt (= 4 Achtel entspricht 2/4 Takt)
        else if (measure.timeSignatureNumerator == 2 && measure.timeSignatureDenominator == 4)
        {
            ticksPerBeamGroup = ppq * 2; // Halbe = ganzer 2/4 Takt
        }
        
        // Sammle Noten mit ihrer Position im Takt
        struct BeamableNote {
            int beatIndex;
            int tickPosition; // Position im Takt in Ticks
            int durationTicks;
        };
        std::vector<BeamableNote> beamableNotes;
        
        int currentTick = 0;
        for (int b = 0; b < measure.beats.size(); ++b)
        {
            const auto& beat = measure.beats[b];
            
            // Berechne Dauer in Ticks
            int durationTicks = ppq; // Standard: Viertel
            switch (beat.duration)
            {
                case NoteDuration::Whole: durationTicks = ppq * 4; break;
                case NoteDuration::Half: durationTicks = ppq * 2; break;
                case NoteDuration::Quarter: durationTicks = ppq; break;
                case NoteDuration::Eighth: durationTicks = ppq / 2; break;
                case NoteDuration::Sixteenth: durationTicks = ppq / 4; break;
                case NoteDuration::ThirtySecond: durationTicks = ppq / 8; break;
            }
            if (beat.isDotted) durationTicks = durationTicks * 3 / 2;
            
            // Nur Beam-fähige Noten (Achtel und kürzer, keine Pausen)
            bool shouldBeam = !beat.isRest && 
                             (beat.duration == NoteDuration::Eighth || 
                              beat.duration == NoteDuration::Sixteenth ||
                              beat.duration == NoteDuration::ThirtySecond);
            
            if (shouldBeam)
            {
                beamableNotes.push_back({b, currentTick, durationTicks});
            }
            
            currentTick += durationTicks;
        }
        
        // Gruppiere nach Beam-Gruppen-Grenzen
        std::vector<std::vector<int>> beamGroups;
        if (!beamableNotes.empty())
        {
            std::vector<int> currentGroup;
            int currentGroupNumber = beamableNotes[0].tickPosition / ticksPerBeamGroup;
            
            for (const auto& note : beamableNotes)
            {
                int noteGroupNumber = note.tickPosition / ticksPerBeamGroup;
                
                // Wenn wir eine neue Beam-Gruppe erreichen, starte neue Gruppe
                if (noteGroupNumber != currentGroupNumber && !currentGroup.empty())
                {
                    beamGroups.push_back(currentGroup);
                    currentGroup.clear();
                }
                
                currentGroup.push_back(note.beatIndex);
                currentGroupNumber = noteGroupNumber;
            }
            
            if (!currentGroup.empty())
            {
                beamGroups.push_back(currentGroup);
            }
        }
        
        // Zeichne jede Beam-Gruppe
        for (const auto& group : beamGroups)
        {
            if (group.size() >= 2)
            {
                // Zeichne verbundene Noten mit Balken
                int beamStart = group.front();
                int beamEnd = group.back();
                float startX = measureX + beatPositions[beamStart];
                float endX = measureX + beatPositions[beamEnd];
                
                // Zeichne alle Stems in der Gruppe
                for (int idx : group)
                {
                    float x = measureX + beatPositions[idx];
                    const auto& beat = measure.beats[idx];
                    
                    // Stem nach unten
                    g.drawLine(x, y, x, y + stemHeight, 1.5f);
                    
                    // Notenkopf
                    g.fillEllipse(x - noteheadWidth/2, y - 3.0f, noteheadWidth, 5.0f);
                    
                    // Dot für gepunktete Noten
                    if (beat.isDotted)
                        g.fillEllipse(x + noteheadWidth/2 + 2.0f, y - 1.0f, 3.0f, 3.0f);
                }
                
                // Zeichne Hauptbalken (für Achtel)
                float beamY = y + stemHeight;
                g.fillRect(startX, beamY - beamThickness/2, endX - startX, beamThickness);
                
                // Zeichne zweiten Balken für Sechzehntel
                for (size_t i = 0; i < group.size(); ++i)
                {
                    int idx = group[i];
                    const auto& beat = measure.beats[idx];
                    if (beat.duration >= NoteDuration::Sixteenth)
                    {
                        float x = measureX + beatPositions[idx];
                        float nextX = (i + 1 < group.size()) ? measureX + beatPositions[group[i + 1]] : x + 8.0f;
                        
                        // Kurzer Balken wenn nur diese Note Sechzehntel ist
                        bool nextIsSixteenth = (i + 1 < group.size() && 
                                                measure.beats[group[i + 1]].duration >= NoteDuration::Sixteenth);
                        
                        if (nextIsSixteenth)
                            g.fillRect(x, beamY + 3.0f, nextX - x, beamThickness);
                        else
                            g.fillRect(x - 4.0f, beamY + 3.0f, 8.0f, beamThickness);
                    }
                }
            }
            else if (group.size() == 1)
            {
                // Einzelne Note mit Fähnchen
                float x = measureX + beatPositions[group[0]];
                const auto& beat = measure.beats[group[0]];
                drawSingleRhythmNote(g, beat, x, y);
            }
        }
        
        // Zeichne nicht-beamable Noten (Viertel, Halbe, Ganze, Pausen)
        for (int b = 0; b < measure.beats.size(); ++b)
        {
            const auto& beat = measure.beats[b];
            bool isBeamable = !beat.isRest && 
                             (beat.duration == NoteDuration::Eighth || 
                              beat.duration == NoteDuration::Sixteenth ||
                              beat.duration == NoteDuration::ThirtySecond);
            
            if (!isBeamable && !beat.isRest && beat.duration < NoteDuration::Eighth)
            {
                // Viertel, Halbe, Ganze
                float x = measureX + beatPositions[b];
                drawSingleRhythmNote(g, beat, x, y);
            }
        }
    }
    
    void drawSingleRhythmNote(juce::Graphics& g, const TabBeat& beat, float x, float y)
    {
        const float stemHeight = 12.0f;
        const float noteheadWidth = 6.0f;
        
        if (beat.isRest)
            return;
        
        // Draw stem (außer für Ganze Noten)
        if (beat.duration != NoteDuration::Whole)
            g.drawLine(x, y, x, y + stemHeight, 1.5f);
        
        // Draw notehead
        bool filled = (beat.duration >= NoteDuration::Quarter);
        if (filled)
            g.fillEllipse(x - noteheadWidth/2, y - 3.0f, noteheadWidth, 5.0f);
        else
            g.drawEllipse(x - noteheadWidth/2, y - 3.0f, noteheadWidth, 5.0f, 1.0f);
        
        // Draw flags for eighth notes and shorter (nur für einzelne Noten)
        if (beat.duration >= NoteDuration::Eighth)
        {
            int flagCount = 0;
            if (beat.duration == NoteDuration::Eighth) flagCount = 1;
            else if (beat.duration == NoteDuration::Sixteenth) flagCount = 2;
            else if (beat.duration == NoteDuration::ThirtySecond) flagCount = 3;
            
            for (int f = 0; f < flagCount; ++f)
            {
                float flagY = y + stemHeight - f * 3.0f;
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
