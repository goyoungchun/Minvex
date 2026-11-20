# Minvex

<p align="center">
  <img src="assets/branding/Minvex_banner_transparent.png" alt="Minvex — Lightweight 3D Graphics Engine" width="760">
</p>

**Lightweight 3D Graphics Engine**

### 한국어
C++ 애플리케이션에 3D 그래픽 기능을 간편하게 추가하는 경량 C++20 SDK 제공
GLB/glTF·OBJ·FBX 모델, Scene, Skeleton·Pose·Animation 및 렌더링 디버깅 지원  
Standalone·Embedded·Editor 실행 방식과 Low·Medium·High·Custom 품질 설정 제공

### English
Minvex is a lightweight C++20 graphics SDK for integrating 3D rendering into C++ applications.  
Supports glTF/GLB, OBJ, FBX, scenes, skeletal animation, and rendering diagnostics.  
Offers Standalone, Embedded, and Editor modes with Low, Medium, High, and Custom quality settings.

## 빠른 설치 및 실행

### Windows
**CMake 3.24+, C++20 컴파일러, Ninja, Developer PowerShell x64**

```powershell
cmake --preset windows-release
cmake --build --preset windows-release --parallel 4
./out/build/windows-release/bin/minvex_standalone.exe "path/to/model.glb"
./out/build/windows-release/bin/minvex_editor.exe
# 금속/비금속·거칠기 비교 예제
./out/build/windows-release/bin/minvex_rendering_enhancement.exe --pbr
./out/build/windows-release/bin/minvex_rendering_enhancement.exe --morph
./out/build/windows-release/bin/minvex_equipment.exe
./out/build/windows-release/bin/minvex_ui_overlay.exe
```

### macOS
**macOS 14.0+ 대상 Apple Silicon arm64 · CMake 3.24+, Xcode Command Line Tools, Ninja**

```sh
cmake --preset macos-release
cmake --build --preset macos-release --parallel 4
./out/build/macos-release/bin/minvex_editor
```

### 기존 C++ 프로젝트에 SDK 연결
**CMake 설치 및 `minvex::viewer` 연결**

Windows 릴리스 ZIP 이름의 `MSVC-19.xx`가 필요한 컴파일러 버전 계열입니다. Developer PowerShell x64에서 `cl /Bv`로 확인하고, 같은 `major.minor`의 MSVC와 아키텍처·빌드 구성·C++ 런타임을 사용합니다. 컴파일러 버전이 다르면 `find_package(Minvex)`가 SDK와 현재 컴파일러 버전을 표시합니다. 일치하는 도구 집합을 선택하거나 같은 릴리스 태그에서 현재 컴파일러로 Minvex를 빌드할 수 있습니다.

```powershell
cmake --install out/build/windows-release --config Release --prefix out/sdk-native
```

```cmake
find_package(Minvex 1.0 CONFIG REQUIRED COMPONENTS viewer graphics)
add_executable(my_viewer main.cpp)
target_link_libraries(my_viewer PRIVATE minvex::viewer)
```

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/path/to/out/sdk-native"
cmake --build build
```

### 설치 SDK로 움직이는 장면 시작

네이티브 SDK의 `share/minvex/examples/QuickStart`에서 두 C++20 예제를 빌드합니다. `minvex_quickstart_procedural`은 `MakeCube()`로 메시를 만들고 팔을 관절의 자식으로 배치합니다. **Space**와 **Shift+Space**로 관절을 돌리면 화면에 각도가 표시됩니다. `minvex_quickstart`는 GLB를 로드해 프레임 경과 시간에 따라 모델을 회전합니다.

```powershell
$sdk = "C:/path/to/extracted/Minvex-SDK"
cmake -S "$sdk/share/minvex/examples/QuickStart" -B quickstart-build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$sdk"
cmake --build quickstart-build
./quickstart-build/minvex_quickstart_procedural.exe
./quickstart-build/minvex_quickstart.exe "$sdk/share/minvex/examples/assets/SharedBox.glb"
```

macOS에서는 실행 파일 이름의 `.exe`를 뺍니다. 예제에 `--frames 3`을 전달하면 그래픽 초기화와 갱신을 확인한 뒤 자동 종료합니다.

같은 절차적 장면을 Emscripten으로 빌드해 브라우저에서 버튼으로 조작할 수 있습니다. [C++ 장면 연결](docs/API.md#c-장면-연결)

## 분야별 지원 기능

| 분야                       | 현재 지원 기능                                                                                                                                                |
| ------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Asset**                | glTF/GLB, OBJ/MTL, FBX(Static·Skin·Animation·Blend Shape Bake), 검사·재임포트·Hot Reload·비동기 로딩                                                               |
| **Scene**                | 객체 계층·Transform, 모델 인스턴스 공유, 소켓·부착 노드, Scene v13 저장·불러오기                                                                                                |
| **Mesh**                 | Static/Skeletal Mesh, GPU Skinning, 정점당 최대 4개 Bone 영향                                                                                                   |
| **Morph**                | Morph Target, GPU Morph, 노드별 독립 Weight, Weight Animation                                                                                                |
| **Material·Lighting**    | Base Color·Texture·Normal Map, 독립 Ambient·Directional·Point·Spot·LTC Rect Light, Sun Basic/PCF/PCSS·Point/Spot Hard/PCF 그림자, HDR·FXAA                                                            |
| **PBR**                  | Metallic/Roughness Material·Texture                                                                                                                     |
| **Camera**               | Perspective/Orthographic, Orbit·Pan·Zoom, 자동 Focus, Bounds Picking                                                                                      |
| **화면 UI**                | 텍스트·버튼·패널, 시스템/Font 폴더 폰트, 계층·Row/Column/Grid 자동 배치·Fixed/Fill/Hug·scroll/clip, 리치텍스트, UI Undo/Redo, 상태별 디자인, UI Designer·별도 UI 파일                      |
| **Skeleton·Pose**        | Bone Transform, Reference/Full/Partial Pose, Override, Pose 저장·불러오기, Shared Skeleton·Leader Pose 의상/파츠 동기화                                              |
| **Animation**            | Clip 재생·정지·반복·역재생, 속도·시간 제어, Pose Blending·채널 마스크, Blend1D/Blend2D, Additive Animation·Joint Mask, Animation Event, Clip Cross-fade, Timeline Scrubbing |
| **실행 방식**                | Standalone Viewer, Embedded Runtime, Editor                                                                                                             |
| **Editor UX**            | OS 파일 탐색기, Quick Open, Drag & Drop, Asset Browser, Hierarchy, Inspector, Gizmo, Blend Tree·Additive·Event Track·Leader Pose·소켓 편집, Undo/Redo            |
| **Debugging**            | Wireframe, Normal·UV·Depth, Skeleton·Bone Weight, Shadow Map·Blocker·Penumbra·Visibility, Bounding Box, GPU Resource Inspector                          |
| **Quality**              | Low·Medium·High·Custom, 프로파일 저장·불러오기, 지원 기능 검사·Fallback                                                                                                 |
| **Hardware·Performance** | CPU/GPU/OS 정보, FPS·Frame Time, 메모리 사용량·예산, 품질 추천                                                                                                        |
| **Optimization**         | Frustum Culling, Rigid LOD, Static Instancing, Vertex Cache/Fetch·LOD 배치 최적화, 선택적 Overdraw 최적화, 16비트 GPU index, BC3, On-Demand Rendering                                                              |
| **SDK**                  | C++20, CMake 설치·연결, Facade API, Result/Error, 공식 예제, CPU-only 구성                                                                                        |
| **Python**               | CPython 3.12, Viewer·Scene·Lighting·Pose·Animation·Blend Tree·Additive·Morph·Material·Quality·Event API, Wheel                                                   |
| **Web**                  | WebAssembly·WebGL2 Canvas, JavaScript API, URL·파일·메모리 로딩, Animation·Blend Tree·Additive·Event·Morph·PBR, Context 복구                                     |
| **Platform**             | Windows x64 / D3D11, macOS Apple Silicon arm64 / Metal, Browser WebGL2                                                                        |

## 실행 요구사항

| 항목          | 사양                                                                 |
| ----------- | ------------------------------------------------------------------ |
| **Windows** | x64, DirectX 11 Feature Level 11_0 이상                              |
| **macOS**   | Apple Silicon arm64, macOS 14.0 이상, Metal. Intel Mac은 공식 지원 대상에 포함되지 않음 |
| **메모리**     | 사용 장면과 자산 크기에 따라 달라짐                                                                |
| **개발 도구**   | CMake 3.24+, C++20 컴파일러, Ninja, 플랫폼 SDK                            |


## API와 라이선스

각 기능의 역할과 사용 방법은 [API 문서](docs/API.md)를 참고함.

Minvex 자체 코드는 [Apache-2.0](LICENSE)을 적용하며 [NOTICE](NOTICE)를 함께 제공함. 외부 의존성은 원래의 라이선스를 유지하며 설치 SDK의 `share/minvex/licenses`에서 고지를 확인할 수 있음.

## 표시 성능

Standalone에서 `--target-fps 0`을 주면 소프트웨어 FPS 제한을 해제하고 VSync를 사용합니다. 낮은 해상도와 주사율 동기화를 함께 사용하려면 `--preset low --target-fps 0`을 사용합니다.

macOS의 화면 표시에는 OS display callback·창 가림·절전 상태가 영향을 줌. 간헐적인 긴 프레임이 관측되며 일정한 최대 프레임 시간을 보장하지 않음. VSync 해제는 화면 주사율을 넘는 callback 빈도를 보장하지 않음.
