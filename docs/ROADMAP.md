# 로드맵

완료 표시는 그 버전의 학습 목표를 데모와 테스트로 확인했다는 뜻이다.

## v0.5 Core와 Editor Foundation

- [x] Object, World, Actor, ActorComponent, SceneComponent
- [x] Z-up cm 좌표계와 attachment
- [x] 명시적 reflection과 자동 Details
- [x] JSON World 저장/로드
- [x] Edit, Simulate, Play In Editor 복제
- [x] AABB 충돌, raycast, sweep
- [x] RenderScene과 RenderProxy

## v0.6 Rendering과 Materials

- [x] off-screen 3D Viewport와 Unreal 방식 에디터 카메라
- [x] 클릭 선택, Transform 기즈모, snapping
- [x] Actor 생성·복제·삭제와 Undo/Redo
- [x] 도킹 레이아웃과 자동 시각 테스트 캡처
- [x] DirectionalLight를 사용하는 기본 Lambert 조명
- [x] Material과 MaterialInstance 연결
- [x] Shadow, Opaque, Debug, UI 렌더 패스 기록과 기본 Shadow 렌더
- [x] 렌더링 디버그 보기

## v0.7 Asset Pipeline

- [x] GUID 기반 asset handle
- [x] StaticMesh와 Material JSON 에셋 CPU 로딩
- [x] glTF와 texture import API
- [x] 요청 시 Texture GPU resource 생성
- [x] StaticMesh GPU resource 생성
- [x] Content Browser import 흐름

## v0.8 Gameplay Framework

- [x] InputAction/InputAxis 설정 파일
- [x] SpringArm 카메라 충돌
- [x] Character sweep와 계단 이동 개선
- [x] collision channel 편집 UI

## v0.9 Gameplay Tools

- [x] Jolt-ready rigid body adapter
- [x] skeletal animation
- [x] ImGuizmo rotate/scale와 Undo/Redo 확장
- [x] Blueprint-lite event/property graph
- [x] 체력, 투사체, 전투 Component

## v1.0 작은 3D RPG

- [ ] 에디터에서 레벨 제작
- [ ] 이동, 카메라, 충돌, 애니메이션, 전투
- [ ] 저장 가능한 World와 asset
- [ ] 별도 게임 실행 패키징

멀티스레드 렌더링, garbage collector, network replication, Nanite, Lumen,
전체 RHI 추상화는 v1.0 범위에 포함하지 않는다.
