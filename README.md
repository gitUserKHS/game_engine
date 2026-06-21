# Cocoa Learning Engine

C++20과 OpenGL로 만드는 작은 Unreal-inspired 교육용 3D 엔진이다.
Unreal Engine의 규모를 복제하기보다 `World -> Actor -> Component` 구조와
에디터에서 Play In Editor(PIE)로 실행하는 흐름을 읽을 수 있는 크기로 구현한다.

현재 버전은 **v0.5.0**이다. 좌표계는 `X-forward`, `Y-right`, `Z-up`이고
거리 1단위는 1cm다.

## 처음 시작하기

1. [처음 읽는 안내서](docs/00_START_HERE.md)
2. [빌드와 실행](docs/01_BUILD_AND_RUN.md)
3. [코드 읽기 순서](docs/02_CODE_READING_ORDER.md)
4. [3D 에디터 첫 실습](docs/03_EDITOR_QUICKSTART.md)
5. [전체 구조](docs/ARCHITECTURE.md)
6. [AI 시각 테스트](docs/AI_VISUAL_TESTING.md)
7. [용어집](docs/GLOSSARY.md)

Visual Studio의 **Developer PowerShell for VS**에서 빠른 빌드:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
.\out\build\windows-debug\topdown_engine.exe
```

## 현재 보이는 것

- Unreal 방식 카메라와 선택 가능한 off-screen 3D Viewport
- 이동·회전·크기 기즈모, snapping, 생성·복제·삭제, Undo/Redo
- 자유롭게 이동하고 저장되는 ImGui 도킹 패널
- `F9` 또는 CLI로 editor/viewport PNG와 JSON metadata 캡처
- `W/A/S/D`로 움직이는 `Character`
- `HealthComponent`, `ProjectileComponent`, `CombatComponent` 기반 단순 전투 도구
- AABB 충돌, raycast, sweep, overlap 이벤트
- World Outliner, Details, Content Browser, Output Log
- `F5`로 편집 World를 보존하는 Play In Editor 시작과 종료
- reflection 기반 프로퍼티 편집과 JSON World 저장/로드

## 에디터 조작

| 입력 | 동작 |
| --- | --- |
| `RMB + W/A/S/D/Q/E` | 에디터 카메라 이동 |
| `RMB` 드래그 | 시점 회전 |
| `Alt + LMB` / `MMB` / 휠 | orbit / pan / dolly |
| `LMB` | 물체 선택 |
| `F` | 선택 물체 포커스 |
| `W/E/R` | 이동/회전/크기 기즈모 |
| `Ctrl+D` / `Delete` | 복제 / 삭제 |
| `F5` / `F9` | PIE 전환 / 스크린샷 |

## 프로젝트 지도

```text
include/engine/   공개 엔진 API
src/              엔진과 데모 구현
tests/            그래픽 창 없이 실행하는 핵심 테스트
Content/          에셋과 안정적인 GUID 메타데이터
shaders/          OpenGL 셰이더
docs/             한국어 학습 문서
```

개발 규칙은 [CONTRIBUTING.md](CONTRIBUTING.md), 버전별 변경은
[CHANGELOG.md](CHANGELOG.md), 앞으로의 순서는 [로드맵](docs/ROADMAP.md)을 참고한다.

이 프로젝트는 [MIT License](LICENSE)로 공개한다.
