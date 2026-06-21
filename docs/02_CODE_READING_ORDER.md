# 코드 읽기 순서

## 1단계: 프로그램의 바깥 테두리

1. `src/main.cpp`
2. `include/engine/Application.hpp`
3. `src/Application.cpp`의 `Application::run`

이 단계에서는 ImGui 세부 코드보다 `입력 -> 고정 업데이트 -> 렌더 -> UI` 순서만
찾는다.

## 2단계: 실행 상태와 게임플레이

1. `include/engine/Gameplay.hpp`
2. `EngineRuntime`
3. `PlayerController`
4. `Character`
5. `HealthComponent`, `ProjectileComponent`, `CombatComponent`
6. `src/Gameplay.cpp`

Edit World와 PIE World가 왜 따로 존재하는지, Controller가 Character를 어떻게
possess하는지 살펴본다. 그다음 `CombatComponent::fireProjectile`에서 투사체
Actor가 만들어지고, `ProjectileComponent::tickComponent`에서 raycast로 피해를
적용하는 흐름을 이어서 읽는다.

## 3단계: 엔진의 중심

1. `include/engine/Core.hpp`
2. `include/engine/World.hpp`
3. `src/World.cpp`

`Object -> ActorComponent -> SceneComponent` 상속과
`World -> Actor -> Component` 소유 관계는 서로 다른 개념이다. 상속은 “어떤
종류인가”, 소유는 “누가 수명을 관리하는가”를 뜻한다.

## 4단계: 화면과 충돌

1. `include/engine/Editor.hpp`
2. `include/engine/Components.hpp`
3. `include/engine/RenderTypes.hpp`
4. `include/engine/Systems.hpp`
5. `src/Editor.cpp`
6. `src/Components.cpp`
7. `src/Systems.cpp`
8. `src/Renderer.cpp`

`StaticMeshComponent -> RenderProxy -> RenderScene -> Renderer`와
`BoxComponent -> CollisionWorld` 두 흐름을 각각 따라간다. 그다음
`EditorViewportController -> EditorRay -> pickRenderProxy` 선택 흐름을 읽는다.

## 5단계: 저장과 편집

1. `src/Reflection.cpp`
2. `WorldSerializer`
3. `AssetRegistry`
4. `TransactionStack`
5. `Application::drawViewportPanel`
6. `Application::drawDetails`

reflection 정보 하나가 Details 패널과 JSON 저장에 함께 쓰이는 점을 찾아본다.

## 6단계: 테스트로 되짚기

`tests/EngineTests.cpp`는 작은 사용 예제 모음처럼 읽을 수 있다.

- `testAttachmentAndCycle`: Transform 계층과 순환 방지
- `testCollision`: overlap, raycast, sweep
- `testReflectionAndSerialization`: 프로퍼티와 World 저장/로드
- `testPieIsolation`: PIE 복제본 격리
- `testRenderProxyDirtyUpdate`: 변경된 proxy만 갱신
- `testCharacterCameraSeesPlayer`: 카메라가 플레이어를 바라보는지 확인
- `testCombatProjectileDamagesHealth`: 투사체가 HealthComponent에 피해를 주는지 확인
- `testEditorViewportMath`: 에디터 카메라와 screen ray
- `testRenderProxyPicking`: 가장 가까운 visible cube 선택
- `testWorldRestoreAndSnapshotTransactions`: 구조 편집 Undo/Redo
- `testApplicationOptions`, `testPngWriter`: 자동 캡처 입력과 PNG 방향

각 테스트에서 객체를 만드는 부분을 먼저 읽고, 검증식이 기대하는 규칙을
한국어 한 문장으로 바꿔 적어 보면 구조가 훨씬 빨리 익숙해진다.
