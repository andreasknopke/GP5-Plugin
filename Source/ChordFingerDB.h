/*
  ==============================================================================

    ChordFingerDB.h
    
    Datenbank für Akkord-Fingersätze. Lädt Fingerpositionen aus einer CSV-Datei
    und ordnet erkannten Akkorden die optimalen Fingersätze zu.
    
    Basierend auf den Erkenntnissen aus:
    "Putting a Finger on Guitars and Algorithms" (Ilczuk & Sköld, KTH 2013)
    
    Die Datenbank enthält für jeden Akkordtyp mehrere Griffvarianten mit den
    zugehörigen Fingernummern (1=Zeigefinger, 2=Mittelfinger, 3=Ringfinger,
    4=kleiner Finger, 0=Leersaite, x=nicht gespielt).
    
    Für Einzelnoten wird ein algorithmischer Fingersatz berechnet, der die
    Complexity Factors aus dem Paper berücksichtigt.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <map>
#include <array>
#include <string>
#include <set>
#include <algorithm>

//==============================================================================
/**
 * ChordFingerEntry
 * 
 * Ein einzelner Eintrag aus der Datenbank: ein Akkord-Griffbild mit Fingern.
 */
struct ChordFingerEntry
{
    juce::String root;           // z.B. "C", "A#", "Gb"
    juce::String type;           // z.B. "maj", "m", "7", "dim7"
    juce::String structure;      // z.B. "1;3;5"
    
    // Finger pro Saite: -1 = nicht gespielt (x), 0 = Leersaite, 1-4 = Finger
    std::array<int, 6> fingers = { -1, -1, -1, -1, -1, -1 };
    
    // Notennamen pro Saite
    std::vector<juce::String> noteNames;
};

//==============================================================================
/**
 * ChordFingerDB
 * 
 * Verwaltet die Akkord-Finger-Datenbank und bietet Matching-Funktionen.
 */
class ChordFingerDB
{
public:
    ChordFingerDB() = default;
    
    //==========================================================================
    // Laden der Datenbank
    //==========================================================================
    
    /**
     * Lädt die Datenbank aus einer CSV-Datei.
     * Format: CHORD_ROOT;CHORD_TYPE;CHORD_STRUCTURE;FINGER_POSITIONS;NOTE_NAMES
     * 
     * @param csvFile Pfad zur CSV-Datei
     * @return true wenn erfolgreich geladen
     */
    bool loadFromFile(const juce::File& csvFile)
    {
        entries.clear();
        
        if (!csvFile.existsAsFile())
            return false;
        
        juce::StringArray lines;
        csvFile.readLines(lines);
        
        if (lines.size() < 2)
            return false;
        
        // Überspringe Header
        for (int i = 1; i < lines.size(); ++i)
        {
            auto line = lines[i].trim();
            if (line.isEmpty())
                continue;
            
            auto entry = parseLine(line);
            if (entry.root.isNotEmpty())
            {
                entries.push_back(entry);
                
                // Index aufbauen
                juce::String key = (entry.root + ";" + entry.type).toLowerCase();
                entryIndex[key.toStdString()].push_back(entries.size() - 1);
            }
        }
        
        DBG("ChordFingerDB: " << entries.size() << " entries loaded from " << csvFile.getFileName());
        loaded = true;
        return true;
    }
    
    bool isLoaded() const { return loaded; }
    int getEntryCount() const { return (int)entries.size(); }
    
    /**
     * Lädt die Datenbank aus BinaryData (eingebettet im Plugin).
     * 
     * @param data Zeiger auf die CSV-Daten
     * @param dataSize Größe der Daten in Bytes
     * @return true wenn erfolgreich geladen
     */
    bool loadFromBinaryData(const char* data, int dataSize)
    {
        entries.clear();
        entryIndex.clear();
        
        if (data == nullptr || dataSize <= 0)
            return false;
        
        juce::String csvContent(data, (size_t)dataSize);
        juce::StringArray lines;
        lines.addLines(csvContent);
        
        if (lines.size() < 2)
            return false;
        
        // Überspringe Header
        for (int i = 1; i < lines.size(); ++i)
        {
            auto line = lines[i].trim();
            if (line.isEmpty())
                continue;
            
            auto entry = parseLine(line);
            if (entry.root.isNotEmpty())
            {
                entries.push_back(entry);
                
                // Index aufbauen
                juce::String key = (entry.root + ";" + entry.type).toLowerCase();
                entryIndex[key.toStdString()].push_back(entries.size() - 1);
            }
        }
        
        DBG("ChordFingerDB: " << entries.size() << " entries loaded from BinaryData");
        loaded = true;
        return true;
    }
    
    //==========================================================================
    // Akkord-Fingersatz finden
    //==========================================================================
    
    /**
     * Findet den besten Fingersatz für einen erkannten Akkord.
     * 
     * @param chordName Der erkannte Akkordname (z.B. "Cmaj", "Am7")
     * @param frets Die aktuellen Bundpositionen pro Saite (-1 = nicht gespielt)
     * @param tuning Das Tuning des Instruments
     * @return Array von Fingernummern pro Saite (-1=nicht, 0=leer, 1-4=Finger)
     */
    std::array<int, 6> findFingers(const juce::String& chordName,
                                    const std::array<int, 6>& frets,
                                    const std::array<int, 6>& tuning) const
    {
        std::array<int, 6> result = { -1, -1, -1, -1, -1, -1 };
        
        if (!loaded)
            return result;
        
        // Parse den Akkordnamen in Root + Type
        auto [root, type] = parseChordName(chordName);
        if (root.isEmpty())
            return result;
        
        // Suche im Index
        juce::String key = (root + ";" + type).toLowerCase();
        auto it = entryIndex.find(key.toStdString());
        if (it == entryIndex.end())
        {
            // Versuche alternative Schreibweisen
            key = (root + ";maj").toLowerCase();
            it = entryIndex.find(key.toStdString());
            if (it == entryIndex.end())
                return result;
        }
        
        // Finde den Eintrag der am besten zu den aktuellen Fret-Positionen passt
        int bestScore = -1000;
        const ChordFingerEntry* bestEntry = nullptr;
        
        for (size_t idx : it->second)
        {
            const auto& entry = entries[idx];
            int score = matchScore(entry, frets, tuning);
            if (score > bestScore)
            {
                bestScore = score;
                bestEntry = &entry;
            }
        }
        
        if (bestEntry != nullptr && bestScore > -100)
        {
            result = bestEntry->fingers;
        }
        
        return result;
    }
    
    //==========================================================================
    // Algorithmischer Fingersatz für Einzelnoten
    //==========================================================================
    
    /**
     * Berechnet den optimalen Finger für eine Einzelnote.
     * Basiert auf den Complexity Factors aus dem KTH Paper:
     * - Distance Rule (6.3.2): Natürliche Fingerspanne
     * - String Change Rule (6.3.3): Saitenwechsel-Ergonomie
     * - Little Finger Rule (6.3.4): Kleiner Finger schwächer
     * 
     * @param fret Bundnummer (0 = Leersaite)
     * @param string Saitennummer (0-5)
     * @param previousFret Vorheriger Bund (-1 = keine vorherige Note)
     * @param previousFinger Vorheriger Finger (-1 = keiner)
     * @param previousString Vorherige Saite (-1 = keine)
     * @return Fingernummer (0 für Leersaite, 1-4 für Finger)
     */
    static int calculateFingerForNote(int fret, int string,
                                       int previousFret = -1, int previousFinger = -1,
                                       int previousString = -1)
    {
        // Leersaite: kein Finger
        if (fret == 0) return 0;
        
        // Ohne vorherige Note: basierend auf Bundposition
        if (previousFret < 0 || previousFinger < 0)
        {
            return defaultFingerForFret(fret);
        }
        
        // Distance Rule (Paper 6.3.2):
        // Die natürliche Fingerspanne bestimmt den optimalen Finger.
        // Finger 1 (Zeigefinger) greift den niedrigsten Bund,
        // Finger 4 (kleiner Finger) den höchsten.
        int fretDelta = fret - previousFret;
        int idealFinger = previousFinger + fretDelta;
        
        // Clamp auf gültige Finger
        idealFinger = juce::jlimit(1, 4, idealFinger);
        
        // String Change Rule (Paper 6.3.3):
        // Bei Saitenwechsel zum gleichen Bund: Barré (gleicher Finger)
        if (previousString >= 0 && string != previousString && fret == previousFret)
        {
            return previousFinger;  // Barré
        }
        
        // Little Finger Rule (Paper 6.3.4):
        // Kleiner Finger ist schwächer → leichte Tendenz zu Finger 3 statt 4
        if (idealFinger == 4 && fretDelta <= 2 && previousFinger <= 2)
        {
            idealFinger = 3;
        }
        
        return idealFinger;
    }
    
    /**
     * Berechnet Finger für eine Gruppe gleichzeitiger Noten (Akkord)
     * ohne DB-Matching, rein algorithmisch.
     * 
     * @param frets Array von Fret-Positionen (6 Saiten, -1 = nicht gespielt)
     * @return Array von Fingernummern pro Saite
     */
    static std::array<int, 6> calculateFingersForChord(const std::array<int, 6>& frets)
    {
        std::array<int, 6> fingers = { -1, -1, -1, -1, -1, -1 };
        
        // Sammle gespielte Noten (sortiert nach Bund)
        struct PlayedNote { int string; int fret; };
        std::vector<PlayedNote> played;
        for (int s = 0; s < 6; ++s)
        {
            if (frets[s] >= 0)
            {
                if (frets[s] == 0)
                {
                    fingers[s] = 0;  // Leersaite
                }
                else
                {
                    played.push_back({s, frets[s]});
                }
            }
        }
        
        if (played.empty())
            return fingers;
        
        // Sortiere nach Bund (niedrigster zuerst)
        std::sort(played.begin(), played.end(), [](const PlayedNote& a, const PlayedNote& b) {
            if (a.fret != b.fret) return a.fret < b.fret;
            return a.string > b.string;  // Tiefere Saite zuerst bei gleichem Bund
        });
        
        // Prüfe auf Barré (mehrere Noten auf dem gleichen niedrigsten Bund)
        int lowestFret = played[0].fret;
        int barreCount = 0;
        for (const auto& p : played)
        {
            if (p.fret == lowestFret) barreCount++;
        }
        
        int nextFinger = 1;
        
        if (barreCount >= 2)
        {
            // Barré mit Finger 1
            for (auto& p : played)
            {
                if (p.fret == lowestFret)
                    fingers[p.string] = 1;
            }
            nextFinger = 2;
            
            // Restliche Noten zuweisen
            int lastFret = lowestFret;
            for (auto& p : played)
            {
                if (p.fret == lowestFret) continue;
                
                if (p.fret > lastFret && nextFinger <= 4)
                {
                    fingers[p.string] = nextFinger++;
                    lastFret = p.fret;
                }
                else if (p.fret == lastFret && nextFinger > 1)
                {
                    // Gleicher Bund wie vorheriger gegriffener: gleicher Finger
                    fingers[p.string] = nextFinger - 1;
                }
                else if (nextFinger <= 4)
                {
                    fingers[p.string] = nextFinger++;
                }
            }
        }
        else
        {
            // Kein Barré: verteile Finger nach Position
            int lastFret = -1;
            for (auto& p : played)
            {
                if (p.fret == lastFret && nextFinger > 1)
                {
                    fingers[p.string] = nextFinger - 1;
                }
                else if (nextFinger <= 4)
                {
                    fingers[p.string] = nextFinger++;
                    lastFret = p.fret;
                }
            }
        }
        
        return fingers;
    }

private:
    std::vector<ChordFingerEntry> entries;
    std::map<std::string, std::vector<size_t>> entryIndex;  // "root;type" -> indices
    bool loaded = false;
    
    /**
     * Default-Finger basierend auf Bundposition.
     * Position 1-4: Finger 1 greift Bund 1, Finger 2 Bund 2, etc.
     * Position 5-8: Finger 1 greift Bund 5, etc.
     * Nutzt die "Ein-Finger-pro-Bund" Regel.
     */
    static int defaultFingerForFret(int fret)
    {
        if (fret <= 0) return 0;
        // Bestimme die "Hand-Position" (welcher 4-Bund-Bereich)
        int positionBase = ((fret - 1) / 4) * 4 + 1;
        int fingerInPosition = fret - positionBase + 1;
        return juce::jlimit(1, 4, fingerInPosition);
    }
    
    /**
     * Parsed eine CSV-Zeile.
     * Format: ROOT;TYPE;"STRUCTURE";FINGERS;NOTES
     */
    ChordFingerEntry parseLine(const juce::String& line) const
    {
        ChordFingerEntry entry;
        
        // Finde die erste und zweite Anführungszeichen für STRUCTURE
        int q1 = line.indexOf("\"");
        int q2 = (q1 >= 0) ? line.indexOf(q1 + 1, "\"") : -1;
        
        if (q1 < 0 || q2 < 0)
            return entry;  // Ungültige Zeile
        
        // Vor dem ersten Quote: ROOT;TYPE;
        juce::String prefix = line.substring(0, q1);
        juce::StringArray prefixParts;
        prefixParts.addTokens(prefix, ";", "");
        
        if (prefixParts.size() < 2)
            return entry;
        
        entry.root = prefixParts[0].trim();
        entry.type = prefixParts[1].trim();
        entry.structure = line.substring(q1 + 1, q2).trim();
        
        // Nach dem zweiten Quote: ;FINGERS;NOTES
        juce::String suffix = line.substring(q2 + 2);  // Skip quote and ;
        int semicolonPos = suffix.indexOf(";");
        
        juce::String fingerStr = (semicolonPos >= 0) ? suffix.substring(0, semicolonPos) : suffix;
        juce::String noteStr = (semicolonPos >= 0) ? suffix.substring(semicolonPos + 1) : "";
        
        // Parse Finger-Positionen (kommagetrennt)
        juce::StringArray fingerParts;
        fingerParts.addTokens(fingerStr, ",", "");
        
        for (int i = 0; i < juce::jmin(6, fingerParts.size()); ++i)
        {
            auto f = fingerParts[i].trim();
            if (f == "x" || f == "X")
                entry.fingers[i] = -1;
            else
                entry.fingers[i] = f.getIntValue();
        }
        
        // Parse Notennamen
        juce::StringArray noteParts;
        noteParts.addTokens(noteStr, ",", "");
        for (const auto& n : noteParts)
            entry.noteNames.push_back(n.trim());
        
        return entry;
    }
    
    /**
     * Berechnet wie gut ein DB-Eintrag zu den aktuellen Fret-Positionen passt.
     * Höherer Score = bessere Übereinstimmung.
     */
    int matchScore(const ChordFingerEntry& entry,
                   const std::array<int, 6>& frets,
                   const std::array<int, 6>& /*tuning*/) const
    {
        int score = 0;
        
        // Vergleiche ob die gleichen Saiten gespielt werden
        for (int s = 0; s < 6; ++s)
        {
            bool entryPlayed = (entry.fingers[s] >= 0);
            bool currentPlayed = (frets[s] >= 0);
            
            if (entryPlayed == currentPlayed)
            {
                score += 10;  // Gleicher Status (gespielt/nicht gespielt)
                
                // Bonus wenn auch der Bund übereinstimmt (ungefähr)
                if (entryPlayed && currentPlayed)
                {
                    // Wir haben keine Fret-Info im Entry, nur Finger
                    // Aber Finger 0 = Leersaite, Finger > 0 = gegriffener Bund
                    if (entry.fingers[s] == 0 && frets[s] == 0)
                        score += 20;  // Beide Leersaite
                    else if (entry.fingers[s] > 0 && frets[s] > 0)
                        score += 5;   // Beide gegriffen
                    else
                        score -= 10;  // Mismatch (Leersaite vs gegriffen)
                }
            }
            else
            {
                score -= 15;  // Unterschiedlicher Status
            }
        }
        
        return score;
    }
    
    /**
     * Parsed einen Akkordnamen in Root und Type.
     * z.B. "Cmaj7" -> ("C", "maj7"), "Am" -> ("A", "m"), "F#m7" -> ("F#", "m7")
     */
    std::pair<juce::String, juce::String> parseChordName(const juce::String& name) const
    {
        if (name.isEmpty())
            return {"", ""};
        
        juce::String root;
        juce::String type;
        
        // Root ist 1-2 Zeichen: Buchstabe + optional # oder b
        int pos = 1;
        if (name.length() > 1)
        {
            juce::juce_wchar second = name[1];
            if (second == '#' || second == 'b')
                pos = 2;
        }
        
        root = name.substring(0, pos);
        type = name.substring(pos);
        
        // Normalisiere Type
        if (type.isEmpty() || type.equalsIgnoreCase("major"))
            type = "maj";
        else if (type.equalsIgnoreCase("minor"))
            type = "m";
        
        return {root, type};
    }
};
