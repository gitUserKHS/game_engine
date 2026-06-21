# 변경 기록

형식은 [Keep a Changelog](https://keepachangelog.com/ko/1.1.0/)를 참고하고,
버전 번호는 [Semantic Versioning](https://semver.org/lang/ko/)을 따른다.

## [Unreleased]

### Added

- 에디터, MCP, 리소스, GameModule SDK를 한 폴더에 설치하는 이동식 패키징 도구
- 배포된 SDK만으로 GameModule을 재빌드하고 MCP를 초기화하는 package smoke test
- `CocoaProject.json` 기반 AI 프로젝트 manifest와 쓰기 경로 정책
- 원자적 `.cocoa.json` 명령을 처리하는 `AuthoringSession`
- MCP `2025-11-25` stdio 서버와 reflection/World/build/capture 도구
- C ABI `cocoa_game_sdk`와 `sample_game_module` DLL target
- 블록 월드·오픈월드 액션 AI authoring 예제와 한국어 가이드

- Texture asset GUID metadata load와 요청 시 OpenGL texture cache 생성 API
- StaticMesh asset을 GUID 기준 `MeshGpuResource`로 캐시하는 Renderer API
- Content Browser의 외부 glTF/texture 경로 입력 import와 Content rescan 버튼
- `Content/Input/default.input.json` 기반 InputAxis/InputAction 설정 로드
- Details 패널의 `ObjectChannel`을 숫자 DragInt 대신 collision channel 콤보로 편집
- `CollisionWorld::moveComponentStepped`와 Character step movement로 낮은 턱 이동 개선
- SpringArm 부모 CameraComponent의 Camera 채널 raycast 기반 카메라 충돌 보정
- `HealthComponent`, `ProjectileComponent`, `CombatComponent` 기반 단순 전투 흐름
- 투사체 raycast 피해 적용과 전투 컴포넌트 단위 테스트
- ImGuizmo Transform transaction이 Location, Rotation, Scale을 함께 Undo/Redo하는 테스트
- Engine Debug 패널의 Opaque, Debug Wire, Selection Overlay 렌더 통계 보기
- `RenderPassRecord` 기반 Shadow, Opaque, Debug, UI 패스 기록과 Debug 패널 표시
- Directional light용 1024px shadow depth map과 기본 shadow factor 셰이더
- `BlueprintComponent` 기반 BeginPlay/Tick 이벤트와 reflection property 실행 그래프
- `SkeletalAnimationComponent` 기반 SceneComponent bone keyframe animation
- `RigidBodyComponent`와 Jolt 교체 지점용 `JoltRigidBodyAdapter`

- 크기 조절 가능한 OpenGL framebuffer 기반 ImGui Viewport
- Unreal 방식 에디터 카메라, visible proxy 클릭 선택, 선택 wireframe
- ImGuizmo 이동·회전·크기 편집, Local/World, snapping
- Actor 생성·복제·삭제와 snapshot 기반 Undo/Redo
- 자유 도킹 패널과 `Saved/Editor/imgui.ini` 배치 저장
- `F9` 및 CLI editor/viewport PNG와 JSON metadata 캡처
- `DirectionalLightComponent` 기반 기본 Lambert 조명과 MaterialInstance 색 반영
- GUID 기반 StaticMesh/Material JSON 에셋 로드 API
- 외부 `.gltf`, `.glb`, 이미지 파일을 Content 에셋과 `.meta`로 등록하는 importer API
- 카메라, picking, snapshot 복원, CLI, PNG 방향 테스트
- 에디터 첫 실습과 AI 시각 테스트 한국어 문서

### Changed

- Dear ImGui를 재현 가능한 `v1.90.9-docking` 태그로 고정했다.
- Windows 한글 작업경로에서 Content, shader, screenshot을 처리한다.

## [0.5.0] - 2026-06-14

### Added

- `Object`, 명시적 reflection, GUID와 `Outer`
- `World -> Actor -> Component` 구조와 지연 삭제
- `Z-up`, 1단위 1cm Transform 계층
- 고정 timestep과 네 개의 Tick 그룹
- `RenderScene`과 불변 `RenderProxy`
- AABB overlap, raycast, sweep, collision response
- `Character`, `PlayerController`, 입력 축 매핑
- Edit, Simulate, Play In Editor와 World 복제
- Outliner, Details, Content Browser, Output Log
- JSON World 저장/로드와 Asset Registry
- 한국어 학습 문서, Windows CI, GitHub 운영 파일

[Unreleased]: https://github.com/gitUserKHS/game_engine/compare/v0.5.0...HEAD
[0.5.0]: https://github.com/gitUserKHS/game_engine/releases/tag/v0.5.0
