# 용어집

| 용어 | 쉬운 설명 |
| --- | --- |
| AABB | 축에 평행한 직육면체 충돌 영역. 최소점과 최대점으로 표현한다. |
| Actor | World 안에 존재하며 여러 Component를 담는 게임 객체다. |
| ActorComponent | Actor에 기능을 붙이는 가장 기본적인 Component다. |
| Asset | 메시, 재질처럼 Content 폴더에서 관리하는 데이터다. |
| Attachment | SceneComponent를 부모와 자식 관계로 연결하는 것이다. |
| BeginPlay | World가 실제 실행을 시작할 때 한 번 호출되는 단계다. |
| Collision channel | 충돌 대상을 종류별로 구분하는 값이다. |
| Component | Actor에 이동, 표시, 충돌 같은 한 가지 책임을 붙이는 객체다. |
| Details | 선택한 객체의 reflection 프로퍼티를 편집하는 패널이다. |
| Fixed timestep | 화면 FPS와 별개로 일정한 시간 간격으로 게임 규칙을 갱신하는 방식이다. |
| GUID | 파일 경로나 이름이 바뀌어도 대상을 식별하는 안정적인 고유 ID다. |
| Local Transform | 부모를 기준으로 한 위치, 회전, 크기다. |
| Outer | 이 Object를 논리적으로 소유하는 바깥 Object다. |
| Pawn | Controller가 possess할 수 있는 Actor다. |
| PIE | Play In Editor. 편집 World의 복제본을 실행하는 모드다. |
| Possess | Controller가 특정 Pawn의 입력과 제어를 맡는 연결이다. |
| Reflection | 타입과 프로퍼티 정보를 실행 중에 조회하고 편집하는 기능이다. |
| RenderProxy | 렌더러에 전달하기 위해 Component 상태를 값으로 복사한 묶음이다. |
| Revision | Component 상태 변경 횟수. RenderScene이 갱신 여부를 판단한다. |
| RootComponent | Actor의 대표 Transform 역할을 하는 SceneComponent다. |
| SceneComponent | Transform과 부모/자식 연결을 가진 Component다. |
| Sweep | 도형을 시작점에서 끝점까지 움직여 도중 충돌을 찾는 query다. |
| Tick | 매 업데이트마다 실행되는 함수다. 기본은 비활성화다. |
| Transaction | 편집 작업을 되돌리거나 다시 적용하기 위한 명령 기록이다. |
| World | Actor, 충돌, 렌더 장면, 입력과 생명주기를 묶는 실행 공간이다. |
| World Transform | 부모 Transform까지 모두 합쳐진 최종 위치, 회전, 크기다. |
| Z-up | Z축을 위쪽으로 사용하는 좌표계다. |
