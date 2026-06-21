# 이동식 배포 패키지 만들기

## 목표

개발 저장소의 `out/build/`에서 exe만 꺼내면 셰이더, Content, 프로젝트 manifest가
빠지기 쉽다. 패키징 도구는 실행과 AI 제작에 필요한 파일을 하나의 독립 폴더에 모은다.

## 패키지 생성

Visual Studio의 **Developer PowerShell for VS**에서 실행한다.

```powershell
.\tools\package_engine.ps1
```

기본 결과는 `out/package/CocoaEngine/`이다. 원하는 외부 디렉터리를 지정할 수도 있다.

```powershell
.\tools\package_engine.ps1 `
  -Configuration Release `
  -OutputDirectory "D:\CocoaGames\MyBlockGame"
```

이미 빌드했다면 `-SkipBuild`를 붙여 설치 단계만 실행한다.

## 무엇이 들어가는가

```text
CocoaEngine/
|-- topdown_engine.exe       에디터
|-- cocoa_mcp_server.exe     AI용 stdio MCP 서버
|-- CocoaProject.json        경로, 빌드 preset, 쓰기 허용 범위
|-- CocoaCommand.schema.json 명령 스크립트 규격
|-- Content/                 World, 입력 설정, 에셋
|-- shaders/                 OpenGL 셰이더
|-- Game/
|   |-- Source/              AI가 수정하는 C++ 영역
|   `-- Binaries/            GameModule DLL 출력
|-- include/                 GameModule C ABI 공개 헤더
|-- lib/cmake/               find_package용 CMake SDK
|-- examples/                블록·액션 예제 명령
|-- docs/                    한국어 엔진 문서
|-- run-editor.cmd
`-- run-mcp.cmd
```

`include/`와 `lib/`는 SDK이므로 읽기 전용으로 생각한다. AI에는 `Game/Source`,
`Content`, `Saved`만 수정하도록 요청한다.

## 다른 컴퓨터에서 사용

1. 패키지 폴더 전체를 복사한다.
2. `run-editor.cmd`로 에디터가 열리는지 확인한다.
3. C++ GameModule을 빌드하려면 64비트 Visual Studio C++ workload, CMake, Ninja를 설치한다.
4. `run-mcp.cmd --allow ...`를 AI 클라이언트의 stdio MCP command로 등록한다.

에디터와 MCP 실행만 할 때는 원본 엔진 소스나 Git 저장소가 필요 없다. AI가 새로운
GameModule DLL을 컴파일할 때만 C++ 빌드 도구가 필요하다.

## GameModule만 빌드

배포 폴더의 `CMakeLists.txt`는 엔진 본체가 아니라 `Game/Source`만 빌드한다.

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
```

결과는 `Game/Binaries/sample_game_module.dll` 또는
`Game/Binaries/ai_game_module.dll`이다. 엔진 코어 소스는 배포본에 없으므로 AI가
실수로 내부 구현을 고칠 가능성도 줄어든다.

## 관련 테스트

`distributable_package_smoke`는 임시 폴더에 실제 install을 수행한 다음 다음을 검사한다.

- 필수 실행 파일과 리소스 존재
- 설치된 `CocoaGameSDKConfig.cmake` 탐색
- 배포 SDK만 이용한 샘플 GameModule 컴파일
- 배포된 MCP 서버의 `initialize` 응답
