#include <iostream>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/shared_ptr.h>

#include "compressor.h"
#include "processor_router.h"
#include "random_lfo.h"
#include "sound_engine.h"
#include "synth_base.h"
#include "synth_filter.h"
#include "synth_lfo.h"
#include "synth_oscillator.h"
#include "value.h"
#include "voice_handler.h"
#include "wave_frame.h"

namespace nb = nanobind;
using namespace vital;


class ModulationDestinationListCache {
 private:
  static std::vector<std::string> cached_list;
  static bool initialized;

  static void initialize() {
    if (!initialized) {
      SoundEngine engine;
      vital::input_map &mono_destination_map =
          engine.getMonoModulationDestinations();
      for (auto &destination_iter : mono_destination_map) {
        cached_list.push_back(destination_iter.first);
      }
      initialized = true;
    }
  }

 public:
  static const std::vector<std::string> &get() {
    initialize();
    return cached_list;
  }
};

class ModulationSourceListCache {
 private:
  static std::vector<std::string> cached_list;
  static bool initialized;

  static void initialize() {
    if (!initialized) {
      SoundEngine engine;
      vital::output_map &source_map = engine.getModulationSources();
      for (auto &source_iter : source_map) {
        cached_list.push_back(source_iter.first);
      }
      initialized = true;
    }
  }

 public:
  static const std::vector<std::string> &get() {
    initialize();
    return cached_list;
  }
};

// Initialize static members
std::vector<std::string> ModulationDestinationListCache::cached_list;
bool ModulationDestinationListCache::initialized = false;
std::vector<std::string> ModulationSourceListCache::cached_list;
bool ModulationSourceListCache::initialized = false;


nb::list get_modulation_destinations() {
  const auto &cpp_list = ModulationDestinationListCache::get();
  nb::list result;

  for (const auto &str : cpp_list) {
    result.append(str);
  }

  return result;
}


nb::list get_modulation_sources() {
  const auto &cpp_list = ModulationSourceListCache::get();
  nb::list result;

  for (const auto &str : cpp_list) {
    result.append(str);
  }

  return result;
}


NB_MODULE(vita, m) {

    m.def("get_modulation_sources", &get_modulation_sources,
		"Returns a list of allowed modulation sources.");

    m.def("get_modulation_destinations", &get_modulation_destinations,
          "Returns a list of allowed modulation destinations");
    
    auto m_constants = m.def_submodule("constants", "Submodule containing constants and enums");
    
    // Expose Enums
    nb::enum_<constants::SourceDestination>(m_constants, "SourceDestination")
        .value("Filter1", constants::SourceDestination::kFilter1)
        .value("Filter2", constants::SourceDestination::kFilter2)
        .value("DualFilters", constants::SourceDestination::kDualFilters)
        .value("Effects", constants::SourceDestination::kEffects)
        .value("DirectOut", constants::SourceDestination::kDirectOut)
        .def("__int__", [](constants::SourceDestination self) {
          return static_cast<int>(self);
        });

    nb::enum_<constants::Effect>(m_constants, "Effect")
        .value("Chorus", constants::Effect::kChorus)
        .value("Compressor", constants::Effect::kCompressor)
        .value("Delay", constants::Effect::kDelay)
        .value("Distortion", constants::Effect::kDistortion)
        .value("Eq", constants::Effect::kEq)
        .value("FilterFx", constants::Effect::kFilterFx)
        .value("Flanger", constants::Effect::kFlanger)
        .value("Phaser", constants::Effect::kPhaser)
        .value("Reverb", constants::Effect::kReverb)
        .def("__int__",
             [](constants::Effect self) { return static_cast<int>(self); });

    nb::enum_<constants::FilterModel>(m_constants, "FilterModel")
        .value("Analog", constants::FilterModel::kAnalog)
        .value("Dirty", constants::FilterModel::kDirty)
        .value("Ladder", constants::FilterModel::kLadder)
        .value("Digital", constants::FilterModel::kDigital)
        .value("Diode", constants::FilterModel::kDiode)
        .value("Formant", constants::FilterModel::kFormant)
        .value("Comb", constants::FilterModel::kComb)
        .value("Phase", constants::FilterModel::kPhase)
        .def("__int__", [](constants::FilterModel self) {
          return static_cast<int>(self);
        });

    nb::enum_<constants::RetriggerStyle>(m_constants, "RetriggerStyle")
        .value("Free", constants::RetriggerStyle::kFree)
        .value("Retrigger", constants::RetriggerStyle::kRetrigger)
        .value("SyncToPlayHead", constants::RetriggerStyle::kSyncToPlayHead)
        .def("__int__", [](constants::RetriggerStyle self) {
          return static_cast<int>(self);
        });

    nb::enum_<SynthOscillator::SpectralMorph>(m_constants, "SpectralMorph")
        .value("NoSpectralMorph", SynthOscillator::SpectralMorph::kNoSpectralMorph)
        .value("Vocode", SynthOscillator::SpectralMorph::kVocode)
        .value("FormScale", SynthOscillator::SpectralMorph::kFormScale)
        .value("HarmonicScale", SynthOscillator::SpectralMorph::kHarmonicScale)
        .value("InharmonicScale", SynthOscillator::SpectralMorph::kInharmonicScale)
        .value("Smear", SynthOscillator::SpectralMorph::kSmear)
        .value("RandomAmplitudes", SynthOscillator::SpectralMorph::kRandomAmplitudes)
        .value("LowPass", SynthOscillator::SpectralMorph::kLowPass)
        .value("HighPass", SynthOscillator::SpectralMorph::kHighPass)
        .value("PhaseDisperse", SynthOscillator::SpectralMorph::kPhaseDisperse)
        .value("ShepardTone", SynthOscillator::SpectralMorph::kShepardTone)
        .value("Skew", SynthOscillator::SpectralMorph::kSkew)
        .def("__int__", [](SynthOscillator::SpectralMorph self) {
          return static_cast<int>(self);
        });
    
    nb::enum_<SynthOscillator::DistortionType>(m_constants, "DistortionType")
            .value("None", SynthOscillator::DistortionType::kNone)
            .value("Sync", SynthOscillator::DistortionType::kSync)
            .value("Formant", SynthOscillator::DistortionType::kFormant)
            .value("Quantize", SynthOscillator::DistortionType::kQuantize)
            .value("Bend", SynthOscillator::DistortionType::kBend)
            .value("Squeeze", SynthOscillator::DistortionType::kSqueeze)
            .value("PulseWidth", SynthOscillator::DistortionType::kPulseWidth)
            .value("FmOscillatorA", SynthOscillator::DistortionType::kFmOscillatorA)
            .value("FmOscillatorB", SynthOscillator::DistortionType::kFmOscillatorB)
            .value("FmSample", SynthOscillator::DistortionType::kFmSample)
            .value("RmOscillatorA", SynthOscillator::DistortionType::kRmOscillatorA)
            .value("RmOscillatorB", SynthOscillator::DistortionType::kRmOscillatorB)
        .value("RmSample", SynthOscillator::DistortionType::kRmSample)
        .def("__int__", [](SynthOscillator::DistortionType self) {
          return static_cast<int>(self);
        });
        
    nb::enum_<SynthOscillator::UnisonStackType>(m_constants, "UnisonStackType")
        .value("Normal", SynthOscillator::UnisonStackType::kNormal)
        .value("CenterDropOctave", SynthOscillator::UnisonStackType::kCenterDropOctave)
        .value("CenterDropOctave2", SynthOscillator::UnisonStackType::kCenterDropOctave2)
        .value("Octave", SynthOscillator::UnisonStackType::kOctave)
        .value("Octave2", SynthOscillator::UnisonStackType::kOctave2)
        .value("PowerChord", SynthOscillator::UnisonStackType::kPowerChord)
        .value("PowerChord2", SynthOscillator::UnisonStackType::kPowerChord2)
        .value("MajorChord", SynthOscillator::UnisonStackType::kMajorChord)
        .value("MinorChord", SynthOscillator::UnisonStackType::kMinorChord)
        .value("HarmonicSeries", SynthOscillator::UnisonStackType::kHarmonicSeries)
        .value("OddHarmonicSeries",
               SynthOscillator::UnisonStackType::kOddHarmonicSeries)
        .def("__int__", [](SynthOscillator::UnisonStackType self) {
          return static_cast<int>(self);
        });
    
    nb::enum_<RandomLfo::RandomType>(m_constants, "RandomLFOStyle")
        .value("Perlin", RandomLfo::RandomType::kPerlin)
        .value("SampleAndHold", RandomLfo::RandomType::kSampleAndHold)
        .value("SinInterpolate", RandomLfo::RandomType::kSinInterpolate)
        .value("LorenzAttractor", RandomLfo::RandomType::kLorenzAttractor)
        .def("__int__",
             [](RandomLfo::RandomType self) { return static_cast<int>(self); });

    nb::enum_<VoiceHandler::VoicePriority>(m_constants, "VoicePriority")
        .value("Newest", VoiceHandler::VoicePriority::kNewest)
        .value("Oldest", VoiceHandler::VoicePriority::kOldest)
        .value("Highest", VoiceHandler::VoicePriority::kHighest)
        .value("Lowest", VoiceHandler::VoicePriority::kLowest)
        .value("RoundRobin", VoiceHandler::VoicePriority::kRoundRobin)
        .def("__int__", [](VoiceHandler::VoicePriority self) {
          return static_cast<int>(self);
        });
    
    nb::enum_<VoiceHandler::VoiceOverride>(m_constants, "VoiceOverride")
        .value("Kill",VoiceHandler::VoiceOverride::kKill)
        .value("Steal", VoiceHandler::VoiceOverride::kSteal)
        .def("__int__", [](VoiceHandler::VoiceOverride self) {
          return static_cast<int>(self);
        });

    nb::enum_<PredefinedWaveFrames::Shape>(m_constants, "WaveShape")
        .value("Sin", PredefinedWaveFrames::kSin)
        .value("SaturatedSin", PredefinedWaveFrames::kSaturatedSin)
        .value("Triangle", PredefinedWaveFrames::kTriangle)
        .value("Square", PredefinedWaveFrames::kSquare)
        .value("Pulse", PredefinedWaveFrames::kPulse)
        .value("Saw", PredefinedWaveFrames::kSaw)
        .def("__int__", [](PredefinedWaveFrames::Shape self) {
          return static_cast<int>(self);
        });

    nb::enum_<SynthLfo::SyncType>(m_constants, "SynthLFOSyncType")
        .value("Trigger", SynthLfo::SyncType::kTrigger)
        .value("Sync", SynthLfo::SyncType::kSync)
        .value("Envelope", SynthLfo::SyncType::kEnvelope)
        .value("SustainEnvelope", SynthLfo::SyncType::kSustainEnvelope)
        .value("LoopPoint", SynthLfo::SyncType::kLoopPoint)
        .value("LoopHold", SynthLfo::SyncType::kLoopHold)
        .def("__int__",
             [](SynthLfo::SyncType self) { return static_cast<int>(self); });
    
    nb::enum_<MultibandCompressor::BandOptions>(m_constants, "CompressorBandOption")
        .value("Multiband", MultibandCompressor::BandOptions::kMultiband)
        .value("LowBand", MultibandCompressor::BandOptions::kLowBand)
        .value("HighBand", MultibandCompressor::BandOptions::kHighBand)
        .value("SingleBand", MultibandCompressor::BandOptions::kSingleBand)
        .def("__int__", [](MultibandCompressor::BandOptions self) {
          return static_cast<int>(self);
        });
    
    nb::enum_<SynthFilter::Style>(m_constants, "SynthFilterStyle")
        .value("k12Db", SynthFilter::Style::k12Db)
        .value("k24Db", SynthFilter::Style::k24Db)
        .value("NotchPassSwap", SynthFilter::Style::kNotchPassSwap)
        .value("DualNotchBand", SynthFilter::Style::kDualNotchBand)
        .value("BandPeakNotch", SynthFilter::Style::kBandPeakNotch)
        .value("Shelving", SynthFilter::Style::kShelving)
        .def("__int__",
             [](SynthFilter::Style self) { return static_cast<int>(self); });
    
//    https://github.com/mtytel/vital/blob/636ca0ef517a4db087a6a08a6a8a5e704e21f836/src/interface/look_and_feel/synth_strings.h#L174
    enum SyncedFrequencyName {
        k32_1,
        k16_1,
        k8_1,
        k4_1,
        k2_1,
        k1_1,
        k1_2,
        k1_4,
        k1_8,
        k1_16,
        k1_32,
        k1_64
    };
    
    nb::enum_<SyncedFrequencyName>(m_constants, "SyncedFrequency")
        .value("k32_1", SyncedFrequencyName::k32_1)
        .value("k16_1", SyncedFrequencyName::k16_1)
        .value("k8_1", SyncedFrequencyName::k8_1)
        .value("k4_1", SyncedFrequencyName::k4_1)
        .value("k2_1", SyncedFrequencyName::k2_1)
        .value("k1_1", SyncedFrequencyName::k1_1)
        .value("k1_2", SyncedFrequencyName::k1_2)
        .value("k1_4", SyncedFrequencyName::k1_4)
        .value("k1_8", SyncedFrequencyName::k1_8)
        .value("k1_16", SyncedFrequencyName::k1_16)
        .value("k1_32", SyncedFrequencyName::k1_32)
        .value("k1_64", SyncedFrequencyName::k1_64)
        .def("__int__",
             [](SyncedFrequencyName self) { return static_cast<int>(self); });
        
//    https://github.com/mtytel/vital/blob/636ca0ef517a4db087a6a08a6a8a5e704e21f836/src/synthesis/modulators/synth_lfo.h#L58C1-L65C9
    enum SyncOption {
        kTime,
        kTempo,
        kDottedTempo,
        kTripletTempo,
        kKeytrack,
    };
    
    nb::enum_<SyncOption>(m_constants, "SynthLFOSyncOption")
        .value("Time", SyncOption::kTime)
        .value("Tempo", SyncOption::kTempo)
        .value("DottedTempo", SyncOption::kDottedTempo)
        .value("TripletTempo", SyncOption::kTripletTempo)
        .value("Keytrack", SyncOption::kKeytrack)
        .def("__int__", [](SyncOption self) { return static_cast<int>(self); });

    // Binding for poly_float
    nb::class_<vital::poly_float>(m, "poly_float")
        .def(nb::init_implicit<float>());
    
    // Expose the ProcessorRouter class
    nb::class_<ProcessorRouter>(m, "ProcessorRouter")
        .def(nb::init<int, int, bool>(),
             nb::arg("num_inputs") = 0,
             nb::arg("num_outputs") = 0,
             nb::arg("control_rate") = false);
    
    nb::class_<vital::Value>(m, "Value")
        .def(nb::init<poly_float, bool>(),
             nb::arg("value") = 0.0f,
             nb::arg("control_rate") = false)
        .def("process", &vital::Value::process, nb::arg("num_samples"))
        .def("set_oversample_amount", &vital::Value::setOversampleAmount, nb::arg("oversample"))
        .def("value", &vital::Value::value)
        // Handle floating point values (both float and double)
        .def("set", [](vital::Value &self, double value) {
            self.set(poly_float(static_cast<float>(value)));
        }, nb::arg("value"))
        // Handle enum values
        .def("set", [](vital::Value &self, SyncedFrequencyName value) {
            self.set(poly_float(static_cast<float>(static_cast<int>(value))));
        }, nb::arg("value"))
        // Handle integer values
        .def("set", [](vital::Value &self, int value) {
            self.set(poly_float(static_cast<float>(value)));
        }, nb::arg("value"))
        .def("__repr__", [](const vital::Value &v) {
            return "<Value value=" + std::to_string(v.value()) + ">";
        });

    nb::class_<vital::cr::Value, vital::Value>(m, "CRValue")
        .def(nb::init<poly_float>(),
             nb::arg("value") = 0.0f)
        .def("process", &vital::cr::Value::process, nb::arg("num_samples"))
        // Add the same method overloads for CRValue
        .def("set", [](vital::cr::Value &self, double value) {
            self.set(poly_float(static_cast<float>(value)));
        }, nb::arg("value"))
        .def("set", [](vital::cr::Value &self, SyncedFrequencyName value) {
            self.set(poly_float(static_cast<float>(static_cast<int>(value))));
        }, nb::arg("value"))
        .def("set", [](vital::cr::Value &self, int value) {
            self.set(poly_float(static_cast<float>(value)));
        }, nb::arg("value"))
        .def("__repr__", [](const vital::cr::Value &v) {
            return "<CRValue value=" + std::to_string(v.value()) + ">";
        });
    
    // Expose the SynthBase class, specifying ProcessorRouter as its base
    nb::class_<HeadlessSynth>(m, "Synth")
        .def(nb::init<>())  // Ensure there's a default constructor or adjust
                            // accordingly
                            // Bind the first overload of connectModulation
        .def("connect_modulation",
             nb::overload_cast<const std::string &, const std::string &>(
                 &HeadlessSynth::pyConnectModulation),
             nb::arg("source"), nb::arg("destination"),
             "Connects a modulation source to a destination by name.")

        .def("disconnect_modulation",
             nb::overload_cast<const std::string &, const std::string &>(
                 &HeadlessSynth::disconnectModulation),
             nb::arg("source"), nb::arg("destination"),
             "Disconnects a modulation source from a destination by name.")

        .def("set_bpm", &HeadlessSynth::pySetBPM, nb::arg("bpm"))

        .def("render_file", &HeadlessSynth::renderAudioToFile2,
             nb::arg("output_path"), nb::arg("midi_note"),
             nb::arg("midi_velocity"), nb::arg("note_dur"),
             nb::arg("render_dur"),
             "Renders audio to a file.\n\n"
             "Parameters:\n"
             "  output_path (str): Path to the output audio file.\n"
             "  midi_note (int): MIDI note to render.\n"
             "  midi_velocity (float): Velocity of the note [0-1].\n"
             "  note_dur (float): Length of the note sustain in seconds.\n"
             "  render_dur (float): Length of the audio render in seconds.\n"
             "\n"
             "Returns:\n"
             "  bool: True if rendering was successful, False otherwise.")

        .def("render", &HeadlessSynth::renderAudioToNumpy, nb::arg("midi_note"),
             nb::arg("midi_velocity"), nb::arg("note_dur"),
             nb::arg("render_dur"),
             "Renders audio to a file.\n\n"
             "Parameters:\n"
             "  midi_note (int): MIDI note to render.\n"
             "  midi_velocity (float): Velocity of the note [0-1].\n"
             "  note_dur (float): Length of the note sustain in seconds.\n"
             "  render_dur (float): Length of the audio render in seconds.\n"
             "\n"
             "Returns:\n"
             "  bool: True if rendering was successful, False otherwise.")

        .def("load_json", &HeadlessSynth::loadFromString, nb::arg("json"))

        .def("to_json", &HeadlessSynth::pyToJson)
    
        .def("load_preset", &HeadlessSynth::pyLoadFromFile, nb::arg("filepath"))
    
        .def("load_init_preset", &HeadlessSynth::loadInitPreset, "Load the initial preset.")
    
        .def("clear_modulations", &HeadlessSynth::clearModulations)
        .def("get_controls", &HeadlessSynth::getControls, nb::rv_policy::reference);
}
