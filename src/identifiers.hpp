#pragma once

namespace atmt {

struct ID {
  static constexpr char pluginID[]          { "pluginID" };

  static constexpr char UNDOABLE[]          { "UNDOABLE" };
  static constexpr char PRESETS[]           { "PRESETS" };

  struct PRESET {
    static constexpr char type[]            { "PRESET" };
    static constexpr char name[]            { "name" };
    static constexpr char parameters[]      { "parameters" };
  };
};

} // namespace atmt
