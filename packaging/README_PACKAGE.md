# Cocoa Engine 이동식 패키지

이 폴더는 원본 엔진 저장소 없이 실행하고 GameModule을 개발할 수 있는 배포본이다.

## 바로 실행

```powershell
.\run-editor.cmd
.\run-mcp.cmd --allow world-write --allow code-write --allow build --allow run
```

## AI가 C++ GameModule 만들기

AI는 `Game/Source/`만 수정한다. `code.scaffold_game_module`을 호출하거나
`SampleGameModule.cpp`를 복사해 `AIGameModule.cpp`를 만든 다음 아래 명령을 실행한다.

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
```

완성된 DLL은 `Game/Binaries/ai_game_module.dll`에 생성된다.
`include/cocoa_game_sdk/`가 공개 C ABI 헤더이며, CMake의 `Cocoa::cocoa_game_sdk` target이
include 경로를 자동으로 연결한다.

## 폴더 소유권

- `Content/`: World와 에셋
- `Game/Source/`: AI 또는 사용자가 작성하는 C++
- `Game/Binaries/`: 빌드된 GameModule DLL
- `include/`, `lib/`: 읽기 전용 SDK
- `examples/`: 재사용 가능한 `.cocoa.json` 예제
- `Saved/`: 로그, 에디터 설정, 스크린샷
