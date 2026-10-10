# numkit/sys

Operating system abstractions, virtual filesystem (VFS), process execution, environment handling, and plugin boundary protocol for NumKit.

Part of the **NumKit** modular runtime architecture (Layer 0.5 System).

## Features

- **Process Execution (`numkit/sys/process.hpp`)**: Cross-platform process launcher with captured stdout/stderr, exit code reporting, and executable presence detection (`runProcess`, `isCommandAvailable`).
- **Virtual Filesystem (`numkit/sys/vfs.hpp`)**: Unified filesystem abstraction supporting `NativeFS` (host OS via `std::filesystem`) and `CallbackFS` (browser/WASM IndexedDB, custom virtual directories).
- **Filesystem Context (`numkit/sys/fs_context.hpp`)**: Engine-agnostic filesystem session, script-origin tracking (`pushScriptOrigin`/`popScriptOrigin`), and path resolution.
- **Plugin ABI & Boundary (`numkit/sys/plugin.hpp`)**: Host-to-plugin interface (`PluginContext`, `CallContext`, `NUMKIT_PLUGIN_ABI_VERSION`), dynamic module registration and lifecycle.
- **Environment Handling (`numkit/sys/env.hpp`)**: Environment variable access (`NUMKIT_CWD`, `NUMKIT_FS`).

## Dependencies

- C++17 compatible compiler
- [numkit/value](https://github.com/numkit/value) (Layer 0 core substrate)

## License

[0BSD](LICENSE)
