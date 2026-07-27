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
#include <stdexcept>
#include "synth_parameters.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace nb = nanobind;
using namespace vital;

namespace {

// How many names a control can actually display: the narrower of its value
// range and its name table. The two disagree for a handful of Vital's controls
// (view_2d spans 0..2 with two names; filter_*_style spans 0..9 with five), so
// sizing the table from the range alone would read past the end of the array.
// Parameters::getStringLookupSize owns that knowledge because the string tables
// have internal linkage.
size_t namedOptionCount(const vital::ValueDetails& details) {
  if (details.value_scale != vital::ValueDetails::kIndexed)
    return 0;
  double span = static_cast<double>(details.max) - static_cast<double>(details.min) + 1.0;
  if (span <= 0.0)
    return 0;
  return std::min(static_cast<size_t>(span), vital::Parameters::getStringLookupSize(details));
}

}  // namespace


// Both lists are derived once from a throwaway SoundEngine and never change.
// They are function-local statics so the C++11 "magic static" rule makes the
// one-time initialization thread-safe on its own, rather than relying on the
// caller happening to hold the GIL.
class ModulationDestinationListCache {
 public:
  static const std::vector<std::string> &get() {
    static const std::vector<std::string> cached_list = [] {
      std::vector<std::string> names;
      SoundEngine engine;
      vital::input_map &mono_destination_map =
          engine.getMonoModulationDestinations();
      for (auto &destination_iter : mono_destination_map) {
        names.push_back(destination_iter.first);
      }
      return names;
    }();
    return cached_list;
  }
};

class ModulationSourceListCache {
 public:
  static const std::vector<std::string> &get() {
    static const std::vector<std::string> cached_list = [] {
      std::vector<std::string> names;
      SoundEngine engine;
      vital::output_map &source_map = engine.getModulationSources();
      for (auto &source_iter : source_map) {
        names.push_back(source_iter.first);
      }
      return names;
    }();
    return cached_list;
  }
};


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

// Get formatted display text for a control (with scaling & units).
static std::string get_control_text(HeadlessSynth &synth, const std::string &name) {
    auto &controls = synth.getControls();
    auto it = controls.find(name);
    if (it == controls.end())
        throw std::runtime_error("No control: " + name);
    mono_float raw = it->second->value();
    const auto &details = Parameters::getDetails(name);
    // Discrete/indexed parameters
    if (details.string_lookup) {
        size_t count = namedOptionCount(details);
        if (count == 0)
            throw std::runtime_error("No name table available for control: " + name);
        long idx = std::lround(raw - details.min);
        if (idx < 0) idx = 0;
        else if (static_cast<size_t>(idx) >= count) idx = static_cast<long>(count) - 1;
        return details.string_lookup[idx];
    }
    // Continuous parameters: apply scaling
    float skewed;
    switch (details.value_scale) {
    case ValueDetails::kQuadratic: skewed = raw * raw; break;
    case ValueDetails::kCubic: skewed = raw * raw * raw; break;
    case ValueDetails::kQuartic: skewed = raw * raw; skewed = skewed * skewed; break;
    case ValueDetails::kExponential:
        if (details.display_invert)
            skewed = 1.0f / std::pow(2.0f, raw);
        else
            skewed = std::pow(2.0f, raw);
        break;
    case ValueDetails::kSquareRoot: skewed = std::sqrt(raw); break;
    default: skewed = raw; break;
    }
    float display_val = details.display_multiply * skewed + details.post_offset;
    return std::to_string(display_val) + details.display_units;
}

// Wrapper class for Value that knows its parameter name
class ControlValue {
private:
    vital::Value* value_;
    std::string name_;
    HeadlessSynth* synth_;

public:
    ControlValue(vital::Value* value, const std::string& name, HeadlessSynth* synth)
        : value_(value), name_(name), synth_(synth) {}

    // Delegate existing Value methods
    float value() const { return value_->value(); }
    void set(double v) { value_->set(poly_float(static_cast<float>(v))); }
    void set(int v) { value_->set(poly_float(static_cast<float>(v))); }
    
    // Normalized control methods
    void set_normalized(double normalized) {
        // Clamp to 0-1
        normalized = std::max(0.0, std::min(1.0, normalized));
        
        const auto &details = Parameters::getDetails(name_);
        float value;
        
        if (details.value_scale == ValueDetails::kIndexed) {
            // For indexed parameters, quantize to discrete values
            int num_options = static_cast<int>(details.max - details.min + 1);
            int index = static_cast<int>(std::round(normalized * (num_options - 1)));
            value = details.min + index;
        } else {
            // For non-linear parameters, normalized represents the display position
            // We need to convert: normalized -> display value -> internal value
            float value_normalized = static_cast<float>(normalized);
            
            switch (details.value_scale) {
            case ValueDetails::kQuadratic:
                // Display = internal^2, so internal = sqrt(display)
                // normalized maps to display range
                value = details.min + std::sqrt(value_normalized) * (details.max - details.min);
                break;
            case ValueDetails::kCubic:
                // Display = internal^3, so internal = display^(1/3)
                value = details.min + std::pow(value_normalized, 1.0f/3.0f) * (details.max - details.min);
                break;
            case ValueDetails::kQuartic: {
                // VST behavior: knob position represents quartic of display position
                // When knob is at 50%, we want display = 4 * 0.5^4 = 0.25 seconds
                // Since display = internal^4 and max_display = max_internal^4
                // We want internal = (normalized^4 * max_display)^(1/4) = normalized * max_internal
                value = details.min + value_normalized * (details.max - details.min);
                break;
            }
            case ValueDetails::kExponential:
                // For exponential, normalized maps directly to the exponent
                if (details.display_invert)
                    value = details.min + (1.0f / std::pow(2.0f, value_normalized)) * (details.max - details.min);
                else
                    value = details.min + std::pow(2.0f, value_normalized) * (details.max - details.min);
                break;
            case ValueDetails::kSquareRoot:
                // Display = sqrt(internal), so internal = display^2
                value = details.min + (value_normalized * value_normalized) * (details.max - details.min);
                break;
            default: // Linear
                value = details.min + value_normalized * (details.max - details.min);
                break;
            }
        }
        
        value_->set(value);
    }
    
    double get_normalized() const {
        const auto &details = Parameters::getDetails(name_);
        float raw = value_->value();
        
        if (details.value_scale == ValueDetails::kIndexed) {
            // For indexed parameters, normalize based on index
            int num_options = static_cast<int>(details.max - details.min + 1);
            int index = static_cast<int>(std::round(raw - details.min));
            return static_cast<double>(index) / (num_options - 1);
        } else {
            // Normalize to 0-1 range within parameter bounds
            float normalized_internal = (raw - details.min) / (details.max - details.min);
            float normalized;
            
            // Convert internal value to display position (0-1)
            switch (details.value_scale) {
            case ValueDetails::kQuadratic:
                // Display = internal^2
                normalized = normalized_internal * normalized_internal;
                break;
            case ValueDetails::kCubic:
                // Display = internal^3
                normalized = normalized_internal * normalized_internal * normalized_internal;
                break;
            case ValueDetails::kQuartic:
                // VST behavior: normalized position = internal position
                normalized = normalized_internal;
                break;
            case ValueDetails::kExponential:
                // For exponential, need to account for the range
                if (details.display_invert)
                    normalized = -std::log2(normalized_internal + 1e-10f);
                else
                    normalized = std::log2(normalized_internal + 1e-10f);
                break;
            case ValueDetails::kSquareRoot:
                // Display = sqrt(internal)
                normalized = std::sqrt(normalized_internal);
                break;
            default: // Linear
                normalized = normalized_internal;
                break;
            }
            
            return std::max(0.0, std::min(1.0, static_cast<double>(normalized)));
        }
    }
    
    std::string get_text() const {
        return get_control_text(*synth_, name_);
    }
};

NB_MODULE(vita, m) {

    m.def("get_modulation_sources", &get_modulation_sources,
		"Returns a list of allowed modulation sources.");

    m.def("get_modulation_destinations", &get_modulation_destinations,
          "Returns a list of allowed modulation destinations");
    
    auto m_constants = m.def_submodule("constants", "Submodule containing constants and enums");
    
    // Expose Enums
    nb::enum_<constants::SourceDestination>(m_constants, "SourceDestination", nb::is_arithmetic())
        .value("Filter1", constants::SourceDestination::kFilter1)
        .value("Filter2", constants::SourceDestination::kFilter2)
        .value("DualFilters", constants::SourceDestination::kDualFilters)
        .value("Effects", constants::SourceDestination::kEffects)
        .value("DirectOut", constants::SourceDestination::kDirectOut);

    nb::enum_<constants::Effect>(m_constants, "Effect", nb::is_arithmetic())
        .value("Chorus", constants::Effect::kChorus)
        .value("Compressor", constants::Effect::kCompressor)
        .value("Delay", constants::Effect::kDelay)
        .value("Distortion", constants::Effect::kDistortion)
        .value("Eq", constants::Effect::kEq)
        .value("FilterFx", constants::Effect::kFilterFx)
        .value("Flanger", constants::Effect::kFlanger)
        .value("Phaser", constants::Effect::kPhaser)
        .value("Reverb", constants::Effect::kReverb);

    nb::enum_<constants::FilterModel>(m_constants, "FilterModel", nb::is_arithmetic())
        .value("Analog", constants::FilterModel::kAnalog)
        .value("Dirty", constants::FilterModel::kDirty)
        .value("Ladder", constants::FilterModel::kLadder)
        .value("Digital", constants::FilterModel::kDigital)
        .value("Diode", constants::FilterModel::kDiode)
        .value("Formant", constants::FilterModel::kFormant)
        .value("Comb", constants::FilterModel::kComb)
        .value("Phase", constants::FilterModel::kPhase);

    nb::enum_<constants::RetriggerStyle>(m_constants, "RetriggerStyle", nb::is_arithmetic())
        .value("Free", constants::RetriggerStyle::kFree)
        .value("Retrigger", constants::RetriggerStyle::kRetrigger)
        .value("SyncToPlayHead", constants::RetriggerStyle::kSyncToPlayHead);

    // Parameter value scaling types
    nb::enum_<vital::ValueDetails::ValueScale>(m_constants, "ValueScale", nb::is_arithmetic())
        .value("Indexed", vital::ValueDetails::kIndexed)
        .value("Linear", vital::ValueDetails::kLinear)
        .value("Quadratic", vital::ValueDetails::kQuadratic)
        .value("Cubic", vital::ValueDetails::kCubic)
        .value("Quartic", vital::ValueDetails::kQuartic)
        .value("SquareRoot", vital::ValueDetails::kSquareRoot)
        .value("Exponential", vital::ValueDetails::kExponential);

    // ControlInfo provides metadata for each parameter
    nb::class_<vital::ValueDetails>(m, "ControlInfo")
        .def(nb::init<>())
        .def_ro("name", &vital::ValueDetails::name)
        .def_ro("min", &vital::ValueDetails::min)
        .def_ro("max", &vital::ValueDetails::max)
        .def_ro("default_value", &vital::ValueDetails::default_value)
        .def_ro("version_added", &vital::ValueDetails::version_added)
        .def_ro("post_offset", &vital::ValueDetails::post_offset)
        .def_ro("display_multiply", &vital::ValueDetails::display_multiply)
        .def_ro("scale", &vital::ValueDetails::value_scale)
        .def_ro("display_units", &vital::ValueDetails::display_units)
        .def_ro("display_name", &vital::ValueDetails::display_name)
        .def_prop_ro("is_discrete",
                     [](const vital::ValueDetails &d) {
            return d.value_scale == vital::ValueDetails::kIndexed;
        })
        .def_prop_ro("options", [](const vital::ValueDetails &d) {
            nb::list opts;
            size_t count = namedOptionCount(d);
            for (size_t i = 0; i < count; ++i)
                opts.append(d.string_lookup[i]);
            return opts;
        }, "Display names for a discrete control's values, in value order.\n\n"
           "Empty for continuous controls. A few controls accept more values\n"
           "than Vital has names for, in which case this is shorter than\n"
           "max - min + 1 and the surplus values have no name.");

    nb::enum_<SynthOscillator::SpectralMorph>(m_constants, "SpectralMorph", nb::is_arithmetic())
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
        .value("Skew", SynthOscillator::SpectralMorph::kSkew);
    
    nb::enum_<SynthOscillator::DistortionType>(m_constants, "DistortionType", nb::is_arithmetic())
            // "Off" rather than "None": the latter is a Python keyword, so
            // DistortionType.None could not be written in Python source at all
            // and only getattr(DistortionType, "None") reached it. It also made
            // the generated type stub syntactically invalid.
            .value("Off", SynthOscillator::DistortionType::kNone)
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
        .value("RmSample", SynthOscillator::DistortionType::kRmSample);
        
    nb::enum_<SynthOscillator::UnisonStackType>(m_constants, "UnisonStackType", nb::is_arithmetic())
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
               SynthOscillator::UnisonStackType::kOddHarmonicSeries);
    
    nb::enum_<RandomLfo::RandomType>(m_constants, "RandomLFOStyle", nb::is_arithmetic())
        .value("Perlin", RandomLfo::RandomType::kPerlin)
        .value("SampleAndHold", RandomLfo::RandomType::kSampleAndHold)
        .value("SinInterpolate", RandomLfo::RandomType::kSinInterpolate)
        .value("LorenzAttractor", RandomLfo::RandomType::kLorenzAttractor);

    nb::enum_<VoiceHandler::VoicePriority>(m_constants, "VoicePriority", nb::is_arithmetic())
        .value("Newest", VoiceHandler::VoicePriority::kNewest)
        .value("Oldest", VoiceHandler::VoicePriority::kOldest)
        .value("Highest", VoiceHandler::VoicePriority::kHighest)
        .value("Lowest", VoiceHandler::VoicePriority::kLowest)
        .value("RoundRobin", VoiceHandler::VoicePriority::kRoundRobin);
    
    nb::enum_<VoiceHandler::VoiceOverride>(m_constants, "VoiceOverride", nb::is_arithmetic())
        .value("Kill",VoiceHandler::VoiceOverride::kKill)
        .value("Steal", VoiceHandler::VoiceOverride::kSteal);

    nb::enum_<PredefinedWaveFrames::Shape>(m_constants, "WaveShape", nb::is_arithmetic())
        .value("Sin", PredefinedWaveFrames::kSin)
        .value("SaturatedSin", PredefinedWaveFrames::kSaturatedSin)
        .value("Triangle", PredefinedWaveFrames::kTriangle)
        .value("Square", PredefinedWaveFrames::kSquare)
        .value("Pulse", PredefinedWaveFrames::kPulse)
        .value("Saw", PredefinedWaveFrames::kSaw);

    nb::enum_<SynthLfo::SyncType>(m_constants, "SynthLFOSyncType", nb::is_arithmetic())
        .value("Trigger", SynthLfo::SyncType::kTrigger)
        .value("Sync", SynthLfo::SyncType::kSync)
        .value("Envelope", SynthLfo::SyncType::kEnvelope)
        .value("SustainEnvelope", SynthLfo::SyncType::kSustainEnvelope)
        .value("LoopPoint", SynthLfo::SyncType::kLoopPoint)
        .value("LoopHold", SynthLfo::SyncType::kLoopHold);
    
    nb::enum_<MultibandCompressor::BandOptions>(m_constants, "CompressorBandOption", nb::is_arithmetic())
        .value("Multiband", MultibandCompressor::BandOptions::kMultiband)
        .value("LowBand", MultibandCompressor::BandOptions::kLowBand)
        .value("HighBand", MultibandCompressor::BandOptions::kHighBand)
        .value("SingleBand", MultibandCompressor::BandOptions::kSingleBand);
    
    nb::enum_<SynthFilter::Style>(m_constants, "SynthFilterStyle", nb::is_arithmetic())
        .value("k12Db", SynthFilter::Style::k12Db)
        .value("k24Db", SynthFilter::Style::k24Db)
        .value("NotchPassSwap", SynthFilter::Style::kNotchPassSwap)
        .value("DualNotchBand", SynthFilter::Style::kDualNotchBand)
        .value("BandPeakNotch", SynthFilter::Style::kBandPeakNotch)
        .value("Shelving", SynthFilter::Style::kShelving);
    
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
    
    nb::enum_<SyncedFrequencyName>(m_constants, "SyncedFrequency", nb::is_arithmetic())
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
        .value("k1_64", SyncedFrequencyName::k1_64);
        
//    https://github.com/mtytel/vital/blob/636ca0ef517a4db087a6a08a6a8a5e704e21f836/src/synthesis/modulators/synth_lfo.h#L58C1-L65C9
    enum SyncOption {
        kTime,
        kTempo,
        kDottedTempo,
        kTripletTempo,
        kKeytrack,
    };
    
    nb::enum_<SyncOption>(m_constants, "SynthLFOSyncOption", nb::is_arithmetic())
        .value("Time", SyncOption::kTime)
        .value("Tempo", SyncOption::kTempo)
        .value("DottedTempo", SyncOption::kDottedTempo)
        .value("TripletTempo", SyncOption::kTripletTempo)
        .value("Keytrack", SyncOption::kKeytrack);

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
        // Overloads are registered narrowest first. nanobind tries every
        // overload without implicit conversion before trying any with it, so
        // the order does not change which one a given argument selects -- but
        // generated stubs list them in registration order, and a type checker
        // treats a leading `float` overload as shadowing `int`.
        // Handle enum values
        .def("set", [](vital::Value &self, SyncedFrequencyName value) {
            self.set(poly_float(static_cast<float>(static_cast<int>(value))));
        }, nb::arg("value"))
        // Handle integer values
        .def("set", [](vital::Value &self, int value) {
            self.set(poly_float(static_cast<float>(value)));
        }, nb::arg("value"))
        // Handle floating point values (both float and double)
        .def("set", [](vital::Value &self, double value) {
            self.set(poly_float(static_cast<float>(value)));
        }, nb::arg("value"))
        .def("__repr__", [](const vital::Value &v) {
            return "<Value value=" + std::to_string(v.value()) + ">";
        });

    nb::class_<vital::cr::Value, vital::Value>(m, "CRValue")
        .def(nb::init<poly_float>(),
             nb::arg("value") = 0.0f)
        .def("process", &vital::cr::Value::process, nb::arg("num_samples"))
        // Add the same method overloads for CRValue, again narrowest first.
        .def("set", [](vital::cr::Value &self, SyncedFrequencyName value) {
            self.set(poly_float(static_cast<float>(static_cast<int>(value))));
        }, nb::arg("value"))
        .def("set", [](vital::cr::Value &self, int value) {
            self.set(poly_float(static_cast<float>(value)));
        }, nb::arg("value"))
        .def("set", [](vital::cr::Value &self, double value) {
            self.set(poly_float(static_cast<float>(value)));
        }, nb::arg("value"))
        .def("__repr__", [](const vital::cr::Value &v) {
            return "<CRValue value=" + std::to_string(v.value()) + ">";
        });
    
    // Bind the ControlValue wrapper class
    nb::class_<ControlValue>(m, "ControlValue")
        .def("value", &ControlValue::value)
        .def("set", nb::overload_cast<int>(&ControlValue::set), nb::arg("value"))
        .def("set", nb::overload_cast<double>(&ControlValue::set), nb::arg("value"))
        .def("set_normalized", &ControlValue::set_normalized, nb::arg("value"),
             "Set control value using normalized 0-1 range")
        .def("get_normalized", &ControlValue::get_normalized,
             "Get control value as normalized 0-1 range")
        .def("get_text", &ControlValue::get_text,
             "Get formatted display text for the control");
    
    // Expose the SynthBase class, specifying ProcessorRouter as its base
    nb::class_<HeadlessSynth>(m, "Synth",
        "A headless Vital synthesizer.\n\n"
        "A new Synth starts on the init preset. Load a different one with\n"
        "load_preset or load_json, adjust it through get_controls, then call\n"
        "render or render_file.\n\n"
        "Give each thread its own Synth. render, render_file, load_preset,\n"
        "load_json and to_json all release the GIL, so separate instances\n"
        "render in parallel. Sharing one instance across threads is safe but\n"
        "serialized, so it gains you nothing.")
        .def(nb::init<>())  // Ensure there's a default constructor or adjust
                            // accordingly
                            // Bind the first overload of connectModulation

        .def("__getstate__", [](HeadlessSynth &synth) {
               return const_cast<HeadlessSynth &>(synth).pyToJson();  // Removes const safely
        })
        .def("__setstate__", [](HeadlessSynth &synth, const std::string &json) {
             new (&synth) HeadlessSynth();
             synth.loadFromString(json);
        })

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

        .def("set_bpm", &HeadlessSynth::pySetBPM, nb::arg("bpm"),
             "Set the tempo used by synced LFOs and delays.\n\n"
             "Parameters:\n"
             "  bpm (float): Beats per minute.")
        .def("set_sample_rate", &HeadlessSynth::setSampleRate, nb::arg("sample_rate"),
             "Set the render sample rate.\n\n"
             "Parameters:\n"
             "  sample_rate (float): Samples per second, e.g. 44100.")

        .def("render_file", &HeadlessSynth::renderAudioToFile2,
             // The whole function is pure C++ DSP + file I/O (no Python or
             // nanobind objects), so release the GIL for its entire duration.
             // nanobind converts the Python args before the guard releases the
             // GIL and converts the bool return after it re-acquires the GIL.
             nb::call_guard<nb::gil_scoped_release>(),
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
             "Renders audio to a NumPy array.\n\n"
             "Releases the GIL during the render, so threads that each own a\n"
             "separate Synth render in parallel.\n"
             "\n"
             "Parameters:\n"
             "  midi_note (int): MIDI note to render.\n"
             "  midi_velocity (float): Velocity of the note [0-1].\n"
             "  note_dur (float): Length of the note sustain in seconds.\n"
             "  render_dur (float): Length of the audio render in seconds.\n"
             "\n"
             "Returns:\n"
             "  numpy.ndarray: float32 array shaped (2, render_dur * sample_rate).")

        // load_json / to_json / load_preset only parse or serialize JSON and
        // mutate C++/Vital state -- no Python or nanobind objects touched while
        // working -- so release the GIL for the whole call. nanobind converts
        // the std::string arg/return on the GIL-held side of the guard.
        .def("load_json", &HeadlessSynth::loadFromString,
             nb::call_guard<nb::gil_scoped_release>(), nb::arg("json"),
             "Load a preset from a JSON string.\n\n"
             "Parameters:\n"
             "  json (str): Contents of a .vital preset.\n"
             "\n"
             "Returns:\n"
             "  bool: True if the preset was loaded, False if it was malformed\n"
             "  or came from a newer version of Vital.")

        .def("to_json", &HeadlessSynth::pyToJson,
             nb::call_guard<nb::gil_scoped_release>(),
             "Serialize the current state to a JSON string.\n\n"
             "Returns:\n"
             "  str: Preset JSON, in the same format as a .vital file.")

        .def("load_preset", &HeadlessSynth::pyLoadFromFile,
             nb::call_guard<nb::gil_scoped_release>(), nb::arg("filepath"),
             "Load a preset from a .vital file.\n\n"
             "Parameters:\n"
             "  filepath (str): Path to the .vital preset.\n"
             "\n"
             "Returns:\n"
             "  bool: True if the preset was loaded, False otherwise.")

        .def("load_init_preset", &HeadlessSynth::loadInitPreset,
             "Reset to the initial preset, clearing any modulations.")

        .def("clear_modulations", &HeadlessSynth::clearModulations,
             "Disconnect every modulation, leaving other controls untouched.")
        .def("get_controls", [](HeadlessSynth &synth) {
            nb::dict result;
            auto &controls = synth.getControls();
            for (const auto &[name, value] : controls) {
                result[name.c_str()] = ControlValue(value, name, &synth);
            }
            return result;
        }, nb::rv_policy::reference_internal,
           "Return every control, keyed by name.\n\n"
           "Returns:\n"
           "  dict[str, ControlValue]: Live handles -- setting one takes effect\n"
           "  on this Synth immediately.")
        .def("get_control_details", [](HeadlessSynth &synth, const std::string &name) {
            // Validate control name
            if (!vital::Parameters::isParameter(name))
                throw std::runtime_error("No metadata for control: " + name);
            // Return parameter metadata
            return vital::Parameters::getDetails(name);
        }, nb::arg("name"),
           "Get metadata for a control.\n\n"
           "Parameters:\n"
           "  name (str): Control name, e.g. \"delay_style\".\n"
           "\n"
           "Returns:\n"
           "  ControlInfo: Range, scale, default and display information.\n"
           "\n"
           "Raises:\n"
           "  RuntimeError: If no control has that name.")
        .def("get_control_text", get_control_text, nb::arg("name"),
             "Get the formatted display text for a control's current value.\n\n"
             "Parameters:\n"
             "  name (str): Control name, e.g. \"delay_style\".\n"
             "\n"
             "Returns:\n"
             "  str: The value as Vital would show it, e.g. \"Ping Pong\".")
        ;
}
