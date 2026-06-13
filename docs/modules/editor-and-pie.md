# Editor와 Play In Editor

## 목표

하나의 실행 파일에서 Edit, Simulate, PlayInEditor 상태를 전환하고 편집 World를
보존하는 방법을 이해한다.

## 필요한 이유

레벨을 편집한 뒤 실행해 보면서 생긴 임시 위치와 상태가 원본에 남으면 작업물이
망가진다. PIE는 원본을 복제해 안전하게 시험하는 흐름이다.

## 핵심 타입

- [`EngineRuntime`](../../include/engine/Gameplay.hpp): 모드와 World 전환
- [`EditorMode`](../../include/engine/Gameplay.hpp): Edit, Simulate, PlayInEditor
- [`Application`](../../include/engine/Application.hpp): ImGui 패널과 viewport
- [`WorldSerializer`](../../include/engine/Systems.hpp): PIE 복제 경계
- [`TransactionStack`](../../include/engine/Systems.hpp): Details Undo/Redo

## 흐름도

```mermaid
flowchart LR
    Edit["Edit World"] -->|serialize| Json["메모리 JSON"]
    Json -->|deserialize| PIE["PIE World"]
    PIE -->|BeginPlay| Run["게임 실행"]
    Run -->|Stop| Drop["PIE World 폐기"]
    Drop --> Edit
```

## 코드 따라가기

1. [`Gameplay.cpp`](../../src/Gameplay.cpp)의 `EngineRuntime::startPlayInEditor`에서
   World 복제와 BeginPlay를 확인한다.
2. `EngineRuntime::stopPlayInEditor`에서 복제본 폐기를 확인한다.
3. [`Application.cpp`](../../src/Application.cpp)의 `drawEditor`에서 Viewport,
   Outliner, Details, Content Browser, Output Log 순서를 찾는다.
4. Details 편집이 `TransactionStack`에 undo/redo 명령을 넣는 부분을 읽는다.
5. ImGuizmo 조작이 선택한 Actor의 RootComponent Transform을 바꾸는지 확인한다.

## 실험 과제

Edit에서 플레이어 위치를 기록하고 PIE에서 이동한 뒤 종료한다. 다시 Edit로
돌아왔을 때 처음 위치가 유지되는지 확인한다.

## 흔한 실수

- Simulate도 복제 World를 사용한다고 생각한다. 현재 Simulate는 Edit World를 실행한다.
- 선택한 Actor 포인터를 PIE 전환 뒤에도 그대로 사용한다.
- 복제할 때 GUID와 attachment 복원을 빼먹는다.
- UI 편집을 Transaction 없이 직접 적용해 Undo가 동작하지 않는다.

## 관련 테스트

[`EngineTests.cpp`](../../tests/EngineTests.cpp)의 `testPieIsolation`이 PIE에서
바꾼 Transform이 Edit World에 남지 않는지 확인한다.
