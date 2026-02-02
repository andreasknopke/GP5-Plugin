/*
  ==============================================================================

    TabModels.h
    
    Datenmodell für die Tabulatur-Darstellung
    Basierend auf Konzepten von TuxGuitar und MuseScore

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

//==============================================================================
// Notenwert / Dauer
//==============================================================================
enum class NoteDuration
{
    Whole = 1,          // Ganze Note
    Half = 2,           // Halbe Note
    Quarter = 4,        // Viertelnote
    Eighth = 8,         // Achtelnote
    Sixteenth = 16,     // Sechzehntelnote
    ThirtySecond = 32   // Zweiunddreißigstelnote
};

//==============================================================================
// Effekte und Artikulationen
//==============================================================================
struct TabBendPoint
{
    int position = 0;    // Position in note duration (0-60, 60 = full duration = 100%)
    int value = 0;       // Bend value in 1/100 semitones (100 = 1/2 tone, 200 = full tone)
    int vibrato = 0;     // Vibrato type: 0=none, 1=fast, 2=average, 3=slow
};

enum class SlideType
{
    None,
    SlideIntoFromBelow,   // Slide von unten
    SlideIntoFromAbove,   // Slide von oben
    SlideOutDownwards,    // Slide nach unten
    SlideOutUpwards,      // Slide nach oben
    ShiftSlide,           // Legato Slide zum nächsten Ton
    LegatoSlide           // Hammer-On Slide
};

enum class HarmonicType
{
    None,
    Natural,      // Natürliches Harmonic
    Artificial,   // Künstliches Harmonic
    Tapped,       // Getapptes Harmonic
    Pinch,        // Pinch Harmonic
    Semi          // Semi-Harmonic
};

struct NoteEffects
{
    // Vibrato
    bool vibrato = false;
    bool wideVibrato = false;
    
    // Slides
    SlideType slideType = SlideType::None;
    
    // Bending
    bool bend = false;
    float bendValue = 0.0f;  // In Halbtönen (0.5 = 1/2, 1.0 = full, 2.0 = 2 Stufen)
    int bendType = 0;        // 0=none, 1=bend, 2=bend+release, 3=release, 4=pre-bend, 5=pre-bend+release
    bool releaseBend = false;
    std::vector<TabBendPoint> bendPoints;
    
    // Harmonics
    HarmonicType harmonic = HarmonicType::None;
    
    // Hammer-On / Pull-Off
    bool hammerOn = false;
    bool pullOff = false;
    
    // Sonstige
    bool letRing = false;
    bool staccato = false;
    bool ghostNote = false;      // Klammern um die Note
    bool accentuatedNote = false;
    bool heavyAccentuatedNote = false;
    bool deadNote = false;       // X statt Zahl
    bool tapping = false;        // T über der Note
};

//==============================================================================
// Einzelne Note auf einer Saite
//==============================================================================
struct TabNote
{
    int fret = 0;           // Bundnummer (0 = Leersaite)
    int string = 0;         // Saitennummer (0 = höchste Saite, 5 = tiefste bei 6-Saiter)
    NoteEffects effects;    // Effekte und Artikulationen
    int velocity = 100;     // Anschlagstärke (0-127)
    
    // Für Ties (gehaltene Noten)
    bool isTied = false;
    
    // Berechnet die Breite des Textes für Layout
    int getDisplayWidth() const
    {
        if (effects.deadNote) return 1;  // "X"
        if (fret >= 10) return 2;        // Zweistellig
        return 1;
    }
};

//==============================================================================
// Ein Beat (Zeitpunkt mit mehreren gleichzeitigen Noten)
//==============================================================================
struct TabBeat
{
    juce::Array<TabNote> notes;     // Alle Noten dieses Beats
    NoteDuration duration = NoteDuration::Quarter;
    
    // Rhythmische Modifikationen
    bool isDotted = false;          // Punktiert
    bool isDoubleDotted = false;    // Doppelt punktiert
    int tupletNumerator = 1;        // Für Triolen etc. (3 bei Triole)
    int tupletDenominator = 1;      // (2 bei Triole -> 3:2)
    
    // Beat-Effekte
    bool isPalmMuted = false;       // P.M.-----
    bool isLetRing = false;         // Let Ring-----
    bool hasDownstroke = false;     // Abschlag-Symbol
    bool hasUpstroke = false;       // Aufschlag-Symbol
    
    // Text annotation (e.g., "Don't pick", "let ring", etc.)
    juce::String text;
    
    // Chord name (e.g., "Am7", "C", "D/F#")
    juce::String chordName;
    
    // Rest (Pause)
    bool isRest = false;
    
    // Berechnet die "Gewichtung" für das Layout
    // Kürzere Noten brauchen mehr Platz pro Zeiteinheit
    float getLayoutWeight() const
    {
        float weight = static_cast<float>(duration);
        if (isDotted) weight *= 0.666f;
        return weight;
    }
    
    // Gibt die Dauer in Vierteln zurück
    float getDurationInQuarters() const
    {
        float base = 4.0f / static_cast<float>(duration);
        if (isDotted) base *= 1.5f;
        if (isDoubleDotted) base *= 1.75f;
        base *= static_cast<float>(tupletDenominator) / static_cast<float>(tupletNumerator);
        return base;
    }
};

//==============================================================================
// Ein Takt (Measure)
//==============================================================================
struct TabMeasure
{
    juce::Array<TabBeat> beats;
    int measureNumber = 1;
    
    // Taktart
    int timeSignatureNumerator = 4;
    int timeSignatureDenominator = 4;
    
    // Wiederholungen
    bool isRepeatOpen = false;      // |:
    bool isRepeatClose = false;     // :|
    int repeatCount = 0;            // Anzahl Wiederholungen
    
    // Alternative Endungen (1., 2., etc.)
    int alternateEnding = 0;
    
    // Marker (z.B. "Verse", "Chorus")
    juce::String marker;
    
    // Layout-Informationen (werden beim Rendern berechnet)
    float calculatedWidth = 0.0f;
    float xPosition = 0.0f;
    
    // Berechnet die Mindestbreite basierend auf Inhalt
    float calculateMinWidth(float baseNoteWidth) const
    {
        float totalWeight = 0.0f;
        for (const auto& beat : beats)
        {
            totalWeight += beat.getLayoutWeight();
        }
        // Mindestens so breit wie die Anzahl der Beats
        return juce::jmax(static_cast<float>(beats.size()) * baseNoteWidth, 
                          totalWeight * baseNoteWidth * 0.5f);
    }
};

//==============================================================================
// Eine Spur (Track) - eine Gitarre/Bass
//==============================================================================
struct TabTrack
{
    juce::String name = "Track 1";
    int stringCount = 6;            // Anzahl Saiten
    juce::Array<int> tuning;        // Stimmung (MIDI-Noten, z.B. E2=40, A2=45...)
    int capo = 0;                   // Kapodaster-Position
    int midiChannel = 0;            // MIDI Kanal (0-15)
    int midiInstrument = 25;        // GM Instrument (0-127), default: Acoustic Guitar Steel
    
    juce::Array<TabMeasure> measures;
    
    // Farbe für die Darstellung
    juce::Colour colour = juce::Colours::orange;
    
    TabTrack()
    {
        // Standard-Stimmung: E-Standard (E4, B3, G3, D3, A2, E2) - High to Low
        tuning = { 64, 59, 55, 50, 45, 40 };
    }
};

//==============================================================================
// Der komplette Song
//==============================================================================
struct TabSong
{
    juce::String title;
    juce::String artist;
    juce::String album;
    
    int tempo = 120;                // BPM
    juce::Array<TabTrack> tracks;
    
    // Aktueller Wiedergabeposition
    int currentMeasure = 0;
    int currentBeat = 0;
};

//==============================================================================
// Layout-Konfiguration
//==============================================================================
struct TabLayoutConfig
{
    // Abstände
    float stringSpacing = 16.0f;        // Abstand zwischen Saiten
    float measurePadding = 15.0f;       // Padding am Taktanfang/-ende (erhöht)
    float minBeatSpacing = 35.0f;       // Minimaler Abstand zwischen Beats (erhöht)
    float baseNoteWidth = 32.0f;        // Basis-Breite für Notenwerte (erhöht)
    float topMargin = 50.0f;            // Platz oben für Bends, Vibrato, etc. (erhöht)
    float bottomMargin = 45.0f;         // Platz unten für Rhythmik mit Beaming
    
    // Schrift
    float fretFontSize = 11.0f;         // Schriftgröße für Bundzahlen
    float measureNumberFontSize = 9.0f; // Schriftgröße für Taktnummern
    
    // Farben
    juce::Colour stringColour = juce::Colour(0xFF555555);
    juce::Colour fretTextColour = juce::Colours::black;
    juce::Colour measureLineColour = juce::Colour(0xFF333333);
    juce::Colour backgroundColor = juce::Colours::white;
    juce::Colour playheadColour = juce::Colour(0xFF4A90D9);
    
    // Effekt-Farben
    juce::Colour slideColour = juce::Colour(0xFF666666);
    juce::Colour vibratoColour = juce::Colour(0xFF666666);
    juce::Colour palmMuteColour = juce::Colour(0xFF888888);
    
    // Berechnet die Gesamthöhe für n Saiten
    float getTotalHeight(int stringCount) const
    {
        return topMargin + (stringCount - 1) * stringSpacing + bottomMargin;
    }
};
