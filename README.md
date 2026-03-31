# RiveQtQuickPlugin2

`RiveQtQuickPlugin2` is a Qt Quick plugin for playing Rive `.riv` files with
the official Rive runtime and renderer. The repository name is
`RiveQtQuickPlugin2`, but the QML import remains `RiveQtQuick`.

The goal is to keep rendering fast by integrating directly with Qt Quick's RHI
backends and sharing rendering resources across all `RiveItem` instances in the
scene. Desktop builds also include a best-effort QPainter software backend for
Qt Quick's `Software` graphics API.

```qml
import RiveQtQuick

RiveItem {
    source: "qrc:/animations/example.riv"
}
```

## Highlights

- Qt `6.6.0` and newer
- official Rive runtime integration
- Qt Quick / RHI based rendering
- shared rendering context across all Rive items
- `.riv` file caching to avoid duplicate loads
- example apps for quick manual testing
- unit tests and QML tests

### Supported backends

- Direct3D 11 on Windows
- Direct3D 12 when available in the bundled Rive runtime
- OpenGL 4.2+ on Windows and Linux through Qt Quick's graphics API switch
- Vulkan when available in the bundled Rive runtime
- Metal on macOS and iOS
- QPainter software rendering on desktop platforms through Qt Quick's
  `QSGRendererInterface::Software` backend

### Not supported

- Android / OpenGL ES
- desktop OpenGL on Apple platforms

### Known software limitations

- software rendering is desktop-only in v1
- advanced blend parity can differ from GPU backends, especially for
  `hue`, `saturation`, `color`, and `luminosity`
- feather softness is approximated and may render without the full soft-edge
  effect
- textured image meshes use a triangle fallback and can show seam or sampling
  differences compared with GPU backends

Right now the project is exercised most heavily on Windows and Apple platforms.
Release builds are the ones that matter for performance. Debug builds are fine
for development, but can be noticeably slower on complex animations.

## The Icon Grid demo

The icon grid demo shows a `100 x 100` table of buttons, each with its own live
`RiveItem`.

https://github.com/user-attachments/assets/077cfbf9-a3d2-4a5f-93f5-b1051273a8dc

## Project layout

- `src/RiveQtQuick` contains the plugin
- `examples` contains the demo applications
- `tests` contains unit and QML coverage
- `scripts` contains helper scripts
- `cmake` contains platform and dependency setup

## Examples

- `riveqtquick_minimal` is the smallest viewer
- `riveqtquick_interactive` is a compact interactive demo app
- `riveqtquick_marketplace_demo` is a marketplace browser / inspector demo that loads assets from the official Rive marketplace
- `riveqtquick_icon_grid` shows a large grid of Qt Quick buttons with live Rive icons inside

## Third-party setup

The repository expects the `3rdparty/` folder to be present.

Fetch or refresh it with:

```bash
python ./scripts/bootstrap.py
```

## Configure options

The top-level CMake project exposes a few useful switches:

- `RIVEQT_BUILD_EXAMPLES=ON|OFF` builds the example applications
- `RIVEQT_BUILD_TESTS=ON|OFF` builds the unit and QML tests
- `RIVEQT_ENABLE_OPENGL=ON|OFF` enables the desktop OpenGL backend on Windows/Linux
- `RIVEQT_ENABLE_METAL=ON|OFF` enables the Metal backend on Apple platforms
- `RIVEQT_ENABLE_SOFTWARE=ON|OFF` enables the desktop QPainter software backend
- `RIVEQT_IOS_BUNDLE_ID_PREFIX=<prefix>` sets the generated iOS example app bundle identifiers
- `RIVEQT_IOS_DEVELOPMENT_TEAM=<team-id>` sets an explicit Apple development team for iOS signing

`RIVEQT_ENABLE_OPENGL` defaults to `ON` on Windows/Linux and `OFF` on Apple platforms.
`RIVEQT_ENABLE_METAL` defaults to `ON` on Apple platforms and `OFF` elsewhere.
`RIVEQT_ENABLE_SOFTWARE` defaults to `ON` on desktop builds and `OFF` on mobile
platforms.

Backend selection stays in Qt. For example, request OpenGL before creating the
first window:

```cpp
QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
```

The desktop OpenGL backend requires an actual OpenGL `4.2+` core context.

Backend selection stays in Qt. For the software backend:

```cpp
QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
```

## Build

### Windows

Typical Windows build with MSVC 2022:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=<qt-path>
cmake --build build --config Release --target RiveQtQuick
```

Build the examples:

```bash
cmake --build build --config Release --target \
    riveqtquick_minimal \
    riveqtquick_interactive \
    riveqtquick_marketplace_demo \
    riveqtquick_icon_grid
```

### macOS

Example Ninja build on macOS:

```bash
cmake -S . -B build-macos -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_PREFIX_PATH=<qt-path> \
    -RIVEQT_BUILD_EXAMPLES=ON \
    -RIVEQT_BUILD_TESTS=ON
cmake --build build-macos --target RiveQtQuick
```

### iOS

For iOS, configure with Qt's iOS toolchain file and a host Qt installation:

```bash
cmake -S . -B build-ios -G Xcode \
    -DCMAKE_TOOLCHAIN_FILE=<qt-ios>/lib/cmake/Qt6/qt.toolchain.cmake \
    -DQT_HOST_PATH=<qt-host-path> \
    -DCMAKE_OSX_SYSROOT=iphoneos \
    -RIVEQT_BUILD_EXAMPLES=ON \
    -RIVEQT_BUILD_TESTS=OFF
```

The CMake setup includes Apple-specific handling for Qt kits that do not ship
arm64 iOS Simulator frameworks. If your simulator build links against device
frameworks by mistake, reconfigure for `x86_64` simulator builds or use
`iphoneos` for device builds.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

The QML test runner also accepts an explicit backend override:

```bash
./tests-bin/riveqtquick_qml_tests --graphics-api opengl
```

`opengl` is supported on Windows/Linux only and requires a desktop OpenGL `4.2+` context.

```bash
./tests-bin/riveqtquick_qml_tests --graphics-api software
```

Multi-config generators such as Visual Studio can still use:

```bash
ctest --test-dir build -C Debug --output-on-failure
```

## CI

The repository includes GitHub Actions workflows for:

- the general CI build and test matrix
- Apple-specific macOS build/test coverage
- unsigned iOS example app builds

## License

MIT
