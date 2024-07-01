#pragma once

namespace atmt {

struct ID {
  static constexpr char pluginID[]          { "pluginID" };

  static constexpr char UNDOABLE[]          { "UNDOABLE" };
  static constexpr char STATES[]            { "STATES" };

  struct STATE {
    static constexpr char type[]            { "STATE" };
    static constexpr char name[]            { "name" };
    static constexpr char parameters[]      { "parameters" };
  };
};

} // namespace atmt
