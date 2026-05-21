#pragma once

/// TNXFUNC() — marks a Construct method for exposure in the script editor.
///
/// Optional named parameters (key=value pairs, comma-separated):
///   DisplayName = "My Label"   — override the tab label shown in the editor.
///
/// Expands to nothing at runtime. The editor's ScriptParser scans source
/// files for this annotation to discover user-exposed functions beyond the
/// built-in lifecycle hooks (PrePhysics, PhysicsStep, PostPhysics, ScalarUpdate).
///
/// Usage:
///   TNXFUNC()
///   void FireWeapon(SimFloat dt);
///
///   TNXFUNC(DisplayName = "Custom Attack")
///   void CustomAttack();
#define TNXFUNC(...)
