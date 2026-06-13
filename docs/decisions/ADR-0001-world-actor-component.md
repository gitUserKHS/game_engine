# ADR-0001: World, Actor, Component 구조 채택

- 상태: 승인
- 날짜: 2026-06-14

## 상황

초기 엔진은 하나의 `SceneObject`가 이름, Transform, 렌더링과 충돌 책임을 함께
가졌다. 기능이 늘면서 객체마다 필요하지 않은 상태가 붙고, 카메라나 충돌처럼
재사용할 기능을 조합하기 어려워졌다.

## 결정

Unreal Engine에서 배우기 쉬운 핵심 흐름인 `World -> Actor -> Component`를
축소해 사용한다.

- `World`가 Actor 수명과 실행 순서를 관리한다.
- `Actor`는 Component 컨테이너이며 Transform을 직접 갖지 않는다.
- `SceneComponent`가 Transform과 attachment를 담당한다.
- Actor의 `RootComponent`가 Actor의 대표 World Transform이다.
- 기존 `SceneObject` API 호환은 유지하지 않는다.

## 이유

- 이동, 렌더, 충돌 기능을 작은 Component로 나눠 조합할 수 있다.
- 소유 관계와 공간 계층을 구분해 설명할 수 있다.
- Unreal의 Actor, Component, Gameplay Framework를 학습할 발판이 된다.
- 렌더링과 물리 시스템에 전달할 데이터를 Component 경계에서 만들 수 있다.

## 결과

좋은 점:

- 타입별 책임과 수명이 선명해진다.
- 플레이어, 카메라, 바닥을 같은 규칙으로 구성할 수 있다.
- reflection, 직렬화, Details 패널이 공통 Object 구조를 사용할 수 있다.

감수할 점:

- 작은 데모치고 클래스 수가 늘어난다.
- attachment와 생명주기 규칙을 별도로 테스트해야 한다.
- Unreal과 이름은 비슷하지만 GC, replication 등은 구현하지 않으므로 차이를
  문서에서 계속 밝혀야 한다.
