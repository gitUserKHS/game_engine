# AI 에이전트 제작 가이드

## 현재 할 수 있는 것

`cocoa_mcp_server`는 Codex 같은 로컬 AI 에이전트에 reflection schema, World 조회,
원자적 편집 명령, 에셋 import, 빌드, CTest, hidden screenshot 도구를 제공한다.
AI가 직접 C++ gameplay를 작성해야 할 때는 `cocoa_game_sdk`와 GameModule DLL을 사용한다.

현재 구현은 **오프라인 World 편집**이다. 실행 중 에디터 named pipe와 승인 UI는 다음
단계이며, `engine.get_capabilities`의 `liveEditorEditing` 값으로 지원 여부를 확인한다.

## 빌드와 서버 실행

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
.\out\build\windows-debug\cocoa_mcp_server.exe --project .\CocoaProject.json
```

조회 전용이 기본이다. 쓰기·빌드·실행은 서버 시작 때 필요한 scope만 허용한다.

```powershell
.\out\build\windows-debug\cocoa_mcp_server.exe `
  --project .\CocoaProject.json `
  --allow world-write `
  --allow asset-write `
  --allow code-write `
  --allow build `
  --allow run
```

Codex MCP 설정에는 위 실행 파일을 `command`, 나머지 값을 `args`로 등록한다. 프로젝트의
절대 경로를 사용하는 편이 작업 디렉터리 차이로 인한 오류를 줄인다.

## 권장 에이전트 순서

1. `engine.get_capabilities`로 구현 상태를 확인한다.
2. `project.describe`, `schema.list_types`, `schema.get_type`을 읽는다.
3. `world.inspect` 또는 `world.query`로 현재 상태를 확인한다.
4. `world.preview_commands`로 `.cocoa.json` 명령을 임시 World에서 검증한다.
5. 변경 내용과 revision을 확인한 뒤 `world.apply_commands`를 호출한다.
6. `world.save`, `build.compile`, `build.test`를 실행한다.
7. `editor.capture`와 `editor.read_log`로 결과를 판독한다.

## 명령 스크립트

명령 형식은 [`CocoaCommand.schema.json`](../CocoaCommand.schema.json)에 정의되어 있다.
한 스크립트 안의 alias는 뒤 명령에서 GUID 대신 사용할 수 있다. 모든 명령은 하나의
transaction으로 적용되며 실패하면 World가 이전 snapshot으로 돌아간다.

- [`BlockWorld.cocoa.json`](../BlockWorld.cocoa.json): 큰 ground block을 만드는 예제
- [`OpenWorldAction.cocoa.json`](../OpenWorldAction.cocoa.json): Character, 체력, 전투, Controller를 연결하는 예제

`baseWorldRevision`을 지정하면 다른 작업이 먼저 World를 바꾼 경우 stale 오류로 중단한다.

## C++ GameModule

[`cocoa_game_sdk.h`](../include/cocoa_game_sdk/cocoa_game_sdk.h)는 DLL 경계를 C ABI로
고정한다. AI 코드는 기본적으로 `Game/Source/`만 수정한다.

`code.scaffold_game_module`은 `AIGameModule.cpp`를 만들고 `ai_game_module` CMake target을
활성화한다. 파일이 추가된 뒤에는 configure를 다시 실행해야 한다.

GameModule에서 사용할 수 있는 host API:

- 로그 출력
- 이름으로 Actor 검색
- reflected Actor 생성
- Component 추가
- JSON 값으로 property 설정
- InputAxis 조회
- World raycast

ABI가 맞지 않거나 필수 export가 없으면 엔진은 DLL을 로드하지 않는다. 현재는 hot reload
대신 모듈을 다시 빌드한 뒤 PIE 또는 게임 프로세스를 재시작한다.

## 안전 규칙

- 임의 shell 도구는 제공하지 않는다.
- CMake/CTest는 `CocoaProject.json`에 등록한 preset만 실행한다.
- 파일 쓰기는 `allowedWriteRoots` 아래로 제한한다.
- `engine-core-write`는 기본 허용하지 않는다.
- stdout은 MCP JSON-RPC 전용이며 진단은 stderr에 기록한다.
