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

## C++ 프로젝트에 연결

| 하려는 작업 | CMake 타깃 | 첫 헤더 |
|---|---|---|
| Minvex가 관리하는 창에 3D 표시 | `minvex::viewer` | `<minvex/Viewer.h>` |
| 앱의 D3D11 또는 Metal 타깃에 렌더링 | `minvex::embedded` | `<minvex/EmbeddedRenderer.h>` |
| 창 없이 모델 로드·검사 | `minvex::assets` | `<minvex/Asset.h>` |
| 장면을 구성하고 저장·불러오기 | `minvex::scene_io` | `<minvex/SceneIO.h>` |

### 설치 SDK로 첫 장면 실행

Windows x64 또는 macOS Apple Silicon arm64용 네이티브 Release SDK를 쓰기 가능한 폴더에 풉니다. 아래 명령에서 `<sdk>`를 SDK 루트 경로로 바꿉니다. CMake 3.24 이상과 Ninja가 필요합니다.

**Windows x64**에서는 ZIP 이름의 `MSVC-19.xx`와 같은 버전 계열의 Developer PowerShell x64를 엽니다. 설치된 Quick Start의 프리셋이 SDK 경로, Release 구성, `/MD` 런타임, Release 컴파일러 검사를 설정합니다. 빌드 결과는 SDK 루트의 `build`에 생성됩니다.

```powershell
$minvexSdk = "<sdk>"
Set-Location "$minvexSdk/share/minvex/examples/QuickStart"
cmake --preset windows-release
cmake --build --preset windows-release
& "$minvexSdk/build/windows-release/minvex_quickstart.exe" --frames 3
```

Quick Start를 다른 위치에 복사했다면 구성 전에 `$env:MINVEX_SDK_ROOT = "<sdk>"`를 지정하고 구성 명령에 `-B <빌드 경로>`를 추가합니다. 이 경우 `cmake --build <빌드 경로>`로 빌드합니다. 설치 폴더가 깊어 Windows 경로 길이 한도에 걸릴 때도 짧은 빌드 경로를 지정할 수 있습니다.

Windows 프리셋은 Developer PowerShell에 연결된 Visual Studio의 Ninja를 먼저 찾습니다. 설치 경로에 시스템 코드 페이지로 표현할 수 없는 문자가 있으면 `ninja -t wincodepage`에서 `UTF-8`을 보고하는 Ninja를 사용합니다. 구성할 때 `-DCMAKE_MAKE_PROGRAM=<ninja.exe 경로>`로 직접 지정할 수 있습니다.

콘솔이 없는 Windows 자동화에서 한글 경로를 사용하려면 Quick Start의 `Invoke-MinvexCMake.ps1`로 구성과 빌드를 실행합니다. 스크립트는 숨김 UTF-8 콘솔에서 CMake를 실행하며, CMake 경로, PATH의 MSVC·Ninja, 코드 페이지, 현재 단계와 출력을 실행 중에 표시합니다. `-TimeoutSeconds`로 대기 한도를 지정할 수 있습니다.

```powershell
$minvexCMake = (Get-Command cmake.exe).Source
./Invoke-MinvexCMake.ps1 -CMake $minvexCMake -CMakeArguments @('--preset', 'windows-release')
./Invoke-MinvexCMake.ps1 -CMake $minvexCMake -CMakeArguments @('--build', '--preset', 'windows-release')
```

**macOS Apple Silicon arm64**에서는 Release 구성과 SDK 경로를 지정합니다.

```sh
cmake -S "<sdk>/share/minvex/examples/QuickStart" -B quickstart-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 "-DCMAKE_PREFIX_PATH=<sdk>" -DMINVEX_QUICKSTART_BUILD_EXTRAS=OFF
cmake --build quickstart-build
./quickstart-build/minvex_quickstart --frames 3
```

`--frames`를 생략하면 창을 닫을 때까지 회전하는 큐브를 표시합니다. 모델 경로를 첫 인자로 주면 해당 모델을 표시합니다.

앱 코드에서 실제 렌더 프레임 수와 제출 통계는 `Viewer::GetTelemetry()`로 확인합니다. [Viewer 실행 검사](docs/API.md#viewer-실행-검사)

#### GLB 정비 매뉴얼 실행

Windows의 설치된 Quick Start 디렉터리에서 전체 예제 프리셋을 구성하고 정비 매뉴얼을 실행합니다. **이전**, **다음** 버튼으로 분해·조립 순서를 살펴보고 **모델 교체** 버튼으로 GLB를 바꿉니다. 모델 영역은 드래그로 회전하고 휠로 확대·축소합니다. 안내 패널에서 휠을 돌리면 부품 목록이 스크롤됩니다.

```powershell
cmake --preset windows-release-examples
cmake --build --preset windows-release-examples --target minvex_maintenance
& "$minvexSdk/build/windows-release-examples/minvex_maintenance.exe" ../assets/MaintenanceGearbox.glb --replacement ../assets/MaintenanceGearboxAlt.glb
```

macOS에서는 위 Ninja 구성에서 `MINVEX_QUICKSTART_BUILD_EXTRAS=ON`을 지정하고 `minvex_maintenance` 타깃을 빌드합니다. 실행 파일에 SDK의 `share/minvex/examples/assets`에 있는 GLB 경로를 전달합니다. 자체 GLB를 연결하는 기준은 API 문서에서 확인할 수 있습니다. [정비 매뉴얼 Quick Start](docs/API.md#glb-정비-매뉴얼-quick-start)

#### 지형 측량·적재 미리보기

현재 소스의 Quick Start 전체 예제에 `minvex_survey`와 `minvex_load_planning`이 포함됩니다. 측량 예제에서 **Shift+클릭**으로 두 점을 측정하고, **O**로 원근·직교를 바꾸고, **C**로 등고선을 표시합니다. 인자를 생략하면 산악 지형을 생성합니다. 적재 예제의 **Preview gravity**를 누르면 팔레트 가장자리에 걸친 상자가 기울고 떨어지는 과정이 청록색 윤곽으로 재생됩니다. 재생 후 **Apply result**를 눌러야 상자의 위치와 회전이 배치에 반영됩니다. **Discard**로 미리보기를 닫거나 상자를 드래그·방향키로 옮겨 새로 검사할 수 있습니다. 이 예제는 선택형 `minvex::physics` 컴포넌트를 사용합니다.

```powershell
cmake --preset windows-release
cmake --build --preset windows-release --target minvex_survey minvex_load_planning
& .\out\build\windows-release\bin\minvex_survey.exe .\examples\assets\SurveySlope.csv
& .\out\build\windows-release\bin\minvex_load_planning.exe
```

Editor의 **Window → Terrain**에서 평면 생성, 산악 프리셋, 브러시, CSV·PNG 가져오기, 등고선과 표고 측정을 사용할 수 있습니다. 지형은 Scene 파일에 함께 저장됩니다. [지형 API](docs/API.md#지형-격자와-측량) · [상자 물리](docs/API.md#상자-받침과-중력-미리보기)

#### Editor에서 장면 편집과 측정

- **Hierarchy**에서 빈 자식 객체를 만들거나, 가져온 에셋을 객체 위로 드래그해 자식으로 배치합니다. **Inspector → Model**에서 기존 객체의 모델을 교체하거나 분리할 수 있습니다.
- **Viewport → Surface Measure**에서 두 점을 클릭하면 모델 표면의 월드 좌표와 두 점 사이의 직선 거리·수평 거리·높이 차이가 표시됩니다. `F`는 선택 객체, `Shift+F`는 전체 장면을 화면에 맞춥니다.
- **Window → Box Physics**에서 선택 객체를 상자 콜라이더로 추가한 뒤 받침을 검사하거나 중력과 회전을 미리 봅니다. **Apply transforms**를 누르면 장면에 반영되며, Undo 한 번으로 되돌릴 수 있습니다. 이 패널은 상자 물리 옵션을 켠 빌드에서 제공됩니다.
- **Terrain → Edit grid sample**에서 행과 열을 지정해 한 지점의 표고를 편집합니다. **Inspector → Materials**에서는 알파 마스크와 컷오프를 설정합니다.
- **UI Designer → + Number**로 숫자 입력 필드를 추가하고 초기값·허용 범위·단위를 설정합니다. **Interact with UI in viewport**를 켜면 화면에서 직접 값을 입력해 확인할 수 있습니다.

### 기존 CMake 앱에 추가

프로젝트의 `CMakeLists.txt`에서 실제 앱 타깃 이름으로 `my_app`을 바꿉니다. 창 또는 네이티브 렌더링이 필요할 때 `graphics` component를 요청하면 CPU 전용 SDK를 구성 단계에서 구분할 수 있습니다.

```cmake
find_package(Minvex 1.0 CONFIG REQUIRED COMPONENTS viewer graphics)
include("${Minvex_DIR}/MinvexMSVCDependencies.cmake" OPTIONAL)
target_link_libraries(my_app PRIVATE minvex::viewer)
```

SDK 경로를 `CMAKE_PREFIX_PATH`에 추가하고 SDK와 같은 빌드 구성을 선택합니다. Release SDK를 다중 구성 생성기에서 쓰려면 `-DCMAKE_CONFIGURATION_TYPES=Release`로 구성한 뒤 `cmake --build build --config Release`로 빌드합니다.

위 `include()`는 MSVC·Ninja에서 설치 헤더 변경을 추적하는 데 필요한 `/showIncludes` 접두사를 설정합니다. Windows Release SDK의 초기 컴파일러 검사에서 `C1902`가 발생하면 새 빌드 디렉터리에서 `-DCMAKE_TRY_COMPILE_CONFIGURATION=Release`를 지정합니다. [CMake 컴파일러 검사 구성](https://cmake.org/cmake/help/latest/variable/CMAKE_TRY_COMPILE_CONFIGURATION.html)

SDK의 컴파일러, 아키텍처, 빌드 구성, component는 기본 경로 `share/minvex/sdk-manifest.json`에서 확인할 수 있습니다. 작업별 API와 설치 예제는 [SDK 통합 안내](docs/API.md#sdk-연결)를 참고하세요.

## 소스에서 빌드 및 실행

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
./out/build/windows-release/bin/minvex_fitting.exe
```

### macOS
**macOS 14.0+ 대상 Apple Silicon arm64 · CMake 3.24+, Xcode Command Line Tools, Ninja**

```sh
cmake --preset macos-release
cmake --build --preset macos-release --parallel 4
./out/build/macos-release/bin/minvex_editor
```

### 로컬 SDK 설치

```powershell
cmake --install out/build/windows-release --config Release --prefix out/sdk-native
```

macOS에서는 `out/build/macos-release`를 사용합니다. 설치한 SDK의 `out/sdk-native`를 `CMAKE_PREFIX_PATH`에 지정합니다.

## 분야별 지원 기능

| 분야                       | 현재 지원 기능                                                                                                                                                |
| ------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Asset**                | glTF/GLB, OBJ/MTL, FBX(Static·Skin·Animation·Blend Shape Bake), 검사·재임포트·Hot Reload·비동기 로딩                                                               |
| **Scene**                | 객체 계층·Transform, 모델 인스턴스 공유, 소켓·부착 노드, Scene v13 및 지형 포함 v14 저장·불러오기                                                                                |
| **Mesh**                 | Static/Skeletal Mesh, GPU Skinning, 정점당 최대 4개 Bone 영향                                                                                                   |
| **Morph**                | Morph Target, GPU Morph, 노드별 독립 Weight, Weight Animation                                                                                                |
| **Material·Lighting**    | Base Color·Texture·Normal Map, 독립 Ambient·Directional·Point·Spot·LTC Rect Light, Sun Basic/PCF/PCSS·Point/Spot Hard/PCF 그림자, HDR·FXAA                                                            |
| **PBR**                  | Metallic/Roughness Material·Texture                                                                                                                     |
| **Camera**               | Perspective/Orthographic, Orbit·Pan·Zoom, 자동 Focus, Standalone Viewer·Editor Bounds Picking                                                             |
| **화면 UI**                | 텍스트·버튼·패널·숫자 입력, 시스템/Font 폴더 폰트, 계층·Row/Column/Grid 자동 배치·Fixed/Fill/Hug·scroll/clip, 리치텍스트, UI Undo/Redo, 상태별 디자인, UI Designer·별도 UI 파일                      |
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
