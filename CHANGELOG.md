# 변경 기록

형식은 [Keep a Changelog](https://keepachangelog.com/ko/1.1.0/)를 참고하고,
버전 번호는 [Semantic Versioning](https://semver.org/lang/ko/)을 따른다.

## [Unreleased]

### Added

- Texture asset GUID metadata load와 요청 시 OpenGL texture cache 생성 API
- StaticMesh asset을 GUID 기준 `MeshGpuResource`로 캐시하는 Renderer API
- Content Browser의 외부 glTF/texture 경로 입력 import와 Content rescan 버튼
- `Content/Input/default.input.json` 기반 InputAxis/InputAction 설정 로드
- Details 패널의 `ObjectChannel`을 숫자 DragInt 대신 collision channel 콤보로 편집
- `CollisionWorld::moveComponentStepped`와 Character step movement로 낮은 턱 이동 개선
- SpringArm 부모 CameraComponent의 Camera 채널 raycast 기반 카메라 충돌 보정
- `HealthComponent`, `ProjectileComponent`, `CombatComponent` 기반 단순 전투 흐름
- 투사체 raycast 피해 적용과 전투 컴포넌트 단위 테스트

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
