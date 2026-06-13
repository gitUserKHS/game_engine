# 빌드와 실행

## 필요한 도구

- Windows 10 또는 11
- Visual Studio 2022의 `Desktop development with C++`
- CMake 3.20 이상
- Ninja
- Git
- OpenGL 3.3 이상을 지원하는 그래픽 드라이버
- 문서 검사에 사용할 Python 3

처음 configure할 때 CMake가 GLFW, GLM, Dear ImGui, ImGuizmo,
`nlohmann/json`을 인터넷에서 내려받는다.

## Debug 빌드

Visual Studio가 제공하는 **Developer PowerShell for VS**를 열고 저장소 루트에서
실행한다. 이 콘솔은 CMake가 `cl.exe` 컴파일러를 찾는 데 필요한 환경을 준비한다.

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
.\out\build\windows-debug\topdown_engine.exe
```

`windows-debug`는 디버거로 코드를 한 줄씩 따라가기 좋은 설정이다.

## Release 빌드

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
.\out\build\windows-release\topdown_engine.exe
```

## 기본 조작

| 입력 | 동작 |
| --- | --- |
| `W/A/S/D` | Simulate 또는 PIE에서 플레이어 이동 |
| `F5` | Play In Editor 시작 또는 종료 |
| `Escape` | 프로그램 종료 |
| Outliner 클릭 | Actor 선택 |
| Details 수정 | reflection 프로퍼티 변경 |
| `Save World` | `Content/Maps/Demo.world.json` 저장 |

## 자주 만나는 문제

### 의존성 다운로드 실패

인터넷 연결을 확인한 뒤 `out/build/windows-debug`를 지우고 configure를 다시 한다.
회사나 학교 네트워크라면 GitHub 접근 정책도 확인한다.

### C 또는 C++ 컴파일러를 찾지 못함

일반 PowerShell이 아니라 `Developer PowerShell for VS`에서 명령을 실행했는지
확인한다. Visual Studio Installer에서 `Desktop development with C++` workload와
Ninja가 설치되어 있어야 한다.

### 실행 파일은 켜지지만 화면이 비어 있음

그래픽 드라이버가 OpenGL 3.3 Core를 지원하는지 확인한다. 에디터 패널이 접혀
있다면 Viewport 창을 찾고, `F5`로 PIE를 시작해 본다.

### 셰이더 파일을 열지 못함

실행 파일을 직접 옮기지 말고 CMake가 만든 위치에서 실행한다. 셰이더와 Content
경로는 configure 시 저장소의 절대 경로로 지정된다.

## CI와 같은 검사

```powershell
python tools/check_docs.py
git diff --check
```

빌드와 테스트까지 모두 성공하면 GitHub Actions의 두 필수 검사와 거의 같은
환경을 로컬에서 확인한 것이다.
