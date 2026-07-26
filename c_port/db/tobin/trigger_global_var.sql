-- DG Scripts-style persisted global variables (trigger_script.h's `global`
-- command + %var% substitution) -- flat key/value store, no per-context
-- namespacing. See trigger_script.h's header comment for the design
-- rationale (2026-07-25: "use the DG_* source files to revamp triggers").
CREATE TABLE IF NOT EXISTS trigger_global_var (
    var_name  VARCHAR(32) NOT NULL PRIMARY KEY,
    var_value VARCHAR(128) NOT NULL DEFAULT ''
);
